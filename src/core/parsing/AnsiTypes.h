#pragma once

#include <cstdint>

namespace dante::core {

// Compact 24-bit RGB color with sentinel for "default".
struct AnsiColor {
    enum Mode : uint8_t { Default = 0, Indexed = 1, Rgb = 2 };
    Mode mode{Default};
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};

    static AnsiColor indexed(uint8_t idx) {
        AnsiColor c; c.mode = Indexed; c.r = idx; return c;
    }
    static AnsiColor rgb(uint8_t r, uint8_t g, uint8_t b) {
        AnsiColor c; c.mode = Rgb; c.r = r; c.g = g; c.b = b; return c;
    }
};

struct CellAttr {
    AnsiColor fg{};
    AnsiColor bg{};
    bool bold{false};
    bool italic{false};
    bool underline{false};
    bool reverse{false};
    bool dim{false};
    bool strikethrough{false};
};

struct Cell {
    char32_t ch{U' '};
    CellAttr attr;
};

}  // namespace dante::core
