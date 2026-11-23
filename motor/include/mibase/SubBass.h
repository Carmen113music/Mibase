#pragma once

#include "mibase/Drive.h"
#include "mibase/Instrument.h"
#include "mibase/Projecte.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

/** 0.88: «Mibase SUB & BASS», el primer sinte del motor de síntesi nou. Només baixos.
    Per veu: 2 oscil·ladors analògics virtuals (polyBLEP; sinus, triangle, serra, quadrada, pols) amb unison, sync i FM,
    sub-oscil·lador, soroll, clic d'atac i envolupant de to; filtre (Ladder 12/24, SVF 12/24, Diode, MS-20), drive i VCA.
    Tot el camí de la veu va a 2× la freqüència de mostreig i es delmat amb un filtre de mitja banda.
    Un LFO global, glide, modes Mono / Legato / Poli (8 veus), velocitat, pitch bend, roda de modulació,
    MIDI learn i CC configurables. Mateixes normes de fils que la resta d'instruments. Sense JUCE. */
namespace mibase::subbass
{
    inline constexpr const char* idInstrument = "sinte:subbass";
    inline constexpr const char* nom = "Mibase SUB & BASS";
    inline bool esSubBass (const Pista& t) { return t.tipus == "midi" && t.instrument == idInstrument; }

    /** El tempo del projecte (el posa la interfície): per a l'LFO sincronitzat. */
    std::atomic<double>& tempo();

    enum Param : int
    {
        O1Ona, O1Oct, O1Semi, O1Fi, O1Pw, O1Niv,
        O2Ona, O2Oct, O2Semi, O2Fi, O2Pw, O2Niv,
        Sync, Fm, Unison, UniDetune, UniAmple, Fase, FaseLliure, Clic,
        SubOna, SubOct, SubNiv, SubFiltre, SorollNiv, SorollColor,
        FModel, FPendent, FTall, FRes, FEnv, FKey, FDrive,
        AA, AD, AS, AR, FA, FD, FS, FR,
        LOna, LVel, LRitme, LDesti, LQuant, LRetrig,
        DTipus, DQuant, DTo, DMix, DLloc,
        RodaTall, RodaLfo, VelVol, VelFiltre, BendRang, PEnvQuant, PEnvTemps,
        Mode, Glide, Volum, Ample, Limit,
        numParams
    };

    enum Seccio : int { SOsc, SSub, SFiltre, SEnv, SLfo, SDrive, SMod, SSortida };
    enum Escala : int { Lineal, Enter, Log, Cub };

    struct InfoParam
    {
        const char* clau;
        const char* nom;
        int seccio;
        float mn, mx, def;
        int escala;
        const char* opcions;   // «A|B|C» per als selectors (nullptr si no n'és)
        const char* ajuda;
    };
    const InfoParam& info (int p);
    /** Les opcions d'un selector (buit si no n'és). */
    std::vector<std::string> opcions (int p);
    std::string textValor (int p, float v);
    float normal (int p, float v) noexcept;      // valor real → 0..1
    float deNormal (int p, float v01) noexcept;  // 0..1 → valor real (arrodonit si és enter)

    /** Durades de l'LFO sincronitzat, en negres (índex 1.. de LRitme; 0 = lliure). */
    double negresRitme (int ritme) noexcept;

    // ---- Presets ----
    using Preset = drive::Preset;                       // nom, categoria, descripció, estat
    using BibliotecaPresets = drive::BibliotecaPresets;  // presets propis i favorits (el mateix sistema que els efectes)
    const std::vector<std::string>& categories();
    const std::vector<Preset>& presetsFabrica();
    /** L'estat del so inicial (sense preset). */
    std::string estatInicial();
    /** Un so nou a l'atzar però sempre útil com a baix (llavor: el mateix número, el mateix so). */
    std::string aleatori (unsigned llavor);
    /** El so actual una mica canviat (quant: 0..1). */
    std::string varia (const std::string& estat, unsigned llavor, float quant);

    /** Una forma d'ona per dibuixar: fa sonar el so (nota «nota») fora del fil d'àudio i en torna «n» punts
        de dos cicles, un cop passat l'atac. Torna el pic. */
    float previsualitza (const std::string& estat, float* sortida, int n, int nota = 36);

    /** El sinte. */
    class SubBass : public Instrument
    {
    public:
        static constexpr int maxVeus = 8, maxUnison = 7, sobre = 2, tros = 256;
        SubBass();

        void prepara (double fs, int maxBloc) override;
        void processa (const EventInstrument* events, int numEvents, float* L, float* R, int n) noexcept override;
        void apagaTot() noexcept override;
        bool fentSo() const noexcept override;

        int numParametres() const override { return numParams; }
        InfoParametre infoParametre (int i) const override;
        void posaParametre (int i, float v01) noexcept override;
        float parametre (int i) const noexcept override;
        std::string textParametre (int i, float v01) const override;

        /** Valors reals (qualsevol fil). */
        void posa (int p, float v) noexcept;
        float valor (int p) const noexcept { return par[(size_t) p].load (std::memory_order_relaxed); }

        /** Tots els ajustos en text (inclou les assignacions de CC). posaEstat: fora del fil d'àudio o des de qualsevol
            fil (només toca atòmics). Un text buit o d'un altre instrument deixa el so inicial. */
        std::string estat() const;
        void posaEstat (const std::string& s);
        /** Només els paràmetres (sense CC ni nom): per comparar amb un preset. */
        std::string estatSo() const;
        /** El nom del preset d'on ve el so (només fil de la interfície; es desa amb estat()). */
        std::string nomPreset;

        // ---- MIDI learn i CC ----
        /** El paràmetre que mou cada CC (-1 cap). CC1 és sempre la roda de modulació i CC64 el pedal. */
        int paramDeCC (int cc) const noexcept { return cc >= 0 && cc < 128 ? ccMapa[(size_t) cc].load (std::memory_order_relaxed) : -1; }
        void assignaCC (int cc, int param) noexcept;
        void treuCC (int param) noexcept;
        int ccDeParam (int param) const noexcept;
        /** El proper CC que arribi s'assignarà a aquest paràmetre (-1: deixa d'escoltar). */
        void aprenCC (int param) noexcept { aprenent.store (param); }
        int aprenentCC() const noexcept { return aprenent.load(); }
        static bool ccReservat (int cc) noexcept { return cc == 1 || cc == 64 || cc >= 120; }

        // ---- Per a la interfície ----
        float carregaCpu() const noexcept { return cpu.load (std::memory_order_relaxed); }
        float roda() const noexcept { return rodaV.load (std::memory_order_relaxed); }
        float bend() const noexcept { return bendV.load (std::memory_order_relaxed); }
        bool notaSona (int n) const noexcept { return n >= 0 && n < 128 && (notesOn[(size_t) (n >> 6)].load (std::memory_order_relaxed) >> (n & 63) & 1u) != 0; }
        float hzDarrera() const noexcept { return hzUltima.load (std::memory_order_relaxed); }
        float nivell() const noexcept { return nivellV.load (std::memory_order_relaxed); }
        float lfo() const noexcept { return lfoV.load (std::memory_order_relaxed); }
        double frequenciaMostreig() const noexcept { return fs; }
        int veusActives() const noexcept { return actives.load (std::memory_order_relaxed); }
        /** Les últimes mostres que han sortit (mono), per a l'oscil·loscopi: «n» ≤ midaOna. */
        static constexpr int midaOna = 4096;
        void copiaOna (float* desti, int n) const noexcept;

    private:
        std::array<std::atomic<float>, numParams> par;
        std::array<std::atomic<int>, 128> ccMapa;
        std::atomic<int> aprenent { -1 };
        std::atomic<float> lfoV { 0.0f }, cpu { 0.0f }, rodaV { 0.0f }, bendV { 0.0f }, hzUltima { 55.0f }, nivellV { 0.0f };
        std::atomic<int> actives { 0 };
        std::array<std::atomic<std::uint64_t>, 2> notesOn;
        std::array<std::atomic<float>, midaOna> ona;
        std::atomic<int> posOna { 0 };
        std::atomic<bool> demanaApagar { false };

        double fs = 48000.0, fsI = 96000.0;

        struct Filtre
        {
            float s[4] {}, ic1 = 0, ic2 = 0, jc1 = 0, jc2 = 0, hp = 0;
            void buida() { for (auto& x : s) x = 0; ic1 = ic2 = jc1 = jc2 = hp = 0; }
        };
        struct Veu
        {
            bool viva = false, tecla = false;
            int nota = 60, ordre = 0;
            float vel = 1.0f;
            double semis = 60.0, objectiu = 60.0;   // to actual (glide) i on va
            double f1[maxUnison] {}, f2[maxUnison] {}, fSub = 0.0;
            float envA = 0, envF = 0;
            int etapaA = 0, etapaF = 0;            // 0 parada, 1 atac, 2 caiguda/sostingut, 3 alliberament
            float tPEnv = 0, clic = 0;
            float soroll = 0, drvTo[2] {}, dc[2] {}, dcx[2] {};
            Filtre fl[2];
            float g = 0.1f, G = 0.1f, k = 0.0f;   // coeficients del filtre del tros
            int compta = 0;
        };
        Veu veus[maxVeus];
        int comptaOrdre = 0;
        int pila[32] {}; int numPila = 0;          // notes premudes (mono/legato)
        bool pedal = false, sostingudes[128] {};
        float rodaMod = 0.0f, bendSemis = 0.0f;
        double fase = 0.0; float lfoVal = 0.0f, lfoSH = 0.0f; std::uint32_t rnd = 22222u;
        double darreraNota = -1.0;               // per al glide en poli
        float bufL[tros * sobre] {}, bufR[tros * sobre] {};
        float histL[64] {}, histR[64] {}; int posHist = 0;
        float hb[64] {}; int numHb = 0;
        float dcL = 0, dcR = 0, dcxL = 0, dcxR = 0;
        float volSuau = 1.0f;

        void noteOn (int nota, int vel) noexcept;
        void noteOff (int nota) noexcept;
        void dispara (Veu& v, int nota, float vel, bool retriga, bool glide) noexcept;
        void allibera (Veu& v) noexcept;
        void controla (int cc, int valor) noexcept;
        void renderitza (int n) noexcept;   // n mostres de sortida
        void renderitzaVeu (Veu& v, int s0, int m, float lfo) noexcept;
        float aleat() noexcept { rnd ^= rnd << 13; rnd ^= rnd >> 17; rnd ^= rnd << 5; return (float) (rnd & 0xffffff) / 8388608.0f - 1.0f; }
        void marcaNota (int nota, bool on) noexcept;
    };
}
