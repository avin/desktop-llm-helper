#ifndef TASKWINDOW_H
#define TASKWINDOW_H

#include <QWidget>
#include <QList>
#include <QEvent>
#include <QKeyEvent>
#include <QTimer>

class TaskWidget;

class TaskWindow : public QWidget {
    Q_OBJECT

public:
    explicit TaskWindow(const QList<TaskWidget *> &tasks, QWidget *parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent *ev) override;
    void changeEvent(QEvent *event) override;

private slots:
    void updateLoadingPosition();

private:
    QWidget *loadingWindow;
    QTimer *loadingTimer;

    void showLoadingIndicator();
    void hideLoadingIndicator();
};

#endif // TASKWINDOW_H