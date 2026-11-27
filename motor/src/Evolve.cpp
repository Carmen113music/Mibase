#include "mibase/Evolve.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <random>

namespace mibase::evolve
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;
        using S = saturacio::Saturacio;
        inline float lim (float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
        inline float dB (float d) { return std::pow (10.0f, d / 20.0f); }

        const InfoParam infos[numParams] = {
            { "evolution", "Evolution", 0, 100, 35, false, false, "El pom gran: de 0 % (so original, sense processar) a 100 % (transformació extrema)." },
            { "drywet", "Dry/Wet", 0, 100, 100, false, false, "Barreja del so processat amb l'original (a 0 % d'Evolution, sempre l'original)." },
            { "entrada", "Input", -24, 12, 0, false, false, "Nivell que entra als efectes (més nivell = el Screamer i el Filter treballen més)." },
            { "sortida", "Output", -24, 12, 0, false, false, "Nivell de sortida del so processat." },
            { "hold", "Hold", 0, 1, 0, true, false, "Manté l'Evolution d'ara: mentre està encès, l'automatització i el pom no el mouen." },
            { "vista", "Vista", 0, 1, 0, true, false, "Simple o Advanced (només la finestra)." },

            { "scron", "Screamer", 0, 1, 1, true, false, "Encén o apaga el Screamer." },
            { "scrmode", "Tipus", 0, 4, 0, true, false, "Analògic (saturació suau), Overdrive, Distorsió, Fuzz o Plec (waveshaping que doblega l'ona)." },
            { "scrdrive", "Drive", 0, 100, 0, false, false, "Quanta distorsió (fins a +36 dB d'empenta). El volum es compensa sol." },
            { "scrtone", "Tone", 0, 100, 50, false, false, "Fosc ↔ brillant." },
            { "scrmix", "Mix", 0, 100, 0, false, false, "Quant Screamer se sent (0 % = res)." },
            { "scrout", "Output", -24, 12, 0, false, false, "Volum de sortida del Screamer." },

            { "filon", "Filter", 0, 1, 1, true, false, "Encén o apaga el Filter." },
            { "filtipus", "Tipus", 0, 3, 0, true, false, "LP (treu aguts), HP (treu greus), BP (només una banda) o Notch (treu una banda)." },
            { "filfreq", "Cutoff", 20, 20000, 20000, false, true, "Freqüència del filtre." },
            { "filreso", "Resonance", 0, 100, 10, false, false, "Ressonància: realça la zona del tall (molta = xiulet)." },
            { "fildrive", "Drive", 0, 100, 0, false, false, "Satura el so abans del filtre." },
            { "fillfodiv", "LFO", 0, 13, 5, true, false, "Velocitat del moviment del filtre, amb el tempo de la cançó." },
            { "fillfo", "Sweep", 0, 100, 0, false, false, "Quant es mou el filtre amb l'LFO (fins a 3 octaves amunt i avall)." },
            { "filmix", "Mix", 0, 100, 0, false, false, "Quant Filter se sent (0 % = res)." },

            { "pmpon", "Pumper", 0, 1, 1, true, false, "Encén o apaga el Pumper." },
            { "pmpmode", "Tipus", 0, 3, 0, true, false, "Ducking (bombeig com un sidechain), Trèmolo, Gate rítmic (patrons) o Pan (d'un costat a l'altre)." },
            { "pmpdiv", "Ritme", 0, 13, 5, true, false, "Cada quant bomba, amb el tempo de la cançó (amb punt i tresets)." },
            { "pmpdepth", "Depth", 0, 100, 0, false, false, "Quant baixa el volum a cada pulsació (0 % = res)." },
            { "pmpforma", "Forma", 0, 100, 50, false, false, "Ducking: com de ràpid torna. Trèmolo: rodó ↔ quadrat. Gate: tall sec ↔ suau." },
            { "pmppatro", "Patró", 0, 7, 0, true, false, "El patró de 16 passos del Gate rítmic." },

            { "shmon", "Shimmer", 0, 1, 1, true, false, "Encén o apaga el Shimmer." },
            { "shmtipus", "Tipus", 0, 2, 0, true, false, "Shimmer (cua que puja de to), Hall dens o Ambient." },
            { "shmpitch", "Pitch", 0, 4, 0, true, false, "Transposició de la cua: +12, +7, +5, -12 o +24 semitons." },
            { "shmamt", "Shimmer", 0, 100, 40, false, false, "Quanta part de la cua es transposa." },
            { "shmdecay", "Decay", 0.3f, 20.0f, 5.0f, false, true, "Quant dura la cua." },
            { "shmsize", "Size", 0, 100, 70, false, false, "Mida de l'espai." },
            { "shmdiff", "Diffusion", 0, 100, 80, false, false, "Densitat de la cua: poca = ecos; molta = llisa." },
            { "shmmod", "Modulation", 0, 100, 40, false, false, "Moviment de la cua (cor suau)." },
            { "shmdamp", "Damping", 0, 100, 40, false, false, "Com de fosca és la cua." },
            { "shmfreeze", "Freeze", 0, 1, 0, true, false, "Congela la cua: queda sonant sense parar." },
            { "shmmix", "Mix", 0, 100, 0, false, false, "Quant Shimmer se sent (0 % = res; 100 % = només la cua)." },

            // 1.14: REVERSER
            { "revon", "Reverser", 0, 1, 1, true, false, "Encén o apaga el Reverser." },
            { "revmode", "Tipus", 0, 1, 0, true, false, "Reverse delay (cada tros sona al revés just després) o Reverse reverb (la cua invertida puja just abans de cada nota; Mibase ho compensa)." },
            { "revdiv", "Tros", 0, 13, 5, true, false, "Llargada de cada tros que es captura i s'inverteix, amb el tempo." },
            { "revfb", "Feedback", 0, 90, 20, false, false, "Reverse delay: quant torna a entrar el so invertit (repeticions)." },
            { "revpitch", "Pitch", 0, 2, 0, true, false, "El tros invertit sona igual, una octava amunt o una octava avall." },
            { "revdecay", "Decay", 0.5f, 12.0f, 3.0f, false, true, "Reverse reverb: llargada de la cua que s'inverteix." },
            { "revmix", "Mix", 0, 100, 0, false, false, "Quant Reverser se sent (0 % = res)." },
            // PITCH
            { "piton", "Pitch", 0, 1, 1, true, false, "Encén o apaga el Pitch." },
            { "pitmode", "Tipus", 0, 1, 0, true, false, "Universal (per a tot: guitarres, acords, bateria) o Veu (una sola veu: conserva el color i permet moure els formants; retarda uns 36 ms, que Mibase compensa)." },
            { "pitsemi1", "Veu 1", -24, 24, 12, true, false, "Semitons de la primera veu (12 = una octava amunt)." },
            { "pitsemi2", "Veu 2", -24, 24, 7, true, false, "Semitons de la segona veu (harmonia)." },
            { "pitniv2", "Nivell 2", 0, 100, 0, false, false, "Volum de la segona veu (0 % = només una veu)." },
            { "pitdetune", "Detune", 0, 50, 0, false, false, "Desafina l'esquerra amunt i la dreta avall (cèntims): fa el so més ample." },
            { "pitfi", "Fi", -100, 100, 0, false, false, "Microafinació (cèntims) de totes dues veus." },
            { "pitformant", "Formant", -12, 12, 0, false, false, "Mode Veu: mou el color de la veu (més petit ↔ més gran) sense canviar la nota." },
            { "pitmix", "Mix", 0, 100, 0, false, false, "Quant Pitch se sent (0 % = res; 100 % = només les veus noves)." },
            // SCATTER
            { "scaon", "Scatter", 0, 1, 1, true, false, "Encén o apaga el Scatter." },
            { "scadiv", "Tall", 2, 13, 11, true, false, "Llargada de cada tall, amb el tempo." },
            { "scadens", "Density", 0, 100, 30, false, false, "Quants talls es trenquen (repeticions, salts enrere, al revés…)." },
            { "scascat", "Scatter", 0, 100, 30, false, false, "Com d'estrany: poc = repeticions; molt = salts, trossos al revés i a doble velocitat." },
            { "scafb", "Feedback", 0, 100, 30, false, false, "A les repeticions, quant baixa cada cop (0 = totes iguals)." },
            { "scamix", "Mix", 0, 100, 0, false, false, "Quant Scatter se sent (0 % = res)." },
            // nivells d'entrada (8) i de sortida (8) de cada mòdul
            { "in_scr", "In", -24, 12, 0, false, false, "Nivell que entra al mòdul." }, { "in_fil", "In", -24, 12, 0, false, false, "Nivell que entra al mòdul." },
            { "in_pmp", "In", -24, 12, 0, false, false, "Nivell que entra al mòdul." }, { "in_shm", "In", -24, 12, 0, false, false, "Nivell que entra al mòdul." },
            { "in_rev", "In", -24, 12, 0, false, false, "Nivell que entra al mòdul." }, { "in_pit", "In", -24, 12, 0, false, false, "Nivell que entra al mòdul." },
            { "in_sca", "In", -24, 12, 0, false, false, "Nivell que entra al mòdul." }, { "in_gra", "In", -24, 12, 0, false, false, "Nivell que entra al mòdul." },
            { "out_scr", "Out", -24, 12, 0, false, false, "Nivell que surt del mòdul." }, { "out_fil", "Out", -24, 12, 0, false, false, "Nivell que surt del mòdul." },
            { "out_pmp", "Out", -24, 12, 0, false, false, "Nivell que surt del mòdul." }, { "out_shm", "Out", -24, 12, 0, false, false, "Nivell que surt del mòdul." },
            { "out_rev", "Out", -24, 12, 0, false, false, "Nivell que surt del mòdul." }, { "out_pit", "Out", -24, 12, 0, false, false, "Nivell que surt del mòdul." },
            { "out_sca", "Out", -24, 12, 0, false, false, "Nivell que surt del mòdul." }, { "out_gra", "Out", -24, 12, 0, false, false, "Nivell que surt del mòdul." },
            // LFO 1 i 2
            { "l1forma", "Forma", 0, 4, 0, true, false, "Sinus, triangle, quadrada, serra o atzar." },
            { "l1sync", "Sync", 0, 1, 1, true, false, "Amb el tempo de la cançó (sí) o en hertzs (no)." },
            { "l1rate", "Rate", 0.02f, 20.0f, 1.0f, false, true, "Velocitat lliure (Hz), quan no va amb el tempo." },
            { "l1div", "Ritme", 0, 13, 5, true, false, "Velocitat amb el tempo." },
            { "l1depth", "Depth", 0, 100, 50, false, false, "Quant modula." },
            { "l1fase", "Phase", 0, 360, 0, false, false, "On comença el cicle." },
            { "l1offset", "Offset", -100, 100, 0, false, false, "Desplaça el centre de la modulació." },
            { "l2forma", "Forma", 0, 4, 1, true, false, "Sinus, triangle, quadrada, serra o atzar." },
            { "l2sync", "Sync", 0, 1, 1, true, false, "Amb el tempo de la cançó (sí) o en hertzs (no)." },
            { "l2rate", "Rate", 0.02f, 20.0f, 0.3f, false, true, "Velocitat lliure (Hz), quan no va amb el tempo." },
            { "l2div", "Ritme", 0, 13, 0, true, false, "Velocitat amb el tempo." },
            { "l2depth", "Depth", 0, 100, 50, false, false, "Quant modula." },
            { "l2fase", "Phase", 0, 360, 0, false, false, "On comença el cicle." },
            { "l2offset", "Offset", -100, 100, 0, false, false, "Desplaça el centre de la modulació." },
            // 1.15: GRANULAR
            { "graon", "Granular", 0, 1, 1, true, false, "Encén o apaga el Granular." },
            { "grasize", "Gra", 10, 500, 80, false, true, "Llargada de cada gra: curts = textura fina; llargs = trossos que es reconeixen." },
            { "gradens", "Density", 1, 80, 20, false, true, "Quants grans per segon: pocs = gotes; molts = núvol continu." },
            { "grapos", "Posició", 0, 2000, 120, false, false, "D'on es llegeixen els grans: quant de temps enrere (ms)." },
            { "graspray", "Dispersió", 0, 1000, 60, false, false, "Quant s'escampen els grans en el temps (ms): fa el núvol més difús." },
            { "grapitch", "Pitch", -24, 24, 0, true, false, "Transposició de tots els grans (semitons)." },
            { "grarandpitch", "Pitch atzar", 0, 100, 0, false, false, "Quant varia el to de cada gra a l'atzar." },
            { "graquant", "Escala", 0, 2, 0, true, false, "El to a l'atzar: lliure, només octaves o octaves i quintes (més musical)." },
            { "grarev", "Reverse", 0, 100, 0, false, false, "Quants grans sonen al revés." },
            { "grafb", "Feedback", 0, 90, 0, false, false, "Quant torna a entrar el núvol: el fa créixer i allargar." },
            { "grawidth", "Amplada", 0, 100, 60, false, false, "Com s'escampen els grans entre esquerra i dreta." },
            { "grafreeze", "Freeze", 0, 1, 0, true, false, "Congela el so: els grans continuen llegint el que hi havia, sense parar." },
            { "gramix", "Mix", 0, 100, 0, false, false, "Quant Granular se sent (0 % = res; 100 % = només el núvol)." },
        };
        static_assert (sizeof (infos) / sizeof (infos[0]) == numParams, "un InfoParam per paràmetre");

        struct Div { const char* nom; double negres; };
        const Div divs[] = {
            { "1/1", 4.0 }, { "1/2.", 3.0 }, { "1/2", 2.0 }, { "1/4.", 1.5 }, { "1/2T", 4.0 / 3.0 }, { "1/4", 1.0 }, { "1/8.", 0.75 },
            { "1/4T", 2.0 / 3.0 }, { "1/8", 0.5 }, { "1/16.", 0.375 }, { "1/8T", 1.0 / 3.0 }, { "1/16", 0.25 }, { "1/16T", 1.0 / 6.0 }, { "1/32", 0.125 },
        };
        struct Pat { const char* nom; const char* passos; };
        const Pat pats[] = {
            { "Corxeres", "x.x.x.x.x.x.x.x." }, { "Semis", "xxxxxxxxxxxxxxx." }, { "Galop", "x.xxx.xxx.xxx.xx" }, { "Trencat", "xx.x.xx.x..xx.x." },
            { "Contratemps", ".x.x.x.x.x.x.x.x" }, { "Tresillo", "x..x..x.x..x..x." }, { "Stutter", "xxxx....xx.x...." }, { "Esquitx", "x.......x.x.xxxx" },
        };
        float fraccio (double x) { return (float) (x - std::floor (x)); }
        float protegeix (float x) noexcept
        {
            const float a = std::fabs (x);
            if (a <= 0.9f) return x;
            const float y = 0.9f + 0.099f * std::tanh ((a - 0.9f) / 0.099f);
            return x < 0.0f ? -y : y;
        }
    }

    const InfoParam& info (int p) { return infos[std::clamp (p, 0, numParams - 1)]; }
    int paramDeClau (const std::string& c)
    {
        for (int i = 0; i < numParams; ++i) if (c == infos[i].clau) return i;
        return -1;
    }

    const char* nomModul (int m)
    {
        static const char* n[numModuls] = { "SCREAMER", "FILTER", "PUMPER", "SHIMMER", "REVERSER", "PITCH", "SCATTER", "GRANULAR" };
        return n[std::clamp (m, 0, numModuls - 1)];
    }
    const char* descripcioModul (int m)
    {
        static const char* n[numModuls] = { "Distorsió creativa: de saturació subtil a destrucció.", "Filtratge creatiu amb ressonància i moviment al tempo.",
                                            "Moviment rítmic: bombeig, trèmolo, gate i panoràmica.", "Espai: cues llargues que pugen de to.",
                                            "Efectes invertits: reverse delay i cues que pugen abans de la nota.", "Octaves, harmonies, detune i formants.",
                                            "Fragments: repeticions, salts enrere i trossos al revés, amb el tempo.",
                                            "Textures i atmosferes: el so convertit en un núvol de grans." };
        return n[std::clamp (m, 0, numModuls - 1)];
    }
    int paramOn (int m) { static const int p[numModuls] = { ScrOn, FilOn, PmpOn, ShmOn, RevOn, PitOn, ScaOn, GraOn }; return p[std::clamp (m, 0, numModuls - 1)]; }
    int paramMix (int m) { static const int p[numModuls] = { ScrMix, FilMix, PmpDepth, ShmMix, RevMix, PitMix, ScaMix, GraMix }; return p[std::clamp (m, 0, numModuls - 1)]; }
    int primerParam (int m) { return paramOn (m); }
    int finalParam (int m) { static const int p[numModuls] = { ScrOut + 1, FilMix + 1, PmpPatro + 1, ShmMix + 1, RevMix + 1, PitMix + 1, ScaMix + 1, GraMix + 1 }; return p[std::clamp (m, 0, numModuls - 1)]; }
    int modulDeParam (int p)
    {
        for (int m = 0; m < numModuls; ++m) if (p >= primerParam (m) && p < finalParam (m)) return m;
        if (p >= NivIn && p < (int) NivIn + (int) numModuls) return p - NivIn;
        if (p >= NivOut && p < (int) NivOut + (int) numModuls) return p - NivOut;
        return -1;
    }
    bool canviaLatencia (int p) { return p == RevOn || p == RevMode || p == RevDiv || p == PitOn || p == PitMode; }
    bool esModulable (int p)
    {
        if (p < 0 || p >= numParams || p == Evolution || p == Hold || p == Vista || canviaLatencia (p)) return false;
        if ((p >= (int) NivIn + (int) numModuls && p < NivIn + 8) || (p >= (int) NivOut + (int) numModuls && p < NivOut + 8)) return false;   // llocs reservats
        return true;
    }
    const char* nomModeReverser (int m) { static const char* n[] = { "Reverse delay", "Reverse reverb" }; return n[std::clamp (m, 0, 1)]; }
    const char* nomModePitch (int m) { static const char* n[] = { "Universal", "Veu" }; return n[std::clamp (m, 0, 1)]; }
    const char* nomQuantGranular (int q) { static const char* n[] = { "Lliure", "Octaves", "Quintes" }; return n[std::clamp (q, 0, 2)]; }
    const char* nomFormaLfo (int f) { static const char* n[] = { "Sinus", "Triangle", "Quadrada", "Serra", "Atzar" }; return n[std::clamp (f, 0, 4)]; }
    float formaLfo (int forma, float f, long long cicle) noexcept
    {
        f -= std::floor (f);
        switch (forma)
        {
            case 1: return f < 0.5f ? 4.0f * f - 1.0f : 3.0f - 4.0f * f;
            case 2: return f < 0.5f ? 1.0f : -1.0f;
            case 3: return 2.0f * f - 1.0f;
            case 4:
            {
                unsigned long long z = (unsigned long long) cicle * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull;
                z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull; z = (z ^ (z >> 27)) * 0x94D049BB133111EBull; z ^= z >> 31;
                return (float) ((double) (z >> 11) / 9007199254740992.0) * 2.0f - 1.0f;
            }
            default: return std::sin ((float) (2.0 * pi) * f);
        }
    }
    const char* nomModeScreamer (int m) { static const char* n[] = { "Analògic", "Overdrive", "Distorsió", "Fuzz", "Plec" }; return n[std::clamp (m, 0, 4)]; }
    const char* nomTipusFiltre (int t) { static const char* n[] = { "LP", "HP", "BP", "Notch" }; return n[std::clamp (t, 0, 3)]; }
    const char* nomModePumper (int m) { static const char* n[] = { "Ducking", "Trèmolo", "Gate", "Pan" }; return n[std::clamp (m, 0, 3)]; }
    const char* nomTipusShimmer (int t) { static const char* n[] = { "Shimmer", "Hall dens", "Ambient" }; return n[std::clamp (t, 0, 2)]; }
    int numDivisions() { return (int) (sizeof divs / sizeof divs[0]); }
    const char* nomDivisio (int i) { return divs[std::clamp (i, 0, numDivisions() - 1)].nom; }
    double divisioEnNegres (int i) { return divs[std::clamp (i, 0, numDivisions() - 1)].negres; }
    int numPatrons() { return (int) (sizeof pats / sizeof pats[0]); }
    const char* nomPatro (int i) { return pats[std::clamp (i, 0, numPatrons() - 1)].nom; }
    const char* patro (int i) { return pats[std::clamp (i, 0, numPatrons() - 1)].passos; }
    const char* nomCorba (int c) { static const char* n[] = { "Lineal", "Suau", "Ràpida", "En S" }; return n[std::clamp (c, 0, numCorbes - 1)]; }

    std::string textValor (int p, float v)
    {
        char b[48];
        switch (p)
        {
            case Entrada: case Sortida: case ScrOut: std::snprintf (b, sizeof b, "%+.1f dB", (double) v); return b;
            default:
                if (p >= NivIn && p < NivOut + 8) { std::snprintf (b, sizeof b, "%+.1f dB", (double) v); return b; }
                break;
            case FilFreq: if (v >= 1000.0f) std::snprintf (b, sizeof b, "%.1f kHz", (double) v / 1000.0); else std::snprintf (b, sizeof b, "%.0f Hz", (double) v); return b;
            case ShmDecay: std::snprintf (b, sizeof b, "%.1f s", (double) v); return b;
            case ScrMode: return nomModeScreamer ((int) std::lround (v));
            case FilTipus: return nomTipusFiltre ((int) std::lround (v));
            case PmpMode: return nomModePumper ((int) std::lround (v));
            case ShmTipus: return nomTipusShimmer ((int) std::lround (v));
            case FilLfoDiv: case PmpDiv: return nomDivisio ((int) std::lround (v));
            case PmpPatro: return nomPatro ((int) std::lround (v));
            case ShmPitch: { const int t = reverb::transposicio ((int) std::lround (v)); std::snprintf (b, sizeof b, "%+d", t); return b; }
            case Vista: return v > 0.5f ? "Advanced" : "Simple";
            case GraSize: case GraPos: case GraSpray: std::snprintf (b, sizeof b, "%.0f ms", (double) v); return b;
            case GraDens: std::snprintf (b, sizeof b, v < 10.0f ? "%.1f /s" : "%.0f /s", (double) v); return b;
            case GraPitch: std::snprintf (b, sizeof b, "%+d st", (int) std::lround (v)); return b;
            case GraQuant: return nomQuantGranular ((int) std::lround (v));
            case Hold: case ScrOn: case FilOn: case PmpOn: case ShmOn: case ShmFreeze: case RevOn: case PitOn: case ScaOn: case L1Sync: case L2Sync:
            case GraOn: case GraFreeze:
                return v > 0.5f ? "Sí" : "No";
            case RevMode: return nomModeReverser ((int) std::lround (v));
            case PitMode: return nomModePitch ((int) std::lround (v));
            case RevDiv: case ScaDiv: case L1Div: case L2Div: return nomDivisio ((int) std::lround (v));
            case RevPitch: { static const char* n[] = { "0", "+12", "-12" }; return n[std::clamp ((int) std::lround (v), 0, 2)]; }
            case RevDecay: std::snprintf (b, sizeof b, "%.1f s", (double) v); return b;
            case PitSemi1: case PitSemi2: case PitFormant: std::snprintf (b, sizeof b, "%+d st", (int) std::lround (v)); return b;
            case PitDetune: case PitFi: std::snprintf (b, sizeof b, "%+.0f ct", (double) v); return b;
            case L1Forma: case L2Forma: return nomFormaLfo ((int) std::lround (v));
            case L1Rate: case L2Rate: std::snprintf (b, sizeof b, v < 1.0f ? "%.2f Hz" : "%.1f Hz", (double) v); return b;
            case L1Fase: case L2Fase: std::snprintf (b, sizeof b, "%.0f°", (double) v); return b;
            case L1Offset: case L2Offset: std::snprintf (b, sizeof b, "%+.0f %%", (double) v); return b;
        }
        std::snprintf (b, sizeof b, "%.0f %%", (double) v);
        return b;
    }

    float aplicaCorba (int c, float t) noexcept
    {
        t = lim (t, 0.0f, 1.0f);
        switch (c)
        {
            case Suau: return t * t;                          // entra a poc a poc i s'accelera
            case Rapida: return std::sqrt (t);                // entra de cop i després s'alenteix
            case EnS: return t * t * (3.0f - 2.0f * t);
            default: return t;
        }
    }
    float valorEnEvolution (const Ruta& r, float base, float evo) noexcept
    {
        float t;
        if (r.a <= r.de + 1e-3f) t = evo >= r.de ? 1.0f : 0.0f;
        else t = (evo - r.de) / (r.a - r.de);
        const float f = aplicaCorba (r.corba, t);
        if (f <= 0.0f) return base;
        if (f >= 1.0f) return r.valor;
        const auto& in = info (r.param);
        if (in.log && base > 0.0f && r.valor > 0.0f)
            return std::exp (std::log (base) + (std::log (r.valor) - std::log (base)) * f);
        return base + (r.valor - base) * f;
    }

    // ---------------------------------------------------------------------------------------------
    Evolve::Evolve() : ctx (&ContextMusical::global())
    {
        for (int i = 0; i < numParams; ++i) par[(size_t) i].store (infos[i].def);
        {
            // 1.14: l'ordre musical de sempre: to → distorsió → filtre → talls → moviment → al revés → espai
            for (int m = 0; m < numModuls; ++m) ord[(size_t) m].store (ordreMusical[(size_t) m]);
        }
        revSw = std::make_unique<reverb::Reverb>();
        rev = std::make_unique<reverb::Reverb>();
        // Per defecte: la guitarra (o el que sigui) va guanyant espai, moviment i color.
        posa (ShmTipus, 0); posa (ShmPitch, 0); posa (FilTipus, 0); posa (PmpMode, 1); posa (PmpDiv, 8);
        posaRutes ({ { ShmMix, 10, 70, 45, Suau }, { ShmDecay, 30, 100, 12, Lineal }, { ShmAmt, 40, 100, 70, Lineal },
                     { FilMix, 20, 50, 100, Rapida }, { FilFreq, 50, 100, 1800, EnS }, { FilLfo, 60, 100, 45, Lineal },
                     { PmpDepth, 55, 95, 55, Suau }, { ScrMix, 70, 100, 45, Suau }, { ScrDrive, 70, 100, 55, Lineal } });
    }
    Evolve::~Evolve() = default;

    void Evolve::posaTempo (const std::atomic<double>* t) noexcept { tempo = t; if (rev != nullptr) rev->posaTempo (t); }

    InfoParametre Evolve::infoParametre (int i) const
    {
        const auto& f = info (i);
        InfoParametre r;
        r.id = f.clau;
        r.nom = f.nom;
        if (const int m = modulDeParam (i); m >= 0 && i != paramOn (m))
        {
            std::string n = nomModul (m);
            for (size_t k = 1; k < n.size(); ++k) n[k] = (char) std::tolower ((unsigned char) n[k]);
            r.nom = n + " · " + f.nom;
        }
        r.passos = f.enter ? (int) (f.mx - f.mn) + 1 : 0;
        r.automatitzable = i != Vista;
        return r;
    }
    void Evolve::posaParametre (int i, float v01) noexcept { posa (i, deNormal (i, v01)); }
    float Evolve::parametre (int i) const noexcept { return normal (i, valor (i)); }
    std::string Evolve::textParametre (int i, float v01) const
    {
        float v = deNormal (i, v01);
        if (info (i).enter) v = (float) std::lround (v);
        return textValor (i, v);
    }
    float Evolve::normal (int p, float v) noexcept
    {
        const auto& f = info (p);
        if (f.log) return lim ((float) (std::log (v / f.mn) / std::log (f.mx / f.mn)), 0.0f, 1.0f);
        return lim ((v - f.mn) / (f.mx - f.mn), 0.0f, 1.0f);
    }
    float Evolve::deNormal (int p, float x) noexcept
    {
        const auto& f = info (p);
        x = lim (x, 0.0f, 1.0f);
        return f.log ? f.mn * std::pow (f.mx / f.mn, x) : f.mn + (f.mx - f.mn) * x;
    }
    void Evolve::posa (int p, float v) noexcept
    {
        if (p < 0 || p >= numParams || ! std::isfinite (v)) return;
        v = lim (v, infos[p].mn, infos[p].mx);
        if (infos[p].enter) v = (float) std::lround (v);
        par[(size_t) p].store (v, std::memory_order_relaxed);
    }
    float Evolve::valor (int p) const noexcept { return p >= 0 && p < numParams ? par[(size_t) p].load (std::memory_order_relaxed) : 0.0f; }

    float Evolve::efectiu (int p, float evo) const noexcept
    {
        if (p < 0 || p >= numParams) return 0.0f;
        float v = valor (p);
        const int n = std::min (nRutes.load (std::memory_order_acquire), maxRutes);
        for (int i = 0; i < n; ++i)
            if (rt[(size_t) i].param.load (std::memory_order_relaxed) == p)
            {
                const Ruta r { p, rt[(size_t) i].de.load (std::memory_order_relaxed), rt[(size_t) i].a.load (std::memory_order_relaxed),
                               rt[(size_t) i].valor.load (std::memory_order_relaxed), rt[(size_t) i].corba.load (std::memory_order_relaxed) };
                v = valorEnEvolution (r, v, evo);
            }
        v = lim (v, infos[p].mn, infos[p].mx);
        if (infos[p].enter) v = (float) std::lround (v);
        return v;
    }

    std::vector<Ruta> Evolve::rutes() const
    {
        std::vector<Ruta> v;
        const int n = std::min (nRutes.load (std::memory_order_acquire), maxRutes);
        for (int i = 0; i < n; ++i)
            v.push_back ({ rt[(size_t) i].param.load(), rt[(size_t) i].de.load(), rt[(size_t) i].a.load(), rt[(size_t) i].valor.load(), rt[(size_t) i].corba.load() });
        return v;
    }
    void Evolve::posaRutes (const std::vector<Ruta>& v)
    {
        nRutes.store (0, std::memory_order_release);   // el fil d'àudio deixa de llegir-les mentre canvien
        int k = 0;
        for (const auto& r : v)
        {
            if (k >= maxRutes) break;
            if (! esModulable (r.param)) continue;
            auto& d = rt[(size_t) k++];
            d.param.store (r.param);
            d.de.store (lim (r.de, 0.0f, 100.0f));
            d.a.store (lim (r.a, 0.0f, 100.0f));
            d.valor.store (lim (r.valor, infos[r.param].mn, infos[r.param].mx));
            d.corba.store (std::clamp (r.corba, 0, numCorbes - 1));
        }
        nRutes.store (k, std::memory_order_release);
    }
    std::array<int, numModuls> Evolve::ordre() const noexcept
    {
        std::array<int, numModuls> o {};
        for (int i = 0; i < numModuls; ++i) o[(size_t) i] = ord[(size_t) i].load (std::memory_order_relaxed);
        return o;
    }
    void Evolve::posaOrdre (const std::array<int, numModuls>& o) noexcept
    {
        // ha de ser una permutació; si no, l'ordre de sempre
        const auto& sempre = ordreMusical;
        bool vist[numModuls] {};
        bool bo = true;
        for (int m : o) { if (m < 0 || m >= numModuls || vist[m]) { bo = false; break; } vist[m] = true; }
        for (int i = 0; i < numModuls; ++i) ord[(size_t) i].store (bo ? o[(size_t) i] : sempre[(size_t) i], std::memory_order_relaxed);
    }

    std::string Evolve::estat() const
    {
        std::string s = "mibase-evolve=1";
        char b[96];
        for (int i = 0; i < numParams; ++i)
        {
            std::snprintf (b, sizeof b, ";%s=%.6g", infos[i].clau, (double) valor (i));
            s += b;
        }
        s += ";ordre=";
        const auto o = ordre();
        for (int i = 0; i < numModuls; ++i) s += (i > 0 ? "," : "") + std::to_string (o[(size_t) i]);
        s += ";lfo=";
        {
            bool pr = true;
            for (const auto& a : assignacions())
            {
                std::snprintf (b, sizeof b, "%s%d:%s:%.6g", pr ? "" : "|", a.lfo, infos[a.param].clau, (double) a.quant);
                s += b;
                pr = false;
            }
        }
        s += ";rutes=";
        bool primera = true;
        for (const auto& r : rutes())
        {
            std::snprintf (b, sizeof b, "%s%s:%.6g:%.6g:%.6g:%d", primera ? "" : "|", infos[r.param].clau, (double) r.de, (double) r.a, (double) r.valor, r.corba);
            s += b;
            primera = false;
        }
        return s;
    }

    void Evolve::posaEstat (const std::string& s)
    {
        if (s.rfind ("mibase-evolve=", 0) != 0) return;
        for (int i = 0; i < numParams; ++i) posa (i, infos[i].def);
        std::vector<int> ol;   // l'ordre desat (pot tenir menys mòduls: estats de la 1.13)
        std::vector<Ruta> rs;
        std::vector<Assignacio> asg;
        size_t a = 0;
        while (a < s.size())
        {
            size_t b = s.find (';', a);
            if (b == std::string::npos) b = s.size();
            const auto parell = s.substr (a, b - a);
            const auto eq = parell.find ('=');
            if (eq != std::string::npos)
            {
                const auto clau = parell.substr (0, eq), val = parell.substr (eq + 1);
                if (clau == "ordre")
                {
                    size_t c = 0;
                    while (c <= val.size() && (int) ol.size() < numModuls)
                    {
                        size_t d = val.find (',', c);
                        if (d == std::string::npos) d = val.size();
                        if (d > c) ol.push_back (std::atoi (val.substr (c, d - c).c_str()));
                        c = d + 1;
                    }
                }
                else if (clau == "lfo")
                {
                    size_t c = 0;
                    while (c < val.size())
                    {
                        size_t d = val.find ('|', c);
                        if (d == std::string::npos) d = val.size();
                        const auto t = val.substr (c, d - c);
                        const auto p1 = t.find (':'), p2 = p1 == std::string::npos ? p1 : t.find (':', p1 + 1);
                        if (p2 != std::string::npos)
                        {
                            Assignacio a;
                            a.lfo = std::atoi (t.substr (0, p1).c_str());
                            a.param = paramDeClau (t.substr (p1 + 1, p2 - p1 - 1));
                            a.quant = (float) std::strtod (t.c_str() + p2 + 1, nullptr);
                            if (a.param >= 0) asg.push_back (a);
                        }
                        c = d + 1;
                    }
                }
                else if (clau == "rutes")
                {
                    size_t c = 0;
                    while (c < val.size())
                    {
                        size_t d = val.find ('|', c);
                        if (d == std::string::npos) d = val.size();
                        const auto t = val.substr (c, d - c);
                        const auto p1 = t.find (':');
                        if (p1 != std::string::npos)
                        {
                            Ruta r;
                            r.param = paramDeClau (t.substr (0, p1));
                            float x[3] { 0, 100, 0 }; int corba = 0;
                            std::sscanf (t.c_str() + p1 + 1, "%f:%f:%f:%d", &x[0], &x[1], &x[2], &corba);
                            r.de = x[0]; r.a = x[1]; r.valor = x[2]; r.corba = corba;
                            if (r.param >= 0) rs.push_back (r);
                        }
                        c = d + 1;
                    }
                }
                else if (const int p = paramDeClau (clau); p >= 0)
                    posa (p, (float) std::strtod (val.c_str(), nullptr));
            }
            a = b + 1;
        }
        // els mòduls que l'estat no diu (per exemple, els nous en un estat de la 1.13) van al final, en l'ordre de sempre
        std::array<int, numModuls> o {};
        {
            bool vist[numModuls] {};
            int k = 0;
            for (int m : ol) if (m >= 0 && m < numModuls && ! vist[m] && k < numModuls) { vist[m] = true; o[(size_t) k++] = m; }
            const auto& nous = ordreMusical;
            if (ol.empty()) for (int m : nous) { vist[m] = true; o[(size_t) k++] = m; }
            for (int m : nous) if (! vist[m] && k < numModuls) { vist[m] = true; o[(size_t) k++] = m; }
        }
        posaOrdre (o);
        posaRutes (rs);
        posaAssignacions (asg);
    }

    std::vector<Assignacio> Evolve::assignacions() const
    {
        std::vector<Assignacio> v;
        const int n = std::min (nAssig.load (std::memory_order_acquire), maxAssig);
        for (int i = 0; i < n; ++i) v.push_back ({ as[(size_t) i].lfo.load(), as[(size_t) i].param.load(), as[(size_t) i].quant.load() });
        return v;
    }
    void Evolve::posaAssignacions (const std::vector<Assignacio>& v)
    {
        nAssig.store (0, std::memory_order_release);
        int k = 0;
        for (const auto& a : v)
        {
            if (k >= maxAssig) break;
            if (! esModulable (a.param) || (a.param >= L1Forma && a.param <= L2Offset)) continue;   // un LFO no modula els LFO
            auto& d = as[(size_t) k++];
            d.lfo.store (std::clamp (a.lfo, 0, numLfos - 1));
            d.param.store (a.param);
            d.quant.store (lim (a.quant, -100.0f, 100.0f));
        }
        nAssig.store (k, std::memory_order_release);
    }

    // ---------------------------------------------------------------------------------------------
    namespace
    {
        long long potencia2b (double x) { long long p = 1; while ((double) p < x) p <<= 1; return p; }
        inline float llegeix (const std::vector<float>& b, long long mascara, double pos) noexcept
        {
            const double f = std::floor (pos);
            const long long i = (long long) f;
            const float t = (float) (pos - f);
            const float a = b[(size_t) (i & mascara)], c = b[(size_t) ((i + 1) & mascara)];
            return a + (c - a) * t;
        }
        unsigned long long barreja (unsigned long long z) noexcept
        {
            z += 0x9E3779B97F4A7C15ull;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull; z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            return z ^ (z >> 31);
        }
        float atzar01 (unsigned long long& z) noexcept { z = barreja (z); return (float) ((double) (z >> 11) / 9007199254740992.0); }
        constexpr double maxTrosRev = 4.0;   // segons: el tros més llarg del Reverser
    }

    void Evolve::prepara (double nouFs, int)
    {
        fs = nouFs > 0.0 ? nouFs : 48000.0;
        rev->prepara (fs, subBloc);
        revSw->prepara (fs, subBloc);
        revSw->posa (reverb::Alg, (float) reverb::Hall);
        revSw->posa (reverb::Mix, 100.0f);
        revSw->posa (reverb::PreDelay, 0.0f);
        revSw->posa (reverb::Size, 85.0f);
        revSw->posa (reverb::LowCut, 150.0f);
        for (auto& c : scr.ch)
        {
            c = Screamer::Canal {};
            static const double qs[4] = { 0.5098, 0.6013, 0.9000, 2.5629 };
            for (int k = 0; k < 4; ++k) { S::passaBaixos (c.up[k], 0.45 * fs, qs[k], fs * 4.0); c.down[k] = c.up[k]; }
            S::passaAlts (c.dc, 10.0, 0.7071, fs * 4.0);
        }
        // 1.14: memòria dels mòduls nous (tota aquí: al fil d'àudio no se'n demana)
        rvs.cap = (int) (maxTrosRev * fs) + 16;
        for (int c = 0; c < 2; ++c) { rvs.rec[c].assign ((size_t) rvs.cap, 0.0f); rvs.play[c].assign ((size_t) rvs.cap, 0.0f); }
        pit.an.prepara (fs);
        for (auto& v : pit.veu) v.prepara (pit.an);
        pit.veuPreparada = true;
        {
            const long long m = potencia2b (2.0 * pit.an.latencia() + 16);
            for (auto& v : pit.sec) v.assign ((size_t) m, 0.0f);
            pit.secMascara = m - 1; pit.secW = 0;
            pit.finestra = std::round (0.045 * fs);
            const long long mb = potencia2b (4.0 * pit.finestra + 64);
            for (auto& v : pit.buf) v.assign ((size_t) mb, 0.0f);
            pit.mascara = mb - 1; pit.w = 0;
        }
        {
            const long long m = potencia2b (9.5 * fs);
            for (auto& v : sca.buf) v.assign ((size_t) m, 0.0f);
            sca.mascara = m - 1; sca.w = 0; sca.valid = 0;
        }
        {
            const long long m = potencia2b (8.0 * fs);   // 1.15: el Granular (posició + dispersió + gra transposat)
            for (auto& v : gra.buf) v.assign ((size_t) m, 0.0f);
            gra.mascara = m - 1; gra.w = 0; gra.valid = 0;
        }
        {
            const long long m = potencia2b ((2.0 * maxTrosRev + 0.2) * fs + 2.0 * subBloc);
            retL.assign ((size_t) m, 0.0f); retR.assign ((size_t) m, 0.0f);
            retMascara = m - 1; retW = 0;
        }
        latRevAra = latPitAra = -1;
        scr.toneVist = -1.0f; scr.modeVist = -1;
        shmMixVist = -1.0f;
        for (auto& g : nivIn) g = 1.0f;
        for (auto& g : nivOut) g = 1.0f;
        primer = true;
        neteja();
    }

    void Evolve::neteja() noexcept
    {
        for (auto& c : scr.ch)
            for (Bq* b : { &c.up[0], &c.up[1], &c.up[2], &c.up[3], &c.down[0], &c.down[1], &c.down[2], &c.down[3], &c.dc, &c.tilt, &c.lp })
                b->neteja();
        scr.mix = 0.0f; scr.eIn = scr.eOut = 0.0f; scr.comp = -1.0f;   // -1: s'estima en començar
        for (int c = 0; c < 2; ++c) fil.s1[c] = fil.s2[c] = 0.0f;
        fil.mix = 0.0f;
        pmp.gL = pmp.gR = pmp.gate = 1.0f; pmp.depth = 0.0f;
        if (rev != nullptr) rev->buida();
        // 1.14
        rvs.recN = rvs.playN = 0; rvs.ph = 0.0; rvs.tros = LLONG_MIN; rvs.mix = 0.0f;
        if (revSw != nullptr) revSw->buida();
        pit.mix = 0.0f; pit.niv2 = 0.0f;
        if (latenciaPit() == 0)   // en mode Veu el retard no s'atura mai (si no, quedaria un forat)
            for (auto& b : pit.buf) std::fill (b.begin(), b.end(), 0.0f);
        sca.valid = sca.w; sca.tros = LLONG_MIN; sca.mix = 0.0f; sca.tipus = 0;
        gra.valid = gra.w; gra.mix = 0.0f; gra.falta = 0.0;
        for (auto& g : gra.g) g.actiu = false;
        for (auto& g : nivIn) g = 1.0f;
        for (auto& g : nivOut) g = 1.0f;
    }

    int Evolve::latenciaRev (double bpm) const noexcept
    {
        if (valor (RevOn) < 0.5f || (int) valor (RevMode) != 1) return 0;
        if (! (bpm > 1.0)) bpm = 120.0;
        const double t = std::min (maxTrosRev, divisioEnNegres ((int) valor (RevDiv)) * 60.0 / bpm);
        return 2 * (int) std::lround (t * fs);
    }
    int Evolve::latenciaPit() const noexcept
    {
        if (! pit.veuPreparada || valor (PitOn) < 0.5f || (int) valor (PitMode) != 1) return 0;
        return pit.an.latencia();
    }
    int Evolve::latencia() const noexcept
    {
        const double bpm = tempo != nullptr ? tempo->load (std::memory_order_relaxed) : 120.0;
        return latenciaRev (bpm) + latenciaPit();
    }

    void Evolve::calculaEfectius (float evo) noexcept
    {
        for (int p = 0; p < numParams; ++p) ef[(size_t) p] = par[(size_t) p].load (std::memory_order_relaxed);
        const int n = std::min (nRutes.load (std::memory_order_acquire), maxRutes);
        for (int i = 0; i < n; ++i)
        {
            const int p = rt[(size_t) i].param.load (std::memory_order_relaxed);
            if (p < 0 || p >= numParams) continue;
            const Ruta r { p, rt[(size_t) i].de.load (std::memory_order_relaxed), rt[(size_t) i].a.load (std::memory_order_relaxed),
                           rt[(size_t) i].valor.load (std::memory_order_relaxed), rt[(size_t) i].corba.load (std::memory_order_relaxed) };
            ef[(size_t) p] = valorEnEvolution (r, ef[(size_t) p], evo);
        }
        for (int p = 0; p < numParams; ++p)
        {
            float v = lim (ef[(size_t) p], infos[p].mn, infos[p].mx);
            if (infos[p].enter) v = (float) std::lround (v);
            ef[(size_t) p] = v;
        }
    }

    void Evolve::calculaLfos (int n, double negres, double bpm) noexcept
    {
        for (int k = 0; k < numLfos; ++k)
        {
            const int forma = (int) ef[(size_t) paramLfo (k, 0)];
            const bool sync = ef[(size_t) paramLfo (k, 1)] > 0.5f;
            const float fase0 = ef[(size_t) paramLfo (k, 5)] / 360.0f;
            double cicles;
            if (sync) cicles = negres / divisioEnNegres ((int) ef[(size_t) paramLfo (k, 3)]) + fase0;
            else
            {
                faseLliure[k] += ef[(size_t) paramLfo (k, 2)] * n / fs;
                faseLliure[k] -= std::floor (faseLliure[k]);
                cicles = faseLliure[k] + fase0;
            }
            const long long cicle = (long long) std::floor (cicles);
            const float f = (float) (cicles - (double) cicle);
            const float v = lim (formaLfo (forma, f, cicle) * ef[(size_t) paramLfo (k, 4)] * 0.01f + ef[(size_t) paramLfo (k, 6)] * 0.01f, -1.0f, 1.0f);
            valLfo[k] = v;
            mes.lfo[(size_t) k].store (v, std::memory_order_relaxed);
            mes.faseLfo[(size_t) k].store (f, std::memory_order_relaxed);
        }
        (void) bpm;
        const int na = std::min (nAssig.load (std::memory_order_acquire), maxAssig);
        for (int i = 0; i < na; ++i)
        {
            const int p = as[(size_t) i].param.load (std::memory_order_relaxed);
            if (p < 0 || p >= numParams) continue;
            const float q = as[(size_t) i].quant.load (std::memory_order_relaxed) * 0.01f * valLfo[std::clamp (as[(size_t) i].lfo.load (std::memory_order_relaxed), 0, numLfos - 1)];
            const auto& in = infos[p];
            float v = ef[(size_t) p];
            if (in.log && v > 0.0f) v = std::exp (std::log (v) + q * std::log (in.mx / in.mn));
            else v += q * (in.mx - in.mn);
            v = lim (v, in.mn, in.mx);
            if (in.enter) v = (float) std::lround (v);
            ef[(size_t) p] = v;
        }
    }

    void Evolve::processa (float* L, float* R, int n) noexcept
    {
        if (n <= 0) return;
        mes.blocs.fetch_add (1, std::memory_order_relaxed);
        if (demanaBuidar.exchange (false)) neteja();
        double bpm = tempo != nullptr ? tempo->load (std::memory_order_relaxed) : 120.0;
        if (! (bpm > 1.0)) bpm = 120.0;
        // on és la cançó (en negres): si sona, del transport; si no (o exportant), el comptador propi continua
        if (ctx != nullptr && ctx->sonant.load (std::memory_order_relaxed)) negresLliure = ctx->segons.load (std::memory_order_relaxed) * bpm / 60.0;
        double negres = negresLliure;
        const double negresPerMostra = bpm / 60.0 / fs;
        float picE = 0.0f, picS = 0.0f, prot = 0.0f;
        std::array<float, numModuls> act {};
        // 1.14: la latència (Reverse reverb i Pitch Veu). L'entrada passa sempre pel retard general.
        const int latR = retL.empty() ? 0 : latenciaRev (bpm), latP = latenciaPit();
        latRevAra = latR; latPitAra = latP;
        const int latT = (int) std::min<long long> (latR + latP, retMascara - 2 * subBloc);
        const double negresEntrada = latT * negresPerMostra;   // l'entrada va «latT» mostres per davant del que se sent
        const int solo1 = solo.load (std::memory_order_relaxed);

        for (int o = 0; o < n; o += subBloc)
        {
            const int m = std::min (subBloc, n - o);
            float* l = L + o;
            float* r = R + o;
            float dl[subBloc], dr[subBloc];
            for (int i = 0; i < m; ++i)
            {
                rawL[i] = l[i]; rawR[i] = r[i];
                picE = std::max (picE, std::max (std::fabs (l[i]), std::fabs (r[i])));
                if (! retL.empty())
                {
                    const long long w = retW++;
                    retL[(size_t) (w & retMascara)] = rawL[i];
                    retR[(size_t) (w & retMascara)] = rawR[i];
                    const long long a = w - latR, b = w - latT;
                    l[i] = latR > 0 ? retL[(size_t) (a & retMascara)] : rawL[i];
                    r[i] = latR > 0 ? retR[(size_t) (a & retMascara)] : rawR[i];
                    dl[i] = latT > 0 ? retL[(size_t) (b & retMascara)] : rawL[i];
                    dr[i] = latT > 0 ? retR[(size_t) (b & retMascara)] : rawR[i];
                }
                else { dl[i] = l[i]; dr[i] = r[i]; }
            }

            // Evolution que sona: la del pom (o la retinguda pel Hold), suavitzada
            const bool hold = valor (Hold) > 0.5f;
            if (hold && ! holdAbans) evoHold = evoS;   // es queda el que sona ara
            holdAbans = hold;
            const float evoObj = hold ? evoHold : valor (Evolution);
            const float kObj = evoObj > 0.0f ? 1.0f : 0.0f;
            if (primer) { evoS = evoObj; kBy = kObj; primer = false; }
            evoS += (evoObj - evoS) * (1.0f - std::exp (-(float) m / (float) (0.04 * fs)));
            if (std::fabs (evoObj - evoS) < 1e-4f) evoS = evoObj;

            if (kBy <= 0.0f && kObj <= 0.0f)
            {
                // 0 %: bypass real, el so surt tal qual (només amb el retard que Mibase ja compensa)
                brut = true;
                if (latP > 0)
                {
                    // el Pitch en mode Veu no es pot aturar (si no, en tornar hi hauria un forat): continua escoltant
                    calculaEfectius (evoS);
                    processaPitch (l, r, m, true);
                }
                for (int i = 0; i < m; ++i)
                {
                    l[i] = dl[i]; r[i] = dr[i];
                    picS = std::max (picS, std::max (std::fabs (l[i]), std::fabs (r[i])));
                }
                negres += m * negresPerMostra;
                continue;
            }
            if (brut) { neteja(); brut = false; }

            calculaEfectius (evoS);
            calculaLfos (m, negres, bpm);

            const float gIn0 = gIn, gIn1 = dB (ef[Entrada]);
            for (int i = 0; i < m; ++i)
            {
                const float g = gIn0 + (gIn1 - gIn0) * (float) (i + 1) / (float) m;
                l[i] *= g; r[i] *= g;
            }
            gIn = gIn1;

            const auto o2 = ordre();
            for (int k = 0; k < numModuls; ++k)
            {
                const int mod = o2[(size_t) k];
                const bool on = ef[(size_t) paramOn (mod)] > 0.5f;
                const bool callat = solo1 >= 0 && solo1 != mod;   // escoltant un altre mòdul sol
                if (! on || callat)
                {
                    if (mod == MPitch && latP > 0) processaPitch (l, r, m, true);   // el retard s'ha de mantenir
                    continue;
                }
                act[(size_t) mod] = std::max (act[(size_t) mod], ef[(size_t) paramMix (mod)] / 100.0f);
                // nivell d'entrada del mòdul
                const float gi0 = nivIn[(size_t) mod], gi1 = dB (ef[(size_t) (NivIn + mod)]);
                if (gi0 != 1.0f || gi1 != 1.0f)
                    for (int i = 0; i < m; ++i) { const float g = gi0 + (gi1 - gi0) * (float) (i + 1) / (float) m; l[i] *= g; r[i] *= g; }
                nivIn[(size_t) mod] = gi1;
                // un mòdul amb Mix a 0 (i que ja no sona) no es calcula: no gasta CPU
                const bool vol = ef[(size_t) paramMix (mod)] > 0.0f;
                switch (mod)
                {
                    case MScreamer: if (vol || scr.mix > 0.0f) processaScreamer (l, r, m); break;
                    case MFiltre: if (vol || fil.mix > 0.0f) processaFiltre (l, r, m, negres, bpm); break;
                    case MPumper: if (vol || pmp.depth > 0.0f || pmp.gL != 1.0f || pmp.gR != 1.0f) processaPumper (l, r, m, negres, bpm); break;
                    case MShimmer: if (vol || shmMixVist > 0.0f) processaShimmer (l, r, m); break;
                    case MReverser: processaReverser (l, r, m, negres + negresEntrada, bpm); break;   // sempre grava (és barat)
                    case MPitch: if (vol || pit.mix > 0.0f || latP > 0) processaPitch (l, r, m, false); break;
                    case MGranular: processaGranular (l, r, m); break;   // sempre grava (i els grans acaben de sonar)
                    default: processaScatter (l, r, m, negres + negresEntrada, bpm); break;            // sempre grava
                }
                const float go0 = nivOut[(size_t) mod], go1 = dB (ef[(size_t) (NivOut + mod)]);
                if (go0 != 1.0f || go1 != 1.0f)
                    for (int i = 0; i < m; ++i) { const float g = go0 + (go1 - go0) * (float) (i + 1) / (float) m; l[i] *= g; r[i] *= g; }
                nivOut[(size_t) mod] = go1;
            }

            // Dry/Wet, sortida, protecció i el pas suau cap al bypass
            const float dw0 = dw, dw1 = ef[DryWet] * 0.01f, gO0 = gOut, gO1 = dB (ef[Sortida]);
            const float pasK = (float) (1.0 / (0.01 * fs));
            for (int i = 0; i < m; ++i)
            {
                const float t = (float) (i + 1) / (float) m;
                const float w = dw0 + (dw1 - dw0) * t, go = gO0 + (gO1 - gO0) * t;
                kBy = kObj > kBy ? std::min (kObj, kBy + pasK) : std::max (kObj, kBy - pasK);
                float pl = dl[i] * (1.0f - w) + l[i] * go * w;
                float pr = dr[i] * (1.0f - w) + r[i] * go * w;
                if (! std::isfinite (pl)) pl = 0.0f;
                if (! std::isfinite (pr)) pr = 0.0f;
                const float ql = protegeix (pl), qr = protegeix (pr);
                if (ql != pl || qr != pr) prot = 1.0f;
                l[i] = dl[i] + kBy * (ql - dl[i]);
                r[i] = dr[i] + kBy * (qr - dr[i]);
                picS = std::max (picS, std::max (std::fabs (l[i]), std::fabs (r[i])));
            }
            dw = dw1; gOut = gO1;
            negres += m * negresPerMostra;
            if (o + m >= n)   // 1.15: el que sona ara (per a la finestra)
                for (int p = 0; p < numParams; ++p) mes.efectiu[(size_t) p].store (ef[(size_t) p], std::memory_order_relaxed);
        }
        negresLliure = negres;

        mes.entrada.store (std::max (picE, mes.entrada.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
        mes.sortida.store (std::max (picS, mes.sortida.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
        mes.evolution.store (evoS, std::memory_order_relaxed);
        mes.bypass.store (1.0f - kBy, std::memory_order_relaxed);
        mes.protegeix.store (std::max (prot, mes.protegeix.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
        for (int k = 0; k < numModuls; ++k) mes.activitat[(size_t) k].store (kBy > 0.0f ? act[(size_t) k] : 0.0f, std::memory_order_relaxed);
    }

    // ---- SCREAMER ----
    void Evolve::processaScreamer (float* L, float* R, int n) noexcept
    {
        const bool on = ef[ScrOn] > 0.5f;
        const int mode = (int) ef[ScrMode];
        const float tone = ef[ScrTone];
        if (mode != scr.modeVist || std::fabs (tone - scr.toneVist) > 0.25f)
        {
            scr.modeVist = mode; scr.toneVist = tone;
            const double fLp = std::min (0.45 * fs, (mode == 3 ? 2500.0 : 3500.0) * std::pow (5.0, tone / 100.0));
            for (auto& c : scr.ch)
            {
                S::prestatgeAlt (c.tilt, 2500.0, (tone - 50.0f) * 0.2f, fs);
                S::passaBaixos (c.lp, fLp, 0.7071, fs);
            }
        }
        const float gObj = dB (ef[ScrDrive] * 0.36f);
        const float mixObj = on ? ef[ScrMix] * 0.01f : 0.0f, outObj = dB (ef[ScrOut]);
        const float g0 = scr.g, m0 = scr.mix, o0 = scr.out;
        if (scr.comp < 0.0f) scr.comp = std::pow (std::max (gObj, 1.0f), -0.6f);
        // Volum automàtic: el so distorsionat surt amb la mateixa energia que el que entra (mesurada a ~300 ms).
        const float aEnv = 1.0f - std::exp (-1.0f / (0.3f * (float) fs));
        float* io[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) (i + 1) / (float) n;
            const float g = g0 + (gObj - g0) * t, mx = m0 + (mixObj - m0) * t, ou = o0 + (outObj - o0) * t;
            float y2[2];
            for (int c = 0; c < 2; ++c)
            {
                auto& k = scr.ch[c];
                const float v = io[c][i] * g;
                float y = 0.0f;
                for (int s = 0; s < 4; ++s)
                {
                    float u = s == 0 ? v * 4.0f : 0.0f;
                    for (auto& f : k.up) u = f (u);
                    switch (mode)
                    {
                        case 0: u = std::tanh (u); break;
                        case 1: u = drive::corba (drive::Overdrive, u); break;
                        case 2: u = drive::corba (drive::Distorsio, u); break;
                        case 3: u = drive::corba (drive::Fuzz, u); break;
                        default: u = std::sin (u); break;   // plec: passat de ±π/2 l'ona es doblega
                    }
                    u = k.dc (u);
                    for (auto& f : k.down) u = f (u);
                    if (s == 0) y = u;
                }
                y2[c] = k.lp (k.tilt (y));
            }
            const float xe = 0.5f * (io[0][i] * io[0][i] + io[1][i] * io[1][i]), ye = 0.5f * (y2[0] * y2[0] + y2[1] * y2[1]);
            scr.eIn += (xe - scr.eIn) * aEnv;
            scr.eOut += (ye - scr.eOut) * aEnv;
            const float objAuto = scr.eOut > 1e-9f ? lim (std::sqrt (scr.eIn / scr.eOut), 0.05f, 2.0f) : scr.comp;
            scr.comp += (objAuto - scr.comp) * aEnv * 4.0f;
            for (int c = 0; c < 2; ++c)
            {
                const float x = io[c][i];
                io[c][i] = x + mx * (y2[c] * scr.comp * ou - x);
            }
        }
        scr.g = gObj; scr.mix = mixObj; scr.out = outObj;
    }

    // ---- FILTER ----
    void Evolve::processaFiltre (float* L, float* R, int n, double negres, double bpm) noexcept
    {
        const bool on = ef[FilOn] > 0.5f;
        const int tipus = (int) ef[FilTipus];
        const double len = divisioEnNegres ((int) ef[FilLfoDiv]);
        const float oct = ef[FilLfo] * 0.03f;
        const double negresFi = negres + n * bpm / 60.0 / fs;
        const float lfo = std::sin ((float) (2.0 * pi) * fraccio (negresFi / len));
        const float f = lim (ef[FilFreq] * std::pow (2.0f, oct * lfo), 20.0f, (float) (0.45 * fs));
        const float gObj = std::tan ((float) pi * f / (float) fs);
        const float kObj = 2.0f - 1.96f * ef[FilReso] * 0.01f;
        const float mixObj = on ? ef[FilMix] * 0.01f : 0.0f;
        const float drvObj = 1.0f + ef[FilDrive] * 0.15f;
        const float g0 = fil.g, k0 = fil.k, m0 = fil.mix, d0 = fil.drv;
        float* io[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) (i + 1) / (float) n;
            const float g = g0 + (gObj - g0) * t, k = k0 + (kObj - k0) * t, mx = m0 + (mixObj - m0) * t, gd = d0 + (drvObj - d0) * t;
            const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const float comp = std::pow (std::min (1.0f, k / 1.4142f), 0.75f);
            const float normD = std::pow (gd, -0.75f);
            for (int c = 0; c < 2; ++c)
            {
                const float x = io[c][i];
                const float xd = gd > 1.001f ? std::tanh (x * gd) * normD : x;
                const float v3 = xd - fil.s2[c];
                const float v1 = a1 * fil.s1[c] + a2 * v3;
                const float v2 = fil.s2[c] + a2 * fil.s1[c] + a3 * v3;
                fil.s1[c] = 2.0f * v1 - fil.s1[c];
                fil.s2[c] = 2.0f * v2 - fil.s2[c];
                float y;
                switch (tipus)
                {
                    case 1: y = (xd - k * v1 - v2) * comp; break;
                    case 2: y = v1 * k; break;
                    case 3: y = xd - k * v1; break;
                    default: y = v2 * comp; break;
                }
                io[c][i] = x + mx * (y - x);
            }
        }
        for (int c = 0; c < 2; ++c)
        {
            if (std::fabs (fil.s1[c]) < 1e-20f || ! std::isfinite (fil.s1[c])) fil.s1[c] = 0.0f;
            if (std::fabs (fil.s2[c]) < 1e-20f || ! std::isfinite (fil.s2[c])) fil.s2[c] = 0.0f;
        }
        fil.g = gObj; fil.k = kObj; fil.mix = mixObj; fil.drv = drvObj;
    }

    // ---- PUMPER ----
    void Evolve::processaPumper (float* L, float* R, int n, double negres, double bpm) noexcept
    {
        const bool on = ef[PmpOn] > 0.5f;
        const int mode = (int) ef[PmpMode];
        const double len = divisioEnNegres ((int) ef[PmpDiv]);
        const float forma = ef[PmpForma] * 0.01f;
        const char* pat = patro ((int) ef[PmpPatro]);
        const float dObj = on ? ef[PmpDepth] * 0.01f : 0.0f, d0 = pmp.depth;
        const double pas = bpm / 60.0 / fs;
        const float msSuau = mode == 2 ? 0.5f + forma * 20.0f : 1.5f;
        const float cS = 1.0f - std::exp (-1.0f / (msSuau * 0.001f * (float) fs));
        const float expo = 1.0f + forma * 8.0f;
        for (int i = 0; i < n; ++i)
        {
            const float d = d0 + (dObj - d0) * (float) (i + 1) / (float) n;
            const double b = negres + i * pas;
            const float ph = fraccio (b / len);
            float gl = 1.0f, gr = 1.0f;
            switch (mode)
            {
                case 0: gl = gr = 1.0f - d * std::pow (1.0f - ph, expo); break;   // ducking: cau al començament i torna
                case 1:
                {
                    float s = 0.5f - 0.5f * std::cos ((float) (2.0 * pi) * ph);
                    const float dur = 1.0f + forma * 12.0f;
                    s = 0.5f + 0.5f * std::tanh ((s - 0.5f) * dur) / std::tanh (0.5f * dur);
                    gl = gr = 1.0f - d * s;
                    break;
                }
                case 2:
                {
                    const long long pasN = (long long) std::floor (b / len);
                    const int k = (int) (((pasN % 16) + 16) % 16);
                    gl = gr = pat[k] == 'x' ? 1.0f : 1.0f - d;
                    break;
                }
                default:
                {
                    const float s = std::sin ((float) (2.0 * pi) * ph);
                    gl = 1.0f - d * std::max (0.0f, s);
                    gr = 1.0f - d * std::max (0.0f, -s);
                    break;
                }
            }
            pmp.gL += (gl - pmp.gL) * cS;
            pmp.gR += (gr - pmp.gR) * cS;
            L[i] *= pmp.gL;
            R[i] *= pmp.gR;
        }
        pmp.depth = dObj;
        if (dObj <= 0.0f && std::fabs (pmp.gL - 1.0f) < 1e-4f && std::fabs (pmp.gR - 1.0f) < 1e-4f) pmp.gL = pmp.gR = 1.0f;
    }

    // ---- SHIMMER (la Reverb de Mibase per dins) ----
    void Evolve::processaShimmer (float* L, float* R, int n) noexcept
    {
        const bool on = ef[ShmOn] > 0.5f;
        static const int algs[3] = { reverb::Shimmer, reverb::Hall, reverb::Ambient };
        const float alg = (float) algs[std::clamp ((int) ef[ShmTipus], 0, 2)];
        auto posaSiCal = [this] (int p, float v) { if (rev->valor (p) != v) rev->posa (p, v); };
        posaSiCal (reverb::Alg, alg);
        posaSiCal (reverb::ShimOn, 1.0f);
        posaSiCal (reverb::ShimPitch, ef[ShmPitch]);
        posaSiCal (reverb::ShimAmt, ef[ShmAmt]);
        posaSiCal (reverb::Decay, ef[ShmDecay]);
        posaSiCal (reverb::Size, ef[ShmSize]);
        posaSiCal (reverb::Diffusion, ef[ShmDiff]);
        posaSiCal (reverb::ModDepth, ef[ShmMod]);
        posaSiCal (reverb::Damping, ef[ShmDamp]);
        posaSiCal (reverb::FreezeOn, ef[ShmFreeze] > 0.5f ? 1.0f : 0.0f);
        posaSiCal (reverb::LowCut, 120.0f);
        posaSiCal (reverb::PreDelay, 15.0f);
        const float mix = on ? ef[ShmMix] : 0.0f;
        posaSiCal (reverb::Mix, mix);
        rev->processa (L, R, n);
        shmMixVist = mix;
    }

    // ---- 1.14: REVERSER ----
    void Evolve::processaReverser (float* L, float* R, int n, double negres, double bpm) noexcept
    {
        if (rvs.cap <= 0) return;
        const int mode = (int) valor (RevMode);   // la base (canvia la latència: no va amb rutes)
        const double len = divisioEnNegres ((int) valor (RevDiv));
        const double pasN = bpm / 60.0 / fs;
        const int trosMax = rvs.cap - 2;
        const float mixObj = ef[RevMix] * 0.01f, m0 = rvs.mix;
        const float fb = ef[RevFeedback] * 0.01f;
        static const double vels[3] = { 1.0, 2.0, 0.5 };
        const double vel = vels[std::clamp ((int) ef[RevPitch], 0, 2)];
        const double fade = 0.005 * fs;
        // Reverse reverb: la cua (de l'entrada que encara no se sent: va per davant) que després s'inverteix
        float cua[2][subBloc];
        const bool calCua = mode == 1 && (mixObj > 0.0f || m0 > 0.0f);
        if (calCua)
        {
            if (revSw->valor (reverb::Decay) != ef[RevDecay]) revSw->posa (reverb::Decay, ef[RevDecay]);
            std::copy (rawL, rawL + n, cua[0]);
            std::copy (rawR, rawR + n, cua[1]);
            revSw->processa (cua[0], cua[1], n);
        }
        for (int i = 0; i < n; ++i)
        {
            const double b = negres + i * pasN;
            const long long t = (long long) std::floor (b / len);
            if (t != rvs.tros)
            {
                // tros nou: el que s'acaba de gravar passa a sonar (al revés)
                std::swap (rvs.rec[0], rvs.play[0]);
                std::swap (rvs.rec[1], rvs.play[1]);
                rvs.playN = rvs.tros == LLONG_MIN ? 0 : rvs.recN;
                rvs.recN = 0; rvs.ph = 0.0; rvs.tros = t;
            }
            float s[2] = { 0.0f, 0.0f };
            if (rvs.playN > 8)
            {
                const double N = (double) rvs.playN;
                const double pos = std::fmod (rvs.ph, N);
                const double idx = N - 1.0 - pos;
                const double transcorregut = rvs.ph / vel;   // mostres des que ha començat el tros
                const float fin = (float) std::min ({ 1.0, pos / fade, (N - pos) / fade, (N - transcorregut) / fade });
                const long long i0 = (long long) idx;
                const float fr = (float) (idx - (double) i0);
                for (int c = 0; c < 2; ++c)
                {
                    const float a = rvs.play[c][(size_t) i0], d = rvs.play[c][(size_t) std::min<long long> (i0 + 1, rvs.playN - 1)];
                    s[c] = (a + (d - a) * fr) * std::max (0.0f, fin);
                }
                rvs.ph += vel;
            }
            const float mx = m0 + (mixObj - m0) * (float) (i + 1) / (float) n;
            const float x[2] = { L[i], R[i] };
            if (rvs.recN < trosMax)
            {
                for (int c = 0; c < 2; ++c)
                    rvs.rec[c][(size_t) rvs.recN] = mode == 1 ? (calCua ? cua[c][i] : 0.0f) : x[c] + fb * s[c];
                ++rvs.recN;
            }
            if (mode == 1) { L[i] = x[0] + mx * 1.4f * s[0]; R[i] = x[1] + mx * 1.4f * s[1]; }   // la cua puja i el so sec es queda
            else { L[i] = x[0] + mx * (s[0] - x[0]); R[i] = x[1] + mx * (s[1] - x[1]); }
        }
        rvs.mix = mixObj;
    }

    // ---- 1.14: PITCH ----
    void Evolve::processaPitch (float* L, float* R, int n, bool calla) noexcept
    {
        if (pit.buf[0].empty()) return;
        const int mode = (int) valor (PitMode);
        const float mixObj = calla ? 0.0f : ef[PitMix] * 0.01f, m0 = pit.mix;
        const float n2Obj = ef[PitNiv2] * 0.01f, n20 = pit.niv2;
        const double fi = ef[PitFi] / 100.0, det = ef[PitDetune] / 100.0;
        const double s1 = ef[PitSemi1] + fi, s2 = ef[PitSemi2] + fi;
        if (mode == 1 && pit.veuPreparada && ! pit.sec[0].empty())
        {
            const int lat = pit.an.latencia();
            const double form = ef[PitFormant];
            for (int i = 0; i < n; ++i)
            {
                const float t = (float) (i + 1) / (float) n;
                const float mx = m0 + (mixObj - m0) * t, n2 = n20 + (n2Obj - n20) * t;
                const float x0 = L[i], x1 = R[i];
                pit.an.entra (0.5f * (x0 + x1));
                const float a = pit.veu[0].processa (pit.an, s1, form, 1.0f);
                const float b = pit.veu[1].processa (pit.an, s2, form, 0.0f);
                const long long w = pit.secW++;
                pit.sec[0][(size_t) (w & pit.secMascara)] = x0;
                pit.sec[1][(size_t) (w & pit.secMascara)] = x1;
                const long long rd = w - lat;
                const float d0 = rd >= 0 ? pit.sec[0][(size_t) (rd & pit.secMascara)] : 0.0f;
                const float d1 = rd >= 0 ? pit.sec[1][(size_t) (rd & pit.secMascara)] : 0.0f;
                const float wet = (a + n2 * b) / (1.0f + 0.5f * n2);
                L[i] = d0 + mx * (wet - d0);
                R[i] = d1 + mx * (wet - d1);
            }
        }
        else
        {
            const double W = pit.finestra;
            const double rt[2][2] = { { std::pow (2.0, (s1 + det) / 12.0), std::pow (2.0, (s1 - det) / 12.0) },
                                      { std::pow (2.0, (s2 + det) / 12.0), std::pow (2.0, (s2 - det) / 12.0) } };
            const double tau = 2.0 * pi;
            for (int i = 0; i < n; ++i)
            {
                const float t = (float) (i + 1) / (float) n;
                const float mx = m0 + (mixObj - m0) * t, n2 = n20 + (n2Obj - n20) * t;
                const long long w = pit.w++;
                float* io[2] = { L + i, R + i };
                for (int c = 0; c < 2; ++c)
                {
                    const float x = *io[c];
                    pit.buf[c][(size_t) (w & pit.mascara)] = x;
                    float v[2];
                    for (int k = 0; k < 2; ++k)
                    {
                        auto& d = pit.d[k][c];
                        if (std::fabs (rt[k][c] - 1.0) < 1e-6) { v[k] = x; continue; }
                        const double f1 = d.fase, f2 = f1 + 0.5 - std::floor (f1 + 0.5);
                        const float g1 = (float) (0.5 - 0.5 * std::cos (tau * f1)), g2 = 1.0f - g1;
                        v[k] = g1 * llegeix (pit.buf[c], pit.mascara, (double) w - f1 * W - 1.0)
                             + g2 * llegeix (pit.buf[c], pit.mascara, (double) w - f2 * W - 1.0);
                        d.avanca (rt[k][c], W);
                    }
                    const float wet = (v[0] + n2 * v[1]) / (1.0f + 0.5f * n2);
                    *io[c] = x + mx * (wet - x);
                }
            }
        }
        pit.mix = mixObj; pit.niv2 = n2Obj;
    }

    // ---- 1.14: SCATTER ----
    void Evolve::processaScatter (float* L, float* R, int n, double negres, double bpm) noexcept
    {
        if (sca.buf[0].empty()) return;
        const double len = divisioEnNegres ((int) ef[ScaDiv]);
        const double pasN = bpm / 60.0 / fs;
        const float mixObj = ef[ScaMix] * 0.01f, m0 = sca.mix;
        const float dens = ef[ScaDensity] * 0.01f, esc = ef[ScaScatter] * 0.01f, fb = ef[ScaFeedback] * 0.01f;
        const double fade = 0.002 * fs;
        const long long llargMax = (sca.mascara + 1) / 10;
        for (int i = 0; i < n; ++i)
        {
            const double b = negres + i * pasN;
            const long long t = (long long) std::floor (b / len);
            const long long w = sca.w;
            sca.buf[0][(size_t) (w & sca.mascara)] = L[i];
            sca.buf[1][(size_t) (w & sca.mascara)] = R[i];
            ++sca.w;
            if (t != sca.tros)
            {
                // tall nou: es decideix què li passa (sempre igual per al mateix tall: soni o s'exporti, fa el mateix)
                sca.tros = t;
                sca.iniciTros = w;
                sca.q = 0;
                sca.llarg = (int) std::clamp<long long> ((long long) std::lround (len * 60.0 / bpm * fs), 64, llargMax);
                unsigned long long z = (unsigned long long) t * 0x2545F4914F6CDD1Dull + 0x1234567ull;
                const float r1 = atzar01 (z), r2 = atzar01 (z), r3 = atzar01 (z);
                sca.tipus = 0;
                if (r1 < dens)
                {
                    if (r2 < 0.45f) sca.tipus = 1;                                  // repetició
                    else if (r2 < 0.45f + 0.25f * (0.4f + esc)) sca.tipus = 2;      // salt enrere
                    else if (r2 < 0.85f) sca.tipus = 3;                             // el tall d'abans, al revés
                    else sca.tipus = esc > 0.4f ? 4 : 1;                            // a doble velocitat
                    sca.k = r3 < 0.5f - 0.3f * esc ? 2 : (r3 < 0.85f - 0.2f * esc ? 4 : 8);
                    const int enrere = 1 + (int) (r3 * (1.0f + 7.0f * esc));
                    sca.font = sca.iniciTros - (long long) enrere * sca.llarg * (sca.tipus == 4 ? 2 : 1);
                }
            }
            const long long q = sca.q++;
            float y[2] = { L[i], R[i] };
            if (sca.tipus != 0)
            {
                long long rd = 0;
                double seg = 0.0, segLlarg = sca.llarg;
                float g = 1.0f;
                switch (sca.tipus)
                {
                    case 1:
                    {
                        const long long sub = std::max<long long> (16, sca.llarg / sca.k);
                        const long long rep = q / sub;
                        seg = (double) (q - rep * sub); segLlarg = (double) sub;
                        rd = sca.iniciTros + (q - rep * sub);
                        g = std::pow (1.0f - 0.6f * fb, (float) rep);
                        break;
                    }
                    case 2: rd = sca.font + q; seg = (double) q; break;
                    case 3: rd = sca.iniciTros - 1 - q; seg = (double) q; break;
                    default: rd = sca.font + 2 * q; seg = (double) q; break;
                }
                const float fin = (float) std::max (0.0, std::min ({ 1.0, seg / fade, (segLlarg - seg) / fade, ((double) sca.llarg - (double) q) / fade }));
                const bool bo = rd >= sca.valid && rd < sca.w && sca.w - rd <= sca.mascara;
                const float gw = std::min (1.0f, (float) std::min ((double) q, (double) sca.llarg - (double) q) / (float) fade);
                for (int c = 0; c < 2; ++c)
                {
                    const float gl = bo ? sca.buf[c][(size_t) (rd & sca.mascara)] * fin * g : 0.0f;
                    y[c] = y[c] * (1.0f - gw) + gl * gw;
                }
            }
            const float mx = m0 + (mixObj - m0) * (float) (i + 1) / (float) n;
            L[i] += mx * (y[0] - L[i]);
            R[i] += mx * (y[1] - R[i]);
        }
        sca.mix = mixObj;
    }

    // ---- 1.15: GRANULAR ----
    void Evolve::processaGranular (float* L, float* R, int n) noexcept
    {
        if (gra.buf[0].empty()) return;
        auto& G = gra;
        const float mixObj = ef[GraMix] * 0.01f, m0 = G.mix;
        const double lenBase = ef[GraSize] * 0.001 * fs;
        // el Freeze només congela quan ja hi ha prou so gravat (si no, amb Freeze des del començament no sonaria res)
        const double calGravat = ef[GraPos] * 0.001 * fs + ef[GraSpray] * 0.001 * fs + 4.0 * lenBase + 0.5 * fs;
        const bool freeze = ef[GraFreeze] > 0.5f && (double) (G.w - G.valid) >= calGravat;
        const double dens = std::max (0.5f, ef[GraDens]);
        const double interval = fs / dens;
        const double sobreposicio = std::max (1.0, dens * ef[GraSize] * 0.001);
        const float norm = (float) (1.5 / std::sqrt (sobreposicio));
        const float fb = ef[GraFeedback] * 0.01f, amplada = ef[GraWidth] * 0.01f;
        const double posBase = ef[GraPos] * 0.001 * fs, spray = ef[GraSpray] * 0.001 * fs;
        const float probRev = ef[GraReverse] * 0.01f, rp = ef[GraRandPitch] * 0.01f;
        const int quant = (int) ef[GraQuant];
        const double semBase = ef[GraPitch];
        const double tau = 2.0 * pi;
        const long long cap = G.mascara + 1;
        int actius = 0;
        if (mixObj <= 0.0f && m0 <= 0.0f)
        {
            // no sona: només grava (així, quan entri, ja té de què fer grans) i no gasta CPU en grans
            if (! freeze)
                for (int i = 0; i < n; ++i)
                {
                    G.buf[0][(size_t) (G.w & G.mascara)] = L[i];
                    G.buf[1][(size_t) (G.w & G.mascara)] = R[i];
                    ++G.w;
                }
            for (auto& g : G.g) g.actiu = false;
            mes.grans.store (0, std::memory_order_relaxed);
            return;
        }
        for (int i = 0; i < n; ++i)
        {
            // un gra nou quan toca (amb una mica d'atzar perquè no soni a màquina)
            G.falta -= 1.0;
            if (G.falta <= 0.0)
            {
                G.falta += interval * (0.7 + 0.6 * atzar01 (G.llavor));
                int lliure = -1;
                for (int k = 0; k < Granular::maxGrans; ++k) if (! G.g[k].actiu) { lliure = k; break; }
                if (lliure >= 0)
                {
                    double sem = semBase;
                    const float a = atzar01 (G.llavor), b = atzar01 (G.llavor);
                    if (rp > 0.0f)
                    {
                        if (quant == 0) sem += (a * 2.0 - 1.0) * 12.0 * rp;
                        else if (b < rp)
                        {
                            static const int oct[] = { -12, 12, 12 }, quin[] = { -12, -5, 7, 12, 12 };
                            sem += quant == 1 ? oct[(int) (a * 2.999f)] : quin[(int) (a * 4.999f)];
                        }
                    }
                    const double ratio = std::pow (2.0, sem / 12.0);
                    const bool alReves = atzar01 (G.llavor) < probRev;
                    const int len = (int) std::max (32.0, lenBase * (0.9 + 0.2 * atzar01 (G.llavor)));
                    const double cap0 = (double) (G.w - 1);
                    const double enrere = posBase + spray * atzar01 (G.llavor);
                    double inici = alReves ? cap0 - enrere - 2.0 : cap0 - enrere - std::max (0.0, (ratio - 1.0) * len) - 2.0;
                    const double fi = alReves ? inici - ratio * len : inici + ratio * len;
                    const double mes_vell = std::min (inici, fi);
                    if (mes_vell >= (double) G.valid && (double) G.w - mes_vell < (double) cap - 4.0)
                    {
                        auto& g = G.g[lliure];
                        g.actiu = true; g.pos = inici; g.vel = alReves ? -ratio : ratio; g.len = len; g.edat = 0;
                        const float p = (atzar01 (G.llavor) * 2.0f - 1.0f) * amplada;
                        const float ang = (p + 1.0f) * (float) (pi * 0.25);
                        g.gl = std::cos (ang) * 1.41421356f; g.gr = std::sin (ang) * 1.41421356f;
                    }
                }
            }
            float w0 = 0.0f, w1 = 0.0f;
            for (auto& g : G.g)
            {
                if (! g.actiu) continue;
                ++actius;
                const float env = (float) (0.5 - 0.5 * std::cos (tau * (double) g.edat / (double) g.len));
                const float s0 = llegeix (G.buf[0], G.mascara, g.pos), s1 = llegeix (G.buf[1], G.mascara, g.pos);
                const float mono = 0.5f * (s0 + s1);
                w0 += env * (s0 + amplada * (mono * g.gl - s0));
                w1 += env * (s1 + amplada * (mono * g.gr - s1));
                g.pos += g.vel;
                if (++g.edat >= g.len) g.actiu = false;
            }
            w0 *= norm; w1 *= norm;
            const float x0 = L[i], x1 = R[i];
            if (! freeze)
            {
                // grava (amb el feedback suavitzat perquè mai s'enfili)
                G.buf[0][(size_t) (G.w & G.mascara)] = x0 + fb * std::tanh (w0);
                G.buf[1][(size_t) (G.w & G.mascara)] = x1 + fb * std::tanh (w1);
                ++G.w;
            }
            const float mx = m0 + (mixObj - m0) * (float) (i + 1) / (float) n;
            L[i] = x0 + mx * (w0 - x0);
            R[i] = x1 + mx * (w1 - x1);
        }
        G.mix = mixObj;
        mes.grans.store (actius / std::max (1, n), std::memory_order_relaxed);
    }

    // ---- Atzar musical ----
    std::string Evolve::estatAtzar (unsigned llavor) const
    {
        std::mt19937 rng (llavor);
        auto u = [&] (float a, float b) { return std::uniform_real_distribution<float> (a, b) (rng); };
        auto e = [&] (int a, int b) { return std::uniform_int_distribution<int> (a, b) (rng); };
        Evolve x;
        x.posa (Evolution, valor (Evolution));
        x.posa (DryWet, 100.0f);
        for (int m = 0; m < numModuls; ++m) x.posa (paramOn (m), 0.0f);
        // 2–4 mòduls, el Shimmer o el Filter gairebé sempre (perquè soni a «transformació» i no a soroll)
        bool usa[numModuls] {};
        usa[e (0, 1) == 0 ? MShimmer : MFiltre] = true;
        const int quants = e (2, 4);
        auto compta = [&] { int c = 0; for (bool b : usa) c += b ? 1 : 0; return c; };
        while (compta() < quants) usa[e (0, numModuls - 1)] = true;
        std::vector<Ruta> rs;
        float entra = u (0.0f, 25.0f);
        auto tram = [&] { const float de = entra; const float a = std::min (100.0f, de + u (25.0f, 55.0f)); entra = std::min (75.0f, entra + u (10.0f, 25.0f)); return std::pair<float, float> (de, a); };
        if (usa[MFiltre])
        {
            x.posa (FilOn, 1); const int t = e (0, 2); x.posa (FilTipus, (float) t);
            x.posa (FilReso, u (10.0f, 45.0f)); x.posa (FilLfoDiv, (float) e (2, 11));
            x.posa (FilFreq, t == 1 ? 20.0f : 20000.0f);
            const auto [de, a] = tram();
            rs.push_back ({ FilMix, de, std::min (100.0f, de + 10.0f), 100.0f, Rapida });
            rs.push_back ({ FilFreq, de, a, t == 1 ? u (300.0f, 1500.0f) : u (500.0f, 3000.0f), EnS });
            rs.push_back ({ FilLfo, a * 0.7f, 100.0f, u (20.0f, 70.0f), Lineal });
            rs.push_back ({ FilReso, a * 0.7f, 100.0f, u (40.0f, 70.0f), Suau });
        }
        if (usa[MScreamer])
        {
            x.posa (ScrOn, 1); x.posa (ScrMode, (float) e (0, 4)); x.posa (ScrTone, u (30.0f, 70.0f));
            const auto [de, a] = tram();
            rs.push_back ({ ScrMix, de, a, u (40.0f, 100.0f), Suau });
            rs.push_back ({ ScrDrive, de, 100.0f, u (35.0f, 85.0f), Lineal });
        }
        if (usa[MPumper])
        {
            x.posa (PmpOn, 1); x.posa (PmpMode, (float) e (0, 3)); x.posa (PmpDiv, (float) e (3, 11));
            x.posa (PmpForma, u (20.0f, 80.0f)); x.posa (PmpPatro, (float) e (0, numPatrons() - 1));
            const auto [de, a] = tram();
            rs.push_back ({ PmpDepth, de, a, u (40.0f, 85.0f), Suau });
        }
        if (usa[MShimmer])
        {
            x.posa (ShmOn, 1); x.posa (ShmTipus, (float) e (0, 2)); x.posa (ShmPitch, (float) (e (0, 3) == 3 ? 3 : e (0, 1)));
            x.posa (ShmSize, u (50.0f, 95.0f)); x.posa (ShmDamp, u (20.0f, 60.0f)); x.posa (ShmMod, u (20.0f, 70.0f)); x.posa (ShmDecay, u (1.5f, 4.0f));
            const auto [de, a] = tram();
            rs.push_back ({ ShmMix, de, a, u (25.0f, 55.0f), Suau });
            rs.push_back ({ ShmDecay, de, 100.0f, u (6.0f, 18.0f), Lineal });
            rs.push_back ({ ShmAmt, de, 100.0f, u (30.0f, 80.0f), Lineal });
        }
        if (usa[MReverser])   // sempre Reverse delay a l'atzar (el Reverse reverb canvia la latència)
        {
            x.posa (RevOn, 1); x.posa (RevMode, 0); x.posa (RevDiv, (float) e (3, 8)); x.posa (RevPitch, (float) (e (0, 3) == 0 ? e (1, 2) : 0));
            x.posa (RevFeedback, u (0.0f, 40.0f));
            const auto [de, a] = tram();
            rs.push_back ({ RevMix, de, a, u (25.0f, 50.0f), Suau });
        }
        if (usa[MPitch])
        {
            static const int semis[] = { 12, -12, 7, 5, 24, 12 };
            x.posa (PitOn, 1); x.posa (PitMode, 0); x.posa (PitSemi1, (float) semis[e (0, 5)]); x.posa (PitSemi2, (float) semis[e (0, 5)]);
            x.posa (PitDetune, u (0.0f, 15.0f));
            const auto [de, a] = tram();
            rs.push_back ({ PitMix, de, a, u (25.0f, 50.0f), Suau });
            if (e (0, 1) == 0) rs.push_back ({ PitNiv2, a, 100.0f, u (30.0f, 80.0f), Lineal });
        }
        if (usa[MGranular])
        {
            x.posa (GraOn, 1); x.posa (GraSize, u (40.0f, 200.0f)); x.posa (GraDens, u (8.0f, 35.0f)); x.posa (GraPos, u (50.0f, 400.0f));
            x.posa (GraSpray, u (20.0f, 300.0f)); x.posa (GraQuant, (float) e (1, 2)); x.posa (GraWidth, u (40.0f, 90.0f));
            const auto [de, a] = tram();
            rs.push_back ({ GraMix, de, a, u (25.0f, 60.0f), Suau });
            rs.push_back ({ GraRandPitch, a * 0.7f, 100.0f, u (10.0f, 50.0f), Lineal });
        }
        if (usa[MScatter])
        {
            x.posa (ScaOn, 1); x.posa (ScaDiv, (float) e (8, 12)); x.posa (ScaFeedback, u (0.0f, 60.0f));
            const auto [de, a] = tram();
            rs.push_back ({ ScaMix, de, std::min (100.0f, de + 5.0f), 100.0f, Rapida });
            rs.push_back ({ ScaDensity, de, 100.0f, u (30.0f, 70.0f), Lineal });
            rs.push_back ({ ScaScatter, de, 100.0f, u (20.0f, 80.0f), Lineal });
        }
        x.posaRutes (rs);
        // de tant en tant, l'LFO 1 mou alguna cosa que ja sona
        if (e (0, 1) == 0)
        {
            std::vector<int> cands;
            if (usa[MFiltre]) cands.push_back (FilFreq);
            if (usa[MShimmer]) cands.push_back (ShmMod);
            if (usa[MPitch]) cands.push_back (PitFi);
            if (usa[MPumper]) cands.push_back (PmpDepth);
            if (! cands.empty())
            {
                x.posa (L1Forma, (float) e (0, 4)); x.posa (L1Sync, 1); x.posa (L1Div, (float) e (0, 8)); x.posa (L1Depth, u (40.0f, 100.0f));
                x.posaAssignacions ({ { 0, cands[(size_t) e (0, (int) cands.size() - 1)], u (-25.0f, 25.0f) } });
            }
        }
        // ordre: el de sempre (to → distorsió → filtre → talls → moviment → al revés → espai), amb algun canvi
        std::array<int, numModuls> o = ordreMusical;
        if (e (0, 3) == 0) std::swap (o[1], o[2]);
        if (e (0, 4) == 0) std::swap (o[4], o[5]);
        x.posaOrdre (o);
        return x.estat();
    }
}
