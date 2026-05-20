#pragma once

#include <QVector>
#include <deque>

#include "core/parsing/AnsiTypes.h"

namespace dante::terminal {

// Bounded FIFO of completed terminal lines.
class Scrollback {
public:
    explicit Scrollback(int capacity = 50000) : m_capacity(capacity) {}

    void push(QVector<core::Cell> line) {
        m_lines.push_back(std::move(line));
        while (static_cast<int>(m_lines.size()) > m_capacity) m_lines.pop_front();
    }

    int size() const { return static_cast<int>(m_lines.size()); }
    int capacity() const { return m_capacity; }
    void clear() { m_lines.clear(); }

    const QVector<core::Cell>& line(int idx) const { return m_lines[idx]; }

private:
    std::deque<QVector<core::Cell>> m_lines;
    int m_capacity{50000};
};

}  // namespace dante::terminal
