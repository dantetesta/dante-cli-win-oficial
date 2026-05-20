#pragma once

#include <QColor>
#include <QDateTime>
#include <QString>

#include "core/util/Uuid.h"

namespace dante::core {

enum class TabKind {
    Terminal = 0,
    Editor   = 1,
    Preview  = 2,
    Video    = 3,
};

struct Tab {
    Id id;
    QString title;
    QColor color{0x21, 0x21, 0x2A};
    QString emoji;          // optional, single emoji
    bool pinned{false};
    TabKind kind{TabKind::Terminal};
    QString cwd;            // initial working directory
    QString initialCommand; // command to send after spawn
    QString shellProfile;   // "cmd", "powershell", "pwsh", "git-bash", "wsl"
    QDateTime createdAt;
    QString paneTreeJson;   // serialised pane tree (split layout per tab)

    static Tab newTerminal(const QString& title);
};

}  // namespace dante::core
