#pragma once

#include <QDialog>

namespace dante::ui {

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);
};

}  // namespace dante::ui
