#pragma once

#include "mibase/Instrument.h"
#include <array>
#include <atomic>
#include <string>
#include <vector>

/** 0.70: «Doubler (Mibase)» («intern:doubler»), el primer mòdul fet per al Vocal Studio.

    Fa sonar dues còpies de la veu una mica desafinades (una amunt i l'altra avall, uns cèntims) i una mica
    endarrerides, una a cada costat: sembla que la veu s'hagi gravat dues vegades (el «micro-pitch» dels
    estudis). La veu original no es toca: les còpies s'hi sumen.

    Com funciona: cada còpia és una línia de retard que es va escurçant (o allargant) sense parar, que és el que
    fa pujar (o baixar) l'afinació; dos caps de lectura es reparteixen la feina amb un encreuament perquè no
    se senti el salt. Sense latència. Mateixes normes de fils que la resta d'efectes interns. */
namespace mibase::doubler
{
    inline constexpr const char* idPlugin = "intern:doubler";
    inline constexpr const char* nomPlugin = "Doubler (Mibase)";

    enum Param : int { Quantitat, Desafina, Retard, Amplada, Color, Variacio, Veu, numParams };   // 0.75: Variació; 0.76: Veu (0 = només les còpies, per als retorns)

    struct InfoParam
    {
        const char* clau;
        const char* nom;
        float mn, mx, def;
        const char* ajuda;
    };
    const InfoParam& info (int p);

    class Doubler : public EfecteIntern
    {
    public:
        Doubler();
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

    private:
        std::array<std::atomic<float>, numParams> par;
        std::atomic<bool> demanaBuidar { false };
        double fs = 48000.0;
        int mida = 8192, mascara = 8191, w = 0;
        std::vector<float> linia;     // la veu (mono) que llegeixen les dues còpies
        double fase[2] { 0.0, 0.5 };  // on és cada còpia dins de la finestra (0..1)
        float lpEstat[2] {};          // «Color»: un filtre suau a les còpies
        // 0.75: «Variació»: cada còpia s'avança i s'endarrereix lentament i a l'atzar (com una segona presa de debò)
        double derivaDe[2] {}, derivaA[2] {}, derivaFase[2] {}, derivaPas[2] {};
        unsigned llavor = 2463534242u;
        double atzar() noexcept { llavor ^= llavor << 13; llavor ^= llavor >> 17; llavor ^= llavor << 5; return (double) llavor / 4294967295.0; }
        float sQuant = 0.0f;
        bool primer = true;
    };
}
