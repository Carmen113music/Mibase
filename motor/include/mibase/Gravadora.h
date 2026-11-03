#pragma once

#include "mibase/Wav.h"

#include <atomic>
#include <filesystem>
#include <thread>
#include <vector>

namespace mibase
{
    /** Grava al disc el que li passa el fil d'àudio, sense que aquest s'hagi d'esperar mai:
        el fil d'àudio deixa les mostres en un anell i un fil a part les escriu al fitxer WAV (24 bits). */
    class Gravadora
    {
    public:
        ~Gravadora() { acaba(); }

        /** Fil de la interfície: obre el fitxer i engega el fil que escriu. */
        bool comenca (const std::filesystem::path& cami, int canals, double freqMostreig);

        struct Resultat
        {
            bool correcte = false;
            long long frames = 0;
            long long perdudes = 0;   // mostres que no han cabut a l'anell (el disc no ha anat prou de pressa)
        };

        /** Fil de la interfície, quan el fil d'àudio ja no hi escriu: buida l'anell, tanca el fitxer i para el fil. */
        Resultat acaba();

        bool activa() const noexcept { return actiu.load (std::memory_order_acquire); }
        int getCanals() const noexcept { return canals; }

        /** Fil d'àudio. Un punter per canal (nullptr = silenci). No demana memòria ni s'espera. */
        void escriu (const float* const* entrada, int numMostres) noexcept;

        /** Frames escrits fins ara (aproximat, per mostrar). */
        long long framesEscrits() const noexcept { return escrits.load (std::memory_order_relaxed); }

    private:
        void bucle();
        size_t buida (std::vector<float>& tros);

        std::vector<float> anell;
        size_t mascara = 0;
        alignas (64) std::atomic<size_t> posEscriu { 0 };
        alignas (64) std::atomic<size_t> posLlegeix { 0 };
        std::atomic<bool> actiu { false }, seguir { false };
        std::atomic<long long> perdudes { 0 }, escrits { 0 };
        int canals = 1;
        EscriptorWav wav;
        std::thread fil;
        bool errorDisc = false;
    };
}
