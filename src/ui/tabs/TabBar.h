#pragma once

#include <QHash>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QWidget>

#include "core/domain/Tab.h"
#include "core/util/Uuid.h"

namespace dante::ui {

// Horizontal scrollable tab strip with close button and emoji.
class TabBar : public QWidget {
    Q_OBJECT
public:
    explicit TabBar(QWidget* parent = nullptr);

    void addTab(const core::Tab& tab);
    void removeTab(const core::Id& id);
    void updateTitle(const core::Id& id, const QString& title);
    void setActive(const core::Id& id);
    int count() const { return m_chips.size(); }
    core::Id activeId() const { return m_activeId; }

signals:
    void newTabRequested();
    void tabActivated(const core::Id& id);
    void tabCloseRequested(const core::Id& id);

private:
    struct Chip {
        QWidget* host{nullptr};
        QPushButton* label{nullptr};
        QPushButton* close{nullptr};
    };

    QHBoxLayout* m_strip{nullptr};
    QPushButton* m_plusBtn{nullptr};
    QHash<core::Id, Chip> m_chips;
    core::Id m_activeId;
};

}  // namespace dante::ui
