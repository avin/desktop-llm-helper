#include "hotkeymanager.h"

#ifdef Q_OS_WIN
#include <QMetaObject>
#include <QDebug>

HHOOK           HotkeyManager::s_hook      = nullptr;
HotkeyManager*  HotkeyManager::s_instance  = nullptr;

//──────────────────────────────────────────────────────────────────────────
HotkeyManager::HotkeyManager(QObject* parent)
    : QObject(parent)
    , id(1)
    , currentModifiers(0)
    , currentVk(0)
{
    s_instance = this;
}

HotkeyManager::~HotkeyManager()
{
    if (s_hook)
        UnhookWindowsHookEx(s_hook);
    UnregisterHotKey(nullptr, id);
    s_instance = nullptr;
}

//──────────────────────────────────────────────────────────────────────────
bool HotkeyManager::parseSequence(const QString& sequence,
                                  UINT&          modifiers,
                                  UINT&          vk) const
{
    modifiers = 0;
    vk        = 0;

    const auto parts = sequence.split('+', Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return false;

    for (const QString& raw : parts) {
        const QString part = raw.trimmed();

        if      (part.compare("Ctrl",  Qt::CaseInsensitive) == 0) modifiers |= MOD_CONTROL;
        else if (part.compare("Alt",   Qt::CaseInsensitive) == 0) modifiers |= MOD_ALT;
        else if (part.compare("Shift", Qt::CaseInsensitive) == 0) modifiers |= MOD_SHIFT;
        else if (part.compare("Win",   Qt::CaseInsensitive) == 0 ||
                 part.compare("Meta",  Qt::CaseInsensitive) == 0) modifiers |= MOD_WIN;
        else {
            if (part.length() == 1) { // одиночный символ
                const wchar_t ch  = part.at(0).toUpper().unicode();
                const SHORT   res = VkKeyScanW(ch);
                if (res == -1) return false;
                vk = LOBYTE(res);
            } else if (part.startsWith('F', Qt::CaseInsensitive)) { // F1…F24
                bool ok = false;
                const int n = part.mid(1).toInt(&ok);
                if (!ok || n < 1 || n > 24) return false;
                vk = VK_F1 + n - 1;
            } else if (part.compare("Tab",   Qt::CaseInsensitive) == 0) vk = VK_TAB;
            else if (part.compare("Space",  Qt::CaseInsensitive) == 0) vk = VK_SPACE;
            else if (part.compare("Enter",  Qt::CaseInsensitive) == 0) vk = VK_RETURN;
            else if (part.compare("Esc",    Qt::CaseInsensitive) == 0 ||
                     part.compare("Escape", Qt::CaseInsensitive) == 0) vk = VK_ESCAPE;
            else return false;
        }
    }
    return vk != 0;
}

//──────────────────────────────────────────────────────────────────────────
bool HotkeyManager::registerHotkey(const QString& sequence)
{
    UINT mods = 0;
    UINT key  = 0;
    if (!parseSequence(sequence, mods, key))
        return false;

    currentModifiers = mods;
    currentVk        = key;

    // штатный RegisterHotKey НЕ используем: нужна полная блокировка
    UnregisterHotKey(nullptr, id);

    if (!s_hook) {
        s_hook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelProc,
                                   GetModuleHandleW(nullptr), 0);
    }
    return s_hook != nullptr;
}

//──────────────────────────────────────────────────────────────────────────
bool HotkeyManager::nativeEventFilter(const QByteArray&,
                                      void*,
                                      qintptr*)
{
    // WM_HOTKEY не используется — хук работает на более низком уровне
    return false;
}

//──────────────────────────────────────────────────────────────────────────
LRESULT CALLBACK HotkeyManager::LowLevelProc(int nCode,
                                             WPARAM      wParam,
                                             LPARAM      lParam)
{
    if (nCode == HC_ACTION && s_instance) {
        const auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            const UINT vk = kb->vkCode;

            const bool ctrl  = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            const bool alt   = (GetAsyncKeyState(VK_MENU)    & 0x8000) != 0;
            const bool shift = (GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0;
            const bool win   = (GetAsyncKeyState(VK_LWIN)    & 0x8000) ||
                               (GetAsyncKeyState(VK_RWIN)    & 0x8000);

            UINT mods = 0;
            if (ctrl)  mods |= MOD_CONTROL;
            if (alt)   mods |= MOD_ALT;
            if (shift) mods |= MOD_SHIFT;
            if (win)   mods |= MOD_WIN;

            if (mods == s_instance->currentModifiers &&
                vk   == s_instance->currentVk)
            {
                // 🔹 Хоткей совпал — испускаем сигнал и блокируем клавишу
                QMetaObject::invokeMethod(s_instance, "hotkeyPressed",
                                          Qt::QueuedConnection);
                return 1;   // ← дальнейшая доставка прерывается
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
#endif // Q_OS_WIN