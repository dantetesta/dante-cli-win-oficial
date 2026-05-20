#pragma once

#include <functional>
#include <span>

#include "core/services/IShellLauncher.h"
#include "core/util/Result.h"

namespace dante::platform {

using core::OutputHandler;
using core::Result;
using core::ShellSpawnConfig;

// IPtyBackend is the implementation contract for ConPty (Windows) and
// the future UnixPtyBackend (Linux/macOS — openpty/forkpty).
class IPtyBackend : public core::IShellLauncher {
public:
    using core::IShellLauncher::IShellLauncher;
};

}  // namespace dante::platform
