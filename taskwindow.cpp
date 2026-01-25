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
#include <QRegularExpression>
#include <QScreen>
#include <QScrollBar>
#include <QStringList>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTextFormat>
#include <QTextCursor>
#include <QTextFragment>
#include <QWheelEvent>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <windows.h>

namespace {
bool isCodeBlock(const QTextBlock &block) {
    const QTextBlockFormat format = block.blockFormat();
    if (format.hasProperty(QTextFormat::BlockCodeFence))
        return true;
    if (format.hasProperty(QTextFormat::BlockCodeLanguage))
        return true;
    return block.charFormat().fontFixedPitch();
}

bool isInlineCodeFormat(const QTextCharFormat &format) {
    if (format.fontFixedPitch())
        return true;
    const QFont font = format.font();
    if (font.fixedPitch())
        return true;
    const QString family = font.family();
    if (family.contains("mono", Qt::CaseInsensitive)
        || family.contains("courier", Qt::CaseInsensitive)
        || family.contains("consolas", Qt::CaseInsensitive)) {
        return true;
    }
    const QStringList families = format.fontFamilies().toStringList();
    for (const QString &entry : families) {
        if (entry.contains("mono", Qt::CaseInsensitive)
            || entry.contains("courier", Qt::CaseInsensitive)
            || entry.contains("consolas", Qt::CaseInsensitive)) {
            return true;
        }
    }
    const QVariant hintProp = format.property(QTextFormat::FontStyleHint);
    if (hintProp.isValid()) {
        const int hint = hintProp.toInt();
        if (hint == QFont::TypeWriter || hint == QFont::Monospace)
            return true;
    }
    const QVariant familyProp = format.property(QTextFormat::FontFamily);
    if (familyProp.isValid()) {
        const QString propFamily = familyProp.toString();
        if (propFamily.contains("mono", Qt::CaseInsensitive)
            || propFamily.contains("courier", Qt::CaseInsensitive)
            || propFamily.contains("consolas", Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

QFont resolveBaseTextFont(QTextDocument *doc) {
    if (!doc)
        return QFont();
    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        if (isCodeBlock(block))
            continue;
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid())
                continue;
            const QTextCharFormat format = fragment.charFormat();
            if (isInlineCodeFormat(format))
                continue;
            const QFont font = format.font();
            if (font.pointSizeF() > 0 || font.pixelSize() > 0)
                return font;
        }
    }
    return doc->defaultFont();
}

class MarkdownCodeHighlighter : public QSyntaxHighlighter {
public:
    explicit MarkdownCodeHighlighter(QTextDocument *parent)
        : QSyntaxHighlighter(parent) {
        keywordFormat.setForeground(QColor("#d73a49"));
        keywordFormat.setFontWeight(QFont::Bold);

        stringFormat.setForeground(QColor("#032f62"));

        numberFormat.setForeground(QColor("#005cc5"));

        commentFormat.setForeground(QColor("#6a737d"));

        keywordPatterns = {
            QRegularExpression("\\b(auto|bool|break|case|catch|class|const|continue|def|default|"
                               "delete|do|else|enum|export|extends|false|final|finally|for|"
                               "foreach|from|function|if|implements|import|inline|interface|"
                               "lambda|let|namespace|new|nullptr|null|operator|private|protected|"
                               "public|return|static|struct|switch|template|this|throw|true|try|"
                               "typedef|typename|using|var|virtual|void|volatile|while)\\b")
        };

        stringPatterns = {
            QRegularExpression(R"("([^"\\]|\\.)*")"),
            QRegularExpression(R"('([^'\\]|\\.)*')")
        };

        numberPattern = QRegularExpression("\\b\\d+(?:\\.\\d+)?\\b");
        singleLineCommentPatterns = {
            QRegularExpression("//[^\\n]*"),
            QRegularExpression("#[^\\n]*")
        };
        multiLineCommentStart = QRegularExpression("/\\*");
        multiLineCommentEnd = QRegularExpression("\\*/");
    }

protected:
    void highlightBlock(const QString &text) override {
        setCurrentBlockState(0);
        if (!isCodeBlock(currentBlock()))
            return;

        for (const QRegularExpression &pattern : keywordPatterns) {
            auto it = pattern.globalMatch(text);
            while (it.hasNext()) {
                const QRegularExpressionMatch match = it.next();
                setFormat(match.capturedStart(), match.capturedLength(), keywordFormat);
            }
        }

        for (const QRegularExpression &pattern : stringPatterns) {
            auto it = pattern.globalMatch(text);
            while (it.hasNext()) {
                const QRegularExpressionMatch match = it.next();
                setFormat(match.capturedStart(), match.capturedLength(), stringFormat);
            }
        }

        auto numberIt = numberPattern.globalMatch(text);
        while (numberIt.hasNext()) {
            const QRegularExpressionMatch match = numberIt.next();
            setFormat(match.capturedStart(), match.capturedLength(), numberFormat);
        }

        for (const QRegularExpression &pattern : singleLineCommentPatterns) {
            auto it = pattern.globalMatch(text);
            while (it.hasNext()) {
                const QRegularExpressionMatch match = it.next();
                setFormat(match.capturedStart(), match.capturedLength(), commentFormat);
            }
        }

        int startIndex = 0;
        if (previousBlockState() == 1) {
            startIndex = 0;
        } else {
            const QRegularExpressionMatch match = multiLineCommentStart.match(text);
            startIndex = match.hasMatch() ? match.capturedStart() : -1;
        }

        while (startIndex >= 0) {
            const QRegularExpressionMatch endMatch = multiLineCommentEnd.match(text, startIndex);
            int commentLength = 0;
            if (endMatch.hasMatch()) {
                commentLength = endMatch.capturedEnd() - startIndex;
                setFormat(startIndex, commentLength, commentFormat);
            } else {
                setCurrentBlockState(1);
                commentLength = text.length() - startIndex;
                setFormat(startIndex, commentLength, commentFormat);
            }
            const QRegularExpressionMatch nextStart = multiLineCommentStart.match(text,
                                                                                  startIndex + commentLength);
            startIndex = nextStart.hasMatch() ? nextStart.capturedStart() : -1;
        }
    }

private:
    QTextCharFormat keywordFormat;
    QTextCharFormat stringFormat;
    QTextCharFormat numberFormat;
    QTextCharFormat commentFormat;
    QList<QRegularExpression> keywordPatterns;
    QList<QRegularExpression> stringPatterns;
    QRegularExpression numberPattern;
    QList<QRegularExpression> singleLineCommentPatterns;
    QRegularExpression multiLineCommentStart;
    QRegularExpression multiLineCommentEnd;
};

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

const QString &markdownCss() {
    static const QString css =
        "body {"
        "  font-family: 'Segoe UI', 'Noto Sans', Helvetica, Arial;"
        "  font-size: 12pt;"
        "  color: #24292f;"
        "}"
        "a { color: #0969da; text-decoration: none; }"
        "a:hover { text-decoration: underline; }"
        "p { margin: 8px 0; }"
        "h1 { font-size: 20pt; border-bottom: 1px solid #d0d7de; padding-bottom: 4px; }"
        "h2 { font-size: 16pt; border-bottom: 1px solid #d0d7de; padding-bottom: 2px; }"
        "h3 { font-size: 14pt; }"
        "ul, ol { margin-left: 20px; }"
        "pre { border: 1px solid #d0d7de; padding: 8px; margin: 8px 0; }"
        "blockquote { color: #57606a; border-left: 4px solid #d0d7de; margin: 8px 0; padding-left: 8px; }"
        "table { border-collapse: collapse; }"
        "th, td { border: 1px solid #d0d7de; padding: 4px 8px; }"
        "hr { border: 0; border-top: 1px solid #d0d7de; margin: 12px 0; }";
    return css;
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
    font.setFamilies(QStringList{"Segoe UI", "Noto Sans", "Helvetica", "Arial"});
    font.setPointSize(12);
    responseView->setFont(font);
    responseView->document()->setDefaultFont(font);
    responseView->setReadOnly(true);
    responseView->setOpenExternalLinks(true);
    responseView->document()->setDefaultStyleSheet(markdownCss());
    responseView->document()->setDocumentMargin(8);
    responseView->setStyleSheet("QTextBrowser { background-color: #ffffff; }");
    responseView->viewport()->installEventFilter(this);
    new MarkdownCodeHighlighter(responseView->document());
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
    responseView->document()->setMarkdown(responseText, QTextDocument::MarkdownDialectGitHub);
    applyMarkdownStyles();
    if (atBottom)
        bar->setValue(bar->maximum());
}

void TaskWindow::applyMarkdownStyles() {
    if (!responseView)
        return;
    QTextDocument *doc = responseView->document();
    if (!doc)
        return;

    QTextCharFormat inlineCodeFormat;
    inlineCodeFormat.setFontFamilies(QStringList{"Consolas"});
    inlineCodeFormat.setFontFixedPitch(true);
    inlineCodeFormat.setBackground(QColor("#f6f8fa"));
    const QFont baseFont = resolveBaseTextFont(doc);
    if (baseFont.pointSizeF() > 0) {
        inlineCodeFormat.setFontPointSize(baseFont.pointSizeF());
    } else if (baseFont.pixelSize() > 0) {
        inlineCodeFormat.setProperty(QTextFormat::FontPixelSize, baseFont.pixelSize());
    }

    QTextCharFormat blockCodeCharFormat = inlineCodeFormat;

    for (QTextBlock block = doc->begin(); block.isValid(); block = block.next()) {
        if (isCodeBlock(block)) {
            QTextCursor blockCursor(block);
            QTextBlockFormat blockFormat = block.blockFormat();
            blockFormat.setBackground(QColor("#f6f8fa"));
            blockCursor.setBlockFormat(blockFormat);

            blockCursor.select(QTextCursor::BlockUnderCursor);
            blockCursor.mergeCharFormat(blockCodeCharFormat);
            continue;
        }

        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid())
                continue;
            if (!isInlineCodeFormat(fragment.charFormat()))
                continue;
            QTextCursor cursor(doc);
            cursor.setPosition(fragment.position());
            cursor.setPosition(fragment.position() + fragment.length(), QTextCursor::KeepAnchor);
            cursor.mergeCharFormat(inlineCodeFormat);
        }
    }
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
    if (responseView && watched == responseView->viewport() && event->type() == QEvent::Wheel) {
        auto *wheelEvent = static_cast<QWheelEvent *>(event);
        if (wheelEvent->modifiers().testFlag(Qt::ControlModifier)) {
            int delta = wheelEvent->angleDelta().y();
            if (delta == 0)
                delta = wheelEvent->pixelDelta().y();
            if (delta == 0)
                return true;
            int steps = qAbs(delta) / 120;
            if (steps == 0)
                steps = 1;
            if (delta > 0)
                responseView->zoomIn(steps);
            else
                responseView->zoomOut(steps);
            applyMarkdownStyles();
            return true;
        }
    }
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
