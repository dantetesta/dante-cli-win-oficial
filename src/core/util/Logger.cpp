#include "Logger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QTextStream>

namespace dante::core {

Q_LOGGING_CATEGORY(lcApp,  "dante.app")
Q_LOGGING_CATEGORY(lcPty,  "dante.pty")
Q_LOGGING_CATEGORY(lcTerm, "dante.term")
Q_LOGGING_CATEGORY(lcDb,   "dante.db")
Q_LOGGING_CATEGORY(lcUi,   "dante.ui")

namespace {

QFile g_logFile;
QMutex g_logMutex;

void messageHandler(QtMsgType type, const QMessageLogContext& ctx, const QString& msg) {
    QMutexLocker lock(&g_logMutex);
    if (!g_logFile.isOpen()) return;

    QString level;
    switch (type) {
        case QtDebugMsg:    level = "DBG"; break;
        case QtInfoMsg:     level = "INF"; break;
        case QtWarningMsg:  level = "WRN"; break;
        case QtCriticalMsg: level = "ERR"; break;
        case QtFatalMsg:    level = "FTL"; break;
    }
    const QString line = QStringLiteral("[%1] [%2] [%3] %4\n")
        .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs))
        .arg(level)
        .arg(QString::fromLatin1(ctx.category ? ctx.category : "default"))
        .arg(msg);

    QTextStream out(&g_logFile);
    out << line;
    out.flush();
    fprintf(stderr, "%s", qPrintable(line));
}

}  // namespace

void installFileLogger(const QString& logDir) {
    QDir().mkpath(logDir);
    const QString fileName = QStringLiteral("%1/dante-%2.log")
        .arg(logDir, QDateTime::currentDateTime().toString("yyyyMMdd"));

    g_logFile.setFileName(fileName);
    g_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    qInstallMessageHandler(messageHandler);
}

}  // namespace dante::core
