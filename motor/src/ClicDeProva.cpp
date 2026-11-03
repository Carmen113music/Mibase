#include "mibase/ClicDeProva.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace mibase
{
    void ClicDeProva::prepara (double freqMostreig) noexcept
    {
        fs = freqMostreig > 0 ? freqMostreig : 48000.0;
        durada = (int) (fs * 0.035);   // 35 ms
        reinicia();
    }

    void ClicDeProva::reinicia() noexcept
    {
        mostresFinsAlTemps = 0;
        temps = 0;
        posicioClic = -1;
    }

    void ClicDeProva::setTempo (double bpm) noexcept
    {
        if (bpm >= 20.0 && bpm <= 400.0)
            tempo = bpm;
    }

    void ClicDeProva::processa (float* sortida, int numMostres) noexcept
    {
        const auto mostresPerTemps = (long long) std::llround (fs * 60.0 / tempo);

        for (int i = 0; i < numMostres; ++i)
        {
            if (mostresFinsAlTemps <= 0)
            {
                const bool primer = (temps == 0);
                posicioClic = 0;
                fase = 0.0;
                incrementFase = 2.0 * std::numbers::pi * (primer ? 2000.0 : 1400.0) / fs;
                amplitud = primer ? 0.5f : 0.35f;
                temps = (temps + 1) % 4;
                mostresFinsAlTemps = mostresPerTemps;
            }

            --mostresFinsAlTemps;

            if (posicioClic >= 0)
            {
                // Atac de 1 ms i caiguda exponencial.
                const double t = (double) posicioClic / fs;
                const double atac = std::min (1.0, t / 0.001);
                const double caiguda = std::exp (-t / 0.008);
                sortida[i] += (float) (amplitud * atac * caiguda * std::sin (fase));
                fase += incrementFase;

                if (++posicioClic >= durada)
                    posicioClic = -1;
            }
        }
    }
}
