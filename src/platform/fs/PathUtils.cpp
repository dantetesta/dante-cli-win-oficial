#include "PathUtils.h"

#include <QDir>
#include <QStandardPaths>

namespace dante::platform {

QString PathUtils::toLongPath(const QString& path) {
    if (path.isEmpty()) return path;
    if (path.startsWith(QStringLiteral("\\\\?\\"))) return path;
    if (path.length() < 248) return path;
    if (path.startsWith(QStringLiteral("\\\\"))) {
        // UNC path → \\?\UNC\server\share\...
        return QStringLiteral("\\\\?\\UNC\\") + path.mid(2);
    }
    return QStringLiteral("\\\\?\\") + QDir::toNativeSeparators(path);
}

QString PathUtils::appDataDir() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(base);
    return base;
}

QString PathUtils::logsDir() {
    const QString d = appDataDir() + QStringLiteral("/logs");
    QDir().mkpath(d);
    return d;
}

QString PathUtils::shellQuote(const QString& path) {
    if (path.isEmpty()) return QStringLiteral("\"\"");
    if (!path.contains(' ') && !path.contains('"') && !path.contains('\t')) return path;
    QString out = "\"";
    for (QChar c : path) {
        if (c == '"') out.append('\\');
        out.append(c);
    }
    out.append('"');
    return out;
}

}  // namespace dante::platform
