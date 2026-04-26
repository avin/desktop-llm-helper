#ifndef MODELSELECTBOX_H
#define MODELSELECTBOX_H

#include <QPushButton>

#include "modelinfo.h"

class QFrame;
class QLabel;
class QLineEdit;
class QScrollArea;
class QVBoxLayout;
class QStackedWidget;

class ModelSelectBox : public QPushButton {
    Q_OBJECT

public:
    explicit ModelSelectBox(QWidget *parent = nullptr);
    ~ModelSelectBox() override;

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

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QFrame *popup = nullptr;
    QLineEdit *searchEdit = nullptr;
    QStackedWidget *stack = nullptr;
    QWidget *loadingPage = nullptr;
    QLabel *loadingLabel = nullptr;
    QScrollArea *scrollArea = nullptr;
    QWidget *rowsWidget = nullptr;
    QVBoxLayout *rowsLayout = nullptr;
    QFrame *tooltip = nullptr;
    QLabel *tooltipName = nullptr;
    QLabel *tooltipDescription = nullptr;
    QLabel *tooltipPricing = nullptr;
    QLabel *tooltipKnowledge = nullptr;
    ModelInfoList loadedModels;
    QString selectedModelId;
    QString noSelectionLabel;
    bool popupLoading = false;
    int reloadGeneration = 0;

    void ensurePopup();
    void showPopup();
    void showLoading();
    void rebuildRows();
    void addRow(const ModelInfo &model, bool isEmptyItem);
    void chooseModel(const QString &modelId);
    void updateButtonLabel();
    void showTooltipFor(const ModelInfo &model, QWidget *anchor);
    void hideTooltip();
    void clearRows();
    ModelInfo modelForObject(QObject *object) const;
};

#endif // MODELSELECTBOX_H
