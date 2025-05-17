#ifndef TASKDIALOG_H
#define TASKDIALOG_H

#include <QDialog>
#include <QList>

class TaskWidget;

class TaskDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TaskDialog(const QList<TaskWidget*>& tasks, QWidget* parent = nullptr);
};

#endif // TASKDIALOG_H