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

/** 0.93: «Mibase NEO», sinte híbrid de disseny de so (pop d'ara, indie, electrònica, cinema, experimental).
    Va sobre el motor comú (sinte::Base). Per veu, a 2× la freqüència de mostreig:
    - 3 oscil·ladors amb 6 motors: Wavetable (16 taules, posició i «Morph»), Analògic (triangle → serra → quadrada →
      pols), Harmònic (síntesi additiva de 24 parcials amb inclinació, senars/parells i estirament), FM (2 operadors amb
      relació i índex), Granular (núvol de grànuls de les wavetables) i Soroll (de color, amb cruixit).
      Deformació de fase per oscil·lador (Corba, Sync, Mirall, PM i FM des d'un altre oscil·lador, Bits).
    - Ring (1×2), sub, soroll, unison avançat (fins a 8: lineal, supersaw, octaves, acord; obertura i barreja).
    - Plec d'ona i distorsió; filtre LP / HP / BP / Notch / Ladder / Formant (vocals) / Comb + / Comb − amb
      envolupant i seguiment; 3 ressonadors afinats a la nota (harmònic, campana, metall, fusta).
    - 3 envolupants (volum, filtre i modulació), 4 LFO (amb tempo i entrada a poc a poc), seqüenciador de 16 passos.
    - Matriu de modulació de 12 vies: font × «via» (una altra font o una macro que l'escala) → destí, amb modulació a
      ritme d'àudio des de l'oscil·lador 3.
    - 8 macros (Caràcter, Moviment, Brillantor, Agressivitat, Amplada, Espai, Evolució, Morph), cadascuna amb dos destins
      propis i com a «via» de la matriu.
    - Rack d'efectes: Crush (bits i mostreig), desplaçador de freqüència, chorus, Delay i Reverb de Mibase, Espectre
      (inclinació i excitador), amplada.
    Polifonia configurable (1–16), Mono / Legato / Poli, glide, arpegiador. */
namespace mibase::neo
{
    inline constexpr const char* idInstrument = "sinte:neo";
    inline constexpr const char* nom = "Mibase NEO";
    inline bool esNeo (const Pista& t) { return t.tipus == "midi" && t.instrument == idInstrument; }

    inline constexpr int numSlots = 12, numMacros = 8, numPassos = 16;
    enum Motor : int { MWavetable, MAnalogic, MHarmonic, MFm, MGranular, MSoroll };
    enum Warp : int { WCap, WCorba, WSync, WMirall, WPm, WFm, WBits };
    enum Font : int { FCap, FLfo1, FLfo2, FLfo3, FLfo4, FEnvVol, FEnvFiltre, FEnvMod, FVel, FNota, FRoda, FAt, FAtzar, FPassos, FOsc3, numFonts };
    enum Via : int { VCap, VMac1, VMac2, VMac3, VMac4, VMac5, VMac6, VMac7, VMac8, VRoda, VAt, VVel, VEnvMod, VLfo1, numVies };
    enum Desti : int
    {
        DCap, DTo, DTo1, DTo2, DTo3, DPos1, DPos2, DPos3, DMorph, DForma, DWarp, DNiv1, DNiv2, DNiv3,
        DTall, DRes, DVocal, DPan, DVol, DUniDet, DUniAmple, DFold, DDrive, DGrDens, DGrMida, DGrSpray, DRess, DSoroll,
        DCrush, DShift, DChorus, DDelay, DReverb, DEspectre, numDestins
    };

    enum Param : int
    {
        O1Motor, O1Taula, O1Pos, O1Forma, O1Warp, O1WarpQ, O1Oct, O1Semi, O1Fi, O1Niv, O1Pan,
        O2Motor, O2Taula, O2Pos, O2Forma, O2Warp, O2WarpQ, O2Oct, O2Semi, O2Fi, O2Niv, O2Pan,
        O3Motor, O3Taula, O3Pos, O3Forma, O3Warp, O3WarpQ, O3Oct, O3Semi, O3Fi, O3Niv, O3Pan,
        Morph, Ring, SubNiv, SubOct, NzNiv, NzColor,
        UniVeus, UniDetune, UniAmple, UniBlend, UniMode, FaseLliure,
        GrDens, GrMida, GrSpray,
        Fold, DTipus, DQuant,
        FTipus, FPendent, FTall, FRes, FEnv, FKey, FDrive, FVocal,
        RsMix, RsTipus, RsDecay,
        AA, AD, AS, AR, FA, FD, FS, FR, EA, ED, ES, ER,
        L1Ona, L1Vel, L1Ritme, L1Entrada, L1Reinicia,
        L2Ona, L2Vel, L2Ritme, L2Entrada, L2Reinicia,
        L3Ona, L3Vel, L3Ritme, L3Entrada, L3Reinicia,
        L4Ona, L4Vel, L4Ritme, L4Entrada, L4Reinicia,
        Pas1, Pas2, Pas3, Pas4, Pas5, Pas6, Pas7, Pas8, Pas9, Pas10, Pas11, Pas12, Pas13, Pas14, Pas15, Pas16,
        PasRitme, PasLlisca, PasNum,
        M1Font, M1Via, M1Desti, M1Quant, M2Font, M2Via, M2Desti, M2Quant, M3Font, M3Via, M3Desti, M3Quant,
        M4Font, M4Via, M4Desti, M4Quant, M5Font, M5Via, M5Desti, M5Quant, M6Font, M6Via, M6Desti, M6Quant,
        M7Font, M7Via, M7Desti, M7Quant, M8Font, M8Via, M8Desti, M8Quant, M9Font, M9Via, M9Desti, M9Quant,
        M10Font, M10Via, M10Desti, M10Quant, M11Font, M11Via, M11Desti, M11Quant, M12Font, M12Via, M12Desti, M12Quant,
        Mac1, Mac2, Mac3, Mac4, Mac5, Mac6, Mac7, Mac8,
        Mc1D1, Mc1Q1, Mc1D2, Mc1Q2, Mc2D1, Mc2Q1, Mc2D2, Mc2Q2, Mc3D1, Mc3Q1, Mc3D2, Mc3Q2, Mc4D1, Mc4Q1, Mc4D2, Mc4Q2,
        Mc5D1, Mc5Q1, Mc5D2, Mc5Q2, Mc6D1, Mc6Q1, Mc6D2, Mc6Q2, Mc7D1, Mc7Q1, Mc7D2, Mc7Q2, Mc8D1, Mc8Q1, Mc8D2, Mc8Q2,
        CrMix, CrBits, CrMostreig,
        ShHz, ShMix,
        ChMix, ChVel, ChProf,
        DlMix, DlNota, DlFeedback, DlPingPong,
        RvMix, RvMida, RvTipus,
        EsTilt, EsExcita,
        Ample, Volum,
        Mode, Veus, Glide, BendRang, VelVol,
        ArpMode, ArpRitme, ArpOct, ArpGate, ArpSwing, ArpPatro,
        numParams
    };
    enum Seccio : int { SOsc, SMix, SFiltre, SEnv, SLfo, SMod, SMacro, SFx, SVeu, SArp };
    inline constexpr int numTaules = 16;   // 0–7 les del LEAD, 8–15 les de l'ATMOS

    const sinte::InfoParam& info (int p);
    std::string textValor (int p, float v);
    const char* nomTaula (int t);
    /** Una mostra de la taula t (0..15) per dibuixar-la (fase 0..1, posició 0..1). */
    float mostraTaula (int t, double fase, float pos);
    /** Els noms de les macros i el que fan per defecte. */
    const char* nomMacro (int k);

    using Preset = drive::Preset;
    using BibliotecaPresets = drive::BibliotecaPresets;
    const std::vector<std::string>& categories();
    const std::vector<Preset>& presetsFabrica();
    std::string estatInicial();
    /** Tipus per a l'atzar (Baix, Lead, Pluck, Tecles, Textura, Pulsació, Metàl·lic, Vocal, Riser, Impacte…). */
    const std::vector<std::string>& tipusAtzar();
    std::string aleatori (unsigned llavor, int tipus = -1);
    std::string varia (const std::string& estat, unsigned llavor, float quant);
    float previsualitza (const std::string& estat, float* sortida, int n, int nota = 60);
    /** Aplica un preset escrit amb el llenguatge dels presets («clau=valor», «m:font>desti=q@via», «mac3:tall=0.5»)
        sobre el so inicial i hi posa les rutes de les macros que falten. Per a les proves i l'atzar. */
    std::string construeix (const std::string& canvis);

    class Neo : public sinte::Base
    {
    public:
        static constexpr int maxVeus = 16, maxUnison = 8, maxGrans = 6, sobre = 2, parcials = 24;
        Neo();
        std::string textValor (int p, float v) const override { return neo::textValor (p, v); }
        std::string nomComplet (int p) const override;

        // ---- per a la interfície ----
        float posAra (int o) const noexcept { return posV[(size_t) std::clamp (o, 0, 2)].load (std::memory_order_relaxed); }
        float lfoAra (int k) const noexcept { return lfosV[(size_t) std::clamp (k, 0, 3)].load (std::memory_order_relaxed); }
        int pasAra() const noexcept { return pasV.load (std::memory_order_relaxed); }
        /** Quant es mou cada via de la matriu ara mateix (-1..1), per a la llumeta. */
        float activitat (int slot) const noexcept { return actV[(size_t) std::clamp (slot, 0, numSlots - 1)].load (std::memory_order_relaxed); }

    protected:
        void preparaSo (double fs) override;
        void notaOn (int nota, int vel) noexcept override;
        void notaOff (int nota) noexcept override;
        void apagaVeus() noexcept override;
        bool veusSonen() const noexcept override;
        int numVeusActives() const noexcept override;
        void renderitza (float* L, float* R, int n) noexcept override;

    private:
        double fsI = 96000.0;
        struct Svf { float a1 = 0, a2 = 0; };
        struct Gra { bool viu = false; double fase = 0.0, dt = 0.0; float pos = 0.0f, t = 0.0f, durada = 0.0f, pan = 0.0f; };
        struct Veu
        {
            bool viva = false, tecla = false;
            int nota = 60, ordre = 0;
            float vel = 1.0f, atzar = 0.0f;
            double semis = 60.0, objectiu = 60.0;
            double ph[3][maxUnison] {}, phM[3][maxUnison] {};
            float darrera[3] {};          // última mostra de cada oscil·lador (per a PM, FM i l'àudio)
            float osc3Mitja = 0.0f;
            Gra grans[3][maxGrans];
            float finsGra[3] {};
            double fSub = 0.0;
            float envA = 0, envF = 0, envE = 0, temps = 0;
            int etapaA = 0, etapaF = 0, etapaE = 0;
            double faseLfo[4] {};
            float shLfo[4] {}, shAnt[4] {};
            float soroll = 0, cruix = 0, drvTo[2] {};
            Svf fl[2][2], fmt[2][3], rs[3];
            float ld[2][4] {};             // ladder
            int combPos = 0;
            std::vector<float> comb[2];
            float mod[numDestins] {};
        };
        std::vector<Veu> veus;
        int comptaOrdre = 0;
        int pila[32] {}; int numPila = 0;
        double darreraNota = -1.0;
        double faseG[4] {};
        float shG[4] {}, shGAnt[4] {};
        float lfoGlob[4] {};
        // passos
        double fasePas = 0.0; int pas = 0; float valPas = 0.0f;
        float bufL[tros * sobre] {}, bufR[tros * sobre] {};
        sinte::Delmador delL, delR;
        float dcL = 0, dcR = 0, dcxL = 0, dcxR = 0, volSuau = 1.0f;
        // efectes
        float crPhase = 0.0f, crL = 0.0f, crR = 0.0f;
        struct Hilbert { float x1[8] {}, x2[8] {}, y1[8] {}, y2[8] {}; float retard = 0.0f; };
        Hilbert hbL, hbR; double shFase = 0.0;
        std::vector<float> chL, chR; int chPos = 0; double chFase = 0.0;
        delay::Delay dl;
        reverb::Reverb rv;
        float darrerDl[4] { -1, -1, -1, -1 }, darrerRv[3] { -1, -1, -1 };
        float tiltL[2] {}, tiltR[2] {}, excL = 0.0f, excR = 0.0f;
        float modFx[numDestins] {};
        // interfície
        std::array<std::atomic<float>, 3> posV {};
        std::array<std::atomic<float>, 4> lfosV {};
        std::array<std::atomic<float>, numSlots> actV {};
        std::atomic<int> pasV { -1 };

        void dispara (Veu& v, int nota, float vel, bool retriga, bool glide) noexcept;
        void allibera (Veu& v) noexcept;
        void renderitzaVeu (Veu& v, int s0, int m) noexcept;
        void calculaMod (Veu& v, float* fonts) noexcept;
        void sumaMacros (float* mod) const noexcept;
        void efectes (float* L, float* R, int n) noexcept;
        float valorLfo (int ona, double fase, float sh, float shAnt) const noexcept;
        float pm (int p, const float* mod, int d) const noexcept;   // paràmetre p amb el destí d sumat (espai 0..1)
    };
}
