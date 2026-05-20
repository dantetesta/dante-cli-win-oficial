#pragma once

#include <QString>
#include <functional>
#include <span>
#include <string>
#include <vector>

#include "AnsiTypes.h"

namespace dante::core {

// Table-driven VT100/VT220/xterm-256color state machine.
// Reference: https://vt100.net/emu/dec_ansi_parser
class AnsiParser {
public:
    struct Callbacks {
        std::function<void(char32_t)>                    onPrint;
        std::function<void()>                            onLineFeed;
        std::function<void()>                            onCarriageReturn;
        std::function<void()>                            onBackspace;
        std::function<void()>                            onTab;
        std::function<void()>                            onBell;
        std::function<void(int row, int col)>            onCursorMove;
        std::function<void(int n)>                       onCursorUp;
        std::function<void(int n)>                       onCursorDown;
        std::function<void(int n)>                       onCursorForward;
        std::function<void(int n)>                       onCursorBack;
        std::function<void(int mode)>                    onEraseDisplay;  // 0,1,2,3
        std::function<void(int mode)>                    onEraseLine;     // 0,1,2
        std::function<void(const CellAttr& attr)>        onSetGraphicAttr;
        std::function<void(int code, const QString&)>    onOsc;           // OSC code + payload
        std::function<void(const QString&)>              onTitle;         // shorthand for OSC 0/1/2
        std::function<void(const QString& cwd)>          onCwdChanged;    // OSC 7
    };

    explicit AnsiParser(Callbacks cb);

    void feed(std::span<const std::byte> bytes);
    void reset();

private:
    enum class State {
        Ground,
        Escape,
        CsiEntry,
        CsiParam,
        CsiIntermediate,
        OscString,
    };

    void handleControl(uint8_t c);
    void handleCsi(uint8_t finalByte);
    void handleOsc();
    void executeSgr(const std::vector<int>& params);

    Callbacks m_cb;
    State m_state{State::Ground};
    std::string m_paramBuf;
    std::string m_oscBuf;
    CellAttr m_currentAttr;

    // UTF-8 decoding scratchpad
    uint32_t m_utf8Code{0};
    int m_utf8Remaining{0};
};

}  // namespace dante::core
