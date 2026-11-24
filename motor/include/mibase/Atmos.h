#pragma once

#include "mibase/Delay.h"
#include "mibase/Drive.h"
#include "mibase/Projecte.h"
#include "mibase/Reverb.h"
#include "mibase/SinteBase.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <string>
#include <vector>

/** 0.90: «Mibase ATMOS», el sinte de pads, textures, drones i atmosferes. Va sobre el motor comú (sinte::Base).

    Per veu (12 veus): 4 oscil·ladors (sinus, triangle, serra, quadrada, pols, soroll, wavetable i granular) amb unison,
    obertura estèreo i pan propi; FM 2→1 i 4→3; «Mescla» entre la parella A (1+2) i la B (3+4) (l'evolució del timbre);
    capa de textura (soroll blanc, vent, cruixit, respiració); filtre ressonant LP / HP / BP de 12 o 24 dB amb saturació;
    ADSR de volum i de filtre; envolupant d'«Evolució» (lenta: puja, baixa, puja i baixa, cicle); 3 LFO (globals o per
    veu, lliures o amb el tempo, amb entrada a poc a poc); matriu de modulació de 8 vies; deriva del to (cada veu es
    desafina sola, lentament).
    Sortida: to (brillantor / calidesa), chorus d'ensemble, phaser, el Delay de Mibase, la Reverb de Mibase (hall, plate,
    ambient, shimmer, room, chamber; amb shimmer i freeze), auto-pan i amplada estèreo.
    8 macros (Atac, Alliberament, Filtre, Moviment, Amplada, Brillantor, Calidesa, Profunditat) que mouen molts
    paràmetres alhora: són els controls del mode SIMPLE de la finestra. */
namespace mibase::atmos
{
    inline constexpr const char* idInstrument = "sinte:atmos";
    inline constexpr const char* nom = "Mibase ATMOS";
    inline bool esAtmos (const Pista& t) { return t.tipus == "midi" && t.instrument == idInstrument; }

    enum Param : int
    {
        O1Ona, O1Oct, O1Semi, O1Fi, O1Forma, O1Taula, O1Niv, O1Pan,
        O2Ona, O2Oct, O2Semi, O2Fi, O2Forma, O2Taula, O2Niv, O2Pan,
        O3Ona, O3Oct, O3Semi, O3Fi, O3Forma, O3Taula, O3Niv, O3Pan,
        O4Ona, O4Oct, O4Semi, O4Fi, O4Forma, O4Taula, O4Niv, O4Pan,
        Unison, UniDetune, UniAmple, Fm21, Fm43, Mescla, FaseLliure,
        GrDens, GrMida, GrSpray, GrTo, GrOct,
        NzNiv, NzColor, NzTipus, NzMov,
        Deriva, DerivaVel,
        FTipus, FPendent, FTall, FRes, FEnv, FKey, FDrive, FVel,
        AA, AD, AS, AR, FA, FD, FS, FR,
        EvTemps, EvForma,
        L1Ona, L1Vel, L1Ritme, L1Fase, L1Entrada,
        L2Ona, L2Vel, L2Ritme, L2Fase, L2Entrada,
        L3Ona, L3Vel, L3Ritme, L3Fase, L3Entrada,
        M1Font, M1Desti, M1Quant, M2Font, M2Desti, M2Quant, M3Font, M3Desti, M3Quant, M4Font, M4Desti, M4Quant,
        M5Font, M5Desti, M5Quant, M6Font, M6Desti, M6Quant, M7Font, M7Desti, M7Quant, M8Font, M8Desti, M8Quant,
        ChMix, ChVel, ChProf,
        PhMix, PhVel, PhProf, PhFeedback,
        DlMix, DlNota, DlFeedback, DlPingPong,
        RvTipus, RvMida, RvMix, RvShimmer, RvFreeze, RvTo,
        ApQuant, ApVel, ApRitme,
        Ample,
        MAtac, MAlliberament, MFiltre, MMoviment, MAmplada, MBrillantor, MCalidesa, MProfunditat,
        VelVol, BendRang, Veus, Glide, Volum, Vista,
        ArpMode, ArpRitme, ArpOct, ArpGate, ArpSwing, ArpPatro,
        numParams
    };
    enum Seccio : int { SOsc, SText, SFiltre, SEnv, SLfo, SMod, SFx, SMacro, SVeu, SArp };
    enum Ona : int { Sinus, Triangle, Serra, Quadrada, Pols, Soroll, Wavetable, Granular };
    enum Font : int { FCap, FLfo1, FLfo2, FLfo3, FEnvFiltre, FEvolucio, FVelocitat, FNota, FRoda, FAftertouch, FAtzar, FEnvVolum, numFonts };
    enum Desti : int { DTo, DTall, DRes, DMescla, DForma1, DForma2, DForma3, DForma4, DFormes, DFm, DDetune, DPan, DVolum,
                       DNiv1, DNiv2, DNiv3, DNiv4, DGranuls, DSoroll, numDestins };
    inline constexpr int numTaules = 16;   // 0–7 les del LEAD, 8–15 les de l'ATMOS

    const sinte::InfoParam& info (int p);
    std::string textValor (int p, float v);
    const char* nomTaula (int t);

    /** Les wavetables pròpies de l'ATMOS (cor, cordes, vidre, orgue, aire, fosc, octaves, formant), sense aliàsing. */
    namespace wt
    {
        constexpr int taules = 8, marcs = 8, nivells = 11, mida = 2048;
        void prepara();
        float llegeix (int taula, double fase, float posicio, double dt) noexcept;
    }

    using Preset = drive::Preset;
    using BibliotecaPresets = drive::BibliotecaPresets;
    const std::vector<std::string>& categories();
    const std::vector<Preset>& presetsFabrica();
    std::string estatInicial();
    /** Els tipus de pad (per a l'atzar musical): Analog Warm, Vintage, Digital… */
    const std::vector<std::string>& tipusPad();
    /** Un pad nou a l'atzar d'un tipus (-1: de qualsevol). Sempre musical: unisons i intervals consonants, filtres i
        envolupants raonables, efectes dosificats i un volum anivellat. */
    std::string aleatori (unsigned llavor, int tipus = -1);
    std::string varia (const std::string& estat, unsigned llavor, float quant);
    float previsualitza (const std::string& estat, float* sortida, int n, int nota = 60);

    class Atmos : public sinte::Base
    {
    public:
        static constexpr int maxVeus = 12, maxUnison = 5, maxGrans = 8, midaVector = 512;
        Atmos();
        std::string textValor (int p, float v) const override { return atmos::textValor (p, v); }
        std::string nomComplet (int p) const override;

        // ---- per a la interfície (atòmics) ----
        float evolucio() const noexcept { return evV.load (std::memory_order_relaxed); }          // 0..1 de la veu més nova
        float mesclaAra() const noexcept { return mesclaV.load (std::memory_order_relaxed); }      // 0..1 efectiva
        float formaAra() const noexcept { return formaV.load (std::memory_order_relaxed); }        // forma de l'osc 1 efectiva
        float lfoN (int k) const noexcept { return lfoNV[(size_t) std::clamp (k, 0, 2)].load (std::memory_order_relaxed); }
        float autoPan() const noexcept { return panV.load (std::memory_order_relaxed); }          // -1..1
        float amplada() const noexcept { return ampladaV.load (std::memory_order_relaxed); }      // 0..1 (correlació → amplada)
        /** Les darreres mostres estèreo (una de cada 8) per al vectorscopi. */
        void copiaVector (float* l, float* r, int n) const noexcept;

    protected:
        void preparaSo (double fs) override;
        void notaOn (int nota, int vel) noexcept override;
        void notaOff (int nota) noexcept override;
        void apagaVeus() noexcept override;
        bool veusSonen() const noexcept override;
        int numVeusActives() const noexcept override;
        void renderitza (float* L, float* R, int n) noexcept override;

    private:
        struct Gra { bool viu = false; double fase = 0.0, dt = 0.0; float pos = 0.0f, t = 0.0f, durada = 0.0f, pan = 0.0f; };
        struct Svf { float a1 = 0, a2 = 0; };
        struct Veu
        {
            bool viva = false, tecla = false;
            int nota = 60, ordre = 0;
            float vel = 1.0f, atzar = 0.0f;
            double semis = 60.0, objectiu = 60.0;
            double ph[4][maxUnison] {};
            Gra grans[4][maxGrans];
            float finsGra[4] {};
            float envA = 0, envF = 0, temps = 0.0f;
            int etapaA = 0, etapaF = 0;
            double faseLfo[3] {};
            float shLfo[3] {}, shAnt[3] {};
            float deriva = 0, derivaObj = 0, tDeriva = 0;
            float nzB[2] {}, nzRosa[3] {}, nzVent = 0.0f, nzVentObj = 0.0f, nzCruix = 0.0f;
            Svf fl[2][2], ventF[2];
        };
        Veu veus[maxVeus];
        int comptaOrdre = 0;
        int pila[32] {}; int numPila = 0;
        double darreraNota = -1.0;
        double faseG[3] {};
        float shG[3] {}, shGAnt[3] {};
        float lfoAra[3] {};
        // sortida
        float dcL = 0, dcR = 0, dcxL = 0, dcxR = 0, volSuau = 1.0f;
        float shelfL[2] {}, shelfH[2] {};
        // chorus d'ensemble i phaser
        std::vector<float> chL, chR; int chPos = 0; double chFase = 0.0;
        float apL[6] {}, apR[6] {}, phFbL = 0, phFbR = 0; double phFase = 0.0;
        double apFase = 0.0;
        delay::Delay dl;
        reverb::Reverb rv;
        float darrerDl[4] { -1, -1, -1, -1 }, darrerRv[7] { -1, -1, -1, -1, -1, -1, -1 };
        // per a la interfície
        std::atomic<float> evV { 0.0f }, mesclaV { 0.5f }, formaV { 0.0f }, panV { 0.0f }, ampladaV { 0.0f };
        std::array<std::atomic<float>, 3> lfoNV {};
        std::array<std::atomic<float>, midaVector> vecL {}, vecR {};
        std::atomic<int> posVec { 0 };
        int compteVec = 0;
        float corrLR = 0.0f, potL = 1e-9f, potR = 1e-9f;

        void dispara (Veu& v, int nota, float vel, bool retriga, bool glide) noexcept;
        void allibera (Veu& v) noexcept;
        void renderitzaVeu (Veu& v, float* bL, float* bR, int s0, int m, const float* lfoGlob) noexcept;
        float valorLfo (int ona, double fase, float sh, float shAnt) const noexcept;
        void efectes (float* L, float* R, int n) noexcept;
        float macro (int p) const noexcept { return valor (p); }
    };
}
