#include "mibase/Reverb.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#if defined(__SSE__) || defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__)
 #include <xmmintrin.h>
 #define MIBASE_REVERB_FTZ 1
#endif

namespace mibase::reverb
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;

        const char* const nomsAlg[numAlgorismes] = { "Room", "Hall", "Chamber", "Plate", "Spring", "Vintage Digital",
                                                     "Ambient", "Shimmer", "Gated", "Reverse", "Early Reflections" };
        const char* const descAlg[numAlgorismes] = {
            "Habitacions petites i mitjanes: reflexions properes i una cua curta i natural.",
            "Sales grans i profundes: entrada suau, cua llarga i ampla.",
            "Cambra de reverberació: molt densa i càlida, amb reflexions fortes.",
            "Placa metàl·lica: atac immediat, molt dens i brillant. Clàssic per a veus i caixes.",
            "Moll (spring) com el dels amplificadors de guitarra: vibrant i amb el «boing» típic.",
            "Reverb digital dels anys 80: amplada, cor suau i aguts arrodonits.",
            "Cues llargues i difuses, sense reflexions marcades: per a textures i pads.",
            "Cua que puja de to (octava, quinta…): brillant i etèria.",
            "Reverb tallada de cop: la cua es manté i es talla en sec (bateries dels 80).",
            "Reverb al revés: el so creix fins al cop i es talla.",
            "Només les primeres reflexions: dona espai i presència sense cua."
        };

        struct Nota { const char* nom; double negres; };
        const Nota notes[] = { { "1/64", 0.0625 }, { "1/32", 0.125 }, { "1/16T", 1.0 / 6.0 }, { "1/16", 0.25 }, { "1/16·", 0.375 },
                               { "1/8T", 1.0 / 3.0 }, { "1/8", 0.5 }, { "1/8·", 0.75 }, { "1/4T", 2.0 / 3.0 }, { "1/4", 1.0 },
                               { "1/4·", 1.5 }, { "1/2", 2.0 }, { "1/1", 4.0 }, { "2/1", 8.0 }, { "4/1", 16.0 } };
        constexpr int nNotes = (int) (sizeof (notes) / sizeof (notes[0]));
        const int transp[] = { 12, 7, 5, -12 };

        // clau, nom, min, max, defecte, enter, log, ajuda
        const InfoParam infos[numParams] = {
            { "alg", "Algorisme", 0, numAlgorismes - 1, Hall, true, false, "El tipus d'espai: cada algorisme té un caràcter propi." },
            { "mix", "Mix", 0, 100, 25, false, false, "Quanta reverb se sent: 0 % = només el so original, 100 % = només la reverb." },
            { "decay", "Decay", 0.1f, 20.0f, 2.2f, false, true, "Quant dura la cua (temps fins que baixa 60 dB)." },
            { "predelay", "Pre-delay", 0, 500, 20, false, false, "Temps entre el so original i l'inici de la reverb. Separa la veu de la reverb." },
            { "presync", "Sincronitza el pre-delay", 0, 1, 0, true, false, "El pre-delay segueix el tempo de la cançó." },
            { "prenota", "Nota del pre-delay", 0, nNotes - 1, 3, true, false, "Durada del pre-delay en notes." },
            { "size", "Size", 0, 100, 60, false, false, "Mida de l'espai: canvia la distància entre reflexions i la densitat." },
            { "width", "Width", 0, 150, 100, false, false, "Amplada estèreo de la reverb (0 % = mono, 150 % = més ample)." },
            { "damping", "Damping", 0, 100, 40, false, false, "Com absorbeixen les parets els aguts: més damping = cua més fosca." },
            { "diffusion", "Diffusion", 0, 100, 75, false, false, "Densitat: poca = ecos separats; molta = cua llisa." },
            { "earlylate", "Early/Late", 0, 100, 60, false, false, "Equilibri entre les primeres reflexions (esquerra) i la cua (dreta)." },
            { "lowcut", "Low Cut", 20, 1000, 80, false, true, "Treu greus del so que entra a la reverb (evita una reverb tèrbola)." },
            { "highcut", "High Cut", 1000, 20000, 14000, false, true, "Treu aguts del so que entra a la reverb (menys sibilants)." },
            { "eqlowf", "Greus: freq.", 40, 600, 200, false, true, "Freqüència del filtre de greus de l'EQ de sortida." },
            { "eqlowg", "Greus", -15, 15, 0, false, false, "Puja o baixa els greus de la reverb." },
            { "eq1f", "Banda 1: freq.", 100, 3000, 500, false, true, "Freqüència de la primera banda." },
            { "eq1g", "Banda 1", -15, 15, 0, false, false, "Guany de la primera banda." },
            { "eq1q", "Banda 1: Q", 0.3f, 6.0f, 1.0f, false, true, "Amplada de la primera banda (Q alta = estreta)." },
            { "eq2f", "Banda 2: freq.", 800, 10000, 3000, false, true, "Freqüència de la segona banda." },
            { "eq2g", "Banda 2", -15, 15, 0, false, false, "Guany de la segona banda." },
            { "eq2q", "Banda 2: Q", 0.3f, 6.0f, 1.0f, false, true, "Amplada de la segona banda." },
            { "eqhighf", "Aguts: freq.", 2000, 16000, 8000, false, true, "Freqüència del filtre d'aguts de l'EQ de sortida." },
            { "eqhighg", "Aguts", -15, 15, 0, false, false, "Puja o baixa els aguts de la reverb." },
            { "decgreus", "Decay greus", 0.25f, 2.0f, 1.0f, false, true, "Durada de la cua dels greus respecte al Decay (×0,5 = la meitat)." },
            { "decaguts", "Decay aguts", 0.1f, 2.0f, 0.7f, false, true, "Durada de la cua dels aguts respecte al Decay." },
            { "creugreu", "Límit greus", 80, 1000, 250, false, true, "Per sota d'aquesta freqüència manen els «Decay greus»." },
            { "creuagut", "Límit aguts", 1000, 12000, 4000, false, true, "Per sobre d'aquesta freqüència manen els «Decay aguts»." },
            { "duckon", "Ducking", 0, 1, 0, true, false, "La reverb s'abaixa sola mentre sona el so original i torna quan calla." },
            { "duckthr", "Llindar", -60, 0, -30, false, false, "A partir de quin nivell del so original s'abaixa la reverb." },
            { "duckatk", "Atac", 1, 200, 10, false, true, "Com de ràpid s'abaixa la reverb." },
            { "duckrel", "Retorn", 20, 2000, 250, false, true, "Com de ràpid torna la reverb quan el so calla." },
            { "duckamt", "Quantitat", 0, 30, 9, false, false, "Quants dB s'abaixa la reverb." },
            { "gateon", "Gate", 0, 1, 0, true, false, "Talla la cua quan el so original baixa del llindar." },
            { "gatethr", "Llindar", -70, 0, -40, false, false, "Per sota d'aquest nivell del so original, la cua es tanca." },
            { "gateatk", "Atac", 0.5f, 100, 2, false, true, "Com de ràpid s'obre." },
            { "gatehold", "Hold", 0, 2000, 250, false, false, "Quant es manté obert després que el so baixi." },
            { "gaterel", "Tancament", 5, 2000, 120, false, true, "Com de ràpid es tanca." },
            { "modmode", "Tipus de modulació", 0, 2, 0, true, false, "Chorus: suau i regular. Aleatori: moviment orgànic. Vintage: cor ample dels 80." },
            { "modrate", "Rate", 0.05f, 8.0f, 0.6f, false, true, "Velocitat del moviment de la cua." },
            { "moddepth", "Depth", 0, 100, 30, false, false, "Quantitat de moviment (treu ressonàncies metàl·liques)." },
            { "modsync", "Sincronitza la modulació", 0, 1, 0, true, false, "La velocitat de la modulació segueix el tempo." },
            { "modnota", "Nota de la modulació", 0, nNotes - 1, 12, true, false, "Durada d'un cicle de modulació en notes." },
            { "shimon", "Shimmer", 0, 1, 0, true, false, "La cua puja (o baixa) de to a cada volta." },
            { "shimpitch", "Transposició", 0, 3, 0, true, false, "Quant es transposa: +12 (octava), +7 (quinta), +5 (quarta) o -12." },
            { "shimamt", "Quantitat", 0, 100, 35, false, false, "Quanta part de la cua es transposa." },
            { "freezeon", "Freeze", 0, 1, 0, true, false, "Congela la cua: queda sonant sense parar i no hi entra so nou." },
            { "freezerel", "Alliberament", 0.05f, 10.0f, 2.0f, false, true, "En treure el Freeze, quant triga la cua a tornar a apagar-se." },
            { "revon", "Reverse", 0, 1, 0, true, false, "Fa sonar la cua al revés (creix en lloc d'apagar-se)." },
            { "feedback", "Feedback", 0, 100, 0, false, false, "Allarga la cua fins a fer-la gairebé infinita." },
            { "separacio", "Separació", 0, 100, 70, false, false, "Com d'independents són els canals esquerre i dret a dins la reverb." },
            { "greusmono", "Greus en mono", 0, 1, 1, true, false, "Els greus de la reverb queden al centre (millor en mono i en sistemes petits)." },
            { "entrada", "Entrada", 0, 1, 0, true, false, "Estèreo: respecta l'estèreo original. Mono→Estèreo: suma l'entrada i en fa un espai estèreo." },
            { "sortida", "Sortida", -24, 6, 0, false, false, "Volum de la reverb (no toca el so original)." },
            { "qualitat", "Qualitat", 0, 2, 1, true, false, "Eco: menys CPU. Normal: equilibrat. Alta: més densitat i suavitat." },
            { "vista", "Vista", 0, 1, 0, true, false, "Simple o Avançat." },
        };

        inline float limita (float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
        inline float dbAGuany (float db) { return std::pow (10.0f, db / 20.0f); }
        inline float guanyADb (float g) { return g > 1e-9f ? 20.0f * std::log10 (g) : -180.0f; }

        std::string num (double v, int decimals)
        {
            char b[32];
            std::snprintf (b, sizeof b, "%.*f", decimals, v);
            std::string s (b);
            for (auto& c : s) if (c == '.') c = ',';
            return s;
        }

        std::string textFreq (double f) { return f < 1000.0 ? num (f, 0) + " Hz" : num (f / 1000.0, f < 10000.0 ? 1 : 0) + " kHz"; }
        std::string textMs (double ms) { return ms < 1000.0 ? num (ms, ms < 10.0 ? 1 : 0) + " ms" : num (ms / 1000.0, 2) + " s"; }
        std::string textDb (double d) { return (d > 0.05 ? "+" : "") + num (d, 1) + " dB"; }

        // ---- Biquads (RBJ) ----
        struct Coef { float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

        Coef passaAlt (double f, double fs, double q = 0.7071)
        {
            const double w = 2 * pi * std::min (f, fs * 0.45) / fs, c = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al;
            return { (float) ((1 + c) / 2 / a0), (float) (-(1 + c) / a0), (float) ((1 + c) / 2 / a0), (float) (-2 * c / a0), (float) ((1 - al) / a0) };
        }
        Coef passaBaix (double f, double fs, double q = 0.7071)
        {
            const double w = 2 * pi * std::min (f, fs * 0.45) / fs, c = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al;
            return { (float) ((1 - c) / 2 / a0), (float) ((1 - c) / a0), (float) ((1 - c) / 2 / a0), (float) (-2 * c / a0), (float) ((1 - al) / a0) };
        }
        Coef pic (double f, double db, double q, double fs)
        {
            const double A = std::pow (10.0, db / 40.0), w = 2 * pi * std::min (f, fs * 0.45) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
            const double a0 = 1 + al / A;
            return { (float) ((1 + al * A) / a0), (float) (-2 * c / a0), (float) ((1 - al * A) / a0), (float) (-2 * c / a0), (float) ((1 - al / A) / a0) };
        }
        Coef prestatge (double f, double db, double fs, bool alt)
        {
            const double A = std::pow (10.0, db / 40.0), w = 2 * pi * std::min (f, fs * 0.45) / fs, c = std::cos (w), s = std::sin (w);
            const double al = s / 2 * std::sqrt (2.0), sa = 2 * std::sqrt (A) * al;
            if (! alt)
            {
                const double a0 = (A + 1) + (A - 1) * c + sa;
                return { (float) (A * ((A + 1) - (A - 1) * c + sa) / a0), (float) (2 * A * ((A - 1) - (A + 1) * c) / a0),
                         (float) (A * ((A + 1) - (A - 1) * c - sa) / a0), (float) (-2 * ((A - 1) + (A + 1) * c) / a0),
                         (float) (((A + 1) + (A - 1) * c - sa) / a0) };
            }
            const double a0 = (A + 1) - (A - 1) * c + sa;
            return { (float) (A * ((A + 1) + (A - 1) * c + sa) / a0), (float) (-2 * A * ((A - 1) + (A + 1) * c) / a0),
                     (float) (A * ((A + 1) + (A - 1) * c - sa) / a0), (float) (2 * ((A - 1) - (A + 1) * c) / a0),
                     (float) (((A + 1) - (A - 1) * c - sa) / a0) };
        }
        double magnitud (const Coef& k, double f, double fs)
        {
            const double w = 2 * pi * f / fs;
            const double cr = std::cos (w), ci = -std::sin (w), c2r = std::cos (2 * w), c2i = -std::sin (2 * w);
            const double nr = k.b0 + k.b1 * cr + k.b2 * c2r, ni = k.b1 * ci + k.b2 * c2i;
            const double dr = 1 + k.a1 * cr + k.a2 * c2r, di = k.a1 * ci + k.a2 * c2i;
            return std::sqrt ((nr * nr + ni * ni) / std::max (1e-30, dr * dr + di * di));
        }
        struct Biquad
        {
            Coef k;
            float z1 = 0, z2 = 0;
            inline float operator() (float x) noexcept
            {
                const float y = k.b0 * x + z1;
                z1 = k.b1 * x - k.a1 * y + z2;
                z2 = k.b2 * x - k.a2 * y;
                return y;
            }
            void reset() { z1 = z2 = 0; }
        };

        // Un pol (TPT): passa-baix; els prestatges i el damping es fan amb això.
        inline float coefPol (double f, double fs) { const double g = std::tan (pi * std::min (f, fs * 0.49) / fs); return (float) (g / (1 + g)); }
        struct Pol
        {
            float s = 0;
            inline float baix (float x, float g) noexcept { const float v = (x - s) * g, y = v + s; s = y + v; return y; }
        };

        // Generador pseudoaleatori petit (determinista, per als patrons de reflexions).
        struct Atzar
        {
            std::uint32_t s;
            explicit Atzar (std::uint32_t llavor) : s (llavor ? llavor : 1u) {}
            float u() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xffffff) / 16777216.0f; }   // 0..1
            float b() { return u() * 2.0f - 1.0f; }                                                                // -1..1
        };

        int potencia2 (int n) { int p = 1; while (p < n) p <<= 1; return p; }

        struct Linia   // línia de retard amb lectura fraccionària
        {
            std::vector<float> b;
            int m = 0, w = 0;
            void crea (int mida) { b.assign ((size_t) potencia2 (std::max (8, mida)), 0.0f); m = (int) b.size() - 1; w = 0; }
            void neteja() { std::fill (b.begin(), b.end(), 0.0f); w = 0; }
            inline void escriu (float x) noexcept { b[(size_t) w] = x; w = (w + 1) & m; }
            inline float llegeix (int d) const noexcept { return b[(size_t) ((w - d) & m)]; }
            inline float lineal (float d) const noexcept
            {
                const int id = (int) d;
                const float f = d - (float) id;
                const float a = b[(size_t) ((w - id) & m)], c = b[(size_t) ((w - id - 1) & m)];
                return a + (c - a) * f;
            }
            inline float hermite (float d) const noexcept
            {
                // Posició = (w - id) - f: entre x0 = w-id-1 i x1 = w-id, amb t = 1 - f.
                const int id = (int) d;
                const float t = 1.0f - (d - (float) id);
                const int i = w - id - 1;
                const float xm1 = b[(size_t) ((i - 1) & m)], x0 = b[(size_t) (i & m)], x1 = b[(size_t) ((i + 1) & m)], x2 = b[(size_t) ((i + 2) & m)];
                const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2, c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
                return ((c3 * t + c2) * t + c1) * t + x0;
            }
        };

        struct Allpass   // Schroeder
        {
            Linia l;
            int d = 1;
            inline float operator() (float x, float g) noexcept
            {
                const float s = l.llegeix (d);
                const float v = x + g * s;
                l.escriu (v);
                return s - g * v;
            }
        };

        // Talla suau dins els llaços: no toca res per sota de 2 i no deixa passar de 3.
        inline float protegeix (float x) noexcept
        {
            const float a = std::fabs (x);
            if (a <= 2.0f) return x;
            const float e = a - 2.0f;
            return std::copysign (2.0f + e / (1.0f + e), x);
        }

        struct Perfil
        {
            float minMs, maxMs;       // línies del tanc a Size = 100 % (abans de l'escala)
            float erMinMs, erSpanMs;  // reflexions primerenques
            float erGuany;
            int erTaps;
            float difEntrada, difLlac;
            float modMs, modRate;     // escala de la modulació
            float color;              // passa-baix de caràcter a la sortida (0 = cap)
            float guanyCua;           // nivell relatiu de la cua
            float apFrac;             // allpass dins el llaç, en proporció de la línia (densitat)
        };
        // Room, Hall, Chamber, Plate, Spring, Digital, Ambient, Shimmer, Gated, Reverse, Early
        const Perfil perfils[numAlgorismes] = {
            {  9,  38, 1.5f, 26, 0.85f, 18, 0.70f, 0.60f, 0.35f, 0.7f, 0, 1.30f, 0.55f },
            { 26, 105, 7.0f, 62, 0.50f, 14, 0.80f, 0.60f, 0.45f, 0.5f, 0, 1.45f, 0.35f },
            { 14,  55, 2.5f, 38, 0.80f, 22, 0.90f, 0.65f, 0.30f, 0.6f, 0, 1.30f, 0.45f },
            { 22,  80, 0.0f,  0, 0.00f,  0, 1.00f, 0.65f, 0.25f, 0.9f, 0, 1.55f, 0.50f },
            { 30,  50, 0.0f,  0, 0.00f,  0, 0.60f, 0.00f, 0.12f, 1.0f, 5200, 1.30f, 0.0f },
            { 20,  85, 5.0f, 45, 0.50f, 10, 0.65f, 0.55f, 0.90f, 0.8f, 9000, 1.50f, 0.30f },
            { 45, 150, 0.0f,  0, 0.00f,  0, 0.95f, 0.65f, 0.80f, 0.3f, 0, 1.70f, 0.35f },
            { 40, 140, 0.0f,  0, 0.00f,  0, 0.95f, 0.65f, 0.60f, 0.35f, 0, 1.60f, 0.35f },
            { 10,  40, 1.0f, 30, 0.90f, 24, 0.90f, 0.60f, 0.10f, 1.0f, 0, 1.20f, 0.45f },
            { 10,  40, 0.0f,  0, 0.00f,  0, 0.85f, 0.00f, 0.00f, 1.0f, 0, 1.20f, 0.0f },
            {  0,   0, 1.0f, 90, 1.00f, 28, 0.60f, 0.00f, 0.00f, 1.0f, 0, 1.15f, 0.0f },
        };

        // Proporcions (0..1) de les longituds del tanc: irregulars perquè no ressonin juntes.
        const float propLinies[16] = { 0.000f, 0.517f, 0.231f, 0.802f, 0.083f, 0.659f, 0.367f, 0.938f,
                                       0.151f, 0.588f, 0.298f, 0.871f, 0.040f, 0.722f, 0.433f, 1.000f };
        const float msDifEntrada[2][6] = { { 1.27f, 2.09f, 3.53f, 5.11f, 7.43f, 10.3f }, { 1.39f, 2.31f, 3.17f, 5.57f, 7.07f, 9.71f } };
    }

    // =========================================================================
    const char* nomAlgorisme (int a) { return a >= 0 && a < numAlgorismes ? nomsAlg[a] : "?"; }
    const char* descripcioAlgorisme (int a) { return a >= 0 && a < numAlgorismes ? descAlg[a] : ""; }
    const InfoParam& info (int p) { return infos[std::clamp (p, 0, numParams - 1)]; }
    int numNotes() { return nNotes; }
    const char* nomNota (int i) { return notes[std::clamp (i, 0, nNotes - 1)].nom; }
    double notaEnTemps (int i) { return notes[std::clamp (i, 0, nNotes - 1)].negres; }
    int numTransposicions() { return 4; }
    int transposicio (int i) { return transp[std::clamp (i, 0, 3)]; }

    std::string textValor (int p, float v)
    {
        switch (p)
        {
            case Alg: return nomAlgorisme ((int) std::lround (v));
            case Mix: case Size: case Width: case Damping: case Diffusion: case ModDepth: case ShimAmt: case Feedback: case Separacio:
                return num (v, 0) + " %";
            case EarlyLate: return num (100 - v, 0) + " / " + num (v, 0);
            case Decay: return v < 1.0f ? num (v * 1000.0, 0) + " ms" : num (v, v < 10.0f ? 2 : 1) + " s";
            case PreDelay: case DuckAtk: case DuckRel: case GateAtk: case GateHold: case GateRel: return textMs (v);
            case LowCut: case HighCut: case EqLowF: case Eq1F: case Eq2F: case EqHighF: case CreuGreu: case CreuAgut: return textFreq (v);
            case EqLowG: case Eq1G: case Eq2G: case EqHighG: case Sortida: return textDb (v);
            case DuckThr: case GateThr: return num (v, 0) + " dB";
            case DuckAmt: return num (v, 0) + " dB";
            case Eq1Q: case Eq2Q: return num (v, 1);
            case DecGreus: case DecAguts: return "×" + num (v, 2);
            case ModRate: return num (v, 2) + " Hz";
            case FreezeRel: return num (v, 1) + " s";
            case PreNota: case ModNota: return nomNota ((int) std::lround (v));
            case ShimPitch: { const int t = transposicio ((int) std::lround (v)); return (t > 0 ? "+" : "") + std::to_string (t); }
            case ModMode: { const int i = (int) std::lround (v); return i == 0 ? "Chorus" : i == 1 ? "Aleatori" : "Vintage"; }
            case Qualitat: { const int i = (int) std::lround (v); return i == 0 ? "Eco" : i == 1 ? "Normal" : "Alta"; }
            case EntradaMode: return v > 0.5f ? "Mono→Estèreo" : "Estèreo";
            case ModeVista: return v > 0.5f ? "Avançat" : "Simple";
            default: return v > 0.5f ? "Sí" : "No";
        }
    }

    bool llegeixValor (int p, const std::string& text, float& v)
    {
        if (info (p).enter) return false;
        std::string s;
        for (char c : text) if (c != ' ' && c != '%' && c != '+') s += (c == ',' ? '.' : (char) std::tolower ((unsigned char) c));
        if (s.empty()) return false;
        double mult = 1.0;
        if (s.size() > 2 && s.substr (s.size() - 2) == "ms") { s.resize (s.size() - 2); if (p == Decay || p == FreezeRel) mult = 0.001; }
        else if (s.size() > 2 && s.substr (s.size() - 2) == "hz") s.resize (s.size() - 2);
        else if (s.size() > 2 && s.substr (s.size() - 2) == "db") s.resize (s.size() - 2);
        if (! s.empty() && s.back() == 'k') { s.pop_back(); mult = 1000.0; if (s.size() > 1 && s.back() == 'h') s.pop_back(); }
        else if (! s.empty() && s.back() == 's') { s.pop_back(); if (p != Decay && p != FreezeRel) mult = 1000.0; }
        else if (! s.empty() && s[0] == 'x') s.erase (0, 1);
        if (s.empty()) return false;
        char* fi = nullptr;
        const double x = std::strtod (s.c_str(), &fi);
        if (fi == s.c_str() || *fi != 0 || ! std::isfinite (x)) return false;
        double r = x * mult;
        v = limita ((float) r, info (p).mn, info (p).mx);
        return true;
    }

    // =========================================================================
    Params::Params() { for (int i = 0; i < numParams; ++i) v[(size_t) i] = infos[i].def; }

    void Params::posa (int p, float x)
    {
        if (p < 0 || p >= numParams || ! std::isfinite (x)) return;
        const auto& i = infos[p];
        x = limita (x, i.mn, i.mx);
        if (i.enter) x = (float) std::lround (x);
        v[(size_t) p] = x;
    }

    std::string Params::text() const
    {
        std::string s = "mibase-reverb=1";
        char b[64];
        for (int i = 0; i < numParams; ++i)
        {
            std::snprintf (b, sizeof b, ";%s=%.6g", infos[i].clau, (double) v[(size_t) i]);
            s += b;
        }
        return s;
    }

    bool Params::llegeix (const std::string& s)
    {
        if (s.rfind ("mibase-reverb=", 0) != 0) return false;
        *this = Params();
        size_t a = 0;
        while (a < s.size())
        {
            size_t b = s.find (';', a);
            if (b == std::string::npos) b = s.size();
            const auto parell = s.substr (a, b - a);
            const auto eq = parell.find ('=');
            if (eq != std::string::npos)
            {
                const auto clau = parell.substr (0, eq);
                const float x = (float) std::strtod (parell.c_str() + eq + 1, nullptr);
                for (int i = 0; i < numParams; ++i)
                    if (clau == infos[i].clau) { posa (i, x); break; }
            }
            a = b + 1;
        }
        return true;
    }

    bool Params::igualSo (const Params& o) const
    {
        for (int i = 0; i < numParams; ++i)
            if (i != ModeVista && v[(size_t) i] != o.v[(size_t) i]) return false;
        return true;
    }

    double predelaySegons (const Params& p, double bpm)
    {
        if (p[PreSync] > 0.5f) return std::min (2.0, notaEnTemps ((int) p[PreNota]) * 60.0 / std::max (20.0, bpm));
        return p[PreDelay] / 1000.0;
    }

    double rateModulacio (const Params& p, double bpm)
    {
        if (p[ModSync] > 0.5f) return std::clamp (std::max (20.0, bpm) / 60.0 / notaEnTemps ((int) p[ModNota]), 0.02, 12.0);
        return p[ModRate];
    }

    double respostaEq (const Params& p, double f)
    {
        const double fs = 96000.0;
        double m = magnitud (passaAlt (p[LowCut], fs), f, fs) * magnitud (passaBaix (p[HighCut], fs), f, fs);
        m *= magnitud (prestatge (p[EqLowF], p[EqLowG], fs, false), f, fs);
        m *= magnitud (pic (p[Eq1F], p[Eq1G], p[Eq1Q], fs), f, fs);
        m *= magnitud (pic (p[Eq2F], p[Eq2G], p[Eq2Q], fs), f, fs);
        m *= magnitud (prestatge (p[EqHighF], p[EqHighG], fs, true), f, fs);
        return 20.0 * std::log10 (std::max (1e-9, m));
    }

    namespace
    {
        // 0 % = cap absorció (el filtre queda fora de la banda audible), 100 % = 1,4 kHz.
        double dampingHz (float damping) { return 1400.0 * std::pow (40000.0 / 1400.0, std::pow (1.0 - damping / 100.0, 1.3)); }
        // Magnitud d'un pol (passa-baix) analògic.
        double magPol (double f, double fc) { return 1.0 / std::sqrt (1.0 + (f / fc) * (f / fc)); }
    }

    double decaimentA (const Params& p, double f)
    {
        if (p[FreezeOn] > 0.5f) return 1e9;
        const double l = 0.05;   // una línia típica de 50 ms
        const double rt = p[Decay];
        const double gM = std::pow (10.0, -3.0 * l / rt), gL = std::pow (10.0, -3.0 * l / (rt * p[DecGreus])),
                     gH = std::pow (10.0, -3.0 * l / (rt * p[DecAguts]));
        const double kL = gL / gM, kH = gH / gM;
        const double lpL = magPol (f, p[CreuGreu]), lpH = magPol (f, p[CreuAgut]);
        // Prestatges d'un pol: |HP + k·LP| i |LP + k·HP| (aproximació de potència).
        const double hpL2 = 1.0 - lpL * lpL, hpH2 = 1.0 - lpH * lpH;
        const double ls = std::sqrt (hpL2 + kL * kL * lpL * lpL), hs = std::sqrt (lpH * lpH + kH * kH * hpH2);
        double g = gM * ls * hs * magPol (f, dampingHz (p[Damping]));
        const double fb = p[Feedback] / 100.0 * 0.995;
        g = g + (1.0 - g) * fb;
        if (g >= 0.999999) return 1e9;
        return std::min (1e9, -3.0 * l / std::log10 (std::max (1e-12, g)));
    }

    // =========================================================================
    // El so
    // =========================================================================
    struct Reverb::Nucli
    {
        static constexpr int SB = 32;          // sub-bloc: aquí es recalculen els coeficients
        static constexpr int maxN = 16;
        static constexpr int numMolls = 3, maxEtapes = 72;

        double fs = 48000.0;
        bool preparat = false;

        // --- Configuració (canvia en canviar d'algorisme o de qualitat, amb fosa) ---
        int alg = -1, qualitat = -1, N = 8;
        int algDemanat = Hall, qualDemanada = 1;
        float fosa = 0.0f;     // 0..1: guany de la part «wet» mentre es canvia de configuració
        int fase = 0;          // 0 normal, 1 baixant
        Perfil pf {};

        // --- Paràmetres suavitzats (un pol per sub-bloc) ---
        float sMix = 0.25f, sWidth = 1.0f, sOut = 1.0f, sSize = 0.6f, sPre = 0.02f * 48000.0f, sEL = 0.6f;
        float sLow = 80, sHigh = 14000, sEqF[4] {}, sEqG[4] {}, sEqQ[2] { 1, 1 };
        float sDecay = 2.0f, sDG = 1.0f, sDA = 0.7f, sCG = 250, sCA = 4000, sDamp = 40, sDif = 0.75f, sFb = 0.0f, sSep = 0.7f;
        float sMod = 0.3f, sShim = 0.0f;
        bool primer = true;

        // --- Entrada ---
        Biquad inHP[2], inLP[2];
        Linia pre[2];
        float preActual = 0.0f;
        Allpass difE[2][6];

        // --- Reflexions primerenques ---
        Linia er[2];
        struct Tap { float pos; float g; };
        Tap taps[2][32] {};
        int numTaps = 0;
        Allpass erSmear[2][2];
        Pol erDamp[2];

        // --- Tanc (FDN) ---
        struct LiniaTanc
        {
            Linia l;
            Allpass ap;
            Pol fL, fH, fD;
            float len = 100.0f;   // en mostres (suavitzat)
            float gM = 0.5f, kL = 1.0f, kH = 1.0f;
            float fase = 0.0f, aleat = 0.0f, aleatObj = 0.0f, aleatPas = 0.0f;
            int aleatCompte = 0;
            Atzar rnd { 1 };
        };
        LiniaTanc lt[maxN];
        float propActual[maxN] {};
        float sgnL[maxN] {}, sgnR[maxN] {}, injL[maxN] {}, injR[maxN] {};
        float gLMod = 0.0f;

        // --- Moll ---
        struct Moll
        {
            Linia l;
            float x1[maxEtapes] {}, y1[maxEtapes] {};
            Pol lp, mod;
            float len = 1000.0f, g = 0.5f;
            float fase = 0.0f;
        };
        Moll molls[numMolls];
        int etapes = 48;

        // --- Reverse (algorisme): línia amb tocs que creixen ---
        Linia rv[2];
        Tap rvTaps[2][40] {};

        // --- Shimmer ---
        Linia shim;
        float shimFase = 0.0f;
        Pol shimHP;
        float shimRet = 0.0f;   // la sortida del shimmer que entra a la volta següent

        // --- Reverse creatiu ---
        Linia rc[2];
        int rcCompte = 0;

        // --- Sortida ---
        Biquad eq[2][4];
        Biquad color[2];
        Pol monoLP[2];
        Pol dcBlock[2];

        // --- Dinàmica ---
        float envDuck = 0.0f, guanyDuck = 1.0f, envGate = 0.0f, gate = 1.0f, gateHold = 0.0f;
        float envGated = 0.0f, gated = 0.0f, gatedHold = 0.0f;
        float fz = 0.0f;
        int decim = 0;
        float decimL = 0, decimR = 0;

        // --- Mesures ---
        float picE = 0, picS = 0, sumW = 0, sumLL = 0, sumRR = 0, sumLR = 0, sumM = 0, sumS = 0;
        int numMes = 0;

        void prepara (double nouFs)
        {
            fs = nouFs > 1000.0 ? nouFs : 48000.0;
            const auto mostres = [&] (double s) { return (int) std::ceil (s * fs) + 8; };
            for (int c = 0; c < 2; ++c)
            {
                pre[c].crea (mostres (2.05));
                er[c].crea (mostres (0.16));
                rv[c].crea (mostres (2.6));
                rc[c].crea (mostres (4.2));
                for (int k = 0; k < 6; ++k) difE[c][k].l.crea (mostres (0.016));
                for (int k = 0; k < 2; ++k) erSmear[c][k].l.crea (mostres (0.012));
            }
            for (int i = 0; i < maxN; ++i)
            {
                lt[i].l.crea (mostres (0.215));
                lt[i].ap.l.crea (mostres (0.14));
                lt[i].rnd = Atzar (0x9e3779b9u * (std::uint32_t) (i + 1));
            }
            for (auto& m : molls) m.l.crea (mostres (0.075));
            shim.crea (mostres (0.11));
            alg = qualitat = -1;
            primer = true;
            preparat = true;
            buida();
        }

        void buida()
        {
            for (int c = 0; c < 2; ++c)
            {
                pre[c].neteja(); er[c].neteja(); rv[c].neteja(); rc[c].neteja();
                for (auto& a : difE[c]) a.l.neteja();
                for (auto& a : erSmear[c]) a.l.neteja();
                inHP[c].reset(); inLP[c].reset(); color[c].reset();
                for (auto& b : eq[c]) b.reset();
                erDamp[c] = {}; monoLP[c] = {}; dcBlock[c] = {};
            }
            for (auto& l : lt) { l.l.neteja(); l.ap.l.neteja(); l.fL = {}; l.fH = {}; l.fD = {}; }
            for (auto& m : molls) { m.l.neteja(); std::fill (std::begin (m.x1), std::end (m.x1), 0.0f); std::fill (std::begin (m.y1), std::end (m.y1), 0.0f); m.lp = {}; }
            shim.neteja();
            shimHP = {};
            shimRet = 0.0f;
            rcCompte = 0;
            envDuck = envGate = envGated = 0.0f;
            guanyDuck = 1.0f;
            gate = 1.0f;
            gated = 0.0f;
            fz = 0.0f;
        }

        // Configura un algorisme: longituds, reflexions, signes. No demana memòria.
        void configura (int a, int q, const Params&)
        {
            alg = a;
            qualitat = q;
            pf = perfils[a];
            N = q >= 2 ? 16 : 8;
            etapes = q == 0 ? 24 : (q == 1 ? 48 : 72);
            // Signes de sortida: dues files ortogonals de Hadamard; injecció alternada.
            for (int i = 0; i < maxN; ++i)
            {
                sgnL[i] = (bitsDe (i & 1) ? -1.0f : 1.0f);
                sgnR[i] = (bitsDe (i & 2) ? -1.0f : 1.0f);
                propActual[i] = propLinies[(i * 7) % 16];
            }
            // Reflexions: patró propi de cada algorisme (llavor diferent a cada canal).
            numTaps = std::min (32, pf.erTaps);
            for (int c = 0; c < 2; ++c)
            {
                Atzar r (0x1234567u + (std::uint32_t) a * 7919u + (std::uint32_t) c * 104729u);
                float suma = 0.0f;
                for (int k = 0; k < numTaps; ++k)
                {
                    const float u = (float) k / (float) std::max (1, numTaps - 1);
                    const float pos = std::clamp (std::pow (u, 1.35f) + r.b() * 0.04f, 0.0f, 1.0f);
                    const float g = (1.0f - 0.75f * pos) * (0.55f + 0.45f * r.u()) * (r.u() < 0.3f ? -1.0f : 1.0f);
                    taps[c][k] = { pos, g };
                    suma += g * g;
                }
                const float nrm = suma > 0.0f ? 1.0f / std::sqrt (suma) : 0.0f;
                for (int k = 0; k < numTaps; ++k) taps[c][k].g *= nrm;
                // Reverse: tocs que creixen fins al final.
                Atzar r2 (0xabcdefu + (std::uint32_t) c * 31337u);
                float s2 = 0.0f;
                for (int k = 0; k < 40; ++k)
                {
                    const float pos = std::clamp (((float) k + r2.u()) / 40.0f, 0.0f, 1.0f);
                    const float g = std::pow (pos, 1.6f) * (0.7f + 0.3f * r2.u()) * (r2.u() < 0.35f ? -1.0f : 1.0f);
                    rvTaps[c][k] = { pos, g };
                    s2 += g * g;
                }
                const float n2 = s2 > 0.0f ? 1.1f / std::sqrt (s2) : 0.0f;
                for (int k = 0; k < 40; ++k) rvTaps[c][k].g *= n2;
            }
            for (int c = 0; c < 2; ++c)
            {
                for (int k = 0; k < 6; ++k) difE[c][k].d = std::max (1, (int) (msDifEntrada[c][k] * 0.001 * fs));
                erSmear[c][0].d = std::max (1, (int) ((c ? 3.3 : 2.9) * 0.001 * fs));
                erSmear[c][1].d = std::max (1, (int) ((c ? 5.1 : 4.6) * 0.001 * fs));
                const double col = pf.color > 0 ? pf.color : 0;
                color[c].k = col > 0 ? passaBaix (col, fs, 0.6) : Coef {};
            }
            const double msMolls[numMolls] = { 33.0, 41.0, 47.0 };
            for (int k = 0; k < numMolls; ++k) molls[k].len = (float) (msMolls[k] * 0.001 * fs);
            buida();
            primer = true;
        }

        static int bitsDe (int x) { int c = 0; while (x) { c += x & 1; x >>= 1; } return c; }

        float escalaMida (float size01) const { return 0.35f + 1.0f * size01; }

        void processa (Reverb& rv, const Params& p, double bpm, float* L, float* R, int n) noexcept
        {
            const int aDem = (int) p[Alg], qDem = (int) p[Qualitat];
            if (alg < 0) { configura (aDem, qDem, p); fosa = 0.0f; fase = 0; }
            else if ((aDem != alg || qDem != qualitat) && fase == 0) fase = 1;

            for (int ofs = 0; ofs < n; ofs += SB)
            {
                const int m = std::min (SB, n - ofs);
                subBloc (p, bpm, L + ofs, R + ofs, m);
            }

            // Mesures (per a la finestra).
            auto& ms = rv.mes;
            ms.entrada.store (picE, std::memory_order_relaxed);
            ms.sortida.store (picS, std::memory_order_relaxed);
            if (numMes > 0)
            {
                ms.cua.store (std::sqrt (sumW / (float) numMes), std::memory_order_relaxed);
                const float den = std::sqrt (sumLL * sumRR);
                ms.correlacio.store (den > 1e-12f ? limita (sumLR / den, -1.0f, 1.0f) : 1.0f, std::memory_order_relaxed);
                ms.amplada.store (sumM + sumS > 1e-12f ? limita (std::sqrt (sumS / (sumM + sumS)) * 1.41f, 0.0f, 1.0f) : 0.0f, std::memory_order_relaxed);
            }
            ms.ducking.store (-guanyADb (guanyDuck), std::memory_order_relaxed);
            ms.gate.store (p[GateOn] > 0.5f ? gate : 1.0f, std::memory_order_relaxed);
            ms.freeze.store (fz, std::memory_order_relaxed);
            ms.blocs.fetch_add (1, std::memory_order_relaxed);
            picE = picS = sumW = sumLL = sumRR = sumLR = sumM = sumS = 0.0f;
            numMes = 0;
        }

        void subBloc (const Params& p, double bpm, float* L, float* R, int n) noexcept
        {
            const float fsf = (float) fs;
            // ---- Canvi de configuració amb fosa ----
            const float pasFosa = (float) n / (0.018f * fsf);
            if (fase == 1)
            {
                fosa -= pasFosa;
                if (fosa <= 0.0f) { fosa = 0.0f; configura ((int) p[Alg], (int) p[Qualitat], p); fase = 0; }
            }
            else fosa = std::min (1.0f, fosa + pasFosa);

            // ---- Suavitzat ----
            const float a = primer ? 1.0f : 1.0f - std::exp (-(float) n / (0.045f * fsf));
            const float aLent = primer ? 1.0f : 1.0f - std::exp (-(float) n / (0.25f * fsf));
            auto seg = [a] (float& s, float obj) { s += (obj - s) * a; };
            const float mixObj = p[Mix] / 100.0f;
            if (primer) { sMix = mixObj; sWidth = p[Width] / 100.0f; sOut = dbAGuany (p[Sortida]); }
            const float mix0 = sMix, wid0 = sWidth, out0 = sOut;
            seg (sMix, mixObj);
            seg (sWidth, p[Width] / 100.0f);
            seg (sOut, dbAGuany (p[Sortida]));
            sSize += (p[Size] / 100.0f - sSize) * aLent;
            seg (sEL, p[EarlyLate] / 100.0f);
            seg (sLow, p[LowCut]); seg (sHigh, p[HighCut]);
            const int fEq[4] = { EqLowF, Eq1F, Eq2F, EqHighF }, gEq[4] = { EqLowG, Eq1G, Eq2G, EqHighG };
            for (int k = 0; k < 4; ++k) { seg (sEqF[k], p[fEq[k]]); seg (sEqG[k], p[gEq[k]]); }
            seg (sEqQ[0], p[Eq1Q]); seg (sEqQ[1], p[Eq2Q]);
            seg (sDecay, p[Decay]); seg (sDG, p[DecGreus]); seg (sDA, p[DecAguts]); seg (sCG, p[CreuGreu]); seg (sCA, p[CreuAgut]);
            seg (sDamp, p[Damping]); seg (sDif, p[Diffusion] / 100.0f); seg (sFb, p[Feedback] / 100.0f); seg (sSep, p[Separacio] / 100.0f);
            seg (sMod, p[ModDepth] / 100.0f);
            seg (sShim, p[ShimOn] > 0.5f || alg == Shimmer ? p[ShimAmt] / 100.0f : 0.0f);
            const float preObj = (float) (predelaySegons (p, bpm) * fs);
            if (primer) { preActual = preObj; }
            primer = false;

            // ---- Coeficients ----
            for (int c = 0; c < 2; ++c)
            {
                inHP[c].k = passaAlt (sLow, fs);
                inLP[c].k = passaBaix (sHigh, fs);
                eq[c][0].k = prestatge (sEqF[0], sEqG[0], fs, false);
                eq[c][1].k = pic (sEqF[1], sEqG[1], sEqQ[0], fs);
                eq[c][2].k = pic (sEqF[2], sEqG[2], sEqQ[1], fs);
                eq[c][3].k = prestatge (sEqF[3], sEqG[3], fs, true);
            }
            const bool eqPla = std::fabs (sEqG[0]) < 0.01f && std::fabs (sEqG[1]) < 0.01f && std::fabs (sEqG[2]) < 0.01f && std::fabs (sEqG[3]) < 0.01f;
            const float escala = escalaMida (sSize);

            // Freeze: puja ràpid; baixa amb el temps d'alliberament.
            const bool freezeOn = p[FreezeOn] > 0.5f;
            if (freezeOn) fz = std::min (1.0f, fz + (float) n / (0.04f * fsf));
            else fz = std::max (0.0f, fz - (float) n / (std::max (0.05f, p[FreezeRel]) * fsf));
            const float fb = std::max (sFb * 0.995f, fz);
            const float entraTanc = 1.0f - fz;

            // Tanc: longituds i guanys de cada línia.
            const float rtBase = alg == Gated ? 3.5f : std::max (0.1f, sDecay);
            const float gDamp = coefPol (dampingHz (sDamp), fs);
            const float gCG = coefPol (sCG, fs), gCA = coefPol (sCA, fs);
            const float dampBypass = fb;
            const bool esTanc = alg != Spring && alg != Reverse && alg != Early;
            const bool apLlac = pf.difLlac > 0.0f && pf.apFrac > 0.0f;
            float gMitja = 0.0f;
            if (esTanc)
                for (int i = 0; i < N; ++i)
                {
                    auto& l = lt[i];
                    const float ms = (pf.minMs + (pf.maxMs - pf.minMs) * propActual[i]) * escala;
                    const float obj = ms * 0.001f * fsf;
                    l.len += (obj - l.len) * 0.25f;
                    l.ap.d = std::max (1, (int) (l.len * pf.apFrac));
                    const float sec = (l.len + (apLlac ? (float) l.ap.d : 0.0f)) / fsf;
                    float gM = std::pow (10.0f, -3.0f * sec / rtBase);
                    float gL = std::pow (10.0f, -3.0f * sec / (rtBase * sDG));
                    float gH = std::pow (10.0f, -3.0f * sec / (rtBase * sDA));
                    gM += (1.0f - gM) * fb; gL += (1.0f - gL) * fb; gH += (1.0f - gH) * fb;
                    float kL = gL / gM, kH = gH / gM;
                    const float lim = fz > 0.999f ? 1.0f : 0.9995f;
                    const float cota = gM * std::max (1.0f, kL) * std::max (1.0f, kH);
                    if (cota > lim) gM *= lim / cota;
                    l.gM = gM; l.kL = kL; l.kH = kH;
                    gMitja += gM;
                }
            gMitja = N > 0 ? gMitja / (float) N : 0.0f;
            // Nivell de la cua: amb cues llargues el tanc acumula més energia; es compensa una part.
            const float normCua = std::pow (std::max (0.0005f, 1.0f - gMitja * gMitja), 0.38f) * 1.6f * pf.guanyCua;

            // Modulació.
            const float rateHz = (float) rateModulacio (p, bpm) * pf.modRate;
            const int modo = (int) p[ModMode];
            const float profMs = pf.modMs * (modo == 2 ? 1.8f : 1.0f) * (1.0f - 0.7f * fz);
            const float prof = sMod * profMs * 0.001f * fsf;
            const float pasFase = rateHz / fsf;
            const bool hermite = qualitat > 0;
            const float gAp = std::min (0.7f, pf.difLlac * (0.35f + 0.5f * sDif));
            const int nDif = qualitat >= 2 ? 6 : 4;
            const float gDif = std::min (0.78f, pf.difEntrada * (0.25f + 0.55f * sDif));

            // Injecció: separació entre canals.
            for (int i = 0; i < N; ++i)
            {
                const bool parell = (i & 1) == 0;
                injL[i] = parell ? 1.0f : 1.0f - sSep;
                injR[i] = parell ? 1.0f - sSep : 1.0f;
            }
            const float injNorm = 1.0f / std::sqrt ((float) N * (1.0f + (1.0f - sSep) * (1.0f - sSep)) * 0.5f);
            const float hadNorm = 1.0f / std::sqrt ((float) N);
            const float outNorm = 1.0f / std::sqrt ((float) N);

            // Reflexions primerenques.
            const float erEsc = alg == Early ? (0.25f + 1.1f * sSize) : escala;
            const float erG = pf.erGuany;
            const float gErDamp = coefPol (std::max (2500.0, dampingHz (sDamp) * 1.6), fs);
            const float gSmear = 0.65f * sDif;
            int tapPos[2][32];
            for (int c = 0; c < 2; ++c)
                for (int k = 0; k < numTaps; ++k)
                    tapPos[c][k] = std::max (1, (int) ((pf.erMinMs + pf.erSpanMs * taps[c][k].pos) * erEsc * 0.001f * fsf));
            // Equilibri early/late.
            const float bal = sEL;
            const float gEarly = (alg == Early || alg == Reverse) ? 1.0f : std::min (1.0f, 2.0f * (1.0f - bal)) * erG;
            const float gLate = alg == Early ? 0.0f : std::min (1.0f, 2.0f * bal);

            // Reverse (algorisme).
            const float lrv = std::clamp (sDecay, 0.15f, 2.5f) * fsf;
            int rvPos[2][40];
            if (alg == Reverse)
                for (int c = 0; c < 2; ++c)
                    for (int k = 0; k < 40; ++k) rvPos[c][k] = std::max (1, (int) (rvTaps[c][k].pos * lrv));

            // Molls.
            if (alg == Spring)
                for (int k = 0; k < numMolls; ++k)
                {
                    auto& mo = molls[k];
                    const float len = mo.len * (0.7f + 0.6f * sSize);
                    const float sec = (len + (float) etapes * 0.9f) / fsf;
                    float g = std::pow (10.0f, -3.0f * sec / std::max (0.1f, sDecay));
                    g += (1.0f - g) * fb;
                    mo.g = std::min (fz > 0.999f ? 1.0f : 0.9995f, g);
                }
            const float aMoll = 0.45f + 0.3f * sDif;
            const float gMollLP = coefPol (4800.0, fs);

            // Shimmer.
            const float ratio = std::pow (2.0f, (float) transposicio ((int) p[ShimPitch]) / 12.0f);
            const float W = 0.06f * fsf;
            const float pasShim = (1.0f - ratio) / W;
            const float gShimHP = coefPol (180.0, fs);
            const bool fesShim = sShim > 0.001f && esTanc;

            // Reverse creatiu.
            const bool revOn = p[RevOn] > 0.5f;
            const int lrc = (int) (std::clamp (sDecay, 0.15f, 2.0f) * fsf);

            // Dinàmica.
            const bool duckOn = p[DuckOn] > 0.5f, gateOn = p[GateOn] > 0.5f;
            const float aDuckA = std::exp (-1.0f / (std::max (1.0f, p[DuckAtk]) * 0.001f * fsf));
            const float aDuckR = std::exp (-1.0f / (std::max (20.0f, p[DuckRel]) * 0.001f * fsf));
            const float thrDuck = p[DuckThr], amtDuck = p[DuckAmt];
            const float aEnvR = std::exp (-1.0f / (0.06f * fsf));
            const float aGateA = 1.0f / (std::max (0.5f, p[GateAtk]) * 0.001f * fsf);
            const float aGateR = 1.0f / (std::max (5.0f, p[GateRel]) * 0.001f * fsf);
            const float thrGate = dbAGuany (p[GateThr]);
            const float holdGate = p[GateHold] * 0.001f * fsf;
            const float holdGated = std::clamp (sDecay * 0.35f, 0.06f, 1.4f) * fsf;
            const float thrGated = dbAGuany (-42.0f);

            const bool monoIn = p[EntradaMode] > 0.5f;
            const bool greusMono = p[GreusMono] > 0.5f;
            const float gMono = coefPol (140.0, fs);
            const float gDc = coefPol (12.0, fs);
            const bool digital = alg == Digital;
            const float gPre = std::exp (-1.0f / (0.06f * fsf));

            for (int s = 0; s < n; ++s)
            {
                const float t = (float) (s + 1) / (float) n;
                const float mix = mix0 + (sMix - mix0) * t;
                const float wid = wid0 + (sWidth - wid0) * t;
                const float out = out0 + (sOut - out0) * t;
                float dL = L[s], dR = R[s];
                if (! std::isfinite (dL)) dL = 0.0f;
                if (! std::isfinite (dR)) dR = 0.0f;
                picE = std::max (picE, std::max (std::fabs (dL), std::fabs (dR)));
                float xL = dL, xR = dR;
                if (monoIn) xL = xR = 0.5f * (dL + dR);

                // Filtres d'entrada i pre-delay.
                xL = inLP[0] (inHP[0] (xL));
                xR = inLP[1] (inHP[1] (xR));
                pre[0].escriu (xL);
                pre[1].escriu (xR);
                preActual = preObj + (preActual - preObj) * gPre;
                const float dPre = std::max (1.0f, preActual);
                xL = pre[0].lineal (dPre);
                xR = pre[1].lineal (dPre);

                // Envolupants de la font (per a gate i ducking).
                const float key = std::max (std::fabs (dL), std::fabs (dR));
                envGate = key > envGate ? key : envGate * aEnvR;

                float wL = 0.0f, wR = 0.0f;
                // ---- Reflexions primerenques ----
                if (numTaps > 0 && (gEarly > 0.0f))
                {
                    er[0].escriu (xL);
                    er[1].escriu (xR);
                    float eL = 0.0f, eR = 0.0f;
                    for (int k = 0; k < numTaps; ++k)
                    {
                        eL += er[0].llegeix (tapPos[0][k]) * taps[0][k].g;
                        eR += er[1].llegeix (tapPos[1][k]) * taps[1][k].g;
                    }
                    eL = erSmear[0][1] (erSmear[0][0] (eL, gSmear), gSmear * 0.8f);
                    eR = erSmear[1][1] (erSmear[1][0] (eR, gSmear), gSmear * 0.8f);
                    wL += erDamp[0].baix (eL, gErDamp) * gEarly;
                    wR += erDamp[1].baix (eR, gErDamp) * gEarly;
                }

                // ---- Cua ----
                float lL = 0.0f, lR = 0.0f;
                if (esTanc)
                {
                    float iL = xL * entraTanc, iR = xR * entraTanc;
                    if (digital)
                    {
                        if ((decim++ & 1) == 0) { decimL = std::round (iL * 2048.0f) / 2048.0f; decimR = std::round (iR * 2048.0f) / 2048.0f; }
                        iL = decimL; iR = decimR;
                    }
                    for (int k = 0; k < nDif; ++k) { iL = difE[0][k] (iL, gDif); iR = difE[1][k] (iR, gDif); }
                    const float sh = shimRet;
                    float o[maxN];
                    for (int i = 0; i < N; ++i)
                    {
                        auto& l = lt[i];
                        // Modulació de cada línia.
                        float md = 0.0f;
                        if (prof > 0.0f)
                        {
                            l.fase += pasFase * (1.0f + 0.13f * (float) i / (float) N);
                            if (l.fase >= 1.0f) l.fase -= 1.0f;
                            const float ph = l.fase + (float) i / (float) N;
                            const float sn = std::sin (6.2831853f * ph);
                            if (modo == 0) md = sn;
                            else
                            {
                                if (--l.aleatCompte <= 0)
                                {
                                    l.aleatCompte = std::max (16, (int) (fsf / std::max (0.05f, rateHz) * (0.6f + 0.8f * l.rnd.u())));
                                    l.aleatObj = l.rnd.b();
                                    l.aleatPas = (l.aleatObj - l.aleat) / (float) l.aleatCompte;
                                }
                                l.aleat += l.aleatPas;
                                md = modo == 1 ? l.aleat : 0.7f * sn + 0.5f * l.aleat;
                            }
                            if (qualitat == 0 && (i & 1)) md = 0.0f;
                            md = (md + 1.0f) * prof;
                        }
                        const float d = l.len + md + 2.0f;
                        float y = hermite ? l.l.hermite (d) : l.l.lineal (d);
                        if (apLlac) y = l.ap (y, gAp);
                        // Decay EQ (prestatges d'un pol), guany i damping.
                        const float lpL = l.fL.baix (y, gCG);
                        y = y + (l.kL - 1.0f) * lpL;
                        const float lpH = l.fH.baix (y, gCA);
                        y = lpH + l.kH * (y - lpH);
                        y *= l.gM;
                        const float lpD = l.fD.baix (y, gDamp);
                        y = lpD + dampBypass * (y - lpD);
                        o[i] = y;
                        lL += y * sgnL[i];
                        lR += y * sgnR[i];
                    }
                    // Barreja (Hadamard ràpid).
                    for (int h = 1; h < N; h <<= 1)
                        for (int i = 0; i < N; i += h << 1)
                            for (int j = i; j < i + h; ++j)
                            {
                                const float x0 = o[j], x1 = o[j + h];
                                o[j] = x0 + x1;
                                o[j + h] = x0 - x1;
                            }
                    for (int i = 0; i < N; ++i)
                    {
                        const float in = (iL * injL[i] + iR * injR[i]) * injNorm + sh * (((i * 5) & 2) ? -0.7f : 0.7f) * hadNorm * 2.0f;
                        lt[i].l.escriu (protegeix (o[i] * hadNorm + in + ((s & 1) ? 1e-20f : -1e-20f)));
                    }
                    lL *= outNorm;
                    lR *= outNorm;
                    // Shimmer: la cua transposada torna a entrar.
                    if (fesShim)
                    {
                        shim.escriu (0.5f * (lL + lR));
                        shimFase += pasShim;
                        shimFase -= std::floor (shimFase);
                        float ps = 0.0f;
                        for (int h = 0; h < 2; ++h)
                        {
                            float ph = shimFase + 0.5f * (float) h;
                            if (ph >= 1.0f) ph -= 1.0f;
                            const float fin = std::sin (3.14159265f * ph);
                            ps += shim.lineal (ph * W + 2.0f) * fin * fin;
                        }
                        ps -= shimHP.baix (ps, gShimHP);
                        shimRet = ps * sShim * 0.85f;
                    }
                    else shimRet = 0.0f;
                    lL *= normCua;
                    lR *= normCua;
                }
                else if (alg == Spring)
                {
                    const float ent[numMolls] = { xL * entraTanc, xR * entraTanc, 0.5f * (xL + xR) * entraTanc };
                    float so[numMolls];
                    for (int k = 0; k < numMolls; ++k)
                    {
                        auto& mo = molls[k];
                        mo.fase += (0.9f + 0.37f * (float) k) / fsf;
                        if (mo.fase >= 1.0f) mo.fase -= 1.0f;
                        const float d = mo.len * (0.7f + 0.6f * sSize) + (1.0f + std::sin (6.2831853f * mo.fase)) * (0.0004f + 0.0012f * sMod) * fsf;
                        const float y = mo.l.lineal (d);
                        so[k] = y;
                        float v = protegeix (ent[k] * 0.6f + y * mo.g);
                        for (int e = 0; e < etapes; ++e)
                        {
                            const float yy = aMoll * v + mo.x1[e] - aMoll * mo.y1[e];
                            mo.x1[e] = v;
                            mo.y1[e] = yy;
                            v = yy;
                        }
                        mo.l.escriu (mo.lp.baix (v, gMollLP) + ((s & 1) ? 1e-20f : -1e-20f));
                    }
                    lL = (so[0] + 0.6f * so[2]) * 0.9f;
                    lR = (so[1] - 0.6f * so[2]) * 0.9f;
                }
                else if (alg == Reverse)
                {
                    float iL = xL, iR = xR;
                    for (int k = 0; k < 4; ++k) { iL = difE[0][k] (iL, 0.6f * sDif + 0.1f); iR = difE[1][k] (iR, 0.6f * sDif + 0.1f); }
                    rv[0].escriu (iL);
                    rv[1].escriu (iR);
                    for (int k = 0; k < 40; ++k)
                    {
                        lL += rv[0].llegeix (rvPos[0][k]) * rvTaps[0][k].g;
                        lR += rv[1].llegeix (rvPos[1][k]) * rvTaps[1][k].g;
                    }
                    lL = erSmear[0][1] (erSmear[0][0] (lL, 0.6f), 0.5f);
                    lR = erSmear[1][1] (erSmear[1][0] (lR, 0.6f), 0.5f);
                    wL = wR = 0.0f;
                }
                wL += lL * gLate;
                wR += lR * gLate;

                // ---- Gated (algorisme): la cua es manté mentre hi ha so i es talla en sec ----
                if (alg == Gated)
                {
                    if (envGate > thrGated) gatedHold = holdGated;
                    else if (gatedHold > 0.0f) gatedHold -= 1.0f;
                    const float obj = gatedHold > 0.0f ? 1.0f : 0.0f;
                    gated += (obj - gated) * (obj > gated ? 0.02f : 1.0f / (0.012f * fsf));
                    wL *= gated; wR *= gated;
                }
                // ---- Gate (secció) ----
                if (gateOn)
                {
                    if (envGate > thrGate) gateHold = holdGate;
                    else if (gateHold > 0.0f) gateHold -= 1.0f;
                    if (gateHold > 0.0f) gate = std::min (1.0f, gate + aGateA);
                    else gate = std::max (0.0f, gate - aGateR);
                    const float gg = gate * gate;
                    wL *= gg; wR *= gg;
                }
                else gate = 1.0f;

                // ---- Reverse creatiu: es torna a llegir la cua al revés, a trossos que s'encavalquen ----
                if (revOn)
                {
                    rc[0].escriu (wL);
                    rc[1].escriu (wR);
                    rcCompte = (rcCompte + 1) % std::max (2, lrc);
                    float aL = 0.0f, aR = 0.0f;
                    for (int h = 0; h < 2; ++h)
                    {
                        int k = rcCompte + h * (lrc / 2);
                        if (k >= lrc) k -= lrc;
                        const float ph = (float) k / (float) lrc;
                        const float fin = std::sin (3.14159265f * ph);
                        const int d = 1 + 2 * k;
                        aL += rc[0].llegeix (d) * fin * fin;
                        aR += rc[1].llegeix (d) * fin * fin;
                    }
                    wL = aL; wR = aR;
                }

                // ---- Color, EQ de sortida, greus al centre ----
                if (pf.color > 0.0f) { wL = color[0] (wL); wR = color[1] (wR); }
                if (! eqPla)
                    for (int k = 0; k < 4; ++k) { wL = eq[0][k] (wL); wR = eq[1][k] (wR); }
                // Treu el continu (els llaços i el shimmer en poden fer).
                wL -= dcBlock[0].baix (wL, gDc);
                wR -= dcBlock[1].baix (wR, gDc);
                float mid = 0.5f * (wL + wR), sid = 0.5f * (wL - wR);
                if (greusMono) sid -= monoLP[0].baix (sid, gMono);
                sid *= wid;
                wL = mid + sid;
                wR = mid - sid;

                // ---- Ducking ----
                if (duckOn)
                {
                    envDuck = key > envDuck ? key : envDuck * aEnvR;   // detector ràpid; l'atac i el retorn són del guany
                    const float over = guanyADb (envDuck) - thrDuck;
                    const float red = limita (over / 6.0f, 0.0f, 1.0f) * amtDuck;
                    const float obj = dbAGuany (-red);
                    guanyDuck = obj < guanyDuck ? obj + (guanyDuck - obj) * aDuckA : obj + (guanyDuck - obj) * aDuckR;
                    wL *= guanyDuck; wR *= guanyDuck;
                }
                else guanyDuck = 1.0f;

                const float gw = out * fosa;
                wL = protegeix (wL * gw);
                wR = protegeix (wR * gw);
                // Mesures del «wet».
                sumW += 0.5f * (wL * wL + wR * wR);
                sumLL += wL * wL; sumRR += wR * wR; sumLR += wL * wR;
                const float mm = 0.5f * (wL + wR), ss = 0.5f * (wL - wR);
                sumM += mm * mm; sumS += ss * ss;
                ++numMes;

                // Mix (potència constant).
                const float gDry = std::cos (mix * 1.5707963f), gWet = std::sin (mix * 1.5707963f);
                float yL = dL * gDry + wL * gWet, yR = dR * gDry + wR * gWet;
                if (! std::isfinite (yL) || ! std::isfinite (yR)) { yL = dL; yR = dR; buida(); }
                L[s] = yL;
                R[s] = yR;
                picS = std::max (picS, std::max (std::fabs (yL), std::fabs (yR)));
            }
        }
    };

    std::vector<std::pair<float, float>> reflexionsVisibles (const Params& p)
    {
        std::vector<std::pair<float, float>> v;
        Reverb::Nucli n;
        n.fs = 48000.0;
        n.configura ((int) p[Alg], 1, p);
        const float s01 = p[Size] / 100.0f;
        const float esc = n.alg == Early ? (0.25f + 1.1f * s01) : n.escalaMida (s01);
        const float bal = p[EarlyLate] / 100.0f;
        const float gE = n.alg == Early ? 1.0f : std::min (1.0f, 2.0f * (1.0f - bal)) * n.pf.erGuany;
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < n.numTaps; ++k)
                v.push_back ({ (n.pf.erMinMs + n.pf.erSpanMs * n.taps[c][k].pos) * esc * 0.001f, n.taps[c][k].g * gE });
        return v;
    }

    // =========================================================================
    Reverb::Reverb() : nucli (std::make_unique<Nucli>())
    {
        const Params p = presetInicial().p;
        for (int i = 0; i < numParams; ++i) par[(size_t) i].store (p[i], std::memory_order_relaxed);
    }

    Reverb::~Reverb() = default;

    void Reverb::prepara (double fs, int)
    {
        if (nucli->preparat && std::fabs (nucli->fs - fs) < 0.5) { nucli->buida(); return; }
        nucli->prepara (fs);
    }

    void Reverb::posa (int p, float v) noexcept
    {
        if (p < 0 || p >= numParams || ! std::isfinite (v)) return;
        const auto& i = infos[p];
        v = limita (v, i.mn, i.mx);
        if (i.enter) v = (float) std::lround (v);
        par[(size_t) p].store (v, std::memory_order_relaxed);
        canvis.fetch_add (1, std::memory_order_relaxed);
    }

    float Reverb::valor (int p) const noexcept { return p >= 0 && p < numParams ? par[(size_t) p].load (std::memory_order_relaxed) : 0.0f; }

    Params Reverb::params() const
    {
        Params p;
        for (int i = 0; i < numParams; ++i) p.v[(size_t) i] = par[(size_t) i].load (std::memory_order_relaxed);
        return p;
    }

    void Reverb::posaParams (const Params& p) noexcept
    {
        for (int i = 0; i < numParams; ++i) par[(size_t) i].store (p[i], std::memory_order_relaxed);
        canvis.fetch_add (1, std::memory_order_relaxed);
    }

    void Reverb::processa (float* L, float* R, int n) noexcept
    {
        if (! nucli->preparat || n <= 0) return;
#ifdef MIBASE_REVERB_FTZ
        const unsigned int csr = _mm_getcsr();
        _mm_setcsr (csr | 0x8040);   // flush-to-zero i denormals-are-zero
#endif
        if (demanaBuidar.exchange (false, std::memory_order_acq_rel)) nucli->buida();
        Params p;
        for (int i = 0; i < numParams; ++i) p.v[(size_t) i] = par[(size_t) i].load (std::memory_order_relaxed);
        const double bpm = tempo != nullptr ? tempo->load (std::memory_order_relaxed) : 120.0;
        nucli->processa (*this, p, bpm > 1.0 ? bpm : 120.0, L, R, n);
#ifdef MIBASE_REVERB_FTZ
        _mm_setcsr (csr);
#endif
    }
}
