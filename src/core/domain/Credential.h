#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

#include "core/util/Uuid.h"

namespace dante::core {

enum class CredentialKind {
    SSH    = 0,
    FTP    = 1,
    API    = 2,
    Custom = 3,
};

struct CredentialField {
    QString label;
    QString value;
    bool sensitive{false};
};

struct Credential {
    Id id;
    QString name;
    CredentialKind kind{CredentialKind::Custom};
    QVector<CredentialField> fields;
    QString notes;
    QStringList tags;
    QString emoji;
    QDateTime createdAt;
};

}  // namespace dante::core
