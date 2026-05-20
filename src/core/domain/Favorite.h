#pragma once

#include <QColor>
#include <QDateTime>
#include <QString>
#include <QStringList>

#include "core/util/Uuid.h"

namespace dante::core {

struct Favorite {
    Id id;
    QString name;
    QString path;
    QStringList tags;
    QColor color{0x4F, 0x9D, 0xFF};
    QString emoji;
    QString initialCommand;
    QDateTime createdAt;
};

}  // namespace dante::core
