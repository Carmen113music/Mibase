#pragma once

#include "mibase/Loudness.h"
#include <array>
#include <string>
#include <vector>

namespace mibase::rack { class Rack; }

/** 1.00: l'ANALYZE del Mastering Studio: escolta la cançó sencera (renderitzada, sense reproduir-la) i en fa un diagnòstic.

    - `analitza`: espectre mitjà de tota la cançó en terços d'octava (Mid i Side per separat, FFT de 8192 amb finestra de
      Hann i 50 % de solapament), loudness (EBU R128), pics, correlació i amplada.
    - `diagnostica`: compara el mix amb una corba de referència (orientativa: la forma mitjana d'un pop/rock d'avui, no una
      norma) i amb el perfil de màster, i en treu troballes de tres menes: problemes, oportunitats i preferències. Cada
      troballa diu què ha vist, on, per què pot ser un problema, quina eina i quin canvi recomana (calculat a partir del que
      ha mesurat), què se n'espera, el nivell de confiança i, si s'han analitzat les pistes, quines pistes en tenen més part:
      si una sola pista en té la majoria, ho marca com a cosa de la mescla (s'arregla millor a la pista que al màster).
    - `aplica`: fa el canvi recomanat al Mastering Studio (afegeix el mòdul si no hi és). */
namespace mibase::diagnosi
{
    constexpr int numBandes = 28;   // terços d'octava de 31,5 Hz a 16 kHz
    float freqBanda (int b);

    struct Analisi
    {
        bool valid = false;
        double segons = 0.0;
        std::array<double, numBandes> mid {}, side {};   // energia total de la cançó a cada banda (unitats internes)
        loudness::Resum loud;
        float correlacio = 1.0f;     // de tota la cançó
        float sideMidDb = -100.0f;   // Side respecte del Mid, tota la cançó
        /** La corba tonal (dB per banda, Mid + Side), relativa a la mitjana de 500 Hz–2 kHz. */
        std::array<float, numBandes> corba() const;
        /** dB de l'energia d'unes bandes (Mid + Side). */
        double energia (int b0, int b1) const;
    };

    Analisi analitza (const float* L, const float* R, std::size_t n, double fs);

    /** La corba de referència (relativa a 500 Hz–2 kHz). */
    float referencia (int banda);

    enum class Mena { Problema, Oportunitat, Preferencia };
    enum class Confianca { Baixa, Mitjana, Alta };

    struct Accio
    {
        enum Tipus { Cap, EqCampana, EqGreus, EqAire, DeEsser, Mono, Amplada, Loudness };
        Tipus tipus = Cap;
        float freq = 1000.0f, guany = 0.0f, q = 0.8f;   // EQ: el canvi (dB) que se suma al que hi ha; Loudness: dB de guany del limitador
        float sostre = -1.0f;                           // Loudness: el sostre del perfil
    };

    struct Culpable { std::string pista; float percent = 0.0f; };

    struct Troballa
    {
        Mena mena = Mena::Problema;
        Confianca confianca = Confianca::Mitjana;
        std::string titol, on, perque, eina, canvi, objectiu;
        std::vector<Culpable> culpables;   // pistes amb més part en aquesta zona (si s'han analitzat)
        bool deMescla = false;             // una sola pista en té la majoria
        Accio accio;
        float gravetat = 0.0f;             // per ordenar
    };

    struct Context { float lufsObjectiu = 0.0f, truePeakObjectiu = -1.0f; std::string perfil; };

    struct Informe
    {
        Analisi mix, master;
        bool ambMaster = false;
        int pistesAnalitzades = 0;
        std::vector<Troballa> troballes;   // ordenades: problemes primer, per gravetat
        std::string enginyer;              // «què faria un enginyer de màster»
        Confianca confianca = Confianca::Mitjana;
        std::string confiancaPerque;
    };

    Informe diagnostica (const Analisi& mix, const Analisi* master, const std::vector<std::pair<std::string, Analisi>>& pistes, const Context& ctx);

    /** Aplica l'acció d'una troballa al Mastering Studio. Torna fals si no hi ha res a fer. */
    bool aplica (rack::Rack& r, const Accio& a);

    const char* nomMena (Mena m);
    const char* nomConfianca (Confianca c);
}
