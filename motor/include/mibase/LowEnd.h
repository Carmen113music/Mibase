#pragma once

#include "mibase/Instrument.h"
#include <array>
#include <atomic>
#include <string>

/** 0.99: «Low End Focus (Mibase)» («intern:lowend»), l'eina per als greus del Mastering Studio.

    Separa els greus (per sota de «Freqüència», amb un encreuament Linkwitz-Riley de 24 dB/oct, perquè sumats tornin
    a donar el so pla) i els treballa a part:
    - Sub: puja o baixa el subgreu (al voltant de 45 Hz).
    - Punch: més cop als atacs dels greus (el bombo). Tight: escurça la cua dels greus (baix més ajustat).
    - Escalfor: harmònics suaus als greus (el baix s'escolta millor en altaveus petits).
    - Fang: treu acumulació al voltant de 250 Hz (a tot el so).
    - Mono: per sota d'aquesta freqüència, tot al mig (el baix i el bombo, compatibles amb mono).
    Només treballa amb el mix estèreo: no pot separar el baix del bombo; Punch i Tight actuen sobre tots els greus.
    Mesura el nivell dels greus a l'entrada i a la sortida (per comparar abans i després). Sense latència. */
namespace mibase::lowend
{
    inline constexpr const char* idPlugin = "intern:lowend";
    inline constexpr const char* nomPlugin = "Low End Focus (Mibase)";

    enum Param : int { Freq, Sub, Punch, Tight, Escalfor, Fang, Mono, Sortida, numParams };
    struct InfoParam { const char* clau; const char* nom; float mn, mx, def; const char* ajuda; };
    const InfoParam& info (int p);

    struct Mesures
    {
        std::atomic<float> greusIn { -120.0f }, greusOut { -120.0f };   // dB RMS (300 ms)
        std::atomic<float> canviCop { 0.0f };                            // el guany de Punch/Tight ara (dB)
    };

    class LowEnd : public EfecteIntern
    {
    public:
        LowEnd();
        void prepara (double fs, int maxBloc) override;
        void processa (float* L, float* R, int n) noexcept override;

        int numParametres() const override { return numParams; }
        InfoParametre infoParametre (int i) const override;
        void posaParametre (int i, float v01) noexcept override;
        float parametre (int i) const noexcept override;
        std::string textParametre (int i, float v01) const override;
        static float normal (int p, float v) noexcept;
        static float deNormal (int p, float v01) noexcept;

        void posa (int p, float v) noexcept;
        float valor (int p) const noexcept;
        std::string estat() const override;
        void posaEstat (const std::string& s) override;
        void buida() noexcept override { demanaBuidar.store (true); }
        Mesures& mesures() noexcept { return mes; }

    private:
        struct Bq { float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
                    float operator() (float x) noexcept { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; } };
        std::array<std::atomic<float>, numParams> par;
        std::atomic<bool> demanaBuidar { false };
        Mesures mes;
        double fs = 48000.0;
        Bq lp[2][2], hp[2][2], sub[2], fang[2], mono[2];
        float fFet = -1, subFet = -100, fangFet = -100, monoFet = -1;
        float aF = 0, aS = 0, sF = 0, sS = 0, gCop = 1.0f;
        double eIn = 0, eOut = 0;
        bool primer = true;
        float gOut = 1.0f;
        void calcula() noexcept;
    };
}
