#pragma once

#include <QDateTime>
#include <QString>

#include "core/util/Uuid.h"

namespace dante::core {

struct Session {
    Id id;
    Id tabId;
    QString cwd;
    QString shellProfile;
    QString initialCommand;
    QDateTime startedAt;
    QDateTime exitedAt;            // null when alive
    int exitCode{-1};

    static Session create(const Id& tabId, const QString& shell, const QString& cwd);
};

}  // namespace dante::core
