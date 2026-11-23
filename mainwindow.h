#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QString>
#include <QEvent>
#include <QList>
#include <QSystemTrayIcon>
#include <QCloseEvent>
#include <QHash>
#include <QPointer>
#include <QSize>

#include <functional>

#include "hotkeymanager.h"
#include "configstore.h"
#include "modelinfo.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class TaskWidget;
class TaskWindow;
class ModelSelectBox;
class ModelListLoader;
class QLineEdit;
class QThread;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void handleTaskTabClicked(int index);
    void handleTaskTabMoved(int from, int to);
    void requestCloseTask(int index);
    void removeTaskWidget(TaskWidget *task);
    void updateTaskResponsePrefs(int taskIndex, const QSize &size, int zoom);
    void commitTaskResponsePrefs();
    void requestModelList(ModelSelectBox *target, int generation);
    void handleModelListLoaded(int requestId, const ModelInfoList &models);
    void handleModelListFailed(int requestId, const QString &message);
    void exportSettings();
    void importSettings();

signals:
    void modelListLoadRequested(int requestId, const QString &baseUrl,
                                const QString &apiKey, const QString &proxyText);

private:
    using ModelListCallback = std::function<void(const ModelInfoList &, const QString &)>;

    struct ModelListRequestParams {
        QString baseUrl;
        QString apiKey;
        QString proxyText;

        bool operator==(const ModelListRequestParams &other) const {
            return baseUrl == other.baseUrl
                && apiKey == other.apiKey
                && proxyText == other.proxyText;
        }
    };

    struct PendingModelRequest {
        ModelListRequestParams params;
        ModelListCallback callback;
    };

    Ui::MainWindow *ui;
    QPointer<QLineEdit> hotkeyField;
    HotkeyManager *hotkeyManager;
    bool loadingConfig;
    QSystemTrayIcon *trayIcon;
    QPointer<TaskWindow> latestTaskWindow;
    QThread *modelLoaderThread;
    ModelListLoader *modelListLoader;
    int nextModelRequestId;
    QHash<int, PendingModelRequest> pendingModelRequests;
    bool hasCachedModelList;
    ModelListRequestParams cachedModelListParams;
    ModelInfoList cachedModelList;
    QJsonObject defaultModelParameters;

    void createTrayIcon();
    void setHotkeyText(const QString &text);
    void handleGlobalHotkey(int taskIndex);
    void updateHotkeys(const AppConfig &config);
    void loadConfig();
    void saveConfig();
    void applyDefaultSettings();
    void applyConfig(const AppConfig &config);
    AppConfig buildConfigFromUi() const;
    QList<TaskDefinition> currentTaskDefinitions() const;
    void addTaskTab(const TaskDefinition &definition, bool makeCurrent);
    void connectTaskSignals(TaskWidget *task);
    void updateTaskTabTitle(TaskWidget *task);
    void clearTasks();
    int addTabIndex() const;
    bool isAddTabIndex(int index) const;
    void ensureAddTab();
    void ensureAddTabLast();
    void setDefaultModel(const QString &defaultModel);
    QString currentDefaultModel() const;
    void loadModelList(ModelListCallback callback);
    void editModelParameters(TaskWidget *task);
    QString suggestedSettingsPath() const;
};

#endif // MAINWINDOW_H
