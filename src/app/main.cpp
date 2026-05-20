#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFontDatabase>
#include <QLocale>
#include <QStandardPaths>
#include <QTranslator>

#include "core/util/Logger.h"
#include "persistence/config/AppConfig.h"
#include "persistence/repository/Database.h"
#include "persistence/repository/SqliteTabRepository.h"
#include "ui/main_window/MainWindow.h"

#ifndef DANTE_VERSION_STRING
#define DANTE_VERSION_STRING "0.0.0"
#endif

namespace {

void applyStylesheet(QApplication& app) {
    QFile qss(":/themes/dark.qss");
    if (qss.open(QIODevice::ReadOnly)) {
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));
    }
}

void installTranslators(QApplication& app) {
    static QTranslator qtTr;
    static QTranslator appTr;

    const QString override = dante::persistence::AppConfig::instance().languageOverride();
    QLocale locale = override.isEmpty() ? QLocale::system() : QLocale(override);

    if (appTr.load(locale, QStringLiteral("dante"), QStringLiteral("_"),
                   QStringLiteral(":/i18n"))) {
        app.installTranslator(&appTr);
    }
}

}  // namespace

int main(int argc, char** argv) {
    QElapsedTimer startupTimer;
    startupTimer.start();

    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication app(argc, argv);
    app.setOrganizationName("Dante");
    app.setApplicationName("Dante CLI");
    app.setApplicationVersion(QStringLiteral(DANTE_VERSION_STRING));
    app.setWindowIcon(QIcon(":/icons/app.svg"));

    const QString logsDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                          + QStringLiteral("/logs");
    QDir().mkpath(logsDir);
    dante::core::installFileLogger(logsDir);

    qCInfo(dante::core::lcApp, "Dante CLI %s starting…", DANTE_VERSION_STRING);

    dante::persistence::AppConfig::instance().load();
    applyStylesheet(app);
    installTranslators(app);

    auto dbResult = dante::persistence::Database::open(
        dante::persistence::Database::defaultPath());
    if (!dbResult) {
        qCCritical(dante::core::lcApp, "DB open failed: %s",
                   dbResult.error().message.c_str());
        return 2;
    }

    auto* tabRepo = new dante::persistence::SqliteTabRepository(dbResult.value());

    dante::ui::MainWindow window(tabRepo);
    window.show();

    qCInfo(dante::core::lcApp, "startup LCP: %lld ms", startupTimer.elapsed());

    const int rc = app.exec();

    dante::persistence::AppConfig::instance().save();
    dante::persistence::Database::closeAll();
    qCInfo(dante::core::lcApp, "Dante CLI exited rc=%d", rc);
    return rc;
}
