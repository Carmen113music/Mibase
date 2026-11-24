#pragma once

#include "mibase/Instrument.h"
#include <array>
#include <atomic>
#include <cstddef>
#include <string>
#include <vector>

/** 0.94: mesura de loudness completa (ITU-R BS.1770-4 / EBU R128), la base del Mastering Studio.

    - Momentary (400 ms), Short-Term (3 s) i Integrated (tota la cançó, amb les dues portes: -70 LUFS i -10 LU).
    - LRA (Loudness Range, EBU Tech 3342: valors de 3 s, portes -70 i -20 LU, del percentil 10 al 95).
    - True Peak (sobremostreig x4, com demana la norma), Sample Peak, RMS i Crest (pic − RMS dels últims 3 s).
    - Correlació estèreo (+1 = mono, 0 = molt ample, negatiu = fase dolenta) i relació Side/Mid dels últims 300 ms.
    - Clipping: mostres a 0 dBFS o més.

    Normes de fils: «processa» al fil d'àudio (sense memòria nova ni esperes); la interfície llegeix «mesures()»
    (atòmics) i demana «reinicia()» quan vol. Els càlculs de l'Integrated i de l'LRA es fan cada 100 ms sobre dos
    histogrames de 0,1 LU, sense guardar la cançó sencera. */
namespace mibase::loudness
{
    constexpr float silenci = -100.0f;   // valor quan no hi ha prou senyal (es mostra «-inf»)

    struct Mesures
    {
        std::atomic<float> momentary { silenci }, shortTerm { silenci }, integrated { silenci }, lra { 0.0f };
        std::atomic<float> momentaryMax { silenci }, shortMax { silenci };
        std::atomic<float> truePeak { silenci }, truePeakMax { silenci };       // dBTP (el de l'últim bloc i el màxim)
        std::atomic<float> samplePeak { silenci }, samplePeakMax { silenci };   // dBFS
        std::atomic<float> rms { silenci }, crest { 0.0f };                     // dB (últims 3 s)
        std::atomic<float> correlacio { 1.0f }, sideMid { silenci };            // -1..1 · dB (Side respecte Mid)
        std::atomic<int> clips { 0 };
        std::atomic<double> segons { 0.0 };                                     // quant s'ha mesurat des del reinici
    };

    /** Un resum (per a les proves i per a l'anàlisi de la cançó sencera). */
    struct Resum
    {
        float integrated = silenci, lra = 0, truePeakMax = silenci, samplePeakMax = silenci, shortMax = silenci, momentaryMax = silenci;
        float correlacio = 1.0f;
        int clips = 0;
        double segons = 0.0;
    };

    class Mesurador
    {
    public:
        Mesurador();
        /** Fora del fil d'àudio. */
        void prepara (double fs);
        /** Fil d'àudio. */
        void processa (const float* L, const float* R, int n) noexcept;
        const Mesures& mesures() const noexcept { return mes; }
        /** Qualsevol fil: el fil d'àudio ho esborra tot al bloc següent. */
        void reinicia() noexcept { demanaReinici.store (true, std::memory_order_relaxed); }
        Resum resum() const noexcept;

    private:
        void reiniciaAra() noexcept;
        void tancaSubbloc() noexcept;
        void calculaPortes() noexcept;
        float truePeakMostra (float x, int canal) noexcept;

        struct Bq { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
                    double operator() (double x) noexcept { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; } };
        Bq k1[2], k2[2];
        double fs = 48000.0;
        int midaSub = 4800, posSub = 0;
        // el subbloc (100 ms) que s'està omplint
        double eK = 0.0, eLin = 0.0, eLL = 0.0, eRR = 0.0, eLR = 0.0, eM = 0.0, eS = 0.0;
        float picSub = 0.0f, tpSub = 0.0f;
        // els últims 30 subblocs (3 s)
        static constexpr int numSubs = 30;
        struct Sub { double eK = 0, eLin = 0, eLL = 0, eRR = 0, eLR = 0, eM = 0, eS = 0; float pic = 0; };
        std::array<Sub, numSubs> subs {};
        int iSub = 0, plens = 0;
        // histogrames de 0,1 LU entre -70 i +10 LUFS: blocs de 400 ms (Integrated) i de 3 s (LRA)
        static constexpr int numBins = 800;
        static constexpr float binMin = -70.0f;
        std::array<double, numBins> histI {}, energiaI {}, histS {}, energiaS {};
        // true peak: sobremostreig x4 (FIR polifàsic de 4 × 12 coeficients)
        static constexpr int tapsFase = 12;
        std::array<std::array<float, tapsFase>, 4> fir {};
        std::array<std::array<float, tapsFase>, 2> hist {};
        int hPos = 0;
        long long mostres = 0;
        std::atomic<bool> demanaReinici { false };
        Mesures mes;
    };

    /** Mesura un tros d'àudio sencer (fora del fil d'àudio). */
    Resum analitza (const float* L, const float* R, std::size_t n, double fs);

    // ---------------------------------------------------------------
    /** 0.94: el mòdul «Loudness Meter» del Mastering Studio: no toca el so, només el mesura. */
    inline constexpr const char* idPlugin = "intern:loudness";
    inline constexpr const char* nomPlugin = "Loudness Meter (Mibase)";

    class Modul : public EfecteIntern
    {
    public:
        void prepara (double f, int) override { m.prepara (f); }
        void processa (float* L, float* R, int n) noexcept override { m.processa (L, R, n); }
        int numParametres() const override { return 0; }
        InfoParametre infoParametre (int) const override { return {}; }
        void posaParametre (int, float) noexcept override {}
        float parametre (int) const noexcept override { return 0.0f; }
        std::string textParametre (int, float) const override { return {}; }
        std::string estat() const override { return "mibase-loudness=1"; }
        void posaEstat (const std::string&) override {}
        Mesurador& mesurador() noexcept { return m; }
    private:
        Mesurador m;
    };

    /** Objectius orientatius de loudness integrat (no són obligatoris: depèn del material i del gènere). */
    struct Objectiu { const char* nom; float lufs; float truePeak; const char* ajuda; };
    const std::vector<Objectiu>& objectius();
}
