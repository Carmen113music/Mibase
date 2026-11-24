#pragma once

#include "mibase/Instrument.h"
#include <array>
#include <atomic>
#include <string>

/** 0.96: «Transient Shaper (Mibase)» («intern:transitoris»).

    Canvia el cop i la cua del so sense mirar el volum: Atac puja (o, en negatiu, suavitza) el principi de cada cop
    (bateria, púes de guitarra); Sustain allarga (o escurça) el que ve després. Funciona comparant dos seguidors de
    volum, un de ràpid i un de lent: on el ràpid passa davant hi ha un atac; on el lent es queda per sobre, una cua.
    Velocitat: com de curt és l'atac que es detecta. Focus: quina zona escolta el detector (tot, el bombo, la caixa
    o els plats); el canvi de volum s'aplica a tot el so, com als transient designers clàssics. Mix i Sortida.
    Sense latència. Al màster: ajuda que el limitador no s'empassi el cop. */
namespace mibase::transitoris
{
    inline constexpr const char* idPlugin = "intern:transitoris";
    inline constexpr const char* nomPlugin = "Transient Shaper (Mibase)";

    enum Focus : int { Tot, Greus, Mitjos, Aguts, numFocus };
    const char* nomFocus (int f);

    enum Param : int { Atac, Sustain, Velocitat, FocusP, Mix, Sortida, numParams };
    struct InfoParam { const char* clau; const char* nom; float mn, mx, def; const char* ajuda; };
    const InfoParam& info (int p);

    struct Mesures { std::atomic<float> guany { 0.0f }; std::atomic<float> guanyMax { 0.0f }; };   // dB (+ o −)

    class Transitoris : public EfecteIntern
    {
    public:
        Transitoris();
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
        Bq hp, lp;
        int focusFet = -1;
        float aFast = 0, aSlow = 0, sFast = 0, sSlow = 0;   // seguidors (quadrat del nivell)
        float gSuau = 1.0f, gOut = 1.0f, gMix = 1.0f;
        bool primer = true;
        void dissenyaFocus (int f) noexcept;
    };
}
