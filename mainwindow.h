#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class TaskWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_pushButtonAddTask_clicked();
    void removeTaskWidget(TaskWidget* task);

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H