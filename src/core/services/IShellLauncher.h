#pragma once

#include <QString>
#include <QStringList>
#include <functional>
#include <span>

#include "core/util/Result.h"

namespace dante::core {

struct ShellSpawnConfig {
    QString executable;
    QStringList args;
    QString cwd;
    QHash<QString, QString> envOverrides;
    int cols{120};
    int rows{30};
};

using OutputHandler = std::function<void(std::span<const std::byte>)>;

class IShellLauncher {
public:
    virtual ~IShellLauncher() = default;

    virtual Result<void> spawn(const ShellSpawnConfig& cfg) = 0;
    virtual Result<void> resize(int cols, int rows) = 0;
    virtual Result<void> write(std::span<const std::byte> data) = 0;
    virtual Result<void> sendSignal(int signal) = 0;
    virtual void setOutputHandler(OutputHandler handler) = 0;
    virtual bool isRunning() const = 0;
    virtual int processId() const = 0;
};

}  // namespace dante::core
