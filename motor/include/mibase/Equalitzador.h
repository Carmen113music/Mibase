#pragma once

#include "mibase/Espectre.h"
#include "mibase/Instrument.h"
#include <array>
#include <atomic>
#include <complex>
#include <string>
#include <vector>

/** 0.59: «EQ Pro (Mibase)», equalitzador paramètric intern de fins a 24 bandes.

    Filtres «ajustats a l'analògic»: els pols es posen exactament on els posaria un EQ analògic (invariància a
    l'impuls) i els zeros es calculen perquè la corba coincideixi amb l'analògica al greu, al centre i a dalt de tot.
    Així les bandes aguts no s'esclafen a prop de la meitat de la freqüència de mostreig (el defecte dels EQ digitals
    senzills). Fase mínima, sense retard.

    Normes de fils (com la Reverb i la Dinàmica): tots els ajustos són atòmics (qualsevol fil); processa() no demana
    memòria ni s'espera. El fil d'àudio treballa a trossos fixos de 16 mostres: el resultat no depèn del buffer.
    Els canvis van suavitzats (sense clics) i els canvis de tipus, pendent o canal es fan amb una fosa. */
namespace mibase::eq
{
    inline constexpr const char* idPlugin = "intern:eq";
    inline constexpr const char* nomPlugin = "EQ Pro (Mibase)";

    constexpr int maxBandes = 24;
    constexpr int maxSeccions = 8;      // 96 dB/oct = 16è ordre = 8 seccions de 2n ordre

    enum Tipus : int { Bell, LowShelf, HighShelf, HighPass, LowPass, Notch, BandPass, Tilt, AllPass, numTipus };
    enum Canal : int { Estereo, Esquerre, Dret, Mid, Side, numCanals };
    constexpr int numPendents = 8;      // 6, 12, 18, 24, 36, 48, 72, 96 dB/oct

    const char* nomTipus (int t);
    const char* nomCanal (int c);       // «Estèreo», «Esquerre»…
    const char* curtCanal (int c);      // «ST», «L», «R», «M», «S»
    int dbPerOctava (int pendent);
    int ordre (int pendent);
    /** Quins controls té cada tipus (els que no té, la finestra no els mostra). */
    bool teGuany (int tipus);
    bool teQ (int tipus, int pendent);
    bool tePendent (int tipus);

    constexpr float freqMin = 10.0f, freqMax = 30000.0f;
    constexpr float guanyMax = 30.0f;
    constexpr float qMin = 0.025f, qMax = 40.0f, qDefecte = 0.7071f;
    constexpr float sortidaMax = 24.0f;

    /** Els valors d'una banda (com els veu la interfície). */
    struct Banda
    {
        bool usada = false, activa = true;
        int tipus = Bell, pendent = 1, canal = Estereo;
        float freq = 1000.0f, guany = 0.0f, q = qDefecte;
        bool operator== (const Banda&) const = default;
    };

    /** Una secció de 2n ordre (o de 1r, amb b2 = a2 = 0). Coeficients amb a0 = 1. */
    struct Seccio { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

    /** Dissenya les seccions d'una banda per a una freqüència de mostreig. Torna quantes n'hi ha (0 = no fa res). */
    int dissenya (const Banda& b, double fs, Seccio* out);
    /** Resposta d'unes seccions a una freqüència (Hz). */
    std::complex<double> resposta (const Seccio* s, int n, double freqHz, double fs);
    /** Resposta analògica de referència d'una banda (per a les proves). */
    double magnitudAnalogica (const Banda& b, double freqHz);

    /** 0.60: el filtre amb què s'escolta una banda sola (la zona del so que toca la banda).
        Bell, Notch, Band Pass i All Pass: passa-banda a la seva freqüència i Q (0 dB al centre).
        Low Shelf i High Pass: passa-baixos (el que hi ha per sota). High Shelf i Low Pass: passa-alts. Tilt: passa-banda ample. */
    Seccio dissenyaSolo (const Banda& b, double fs);
    /** 0.60: el canal «germà» (Mid ↔ Side, L ↔ R); l'estèreo es queda igual. */
    int canalOposat (int c);

    // ---- Paràmetres (automatització): Sortida, Bypass i, per banda, Encesa, Freqüència, Guany i Q ----
    constexpr int paramsPerBanda = 4;
    enum ParamGlobal : int { PSortida, PBypass, numGlobals };
    enum ParamBanda : int { PActiva, PFreq, PGuany, PQ };
    constexpr int numParams = numGlobals + maxBandes * paramsPerBanda;
    inline constexpr int indexParam (int banda, int p) { return numGlobals + banda * paramsPerBanda + p; }

    class Equalitzador : public EfecteIntern
    {
    public:
        Equalitzador();
        void prepara (double fs, int maxBloc) override;
        void processa (float* L, float* R, int n) noexcept override;

        int numParametres() const override { return numParams; }
        InfoParametre infoParametre (int i) const override;
        void posaParametre (int i, float v01) noexcept override;
        float parametre (int i) const noexcept override;
        std::string textParametre (int i, float v01) const override;

        std::string estat() const override;
        void posaEstat (const std::string& s) override;
        void buida() noexcept override { demanaBuidar.store (true); }

        // ---- Per a la finestra (qualsevol fil) ----
        Banda banda (int k) const noexcept;
        void posaBanda (int k, const Banda& b) noexcept;
        /** Primera banda lliure (-1 si totes són usades). */
        int bandaLliure() const noexcept;
        void esborra (int k) noexcept;
        float sortida() const noexcept { return sortidaDb.load (std::memory_order_relaxed); }
        void posaSortida (float db) noexcept;
        bool bypass() const noexcept { return byp.load (std::memory_order_relaxed); }
        void posaBypass (bool b) noexcept { byp.store (b, std::memory_order_relaxed); }
        /** Comptador que puja a cada canvi (la finestra el mira per saber si s'ha de redibuixar). */
        unsigned versio() const noexcept { return canvis.load (std::memory_order_relaxed); }
        double frequenciaMostreig() const noexcept { return fsAtom.load (std::memory_order_relaxed); }
        /** Nivells de pic (lineals) d'entrada i de sortida des de l'última lectura. */
        float picEntrada() noexcept { return picIn.exchange (0.0f, std::memory_order_relaxed); }
        float picSortida() noexcept { return picOut.exchange (0.0f, std::memory_order_relaxed); }

        // ---- 0.60: analitzador i escolta d'una banda ----
        /** Amb la captura encesa, el fil d'àudio copia l'entrada i la sortida (mitjana de L i R) als anells. */
        void posaCaptura (bool b) noexcept { captura.store (b, std::memory_order_relaxed); }
        bool capturant() const noexcept { return captura.load (std::memory_order_relaxed); }
        const espectre::Anell& anellEntrada() const noexcept { return anellIn; }
        const espectre::Anell& anellSortida() const noexcept { return anellOut; }
        /** Escolta només la zona d'una banda (-1 = so normal). No es desa ni s'automatitza. */
        void posaSolo (int k) noexcept { solo.store (k >= 0 && k < maxBandes ? k : -1, std::memory_order_relaxed); toca(); }
        int soloBanda() const noexcept { return solo.load (std::memory_order_relaxed); }

    private:
        struct Atomica
        {
            std::atomic<bool> usada { false }, activa { true };
            std::atomic<int> tipus { Bell }, pendent { 1 }, canal { Estereo };
            std::atomic<float> freq { 1000.0f }, guany { 0.0f }, q { qDefecte };
        };
        std::array<Atomica, maxBandes> at;
        std::atomic<float> sortidaDb { 0.0f };
        std::atomic<bool> byp { false };
        std::atomic<bool> demanaBuidar { false };
        std::atomic<unsigned> canvis { 0 };
        std::atomic<double> fsAtom { 48000.0 };
        std::atomic<float> picIn { 0.0f }, picOut { 0.0f };
        std::atomic<bool> captura { false };
        std::atomic<int> solo { -1 };
        espectre::Anell anellIn, anellOut;

        // ---- Estat del fil d'àudio ----
        struct Viva
        {
            int tipus = Bell, pendent = 1, canal = Estereo;   // l'estructura que sona ara
            double lf = 0, g = 0, lq = 0;                     // valors suavitzats (log freq, dB, log Q)
            double pres = 0;                                  // 0..1: quant sona (fosa en encendre/apagar)
            bool viva = false, coefsBons = false, interp = false;
            int n = 0;
            Seccio s[maxSeccions], sAnt[maxSeccions];          // coeficients d'aquest tros i de l'anterior (s'hi passa mostra a mostra)
            double z[2][maxSeccions][2] {};                    // estat de cada canal (TDF-II)
        };
        std::array<Viva, maxBandes> vi;
        double fs = 48000.0;
        int dinsTros = 0;                 // mostres que queden del tros de 16
        double kSuau = 0.1, pasFosa = 0.01;
        double gSort = 1.0, wet = 1.0;    // sortida i bypass suavitzats
        bool apagatDelTot = false;
        // escolta (solo): banda que sona, quant (0..1, amb fosa) i el filtre
        int soloViu = -1;
        double soloMix = 0.0;
        Seccio soloS;
        double soloZ[2][2] {};

        static constexpr int tros = 16;
        void actualitza() noexcept;
        void tros16 (float* L, float* R, int n, int ofs) noexcept;
        void toca() noexcept { canvis.fetch_add (1, std::memory_order_relaxed); }
    };
}
