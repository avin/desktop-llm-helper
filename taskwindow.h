#ifndef TASKWINDOW_H
#define TASKWINDOW_H

#include <QWidget>
#include <QList>
#include <QEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QLabel>
#include <QPointer>
#include <QSize>

#include <memory>

#include <windows.h>

#include "configstore.h"

class QByteArray;
class QHideEvent;
class QPushButton;
class QShowEvent;
class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class QTextBrowser;
class QDialog;
class QPlainTextEdit;
class QMimeData;
class QUrl;

class TaskRequestWorker : public QObject {
    Q_OBJECT

public:
    explicit TaskRequestWorker(QObject *parent = nullptr);

public slots:
    void startRequest(const QUrl &url,
                      const QByteArray &authorizationHeader,
                      const QByteArray &body,
                      const QString &proxyText);
    void abortRequest();

signals:
    void readyRead(const QByteArray &chunk);
    void finished(int error, const QString &errorString, int statusCode);

private:
    QNetworkAccessManager *networkManager;
    QPointer<QNetworkReply> reply;

    void applyProxy(const QString &proxyText);
};

struct ChatMessage {
    QString role;
    QString content;
};

class TaskWindow : public QWidget {
    Q_OBJECT

public:
    explicit TaskWindow(const QList<TaskDefinition> &taskList,
                        const AppSettings &settings,
                        QWidget *parent = nullptr);
    ~TaskWindow() override;

signals:
    void taskResponsePrefsChanged(int taskIndex, const QSize &size, int zoom);
    void taskResponsePrefsCommitRequested();

protected:
    void keyPressEvent(QKeyEvent *ev) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private slots:
    void updateLoadingPosition();
    void animateLoadingText();
    void sendFollowUpMessage();

private:
    QList<TaskDefinition> tasks;
    TaskDefinition activeRequestTask;
    int activeTaskIndex;
    AppSettings settings;
    QThread *requestThread;
    TaskRequestWorker *requestWorker;
    QWidget *loadingWindow;
    QTimer *loadingTimer;
    QLabel *loadingLabel;
    QTimer *animationTimer;
    int dotCount;

    QPointer<QDialog> responseWindow;
    QPointer<QTextBrowser> responseView;
    QPointer<QPlainTextEdit> followUpInput;
    QPointer<QPushButton> actionButton;
    QByteArray responseBody;
    QByteArray streamBuffer;
    QString transcriptText;
    QString pendingResponseText;
    QList<ChatMessage> messageHistory;
    bool sawStreamFormat;
    bool requestInFlight;
    bool responseScrollDragActive;
    bool pendingResponseViewUpdate;
    bool originalClipboardWasEmpty;
    QList<QPushButton *> menuButtons;
    int menuActiveIndex;
    std::unique_ptr<QMimeData> originalClipboardData;

    static TaskWindow *s_activeMenu;
    static TaskWindow *s_activeOperation;
    static HHOOK s_keyboardHook;
    static HHOOK s_mouseHook;
    static HHOOK s_operationKeyboardHook;
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK LowLevelOperationKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

    QString captureSelectedText();
    void saveOriginalClipboard();
    void restoreOriginalClipboard();
    void clearOriginalClipboardSnapshot();
    void setClipboardText(const QString &text, bool excludeFromHistory);
    QString applyCharLimit(const QString &text) const;
    void startConversation(const TaskDefinition &task, const QString &originalText);
    void sendRequestWithHistory(const TaskDefinition &task);
    void handleRequestReadyRead(const QByteArray &chunk);
    void handleRequestFinished(int error, const QString &errorString, int statusCode);
    void insertResponse(const QString &text);
    void ensureResponseWindow();
    void updateResponseView();
    void applyMarkdownStyles();
    void updateFollowUpHeight();
    void appendMessageToHistory(const QString &role, const QString &content);
    void appendTranscriptBlock(const QString &markdown);
    QString normalizeMarkdownBlock(const QString &markdown) const;
    QString formatUserMessageBlock(const QString &text) const;
    QString buildDisplayMarkdown() const;
    QString extractResponseTextFromJson(const QByteArray &data) const;
    void resetRequestState();
    void resetConversationState();
    void setRequestInFlight(bool inFlight);
    void updateActionButtonState();
    void cancelRequest();
    QString parseStreamDelta(const QByteArray &line);
    void applyResponsePrefs();
    void handleResponseResize(const QSize &size);
    void handleResponseZoomDelta(int steps);
    void installMenuHooks();
    void removeMenuHooks();
    void installOperationCancelHook();
    void removeOperationCancelHook();
    bool handleHookKey(UINT vk);
    bool handleOperationHookKey(UINT vk);
    void handleHookMouseClick(const POINT &pt);
    bool isPointInsideMenu(const POINT &pt) const;
    void setMenuActiveIndex(int index);
    void selectNextMenuItem();
    void selectPreviousMenuItem();
    void activateMenuItem();
    void applyNoActivateStyle();

    void showLoadingIndicator();
    void hideLoadingIndicator();
};

#endif // TASKWINDOW_H
