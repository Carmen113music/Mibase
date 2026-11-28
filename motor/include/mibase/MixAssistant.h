#pragma once

#include "mibase/Diagnosi.h"
#include <array>
#include <string>
#include <utility>
#include <vector>

namespace mibase { struct Pista; }

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
        enum Tipus { Cap, PassaAlts, Campana, PrestatgeGreus, PrestatgeAguts, DeEsser, Compressor, MonoGreus, Revisa };
        Tipus tipus = Cap;
        float freq = 0.0f, guany = 0.0f, q = 0.7f;        // EQ (el guany en dB; el passa-alts, 12 dB/oct)
        float ratio = 0.0f, reduccioDb = 0.0f;            // compressor i de-esser
        float atacMs = 0.0f, alliberamentMs = 0.0f;
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
    };

    struct DiagnosiPista
    {
        std::string pista, nom;
        Identificacio identificacio;
        std::vector<Incidencia> incidencies;   // ordenades per gravetat
        std::string resum;                      // una frase
        std::string mesures;                    // les xifres (desplegable)
    };

    /** esGravacio: pista d'àudio (un sinte pot fer ones quadrades, que no són saturació). */
    DiagnosiPista diagnosticaPista (const std::string& idPista, const std::string& nom, const AnalisiPista& a,
                                    const Identificacio& id, const std::vector<Tram>& trams = {}, bool esGravacio = true);

    /** «1:12» */
    std::string textTemps (double segons);
    /** El nom del tram on cau un moment («Tornada»), o buit. */
    std::string tramDe (const std::vector<Tram>& trams, double segons);
}
