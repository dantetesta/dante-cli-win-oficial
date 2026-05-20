#include "ProcessStats.h"

#include <QHash>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif

namespace dante::platform {

MemoryStats ProcessStats::forCurrent() {
#ifdef _WIN32
    return forPid(static_cast<int>(GetCurrentProcessId()));
#else
    return {};
#endif
}

MemoryStats ProcessStats::forPid(int pid) {
    MemoryStats out;
#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
                           FALSE, static_cast<DWORD>(pid));
    if (!h) return out;

    PROCESS_MEMORY_COUNTERS_EX pmc{};
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(h, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        out.privateBytes = pmc.PrivateUsage;
        out.workingSetBytes = pmc.WorkingSetSize;
        out.valid = true;
    }
    CloseHandle(h);
#else
    Q_UNUSED(pid);
#endif
    return out;
}

namespace {

struct CpuSample {
    ULONGLONG kernel{0};
    ULONGLONG user{0};
    ULONGLONG when{0};
};

QHash<int, CpuSample>& cpuTable() {
    static QHash<int, CpuSample> t;
    return t;
}

}  // namespace

double ProcessStats::cpuPercent(int pid) {
#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!h) return 0.0;

    FILETIME create{}, exit{}, kernel{}, user{};
    if (!GetProcessTimes(h, &create, &exit, &kernel, &user)) {
        CloseHandle(h);
        return 0.0;
    }
    CloseHandle(h);

    auto toU64 = [](const FILETIME& ft) -> ULONGLONG {
        ULARGE_INTEGER li;
        li.LowPart = ft.dwLowDateTime;
        li.HighPart = ft.dwHighDateTime;
        return li.QuadPart;
    };

    const ULONGLONG nowKernel = toU64(kernel);
    const ULONGLONG nowUser   = toU64(user);
    const ULONGLONG nowTs     = GetTickCount64();

    CpuSample& prev = cpuTable()[pid];
    double pct = 0.0;

    if (prev.when != 0) {
        const ULONGLONG dCpu = (nowKernel - prev.kernel) + (nowUser - prev.user);
        const ULONGLONG dTime = (nowTs - prev.when) * 10000ULL;  // ms → 100ns
        if (dTime > 0) {
            SYSTEM_INFO si{};
            GetSystemInfo(&si);
            pct = static_cast<double>(dCpu) /
                  static_cast<double>(dTime) /
                  static_cast<double>(si.dwNumberOfProcessors) * 100.0;
            if (pct < 0.0) pct = 0.0;
            if (pct > 100.0) pct = 100.0;
        }
    }

    prev.kernel = nowKernel;
    prev.user   = nowUser;
    prev.when   = nowTs;
    return pct;
#else
    Q_UNUSED(pid);
    return 0.0;
#endif
}

}  // namespace dante::platform
