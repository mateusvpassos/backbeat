#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace bb
{

#if defined (_MSC_VER)
 #pragma warning (push)
 #pragma warning (disable: 4324) // preenchimento proposital: índices em linhas de cache separadas
#endif

// Fila lock-free de um produtor e um consumidor, tamanho fixo (sem alocação).
// Segura para usar na thread de áudio: push/pop nunca bloqueiam nem alocam.
template <typename T, std::size_t Capacity>
class SpscQueue
{
    static_assert (Capacity >= 2 && (Capacity & (Capacity - 1)) == 0, "Capacity precisa ser potencia de 2");
    static_assert (std::is_trivially_copyable_v<T>, "T precisa ser trivialmente copiavel");

public:
    // Retorna false se a fila estiver cheia (o item é descartado).
    bool push (const T& item) noexcept
    {
        const auto w = writeIndex.load (std::memory_order_relaxed);
        const auto r = readIndex.load (std::memory_order_acquire);

        if (w - r >= Capacity)
            return false;

        buffer[w & mask] = item;
        writeIndex.store (w + 1, std::memory_order_release);
        return true;
    }

    // Retorna false se a fila estiver vazia.
    bool pop (T& out) noexcept
    {
        const auto r = readIndex.load (std::memory_order_relaxed);
        const auto w = writeIndex.load (std::memory_order_acquire);

        if (r == w)
            return false;

        out = buffer[r & mask];
        readIndex.store (r + 1, std::memory_order_release);
        return true;
    }

    std::size_t size() const noexcept
    {
        return writeIndex.load (std::memory_order_acquire) - readIndex.load (std::memory_order_acquire);
    }

    static constexpr std::size_t capacity() noexcept { return Capacity; }

private:
    static constexpr std::size_t mask = Capacity - 1;

    std::array<T, Capacity> buffer {};
    alignas (64) std::atomic<std::size_t> writeIndex { 0 };
    alignas (64) std::atomic<std::size_t> readIndex { 0 };
};

#if defined (_MSC_VER)
 #pragma warning (pop)
#endif

} // namespace bb
