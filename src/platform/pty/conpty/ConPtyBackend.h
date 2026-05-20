#pragma once

#ifdef _WIN32

#include <atomic>
#include <memory>
#include <thread>

#include "platform/pty/IPtyBackend.h"
#include "platform/pty/PtyEvents.h"
#include "platform/pty/conpty/WindowsHandle.h"

namespace dante::platform {

class ConPtyBackend : public IPtyBackend {
public:
    ConPtyBackend();
    ~ConPtyBackend() override;

    core::Result<void> spawn(const core::ShellSpawnConfig& cfg) override;
    core::Result<void> resize(int cols, int rows) override;
    core::Result<void> write(std::span<const std::byte> data) override;
    core::Result<void> sendSignal(int signal) override;
    void setOutputHandler(core::OutputHandler handler) override;
    bool isRunning() const override { return m_running.load(); }
    int processId() const override { return m_pid; }

    PtyEvents* events() { return &m_events; }

private:
    void readerLoop();
    void shutdownChild();

    HPCON m_hPC{nullptr};
    WindowsHandle m_hPipeIn;          // we write to child stdin
    WindowsHandle m_hPipeOut;         // we read child stdout/stderr (combined)
    WindowsHandle m_hChildOutWrite;   // ConPTY's end (child stdout write)
    WindowsHandle m_hChildInRead;     // ConPTY's end (child stdin read)
    WindowsHandle m_hProcess;
    WindowsHandle m_hThread;
    WindowsHandle m_hJob;

    std::thread m_reader;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_closing{false};
    int m_pid{0};

    core::OutputHandler m_handler;
    PtyEvents m_events;
};

}  // namespace dante::platform

#endif  // _WIN32
