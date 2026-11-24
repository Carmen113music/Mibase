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

/** 1.4: «Mibase VINTAGE», sinte analògic virtual de caràcter clàssic (famílies Moog, Prophet, Juno, Jupiter, Oberheim,
    ARP, MS-20, CS-80). Va sobre el motor comú (sinte::Base). Per veu («targeta de veu»), a 2× la freqüència de mostreig:
    - 3 oscil·ladors (serra, quadrada, triangle, sinus, pols) amb peus de 32' a 2', semitons i fi, PWM, sync (2 → 1),
      FM lineal (3 → 1), envolupant del filtre al to de l'osc 2 (escombrades de sync) i osc 3 a freqüència baixa
      (com a font de modulació, a la manera del Minimoog). Sub (−1/−2 octaves), soroll blanc o rosa i ring (1×2).
    - Filtres: Ladder (24/12 dB, retroalimentació saturada), OTA (24/12 dB, net, tipus IR3109/CEM), SEM (12 dB d'estat
      variable, suau) i MS-20 (12 dB, agressiu i que xiula). Modes LP, BP, HP i Notch. Drive a l'entrada i HPF a la sortida.
    - 2 ADSR analògiques (corbes de condensador), LFO (global o per veu, amb retard), font de modulació LFO / Osc 3 / Soroll,
      roda de modulació (vibrato i filtre) i aftertouch al filtre.
    - Mono, Legato i Poli (fins a 16 veus), glide, unison (fins a 8 veus per nota).
    - Caràcter analògic, tot desactivable: deriva, inestabilitat, variació del filtre entre veus, calidesa, saturació,
      soroll de fons, edat (toleràncies de components) i variació estèreo entre veus.
    - Efectes vintage: chorus de BBD (I, II, I+II), phaser, delay de cinta o BBD i reverb de molla, placa, cambra o sala. */
namespace mibase::analogic
{
    inline constexpr const char* idInstrument = "sinte:vintage";
    inline constexpr const char* nom = "Mibase VINTAGE";
    inline bool esVintage (const Pista& t) { return t.tipus == "midi" && t.instrument == idInstrument; }

    enum Ona : int { OSerra, OQuadrada, OTriangle, OSinus, OPols };
    enum Filtre : int { FLadder, FOta, FSem, FMs20 };
    enum ModeF : int { MLp, MBp, MHp, MNotch };

    enum Param : int
    {
        O1Ona, O1Peu, O1Semi, O1Fi, O1Pw, O1Niv,
        O2Ona, O2Peu, O2Semi, O2Fi, O2Pw, O2Niv,
        O3Ona, O3Peu, O3Semi, O3Fi, O3Pw, O3Niv,
        O3Baixa, Sync, EnvA2, Fm,
        SubNiv, SubOct, SubOna, NzNiv, NzColor, Ring,
        FTipus, FPendent, FMode, FTall, FRes, FEnv, FKey, FVel, FDrive, FHpf,
        AA, AD, AS, AR, FA, FD, FS, FR, VelVol,
        LOna, LVel, LRitme, LRetard, LPerVeu, LFont, LTo, LPw, LTall, LVol,
        RodaTo, RodaTall, AtTall,
        Analogic, Deriva, Inestable, VarFiltre, Calidesa, Saturacio, Soroll, Edat, VarEstereo,
        Chorus, PhMix, PhVel, DlMix, DlNota, DlFeedback, DlTipus, RvMix, RvTipus, RvMida,
        Ample, Volum,
        Mode, Veus, Unison, UniDetune, Glide, BendRang, FaseLliure,
        ArpMode, ArpRitme, ArpOct, ArpGate, ArpSwing, ArpPatro,
        Vista,
        numParams
    };
    enum Seccio : int { SOsc, SMod, SMix, SFiltre, SEnv, SLfo, SAnal, SFx, SVeu, SArp };

    const sinte::InfoParam& info (int p);
    std::string textValor (int p, float v);

    using Preset = drive::Preset;
    using BibliotecaPresets = drive::BibliotecaPresets;
    /** Les 10 categories (Bass, Lead, Pad…). La família (Moog Bass, Juno Pads…) va al principi de la descripció. */
    const std::vector<std::string>& categories();
    const std::vector<std::string>& families();
    const std::vector<Preset>& presetsFabrica();
    std::string estatInicial();
    /** Aplica un preset escrit amb «clau=valor» (separats per espais) sobre el so inicial. */
    std::string construeix (const std::string& canvis);
    std::string aleatori (unsigned llavor, int tipus = -1);
    const std::vector<std::string>& tipusAtzar();
    std::string varia (const std::string& estat, unsigned llavor, float quant);
    float previsualitza (const std::string& estat, float* sortida, int n, int nota = 48);

    class Vintage : public sinte::Base
    {
    public:
        static constexpr int maxVeus = 16, sobre = 2;
        Vintage();
        std::string textValor (int p, float v) const override { return analogic::textValor (p, v); }
        std::string nomComplet (int p) const override;
        /** Per a la interfície: l'LFO global (-1..1) i quant està obert el filtre de la veu més nova (0..1). */
        float lfoAra() const noexcept { return lfoV.load (std::memory_order_relaxed); }
        float filtreAra() const noexcept { return filtreV.load (std::memory_order_relaxed); }

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
        struct Veu
        {
            bool viva = false, tecla = false;
            int nota = 60, ordre = 0, grup = 0;
            float vel = 1.0f, uniDet = 0.0f, uniPan = 0.0f;
            double semis = 60.0, objectiu = 60.0;
            double ph[3] {}, phSub = 0.0;
            float envA = 0, envF = 0;
            int etapaA = 0, etapaF = 0;        // 0 res, 1 atac, 2 caiguda/sosteniment, 3 alliberament, 4 apagada ràpida
            double faseLfo = 0.0;
            float shLfo = 0, shAnt = 0, temps = 0;
            float deriva[3] {}, sorollMod = 0, rosa[3] {};
            float ld[4] {}, sv1 = 0, sv2 = 0, hp = 0, hpx = 0, edatLp = 0;
            float dVa = 0;                       // per a les corbes de l'osc 3 com a modulador
        };
        /** Toleràncies fixes de cada targeta de veu (com si fossin components reals): no canvien d'una nota a l'altra. */
        struct Tolerancia { float to[3], tall, res, env, pan; };
        std::array<Tolerancia, maxVeus> tol {};
        std::vector<Veu> veus;
        int comptaOrdre = 0, comptaGrup = 0;
        int pila[32] {}; int numPila = 0;
        double darreraNota = -1.0;
        double faseG = 0.0; float shG = 0, shGAnt = 0;
        float bufL[tros * sobre] {}, bufR[tros * sobre] {};
        sinte::Delmador delL, delR;
        float dcL = 0, dcR = 0, dcxL = 0, dcxR = 0, volSuau = 1.0f, nivellVeus = 0.0f;
        // caràcter global
        double faseHum = 0.0; float calL = 0, calR = 0;
        // efectes
        std::vector<float> chL, chR; int chPos = 0; double chFase = 0.0; float chLpL = 0, chLpR = 0;
        float phAp[2][6] {}, phFb[2] {}; double phFase = 0.0;
        delay::Delay dl;
        reverb::Reverb rv;
        float darrerDl[4] { -1, -1, -1, -1 }, darrerRv[3] { -1, -1, -1 };
        std::atomic<float> lfoV { 0.0f }, filtreV { 0.0f };

        void dispara (Veu& v, int nota, float vel, bool retriga, bool glide) noexcept;
        void allibera (Veu& v) noexcept;
        void renderitzaVeu (Veu& v, int idx, int s0, int m, float lfoGlob) noexcept;
        void efectes (float* L, float* R, int n) noexcept;
        float valorLfo (double fase, float sh, float shAnt) const noexcept;
        int numUnison() const noexcept;
    };
}
