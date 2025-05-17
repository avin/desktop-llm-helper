#include "taskdialog.h"
#include "taskwidget.h"

#include <QVBoxLayout>
#include <QPushButton>

TaskDialog::TaskDialog(const QList<TaskWidget*>& tasks, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Выберите задачу"));
    auto* layout = new QVBoxLayout(this);

    for (TaskWidget* task : tasks) {
        if (!task) continue;
        const QString btnText = task->name().isEmpty() ? tr("Задача") : task->name();
        auto* btn = new QPushButton(btnText, this);
        connect(btn, &QPushButton::clicked, this, [this, task]() {
            // TODO: здесь может быть вызов выполнения task
            accept();
        });
        layout->addWidget(btn);
    }
    setLayout(layout);
    resize(300, tasks.size() * 40 + 40);
}