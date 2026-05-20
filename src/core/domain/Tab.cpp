#include "Tab.h"

namespace dante::core {

Tab Tab::newTerminal(const QString& title) {
    Tab t;
    t.id = newId();
    t.title = title;
    t.kind = TabKind::Terminal;
    t.shellProfile = QStringLiteral("powershell");
    t.createdAt = QDateTime::currentDateTimeUtc();
    return t;
}

}  // namespace dante::core
