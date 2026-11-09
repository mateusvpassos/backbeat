#pragma once

#include <atomic>
#include <cstring>
#include <type_traits>

namespace bb
{

// Valor pequeno compartilhado entre UM escritor (thread de mensagens) e leitores de tempo real
// (thread MIDI/áudio). Leitura sem lock e sem alocação: se pegar uma escrita no meio, tenta de novo.
template <typename T>
class SeqLock
{
    static_assert (std::is_trivially_copyable_v<T>, "T precisa ser trivialmente copiavel");

public:
    SeqLock() = default;
    explicit SeqLock (const T& initial) { std::memcpy (&value, &initial, sizeof (T)); }

    void store (const T& v) noexcept // só um escritor
    {
        const auto s = sequence.load (std::memory_order_relaxed);
        sequence.store (s + 1, std::memory_order_relaxed);
        std::atomic_thread_fence (std::memory_order_release);
        std::memcpy (&value, &v, sizeof (T));
        sequence.store (s + 2, std::memory_order_release);
    }

    T load() const noexcept
    {
        T out;
        for (;;)
        {
            const auto s1 = sequence.load (std::memory_order_acquire);
            if ((s1 & 1u) != 0)
                continue;

            std::memcpy (&out, &value, sizeof (T));
            std::atomic_thread_fence (std::memory_order_acquire);

            if (sequence.load (std::memory_order_relaxed) == s1)
                return out;
        }
    }

private:
    T value {};
    std::atomic<unsigned> sequence { 0 };
};

} // namespace bb
