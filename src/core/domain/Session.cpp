#include "Session.h"

namespace dante::core {

Session Session::create(const Id& tabId, const QString& shell, const QString& cwd) {
    Session s;
    s.id = newId();
    s.tabId = tabId;
    s.shellProfile = shell;
    s.cwd = cwd;
    s.startedAt = QDateTime::currentDateTimeUtc();
    return s;
}

}  // namespace dante::core
