#ifndef MODELLISTLOADER_H
#define MODELLISTLOADER_H

#include <QObject>
#include <QString>

#include "modelinfo.h"

class ModelListLoader : public QObject {
    Q_OBJECT

public:
    explicit ModelListLoader(QObject *parent = nullptr);

public slots:
    void loadModels(int requestId, const QString &baseUrl, const QString &apiKey, const QString &proxyText);

signals:
    void modelsLoaded(int requestId, const ModelInfoList &models);
    void loadFailed(int requestId, const QString &message);
};

#endif // MODELLISTLOADER_H
