#pragma once

#include "mibase/ClicDeProva.h"
#include "mibase/CuaSenseBloqueig.h"
#include "mibase/MesuraLatencia.h"
#include "mibase/Mesurador.h"
#include "mibase/SinteSenzill.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace mibase
{
    struct EsdevenimentMidi
    {
        std::uint8_t estat = 0, dada1 = 0, dada2 = 0;
    };

    /** El motor de la pantalla de diagnòstic (versió 0.1):
        clic de prova, so per al teclat MIDI, vúmetres de les entrades i mesura de la latència.
        No depèn de JUCE ni de Windows: es prova sol. */
    class MotorDiagnostic
    {
    public:
        static constexpr int maxEntrades = 32;

        /** Fora del fil d'àudio (en arrencar el dispositiu): prepara la memòria. */
        void prepara (double freqMostreig, int maxMostresPerBloc);

        /** Fil d'àudio. Omple totes les sortides. */
        void processa (const float* const* entrades, int numEntrades,
                       float* const* sortides, int numSortides, int numMostres) noexcept;

        /** Fil del MIDI: envia una nota/control al so de prova. Un sol fil alhora. */
        bool enviaMidi (const EsdevenimentMidi& e) noexcept { return cuaMidi.posa (e); }

        Mesurador& mesurador (int canal) noexcept { return mesuradors[(size_t) canal]; }
        MesuraLatencia& latencia() noexcept { return mesuraLatencia; }

        std::atomic<bool> clicActiu { false };
        std::atomic<bool> sinteActiu { true };
        std::atomic<double> tempoClic { 120.0 };
        std::atomic<int> soClic { 0 };
        std::atomic<float> volumClic { 1.0f };
        std::atomic<int> tempsPerCompasClic { 4 };

        /** Vegades que el fil d'àudio ha rebut un bloc més gran del previst. */
        std::atomic<int> blocsMassaGrans { 0 };

    private:
        ClicDeProva clic;
        SinteSenzill sinte;
        MesuraLatencia mesuraLatencia;
        std::array<Mesurador, maxEntrades> mesuradors;
        CuaSenseBloqueig<EsdevenimentMidi, 1024> cuaMidi;

        std::vector<float> mono;
        bool clicSonava = false;
    };
}
