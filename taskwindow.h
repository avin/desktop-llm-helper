#ifndef TASKWINDOW_H
#define TASKWINDOW_H

#include <QWidget>
#include <QList>
#include <QEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QLabel>

#include "configstore.h"

class QNetworkAccessManager;
class QNetworkReply;

class TaskWindow : public QWidget {
    Q_OBJECT

public:
    explicit TaskWindow(const QList<TaskDefinition> &tasks,
                        const AppSettings &settings,
                        QWidget *parent = nullptr);
    ~TaskWindow() override;

protected:
    void keyPressEvent(QKeyEvent *ev) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void updateLoadingPosition();
    void animateLoadingText();

private:
    AppSettings settings;
    QNetworkAccessManager *networkManager;
    QWidget *loadingWindow;
    QTimer *loadingTimer;
    QLabel *loadingLabel;
    QTimer *animationTimer;
    int dotCount;

    QString captureSelectedText();
    QString applyCharLimit(const QString &text) const;
    void sendRequest(const TaskDefinition &task, const QString &originalText);
    void handleReply(const TaskDefinition &task, QNetworkReply *reply);
    void insertResponse(const QString &text);
    void showResponseWindow(const QString &text);

    void showLoadingIndicator();
    void hideLoadingIndicator();
};

#endif // TASKWINDOW_H
