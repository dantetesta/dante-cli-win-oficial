#pragma once

#include <QSqlDatabase>

#include "core/services/ITabRepository.h"

namespace dante::persistence {

class SqliteTabRepository : public core::ITabRepository {
    Q_OBJECT
public:
    explicit SqliteTabRepository(QSqlDatabase db, QObject* parent = nullptr);

    QList<core::Tab> all() const override;
    core::Result<core::Tab> get(const core::Id& id) const override;
    core::Result<void> create(const core::Tab& tab) override;
    core::Result<void> update(const core::Id& id,
                              std::function<void(core::Tab&)> mutator) override;
    core::Result<void> remove(const core::Id& id) override;

private:
    core::Result<void> persist(const core::Tab& tab, bool isInsert);

    mutable QSqlDatabase m_db;
};

}  // namespace dante::persistence
