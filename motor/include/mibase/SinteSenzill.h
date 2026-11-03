#pragma once

#include <array>
#include <cstdint>

namespace mibase
{
    /** So senzill per provar el teclat MIDI: 8 veus, ona suau amb envolupant.
        No demana memòria mentre sona. */
    class SinteSenzill
    {
    public:
        static constexpr int numVeus = 8;

        void prepara (double freqMostreig) noexcept;

        /** Missatge MIDI cru (estat, dada1, dada2). */
        void midi (std::uint8_t estat, std::uint8_t dada1, std::uint8_t dada2) noexcept;

        void notaOn (int nota, int velocitat) noexcept;
        void notaOff (int nota) noexcept;
        void silenciaTot() noexcept;

        /** Suma el so al buffer (mono). */
        void processa (float* sortida, int numMostres) noexcept;

        int veusSonant() const noexcept;

    private:
        enum class Fase : std::uint8_t { Lliure, Atac, Sosteniment, Alliberament };

        struct Veu
        {
            Fase fase = Fase::Lliure;
            int nota = -1;
            double angle = 0.0, increment = 0.0;
            float nivell = 0.0f, objectiu = 0.0f;
            std::uint32_t ordre = 0;
        };

        std::array<Veu, numVeus> veus {};
        double fs = 48000.0;
        float pasAtac = 0.0f, factorAlliberament = 0.0f;
        std::uint32_t comptador = 0;
    };
}
