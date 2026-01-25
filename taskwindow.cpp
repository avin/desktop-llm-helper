#include "taskwindow.h"

#include <QClipboard>
#include <QColor>
#include <QCoreApplication>
#include <QCursor>
#include <QDialog>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFont>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QTextBrowser>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <windows.h>

namespace {
QPoint clampToScreen(const QPoint &pos, const QSize &size, const QRect &available) {
    int x = pos.x();
    int y = pos.y();
    const int maxX = available.x() + available.width() - size.width();
    const int maxY = available.y() + available.height() - size.height();
    if (x > maxX)
        x = maxX;
    if (y > maxY)
        y = maxY;
    if (x < available.x())
        x = available.x();
    if (y < available.y())
        y = available.y();
    return QPoint(x, y);
}

void moveNearCursor(QWidget *widget, const QPoint &cursorPos) {
    QScreen *screen = QGuiApplication::screenAt(cursorPos);
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect available = screen->availableGeometry();
    widget->move(clampToScreen(cursorPos, widget->size(), available));
}
}

TaskWindow::TaskWindow(const QList<TaskDefinition> &tasks,
                       const AppSettings &settings,
                       QWidget *parent)
    : QWidget(parent,
              Qt::Window | Qt::WindowStaysOnTopHint | Qt::CustomizeWindowHint
              | Qt::FramelessWindowHint)
    , settings(settings)
    , networkManager(new QNetworkAccessManager(this))
    , loadingWindow(nullptr)
    , loadingTimer(nullptr)
    , loadingLabel(nullptr)
    , animationTimer(nullptr)
    , dotCount(0)
    , responseWindow(nullptr)
    , responseView(nullptr)
    , sawStreamFormat(false) {
    setAttribute(Qt::WA_DeleteOnClose, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::StrongFocus);

    if (!this->settings.proxy.isEmpty()) {
        QUrl proxyUrl(this->settings.proxy);
        if (proxyUrl.isValid()) {
            QNetworkProxy proxy;
            proxy.setType(QNetworkProxy::HttpProxy);
            proxy.setHostName(proxyUrl.host());
            proxy.setPort(proxyUrl.port());
            networkManager->setProxy(proxy);
        }
    }

    QPushButton *firstButton = nullptr;

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(0);

    auto *container = new QWidget(this);
    container->setObjectName("container");
    container->setStyleSheet("QWidget#container { "
                             "  background-color: white; "
                             "  border-radius: 0; "
                             "}");

    auto *shadow = new QGraphicsDropShadowEffect(container);
    shadow->setBlurRadius(10);
    shadow->setColor(QColor(0, 0, 0, 160));
    shadow->setOffset(0, 0);
    container->setGraphicsEffect(shadow);

    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(2);

    for (const TaskDefinition &task : tasks) {
        QString text = task.name.isEmpty() ? tr("<Unnamed>") : task.name;
        auto *btn = new QPushButton(text, container);
        btn->setStyleSheet(
            "QPushButton {"
            "   text-align: left;"
            "   padding: 2px 8px;"
            "   border: 1px solid #adadad;"
            "   background-color: #e1e1e1;"
            "}"
            "QPushButton:focus {"
            "   outline: 0;"
            "   border: 1px solid #0078d7;"
            "   background-color: #e5f1fb;"
            "}"
        );
        btn->setMouseTracking(true);
        btn->installEventFilter(this);
        if (!firstButton)
            firstButton = btn;
        connect(btn, &QPushButton::clicked, this, [this, task]() {
            hide();
            showLoadingIndicator();

            const QString original = captureSelectedText();
            if (original.isEmpty()) {
                hideLoadingIndicator();
                return;
            }

            QClipboard *clipboard = QGuiApplication::clipboard();
            clipboard->setText(task.prompt + original);

            sendRequest(task, original);
        });
        layout->addWidget(btn);
    }

    mainLayout->addWidget(container);

    adjustSize();

    const int btnSize = 16;
    auto *closeBtn = new QPushButton(QString::fromUtf8("\xC3\x97"), this);
    closeBtn->setFixedSize(btnSize, btnSize);
    closeBtn->setFlat(true);
    closeBtn->setStyleSheet("QPushButton { "
                            "   background-color: #FFFFFF; "
                            "   border: 1px solid #BBBBBB; "
                            "   border-radius: 8px; "
                            "   padding-bottom: 2px; "
                            "} "
                            "QPushButton:hover, QPushButton:focus { "
                            "   background-color: #e81123; "
                            "   color: #FFFFFF; "
                            "   outline: 0; "
                            "}");
    int xBtn = width() - mainLayout->contentsMargins().right() - btnSize / 2;
    int yBtn = mainLayout->contentsMargins().top() - btnSize / 2;
    closeBtn->move(xBtn, yBtn);
    closeBtn->raise();
    connect(closeBtn, &QPushButton::clicked, this, &TaskWindow::close);

    const QPoint cursorPos = QCursor::pos();
    moveNearCursor(this, cursorPos);

    show();
    raise();
    activateWindow();
    setFocus(Qt::OtherFocusReason);
    if (firstButton)
        firstButton->setFocus(Qt::TabFocusReason);
}

TaskWindow::~TaskWindow() {
    hideLoadingIndicator();
}

QString TaskWindow::captureSelectedText() {
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->clear(QClipboard::Clipboard);
    INPUT copyInputs[4] = {};
    copyInputs[0].type = INPUT_KEYBOARD;
    copyInputs[0].ki.wVk = VK_CONTROL;
    copyInputs[1].type = INPUT_KEYBOARD;
    copyInputs[1].ki.wVk = 'C';
    copyInputs[2].type = INPUT_KEYBOARD;
    copyInputs[2].ki.wVk = 'C';
    copyInputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    copyInputs[3].type = INPUT_KEYBOARD;
    copyInputs[3].ki.wVk = VK_CONTROL;
    copyInputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, copyInputs, sizeof(INPUT));

    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 200) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        const QString text = clipboard->text();
        if (!text.isEmpty())
            return text;
    }
    return QString();
}

QString TaskWindow::applyCharLimit(const QString &text) const {
    if (settings.maxChars > 0 && text.length() > settings.maxChars)
        return text.left(settings.maxChars);
    return text;
}

void TaskWindow::sendRequest(const TaskDefinition &task, const QString &originalText) {
    resetResponseState();
    const QString sendText = applyCharLimit(originalText);

    QNetworkRequest request(QUrl(settings.apiEndpoint));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", "Bearer " + settings.apiKey.toUtf8());

    QJsonObject systemMessage;
    systemMessage["role"] = "system";
    systemMessage["content"] = task.prompt;
    QJsonObject userMessage;
    userMessage["role"] = "user";
    userMessage["content"] = sendText;
    QJsonArray messagesArray;
    messagesArray.append(systemMessage);
    messagesArray.append(userMessage);

    QJsonObject body;
    body["model"] = settings.modelName;
    body["messages"] = messagesArray;
    body["max_tokens"] = task.maxTokens;
    body["temperature"] = task.temperature;
    if (!task.insertMode)
        body["stream"] = true;
    QJsonDocument bodyDoc(body);

    QNetworkReply *reply = networkManager->post(request, bodyDoc.toJson());
    connect(reply, &QNetworkReply::readyRead, this, [this, task, reply]() {
        handleReplyReadyRead(task, reply);
    });
    connect(reply, &QNetworkReply::finished, this, [this, task, reply]() {
        handleReplyFinished(task, reply);
    });
}

void TaskWindow::handleReplyReadyRead(const TaskDefinition &task, QNetworkReply *reply) {
    const QByteArray chunk = reply->readAll();
    if (chunk.isEmpty())
        return;

    responseBody.append(chunk);
    streamBuffer.append(chunk);

    bool appended = false;
    while (true) {
        const int lineEnd = streamBuffer.indexOf('\n');
        if (lineEnd < 0)
            break;
        const QByteArray line = streamBuffer.left(lineEnd);
        streamBuffer.remove(0, lineEnd + 1);
        const QString delta = parseStreamDelta(line);
        if (!delta.isEmpty()) {
            responseText += delta;
            appended = true;
        }
    }

    if (sawStreamFormat) {
        hideLoadingIndicator();
        if (!task.insertMode)
            ensureResponseWindow();
    }

    if (appended && !task.insertMode)
        updateResponseView();
}

void TaskWindow::handleReplyFinished(const TaskDefinition &task, QNetworkReply *reply) {
    hideLoadingIndicator();

    if (!streamBuffer.isEmpty()) {
        const QString delta = parseStreamDelta(streamBuffer);
        if (!delta.isEmpty())
            responseText += delta;
        streamBuffer.clear();
    }

    if (reply->error() != QNetworkReply::NoError) {
        const QString errStr = reply->errorString();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QMessageBox::critical(this,
                              tr("Error"),
                              tr("LLM request failed (%1): HTTP status %2")
                                  .arg(errStr)
                                  .arg(statusCode));
        reply->deleteLater();
        return;
    }

    if (!sawStreamFormat && responseText.isEmpty()) {
        responseText = extractResponseTextFromJson(responseBody);
    }

    reply->deleteLater();

    if (task.insertMode) {
        if (!responseText.isEmpty())
            insertResponse(responseText);
        return;
    }

    if (responseWindow || !responseText.isEmpty()) {
        ensureResponseWindow();
        updateResponseView();
    }
}

void TaskWindow::insertResponse(const QString &text) {
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(text);

    INPUT pasteInputs[4] = {};
    pasteInputs[0].type = INPUT_KEYBOARD;
    pasteInputs[0].ki.wVk = VK_CONTROL;
    pasteInputs[1].type = INPUT_KEYBOARD;
    pasteInputs[1].ki.wVk = 'V';
    pasteInputs[2].type = INPUT_KEYBOARD;
    pasteInputs[2].ki.wVk = 'V';
    pasteInputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    pasteInputs[3].type = INPUT_KEYBOARD;
    pasteInputs[3].ki.wVk = VK_CONTROL;
    pasteInputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, pasteInputs, sizeof(INPUT));
}

void TaskWindow::ensureResponseWindow() {
    if (responseWindow)
        return;

    responseWindow = new QDialog(this);
    responseWindow->setAttribute(Qt::WA_DeleteOnClose, true);
    responseWindow->setWindowFlags(responseWindow->windowFlags() | Qt::Dialog);

    auto *lay = new QVBoxLayout(responseWindow);
    responseView = new QTextBrowser(responseWindow);
    QFont font = responseView->font();
    font.setPointSize(12);
    responseView->setFont(font);
    responseView->setReadOnly(true);
    responseView->setOpenExternalLinks(true);
    lay->addWidget(responseView);

    responseWindow->setWindowTitle(tr("LLM Response"));
    responseWindow->resize(600, 200);

    const QPoint cursorPos = QCursor::pos();
    moveNearCursor(responseWindow, cursorPos);

    responseWindow->show();
    responseWindow->raise();
    responseWindow->activateWindow();
}

void TaskWindow::updateResponseView() {
    if (!responseView)
        return;
    QScrollBar *bar = responseView->verticalScrollBar();
    const bool atBottom = bar && bar->value() >= bar->maximum();
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    responseView->setMarkdown(responseText);
#else
    responseView->setPlainText(responseText);
#endif
    if (atBottom)
        bar->setValue(bar->maximum());
}

QString TaskWindow::extractResponseTextFromJson(const QByteArray &data) const {
    const QJsonDocument respDoc = QJsonDocument::fromJson(data);
    if (!respDoc.isObject())
        return QString();
    const QJsonObject respObj = respDoc.object();
    const QJsonArray choices = respObj.value("choices").toArray();
    if (choices.isEmpty())
        return QString();
    const QJsonObject msg = choices.first().toObject()
        .value("message")
        .toObject();
    return msg.value("content").toString();
}

void TaskWindow::resetResponseState() {
    responseBody.clear();
    streamBuffer.clear();
    responseText.clear();
    sawStreamFormat = false;
    responseWindow = nullptr;
    responseView = nullptr;
}

QString TaskWindow::parseStreamDelta(const QByteArray &line) {
    const QByteArray trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return QString();
    if (!trimmed.startsWith("data:"))
        return QString();
    sawStreamFormat = true;
    const QByteArray payload = trimmed.mid(5).trimmed();
    if (payload == "[DONE]")
        return QString();

    const QJsonDocument doc = QJsonDocument::fromJson(payload);
    if (!doc.isObject())
        return QString();
    const QJsonObject obj = doc.object();
    const QJsonArray choices = obj.value("choices").toArray();
    if (choices.isEmpty())
        return QString();
    const QJsonObject choice = choices.first().toObject();
    QString deltaText = choice.value("delta").toObject().value("content").toString();
    if (deltaText.isEmpty())
        deltaText = choice.value("message").toObject().value("content").toString();
    return deltaText;
}

bool TaskWindow::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent*>(event);
        if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter)
            && qobject_cast<QPushButton*>(watched)) {
            auto *btn = qobject_cast<QPushButton*>(watched);
            btn->click();
            return true;
        }
    }
    if (event->type() == QEvent::Enter) {
        if (auto *btn = qobject_cast<QPushButton *>(watched))
            btn->setFocus(Qt::MouseFocusReason);
    }
    return QWidget::eventFilter(watched, event);
}

void TaskWindow::showLoadingIndicator() {
    if (loadingWindow)
        return;
    loadingWindow = new QWidget(nullptr,
                                Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    loadingWindow->setAttribute(Qt::WA_ShowWithoutActivating);
    loadingWindow->setAttribute(Qt::WA_TransparentForMouseEvents);
    loadingWindow->setFocusPolicy(Qt::NoFocus);

    QLabel *label = new QLabel(tr("Loading"), loadingWindow);
    loadingLabel = label;
    dotCount = 0;

    QVBoxLayout *lay = new QVBoxLayout(loadingWindow);
    lay->setContentsMargins(5, 5, 5, 5);
    lay->addWidget(label);
    loadingWindow->setLayout(lay);

    loadingWindow->adjustSize();
    updateLoadingPosition();
    loadingWindow->show();

    loadingTimer = new QTimer(this);
    connect(loadingTimer, &QTimer::timeout,
            this, &TaskWindow::updateLoadingPosition);
    loadingTimer->start(16);

    animationTimer = new QTimer(this);
    connect(animationTimer, &QTimer::timeout,
            this, &TaskWindow::animateLoadingText);
    animationTimer->start(500);
}

void TaskWindow::hideLoadingIndicator() {
    if (loadingTimer) {
        loadingTimer->stop();
        loadingTimer->deleteLater();
        loadingTimer = nullptr;
    }
    if (animationTimer) {
        animationTimer->stop();
        animationTimer->deleteLater();
        animationTimer = nullptr;
    }
    if (loadingWindow) {
        loadingWindow->close();
        loadingWindow->deleteLater();
        loadingWindow = nullptr;
        loadingLabel = nullptr;
    }
}

void TaskWindow::updateLoadingPosition() {
    if (!loadingWindow)
        return;
    const QPoint pos = QCursor::pos();
    loadingWindow->move(pos.x() + 10, pos.y() + 10);
}

void TaskWindow::animateLoadingText() {
    if (!loadingLabel)
        return;
    dotCount = (dotCount + 1) % 4;
    loadingLabel->setText(tr("Loading") + QString(dotCount, '.'));
    if (loadingWindow) {
        loadingWindow->adjustSize();
        updateLoadingPosition();
    }
}

void TaskWindow::changeEvent(QEvent *event) {
    if (event->type() == QEvent::ActivationChange && !isActiveWindow())
        close();
    QWidget::changeEvent(event);
}

void TaskWindow::keyPressEvent(QKeyEvent *ev) {
    if (ev->key() == Qt::Key_Escape)
        close();
    else
        QWidget::keyPressEvent(ev);
}
