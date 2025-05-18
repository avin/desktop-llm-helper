#include "taskwidget.h"
#include "ui_taskwidget.h"
#include <QLineEdit>
#include <QTextEdit>
#include <QRadioButton>
#include <QSpinBox>
#include <QDoubleSpinBox>

TaskWidget::TaskWidget(QWidget *parent)
    : QWidget(parent)
      , ui(new Ui::TaskWidget) {
    ui->setupUi(this);
    ui->radioInsert->setChecked(true);

    connect(ui->lineEditName, &QLineEdit::textChanged, this, &TaskWidget::configChanged);
    connect(ui->textEditPrompt, &QTextEdit::textChanged, this, &TaskWidget::configChanged);
    connect(ui->radioInsert, &QRadioButton::toggled, this, &TaskWidget::configChanged);
    connect(ui->radioWindow, &QRadioButton::toggled, this, &TaskWidget::configChanged);

    connect(ui->spinBoxMaxTokens, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TaskWidget::configChanged);
    connect(ui->doubleSpinBoxTemperature, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &TaskWidget::configChanged);
}

TaskWidget::~TaskWidget() {
    delete ui;
}

void TaskWidget::on_pushButtonDelete_clicked() {
    emit removeRequested(this);
}

QString TaskWidget::name() const {
    return ui->lineEditName->text();
}

QString TaskWidget::prompt() const {
    return ui->textEditPrompt->toPlainText();
}

bool TaskWidget::insertMode() const {
    return ui->radioInsert->isChecked();
}

void TaskWidget::setName(const QString &name) {
    ui->lineEditName->setText(name);
}

void TaskWidget::setPrompt(const QString &prompt) {
    ui->textEditPrompt->setPlainText(prompt);
}

void TaskWidget::setInsertMode(bool insert) {
    if (insert)
        ui->radioInsert->setChecked(true);
    else
        ui->radioWindow->setChecked(true);
}

int TaskWidget::maxTokens() const {
    return ui->spinBoxMaxTokens->value();
}

double TaskWidget::temperature() const {
    return ui->doubleSpinBoxTemperature->value();
}

void TaskWidget::setMaxTokens(int tokens) {
    ui->spinBoxMaxTokens->setValue(tokens);
}

void TaskWidget::setTemperature(double temp) {
    ui->doubleSpinBoxTemperature->setValue(temp);
}