#include "modellistloader.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QTimer>
#include <QUrl>

namespace {
constexpr const char kDefaultModelLabel[] = "Default";
constexpr int kRequestTimeoutMs = 30000;

QUrl buildApiUrl(const QString &baseUrl, const QString &pathSuffix) {
    QUrl url(baseUrl.trimmed());
    if (!url.isValid())
        return QUrl();
    QString path = url.path();
    if (!path.endsWith('/'))
        path += '/';
    QString suffix = pathSuffix;
    if (suffix.startsWith('/'))
        suffix.remove(0, 1);
    url.setPath(path + suffix);
    return url;
}

void applyProxyToManager(QNetworkAccessManager *manager, const QString &proxyText) {
    if (!manager)
        return;
    const QString trimmed = proxyText.trimmed();
    if (trimmed.isEmpty()) {
        QNetworkProxy proxy;
        proxy.setType(QNetworkProxy::NoProxy);
        manager->setProxy(proxy);
        return;
    }
    QUrl proxyUrl(trimmed);
    if (!proxyUrl.isValid()) {
        QNetworkProxy proxy;
        proxy.setType(QNetworkProxy::NoProxy);
        manager->setProxy(proxy);
        return;
    }
    QNetworkProxy proxy(QNetworkProxy::HttpProxy, proxyUrl.host(), proxyUrl.port());
    manager->setProxy(proxy);
}

ModelInfoList parseModelList(const QJsonDocument &doc) {
    ModelInfoList models;
    QSet<QString> seenIds;
    const QJsonArray data = doc.object().value("data").toArray();

    for (const QJsonValue &value : data) {
        if (!value.isObject())
            continue;

        const QJsonObject object = value.toObject();
        const QString id = object.value("id").toString().trimmed();
        if (id.isEmpty() || id == QLatin1String(kDefaultModelLabel) || seenIds.contains(id))
            continue;

        ModelInfo model;
        model.id = id;
        model.name = object.value("name").toString();
        model.description = object.value("description").toString();
        model.knowledgeCutoff = object.value("knowledge_cutoff").toString();

        const QJsonObject pricing = object.value("pricing").toObject();
        model.promptPrice = pricing.value("prompt").toString();
        model.completionPrice = pricing.value("completion").toString();
        model.inputCacheReadPrice = pricing.value("input_cache_read").toString();

        seenIds.insert(id);
        models.append(model);
    }

    return models;
}
}

ModelListLoader::ModelListLoader(QObject *parent)
    : QObject(parent) {
}

void ModelListLoader::loadModels(int requestId, const QString &baseUrl, const QString &apiKey,
                                 const QString &proxyText) {
    const QUrl url = buildApiUrl(baseUrl, QStringLiteral("models"));
    if (!url.isValid() || url.isEmpty()) {
        emit loadFailed(requestId, tr("Invalid API base URL."));
        return;
    }

    QNetworkAccessManager manager;
    applyProxyToManager(&manager, proxyText);

    QNetworkRequest request(url);
    const QString trimmedApiKey = apiKey.trimmed();
    if (!trimmedApiKey.isEmpty())
        request.setRawHeader("Authorization", "Bearer " + trimmedApiKey.toUtf8());

    QNetworkReply *reply = manager.get(request);
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    connect(&timeoutTimer, &QTimer::timeout, reply, &QNetworkReply::abort);
    timeoutTimer.start(kRequestTimeoutMs);
    loop.exec();

    const QByteArray payload = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
        const QString errorText = timeoutTimer.isActive()
            ? tr("Failed to load models: %1").arg(reply->errorString())
            : tr("Failed to load models: request timed out.");
        reply->deleteLater();
        emit loadFailed(requestId, errorText);
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject()) {
        reply->deleteLater();
        emit loadFailed(requestId, tr("Invalid response format."));
        return;
    }

    const ModelInfoList models = parseModelList(doc);
    reply->deleteLater();
    emit modelsLoaded(requestId, models);
}
