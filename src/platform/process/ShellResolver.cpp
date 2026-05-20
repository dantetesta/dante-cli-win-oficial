#include "ShellResolver.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#ifdef _WIN32
#include <windows.h>
#endif

namespace dante::platform {

namespace {

QString envPath(const char* name) {
    const QByteArray v = qgetenv(name);
    return QString::fromLocal8Bit(v);
}

bool exists(const QString& path) { return !path.isEmpty() && QFileInfo::exists(path); }

QString gitBashFromRegistry() {
#ifdef _WIN32
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\GitForWindows",
                      0, KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS) {
        return {};
    }
    wchar_t buf[MAX_PATH] = {};
    DWORD size = sizeof(buf);
    DWORD type = 0;
    LONG r = RegQueryValueExW(hKey, L"InstallPath", nullptr, &type,
                              reinterpret_cast<LPBYTE>(buf), &size);
    RegCloseKey(hKey);
    if (r != ERROR_SUCCESS || type != REG_SZ) return {};
    const QString install = QString::fromWCharArray(buf);
    const QString bashPath = install + QStringLiteral("/bin/bash.exe");
    return exists(bashPath) ? QDir::toNativeSeparators(bashPath) : QString{};
#else
    return {};
#endif
}

QList<ShellProfile> detectWslDistros() {
    QList<ShellProfile> result;
#ifdef _WIN32
    QProcess proc;
    proc.start(QStringLiteral("wsl.exe"), {QStringLiteral("-l"), QStringLiteral("-q")});
    if (!proc.waitForFinished(2000)) return result;
    const QString out = QString::fromLocal8Bit(proc.readAllStandardOutput());
    for (const QString& line : out.split('\n', Qt::SkipEmptyParts)) {
        const QString distro = line.trimmed();
        if (distro.isEmpty()) continue;
        ShellProfile p;
        p.id = QStringLiteral("wsl-%1").arg(distro.toLower());
        p.name = QStringLiteral("WSL (%1)").arg(distro);
        p.executable = QStringLiteral("wsl.exe");
        p.args = {QStringLiteral("-d"), distro};
        p.icon = QStringLiteral(":/icons/shell-wsl.svg");
        p.available = true;
        result.append(p);
    }
#endif
    return result;
}

}  // namespace

QList<ShellProfile> ShellResolver::detectAvailable() {
    QList<ShellProfile> result;

#ifdef _WIN32
    const QString windir = envPath("WINDIR");

    // cmd.exe — always present
    {
        ShellProfile p;
        p.id = "cmd";
        p.name = "Command Prompt";
        p.executable = windir + QStringLiteral("/System32/cmd.exe");
        p.icon = ":/icons/shell-cmd.svg";
        p.available = exists(p.executable);
        if (p.available) result.append(p);
    }

    // Windows PowerShell 5.1
    {
        ShellProfile p;
        p.id = "powershell";
        p.name = "Windows PowerShell";
        p.executable = windir + QStringLiteral("/System32/WindowsPowerShell/v1.0/powershell.exe");
        p.args = {QStringLiteral("-NoLogo")};
        p.icon = ":/icons/shell-powershell.svg";
        p.available = exists(p.executable);
        if (p.available) result.append(p);
    }

    // PowerShell 7+ (pwsh.exe)
    {
        ShellProfile p;
        p.id = "pwsh";
        p.name = "PowerShell 7";
        const QString pf = envPath("ProgramFiles");
        const QString candidate = pf + QStringLiteral("/PowerShell/7/pwsh.exe");
        if (exists(candidate)) {
            p.executable = candidate;
        } else {
            // Fallback: search PATH
            p.executable = QStandardPaths::findExecutable(QStringLiteral("pwsh"));
        }
        p.args = {QStringLiteral("-NoLogo")};
        p.icon = ":/icons/shell-pwsh.svg";
        p.available = exists(p.executable);
        if (p.available) result.append(p);
    }

    // Git Bash
    {
        ShellProfile p;
        p.id = "git-bash";
        p.name = "Git Bash";
        p.executable = gitBashFromRegistry();
        if (p.executable.isEmpty()) {
            // Common default install location
            const QString def = QStringLiteral("C:/Program Files/Git/bin/bash.exe");
            if (exists(def)) p.executable = def;
        }
        p.args = {QStringLiteral("--login"), QStringLiteral("-i")};
        p.icon = ":/icons/shell-gitbash.svg";
        p.available = exists(p.executable);
        if (p.available) result.append(p);
    }

    // WSL distros
    result.append(detectWslDistros());
#endif

    return result;
}

ShellProfile ShellResolver::defaultProfile() {
    const auto profiles = detectAvailable();
    for (const auto& p : profiles) {
        if (p.id == "powershell") return p;
    }
    for (const auto& p : profiles) {
        if (p.id == "cmd") return p;
    }
    if (!profiles.isEmpty()) return profiles.first();
    return {};
}

ShellProfile ShellResolver::findById(const QString& id) {
    for (const auto& p : detectAvailable()) {
        if (p.id == id) return p;
    }
    return {};
}

}  // namespace dante::platform
