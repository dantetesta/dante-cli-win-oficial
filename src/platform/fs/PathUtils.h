#pragma once

#include <QString>

namespace dante::platform {

class PathUtils {
public:
    // Normalises a Windows path to its long-path form when it exceeds MAX_PATH.
    static QString toLongPath(const QString& path);

    // %APPDATA%\Dante CLI
    static QString appDataDir();

    // %APPDATA%\Dante CLI\logs
    static QString logsDir();

    // Quote a path for safe shell injection (handles spaces and quotes).
    static QString shellQuote(const QString& path);
};

}  // namespace dante::platform
