#pragma once

#include "mibase/Projecte.h"

#include <string>
#include <vector>

/** Harmonia: escales, acords, tonalitat i enllaç d'acords (sense interfície: es prova sol).
    Les notes són números MIDI (60 = C4); les classes de to (pc) van de 0 (C) a 11 (B). */
namespace mibase::harmonia
{
    // ---- Escales ----
    struct Escala
    {
        const char* id;
        const char* nom;
        std::vector<int> intervals;   // semitons des de la fonamental (0 inclòs), creixents
    };
    /** Les 13: Major, Menor natural, Menor harmònica, Menor melòdica, Dòrica, Frígia, Lídia, Mixolídia, Eòlica,
        Lòcria, Pentatònica major, Pentatònica menor, Blues. */
    const std::vector<Escala>& escales();
    int escalaPerId (const std::string& id);   // -1 si no hi és

    std::string nomTo (int pc);                 // «C», «C#», «D»… (amb bemolls a les tonalitats que en porten: «Bb»)
    std::string nomTo (int pc, const Tonalitat& t);
    std::string nomTonalitat (const Tonalitat& t);   // «A Menor natural»

    bool aEscala (int nota, const Tonalitat& t);
    /** Les classes de to de l'escala, en ordre des de la fonamental. */
    std::vector<int> notesEscala (const Tonalitat& t);
    /** La nota de l'escala més propera. Si n'hi ha dues a la mateixa distància, «direccio» (+1 / -1) tria; 0 = la de sota. */
    int ajusta (int nota, const Tonalitat& t, int direccio = 0);
    /** La nota de l'escala «passos» graus amunt (positiu) o avall. Si la nota no és de l'escala, primer s'hi ajusta. */
    int grau (int nota, const Tonalitat& t, int passos);

    // ---- Acords ----
    struct TipusAcord
    {
        const char* id;
        const char* sufix;    // «», «m», «7», «maj7», «m7b5», «sus4»…
        const char* nom;      // «Major», «Menor»…
        std::vector<int> intervals;
    };
    /** Major, menor, sus2, sus4, disminuït, augmentat, maj7, m7, 7, m7b5, 9, maj9, m9, 11, 13, add9, 6, m6. */
    const std::vector<TipusAcord>& tipusAcords();
    int tipusPerSufix (const std::string& sufix);

    enum class Voicing { Tancat = 0, Obert = 1 };

    /** Notes d'un acord. «fonamental»: nota MIDI de la fonamental en posició fonamental. «inversio»: 0 = fonamental al baix,
        1 = primera… «obert»: la segona veu de dalt baixa una octava (drop 2). «octaves» = 2: es dobla tot una octava amunt.
        «baix»: afegeix la fonamental una octava per sota. Notes ordenades i dins 0..127. */
    std::vector<int> acord (int fonamental, int tipus, int inversio = 0, Voicing v = Voicing::Tancat, int octaves = 1, bool baix = false);
    std::string nomAcord (int pcFonamental, int tipus, int pcBaix = -1);

    /** Acords de la tonalitat (un per grau, apilant terceres de l'escala). Les pentatòniques i el blues fan servir l'escala
        major o menor de la mateixa fonamental. */
    struct AcordGrau
    {
        int grau = 0;          // 0..6
        int pc = 0;            // fonamental
        int tipus = 0;
        std::string nom;       // «Am»
        std::string roma;      // «vi», «V7», «vii°» (xifra romana)
    };
    std::vector<AcordGrau> acordsDeTonalitat (const Tonalitat& t, bool setenes = false);

    /** Quin acord fan unes notes (2 o més classes de to diferents). Si no se'n reconeix cap, «trobat» és false. */
    struct Reconegut
    {
        bool trobat = false;
        int pc = 0, tipus = 0, baix = 0;
        std::string nom;       // «Am», «C/E», «G7»
    };
    Reconegut reconeix (const std::vector<int>& notes);

    // ---- Tonalitat de la cançó ----
    struct Deteccio
    {
        bool fiable = false;      // hi ha prou notes i la millor tonalitat destaca
        Tonalitat tonalitat;      // Major o Menor natural
        double confianca = 0.0;   // 0..1
        int notes = 0;
    };
    /** A partir de totes les notes MIDI del projecte (durada de cada nota), amb els perfils de Krumhansl. */
    Deteccio detecta (const Projecte& p);
    /** La que fa servir el programa: la triada a mà o, en automàtic, la detectada (si és fiable; si no, la desada). */
    Tonalitat tonalitatEfectiva (const Projecte& p);

    /** Notes MIDI que sonen a l'instant «t» (segons) a totes les pistes MIDI que sonen (clips no silenciats, dins la finestra). */
    std::vector<int> notesSonant (const Projecte& p, double t);

    // ---- Enllaç d'acords i progressions ----
    /** Reordena les notes d'un acord (mateixes classes de to, mateix nombre de notes) perquè es moguin el mínim des de
        l'«anterior». Si «anterior» és buit, el torna tal com és. */
    std::vector<int> enllaca (const std::vector<int>& anterior, const std::vector<int>& acord);

    /** Una progressió, en graus (0 = I … 6 = VII). Les «majors» es fan amb la tonalitat major de la cançó (o la relativa major,
        si la cançó és menor: el Pop a La menor fa C G Am F); les «menors», amb la menor (o la relativa menor).
        «vMajor»: a les menors, el V es fa major (com a la menor harmònica: Am G F E). */
    struct Progressio
    {
        const char* nom;
        std::vector<int> graus;
        bool menor = false;
        bool vMajor = false;
    };
    /** La major que comparteix més notes amb l'escala (a igualtat, la relativa). */
    Tonalitat relativaMajor (const Tonalitat& t);
    const std::vector<Progressio>& progressions();
    /** Acords d'una progressió per a «compassos» compassos (un acord per compàs; la progressió es repeteix o s'escurça).
        «octava»: octava de la fonamental del primer acord (4 = al voltant de C4). */
    std::vector<std::vector<int>> generaProgressio (const Tonalitat& t, int progressio, int compassos, bool setenes,
                                                    Voicing v, bool movimentMinim, int octava = 4);
    /** Els acords d'una progressió en aquesta tonalitat (per mostrar-los: «Am F C G»), una volta. */
    std::vector<AcordGrau> acordsProgressio (const Tonalitat& t, int progressio, bool setenes);

    // ---- Sobre les notes d'un clip (tot són notes MIDI normals) ----
    /** Afegeix un acord (totes les notes al mateix inici i durada). Retorna les notes noves. */
    std::vector<int> afegeixAcord (Clip& c, const std::vector<int>& notes, long long ini, long long dur, int vel);
    /** Les notes triades, agrupades per inici (cada grup és un acord): la de baix puja una octava (o la de dalt baixa). */
    void inverteix (Clip& c, std::vector<int>& idx, bool amunt);
    /** Els acords triats (agrupats per inici) es reordenen perquè es moguin el mínim d'un a l'altre. */
    void enllacaTriades (Clip& c, std::vector<int>& idx);
    /** Converteix cada nota triada en un acord amb ella de fonamental (la nota queda; s'hi afegeixen les altres).
        «deTonalitat»: el tipus el dona l'escala (Am a la de La menor…); si no, «tipus». Retorna les notes noves. */
    std::vector<int> ferAcords (Clip& c, const std::vector<int>& idx, const Tonalitat& t, int tipus, bool deTonalitat,
                                int inversio = 0, Voicing v = Voicing::Tancat);
}
