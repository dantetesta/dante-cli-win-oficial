#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QKeySequence>
#include <QMenuBar>
#include <QShortcut>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

#include "core/util/Logger.h"
#include "platform/process/ShellResolver.h"
#include "terminal/registry/TerminalRegistry.h"
#include "ui/settings/SettingsDialog.h"

#ifdef _WIN32
#include "platform/pty/conpty/ConPtyBackend.h"
#endif

namespace dante::ui {

MainWindow::MainWindow(core::ITabRepository* tabRepo, QWidget* parent)
    : QMainWindow(parent), m_tabRepo(tabRepo) {
    setWindowTitle(QStringLiteral("Dante CLI"));
    resize(1280, 800);

    // Sidebar dock (left)
    m_sidebar = new SidebarDock(this);
    addDockWidget(Qt::LeftDockWidgetArea, m_sidebar);
    connect(m_sidebar, &SidebarDock::shellInjectionRequested,
            this, &MainWindow::onShellInjection);

    // Top toolbar with AI launchers + process stats
    m_toolbar = new ToolbarWidget(this);
    addToolBar(Qt::TopToolBarArea, m_toolbar);
    connect(m_toolbar, &ToolbarWidget::aiLaunchRequested,
            this, &MainWindow::onShellInjection);
    connect(m_toolbar, &ToolbarWidget::settingsRequested,
            this, &MainWindow::onSettings);
    connect(m_toolbar, &ToolbarWidget::newTabRequested,
            this, &MainWindow::onNewTab);

    // Central area: tab bar + stacked widget for terminals
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tabBar = new TabBar(central);
    layout->addWidget(m_tabBar);

    m_stack = new QStackedWidget(central);
    layout->addWidget(m_stack, 1);

    setCentralWidget(central);
    statusBar()->showMessage(
        QStringLiteral("Dante CLI %1 — pronto").arg(QApplication::applicationVersion()));

    connect(m_tabBar, &TabBar::newTabRequested, this, &MainWindow::onNewTab);
    connect(m_tabBar, &TabBar::tabActivated,    this, &MainWindow::onTabActivated);
    connect(m_tabBar, &TabBar::tabCloseRequested, this, &MainWindow::onCloseTab);

    // Restore tabs from repo
    const auto persisted = m_tabRepo->all();
    if (persisted.isEmpty()) {
        onNewTab();
    } else {
        for (const auto& t : persisted) {
            m_tabBar->addTab(t);
            spawnSessionFor(t);
        }
        m_tabBar->setActive(persisted.first().id);
    }

    buildShortcuts();
}

void MainWindow::buildShortcuts() {
    auto add = [this](const QString& seq, auto slot) {
        auto* s = new QShortcut(QKeySequence(seq), this);
        connect(s, &QShortcut::activated, this, slot);
    };
    add("Ctrl+T",       [this]{ onNewTab(); });
    add("Ctrl+W",       [this]{
        const auto id = m_tabBar->activeId();
        if (!id.isNull()) onCloseTab(id);
    });
    add("Ctrl+,",       [this]{ onSettings(); });
    add("Ctrl+Shift+C", [this]{ onShellInjection(QStringLiteral("claude")); });
    add("Ctrl+Shift+G", [this]{ onShellInjection(QStringLiteral("gemini")); });
}

void MainWindow::onNewTab() {
    core::Tab t = core::Tab::newTerminal(QStringLiteral("Terminal %1").arg(m_tabBar->count() + 1));
    const auto shell = platform::ShellResolver::defaultProfile();
    if (!shell.id.isEmpty()) t.shellProfile = shell.id;

    const auto res = m_tabRepo->create(t);
    if (!res) {
        qCWarning(core::lcUi, "tab create failed: %s", res.error().message.c_str());
        return;
    }
    m_tabBar->addTab(t);
    spawnSessionFor(t);
    m_tabBar->setActive(t.id);
}

void MainWindow::spawnSessionFor(const core::Tab& tab) {
#ifdef _WIN32
    auto backend = std::make_shared<platform::ConPtyBackend>();
    const auto shell = platform::ShellResolver::findById(tab.shellProfile);

    core::ShellSpawnConfig cfg;
    cfg.executable = shell.executable.isEmpty()
        ? platform::ShellResolver::defaultProfile().executable
        : shell.executable;
    cfg.args = shell.args;
    cfg.cwd = tab.cwd;
    cfg.cols = 120;
    cfg.rows = 30;

    auto spawnResult = backend->spawn(cfg);
    if (!spawnResult) {
        qCCritical(core::lcUi, "spawn failed: %s", spawnResult.error().message.c_str());
        statusBar()->showMessage(QString::fromStdString(spawnResult.error().message));
        return;
    }

    auto* term = terminal::TerminalRegistry::instance().ensure(tab.id);
    term->attachBackend(backend);
    m_backends.insert(tab.id, backend);

    connect(backend->events(), &platform::PtyEvents::exited, this,
            [this, id = tab.id](int code) {
                qCInfo(core::lcUi, "session %s exited code=%d",
                       qPrintable(core::idToString(id)), code);
            });

    connect(term, &terminal::TerminalWidget::titleChanged, this,
            [this, id = tab.id](const QString& title) { m_tabBar->updateTitle(id, title); });
    connect(term, &terminal::TerminalWidget::cwdChanged, this,
            [this, id = tab.id](const QString& cwd) {
                m_tabRepo->update(id, [&](core::Tab& t) { t.cwd = cwd; });
            });

    m_terminals.insert(tab.id, term);
    m_stack->addWidget(term);
    m_stack->setCurrentWidget(term);

    if (!tab.initialCommand.isEmpty()) {
        QTimer::singleShot(300, term, [backend, cmd = tab.initialCommand]() {
            const auto data = (cmd + "\r").toUtf8();
            const auto* p = reinterpret_cast<const std::byte*>(data.constData());
            backend->write(std::span<const std::byte>(p, data.size()));
        });
    }
#else
    Q_UNUSED(tab);
    qCWarning(core::lcUi, "PTY backend not available on this platform");
#endif
}

void MainWindow::onCloseTab(const core::Id& id) {
    m_tabBar->removeTab(id);
    if (auto term = m_terminals.take(id)) {
        if (term) {
            m_stack->removeWidget(term);
            terminal::TerminalRegistry::instance().destroy(id);
        }
    }
    m_backends.remove(id);
    m_tabRepo->remove(id);
    if (m_terminals.isEmpty()) {
        onNewTab();
    }
}

void MainWindow::onTabActivated(const core::Id& id) {
    if (auto t = m_terminals.value(id); t) {
        m_stack->setCurrentWidget(t);
        t->setFocus();
    }
}

void MainWindow::onShellInjection(const QString& text) {
    const auto id = m_tabBar->activeId();
    auto backend = m_backends.value(id);
    if (!backend) return;
    const QByteArray ba = text.toUtf8();
    const auto* p = reinterpret_cast<const std::byte*>(ba.constData());
    backend->write(std::span<const std::byte>(p, ba.size()));
}

void MainWindow::onSettings() {
    SettingsDialog dlg(this);
    dlg.exec();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Best-effort cleanup; backends destruct via shared_ptr.
    m_backends.clear();
    event->accept();
}

}  // namespace dante::ui
