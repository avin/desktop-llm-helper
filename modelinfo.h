#ifndef MODELINFO_H
#define MODELINFO_H

#include <QString>
#include <QVector>
#include <QMetaType>

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

Q_DECLARE_METATYPE(ModelInfoList)

#endif // MODELINFO_H
