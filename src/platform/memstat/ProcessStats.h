#pragma once

#include <QString>
#include <cstdint>

namespace dante::platform {

struct MemoryStats {
    uint64_t privateBytes{0};   // PROCESS_MEMORY_COUNTERS_EX::PrivateUsage
    uint64_t workingSetBytes{0};
    bool valid{false};
};

class ProcessStats {
public:
    // Returns memory stats for the current process or a child PID.
    static MemoryStats forCurrent();
    static MemoryStats forPid(int pid);

    // Returns CPU usage (0..100) over the last sample interval.
    // First call returns 0; subsequent calls use the stored timestamp delta.
    static double cpuPercent(int pid);
};

}  // namespace dante::platform
