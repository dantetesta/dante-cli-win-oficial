#include "AppConfig.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTimer>

namespace dante::persistence {

namespace {
QTimer* debounceTimer() {
    static QTimer t;
    static bool init = false;
    if (!init) {
        t.setSingleShot(true);
        t.setInterval(500);
        init = true;
    }
    return &t;
}
}  // namespace

AppConfig& AppConfig::instance() {
    static AppConfig c;
    return c;
}

QString AppConfig::defaultPath() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(base);
    return base + QStringLiteral("/config.json");
}

void AppConfig::load() {
    m_path = defaultPath();
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly)) return;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (doc.isObject()) m_root = doc.object();
}

void AppConfig::save() {
    if (m_path.isEmpty()) m_path = defaultPath();
    QFile f(m_path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    f.write(QJsonDocument(m_root).toJson(QJsonDocument::Indented));
}

void AppConfig::scheduleSave() {
    auto* t = debounceTimer();
    QObject::disconnect(t, &QTimer::timeout, nullptr, nullptr);
    QObject::connect(t, &QTimer::timeout, []() { AppConfig::instance().save(); });
    t->start();
}

// --- accessors -----------------------------------------------------------

QString AppConfig::theme() const {
    return m_root.value("appearance").toObject().value("theme").toString("dracula");
}
void AppConfig::setTheme(const QString& v) {
    auto a = m_root.value("appearance").toObject();
    a["theme"] = v;
    m_root["appearance"] = a;
    scheduleSave();
}

QString AppConfig::fontFamily() const {
    return m_root.value("appearance").toObject().value("font").toString("Cascadia Code");
}
void AppConfig::setFontFamily(const QString& v) {
    auto a = m_root.value("appearance").toObject();
    a["font"] = v;
    m_root["appearance"] = a;
    scheduleSave();
}

int AppConfig::fontSize() const {
    return m_root.value("appearance").toObject().value("font_size").toInt(13);
}
void AppConfig::setFontSize(int v) {
    auto a = m_root.value("appearance").toObject();
    a["font_size"] = v;
    m_root["appearance"] = a;
    scheduleSave();
}

QString AppConfig::defaultScheme() const {
    return m_root.value("terminal").toObject().value("scheme").toString("dracula");
}
void AppConfig::setDefaultScheme(const QString& v) {
    auto a = m_root.value("terminal").toObject();
    a["scheme"] = v;
    m_root["terminal"] = a;
    scheduleSave();
}

int AppConfig::scrollbackLines() const {
    return m_root.value("terminal").toObject().value("scrollback").toInt(50000);
}
void AppConfig::setScrollbackLines(int v) {
    auto a = m_root.value("terminal").toObject();
    a["scrollback"] = v;
    m_root["terminal"] = a;
    scheduleSave();
}

QString AppConfig::groqApiKey() const {
    return m_root.value("voice").toObject().value("groq_api_key").toString();
}
void AppConfig::setGroqApiKey(const QString& v) {
    auto a = m_root.value("voice").toObject();
    a["groq_api_key"] = v;
    m_root["voice"] = a;
    scheduleSave();
}

QString AppConfig::voiceLanguage() const {
    return m_root.value("voice").toObject().value("language").toString("pt-BR");
}
void AppConfig::setVoiceLanguage(const QString& v) {
    auto a = m_root.value("voice").toObject();
    a["language"] = v;
    m_root["voice"] = a;
    scheduleSave();
}

bool AppConfig::autoCheckUpdates() const {
    return m_root.value("updates").toObject().value("auto_check").toBool(true);
}
void AppConfig::setAutoCheckUpdates(bool v) {
    auto a = m_root.value("updates").toObject();
    a["auto_check"] = v;
    m_root["updates"] = a;
    scheduleSave();
}

QString AppConfig::manifestUrl() const {
    return m_root.value("updates").toObject().value("manifest_url")
        .toString("https://dante.cli/updates/win.json");
}

QString AppConfig::languageOverride() const {
    return m_root.value("language_override").toString();
}
void AppConfig::setLanguageOverride(const QString& v) {
    m_root["language_override"] = v;
    scheduleSave();
}

}  // namespace dante::persistence
