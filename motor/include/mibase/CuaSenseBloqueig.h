#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace mibase
{
    /** Cua d'un sol productor i un sol consumidor, sense bloqueigs ni memòria nova.
        Serveix per passar missatges entre fils (per exemple, del MIDI al fil d'àudio)
        sense que el fil d'àudio s'hagi d'esperar mai. */
    template <typename T, std::size_t Capacitat>
    class CuaSenseBloqueig
    {
        static_assert (Capacitat >= 2 && (Capacitat & (Capacitat - 1)) == 0,
                       "La capacitat ha de ser una potència de 2");

    public:
        /** Afegeix un element. Retorna false si la cua és plena (l'element es perd). */
        bool posa (const T& valor) noexcept
        {
            const auto e = escriptura.load (std::memory_order_relaxed);
            const auto l = lectura.load (std::memory_order_acquire);

            if (e - l >= Capacitat)
                return false;

            dades[e & (Capacitat - 1)] = valor;
            escriptura.store (e + 1, std::memory_order_release);
            return true;
        }

        /** Treu el primer element. Retorna false si la cua és buida. */
        bool treu (T& valor) noexcept
        {
            const auto l = lectura.load (std::memory_order_relaxed);
            const auto e = escriptura.load (std::memory_order_acquire);

            if (l == e)
                return false;

            valor = dades[l & (Capacitat - 1)];
            lectura.store (l + 1, std::memory_order_release);
            return true;
        }

    private:
        std::array<T, Capacitat> dades {};
        alignas (64) std::atomic<std::size_t> escriptura { 0 };
        alignas (64) std::atomic<std::size_t> lectura { 0 };
    };
}
