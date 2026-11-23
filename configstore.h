#ifndef CONFIGSTORE_H
#define CONFIGSTORE_H

#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QString>

struct ChatFonts {
    static constexpr int pointSize = 12;
    QString messageFamily = QStringLiteral("Segoe UI");
    QString codeFamily = QStringLiteral("Consolas");
};

struct AppSettings {
    QString apiEndpoint;
    QString modelName;
    QString apiKey;
    QString proxy;
    QString hotkey;
    int maxChars = 0;
    QJsonObject modelParameters;
    ChatFonts chatFonts;
};

struct TaskDefinition {
    QString name;
    QString prompt;
    QString modelName;
    QString hotkey;
    bool showInMenu = true;
    bool insertMode = true;
    int maxTokens = 300;
    double temperature = 0.5;
    int responseWidth = 600;
    int responseHeight = 200;
    int responseZoom = 0;
    QJsonObject modelParameters;
};

struct AppConfig {
    AppSettings settings;
    QList<TaskDefinition> tasks;
};

class ConfigStore {
public:
    static QString configFilePath();
    static AppConfig defaultConfig();
    static AppConfig fromJson(const QJsonDocument &doc, bool *ok = nullptr);
    static QJsonDocument toJson(const AppConfig &config);
    static bool loadFromFile(const QString &path, AppConfig *config);
    static bool saveToFile(const QString &path, const AppConfig &config);
};

#endif // CONFIGSTORE_H
