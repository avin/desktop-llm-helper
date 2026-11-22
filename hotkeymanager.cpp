#include "hotkeymanager.h"

#include <QMetaObject>

namespace {
struct Modifier {
    const char *name;
    UINT flag;
    UINT leftKey;
    UINT rightKey;
};

constexpr Modifier kModifiers[] = {
    {"Ctrl", MOD_CONTROL, VK_LCONTROL, VK_RCONTROL},
    {"Alt", MOD_ALT, VK_LMENU, VK_RMENU},
    {"Shift", MOD_SHIFT, VK_LSHIFT, VK_RSHIFT},
    {"Win", MOD_WIN, VK_LWIN, VK_RWIN},
};

struct NamedKey {
    const char *name;
    UINT key;
};

constexpr NamedKey kNamedKeys[] = {
    {"Tab", VK_TAB},
    {"Space", VK_SPACE},
    {"Enter", VK_RETURN},
    {"Esc", VK_ESCAPE},
    {"Backspace", VK_BACK},
    {"Delete", VK_DELETE},
    {"Insert", VK_INSERT},
    {"Home", VK_HOME},
    {"End", VK_END},
    {"Page Up", VK_PRIOR},
    {"Page Down", VK_NEXT},
    {"Left", VK_LEFT},
    {"Right", VK_RIGHT},
    {"Up", VK_UP},
    {"Down", VK_DOWN},
};

UINT modifiersDown(UINT releasedKey = 0) {
    const auto isDown = [releasedKey](UINT key) {
        return key != releasedKey && (GetAsyncKeyState(key) & 0x8000) != 0;
    };
    UINT modifiers = 0;
    for (const Modifier &modifier : kModifiers) {
        if (isDown(modifier.leftKey) || isDown(modifier.rightKey))
            modifiers |= modifier.flag;
    }
    return modifiers;
}

UINT keyFromName(const QString &name) {
    for (const NamedKey &entry : kNamedKeys) {
        if (name.compare(QLatin1String(entry.name), Qt::CaseInsensitive) == 0)
            return entry.key;
    }
    if (name.compare("Escape", Qt::CaseInsensitive) == 0)
        return VK_ESCAPE;
    if (name.size() == 1) {
        const ushort ch = name.at(0).toUpper().unicode();
        if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))
            return ch;
        const SHORT key = VkKeyScanW(ch);
        return key == -1 ? 0 : LOBYTE(key);
    }
    if (name.startsWith('F', Qt::CaseInsensitive)) {
        bool ok = false;
        const int n = name.mid(1).toInt(&ok);
        if (ok && n >= 1 && n <= 24)
            return VK_F1 + n - 1;
    }
    return 0;
}

QString sequenceForKey(UINT key, UINT modifiers) {
    QString name;
    for (const NamedKey &entry : kNamedKeys) {
        if (entry.key == key) {
            name = QLatin1String(entry.name);
            break;
        }
    }
    if (name.isEmpty() && key >= VK_F1 && key <= VK_F24)
        name = QStringLiteral("F%1").arg(key - VK_F1 + 1);
    if (name.isEmpty()) {
        const QChar ch(static_cast<ushort>(MapVirtualKeyW(key, MAPVK_VK_TO_CHAR)));
        if (ch.isNull() || keyFromName(QString(ch)) != key)
            return {};
        name = ch;
    }

    QString sequence;
    for (const Modifier &modifier : kModifiers) {
        if (modifiers & modifier.flag)
            sequence += QLatin1String(modifier.name) + QLatin1Char('+');
    }
    return sequence + name;
}
} // namespace

HotkeyManager *HotkeyManager::s_instance = nullptr;

HotkeyManager::HotkeyManager(QObject *parent)
    : QObject(parent) {
    s_instance = this;
    hook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelProc, GetModuleHandleW(nullptr), 0);
}

HotkeyManager::~HotkeyManager() {
    if (hook)
        UnhookWindowsHookEx(hook);
    s_instance = nullptr;
}

UINT HotkeyManager::hotkeyCode(const QString &sequence) {
    const QString text = sequence.trimmed();
    const int separator = text.size() > 1 ? text.lastIndexOf('+', text.size() - 2) : -1;
    const UINT key = keyFromName(text.mid(separator + 1).trimmed());
    if (!key)
        return 0;

    UINT modifiers = 0;
    if (separator >= 0) {
        for (const QString &part : text.left(separator).split('+', Qt::SkipEmptyParts)) {
            const QString name = part.trimmed();
            UINT flag = name.compare("Meta", Qt::CaseInsensitive) == 0 ? MOD_WIN : 0;
            for (const Modifier &modifier : kModifiers) {
                if (name.compare(QLatin1String(modifier.name), Qt::CaseInsensitive) == 0) {
                    flag = modifier.flag;
                    break;
                }
            }
            if (!flag)
                return 0;
            modifiers |= flag;
        }
    }
    return (modifiers << 16) | key;
}

void HotkeyManager::setHotkeys(const QString &menuHotkey, const QStringList &taskHotkeys) {
    QHash<UINT, int> bindings;
    if (const UINT code = hotkeyCode(menuHotkey))
        bindings.insert(code, MenuHotkey);
    for (int i = 0; i < taskHotkeys.size(); ++i) {
        const UINT code = hotkeyCode(taskHotkeys.at(i));
        if (code && !bindings.contains(code))
            bindings.insert(code, i);
    }
    if (hotkeys != bindings) {
        hotkeys.swap(bindings);
        pendingTaskHotkey = 0;
    }
}

void HotkeyManager::setCapturing(bool enabled) {
    capturing = enabled;
    captureModifiers = 0;
    pendingTaskHotkey = 0;
}

bool HotkeyManager::isHotkeyAssigned(const QString &sequence) const {
    return hotkeys.contains(hotkeyCode(sequence));
}

void HotkeyManager::dispatchHotkey(UINT code) {
    const auto binding = hotkeys.constFind(code);
    if (binding == hotkeys.cend())
        return;
    const int taskIndex = binding.value();
    QMetaObject::invokeMethod(this, [this, code, taskIndex]() {
        if (!capturing && hotkeys.value(code, -2) == taskIndex)
            emit hotkeyPressed(taskIndex);
    }, Qt::QueuedConnection);
}

bool HotkeyManager::handleKey(UINT key, bool pressed) {
    if (key == pressedTaskKey) {
        if (!pressed) {
            pressedTaskKey = 0;
            if (pendingTaskHotkey && !modifiersDown()) {
                dispatchHotkey(pendingTaskHotkey);
                pendingTaskHotkey = 0;
            }
        }
        return true;
    }

    if (capturing) {
        for (const Modifier &modifier : kModifiers) {
            if (key == modifier.leftKey || key == modifier.rightKey) {
                if (pressed)
                    captureModifiers |= modifier.flag;
                else
                    captureModifiers &= ~modifier.flag;
                return true;
            }
        }
        if (pressed) {
            const QString sequence = sequenceForKey(key, captureModifiers);
            if (!sequence.isEmpty()) {
                QMetaObject::invokeMethod(this, [this, sequence]() {
                    if (capturing)
                        emit hotkeyCaptured(sequence);
                }, Qt::QueuedConnection);
            }
        }
        return true;
    }

    if (!pressed) {
        if (pendingTaskHotkey && !pressedTaskKey && !modifiersDown(key)) {
            dispatchHotkey(pendingTaskHotkey);
            pendingTaskHotkey = 0;
        }
        return false;
    }

    const UINT code = (modifiersDown() << 16) | key;
    const auto binding = hotkeys.constFind(code);
    if (binding == hotkeys.cend())
        return false;
    if (binding.value() == MenuHotkey) {
        dispatchHotkey(code);
        return true;
    }
    if (pressedTaskKey || pendingTaskHotkey)
        return false;

    // Copying the selection sends Ctrl+C after the shortcut is fully released.
    pressedTaskKey = key;
    pendingTaskHotkey = code;
    return true;
}

LRESULT CALLBACK HotkeyManager::LowLevelProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && s_instance) {
        const auto *event = reinterpret_cast<const KBDLLHOOKSTRUCT *>(lParam);
        if (event->dwExtraInfo != ClipboardInputTag) {
            const bool pressed = wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN;
            const bool released = wParam == WM_KEYUP || wParam == WM_SYSKEYUP;
            if ((pressed || released) && s_instance->handleKey(event->vkCode, pressed))
                return 1;
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}
