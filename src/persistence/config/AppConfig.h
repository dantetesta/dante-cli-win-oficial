#pragma once

#include <QJsonObject>
#include <QString>

namespace dante::persistence {

// Lightweight JSON config (settings.json) — saved with 500 ms debounce.
class AppConfig {
public:
    static AppConfig& instance();

    void load();
    void save();  // sync
    void scheduleSave();  // debounced 500 ms

    // appearance
    QString theme() const;
    void setTheme(const QString& v);

    QString fontFamily() const;
    void setFontFamily(const QString& v);

    int fontSize() const;
    void setFontSize(int v);

    // terminal defaults
    QString defaultScheme() const;
    void setDefaultScheme(const QString& v);

    int scrollbackLines() const;
    void setScrollbackLines(int v);

    // voice
    QString groqApiKey() const;
    void setGroqApiKey(const QString& v);

    QString voiceLanguage() const;
    void setVoiceLanguage(const QString& v);

    // updates
    bool autoCheckUpdates() const;
    void setAutoCheckUpdates(bool v);

    QString manifestUrl() const;

    // language override: "", "en", "pt-BR"
    QString languageOverride() const;
    void setLanguageOverride(const QString& v);

    static QString defaultPath();

private:
    AppConfig() = default;
    QJsonObject m_root;
    QString m_path;
};

}  // namespace dante::persistence
