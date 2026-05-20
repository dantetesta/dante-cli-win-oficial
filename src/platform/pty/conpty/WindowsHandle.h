#pragma once

#ifdef _WIN32

#include <windows.h>

namespace dante::platform {

// RAII wrapper for any HANDLE that should be freed with CloseHandle.
// Non-copyable, move-only. Tracks the sentinel value (INVALID_HANDLE_VALUE or NULL).
class WindowsHandle {
public:
    WindowsHandle() = default;
    explicit WindowsHandle(HANDLE h, HANDLE sentinel = INVALID_HANDLE_VALUE)
        : m_handle(h), m_sentinel(sentinel) {}

    WindowsHandle(const WindowsHandle&) = delete;
    WindowsHandle& operator=(const WindowsHandle&) = delete;

    WindowsHandle(WindowsHandle&& other) noexcept
        : m_handle(other.m_handle), m_sentinel(other.m_sentinel) {
        other.m_handle = other.m_sentinel;
    }
    WindowsHandle& operator=(WindowsHandle&& other) noexcept {
        if (this != &other) {
            close();
            m_handle = other.m_handle;
            m_sentinel = other.m_sentinel;
            other.m_handle = other.m_sentinel;
        }
        return *this;
    }

    ~WindowsHandle() { close(); }

    HANDLE get() const { return m_handle; }
    HANDLE* address() { return &m_handle; }
    bool valid() const { return m_handle != m_sentinel && m_handle != nullptr; }

    HANDLE release() {
        HANDLE h = m_handle;
        m_handle = m_sentinel;
        return h;
    }

    void reset(HANDLE h = INVALID_HANDLE_VALUE) {
        close();
        m_handle = h;
    }

    void close() {
        if (valid()) {
            CloseHandle(m_handle);
            m_handle = m_sentinel;
        }
    }

private:
    HANDLE m_handle{INVALID_HANDLE_VALUE};
    HANDLE m_sentinel{INVALID_HANDLE_VALUE};
};

}  // namespace dante::platform

#endif  // _WIN32
