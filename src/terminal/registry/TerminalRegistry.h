#pragma once

#include <QHash>
#include <QObject>
#include <memory>

#include "core/util/Uuid.h"
#include "platform/pty/IPtyBackend.h"
#include "terminal/widget/TerminalWidget.h"

namespace dante::terminal {

// One terminal widget per session id, lives across UI reparenting events
// (cf. lesson §3.4.1 — UI churn must NOT destroy the PTY).
class TerminalRegistry : public QObject {
    Q_OBJECT
public:
    static TerminalRegistry& instance();

    // Returns existing widget if any, or creates one (without attaching).
    TerminalWidget* ensure(const core::Id& sessionId);

    void attachBackend(const core::Id& sessionId,
                       std::shared_ptr<platform::IPtyBackend> backend);

    void destroy(const core::Id& sessionId);

private:
    TerminalRegistry();
    QHash<core::Id, TerminalWidget*> m_widgets;
    QHash<core::Id, std::shared_ptr<platform::IPtyBackend>> m_backends;
};

}  // namespace dante::terminal
