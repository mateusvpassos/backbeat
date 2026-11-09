#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

namespace bb
{

// Atalhos que funcionam com qualquer janela em foco (ex.: Songsterr no Chrome), via RegisterHotKey.
// Uma thread própria tem a fila de mensagens Win32; o callback é entregue na thread de mensagens do JUCE.
// Atenção: enquanto registrada, a tecla deixa de chegar aos outros programas.
class GlobalHotkeys final : private juce::Thread
{
public:
    struct Binding
    {
        int id = 0;
        juce::KeyPress key;
    };

    GlobalHotkeys();
    ~GlobalHotkeys() override;

    // Substitui todos os atalhos. Lista vazia = nenhum.
    void setBindings (const std::vector<Binding>& bindings);

    std::function<void (int id)> onHotkey;                       // thread de mensagens
    std::function<void (const juce::StringArray& failed)> onRegistrationResult; // teclas que não registraram

    static bool canRegister (const juce::KeyPress& key);

    // juce::KeyPress → código virtual do Windows + modificadores do RegisterHotKey (MOD_*).
    static bool toVirtualKey (const juce::KeyPress& key, unsigned& vk, unsigned& mods);

private:
    void run() override;
    void applyPending();

    juce::CriticalSection pendingLock;  // só entre thread de mensagens e esta thread (nada de áudio)
    std::vector<Binding> pending;
    std::vector<int> registeredIds;
    std::atomic<unsigned long> threadId { 0 };
    std::shared_ptr<bool> alive = std::make_shared<bool> (true); // callbacks pendentes checam na thread de mensagens
    juce::WaitableEvent started;
};

} // namespace bb
