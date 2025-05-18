#include "taskwindow.h"
#include "taskwidget.h"

#include <QColor>
#include <QCursor>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QGuiApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

TaskWindow::TaskWindow(const QList<TaskWidget *> &tasks, QWidget *parent)
    : QWidget(parent,
              Qt::Window | Qt::WindowStaysOnTopHint | Qt::CustomizeWindowHint
                  | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::StrongFocus);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(0);

    auto *container = new QWidget(this);
    container->setObjectName("container");
    container->setStyleSheet("QWidget#container { "
                             "  background-color: white; "
                             "  border-radius: 0px; "
                             "}");

    auto *shadow = new QGraphicsDropShadowEffect(container);
    shadow->setBlurRadius(10);
    shadow->setColor(QColor(0, 0, 0, 160));
    shadow->setOffset(0, 0);
    container->setGraphicsEffect(shadow);

    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(2, 2, 2, 2);
    layout->setSpacing(0);

    for (TaskWidget *task : tasks) {
        if (!task)
            continue;
        QString text = task->name().isEmpty() ? tr("<Без имени>") : task->name();
        auto *btn = new QPushButton(text, container);
        connect(btn, &QPushButton::clicked, this, [this, task]() {
            close();
#ifdef Q_OS_WIN
            // Симуляция Ctrl+C
            INPUT inputs[4] = {};
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki.wVk = VK_CONTROL;
            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki.wVk = 'C';
            inputs[2].type = INPUT_KEYBOARD;
            inputs[2].ki.wVk = 'C';
            inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
            inputs[3].type = INPUT_KEYBOARD;
            inputs[3].ki.wVk = VK_CONTROL;
            inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(4, inputs, sizeof(INPUT));

            Sleep(50);

            // Чтение и объединение текста
            QClipboard *clipboard = QGuiApplication::clipboard();
            QString original = clipboard->text();
            QString prompt = task->prompt();
            QString combined = prompt + original;
            clipboard->setText(combined);

            // Симуляция Ctrl+V
            INPUT inputs2[4] = {};
            inputs2[0].type = INPUT_KEYBOARD;
            inputs2[0].ki.wVk = VK_CONTROL;
            inputs2[1].type = INPUT_KEYBOARD;
            inputs2[1].ki.wVk = 'V';
            inputs2[2].type = INPUT_KEYBOARD;
            inputs2[2].ki.wVk = 'V';
            inputs2[2].ki.dwFlags = KEYEVENTF_KEYUP;
            inputs2[3].type = INPUT_KEYBOARD;
            inputs2[3].ki.wVk = VK_CONTROL;
            inputs2[3].ki.dwFlags = KEYEVENTF_KEYUP;
            SendInput(4, inputs2, sizeof(INPUT));
#endif
        });
        layout->addWidget(btn);
    }

    mainLayout->addWidget(container);

    adjustSize();

    QPoint cursorPos = QCursor::pos();
    QScreen *screen = QGuiApplication::screenAt(cursorPos);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    QRect available = screen->availableGeometry();
    int x = cursorPos.x();
    int y = cursorPos.y();
    int w = width();
    int h = height();
    if (x + w > available.x() + available.width()) {
        x = available.x() + available.width() - w;
    }
    if (y + h > available.y() + available.height()) {
        y = available.y() + available.height() - h;
    }
    if (x < available.x()) {
        x = available.x();
    }
    if (y < available.y()) {
        y = available.y();
    }
    move(x, y);

    show();
    raise();
    activateWindow();
    setFocus(Qt::OtherFocusReason);
}

void TaskWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::ActivationChange && !isActiveWindow()) {
        close();
    }
    QWidget::changeEvent(event);
}

void TaskWindow::keyPressEvent(QKeyEvent *ev)
{
    if (ev->key() == Qt::Key_Escape)
        close();
    else
        QWidget::keyPressEvent(ev);
}