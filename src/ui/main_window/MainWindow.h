#pragma once

#include <QMainWindow>
#include <QPointer>
#include <QStackedWidget>
#include <memory>

#include "core/services/ITabRepository.h"
#include "platform/pty/IPtyBackend.h"
#include "terminal/widget/TerminalWidget.h"
#include "ui/sidebar/SidebarDock.h"
#include "ui/tabs/TabBar.h"
#include "ui/toolbar/ToolbarWidget.h"

namespace dante::ui {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(core::ITabRepository* tabRepo, QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onNewTab();
    void onCloseTab(const core::Id& id);
    void onTabActivated(const core::Id& id);
    void onShellInjection(const QString& text);
    void onSettings();

private:
    void buildShortcuts();
    void spawnSessionFor(const core::Tab& tab);

    core::ITabRepository* m_tabRepo;
    TabBar* m_tabBar{nullptr};
    QStackedWidget* m_stack{nullptr};
    SidebarDock* m_sidebar{nullptr};
    ToolbarWidget* m_toolbar{nullptr};
    QHash<core::Id, QPointer<terminal::TerminalWidget>> m_terminals;
    QHash<core::Id, std::shared_ptr<platform::IPtyBackend>> m_backends;
};

}  // namespace dante::ui
