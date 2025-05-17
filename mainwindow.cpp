#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "taskwidget.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButtonAddTask_clicked()
{
    TaskWidget* task = new TaskWidget;
    connect(task, &TaskWidget::removeRequested, this, &MainWindow::removeTaskWidget);
    int index = ui->tasksTabWidget->addTab(task, tr("Задача %1").arg(ui->tasksTabWidget->count() + 1));
    ui->tasksTabWidget->setCurrentIndex(index);
}

void MainWindow::removeTaskWidget(TaskWidget* task)
{
    int index = ui->tasksTabWidget->indexOf(task);
    if (index != -1) {
        QWidget* page = ui->tasksTabWidget->widget(index);
        ui->tasksTabWidget->removeTab(index);
        page->deleteLater();
    }
}