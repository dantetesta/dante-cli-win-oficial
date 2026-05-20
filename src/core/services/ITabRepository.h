#pragma once

#include <QList>
#include <QObject>
#include <functional>

#include "core/domain/Tab.h"
#include "core/util/Result.h"

namespace dante::core {

class ITabRepository : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~ITabRepository() override = default;

    virtual QList<Tab> all() const = 0;
    virtual Result<Tab> get(const Id& id) const = 0;
    virtual Result<void> create(const Tab& tab) = 0;
    virtual Result<void> update(const Id& id, std::function<void(Tab&)> mutator) = 0;
    virtual Result<void> remove(const Id& id) = 0;

signals:
    void tabCreated(const Id& id);
    void tabUpdated(const Id& id);
    void tabRemoved(const Id& id);
};

}  // namespace dante::core
