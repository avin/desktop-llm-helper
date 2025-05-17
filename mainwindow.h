#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>

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
    void loadConfig();
    void saveConfig();
    QString configFilePath() const;
};

#endif // MAINWINDOW_H