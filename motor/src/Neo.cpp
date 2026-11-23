#include "mibase/Neo.h"
#include "mibase/Atmos.h"
#include "mibase/Lead.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#if defined(__SSE__) || defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__)
 #include <xmmintrin.h>
 #define MIBASE_NEO_FTZ 1
#endif

namespace mibase::neo
{
    using sinte::tanhR; using sinte::blep; using sinte::coef; using sinte::num; using sinte::ms;
    using sinte::Lineal; using sinte::Enter; using sinte::Log; using sinte::Cub;

    namespace
    {
        constexpr float pi = 3.14159265358979f;
        constexpr double dpi = 3.14159265358979323846;
        const char* const motors = "Wavetable|Analògic|Harmònic|FM|Granular|Soroll";
        const char* const taules = "Bàsica|PWM|Vocal|Harmònics|Digital|Campana|Sync|Metall|Cor|Cordes|Vidre|Orgue|Aire|Fosc|Octaves|Formant";
        const char* const warps = "Cap|Corba|Sync|Mirall|PM|FM|Bits";
        const char* const onesLfo = "Sinus|Triangle|Serra|Quadrada|Atzar|Atzar suau";
        const char* const noSi = "No|Sí";
        const char* const fonts = "Cap|LFO 1|LFO 2|LFO 3|LFO 4|Env volum|Env filtre|Env mod|Velocitat|Nota|Roda|Aftertouch|Atzar|Passos|Osc 3 (àudio)";
        const char* const vies = "Sempre|Caràcter|Moviment|Brillantor|Agressivitat|Amplada|Espai|Evolució|Morph|Roda|Aftertouch|Velocitat|Env mod|LFO 1";
        const char* const destins = "Cap|To|To osc 1|To osc 2|To osc 3|Posició 1|Posició 2|Posició 3|Morph|Forma|Warp|Nivell 1|Nivell 2|Nivell 3|"
                                    "Tall|Ressonància|Vocal|Pan|Volum|Unison detune|Unison obertura|Plec|Drive|Grans densitat|Grans mida|"
                                    "Grans spray|Ressonadors|Soroll|Crush|Shifter|Chorus|Delay|Reverb|Espectre";
        const char* const notesDelay = "1/16|1/8T|1/8|1/8.|1/4T|1/4|1/4.|1/2";
        const int notaDelay[] = { 2, 3, 5, 7, 6, 8, 10, 11 };

#define OSC(n, mot, niv, oct) \
        { "o" #n "m", "Motor", SOsc, 0, 5, mot, Enter, motors, "Com fa el so l'oscil·lador " #n ": Wavetable, Analògic, Harmònic (additiu), FM, Granular o Soroll" }, \
        { "o" #n "t", "Taula", SOsc, 0, 15, 0, Enter, taules, "Wavetable (o font dels grànuls) de l'oscil·lador " #n }, \
        { "o" #n "p", "Posició", SOsc, 0, 1, 0.2f, Lineal, nullptr, "Wavetable i Granular: posició a la taula (morph). Analògic: amplada del pols. Harmònic: senars ↔ parells. FM: índex. Soroll: cruixit" }, \
        { "o" #n "f", "Forma", SOsc, 0, 1, 0.5f, Lineal, nullptr, "Wavetable/Granular: color (fosc ↔ brillant). Analògic: triangle → serra → quadrada → pols. Harmònic: inclinació. FM: relació. Soroll: color" }, \
        { "o" #n "w", "Warp", SOsc, 0, 6, 0, Enter, warps, "Deformació de la fase: Corba, Sync, Mirall, PM i FM (des de l'oscil·lador anterior), Bits" }, \
        { "o" #n "wq", "Warp q.", SOsc, 0, 1, 0, Lineal, nullptr, "Quantitat de la deformació" }, \
        { "o" #n "o", "Octava", SOsc, -3, 3, oct, Enter, nullptr, "Octava de l'oscil·lador " #n }, \
        { "o" #n "s", "Semitò", SOsc, -12, 12, 0, Enter, nullptr, "Semitons de l'oscil·lador " #n }, \
        { "o" #n "fi", "Fi", SOsc, -100, 100, 0, Lineal, nullptr, "Afinació fina (centèsimes)" }, \
        { "o" #n "n", "Nivell", SOsc, 0, 1, niv, Lineal, nullptr, "Volum de l'oscil·lador " #n }, \
        { "o" #n "pan", "Pan", SOsc, -1, 1, 0, Lineal, nullptr, "Panorama de l'oscil·lador " #n },
#define LFO(n, ona, vel) \
        { "l" #n "w", "Ona", SLfo, 0, 5, ona, Enter, onesLfo, "Forma de l'LFO " #n }, \
        { "l" #n "r", "Velocitat", SLfo, 0.02f, 30, vel, Log, nullptr, "Velocitat de l'LFO " #n " (si no va amb el tempo)" }, \
        { "l" #n "s", "Tempo", SLfo, 0, 11, 0, Enter, sinte::ritmesLfo, "LFO " #n " lliure o amb el tempo" }, \
        { "l" #n "in", "Entrada", SLfo, 0, 4000, 0, Cub, nullptr, "L'LFO " #n " entra a poc a poc després de la nota" }, \
        { "l" #n "ret", "Reinicia", SLfo, 0, 1, 1, Enter, noSi, "Sí: cada nota té el seu LFO, que comença de zero. No: un sol LFO lliure per a totes" },
#define PAS(n, v) { "p" #n, "Pas " #n, SMod, -1, 1, v, Lineal, nullptr, "Valor del pas " #n " del seqüenciador de modulació" },
#define SLOT(n) \
        { "m" #n "f", "Font", SMod, 0, 14, 0, Enter, fonts, "Font de la via " #n " de la matriu" }, \
        { "m" #n "v", "Via", SMod, 0, 13, 0, Enter, vies, "La via " #n " es multiplica per aquesta (una macro, la roda…). «Sempre»: no s'escala" }, \
        { "m" #n "d", "Destí", SMod, 0, 33, 0, Enter, destins, "Què mou la via " #n }, \
        { "m" #n "q", "Quantitat", SMod, -1, 1, 0, Lineal, nullptr, "Quant i en quin sentit mou la via " #n },
#define MACRO(n, nomM, ajuda) { "mac" #n, nomM, SMacro, 0, 1, 0, Lineal, nullptr, ajuda },
#define RUTA(n) \
        { "mc" #n "d1", "Destí A", SMacro, 0, 33, 0, Enter, destins, "Primer destí de la macro " #n }, \
        { "mc" #n "q1", "Quant. A", SMacro, -1, 1, 0, Lineal, nullptr, "Quant mou la macro " #n " el primer destí" }, \
        { "mc" #n "d2", "Destí B", SMacro, 0, 33, 0, Enter, destins, "Segon destí de la macro " #n }, \
        { "mc" #n "q2", "Quant. B", SMacro, -1, 1, 0, Lineal, nullptr, "Quant mou la macro " #n " el segon destí" },

        const sinte::InfoParam infos[numParams] = {
            OSC (1, 0, 0.8f, 0) OSC (2, 1, 0.0f, 0) OSC (3, 0, 0.0f, -1)
            { "morph", "Morph", SMix, 0, 1, 0, Lineal, nullptr, "Mou alhora la posició de les tres wavetables (i el que hi ha assignat): el so es transforma" },
            { "ring", "Ring", SMix, 0, 1, 0, Lineal, nullptr, "Modulació en anell (osc 1 × osc 2)" },
            { "subn", "Sub", SMix, 0, 1, 0, Lineal, nullptr, "Sub-oscil·lador (sinus)" },
            { "subo", "Sub oct", SMix, 0, 1, 0, Enter, "-1 oct|-2 oct", "Octava del sub" },
            { "nz", "Soroll", SMix, 0, 1, 0, Lineal, nullptr, "Soroll de fons (aire)" },
            { "nzc", "Color", SMix, 0, 1, 0.4f, Lineal, nullptr, "Color del soroll" },
            { "uni", "Unison", SMix, 1, 8, 1, Enter, nullptr, "Còpies de cada oscil·lador" },
            { "unid", "Detune", SMix, 0, 1, 0.3f, Lineal, nullptr, "Desafinació de les còpies" },
            { "unia", "Obertura", SMix, 0, 1, 0.6f, Lineal, nullptr, "Com s'obren les còpies a l'estèreo" },
            { "unib", "Barreja", SMix, 0, 1, 0.8f, Lineal, nullptr, "Volum de les còpies respecte de la central" },
            { "unim", "Mode", SMix, 0, 3, 0, Enter, "Lineal|Supersaw|Octaves|Acord", "Com es reparteixen les còpies: desafinades, a l'estil supersaw, en octaves o en acord de quinta" },
            { "lliure", "Fase lliure", SMix, 0, 1, 1, Enter, noSi, "Sí: els oscil·ladors no es reinicien a cada nota" },
            { "gd", "Densitat", SMix, 2, 200, 30, Log, nullptr, "Grànuls per segon (motor Granular)" },
            { "gm", "Mida", SMix, 8, 400, 70, Log, nullptr, "Durada de cada grànul" },
            { "gs", "Spray", SMix, 0, 1, 0.3f, Lineal, nullptr, "Com s'escampen els grànuls (posició, to i panorama)" },
            { "fold", "Plec", SMix, 0, 1, 0, Lineal, nullptr, "Plec d'ona: l'ona es doblega sobre si mateixa (harmònics nous, metàl·lic)" },
            { "dt", "Tipus", SMix, 0, 3, 0, Enter, "Suau|Dur|Fuzz|Rectifica", "Tipus de distorsió" },
            { "dq", "Drive", SMix, 0, 1, 0, Lineal, nullptr, "Quanta distorsió (a cada veu)" },
            { "ft", "Tipus", SFiltre, 0, 7, 0, Enter, "LP|HP|BP|Notch|Ladder|Formant|Comb +|Comb −", "Tipus de filtre. Formant: vocals. Comb: ressonància metàl·lica afinada al tall" },
            { "fp", "Pendent", SFiltre, 0, 1, 1, Enter, "12 dB|24 dB", "Pendent (LP, HP, BP, Notch, Ladder)" },
            { "cut", "Tall", SFiltre, 30, 18000, 4000, Log, nullptr, "Freqüència de tall (Formant: mida de la boca; Comb: afinació)" },
            { "res", "Ressonància", SFiltre, 0, 1, 0.2f, Lineal, nullptr, "Ressonància (Comb: realimentació)" },
            { "fenv", "Envolupant", SFiltre, -1, 1, 0.2f, Lineal, nullptr, "Quant mou el tall l'envolupant del filtre" },
            { "key", "Seguiment", SFiltre, 0, 1, 0.5f, Lineal, nullptr, "Seguiment de teclat" },
            { "fdrv", "Saturació", SFiltre, 0, 1, 0.05f, Lineal, nullptr, "Saturació a l'entrada del filtre" },
            { "voc", "Vocal", SFiltre, 0, 1, 0, Lineal, nullptr, "Vocal del filtre Formant: A → E → I → O → U" },
            { "rsm", "Ressonadors", SFiltre, 0, 1, 0, Lineal, nullptr, "Tres ressonadors afinats a la nota: el so sembla d'un objecte real" },
            { "rst", "Material", SFiltre, 0, 3, 0, Enter, "Harmònic|Campana|Metall|Fusta", "Material dels ressonadors" },
            { "rsd", "Durada", SFiltre, 0, 1, 0.5f, Lineal, nullptr, "Quant ressonen" },
            { "aa", "A", SEnv, 0.3f, 8000, 2, Log, nullptr, "Atac del volum" },
            { "ad", "D", SEnv, 5, 10000, 600, Log, nullptr, "Caiguda del volum" },
            { "as", "S", SEnv, 0, 1, 0.8f, Lineal, nullptr, "Sostingut del volum" },
            { "ar", "R", SEnv, 3, 10000, 300, Log, nullptr, "Alliberament del volum" },
            { "fa", "A", SEnv, 0.3f, 8000, 2, Log, nullptr, "Atac del filtre" },
            { "fd", "D", SEnv, 5, 10000, 500, Log, nullptr, "Caiguda del filtre" },
            { "fs", "S", SEnv, 0, 1, 0.4f, Lineal, nullptr, "Sostingut del filtre" },
            { "fr", "R", SEnv, 3, 10000, 300, Log, nullptr, "Alliberament del filtre" },
            { "ea", "A", SEnv, 0.3f, 8000, 600, Log, nullptr, "Atac de l'envolupant de modulació (font «Env mod»)" },
            { "ed", "D", SEnv, 5, 10000, 2500, Log, nullptr, "Caiguda de l'envolupant de modulació" },
            { "es", "S", SEnv, 0, 1, 0.3f, Lineal, nullptr, "Sostingut de l'envolupant de modulació" },
            { "er", "R", SEnv, 3, 10000, 600, Log, nullptr, "Alliberament de l'envolupant de modulació" },
            LFO (1, 0, 0.5f) LFO (2, 1, 2.0f) LFO (3, 5, 0.2f) LFO (4, 0, 5.5f)
            PAS (1, 1.0f) PAS (2, 0.0f) PAS (3, 0.5f) PAS (4, -0.5f) PAS (5, 0.8f) PAS (6, 0.2f) PAS (7, -0.2f) PAS (8, 0.6f)
            PAS (9, 1.0f) PAS (10, -0.6f) PAS (11, 0.4f) PAS (12, 0.0f) PAS (13, 0.7f) PAS (14, -0.3f) PAS (15, 0.3f) PAS (16, -1.0f)
            { "pr", "Ritme", SMod, 0, 5, 3, Enter, sinte::ritmesArp, "Durada de cada pas del seqüenciador de modulació (amb el tempo)" },
            { "pll", "Llisca", SMod, 0, 1, 0, Lineal, nullptr, "Com llisca d'un pas al següent (0: salts secs)" },
            { "pn", "Passos", SMod, 2, 16, 16, Enter, nullptr, "Quants passos té la seqüència" },
            SLOT (1) SLOT (2) SLOT (3) SLOT (4) SLOT (5) SLOT (6) SLOT (7) SLOT (8) SLOT (9) SLOT (10) SLOT (11) SLOT (12)
            MACRO (1, "Caràcter", "Caràcter: canvia el timbre (forma, deformació)")
            MACRO (2, "Moviment", "Moviment: més animació (LFO, passos)")
            MACRO (3, "Brillantor", "Brillantor: obre el filtre i l'espectre")
            MACRO (4, "Agressivitat", "Agressivitat: distorsió i plec")
            MACRO (5, "Amplada", "Amplada: unison i chorus")
            MACRO (6, "Espai", "Espai: reverb i delay")
            MACRO (7, "Evolució", "Evolució: el so es transforma al llarg de la nota")
            MACRO (8, "Morph", "Morph: transforma les wavetables")
            RUTA (1) RUTA (2) RUTA (3) RUTA (4) RUTA (5) RUTA (6) RUTA (7) RUTA (8)
            { "crm", "Crush", SFx, 0, 1, 0, Lineal, nullptr, "Quantitat de bit crusher" },
            { "crb", "Bits", SFx, 2, 16, 8, Lineal, nullptr, "Resolució (menys bits, més brut)" },
            { "crr", "Mostreig", SFx, 1, 40, 4, Log, nullptr, "Divideix la freqüència de mostreig (so digital, «lo-fi»)" },
            { "shh", "Hz", SFx, -1000, 1000, 0, Lineal, nullptr, "Desplaçament de freqüència: totes les freqüències es mouen els mateixos Hz (inharmònic)" },
            { "shm", "Shifter", SFx, 0, 1, 0, Lineal, nullptr, "Quantitat del desplaçador de freqüència" },
            { "chm", "Chorus", SFx, 0, 1, 0, Lineal, nullptr, "Quantitat de chorus" },
            { "chr", "Vel.", SFx, 0.1f, 5, 0.6f, Log, nullptr, "Velocitat del chorus" },
            { "chp", "Prof.", SFx, 0, 1, 0.5f, Lineal, nullptr, "Profunditat del chorus" },
            { "dlm", "Delay", SFx, 0, 1, 0, Lineal, nullptr, "Quantitat de delay" },
            { "dlt", "Temps", SFx, 0, 7, 3, Enter, notesDelay, "Temps del delay, amb el tempo" },
            { "dlf", "Feedback", SFx, 0, 0.92f, 0.35f, Lineal, nullptr, "Repeticions del delay" },
            { "dlpp", "Ping-pong", SFx, 0, 1, 1, Enter, noSi, "Els ecos van d'un costat a l'altre" },
            { "rvm", "Reverb", SFx, 0, 1, 0, Lineal, nullptr, "Quantitat de reverb" },
            { "rvs", "Mida", SFx, 0, 1, 0.5f, Lineal, nullptr, "Mida i durada de la reverb" },
            { "rvt", "Tipus", SFx, 0, 3, 0, Enter, "Plate|Hall|Ambient|Shimmer", "Tipus de reverb" },
            { "tilt", "Inclinació", SFx, -1, 1, 0, Lineal, nullptr, "Espectre: més greus (−) o més aguts (+)" },
            { "exc", "Excitador", SFx, 0, 1, 0, Lineal, nullptr, "Espectre: harmònics aguts nous (presència)" },
            { "width", "Amplada", SFx, 0, 1.5f, 1, Lineal, nullptr, "Amplada estèreo final" },
            { "vol", "Volum", SFx, -24, 6, -4, Lineal, nullptr, "Volum de sortida" },
            { "mode", "Mode", SVeu, 0, 2, 2, Enter, "Mono|Legato|Poli", "Mono, Legato (les notes lligades llisquen) o Poli" },
            { "veus", "Veus", SVeu, 1, 16, 8, Enter, nullptr, "Polifonia màxima (en mode Poli)" },
            { "glide", "Glide", SVeu, 0, 2000, 0, Cub, nullptr, "Portament" },
            { "bend", "Bend", SVeu, 0, 24, 2, Enter, nullptr, "Rang del pitch bend (semitons)" },
            { "vv", "Vel → vol", SVeu, 0, 1, 0.5f, Lineal, nullptr, "Com canvia el volum amb la força de la nota" },
            { "arp", "Arp", SArp, 0, 7, 0, Enter, sinte::modesArp, "Arpegiador / seqüenciador: amb una tecla o un acord, toca sol amb el tempo" },
            { "arpr", "Ritme", SArp, 0, 5, 3, Enter, sinte::ritmesArp, "Durada de cada pas" },
            { "arpo", "Octaves", SArp, 1, 4, 1, Enter, nullptr, "Octaves que recorre" },
            { "arpg", "Gate", SArp, 0.05f, 1, 0.5f, Lineal, nullptr, "Durada de cada nota (al màxim, lligades)" },
            { "arpsw", "Swing", SArp, 0, 0.6f, 0, Lineal, nullptr, "Swing" },
            { "arpp", "Patró", SArp, 0, 15, 0, Enter, sinte::nomsPatrons, "Patró del seqüenciador (mode «Patró»)" },
        };
#undef OSC
#undef LFO
#undef PAS
#undef SLOT
#undef MACRO
#undef RUTA

        // taula de sinus per a l'additiu i l'FM
        constexpr int midaSin = 4096;
        float taulaSin[midaSin + 1];
        const bool sinFet = []
        {
            for (int i = 0; i <= midaSin; ++i) taulaSin[i] = (float) std::sin (2.0 * dpi * i / midaSin);
            return true;
        }();
        inline float sinT (double fase) noexcept
        {
            fase -= std::floor (fase);
            const double x = fase * midaSin;
            const int i = (int) x;
            const float k = (float) (x - i);
            return taulaSin[i] + (taulaSin[i + 1] - taulaSin[i]) * k;
        }
        inline float llegeixTaula (int t, double fase, float pos, double dt) noexcept
        {
            return t < 8 ? lead::wt::llegeix (t, fase, pos, dt) : atmos::wt::llegeix (t - 8, fase, pos, dt);
        }
        // ressonadors: relacions de freqüència de cada material
        const float relRs[4][3] = { { 1.0f, 2.0f, 3.0f }, { 1.0f, 2.76f, 5.4f }, { 1.0f, 1.59f, 2.14f }, { 1.0f, 3.93f, 9.5f } };
        // vocals del filtre formant (F1, F2, F3)
        const float vocals[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 350, 2300, 3000 }, { 450, 800, 2830 }, { 325, 700, 2530 } };
        // Hilbert (Olli Niemitalo): dues cadenes de 4 passa-tot de segon ordre (en z^-2)
        const double hbA[4] = { 0.6923878, 0.9360654322959, 0.9882295226860, 0.9987488452737 };
        const double hbB[4] = { 0.4021921162426, 0.8561710882420, 0.9722909545651, 0.9952884791278 };
    }

    const sinte::InfoParam& info (int p) { return infos[std::clamp (p, 0, numParams - 1)]; }
    const char* nomTaula (int t) { return atmos::nomTaula (t); }
    float mostraTaula (int t, double fase, float pos) { return llegeixTaula (std::clamp (t, 0, numTaules - 1), fase, pos, 1.0 / 4096.0); }
    const char* nomMacro (int k)
    {
        static const char* n[] = { "CARÀCTER", "MOVIMENT", "BRILLANTOR", "AGRESSIVITAT", "AMPLADA", "ESPAI", "EVOLUCIÓ", "MORPH" };
        return n[std::clamp (k, 0, 7)];
    }

    std::string textValor (int p, float v)
    {
        const auto& i = info (p);
        if (i.opcions != nullptr)
        {
            const auto o = sinte::opcions (i);
            return o[(size_t) std::clamp ((int) std::lround (v - i.mn), 0, (int) o.size() - 1)];
        }
        switch (p)
        {
            case O1Oct: case O2Oct: case O3Oct: return (v > 0 ? "+" : "") + num (v, 0) + " oct";
            case O1Semi: case O2Semi: case O3Semi: return (v > 0 ? "+" : "") + num (v, 0) + " st";
            case O1Fi: case O2Fi: case O3Fi: return (v > 0 ? "+" : "") + num (v, 0) + " ct";
            case O1Pan: case O2Pan: case O3Pan: return std::abs (v) < 0.02f ? std::string ("C") : (v < 0 ? "L " : "R ") + num (std::abs (v) * 100.0, 0);
            case O1Forma: case O2Forma: case O3Forma:
            {
                return num (v * 100.0, 0) + " %";
            }
            case UniVeus: return v < 1.5f ? std::string ("Off") : num (v, 0) + " veus";
            case UniDetune: return num (v * v * 80.0, 0) + " ct";
            case GrDens: return num (v, v < 10.0f ? 1 : 0) + " /s";
            case GrMida: case AA: case AD: case AR: case FA: case FD: case FR: case EA: case ED: case ER: return ms (v);
            case Glide: case L1Entrada: case L2Entrada: case L3Entrada: case L4Entrada: return v < 0.5f ? std::string ("Off") : ms (v);
            case FTall: return v < 1000.0f ? num (v, 0) + " Hz" : num (v / 1000.0, v < 10000.0f ? 2 : 1) + " kHz";
            case L1Vel: case L2Vel: case L3Vel: case L4Vel: case ChVel: return num (v, v < 10.0f ? 2 : 1) + " Hz";
            case CrBits: return num (v, 1) + " bits";
            case CrMostreig: return v < 1.05f ? std::string ("Off") : "÷" + num (v, 1);
            case ShHz: return (v > 0 ? "+" : "") + num (v, 0) + " Hz";
            case Volum: return (v > 0 ? "+" : "") + num (v, 1) + " dB";
            case BendRang: return num (v, 0) + " st";
            case Veus: return num (v, 0);
            case PasNum: return num (v, 0);
            case ArpOct: return num (v, 0) + " oct";
            case ArpGate: return v >= 0.99f ? std::string ("Lligat") : num (v * 100.0, 0) + " %";
            case FEnv: case EsTilt: return (v > 0 ? "+" : "") + num (v * 100.0, 0) + " %";
            default:
                if (i.mn < 0.0f) return (v > 0 ? "+" : "") + num (v * 100.0, 0) + " %";
                return num (v * 100.0, 0) + " %";
        }
    }

    //==========================================================================
    Neo::Neo() : sinte::Base (infos, numParams, "mibase-neo", { ArpMode, ArpRitme, ArpOct, ArpGate, ArpSwing, ArpPatro })
    {
        (void) sinFet;
        lead::wt::prepara();
        atmos::wt::prepara();
        assignaCC (74, FTall);
        assignaCC (71, FRes);
        dl.posaTempo (&sinte::tempo());
        rv.posaTempo (&sinte::tempo());
        dl.posa (delay::Sync, 1.0f);
        dl.posa (delay::CaracterP, (float) delay::Digital);
        dl.posa (delay::HighCut, 8000.0f);
        dl.posa (delay::LowCut, 160.0f);
        rv.posa (reverb::PreDelay, 20.0f);
        rv.posa (reverb::LowCut, 180.0f);
        veus.resize (maxVeus);
    }

    std::string Neo::nomComplet (int i) const
    {
        static const char* seccions[] = { "Osc", "Mix", "Filtre", "Env", "LFO", "Matriu", "Macro", "FX", "Veu", "Arp" };
        const auto& x = info (i);
        auto amb = [&] (const char* pre) { return std::string (pre) + " · " + x.nom; };
        if (i >= O1Motor && i <= O1Pan) return amb ("Osc 1");
        if (i >= O2Motor && i <= O2Pan) return amb ("Osc 2");
        if (i >= O3Motor && i <= O3Pan) return amb ("Osc 3");
        if (i >= AA && i <= AR) return amb ("Env volum");
        if (i >= FA && i <= FR) return amb ("Env filtre");
        if (i >= EA && i <= ER) return amb ("Env mod");
        if (i >= L1Ona && i <= L4Reinicia) return "LFO " + std::to_string ((i - L1Ona) / 5 + 1) + " · " + x.nom;
        if (i >= M1Font && i <= M12Quant) return "Matriu " + std::to_string ((i - M1Font) / 4 + 1) + " · " + x.nom;
        if (i >= Mac1 && i <= Mac8) return std::string ("Macro · ") + x.nom;
        if (i >= Mc1D1 && i <= Mc8Q2) return std::string ("Macro ") + std::to_string ((i - Mc1D1) / 4 + 1) + " · " + x.nom;
        if (i >= Pas1 && i <= PasNum) return std::string ("Passos · ") + x.nom;
        return std::string (seccions[x.seccio]) + " · " + x.nom;
    }

    void Neo::preparaSo (double f)
    {
        fsI = f * sobre;
        veus.assign (maxVeus, Veu());
        const size_t midaComb = (size_t) (fsI * 0.05) + 8;
        for (auto& v : veus) for (auto& c : v.comb) c.assign (midaComb, 0.0f);
        delL.buida(); delR.buida();
        numPila = 0;
        dcL = dcR = dcxL = dcxR = 0.0f;
        chL.assign ((size_t) (f * 0.05) + 4, 0.0f);
        chR.assign ((size_t) (f * 0.05) + 4, 0.0f);
        chPos = 0;
        hbL = Hilbert(); hbR = Hilbert();
        dl.prepara (f, tros);
        rv.prepara (f, tros);
        for (auto& x : darrerDl) x = -1.0f;
        for (auto& x : darrerRv) x = -1.0f;
    }

    bool Neo::veusSonen() const noexcept
    {
        for (const auto& v : veus) if (v.viva) return true;
        return valor (DlMix) > 0.001f || valor (RvMix) > 0.001f || valor (Mac6) > 0.001f ? nivell() > 1e-4f : false;
    }
    int Neo::numVeusActives() const noexcept { int n = 0; for (const auto& v : veus) n += v.viva ? 1 : 0; return n; }
    void Neo::apagaVeus() noexcept
    {
        for (auto& v : veus) if (v.viva) { v.etapaA = 4; v.tecla = false; v.etapaF = 3; v.etapaE = 3; }
        numPila = 0;
    }

    // ---- notes ----
    void Neo::dispara (Veu& v, int nota, float vel, bool retriga, bool glide) noexcept
    {
        const bool estava = v.viva && v.envA > 1e-4f;
        bool algunaTecla = false;
        for (const auto& w : veus) if (w.tecla && w.viva) algunaTecla = true;
        if (! algunaTecla) { fasePas = 0.0; pas = 0; }
        v.nota = nota;
        v.objectiu = nota;
        if (! glide || ! v.viva) v.semis = nota;
        v.vel = vel;
        v.tecla = true;
        v.ordre = ++comptaOrdre;
        if (retriga)
        {
            v.etapaA = v.etapaF = v.etapaE = 1;
            v.temps = 0.0f;
            v.atzar = aleat();
            for (int k = 0; k < 4; ++k) if (valor (L1Reinicia + k * 5) > 0.5f) { v.faseLfo[k] = 0.0; v.shLfo[k] = aleat(); v.shAnt[k] = v.shLfo[k]; }
            if (! estava)
            {
                v.envA = v.envF = v.envE = 0.0f;
                const bool lliure = valor (FaseLliure) > 0.5f;
                for (int o = 0; o < 3; ++o)
                    for (int k = 0; k < maxUnison; ++k)
                    {
                        const double ph = lliure ? 0.5 + 0.5 * aleat() : k * 0.1373 + o * 0.29;
                        v.ph[o][k] = ph - std::floor (ph);
                        v.phM[o][k] = lliure ? 0.5 + 0.5 * aleat() : 0.0;
                    }
                for (auto& gr : v.grans) for (auto& g : gr) g.viu = false;
                for (auto& f : v.finsGra) f = 0.0f;
                v.fSub = 0.0;
                for (auto& c : v.fl) for (auto& s : c) s = Svf();
                for (auto& c : v.fmt) for (auto& s : c) s = Svf();
                for (auto& s : v.rs) s = Svf();
                for (auto& c : v.ld) for (auto& x : c) x = 0.0f;
                for (auto& c : v.comb) std::fill (c.begin(), c.end(), 0.0f);
                v.drvTo[0] = v.drvTo[1] = 0.0f;
                for (auto& d : v.darrera) d = 0.0f;
            }
        }
        v.viva = true;
        posaHz ((float) (440.0 * std::pow (2.0, (nota - 69) / 12.0)));
    }

    void Neo::allibera (Veu& v) noexcept
    {
        v.tecla = false;
        if (v.etapaA != 0 && v.etapaA != 4) v.etapaA = 3;
        if (v.etapaF != 0) v.etapaF = 3;
        if (v.etapaE != 0) v.etapaE = 3;
    }

    void Neo::notaOn (int nota, int vel) noexcept
    {
        const float vf = std::clamp (vel / 127.0f, 0.0f, 1.0f);
        const int mode = (int) valor (Mode);
        const bool ambGlide = valor (Glide) > 0.5f;
        if (mode == 2)
        {
            const int maxV = std::clamp ((int) valor (Veus), 1, maxVeus);
            // les veus de més enllà del límit s'apaguen
            for (int i = maxV; i < maxVeus; ++i) if (veus[(size_t) i].viva) { veus[(size_t) i].etapaA = 4; veus[(size_t) i].tecla = false; }
            Veu* tria = nullptr;
            for (int i = 0; i < maxV; ++i) if (! veus[(size_t) i].viva) { tria = &veus[(size_t) i]; break; }
            if (tria == nullptr) for (int i = 0; i < maxV; ++i) { auto& v = veus[(size_t) i]; if (! v.tecla && (tria == nullptr || v.envA < tria->envA)) tria = &v; }
            if (tria == nullptr) for (int i = 0; i < maxV; ++i) { auto& v = veus[(size_t) i]; if (tria == nullptr || v.ordre < tria->ordre) tria = &v; }
            const bool g = ambGlide && darreraNota >= 0.0;
            if (g && ! tria->viva) { tria->semis = darreraNota; tria->viva = true; tria->envA = 0.0f; tria->etapaA = 0; }
            dispara (*tria, nota, vf, true, g);
            darreraNota = nota;
            return;
        }
        for (int i = 0; i < numPila; ++i)
            if (pila[i] == nota) { std::memmove (pila + i, pila + i + 1, sizeof (int) * (size_t) (numPila - i - 1)); --numPila; break; }
        const bool lligada = numPila > 0;
        if (numPila < 32) pila[numPila++] = nota;
        for (int i = 1; i < maxVeus; ++i) if (veus[(size_t) i].viva) { veus[(size_t) i].etapaA = 4; veus[(size_t) i].tecla = false; }
        auto& v = veus[0];
        if (mode == 1) dispara (v, nota, lligada ? v.vel : vf, ! lligada, lligada);
        else dispara (v, nota, vf, true, ambGlide && v.viva && v.envA > 1e-4f);
        darreraNota = nota;
    }

    void Neo::notaOff (int nota) noexcept
    {
        const int mode = (int) valor (Mode);
        if (mode == 2)
        {
            for (auto& v : veus) if (v.viva && v.tecla && v.nota == nota) allibera (v);
            return;
        }
        for (int i = 0; i < numPila; ++i)
            if (pila[i] == nota) { std::memmove (pila + i, pila + i + 1, sizeof (int) * (size_t) (numPila - i - 1)); --numPila; break; }
        auto& v = veus[0];
        if (v.nota != nota) return;
        if (numPila > 0) dispara (v, pila[numPila - 1], v.vel, mode == 0, mode == 1 || valor (Glide) > 0.5f);
        else allibera (v);
    }

    // ---- modulació ----
    float Neo::valorLfo (int o, double ph, float sh, float shAnt) const noexcept
    {
        switch (o)
        {
            case 0: return sinT (ph);
            case 1: return 1.0f - 4.0f * std::abs ((float) ph - 0.5f);
            case 2: return 1.0f - 2.0f * (float) ph;
            case 3: return ph < 0.5 ? 1.0f : -1.0f;
            case 4: return sh;
            default: { const float t = (float) ph; const float s = t * t * (3.0f - 2.0f * t); return shAnt + (sh - shAnt) * s; }
        }
    }

    float Neo::pm (int p, const float* mod, int d) const noexcept
    {
        const auto& i = infos[p];
        if (d <= 0 || mod[d] == 0.0f) return valor (p);
        return sinte::deNormal (i, sinte::normal (i, valor (p)) + mod[d]);
    }

    void Neo::calculaMod (Veu& v, float* f) noexcept
    {
        // f: valors de les fonts (-1..1 o 0..1)
        float* mod = v.mod;
        for (int d = 0; d < numDestins; ++d) mod[d] = 0.0f;
        for (int s = 0; s < numSlots; ++s)
        {
            const int b = M1Font + s * 4;
            const int fo = (int) valor (b), de = (int) valor (b + 2);
            if (fo <= FCap || de <= DCap || fo == FOsc3) continue;   // l'àudio va a part
            const int vi = (int) valor (b + 1);
            float via = 1.0f;
            if (vi >= VMac1 && vi <= VMac8) via = 2.0f * valor (Mac1 + vi - VMac1);   // al mig (0,5): tal com és
            else if (vi == VRoda) via = rodaMod;
            else if (vi == VAt) via = pressioAra;
            else if (vi == VVel) via = v.vel;
            else if (vi == VEnvMod) via = v.envE;
            else if (vi == VLfo1) via = 0.5f + 0.5f * f[FLfo1];
            mod[de] += valor (b + 3) * f[fo] * via;
        }
        sumaMacros (mod);
    }

    void Neo::sumaMacros (float* mod) const noexcept
    {
        for (int k = 0; k < numMacros; ++k)
        {
            const float m = k == 1 ? 2.0f * (valor (Mac2) - 0.5f) : valor (Mac1 + k);   // Moviment: el mig és el preset tal com és
            if (m == 0.0f) continue;
            const int b = Mc1D1 + k * 4;
            const int d1 = (int) valor (b), d2 = (int) valor (b + 2);
            if (d1 > 0) mod[d1] += valor (b + 1) * m;
            if (d2 > 0) mod[d2] += valor (b + 3) * m;
        }
    }

    void Neo::renderitza (float* L, float* R, int n) noexcept
    {
#ifdef MIBASE_NEO_FTZ
        // els ressonadors i els filtres, quan s'apaguen, cauen a valors desnormals (molt lents a la CPU)
        struct Ftz { unsigned int csr = _mm_getcsr(); Ftz() { _mm_setcsr (csr | 0x8040); } ~Ftz() { _mm_setcsr (csr); } } ftz;
#endif
        const int nI = n * sobre;
        std::memset (bufL, 0, sizeof (float) * (size_t) nI);
        std::memset (bufR, 0, sizeof (float) * (size_t) nI);
        constexpr int sub = 16;
        const double tempoAra = std::max (20.0, sinte::tempo().load (std::memory_order_relaxed));
        double hz[4];
        for (int k = 0; k < 4; ++k)
        {
            const int r = (int) valor (L1Ritme + k * 5);
            hz[k] = r <= 0 ? (double) valor (L1Vel + k * 5) : tempoAra / 60.0 / sinte::negresRitme (r);
        }
        const double hzPas = tempoAra / 60.0 / sinte::negresArp ((int) valor (PasRitme));
        const int numP = std::clamp ((int) valor (PasNum), 2, numPassos);
        const float llisca = valor (PasLlisca);
        for (int s = 0; s < nI; s += sub)
        {
            const int m = std::min (sub, nI - s);
            const double dtS = (double) m / fsI;
            for (int k = 0; k < 4; ++k)
            {
                shGAnt[k] = shG[k];
                faseG[k] += hz[k] * dtS;
                if (faseG[k] >= 1.0) { faseG[k] -= std::floor (faseG[k]); shGAnt[k] = shG[k]; shG[k] = aleat(); }
            }
            fasePas += hzPas * dtS;
            if (fasePas >= 1.0) { fasePas -= std::floor (fasePas); pas = (pas + 1) % numP; }
            const float objPas = valor (Pas1 + std::min (pas, numP - 1));
            valPas += (objPas - valPas) * (llisca < 0.01f ? 1.0f : (float) std::min (1.0, dtS * 60.0 / (0.05 + llisca * 4.0)));
            for (auto& v : veus) if (v.viva) renderitzaVeu (v, s, m);
        }
        pasV.store (numTecles() > 0 || veusSonen() ? pas : -1, std::memory_order_relaxed);
        // modulació per als efectes: la de la veu més nova (o només les macros si no sona res)
        const Veu* nova = nullptr;
        for (const auto& v : veus) if (v.viva && (nova == nullptr || v.ordre > nova->ordre)) nova = &v;
        if (nova != nullptr) std::memcpy (modFx, nova->mod, sizeof (modFx));
        else
        {
            for (auto& x : modFx) x = 0.0f;
            sumaMacros (modFx);
        }
        float oL[tros], oR[tros];
        delL.processa (bufL, oL, n);
        delR.processa (bufR, oR, n);
        for (int i = 0; i < n; ++i)
        {
            const float yl = oL[i], yr = oR[i];
            const float ol = yl - dcxL + 0.9995f * dcL; dcxL = yl; dcL = ol;
            const float orr = yr - dcxR + 0.9995f * dcR; dcxR = yr; dcR = orr;
            oL[i] = ol; oR[i] = orr;
        }
        efectes (oL, oR, n);
        const float volObj = std::pow (10.0f, valor (Volum) / 20.0f);
        const float amp = valor (Ample);
        for (int i = 0; i < n; ++i)
        {
            volSuau += (volObj - volSuau) * 0.002f;
            const float mid = 0.5f * (oL[i] + oR[i]), sid = 0.5f * (oL[i] - oR[i]) * amp;
            auto sl = [] (float x) { const float ax = std::abs (x); return ax <= 0.8f ? x : std::copysign (0.8f + 0.18f * tanhR ((ax - 0.8f) / 0.18f), x); };   // mai arriba a 0 dBFS
            L[i] = sl ((mid + sid) * volSuau);
            R[i] = sl ((mid - sid) * volSuau);
        }
    }

    void Neo::renderitzaVeu (Veu& v, int s0, int m) noexcept
    {
        const int fi = s0 + m;
        const double dtS = (double) m / fsI;
        v.temps += (float) dtS;

        // ---- fonts ----
        float f[numFonts] {};
        for (int k = 0; k < 4; ++k)
        {
            const int b = L1Ona + k * 5;
            float l;
            if (valor (b + 4) > 0.5f)
            {
                const int r = (int) valor (b + 2);
                const double h = r <= 0 ? (double) valor (b + 1)
                                        : std::max (20.0, sinte::tempo().load (std::memory_order_relaxed)) / 60.0 / sinte::negresRitme (r);
                v.faseLfo[k] += h * dtS;
                if (v.faseLfo[k] >= 1.0) { v.faseLfo[k] -= std::floor (v.faseLfo[k]); v.shAnt[k] = v.shLfo[k]; v.shLfo[k] = aleat(); }
                l = valorLfo ((int) valor (b), v.faseLfo[k], v.shLfo[k], v.shAnt[k]);
            }
            else l = valorLfo ((int) valor (b), faseG[k], shG[k], shGAnt[k]);
            const float entrada = valor (b + 3) / 1000.0f;
            if (entrada > 0.001f) l *= std::clamp (v.temps / entrada, 0.0f, 1.0f);
            f[FLfo1 + k] = l;
            lfoGlob[k] = l;
        }
        f[FEnvVol] = v.envA; f[FEnvFiltre] = v.envF; f[FEnvMod] = v.envE;
        f[FVel] = v.vel; f[FNota] = (float) ((v.semis - 60.0) / 48.0);
        f[FRoda] = rodaMod; f[FAt] = pressioAra; f[FAtzar] = v.atzar; f[FPassos] = valPas;
        f[FOsc3] = v.darrera[2];
        calculaMod (v, f);
        const float* mod = v.mod;
        // activitat per a la interfície (de la veu més nova)
        bool laMesNova = true;
        for (const auto& w : veus) if (w.viva && w.ordre > v.ordre) { laMesNova = false; break; }
        if (laMesNova)
        {
            for (int s = 0; s < numSlots; ++s)
            {
                const int b = M1Font + s * 4;
                const int fo = (int) valor (b);
                actV[(size_t) s].store (fo > 0 && (int) valor (b + 2) > 0 ? f[fo] * valor (b + 3) : 0.0f, std::memory_order_relaxed);
            }
            for (int k = 0; k < 4; ++k) lfosV[(size_t) k].store (f[FLfo1 + k], std::memory_order_relaxed);
            posaLfo (f[FLfo1]);
        }
        // modulació a ritme d'àudio (font «Osc 3»)
        float aPm[3] {}, aPos[3] {}, aVol = 0.0f;
        for (int s = 0; s < numSlots; ++s)
        {
            const int b = M1Font + s * 4;
            if ((int) valor (b) != FOsc3) continue;
            const int d = (int) valor (b + 2);
            const float q = valor (b + 3);
            if (d == DTo) { aPm[0] += q; aPm[1] += q; }
            else if (d >= DTo1 && d <= DTo3) aPm[d - DTo1] += q;
            else if (d >= DPos1 && d <= DPos3) aPos[d - DPos1] += q;
            else if (d == DMorph) { for (auto& x : aPos) x += q; }
            else if (d == DVol) aVol += q;
            // la resta de destins: amb el valor de l'oscil·lador al principi del tros (més lent, però també es mou)
        }

        // ---- to ----
        const float gl = valor (Glide);
        if (gl < 0.5f) v.semis = v.objectiu;
        else v.semis += (v.objectiu - v.semis) * (1.0 - std::exp (-dtS / (gl / 1000.0 / 3.0)));
        const double semis = v.semis + bendCru * valor (BendRang) + mod[DTo] * 12.0;
        const double hzNota = 440.0 * std::pow (2.0, (semis - 69.0) / 12.0);

        // ---- oscil·ladors ----
        const int nu = std::clamp ((int) valor (UniVeus), 1, maxUnison);
        const int uMode = (int) valor (UniMode);
        const float uDet = std::clamp (pm (UniDetune, mod, DUniDet), 0.0f, 1.0f), uAmp = std::clamp (pm (UniAmple, mod, DUniAmple), 0.0f, 1.0f);
        const float uBlend = valor (UniBlend);
        const float morph = std::clamp (pm (Morph, mod, DMorph), 0.0f, 1.0f);
        int motor[3], taula[3], warp[3], cnt[3];
        float pos[3], forma[3], wq[3], niv[3], panO[3];
        double dt[3][maxUnison], dtM[3];
        float gL[3][maxUnison], gR[3][maxUnison];
        float amps[3][parcials] {}; int nParc[3] {};
        static const float octavesU[maxUnison] = { 0, 12, -12, 24, 0, 12, -12, 24 };
        static const float acordU[maxUnison] = { 0, 7, 12, 19, -12, 24, 7, 12 };
        static const float jp[maxUnison] = { 0.0f, -0.11f, 0.11f, -0.36f, 0.36f, -0.69f, 0.69f, -1.0f };
        static const float ratiosFm[] = { 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f };
        bool estereo = false;
        for (int o = 0; o < 3; ++o)
        {
            const int b = o * 11;
            motor[o] = (int) valor (O1Motor + b);
            taula[o] = (int) valor (O1Taula + b);
            pos[o] = std::clamp (pm (O1Pos + b, mod, DPos1 + o) + (motor[o] == MSoroll ? 0.0f : morph), 0.0f, 1.0f);
            forma[o] = std::clamp (valor (O1Forma + b) + mod[DForma], 0.0f, 1.0f);
            warp[o] = (int) valor (O1Warp + b);
            wq[o] = std::clamp (valor (O1WarpQ + b) + mod[DWarp], 0.0f, 1.0f);
            niv[o] = std::clamp (pm (O1Niv + b, mod, DNiv1 + o), 0.0f, 1.0f) * 0.6f;
            panO[o] = valor (O1Pan + b);
            const double hz = hzNota * std::pow (2.0, valor (O1Oct + b) + valor (O1Semi + b) / 12.0 + valor (O1Fi + b) / 1200.0 + mod[DTo1 + o]);
            cnt[o] = (motor[o] == MGranular || motor[o] == MSoroll) ? 1 : nu;
            float sumaG = 0.0f;
            for (int k = 0; k < cnt[o]; ++k)
            {
                const float x = cnt[o] == 1 ? 0.0f : (float) k / (cnt[o] - 1) * 2.0f - 1.0f;
                double semU = 0.0;
                float g = 1.0f;
                switch (uMode)
                {
                    case 0: semU = x * uDet * uDet * 0.8; g = std::abs (x) < 0.01f ? 1.0f : uBlend; break;
                    case 1: semU = jp[k] * (0.05 + uDet * 0.6); g = k == 0 ? 1.0f : uBlend; break;
                    case 2: semU = octavesU[k] + (k > 0 ? (k % 2 ? 1 : -1) * uDet * 0.15 : 0.0); g = k == 0 ? 1.0f : uBlend * (std::abs (octavesU[k]) > 13 ? 0.5f : 0.8f); break;
                    default: semU = acordU[k] + (k > 0 ? (k % 2 ? 1 : -1) * uDet * 0.12 : 0.0); g = k == 0 ? 1.0f : uBlend * 0.7f; break;
                }
                if (cnt[o] == 1) { semU = 0.0; g = 1.0f; }
                dt[o][k] = std::min (0.45, hz * std::pow (2.0, semU / 12.0) / fsI);
                const float p = std::clamp (panO[o] + (cnt[o] == 1 ? 0.0f : (uMode == 1 ? jp[k] : x) * uAmp) + mod[DPan], -1.0f, 1.0f);
                gL[o][k] = g * std::sqrt (0.5f * (1.0f - p)) * 1.4142f;
                gR[o][k] = g * std::sqrt (0.5f * (1.0f + p)) * 1.4142f;
                sumaG += g * g;
            }
            const float nrm = 1.0f / std::sqrt (std::max (1.0f, sumaG));
            for (int k = 0; k < cnt[o]; ++k) { gL[o][k] *= nrm; gR[o][k] *= nrm; }
            if ((cnt[o] > 1 && uAmp > 0.001f) || std::abs (panO[o]) > 0.01f || std::abs (mod[DPan]) > 0.01f) estereo = estereo || niv[o] > 0.0f;
            dtM[o] = ratiosFm[std::clamp ((int) std::lround (forma[o] * 11.0f), 0, 11)];
            if (motor[o] == MHarmonic)
            {
                const float e = 2.4f - 2.2f * forma[o];
                const float pOdd = std::min (1.0f, 2.0f * (1.0f - pos[o])), pEven = std::min (1.0f, 2.0f * pos[o]);
                float suma = 0.0f;
                int np = 0;
                for (int h = 1; h <= parcials; ++h)
                {
                    if (h * dt[o][0] > 0.45) break;
                    float a = std::pow ((float) h, -e);
                    if (h > 1) a *= (h % 2 ? pOdd : pEven);
                    amps[o][h - 1] = a;
                    suma += a;
                    np = h;
                }
                nParc[o] = np;
                for (int h = 0; h < np; ++h) amps[o][h] *= 1.3f / std::max (0.2f, suma);
            }
        }
        // granular
        const float dens = pm (GrDens, mod, DGrDens), durGra = pm (GrMida, mod, DGrMida) / 1000.0f, spray = std::clamp (pm (GrSpray, mod, DGrSpray), 0.0f, 1.0f);
        float normGra = 1.0f;
        for (int o = 0; o < 3; ++o)
        {
            if (motor[o] != MGranular || niv[o] <= 0.0f) continue;
            const float d = dens * (0.4f + 1.2f * forma[o]);
            v.finsGra[o] -= (float) dtS;
            int nous = 0;
            while (v.finsGra[o] <= 0.0f && nous < 4)
            {
                v.finsGra[o] += (1.0f / std::max (1.0f, d)) * (0.6f + 0.8f * (0.5f + 0.5f * aleat()));
                ++nous;
                for (auto& g : v.grans[o])
                    if (! g.viu)
                    {
                        g.viu = true;
                        g.fase = 0.5 + 0.5 * aleat();
                        g.dt = std::min (0.45, dt[o][0] * std::pow (2.0, aleat() * spray * 30.0 / 1200.0));
                        g.pos = std::clamp (pos[o] + spray * aleat() * 0.5f, 0.0f, 1.0f);
                        g.t = 0.0f;
                        g.durada = durGra * (0.8f + 0.4f * (0.5f + 0.5f * aleat()));
                        g.pan = std::clamp (panO[o] + aleat() * std::max (spray, uAmp), -1.0f, 1.0f);
                        break;
                    }
            }
            normGra = 1.0f / std::sqrt (std::max (1.0f, d * durGra * 0.5f));
            estereo = true;
        }
        const float ring = valor (Ring) * 0.5f;
        const double dSub = std::min (0.45, hzNota * (valor (SubOct) > 0.5f ? 0.25 : 0.5) / fsI);
        const float nSub = valor (SubNiv) * 0.45f;
        const float nz = std::clamp (pm (NzNiv, mod, DSoroll), 0.0f, 1.0f) * 0.4f, nzC = 1.0f - 0.97f * valor (NzColor);
        const float fold = std::clamp (pm (Fold, mod, DFold), 0.0f, 1.0f);
        const int dtp = (int) valor (DTipus);
        const float dq = std::clamp (pm (DQuant, mod, DDrive), 0.0f, 1.0f);
        const float dA = 1.0f + dq * dq * 30.0f, dMake = 1.0f / std::pow (dA, 0.4f);

        // envolupants
        auto cA = [&] (int p) { return coef (valor (p) / 1000.0 / 1.609, fsI); };
        auto cDR = [&] (int p) { return coef (valor (p) / 1000.0 / 4.0, fsI); };
        const float caA = cA (AA), cdA = cDR (AD), crA = cDR (AR), sA = valor (AS);
        const float caF = cA (FA), cdF = cDR (FD), crF = cDR (FR), sF = valor (FS);
        const float caE = cA (EA), cdE = cDR (ED), crE = cDR (ER), sE = valor (ES);
        const float crRapid = coef (0.004, fsI);

        // filtre
        const int tipus = (int) valor (FTipus);
        const bool p24 = valor (FPendent) > 0.5f;
        const double oct = valor (FEnv) * 7.0 * v.envF + valor (FKey) * (semis - 60.0) / 12.0 + mod[DTall] * 6.0;
        const double fc = std::clamp (valor (FTall) * std::pow (2.0, oct), 20.0, std::min (20000.0, fsI * 0.45));
        const float res = std::clamp (pm (FRes, mod, DRes), 0.0f, 1.0f);
        const float g = (float) std::tan (dpi * fc / fsI);
        const float k0 = 2.0f - 1.97f * res, kb = 1.3f;
        const float A1 = 1.0f / (1.0f + g * (g + k0)), A2 = g * A1, A3 = g * A2;
        const float B1 = 1.0f / (1.0f + g * (g + kb)), B2 = g * B1, B3 = g * B2;
        const int N = p24 ? 4 : 2;
        const float kl = res * (N == 4 ? 4.1f : 2.6f);
        const float G = g / (1.0f + g), GN = N == 4 ? G * G * G * G : G * G;
        const float fDrv = 1.0f + 3.0f * valor (FDrive);
        // formant
        float fmG[3] {}, fmA[3][3] {};
        if (tipus == 5)
        {
            const float vq = std::clamp (pm (FVocal, mod, DVocal), 0.0f, 1.0f) * 4.0f;
            const int a = std::min (3, (int) vq);
            const float kk = vq - a;
            const float mida = (float) std::clamp (fc / 1000.0, 0.5, 2.0);
            const float q = 0.25f + res * 0.6f;
            for (int j = 0; j < 3; ++j)
            {
                const float ff = std::min ((float) (fsI * 0.4), (vocals[a][j] * (1.0f - kk) + vocals[a + 1][j] * kk) * mida);
                const float gg = (float) std::tan (dpi * ff / fsI);
                const float kk2 = 2.0f - 1.9f * q - (j == 0 ? 0.0f : 0.05f);
                fmA[j][0] = 1.0f / (1.0f + gg * (gg + kk2)); fmA[j][1] = gg * fmA[j][0]; fmA[j][2] = gg * fmA[j][1];
                fmG[j] = j == 0 ? 1.0f : j == 1 ? 0.7f : 0.4f;
            }
        }
        // comb
        const double combD = std::clamp (fsI / fc, 2.0, (double) v.comb[0].size() - 4.0);
        const float combG = (tipus == 6 ? 1.0f : -1.0f) * (0.3f + 0.67f * res);
        // ressonadors
        const float rsMix = std::clamp (pm (RsMix, mod, DRess), 0.0f, 1.0f);
        float rsC[3][3] {};
        if (rsMix > 0.001f)
        {
            const int mat = (int) valor (RsTipus);
            const float k = 0.02f + (1.0f - valor (RsDecay)) * 0.25f;
            for (int j = 0; j < 3; ++j)
            {
                const double ff = std::min (fsI * 0.4, hzNota * relRs[mat][j]);
                const float gg = (float) std::tan (dpi * ff / fsI);
                rsC[j][0] = 1.0f / (1.0f + gg * (gg + k)); rsC[j][1] = gg * rsC[j][0]; rsC[j][2] = gg * rsC[j][1];
            }
        }

        float vca = 1.0f - valor (VelVol) * (1.0f - v.vel);
        vca *= std::clamp (1.0f + mod[DVol], 0.0f, 2.0f);
        float suma3 = 0.0f;

        auto gen = [&] (int o, int k, double pmod, double dtMult) noexcept
        {
            double& ph = v.ph[o][k];
            const double d = dt[o][k] * dtMult;
            double q = ph;
            const float w = wq[o];
            switch (warp[o])
            {
                case WCorba: { const double x = 0.5 - 0.48 * w; q = q < x ? 0.5 * q / x : 0.5 + 0.5 * (q - x) / (1.0 - x); break; }
                case WSync: q = q * (1.0 + w * 7.0); q -= std::floor (q); break;
                case WMirall: q = (q < 0.5 ? q : 1.0 - q) * 2.0 * (1.0 + w * 3.0); q -= std::floor (q); break;
                case WBits: { const double st = 2.0 + (1.0 - w) * 62.0; q = std::floor (q * st) / st; break; }
                default: break;
            }
            if (pmod != 0.0) { q += pmod; q -= std::floor (q); }
            const bool net = warp[o] == WCap && pmod == 0.0;
            float out;
            switch (motor[o])
            {
                case MWavetable: out = llegeixTaula (taula[o], q, pos[o], d * std::pow (2.0, (0.5 - forma[o]) * 6.0)); break;
                case MAnalogic:
                {
                    const float fm = forma[o];
                    const float serra = (float) (2.0 * q - 1.0) - (net ? blep (q, d) : 0.0f);
                    if (fm < 0.33f)
                    {
                        const float tri = 1.0f - 4.0f * std::abs ((float) q - 0.5f);
                        const float kk = fm / 0.33f;
                        out = tri * (1.0f - kk) + serra * kk;
                    }
                    else
                    {
                        const double pw = fm < 0.66f ? 0.5 : 0.5 - 0.44 * (fm - 0.66) / 0.34 - 0.0 * pos[o];
                        const double pwE = std::clamp (pw - (pos[o] - 0.2) * 0.4, 0.04, 0.96);
                        float pul = q < pwE ? 1.0f : -1.0f;
                        if (net) { pul += blep (q, d); double r = q + 1.0 - pwE; if (r >= 1.0) r -= 1.0; pul -= blep (r, d); }
                        const float kk = fm < 0.66f ? (fm - 0.33f) / 0.33f : 1.0f;
                        out = serra * (1.0f - kk) + pul * kk;
                    }
                    break;
                }
                case MHarmonic:
                {
                    // sin (h·x) per recurrència: s(h+1) = 2·cos(x)·s(h) − s(h−1)
                    const float s1 = sinT (q), c2 = 2.0f * sinT (q + 0.25);
                    float sa = 0.0f, sb = s1;
                    out = 0.0f;
                    for (int h = 0; h < nParc[o]; ++h) { out += amps[o][h] * sb; const float sn = c2 * sb - sa; sa = sb; sb = sn; }
                    break;
                }
                case MFm:
                {
                    double& pmd = v.phM[o][k];
                    const float idx = pos[o] * 8.0f;
                    out = sinT (q + idx * 0.159f * sinT (pmd));
                    pmd += d * dtM[o]; pmd -= std::floor (pmd);
                    break;
                }
                default:   // soroll (el granular va a part)
                {
                    v.soroll += (aleat() - v.soroll) * (1.0f - 0.97f * (1.0f - forma[o]));
                    out = v.soroll * (1.0f + forma[o] * 0.0f) * (1.6f - forma[o]);
                    if (pos[o] > 0.01f && aleat() > 1.0f - pos[o] * 0.004f) v.cruix = aleat() > 0.0f ? 1.0f : -1.0f;
                    out += v.cruix; v.cruix *= 0.92f;
                    break;
                }
            }
            ph += d;
            if (ph >= 1.0) ph -= std::floor (ph);
            return out;
        };

        for (int i = s0; i < fi; ++i)
        {
            switch (v.etapaA)
            {
                case 1: v.envA += (1.25f - v.envA) * caA; if (v.envA >= 1.0f) { v.envA = 1.0f; v.etapaA = 2; } break;
                case 2: v.envA += (sA - v.envA) * cdA; break;
                case 3: v.envA += (0.0f - v.envA) * crA; break;
                case 4: v.envA += (0.0f - v.envA) * crRapid; break;
                default: break;
            }
            switch (v.etapaF)
            {
                case 1: v.envF += (1.25f - v.envF) * caF; if (v.envF >= 1.0f) { v.envF = 1.0f; v.etapaF = 2; } break;
                case 2: v.envF += (sF - v.envF) * cdF; break;
                case 3: v.envF += (0.0f - v.envF) * crF; break;
                default: break;
            }
            switch (v.etapaE)
            {
                case 1: v.envE += (1.25f - v.envE) * caE; if (v.envE >= 1.0f) { v.envE = 1.0f; v.etapaE = 2; } break;
                case 2: v.envE += (sE - v.envE) * cdE; break;
                case 3: v.envE += (0.0f - v.envE) * crE; break;
                default: break;
            }
            float aL = 0.0f, aR = 0.0f;
            float mo[3] = { 0.0f, 0.0f, 0.0f };
            const float o3ant = v.darrera[2];
            // ordre: 3, 1, 2 (el 3 pot modular l'1 a ritme d'àudio; cada un modula el següent amb PM/FM)
            static const int ordre[3] = { 2, 0, 1 };
            for (int oi = 0; oi < 3; ++oi)
            {
                const int o = ordre[oi];
                const bool calModul = o == 2 && (aPm[0] != 0.0f || aPm[1] != 0.0f || aVol != 0.0f || aPos[0] != 0.0f || aPos[1] != 0.0f || warp[0] >= WPm);
                if (niv[o] <= 0.0f && ! calModul && ! (o == 0 && (ring > 0.0f || warp[1] >= WPm)) && ! (o == 1 && warp[2] >= WPm)) continue;
                const int fontW = o == 0 ? 2 : o == 1 ? 0 : 1;
                const float modulador = o == 2 ? v.darrera[1] : mo[fontW];
                double pmod = (warp[o] == WPm ? wq[o] * 1.5 * modulador : 0.0) + aPm[o] * 0.5 * o3ant;
                const double dtMult = warp[o] == WFm ? std::max (0.0, 1.0 + wq[o] * 4.0 * modulador) : 1.0;
                const float posGuarda = pos[o];
                if (aPos[o] != 0.0f) pos[o] = std::clamp (pos[o] + aPos[o] * o3ant, 0.0f, 1.0f);
                float suma = 0.0f;
                if (motor[o] == MGranular)
                {
                    for (auto& gr : v.grans[o])
                    {
                        if (! gr.viu) continue;
                        const float x = gr.t / gr.durada;
                        if (x >= 1.0f) { gr.viu = false; continue; }
                        const float w = 0.5f - 0.5f * sinT (0.25 + x);
                        const float s = llegeixTaula (taula[o], gr.fase, gr.pos, gr.dt * std::pow (2.0, (0.5 - forma[o]) * 4.0)) * w * normGra;
                        gr.fase += gr.dt; if (gr.fase >= 1.0) gr.fase -= 1.0;
                        gr.t += (float) (1.0 / fsI);
                        const float glg = std::sqrt (0.5f * (1.0f - gr.pan)) * 1.4142f, grg = std::sqrt (0.5f * (1.0f + gr.pan)) * 1.4142f;
                        aL += s * niv[o] * glg; aR += s * niv[o] * grg;
                        suma += s;
                    }
                }
                else
                    for (int k = 0; k < cnt[o]; ++k)
                    {
                        const float x = gen (o, k, pmod, dtMult);
                        suma += x / cnt[o];
                        aL += x * niv[o] * gL[o][k]; aR += x * niv[o] * gR[o][k];
                    }
                pos[o] = posGuarda;
                mo[o] = suma;
                v.darrera[o] = suma;
            }
            suma3 += mo[2];
            float extra = mo[0] * mo[1] * ring;
            if (nSub > 0.0f)
            {
                extra += sinT (v.fSub) * nSub;
                v.fSub += dSub; if (v.fSub >= 1.0) v.fSub -= 1.0;
            }
            if (nz > 0.0f) { v.soroll += (aleat() - v.soroll) * nzC; extra += v.soroll * nz * (1.0f + (1.0f - nzC) * 2.0f); }
            float xL = (estereo ? aL : 0.5f * (aL + aR)) + extra, xR = (estereo ? aR : 0.5f * (aL + aR)) + extra;
            float y[2];
            float monoRs = 0.0f;
            for (int c = 0; c < (estereo ? 2 : 1); ++c)
            {
                float x = c == 0 ? xL : xR;
                // plec i distorsió (abans del filtre: el filtre arrodoneix)
                if (fold > 0.001f) x = x * (1.0f - fold) + std::sin (std::clamp (x * (1.0f + fold * 6.0f), -20.0f, 20.0f) * 0.5f * pi) * fold * 0.8f;
                if (dq > 0.001f)
                {
                    const float xa = x * dA;
                    float yd;
                    switch (dtp)
                    {
                        case 0: yd = tanhR (xa); break;
                        case 1: { const float z = xa + 0.2f; yd = (z > 0.0f ? tanhR (z) : tanhR (z * 0.6f) * 0.7f / 0.6f) - 0.15f; break; }
                        case 2: { const float t = tanhR (xa * 1.5f); yd = std::clamp (0.6f * t + 0.6f * std::abs (t) - 0.25f, -1.0f, 1.0f); break; }
                        default: yd = tanhR (std::abs (xa) * 1.2f) * 1.4f - 0.5f; break;
                    }
                    x = yd * dMake * 1.5f;
                }
                x = tanhR (x * fDrv * 0.7f) * 1.4286f + 1e-20f;
                float out = x;
                auto svf = [] (Svf& s, float in, float a1, float a2, float a3, float& v1o) noexcept
                {
                    const float v3 = in - s.a2;
                    const float v1 = a1 * s.a1 + a2 * v3, v2 = s.a2 + a2 * s.a1 + a3 * v3;
                    s.a1 = 2.0f * v1 - s.a1; s.a2 = 2.0f * v2 - s.a2;
                    v1o = v1;
                    return v2;
                };
                switch (tipus)
                {
                    case 4:   // ladder
                    {
                        float S = 0.0f;
                        for (int st = 0; st < N; ++st) S = S * G + (1.0f - G) * v.ld[c][st];
                        const float yN = (GN * x + S) / (1.0f + kl * GN);
                        float u = tanhR (x - kl * yN);
                        for (int st = 0; st < N; ++st) { const float vv = (u - v.ld[c][st]) * G; const float yy = vv + v.ld[c][st]; v.ld[c][st] = yy + vv; u = yy; }
                        out = u * (1.0f + kl * 0.28f);
                        break;
                    }
                    case 5:   // formant
                    {
                        out = 0.0f;
                        for (int j = 0; j < 3; ++j) { float b1; svf (v.fmt[c][j], x, fmA[j][0], fmA[j][1], fmA[j][2], b1); out += b1 * fmG[j] * 1.6f; }
                        break;
                    }
                    case 6: case 7:   // comb
                    {
                        auto& cb = v.comb[c];
                        const int M = (int) cb.size();
                        double rp = v.combPos - combD;
                        while (rp < 0.0) rp += M;
                        const int a = (int) rp, b = (a + 1) % M;
                        const float kk = (float) (rp - a);
                        const float d = cb[(size_t) a] + (cb[(size_t) b] - cb[(size_t) a]) * kk;
                        const float yv = x + combG * d;
                        cb[(size_t) v.combPos] = tanhR (yv);
                        out = yv * (0.6f - 0.25f * res);
                        if (c == (estereo ? 1 : 0)) v.combPos = (v.combPos + 1) % M;
                        break;
                    }
                    default:
                    {
                        for (int st = 0; st < (p24 ? 2 : 1); ++st)
                        {
                            const float a1 = st == 0 ? A1 : B1, a2 = st == 0 ? A2 : B2, a3 = st == 0 ? A3 : B3, kk = st == 0 ? k0 : kb;
                            float bp;
                            const float in = out;
                            const float lp = svf (v.fl[c][st], in, a1, a2, a3, bp);
                            out = tipus == 0 ? lp : tipus == 1 ? in - kk * bp - lp : tipus == 2 ? bp * (st == 0 ? 1.0f : 1.4f) : in - kk * bp;
                        }
                        break;
                    }
                }
                y[c] = out;
                monoRs += out;
            }
            if (! estereo) y[1] = y[0];
            if (rsMix > 0.001f)
            {
                const float in = monoRs / (estereo ? 2.0f : 1.0f);
                float r = 0.0f;
                for (int j = 0; j < 3; ++j) { float bp; { const float v3 = in - v.rs[j].a2; const float v1 = rsC[j][0] * v.rs[j].a1 + rsC[j][1] * v3; const float v2 = v.rs[j].a2 + rsC[j][1] * v.rs[j].a1 + rsC[j][2] * v3; v.rs[j].a1 = 2.0f * v1 - v.rs[j].a1; v.rs[j].a2 = 2.0f * v2 - v.rs[j].a2; bp = v1; } r += bp * (j == 0 ? 1.0f : 0.6f); }
                r = tanhR (r * 0.5f) * 1.4f;
                y[0] = y[0] * (1.0f - rsMix * 0.6f) + r * rsMix;
                y[1] = y[1] * (1.0f - rsMix * 0.6f) + r * rsMix;
            }
            float a = v.envA * vca;
            if (aVol != 0.0f) a *= std::clamp (1.0f + aVol * o3ant, 0.0f, 2.0f);
            bufL[i] += y[0] * a;
            bufR[i] += y[1] * a;
        }
        v.osc3Mitja = suma3 / std::max (1, m);
        if (laMesNova) for (int o = 0; o < 3; ++o) posV[(size_t) o].store (pos[o], std::memory_order_relaxed);
        if ((v.etapaA == 3 || v.etapaA == 4) && v.envA < 1e-5f) { v.viva = false; v.etapaA = 0; v.envA = 0.0f; }
    }

    void Neo::efectes (float* L, float* R, int n) noexcept
    {
        auto pf = [&] (int p, int d) { return pm (p, modFx, d); };
        // crush
        const float crm = std::clamp (pf (CrMix, DCrush), 0.0f, 1.0f);
        if (crm > 0.001f)
        {
            const float bits = valor (CrBits), hold = valor (CrMostreig);
            const float q = std::pow (2.0f, bits - 1.0f);
            for (int i = 0; i < n; ++i)
            {
                crPhase += 1.0f;
                if (crPhase >= hold) { crPhase -= hold; crL = std::round (L[i] * q) / q; crR = std::round (R[i] * q) / q; }
                L[i] += (crL - L[i]) * crm;
                R[i] += (crR - R[i]) * crm;
            }
        }
        // desplaçador de freqüència (Hilbert)
        const float shm = valor (ShMix);
        if (shm > 0.001f)
        {
            const double hz = pf (ShHz, DShift);
            const double dph = hz / fs;
            auto hilbert = [] (Hilbert& h, float x, float& re, float& im) noexcept
            {
                float a = x, b = x;
                for (int k = 0; k < 4; ++k)
                {
                    const float ca = (float) (hbA[k] * hbA[k]), cb = (float) (hbB[k] * hbB[k]);
                    const float ya = ca * (a + h.y2[k]) - h.x2[k];
                    h.x2[k] = h.x1[k]; h.x1[k] = a; h.y2[k] = h.y1[k]; h.y1[k] = ya; a = ya;
                    const float yb = cb * (b + h.y2[k + 4]) - h.x2[k + 4];
                    h.x2[k + 4] = h.x1[k + 4]; h.x1[k + 4] = b; h.y2[k + 4] = h.y1[k + 4]; h.y1[k + 4] = yb; b = yb;
                }
                re = a;
                im = h.retard;
                h.retard = b;
            };
            for (int i = 0; i < n; ++i)
            {
                shFase += dph; shFase -= std::floor (shFase);
                const float c = sinT (shFase + 0.25), s = sinT (shFase);
                float re, im;
                hilbert (hbL, L[i], re, im);
                const float yl = re * c - im * s;
                hilbert (hbR, R[i], re, im);
                const float yr = re * c - im * s;
                L[i] += (yl - L[i]) * shm;
                R[i] += (yr - R[i]) * shm;
            }
        }
        // chorus
        const float chm = std::clamp (pf (ChMix, DChorus), 0.0f, 1.0f);
        if (chm > 0.001f && ! chL.empty())
        {
            const int Nn = (int) chL.size();
            const double dph = valor (ChVel) / fs;
            const float prof = valor (ChProf) * 0.004f * (float) fs, base = 0.008f * (float) fs;
            for (int i = 0; i < n; ++i)
            {
                chL[(size_t) chPos] = L[i];
                chR[(size_t) chPos] = R[i];
                chFase += dph; if (chFase >= 1.0) chFase -= 1.0;
                const float dLs = base + prof * (0.5f + 0.5f * sinT (chFase)), dRs = base + prof * (0.5f + 0.5f * sinT (chFase + 0.25));
                auto llegeix = [&] (const std::vector<float>& b, float d)
                {
                    float x = (float) chPos - d;
                    while (x < 0.0f) x += (float) Nn;
                    int a = (int) x;
                    if (a >= Nn) a -= Nn;
                    const int c = (a + 1) % Nn;
                    const float k = x - std::floor (x);
                    return b[(size_t) a] + (b[(size_t) c] - b[(size_t) a]) * k;
                };
                const float wl = llegeix (chL, dLs), wr = llegeix (chR, dRs);
                L[i] = L[i] * (1.0f - 0.45f * chm) + wr * 0.75f * chm;
                R[i] = R[i] * (1.0f - 0.45f * chm) + wl * 0.75f * chm;
                chPos = (chPos + 1) % Nn;
            }
        }
        // delay
        const float dm = std::clamp (pf (DlMix, DDelay), 0.0f, 1.0f);
        if (dm > 0.001f)
        {
            const float vv[4] = { (float) notaDelay[std::clamp ((int) valor (DlNota), 0, 7)], valor (DlFeedback) * 100.0f, dm * 100.0f, valor (DlPingPong) };
            if (vv[0] != darrerDl[0]) dl.posa (delay::Nota, vv[0]);
            if (vv[1] != darrerDl[1]) dl.posa (delay::Feedback, vv[1]);
            if (std::abs (vv[2] - darrerDl[2]) > 0.2f) dl.posa (delay::Mix, vv[2]);
            if (vv[3] != darrerDl[3]) dl.posa (delay::EncaminamentP, vv[3] > 0.5f ? (float) delay::PingPong : (float) delay::Estereo);
            for (int k = 0; k < 4; ++k) darrerDl[k] = vv[k];
            dl.processa (L, R, n);
        }
        // reverb
        const float rm = std::clamp (pf (RvMix, DReverb), 0.0f, 1.0f);
        if (rm > 0.001f)
        {
            static const int algs[] = { reverb::Plate, reverb::Hall, reverb::Ambient, reverb::Shimmer };
            const int t = std::clamp ((int) valor (RvTipus), 0, 3);
            const float mida = valor (RvMida);
            if ((float) t != darrerRv[2]) { rv.posa (reverb::Alg, (float) algs[t]); darrerRv[2] = (float) t; darrerRv[0] = -1.0f; }
            if (mida != darrerRv[0]) { rv.posa (reverb::Decay, 0.6f + mida * mida * 8.0f); rv.posa (reverb::Size, 30.0f + 70.0f * mida); darrerRv[0] = mida; }
            if (std::abs (rm - darrerRv[1]) > 0.002f) { rv.posa (reverb::Mix, rm * 100.0f); darrerRv[1] = rm; }
            rv.processa (L, R, n);
        }
        // espectre: inclinació i excitador
        const float tilt = std::clamp (pf (EsTilt, DEspectre), -1.0f, 1.0f), exc = valor (EsExcita);
        if (std::abs (tilt) > 0.01f || exc > 0.001f)
        {
            const float c1 = (float) (1.0 - std::exp (-2.0 * dpi * 900.0 / fs)), c2 = (float) (1.0 - std::exp (-2.0 * dpi * 3500.0 / fs));
            const float gb = 1.0f - tilt * 0.45f, ga = 1.0f + tilt * 0.9f;
            auto fes = [&] (float x, float* t, float& e) noexcept
            {
                t[0] += (x - t[0]) * c1;
                const float lo = t[0], hi = x - lo;
                float y = lo * gb + hi * ga;
                if (exc > 0.001f) { t[1] += (x - t[1]) * c2; const float h2 = x - t[1]; e = tanhR (h2 * 6.0f) * 0.25f; y += e * exc; }
                return y;
            };
            for (int i = 0; i < n; ++i) { L[i] = fes (L[i], tiltL, excL); R[i] = fes (R[i], tiltR, excR); }
        }
    }

    //==========================================================================
    float previsualitza (const std::string& estat, float* sortida, int n, int nota)
    {
        auto sp = std::make_unique<Neo>();
        auto& s = *sp;
        s.posaEstat (estat.empty() ? estatInicial() : estat);
        for (int p : { (int) Volum, (int) Ample, (int) ChMix, (int) DlMix, (int) RvMix, (int) ArpMode, (int) ShMix, (int) Mac6 })
            s.posa (p, 0.0f);
        for (int k = 0; k < numSlots; ++k) { const int f = (int) s.valor (M1Font + k * 4); if (f >= FLfo1 && f <= FLfo4) s.posa (M1Quant + k * 4, 0.0f); }
        const double fs = 48000.0;
        s.prepara (fs, 512);
        EventInstrument on { 0, 0x90, (std::uint8_t) nota, 110 };
        std::vector<float> L (256), R (256);
        const int espera = (int) (fs * std::clamp ((double) s.valor (AA) / 1000.0 + 0.06, 0.06, 0.4));
        s.processa (&on, 1, L.data(), R.data(), 256);
        int fet = 256;
        while (fet < espera) { s.processa (nullptr, 0, L.data(), R.data(), 256); fet += 256; }
        const double hz = 440.0 * std::pow (2.0, (nota - 69) / 12.0);
        const int periode = (int) std::lround (fs / hz);
        std::vector<float> buf;
        for (int k = 0; k < 4 * periode / 256 + 2; ++k) { s.processa (nullptr, 0, L.data(), R.data(), 256); buf.insert (buf.end(), L.begin(), L.end()); }
        int ini = 0;
        for (int i = 1; i < periode && i + 1 < (int) buf.size(); ++i) if (buf[(size_t) i - 1] < 0.0f && buf[(size_t) i] >= 0.0f) { ini = i; break; }
        float pic = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const double x = ini + (double) i / std::max (1, n - 1) * 2.0 * periode;
            const int a = std::min ((int) buf.size() - 2, (int) x);
            const float t = (float) (x - a);
            sortida[i] = buf[(size_t) a] * (1.0f - t) + buf[(size_t) a + 1] * t;
            pic = std::max (pic, std::abs (sortida[i]));
        }
        return pic;
    }
}
