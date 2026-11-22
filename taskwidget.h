#ifndef TASKWIDGET_H
#define TASKWIDGET_H

#include <QWidget>
#include <QString>
#include <QSize>
#include <QJsonObject>

namespace Ui {
    class TaskWidget;
}

struct TaskDefinition;
class ModelSelectBox;

class TaskWidget : public QWidget {
    Q_OBJECT

public:
    explicit TaskWidget(QWidget *parent = nullptr);
    ~TaskWidget();

    QString name() const;
    QString prompt() const;
    QString modelName() const;
    QString effectiveModelName() const;
    QJsonObject modelParameters() const;
    bool insertMode() const;
    void setName(const QString &name);
    void setPrompt(const QString &prompt);
    void setModelName(const QString &modelName);
    void setDefaultModel(const QString &defaultModel);
    void setModelParameters(const QJsonObject &parameters);
    void setInsertMode(bool insert);

    int maxTokens() const;
    double temperature() const;
    void setMaxTokens(int tokens);
    void setTemperature(double temp);

    void setResponseWindowSize(const QSize &size);
    void setResponseZoom(int zoom);

    TaskDefinition toDefinition() const;
    void applyDefinition(const TaskDefinition &definition);

signals:
    void configChanged();
    void refreshModelsRequested(ModelSelectBox *target, int generation);
    void modelParametersRequested();

private:
    Ui::TaskWidget *ui;
    QString defaultModelName;
    QJsonObject selectedModelParameters;
    int responseWidth = 600;
    int responseHeight = 200;
    int responseZoomValue = 0;
};

#endif // TASKWIDGET_H
