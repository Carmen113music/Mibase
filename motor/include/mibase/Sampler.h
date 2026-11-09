#pragma once

#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include "Bateria.h"
#include "Instrument.h"
#include "Projecte.h"

/** 0.26: el samplejador. Un sample (un fitxer) tocat a qualsevol nota respecte a la nota original (Root), amb
    fragment (inici/final), bucle de sustain (Off, Endavant, Ping-pong, Sustain + release des del final del bucle),
    crossfade al punt d'unió, fades, ADSR, filtre, velocitat, Mono/Poli/Legato i robatori de veus.

    El que costa (reversa, suma a mono, time-stretch de l'Speed) es fa FORA del fil d'àudio en preparar la mostra
    («prepara»); els marcadors, l'ADSR, el filtre, el to… són atòmics i es poden canviar mentre sona. */
namespace mibase::sampler
{
    using Mostra = bateria::Mostra;

    /** La mostra a punt per sonar (ja girada, a mono o estirada) i el factor que s'ha aplicat als temps
        (els marcadors en segons del fitxer original es converteixen amb «aMostra»). */
    struct Preparada
    {
        Mostra m;
        double factorTemps = 1.0;     // durada de la preparada / durada de l'original
        bool girada = false;
        double duradaOriginal = 0.0;  // segons
        std::vector<float> pics;      // màxim absolut per cada «midaPic» mostres (per al FLAT)
        static constexpr int midaPic = 512;
        /** Segons del fitxer original → mostra de la preparada. */
        double aMostra (double segons) const noexcept;
    };

    /** Signatura del que obliga a refer la mostra preparada (fitxer, reversa, mono, estirament). */
    std::string signatura (const ParamsSampler& p);

    /** Prepara la mostra (fora del fil d'àudio). */
    std::shared_ptr<const Preparada> prepara (std::shared_ptr<const Mostra> original, const ParamsSampler& p);

    /** Time-stretch (WSOLA): mateixa afinació, durada × factor (factor > 1 = més llarg). */
    std::vector<float> estira (const std::vector<float>& x, double factor, double fs, const std::vector<float>* guia = nullptr);
    void estiraEstereo (const std::vector<float>& L, const std::vector<float>& R, double factor, double fs,
                        std::vector<float>& outL, std::vector<float>& outR);

    /** El pas per zero més proper a «segons» (busca ±10 ms; si no n'hi ha, el mateix punt). */
    double zeroCrossing (const Mostra& m, double segons);

    /** La nota (MIDI) del so, mirant-ne la part central. -1 si no té to clar. */
    int detectaRoot (const Mostra& m);

    /** El samplejador com a instrument. */
    class Samplejador : public Instrument
    {
    public:
        explicit Samplejador (std::shared_ptr<const Preparada> mostra);
        /** Qualsevol fil: paràmetres (marcadors, ADSR, filtre…). La part de la mostra la decideix la preparada. */
        void posaParams (const ParamsSampler& p) noexcept;

        void prepara (double fs, int maxBloc) override;
        void processa (const EventInstrument* ev, int num, float* L, float* R, int n) noexcept override;
        void apagaTot() noexcept override;
        bool fentSo() const noexcept override { return actives > 0; }

        /** Qualsevol fil: escolta el sample des d'un punt (segons del fitxer original) fins al final del fragment,
            sense bucle ni envolupant (per a l'editor). */
        void escolta (double segons) noexcept { peticio.store (std::max (0.0, segons), std::memory_order_relaxed); }
        void aturaEscolta() noexcept { peticio.store (-2.0, std::memory_order_relaxed); }

        const Preparada* preparada() const noexcept { return mostra.get(); }
        /** Per a la interfície: on és ara la veu més recent (mostra de la preparada; -1 si no en sona cap). */
        double posicioActual() const noexcept { return posVisible.load (std::memory_order_relaxed); }

        static constexpr int maxVeusTotal = 40;

    private:
        struct Atomics
        {
            std::atomic<double> inici { 0 }, fi { 0 }, bucleDe { -1 }, bucleA { -1 }, crossfade { 0 }, fadeIn { 0 }, fadeOut { 0 };
            std::atomic<int> modeBucle { 0 }, modeToc { 0 }, veus { 0 }, maxVeus { 16 }, root { 60 }, filtre { 0 }, pendent { 12 };
            std::atomic<float> guany { 1 }, pan { 0 }, width { 1 }, tune { 0 }, cutoff { 20000 }, reso { 0 }, drive { 0 };
            std::atomic<float> atac { 0.002f }, decai { 0.2f }, sost { 1 }, allib { 0.15f }, vel { 0.7f }, ritme { 1 };
        };
        enum class Env { Atac, Decai, Sost, Allib, Mort, Res };
        struct Veu
        {
            int nota = -1;
            bool premuda = false, pedal = false, alliberada = false;   // alliberada: s'ha deixat anar (i el pedal també)
            bool dinsBucle = false, audicio = false;
            double pos = 0.0, pas = 1.0;
            int dir = 1;
            Env env = Env::Res;
            float nivell = 0.0f, guanyVel = 1.0f, mort = 1.0f;
            std::uint64_t ordre = 0;
            float s1[2][2] {}, s2[2][2] {};   // filtre (2 etapes × canal × 2 estats)
            bool actiu() const noexcept { return env != Env::Res; }
        };

        void notaOn (int nota, int vel) noexcept;
        void notaOff (int nota) noexcept;
        void allibera (Veu& v) noexcept;
        void mata (Veu& v) noexcept;
        Veu* veuLliure() noexcept;
        double pasDe (int nota) const noexcept;
        void renderitza (Veu& v, float* L, float* R, int ini, int fi) noexcept;
        float llegeix (const float* x, double pos) const noexcept;

        std::shared_ptr<const Preparada> mostra;
        Atomics a;
        std::array<Veu, maxVeusTotal> veus;
        int actives = 0;
        std::uint64_t comptador = 0;
        double fs = 48000.0;
        bool pedal = false;
        float bend = 0.0f;      // semitons
        std::array<int, 16> premudes {};   // notes premudes (per al Mono/Legato), la darrera al final
        int numPremudes = 0;
        std::atomic<double> posVisible { -1.0 };
        std::atomic<double> peticio { -1.0 };
    };
}
