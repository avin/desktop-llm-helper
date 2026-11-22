#ifndef HOTKEYMANAGER_H
#define HOTKEYMANAGER_H

#include <QHash>
#include <QObject>
#include <QStringList>

#include <windows.h>

class HotkeyManager : public QObject {
    Q_OBJECT

public:
    static constexpr int MenuHotkey = -1;
    static constexpr ULONG_PTR ClipboardInputTag = 0x444C4C4D;

    explicit HotkeyManager(QObject *parent = nullptr);
    ~HotkeyManager() override;

    void setHotkeys(const QString &menuHotkey, const QStringList &taskHotkeys);
    void setCapturing(bool enabled);
    bool isHotkeyAssigned(const QString &sequence) const;
    static UINT hotkeyCode(const QString &sequence);

signals:
    void hotkeyCaptured(const QString &sequence);
    void hotkeyPressed(int taskIndex);

private:
    HHOOK hook = nullptr;
    QHash<UINT, int> hotkeys;
    bool capturing = false;
    UINT captureModifiers = 0;
    UINT pressedTaskKey = 0;
    UINT pendingTaskHotkey = 0;

    static HotkeyManager *s_instance;
    static LRESULT CALLBACK LowLevelProc(int nCode, WPARAM wParam, LPARAM lParam);
    bool handleKey(UINT key, bool pressed);
    void dispatchHotkey(UINT code);
};

#endif // HOTKEYMANAGER_H
