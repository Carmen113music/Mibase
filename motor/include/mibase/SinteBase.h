#pragma once

#include "mibase/Instrument.h"
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

/** 0.89: el motor comú dels sintes de Mibase (SUB & BASS, LEAD i els que vindran). Hi ha el que tots fan igual:
    la taula de paràmetres (valors reals, normalitzats, text), desar i llegir l'estat, MIDI learn i CC, pedal,
    roda, pitch bend i aftertouch, l'arpegiador / seqüenciador sincronitzat amb el tempo, l'oscil·loscopi, el nivell
    i la càrrega de CPU. Cada sinte només fa les veus (notaOn, notaOff, renderitza). Sense JUCE. */
namespace mibase::sinte
{
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
    float normal (const InfoParam& i, float v) noexcept;      // valor real → 0..1
    float deNormal (const InfoParam& i, float x) noexcept;    // 0..1 → valor real (arrodonit si és enter)
    std::vector<std::string> opcions (const InfoParam& i);
    std::string num (double v, int dec);                      // amb coma decimal
    std::string ms (double v);                                // «12 ms», «1,20 s»

    /** El tempo del projecte (el posa la interfície): LFO, delay i arpegiador. */
    std::atomic<double>& tempo();
    /** Durades de l'LFO sincronitzat, en negres (índex 1.. de «Lliure|1/1|1/2|1/4|1/8|1/16|1/32|1/4T|1/8T|1/16T|1/4.|1/8.»; 0 = lliure). */
    double negresRitme (int ritme) noexcept;
    inline constexpr const char* ritmesLfo = "Lliure|1/1|1/2|1/4|1/8|1/16|1/32|1/4T|1/8T|1/16T|1/4.|1/8.";

    // ---- arpegiador ----
    inline constexpr const char* modesArp = "Off|Amunt|Avall|Amunt-avall|Atzar|Ordre|Acord|Patró";
    inline constexpr const char* ritmesArp = "1/4|1/8|1/8T|1/16|1/16T|1/32";
    inline constexpr const char* nomsPatrons = "Octaves|Arrel i quinta|Berlinesa|Galop|Àcid 1|Àcid 2|Trance|Synthwave|Arpegi menor|Arpegi major|Pulsació|Funk|Techno|Escala|Hook|Repeteix";
    double negresArp (int ritme) noexcept;
    struct PasPatro { int to = 0; bool silenci = false, accent = false, lligat = false; };
    struct Patro { const char* nom; std::vector<PasPatro> passos; };
    /** Els patrons del seqüenciador (to relatiu a la nota més greu premuda). */
    const std::vector<Patro>& patrons();
    std::string opcionsPatrons();   // «Octaves|Arrel i quinta|…»
    /** On són els paràmetres de l'arpegiador a la taula del sinte (-1: no en té). */
    struct IndexArp { int mode = -1, ritme = -1, octaves = -1, gate = -1, swing = -1, patro = -1; };

    // ---- ajudes de so ----
    inline float tanhR (float x) noexcept
    {
        if (x > 3.0f) return 1.0f;
        if (x < -3.0f) return -1.0f;
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }
    inline float blep (double t, double dt) noexcept
    {
        if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
        return 0.0f;
    }
    inline float coef (double temps, double fs) noexcept { return temps <= 0.0 ? 1.0f : (float) (1.0 - std::exp (-1.0 / (temps * fs))); }

    /** Delmador 2× → 1× (FIR de mitja banda, 47 coeficients). */
    class Delmador
    {
    public:
        static constexpr int taps = 47;
        void buida() noexcept { for (auto& h : hist) h = 0.0f; }
        /** «entrada» té 2n mostres; «sortida», n. n ≤ 512. */
        void processa (const float* entrada, float* sortida, int n) noexcept;
    private:
        float hist[taps] {};
    };

    //==========================================================================
    class Base : public Instrument
    {
    public:
        static constexpr int midaOna = 4096, tros = 256;
        Base (const InfoParam* taula, int numParams, std::string prefix, IndexArp arp);

        // ---- paràmetres (Parametritzable) ----
        int numParametres() const override { return numP; }
        InfoParametre infoParametre (int i) const override;
        void posaParametre (int i, float v01) noexcept override;
        float parametre (int i) const noexcept override;
        std::string textParametre (int i, float v01) const override;
        /** Text d'un valor real (cada sinte pot afinar-lo). */
        virtual std::string textValor (int p, float v) const;
        /** Nom amb la secció, per a l'automatització («Filtre · Tall»). */
        virtual std::string nomComplet (int p) const;
        const InfoParam& info (int p) const noexcept { return taula[p < 0 ? 0 : p >= numP ? numP - 1 : p]; }
        void posa (int p, float v) noexcept;
        float valor (int p) const noexcept { return par[(size_t) p].load (std::memory_order_relaxed); }

        /** Estat en text (paràmetres, CC i nom del preset). posaEstat llegeix tot abans de posar-ho (el fil d'àudio no
            veu mai valors a mitges); un estat sense «cc=» (un preset) no toca les assignacions. */
        std::string estatSo() const;
        std::string estat() const;
        void posaEstat (const std::string& s);
        bool esEstatMeu (const std::string& s) const { return s.rfind (prefix + "=", 0) == 0; }
        /** El nom del preset d'on ve el so (només fil de la interfície). */
        std::string nomPreset;

        // ---- MIDI learn i CC ----
        int paramDeCC (int cc) const noexcept { return cc >= 0 && cc < 128 ? ccMapa[(size_t) cc].load (std::memory_order_relaxed) : -1; }
        void assignaCC (int cc, int param) noexcept;
        void treuCC (int param) noexcept;
        int ccDeParam (int param) const noexcept;
        void aprenCC (int param) noexcept { aprenent.store (param); }
        int aprenentCC() const noexcept { return aprenent.load(); }
        static bool ccReservat (int cc) noexcept { return cc == 1 || cc == 64 || cc >= 120; }

        // ---- instrument ----
        void prepara (double fs, int maxBloc) override;
        void processa (const EventInstrument* events, int numEvents, float* L, float* R, int n) noexcept override final;
        void apagaTot() noexcept override;
        bool fentSo() const noexcept override;

        // ---- per a la interfície ----
        float carregaCpu() const noexcept { return cpu.load (std::memory_order_relaxed); }
        float roda() const noexcept { return rodaV.load (std::memory_order_relaxed); }
        float bend() const noexcept { return bendV.load (std::memory_order_relaxed); }
        float pressio() const noexcept { return pressioV.load (std::memory_order_relaxed); }
        bool notaSona (int n) const noexcept { return n >= 0 && n < 128 && (notesOn[(size_t) (n >> 6)].load (std::memory_order_relaxed) >> (n & 63) & 1u) != 0; }
        float hzDarrera() const noexcept { return hzUltima.load (std::memory_order_relaxed); }
        float nivell() const noexcept { return nivellV.load (std::memory_order_relaxed); }
        float lfo() const noexcept { return lfoV.load (std::memory_order_relaxed); }
        double frequenciaMostreig() const noexcept { return fs; }
        int veusActives() const noexcept { return actives.load (std::memory_order_relaxed); }
        int pasArp() const noexcept { return pasArpV.load (std::memory_order_relaxed); }   // -1: aturat
        void copiaOna (float* desti, int n) const noexcept;

    protected:
        // ---- el que fa cada sinte ----
        virtual void preparaSo (double fs) = 0;
        virtual void notaOn (int nota, int vel) noexcept = 0;
        virtual void notaOff (int nota) noexcept = 0;
        virtual void apagaVeus() noexcept = 0;
        virtual bool veusSonen() const noexcept = 0;
        virtual int numVeusActives() const noexcept = 0;
        /** Escriu n mostres (n ≤ tros) a L i R. */
        virtual void renderitza (float* L, float* R, int n) noexcept = 0;

        double fs = 48000.0;
        float rodaMod = 0.0f, bendCru = 0.0f, pressioAra = 0.0f;   // 0..1, -1..1, 0..1
        void posaHz (float hz) noexcept { hzUltima.store (hz, std::memory_order_relaxed); }
        void posaLfo (float v) noexcept { lfoV.store (v, std::memory_order_relaxed); }
        float aleat() noexcept { rnd ^= rnd << 13; rnd ^= rnd >> 17; rnd ^= rnd << 5; return (float) (rnd & 0xffffff) / 8388608.0f - 1.0f; }
        /** Notes premudes ara (les reals, no les de l'arpegiador). */
        int numTecles() const noexcept { return nHeld; }

    private:
        const InfoParam* taula;
        int numP;
        std::string prefix;
        IndexArp ia;
        std::vector<std::atomic<float>> par;
        std::array<std::atomic<int>, 128> ccMapa;
        std::atomic<int> aprenent { -1 };
        std::atomic<float> lfoV { 0.0f }, cpu { 0.0f }, rodaV { 0.0f }, bendV { 0.0f }, pressioV { 0.0f }, hzUltima { 55.0f }, nivellV { 0.0f };
        std::atomic<int> actives { 0 }, pasArpV { -1 };
        std::array<std::atomic<std::uint64_t>, 2> notesOn;
        std::vector<std::atomic<float>> ona;
        std::atomic<int> posOna { 0 };
        std::uint32_t rnd = 22222u;
        bool pedal = false, sostingudes[128] {};

        // arpegiador
        struct Tecla { int nota, vel, ordre; };
        Tecla held[32] {}; int nHeld = 0, comptaOrdre = 0;
        bool arpEnMarxa = false;
        int pas = 0, sonants[32] {}, nSonants = 0, velArp = 100;
        double finsPas = 0.0, finsOff = -1.0;

        void entraNota (int nota, int vel) noexcept;
        void surtNota (int nota) noexcept;
        void controla (int cc, int val) noexcept;
        void marcaNota (int n, bool on) noexcept;
        bool arpActiu() const noexcept { return ia.mode >= 0 && valor (ia.mode) > 0.5f; }
        void arpAtura() noexcept;
        void arpPas() noexcept;
        void arpOffs() noexcept;
        double mostresPas() const noexcept;
    };
}
