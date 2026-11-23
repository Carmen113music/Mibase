#pragma once

#include "mibase/Drive.h"
#include "mibase/Projecte.h"
#include "mibase/SinteBase.h"
#include <string>
#include <vector>

/** 0.88: «Mibase SUB & BASS», el primer sinte del motor de síntesi nou. Només baixos.
    Per veu: 2 oscil·ladors analògics virtuals (polyBLEP; sinus, triangle, serra, quadrada, pols) amb unison, sync i FM,
    sub-oscil·lador, soroll, clic d'atac i envolupant de to; filtre (Ladder 12/24, SVF 12/24, Diode, MS-20), drive i VCA.
    Tot el camí de la veu va a 2× la freqüència de mostreig i es delma amb un filtre de mitja banda.
    Un LFO global, glide, modes Mono / Legato / Poli (8 veus), velocitat, pitch bend, roda de modulació.
    0.89: va sobre el motor comú (sinte::Base): MIDI learn, CC, pedal i l'arpegiador / seqüenciador. */
namespace mibase::subbass
{
    inline constexpr const char* idInstrument = "sinte:subbass";
    inline constexpr const char* nom = "Mibase SUB & BASS";
    inline bool esSubBass (const Pista& t) { return t.tipus == "midi" && t.instrument == idInstrument; }

    /** El tempo del projecte (el mateix per a tots els sintes). */
    inline std::atomic<double>& tempo() { return sinte::tempo(); }

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
        ArpMode, ArpRitme, ArpOct, ArpGate, ArpSwing, ArpPatro,   // 0.89
        numParams
    };

    enum Seccio : int { SOsc, SSub, SFiltre, SEnv, SLfo, SDrive, SMod, SSortida, SArp };
    using sinte::Escala;
    using sinte::Lineal; using sinte::Enter; using sinte::Log; using sinte::Cub;
    using InfoParam = sinte::InfoParam;

    const InfoParam& info (int p);
    std::vector<std::string> opcions (int p);
    std::string textValor (int p, float v);
    float normal (int p, float v) noexcept;
    float deNormal (int p, float v01) noexcept;
    inline double negresRitme (int ritme) noexcept { return sinte::negresRitme (ritme); }

    // ---- Presets ----
    using Preset = drive::Preset;
    using BibliotecaPresets = drive::BibliotecaPresets;
    const std::vector<std::string>& categories();
    const std::vector<Preset>& presetsFabrica();
    std::string estatInicial();
    std::string aleatori (unsigned llavor);
    std::string varia (const std::string& estat, unsigned llavor, float quant);
    float previsualitza (const std::string& estat, float* sortida, int n, int nota = 36);

    class SubBass : public sinte::Base
    {
    public:
        static constexpr int maxVeus = 8, maxUnison = 7, sobre = 2;
        SubBass();
        std::string textValor (int p, float v) const override { return subbass::textValor (p, v); }
        std::string nomComplet (int p) const override;

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
            double semis = 60.0, objectiu = 60.0;
            double f1[maxUnison] {}, f2[maxUnison] {}, fSub = 0.0;
            float envA = 0, envF = 0;
            int etapaA = 0, etapaF = 0;            // 0 parada, 1 atac, 2 caiguda/sostingut, 3 alliberament, 4 ràpid
            float tPEnv = 0, clic = 0;
            float soroll = 0, drvTo[2] {};
            Filtre fl[2];
        };
        Veu veus[maxVeus];
        int comptaOrdre = 0;
        int pila[32] {}; int numPila = 0;
        double fase = 0.0; float lfoVal = 0.0f, lfoSH = 0.0f;
        double darreraNota = -1.0;
        float bufL[tros * sobre] {}, bufR[tros * sobre] {};
        sinte::Delmador delL, delR;
        float dcL = 0, dcR = 0, dcxL = 0, dcxR = 0;
        float volSuau = 1.0f;

        void dispara (Veu& v, int nota, float vel, bool retriga, bool glide) noexcept;
        void allibera (Veu& v) noexcept;
        void renderitza2x (int n) noexcept;
        void renderitzaVeu (Veu& v, int s0, int m, float lfo) noexcept;
    };
}
