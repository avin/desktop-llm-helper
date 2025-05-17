#include "taskwindow.h"
#include "taskwidget.h"

#include <QVBoxLayout>
#include <QPushButton>
#include <QKeyEvent>

TaskWindow::TaskWindow(const QList<TaskWidget*>& tasks, QWidget* parent)
    : QWidget(parent,
              Qt::Window
              | Qt::WindowStaysOnTopHint
              | Qt::CustomizeWindowHint
              | Qt::WindowTitleHint)
{
    setWindowTitle(tr("Выберите задачу"));
    setAttribute(Qt::WA_DeleteOnClose, true);

    auto* layout = new QVBoxLayout(this);

    for (TaskWidget* task : tasks) {
        if (!task) continue;
        const QString text = task->name().isEmpty() ? tr("Задача") : task->name();
        auto* btn = new QPushButton(text, this);
        connect(btn, &QPushButton::clicked, this, [this, task]() {
            // TODO: вызвать выполнение задачи (task)
            close();
        });
        layout->addWidget(btn);
    }
    setLayout(layout);
    layout->setContentsMargins(12, 12, 12, 12);
    adjustSize();

    // Показ сразу после конструктора
    show();
    raise();
    activateWindow();
    setFocus(Qt::OtherFocusReason);
}

void TaskWindow::keyPressEvent(QKeyEvent* ev)
{
    if (ev->key() == Qt::Key_Escape)
        close();
    else
        QWidget::keyPressEvent(ev);
}