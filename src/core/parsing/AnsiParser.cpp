#include "AnsiParser.h"

#include <QStringList>
#include <cstdint>

namespace dante::core {

namespace {

constexpr uint8_t kBell  = 0x07;
constexpr uint8_t kBS    = 0x08;
constexpr uint8_t kTab   = 0x09;
constexpr uint8_t kLF    = 0x0A;
constexpr uint8_t kCR    = 0x0D;
constexpr uint8_t kESC   = 0x1B;
constexpr uint8_t kST    = 0x9C;  // String Terminator (rare in 8-bit mode)

std::vector<int> parseParams(const std::string& s) {
    std::vector<int> out;
    if (s.empty()) return out;
    int current = 0;
    bool any = false;
    for (char ch : s) {
        if (ch >= '0' && ch <= '9') {
            current = current * 10 + (ch - '0');
            any = true;
        } else if (ch == ';') {
            out.push_back(any ? current : 0);
            current = 0;
            any = false;
        }
    }
    out.push_back(any ? current : 0);
    return out;
}

}  // namespace

AnsiParser::AnsiParser(Callbacks cb) : m_cb(std::move(cb)) {}

void AnsiParser::reset() {
    m_state = State::Ground;
    m_paramBuf.clear();
    m_oscBuf.clear();
    m_utf8Code = 0;
    m_utf8Remaining = 0;
}

void AnsiParser::feed(std::span<const std::byte> bytes) {
    for (auto b : bytes) {
        const uint8_t c = static_cast<uint8_t>(b);

        // OSC payload: collect until BEL or ESC \
        if (m_state == State::OscString) {
            if (c == kBell || c == kST) {
                handleOsc();
                m_state = State::Ground;
                m_oscBuf.clear();
            } else if (c == kESC) {
                // wait for following '\' (ST)
                m_state = State::Escape;  // will dispatch on next byte
            } else {
                m_oscBuf.push_back(static_cast<char>(c));
            }
            continue;
        }

        switch (m_state) {
        case State::Ground:
            if (c < 0x20) {
                handleControl(c);
            } else if (c == 0x7F) {
                // DEL — ignore
            } else if (c < 0x80) {
                if (m_cb.onPrint) m_cb.onPrint(static_cast<char32_t>(c));
            } else {
                // UTF-8 multibyte start
                if ((c & 0xE0) == 0xC0) { m_utf8Code = c & 0x1F; m_utf8Remaining = 1; }
                else if ((c & 0xF0) == 0xE0) { m_utf8Code = c & 0x0F; m_utf8Remaining = 2; }
                else if ((c & 0xF8) == 0xF0) { m_utf8Code = c & 0x07; m_utf8Remaining = 3; }
                else if ((c & 0xC0) == 0x80) {
                    // continuation
                    m_utf8Code = (m_utf8Code << 6) | (c & 0x3F);
                    if (--m_utf8Remaining <= 0 && m_cb.onPrint) {
                        m_cb.onPrint(static_cast<char32_t>(m_utf8Code));
                        m_utf8Code = 0;
                    }
                }
            }
            break;

        case State::Escape:
            if (c == '[')      { m_state = State::CsiEntry; m_paramBuf.clear(); }
            else if (c == ']') { m_state = State::OscString; m_oscBuf.clear(); }
            else if (c == '\\') {
                // ST end of OSC handled by collecting; here it's a no-op fall-through
                m_state = State::Ground;
            } else {
                // Two-byte escapes we don't model: just return to ground
                m_state = State::Ground;
            }
            break;

        case State::CsiEntry:
        case State::CsiParam:
            if (c >= '0' && c <= '9') {
                m_paramBuf.push_back(static_cast<char>(c));
                m_state = State::CsiParam;
            } else if (c == ';') {
                m_paramBuf.push_back(';');
                m_state = State::CsiParam;
            } else if (c >= 0x20 && c <= 0x2F) {
                m_state = State::CsiIntermediate;
            } else if (c >= 0x40 && c <= 0x7E) {
                handleCsi(c);
                m_state = State::Ground;
                m_paramBuf.clear();
            } else {
                m_state = State::Ground;
            }
            break;

        case State::CsiIntermediate:
            if (c >= 0x40 && c <= 0x7E) {
                handleCsi(c);
                m_state = State::Ground;
                m_paramBuf.clear();
            }
            break;

        case State::OscString:
            // handled above
            break;
        }
    }
}

void AnsiParser::handleControl(uint8_t c) {
    switch (c) {
    case kESC: m_state = State::Escape; break;
    case kBell: if (m_cb.onBell) m_cb.onBell(); break;
    case kBS:   if (m_cb.onBackspace) m_cb.onBackspace(); break;
    case kTab:  if (m_cb.onTab) m_cb.onTab(); break;
    case kLF:   if (m_cb.onLineFeed) m_cb.onLineFeed(); break;
    case kCR:   if (m_cb.onCarriageReturn) m_cb.onCarriageReturn(); break;
    default: break;
    }
}

void AnsiParser::handleCsi(uint8_t finalByte) {
    const auto params = parseParams(m_paramBuf);
    const auto p = [&](size_t i, int def) {
        return i < params.size() ? std::max(params[i], 0) : def;
    };

    switch (finalByte) {
    case 'A': if (m_cb.onCursorUp)       m_cb.onCursorUp(p(0, 1));      break;
    case 'B': if (m_cb.onCursorDown)     m_cb.onCursorDown(p(0, 1));    break;
    case 'C': if (m_cb.onCursorForward)  m_cb.onCursorForward(p(0, 1)); break;
    case 'D': if (m_cb.onCursorBack)     m_cb.onCursorBack(p(0, 1));    break;
    case 'H':
    case 'f':
        if (m_cb.onCursorMove) m_cb.onCursorMove(p(0, 1), p(1, 1));
        break;
    case 'J': if (m_cb.onEraseDisplay) m_cb.onEraseDisplay(p(0, 0)); break;
    case 'K': if (m_cb.onEraseLine)    m_cb.onEraseLine(p(0, 0));    break;
    case 'm': executeSgr(params); break;
    default: break;  // unhandled CSI — ignore (will be added as needed)
    }
}

void AnsiParser::handleOsc() {
    // Format: <code> ';' <payload>
    const auto sep = m_oscBuf.find(';');
    int code = -1;
    QString payload;
    if (sep != std::string::npos) {
        try { code = std::stoi(m_oscBuf.substr(0, sep)); } catch (...) {}
        payload = QString::fromStdString(m_oscBuf.substr(sep + 1));
    } else {
        try { code = std::stoi(m_oscBuf); } catch (...) {}
    }

    if (m_cb.onOsc) m_cb.onOsc(code, payload);

    if (code == 0 || code == 1 || code == 2) {
        if (m_cb.onTitle) m_cb.onTitle(payload);
    } else if (code == 7) {
        // OSC 7 reports cwd as file:// URL
        QString cwd = payload;
        if (cwd.startsWith("file://")) {
            cwd = cwd.mid(7);
            const int slash = cwd.indexOf('/');
            if (slash >= 0) cwd = cwd.mid(slash);
            // normalise leading slash on Windows-style paths
            if (cwd.size() >= 3 && cwd[0] == '/' && cwd[2] == ':') {
                cwd = cwd.mid(1);
            }
        }
        if (m_cb.onCwdChanged) m_cb.onCwdChanged(cwd);
    }
}

void AnsiParser::executeSgr(const std::vector<int>& params) {
    if (params.empty() || (params.size() == 1 && params[0] == 0)) {
        m_currentAttr = CellAttr{};
        if (m_cb.onSetGraphicAttr) m_cb.onSetGraphicAttr(m_currentAttr);
        return;
    }

    for (size_t i = 0; i < params.size(); ++i) {
        const int code = params[i];
        if (code == 0) m_currentAttr = CellAttr{};
        else if (code == 1)  m_currentAttr.bold = true;
        else if (code == 2)  m_currentAttr.dim = true;
        else if (code == 3)  m_currentAttr.italic = true;
        else if (code == 4)  m_currentAttr.underline = true;
        else if (code == 7)  m_currentAttr.reverse = true;
        else if (code == 9)  m_currentAttr.strikethrough = true;
        else if (code == 22) { m_currentAttr.bold = false; m_currentAttr.dim = false; }
        else if (code == 23) m_currentAttr.italic = false;
        else if (code == 24) m_currentAttr.underline = false;
        else if (code == 27) m_currentAttr.reverse = false;
        else if (code == 29) m_currentAttr.strikethrough = false;
        else if (code >= 30 && code <= 37)
            m_currentAttr.fg = AnsiColor::indexed(static_cast<uint8_t>(code - 30));
        else if (code == 39) m_currentAttr.fg = AnsiColor{};
        else if (code >= 40 && code <= 47)
            m_currentAttr.bg = AnsiColor::indexed(static_cast<uint8_t>(code - 40));
        else if (code == 49) m_currentAttr.bg = AnsiColor{};
        else if (code >= 90 && code <= 97)
            m_currentAttr.fg = AnsiColor::indexed(static_cast<uint8_t>(code - 90 + 8));
        else if (code >= 100 && code <= 107)
            m_currentAttr.bg = AnsiColor::indexed(static_cast<uint8_t>(code - 100 + 8));
        else if (code == 38 || code == 48) {
            // Extended color: 38;5;n (indexed) or 38;2;r;g;b (RGB)
            if (i + 1 >= params.size()) break;
            const int kind = params[i + 1];
            if (kind == 5 && i + 2 < params.size()) {
                const auto c = AnsiColor::indexed(static_cast<uint8_t>(params[i + 2]));
                if (code == 38) m_currentAttr.fg = c; else m_currentAttr.bg = c;
                i += 2;
            } else if (kind == 2 && i + 4 < params.size()) {
                const auto c = AnsiColor::rgb(
                    static_cast<uint8_t>(params[i + 2]),
                    static_cast<uint8_t>(params[i + 3]),
                    static_cast<uint8_t>(params[i + 4]));
                if (code == 38) m_currentAttr.fg = c; else m_currentAttr.bg = c;
                i += 4;
            }
        }
    }

    if (m_cb.onSetGraphicAttr) m_cb.onSetGraphicAttr(m_currentAttr);
}

}  // namespace dante::core
