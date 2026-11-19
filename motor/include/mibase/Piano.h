#pragma once
// 0.47: patrons de piano a dues mans (dels quatre PDF d'en Roger: catàleg, arpegis i dos volums de cançons).
// Com els rasgueigs de guitarra: un patró és només el ritme i la manera de tocar; l'acord ve de fora.
#include "mibase/Harmonia.h"
#include "mibase/Projecte.h"
#include "mibase/PartCanco.h"
#include <string>
#include <vector>

namespace mibase::piano
{
    /** Un patró. Cada mà té la seva fila de passos (poden anar amb subdivisions diferents: corxeres a l'esquerra i
        tresets a la dreta, per exemple).
        Mà dreta: C acord sencer · T les notes agudes · P el polze (la nota greu de la mà) · T8 les agudes una octava
        amunt · Cs4 / Cs2 l'acord amb la quarta o la segona en lloc de la tercera · 1 3 5 7 8 9 10… graus de l'acord.
        Mà esquerra: R fonamental · 5 quinta · 8 octava · 3 6 7 9 10 altres graus · «R+8», «R+5+8»… juntes.
        A totes dues: «.» deixa sonar el que hi havia. */
    struct Patro
    {
        std::string id, nom, grup, compas;
        std::string dreta, esquerra;
        long long pasDreta = 480, pasEsquerra = 480;
        double gate = 0.9;      // quant dura cada nota del tros que li toca
        int pedal = 1;          // 0 sense pedal · 1 un cop per compàs · 2 dos cops per compàs
        int vel = 80;
        int octavaDreta = 0;    // variacions: la mà dreta una octava amunt…
        bool esquerraPlena = false, suau = false;
    };

    const std::vector<Patro>& patrons();
    /** Per id; també les variacions («pa01~8va», «~ple», «~suau»). */
    const Patro* patro (const std::string& id);
    std::vector<Patro> variacions (const Patro& p);
    /** 0.51: la variació per a una part de la cançó (id «<id>~estrofa»…). Estrofa: suau i amb el baix espaiat ·
        Pre-tornada: el baix a cada temps, una mica més fort · Tornada: plena (acords a tots els temps, baix en octaves) ·
        Pont: de balada, al principi i a mig compàs, deixant sonar. */
    Patro variacioPart (const Patro& p, PartCanco v);
    /** Un dibuix curt del patró per a les llistes: «D: C · C · C · C ·   E: R+8 · · ·». */
    std::string dibuix (const Patro& p);
    long long ticksCompas (const std::string& compas);

    struct Opcions { int vel = 0; double tempo = 120.0; double variacio = 0.0; unsigned llavor = 1;
                     int inversio = 0;   // 0.61: inversió de la mà dreta (cíclica; 0 = posició fonamental)
    };
    /** 0.61: les notes de la mà dreta (acord tancat) amb una inversió: la més greu puja una octava a cada pas, el
        cicle torna a l'original; l'octava es tria perquè l'acord quedi a prop de l'original (sense salts grossos). */
    std::vector<int> invertida (const std::vector<int>& fonamental, int inversio);
    /** Les notes del patró amb aquest acord durant «durada» ticks (el patró es repeteix). Ticks des de zero. */
    std::vector<Nota> toca (const harmonia::AcordPad& acord, const Patro& p, long long durada, const Opcions& o = {});

    /** El bloc de patró d'un clip (la «recepta») a partir d'un patró de piano, i al revés. */
    BlocPatro blocDe (const Patro* p, long long ini, long long dur, double variacio, unsigned llavor, int vel);
    Patro patroDeBloc (const BlocPatro& b);
}
