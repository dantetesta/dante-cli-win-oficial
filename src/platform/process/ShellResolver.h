#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace dante::platform {

struct ShellProfile {
    QString id;          // "cmd", "powershell", "pwsh", "git-bash", "wsl-ubuntu"
    QString name;        // human-readable
    QString executable;  // absolute path
    QStringList args;
    QString icon;        // resource id
    bool available{false};
};

class ShellResolver {
public:
    // Detects every shell available on this Windows machine.
    // Order: cmd, powershell, pwsh, git-bash, wsl distros.
    static QList<ShellProfile> detectAvailable();

    // Returns the user's preferred default (powershell if available, then cmd).
    static ShellProfile defaultProfile();

    // Look up a profile by id, even if not currently available.
    static ShellProfile findById(const QString& id);
};

}  // namespace dante::platform
