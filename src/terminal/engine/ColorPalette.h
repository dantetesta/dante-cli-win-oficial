#pragma once

#include <QColor>
#include <QString>
#include <array>

#include "core/parsing/AnsiTypes.h"

namespace dante::terminal {

struct ColorScheme {
    QString id;
    QString name;
    QColor background;
    QColor foreground;
    QColor cursor;
    QColor selection;
    std::array<QColor, 16> ansi;  // 0..15 standard ANSI
};

class ColorPalette {
public:
    static const ColorScheme& dracula();
    static const ColorScheme& solarizedDark();
    static const ColorScheme& tokyoNight();
    static const ColorScheme& nord();
    static const ColorScheme& oneDark();
    static const ColorScheme& gruvboxDark();
    static const ColorScheme& monokai();
    static const ColorScheme& catppuccin();
    static const ColorScheme& githubDark();
    static const ColorScheme& materialDark();

    static const ColorScheme& byId(const QString& id);

    static QColor resolve(const core::AnsiColor& c, const ColorScheme& scheme, bool isBackground);
};

}  // namespace dante::terminal
