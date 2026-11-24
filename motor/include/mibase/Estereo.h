#pragma once

#include "mibase/Instrument.h"
#include <array>
#include <atomic>
#include <string>

/** 0.94: «Estèreo (Mibase)» («intern:estereo»), l'Stereo Imager del Mastering Studio.

    Treballa en Mid/Side: el Mid (el que és al mig: veu, bombo, baix, caixa) i el Side (el que és diferent a cada
    costat: guitarres obertes, pads, reverbs). L'amplada només toca el Side, i es pot repartir en tres bandes
    (greus, mitjos, aguts): per exemple, obrir els aguts i tancar els greus. Les bandes es fan restant (el que
    no és greu ni agut és mitjà), de manera que amb tot al 100 % el so surt igual que entra.

    «Mono als greus» treu el Side per sota d'una freqüència (filtre de 24 dB/oct): el baix i el bombo, al mig i
    compatibles amb mono. També: nivell del Mid i del Side, balanç, escolta en mono i sortida.
    Mesura la correlació (entrada i sortida) i el Side respecte del Mid. Sense latència. */
namespace mibase::estereo
{
    inline constexpr const char* idPlugin = "intern:estereo";
    inline constexpr const char* nomPlugin = "Estèreo (Mibase)";

    enum Param : int { Amplada, AmpGreus, AmpMitjos, AmpAguts, Creu1, Creu2, MonoGreus, Mid, Side, Balanc, Mono, Sortida, numParams };

    struct InfoParam { const char* clau; const char* nom; float mn, mx, def; const char* ajuda; };
    const InfoParam& info (int p);

    struct Mesures
    {
        std::atomic<float> correlacioIn { 1.0f }, correlacioOut { 1.0f };
        std::atomic<float> sideMidIn { -100.0f }, sideMidOut { -100.0f };   // dB
    };

    class Estereo : public EfecteIntern
    {
    public:
        Estereo();
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
        const Mesures& mesures() const noexcept { return mes; }

    private:
        struct Bq { float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
                    float operator() (float x) noexcept { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; } };
        void calcula() noexcept;
        std::array<std::atomic<float>, numParams> par;
        std::atomic<bool> demanaBuidar { false };
        Mesures mes;
        double fs = 48000.0;
        Bq lp1, hp2, hpMono[2];
        float fC1 = -1, fC2 = -1, fMono = -2;
        float g[6] {};   // greus, mitjos, aguts, mid, side, sortida (suavitzats)
        float bl = 1, br = 1;
        // mesures (suavitzades)
        double ll[2] {}, rr[2] {}, lr[2] {}, mm[2] {}, ss[2] {};
        bool primer = true;
    };
}
