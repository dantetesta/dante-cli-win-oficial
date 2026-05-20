#pragma once

#include <QSqlDatabase>

#include "core/util/Result.h"

namespace dante::persistence {

class Migrations {
public:
    // Applies all pending migrations in order, idempotent.
    // Reads SQL from :/sql/NNN_*.sql Qt resources.
    static core::Result<int> apply(QSqlDatabase& db);

private:
    static int currentVersion(QSqlDatabase& db);
};

}  // namespace dante::persistence
