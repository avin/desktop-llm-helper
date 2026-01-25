#ifndef TASKWINDOW_H
#define TASKWINDOW_H

#include <QWidget>
#include <QList>
#include <QEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QLabel>
#include <QPointer>

#include "configstore.h"

class QNetworkAccessManager;
class QNetworkReply;
class QTextBrowser;
class QDialog;

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

    QPointer<QDialog> responseWindow;
    QPointer<QTextBrowser> responseView;
    QByteArray responseBody;
    QByteArray streamBuffer;
    QString responseText;
    bool sawStreamFormat;

    QString captureSelectedText();
    QString applyCharLimit(const QString &text) const;
    void sendRequest(const TaskDefinition &task, const QString &originalText);
    void handleReplyReadyRead(const TaskDefinition &task, QNetworkReply *reply);
    void handleReplyFinished(const TaskDefinition &task, QNetworkReply *reply);
    void insertResponse(const QString &text);
    void ensureResponseWindow();
    void updateResponseView();
    QString extractResponseTextFromJson(const QByteArray &data) const;
    void resetResponseState();
    QString parseStreamDelta(const QByteArray &line);

    void showLoadingIndicator();
    void hideLoadingIndicator();
};

#endif // TASKWINDOW_H
