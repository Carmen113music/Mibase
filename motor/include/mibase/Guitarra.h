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
    struct Pas
    {
        Cop cop = Cop::Res;
        bool accent = false;
        bool operator== (const Pas&) const = default;
    };
    /** Un ritme de rasgueig: una fila de passos iguals (corxeres o semicorxeres) que es repeteix. */
    struct Patro
    {
        std::string id, nom;
        long long pasTicks = 480;       // 480 = corxera, 240 = semicorxera; 0 = un sol rasgueig i prou
        std::vector<Pas> passos;
        std::string dibuix;             // per al menú: «↓ · ↓ ↑ · ↑ ↓ ↑»
    };
    /** «D» avall, «U» amunt, «D>» / «U>» amb accent, «X» / «x» cop apagat (avall / amunt), «.» deixa sonar, «_» silenci. */
    std::vector<Pas> llegeixPassos (const std::string& text);
    const std::vector<Patro>& patrons();
    /** El patró amb aquest id (nullptr si no n'hi ha cap). */
    const Patro* patro (const std::string& id);

    struct Opcions
    {
        int vel = 90;               // força de base (1..127)
        double tempo = 120.0;       // per passar els mil·lisegons a ticks
        double msPerCorda = 7.0;    // retard entre corda i corda
    };
    /** Escriu el rasgueig d'un acord: «cordes» són les notes de greu a aguda. Les notes surten en ticks des de 0 i
        no passen de «durada». Avall: totes les cordes, de greu a aguda, la greu més forta. Amunt: només les 4 agudes,
        d'aguda a greu, més fluix. Cada corda sona fins que es torna a tocar, s'apaga (cop apagat o silenci) o s'acaba
        la durada. Sempre dona el mateix resultat (la petita variació de força no és a l'atzar). */
    std::vector<Nota> rasgueja (const std::vector<int>& cordes, const Patro& p, long long durada, const Opcions& o);
    /** Posa les notes al clip a partir de «ini» (ticks del clip). Retorna els índexs de les notes noves. */
    std::vector<int> afegeix (Clip& c, const std::vector<Nota>& notes, long long ini);
}
