#ifndef TASKWIDGET_H
#define TASKWIDGET_H

#include <QWidget>
#include <QString>

namespace Ui {
    class TaskWidget;
}

class TaskWidget : public QWidget {
    Q_OBJECT

public:
    explicit TaskWidget(QWidget *parent = nullptr);
    ~TaskWidget();

    QString name() const;
    QString prompt() const;
    bool insertMode() const;
    void setName(const QString &name);
    void setPrompt(const QString &prompt);
    void setInsertMode(bool insert);

    int maxTokens() const;
    double temperature() const;
    void setMaxTokens(int tokens);
    void setTemperature(double temp);

signals:
    void removeRequested(TaskWidget *task);
    void configChanged();

private slots:
    void on_pushButtonDelete_clicked();

private:
    Ui::TaskWidget *ui;
};

#endif // TASKWIDGET_H