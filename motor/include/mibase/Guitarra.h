#pragma once
// 0.29: guitarra — posicions d'acords al mànec i rasgueigs escrits com a notes MIDI normals.
// No és cap instrument: genera notes que després sonen amb el so que tingui la pista (sinte, samplejador o VST3).
#include "Harmonia.h"
#include "Projecte.h"
#include <array>
#include <string>
#include <vector>

namespace mibase::guitarra
{
    constexpr int numCordes = 6;
    /** Afinació normal, de la 6a corda (greu) a la 1a: E2 A2 D3 G3 B3 E4. */
    constexpr std::array<int, numCordes> afinacio { 40, 45, 50, 55, 59, 64 };

    /** Una posició al mànec: el trast de cada corda, de la 6a a la 1a (0 = a l'aire, -1 = no sona). */
    struct Posicio
    {
        std::array<int, numCordes> trast { -1, -1, -1, -1, -1, -1 };
        bool buida() const noexcept;
        /** Les notes que sonen, de la corda més greu a la més aguda. */
        std::vector<int> notes() const;
        /** «x32010» (amb trasts de dues xifres, separats per guions: «x-10-12-12-11-10»). */
        std::string text() const;
        bool operator== (const Posicio&) const = default;
    };
    /** Llegeix «x32010» (per a les proves i la taula d'acords oberts). */
    Posicio llegeixPosicio (const std::string& text);

    /** La posició de guitarra d'un acord dels pads: els acords oberts de sempre (C, G, D, Am, Em…) i, per a la resta,
        la posició més còmoda que es troba (celleta, màxim 4 dits i 4 trasts). La inversió del pad no compta (la guitarra
        té el seu ordre); la nota greu («C/E») sí. Buida si no se'n troba cap. */
    Posicio posicio (const harmonia::AcordPad& a);
    /** Les notes de la posició, de greu a aguda (si no n'hi ha, les del pad amb voicing tancat). */
    std::vector<int> notes (const harmonia::AcordPad& a);

    // ---- Rasgueig ----
    enum class Cop { Res, Silenci, Avall, Amunt, MutAvall, MutAmunt };
    /** 0.32: com es toca el cop (a més de la direcció). */
    enum class Tecnica { Obert, Palm, Slap };           // Palm: apagat amb el palmell (so curt amb nota); Slap: cop sec percussiu
    enum class Abast { Totes, Baix, Aguts };             // (b) només la corda greu; (t) només les 4 agudes
    enum class Dit { Cap, P, P2, I, M, A, Pluck, PolzeAcord, Tap, Rasguejat };
    struct Pas
    {
        Cop cop = Cop::Res;
        bool accent = false;
        Tecnica tec = Tecnica::Obert;
        Abast abast = Abast::Totes;
        Dit dit = Dit::Cap;
        bool curt = false;       // «!»: staccato (s'apaga a un terç del pas)
        bool suau = false;       // «d» / «u» en minúscula: més fluix
        bool operator== (const Pas&) const = default;
    };
    /** Un ritme de rasgueig: una fila de passos iguals (corxeres o semicorxeres) que es repeteix. */
    struct Patro
    {
        std::string id, nom;
        long long pasTicks = 480;       // 480 = corxera, 240 = semicorxera; 0 = un sol rasgueig i prou
        std::vector<Pas> passos;
        std::string dibuix;             // per al menú: «↓ · ↓ ↑ · ↑ ↓ ↑»
        std::string grup;               // 0.32: submenú («Bàsics», «Pop, rock i indie»…)
        std::string compas = "4/4";     // 0.32: per al nom del menú
        double msCorda = 0.0;           // 0.32: retard entre cordes propi del patró (0 = el de les opcions)
        int crescendo = 0;              // 0.32: la força puja aquests punts al llarg del patró
    };
    /** Una casella per paraula, separades per espais:
        «D» avall, «U» amunt («d» / «u»: fluix), «.» deixa sonar, «_» silenci, «X» / «x» nota morta (avall / amunt).
        Davant: «P» palm mute («PD», «PU»), «X» nota morta («XD», «XU»), «S» slap («S» o «SD»).
        Darrere: «>» accent, «!» curt, «(b)» només la corda greu, «(t)» només les agudes.
        Dits: «[p]» polze (la greu), «[p2]» la segona greu, «[i]» «[m]» «[a]» les tres agudes, «[pc]» polze + acord,
        «[pl]» pluck de les tres agudes alhora, «[tap]» tapping (nota aguda), «[r]» rasgueado flamenc (4 dits). */
    std::vector<Pas> llegeixPassos (const std::string& text);
    const std::vector<Patro>& patrons();
    /** El patró amb aquest id (nullptr si no n'hi ha cap). */
    const Patro* patro (const std::string& id);

    struct Opcions
    {
        int vel = 90;               // força de base (1..127)
        double tempo = 120.0;       // per passar els mil·lisegons a ticks
        double msPerCorda = 7.0;    // retard entre corda i corda (si el patró no en porta un de propi)
        /** 0.32: variació (humanize), 0..1. Amb 0 sempre surt igual. Amb més, cada repetició canvia una mica:
            moments, força, cops fantasma de més, algun amunt que no hi és, rasgueigs que no agafen totes les cordes. */
        double variacio = 0.0;
        unsigned llavor = 1;        // la mateixa llavor dona la mateixa variació
    };
    /** Escriu el rasgueig d'un acord: «cordes» són les notes de greu a aguda. Les notes surten en ticks des de 0 i
        no passen de «durada». Avall: totes les cordes, de greu a aguda, la greu més forta. Amunt: només les 4 agudes,
        d'aguda a greu, més fluix. Cada corda sona fins que es torna a tocar, s'apaga (cop apagat o silenci) o s'acaba
        la durada. */
    std::vector<Nota> rasgueja (const std::vector<int>& cordes, const Patro& p, long long durada, const Opcions& o);
    // ---- 0.40: editor de rasgueigs, rasgueigs propis i blocs de patró ----
    /** El contrari de llegeixPassos: el text d'una fila de passos. */
    std::string textPassos (const std::vector<Pas>& v);
    /** El dibuix amb fletxes («↓ · ↓ ↑») d'una fila de passos. */
    std::string dibuixPassos (const std::vector<Pas>& v);
    inline constexpr const char* grupPropis = "Els meus rasgueigs";
    /** Els rasgueigs que ha fet l'usuari (id «propi:<nom>»). patro() també els troba. */
    const std::vector<Patro>& propis();
    void posaPropis (std::vector<Patro> v);
    Patro fesPropi (const std::string& nom, const std::string& compas, long long pasTicks, const std::vector<Pas>& passos);
    /** Per desar-los en un fitxer (JSON) i tornar-los a llegir. */
    std::string textPropis (const std::vector<Patro>& v);
    std::vector<Patro> llegeixPropis (const std::string& json);

    BlocPatro blocDe (const Patro* p, const std::string& acord, long long ini, long long dur, double variacio, unsigned llavor, int vel);
    Patro patroDeBloc (const BlocPatro& b);
    /** Torna a escriure les notes d'un bloc: treu les que comencen dins seu, talla les que hi entren sonant
        i hi posa el rasgueig de la recepta. */
    void escriuBloc (Clip& c, int quin, double tempo);
    /** Posa un bloc (i n'escriu les notes). Els blocs que trepitja s'escurcen o desapareixen. Torna'n l'índex. */
    int posaBloc (Clip& c, BlocPatro b, double tempo);
    /** Treu un bloc; amb «ambNotes», també les notes que comencen dins seu. */
    void treuBloc (Clip& c, int quin, bool ambNotes);
    /** Omple el clip amb un bloc per cada acord de la pista d'Acords que hi cau a sobre (treu els blocs i les
        notes que hi havia als trossos nous). Torna quants n'ha posat. */
    int blocsDeAcords (Projecte& p, int pista, int clip, const Patro* patro, double variacio, int vel, double tempo, unsigned llavor);

    /** Posa les notes al clip a partir de «ini» (ticks del clip). Retorna els índexs de les notes noves. */
    std::vector<int> afegeix (Clip& c, const std::vector<Nota>& notes, long long ini);
}
