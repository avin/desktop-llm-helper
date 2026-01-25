#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "taskwidget.h"
#include "taskwindow.h"
#include "hotkeymanager.h"

#include <QFile>
#include <QJsonDocument>
#include <QCoreApplication>
#include <QLineEdit>
#include <QMetaObject>
#include <QTabBar>
#include <QTabWidget>
#include <QStyle>
#include <QStyleOptionTab>
#include <QStylePainter>
#include <QFontMetrics>
#include <QMenu>
#include <QAction>
#include <QSystemTrayIcon>
#include <QIcon>
#include <QCloseEvent>
#include <QApplication>
#include <QVariant>

#include <windows.h>

QPointer<MainWindow> MainWindow::instance = nullptr;

namespace {
constexpr const char kAddTabMarker[] = "add_tab";

class TaskTabBar : public QTabBar {
public:
    explicit TaskTabBar(QWidget *parent = nullptr)
        : QTabBar(parent) {}

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

class GlobalKeyInterceptor {
public:
    static bool capturing;
    static HHOOK hookHandle;

    enum ModBits { MB_CTRL = 1, MB_ALT = 2, MB_SHIFT = 4, MB_WIN = 8 };

    static int modState;

    static LRESULT CALLBACK hookProc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode < 0)
            return CallNextHookEx(hookHandle, nCode, wParam, lParam);
        auto kb = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
        if (!capturing)
            return CallNextHookEx(hookHandle, nCode, wParam, lParam);
        if (wParam == WM_KEYDOWN) {
            int vk = kb->vkCode;
            switch (vk) {
                case VK_CONTROL:
                case VK_LCONTROL:
                case VK_RCONTROL:
                    modState |= MB_CTRL;
                    return 1;
                case VK_MENU:
                case VK_LMENU:
                case VK_RMENU:
                    modState |= MB_ALT;
                    return 1;
                case VK_SHIFT:
                case VK_LSHIFT:
                case VK_RSHIFT:
                    modState |= MB_SHIFT;
                    return 1;
                case VK_LWIN:
                case VK_RWIN:
                    modState |= MB_WIN;
                    return 1;
                default: {
                    QString seq;
                    if (modState & MB_CTRL) seq += "Ctrl+";
                    if (modState & MB_ALT) seq += "Alt+";
                    if (modState & MB_SHIFT) seq += "Shift+";
                    if (modState & MB_WIN) seq += "Win+";

                    wchar_t name[64] = {0};
                    if (GetKeyNameTextW(
                            MapVirtualKeyW(vk, MAPVK_VK_TO_VSC) << 16,
                            name, 64) > 0) {
                        seq += QString::fromWCharArray(name);
                    } else {
                        seq += QString::number(vk);
                    }

                    if (MainWindow::instance) {
                        QMetaObject::invokeMethod(
                            MainWindow::instance,
                            "setHotkeyText",
                            Qt::QueuedConnection,
                            Q_ARG(QString, seq)
                        );
                    }
                    modState = 0;
                    return 1;
                }
            }
        } else if (wParam == WM_KEYUP) {
            int vk = kb->vkCode;
            switch (vk) {
                case VK_CONTROL:
                case VK_LCONTROL:
                case VK_RCONTROL:
                    modState &= ~MB_CTRL;
                    break;
                case VK_MENU:
                case VK_LMENU:
                case VK_RMENU:
                    modState &= ~MB_ALT;
                    break;
                case VK_SHIFT:
                case VK_LSHIFT:
                case VK_RSHIFT:
                    modState &= ~MB_SHIFT;
                    break;
                case VK_LWIN:
                case VK_RWIN:
                    modState &= ~MB_WIN;
                    break;
                default: break;
            }
        }
        return CallNextHookEx(hookHandle, nCode, wParam, lParam);
    }

    static void start() {
        if (!hookHandle) {
            hookHandle = SetWindowsHookExW(
                WH_KEYBOARD_LL,
                hookProc,
                GetModuleHandleW(nullptr),
                0
            );
        }
    }

    static void stop() {
        if (hookHandle) {
            UnhookWindowsHookEx(hookHandle);
            hookHandle = nullptr;
        }
    }
};

bool GlobalKeyInterceptor::capturing = false;
HHOOK GlobalKeyInterceptor::hookHandle = nullptr;
int GlobalKeyInterceptor::modState = 0;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
      , ui(new Ui::MainWindow)
      , hotkeyCaptured(false)
      , hotkeyManager(new HotkeyManager(this))
      , loadingConfig(false)
      , trayIcon(nullptr) {
    instance = this;
    ui->setupUi(this);
    // Include application name in the window title
    setWindowTitle(QCoreApplication::applicationName() + " - " + tr("Settings"));

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
    connect(ui->tasksTabWidget->tabBar(), &QTabBar::tabMoved,
            this, &MainWindow::handleTaskTabMoved);
    connect(ui->tasksTabWidget->tabBar(), &QTabBar::tabBarClicked,
            this, &MainWindow::handleTaskTabClicked);
    ensureAddTab();

    connect(ui->lineEditApiEndpoint, &QLineEdit::textChanged, this, &MainWindow::saveConfig);
    connect(ui->lineEditModelName, &QLineEdit::textChanged, this, &MainWindow::saveConfig);
    connect(ui->lineEditApiKey, &QLineEdit::textChanged, this, &MainWindow::saveConfig);
    connect(ui->lineEditProxy, &QLineEdit::textChanged, this, &MainWindow::saveConfig);
    connect(ui->lineEditHotkey, &QLineEdit::textChanged, this, &MainWindow::saveConfig);
    connect(ui->lineEditMaxChars, &QLineEdit::textChanged, this, &MainWindow::saveConfig);

    ui->lineEditHotkey->installEventFilter(this);

    createTrayIcon();

    connect(hotkeyManager, &HotkeyManager::hotkeyPressed,
            this, &MainWindow::handleGlobalHotkey);

    loadConfig();
    hotkeyManager->registerHotkey(ui->lineEditHotkey->text());
}

MainWindow::~MainWindow() {
    GlobalKeyInterceptor::stop();
    delete ui;
    instance = nullptr;
}

bool MainWindow::eventFilter(QObject *obj, QEvent *ev) {
    if (obj == ui->lineEditHotkey) {
        if (ev->type() == QEvent::FocusIn) {
            prevHotkey = ui->lineEditHotkey->text();
            hotkeyCaptured = false;
            GlobalKeyInterceptor::capturing = true;
            GlobalKeyInterceptor::start();
        } else if (ev->type() == QEvent::FocusOut) {
            GlobalKeyInterceptor::capturing = false;
            if (!hotkeyCaptured)
                ui->lineEditHotkey->setText(prevHotkey);
        }
    }
    return QMainWindow::eventFilter(obj, ev);
}

void MainWindow::closeEvent(QCloseEvent *event) {
    event->ignore();
    hide();
}

void MainWindow::setHotkeyText(const QString &text) {
    ui->lineEditHotkey->setText(text);
    hotkeyCaptured = true;
    saveConfig();
}

void MainWindow::applyDefaultSettings() {
    applyConfig(ConfigStore::defaultConfig());
}

void MainWindow::loadConfig() {
    const QString path = ConfigStore::configFilePath();
    QFile file(path);
    if (!file.exists()) {
        loadingConfig = true;
        applyDefaultSettings();
        loadingConfig = false;
        saveConfig();
        return;
    }
    if (!file.open(QIODevice::ReadOnly))
        return;

    loadingConfig = true;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    bool ok = false;
    const AppConfig config = ConfigStore::fromJson(doc, &ok);
    if (ok)
        applyConfig(config);

    loadingConfig = false;
}

void MainWindow::saveConfig() {
    if (loadingConfig)
        return;

    const AppConfig config = buildConfigFromUi();
    const QJsonDocument doc = ConfigStore::toJson(config);
    QFile file(ConfigStore::configFilePath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(doc.toJson());
        file.close();
    }

    hotkeyManager->registerHotkey(config.settings.hotkey);
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
    ui->lineEditModelName->setText(config.settings.modelName);
    ui->lineEditApiKey->setText(config.settings.apiKey);
    ui->lineEditProxy->setText(config.settings.proxy);
    ui->lineEditHotkey->setText(config.settings.hotkey);
    ui->lineEditMaxChars->setText(QString::number(config.settings.maxChars));

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
    config.settings.modelName = ui->lineEditModelName->text();
    config.settings.apiKey = ui->lineEditApiKey->text();
    config.settings.proxy = ui->lineEditProxy->text();
    config.settings.hotkey = ui->lineEditHotkey->text();
    config.settings.maxChars = ui->lineEditMaxChars->text().toInt();
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
    connect(task, &TaskWidget::removeRequested, this, &MainWindow::removeTaskWidget);
    connect(task, &TaskWidget::configChanged, this, &MainWindow::saveConfig);
    connect(task, &TaskWidget::configChanged, this, [this, task]() {
        updateTaskTabTitle(task);
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

void MainWindow::handleGlobalHotkey() {
    if (menuWindow) {
        menuWindow->close();
        menuWindow = nullptr;
    }
    const AppConfig config = buildConfigFromUi();
    menuWindow = new TaskWindow(config.tasks, config.settings);
    connect(menuWindow, &TaskWindow::taskResponsePrefsChanged,
            this, &MainWindow::updateTaskResponsePrefs);
    connect(menuWindow, &TaskWindow::taskResponsePrefsCommitRequested,
            this, &MainWindow::commitTaskResponsePrefs);
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
