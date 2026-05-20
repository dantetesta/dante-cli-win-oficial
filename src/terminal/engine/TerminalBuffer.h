#pragma once

#include <QString>
#include <QVector>

#include "core/parsing/AnsiTypes.h"
#include "terminal/engine/Scrollback.h"

namespace dante::terminal {

// Rectangular grid of cells representing the visible terminal.
// Anything that scrolls off the top is pushed into the Scrollback ring.
class TerminalBuffer {
public:
    TerminalBuffer(int cols = 120, int rows = 30, int scrollbackCap = 50000);

    void resize(int cols, int rows);

    int cols() const { return m_cols; }
    int rows() const { return m_rows; }
    int cursorRow() const { return m_cursorRow; }
    int cursorCol() const { return m_cursorCol; }
    int dirtyRevision() const { return m_revision; }

    const core::Cell& cell(int row, int col) const;
    void putChar(char32_t ch);
    void lineFeed();
    void carriageReturn();
    void backspace();
    void tab();
    void cursorAbsolute(int row, int col);
    void cursorRelative(int dRow, int dCol);
    void eraseDisplay(int mode);  // 0=below, 1=above, 2=all, 3=all+scrollback
    void eraseLine(int mode);     // 0=right of cursor, 1=left, 2=whole
    void setAttr(const core::CellAttr& attr);

    const Scrollback& scrollback() const { return m_scrollback; }

private:
    void markDirty() { ++m_revision; }
    void wrapIfNeeded();
    QVector<core::Cell> makeRow() const;
    void scrollUp();

    int m_cols{120};
    int m_rows{30};
    int m_cursorRow{0};
    int m_cursorCol{0};
    core::CellAttr m_attr;
    QVector<QVector<core::Cell>> m_grid;
    Scrollback m_scrollback;
    int m_revision{0};
};

}  // namespace dante::terminal
