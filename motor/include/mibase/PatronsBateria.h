#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>
#include <mibase/Projecte.h>

/** 0.50: la biblioteca de patrons de bateria (150 ritmes de 4 compassos, de 16 caselles per compàs), per capes.
    De cada patró se'n pot posar tot o només unes capes (bombo, caixa, hihat, plats, toms), i en surten variacions
    per a les parts de la cançó (estrofa, pre-tornada, tornada, pont, sense tall). Les velocitats no són fixes:
    es calculen com les tocaria un bateria (temps forts, notes fantasma, dobles de bombo, redobles que creixen). */
namespace mibase::bateria
{
    /** Les capes que es poden triar (bits). */
    enum Capa : unsigned
    {
        CapaBombo = 1,
        CapaCaixa = 2,
        CapaHihat = 4,
        CapaPlats = 8,     // ride i crash
        CapaToms = 16,
        CapaTot = 31
    };
    constexpr int numCapes = 5;
    const char* nomCapa (int k);    // 0..4: «Bombo», «Caixa», «Hihat», «Plats», «Toms»

    /** Les pistes d'un patró, sempre per aquest ordre. */
    enum PistaPatro { PHH = 0, PRD, PCR, PSN, PKK, PT1, PT2, PFT, numPistesPatro };
    constexpr int casellesPatro = 16, compassosPatro = 4, llargPatro = casellesPatro * compassosPatro;

    struct PatroBat
    {
        int num;                 // 1..150
        const char* nom;
        const char* ref;         // cançó o estil de referència
        const char* grup;
        const char* ajuda;       // anotació i ús
        std::vector<std::pair<const char*, const char*>> files;   // «HH» → 64 caselles
    };
    const std::vector<PatroBat>& patronsBat();
    const PatroBat* patroBat (int num);

    enum class Variacio { Original = 0, Estrofa, PreTornada, Tornada, Pont, SenseTall };
    constexpr int numVariacions = 6;
    const char* nomVariacio (Variacio v);
    const char* ajudaVariacio (Variacio v);

    /** El patró en una graella: una tira de 64 caselles per pista («-» on no hi ha res). */
    struct GraellaPatro
    {
        std::array<std::string, numPistesPatro> p;
        bool operator== (const GraellaPatro&) const = default;
    };
    GraellaPatro graella (const PatroBat& p, Variacio v = Variacio::Original);
    /** Quines capes tenen algun cop. */
    unsigned capesDe (const GraellaPatro& g);
    /** La capa d'una pista del patró (PHH → CapaHihat…). */
    unsigned capaDePista (int pista);
    /** La capa d'un tipus de fila («kick» → CapaBombo…); 0 si no és de cap (percussió, altres). */
    unsigned capaDeTipus (const std::string& tipus);

    /** Velocitat (1..127) del cop de la casella «pas» (0..63) de la pista; 0 si no n'hi ha.
        Sempre igual per al mateix patró i la mateixa variació. */
    int velocitatPatro (const PatroBat& p, const GraellaPatro& g, Variacio v, int pista, int pas);

    /** Notes del patró per a «compassos» compassos a partir d'«iniTicks» (ticks del clip); es repeteix cada 4 compassos.
        Només les capes triades. Les notes són les de les files de la pista (si no té ride, va al hihat; sense crash, al ride). */
    std::vector<Nota> notesDePatro (const PatroBat& p, Variacio v, unsigned capes, int compassos, long long ticksCompas,
                                    const std::vector<FilaBateria>& files, long long iniTicks);
    /** Posa el patró al clip dins [a, fi): treu els cops que hi havia de les capes triades (els de les altres es queden)
        i hi posa els del patró. Torna quantes notes ha posat. */
    int posaPatro (Clip& c, const PatroBat& p, Variacio v, unsigned capes, const std::vector<FilaBateria>& files,
                   long long ticksCompas, long long a, long long fi);
}
