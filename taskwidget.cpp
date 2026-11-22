#include "taskwidget.h"
#include "ui_taskwidget.h"
#include "configstore.h"
#include "modelselectbox.h"
#include <QLineEdit>
#include <QTextEdit>
#include <QRadioButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QPushButton>

namespace {
constexpr const char kDefaultModelLabel[] = "Default";
}

TaskWidget::TaskWidget(QWidget *parent)
    : QWidget(parent)
      , ui(new Ui::TaskWidget) {
    ui->setupUi(this);
    ui->radioInsert->setChecked(true);
    ui->textEditPrompt->setAcceptRichText(false);

    connect(ui->lineEditName, &QLineEdit::textChanged, this, &TaskWidget::configChanged);
    connect(ui->textEditPrompt, &QTextEdit::textChanged, this, &TaskWidget::configChanged);
    connect(ui->checkBoxShowInMenu, &QCheckBox::toggled, this, &TaskWidget::configChanged);
    connect(ui->lineEditTaskHotkey, &QLineEdit::textChanged, this, &TaskWidget::configChanged);
    connect(ui->pushButtonClearHotkey, &QToolButton::clicked, ui->lineEditTaskHotkey, &QLineEdit::clear);
    connect(ui->radioInsert, &QRadioButton::toggled, this, &TaskWidget::configChanged);
    connect(ui->radioWindow, &QRadioButton::toggled, this, &TaskWidget::configChanged);

    connect(ui->spinBoxMaxTokens, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TaskWidget::configChanged);
    connect(ui->doubleSpinBoxTemperature, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &TaskWidget::configChanged);
    ui->modelSelectBoxModel->setEmptyLabel(tr(kDefaultModelLabel));
    ui->pushButtonModelParameters->setEnabled(false);
    connect(ui->modelSelectBoxModel, &ModelSelectBox::currentModelChanged,
            this, [this]() {
                selectedModelParameters = QJsonObject();
                ui->pushButtonModelParameters->setEnabled(!effectiveModelName().isEmpty());
                emit configChanged();
            });
    connect(ui->modelSelectBoxModel, &ModelSelectBox::modelsReloadRequested,
            this, &TaskWidget::refreshModelsRequested);
    connect(ui->pushButtonModelParameters, &QPushButton::clicked,
            this, &TaskWidget::modelParametersRequested);

    ui->horizontalLayoutModel->setStretch(0, 0);
    ui->horizontalLayoutModel->setStretch(1, 1);
}

TaskWidget::~TaskWidget() {
    delete ui;
}

QString TaskWidget::name() const {
    return ui->lineEditName->text();
}

QString TaskWidget::prompt() const {
    return ui->textEditPrompt->toPlainText();
}

QString TaskWidget::modelName() const {
    return ui->modelSelectBoxModel->currentModel();
}

QString TaskWidget::effectiveModelName() const {
    const QString selectedModel = modelName();
    return selectedModel.isEmpty() ? defaultModelName : selectedModel;
}

QJsonObject TaskWidget::modelParameters() const {
    return selectedModelParameters;
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

void TaskWidget::setModelName(const QString &modelName) {
    QString normalized = modelName;
    if (normalized == QLatin1String(kDefaultModelLabel))
        normalized.clear();

    ui->modelSelectBoxModel->setCurrentModel(normalized);
    ui->pushButtonModelParameters->setEnabled(!effectiveModelName().isEmpty());
}

void TaskWidget::setDefaultModel(const QString &defaultModel) {
    if (defaultModelName == defaultModel)
        return;

    defaultModelName = defaultModel;
    if (modelName().isEmpty())
        selectedModelParameters = QJsonObject();
    ui->modelSelectBoxModel->setEmptyLabel(defaultModelName.isEmpty()
        ? tr(kDefaultModelLabel) : tr("Default (%1)").arg(defaultModelName));
    ui->pushButtonModelParameters->setEnabled(!effectiveModelName().isEmpty());
}

void TaskWidget::setModelParameters(const QJsonObject &parameters) {
    selectedModelParameters = parameters;
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

void TaskWidget::setResponseWindowSize(const QSize &size) {
    if (!size.isValid())
        return;
    responseWidth = size.width();
    responseHeight = size.height();
}

void TaskWidget::setResponseZoom(int zoom) {
    responseZoomValue = zoom;
}

TaskDefinition TaskWidget::toDefinition() const {
    TaskDefinition def;
    def.name = name();
    def.prompt = prompt();
    def.modelName = modelName();
    def.modelParameters = modelParameters();
    def.hotkey = ui->lineEditTaskHotkey->text();
    def.showInMenu = ui->checkBoxShowInMenu->isChecked();
    def.insertMode = insertMode();
    def.maxTokens = maxTokens();
    def.temperature = temperature();
    def.responseWidth = responseWidth;
    def.responseHeight = responseHeight;
    def.responseZoom = responseZoomValue;
    return def;
}

void TaskWidget::applyDefinition(const TaskDefinition &definition) {
    setName(definition.name);
    setPrompt(definition.prompt);
    setModelName(definition.modelName);
    setModelParameters(definition.modelParameters);
    ui->lineEditTaskHotkey->setText(definition.hotkey);
    ui->checkBoxShowInMenu->setChecked(definition.showInMenu);
    setInsertMode(definition.insertMode);
    setMaxTokens(definition.maxTokens);
    setTemperature(definition.temperature);
    responseWidth = definition.responseWidth;
    responseHeight = definition.responseHeight;
    responseZoomValue = definition.responseZoom;
}

QLineEdit *TaskWidget::hotkeyEdit() const {
    return ui->lineEditTaskHotkey;
}
