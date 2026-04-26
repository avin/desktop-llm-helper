#ifndef MODELINFO_H
#define MODELINFO_H

#include <QString>
#include <QVector>

struct ModelInfo {
    QString id;
    QString name;
    QString description;
    QString promptPrice;
    QString completionPrice;
    QString inputCacheReadPrice;
    QString knowledgeCutoff;
};

using ModelInfoList = QVector<ModelInfo>;

#endif // MODELINFO_H
