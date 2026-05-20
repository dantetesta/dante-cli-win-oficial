#pragma once

#include <QString>
#include <QUuid>

namespace dante::core {

using Id = QUuid;

inline Id newId() { return QUuid::createUuid(); }

inline QString idToString(const Id& id) {
    return id.toString(QUuid::WithoutBraces);
}

inline Id idFromString(const QString& s) {
    return QUuid::fromString(s);
}

}  // namespace dante::core
