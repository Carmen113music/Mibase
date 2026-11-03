#include "mibase/ClicDeProva.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace mibase
{
    namespace
    {
        constexpr double duradaSo[ClicDeProva::numSons] = { 0.035, 0.040, 0.045, 0.120 };
    }

    void ClicDeProva::prepara (double freqMostreig) noexcept
    {
        fs = freqMostreig > 0 ? freqMostreig : 48000.0;
        setSo (so);
        reinicia();
    }

    void ClicDeProva::setSo (int nou) noexcept
    {
        so = std::clamp (nou, 0, numSons - 1);
        durada = (int) (fs * duradaSo[so]);
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

    float ClicDeProva::mostra (double t) noexcept
    {
        switch (so)
        {
            case 1:   // fusta: dues ressonàncies que s'apaguen de pressa i un cop de soroll al començament
            {
                soroll = soroll * 1664525u + 1013904223u;
                const double cop = t < 0.002 ? ((double) (soroll >> 9) / 4194304.0 - 1.0) * (1.0 - t / 0.002) * 0.6 : 0.0;
                const double r = std::sin (fase) * std::exp (-t / 0.010) + 0.6 * std::sin (fase2) * std::exp (-t / 0.006);
                return (float) (0.8 * r + cop);
            }
            case 2:   // analògic: ona triangular que baixa una mica de to
            {
                const double tri = (2.0 / std::numbers::pi) * std::asin (std::sin (fase));
                return (float) (tri * std::exp (-t / 0.014) * std::min (1.0, t / 0.0008));
            }
            case 3:   // campana: dues ones quadrades suaus desafinades, més llarga
            {
                const double a = std::tanh (3.0 * std::sin (fase)), b = std::tanh (3.0 * std::sin (fase2));
                return (float) (0.45 * (a + b) * std::exp (-t / 0.045) * std::min (1.0, t / 0.001));
            }
            default:  // digital: pip
            {
                const double atac = std::min (1.0, t / 0.001);
                return (float) (atac * std::exp (-t / 0.008) * std::sin (fase));
            }
        }
    }

    void ClicDeProva::processa (float* sortida, int numMostres) noexcept
    {
        const auto mostresPerTemps = (long long) std::llround (fs * 60.0 / tempo);
        const double dosPi = 2.0 * std::numbers::pi;

        for (int i = 0; i < numMostres; ++i)
        {
            if (mostresFinsAlTemps <= 0)
            {
                accent = (temps == 0);
                posicioClic = 0;
                fase = fase2 = 0.0;
                switch (so)
                {
                    case 1:  freq1 = accent ? 1400.0 : 1100.0; freq2 = accent ? 2150.0 : 1700.0; break;
                    case 2:  freq1 = accent ? 1250.0 : 900.0;  freq2 = 0.0; break;
                    case 3:  freq1 = accent ? 675.0 : 540.0;   freq2 = accent ? 1000.0 : 800.0; break;
                    default: freq1 = accent ? 2000.0 : 1400.0; freq2 = 0.0; break;
                }
                amplitud = accent ? 0.5f : 0.35f;
                temps = (temps + 1) % tempsPerCompas;
                mostresFinsAlTemps = mostresPerTemps;
            }

            --mostresFinsAlTemps;

            if (posicioClic >= 0)
            {
                const double t = (double) posicioClic / fs;
                double f1 = freq1;
                if (so == 2) f1 *= 1.0 - 0.25 * std::min (1.0, t / 0.03);   // baixa de to
                sortida[i] += amplitud * volum * mostra (t);
                fase += dosPi * f1 / fs;
                fase2 += dosPi * freq2 / fs;

                if (++posicioClic >= durada)
                    posicioClic = -1;
            }
        }
    }
}
