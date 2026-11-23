#ifndef MODELSELECTBOX_H
#define MODELSELECTBOX_H

#include "modelinfo.h"
#include "searchableselectbox.h"

#include <QHash>

class QFrame;
class QLabel;

class ModelSelectBox : public SearchableSelectBox {
    Q_OBJECT

public:
    explicit ModelSelectBox(QWidget *parent = nullptr);

    QString currentModel() const;
    void setCurrentModel(const QString &modelId);
    void setEmptyLabel(const QString &label);
    QString emptyLabel() const;
    int currentReloadGeneration() const;

public slots:
    void setModels(const ModelInfoList &models);
    void setLoadError(const QString &message);

signals:
    void currentModelChanged(const QString &modelId);
    void modelsReloadRequested(ModelSelectBox *target, int generation);

private:
    QHash<QString, ModelInfo> modelsById;
    QFrame *tooltip = nullptr;
    QLabel *tooltipName = nullptr;
    QLabel *tooltipDescription = nullptr;
    QLabel *tooltipPricing = nullptr;
    QLabel *tooltipKnowledge = nullptr;
    int reloadGeneration = 0;

    void preparePopup() override;
    void showTooltipFor(const ModelInfo &model, const QRect &globalAnchor);
    void hideTooltip();
};

#endif // MODELSELECTBOX_H
