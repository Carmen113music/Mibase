#pragma once

#include "mibase/Delay.h"
#include "mibase/Drive.h"
#include "mibase/Projecte.h"
#include "mibase/Reverb.h"
#include "mibase/SinteBase.h"
#include <string>
#include <vector>

/** 0.89: «Mibase LEAD», sinte per a leads, melodies, solos, hooks i riffs. Va sobre el motor comú (sinte::Base).
    Per veu, a 2× la freqüència de mostreig: 3 oscil·ladors (sinus, triangle, serra, quadrada, pols, supersaw, wavetable,
    soroll) amb «Forma» (PW, detune del supersaw, posició de la wavetable…), unison, sync (2 i 3 a l'1), FM (1→2 i 3→1),
    ring (1×2), sub, soroll, deriva analògica; filtre LP / HP / BP / Ladder de 12, 24 o 36 dB; drive (saturació,
    overdrive, distorsió, fuzz); 2 ADSR. Dos LFO assignables amb retard (vibrato que entra a poc a poc), vibrato a la
    roda i a l'aftertouch, envolupant de to, glide, Mono / Legato / Poli (8 veus), arpegiador.
    Efectes de sortida: chorus, el Delay de Mibase (sincronitzat) i la Reverb de Mibase (plate). */
namespace mibase::lead
{
    inline constexpr const char* idInstrument = "sinte:lead";
    inline constexpr const char* nom = "Mibase LEAD";
    inline bool esLead (const Pista& t) { return t.tipus == "midi" && t.instrument == idInstrument; }

    enum Param : int
    {
        O1Ona, O1Oct, O1Semi, O1Fi, O1Forma, O1Taula,
        O2Ona, O2Oct, O2Semi, O2Fi, O2Forma, O2Taula,
        O3Ona, O3Oct, O3Semi, O3Fi, O3Forma, O3Taula,
        Sync2, Sync3, Fm12, Fm31, Unison, UniDetune, UniAmple, FaseLliure, Deriva,
        Niv1, Niv2, Niv3, SubNiv, SubOct, SubOna, SorollNiv, SorollColor, Ring,
        FTipus, FPendent, FTall, FRes, FEnv, FKey, FDrive,
        AA, AD, AS, AR, FA, FD, FS, FR,
        L1Ona, L1Vel, L1Ritme, L1Desti, L1Quant, L1Retard, L1Reinicia,
        L2Ona, L2Vel, L2Ritme, L2Desti, L2Quant, L2Retard, L2Reinicia,
        DTipus, DQuant, DTo, ChMix, ChVel, ChProf, DlNota, DlFeedback, DlMix, DlPingPong, RvMida, RvMix,
        RodaVib, RodaTall, AtVib, AtTall, VelVol, VelFiltre, BendRang, PEnvQuant, PEnvTemps, VibVel,
        Mode, Glide, Volum, Ample,
        ArpMode, ArpRitme, ArpOct, ArpGate, ArpSwing, ArpPatro,
        numParams
    };
    enum Seccio : int { SOsc, SMix, SFiltre, SEnv, SLfo, SFx, SMod, SVeu, SArp };
    enum Ona : int { Sinus, Triangle, Serra, Quadrada, Pols, Supersaw, Wavetable, Soroll };

    const sinte::InfoParam& info (int p);
    std::string textValor (int p, float v);

    /** Les wavetables pròpies (fetes amb codi el primer cop): 8 taules de 8 marcs, amb nivells sense aliàsing. */
    namespace wt
    {
        constexpr int numTaules = 8, marcs = 8, nivells = 11, mida = 2048;
        const char* nomTaula (int t);
        /** Mostra (fase 0..1, posició 0..1) d'una taula, al nivell que deixa harmònics fins a la meitat de 1/dt. */
        float llegeix (int taula, double fase, float posicio, double dt) noexcept;
        void prepara();   // fa les taules (fora del fil d'àudio)
    }

    using Preset = drive::Preset;
    using BibliotecaPresets = drive::BibliotecaPresets;
    const std::vector<std::string>& categories();
    const std::vector<Preset>& presetsFabrica();
    std::string estatInicial();
    std::string aleatori (unsigned llavor);
    std::string varia (const std::string& estat, unsigned llavor, float quant);
    float previsualitza (const std::string& estat, float* sortida, int n, int nota = 60);
    /** 0.89: el sinte bàsic antic ja no hi és. Les pistes amb un dels seus sons («intern:piano»… o sense instrument)
        passen al LEAD (o al SUB & BASS, el «baix») amb un preset semblant. Torna quantes pistes ha canviat. */
    int migraSintesAntics (Projecte& p);
    std::string estatPerSoAntic (const std::string& id, std::string& instrument);

    class Lead : public sinte::Base
    {
    public:
        static constexpr int maxVeus = 8, maxUnison = 7, sobre = 2;
        Lead();
        std::string textValor (int p, float v) const override { return lead::textValor (p, v); }
        std::string nomComplet (int p) const override;
        float lfo2() const noexcept { return lfo2V.load (std::memory_order_relaxed); }

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
        struct Filtre { float a1[3] {}, a2[3] {}, s[4] {}; void buida() { for (auto& x : a1) x = 0; for (auto& x : a2) x = 0; for (auto& x : s) x = 0; } };
        struct Veu
        {
            bool viva = false, tecla = false;
            int nota = 60, ordre = 0;
            float vel = 1.0f;
            double semis = 60.0, objectiu = 60.0;
            double ph[3][maxUnison] {}, fSub = 0.0;
            float envA = 0, envF = 0, tPEnv = 0;
            int etapaA = 0, etapaF = 0;
            float soroll = 0, drvTo[2] {}, deriva[4] {}, derivaObj[4] {};
            float tDeriva = 0;
            Filtre fl[2];
        };
        Veu veus[maxVeus];
        int comptaOrdre = 0;
        int pila[32] {}; int numPila = 0;
        double darreraNota = -1.0;
        double fase1 = 0.0, fase2 = 0.0, faseVib = 0.0;
        float sh1 = 0.0f, sh2 = 0.0f, lfoV1 = 0.0f, lfoV2 = 0.0f, retard1 = 1.0f, retard2 = 1.0f, tRetard = 0.0f;
        std::atomic<float> lfo2V { 0.0f };
        float bufL[tros * sobre] {}, bufR[tros * sobre] {};
        sinte::Delmador delL, delR;
        float dcL = 0, dcR = 0, dcxL = 0, dcxR = 0, volSuau = 1.0f;
        // chorus
        std::vector<float> chL, chR; int chPos = 0; double chFase = 0.0;
        // delay i reverb de Mibase
        delay::Delay dl;
        reverb::Reverb rv;
        float darrerDl[4] { -1, -1, -1, -1 }, darrerRv[2] { -1, -1 };

        void dispara (Veu& v, int nota, float vel, bool retriga, bool glide) noexcept;
        void allibera (Veu& v) noexcept;
        void renderitzaVeu (Veu& v, int s0, int m, float l1, float l2, float vib) noexcept;
        float valorLfo (int ona, double fase, float sh) const noexcept;
        void efectes (float* L, float* R, int n) noexcept;
    };
}
