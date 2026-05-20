#pragma once

#include <QObject>
#include <QByteArray>

#include "core/util/Uuid.h"

namespace dante::platform {

// Forwarder QObject so PTY backends can emit Qt signals from worker threads
// via QueuedConnection — keeps Core free of QObject inheritance.
class PtyEvents : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;

signals:
    void output(QByteArray bytes);
    void exited(int exitCode);
    void error(QString message);
};

}  // namespace dante::platform
