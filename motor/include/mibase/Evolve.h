#pragma once

#include "mibase/ContextMusical.h"
#include "mibase/Drive.h"
#include "mibase/Harmonitzador.h"
#include <climits>
#include <cmath>
#include "mibase/Reverb.h"
#include "mibase/Saturacio.h"
#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

/** 1.13: «EVOLVE (Mibase)», efecte creatiu intern («intern:evolve»). 1.14: fase 2.

    Un sol pom gran, EVOLUTION (0–100 %), transforma el so a poc a poc: cada preset porta la seva «matriu
    d'evolució», una llista de rutes. Cada ruta mou un paràmetre des del seu valor base (el que hi ha a 0 %)
    fins a un valor objectiu, entre dos punts d'Evolution (on comença i on arriba) i amb una corba.

    Mòduls (cadascun amb encès, Mix, nivell d'entrada i de sortida i els seus paràmetres), en l'ordre que es vulgui:
    SCREAMER (saturació, overdrive, distorsió, fuzz i plec, a ×4), FILTER (LP/HP/BP/Notch amb LFO al tempo),
    PUMPER (ducking, trèmolo, gate rítmic i pan), SHIMMER (la Reverb de Mibase per dins), i des de la 1.14
    REVERSER (reverse delay, o reverse reverb: la cua invertida puja just abans de cada tros; mira endavant 2 trossos,
    que el PDC compensa), PITCH (universal amb dos capçals per a qualsevol so, o Veu amb el motor del Vocal Studio:
    conserva el color i mou formants; uns 36 ms de retard) i SCATTER (talls al tempo: repeticions, salts enrere,
    trossos al revés i a doble velocitat, sempre iguals per al mateix tall). Dos LFO (forma, tempo o Hz, fase, offset)
    es poden assignar a qualsevol paràmetre. Solo per escoltar un mòdul (no es desa ni s'exporta). Fase 3: Granular.

    0 % = bypass real: el so surt idèntic (amb el mateix retard que Mibase ja compensa, si n'hi ha). Per sobre, una
    protecció suau impedeix que la sortida passi de 0 dBFS.

    Mateixes normes de fils que la resta d'efectes interns: paràmetres atòmics, prepara() fora del fil d'àudio,
    processa() sense demanar memòria ni esperar-se. Les rutes, les assignacions i l'ordre es canvien des de la interfície. */
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
        // 1.14 (fase 2)
        RevOn, RevMode, RevDiv, RevFeedback, RevPitch, RevDecay, RevMix,
        PitOn, PitMode, PitSemi1, PitSemi2, PitNiv2, PitDetune, PitFi, PitFormant, PitMix,
        ScaOn, ScaDiv, ScaDensity, ScaScatter, ScaFeedback, ScaMix,
        NivIn, NivOut = NivIn + 8,                       // nivell d'entrada i de sortida de cada mòdul (8 llocs: el Granular hi cabrà)
        L1Forma = NivOut + 8, L1Sync, L1Rate, L1Div, L1Depth, L1Fase, L1Offset,
        L2Forma, L2Sync, L2Rate, L2Div, L2Depth, L2Fase, L2Offset,
        numParams
    };
    constexpr int numLfos = 2;
    constexpr int paramsPerLfo = L2Forma - L1Forma;
    inline int paramLfo (int lfo, int k) { return L1Forma + lfo * paramsPerLfo + k; }   // k: 0 forma … 6 offset

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

    enum Modul : int { MScreamer, MFiltre, MPumper, MShimmer, MReverser, MPitch, MScatter, numModuls };
    const char* nomModul (int m);               // «SCREAMER»…
    const char* descripcioModul (int m);
    int paramOn (int m);                        // ScrOn, FilOn…
    int paramMix (int m);                       // el paràmetre que fa d'entrada del mòdul (Mix o Depth)
    /** El primer i l'últim (exclòs) paràmetre del mòdul, inclòs l'encès. */
    int primerParam (int m);
    int finalParam (int m);
    int modulDeParam (int p);                   // -1: global (els nivells In/Out també diuen de quin mòdul són)
    /** 1.14: paràmetres que canvien la latència: no poden ser destí de rutes ni de LFO. */
    bool canviaLatencia (int p);
    bool esModulable (int p);                   // pot ser destí d'una ruta o d'un LFO

    const char* nomModeScreamer (int m);        // Analògic, Overdrive, Distorsió, Fuzz, Plec
    const char* nomTipusFiltre (int t);         // LP, HP, BP, Notch
    const char* nomModePumper (int m);          // Ducking, Trèmolo, Gate, Pan
    const char* nomTipusShimmer (int t);        // Shimmer, Hall dens, Ambient
    const char* nomModeReverser (int m);        // Reverse delay, Reverse reverb
    const char* nomModePitch (int m);           // Universal, Veu
    const char* nomFormaLfo (int f);            // Sinus, Triangle, Quadrada, Serra, Atzar
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
    /** 1.14: una assignació d'un LFO a un paràmetre. «quant»: -100..100 (% del recorregut del paràmetre). */
    struct Assignacio
    {
        int lfo = 0;
        int param = 0;
        float quant = 0.0f;
    };
    /** El valor d'un LFO (-1..1) amb aquestes formes a la fase «f» (0..1); «cicle» és el número de cicle (per a l'atzar). */
    float formaLfo (int forma, float f, long long cicle) noexcept;

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
        std::array<std::atomic<float>, numLfos> lfo {};        // 1.14: el valor de cada LFO ara (-1..1)
        std::array<std::atomic<float>, numLfos> faseLfo {};
        std::atomic<std::uint32_t> blocs { 0 };
    };

    class Evolve : public EfecteIntern
    {
    public:
        static constexpr int maxRutes = 64;
        static constexpr int maxAssig = 24;
        static constexpr int subBloc = 32;
        using Bq = saturacio::Saturacio::Bq;

        Evolve();
        ~Evolve() override;
        void prepara (double fs, int maxBloc) override;
        void processa (float* L, float* R, int n) noexcept override;
        /** 1.14: el Reverse reverb mira endavant (2 trossos) i el Pitch en mode Veu, uns 36 ms: el PDC ho compensa. */
        int latencia() const noexcept override;

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
        std::vector<Assignacio> assignacions() const;
        void posaAssignacions (const std::vector<Assignacio>& v);
        /** 1.14: escolta només un mòdul (-1: tots). No es desa mai (ni s'exporta). */
        void posaSolo (int m) noexcept { solo.store (m); }
        int soloActual() const noexcept { return solo.load(); }

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
        struct AssigA { std::atomic<int> lfo { 0 }, param { 0 }; std::atomic<float> quant { 0.0f }; };
        std::array<AssigA, maxAssig> as;
        std::atomic<int> nAssig { 0 }, solo { -1 };
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

        // ---- 1.14 ----
        std::array<float, 8> nivIn {}, nivOut {};
        double faseLliure[numLfos] {};
        float valLfo[numLfos] {};
        // retard general (latència): l'entrada passa per aquí; la cadena llegeix «latRev» mostres enrere i el so sec
        // de referència, «latRev + latPit»
        std::vector<float> retL, retR;
        long long retW = 0, retMascara = 0;
        int latRevAra = -1, latPitAra = -1;
        struct Reverser
        {
            std::vector<float> rec[2], play[2];
            int recN = 0, playN = 0, cap = 0;
            double ph = 0.0;          // posició de lectura (mostres) dins del tros que sona
            long long tros = LLONG_MIN;
            float mix = 0.0f;
        } rvs;
        std::unique_ptr<reverb::Reverb> revSw;   // la reverb del Reverse reverb
        struct Desplacador   // canvi de to «universal» (dos capçals que giren sobre un retard): serveix per a tot
        {
            double fase = 0.0;
            void avanca (double r, double finestra) noexcept { fase += (1.0 - r) / finestra; fase -= std::floor (fase); }
        };
        struct Pitch
        {
            std::vector<float> buf[2];
            long long w = 0, mascara = 0;
            Desplacador d[2][2];     // [veu][canal]
            double finestra = 2048.0;
            float mix = 0.0f, niv2 = 0.0f;
            canvito::Analitzador an;
            canvito::Veu veu[2];
            std::vector<float> sec[2];
            long long secW = 0, secMascara = 0;
            bool veuPreparada = false;
        } pit;
        struct Scatter
        {
            std::vector<float> buf[2];
            long long w = 0, mascara = 0, valid = 0;
            long long tros = LLONG_MIN, iniciTros = 0, q = 0;
            int tipus = 0, k = 1, llarg = 1;
            long long font = 0;
            float mix = 0.0f, guany = 1.0f;
        } sca;
        float rawL[subBloc] {}, rawR[subBloc] {};

        int latenciaRev (double bpm) const noexcept;
        int latenciaPit() const noexcept;
        void calculaLfos (int n, double negres, double bpm) noexcept;
        void processaReverser (float* L, float* R, int n, double negres, double bpm) noexcept;
        void processaPitch (float* L, float* R, int n, bool calla) noexcept;
        void processaScatter (float* L, float* R, int n, double negres, double bpm) noexcept;

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
