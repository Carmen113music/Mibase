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

    // ---- Acords dels pads (com l'editor d'acords: fonamental, tipus, tensions i nota greu) ----
    /** Tensions que es poden afegir (es combinen). */
    enum Tensio : unsigned
    {
        T7 = 1u << 0,       // 7a menor
        TMaj7 = 1u << 1,    // 7a major (j7)
        Tb9 = 1u << 2,
        T9 = 1u << 3,
        Ts9 = 1u << 4,
        T11 = 1u << 5,
        Ts11 = 1u << 6,     // b5 / #11
        Tb13 = 1u << 7,     // #5 / b13
        T13 = 1u << 8,      // 6 / 13 (sense 7a és la 6a)
    };
    constexpr int numTensions = 9;
    /** Nom de cada tensió (per als botons): «7», «j7», «b9»… */
    const char* nomTensio (int i);
    /** Tipus de base dels pads: 0 maj, 1 min, 2 dim, 3 sus4, 4 sus2, 5 aug. */
    constexpr int numTipusPad = 6;
    const char* nomTipusPad (int i);

    struct AcordPad
    {
        int arrel = -1;           // 0..11; -1 = pad buit
        int tipus = 0;
        unsigned tensions = 0;
        int baix = -1;            // nota greu (0..11); -1 = la fonamental
        int inversio = 0;         // 0.10: +1 = la nota de baix puja una octava (1a inversió); -1 = la de dalt baixa (-3..+3)
        bool buit() const noexcept { return arrel < 0; }
        bool operator== (const AcordPad&) const = default;
    };
    /** «Am7», «G7/b9», «C/E», «Fmaj9»… (buit: «»). */
    std::string nomPad (const AcordPad& a);
    /** Com es desa un pad: el nom i, si està invertit, «^n» al final («Am^1», «G7^-1»). */
    std::string textPad (const AcordPad& a);
    /** Quants graus de l'escala hi ha de «de» a «a» (totes dues de l'escala; negatiu si «a» és més greu). */
    int grausEntre (int de, int a, const Tonalitat& t);
    /** Mou una nota n graus de l'escala (n negatiu: avall). Si la nota no és de l'escala, primer s'hi ajusta. */
    int mouGraus (int to, const Tonalitat& t, int n);
    constexpr int maxInversio = 3;
    /** Llegeix un nom d'acord («Ebmaj7», «C#m7», «Bb/D», «G7(b9)», «F#m7b5», «Dsus4», «E9»…). Si no s'entén, un pad buit. */
    AcordPad llegeixAcord (const std::string& text);
    /** Notes del pad: fonamental a l'octava «octava» (4 = la del C4; les fonamentals de A a B, una octava avall),
        voicing tancat o obert, la inversió i la nota greu (si n'hi ha) per sota. */
    std::vector<int> notesPad (const AcordPad& a, int octava = 4, Voicing v = Voicing::Tancat);
}
