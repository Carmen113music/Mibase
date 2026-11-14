#pragma once

#include <functional>
#include "mibase/Projecte.h"

#include <string>
#include <vector>

/** Operacions del MIDI (notes dels clips i piano roll), sense interfície: es proven soles.
    Les notes van en ticks (ticksPerNegra per negra) des de Clip::pos; la línia de temps, en segons.
    Totes les funcions que canvien notes mantenen la llista ordenada (per inici i to) i retornen
    (o actualitzen) els índexs de les notes triades. */
namespace mibase::midi
{
    // ---- Temps ----
    double segonsPerTick (const Projecte& p);
    double segons (const Projecte& p, double ticks);
    double ticks (const Projecte& p, double segons);
    long long ticksPerCompas (const Projecte& p);
    /** Temps absolut (segons) d'un tick d'un clip. */
    double segonsAbs (const Projecte& p, const Clip& c, double tick);
    /** Tick d'un clip a partir d'un temps absolut. */
    double tickDe (const Projecte& p, const Clip& c, double segonsAbs);

    // ---- Noms i teclat ----
    /** «C4» (60), «F#3»… Do central = C4 = 60. */
    std::string nomNota (int to);
    /** És una tecla negra? */
    bool esNegra (int to);

    // ---- Graella del piano roll ----
    /** Valors de la graella (quantització): rodona, blanca, negra, corxera, semicorxera, fusa i tresets.
        Índex 0 = lliure (sense graella). */
    struct ValorGraella { const char* nom; long long ticks; };
    const std::vector<ValorGraella>& valorsGraella();
    /** Enganxa a la graella (pas 0: no fa res). */
    long long enganxa (long long tick, long long pas);
    /** Enganxa cap avall (el tros de graella on cau). */
    long long enganxaAvall (long long tick, long long pas);

    // ---- Notes d'un clip ----
    /** Ordena les notes i actualitza els índexs triats perquè continuïn apuntant a les mateixes notes. */
    void ordena (std::vector<Nota>& notes, std::vector<int>& triades);
    /** Afegeix una nota (ja ordenada). Retorna el seu índex. */
    int afegeix (Clip& c, const Nota& n);
    void esborra (Clip& c, std::vector<int> idx);
    /** Mou en el temps (ticks) i en to (semitons). Ni el to surt de 0..127 ni l'inici de «minTick»
        (el grup es mou junt: si una nota hi topa, s'atura tot el grup). */
    void mou (Clip& c, std::vector<int>& idx, long long dTicks, int dTo, long long minTick = 0);
    /** Canvia la durada (en ticks). «perInici»: allarga o escurça per l'inici (el final no es mou). */
    void canviaDurada (Clip& c, std::vector<int>& idx, long long dTicks, bool perInici = false, long long durMinima = 1);
    /** Posa la mateixa durada a totes. */
    void fixaDurada (Clip& c, const std::vector<int>& idx, long long dur);
    /** Quantitza l'inici (i, si «tambeFinal», el final) al pas. «forca» de 0 a 1 (1 = a la graella exacta). */
    void quantitza (Clip& c, std::vector<int>& idx, long long pas, double forca = 1.0, bool tambeFinal = false);
    void transposa (Clip& c, std::vector<int>& idx, int semitons);
    void velocitat (Clip& c, const std::vector<int>& idx, int vel);
    void sumaVelocitat (Clip& c, const std::vector<int>& idx, int d);
    /** Duplica les notes just després del grup (el salt s'arrodoneix cap amunt al pas, si n'hi ha). Retorna les còpies. */
    std::vector<int> duplica (Clip& c, const std::vector<int>& idx, long long pas);

    /** Notes copiades (inici relatiu a la primera). */
    struct RetallNotes
    {
        long long durada = 0;
        std::vector<Nota> notes;
        bool buit() const noexcept { return notes.empty(); }
    };
    RetallNotes copia (const Clip& c, const std::vector<int>& idx);
    /** Enganxa amb la primera nota a «tick». Retorna les notes noves. */
    std::vector<int> enganxa (Clip& c, const RetallNotes& r, long long tick);

    /** Notes que cauen (comencen) dins [t0, t1) ticks i [toMin, toMax]. */
    std::vector<int> notesDins (const Clip& c, long long t0, long long t1, int toMin, int toMax);
    /** Per a cada nota, si en trepitja una altra del mateix to (per dibuixar-les diferents). */
    std::vector<bool> trepitjades (const std::vector<Nota>& notes);
    /** Final de l'última nota (ticks). */
    long long finalNotes (const Clip& c);

    // ---- Clips ----
    /** Clip MIDI nou i buit a la línia de temps, de «de» a «a» (segons). */
    Clip clipNou (double de, double a);
    /** Si alguna nota passa del final (o de l'inici) de la finestra, l'allarga perquè hi càpiga. */
    void encabeix (const Projecte& p, Clip& c);
    /** Ajunta dos clips MIDI en un (la finestra va del primer inici a l'últim final; només les notes que sonaven). */
    Clip ajunta (const Projecte& p, const Clip& a, const Clip& b);
    /** Canvia el tempo del projecte: els clips MIDI es mouen i s'estiren perquè les notes quedin als mateixos
        compassos i temps (l'àudio no es toca). */
    void canviaTempo (Projecte& p, double nouTempo);
    /** 0.42: fa un canvi de tempo (de la cançó o d'una part) i deixa els clips MIDI i els de la pista d'Acords
        exactament als mateixos compassos i amb la mateixa llargada en compassos. Els d'àudio no es mouen. */
    void ambMusicaFixa (Projecte& p, const std::function<void (Projecte&)>& canvi);
    /** Quina pista pot tenir aquest clip? (els clips MIDI només van a pistes MIDI i al revés). */
    bool compatible (const Pista& t, const Clip& c);

    // ---- Instruments interns ----
    struct SoIntern { const char* id; const char* nom; };
    /** Sons del sinte intern: «piano», «epiano», «orgue», «pad», «baix», «pluck», «lead», «cordes». */
    const std::vector<SoIntern>& sonsInterns();
    /** Índex del so intern d'un text d'instrument («intern:piano» → 0). Si no és intern, -1. */
    int soIntern (const std::string& instrument);
    std::string nomInstrument (const std::string& instrument);
}
