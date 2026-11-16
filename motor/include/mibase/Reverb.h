#pragma once

#include "mibase/Instrument.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

/** 0.55: la Reverb de Mibase (efecte intern d'insert, «intern:reverb»).

    Tot és del motor (sense JUCE ni Windows) perquè es pugui provar: els paràmetres, el so (11 algorismes, filtres,
    Decay EQ, ducking, gate, modulació, shimmer, freeze, reverse), els 100 presets de fàbrica i la biblioteca de presets
    de l'usuari (favorits i presets personals).

    Fils: els paràmetres es poden canviar des de qualsevol fil (són atòmics). prepara() fora del fil d'àudio
    (és l'única que demana memòria). processa() només des del fil d'àudio: no demana memòria ni s'espera. */
namespace mibase::reverb
{
    inline constexpr const char* idPlugin = "intern:reverb";
    inline constexpr const char* nomPlugin = "Reverb (Mibase)";

    enum Algorisme : int { Room, Hall, Chamber, Plate, Spring, Digital, Ambient, Shimmer, Gated, Reverse, Early, numAlgorismes };
    const char* nomAlgorisme (int a);
    const char* descripcioAlgorisme (int a);

    /** Paràmetres. L'ordre no es pot canviar a la lleugera: les claus de text («clau») són les que es desen. */
    enum Param : int
    {
        Alg, Mix, Decay, PreDelay, PreSync, PreNota, Size, Width, Damping, Diffusion, EarlyLate,
        LowCut, HighCut,
        EqLowF, EqLowG, Eq1F, Eq1G, Eq1Q, Eq2F, Eq2G, Eq2Q, EqHighF, EqHighG,
        DecGreus, DecAguts, CreuGreu, CreuAgut,
        DuckOn, DuckThr, DuckAtk, DuckRel, DuckAmt,
        GateOn, GateThr, GateAtk, GateHold, GateRel,
        ModMode, ModRate, ModDepth, ModSync, ModNota,
        ShimOn, ShimPitch, ShimAmt,
        FreezeOn, FreezeRel,
        RevOn, Feedback,
        Separacio, GreusMono, EntradaMode, Sortida, Qualitat,
        ModeVista,      // només de la finestra (Simple / Avançat): no toca el so
        numParams
    };

    struct InfoParam
    {
        const char* clau;    // per desar («decay»)
        const char* nom;     // per mostrar («Decay»)
        float mn, mx, def;
        bool enter;          // valors enters (selectors, interruptors)
        bool log;            // el pom es mou en escala logarítmica (freqüències, temps)
        const char* ajuda;   // text d'ajuda (tooltip)
    };
    const InfoParam& info (int p);
    /** Text del valor amb les seves unitats («2,4 s», «180 Hz», «+3,0 dB»…). */
    std::string textValor (int p, float v);
    /** Llegeix un valor escrit a mà (accepta coma o punt, i «k» per a kHz). false si no s'entén. */
    bool llegeixValor (int p, const std::string& text, float& v);

    /** Valors de nota per a la sincronització amb el tempo (pre-delay i modulació). */
    int numNotes();
    const char* nomNota (int i);
    double notaEnTemps (int i);   // en negres (1 = una negra)
    /** Transposicions del shimmer: +12, +7, +5, -12. */
    int numTransposicions();
    int transposicio (int i);

    /** Un joc complet de paràmetres (el que guarda un preset, l'A/B o el desfer). */
    struct Params
    {
        std::array<float, numParams> v {};
        Params();                                  // tots als valors per defecte
        float operator[] (int p) const noexcept { return v[(size_t) p]; }
        float& operator[] (int p) noexcept { return v[(size_t) p]; }
        void posa (int p, float x);                // amb límits
        /** «v=1;alg=1;mix=25;…» (els valors per defecte també hi són, perquè sigui estable). */
        std::string text() const;
        /** Llegeix el text (les claus que falten queden per defecte). false si no és un estat de la reverb. */
        bool llegeix (const std::string& s);
        bool igualSo (const Params& o) const;      // igual sense comptar ModeVista
        bool operator== (const Params& o) const { return v == o.v; }
    };

    /** Per a la finestra (es calcula, no escolta el so). */
    double predelaySegons (const Params& p, double bpm);
    double rateModulacio (const Params& p, double bpm);
    /** Resposta de l'EQ (filtres d'entrada i EQ de sortida), en dB, a la freqüència f. */
    double respostaEq (const Params& p, double f);
    /** Temps de decaïment (RT60, s) a la freqüència f, amb Decay EQ i damping. Infinit (1e9) amb freeze. */
    double decaimentA (const Params& p, double f);

    /** El que mesura el so (per a la visualització). Atòmics: el fil d'àudio ho escriu, la finestra ho llegeix. */
    struct Mesures
    {
        std::atomic<float> entrada { 0.0f }, sortida { 0.0f };   // pics (lineal)
        std::atomic<float> cua { 0.0f };                          // nivell RMS del so de la reverb
        std::atomic<float> correlacio { 1.0f };                   // -1 … +1 (estèreo de la cua)
        std::atomic<float> amplada { 0.0f };                      // 0 (mono) … 1 (molt ample)
        std::atomic<float> ducking { 0.0f };                      // reducció ara (dB, positiu)
        std::atomic<float> gate { 1.0f };                         // 0 tancat … 1 obert
        std::atomic<float> freeze { 0.0f };                       // 0 … 1
        std::atomic<std::uint32_t> blocs { 0 };
    };

    /** El processador. */
    class Reverb : public Efecte
    {
    public:
        Reverb();
        ~Reverb() override;

        void prepara (double fs, int maxBloc) override;
        void processa (float* L, float* R, int n) noexcept override;

        // ---- Paràmetres (qualsevol fil) ----
        void posa (int p, float v) noexcept;
        float valor (int p) const noexcept;
        Params params() const;
        void posaParams (const Params& p) noexcept;
        std::string estat() const { return params().text(); }
        void posaEstat (const std::string& s) { Params p; if (p.llegeix (s)) posaParams (p); }

        /** Demana oblidar la cua (es fa al pròxim bloc, al fil d'àudio). */
        void buida() noexcept { demanaBuidar.store (true, std::memory_order_release); }
        /** D'on llegeix el tempo (bpm) per a les sincronitzacions. nullptr = 120. */
        void posaTempo (const std::atomic<double>* t) noexcept { tempo = t; }

        const Mesures& mesures() const noexcept { return mes; }
        /** Comptador de canvis de paràmetres (la finestra el mira per saber si s'ha de redibuixar). */
        std::uint32_t versio() const noexcept { return canvis.load (std::memory_order_relaxed); }

        struct Nucli;   // el so (Reverb.cpp)

    private:
        std::array<std::atomic<float>, numParams> par;
        std::atomic<std::uint32_t> canvis { 0 };
        std::atomic<bool> demanaBuidar { false };
        const std::atomic<double>* tempo = nullptr;
        Mesures mes;
        std::unique_ptr<Nucli> nucli;
    };

    // ---- Presets ----
    struct Preset
    {
        std::string nom, categoria, descripcio;
        Params p;
    };
    /** Les categories de fàbrica, per ordre. */
    const std::vector<std::string>& categories();
    /** Els 100 presets de fàbrica. */
    const std::vector<Preset>& presetsFabrica();
    /** El preset per defecte quan es posa una reverb nova. */
    const Preset& presetInicial();

    /** Presets personals i favorits (un fitxer JSON a Documents\Mibase\Reverb). Fil de la interfície. */
    class BibliotecaPresets
    {
    public:
        std::vector<Preset> usuari;
        std::vector<std::string> favorits;     // claus: «f:Nom» (fàbrica) o «u:Nom» (usuari)

        bool carrega (const std::filesystem::path& fitxer);
        bool desa (const std::filesystem::path& fitxer) const;

        /** Desa (o substitueix si ja hi és amb el mateix nom). */
        void desaPreset (const Preset& p);
        bool esborra (const std::string& nom);
        bool reanomena (const std::string& nom, const std::string& nou);
        bool mou (const std::string& nom, const std::string& carpeta);
        const Preset* busca (const std::string& nom) const;
        std::vector<std::string> carpetes() const;   // les carpetes (categories) de l'usuari

        static std::string clauFabrica (const std::string& nom) { return "f:" + nom; }
        static std::string clauUsuari (const std::string& nom) { return "u:" + nom; }
        bool esFavorit (const std::string& clau) const;
        void commutaFavorit (const std::string& clau);
        /** Un nom que no hi sigui encara («Nom 2», «Nom 3»…). */
        std::string nomLliure (const std::string& base) const;
    };

    /** Cerca sense distingir majúscules ni accents (per al cercador de presets). */
    bool coincideix (const std::string& text, const std::string& cerca);
}
