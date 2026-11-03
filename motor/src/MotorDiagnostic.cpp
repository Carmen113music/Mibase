#include "mibase/MotorDiagnostic.h"

#include <algorithm>

namespace mibase
{
    void MotorDiagnostic::prepara (double freqMostreig, int maxMostresPerBloc)
    {
        clic.prepara (freqMostreig);
        sinte.prepara (freqMostreig);
        mesuraLatencia.prepara (freqMostreig);
        mono.assign ((size_t) std::max (maxMostresPerBloc, 16) * 2, 0.0f);
        clicSonava = false;
    }

    void MotorDiagnostic::processa (const float* const* entrades, int numEntrades,
                                    float* const* sortides, int numSortides, int numMostres) noexcept
    {
        // Vúmetres de les entrades.
        for (int c = 0; c < std::min (numEntrades, maxEntrades); ++c)
            if (entrades[c] != nullptr)
                mesuradors[(size_t) c].processa (entrades[c], numMostres);

        // Missatges MIDI pendents.
        EsdevenimentMidi e;
        while (cuaMidi.treu (e))
            if (sinteActiu.load (std::memory_order_relaxed))
                sinte.midi (e.estat, e.dada1, e.dada2);

        if (! sinteActiu.load (std::memory_order_relaxed) && sinte.veusSonant() > 0)
            sinte.silenciaTot();

        int fet = 0;
        while (fet < numMostres)
        {
            // Per si el controlador envia un bloc més gran del previst: es fa a trossos.
            const int n = std::min (numMostres - fet, (int) mono.size());
            if (n <= 0)
            {
                blocsMassaGrans.fetch_add (1, std::memory_order_relaxed);
                for (int c = 0; c < numSortides; ++c)
                    if (sortides[c] != nullptr)
                        std::fill (sortides[c], sortides[c] + numMostres, 0.0f);
                return;
            }

            float* m = mono.data();
            std::fill (m, m + n, 0.0f);

            const float* entradesTros[maxEntrades] {};
            const int numEntradesTros = std::min (numEntrades, maxEntrades);
            for (int c = 0; c < numEntradesTros; ++c)
                entradesTros[c] = entrades[c] != nullptr ? entrades[c] + fet : nullptr;

            const bool mesurant = mesuraLatencia.processa (entradesTros, numEntradesTros, m, n);

            if (! mesurant)
            {
                const bool clicAra = clicActiu.load (std::memory_order_relaxed);
                if (clicAra)
                {
                    if (! clicSonava)
                        clic.reinicia();
                    clic.setTempo (tempoClic.load (std::memory_order_relaxed));
                    clic.processa (m, n);
                }
                clicSonava = clicAra;

                sinte.processa (m, n);

                for (int i = 0; i < n; ++i)
                    m[i] = std::clamp (m[i], -1.0f, 1.0f);
            }

            for (int c = 0; c < numSortides; ++c)
                if (sortides[c] != nullptr)
                    std::copy (m, m + n, sortides[c] + fet);

            fet += n;
        }
    }
}
