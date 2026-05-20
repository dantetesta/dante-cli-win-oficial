#include "SidebarDock.h"

#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>

namespace dante::ui {

SidebarDock::SidebarDock(QWidget* parent) : QDockWidget(tr("Sidebar"), parent) {
    setObjectName("DanteSidebar");
    setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    setMinimumWidth(280);

    auto* root = new QWidget(this);
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    m_tabs = new QTabWidget(root);
    m_tabs->setDocumentMode(true);
    layout->addWidget(m_tabs);

    setWidget(root);

    buildFavoritesTab();
    buildFilesTab();
    buildSnippetsTab();
    buildCredentialsTab();
}

void SidebarDock::buildFavoritesTab() {
    auto* w = new QWidget(this);
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(4, 4, 4, 4);

    auto* search = new QLineEdit(w);
    search->setPlaceholderText(tr("Buscar favoritos..."));
    l->addWidget(search);

    m_favorites = new QListWidget(w);
    l->addWidget(m_favorites);

    connect(m_favorites, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        const QString path = item->data(Qt::UserRole).toString();
        if (!path.isEmpty()) {
            emit shellInjectionRequested(QStringLiteral("cd \"%1\"\r").arg(path));
        }
    });

    m_tabs->addTab(w, QStringLiteral("★ ") + tr("Favoritos"));
}

void SidebarDock::buildFilesTab() {
    auto* model = new QFileSystemModel(this);
    model->setRootPath(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    auto* tree = new QTreeView(this);
    tree->setModel(model);
    tree->setRootIndex(model->index(model->rootPath()));
    tree->setColumnHidden(1, true);
    tree->setColumnHidden(2, true);
    tree->setColumnHidden(3, true);
    tree->setHeaderHidden(true);

    connect(tree, &QTreeView::doubleClicked, this, [this, model](const QModelIndex& idx) {
        const QString path = model->filePath(idx);
        if (!path.isEmpty()) {
            emit shellInjectionRequested(QStringLiteral("\"%1\" ").arg(path));
        }
    });

    m_tabs->addTab(tree, QStringLiteral("📁 ") + tr("Pastas"));
}

void SidebarDock::buildSnippetsTab() {
    auto* w = new QWidget(this);
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(4, 4, 4, 4);
    auto* search = new QLineEdit(w);
    search->setPlaceholderText(tr("Buscar snippets..."));
    l->addWidget(search);

    m_snippets = new QListWidget(w);
    l->addWidget(m_snippets);

    connect(m_snippets, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        const QString cmd = item->data(Qt::UserRole).toString();
        if (!cmd.isEmpty()) emit shellInjectionRequested(cmd);
    });

    m_tabs->addTab(w, QStringLiteral("⚡ ") + tr("Snippets"));
}

void SidebarDock::buildCredentialsTab() {
    auto* w = new QWidget(this);
    auto* l = new QVBoxLayout(w);
    l->setContentsMargins(4, 4, 4, 4);
    auto* search = new QLineEdit(w);
    search->setPlaceholderText(tr("Buscar credenciais..."));
    l->addWidget(search);

    m_credentials = new QListWidget(w);
    l->addWidget(m_credentials);

    connect(m_credentials, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        const QString block = item->data(Qt::UserRole).toString();
        if (!block.isEmpty()) emit shellInjectionRequested(block);
    });

    m_tabs->addTab(w, QStringLiteral("🔑 ") + tr("Chaves"));
}

}  // namespace dante::ui
