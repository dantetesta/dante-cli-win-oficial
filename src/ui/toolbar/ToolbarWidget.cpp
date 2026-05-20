#include "ToolbarWidget.h"

#include <QAction>
#include <QLabel>
#include <QTimer>
#include <QWidget>

#include "platform/memstat/ProcessStats.h"

namespace dante::ui {

ToolbarWidget::ToolbarWidget(QWidget* parent) : QToolBar(tr("Main"), parent) {
    setMovable(false);
    setFloatable(false);
    setIconSize(QSize(18, 18));

    buildActions();

    auto* timer = new QTimer(this);
    timer->setInterval(2000);
    connect(timer, &QTimer::timeout, this, &ToolbarWidget::refreshStats);
    timer->start();
    refreshStats();
}

void ToolbarWidget::buildActions() {
    auto* newTab = addAction(QStringLiteral("➕ ") + tr("Novo"));
    connect(newTab, &QAction::triggered, this, &ToolbarWidget::newTabRequested);

    addSeparator();

    auto* claude = addAction(QStringLiteral("🤖 Claude"));
    connect(claude, &QAction::triggered, this,
            [this]() { emit aiLaunchRequested(QStringLiteral("claude\r")); });

    auto* gemini = addAction(QStringLiteral("✨ Gemini"));
    connect(gemini, &QAction::triggered, this,
            [this]() { emit aiLaunchRequested(QStringLiteral("gemini\r")); });

    auto* codex = addAction(QStringLiteral("🧪 Codex"));
    connect(codex, &QAction::triggered, this,
            [this]() { emit aiLaunchRequested(QStringLiteral("codex\r")); });

    addSeparator();

    auto* clearLine = addAction(QStringLiteral("✂ ") + tr("Clear line"));
    connect(clearLine, &QAction::triggered, this,
            [this]() { emit aiLaunchRequested(QStringLiteral("\x15")); });

    auto* spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addWidget(spacer);

    m_statsLabel = new QLabel(this);
    m_statsLabel->setStyleSheet("color: #C0CAF5; padding: 0 8px; font-family: 'Cascadia Code';");
    addWidget(m_statsLabel);

    auto* settings = addAction(QStringLiteral("⚙"));
    settings->setToolTip(tr("Configurações (Ctrl+,)"));
    connect(settings, &QAction::triggered, this, &ToolbarWidget::settingsRequested);
}

void ToolbarWidget::refreshStats() {
    if (!m_statsLabel) return;
    const auto mem = platform::ProcessStats::forCurrent();
    if (!mem.valid) {
        m_statsLabel->setText(QStringLiteral("—"));
        return;
    }
    const double mb = static_cast<double>(mem.privateBytes) / (1024.0 * 1024.0);
    m_statsLabel->setText(QStringLiteral("RAM %1 MB").arg(mb, 0, 'f', 1));
}

}  // namespace dante::ui
