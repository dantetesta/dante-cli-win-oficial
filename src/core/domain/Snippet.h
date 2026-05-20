#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

#include "core/util/Uuid.h"

namespace dante::core {

struct Snippet {
    Id id;
    QString name;
    QString command;
    QStringList tags;
    QString emoji;
    QDateTime createdAt;
};

}  // namespace dante::core
