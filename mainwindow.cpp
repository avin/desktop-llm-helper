#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "taskwidget.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
#include <QDir>
#include <QLineEdit>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->lineEditApiEndpoint, &QLineEdit::textChanged, this, &MainWindow::saveConfig);
    connect(ui->lineEditModelName, &QLineEdit::textChanged, this, &MainWindow::saveConfig);
    connect(ui->lineEditApiKey, &QLineEdit::textChanged, this, &MainWindow::saveConfig);
    connect(ui->lineEditProxy, &QLineEdit::textChanged, this, &MainWindow::saveConfig);
    connect(ui->lineEditHotkey, &QLineEdit::textChanged, this, &MainWindow::saveConfig);

    loadConfig();
}

MainWindow::~MainWindow()
{
    delete ui;
}

QString MainWindow::configFilePath() const
{
    return QCoreApplication::applicationDirPath()
         + QDir::separator()
         + "config.json";
}

void MainWindow::loadConfig()
{
    QString path = configFilePath();
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return;

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject())
        return;

    QJsonObject root = doc.object();
    QJsonObject settings = root.value("settings").toObject();

    ui->lineEditApiEndpoint->setText(settings.value("apiEndpoint").toString());
    ui->lineEditModelName->setText(settings.value("modelName").toString());
    ui->lineEditApiKey->setText(settings.value("apiKey").toString());
    ui->lineEditProxy->setText(settings.value("proxy").toString());
    ui->lineEditHotkey->setText(settings.value("hotkey").toString());

    QJsonArray tasks = root.value("tasks").toArray();
    for (const QJsonValue &value : tasks) {
        QJsonObject obj = value.toObject();
        TaskWidget* task = new TaskWidget;
        task->setName(obj.value("name").toString());
        task->setPrompt(obj.value("prompt").toString());
        task->setInsertMode(obj.value("insert").toBool(true));

        connect(task, &TaskWidget::removeRequested, this, &MainWindow::removeTaskWidget);
        connect(task, &TaskWidget::configChanged, this, &MainWindow::saveConfig);

        int index = ui->tasksTabWidget->addTab(task,
            tr("Задача %1").arg(ui->tasksTabWidget->count() + 1));
        Q_UNUSED(index);
    }
}

void MainWindow::saveConfig()
{
    QJsonObject root;
    QJsonObject settings;
    settings["apiEndpoint"] = ui->lineEditApiEndpoint->text();
    settings["modelName"]   = ui->lineEditModelName->text();
    settings["apiKey"]      = ui->lineEditApiKey->text();
    settings["proxy"]       = ui->lineEditProxy->text();
    settings["hotkey"]      = ui->lineEditHotkey->text();
    root["settings"] = settings;

    QJsonArray tasksArray;
    for (int i = 0; i < ui->tasksTabWidget->count(); ++i) {
        TaskWidget* task = qobject_cast<TaskWidget*>(
            ui->tasksTabWidget->widget(i));
        if (!task) continue;

        QJsonObject obj;
        obj["name"]   = task->name();
        obj["prompt"] = task->prompt();
        obj["insert"] = task->insertMode();
        tasksArray.append(obj);
    }
    root["tasks"] = tasksArray;

    QJsonDocument doc(root);
    QFile file(configFilePath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
    }
}

void MainWindow::on_pushButtonAddTask_clicked()
{
    TaskWidget* task = new TaskWidget;
    connect(task, &TaskWidget::removeRequested, this, &MainWindow::removeTaskWidget);
    connect(task, &TaskWidget::configChanged,   this, &MainWindow::saveConfig);

    int index = ui->tasksTabWidget->addTab(
        task, tr("Задача %1").arg(ui->tasksTabWidget->count() + 1));
    ui->tasksTabWidget->setCurrentIndex(index);

    saveConfig();
}

void MainWindow::removeTaskWidget(TaskWidget* task)
{
    int index = ui->tasksTabWidget->indexOf(task);
    if (index != -1) {
        QWidget* page = ui->tasksTabWidget->widget(index);
        ui->tasksTabWidget->removeTab(index);
        page->deleteLater();
        saveConfig();
    }
}