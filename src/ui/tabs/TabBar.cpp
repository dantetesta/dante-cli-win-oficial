#include "TabBar.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QSpacerItem>
#include <QStyle>
#include <QToolButton>

namespace dante::ui {

TabBar::TabBar(QWidget* parent) : QWidget(parent) {
    setObjectName("DanteTabBar");
    setFixedHeight(40);
    setStyleSheet(R"(
        #DanteTabBar { background: #1A1B26; }
        QPushButton.dante-chip {
            background: #24283B;
            color: #C0CAF5;
            border: none;
            padding: 6px 14px;
            border-radius: 6px;
            font-size: 12px;
        }
        QPushButton.dante-chip[active="true"] {
            background: #7AA2F7;
            color: #1A1B26;
            font-weight: 600;
        }
        QPushButton.dante-close {
            background: transparent;
            color: #C0CAF5;
            border: none;
            padding: 0 8px;
            font-size: 14px;
        }
        QPushButton.dante-plus {
            background: transparent;
            color: #7AA2F7;
            border: none;
            font-size: 18px;
            padding: 0 12px;
        }
    )");

    auto* outer = new QHBoxLayout(this);
    outer->setContentsMargins(8, 4, 8, 4);
    outer->setSpacing(4);

    m_strip = new QHBoxLayout();
    m_strip->setContentsMargins(0, 0, 0, 0);
    m_strip->setSpacing(4);
    outer->addLayout(m_strip);

    m_plusBtn = new QPushButton("+", this);
    m_plusBtn->setProperty("class", "dante-plus");
    m_plusBtn->setObjectName("dantePlus");
    m_plusBtn->setCursor(Qt::PointingHandCursor);
    m_plusBtn->setToolTip(tr("Novo terminal (Ctrl+T)"));
    connect(m_plusBtn, &QPushButton::clicked, this, &TabBar::newTabRequested);
    outer->addWidget(m_plusBtn);
    outer->addStretch(1);
}

void TabBar::addTab(const core::Tab& tab) {
    Chip c;
    c.host = new QWidget(this);
    auto* hl = new QHBoxLayout(c.host);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(0);

    const QString display = tab.emoji.isEmpty()
        ? tab.title
        : QStringLiteral("%1  %2").arg(tab.emoji, tab.title);

    c.label = new QPushButton(display, c.host);
    c.label->setProperty("class", "dante-chip");
    c.label->setProperty("active", false);
    c.label->setCursor(Qt::PointingHandCursor);
    hl->addWidget(c.label);

    c.close = new QPushButton("×", c.host);
    c.close->setProperty("class", "dante-close");
    c.close->setCursor(Qt::PointingHandCursor);
    c.close->setFixedWidth(22);
    hl->addWidget(c.close);

    connect(c.label, &QPushButton::clicked, this, [this, id = tab.id]() {
        emit tabActivated(id);
        setActive(id);
    });
    connect(c.close, &QPushButton::clicked, this, [this, id = tab.id]() {
        emit tabCloseRequested(id);
    });

    m_strip->addWidget(c.host);
    m_chips.insert(tab.id, c);
}

void TabBar::removeTab(const core::Id& id) {
    auto it = m_chips.find(id);
    if (it == m_chips.end()) return;
    it.value().host->deleteLater();
    m_chips.erase(it);
    if (m_activeId == id) m_activeId = core::Id();
    if (!m_chips.isEmpty() && m_activeId.isNull()) {
        setActive(m_chips.constBegin().key());
    }
}

void TabBar::updateTitle(const core::Id& id, const QString& title) {
    if (auto it = m_chips.find(id); it != m_chips.end()) {
        it.value().label->setText(title);
    }
}

void TabBar::setActive(const core::Id& id) {
    m_activeId = id;
    for (auto it = m_chips.begin(); it != m_chips.end(); ++it) {
        const bool active = (it.key() == id);
        it.value().label->setProperty("active", active);
        it.value().label->style()->unpolish(it.value().label);
        it.value().label->style()->polish(it.value().label);
    }
    emit tabActivated(id);
}

}  // namespace dante::ui
