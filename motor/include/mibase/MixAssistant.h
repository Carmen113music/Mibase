#pragma once

#include "mibase/Diagnosi.h"
#include <array>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace mibase { struct Pista; struct Projecte; struct Insert; }

/** 1.23: el MIX ASSISTANT (fase 1). Analitza cada pista tal com sona a la mescla (renderitzada, sense reproduir-la i sense
    tocar el so original), endevina quin instrument és i en fa un diagnòstic: què hi veu, per què pot ser un problema i
    què proposa (amb freqüència, guany, Q, tipus de filtre i eina quan toca). Encara no canvia res de la mescla.

    - La part global (to per terços d'octava, loudness, true peak, correlació) és la de `diagnosi::analitza` (la de
      l'Analitza del Mastering Studio): no es duplica.
    - A sobre, una passada pròpia amb finestres curtes (FFT de 4096, salt de 2048): nivell, correlació i energia per bandes
      al llarg del temps (per a la fase 2: emmascarament), espectre fi (ressonàncies), sibilància, dinàmica i activitat.
    - Res d'això va al fil d'àudio: tot és fora de línia. */
namespace mibase::mix
{
    using diagnosi::Confianca;

    // ---------------- Instruments ----------------
    enum class Instrument { Desconegut, Veu, Cors, Bombo, Caixa, Toms, Plats, Bateria, Percussio, Baix,
                            GuitarraElectrica, GuitarraAcustica, Piano, Teclats, Pad, Sinte, Corda, Vents, Altres };
    constexpr int numInstruments = 19;
    const char* nomInstrument (Instrument i);          // «Veu principal», «Bombo»…
    const char* clauInstrument (Instrument i);         // «veu», «bombo»… (es desa al projecte)
    Instrument instrumentDeClau (const std::string& clau);

    struct Identificacio
    {
        Instrument instrument = Instrument::Desconegut;
        Confianca confianca = Confianca::Baixa;
        std::string perque;                 // «pel nom de la pista», «és un kit de bateria»…
        bool triatPerTu = false;            // en Roger ho ha dit (Pista::mixInstrument)
        /** Si no se n'està segur: cal preguntar-ho (i, si n'hi ha, el que sembla). */
        bool calPreguntar() const noexcept { return ! triatPerTu && confianca != Confianca::Alta; }
    };

    /** Pel nom (català, castellà i anglès, sense accents ni majúscules). Desconegut si no hi ha res clar. */
    Identificacio identificaPerNom (const std::string& nom);

    struct AnalisiPista;
    /** Tot junt, per ordre: el que ha triat en Roger, el tipus de pista i l'instrument que la fa sonar, el nom i (si hi és)
        el so. El so tot sol mai dona confiança alta: llavors es pregunta. */
    Identificacio identifica (const Pista& t, const AnalisiPista* a = nullptr);

    // ---------------- Anàlisi d'una pista ----------------
    struct Ressonancia
    {
        float freq = 0.0f;          // Hz
        float excesDb = 0.0f;       // per sobre de l'espectre del voltant
        float q = 4.0f;
        float persistencia = 0.0f;  // part dels moments actius en què hi és (0–1): una nota musical no hi és sempre
    };

    struct AnalisiPista
    {
        bool valid = false;
        double fs = 48000.0, segons = 0.0;
        double inici = 0.0;                       // on comença a la cançó (segons): els moments i els trams ja hi compten
        diagnosi::Analisi global;                 // la de l'Analitza del màster

        // al llarg del temps (un valor per finestra de `salt` segons)
        double salt = 0.0;
        std::vector<float> nivellDb;              // RMS de la finestra (dBFS)
        std::vector<float> correlacio;            // de la finestra
        std::vector<std::array<float, diagnosi::numBandes>> bandesDb;   // energia per banda (dB, Mid + Side)
        std::vector<char> actiu;                  // la pista hi sona

        // espectre fi mitjà dels moments actius (dB per bin, FFT de 4096)
        int midaFFT = 4096;
        std::vector<float> espectreDb;

        float parteActiva = 0.0f;                 // 0–1
        float nivellActiuDb = -100.0f;            // mitjana d'energia dels moments actius
        float picDb = -100.0f, truePeakDb = -100.0f;
        float crestaDb = 0.0f;                    // pic − nivell actiu
        float rangDinamicDb = 0.0f;               // P90 − P10 del nivell en finestres de 0,4 s (moments actius)
        float nivellFortDb = -100.0f;             // 1.25: P90 d'aquests nivells (les parts fortes), per als llindars
        float atacsPerSegon = 0.0f;               // transitoris
        float centroideHz = 0.0f;
        float greusPart = 0.0f, agutsPart = 0.0f; // part de l'energia per sota de 150 Hz i per sobre de 5 kHz
        std::vector<Ressonancia> ressonancies;    // ordenades per excés
        float sibilancia = 0.0f;                  // part dels moments actius amb esses fortes
        float freqSibilancia = 0.0f;
        std::vector<double> momentsSibilancia;    // segons (els més forts, fins a 4)
        float correlacioMin = 1.0f;               // P5 de la correlació dels moments actius
        float perduaMonoDb = 0.0f;                // quant baixa en sumar-la a mono (dB, ≥ 0)
        float sideGreusDb = -100.0f;              // Side respecte del Mid per sota de 150 Hz
        bool esMono = true;                       // L i R gairebé iguals
        int saturacions = 0;                      // trossos plans al pic (gravació saturada)
        float sorollDb = -200.0f;                 // 1.24: soroll de fons constant entre frases (dBFS; -200 = no n'hi ha)
        std::vector<double> momentsSaturacio;     // segons (fins a 4)
        std::vector<std::pair<double, double>> trams;   // on sona (segons), units si el forat és de menys d'1 s
    };

    /** inici: el segon de la cançó on comença el so (per dir els moments en temps de la cançó). */
    AnalisiPista analitzaPista (const float* L, const float* R, std::size_t n, double fs, double inici = 0.0);

    // ---------------- Diagnòstic d'una pista ----------------
    enum class Prioritat { Alta, Mitjana, Baixa };
    const char* nomPrioritat (Prioritat p);

    struct Proposta
    {
        enum Tipus { Cap, PassaAlts, Campana, PrestatgeGreus, PrestatgeAguts, DeEsser, Compressor, MonoGreus, Revisa,
                     Fader, Panorama };   // 1.24: equilibri
        Tipus tipus = Cap;
        float freq = 0.0f, guany = 0.0f, q = 0.7f;        // EQ (el guany en dB; el passa-alts, 12 dB/oct)
        float ratio = 0.0f, reduccioDb = 0.0f;            // compressor i de-esser
        float atacMs = 0.0f, alliberamentMs = 0.0f;
        float abans = 0.0f, despres = 0.0f;               // 1.24: fader (dB) o panorama (−1…+1)
        std::string eina;                                  // «EQ Pro», «Dinàmica (de-esser)»…
    };

    /** Una part del projecte (estrofa, tornada…), per dir on passa cada cosa. */
    struct Tram { std::string nom; double desDe = 0.0, finsA = 0.0; };

    struct Incidencia
    {
        std::string id;              // estable entre anàlisis: tipus:pista:zona (per al seguiment de la fase 5)
        std::string pista;           // id de la pista
        Prioritat prioritat = Prioritat::Mitjana;
        Confianca confianca = Confianca::Mitjana;
        std::string titol;           // curt i entenedor
        std::string queVeig;         // què detecta
        std::string perque;          // per què pot ser un problema
        std::string solucio;         // què proposa, en paraules
        std::string detalls;         // la part tècnica (desplegable)
        std::string on;              // «a tota la cançó», «sobretot a Tornada (1:12)»…
        Proposta proposta;
        float gravetat = 0.0f;       // per ordenar (0–1)
        // 1.24
        std::vector<std::string> altres;   // altres solucions, cadascuna amb el seu pro i contra
        std::vector<std::string> pistes;   // les pistes implicades (conflictes i equilibri); buit = només «pista»
        bool origen = false;               // cosa de la gravació (surt també en mode Creació)
        bool deMescla = false;             // parla de la mescla sencera (no d'una pista)
    };

    struct DiagnosiPista
    {
        std::string pista, nom;
        Identificacio identificacio;
        std::vector<Incidencia> incidencies;   // ordenades per gravetat
        std::string resum;                      // una frase
        std::string mesures;                    // les xifres (desplegable)
    };

    /** esGravacio: pista d'àudio (un sinte pot fer ones quadrades, que no són saturació).
        guanyDb (1.24): el que hi posa el fader (el so analitzat ja el porta); NAN si té automatització de volum. */
    DiagnosiPista diagnosticaPista (const std::string& idPista, const std::string& nom, const AnalisiPista& a,
                                    const Identificacio& id, const std::vector<Tram>& trams = {}, bool esGravacio = true,
                                    float guanyDb = 0.0f);

    // ---------------- 1.24: fase 2 — la mescla sencera ----------------
    /** Una pista per a l'anàlisi global (el so analitzat porta el fader i el panorama). */
    struct PistaMescla
    {
        std::string id, nom;
        Identificacio ident;
        const AnalisiPista* analisi = nullptr;
        double faderDb = 0.0, pan = 0.0;
        bool autoVolum = false, autoPan = false;
    };

    /** Un canvi del «Balanç proposat». */
    struct CanviBalanc
    {
        std::string pista, nom;
        double faderAbans = 0.0, fader = 0.0, panAbans = 0.0, pan = 0.0;
        bool tocaFader() const noexcept { return std::abs (fader - faderAbans) > 0.05; }
        bool tocaPan() const noexcept { return std::abs (pan - panAbans) > 0.01; }
    };

    /** Conflictes entre instruments: dues pistes que es tapen al mateix lloc i AL MATEIX MOMENT. No n'hi ha prou que
        comparteixin freqüències: cal que la que tapa hi tingui tanta energia com la tapada durant una bona part del
        temps que sonen juntes. */
    std::vector<Incidencia> conflictes (const std::vector<PistaMescla>& pistes, const std::vector<Tram>& trams = {});

    /** 1.27: com sonen aquestes pistes respecte de totes les altres quan sonen alhora (mediana, dB, amb pes de l'oïda).
        «segons»: quant de temps sonen juntes (0 = mai). */
    double nivellRelatiu (const std::vector<PistaMescla>& pistes, const std::vector<std::string>& qui, double* segons = nullptr);

    /** Balance Assistant: nivells relatius (veu i resta, bombo i baix, caixa, guitarres…) i panorames. Les xifres són
        orientatives (pop/rock). «proposta» rep el Balanç proposat (mai es posa sol). */
    std::vector<Incidencia> balanc (const std::vector<PistaMescla>& pistes, std::vector<CanviBalanc>& proposta);

    /** La mescla sencera (renderitzada sense el màster): to comparat amb la corba orientativa (amb les pistes que hi
        tenen més part), què passa quan sona tota la banda, i fase. */
    std::vector<Incidencia> diagnosticaMescla (const AnalisiPista& mix, const std::vector<PistaMescla>& pistes,
                                               const std::vector<Tram>& trams = {});

    /** «1:12» */
    std::string textTemps (double segons);
    /** El nom del tram on cau un moment («Tornada»), o buit. */
    std::string tramDe (const std::vector<Tram>& trams, double segons);

    // ---------------- 1.25: fase 3 — correccions i seguiment ----------------
    /** La correcció que es pot preparar per a una incidència: un efecte intern nou (EQ Pro, Dinàmica o Estèreo) amb els
        ajustos de la proposta, per posar al final de la cadena de la pista. Plugin buit si no se'n pot preparar res
        (cal escoltar, tornar a gravar, tocar el fader…). */
    struct Correccio
    {
        std::string pista;
        std::string plugin, nom, estat;
        std::string resum;      // què fa, en paraules («EQ Pro: −3 dB a 315 Hz»)
        bool valida() const noexcept { return ! plugin.empty(); }
    };
    /** Es pot preparar? (EQ, de-esser, compressor, mono als greus; ni fader, ni panorama, ni «revisa»). */
    bool espotPreparar (const Incidencia& i);
    /** a: l'anàlisi de la pista (per als llindars del compressor i del de-esser; pot ser nul). guanyDb: el fader
        (el so analitzat el porta i l'efecte va abans del fader); NAN si té automatització de volum. */
    Correccio preparaCorreccio (const Incidencia& i, const AnalisiPista* a = nullptr, float guanyDb = 0.0f);
    /** El sufix del nom de l'efecte mentre és una prova (encara no aplicat). */
    inline constexpr const char* sufixProva = " (prova)";

    /** Seguiment: què ha passat amb cada incidència entre dues anàlisis. Que les mesures millorin no vol dir que soni millor. */
    struct Evolucio
    {
        enum Estat { Solucionat, Millorat, Continua, Empitjorat, Nou };
        Estat estat = Continua;
        std::string id, pista, titol;
    };
    std::vector<Evolucio> compara (const std::vector<Incidencia>& abans, const std::vector<Incidencia>& ara);
    const char* nomEvolucio (Evolucio::Estat e);

    // ---------------- 1.26: un sol efecte de cada mena per pista ----------------
    /** Els efectes del Mix Assistant, sempre al final de la cadena i en aquest ordre. */
    inline constexpr const char* nomEqMix = "EQ Pro · Mix Assistant";
    inline constexpr const char* nomCompressorMix = "Compressor · Mix Assistant";
    inline constexpr const char* nomDeEsserMix = "De-esser · Mix Assistant";
    inline constexpr const char* nomEstereoMix = "Estèreo · Mix Assistant";
    /** On és una correcció: la pista, l'efecte i (a l'EQ) la banda. */
    struct LlocCorreccio
    {
        std::string pista, insert;
        int banda = -1;   // EQ: la banda; els altres, -1 (tot l'efecte)
        bool valid() const noexcept { return ! insert.empty(); }
    };
    /** Posa la correcció a la pista: l'EQ hi afegeix una banda (a l'únic «EQ Pro · Mix Assistant», que es crea si no hi és,
        respectant el que hi hagi tocat en Roger); el compressor, el de-esser i l'Estèreo creen o actualitzen el seu efecte.
        «encesa» false: la banda (o l'efecte) queda apagada, per escoltar-la abans. Lloc buit si no cap (EQ ple). */
    LlocCorreccio posaCorreccio (Projecte& p, int pista, const Correccio& c, bool encesa);
    /** −1: ja no hi és; 0: apagada; 1: encesa. */
    int estatCorreccio (const Projecte& p, const LlocCorreccio& l);
    bool encenCorreccio (Projecte& p, const LlocCorreccio& l, bool encesa);
    /** Treu la banda (o l'efecte). Si l'EQ es queda sense bandes, també es treu. */
    bool treuCorreccio (Projecte& p, const LlocCorreccio& l);
    /** Els efectes del Mix Assistant, al final i en ordre (EQ, compressor, de-esser, Estèreo); la resta no es mou. */
    void ordenaEfectesMix (Pista& t);
    /** Els projectes de la 1.25 (un EQ per correcció, «… (prova)»): els ajunta en un sol EQ per pista. Torna quants n'ha ajuntat. */
    int ajuntaEqsMix (Projecte& p);
    /** És un efecte del Mix Assistant? */
    bool esEfecteMix (const Insert& x);
}
