#pragma once

#include "mibase/Dinamica.h"
#include "mibase/Instrument.h"
#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <vector>

/** 0.70: el motor de rack modular i el seu primer ús, el «Vocal Studio (Mibase)» («intern:vocalstudio»).

    Un rack és una cadena de mòduls (els efectes interns de Mibase: EQ Pro, compressor, de-esser, Decapitor,
    Drive, Doubler, Delay, Reverb, Limitador…) que sonen un rere l'altre dins d'un sol insert. Cada mòdul es pot
    apagar, té la seva barreja (Mix: en paral·lel amb el so que li arriba) i s'obre amb la seva finestra de sempre.
    Vuit macros mouen d'un cop diversos paràmetres de diversos mòduls (cada enllaç amb el seu rang, també al revés).
    El mateix motor servirà més endavant per al màster, els busos i un rack de guitarra.

    Normes de fils:
    - La cadena (quins mòduls i en quin ordre, i els enllaços dels macros) només la canvia la interfície: en fa una de
      nova (els mòduls que ja hi eren són els mateixos objectes, no es tallen les cues) i la publica; el fil d'àudio la
      fa servir des del bloc següent. La vella s'esborra a la interfície quan el fil d'àudio ja no la pot fer servir.
    - Encès, Mix, entrada, sortida i macros són atòmics: es poden moure des de qualsevol fil (també l'automatització).
    - Un mòdul amb latència (el Limitador) no fa barreja: sona sempre al 100 % (si no, sonaria desfasat). */
namespace mibase { struct Projecte; }

namespace mibase::rack
{
    /** 0.78: «Implementa a pistes reals». Els retorns del Vocal Studio de l'insert «idInsert» de la pista «pista» passen a ser
        busos d'efectes de debò (un per retorn), dins d'una carpeta «VS · <pista>»:
        - cada bus porta els efectes del retorn com a inserts (Doubler, Harmonitzador i Octava, que no poden anar sols, dins
          d'un Vocal Studio petit), el volum (Retorn) al fader i el pan;
        - la pista hi envia (post-fader) amb el nivell de l'Envia;
        - si el retorn tenia ducking, al final del bus hi ha una Dinàmica amb sidechain de la pista;
        - al Vocal Studio de la pista hi queda només la cadena principal.
        Tot porta el prefix «VS · » per poder-ho treure. */
    inline constexpr const char* prefixDesplega = "VS · ";
    struct ResultatDesplega { bool correcte = false; std::string error; int busos = 0; std::string carpeta; };
    ResultatDesplega desplega (Projecte& p, int pista, const std::string& idInsert);
    /** Els busos «VS · » que reben un enviament de la pista (el que ha creat «desplega»). */
    std::vector<int> busosDesplegats (const Projecte& p, int pista);
    /** Esborra aquests busos (i la carpeta «VS · …» si queda buida). Torna quants n'ha esborrat. */
    int treuDesplegament (Projecte& p, int pista);
    inline constexpr const char* idPlugin = "intern:vocalstudio";
    inline constexpr const char* nomPlugin = "Vocal Studio (Mibase)";

    constexpr int maxModuls = 24;     // 0.76: 24 entre la cadena principal i els retorns
    constexpr int maxRetorns = 4;     // 0.76: camins en paral·lel (reverb, delay, compressió paral·lela, dobles…)
    constexpr int numMacros = 8;
    constexpr int maxEnllacos = 64;

    /** Els mòduls que es poden afegir (el menú «Afegeix»). Alguns són el mateix efecte amb un punt de partida diferent
        (Compressor, De-esser i Porta són la Dinàmica). */
    struct TipusModul
    {
        const char* clau;        // es desa al projecte
        const char* plugin;      // l'efecte intern
        const char* nom;
        const char* descripcio;
    };
    const std::vector<TipusModul>& tipus();
    const TipusModul* busca (const std::string& clau);
    /** Crea un mòdul nou d'aquest tipus (amb el seu punt de partida). nullptr si la clau no existeix. */
    std::shared_ptr<EfecteIntern> creaModul (const std::string& clau, const std::atomic<double>* tempo);
    /** El nom curt d'un mòdul tal com està ara (la Dinàmica diu «Compressor», «De-esser»…). */
    std::string nomModul (const std::string& clau, const EfecteIntern* fx);

    struct Modul
    {
        std::string clau;
        std::shared_ptr<EfecteIntern> fx;
        std::atomic<bool> on { true };
        std::atomic<float> mix { 1.0f };   // 0..1
        // fil d'àudio
        std::vector<float> secL, secR;
        float mixAnt = 1.0f;
        bool estavaOn = true;
    };

    /** Un macro mou un paràmetre d'un mòdul: amb el macro a 0, el paràmetre val «mn»; a 1, «mx» (normalitzats 0..1;
        mn > mx = al revés). */
    struct Enllac
    {
        int macro = 0, modul = 0, param = 0;   // 0.76: modul = -1 → un paràmetre del mateix rack (enviament, retorn…)
        float mn = 0.0f, mx = 1.0f;
        bool operator== (const Enllac&) const = default;
    };

    /** 0.76: un retorn és un camí en paral·lel: rep la sortida de la cadena principal (amb el seu enviament), la passa
        pels seus mòduls i la torna a sumar (amb el seu volum, pan i ducking: baixa mentre la veu sona). */
    struct InfoRetorn { bool actiu = false; std::string nom; bool operator== (const InfoRetorn&) const = default; };

    struct Cadena
    {
        std::vector<std::shared_ptr<Modul>> moduls;
        std::vector<int> camins;                          // 0.76: per mòdul: 0 = principal, 1..4 = retorn
        std::array<InfoRetorn, maxRetorns> retorns {};    // 0.76
        std::vector<Enllac> enllacos;
    };

    enum ParamRack : int { Entrada, Sortida, Macro1,
                           Envia1 = Macro1 + numMacros,           // 0.76: dB que s'envien a cada retorn (-60 = res)
                           Retorn1 = Envia1 + maxRetorns,         // dB amb què torna cada retorn
                           PanRetorn1 = Retorn1 + maxRetorns,     // -100..100
                           Ducking1 = PanRetorn1 + maxRetorns,    // 0..24 dB que baixa mentre la veu sona
                           numParams = Ducking1 + maxRetorns };
    constexpr float minDbRetorn = -60.0f;
    /** 0.76: rang i valor per defecte dels paràmetres dels retorns (en les seves unitats). */
    void rangParam (int p, float& mn, float& mx, float& def);

    struct Mesures
    {
        std::atomic<float> entrada { 0.0f }, sortida { 0.0f };   // pics (lineal) des de l'última lectura
    };

    using Preset = dinamica::Preset;
    using BibliotecaPresets = dinamica::BibliotecaPresets;
    const std::vector<std::string>& categories();
    const std::vector<Preset>& presetsFabrica();
    /** L'estat amb què neix un Vocal Studio nou (no comença mai buit). */
    const std::string& estatInicial();

    class Rack : public EfecteIntern
    {
    public:
        Rack();
        ~Rack() override;
        void prepara (double fs, int maxBloc) override;
        void processa (float* L, float* R, int n) noexcept override;
        int latencia() const noexcept override;

        int numParametres() const override { return numParams; }
        InfoParametre infoParametre (int i) const override;
        void posaParametre (int i, float v01) noexcept override;
        float parametre (int i) const noexcept override;
        std::string textParametre (int i, float v01) const override;

        std::string estat() const override;
        void posaEstat (const std::string& s) override;
        void buida() noexcept override;

        void posaTempo (const std::atomic<double>* t) noexcept { tempo = t; }

        // ---- interfície (fil de la interfície) ----
        /** La cadena d'ara (no la guardis gaire: si es canvia, n'hi ha una de nova). */
        std::shared_ptr<const Cadena> cadena() const { return ui; }
        int numModuls() const { return (int) ui->moduls.size(); }
        Modul* modul (int i) const { return i >= 0 && i < numModuls() ? ui->moduls[(size_t) i].get() : nullptr; }
        /** Afegeix un mòdul (clau de tipus()) a la posició «pos» (-1 = al final). Torna on ha quedat (-1 si no cap). */
        int afegeix (const std::string& clau, int pos = -1, int cami = 0);
        /** Afegeix un mòdul ja fet (per als presets i les proves). */
        int afegeix (const std::string& clau, std::shared_ptr<EfecteIntern> fx, int pos = -1, int cami = 0);
        /** 0.76: a quin camí és el mòdul (0 = principal, 1..4 = retorn). */
        int cami (int i) const { return i >= 0 && i < (int) ui->camins.size() ? ui->camins[(size_t) i] : 0; }
        void posaCami (int i, int cami);
        const InfoRetorn& retorn (int k) const { static const InfoRetorn buit; return k >= 0 && k < maxRetorns ? ui->retorns[(size_t) k] : buit; }
        void activaRetorn (int k, bool actiu, const std::string& nom = {});
        /** 0.76: els paràmetres dels retorns en les seves unitats (dB, %). */
        float valor (int p) const noexcept { return p >= 0 && p < numParams ? par[(size_t) p].load (std::memory_order_relaxed) : 0.0f; }
        void posaValor (int p, float v) noexcept;
        void treu (int i);
        void mou (int de, int a);
        void posaOn (int i, bool on);
        void posaMix (int i, float mix01);
        const std::vector<Enllac>& enllacos() const { return ui->enllacos; }
        bool enllaca (const Enllac& e);
        void treuEnllac (int k);
        void posaEnllac (int k, const Enllac& e);
        /** Mou els paràmetres enllaçats al macro m com si el macro valgués el que val ara. */
        void aplicaMacro (int m) noexcept;
        std::string nomMacro (int m) const { return m >= 0 && m < numMacros ? noms[(size_t) m] : std::string(); }
        void posaNomMacro (int m, const std::string& n) { if (m >= 0 && m < numMacros) { noms[(size_t) m] = n; ++canvis; } }
        float macro (int m) const noexcept { return parametre (Macro1 + m); }
        void posaMacro (int m, float v01) noexcept { posaParametre (Macro1 + m, v01); }
        float entradaDb() const noexcept { return par[Entrada].load (std::memory_order_relaxed); }
        float sortidaDb() const noexcept { return par[Sortida].load (std::memory_order_relaxed); }
        /** Puja a cada canvi de la cadena (la finestra el mira per saber si s'ha de refer). */
        unsigned versio() const noexcept { return canvis.load (std::memory_order_relaxed); }
        Mesures& mesures() noexcept { return mes; }
        /** Esborra les cadenes velles que el fil d'àudio ja no fa servir (la finestra ho fa sovint). */
        void recull();
        /** Per a les proves: quantes cadenes velles hi ha esperant. */
        int cadenesGuardades() const { return (int) guardades.size(); }

    private:
        void publica (std::shared_ptr<Cadena> c);
        std::shared_ptr<Cadena> copia() const;
        void preparaModul (Modul& m);
        void aplica (Modul& m, float* L, float* R, int n) noexcept;
        void processaBloc (float* L, float* R, int n) noexcept;
        struct Linia   // retard per alinear els camins
        {
            std::vector<float> l, r;
            int w = 0, mascara = 0;
            void prepara (int minim);
            void passa (float* L, float* R, int n, int retard) noexcept;
            void buida() noexcept { std::fill (l.begin(), l.end(), 0.0f); std::fill (r.begin(), r.end(), 0.0f); }
        };
        Linia liniaPrincipal;
        std::array<Linia, maxRetorns> liniesRetorn;
        std::array<std::vector<float>, maxRetorns> retL, retR;
        std::array<float, maxRetorns> gEnvia {}, gRetorn {}, gDuck {};
        float envClau = 0.0f;
        void aplicaMacro (const Cadena& c, int m, float v) noexcept;

        std::array<std::atomic<float>, numParams> par;
        std::array<std::string, numMacros> noms;
        const std::atomic<double>* tempo = nullptr;
        double fs = 48000.0;
        int maxBloc = 512;
        bool preparat = false;
        std::shared_ptr<Cadena> ui;
        std::atomic<Cadena*> actual { nullptr };      // la que fa servir el fil d'àudio
        std::atomic<unsigned long long> epoca { 0 };  // puja a cada bloc del fil d'àudio
        struct Guardada { std::shared_ptr<Cadena> c; unsigned long long retirada; };
        std::vector<Guardada> guardades;
        std::atomic<unsigned> canvis { 0 };
        Mesures mes;
        float gIn = 1.0f, gOut = 1.0f;   // fil d'àudio (suavitzats)
        bool primerBloc = true;
    };
}
