#pragma once

#include <QColor>
#include <QString>

#include "core/util/Uuid.h"

namespace dante::core {

struct AIProvider {
    Id id;
    QString name;        // e.g. "Claude", "Gemini", "Codex"
    QString command;     // executable to invoke (e.g. "claude")
    QString icon;        // path/resource id
    QColor color;
    QString shortcut;    // e.g. "Ctrl+Shift+C"
};

}  // namespace dante::core
