#include "Database.h"

#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

#include "core/util/Logger.h"
#include "persistence/schema/Migrations.h"

namespace dante::persistence {

QString Database::defaultPath() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(base);
    return base + QStringLiteral("/dante.db");
}

core::Result<QSqlDatabase> Database::open(const QString& path) {
    QSqlDatabase db = QSqlDatabase::contains("dante")
        ? QSqlDatabase::database("dante")
        : QSqlDatabase::addDatabase("QSQLITE", "dante");

    db.setDatabaseName(path);
    if (!db.open()) {
        return core::Result<QSqlDatabase>::fail(1, db.lastError().text().toStdString());
    }

    QSqlQuery pragma(db);
    pragma.exec("PRAGMA journal_mode=WAL");
    pragma.exec("PRAGMA synchronous=NORMAL");
    pragma.exec("PRAGMA foreign_keys=ON");
    pragma.exec("PRAGMA temp_store=MEMORY");
    pragma.exec("PRAGMA cache_size=-20000");  // ~20 MiB

    const auto migRes = Migrations::apply(db);
    if (!migRes) {
        return core::Result<QSqlDatabase>::fail(migRes.error().code, migRes.error().message);
    }

    qCInfo(core::lcDb, "database open at %s (migrations applied=%d)",
           qPrintable(path), migRes.value());
    return core::Result<QSqlDatabase>::ok(db);
}

void Database::closeAll() {
    if (QSqlDatabase::contains("dante")) {
        QSqlDatabase::database("dante").close();
        QSqlDatabase::removeDatabase("dante");
    }
}

}  // namespace dante::persistence
