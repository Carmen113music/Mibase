#pragma once

#include "mibase/Drive.h"
#include "mibase/Instrument.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/** 1.5: «Guitar Rack (Mibase)», l'estudi de guitarra elèctrica: una pedalera (pedals abans de l'ampli i, si vols, al loop
    d'efectes, després de la pantalla), un amplificador, una pantalla amb dos micròfons i una mica de sala.

    Tot processa so de debò, amb DSP propi:
    - Amplificadors: etapes de preamplificador amb vàlvula asimètrica a ×4, pila de tons passiva (model de components
      Fender / Marshall / Vox / Mesa…), etapa de potència amb «sag», presence i resonance.
    - Pantalla: model de la caixa (ressonància, obert o tancat), de l'altaveu, del micròfon i de la posició, distància i angle.
      Sense IR ni mostres de ningú.
    - Pedals: 12 motors (drive, compressor, chorus, phaser, flanger, trèmolo, delay, eco de cinta, reverb, wah i filtres,
      octavadors i pitch, utilitats) amb variants per a cada model. Els delays i les reverbs fan servir el Delay i la
      Reverb de Mibase per dins.

    Normes de fils: com la resta d'efectes interns. Els canvis d'estructura (pedals, ampli, pantalla, micròfons) es preparen
    al fil de la interfície i el fil d'àudio hi passa amb una fosa curta (sense clics). Els poms es suavitzen.
    Els noms dels models són de Mibase; «inspirat» diu en quin equip real es basa cadascun. */
namespace mibase::guitarrack
{
    inline constexpr const char* idPlugin = "intern:guitarrack";
    inline constexpr const char* nomPlugin = "Guitar Rack (Mibase)";

    constexpr int maxControls = 8;
    constexpr int maxPedals = 12;
    constexpr int maxCanals = 4;

    // ======================================================================================== catàleg
    enum Categoria : int { CatDrive, CatComp, CatMod, CatDelay, CatReverb, CatWah, CatAltres, numCategories };
    const char* nomCategoria (int c);

    enum class TipusControl : int { Pom, Interruptor, Selector, Pedal };
    struct Control
    {
        std::string clau, nom;
        float mn = 0.0f, mx = 10.0f, def = 5.0f;
        TipusControl tipus = TipusControl::Pom;
        std::vector<std::string> opcions;   // selector / interruptor
        std::string unitat;                 // "ms", "dB", "%"…
        bool log = false;
        std::string ajuda;
    };

    /** La forma del pedal (com es dibuixa). */
    enum class Forma : int { Compacte, Petit, Gran, Basculant, Ample };

    enum Motor : int { MDrive, MComp, MChorus, MPhaser, MFlanger, MTremolo, MDelay, MCinta, MReverb, MWah, MPitch, MUtil, numMotors };

    struct ModelPedal
    {
        std::string clau, nom, inspirat, marca;
        int categoria = CatDrive;
        int motor = MDrive, variant = 0;
        std::string caracter, estils, descripcio;
        std::uint32_t color = 0xff808080, colorText = 0xff000000, colorPom = 0xff151515;
        Forma forma = Forma::Compacte;
        std::vector<Control> controls;   // fins a maxControls
    };
    const std::vector<ModelPedal>& pedals();
    int indexPedal (const std::string& clau);

    /** Els poms de l'ampli (sempre els mateixos índexs; cada model diu quins té i com es diuen). */
    enum AmpCtl : int { AGain, ABass, AMiddle, ATreble, APresence, AMaster, AReverb, AExtra, numAmpCtl };
    enum class Extra : int { Cap, Resonance, Depth, GraficV, Cut };
    enum class Interruptor : int { Cap, Bright, Boost, Tight, ToneShift };
    enum Pila : int { PFender, PMarshall, PVox, PMesa, PHiwatt, PModern, POrange, numPiles };

    /** Com sona un canal de l'ampli. */
    struct Veu
    {
        float guanyMin = 0.0f, guanyMax = 30.0f;   // dB de preamplificador amb Gain a 0 i a 10
        int etapes = 2;                            // 1..4
        float biaix = 0.25f;                       // asimetria (harmònics parells)
        float duresa = 1.0f;                       // 1 = suau … 3 = dur
        float hpEntrada = 40.0f, hpEtapa = 20.0f, lpEtapa = 9000.0f;
        float brillant = 0.0f;                     // dB d'aguts que passen sense el pom de guany (bright cap)
        float midF = 800.0f, midDb = 0.0f;         // color del canal abans de la pila
        int posPila = 1;                           // després de quina etapa va la pila (etapes = al final)
        float potencia = 0.0f;                     // dB extra a l'etapa de potència amb el Master a 10
        float sag = 0.3f;
        float hpSortida = 60.0f, lpSortida = 9000.0f;
        float nivell = 0.0f;                       // dB per igualar el volum entre models
    };

    struct ModelAmp
    {
        std::string clau, nom, inspirat, marca, origen, caracter, estils, descripcio;
        std::array<std::string, numAmpCtl> etiqueta;   // "" = no hi és
        std::array<float, numAmpCtl> def {};
        std::vector<std::string> canals;              // buit = un sol canal
        std::vector<Veu> veus;                        // una per canal
        int pila = PMarshall;
        bool teMaster = true;                         // sense master: el Gain (Volume) també apreta l'etapa de potència
        Extra extra = Extra::Cap;
        Interruptor interruptor = Interruptor::Cap;
        std::string nomInterruptor;
        std::string cab, altaveu;                     // la pantalla i l'altaveu que hi van bé
        std::uint32_t tolex = 0xff111111, panell = 0xffc8a24a, textPanell = 0xff111111, reixa = 0xff2a2620;
        int estilPom = 0;                             // 0 tapa daurada, 1 faldilla amb números, 2 cap de pollastre, 3 cromat, 4 negre llis
        int canalDef = 0;
    };
    const std::vector<ModelAmp>& amplis();
    int indexAmpli (const std::string& clau);

    struct ModelCab
    {
        std::string clau, nom, inspirat;
        int altaveus = 4;
        bool obert = false;
        float resF = 100.0f, resDb = 4.0f, resQ = 1.4f, hp = 70.0f, cosDb = 0.0f;   // cosDb: greus-mitjos de la fusta
        std::string altaveu;
        std::uint32_t tolex = 0xff111111, reixa = 0xff2a2620;
    };
    const std::vector<ModelCab>& cabs();
    int indexCab (const std::string& clau);

    struct ModelAltaveu
    {
        std::string clau, nom, inspirat, caracter;
        float lp = 5500.0f, pic1F = 2500.0f, pic1Db = 4.0f, pic1Q = 1.2f, pic2F = 1000.0f, pic2Db = 0.0f, pic2Q = 1.0f, greusDb = 0.0f;
    };
    const std::vector<ModelAltaveu>& altaveus();
    int indexAltaveu (const std::string& clau);

    struct ModelMicro
    {
        std::string clau, nom, inspirat, caracter;
        float proximitat = 3.0f, presF = 6000.0f, presDb = 4.0f, presQ = 1.2f, agutsDb = 0.0f, lp = 15000.0f, hp = 50.0f;
    };
    const std::vector<ModelMicro>& micros();

    /** Amb què es pot buscar (nom, marca, tipus, caràcter i estil), en minúscules i sense accents. */
    std::string textCerca (const ModelPedal& p);
    std::string textCerca (const ModelAmp& a);
    std::string senseAccents (const std::string& s);

    // ======================================================================================== paràmetres
    enum Param : int
    {
        Entrada, Sortida, Gate,
        AmpOn, Canal, BrightP,
        AmpGain, AmpBass, AmpMiddle, AmpTreble, AmpPresence, AmpMaster, AmpReverb, AmpExtra,
        CabOn, MicA, MicB, PosA, PosB, DistA, DistB, AngleA, AngleB, MesclaB, Room,
        numFixos
    };
    /** Paràmetres dels pedals per posició a la pedalera: [On, control 0 … control 7]. */
    constexpr int paramsPerPedal = 1 + maxControls;
    constexpr int numParams = numFixos + maxPedals * paramsPerPedal;
    inline constexpr int paramPedal (int k, int c) { return numFixos + k * paramsPerPedal + 1 + c; }
    inline constexpr int paramOnPedal (int k) { return numFixos + k * paramsPerPedal; }

    struct InfoFix { const char* clau; const char* nom; float mn, mx, def; int passos; const char* ajuda; };
    const InfoFix& infoFix (int p);

    // ======================================================================================== un pedal
    class Pedal
    {
    public:
        explicit Pedal (int model);
        virtual ~Pedal() = default;
        const ModelPedal& model() const { return pedals()[(size_t) idx]; }
        int indexModel() const noexcept { return idx; }
        int numControls() const { return (int) model().controls.size(); }

        std::atomic<bool> on { true };
        std::atomic<bool> loop { false };   // al loop d'efectes (després de la pantalla)
        void posa (int c, float v) noexcept;
        float valor (int c) const noexcept { return c >= 0 && c < maxControls ? val[(size_t) c].load (std::memory_order_relaxed) : 0.0f; }
        static float normal (const Control& c, float v) noexcept;
        static float deNormal (const Control& c, float v01) noexcept;
        std::string text (int c, float v) const;

        void prepara (double fs, int maxBloc);
        /** Fil d'àudio. R pot ser nul (mono, abans de l'ampli). Fa la fosa de l'interruptor sense clics. */
        void passa (float* L, float* R, int n) noexcept;
        virtual void buida() noexcept {}
        /** D'on llegeix el tempo (els delays amb sync). */
        virtual void posaTempo (const std::atomic<double>*) {}
        /** 0..1: per al llum d'alguns pedals (compressor, gate, wah automàtic…). */
        float activitat() const noexcept { return act.load (std::memory_order_relaxed); }

    protected:
        virtual void preparaMotor (double fs, int maxBloc) = 0;
        virtual void processa (float* L, float* R, int n) noexcept = 0;
        double fs = 48000.0;
        std::atomic<float> act { 0.0f };
        int idx = 0;
    private:
        std::array<std::atomic<float>, maxControls> val;
        std::vector<float> secL, secR;
        float gOn = 1.0f;
        bool estavaOn = true, calBuidar = false;
    };
    std::shared_ptr<Pedal> creaPedal (const std::string& clau);

    struct Mesures
    {
        std::atomic<float> entrada { 0.0f }, sortida { 0.0f };   // pics (lineal) des de l'última lectura
        std::atomic<float> gate { 1.0f };
        std::atomic<float> ampli { 0.0f };                        // pic a la sortida de l'ampli (per al llum pilot)
    };

    struct Placa
    {
        std::vector<std::shared_ptr<Pedal>> pedals;   // en ordre: primer els d'abans de l'ampli, després els del loop
        std::vector<char> loop;                       // per pedal: 1 = al loop (el fil d'àudio llegeix això, no Pedal::loop)
    };

    // ======================================================================================== el rack
    class GuitarRack : public EfecteIntern
    {
    public:
        GuitarRack();
        ~GuitarRack() override;

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
        void posaTempo (const std::atomic<double>* t);

        // ---- valors fixos (en les seves unitats) ----
        float valor (int p) const noexcept { return p >= 0 && p < numFixos ? fix[(size_t) p].load (std::memory_order_relaxed) : 0.0f; }
        void posa (int p, float v) noexcept;

        // ---- estructura (fil de la interfície) ----
        int ampli() const noexcept { return iAmp.load(); }
        /** Canvia d'ampli; els poms agafen els valors de sempre del model (i el canal per defecte). */
        void posaAmpli (int i, bool ambPoms = true);
        int cab() const noexcept { return iCab.load(); }
        void posaCab (int i, bool ambAltaveu = true);
        int altaveu() const noexcept { return iAlt.load(); }
        void posaAltaveu (int i);

        int numPedals() const { return (int) ui->pedals.size(); }
        Pedal* pedal (int k) const { return k >= 0 && k < numPedals() ? ui->pedals[(size_t) k].get() : nullptr; }
        std::shared_ptr<const Placa> placa() const { return ui; }
        /** Afegeix un pedal (clau del catàleg) a la posició «pos» (-1 = al final dels d'abans de l'ampli). -1 si no cap. */
        int afegeix (const std::string& clau, int pos = -1, bool aLoop = false);
        void treu (int k);
        /** Mou el pedal «de» a la posició «a» (i, si cal, el passa al loop o a davant de l'ampli). */
        void mou (int de, int a, bool aLoop);
        void posaLoop (int k, bool aLoop);
        void buidaPlaca();

        const std::string& nomPreset() const noexcept { return presetNom; }
        bool presetModificat() const noexcept { return presetMod; }
        void posaNomPreset (const std::string& n, bool modificat = false) { presetNom = n; presetMod = modificat; ++canvis; }
        void marcaModificat() noexcept { if (! presetNom.empty() && ! presetMod) { presetMod = true; ++canvis; } }
        unsigned versio() const noexcept { return canvis.load (std::memory_order_relaxed); }
        Mesures& mesures() noexcept { return mes; }
        /** Allibera les plaques velles que el fil d'àudio ja no fa servir (la finestra ho fa sovint). */
        void recull();
        int plaquesGuardades() const { return (int) guardades.size(); }

    private:
        struct Imp;
        std::unique_ptr<Imp> imp;
        std::array<std::atomic<float>, numFixos> fix;
        std::atomic<int> iAmp { 0 }, iCab { 0 }, iAlt { 0 };
        std::shared_ptr<Placa> ui;
        std::atomic<Placa*> actual { nullptr };
        std::atomic<Placa*> usA { nullptr };
        std::array<std::atomic<Pedal*>, maxPedals> slot {};
        std::atomic<unsigned long long> epoca { 0 };
        struct Guardada { std::shared_ptr<Placa> p; unsigned long long retirada; };
        std::vector<Guardada> guardades;
        void publica (std::shared_ptr<Placa> p);
        void processaBloc (float* L, float* R, int n) noexcept;
        std::shared_ptr<Placa> copia() const { return std::make_shared<Placa> (*ui); }
        std::atomic<bool> demanaBuidar { false };
        std::atomic<unsigned> canvis { 0 };
        const std::atomic<double>* tempo = nullptr;
        std::string presetNom;
        bool presetMod = false;
        Mesures mes;
        double fs = 48000.0;
        int maxBloc = 512;
        bool preparat = false;
    };

    // ======================================================================================== presets
    using Preset = drive::Preset;
    using BibliotecaPresets = drive::BibliotecaPresets;
    const std::vector<std::string>& categories();
    const std::vector<Preset>& presetsFabrica();
    /** Construeix un estat a partir d'una descripció curta (la dels presets de fàbrica):
        «amp=plexi gain=6 … | ts808 drive=2 tone=6 | ce2 loop rate=3» (vegeu GuitarRackPresets.cpp). */
    std::string construeix (const std::string& descripcio);
    /** L'estat amb què neix un Guitar Rack nou (un so net amb una mica de crunch a punt). */
    const std::string& estatInicial();
}
