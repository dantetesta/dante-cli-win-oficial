#include "SettingsDialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>

#include "persistence/config/AppConfig.h"

namespace dante::ui {

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Configurações"));
    setMinimumWidth(420);

    auto& cfg = persistence::AppConfig::instance();

    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout();

    auto* theme = new QComboBox(this);
    theme->addItems({"dracula", "tokyo-night", "nord", "one-dark",
                     "solarized-dark", "gruvbox-dark", "monokai",
                     "catppuccin-mocha", "github-dark", "material-dark"});
    theme->setCurrentText(cfg.theme());
    connect(theme, &QComboBox::currentTextChanged,
            [&cfg](const QString& v) { cfg.setTheme(v); });
    form->addRow(tr("Tema"), theme);

    auto* fontCombo = new QFontComboBox(this);
    fontCombo->setCurrentFont(QFont(cfg.fontFamily()));
    connect(fontCombo, &QFontComboBox::currentFontChanged,
            [&cfg](const QFont& f) { cfg.setFontFamily(f.family()); });
    form->addRow(tr("Fonte"), fontCombo);

    auto* fontSize = new QSpinBox(this);
    fontSize->setRange(9, 28);
    fontSize->setValue(cfg.fontSize());
    connect(fontSize, &QSpinBox::valueChanged,
            [&cfg](int v) { cfg.setFontSize(v); });
    form->addRow(tr("Tamanho da fonte"), fontSize);

    auto* scrollback = new QSpinBox(this);
    scrollback->setRange(1000, 200000);
    scrollback->setSingleStep(1000);
    scrollback->setValue(cfg.scrollbackLines());
    connect(scrollback, &QSpinBox::valueChanged,
            [&cfg](int v) { cfg.setScrollbackLines(v); });
    form->addRow(tr("Scrollback (linhas)"), scrollback);

    auto* groq = new QLineEdit(this);
    groq->setEchoMode(QLineEdit::Password);
    groq->setText(cfg.groqApiKey());
    connect(groq, &QLineEdit::textChanged,
            [&cfg](const QString& v) { cfg.setGroqApiKey(v); });
    form->addRow(tr("Groq API Key"), groq);

    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::accept);
    layout->addWidget(buttons);
}

}  // namespace dante::ui
