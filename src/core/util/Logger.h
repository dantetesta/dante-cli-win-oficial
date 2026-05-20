#pragma once

#include <QLoggingCategory>
#include <QString>

namespace dante::core {

Q_DECLARE_LOGGING_CATEGORY(lcApp)
Q_DECLARE_LOGGING_CATEGORY(lcPty)
Q_DECLARE_LOGGING_CATEGORY(lcTerm)
Q_DECLARE_LOGGING_CATEGORY(lcDb)
Q_DECLARE_LOGGING_CATEGORY(lcUi)

void installFileLogger(const QString& logDir);

}  // namespace dante::core
