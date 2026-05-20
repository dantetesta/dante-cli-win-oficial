#include "Migrations.h"

#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

#include "core/util/Logger.h"

namespace dante::persistence {

namespace {

struct Migration {
    int version;
    QString resourcePath;
};

const QVector<Migration>& schemaList() {
    static const QVector<Migration> list = {
        { 1, QStringLiteral(":/sql/schema/001_initial.sql") },
    };
    return list;
}

QString readResource(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    return QString::fromUtf8(f.readAll());
}

bool execScript(QSqlDatabase& db, const QString& script) {
    // SQLite cannot execute multiple statements in a single QSqlQuery::exec
    // call. Split on ';' at statement boundaries (naive but enough for our
    // schema, which has no procedural blocks).
    const QStringList rawStatements = script.split(';', Qt::SkipEmptyParts);
    QSqlQuery q(db);
    for (const QString& raw : rawStatements) {
        const QString stmt = raw.trimmed();
        if (stmt.isEmpty() || stmt.startsWith("--")) continue;
        if (!q.exec(stmt)) {
            qCCritical(core::lcDb, "migration statement failed: %s\n%s",
                       qPrintable(q.lastError().text()),
                       qPrintable(stmt));
            return false;
        }
    }
    return true;
}

}  // namespace

int Migrations::currentVersion(QSqlDatabase& db) {
    QSqlQuery q(db);
    if (!q.exec(QStringLiteral("SELECT version FROM schema_version LIMIT 1"))) return 0;
    if (q.next()) return q.value(0).toInt();
    return 0;
}

core::Result<int> Migrations::apply(QSqlDatabase& db) {
    if (!db.transaction()) {
        return core::Result<int>::fail(1, db.lastError().text().toStdString());
    }

    QSqlQuery q(db);
    q.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS schema_version (version INTEGER PRIMARY KEY)"));

    const int from = currentVersion(db);
    int applied = 0;

    for (const auto& m : schemaList()) {
        if (m.version <= from) continue;
        const QString script = readResource(m.resourcePath);
        if (script.isEmpty()) {
            db.rollback();
            return core::Result<int>::fail(2,
                ("missing migration resource " + m.resourcePath).toStdString());
        }
        if (!execScript(db, script)) {
            db.rollback();
            return core::Result<int>::fail(3,
                ("migration " + QString::number(m.version) + " failed").toStdString());
        }
        QSqlQuery up(db);
        up.prepare(QStringLiteral("INSERT OR REPLACE INTO schema_version(version) VALUES(?)"));
        up.addBindValue(m.version);
        up.exec();
        ++applied;
    }

    if (!db.commit()) {
        return core::Result<int>::fail(4, db.lastError().text().toStdString());
    }

    qCInfo(core::lcDb, "migrations applied: %d (from v%d to v%d)",
           applied, from, from + applied);
    return core::Result<int>::ok(applied);
}

}  // namespace dante::persistence
