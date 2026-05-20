#pragma once

#include <QSqlDatabase>
#include <QString>

#include "core/util/Result.h"

namespace dante::persistence {

class Database {
public:
    static core::Result<QSqlDatabase> open(const QString& path);

    // Default location: %APPDATA%\Dante CLI\dante.db
    static QString defaultPath();

    static void closeAll();
};

}  // namespace dante::persistence
