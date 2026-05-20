#include "TerminalWidget.h"

#include <QFontMetricsF>
#include <QKeyEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QString>
#include <QWheelEvent>

#include "core/util/Logger.h"
#include "platform/pty/PtyEvents.h"

namespace dante::terminal {

TerminalWidget::TerminalWidget(core::Id sessionId, QWidget* parent)
    : QWidget(parent), m_sessionId(sessionId) {
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    m_scheme = ColorPalette::dracula();
    m_buffer = std::make_unique<TerminalBuffer>(120, 30);

    core::AnsiParser::Callbacks cb;
    cb.onPrint           = [this](char32_t c) { m_buffer->putChar(c); };
    cb.onLineFeed        = [this]() { m_buffer->lineFeed(); };
    cb.onCarriageReturn  = [this]() { m_buffer->carriageReturn(); };
    cb.onBackspace       = [this]() { m_buffer->backspace(); };
    cb.onTab             = [this]() { m_buffer->tab(); };
    cb.onBell            = [this]() { emit bell(); };
    cb.onCursorMove      = [this](int r, int c) { m_buffer->cursorAbsolute(r, c); };
    cb.onCursorUp        = [this](int n) { m_buffer->cursorRelative(-n, 0); };
    cb.onCursorDown      = [this](int n) { m_buffer->cursorRelative( n, 0); };
    cb.onCursorForward   = [this](int n) { m_buffer->cursorRelative(0,  n); };
    cb.onCursorBack      = [this](int n) { m_buffer->cursorRelative(0, -n); };
    cb.onEraseDisplay    = [this](int m) { m_buffer->eraseDisplay(m); };
    cb.onEraseLine       = [this](int m) { m_buffer->eraseLine(m); };
    cb.onSetGraphicAttr  = [this](const core::CellAttr& a) { m_buffer->setAttr(a); };
    cb.onTitle           = [this](const QString& t) {
        m_title = t;
        emit titleChanged(t);
    };
    cb.onCwdChanged      = [this](const QString& cwd) {
        m_cwd = cwd;
        emit cwdChanged(cwd);
    };
    m_parser = std::make_unique<core::AnsiParser>(std::move(cb));

    m_font = QFont("Cascadia Code", 11);
    m_font.setStyleStrategy(QFont::PreferAntialias);
    recomputeCellMetrics();

    m_paintTimer.setInterval(16);  // ~60 fps coalesce
    m_paintTimer.setSingleShot(true);
    connect(&m_paintTimer, &QTimer::timeout, this, QOverload<>::of(&QWidget::update));
}

TerminalWidget::~TerminalWidget() = default;

void TerminalWidget::attachBackend(std::shared_ptr<platform::IPtyBackend> backend) {
    m_backend = std::move(backend);

    // Single delivery path: worker thread → setOutputHandler → invokeMethod
    // (QueuedConnection) → onOutput on the UI thread.
    m_backend->setOutputHandler([this](std::span<const std::byte> bytes) {
        QByteArray copy(reinterpret_cast<const char*>(bytes.data()),
                        static_cast<int>(bytes.size()));
        QMetaObject::invokeMethod(this, "onOutput", Qt::QueuedConnection,
                                  Q_ARG(QByteArray, copy));
    });

    resizePty();
}

void TerminalWidget::setColorScheme(const QString& schemeId) {
    m_scheme = ColorPalette::byId(schemeId);
    update();
}

void TerminalWidget::setFontFamily(const QString& family) {
    m_font.setFamily(family);
    recomputeCellMetrics();
    update();
}

void TerminalWidget::setFontSize(int pt) {
    m_font.setPointSize(std::clamp(pt, 9, 28));
    recomputeCellMetrics();
    update();
}

void TerminalWidget::recomputeCellMetrics() {
    QFontMetricsF fm(m_font);
    m_cellWidth = std::max(1, static_cast<int>(fm.horizontalAdvance(QChar('M'))));
    m_cellHeight = std::max(1, static_cast<int>(fm.height()));
}

void TerminalWidget::scheduleRepaint() {
    if (!m_paintTimer.isActive()) m_paintTimer.start();
}

void TerminalWidget::sendToShell(const QByteArray& data) {
    if (!m_backend) return;
    const auto* bytes = reinterpret_cast<const std::byte*>(data.constData());
    m_backend->write(std::span<const std::byte>(bytes, data.size()));
}

void TerminalWidget::resizePty() {
    if (!m_backend) return;
    const int cols = std::max(1, width()  / m_cellWidth);
    const int rows = std::max(1, height() / m_cellHeight);
    m_buffer->resize(cols, rows);
    m_backend->resize(cols, rows);
}

void TerminalWidget::onOutput(const QByteArray& bytes) {
    const auto* p = reinterpret_cast<const std::byte*>(bytes.constData());
    m_parser->feed(std::span<const std::byte>(p, bytes.size()));
    if (m_buffer->dirtyRevision() != m_lastRevision) {
        m_lastRevision = m_buffer->dirtyRevision();
        scheduleRepaint();
    }
}

void TerminalWidget::onExited(int code) {
    qCInfo(core::lcTerm, "session exited code=%d", code);
}

void TerminalWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.fillRect(rect(), m_scheme.background);
    p.setFont(m_font);

    QFontMetricsF fm(m_font);
    const qreal baseline = fm.ascent();
    const int rows = m_buffer->rows();
    const int cols = m_buffer->cols();

    for (int r = 0; r < rows; ++r) {
        const qreal y = r * m_cellHeight;
        for (int c = 0; c < cols; ++c) {
            const auto& cell = m_buffer->cell(r, c);
            const QColor fg = ColorPalette::resolve(cell.attr.fg, m_scheme, false);
            const QColor bg = ColorPalette::resolve(cell.attr.bg, m_scheme, true);

            if (bg != m_scheme.background) {
                p.fillRect(QRectF(c * m_cellWidth, y, m_cellWidth, m_cellHeight), bg);
            }

            if (cell.ch != U' ' && cell.ch != 0) {
                p.setPen(fg);
                QFont f = m_font;
                if (cell.attr.bold)   f.setBold(true);
                if (cell.attr.italic) f.setItalic(true);
                p.setFont(f);

                const QString s = QString::fromUcs4(reinterpret_cast<const char32_t*>(&cell.ch), 1);
                p.drawText(QPointF(c * m_cellWidth, y + baseline), s);
            }
        }
    }

    // Cursor
    if (m_focused) {
        const QRectF cur(m_buffer->cursorCol() * m_cellWidth,
                         m_buffer->cursorRow() * m_cellHeight,
                         m_cellWidth, m_cellHeight);
        p.fillRect(cur, QColor(m_scheme.cursor.red(), m_scheme.cursor.green(),
                               m_scheme.cursor.blue(), 160));
    }
}

void TerminalWidget::keyPressEvent(QKeyEvent* e) {
    if (!m_backend) { QWidget::keyPressEvent(e); return; }

    const int mod = e->modifiers();
    const int key = e->key();

    // Ctrl-C → 0x03 (also a "send signal" semantically)
    if ((mod & Qt::ControlModifier) && key >= Qt::Key_A && key <= Qt::Key_Z) {
        QByteArray b;
        b.append(static_cast<char>(key - Qt::Key_A + 1));
        sendToShell(b);
        return;
    }

    switch (key) {
    case Qt::Key_Return:
    case Qt::Key_Enter:    sendToShell("\r"); return;
    case Qt::Key_Backspace:sendToShell("\x7F"); return;
    case Qt::Key_Tab:      sendToShell("\t"); return;
    case Qt::Key_Escape:   sendToShell("\x1B"); return;
    case Qt::Key_Up:       sendToShell("\x1B[A"); return;
    case Qt::Key_Down:     sendToShell("\x1B[B"); return;
    case Qt::Key_Right:    sendToShell("\x1B[C"); return;
    case Qt::Key_Left:     sendToShell("\x1B[D"); return;
    case Qt::Key_Home:     sendToShell("\x1B[H"); return;
    case Qt::Key_End:      sendToShell("\x1B[F"); return;
    case Qt::Key_PageUp:   sendToShell("\x1B[5~"); return;
    case Qt::Key_PageDown: sendToShell("\x1B[6~"); return;
    case Qt::Key_Delete:   sendToShell("\x1B[3~"); return;
    }

    const QString text = e->text();
    if (!text.isEmpty()) sendToShell(text.toUtf8());
}

void TerminalWidget::inputMethodEvent(QInputMethodEvent* e) {
    const QString commit = e->commitString();
    if (!commit.isEmpty()) sendToShell(commit.toUtf8());
    e->accept();
}

void TerminalWidget::resizeEvent(QResizeEvent* /*event*/) {
    resizePty();
    update();
}

void TerminalWidget::focusInEvent(QFocusEvent*) { m_focused = true; update(); }
void TerminalWidget::focusOutEvent(QFocusEvent*) { m_focused = false; update(); }

void TerminalWidget::wheelEvent(QWheelEvent* e) {
    // Stub: a future revision will scroll through Scrollback.
    Q_UNUSED(e);
}

}  // namespace dante::terminal
