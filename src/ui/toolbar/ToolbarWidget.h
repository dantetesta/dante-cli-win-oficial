#pragma once

#include <QLabel>
#include <QToolBar>

namespace dante::ui {

class ToolbarWidget : public QToolBar {
    Q_OBJECT
public:
    explicit ToolbarWidget(QWidget* parent = nullptr);

signals:
    void aiLaunchRequested(const QString& command);
    void settingsRequested();
    void newTabRequested();

private:
    void buildActions();
    void refreshStats();

    QLabel* m_statsLabel{nullptr};
};

}  // namespace dante::ui
