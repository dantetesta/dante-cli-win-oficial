#include "SqliteTabRepository.h"

#include <QSqlError>
#include <QSqlQuery>

#include "core/util/Logger.h"

namespace dante::persistence {

namespace {

core::Tab fromRow(const QSqlQuery& q) {
    core::Tab t;
    t.id              = core::idFromString(q.value("id").toString());
    t.title           = q.value("title").toString();
    t.color           = QColor(q.value("color_hex").toString());
    t.emoji           = q.value("emoji").toString();
    t.pinned          = q.value("pinned").toInt() != 0;
    t.kind            = static_cast<core::TabKind>(q.value("kind").toInt());
    t.cwd             = q.value("cwd").toString();
    t.initialCommand  = q.value("initial_command").toString();
    t.shellProfile    = q.value("shell_profile").toString();
    t.paneTreeJson    = q.value("pane_tree_json").toString();
    t.createdAt       = QDateTime::fromString(q.value("created_at").toString(), Qt::ISODate);
    return t;
}

}  // namespace

SqliteTabRepository::SqliteTabRepository(QSqlDatabase db, QObject* parent)
    : core::ITabRepository(parent), m_db(std::move(db)) {}

QList<core::Tab> SqliteTabRepository::all() const {
    QList<core::Tab> out;
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM tabs ORDER BY sort_order, created_at"));
    if (!q.exec()) {
        qCWarning(core::lcDb, "tabs.all failed: %s", qPrintable(q.lastError().text()));
        return out;
    }
    while (q.next()) out.append(fromRow(q));
    return out;
}

core::Result<core::Tab> SqliteTabRepository::get(const core::Id& id) const {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM tabs WHERE id = ?"));
    q.addBindValue(core::idToString(id));
    if (!q.exec()) {
        return core::Result<core::Tab>::fail(1, q.lastError().text().toStdString());
    }
    if (!q.next()) {
        return core::Result<core::Tab>::fail(404, "tab not found");
    }
    return core::Result<core::Tab>::ok(fromRow(q));
}

core::Result<void> SqliteTabRepository::create(const core::Tab& tab) {
    const auto r = persist(tab, /*isInsert=*/true);
    if (r) emit tabCreated(tab.id);
    return r;
}

core::Result<void> SqliteTabRepository::update(const core::Id& id,
                                               std::function<void(core::Tab&)> mutator) {
    auto current = get(id);
    if (!current) {
        return core::Result<void>::fail(current.error().code, current.error().message);
    }
    auto t = current.value();
    mutator(t);
    const auto r = persist(t, /*isInsert=*/false);
    if (r) emit tabUpdated(id);
    return r;
}

core::Result<void> SqliteTabRepository::remove(const core::Id& id) {
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM tabs WHERE id = ?"));
    q.addBindValue(core::idToString(id));
    if (!q.exec()) {
        return core::Result<void>::fail(1, q.lastError().text().toStdString());
    }
    emit tabRemoved(id);
    return core::Result<void>::ok();
}

core::Result<void> SqliteTabRepository::persist(const core::Tab& tab, bool isInsert) {
    QSqlQuery q(m_db);
    if (isInsert) {
        q.prepare(QStringLiteral(
            "INSERT INTO tabs "
            "(id, title, color_hex, emoji, pinned, kind, cwd, initial_command, "
            "shell_profile, pane_tree_json, created_at, sort_order) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    } else {
        q.prepare(QStringLiteral(
            "UPDATE tabs SET "
            "title=?, color_hex=?, emoji=?, pinned=?, kind=?, cwd=?, "
            "initial_command=?, shell_profile=?, pane_tree_json=?, "
            "created_at=?, sort_order=? "
            "WHERE id=?"));
    }

    if (isInsert) q.addBindValue(core::idToString(tab.id));
    q.addBindValue(tab.title);
    q.addBindValue(tab.color.name());
    q.addBindValue(tab.emoji);
    q.addBindValue(tab.pinned ? 1 : 0);
    q.addBindValue(static_cast<int>(tab.kind));
    q.addBindValue(tab.cwd);
    q.addBindValue(tab.initialCommand);
    q.addBindValue(tab.shellProfile);
    q.addBindValue(tab.paneTreeJson);
    q.addBindValue(tab.createdAt.toString(Qt::ISODate));
    q.addBindValue(0);
    if (!isInsert) q.addBindValue(core::idToString(tab.id));

    if (!q.exec()) {
        return core::Result<void>::fail(1, q.lastError().text().toStdString());
    }
    return core::Result<void>::ok();
}

}  // namespace dante::persistence
