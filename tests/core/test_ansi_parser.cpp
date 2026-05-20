#include <QTest>
#include <QString>
#include <vector>

#include "core/parsing/AnsiParser.h"

class TestAnsiParser : public QObject {
    Q_OBJECT
private slots:
    void prints_plain_ascii();
    void handles_control_chars();
    void parses_sgr_color_basic();
    void parses_sgr_truecolor();
    void parses_csi_cursor_move();
    void parses_osc_title();
};

namespace {
std::vector<std::byte> bytesOf(const QByteArray& s) {
    std::vector<std::byte> v;
    v.reserve(s.size());
    for (char c : s) v.push_back(static_cast<std::byte>(c));
    return v;
}
}

void TestAnsiParser::prints_plain_ascii() {
    QString collected;
    dante::core::AnsiParser::Callbacks cb;
    cb.onPrint = [&](char32_t c) { collected.append(QChar(static_cast<uint>(c))); };
    dante::core::AnsiParser p(std::move(cb));

    auto bytes = bytesOf("hello");
    p.feed(std::span<const std::byte>(bytes.data(), bytes.size()));
    QCOMPARE(collected, QStringLiteral("hello"));
}

void TestAnsiParser::handles_control_chars() {
    int crCount = 0, lfCount = 0;
    dante::core::AnsiParser::Callbacks cb;
    cb.onCarriageReturn = [&]() { ++crCount; };
    cb.onLineFeed = [&]() { ++lfCount; };
    dante::core::AnsiParser p(std::move(cb));

    auto bytes = bytesOf("a\r\nb\r\n");
    p.feed(std::span<const std::byte>(bytes.data(), bytes.size()));
    QCOMPARE(crCount, 2);
    QCOMPARE(lfCount, 2);
}

void TestAnsiParser::parses_sgr_color_basic() {
    dante::core::CellAttr lastAttr;
    dante::core::AnsiParser::Callbacks cb;
    cb.onSetGraphicAttr = [&](const dante::core::CellAttr& a) { lastAttr = a; };
    dante::core::AnsiParser p(std::move(cb));

    auto bytes = bytesOf("\x1B[31;1m");  // red + bold
    p.feed(std::span<const std::byte>(bytes.data(), bytes.size()));
    QVERIFY(lastAttr.bold);
    QCOMPARE(lastAttr.fg.mode, dante::core::AnsiColor::Indexed);
    QCOMPARE(lastAttr.fg.r, uint8_t{1});  // 31 -> red(1)
}

void TestAnsiParser::parses_sgr_truecolor() {
    dante::core::CellAttr lastAttr;
    dante::core::AnsiParser::Callbacks cb;
    cb.onSetGraphicAttr = [&](const dante::core::CellAttr& a) { lastAttr = a; };
    dante::core::AnsiParser p(std::move(cb));

    auto bytes = bytesOf("\x1B[38;2;255;128;64m");
    p.feed(std::span<const std::byte>(bytes.data(), bytes.size()));
    QCOMPARE(lastAttr.fg.mode, dante::core::AnsiColor::Rgb);
    QCOMPARE(lastAttr.fg.r, uint8_t{255});
    QCOMPARE(lastAttr.fg.g, uint8_t{128});
    QCOMPARE(lastAttr.fg.b, uint8_t{64});
}

void TestAnsiParser::parses_csi_cursor_move() {
    int row = 0, col = 0;
    dante::core::AnsiParser::Callbacks cb;
    cb.onCursorMove = [&](int r, int c) { row = r; col = c; };
    dante::core::AnsiParser p(std::move(cb));

    auto bytes = bytesOf("\x1B[12;34H");
    p.feed(std::span<const std::byte>(bytes.data(), bytes.size()));
    QCOMPARE(row, 12);
    QCOMPARE(col, 34);
}

void TestAnsiParser::parses_osc_title() {
    QString title;
    dante::core::AnsiParser::Callbacks cb;
    cb.onTitle = [&](const QString& t) { title = t; };
    dante::core::AnsiParser p(std::move(cb));

    auto bytes = bytesOf("\x1B]0;My Title\x07");
    p.feed(std::span<const std::byte>(bytes.data(), bytes.size()));
    QCOMPARE(title, QStringLiteral("My Title"));
}

QTEST_APPLESS_MAIN(TestAnsiParser)
#include "test_ansi_parser.moc"
