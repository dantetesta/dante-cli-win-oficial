#include <QTest>

// We test buffer behaviour by including the source files directly to avoid
// needing the dante::terminal library here (which depends on Qt Widgets).
#include "core/parsing/AnsiTypes.h"

class TestTerminalBufferStub : public QObject {
    Q_OBJECT
private slots:
    void cell_default_is_space();
};

void TestTerminalBufferStub::cell_default_is_space() {
    dante::core::Cell c;
    QCOMPARE(c.ch, char32_t{U' '});
    QCOMPARE(c.attr.bold, false);
}

QTEST_APPLESS_MAIN(TestTerminalBufferStub)
#include "test_terminal_buffer.moc"
