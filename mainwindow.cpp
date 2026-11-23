#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "taskwidget.h"
#include "taskwindow.h"
#include "hotkeymanager.h"
#include "fontselectbox.h"
#include "modelselectbox.h"
#include "modellistloader.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QCoreApplication>
#include <QLineEdit>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QFormLayout>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QValidator>
#include <QMetaObject>
#include <QTabBar>
#include <QTabWidget>
#include <QStyle>
#include <QStyleOptionTab>
#include <QStylePainter>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QMenu>
#include <QAction>
#include <QSystemTrayIcon>
#include <QIcon>
#include <QCloseEvent>
#include <QApplication>
#include <QVariant>
#include <QMessageBox>
#include <QStandardPaths>
#include <QThread>
#include <QToolTip>

#include <algorithm>
#include <functional>
#include <limits>

namespace {
constexpr const char kAddTabMarker[] = "add_tab";
constexpr const char kDefaultModelLabel[] = "Default";

class TaskTabBar : public QTabBar {
public:
    explicit TaskTabBar(QWidget *parent = nullptr)
        : QTabBar(parent) {
        setMouseTracking(true);
    }

    using CloseHandler = std::function<void(int)>;

    void setCloseRequestHandler(CloseHandler handler) {
        closeRequestHandler = std::move(handler);
    }

    QSize tabSizeHint(int index) const override {
        QStyleOptionTab opt;
        initStyleOption(&opt, index);
        QFontMetrics fm = opt.fontMetrics;
        QSize textSize = fm.size(Qt::TextShowMnemonic, opt.text);
        QSize iconSize = opt.icon.isNull() ? QSize(0, 0) : opt.iconSize;

        const int hspace = style()->pixelMetric(QStyle::PM_TabBarTabHSpace, &opt, this);
        const int vspace = style()->pixelMetric(QStyle::PM_TabBarTabVSpace, &opt, this);
        int width = textSize.width() + iconSize.width();
        if (!opt.icon.isNull() && !opt.text.isEmpty())
            width += fm.horizontalAdvance(QLatin1Char(' '));
        if (hasCloseButton(index))
            width += closeButtonSide(opt, tabRect(index)) + closeButtonPadding();
        width += hspace;
        int height = qMax(textSize.height(), iconSize.height()) + vspace;
        return QSize(width, height);
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event);
        QStylePainter painter(this);
        QStyleOptionTab opt;
        for (int i = 0; i < count(); ++i) {
            initStyleOption(&opt, i);
            painter.drawControl(QStyle::CE_TabBarTabShape, opt);

            QRect textRect = opt.rect;
            int hspace = style()->pixelMetric(QStyle::PM_TabBarTabHSpace, &opt, this) / 2;
            int vspace = style()->pixelMetric(QStyle::PM_TabBarTabVSpace, &opt, this) / 2;
            textRect.adjust(hspace, vspace, -hspace, -vspace);

            QRect closeRect;
            if (hasCloseButton(i)) {
                closeRect = closeRectForTab(i, opt);
                int rightLimit = closeRect.left() - 4;
                if (rightLimit > textRect.left())
                    textRect.setRight(rightLimit);
            }

            if (!opt.icon.isNull()) {
                QSize iconSize = opt.iconSize.isValid() ? opt.iconSize : QSize(16, 16);
                QRect iconRect = textRect;
                iconRect.setWidth(iconSize.width());
                iconRect.setHeight(iconSize.height());
                iconRect.moveTop(textRect.top() + (textRect.height() - iconSize.height()) / 2);
                QIcon::Mode iconMode = (opt.state & QStyle::State_Enabled) ? QIcon::Normal : QIcon::Disabled;
                QIcon::State iconState = (opt.state & QStyle::State_Selected) ? QIcon::On : QIcon::Off;
                painter.drawItemPixmap(iconRect, Qt::AlignLeft | Qt::AlignVCenter,
                                       opt.icon.pixmap(iconSize, iconMode, iconState));
                textRect.setLeft(iconRect.right() + 4);
            }

            Qt::Alignment textAlign = Qt::AlignVCenter | Qt::AlignLeft;
            if (opt.text == "+")
                textAlign = Qt::AlignCenter;
            style()->drawItemText(&painter, textRect, textAlign, opt.palette,
                                  opt.state & QStyle::State_Enabled, opt.text, QPalette::WindowText);

            if (shouldShowClose(i))
                drawCloseButton(painter, opt, closeRect, isCloseHot(i));
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        updateHoverState(event->pos());
        QTabBar::mouseMoveEvent(event);
    }

    void leaveEvent(QEvent *event) override {
        hoverIndex = -1;
        hoverCloseIndex = -1;
        pressIndex = -1;
        update();
        QTabBar::leaveEvent(event);
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            int index = tabAt(event->pos());
            if (shouldShowClose(index)) {
                QStyleOptionTab opt;
                initStyleOption(&opt, index);
                QRect closeRect = closeHitRectForTab(index, opt);
                if (closeRect.contains(event->pos())) {
                    pressIndex = index;
                    update();
                    event->accept();
                    return;
                }
            }
        }
        QTabBar::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton && pressIndex != -1) {
            int index = pressIndex;
            pressIndex = -1;
            QStyleOptionTab opt;
            initStyleOption(&opt, index);
            QRect closeRect = closeHitRectForTab(index, opt);
            bool shouldClose = (tabAt(event->pos()) == index) && closeRect.contains(event->pos());
            updateHoverState(event->pos());
            if (shouldClose && closeRequestHandler) {
                closeRequestHandler(index);
                event->accept();
                return;
            }
        }
        QTabBar::mouseReleaseEvent(event);
    }

private:
    CloseHandler closeRequestHandler;
    int hoverIndex = -1;
    int hoverCloseIndex = -1;
    int pressIndex = -1;

    bool isAddTab(int index) const {
        if (index < 0 || index >= count())
            return false;
        return tabData(index).toString() == QLatin1String(kAddTabMarker);
    }

    bool hasCloseButton(int index) const {
        return index >= 0 && index < count() && !isAddTab(index);
    }

    bool shouldShowClose(int index) const {
        return hasCloseButton(index) && (index == hoverIndex || index == pressIndex);
    }

    bool isCloseHot(int index) const {
        return hasCloseButton(index) && (index == hoverCloseIndex || index == pressIndex);
    }

    int closeButtonSide(const QStyleOptionTab &opt, const QRect &tabRect) const {
        int base = qMax(12, opt.fontMetrics.height() - 2);
        int maxSide = qMax(10, tabRect.height() - 6);
        return qMin(base, maxSide);
    }

    int closeButtonPadding() const {
        return 6;
    }

    int closeHitPadding() const {
        return 4;
    }

    QRect closeRectForTab(int index, const QStyleOptionTab &opt) const {
        QRect rect = tabRect(index);
        int side = closeButtonSide(opt, rect);
        int padding = closeButtonPadding();
        int x = rect.right() - padding - side;
        int y = rect.center().y() - (side / 2);
        return QRect(x, y, side, side);
    }

    QRect closeHitRectForTab(int index, const QStyleOptionTab &opt) const {
        QRect rect = closeRectForTab(index, opt);
        int padding = closeHitPadding();
        rect.adjust(-padding, -padding, padding, padding);
        return rect;
    }

    void drawCloseButton(QStylePainter &painter, const QStyleOptionTab &opt,
                         const QRect &rect, bool hot) const {
        if (!rect.isValid())
            return;
        qreal inset = qMax(2.0, rect.width() / 4.0);
        QRectF drawRect = QRectF(rect).adjusted(inset, inset, -inset, -inset);
        if (drawRect.width() < 2.0 || drawRect.height() < 2.0)
            return;
        QColor color = hot ? QColor(200, 60, 60) : opt.palette.color(QPalette::WindowText);
        if (!hot)
            color.setAlphaF(0.7);
        qreal penWidth = qMax(1.4, drawRect.width() / 6.0);
        QPen pen(color, penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(pen);
        painter.drawLine(drawRect.topLeft(), drawRect.bottomRight());
        painter.drawLine(drawRect.topRight(), drawRect.bottomLeft());
        painter.restore();
    }

    void updateHoverState(const QPoint &pos) {
        int newHoverIndex = tabAt(pos);
        int newHoverCloseIndex = -1;
        if (hasCloseButton(newHoverIndex)) {
            QStyleOptionTab opt;
            initStyleOption(&opt, newHoverIndex);
            QRect closeRect = closeHitRectForTab(newHoverIndex, opt);
            if (closeRect.contains(pos))
                newHoverCloseIndex = newHoverIndex;
        }
        if (newHoverIndex != hoverIndex || newHoverCloseIndex != hoverCloseIndex) {
            hoverIndex = newHoverIndex;
            hoverCloseIndex = newHoverCloseIndex;
            update();
        }
    }
};

class TaskTabWidget : public QTabWidget {
public:
    explicit TaskTabWidget(QWidget *parent = nullptr)
        : QTabWidget(parent) {
        setTabBar(new TaskTabBar(this));
    }
};
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
      , ui(new Ui::MainWindow)
      , hotkeyManager(new HotkeyManager(this))
      , loadingConfig(false)
      , trayIcon(nullptr)
      , latestTaskWindow(nullptr)
      , modelLoaderThread(new QThread(this))
      , modelListLoader(new ModelListLoader)
      , nextModelRequestId(0)
      , hasCachedModelList(false) {
    qRegisterMetaType<ModelInfoList>("ModelInfoList");
    ui->setupUi(this);
    // Include application name in the window title
    setWindowTitle(QCoreApplication::applicationName() + " - " + tr("Settings"));
    ui->modelSelectBoxModelName->setEmptyLabel(tr("Not selected"));
    ui->pushButtonModelParameters->setEnabled(false);

    auto *oldTabs = ui->tasksTabWidget;
    auto *newTabs = new TaskTabWidget(ui->tab_2);
    newTabs->setObjectName(oldTabs->objectName());
    int tabsIndex = ui->verticalLayoutTab2->indexOf(oldTabs);
    ui->verticalLayoutTab2->removeWidget(oldTabs);
    if (tabsIndex < 0)
        ui->verticalLayoutTab2->addWidget(newTabs);
    else
        ui->verticalLayoutTab2->insertWidget(tabsIndex, newTabs);
    oldTabs->deleteLater();
    ui->tasksTabWidget = newTabs;

    ui->tasksTabWidget->setTabPosition(QTabWidget::West);
    ui->tasksTabWidget->tabBar()->setExpanding(false);
    ui->tasksTabWidget->setMovable(true);
    auto *taskTabBar = static_cast<TaskTabBar *>(ui->tasksTabWidget->tabBar());
    taskTabBar->setCloseRequestHandler([this](int index) {
        requestCloseTask(index);
    });
    connect(ui->tasksTabWidget->tabBar(), &QTabBar::tabMoved,
            this, &MainWindow::handleTaskTabMoved);
    connect(ui->tasksTabWidget->tabBar(), &QTabBar::tabBarClicked,
            this, &MainWindow::handleTaskTabClicked);
    ensureAddTab();

    connect(ui->lineEditApiEndpoint, &QLineEdit::textChanged, this, [this]() {
        if (loadingConfig)
            return;
        defaultModelParameters = QJsonObject();
        for (int i = 0; i < ui->tasksTabWidget->count(); ++i) {
            if (auto *task = qobject_cast<TaskWidget *>(ui->tasksTabWidget->widget(i)))
                task->setModelParameters(QJsonObject());
        }
        saveConfig();
    });
    connect(ui->modelSelectBoxModelName, &ModelSelectBox::currentModelChanged,
            this, [this](const QString &modelId) {
                defaultModelParameters = QJsonObject();
                ui->pushButtonModelParameters->setEnabled(!modelId.isEmpty());
                for (int i = 0; i < ui->tasksTabWidget->count(); ++i) {
                    if (auto *task = qobject_cast<TaskWidget *>(ui->tasksTabWidget->widget(i)))
                        task->setDefaultModel(modelId);
                }
                saveConfig();
            });
    connect(ui->pushButtonModelParameters, &QPushButton::clicked, this, [this]() {
        editModelParameters(nullptr);
    });
    connect(ui->lineEditApiKey, &QLineEdit::textChanged, this, &MainWindow::saveConfig);
    connect(ui->lineEditProxy, &QLineEdit::textChanged, this, &MainWindow::saveConfig);
    connect(ui->lineEditHotkey, &QLineEdit::textChanged, this, &MainWindow::saveConfig);
    connect(ui->lineEditMaxChars, &QLineEdit::textChanged, this, &MainWindow::saveConfig);
    connect(ui->pushButtonChatFonts, &QPushButton::clicked, this, &MainWindow::editChatFonts);
    connect(ui->modelSelectBoxModelName, &ModelSelectBox::modelsReloadRequested,
            this, &MainWindow::requestModelList);
    connect(ui->pushButtonExportSettings, &QPushButton::clicked,
            this, &MainWindow::exportSettings);
    connect(ui->pushButtonImportSettings, &QPushButton::clicked,
            this, &MainWindow::importSettings);

    ui->lineEditHotkey->installEventFilter(this);

    createTrayIcon();

    connect(hotkeyManager, &HotkeyManager::hotkeyPressed,
            this, &MainWindow::handleGlobalHotkey);
    connect(hotkeyManager, &HotkeyManager::hotkeyCaptured,
            this, &MainWindow::setHotkeyText);

    modelListLoader->moveToThread(modelLoaderThread);
    connect(modelLoaderThread, &QThread::finished, modelListLoader, &QObject::deleteLater);
    connect(this, &MainWindow::modelListLoadRequested,
            modelListLoader, &ModelListLoader::loadModels);
    connect(modelListLoader, &ModelListLoader::modelsLoaded,
            this, &MainWindow::handleModelListLoaded);
    connect(modelListLoader, &ModelListLoader::loadFailed,
            this, &MainWindow::handleModelListFailed);
    modelLoaderThread->start();

    loadConfig();
}

MainWindow::~MainWindow() {
    modelLoaderThread->quit();
    modelLoaderThread->wait();
    delete ui;
}

bool MainWindow::eventFilter(QObject *obj, QEvent *ev) {
    if (ev->type() == QEvent::FocusIn) {
        hotkeyField = qobject_cast<QLineEdit *>(obj);
        hotkeyManager->setCapturing(!hotkeyField.isNull());
    } else if (ev->type() == QEvent::FocusOut && obj == hotkeyField) {
        hotkeyField.clear();
        hotkeyManager->setCapturing(false);
    }
    return QMainWindow::eventFilter(obj, ev);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    event->ignore();
    hide();
}

void MainWindow::setHotkeyText(const QString &text) {
    if (!hotkeyField)
        return;
    if (HotkeyManager::hotkeyCode(text) == HotkeyManager::hotkeyCode(hotkeyField->text()))
        return;
    if (hotkeyManager->isHotkeyAssigned(text)) {
        QToolTip::showText(hotkeyField->mapToGlobal(hotkeyField->rect().bottomLeft()),
                           tr("This hotkey is already assigned."), hotkeyField);
        return;
    }
    QToolTip::hideText();
    hotkeyField->setText(text);
}

void MainWindow::applyDefaultSettings() {
    applyConfig(ConfigStore::defaultConfig());
}

void MainWindow::loadConfig() {
    const QString path = ConfigStore::configFilePath();
    if (!QFile::exists(path)) {
        loadingConfig = true;
        applyDefaultSettings();
        loadingConfig = false;
        saveConfig();
        return;
    }

    AppConfig config;
    if (!ConfigStore::loadFromFile(path, &config))
        return;

    loadingConfig = true;
    applyConfig(config);
    loadingConfig = false;
    updateHotkeys(config);
}

void MainWindow::saveConfig() {
    if (loadingConfig)
        return;

    const AppConfig config = buildConfigFromUi();
    ConfigStore::saveToFile(ConfigStore::configFilePath(), config);

    updateHotkeys(config);
}

void MainWindow::updateHotkeys(const AppConfig &config) {
    QStringList taskHotkeys;
    for (const TaskDefinition &task : config.tasks)
        taskHotkeys.append(task.hotkey);
    hotkeyManager->setHotkeys(config.settings.hotkey, taskHotkeys);
}

QString MainWindow::suggestedSettingsPath() const {
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (baseDir.isEmpty())
        baseDir = QDir::homePath();
    return QDir(baseDir).filePath("DesktopLLMHelper_settings.json");
}

void MainWindow::exportSettings() {
    const QString filter = tr("JSON Files (*.json)");
    const QString path = QFileDialog::getSaveFileName(
        this,
        tr("Export Settings"),
        suggestedSettingsPath(),
        filter
    );
    if (path.isEmpty())
        return;

    const AppConfig config = buildConfigFromUi();
    if (!ConfigStore::saveToFile(path, config)) {
        QMessageBox::warning(this, tr("Export Settings"),
                             tr("Failed to export settings."));
    }
}

void MainWindow::importSettings() {
    const QString filter = tr("JSON Files (*.json);;All Files (*)");
    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Import Settings"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        filter
    );
    if (path.isEmpty())
        return;

    AppConfig config;
    if (!ConfigStore::loadFromFile(path, &config)) {
        QMessageBox::warning(this, tr("Import Settings"),
                             tr("Failed to import settings."));
        return;
    }

    loadingConfig = true;
    applyConfig(config);
    loadingConfig = false;
    saveConfig();
}

void MainWindow::handleTaskTabClicked(int index) {
    if (!isAddTabIndex(index))
        return;

    TaskDefinition definition;
    addTaskTab(definition, true);
    saveConfig();
}

void MainWindow::handleTaskTabMoved(int, int) {
    ensureAddTabLast();
    saveConfig();
}

void MainWindow::requestCloseTask(int index) {
    if (isAddTabIndex(index))
        return;

    auto *task = qobject_cast<TaskWidget *>(ui->tasksTabWidget->widget(index));
    if (!task)
        return;

    QString name = task->name().trimmed();
    QString title = tr("Remove Task");
    QString message = name.isEmpty()
        ? tr("Do you want to remove this task?")
        : tr("Do you want to remove \"%1\"?").arg(name);

    if (QMessageBox::question(this, title, message,
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    removeTaskWidget(task);
}

void MainWindow::removeTaskWidget(TaskWidget *task) {
    int index = ui->tasksTabWidget->indexOf(task);
    if (index != -1) {
        QWidget *page = ui->tasksTabWidget->widget(index);
        ui->tasksTabWidget->removeTab(index);
        page->deleteLater();
        saveConfig();
    }
}

void MainWindow::updateTaskResponsePrefs(int taskIndex, const QSize &size, int zoom) {
    if (taskIndex < 0 || taskIndex >= ui->tasksTabWidget->count())
        return;
    if (isAddTabIndex(taskIndex))
        return;
    auto *task = qobject_cast<TaskWidget *>(ui->tasksTabWidget->widget(taskIndex));
    if (!task)
        return;
    task->setResponseWindowSize(size);
    task->setResponseZoom(zoom);
}

void MainWindow::commitTaskResponsePrefs() {
    saveConfig();
}

void MainWindow::applyConfig(const AppConfig &config) {
    ui->lineEditApiEndpoint->setText(config.settings.apiEndpoint);
    ui->lineEditApiKey->setText(config.settings.apiKey);
    ui->lineEditProxy->setText(config.settings.proxy);
    ui->lineEditHotkey->setText(config.settings.hotkey);
    ui->lineEditMaxChars->setText(QString::number(config.settings.maxChars));
    setDefaultModel(config.settings.modelName);
    defaultModelParameters = config.settings.modelParameters;
    chatFonts = config.settings.chatFonts;
    emit chatFontsChanged(chatFonts);

    clearTasks();
    for (const TaskDefinition &task : config.tasks)
        addTaskTab(task, false);

    ensureAddTabLast();
    int addIndex = addTabIndex();
    if (addIndex > 0 || (addIndex == -1 && ui->tasksTabWidget->count() > 0))
        ui->tasksTabWidget->setCurrentIndex(0);
}

AppConfig MainWindow::buildConfigFromUi() const {
    AppConfig config;
    config.settings.apiEndpoint = ui->lineEditApiEndpoint->text();
    config.settings.modelName = currentDefaultModel();
    config.settings.modelParameters = defaultModelParameters;
    config.settings.apiKey = ui->lineEditApiKey->text();
    config.settings.proxy = ui->lineEditProxy->text();
    config.settings.hotkey = ui->lineEditHotkey->text();
    config.settings.maxChars = ui->lineEditMaxChars->text().toInt();
    config.settings.chatFonts = chatFonts;
    config.tasks = currentTaskDefinitions();
    return config;
}

QList<TaskDefinition> MainWindow::currentTaskDefinitions() const {
    QList<TaskDefinition> list;
    for (int i = 0; i < ui->tasksTabWidget->count(); ++i) {
        if (isAddTabIndex(i))
            continue;
        if (auto *task = qobject_cast<TaskWidget *>(ui->tasksTabWidget->widget(i)))
            list.append(task->toDefinition());
    }
    return list;
}

void MainWindow::addTaskTab(const TaskDefinition &definition, bool makeCurrent) {
    auto *task = new TaskWidget;
    task->setDefaultModel(currentDefaultModel());
    task->applyDefinition(definition);
    connectTaskSignals(task);

    QString tabLabel = task->name().isEmpty() ? tr("<Unnamed>") : task->name();
    int insertIndex = addTabIndex();
    if (insertIndex < 0)
        insertIndex = ui->tasksTabWidget->count();
    int index = ui->tasksTabWidget->insertTab(insertIndex, task, tabLabel);
    if (makeCurrent)
        ui->tasksTabWidget->setCurrentIndex(index);
}

void MainWindow::connectTaskSignals(TaskWidget *task) {
    task->hotkeyEdit()->installEventFilter(this);
    connect(task, &TaskWidget::configChanged, this, &MainWindow::saveConfig);
    connect(task, &TaskWidget::configChanged, this, [this, task]() {
        updateTaskTabTitle(task);
    });
    connect(task, &TaskWidget::refreshModelsRequested,
            this, &MainWindow::requestModelList);
    connect(task, &TaskWidget::modelParametersRequested, this, [this, task]() {
        editModelParameters(task);
    });
}

void MainWindow::updateTaskTabTitle(TaskWidget *task) {
    int idx = ui->tasksTabWidget->indexOf(task);
    if (idx != -1) {
        QString label = task->name().isEmpty() ? tr("<Unnamed>") : task->name();
        ui->tasksTabWidget->setTabText(idx, label);
    }
}

void MainWindow::clearTasks() {
    for (int i = ui->tasksTabWidget->count() - 1; i >= 0; --i) {
        if (isAddTabIndex(i))
            continue;
        QWidget *page = ui->tasksTabWidget->widget(i);
        ui->tasksTabWidget->removeTab(i);
        page->deleteLater();
    }
}

int MainWindow::addTabIndex() const {
    QTabBar *bar = ui->tasksTabWidget->tabBar();
    for (int i = 0; i < bar->count(); ++i) {
        if (bar->tabData(i).toString() == QLatin1String(kAddTabMarker))
            return i;
    }
    return -1;
}

bool MainWindow::isAddTabIndex(int index) const {
    QTabBar *bar = ui->tasksTabWidget->tabBar();
    if (index < 0 || index >= bar->count())
        return false;
    return bar->tabData(index).toString() == QLatin1String(kAddTabMarker);
}

void MainWindow::ensureAddTab() {
    if (addTabIndex() != -1)
        return;

    auto *placeholder = new QWidget;
    int index = ui->tasksTabWidget->addTab(placeholder, "+");
    QTabBar *bar = ui->tasksTabWidget->tabBar();
    bar->setTabData(index, QLatin1String(kAddTabMarker));
    bar->setTabToolTip(index, tr("Add Task"));
    bar->updateGeometry();
    ensureAddTabLast();
}

void MainWindow::ensureAddTabLast() {
    int addIndex = addTabIndex();
    if (addIndex < 0)
        return;
    int lastIndex = ui->tasksTabWidget->count() - 1;
    if (addIndex != lastIndex)
        ui->tasksTabWidget->tabBar()->moveTab(addIndex, lastIndex);
}

QString MainWindow::currentDefaultModel() const {
    return ui->modelSelectBoxModelName->currentModel();
}

void MainWindow::setDefaultModel(const QString &defaultModel) {
    QString normalized = defaultModel.trimmed();
    if (normalized == QLatin1String(kDefaultModelLabel))
        normalized.clear();
    ui->modelSelectBoxModelName->setCurrentModel(normalized);
    ui->pushButtonModelParameters->setEnabled(!normalized.isEmpty());
}

void MainWindow::requestModelList(ModelSelectBox *target, int generation) {
    QPointer<ModelSelectBox> targetSelector(target);
    loadModelList([targetSelector, generation](const ModelInfoList &models, const QString &error) {
        if (!targetSelector || targetSelector->currentReloadGeneration() != generation)
            return;
        if (error.isEmpty())
            targetSelector->setModels(models);
        else
            targetSelector->setLoadError(error);
    });
}

void MainWindow::loadModelList(ModelListCallback callback) {
    const ModelListRequestParams params{
        ui->lineEditApiEndpoint->text().trimmed(),
        ui->lineEditApiKey->text().trimmed(),
        ui->lineEditProxy->text().trimmed()
    };

    if (hasCachedModelList && cachedModelListParams == params) {
        callback(cachedModelList, QString());
        return;
    }

    const int requestId = ++nextModelRequestId;
    pendingModelRequests.insert(requestId, PendingModelRequest{params, std::move(callback)});
    emit modelListLoadRequested(requestId, params.baseUrl, params.apiKey, params.proxyText);
}

void MainWindow::handleModelListLoaded(int requestId, const ModelInfoList &models) {
    if (!pendingModelRequests.contains(requestId))
        return;

    const PendingModelRequest request = pendingModelRequests.take(requestId);
    hasCachedModelList = true;
    cachedModelListParams = request.params;
    cachedModelList = models;

    request.callback(models, QString());
}

void MainWindow::handleModelListFailed(int requestId, const QString &message) {
    if (!pendingModelRequests.contains(requestId))
        return;

    const PendingModelRequest request = pendingModelRequests.take(requestId);
    request.callback(ModelInfoList(), message);
}

void MainWindow::editChatFonts() {
    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Chat Fonts"));
    dialog->setMinimumWidth(360);
    auto *layout = new QFormLayout(dialog);

    auto *messageFont = new FontSelectBox(FontSelectBox::Filter::All, chatFonts.messageFamily, dialog);
    auto *codeFont = new FontSelectBox(FontSelectBox::Filter::Monospaced, chatFonts.codeFamily, dialog);
    layout->addRow(tr("Message font"), messageFont);
    layout->addRow(tr("Code font"), codeFont);

    auto *preview = new QGroupBox(tr("Preview"), dialog);
    auto *previewLayout = new QVBoxLayout(preview);
    auto *messageSample = new QLabel(tr("Here is a short example."), preview);
    auto *codeSample = new QLabel(QStringLiteral("const auto answer = 42;"), preview);
    previewLayout->addWidget(messageSample);
    previewLayout->addWidget(codeSample);
    layout->addRow(preview);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    const auto updatePreview = [=]() {
        messageSample->setFont(QFont(messageFont->currentValue(), ChatFonts::pointSize));
        codeSample->setFont(QFont(codeFont->currentValue(), ChatFonts::pointSize));
        buttons->button(QDialogButtonBox::Ok)->setEnabled(
            !messageFont->currentValue().isEmpty() && !codeFont->currentValue().isEmpty());
    };
    connect(messageFont, &FontSelectBox::currentValueChanged, dialog, updatePreview);
    connect(codeFont, &FontSelectBox::currentValueChanged, dialog, updatePreview);
    updatePreview();
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    connect(dialog, &QDialog::accepted, this, [this, messageFont, codeFont]() {
        chatFonts.messageFamily = messageFont->currentValue();
        chatFonts.codeFamily = codeFont->currentValue();
        emit chatFontsChanged(chatFonts);
        saveConfig();
    });
    dialog->open();
}

void MainWindow::editModelParameters(TaskWidget *task) {
    const QString modelId = task ? task->effectiveModelName() : currentDefaultModel();
    if (modelId.isEmpty())
        return;

    const QJsonObject saved = task ? task->modelParameters() : defaultModelParameters;
    const bool inherits = task && task->modelName().isEmpty();
    const QJsonObject inherited = inherits ? defaultModelParameters : QJsonObject();
    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Model Parameters"));
    auto *layout = new QFormLayout(dialog);
    auto *status = new QLabel(tr("Loading model parameters..."), dialog);
    status->setTextFormat(Qt::PlainText);
    status->setWordWrap(true);
    layout->addRow(status);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    auto *ok = buttons->button(QDialogButtonBox::Ok);
    ok->setEnabled(false);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    dialog->open();

    loadModelList([this, dialog = QPointer<QDialog>(dialog), layout, status, buttons, ok,
                   modelId, saved, inherits, inherited,
                   task = QPointer<TaskWidget>(task), taskScope = task != nullptr]
                  (const ModelInfoList &models, const QString &error) {
        if (!dialog)
            return;
        if (!error.isEmpty()) {
            status->setText(error);
            return;
        }

        ModelInfo model;
        for (const ModelInfo &entry : models) {
            if (entry.id == modelId) {
                model = entry;
                break;
            }
        }

        int row = 1;
        auto unsetLabel = [&](const QString &value) {
            if (!inherits)
                return tr("Provider default");
            return value.isEmpty() ? tr("Inherit from General Settings") : tr("Inherit (%1)").arg(value);
        };
        auto addChoice = [&](const QString &label, const QStringList &values,
                             const QString &value, const QString &inheritedValue) {
            auto *choice = new QComboBox(dialog);
            choice->addItem(unsetLabel(inheritedValue));
            choice->addItems(values);
            choice->setCurrentIndex(qMax(0, choice->findText(value)));
            layout->insertRow(row++, label, choice);
            return choice;
        };

        QComboBox *effort = nullptr;
        const bool useReasoningObject = model.supportedParameters.contains("reasoning");
        if (!model.reasoningEfforts.isEmpty()
            && (useReasoningObject || model.supportedParameters.contains("reasoning_effort"))) {
            auto reasoningEffort = [](const QJsonObject &parameters) {
                return parameters.contains("reasoning")
                    ? parameters.value("reasoning").toObject().value("effort").toString()
                    : parameters.value("reasoning_effort").toString();
            };
            effort = addChoice(tr("Reasoning effort"), model.reasoningEfforts,
                               reasoningEffort(saved), reasoningEffort(inherited));
        }

        QComboBox *verbosity = nullptr;
        if (model.supportedParameters.contains("verbosity")) {
            verbosity = addChoice(tr("Verbosity"), {"low", "medium", "high"},
                                  saved.value("verbosity").toString(), inherited.value("verbosity").toString());
        }

        QHash<QString, QLineEdit *> numbers;
        for (const QString &key : {QStringLiteral("top_p"), QStringLiteral("top_k")}) {
            if (!model.supportedParameters.contains(key))
                continue;
            auto *edit = new QLineEdit(dialog);
            const bool isTopP = key == QLatin1String("top_p");
            QValidator *validator;
            if (isTopP) {
                auto *decimal = new QDoubleValidator(0.0, 1.0, 6, edit);
                decimal->setNotation(QDoubleValidator::StandardNotation);
                validator = decimal;
            } else {
                validator = new QIntValidator(0, std::numeric_limits<int>::max(), edit);
            }
            QLocale locale = QLocale::c();
            locale.setNumberOptions(QLocale::RejectGroupSeparator);
            validator->setLocale(locale);
            edit->setValidator(validator);
            const QJsonValue inheritedValue = inherited.value(key);
            edit->setPlaceholderText(unsetLabel(inheritedValue.isDouble()
                ? QString::number(inheritedValue.toDouble()) : QString()));
            const QJsonValue value = saved.value(key);
            if (value.isDouble())
                edit->setText(isTopP ? QString::number(value.toDouble(), 'f', 6)
                                    : QString::number(value.toInt()));
            layout->insertRow(row++, isTopP ? tr("Top P") : tr("Top K"), edit);
            numbers.insert(key, edit);
        }

        if (row == 1) {
            status->setText(saved.isEmpty()
                ? tr("The provider catalog lists no configurable parameters for this model.")
                : tr("The provider catalog lists no configurable parameters for this model. "
                     "Click OK to clear the saved parameters."));
            ok->setEnabled(!saved.isEmpty());
        } else {
            status->hide();
            layout->removeWidget(status);
            auto updateOk = [ok, numbers]() {
                for (const auto *edit : numbers) {
                    if (!edit->text().isEmpty() && !edit->hasAcceptableInput()) {
                        ok->setEnabled(false);
                        return;
                    }
                }
                ok->setEnabled(true);
            };
            for (auto *edit : numbers)
                connect(edit, &QLineEdit::textChanged, dialog, updateOk);
            updateOk();
        }
        connect(buttons, &QDialogButtonBox::accepted, dialog,
                [this, dialog, effort, verbosity, useReasoningObject, numbers, task, taskScope]() {
            if (taskScope && !task) {
                dialog->reject();
                return;
            }
            QJsonObject parameters;
            if (effort && effort->currentIndex() > 0) {
                const QString value = effort->currentText();
                if (useReasoningObject)
                    parameters.insert("reasoning", QJsonObject{{"effort", value}});
                else
                    parameters.insert("reasoning_effort", value);
            }
            if (verbosity && verbosity->currentIndex() > 0)
                parameters.insert("verbosity", verbosity->currentText());
            for (auto it = numbers.cbegin(); it != numbers.cend(); ++it) {
                if (!it.value()->text().isEmpty())
                    parameters.insert(it.key(), it.value()->text().toDouble());
            }
            if (task)
                task->setModelParameters(parameters);
            else
                defaultModelParameters = parameters;
            saveConfig();
            dialog->accept();
        });
    });
}

void MainWindow::handleGlobalHotkey(int taskIndex) {
    if (latestTaskWindow && latestTaskWindow->isCapturingSelection())
        return;

    const AppConfig config = buildConfigFromUi();
    const bool openMenu = taskIndex == HotkeyManager::MenuHotkey;
    if (openMenu && std::none_of(config.tasks.cbegin(), config.tasks.cend(),
                                [](const TaskDefinition &task) { return task.showInMenu; }))
        return;
    if (!openMenu && (taskIndex < 0 || taskIndex >= config.tasks.size()))
        return;

    if (latestTaskWindow && !latestTaskWindow->hasResponseWindow())
        latestTaskWindow->close();
    auto *taskWindow = new TaskWindow(config.tasks, config.settings);
    latestTaskWindow = taskWindow;
    connect(qApp, &QApplication::aboutToQuit, taskWindow, &QWidget::close);
    connect(this, &MainWindow::chatFontsChanged, taskWindow, &TaskWindow::setChatFonts);
    connect(taskWindow, &TaskWindow::taskResponsePrefsChanged,
            this, &MainWindow::updateTaskResponsePrefs);
    connect(taskWindow, &TaskWindow::taskResponsePrefsCommitRequested,
            this, &MainWindow::commitTaskResponsePrefs);
    if (openMenu)
        taskWindow->showMenu();
    else
        taskWindow->runTask(taskIndex);
}

void MainWindow::createTrayIcon() {
    QMenu *trayMenu = new QMenu(this);
    QAction *restoreAction = trayMenu->addAction(tr("Settings"));
    QAction *quitAction = trayMenu->addAction(tr("Exit"));

    connect(restoreAction, &QAction::triggered, this, [this]() {
        showNormal();
        raise();
        activateWindow();
    });
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":/icons/app.png"));
    trayIcon->setToolTip(QCoreApplication::applicationName());
    trayIcon->setContextMenu(trayMenu);
    connect(trayIcon, &QSystemTrayIcon::activated,
            this, &MainWindow::onTrayIconActivated);
    trayIcon->show();
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger ||
        reason == QSystemTrayIcon::DoubleClick) {
        showNormal();
        raise();
        activateWindow();
    }
}
