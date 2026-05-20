#pragma once

#include <QDockWidget>
#include <QListWidget>
#include <QTabWidget>

namespace dante::ui {

class SidebarDock : public QDockWidget {
    Q_OBJECT
public:
    explicit SidebarDock(QWidget* parent = nullptr);

signals:
    void shellInjectionRequested(const QString& text);

private:
    void buildFavoritesTab();
    void buildFilesTab();
    void buildSnippetsTab();
    void buildCredentialsTab();

    QTabWidget* m_tabs{nullptr};
    QListWidget* m_favorites{nullptr};
    QListWidget* m_snippets{nullptr};
    QListWidget* m_credentials{nullptr};
};

}  // namespace dante::ui
