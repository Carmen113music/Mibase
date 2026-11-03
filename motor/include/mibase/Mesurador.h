#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>

namespace mibase
{
    /** Vúmetre d'un canal. El fil d'àudio hi escriu el pic; la interfície el llegeix.
        Sense bloqueigs: només valors atòmics. */
    class Mesurador
    {
    public:
        void processa (const float* mostres, int numMostres) noexcept
        {
            float pic = 0.0f;
            for (int i = 0; i < numMostres; ++i)
                pic = std::max (pic, std::abs (mostres[i]));

            float anterior = picMax.load (std::memory_order_relaxed);
            while (pic > anterior && ! picMax.compare_exchange_weak (anterior, pic, std::memory_order_relaxed))
                {}

            if (pic >= 0.999f)
                saturacions.fetch_add (1, std::memory_order_relaxed);
        }

        /** Pic més alt des de l'última lectura (0..1). La interfície el crida unes 30 vegades per segon. */
        float llegeixIReinicia() noexcept { return picMax.exchange (0.0f, std::memory_order_relaxed); }

        int getSaturacions() const noexcept { return saturacions.load (std::memory_order_relaxed); }
        void reiniciaSaturacions() noexcept { saturacions.store (0, std::memory_order_relaxed); }

    private:
        std::atomic<float> picMax { 0.0f };
        std::atomic<int> saturacions { 0 };
    };
}
