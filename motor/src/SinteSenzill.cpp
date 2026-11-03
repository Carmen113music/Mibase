#include "mibase/SinteSenzill.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace mibase
{
    void SinteSenzill::prepara (double freqMostreig) noexcept
    {
        fs = freqMostreig > 0 ? freqMostreig : 48000.0;
        pasAtac = (float) (1.0 / (fs * 0.005));                     // 5 ms de pujada
        factorAlliberament = (float) std::exp (-1.0 / (fs * 0.06)); // ~0,5 s fins al silenci
        silenciaTot();
    }

    void SinteSenzill::midi (std::uint8_t estat, std::uint8_t dada1, std::uint8_t dada2) noexcept
    {
        const int tipus = estat & 0xF0;

        if (tipus == 0x90 && dada2 > 0)
            notaOn (dada1, dada2);
        else if (tipus == 0x80 || (tipus == 0x90 && dada2 == 0))
            notaOff (dada1);
        else if (tipus == 0xB0 && (dada1 == 120 || dada1 == 123))
            silenciaTot();
    }

    void SinteSenzill::notaOn (int nota, int velocitat) noexcept
    {
        if (nota < 0 || nota > 127)
            return;

        // Veu lliure; si no n'hi ha, la que fa més estona que sona.
        Veu* triada = nullptr;

        for (auto& v : veus)
            if (v.fase == Fase::Lliure) { triada = &v; break; }

        if (triada == nullptr)
        {
            triada = &veus[0];
            for (auto& v : veus)
                if (v.ordre < triada->ordre)
                    triada = &v;
        }

        const double freq = 440.0 * std::pow (2.0, (nota - 69) / 12.0);
        const float vel = (float) std::clamp (velocitat, 1, 127) / 127.0f;

        triada->nota = nota;
        triada->increment = 2.0 * std::numbers::pi * freq / fs;
        triada->objectiu = 0.12f + 0.18f * vel;
        triada->fase = Fase::Atac;
        triada->ordre = ++comptador;
    }

    void SinteSenzill::notaOff (int nota) noexcept
    {
        for (auto& v : veus)
            if (v.nota == nota && (v.fase == Fase::Atac || v.fase == Fase::Sosteniment))
                v.fase = Fase::Alliberament;
    }

    void SinteSenzill::silenciaTot() noexcept
    {
        for (auto& v : veus)
            v = Veu {};
    }

    void SinteSenzill::processa (float* sortida, int numMostres) noexcept
    {
        for (auto& v : veus)
        {
            if (v.fase == Fase::Lliure)
                continue;

            for (int i = 0; i < numMostres; ++i)
            {
                switch (v.fase)
                {
                    case Fase::Atac:
                        v.nivell += pasAtac * v.objectiu;
                        if (v.nivell >= v.objectiu) { v.nivell = v.objectiu; v.fase = Fase::Sosteniment; }
                        break;
                    case Fase::Alliberament:
                        v.nivell *= factorAlliberament;
                        if (v.nivell < 0.0002f) { v.nivell = 0.0f; v.fase = Fase::Lliure; v.nota = -1; }
                        break;
                    default:
                        break;
                }

                if (v.fase == Fase::Lliure)
                    break;

                // Ona suau: fonamental + una mica de 2n i 3r harmònic.
                const double s = std::sin (v.angle) + 0.3 * std::sin (2.0 * v.angle) + 0.12 * std::sin (3.0 * v.angle);
                sortida[i] += (float) (s * v.nivell * 0.7);

                v.angle += v.increment;
                if (v.angle > 2.0 * std::numbers::pi)
                    v.angle -= 2.0 * std::numbers::pi;
            }
        }
    }

    int SinteSenzill::veusSonant() const noexcept
    {
        int n = 0;
        for (auto& v : veus)
            if (v.fase != Fase::Lliure)
                ++n;
        return n;
    }
}
