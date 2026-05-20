#ifdef _WIN32

#include "ConPtyBackend.h"

#include <QString>
#include <QStringList>

#include <array>
#include <vector>

#include "core/util/Logger.h"

namespace dante::platform {

namespace {

QString quoteArg(const QString& a) {
    if (a.isEmpty()) return QStringLiteral("\"\"");
    if (!a.contains(' ') && !a.contains('\t') && !a.contains('"')) return a;
    QString out = "\"";
    int slashes = 0;
    for (QChar c : a) {
        if (c == '\\') { ++slashes; out.append(c); }
        else if (c == '"') {
            out.append(QString(slashes + 1, '\\'));
            out.append('"');
            slashes = 0;
        } else {
            slashes = 0;
            out.append(c);
        }
    }
    out.append(QString(slashes, '\\'));
    out.append('"');
    return out;
}

QString joinCommandLine(const QString& exe, const QStringList& args) {
    QString cmd = quoteArg(exe);
    for (const QString& a : args) {
        cmd.append(' ');
        cmd.append(quoteArg(a));
    }
    return cmd;
}

}  // namespace

ConPtyBackend::ConPtyBackend() = default;

ConPtyBackend::~ConPtyBackend() {
    m_closing.store(true);

    if (m_hPC) {
        ClosePseudoConsole(m_hPC);
        m_hPC = nullptr;
    }

    // Closing the in-pipe to the child signals EOF on stdin.
    m_hPipeIn.close();

    if (m_reader.joinable()) {
        m_reader.join();
    }

    shutdownChild();
}

void ConPtyBackend::shutdownChild() {
    // Job object with KILL_ON_JOB_CLOSE → closing m_hJob kills the process tree.
    m_hJob.close();
    m_hProcess.close();
    m_hThread.close();
    m_hPipeOut.close();
    m_hChildInRead.close();
    m_hChildOutWrite.close();
}

core::Result<void> ConPtyBackend::spawn(const core::ShellSpawnConfig& cfg) {
    using core::Result;

    if (cfg.executable.isEmpty()) {
        return Result<void>::fail(1, "executable is empty");
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = FALSE;

    // Pipe pair 1: our_in_write → child_in_read (child stdin)
    HANDLE inRead = INVALID_HANDLE_VALUE, inWrite = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&inRead, &inWrite, &sa, 0)) {
        return Result<void>::fail(GetLastError(), "CreatePipe(in) failed");
    }
    m_hChildInRead.reset(inRead);
    m_hPipeIn.reset(inWrite);

    // Pipe pair 2: child_out_write → our_out_read (child stdout/stderr)
    HANDLE outRead = INVALID_HANDLE_VALUE, outWrite = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&outRead, &outWrite, &sa, 0)) {
        return Result<void>::fail(GetLastError(), "CreatePipe(out) failed");
    }
    m_hPipeOut.reset(outRead);
    m_hChildOutWrite.reset(outWrite);

    // Create pseudo console
    COORD size{};
    size.X = static_cast<SHORT>(std::max(cfg.cols, 1));
    size.Y = static_cast<SHORT>(std::max(cfg.rows, 1));
    HRESULT hr = CreatePseudoConsole(size, m_hChildInRead.get(), m_hChildOutWrite.get(), 0, &m_hPC);
    if (FAILED(hr)) {
        return Result<void>::fail(static_cast<int>(hr), "CreatePseudoConsole failed");
    }

    // Build STARTUPINFOEX with PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
    SIZE_T attrSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
    std::vector<unsigned char> attrBuf(attrSize);
    auto* attrList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrBuf.data());

    if (!InitializeProcThreadAttributeList(attrList, 1, 0, &attrSize)) {
        return Result<void>::fail(GetLastError(), "InitializeProcThreadAttributeList failed");
    }
    if (!UpdateProcThreadAttribute(
            attrList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
            m_hPC, sizeof(m_hPC), nullptr, nullptr)) {
        DeleteProcThreadAttributeList(attrList);
        return Result<void>::fail(GetLastError(), "UpdateProcThreadAttribute failed");
    }

    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    si.lpAttributeList = attrList;

    PROCESS_INFORMATION pi{};
    const QString cmdLine = joinCommandLine(cfg.executable, cfg.args);

    std::wstring wCmd = cmdLine.toStdWString();
    std::wstring wCwd = cfg.cwd.toStdWString();

    // Environment block: inherit current + apply overrides
    std::wstring envBlock;
    if (!cfg.envOverrides.isEmpty()) {
        for (auto it = cfg.envOverrides.begin(); it != cfg.envOverrides.end(); ++it) {
            envBlock += it.key().toStdWString();
            envBlock += L'=';
            envBlock += it.value().toStdWString();
            envBlock.push_back(L'\0');
        }
        envBlock.push_back(L'\0');
    }

    const BOOL ok = CreateProcessW(
        nullptr,
        wCmd.data(),
        nullptr, nullptr, FALSE,
        EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
        envBlock.empty() ? nullptr : envBlock.data(),
        wCwd.empty() ? nullptr : wCwd.c_str(),
        &si.StartupInfo, &pi);

    DeleteProcThreadAttributeList(attrList);

    if (!ok) {
        const DWORD err = GetLastError();
        return Result<void>::fail(static_cast<int>(err),
            QStringLiteral("CreateProcessW failed (err=%1) cmd=%2").arg(err).arg(cmdLine).toStdString());
    }

    m_hProcess.reset(pi.hProcess);
    m_hThread.reset(pi.hThread);
    m_pid = static_cast<int>(pi.dwProcessId);

    // Job Object — KILL_ON_JOB_CLOSE ensures full process tree cleanup.
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
        info.BasicLimitInformation.LimitFlags =
            JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
            JOB_OBJECT_LIMIT_BREAKAWAY_OK;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info));
        AssignProcessToJobObject(job, pi.hProcess);
        m_hJob.reset(job);
    }

    m_running.store(true);
    m_reader = std::thread([this] { readerLoop(); });

    qCInfo(core::lcPty, "ConPTY spawned pid=%d cmd=%s", m_pid, qPrintable(cmdLine));
    return Result<void>::ok();
}

void ConPtyBackend::readerLoop() {
    constexpr DWORD kBufSize = 64 * 1024;
    std::vector<std::byte> buffer(kBufSize);

    while (!m_closing.load()) {
        DWORD bytesRead = 0;
        const BOOL ok = ReadFile(m_hPipeOut.get(), buffer.data(),
                                  static_cast<DWORD>(buffer.size()), &bytesRead, nullptr);
        if (!ok || bytesRead == 0) {
            const DWORD err = GetLastError();
            if (err == ERROR_BROKEN_PIPE || bytesRead == 0) {
                break;
            }
            qCWarning(core::lcPty, "ReadFile failed err=%lu", err);
            break;
        }

        std::span<const std::byte> chunk(buffer.data(), bytesRead);
        if (m_handler) m_handler(chunk);
    }

    m_running.store(false);

    DWORD exitCode = 0;
    if (m_hProcess.valid()) {
        WaitForSingleObject(m_hProcess.get(), 2000);
        GetExitCodeProcess(m_hProcess.get(), &exitCode);
    }

    emit m_events.exited(static_cast<int>(exitCode));
}

core::Result<void> ConPtyBackend::resize(int cols, int rows) {
    if (!m_hPC) return core::Result<void>::fail(1, "no pseudo console");
    COORD s{};
    s.X = static_cast<SHORT>(std::max(cols, 1));
    s.Y = static_cast<SHORT>(std::max(rows, 1));
    const HRESULT hr = ResizePseudoConsole(m_hPC, s);
    if (FAILED(hr)) {
        return core::Result<void>::fail(static_cast<int>(hr), "ResizePseudoConsole failed");
    }
    return core::Result<void>::ok();
}

core::Result<void> ConPtyBackend::write(std::span<const std::byte> data) {
    if (!m_hPipeIn.valid()) return core::Result<void>::fail(1, "stdin pipe closed");
    DWORD written = 0;
    if (!WriteFile(m_hPipeIn.get(), data.data(),
                   static_cast<DWORD>(data.size()), &written, nullptr)) {
        return core::Result<void>::fail(GetLastError(), "WriteFile failed");
    }
    return core::Result<void>::ok();
}

core::Result<void> ConPtyBackend::sendSignal(int /*signal*/) {
    // Inject Ctrl-C as 0x03 byte into the PTY stream.
    const std::byte ctrlC[1] = { std::byte{0x03} };
    return write(std::span<const std::byte>(ctrlC, 1));
}

void ConPtyBackend::setOutputHandler(core::OutputHandler handler) {
    m_handler = std::move(handler);
}

}  // namespace dante::platform

#endif  // _WIN32
