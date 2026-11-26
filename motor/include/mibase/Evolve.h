#pragma once

#include "mibase/ContextMusical.h"
#include "mibase/Drive.h"
#include "mibase/Reverb.h"
#include "mibase/Saturacio.h"
#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

/** 1.13: «EVOLVE (Mibase)», efecte creatiu intern («intern:evolve»), fase 1.

    Un sol pom gran, EVOLUTION (0–100 %), transforma el so a poc a poc: cada preset porta la seva «matriu
    d'evolució», una llista de rutes. Cada ruta mou un paràmetre des del seu valor base (el que hi ha a 0 %)
    fins a un valor objectiu, entre dos punts d'Evolution (on comença i on arriba) i amb una corba.

    Mòduls d'aquesta fase (cadascun amb encès, Mix i els seus paràmetres): SCREAMER (saturació, overdrive,
    distorsió, fuzz i plec d'ona, a ×4), FILTER (LP/HP/BP/Notch amb ressonància, drive i LFO amb el tempo),
    PUMPER (ducking, trèmolo, gate rítmic i panoràmica, amb divisions amb punt i tresets) i SHIMMER (la Reverb
    de Mibase per dins: shimmer, hall dens o ambient, amb freeze). Fases següents: Scatter, Granular, Reverser,
    Pitch, mode Advanced complet i els LFO lliures. L'ordre dels mòduls es desa amb l'estat.

    0 % = bypass real: el so surt idèntic (no passa per res; ni l'entrada ni la sortida). Per sobre, una protecció
    suau impedeix que la sortida passi de 0 dBFS. Sense retard (latència 0).

    Mateixes normes de fils que la resta d'efectes interns: paràmetres atòmics, prepara() fora del fil d'àudio,
    processa() sense demanar memòria ni esperar-se. Les rutes i l'ordre es canvien des de la interfície. */
namespace mibase::evolve
{
    inline constexpr const char* idPlugin = "intern:evolve";
    inline constexpr const char* nomPlugin = "EVOLVE (Mibase)";

    /** Els paràmetres. Les claus de text es desen: no es poden canviar. Els mòduls nous s'afegiran al final. */
    enum Param : int
    {
        Evolution, DryWet, Entrada, Sortida, Hold, Vista,
        ScrOn, ScrMode, ScrDrive, ScrTone, ScrMix, ScrOut,
        FilOn, FilTipus, FilFreq, FilReso, FilDrive, FilLfoDiv, FilLfo, FilMix,
        PmpOn, PmpMode, PmpDiv, PmpDepth, PmpForma, PmpPatro,
        ShmOn, ShmTipus, ShmPitch, ShmAmt, ShmDecay, ShmSize, ShmDiff, ShmMod, ShmDamp, ShmFreeze, ShmMix,
        numParams
    };

    struct InfoParam
    {
        const char* clau;
        const char* nom;
        float mn, mx, def;
        bool enter, log;
        const char* ajuda;
    };
    const InfoParam& info (int p);
    std::string textValor (int p, float v);
    int paramDeClau (const std::string& clau);

    enum Modul : int { MScreamer, MFiltre, MPumper, MShimmer, numModuls };
    const char* nomModul (int m);               // «SCREAMER»…
    const char* descripcioModul (int m);
    int paramOn (int m);                        // ScrOn, FilOn…
    int paramMix (int m);                       // el paràmetre que fa d'entrada del mòdul (Mix o Depth)
    /** El primer i l'últim (exclòs) paràmetre del mòdul, inclòs l'encès. */
    int primerParam (int m);
    int finalParam (int m);
    int modulDeParam (int p);                   // -1: global

    const char* nomModeScreamer (int m);        // Analògic, Overdrive, Distorsió, Fuzz, Plec
    const char* nomTipusFiltre (int t);         // LP, HP, BP, Notch
    const char* nomModePumper (int m);          // Ducking, Trèmolo, Gate, Pan
    const char* nomTipusShimmer (int t);        // Shimmer, Hall dens, Ambient
    int numDivisions();
    const char* nomDivisio (int i);             // 1/1, 1/2., … 1/32 (de més llarg a més curt)
    double divisioEnNegres (int i);
    int numPatrons();
    const char* nomPatro (int i);
    const char* patro (int i);                  // 16 passos: 'x' sona, '.' calla

    /** Una ruta de la matriu d'evolució. «de» i «a»: punts d'Evolution (0–100) on comença a moure's i on arriba
        al valor objectiu. Fora d'aquest tram, el paràmetre es queda al valor base o a l'objectiu. */
    enum Corba : int { Lineal, Suau, Rapida, EnS, numCorbes };
    const char* nomCorba (int c);
    struct Ruta
    {
        int param = 0;
        float de = 0.0f, a = 100.0f;
        float valor = 0.0f;
        int corba = Lineal;
    };
    /** La forma de la corba (t de 0 a 1). */
    float aplicaCorba (int corba, float t) noexcept;
    /** El valor d'un paràmetre amb aquesta Evolution, a partir del valor base i de la ruta (si n'hi ha). */
    float valorEnEvolution (const Ruta& r, float base, float evolution) noexcept;

    struct Mesures
    {
        std::atomic<float> entrada { 0.0f }, sortida { 0.0f };
        std::atomic<float> evolution { 0.0f };                 // la que sona ara (suavitzada, amb el Hold)
        std::atomic<float> bypass { 1.0f };                    // 1 = so original (0 %)
        std::array<std::atomic<float>, numModuls> activitat {}; // 0..1: quant treballa cada mòdul ara
        std::atomic<float> protegeix { 0.0f };                 // la protecció de sortida actua (0..1)
        std::atomic<std::uint32_t> blocs { 0 };
    };

    class Evolve : public EfecteIntern
    {
    public:
        static constexpr int maxRutes = 64;
        static constexpr int subBloc = 32;
        using Bq = saturacio::Saturacio::Bq;

        Evolve();
        ~Evolve() override;
        void prepara (double fs, int maxBloc) override;
        void processa (float* L, float* R, int n) noexcept override;
        int latencia() const noexcept override { return 0; }

        int numParametres() const override { return numParams; }
        InfoParametre infoParametre (int i) const override;
        void posaParametre (int i, float v01) noexcept override;
        float parametre (int i) const noexcept override;
        std::string textParametre (int i, float v01) const override;
        static float normal (int p, float v) noexcept;
        static float deNormal (int p, float v01) noexcept;

        void posa (int p, float v) noexcept;
        float valor (int p) const noexcept;
        /** El valor que fa servir el so amb aquesta Evolution (base + ruta). Qualsevol fil. */
        float efectiu (int p, float evolution) const noexcept;

        // ---- Matriu i ordre (fil de la interfície) ----
        std::vector<Ruta> rutes() const;
        void posaRutes (const std::vector<Ruta>& r);
        std::array<int, numModuls> ordre() const noexcept;
        void posaOrdre (const std::array<int, numModuls>& o) noexcept;

        std::string estat() const override;
        void posaEstat (const std::string& s) override;
        void buida() noexcept override { demanaBuidar.store (true); }
        const Mesures& mesures() const noexcept { return mes; }

        /** D'on llegeix el tempo (bpm). nullptr = 120. On és la cançó ho diu ContextMusical. */
        void posaTempo (const std::atomic<double>* t) noexcept;

        /** Una combinació a l'atzar però musical i segura (2–4 mòduls, rutes escalonades, límits de seguretat).
            Torna l'estat sencer; l'Evolution d'ara es manté. */
        std::string estatAtzar (unsigned llavor) const;

    private:
        struct RutaA
        {
            std::atomic<int> param { 0 }, corba { 0 };
            std::atomic<float> de { 0.0f }, a { 100.0f }, valor { 0.0f };
        };
        std::array<std::atomic<float>, numParams> par;
        std::array<RutaA, maxRutes> rt;
        std::atomic<int> nRutes { 0 };
        std::array<std::atomic<int>, numModuls> ord;
        std::atomic<bool> demanaBuidar { false };
        const std::atomic<double>* tempo = nullptr;
        const ContextMusical* ctx = nullptr;
        Mesures mes;
        double fs = 48000.0;

        // ---- estat del fil d'àudio ----
        float evoS = 0.0f, evoHold = 0.0f, kBy = 0.0f, gIn = 1.0f, gOut = 1.0f, dw = 1.0f;
        bool holdAbans = false, brut = false, primer = true;
        double negresLliure = 0.0;
        std::array<float, numParams> ef {};

        struct Screamer
        {
            struct Canal { Bq up[4], down[4], dc, tilt, lp; };
            Canal ch[2];
            float g = 1.0f, comp = 1.0f, mix = 0.0f, out = 1.0f, eIn = 0.0f, eOut = 0.0f;
            float toneVist = -1.0f; int modeVist = -1;
        } scr;
        struct Filtre
        {
            float s1[2] {}, s2[2] {};
            float g = 0.1f, k = 1.4f, mix = 0.0f, drv = 1.0f;
        } fil;
        struct Pumper
        {
            float gL = 1.0f, gR = 1.0f, gate = 1.0f, depth = 0.0f;
        } pmp;
        std::unique_ptr<reverb::Reverb> rev;
        float shmMixVist = -1.0f;

        void neteja() noexcept;
        void calculaEfectius (float evolution) noexcept;
        void processaScreamer (float* L, float* R, int n) noexcept;
        void processaFiltre (float* L, float* R, int n, double negres, double bpm) noexcept;
        void processaPumper (float* L, float* R, int n, double negres, double bpm) noexcept;
        void processaShimmer (float* L, float* R, int n) noexcept;
    };

    // ---- Presets ----
    using Preset = drive::Preset;                       // { nom, categoria, descripcio, estat }
    using BibliotecaPresets = drive::BibliotecaPresets; // la mateixa biblioteca (fitxer a part)
    const std::vector<std::string>& categories();
    const std::vector<Preset>& presetsFabrica();
}
