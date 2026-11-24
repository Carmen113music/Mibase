#include "mibase/Atmos.h"
#include "mibase/FFT.h"
#include "mibase/Lead.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>

namespace mibase::atmos
{
    using sinte::tanhR; using sinte::blep; using sinte::coef; using sinte::num; using sinte::ms;
    using sinte::Lineal; using sinte::Enter; using sinte::Log; using sinte::Cub;

    namespace
    {
        constexpr float pi = 3.14159265358979f;
        constexpr double dpi = 3.14159265358979323846;
        const char* const ones = "Sinus|Triangle|Serra|Quadrada|Pols|Soroll|Wavetable|Granular";
        const char* const taules = "Bàsica|PWM|Vocal|Harmònics|Digital|Campana|Sync|Metall|Cor|Cordes|Vidre|Orgue|Aire|Fosc|Octaves|Formant";
        const char* const onesLfo = "Sinus|Triangle|Serra|Quadrada|Atzar|Atzar suau";
        const char* const fonts = "Cap|LFO 1|LFO 2|LFO 3|Env filtre|Evolució|Velocitat|Nota|Roda|Aftertouch|Atzar per nota|Env volum";
        const char* const destins = "To|Tall|Ressonància|Mescla A/B|Forma 1|Forma 2|Forma 3|Forma 4|Totes les formes|FM|Detune|Pan|Volum|Nivell 1|Nivell 2|Nivell 3|Nivell 4|Grànuls|Textura";
        const char* const noSi = "No|Sí";
        const char* const notesDelay = "1/16|1/8T|1/8|1/8.|1/4T|1/4|1/4.|1/2";
        const int notaDelay[] = { 2, 3, 5, 7, 6, 8, 10, 11 };   // índex de delay::nomNota

        #define OSC(n, w, o, f, sh, t, nv) \
            { "o" #n "w", "Ona", SOsc, 0, 7, w, Enter, ones, "Forma d'ona de l'oscil·lador " #n " (Granular: núvol de grànuls de la wavetable)" }, \
            { "o" #n "o", "Octava", SOsc, -3, 3, o, Enter, nullptr, "Octava de l'oscil·lador " #n }, \
            { "o" #n "s", "Semitò", SOsc, -12, 12, 0, Enter, nullptr, "Semitons de l'oscil·lador " #n " (7 = quinta, 12 = octava)" }, \
            { "o" #n "f", "Fi", SOsc, -100, 100, f, Lineal, nullptr, "Afinació fina de l'oscil·lador " #n " (centèsimes): el batec amb els altres" }, \
            { "o" #n "sh", "Forma", SOsc, 0, 1, sh, Lineal, nullptr, "Wavetable i granular: posició dins la taula (el timbre). Pols: amplada. Serra: brillantor. Soroll: color" }, \
            { "o" #n "t", "Taula", SOsc, 0, 15, t, Enter, taules, "Wavetable de l'oscil·lador " #n " (també la font dels grànuls)" }, \
            { "o" #n "n", "Nivell", SOsc, 0, 1, nv, Lineal, nullptr, "Volum de l'oscil·lador " #n }, \
            { "o" #n "p", "Pan", SOsc, -1, 1, 0, Lineal, nullptr, "On sona l'oscil·lador " #n " (esquerra – dreta)" }

        #define LFO(n, vel) \
            { "l" #n "w", "Ona", SLfo, 0, 5, 0, Enter, onesLfo, "Forma de l'LFO " #n " (Atzar suau: moviment orgànic, sense salts)" }, \
            { "l" #n "r", "Velocitat", SLfo, 0.01f, 20, vel, Log, nullptr, "Velocitat de l'LFO " #n " (molt lenta = el so canvia a poc a poc)" }, \
            { "l" #n "s", "Tempo", SLfo, 0, 11, 0, Enter, sinte::ritmesLfo, "LFO " #n " lliure o amb el tempo del projecte" }, \
            { "l" #n "ph", "Fase", SLfo, 0, 1, 0, Enter, "Global|Per veu", "Global: totes les notes es mouen juntes. Per veu: cada nota té el seu moviment (més ric)" }, \
            { "l" #n "in", "Entrada", SLfo, 0, 10000, 0, Cub, nullptr, "L'LFO " #n " entra a poc a poc després de la nota" }

        #define MOD(n, f, d, q) \
            { "m" #n "f", "Font", SMod, 0, 11, f, Enter, fonts, "Què mou la via " #n " de la matriu" }, \
            { "m" #n "d", "Destí", SMod, 0, 18, d, Enter, destins, "Què es mou (via " #n ")" }, \
            { "m" #n "q", "Quantitat", SMod, -1, 1, q, Lineal, nullptr, "Quant i cap a on (via " #n ")" }

        const sinte::InfoParam infos[numParams] = {
            OSC (1, 2, 0, 0, 0.0f, 0, 0.7f),
            OSC (2, 2, 0, 7, 0.0f, 0, 0.6f),
            OSC (3, 6, 0, -4, 0.3f, 8, 0.0f),
            OSC (4, 0, -1, 0, 0.0f, 0, 0.0f),
            { "uni", "Unison", SOsc, 1, 5, 3, Enter, nullptr, "Còpies de cada oscil·lador (no del soroll ni dels grànuls)" },
            { "unid", "Detune", SOsc, 0, 1, 0.3f, Lineal, nullptr, "Desafinació de les còpies: més = més gruixut i més mogut" },
            { "unia", "Obertura", SOsc, 0, 1, 0.7f, Lineal, nullptr, "Com s'obren les còpies a esquerra i dreta" },
            { "fm21", "FM 2→1", SOsc, 0, 1, 0, Lineal, nullptr, "L'oscil·lador 2 modula la fase de l'1 (campanes, vidre, digital)" },
            { "fm43", "FM 4→3", SOsc, 0, 1, 0, Lineal, nullptr, "L'oscil·lador 4 modula la fase del 3" },
            { "mix", "Mescla A/B", SOsc, 0, 1, 0.5f, Lineal, nullptr, "Parella A (osc 1 + 2) ↔ parella B (osc 3 + 4). Al mig, totes dues. Movent-la (evolució, LFO), el timbre canvia" },
            { "lliure", "Fase lliure", SOsc, 0, 1, 1, Enter, noSi, "Sí: cada nota comença amb les fases a l'atzar (analògic, viu)" },
            { "grd", "Densitat", SText, 2, 80, 20, Log, nullptr, "Grànuls per segon dels oscil·ladors granulars" },
            { "grm", "Mida gra", SText, 10, 500, 120, Log, nullptr, "Durada de cada gra (curts: textura; llargs: núvol suau)" },
            { "grs", "Dispersió", SText, 0, 1, 0.3f, Lineal, nullptr, "Com s'escampen els grànuls per la taula (timbre)" },
            { "grt", "Afinació gra", SText, 0, 1, 0.1f, Lineal, nullptr, "Desafinació a l'atzar de cada gra (fins a mig semitò)" },
            { "gro", "Octaves gra", SText, 0, 3, 0, Enter, "No|+1 oct|±1 oct|Quintes", "Alguns grànuls salten d'octava o de quinta (brillantor, shimmer)" },
            { "nz", "Textura", SText, 0, 1, 0, Lineal, nullptr, "Volum de la capa de textura (soroll)" },
            { "nzc", "Color", SText, 0, 1, 0.5f, Lineal, nullptr, "Color de la textura: brillant (0) o fosc (1)" },
            { "nzt", "Tipus", SText, 0, 3, 0, Enter, "Blanc|Vent|Cruixit|Respiració", "Blanc: aire. Vent: soroll que bufa i es mou. Cruixit: com un vinil. Respiració: buf que puja i baixa" },
            { "nzm", "Moviment", SText, 0, 1, 0.4f, Lineal, nullptr, "Com es mou la textura (vent, cruixit, respiració)" },
            { "der", "Deriva", SText, 0, 1, 0.2f, Lineal, nullptr, "Cada nota es desafina sola, lentament (analògic, viu)" },
            { "derv", "Vel. deriva", SText, 0, 1, 0.3f, Lineal, nullptr, "Com de ràpida és la deriva" },
            { "ft", "Tipus", SFiltre, 0, 2, 0, Enter, "LP|HP|BP", "LP: treu aguts. HP: treu greus. BP: deixa una banda" },
            { "fp", "Pendent", SFiltre, 0, 1, 0, Enter, "12 dB|24 dB", "Com de fort talla" },
            { "cut", "Tall", SFiltre, 30, 18000, 3500, Log, nullptr, "Freqüència de tall" },
            { "res", "Ressonància", SFiltre, 0, 1, 0.15f, Lineal, nullptr, "Realça el tall" },
            { "fenv", "Envolupant", SFiltre, -1, 1, 0.1f, Lineal, nullptr, "Quant mou el tall l'envolupant del filtre" },
            { "key", "Seguiment", SFiltre, 0, 1, 0.3f, Lineal, nullptr, "Les notes agudes obren més el filtre" },
            { "fdrv", "Saturació", SFiltre, 0, 1, 0.05f, Lineal, nullptr, "Saturació a l'entrada del filtre (calidesa)" },
            { "fvel", "Velocitat", SFiltre, 0, 1, 0.2f, Lineal, nullptr, "Com obre el filtre la força de la nota" },
            { "aa", "A", SEnv, 1, 20000, 800, Log, nullptr, "Atac del volum" },
            { "ad", "D", SEnv, 10, 20000, 2000, Log, nullptr, "Caiguda del volum" },
            { "as", "S", SEnv, 0, 1, 0.8f, Lineal, nullptr, "Sostingut del volum" },
            { "ar", "R", SEnv, 10, 30000, 2500, Log, nullptr, "Alliberament del volum (la cua en deixar la tecla)" },
            { "fa", "A", SEnv, 1, 20000, 1500, Log, nullptr, "Atac del filtre" },
            { "fd", "D", SEnv, 10, 20000, 3000, Log, nullptr, "Caiguda del filtre" },
            { "fs", "S", SEnv, 0, 1, 0.6f, Lineal, nullptr, "Sostingut del filtre" },
            { "fr", "R", SEnv, 10, 30000, 3000, Log, nullptr, "Alliberament del filtre" },
            { "evt", "Evolució", SEnv, 0.2f, 60, 8, Log, nullptr, "Quant triga l'evolució (font «Evolució» de la matriu): el so canvia al llarg de la nota" },
            { "evf", "Forma ev.", SEnv, 0, 3, 0, Enter, "Puja|Baixa|Puja i baixa|Cicle", "Com va l'evolució: puja, baixa, puja i torna, o va i ve sense parar" },
            LFO (1, 0.25f), LFO (2, 0.07f), LFO (3, 3.0f),
            MOD (1, FLfo1, DTall, 0.12f), MOD (2, FLfo2, DForma3, 0.3f), MOD (3, FEvolucio, DMescla, 0.3f), MOD (4, FRoda, DTall, 0.5f),
            MOD (5, FAftertouch, DVolum, 0.0f), MOD (6, FCap, DTo, 0.0f), MOD (7, FCap, DTo, 0.0f), MOD (8, FCap, DTo, 0.0f),
            { "chm", "Chorus", SFx, 0, 1, 0.3f, Lineal, nullptr, "Chorus d'ensemble (tres veus): ample i càlid" },
            { "chr", "Vel. chorus", SFx, 0.05f, 5, 0.4f, Log, nullptr, "Velocitat del chorus" },
            { "chp", "Prof. chorus", SFx, 0, 1, 0.5f, Lineal, nullptr, "Profunditat del chorus" },
            { "phm", "Phaser", SFx, 0, 1, 0, Lineal, nullptr, "Quantitat de phaser" },
            { "phr", "Vel. phaser", SFx, 0.02f, 5, 0.15f, Log, nullptr, "Velocitat del phaser" },
            { "php", "Prof. phaser", SFx, 0, 1, 0.6f, Lineal, nullptr, "Profunditat del phaser" },
            { "phf", "Feedback", SFx, 0, 0.9f, 0.4f, Lineal, nullptr, "Realimentació del phaser (més marcat)" },
            { "dlm", "Delay", SFx, 0, 1, 0.15f, Lineal, nullptr, "Quantitat de delay" },
            { "dlt", "Temps", SFx, 0, 7, 5, Enter, notesDelay, "Temps del delay, amb el tempo del projecte" },
            { "dlf", "Feedback", SFx, 0, 0.92f, 0.4f, Lineal, nullptr, "Repeticions del delay" },
            { "dlpp", "Ping-pong", SFx, 0, 1, 1, Enter, noSi, "Els ecos van d'un costat a l'altre" },
            { "rvt", "Reverb", SFx, 0, 5, 0, Enter, "Hall|Plate|Ambient|Shimmer|Room|Chamber", "Tipus de reverb (la Reverb de Mibase)" },
            { "rvs", "Mida", SFx, 0, 1, 0.6f, Lineal, nullptr, "Mida i durada de la reverb (al màxim, cues de més de 15 s)" },
            { "rvm", "Mix rev.", SFx, 0, 1, 0.35f, Lineal, nullptr, "Quantitat de reverb" },
            { "rvsh", "Shimmer", SFx, 0, 1, 0, Lineal, nullptr, "La cua de la reverb puja d'octava a cada volta (cels, llum)" },
            { "rvfz", "Freeze", SFx, 0, 1, 0, Enter, noSi, "Congela la cua de la reverb: el que sona es queda sonant sense parar" },
            { "rvto", "To rev.", SFx, 0, 1, 0.5f, Lineal, nullptr, "Brillantor de la cua de la reverb" },
            { "apq", "Auto-pan", SFx, 0, 1, 0, Lineal, nullptr, "El so va d'un costat a l'altre" },
            { "apr", "Vel. pan", SFx, 0.02f, 10, 0.2f, Log, nullptr, "Velocitat de l'auto-pan" },
            { "aps", "Tempo pan", SFx, 0, 11, 0, Enter, sinte::ritmesLfo, "Auto-pan lliure o amb el tempo" },
            { "width", "Amplada", SFx, 0, 2, 1.1f, Lineal, nullptr, "Amplada estèreo final (0 = mono, 2 = molt ample)" },
            { "mat", "Atac", SMacro, -1, 1, 0, Lineal, nullptr, "Macro: atac més ràpid (-) o més lent (+), del volum i del filtre" },
            { "mrl", "Alliberament", SMacro, -1, 1, 0, Lineal, nullptr, "Macro: cua més curta (-) o més llarga (+)" },
            { "mfl", "Filtre", SMacro, -1, 1, 0, Lineal, nullptr, "Macro: filtre més tancat (-) o més obert (+)" },
            { "mmv", "Moviment", SMacro, -1, 1, 0, Lineal, nullptr, "Macro: menys (-) o més (+) moviment (LFO, evolució, deriva, auto-pan)" },
            { "mwd", "Amplada", SMacro, -1, 1, 0, Lineal, nullptr, "Macro: més estret (-) o més ample (+)" },
            { "mbr", "Brillantor", SMacro, -1, 1, 0, Lineal, nullptr, "Macro: més fosc (-) o més brillant (+)" },
            { "mwm", "Calidesa", SMacro, -1, 1, 0, Lineal, nullptr, "Macro: més prim (-) o més càlid i saturat (+)" },
            { "mdp", "Profunditat", SMacro, -1, 1, 0, Lineal, nullptr, "Macro: més a prop (-) o més lluny i profund (+): reverb i delay" },
            { "vv", "Vel → vol", SVeu, 0, 1, 0.3f, Lineal, nullptr, "Com canvia el volum amb la força de la nota" },
            { "bend", "Bend", SVeu, 0, 12, 2, Enter, nullptr, "Rang del pitch bend (semitons)" },
            { "veus", "Veus", SVeu, 0, 1, 0, Enter, "Poli|Mono", "Poli: acords (12 veus). Mono: una nota (drones, línies)" },
            { "glide", "Glide", SVeu, 0, 2000, 0, Cub, nullptr, "Portament entre notes" },
            { "vol", "Volum", SVeu, -24, 6, -6, Lineal, nullptr, "Volum de sortida" },
            { "vista", "Vista", SVeu, 0, 1, 0, Enter, "Simple|Avançat", "Com es mostra la finestra (no canvia el so)" },
            { "arp", "Arp", SArp, 0, 7, 0, Enter, sinte::modesArp, "Arpegiador: amb «Acord», el pad es repeteix amb el tempo (pads rítmics)" },
            { "arpr", "Ritme", SArp, 0, 5, 3, Enter, sinte::ritmesArp, "Durada de cada pas" },
            { "arpo", "Octaves", SArp, 1, 4, 1, Enter, nullptr, "Octaves que recorre" },
            { "arpg", "Gate", SArp, 0.05f, 1, 0.5f, Lineal, nullptr, "Durada de cada nota (al màxim, lligades)" },
            { "arpsw", "Swing", SArp, 0, 0.6f, 0, Lineal, nullptr, "Swing" },
            { "arpp", "Patró", SArp, 0, 15, 0, Enter, sinte::nomsPatrons, "Patró del seqüenciador (mode «Patró»)" },
        };
        #undef OSC
        #undef LFO
        #undef MOD

        // sinus ràpid (taula)
        constexpr int midaSin = 4096;
        float taulaSin[midaSin + 1];
        std::once_flag sinFet;
        void preparaSin() { std::call_once (sinFet, [] { for (int i = 0; i <= midaSin; ++i) taulaSin[i] = (float) std::sin (2.0 * dpi * i / midaSin); }); }
        inline float sinR (double ph) noexcept   // ph en cicles (0..1)
        {
            const double x = (ph - std::floor (ph)) * midaSin;
            const int i = (int) x;
            const float k = (float) (x - i);
            return taulaSin[i] + (taulaSin[i + 1] - taulaSin[i]) * k;
        }
        inline float smooth (float x) noexcept { x = std::clamp (x, 0.0f, 1.0f); return x * x * (3.0f - 2.0f * x); }
    }

    const sinte::InfoParam& info (int p) { return infos[std::clamp (p, 0, numParams - 1)]; }

    const char* nomTaula (int t)
    {
        static const char* n[] = { "Bàsica", "PWM", "Vocal", "Harmònics", "Digital", "Campana", "Sync", "Metall",
                                   "Cor", "Cordes", "Vidre", "Orgue", "Aire", "Fosc", "Octaves", "Formant" };
        return n[std::clamp (t, 0, numTaules - 1)];
    }

    std::string textValor (int p, float v)
    {
        const auto& i = info (p);
        if (i.opcions != nullptr)
        {
            const auto o = sinte::opcions (i);
            return o[(size_t) std::clamp ((int) std::lround (v - i.mn), 0, (int) o.size() - 1)];
        }
        if (p >= O1Ona && p <= O4Pan)
        {
            const int k = (p - O1Ona) % 8;
            if (k == 1) return (v > 0 ? "+" : "") + num (v, 0) + " oct";
            if (k == 2) return (v > 0 ? "+" : "") + num (v, 0) + " st";
            if (k == 3) return (v > 0 ? "+" : "") + num (v, 0) + " ct";
            if (k == 7) return std::abs (v) < 0.02f ? std::string ("C") : (v < 0 ? "E " : "D ") + num (std::abs (v) * 100.0, 0);
        }
        if (p >= M1Font && p <= M8Quant && (p - M1Font) % 3 == 2) return (v > 0 ? "+" : "") + num (v * 100.0, 0) + " %";
        if (p >= MAtac && p <= MProfunditat) return (v > 0 ? "+" : "") + num (v * 100.0, 0);
        switch (p)
        {
            case BendRang: return num (v, 0) + " st";
            case UniDetune: return num (v * v * 50.0, 0) + " ct";
            case Unison: return v < 1.5f ? std::string ("Off") : num (v, 0) + " veus";
            case FTall: return v < 1000.0f ? num (v, 0) + " Hz" : num (v / 1000.0, v < 10000.0f ? 2 : 1) + " kHz";
            case L1Vel: case L2Vel: case L3Vel: case ChVel: case PhVel: case ApVel: return num (v, v < 1.0f ? 3 : (v < 10.0f ? 2 : 1)) + " Hz";
            case GrDens: return num (v, 0) + " /s";
            case GrMida: case AA: case AD: case AR: case FA: case FD: case FR: return ms (v);
            case EvTemps: return num (v, v < 10.0f ? 1 : 0) + " s";
            case Glide: case L1Entrada: case L2Entrada: case L3Entrada: return v < 0.5f ? std::string ("Off") : ms (v);
            case Volum: return (v > 0 ? "+" : "") + num (v, 1) + " dB";
            case FEnv: return (v > 0 ? "+" : "") + num (v * 100.0, 0) + " %";
            case GrTo: return num (v * 50.0, 0) + " ct";
            case ArpOct: return num (v, 0) + " oct";
            case ArpGate: return v >= 0.99f ? std::string ("Lligat") : num (v * 100.0, 0) + " %";
            default: return num (v * 100.0, 0) + " %";
        }
    }

    //==========================================================================
    namespace wt
    {
        namespace
        {
            std::vector<float>& dades() { static std::vector<float> d; return d; }
            std::once_flag fet;
            inline size_t index (int t, int f, int l) { return (((size_t) t * marcs + (size_t) f) * nivells + (size_t) l) * mida; }

            /** Amplitud de l'harmònic h al marc x (0..1) de cada taula. */
            double amplitud (int t, int h, double x)
            {
                const double hd = h;
                auto campana = [] (double v, double c, double w) { return std::exp (-0.5 * std::pow ((v - c) / w, 2.0)); };
                switch (t)
                {
                    case 0:   // Cor: vocals obertes (a → o → u) amb molts harmònics suaus
                    {
                        const double f1 = 7.0 - 3.5 * x, f2 = 12.0 - 6.0 * x, f3 = 26.0 - 6.0 * x;
                        return (campana (hd, f1, 2.4) + 0.7 * campana (hd, f2, 3.0) + 0.25 * campana (hd, f3, 5.0) + 0.05) / std::pow (hd, 0.55);
                    }
                    case 1:   // Cordes: serra amb ressonàncies de caixa, cada cop més brillant
                    {
                        const double cos = 1.0 + 0.6 * campana (hd, 3.0, 1.0) + 0.5 * campana (hd, 7.0, 1.5) + 0.4 * campana (hd, 12.0, 2.5);
                        const double tall = 6.0 + x * x * 60.0;
                        return cos / hd / (1.0 + std::pow (hd / tall, 3.0));
                    }
                    case 2:   // Vidre: parcials esparsos, com una campana de vidre
                    {
                        static const int p[] = { 1, 3, 5, 8, 11, 15, 19, 24, 30, 37 };
                        const int quants = 3 + (int) (x * 7.0);
                        for (int k = 0; k < quants; ++k) if (p[k] == h) return (k == 0 ? 1.0 : 0.65 / std::sqrt ((double) k + 1.0));
                        return 0.0;
                    }
                    case 3:   // Orgue: registres (8', 4', 2 2/3', 2', 1 3/5', 1')
                    {
                        static const double reg[5][6] = { { 1, 0, 0, 0, 0, 0 }, { 1, .6, 0, 0, 0, 0 }, { 1, .7, .5, .3, 0, 0 }, { 1, .8, .6, .5, .3, .2 }, { .8, .8, .7, .7, .5, .5 } };
                        static const int hh[] = { 1, 2, 3, 4, 5, 8 };
                        const double q = x * 4.0;
                        const int a = std::min (3, (int) q);
                        const double k = q - a;
                        for (int i = 0; i < 6; ++i) if (hh[i] == h) return reg[a][i] * (1.0 - k) + reg[a + 1][i] * k;
                        return 0.0;
                    }
                    case 4:   // Aire: fonamental suau i una banda d'harmònics aguts febles (buf)
                        return (h == 1 ? 1.0 : 0.0) + 0.18 * campana (hd, 14.0 + 30.0 * x, 6.0 + 6.0 * x) + (h == 2 ? 0.25 : 0.0);
                    case 5:   // Fosc: pocs harmònics, que cauen ràpid (rodó, càlid)
                        return 1.0 / std::pow (hd, 2.6 - 1.2 * x);
                    case 6:   // Octaves: harmònics 1, 2, 4, 8, 16, 32 (llum, shimmer)
                    {
                        if ((h & (h - 1)) != 0) return 0.0;
                        const double o = std::log2 (hd);
                        return campana (o, x * 5.0, 1.3 + 0.4 * x) / (1.0 + 0.1 * o);
                    }
                    default:  // Formant: una ressonància que passa pels harmònics
                    {
                        const double c = 2.0 + x * 40.0;
                        return (0.15 + campana (hd, c, 1.5 + c * 0.12)) / std::pow (hd, 0.7);
                    }
                }
            }

            void construeix()
            {
                auto& d = dades();
                d.assign ((size_t) taules * marcs * nivells * mida, 0.0f);
                fft::FFT fft (mida);
                std::vector<fft::C> S ((size_t) mida), Y ((size_t) mida);
                for (int t = 0; t < taules; ++t)
                    for (int f = 0; f < marcs; ++f)
                    {
                        const double x = (double) f / (marcs - 1);
                        for (auto& c : S) c = { 0.0f, 0.0f };
                        // fases fixes però diferents per harmònic: el pic no es dispara (sona igual, més rodó)
                        for (int h = 1; h < mida / 2; ++h)
                        {
                            const double a = amplitud (t, h, x);
                            if (a == 0.0) continue;
                            const double ph = (double) ((h * h * 7 + h * 3) % 97) / 97.0 * 2.0 * dpi * (t == 6 || t == 3 ? 0.0 : 1.0);
                            S[(size_t) h] = { (float) (a * std::sin (ph) * mida / 2.0), (float) (-a * std::cos (ph) * mida / 2.0) };
                        }
                        float escala = 1.0f;
                        for (int l = 0; l < nivells; ++l)
                        {
                            const int H = std::min (mida / 2 - 1, (mida / 2) >> l);
                            for (auto& c : Y) c = { 0.0f, 0.0f };
                            for (int h = 1; h <= H; ++h)
                            {
                                Y[(size_t) h] = S[(size_t) h];
                                Y[(size_t) (mida - h)] = { S[(size_t) h].r, -S[(size_t) h].i };
                            }
                            fft.fa (Y.data(), true);
                            float* out = d.data() + index (t, f, l);
                            if (l == 0)
                            {
                                float pic = 1e-9f;
                                for (int i = 0; i < mida; ++i) pic = std::max (pic, std::abs (Y[(size_t) i].r));
                                escala = 1.0f / pic;
                            }
                            for (int i = 0; i < mida; ++i) out[i] = Y[(size_t) i].r * escala;
                        }
                    }
            }
        }

        void prepara() { std::call_once (fet, construeix); }

        float llegeix (int taula, double fase, float pos, double dt) noexcept
        {
            const auto& d = dades();
            if (d.empty()) return 0.0f;
            const double hmax = 0.5 / std::max (1e-6, dt);
            int l = 0;
            while (l < nivells - 1 && (double) ((mida / 2) >> l) > hmax) ++l;
            const float fp = std::clamp (pos, 0.0f, 1.0f) * (marcs - 1);
            const int f0 = std::min (marcs - 2, (int) fp);
            const float fr = fp - f0;
            const double x = (fase - std::floor (fase)) * mida;
            const int i0 = (int) x & (mida - 1), i1 = (i0 + 1) & (mida - 1);
            const float k = (float) (x - std::floor (x));
            const float* a = d.data() + index (std::clamp (taula, 0, taules - 1), f0, l);
            const float* b = a + (size_t) nivells * mida;
            const float va = a[i0] + (a[i1] - a[i0]) * k, vb = b[i0] + (b[i1] - b[i0]) * k;
            return va + (vb - va) * fr;
        }
    }

    namespace
    {
        inline float llegeixTaula (int t, double fase, float pos, double dt) noexcept
        {
            return t < 8 ? lead::wt::llegeix (t, fase, pos, dt) : wt::llegeix (t - 8, fase, pos, dt);
        }
    }

    //==========================================================================
    Atmos::Atmos() : sinte::Base (infos, numParams, "mibase-atmos", { ArpMode, ArpRitme, ArpOct, ArpGate, ArpSwing, ArpPatro })
    {
        preparaSin();
        lead::wt::prepara();
        wt::prepara();
        assignaCC (74, FTall);
        assignaCC (71, FRes);
        dl.posaTempo (&sinte::tempo());
        rv.posaTempo (&sinte::tempo());
        dl.posa (delay::Sync, 1.0f);
        dl.posa (delay::CaracterP, (float) delay::Cinta);
        dl.posa (delay::HighCut, 6000.0f);
        dl.posa (delay::LowCut, 180.0f);
        rv.posa (reverb::PreDelay, 25.0f);
        rv.posa (reverb::LowCut, 140.0f);
        rv.posa (reverb::ModDepth, 45.0f);
        rv.posa (reverb::Width, 130.0f);
    }

    std::string Atmos::nomComplet (int i) const
    {
        static const char* seccions[] = { "Osc", "Textura", "Filtre", "Env", "LFO", "Matriu", "FX", "Macro", "Veu", "Arp" };
        const auto& x = info (i);
        if (i >= O1Ona && i <= O4Pan) return "Osc " + std::to_string ((i - O1Ona) / 8 + 1) + " · " + x.nom;
        if (i >= AA && i <= AR) return std::string ("Env volum · ") + x.nom;
        if (i >= FA && i <= FR) return std::string ("Env filtre · ") + x.nom;
        if (i >= L1Ona && i <= L3Entrada) return "LFO " + std::to_string ((i - L1Ona) / 5 + 1) + " · " + x.nom;
        if (i >= M1Font && i <= M8Quant) return "Matriu " + std::to_string ((i - M1Font) / 3 + 1) + " · " + x.nom;
        return std::string (seccions[x.seccio]) + " · " + x.nom;
    }

    void Atmos::preparaSo (double f)
    {
        for (auto& v : veus) v = Veu();
        numPila = 0;
        dcL = dcR = dcxL = dcxR = 0.0f;
        for (auto& x : shelfL) x = 0.0f;
        for (auto& x : shelfH) x = 0.0f;
        chL.assign ((size_t) (f * 0.06) + 4, 0.0f);
        chR.assign ((size_t) (f * 0.06) + 4, 0.0f);
        chPos = 0;
        for (auto& x : apL) x = 0.0f;
        for (auto& x : apR) x = 0.0f;
        phFbL = phFbR = 0.0f;
        dl.prepara (f, tros);
        rv.prepara (f, tros);
        for (auto& x : darrerDl) x = -1.0f;
        for (auto& x : darrerRv) x = -1.0f;
    }

    bool Atmos::veusSonen() const noexcept
    {
        for (const auto& v : veus) if (v.viva) return true;
        if (valor (RvFreeze) > 0.5f && valor (RvMix) > 0.001f) return true;
        return valor (DlMix) > 0.001f || valor (RvMix) > 0.001f || valor (ChMix) > 0.001f ? nivell() > 1e-4f : false;
    }
    int Atmos::numVeusActives() const noexcept { int n = 0; for (const auto& v : veus) n += v.viva ? 1 : 0; return n; }
    void Atmos::apagaVeus() noexcept
    {
        for (auto& v : veus) if (v.viva) { v.etapaA = 4; v.tecla = false; v.etapaF = 3; }
        numPila = 0;
    }

    void Atmos::copiaVector (float* l, float* r, int n) const noexcept
    {
        const int p = posVec.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
        {
            const int k = (p - n + i + midaVector * 4) % midaVector;
            l[i] = vecL[(size_t) k].load (std::memory_order_relaxed);
            r[i] = vecR[(size_t) k].load (std::memory_order_relaxed);
        }
    }

    // ---- notes ----
    void Atmos::dispara (Veu& v, int nota, float vel, bool retriga, bool glide) noexcept
    {
        const bool estava = v.viva && v.envA > 1e-4f;
        v.nota = nota;
        v.objectiu = nota;
        if (! glide || ! v.viva) v.semis = nota;
        v.vel = vel;
        v.tecla = true;
        v.ordre = ++comptaOrdre;
        if (retriga)
        {
            v.etapaA = 1;
            v.etapaF = 1;
            v.temps = 0.0f;
            v.atzar = aleat();
            for (int k = 0; k < 3; ++k) { v.faseLfo[k] = 0.5 + 0.5 * aleat(); v.shLfo[k] = aleat(); v.shAnt[k] = v.shLfo[k]; }
            if (! estava)
            {
                v.envA = 0.0f;
                v.envF = 0.0f;
                const bool lliure = valor (FaseLliure) > 0.5f;
                for (int o = 0; o < 4; ++o)
                {
                    for (int k = 0; k < maxUnison; ++k)
                    {
                        double ph = lliure ? 0.5 + 0.5 * aleat() : k * 0.1373 + o * 0.29;
                        v.ph[o][k] = ph - std::floor (ph);
                    }
                    for (auto& g : v.grans[o]) g.viu = false;
                    v.finsGra[o] = 0.0f;
                }
                v.deriva = aleat() * 0.3f;
                v.derivaObj = aleat();
                v.tDeriva = 0.0f;
                for (auto& c : v.fl) for (auto& s : c) s = Svf();
                for (auto& s : v.ventF) s = Svf();
                v.nzVent = 0.5f + 0.5f * aleat();
                v.nzVentObj = 0.5f + 0.5f * aleat();
            }
        }
        v.viva = true;
        posaHz ((float) (440.0 * std::pow (2.0, (nota - 69) / 12.0)));
    }

    void Atmos::allibera (Veu& v) noexcept
    {
        v.tecla = false;
        if (v.etapaA != 0 && v.etapaA != 4) v.etapaA = 3;
        if (v.etapaF != 0) v.etapaF = 3;
    }

    void Atmos::notaOn (int nota, int vel) noexcept
    {
        const float vf = std::clamp (vel / 127.0f, 0.0f, 1.0f);
        const bool ambGlide = valor (Glide) > 0.5f;
        if (valor (Veus) < 0.5f)   // poli
        {
            Veu* tria = nullptr;
            for (auto& v : veus) if (v.viva && ! v.tecla && v.nota == nota) { tria = &v; break; }   // la mateixa nota que s'apaga: la reaprofita
            if (tria == nullptr) for (auto& v : veus) if (! v.viva) { tria = &v; break; }
            if (tria == nullptr) for (auto& v : veus) if (! v.tecla && (tria == nullptr || v.envA < tria->envA)) tria = &v;
            if (tria == nullptr) for (auto& v : veus) if (tria == nullptr || v.ordre < tria->ordre) tria = &v;
            const bool g = ambGlide && darreraNota >= 0.0;
            if (g && ! tria->viva) { tria->semis = darreraNota; }
            dispara (*tria, nota, vf, true, g);
            darreraNota = nota;
            return;
        }
        for (int i = 0; i < numPila; ++i)
            if (pila[i] == nota) { std::memmove (pila + i, pila + i + 1, sizeof (int) * (size_t) (numPila - i - 1)); --numPila; break; }
        const bool lligada = numPila > 0;
        if (numPila < 32) pila[numPila++] = nota;
        for (int i = 1; i < maxVeus; ++i) if (veus[i].viva) { veus[i].etapaA = 4; veus[i].tecla = false; }
        auto& v = veus[0];
        dispara (v, nota, lligada ? v.vel : vf, ! lligada || ! v.viva, (ambGlide || lligada) && v.viva);
        darreraNota = nota;
    }

    void Atmos::notaOff (int nota) noexcept
    {
        if (valor (Veus) < 0.5f)
        {
            for (auto& v : veus) if (v.viva && v.tecla && v.nota == nota) allibera (v);
            return;
        }
        for (int i = 0; i < numPila; ++i)
            if (pila[i] == nota) { std::memmove (pila + i, pila + i + 1, sizeof (int) * (size_t) (numPila - i - 1)); --numPila; break; }
        auto& v = veus[0];
        if (v.nota != nota) return;
        if (numPila > 0) dispara (v, pila[numPila - 1], v.vel, false, true);
        else allibera (v);
    }

    // ---- so ----
    float Atmos::valorLfo (int o, double ph, float sh, float shAnt) const noexcept
    {
        switch (o)
        {
            case 0: return sinR (ph);
            case 1: return 1.0f - 4.0f * std::abs ((float) ph - 0.5f);
            case 2: return 1.0f - 2.0f * (float) ph;
            case 3: return ph < 0.5 ? 1.0f : -1.0f;
            case 4: return sh;
            default: return shAnt + (sh - shAnt) * smooth ((float) ph);
        }
    }

    void Atmos::renderitza (float* L, float* R, int n) noexcept
    {
        float bL[tros] {}, bR[tros] {};
        constexpr int sub = 16;
        double hz[3];
        for (int k = 0; k < 3; ++k)
        {
            const int b = k * 5;
            const int r = (int) valor (L1Ritme + b);
            hz[k] = r <= 0 ? (double) valor (L1Vel + b) : std::max (20.0, sinte::tempo().load (std::memory_order_relaxed)) / 60.0 / sinte::negresRitme (r);
        }
        for (int s = 0; s < n; s += sub)
        {
            const int m = std::min (sub, n - s);
            const double dtS = (double) m / fs;
            float lg[3];
            for (int k = 0; k < 3; ++k)
            {
                faseG[k] += hz[k] * dtS;
                if (faseG[k] >= 1.0) { faseG[k] -= std::floor (faseG[k]); shGAnt[k] = shG[k]; shG[k] = aleat(); }
                lg[k] = valorLfo ((int) valor (L1Ona + k * 5), faseG[k], shG[k], shGAnt[k]);
                lfoAra[k] += (lg[k] - lfoAra[k]) * ((int) valor (L1Ona + k * 5) == 3 || (int) valor (L1Ona + k * 5) == 4 ? 0.5f : 1.0f);
                lg[k] = lfoAra[k];
            }
            posaLfo (lg[0]);
            for (int k = 0; k < 3; ++k) lfoNV[(size_t) k].store (lg[k], std::memory_order_relaxed);
            for (auto& v : veus)
                if (v.viva)
                {
                    for (int k = 0; k < 3; ++k)
                    {
                        v.faseLfo[k] += hz[k] * dtS;
                        if (v.faseLfo[k] >= 1.0) { v.faseLfo[k] -= std::floor (v.faseLfo[k]); v.shAnt[k] = v.shLfo[k]; v.shLfo[k] = aleat(); }
                    }
                    renderitzaVeu (v, bL, bR, s, m, lg);
                }
        }
        // sortida: DC, to (brillantor i calidesa)
        const float mBr = macro (MBrillantor), mCa = macro (MCalidesa);
        const float gH = std::pow (10.0f, 9.0f * mBr / 20.0f), gLo = std::pow (10.0f, (mCa > 0 ? 6.0f : 8.0f) * mCa / 20.0f);
        const float aH = coef (1.0 / (2.0 * dpi * 3200.0), fs), aLo = coef (1.0 / (2.0 * dpi * 220.0), fs);
        const float sat = std::max (0.0f, mCa);
        for (int i = 0; i < n; ++i)
        {
            float x[2] = { bL[i], bR[i] };
            const float ol = x[0] - dcxL + 0.9995f * dcL; dcxL = x[0]; dcL = ol;
            const float orr = x[1] - dcxR + 0.9995f * dcR; dcxR = x[1]; dcR = orr;
            x[0] = ol; x[1] = orr;
            for (int c = 0; c < 2; ++c)
            {
                shelfH[c] += (x[c] - shelfH[c]) * aH;
                float y = shelfH[c] + (x[c] - shelfH[c]) * gH;
                shelfL[c] += (y - shelfL[c]) * aLo;
                y = (y - shelfL[c]) + shelfL[c] * gLo;
                if (sat > 0.001f) y = tanhR (y * (1.0f + sat * 1.5f)) / (1.0f + sat * 0.6f);
                x[c] = y;
            }
            L[i] = x[0]; R[i] = x[1];
        }
        efectes (L, R, n);
        // amplada, volum, mesures
        const float volObj = std::pow (10.0f, valor (Volum) / 20.0f);
        const float amp = std::clamp (valor (Ample) * (1.0f + macro (MAmplada)), 0.0f, 2.0f);
        for (int i = 0; i < n; ++i)
        {
            volSuau += (volObj - volSuau) * 0.002f;
            const float mid = 0.5f * (L[i] + R[i]), sid = 0.5f * (L[i] - R[i]) * amp;
            auto sl = [] (float x) { const float ax = std::abs (x); return ax <= 0.8f ? x : std::copysign (0.8f + 0.2f * tanhR ((ax - 0.8f) / 0.2f), x); };
            L[i] = sl ((mid + sid) * volSuau);
            R[i] = sl ((mid - sid) * volSuau);
            corrLR = corrLR * 0.9995f + L[i] * R[i] * 0.0005f;
            potL = potL * 0.9995f + L[i] * L[i] * 0.0005f;
            potR = potR * 0.9995f + R[i] * R[i] * 0.0005f;
            if (++compteVec >= 8)
            {
                compteVec = 0;
                const int p = posVec.load (std::memory_order_relaxed);
                vecL[(size_t) p].store (L[i], std::memory_order_relaxed);
                vecR[(size_t) p].store (R[i], std::memory_order_relaxed);
                posVec.store ((p + 1) % midaVector, std::memory_order_relaxed);
            }
        }
        const float corr = corrLR / std::sqrt (potL * potR + 1e-12f);
        ampladaV.store (std::clamp (1.0f - corr, 0.0f, 1.0f), std::memory_order_relaxed);
    }

    void Atmos::efectes (float* L, float* R, int n) noexcept
    {
        const float mov = std::clamp (1.0f + macro (MMoviment), 0.0f, 2.0f);
        // chorus d'ensemble: tres línies per canal, desfasades 120°
        const float chm = valor (ChMix);
        if (chm > 0.001f && ! chL.empty())
        {
            const int N = (int) chL.size();
            const double dph = valor (ChVel) / fs;
            const float prof = valor (ChProf) * 0.0045f * (float) fs, base = 0.009f * (float) fs;
            auto llegeix = [&] (const std::vector<float>& b, float d)
            {
                float x = (float) chPos - d;
                while (x < 0.0f) x += (float) N;
                int a = (int) x;
                if (a >= N) { a -= N; x -= (float) N; }   // 0.92: l'arrodoniment podia deixar x = N (lectura fora del buffer)
                const int c = (a + 1) % N;
                const float k = x - (float) a;
                return b[(size_t) a] + (b[(size_t) c] - b[(size_t) a]) * k;
            };
            for (int i = 0; i < n; ++i)
            {
                chL[(size_t) chPos] = L[i];
                chR[(size_t) chPos] = R[i];
                chFase += dph; if (chFase >= 1.0) chFase -= 1.0;
                float wl = 0.0f, wr = 0.0f;
                for (int k = 0; k < 3; ++k)
                {
                    const double f = chFase + k / 3.0;
                    wl += llegeix (chL, base + prof * (0.5f + 0.5f * sinR (f)));
                    wr += llegeix (chR, base + prof * (0.5f + 0.5f * sinR (f + 0.25)));
                }
                L[i] = L[i] * (1.0f - 0.5f * chm) + wl * 0.36f * chm;
                R[i] = R[i] * (1.0f - 0.5f * chm) + wr * 0.36f * chm;
                chPos = (chPos + 1) % N;
            }
        }
        // phaser: 6 passa-tot per canal, el de la dreta desfasat 90°
        const float phm = valor (PhMix);
        if (phm > 0.001f)
        {
            const double dph = valor (PhVel) / fs;
            const float prof = valor (PhProf) * std::min (1.5f, mov), fb = valor (PhFeedback);
            for (int s = 0; s < n; s += 16)
            {
                const int m = std::min (16, n - s);
                phFase += dph * m; if (phFase >= 1.0) phFase -= 1.0;
                auto coefAp = [&] (double ph)
                {
                    const double f = 250.0 * std::pow (2.0, prof * 4.5 * (0.5 + 0.5 * sinR (ph)));
                    const double t = std::tan (dpi * std::min (f, fs * 0.45) / fs);
                    return (float) ((1.0 - t) / (1.0 + t));
                };
                const float aL = coefAp (phFase), aR = coefAp (phFase + 0.25);
                for (int i = s; i < s + m; ++i)
                {
                    float xl = L[i] + phFbL * fb, xr = R[i] + phFbR * fb;
                    for (int k = 0; k < 6; ++k)
                    {
                        const float yl = -aL * xl + apL[k]; apL[k] = xl + aL * yl; xl = yl;
                        const float yr = -aR * xr + apR[k]; apR[k] = xr + aR * yr; xr = yr;
                    }
                    phFbL = xl; phFbR = xr;
                    L[i] = L[i] * (1.0f - 0.5f * phm) + xl * 0.5f * phm;
                    R[i] = R[i] * (1.0f - 0.5f * phm) + xr * 0.5f * phm;
                }
            }
        }
        const float prof = 1.0f + macro (MProfunditat);
        // delay de Mibase
        const float dm = std::clamp (valor (DlMix) * prof, 0.0f, 1.0f);
        if (dm > 0.001f)
        {
            const float v[4] = { (float) notaDelay[std::clamp ((int) valor (DlNota), 0, 7)], valor (DlFeedback) * 100.0f, dm * 100.0f, valor (DlPingPong) };
            if (v[0] != darrerDl[0]) dl.posa (delay::Nota, v[0]);
            if (v[1] != darrerDl[1]) dl.posa (delay::Feedback, v[1]);
            if (v[2] != darrerDl[2]) dl.posa (delay::Mix, v[2]);
            if (v[3] != darrerDl[3]) dl.posa (delay::EncaminamentP, v[3] > 0.5f ? (float) delay::PingPong : (float) delay::Estereo);
            for (int k = 0; k < 4; ++k) darrerDl[k] = v[k];
            dl.processa (L, R, n);
        }
        // reverb de Mibase
        const float rm = std::clamp (valor (RvMix) * prof, 0.0f, 0.95f);
        if (rm > 0.001f || valor (RvFreeze) > 0.5f)
        {
            static const int algs[] = { reverb::Hall, reverb::Plate, reverb::Ambient, reverb::Shimmer, reverb::Room, reverb::Chamber };
            const int tipus = std::clamp ((int) valor (RvTipus), 0, 5);
            const float mida = std::clamp (valor (RvMida) + 0.25f * macro (MProfunditat), 0.0f, 1.0f);
            const float shim = valor (RvShimmer);
            const float vals[7] = { (float) tipus, mida, rm, shim, valor (RvFreeze), valor (RvTo), 0.0f };
            if (vals[0] != darrerRv[0]) rv.posa (reverb::Alg, (float) algs[tipus]);
            if (vals[1] != darrerRv[1]) { rv.posa (reverb::Decay, 0.8f + mida * mida * 16.0f); rv.posa (reverb::Size, 35.0f + 65.0f * mida); }
            if (vals[2] != darrerRv[2]) rv.posa (reverb::Mix, rm * 100.0f);
            if (vals[3] != darrerRv[3] || vals[0] != darrerRv[0])
            {
                const bool on = shim > 0.01f || tipus == 3;
                rv.posa (reverb::ShimOn, on ? 1.0f : 0.0f);
                rv.posa (reverb::ShimAmt, (tipus == 3 ? std::max (shim, 0.35f) : shim) * 100.0f);
            }
            if (vals[4] != darrerRv[4]) rv.posa (reverb::FreezeOn, vals[4] > 0.5f ? 1.0f : 0.0f);
            if (vals[5] != darrerRv[5]) { rv.posa (reverb::Damping, (1.0f - vals[5]) * 80.0f); rv.posa (reverb::HighCut, 3000.0f * std::pow (5.0f, vals[5])); }
            for (int k = 0; k < 6; ++k) darrerRv[k] = vals[k];
            rv.processa (L, R, n);
        }
        // auto-pan
        const float apq = std::clamp (valor (ApQuant) * mov, 0.0f, 1.0f);
        if (apq > 0.001f)
        {
            const int r = (int) valor (ApRitme);
            const double hz = r <= 0 ? (double) valor (ApVel) : std::max (20.0, sinte::tempo().load (std::memory_order_relaxed)) / 60.0 / sinte::negresRitme (r);
            for (int i = 0; i < n; ++i)
            {
                apFase += hz / fs; if (apFase >= 1.0) apFase -= 1.0;
                const float p = apq * sinR (apFase);
                const float gl = std::sqrt (0.5f * (1.0f - p)) * 1.4142f, gr = std::sqrt (0.5f * (1.0f + p)) * 1.4142f;
                const float mL = L[i], mR = R[i];
                L[i] = mL * gl; R[i] = mR * gr;
            }
            panV.store (apq * sinR (apFase), std::memory_order_relaxed);
        }
        else panV.store (0.0f, std::memory_order_relaxed);
    }

    void Atmos::renderitzaVeu (Veu& v, float* bL, float* bR, int s0, int m, const float* lfoGlob) noexcept
    {
        const int fi = s0 + m;
        const double dtS = (double) m / fs;
        const float mov = std::clamp (1.0f + macro (MMoviment), 0.0f, 2.0f);
        v.temps += (float) dtS;

        // fonts de modulació
        float src[numFonts] {};
        for (int k = 0; k < 3; ++k)
        {
            const int b = k * 5;
            float x = valor (L1Fase + b) > 0.5f ? valorLfo ((int) valor (L1Ona + b), v.faseLfo[k], v.shLfo[k], v.shAnt[k]) : lfoGlob[k];
            const float ent = valor (L1Entrada + b) / 1000.0f;
            if (ent > 0.001f) x *= std::clamp (v.temps / ent, 0.0f, 1.0f);
            src[FLfo1 + k] = x;
        }
        src[FEnvFiltre] = v.envF;
        {
            const float t = v.temps / std::max (0.05f, valor (EvTemps));
            float e;
            switch ((int) valor (EvForma))
            {
                case 1: e = 1.0f - smooth (t); break;
                case 2: e = t < 1.0f ? smooth (t) : 1.0f - smooth (t - 1.0f); break;
                case 3: e = 0.5f - 0.5f * sinR (0.25 + 0.5 * t); break;
                default: e = smooth (t); break;
            }
            src[FEvolucio] = e;
            if (v.ordre == comptaOrdre) evV.store (e, std::memory_order_relaxed);
        }
        src[FVelocitat] = v.vel;
        src[FNota] = (float) ((v.semis - 60.0) / 24.0);
        src[FRoda] = rodaMod;
        src[FAftertouch] = pressioAra;
        src[FAtzar] = v.atzar;
        src[FEnvVolum] = v.envA;
        float dest[numDestins] {};
        for (int k = 0; k < 8; ++k)
        {
            const int b = k * 3;
            const int f = (int) valor (M1Font + b);
            if (f <= FCap || f >= numFonts) continue;
            float q = valor (M1Quant + b);
            if (f == FLfo1 || f == FLfo2 || f == FLfo3 || f == FEvolucio || f == FAtzar) q *= mov;
            const int d = std::clamp ((int) valor (M1Desti + b), 0, numDestins - 1);
            dest[d] += src[f] * q;
        }

        // deriva (un passeig lent)
        const float der = valor (Deriva) * mov;
        v.tDeriva += (float) dtS;
        const float periodeDer = 3.0f - 2.6f * valor (DerivaVel);
        if (v.tDeriva > periodeDer) { v.tDeriva = 0.0f; v.derivaObj = aleat(); }
        v.deriva += (v.derivaObj - v.deriva) * (float) std::min (1.0, dtS / (periodeDer * 0.6));

        // to
        const float gl = valor (Glide);
        if (gl < 0.5f) v.semis = v.objectiu;
        else v.semis += (v.objectiu - v.semis) * (1.0 - std::exp (-dtS / (gl / 1000.0 / 3.0)));
        const double semis = v.semis + bendCru * valor (BendRang) + dest[DTo] * 12.0 + v.deriva * der * 0.3;
        const double hzNota = 440.0 * std::pow (2.0, (semis - 69.0) / 12.0);

        // oscil·ladors
        const float mescla = std::clamp (valor (Mescla) + dest[DMescla], 0.0f, 1.0f);
        if (v.ordre == comptaOrdre) mesclaV.store (mescla, std::memory_order_relaxed);
        const float gA = std::min (1.0f, 2.0f * (1.0f - mescla)), gB = std::min (1.0f, 2.0f * mescla);
        const int nu = std::clamp ((int) valor (Unison), 1, maxUnison);
        const float uniD = std::clamp (valor (UniDetune) + dest[DDetune], 0.0f, 1.0f);
        const float uniA = std::clamp (valor (UniAmple) * (1.0f + 0.5f * macro (MAmplada)), 0.0f, 1.0f);
        int ona[4], cnt[4], taulaO[4];
        float forma[4], niv[4], pan[4];
        double dt[4][maxUnison];
        float gL[4][maxUnison], gR[4][maxUnison];
        for (int o = 0; o < 4; ++o)
        {
            const int b = o * 8;
            ona[o] = (int) valor (O1Ona + b);
            taulaO[o] = (int) valor (O1Taula + b);
            forma[o] = std::clamp (valor (O1Forma + b) + dest[DForma1 + o] + dest[DFormes], 0.0f, 1.0f);
            niv[o] = std::max (0.0f, valor (O1Niv + b) * (1.0f + dest[DNiv1 + o])) * (o < 2 ? gA : gB) * 0.3f;
            pan[o] = valor (O1Pan + b);
            const double hz = hzNota * std::pow (2.0, valor (O1Oct + b) + valor (O1Semi + b) / 12.0 + valor (O1Fi + b) / 1200.0);
            cnt[o] = (ona[o] == Soroll || ona[o] == Granular) ? 1 : nu;
            const float gu = 1.0f / std::sqrt ((float) cnt[o]);
            for (int k = 0; k < cnt[o]; ++k)
            {
                const float x = cnt[o] == 1 ? 0.0f : (float) k / (cnt[o] - 1) * 2.0f - 1.0f;
                dt[o][k] = std::min (0.45, hz * std::pow (2.0, x * uniD * uniD * 50.0 / 1200.0) / fs);
                const float p = std::clamp (pan[o] + x * uniA, -1.0f, 1.0f);
                gL[o][k] = gu * std::sqrt (0.5f * (1.0f - p)) * 1.4142f;
                gR[o][k] = gu * std::sqrt (0.5f * (1.0f + p)) * 1.4142f;
            }
        }
        if (v.ordre == comptaOrdre) formaV.store (forma[0], std::memory_order_relaxed);
        const float fm21 = std::clamp (valor (Fm21) + dest[DFm], 0.0f, 1.0f), fm43 = std::clamp (valor (Fm43) + dest[DFm], 0.0f, 1.0f);

        // grànuls: nous grànuls d'aquest tros
        const float dens = valor (GrDens) * (1.0f + dest[DGranuls]);
        const float durGra = valor (GrMida) / 1000.0f, spray = valor (GrSpray), grTo = valor (GrTo);
        const int grOct = (int) valor (GrOct);
        for (int o = 0; o < 4; ++o)
        {
            if (ona[o] != Granular || niv[o] <= 0.0f) continue;
            v.finsGra[o] -= (float) dtS;
            int nous = 0;
            while (v.finsGra[o] <= 0.0f && nous < 4)
            {
                v.finsGra[o] += (1.0f / std::max (1.0f, dens)) * (0.6f + 0.8f * (0.5f + 0.5f * aleat()));
                ++nous;
                for (auto& g : v.grans[o])
                    if (! g.viu)
                    {
                        double cents = aleat() * grTo * 50.0;
                        const float r = 0.5f + 0.5f * aleat();
                        if (grOct == 1 && r > 0.7f) cents += 1200.0;
                        else if (grOct == 2 && r > 0.75f) cents += r > 0.88f ? 1200.0 : -1200.0;
                        else if (grOct == 3 && r > 0.7f) cents += r > 0.85f ? 700.0 : 1200.0;
                        g.viu = true;
                        g.fase = 0.5 + 0.5 * aleat();
                        g.dt = std::min (0.45, dt[o][0] * std::pow (2.0, cents / 1200.0));
                        g.pos = std::clamp (forma[o] + spray * aleat() * 0.5f, 0.0f, 1.0f);
                        g.t = 0.0f;
                        g.durada = durGra * (0.8f + 0.4f * (0.5f + 0.5f * aleat()));
                        g.pan = std::clamp (pan[o] + aleat() * uniA, -1.0f, 1.0f);
                        break;
                    }
            }
        }
        const float normGra = 1.0f / std::sqrt (std::max (1.0f, dens * durGra * 0.5f));

        // textura (soroll)
        const float nz = std::max (0.0f, valor (NzNiv) * (1.0f + dest[DSoroll])) * 0.3f;
        const int nzT = (int) valor (NzTipus);
        const float nzC = 1.0f - 0.97f * valor (NzColor), nzM = valor (NzMov) * std::min (1.5f, mov);
        float gVent = 0.0f, ventCoefR = 0.0f;
        if (nz > 0.0f && nzT == 1)
        {
            v.nzVent += (v.nzVentObj - v.nzVent) * (float) std::min (1.0, dtS * (0.3 + 2.0 * nzM));
            if (std::abs (v.nzVentObj - v.nzVent) < 0.05f) v.nzVentObj = 0.5f + 0.5f * aleat();
            const double fc = 250.0 * std::pow (2.0, 1.5 + 4.5 * v.nzVent * (1.0f - 0.5f * valor (NzColor)));
            gVent = (float) std::tan (dpi * std::min (fc, fs * 0.4) / fs);
            ventCoefR = 0.35f;   // ressonància moderada
        }
        const float respira = nzT == 3 ? 0.5f + 0.5f * sinR (v.temps * (0.08 + 0.5 * nzM) + v.atzar) : 1.0f;

        // envolupants
        const float fAt = std::pow (4.0f, macro (MAtac)), fRl = std::pow (4.0f, macro (MAlliberament));
        auto cA = [&] (int p, float f) { return coef (valor (p) * f / 1000.0 / 1.609, fs); };
        auto cDR = [&] (int p, float f) { return coef (valor (p) * f / 1000.0 / 4.0, fs); };
        const float caA = cA (AA, fAt), cdA = cDR (AD, 1.0f), crA = cDR (AR, fRl), sA = valor (AS);
        const float caF = cA (FA, fAt), cdF = cDR (FD, 1.0f), crF = cDR (FR, fRl), sF = valor (FS);
        const float crRapid = coef (0.004, fs);

        // filtre
        const int tipus = (int) valor (FTipus), etapes = valor (FPendent) > 0.5f ? 2 : 1;
        const double oct = valor (FEnv) * 6.0 * v.envF + valor (FKey) * (semis - 60.0) / 12.0 + valor (FVel) * 3.0 * (v.vel - 0.6)
                         + dest[DTall] * 4.0 + macro (MFiltre) * 3.0 + macro (MBrillantor) * 0.7 + v.deriva * der * 0.25;
        const double fc = std::clamp (valor (FTall) * std::pow (2.0, oct), 20.0, std::min (20000.0, fs * 0.45));
        const float g = (float) std::tan (dpi * fc / fs);
        const float res = std::clamp (valor (FRes) + dest[DRes], 0.0f, 1.0f);
        const float k0 = 2.0f - 1.94f * res, kb = 1.4f;
        const float A1 = 1.0f / (1.0f + g * (g + k0)), A2 = g * A1, A3 = g * A2;
        const float B1 = 1.0f / (1.0f + g * (g + kb)), B2 = g * B1, B3 = g * B2;
        const float drv = valor (FDrive) + 0.4f * std::max (0.0f, macro (MCalidesa));
        const float fDrv = 1.0f + 3.0f * drv;
        const float comp = 1.0f / (1.0f + 0.5f * drv);

        float vca = (1.0f - valor (VelVol) * (1.0f - v.vel)) * std::max (0.0f, 1.0f + dest[DVolum]);
        const float pv = std::clamp (dest[DPan], -1.0f, 1.0f);
        const float pL = std::sqrt (0.5f * (1.0f - pv)) * 1.4142f, pR = std::sqrt (0.5f * (1.0f + pv)) * 1.4142f;

        auto osc = [&] (int o, int k, double pm) noexcept
        {
            double& ph = v.ph[o][k];
            const double d = dt[o][k];
            double q = ph;
            const bool amb = pm == 0.0;
            if (! amb) { q = ph + pm; q -= std::floor (q); }
            float out;
            const float f = forma[o];
            switch (ona[o])
            {
                case Sinus: out = sinR (q); break;
                case Triangle: { const float x = (float) q; out = x < 0.5f ? -1.0f + 4.0f * x : 3.0f - 4.0f * x; break; }
                case Serra:
                {
                    out = (float) (2.0 * q - 1.0) - (amb ? blep (q, d) : 0.0f);
                    if (f > 0.001f)
                    {
                        double q2 = 2.0 * q; q2 -= std::floor (q2);
                        out = (out + f * ((float) (2.0 * q2 - 1.0) - (amb ? blep (q2, 2.0 * d) : 0.0f))) / (1.0f + 0.5f * f);
                    }
                    break;
                }
                case Quadrada: case Pols:
                {
                    const double w = ona[o] == Quadrada ? 0.5 - 0.45 * f : 0.25 - 0.22 * f;
                    out = q < w ? 1.0f : -1.0f;
                    if (amb) { out += blep (q, d); double r = q + 1.0 - w; if (r >= 1.0) r -= 1.0; out -= blep (r, d); }
                    break;
                }
                case Wavetable: out = llegeixTaula (taulaO[o], q, f, d); break;
                default: out = aleat() * 0.6f; break;   // soroll (el color, més avall)
            }
            ph += d;
            if (ph >= 1.0) ph -= 1.0;
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
            float aL = 0.0f, aR = 0.0f;
            float mod2 = 0.0f, mod4 = 0.0f;
            // l'ordre: 2 (modula l'1), 1, 4 (modula el 3), 3
            static constexpr int ordre[4] = { 1, 0, 3, 2 };
            for (int oi = 0; oi < 4; ++oi)
            {
                const int o = ordre[oi];
                const bool calModul = (o == 1 && fm21 > 0.001f) || (o == 3 && fm43 > 0.001f);
                if (niv[o] <= 0.0f && ! calModul) continue;
                if (ona[o] == Granular)
                {
                    for (auto& gr : v.grans[o])
                    {
                        if (! gr.viu) continue;
                        const float x = gr.t / gr.durada;
                        if (x >= 1.0f) { gr.viu = false; continue; }
                        const float w = 0.5f - 0.5f * sinR (0.25 + x);   // Hann
                        const float s = llegeixTaula (taulaO[o], gr.fase, gr.pos, gr.dt) * w * normGra;
                        gr.fase += gr.dt; if (gr.fase >= 1.0) gr.fase -= 1.0;
                        gr.t += (float) (1.0 / fs);
                        const float glg = std::sqrt (0.5f * (1.0f - gr.pan)) * 1.4142f, grg = std::sqrt (0.5f * (1.0f + gr.pan)) * 1.4142f;
                        aL += s * niv[o] * glg; aR += s * niv[o] * grg;
                    }
                    continue;
                }
                const double pm = o == 0 ? (double) (fm21 * fm21 * 1.2f * mod2) : o == 2 ? (double) (fm43 * fm43 * 1.2f * mod4) : 0.0;
                float suma = 0.0f;
                for (int k = 0; k < cnt[o]; ++k)
                {
                    float x = osc (o, k, pm);
                    if (ona[o] == Soroll) { v.nzB[o & 1] += (x - v.nzB[o & 1]) * (1.0f - 0.95f * forma[o]); x = v.nzB[o & 1] * (1.0f + 2.0f * forma[o]); }
                    suma += x;
                    aL += x * niv[o] * gL[o][k]; aR += x * niv[o] * gR[o][k];
                }
                if (o == 1) mod2 = suma / cnt[o];
                if (o == 3) mod4 = suma / cnt[o];
            }
            // textura
            float tL = 0.0f, tR = 0.0f;
            if (nz > 0.0f)
            {
                const float w0 = aleat(), w1 = aleat();
                switch (nzT)
                {
                    case 1:   // vent: passa-banda que es mou
                    {
                        float out[2];
                        const float wi[2] = { w0, w1 };
                        for (int c = 0; c < 2; ++c)
                        {
                            auto& s = v.ventF[c];
                            const float gg = c == 0 ? gVent : gVent * 1.12f;
                            const float kk = 2.0f - 2.0f * (1.0f - ventCoefR) * 0.6f;
                            const float a1 = 1.0f / (1.0f + gg * (gg + kk)), a2 = gg * a1, a3 = gg * a2;
                            const float v3 = wi[c] - s.a2;
                            const float v1 = a1 * s.a1 + a2 * v3, v2 = s.a2 + a2 * s.a1 + a3 * v3;
                            s.a1 = 2.0f * v1 - s.a1; s.a2 = 2.0f * v2 - s.a2;
                            out[c] = v1 * 2.5f;
                        }
                        tL = out[0]; tR = out[1];
                        break;
                    }
                    case 2:   // cruixit: espurnes rares sobre un fons molt fluix
                    {
                        float c = 0.0f;
                        if (0.5f + 0.5f * w0 < 0.0004f + 0.006f * nzM) c = w1 * 2.5f;
                        v.nzCruix = c + v.nzCruix * 0.3f;
                        v.nzB[0] += (w0 - v.nzB[0]) * nzC;
                        tL = tR = v.nzCruix + v.nzB[0] * 0.08f;
                        break;
                    }
                    default:  // blanc o respiració
                    {
                        v.nzB[0] += (w0 - v.nzB[0]) * nzC;
                        v.nzB[1] += (w1 - v.nzB[1]) * nzC;
                        const float k = 1.0f + (1.0f - nzC) * 2.0f;
                        tL = v.nzB[0] * k * respira; tR = v.nzB[1] * k * respira;
                        break;
                    }
                }
            }
            float xs[2] = { aL + tL * nz, aR + tR * nz };
            for (int c = 0; c < 2; ++c)
            {
                float x = tanhR (xs[c] * fDrv * 0.8f) * 1.25f + 1e-20f;
                for (int st = 0; st < etapes; ++st)
                {
                    auto& s = v.fl[c][st];
                    const float a1 = st == 0 ? A1 : B1, a2 = st == 0 ? A2 : B2, a3 = st == 0 ? A3 : B3, kk = st == 0 ? k0 : kb;
                    const float v3 = x - s.a2;
                    const float v1 = a1 * s.a1 + a2 * v3, v2 = s.a2 + a2 * s.a1 + a3 * v3;
                    s.a1 = 2.0f * v1 - s.a1; s.a2 = 2.0f * v2 - s.a2;
                    x = tipus == 0 ? v2 : tipus == 2 ? v1 * (st == 0 ? 1.0f : 1.4f) : x - kk * v1 - v2;
                }
                xs[c] = x * comp;
            }
            const float a = v.envA * vca;
            bL[i] += xs[0] * a * pL;
            bR[i] += xs[1] * a * pR;
        }
        if ((v.etapaA == 3 || v.etapaA == 4) && v.envA < 1e-5f) { v.viva = false; v.etapaA = 0; v.envA = 0.0f; }
    }

    //==========================================================================
    float previsualitza (const std::string& estat, float* sortida, int n, int nota)
    {
        auto sp = std::make_unique<Atmos>();
        auto& s = *sp;
        s.posaEstat (estat.empty() ? estatInicial() : estat);
        for (int p : { (int) Volum, (int) ChMix, (int) PhMix, (int) DlMix, (int) RvMix, (int) ApQuant, (int) ArpMode, (int) Deriva, (int) NzNiv,
                       (int) M1Quant, (int) M2Quant, (int) M3Quant, (int) M4Quant, (int) M5Quant, (int) M6Quant, (int) M7Quant, (int) M8Quant })
            s.posa (p, 0.0f);
        s.posa (Unison, 1.0f);
        s.posa (AA, 1.0f);
        s.posa (FA, 1.0f);
        s.posa (MAtac, 0.0f);
        const double fs = 48000.0;
        s.prepara (fs, 512);
        EventInstrument on { 0, 0x90, (std::uint8_t) nota, 110 };
        std::vector<float> L (256), R (256);
        s.processa (&on, 1, L.data(), R.data(), 256);
        for (int fet = 256; fet < (int) (fs * 0.15); fet += 256) s.processa (nullptr, 0, L.data(), R.data(), 256);
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
