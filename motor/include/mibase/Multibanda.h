#pragma once

#include "mibase/Instrument.h"
#include <array>
#include <atomic>
#include <string>

/** 0.99: «Multibanda (Mibase)» («intern:multibanda»), el compressor multibanda del Mastering Studio.

    Parteix el so en 2, 3 o 4 bandes amb encreuaments Linkwitz-Riley de 24 dB/oct (amb allpass a les bandes baixes,
    perquè sumades tornin a donar el so pla: no sona com quatre compressors separats) i comprimeix cada banda per
    separat: greus que s'escapen, mitjos congestionats, veus agressives o aguts que punxen, sense tocar la resta.
    Cada banda: Llindar, Ratio, Atac, Alliberament, Guany, Solo i Mute; el detector és estèreo enllaçat. Globals:
    nombre de bandes, els tres encreuaments, Genoll, Mix (paral·lel: el sec passa pels mateixos filtres, sense comb)
    i Sortida. Mesura la reducció de cada banda. Sense latència. */
namespace mibase::multibanda
{
    inline constexpr const char* idPlugin = "intern:multibanda";
    inline constexpr const char* nomPlugin = "Multibanda (Mibase)";

    constexpr int maxBandes = 4;
    enum Global : int { Bandes, X1, X2, X3, Genoll, Mix, Sortida, numGlobals };
    enum PerBanda : int { Llindar, Ratio, Atac, Alliberament, Guany, Solo, Mute, paramsPerBanda };
    constexpr int numParams = numGlobals + maxBandes * paramsPerBanda;
    constexpr int index (int banda, int k) { return numGlobals + banda * paramsPerBanda + k; }

    struct InfoParam { const char* clau; const char* nom; float mn, mx, def; const char* ajuda; };
    /** Per a un paràmetre de banda, la clau és «<clau><banda+1>» (llindar1…). */
    InfoParam info (int p);
    const char* nomBanda (int banda, int numBandes);

    struct Mesures
    {
        std::array<std::atomic<float>, maxBandes> reduccio {};   // dB (positiu)
        std::array<std::atomic<float>, maxBandes> nivell {};     // dB (entrada de la banda)
    };

    class Multibanda : public EfecteIntern
    {
    public:
        Multibanda();
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
        int numBandes() const noexcept;
        std::string estat() const override;
        void posaEstat (const std::string& s) override;
        void buida() noexcept override { demanaBuidar.store (true); }
        Mesures& mesures() noexcept { return mes; }

    private:
        struct Bq { float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
                    float operator() (float x) noexcept { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; } };
        struct Creuament { Bq lp[2], hp[2]; };   // LR4 = dos Butterworth en sèrie
        // per canal: 3 encreuaments i els allpass (banda 1: f2 i f3; banda 2: f3)
        std::array<std::array<Creuament, 3>, 2> cr {};
        std::array<std::array<Bq, 3>, 2> ap {};   // [canal][0: b1@f2, 1: b1@f3, 2: b2@f3]
        std::array<float, 3> fFet { -1, -1, -1 };
        std::array<float, maxBandes> env {}, gr {};
        std::array<std::atomic<float>, numParams> par;
        std::atomic<bool> demanaBuidar { false };
        Mesures mes;
        double fs = 48000.0;
        float gOut = 1.0f, gMix = 1.0f;
        bool primer = true;
        void calculaFiltres() noexcept;
        void buidaAra() noexcept;
    };
}
