#include "TerminalBuffer.h"

namespace dante::terminal {

TerminalBuffer::TerminalBuffer(int cols, int rows, int scrollbackCap)
    : m_cols(cols), m_rows(rows), m_scrollback(scrollbackCap) {
    m_grid.reserve(m_rows);
    for (int i = 0; i < m_rows; ++i) m_grid.append(makeRow());
}

QVector<core::Cell> TerminalBuffer::makeRow() const {
    QVector<core::Cell> r;
    r.resize(m_cols);
    for (auto& c : r) c = core::Cell{};
    return r;
}

void TerminalBuffer::resize(int cols, int rows) {
    if (cols == m_cols && rows == m_rows) return;
    cols = std::max(cols, 1);
    rows = std::max(rows, 1);

    // Resize each row to new column count
    for (auto& row : m_grid) {
        if (row.size() < cols) {
            const int delta = cols - row.size();
            for (int i = 0; i < delta; ++i) row.append(core::Cell{});
        } else if (row.size() > cols) {
            row.resize(cols);
        }
    }

    // Pad rows up
    while (m_grid.size() < rows) m_grid.append(QVector<core::Cell>(cols, core::Cell{}));
    // Push excess into scrollback (from top)
    while (m_grid.size() > rows) {
        m_scrollback.push(m_grid.takeFirst());
    }

    m_cols = cols;
    m_rows = rows;
    if (m_cursorRow >= m_rows) m_cursorRow = m_rows - 1;
    if (m_cursorCol >= m_cols) m_cursorCol = m_cols - 1;
    markDirty();
}

const core::Cell& TerminalBuffer::cell(int row, int col) const {
    static const core::Cell empty{};
    if (row < 0 || row >= m_rows || col < 0 || col >= m_cols) return empty;
    return m_grid[row][col];
}

void TerminalBuffer::wrapIfNeeded() {
    if (m_cursorCol >= m_cols) {
        m_cursorCol = 0;
        ++m_cursorRow;
        if (m_cursorRow >= m_rows) scrollUp();
    }
}

void TerminalBuffer::scrollUp() {
    if (m_grid.isEmpty()) return;
    m_scrollback.push(m_grid.takeFirst());
    m_grid.append(makeRow());
    m_cursorRow = m_rows - 1;
}

void TerminalBuffer::putChar(char32_t ch) {
    wrapIfNeeded();
    core::Cell c;
    c.ch = ch;
    c.attr = m_attr;
    m_grid[m_cursorRow][m_cursorCol] = c;
    ++m_cursorCol;
    markDirty();
}

void TerminalBuffer::lineFeed() {
    ++m_cursorRow;
    if (m_cursorRow >= m_rows) scrollUp();
    markDirty();
}

void TerminalBuffer::carriageReturn() {
    m_cursorCol = 0;
    markDirty();
}

void TerminalBuffer::backspace() {
    if (m_cursorCol > 0) --m_cursorCol;
    markDirty();
}

void TerminalBuffer::tab() {
    const int next = ((m_cursorCol / 8) + 1) * 8;
    m_cursorCol = std::min(next, m_cols - 1);
    markDirty();
}

void TerminalBuffer::cursorAbsolute(int row, int col) {
    m_cursorRow = std::clamp(row - 1, 0, m_rows - 1);
    m_cursorCol = std::clamp(col - 1, 0, m_cols - 1);
    markDirty();
}

void TerminalBuffer::cursorRelative(int dRow, int dCol) {
    m_cursorRow = std::clamp(m_cursorRow + dRow, 0, m_rows - 1);
    m_cursorCol = std::clamp(m_cursorCol + dCol, 0, m_cols - 1);
    markDirty();
}

void TerminalBuffer::eraseDisplay(int mode) {
    auto clearRow = [this](int r, int cFrom, int cTo) {
        for (int c = cFrom; c < cTo; ++c) m_grid[r][c] = core::Cell{};
    };

    if (mode == 0) {
        clearRow(m_cursorRow, m_cursorCol, m_cols);
        for (int r = m_cursorRow + 1; r < m_rows; ++r) clearRow(r, 0, m_cols);
    } else if (mode == 1) {
        for (int r = 0; r < m_cursorRow; ++r) clearRow(r, 0, m_cols);
        clearRow(m_cursorRow, 0, m_cursorCol + 1);
    } else if (mode == 2 || mode == 3) {
        for (int r = 0; r < m_rows; ++r) clearRow(r, 0, m_cols);
        if (mode == 3) m_scrollback.clear();
    }
    markDirty();
}

void TerminalBuffer::eraseLine(int mode) {
    if (mode == 0) {
        for (int c = m_cursorCol; c < m_cols; ++c) m_grid[m_cursorRow][c] = core::Cell{};
    } else if (mode == 1) {
        for (int c = 0; c <= m_cursorCol; ++c) m_grid[m_cursorRow][c] = core::Cell{};
    } else {
        for (int c = 0; c < m_cols; ++c) m_grid[m_cursorRow][c] = core::Cell{};
    }
    markDirty();
}

void TerminalBuffer::setAttr(const core::CellAttr& attr) { m_attr = attr; }

}  // namespace dante::terminal
