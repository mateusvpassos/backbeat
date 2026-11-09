#include "input/GlobalHotkeys.h"

#ifndef NOMINMAX
 #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace bb
{

namespace
{
    constexpr UINT applyMessage = WM_APP + 1;
    constexpr int idBase = 0x4242;

    bool toWin32 (const juce::KeyPress& key, UINT& vk, UINT& mods)
    {
        unsigned v = 0, m = 0;
        if (! GlobalHotkeys::toVirtualKey (key, v, m))
            return false;
        vk = v;
        mods = m;
        return true;
    }
}

bool GlobalHotkeys::toVirtualKey (const juce::KeyPress& key, unsigned& vk, unsigned& mods)
{
    // No Windows o JUCE marca teclas especiais (F1–F12, setas, teclado numérico...) com o bit 0x10000.
    // Só letras SEM esse bit podem ser convertidas para maiúscula: o F9 é 0x78, o mesmo número do 'x'.
    constexpr int juceExtendedKey = 0x10000;
    const int raw = key.getKeyCode();
    int code = raw & 0xFFFF;

    if ((raw & juceExtendedKey) == 0 && code >= 'a' && code <= 'z')
        code -= 'a' - 'A';

    if (code == 0)
        return false;

    vk = (unsigned) code;
    const auto m = key.getModifiers();
    mods = MOD_NOREPEAT;
    if (m.isCtrlDown())  mods |= MOD_CONTROL;
    if (m.isAltDown())   mods |= MOD_ALT;
    if (m.isShiftDown()) mods |= MOD_SHIFT;
    return true;
}

GlobalHotkeys::GlobalHotkeys() : juce::Thread ("Backbeat atalhos globais")
{
    startThread();
    started.wait (2000);
}

GlobalHotkeys::~GlobalHotkeys()
{
    *alive = false;
    signalThreadShouldExit();
    if (const auto tid = threadId.load())
        PostThreadMessageW ((DWORD) tid, WM_QUIT, 0, 0);
    stopThread (2000);
}

bool GlobalHotkeys::canRegister (const juce::KeyPress& key)
{
    UINT vk = 0, mods = 0;
    return key.isValid() && toWin32 (key, vk, mods);
}

void GlobalHotkeys::setBindings (const std::vector<Binding>& bindings)
{
    {
        const juce::ScopedLock sl (pendingLock);
        pending = bindings;
    }

    if (const auto tid = threadId.load())
        PostThreadMessageW ((DWORD) tid, applyMessage, 0, 0);
}

void GlobalHotkeys::applyPending()
{
    for (const int id : registeredIds)
        UnregisterHotKey (nullptr, id);
    registeredIds.clear();

    std::vector<Binding> wanted;
    {
        const juce::ScopedLock sl (pendingLock);
        wanted = pending;
    }

    juce::StringArray failed, done;
    for (const auto& b : wanted)
    {
        UINT vk = 0, mods = 0;
        const int id = idBase + b.id;

        if (toWin32 (b.key, vk, mods) && RegisterHotKey (nullptr, id, mods, vk))
        {
            registeredIds.push_back (id);
            done.add (b.key.getTextDescription() + juce::String::formatted (" (vk %02X)", vk));
        }
        else
        {
            failed.add (b.key.getTextDescription());
        }
    }

    juce::Logger::writeToLog ("RegisterHotKey: " + (done.isEmpty() ? juce::String ("nenhum") : done.joinIntoString (", ")));

    juce::MessageManager::callAsync ([this, failed, flag = alive]
    {
        if (*flag && onRegistrationResult)
            onRegistrationResult (failed);
    });
}

void GlobalHotkeys::run()
{
    MSG msg;
    PeekMessageW (&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE); // cria a fila desta thread
    threadId.store (GetCurrentThreadId());
    started.signal();

    applyPending();

    while (! threadShouldExit() && GetMessageW (&msg, nullptr, 0, 0) > 0)
    {
        if (msg.message == WM_HOTKEY)
        {
            const int id = (int) msg.wParam - idBase;
            juce::MessageManager::callAsync ([this, id, flag = alive]
            {
                if (*flag && onHotkey)
                    onHotkey (id);
            });
        }
        else if (msg.message == applyMessage)
        {
            applyPending();
        }
    }

    for (const int id : registeredIds)
        UnregisterHotKey (nullptr, id);
    registeredIds.clear();
}

} // namespace bb
