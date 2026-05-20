#include "ColorPalette.h"

namespace dante::terminal {

namespace {

constexpr int kIndexed256[256][3] = {
    // 0..15 standard ANSI (overridden by scheme)
    {0,0,0},        {128,0,0},      {0,128,0},      {128,128,0},
    {0,0,128},      {128,0,128},    {0,128,128},    {192,192,192},
    {128,128,128},  {255,0,0},      {0,255,0},      {255,255,0},
    {0,0,255},      {255,0,255},    {0,255,255},    {255,255,255},
    // 16..231: 6×6×6 color cube
#define CUBE(i) { ((i / 36) % 6) * 51, ((i / 6) % 6) * 51, (i % 6) * 51 }
    CUBE(0),  CUBE(1),  CUBE(2),  CUBE(3),  CUBE(4),  CUBE(5),
    CUBE(6),  CUBE(7),  CUBE(8),  CUBE(9),  CUBE(10), CUBE(11),
    CUBE(12), CUBE(13), CUBE(14), CUBE(15), CUBE(16), CUBE(17),
    CUBE(18), CUBE(19), CUBE(20), CUBE(21), CUBE(22), CUBE(23),
    CUBE(24), CUBE(25), CUBE(26), CUBE(27), CUBE(28), CUBE(29),
    CUBE(30), CUBE(31), CUBE(32), CUBE(33), CUBE(34), CUBE(35),
    CUBE(36), CUBE(37), CUBE(38), CUBE(39), CUBE(40), CUBE(41),
    CUBE(42), CUBE(43), CUBE(44), CUBE(45), CUBE(46), CUBE(47),
    CUBE(48), CUBE(49), CUBE(50), CUBE(51), CUBE(52), CUBE(53),
    CUBE(54), CUBE(55), CUBE(56), CUBE(57), CUBE(58), CUBE(59),
    CUBE(60), CUBE(61), CUBE(62), CUBE(63), CUBE(64), CUBE(65),
    CUBE(66), CUBE(67), CUBE(68), CUBE(69), CUBE(70), CUBE(71),
    CUBE(72), CUBE(73), CUBE(74), CUBE(75), CUBE(76), CUBE(77),
    CUBE(78), CUBE(79), CUBE(80), CUBE(81), CUBE(82), CUBE(83),
    CUBE(84), CUBE(85), CUBE(86), CUBE(87), CUBE(88), CUBE(89),
    CUBE(90), CUBE(91), CUBE(92), CUBE(93), CUBE(94), CUBE(95),
    CUBE(96), CUBE(97), CUBE(98), CUBE(99), CUBE(100),CUBE(101),
    CUBE(102),CUBE(103),CUBE(104),CUBE(105),CUBE(106),CUBE(107),
    CUBE(108),CUBE(109),CUBE(110),CUBE(111),CUBE(112),CUBE(113),
    CUBE(114),CUBE(115),CUBE(116),CUBE(117),CUBE(118),CUBE(119),
    CUBE(120),CUBE(121),CUBE(122),CUBE(123),CUBE(124),CUBE(125),
    CUBE(126),CUBE(127),CUBE(128),CUBE(129),CUBE(130),CUBE(131),
    CUBE(132),CUBE(133),CUBE(134),CUBE(135),CUBE(136),CUBE(137),
    CUBE(138),CUBE(139),CUBE(140),CUBE(141),CUBE(142),CUBE(143),
    CUBE(144),CUBE(145),CUBE(146),CUBE(147),CUBE(148),CUBE(149),
    CUBE(150),CUBE(151),CUBE(152),CUBE(153),CUBE(154),CUBE(155),
    CUBE(156),CUBE(157),CUBE(158),CUBE(159),CUBE(160),CUBE(161),
    CUBE(162),CUBE(163),CUBE(164),CUBE(165),CUBE(166),CUBE(167),
    CUBE(168),CUBE(169),CUBE(170),CUBE(171),CUBE(172),CUBE(173),
    CUBE(174),CUBE(175),CUBE(176),CUBE(177),CUBE(178),CUBE(179),
    CUBE(180),CUBE(181),CUBE(182),CUBE(183),CUBE(184),CUBE(185),
    CUBE(186),CUBE(187),CUBE(188),CUBE(189),CUBE(190),CUBE(191),
    CUBE(192),CUBE(193),CUBE(194),CUBE(195),CUBE(196),CUBE(197),
    CUBE(198),CUBE(199),CUBE(200),CUBE(201),CUBE(202),CUBE(203),
    CUBE(204),CUBE(205),CUBE(206),CUBE(207),CUBE(208),CUBE(209),
    CUBE(210),CUBE(211),CUBE(212),CUBE(213),CUBE(214),CUBE(215),
#undef CUBE
    // 232..255 grayscale ramp
#define GRAY(i) { 8 + (i) * 10, 8 + (i) * 10, 8 + (i) * 10 }
    GRAY(0),  GRAY(1),  GRAY(2),  GRAY(3),  GRAY(4),  GRAY(5),
    GRAY(6),  GRAY(7),  GRAY(8),  GRAY(9),  GRAY(10), GRAY(11),
    GRAY(12), GRAY(13), GRAY(14), GRAY(15), GRAY(16), GRAY(17),
    GRAY(18), GRAY(19), GRAY(20), GRAY(21), GRAY(22), GRAY(23),
#undef GRAY
};

}  // namespace

const ColorScheme& ColorPalette::dracula() {
    static const ColorScheme s {
        "dracula", "Dracula",
        QColor("#282A36"), QColor("#F8F8F2"), QColor("#FF79C6"), QColor("#44475A"),
        {{
            QColor("#21222C"), QColor("#FF5555"), QColor("#50FA7B"), QColor("#F1FA8C"),
            QColor("#BD93F9"), QColor("#FF79C6"), QColor("#8BE9FD"), QColor("#F8F8F2"),
            QColor("#6272A4"), QColor("#FF6E6E"), QColor("#69FF94"), QColor("#FFFFA5"),
            QColor("#D6ACFF"), QColor("#FF92DF"), QColor("#A4FFFF"), QColor("#FFFFFF")
        }}
    };
    return s;
}

const ColorScheme& ColorPalette::solarizedDark() {
    static const ColorScheme s {
        "solarized-dark", "Solarized Dark",
        QColor("#002B36"), QColor("#839496"), QColor("#93A1A1"), QColor("#073642"),
        {{
            QColor("#073642"), QColor("#DC322F"), QColor("#859900"), QColor("#B58900"),
            QColor("#268BD2"), QColor("#D33682"), QColor("#2AA198"), QColor("#EEE8D5"),
            QColor("#586E75"), QColor("#CB4B16"), QColor("#586E75"), QColor("#657B83"),
            QColor("#839496"), QColor("#6C71C4"), QColor("#93A1A1"), QColor("#FDF6E3")
        }}
    };
    return s;
}

const ColorScheme& ColorPalette::tokyoNight() {
    static const ColorScheme s {
        "tokyo-night", "Tokyo Night",
        QColor("#1A1B26"), QColor("#A9B1D6"), QColor("#C0CAF5"), QColor("#2C3151"),
        {{
            QColor("#15161E"), QColor("#F7768E"), QColor("#9ECE6A"), QColor("#E0AF68"),
            QColor("#7AA2F7"), QColor("#BB9AF7"), QColor("#7DCFFF"), QColor("#A9B1D6"),
            QColor("#414868"), QColor("#F7768E"), QColor("#9ECE6A"), QColor("#E0AF68"),
            QColor("#7AA2F7"), QColor("#BB9AF7"), QColor("#7DCFFF"), QColor("#C0CAF5")
        }}
    };
    return s;
}

const ColorScheme& ColorPalette::nord() {
    static const ColorScheme s {
        "nord", "Nord",
        QColor("#2E3440"), QColor("#D8DEE9"), QColor("#88C0D0"), QColor("#434C5E"),
        {{
            QColor("#3B4252"), QColor("#BF616A"), QColor("#A3BE8C"), QColor("#EBCB8B"),
            QColor("#81A1C1"), QColor("#B48EAD"), QColor("#88C0D0"), QColor("#E5E9F0"),
            QColor("#4C566A"), QColor("#BF616A"), QColor("#A3BE8C"), QColor("#EBCB8B"),
            QColor("#81A1C1"), QColor("#B48EAD"), QColor("#8FBCBB"), QColor("#ECEFF4")
        }}
    };
    return s;
}

const ColorScheme& ColorPalette::oneDark() {
    static const ColorScheme s {
        "one-dark", "One Dark",
        QColor("#282C34"), QColor("#ABB2BF"), QColor("#528BFF"), QColor("#3E4451"),
        {{
            QColor("#000000"), QColor("#E06C75"), QColor("#98C379"), QColor("#E5C07B"),
            QColor("#61AFEF"), QColor("#C678DD"), QColor("#56B6C2"), QColor("#ABB2BF"),
            QColor("#5C6370"), QColor("#E06C75"), QColor("#98C379"), QColor("#E5C07B"),
            QColor("#61AFEF"), QColor("#C678DD"), QColor("#56B6C2"), QColor("#FFFFFF")
        }}
    };
    return s;
}

const ColorScheme& ColorPalette::gruvboxDark() {
    static const ColorScheme s {
        "gruvbox-dark", "Gruvbox Dark",
        QColor("#282828"), QColor("#EBDBB2"), QColor("#FE8019"), QColor("#3C3836"),
        {{
            QColor("#282828"), QColor("#CC241D"), QColor("#98971A"), QColor("#D79921"),
            QColor("#458588"), QColor("#B16286"), QColor("#689D6A"), QColor("#A89984"),
            QColor("#928374"), QColor("#FB4934"), QColor("#B8BB26"), QColor("#FABD2F"),
            QColor("#83A598"), QColor("#D3869B"), QColor("#8EC07C"), QColor("#EBDBB2")
        }}
    };
    return s;
}

const ColorScheme& ColorPalette::monokai() {
    static const ColorScheme s {
        "monokai", "Monokai",
        QColor("#272822"), QColor("#F8F8F2"), QColor("#F8F8F0"), QColor("#49483E"),
        {{
            QColor("#272822"), QColor("#F92672"), QColor("#A6E22E"), QColor("#F4BF75"),
            QColor("#66D9EF"), QColor("#AE81FF"), QColor("#A1EFE4"), QColor("#F8F8F2"),
            QColor("#75715E"), QColor("#F92672"), QColor("#A6E22E"), QColor("#F4BF75"),
            QColor("#66D9EF"), QColor("#AE81FF"), QColor("#A1EFE4"), QColor("#F9F8F5")
        }}
    };
    return s;
}

const ColorScheme& ColorPalette::catppuccin() {
    static const ColorScheme s {
        "catppuccin-mocha", "Catppuccin Mocha",
        QColor("#1E1E2E"), QColor("#CDD6F4"), QColor("#F5E0DC"), QColor("#45475A"),
        {{
            QColor("#45475A"), QColor("#F38BA8"), QColor("#A6E3A1"), QColor("#F9E2AF"),
            QColor("#89B4FA"), QColor("#F5C2E7"), QColor("#94E2D5"), QColor("#BAC2DE"),
            QColor("#585B70"), QColor("#F38BA8"), QColor("#A6E3A1"), QColor("#F9E2AF"),
            QColor("#89B4FA"), QColor("#F5C2E7"), QColor("#94E2D5"), QColor("#A6ADC8")
        }}
    };
    return s;
}

const ColorScheme& ColorPalette::githubDark() {
    static const ColorScheme s {
        "github-dark", "GitHub Dark",
        QColor("#0D1117"), QColor("#C9D1D9"), QColor("#58A6FF"), QColor("#30363D"),
        {{
            QColor("#484F58"), QColor("#FF7B72"), QColor("#3FB950"), QColor("#D29922"),
            QColor("#58A6FF"), QColor("#BC8CFF"), QColor("#39C5CF"), QColor("#B1BAC4"),
            QColor("#6E7681"), QColor("#FFA198"), QColor("#56D364"), QColor("#E3B341"),
            QColor("#79C0FF"), QColor("#D2A8FF"), QColor("#56D4DD"), QColor("#F0F6FC")
        }}
    };
    return s;
}

const ColorScheme& ColorPalette::materialDark() {
    static const ColorScheme s {
        "material-dark", "Material Dark",
        QColor("#212121"), QColor("#EEFFFF"), QColor("#FFCB6B"), QColor("#414141"),
        {{
            QColor("#000000"), QColor("#F07178"), QColor("#C3E88D"), QColor("#FFCB6B"),
            QColor("#82AAFF"), QColor("#C792EA"), QColor("#89DDFF"), QColor("#EEFFFF"),
            QColor("#545454"), QColor("#FF5370"), QColor("#C3E88D"), QColor("#FFCB6B"),
            QColor("#82AAFF"), QColor("#C792EA"), QColor("#89DDFF"), QColor("#FFFFFF")
        }}
    };
    return s;
}

const ColorScheme& ColorPalette::byId(const QString& id) {
    if (id == "solarized-dark")  return solarizedDark();
    if (id == "tokyo-night")     return tokyoNight();
    if (id == "nord")            return nord();
    if (id == "one-dark")        return oneDark();
    if (id == "gruvbox-dark")    return gruvboxDark();
    if (id == "monokai")         return monokai();
    if (id == "catppuccin-mocha")return catppuccin();
    if (id == "github-dark")     return githubDark();
    if (id == "material-dark")   return materialDark();
    return dracula();
}

QColor ColorPalette::resolve(const core::AnsiColor& c,
                             const ColorScheme& scheme,
                             bool isBackground) {
    switch (c.mode) {
    case core::AnsiColor::Default:
        return isBackground ? scheme.background : scheme.foreground;
    case core::AnsiColor::Indexed:
        if (c.r < 16) return scheme.ansi[c.r];
        return QColor(kIndexed256[c.r][0], kIndexed256[c.r][1], kIndexed256[c.r][2]);
    case core::AnsiColor::Rgb:
        return QColor(c.r, c.g, c.b);
    }
    return scheme.foreground;
}

}  // namespace dante::terminal
