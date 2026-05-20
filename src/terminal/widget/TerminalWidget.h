#pragma once

#include <QFont>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QWidget>
#include <memory>

#include "core/parsing/AnsiParser.h"
#include "core/util/Uuid.h"
#include "platform/pty/IPtyBackend.h"
#include "terminal/engine/ColorPalette.h"
#include "terminal/engine/TerminalBuffer.h"

namespace dante::terminal {

class TerminalWidget : public QWidget {
    Q_OBJECT
public:
    explicit TerminalWidget(core::Id sessionId, QWidget* parent = nullptr);
    ~TerminalWidget() override;

    void attachBackend(std::shared_ptr<platform::IPtyBackend> backend);
    void setColorScheme(const QString& schemeId);
    void setFontFamily(const QString& family);
    void setFontSize(int pt);

    core::Id sessionId() const { return m_sessionId; }
    const QString& currentCwd() const { return m_cwd; }
    const QString& title() const { return m_title; }

signals:
    void titleChanged(const QString& title);
    void cwdChanged(const QString& cwd);
    void bell();

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;

private slots:
    void onOutput(const QByteArray& bytes);
    void onExited(int code);

private:
    void recomputeCellMetrics();
    void scheduleRepaint();
    void sendToShell(const QByteArray& data);
    void resizePty();

    core::Id m_sessionId;
    std::shared_ptr<platform::IPtyBackend> m_backend;
    std::unique_ptr<core::AnsiParser> m_parser;
    std::unique_ptr<TerminalBuffer> m_buffer;
    ColorScheme m_scheme;
    QFont m_font;
    int m_cellWidth{8};
    int m_cellHeight{16};
    int m_lastRevision{-1};
    QTimer m_paintTimer;
    QString m_title;
    QString m_cwd;
    int m_scrollOffset{0};  // 0 = follow tail; positive = scrolled up
    bool m_focused{false};
};

}  // namespace dante::terminal
