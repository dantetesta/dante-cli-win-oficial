#include "TerminalRegistry.h"

namespace dante::terminal {

TerminalRegistry& TerminalRegistry::instance() {
    static TerminalRegistry r;
    return r;
}

TerminalRegistry::TerminalRegistry() = default;

TerminalWidget* TerminalRegistry::ensure(const core::Id& sessionId) {
    auto it = m_widgets.find(sessionId);
    if (it != m_widgets.end()) return it.value();
    auto* w = new TerminalWidget(sessionId);
    m_widgets.insert(sessionId, w);
    return w;
}

void TerminalRegistry::attachBackend(const core::Id& sessionId,
                                     std::shared_ptr<platform::IPtyBackend> backend) {
    m_backends.insert(sessionId, backend);
    if (auto* w = ensure(sessionId)) {
        w->attachBackend(backend);
    }
}

void TerminalRegistry::destroy(const core::Id& sessionId) {
    if (auto it = m_widgets.find(sessionId); it != m_widgets.end()) {
        it.value()->deleteLater();
        m_widgets.erase(it);
    }
    m_backends.remove(sessionId);
}

}  // namespace dante::terminal
