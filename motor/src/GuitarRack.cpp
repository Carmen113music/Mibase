#include "mibase/GuitarRack.h"

#include "mibase/Convolucio.h"
#include "mibase/Delay.h"
#include "mibase/Reverb.h"

#include <nlohmann/json.hpp>

#include <set>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <mutex>

#if defined(__SSE__) || defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__)
 #include <xmmintrin.h>
 #define MIBASE_GR_FTZ 1
#endif

// 1.5: el motor del Guitar Rack (amplis, pantalles i pedals). Vegeu GuitarRack.h.
namespace mibase::guitarrack
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;
        inline float dbLin (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
        inline float lim (float v, float a, float b) noexcept { return v < a ? a : (v > b ? b : v); }
        inline float tanhR (float x) noexcept
        {
            if (x > 3.0f) return 1.0f;
            if (x < -3.0f) return -1.0f;
            const float x2 = x * x;
            return x * (27.0f + x2) / (27.0f + 9.0f * x2);
        }
        inline float dur (float x) noexcept { return lim (x, -1.0f, 1.0f); }
        /** Corba de saturació: barreja de suau (tanh) i dura segons «k» (0..1). */
        inline float corba (float x, float k) noexcept { return k <= 0.0f ? tanhR (x) : tanhR (x) * (1.0f - k) + dur (x) * k; }
        inline float coefSuau (double fs, double ms) noexcept { return (float) (1.0 - std::exp (-1.0 / (0.001 * ms * fs))); }

        // ------------------------------------------------------------------ biquad
        struct Bq
        {
            float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
            inline float operator() (float x) noexcept
            {
                const float y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;
                return y;
            }
            void neteja() noexcept { z1 = z2 = 0.0f; }
            void pla() noexcept { b0 = 1; b1 = b2 = a1 = a2 = 0; }
            void posa (double B0, double B1, double B2, double A0, double A1, double A2) noexcept
            {
                b0 = (float) (B0 / A0); b1 = (float) (B1 / A0); b2 = (float) (B2 / A0); a1 = (float) (A1 / A0); a2 = (float) (A2 / A0);
            }
            void antiDenormal() noexcept { if (std::abs (z1) < 1e-20f) z1 = 0.0f; if (std::abs (z2) < 1e-20f) z2 = 0.0f; }
        };
        double limF (double f, double fs) { return std::clamp (f, 10.0, fs * 0.47); }
        void lp (Bq& b, double f, double q, double fs)
        {
            const double w = 2 * pi * limF (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
            b.posa ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
        }
        void hp (Bq& b, double f, double q, double fs)
        {
            const double w = 2 * pi * limF (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
            b.posa ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
        }
        void lp1 (Bq& b, double f, double fs)
        {
            const double k = std::tan (pi * limF (f, fs) / fs);
            b.posa (k, k, 0, k + 1, k - 1, 0);
        }
        void hp1 (Bq& b, double f, double fs)
        {
            const double k = std::tan (pi * limF (f, fs) / fs);
            b.posa (1, -1, 0, k + 1, k - 1, 0);
        }
        void pic (Bq& b, double f, double q, double db, double fs)
        {
            if (std::abs (db) < 0.01) { b.pla(); return; }
            const double A = std::pow (10.0, db / 40.0), w = 2 * pi * limF (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
            b.posa (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
        }
        void prestatge (Bq& b, double f, double db, double fs, bool alt)
        {
            if (std::abs (db) < 0.01) { b.pla(); return; }
            const double A = std::pow (10.0, db / 40.0), w = 2 * pi * limF (f, fs) / fs, c = std::cos (w), s = std::sin (w);
            const double al = s / 2 * std::sqrt (2.0), sa = 2 * std::sqrt (A) * al;
            if (alt) b.posa (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
                             (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
            else b.posa (A * ((A + 1) - (A - 1) * c + sa), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sa),
                         (A + 1) + (A - 1) * c + sa, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sa);
        }

        // ------------------------------------------------------------------ sobremostreig ×4
        struct Sobre4
        {
            Bq up[3], dn[3];
            void prepara (double fs)
            {
                const double f4 = fs * 4.0, fc = std::min (0.43 * fs, 20000.0);
                const double q[3] = { 0.5176, 0.7071, 1.9319 };
                for (int i = 0; i < 3; ++i) { lp (up[i], fc, q[i], f4); lp (dn[i], fc, q[i], f4); }
                neteja();
            }
            void neteja() noexcept { for (auto& b : up) b.neteja(); for (auto& b : dn) b.neteja(); }
            template <typename F>
            inline float passa (float x, F&& f) noexcept
            {
                float y = 0.0f;
                for (int k = 0; k < 4; ++k)
                {
                    float u = k == 0 ? x * 4.0f : 0.0f;
                    u = up[0] (u); u = up[1] (u); u = up[2] (u);
                    float w = f (u);
                    w = dn[0] (w); w = dn[1] (w); w = dn[2] (w);
                    if (k == 3) y = w;
                }
                return y;
            }
            void antiDenormal() noexcept { for (auto& b : up) b.antiDenormal(); for (auto& b : dn) b.antiDenormal(); }
        };

        // ------------------------------------------------------------------ pila de tons (Yeh i Smith, components reals)
        struct Components { double R1, R2, R3, R4, C1, C2, C3; };
        Components components (int p)
        {
            switch (p)
            {
                case PFender:   return { 250e3, 1e6, 25e3, 56e3, 250e-12, 20e-9, 20e-9 };
                case PMarshall: return { 220e3, 1e6, 22e3, 33e3, 470e-12, 22e-9, 22e-9 };
                case PVox:      return { 220e3, 1e6, 10e3, 100e3, 220e-12, 22e-9, 47e-9 };
                case PMesa:     return { 250e3, 250e3, 25e3, 100e3, 250e-12, 100e-9, 47e-9 };
                case PHiwatt:   return { 220e3, 1e6, 25e3, 47e3, 330e-12, 22e-9, 22e-9 };
                case PModern:   return { 220e3, 1e6, 25e3, 39e3, 500e-12, 22e-9, 22e-9 };
                case POrange:   return { 220e3, 1e6, 25e3, 33e3, 560e-12, 47e-9, 22e-9 };
                default:        return { 220e3, 1e6, 22e3, 33e3, 470e-12, 22e-9, 22e-9 };
            }
        }
        struct PilaTons
        {
            double b[4] {}, a[4] {};
            double z[3] {};
            double B[4] {}, A[4] {};   // en doble: a ×4 els pols queden molt a prop d'1
            /** t, m, l: 0..1 (l ja amb la corba logarítmica). */
            static void analog (const Components& c, double t, double m, double l, double* bb, double* aa)
            {
                const double R1 = c.R1, R2 = c.R2, R3 = c.R3, R4 = c.R4, C1 = c.C1, C2 = c.C2, C3 = c.C3;
                bb[0] = 0.0;
                bb[1] = t * C1 * R1 + m * C3 * R3 + l * (C1 * R2 + C2 * R2) + (C1 * R3 + C2 * R3);
                bb[2] = t * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4) - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                        + m * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                        + l * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4)
                        + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
                        + (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);
                bb[3] = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
                        - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                        + m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                        + t * C1 * C2 * C3 * R1 * R3 * R4 - t * m * C1 * C2 * C3 * R1 * R3 * R4
                        + t * l * C1 * C2 * C3 * R1 * R2 * R4;
                aa[0] = 1.0;
                aa[1] = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4) + m * C3 * R3 + l * (C1 * R2 + C2 * R2);
                aa[2] = m * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                        + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
                        - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
                        + l * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4)
                        + (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4 + C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4);
                aa[3] = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
                        - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
                        + m * (C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3 - C1 * C2 * C3 * R1 * R3 * R4)
                        + l * C1 * C2 * C3 * R1 * R2 * R4 + C1 * C2 * C3 * R1 * R3 * R4;
            }
            /** El guany (lineal) de la resposta analògica a la freqüència f. */
            static double modul (const double* bb, const double* aa, double f)
            {
                const double w = 2 * pi * f;
                // s = jw: num = b1 s + b2 s² + b3 s³
                const double nr = -bb[2] * w * w, ni = bb[1] * w - bb[3] * w * w * w;
                const double dr = aa[0] - aa[2] * w * w, di = aa[1] * w - aa[3] * w * w * w;
                return std::sqrt ((nr * nr + ni * ni) / std::max (1e-30, dr * dr + di * di));
            }
            void calcula (const Components& c, double t, double m, double l, double fs, double norma)
            {
                analog (c, std::clamp (t, 0.0, 1.0), std::clamp (m, 0.0, 1.0), std::clamp (l, 0.0, 1.0), b, a);
                const double k = 2.0 * fs, k2 = k * k, k3 = k2 * k;
                const double B0 = -b[1] * k - b[2] * k2 - b[3] * k3;
                const double B1 = -b[1] * k + b[2] * k2 + 3 * b[3] * k3;
                const double B2 = b[1] * k + b[2] * k2 - 3 * b[3] * k3;
                const double B3 = b[1] * k - b[2] * k2 + b[3] * k3;
                const double A0 = -a[0] - a[1] * k - a[2] * k2 - a[3] * k3;
                const double A1 = -3 * a[0] - a[1] * k + a[2] * k2 + 3 * a[3] * k3;
                const double A2 = -3 * a[0] + a[1] * k + a[2] * k2 - 3 * a[3] * k3;
                const double A3 = -a[0] + a[1] * k - a[2] * k2 + a[3] * k3;
                B[0] = B0 / A0 * norma; B[1] = B1 / A0 * norma; B[2] = B2 / A0 * norma; B[3] = B3 / A0 * norma;
                A[1] = A1 / A0; A[2] = A2 / A0; A[3] = A3 / A0;
            }
            inline float operator() (float xf) noexcept
            {
                const double x = xf, y = B[0] * x + z[0];
                z[0] = B[1] * x - A[1] * y + z[1];
                z[1] = B[2] * x - A[2] * y + z[2];
                z[2] = B[3] * x - A[3] * y;
                return (float) y;
            }
            void neteja() noexcept { z[0] = z[1] = z[2] = 0.0; }
        };
        /** La corba logarítmica del pom de greus (com un potenciòmetre de tipus A). */
        double corbaGreus (double v01) { return (std::exp (3.4 * (v01 - 1.0)) - std::exp (-3.4)) / (1.0 - std::exp (-3.4)); }

        // ------------------------------------------------------------------ línia de retard
        struct Linia
        {
            std::vector<float> b;
            int w = 0, mascara = 0;
            void prepara (int minim)
            {
                int n = 1;
                while (n < minim + 4) n <<= 1;
                b.assign ((size_t) n, 0.0f);
                mascara = n - 1;
                w = 0;
            }
            void neteja() noexcept { std::fill (b.begin(), b.end(), 0.0f); }
            inline void escriu (float x) noexcept { b[(size_t) w] = x; w = (w + 1) & mascara; }
            /** d mostres enrere (fraccionari; d ≥ 1). */
            inline float llegeix (float d) const noexcept
            {
                const float p = (float) w - d;
                const int i = (int) std::floor (p);
                const float fr = p - (float) i;
                const float a = b[(size_t) (i & mascara)];
                const float a1 = b[(size_t) ((i + 1) & mascara)];
                return a + (a1 - a) * fr;
            }
        };

        // ------------------------------------------------------------------ canvi de to (dos capçals que giren)
        struct Desplacador
        {
            Linia l;
            float fin = 1920.0f;
            double fase = 0.0;
            void prepara (double fs, double ms) { fin = (float) (fs * ms * 0.001); l.prepara ((int) fin + 8); fase = 0.0; }
            void neteja() noexcept { l.neteja(); fase = 0.0; }
            inline float passa (float x, float ratio) noexcept
            {
                l.escriu (x);
                fase += (1.0 - (double) ratio) / (double) fin;
                fase -= std::floor (fase);
                const float f1 = (float) fase, f2 = (float) (fase + 0.5 - std::floor (fase + 0.5));
                const float g1 = std::sin ((float) pi * f1), g2 = std::sin ((float) pi * f2);
                return l.llegeix (1.0f + f1 * fin) * g1 * g1 + l.llegeix (1.0f + f2 * fin) * g2 * g2;
            }
        };

        float valvulaC (float x, float biaix, float k) noexcept { return corba (x + biaix, k) - corba (biaix, k); }
    }

    // ==================================================================================== paràmetres fixos
    const InfoFix& infoFix (int p)
    {
        static const InfoFix t[numFixos] = {
            { "entrada", "Entrada", -24, 24, 0, 0, "Guany a l'entrada del rack (dB): puja-ho si la guitarra entra fluixa." },
            { "sortida", "Sortida", -24, 12, 0, 0, "Volum de sortida del rack (dB). Al final hi ha una protecció perquè no saturi." },
            { "gate", "Gate", -96, -30, -96, 0, "Porta de soroll a l'entrada: per sota d'aquest nivell es tanca. Al mínim, apagada." },
            { "ampon", "Ampli", 0, 1, 1, 2, "Encén o apaga l'amplificador." },
            { "canal", "Canal", 0, 3, 0, 4, "El canal de l'ampli (si en té)." },
            { "bright", "Interruptor", 0, 1, 0, 2, "L'interruptor de l'ampli (Bright, Boost, Tight o Tone Shift, segons el model)." },
            { "gain", "Gain", 0, 10, 5, 0, "Guany del preamplificador." },
            { "bass", "Bass", 0, 10, 5, 0, "Greus (a la pila de tons de l'ampli)." },
            { "middle", "Middle", 0, 10, 5, 0, "Mitjos (a la pila de tons de l'ampli)." },
            { "treble", "Treble", 0, 10, 5, 0, "Aguts (a la pila de tons de l'ampli)." },
            { "presence", "Presence", 0, 10, 5, 0, "Presència: aguts a l'etapa de potència." },
            { "master", "Master", 0, 10, 5, 0, "Volum general de l'ampli: com més alt, més treballen les vàlvules de potència." },
            { "reverb", "Reverb", 0, 10, 0, 0, "La reverb de molla de l'ampli." },
            { "extra", "Extra", 0, 10, 5, 0, "El control propi del model (Resonance, Depth, Graphic V…)." },
            { "cabon", "Pantalla", 0, 1, 1, 2, "Encén o apaga la pantalla (sense pantalla, el so de l'ampli és molt aspre)." },
            { "mica", "Micro A", 0, 3, 0, 4, "El micròfon principal." },
            { "micb", "Micro B", 0, 3, 2, 4, "El segon micròfon (es barreja amb «Mescla B»)." },
            { "posa", "Posició A", 0, 1, 0.3f, 0, "On apunta el micròfon A: 0 = al centre del con (brillant), 1 = a la vora (fosc)." },
            { "posb", "Posició B", 0, 1, 0.6f, 0, "On apunta el micròfon B." },
            { "dista", "Distància A", 0, 1, 0.1f, 0, "Distància del micròfon A: a prop, més greus (proximitat); lluny, més sala." },
            { "distb", "Distància B", 0, 1, 0.3f, 0, "Distància del micròfon B." },
            { "anglea", "Angle A", 0, 45, 0, 0, "Angle del micròfon A (graus): girat, menys aguts." },
            { "angleb", "Angle B", 0, 45, 15, 0, "Angle del micròfon B (graus)." },
            { "mesclab", "Mescla B", 0, 100, 0, 0, "Quant del micròfon B es barreja amb l'A (%)." },
            { "room", "Sala", 0, 100, 10, 0, "Ambient de l'habitació on hi ha la pantalla (%)." },
            { "iron", "IR", 0, 1, 0, 2, "Fes servir la IR de pantalla carregada (en lloc de la simulació, o barrejada)." },
            { "irmescla", "Mescla IR", 0, 100, 100, 0, "Quant de la IR se sent: 0 % només la simulació de Mibase, 100 % només la IR." },
        };
        return t[std::clamp (p, 0, numFixos - 1)];
    }

    // ==================================================================================== pedal (base)
    Pedal::Pedal (int model) : idx (model)
    {
        const auto& m = this->model();
        for (int c = 0; c < maxControls; ++c) val[(size_t) c].store (c < (int) m.controls.size() ? m.controls[(size_t) c].def : 0.0f);
    }

    float Pedal::normal (const Control& c, float v) noexcept
    {
        if (c.mx <= c.mn) return 0.0f;
        v = lim (v, c.mn, c.mx);
        if (c.log && c.mn > 0.0f) return std::log (v / c.mn) / std::log (c.mx / c.mn);
        return (v - c.mn) / (c.mx - c.mn);
    }
    float Pedal::deNormal (const Control& c, float v01) noexcept
    {
        v01 = lim (v01, 0.0f, 1.0f);
        float v = c.log && c.mn > 0.0f ? c.mn * std::pow (c.mx / c.mn, v01) : c.mn + v01 * (c.mx - c.mn);
        if (c.tipus == TipusControl::Selector || c.tipus == TipusControl::Interruptor) v = std::round (v);
        return v;
    }

    void Pedal::posa (int c, float v) noexcept
    {
        const auto& m = model();
        if (c < 0 || c >= (int) m.controls.size() || ! std::isfinite (v)) return;
        const auto& k = m.controls[(size_t) c];
        v = lim (v, k.mn, k.mx);
        if (k.tipus == TipusControl::Selector || k.tipus == TipusControl::Interruptor) v = std::round (v);
        val[(size_t) c].store (v, std::memory_order_relaxed);
    }

    std::string Pedal::text (int c, float v) const
    {
        const auto& m = model();
        if (c < 0 || c >= (int) m.controls.size()) return {};
        const auto& k = m.controls[(size_t) c];
        char b[48];
        if (! k.opcions.empty()) return k.opcions[(size_t) std::clamp ((int) std::lround (v), 0, (int) k.opcions.size() - 1)];
        if (k.unitat == "ms") { if (v >= 1000.0f) std::snprintf (b, sizeof b, "%.2f s", (double) v / 1000.0); else std::snprintf (b, sizeof b, "%.0f ms", (double) v); }
        else if (k.unitat == "dB") std::snprintf (b, sizeof b, "%+.1f dB", (double) v);
        else if (k.unitat == "%") std::snprintf (b, sizeof b, "%.0f %%", (double) v);
        else std::snprintf (b, sizeof b, "%.1f", (double) v);
        std::string s = b;
        for (auto& ch : s) if (ch == '.') ch = ',';
        return s;
    }

    void Pedal::prepara (double f, int maxBloc)
    {
        fs = f;
        secL.assign ((size_t) std::max (1, maxBloc), 0.0f);
        secR.assign ((size_t) std::max (1, maxBloc), 0.0f);
        gOn = on.load() ? 1.0f : 0.0f;
        estavaOn = on.load();
        preparaMotor (f, maxBloc);
    }

    void Pedal::passa (float* L, float* R, int n) noexcept
    {
        const bool o = on.load (std::memory_order_relaxed);
        if (! o && gOn <= 0.0f) { calBuidar = true; act.store (0.0f, std::memory_order_relaxed); return; }   // bypass de debò
        if (o && calBuidar) { buida(); calBuidar = false; }
        if (o && gOn >= 1.0f) { processa (L, R, n); return; }
        n = std::min (n, (int) secL.size());
        std::copy (L, L + n, secL.data());
        if (R != nullptr) std::copy (R, R + n, secR.data());
        processa (L, R, n);
        const float pas = 1.0f / (float) std::max (1.0, 0.012 * fs) * (o ? 1.0f : -1.0f);
        for (int i = 0; i < n; ++i)
        {
            gOn = lim (gOn + pas, 0.0f, 1.0f);
            L[i] = secL[(size_t) i] + (L[i] - secL[(size_t) i]) * gOn;
            if (R != nullptr) R[i] = secR[(size_t) i] + (R[i] - secR[(size_t) i]) * gOn;
        }
    }

    // ==================================================================================== motors dels pedals
    namespace
    {
        // ---------------------------------------------------------------------------- Drive
        struct VarDrive
        {
            float gMin, gMax, hpPre, lpPre, midF, midDb;
            int clip;            // 0 suau, 1 asimètric, 2 dur, 3 germani, 4 fuzz de dues etapes, 5 vàlvula de dues etapes
            float biaix, net;
            int to;              // 0 LP, 1 inclinació, 2 barreja LP/HP, 3 filtre RAT, 4 tres bandes, 5 greus/aguts/presència, 6 fix, 7 OCD
            float tMin, tMax, trim;
            int iGain, iTone, iLevel, iA, iB, iC, iD;   // extres segons el tipus de to
        };
        const VarDrive& varDrive (int v)
        {
            static const VarDrive t[] = {
                // gMin gMax  hpPre  lpPre  midF  midDb clip biaix net  to  tMin  tMax  trim   gain tone level  A  B  C  D
                { 12, 41, 720, 6000, 0, 0,     0, 0.00f, 1.0f, 0, 900, 6000, -4.0f, 0, 1, 2, -1, -1, -1, -1 },   // TS808
                { 13, 42, 720, 7000, 0, 0,     0, 0.00f, 1.0f, 0, 1100, 7500, -4.5f, 0, 1, 2, -1, -1, -1, -1 },  // TS9
                { 12, 40, 700, 6500, 0, 0,     1, 0.25f, 0.7f, 0, 1000, 7000, -4.0f, 2, 1, 0, -1, -1, -1, -1 },  // SD-1
                { 18, 46, 120, 8000, 0, 0,     2, 0.00f, 0.0f, 2, 450, 1800, -9.0f, 2, 1, 0, -1, -1, -1, -1 },   // DS-1
                { 15, 62, 60, 0, 0, 0,         2, 0.05f, 0.0f, 3, 475, 22000, -10.0f, 0, 1, 2, -1, -1, -1, -1 }, // RAT
                { 6, 38, 90, 9000, 0, 0,       1, 0.20f, 0.0f, 1, 0, 0, -6.0f, 2, 1, 0, -1, -1, -1, -1 },        // BD-2
                { 3, 34, 150, 8000, 1000, 2,   3, 0.05f, 0.6f, 1, 0, 0, -3.0f, 0, 1, 2, -1, -1, -1, -1 },        // Klon
                { 6, 44, 80, 8000, 0, 0,       1, 0.30f, 0.0f, 7, 1200, 9000, -7.0f, 1, 2, 0, 3, -1, -1, -1 },   // OCD
                { 20, 54, 80, 0, 0, 0,         4, 0.05f, 0.0f, 2, 600, 1300, -10.0f, 2, 1, 0, -1, -1, -1, -1 },  // Big Muff
                { 8, 44, 60, 6000, 0, 0,       3, 0.00f, 0.0f, 6, 0, 5000, -8.0f, 1, -1, 0, -1, -1, -1, -1 },    // Distortion+
                { 25, 62, 100, 7000, 1000, 5,  2, 0.05f, 0.0f, 4, 0, 0, -12.0f, 1, -1, 0, 2, 3, 4, 5 },          // MT-2 (A high, B low, C mid, D midF)
                { 12, 52, 0, 7500, 900, 3,     5, 0.30f, 0.0f, 5, 0, 0, -10.0f, 1, -1, 0, 2, 3, 4, 5 },          // BE-OD (A bass, B treble, C tight, D presence)
            };
            return t[std::clamp (v, 0, (int) (sizeof t / sizeof t[0]) - 1)];
        }

        class PedalDrive : public Pedal
        {
        public:
            using Pedal::Pedal;
        protected:
            void preparaMotor (double f, int) override
            {
                os.prepara (f);
                ultim.fill (-999.0f);
                buida();
            }
            void buida() noexcept override
            {
                os.neteja();
                for (Bq* b : { &hpPre, &midPre, &lpPre4, &entre4, &dc, &t1, &t2, &t3, &t4, &post, &tight }) b->neteja();
            }
            void recalcula (const VarDrive& d) noexcept
            {
                const double f = fs, f4 = fs * 4.0;
                const float tone = d.iTone >= 0 ? valor (d.iTone) : 5.0f;
                const float gain = valor (d.iGain);
                if (d.hpPre > 0) hp1 (hpPre, d.hpPre, f); else hpPre.pla();
                if (d.midDb != 0.0f) pic (midPre, d.midF, 0.8, d.midDb, f); else midPre.pla();
                // el LP abans de retallar: el de la RAT baixa quan puja el guany (l'operacional no arriba)
                const double lpp = d.lpPre > 0 ? d.lpPre : 1e6 / std::max (1.0, std::pow (10.0, (d.gMin + (d.gMax - d.gMin) * gain / 10.0) / 20.0)) + 1500.0;
                lp1 (lpPre4, std::min (lpp, 18000.0), f4);
                lp1 (entre4, d.clip == 4 ? 3200.0 : 5000.0, f4);
                hp1 (dc, 20.0, f);
                if (d.clip == 5)
                {
                    const float tt = d.iC >= 0 ? valor (d.iC) : 4.0f;
                    hp1 (tight, 50.0 + 250.0 * tt / 10.0, f);
                }
                else tight.pla();
                t1.pla(); t2.pla(); t3.pla(); t4.pla();
                switch (d.to)
                {
                    case 0: lp1 (t1, d.tMin * std::pow (d.tMax / d.tMin, tone / 10.0), f); break;
                    case 1: prestatge (t1, 900.0, (tone - 5.0f) * 1.4f, f, true); prestatge (t2, 300.0, -(tone - 5.0f) * 0.6f, f, false); lp1 (t3, 9000.0, f); break;
                    case 2: lp1 (t1, d.tMin, f); hp1 (t2, d.tMax, f); break;
                    case 3: lp1 (t1, d.tMax * std::pow (d.tMin / d.tMax, tone / 10.0), f); break;
                    case 4:
                        prestatge (t1, 4000.0, valor (d.iA), f, true);
                        prestatge (t2, 120.0, valor (d.iB), f, false);
                        pic (t3, 200.0 * std::pow (25.0, valor (d.iD) / 10.0), 1.2, valor (d.iC), f);
                        lp (t4, 7500.0, 0.7, f);
                        break;
                    case 5:
                        prestatge (t1, 150.0, (valor (d.iA) - 5.0f) * 1.8f, f, false);
                        prestatge (t2, 2500.0, (valor (d.iB) - 5.0f) * 1.8f, f, true);
                        prestatge (t3, 5000.0, (valor (d.iD) - 5.0f) * 1.2f, f, true);
                        lp (t4, 8000.0, 0.7, f);
                        break;
                    case 6: lp1 (t1, d.tMax, f); break;
                    case 7:
                    {
                        const bool hpMode = d.iA >= 0 && valor (d.iA) > 0.5f;
                        lp1 (t1, d.tMin * std::pow (d.tMax / d.tMin, tone / 10.0), f);
                        prestatge (t2, 2000.0, hpMode ? 3.0 : -1.0, f, true);
                        prestatge (t3, 120.0, hpMode ? -2.0 : 1.5, f, false);
                        break;
                    }
                    default: break;
                }
                lp (post, 11000.0, 0.7, f);
            }
            void processa (float* L, float* R, int n) noexcept override
            {
                const auto& d = varDrive (model().variant);
                bool canvi = false;
                for (int c = 0; c < numControls(); ++c) if (valor (c) != ultim[(size_t) c]) { ultim[(size_t) c] = valor (c); canvi = true; }
                if (canvi) recalcula (d);
                const float gain = valor (d.iGain) / 10.0f;
                const float gDb = d.gMin + (d.gMax - d.gMin) * (d.clip == 4 ? std::sqrt (gain) : gain);
                const float level = d.iLevel >= 0 ? valor (d.iLevel) : 5.0f;
                const float gOut = dbLin ((level - 5.0f) * 2.6f + d.trim);
                const float gIn = dbLin (d.clip == 4 || d.clip == 5 ? gDb * 0.5f : gDb);
                const float g2 = dbLin (gDb * 0.5f);
                const float kDur = d.clip == 2 ? 0.75f : (d.clip == 3 ? 0.45f : (d.clip == 4 ? 0.6f : 0.0f));
                const float b = d.biaix;
                const float cB = corba (b, kDur);
                const float tone = d.iTone >= 0 ? valor (d.iTone) / 10.0f : 0.5f;
                if (gSuau < 0.0f) gSuau = gIn;
                const float kS = coefSuau (fs, 15.0);
                float pk = 0.0f;
                for (int i = 0; i < n; ++i)
                {
                    const float x = R != nullptr ? 0.5f * (L[i] + R[i]) : L[i];
                    gSuau += (gIn - gSuau) * kS;
                    const float g = gSuau;
                    float u = midPre (tight (hpPre (x)));
                    float y = os.passa (u, [&] (float s) noexcept
                    {
                        s = lpPre4 (s * g);
                        switch (d.clip)
                        {
                            case 1: return corba (s + b, 0.1f) - cB;
                            case 4: { const float a = entre4 (corba (s + b, kDur) - cB); return corba (a * g2, kDur); }
                            case 5: { const float a = entre4 (corba (s + b, 0.15f) - cB); return corba (a * g2 * 0.8f + b * 0.5f, 0.25f) - corba (b * 0.5f, 0.25f); }
                            default: return corba (s + b, kDur) - cB;
                        }
                    });
                    y = dc (y);
                    if (d.net > 0.0f) y = 0.7f * y + d.net * x * (d.clip == 3 ? (1.2f - gain) : 1.0f);
                    if (d.to == 2) y = t1 (y) * (1.0f - tone) + t2 (y) * tone * 1.2f;
                    else y = t4 (t3 (t2 (t1 (y))));
                    y = post (y) * gOut;
                    pk = std::max (pk, std::abs (y));
                    L[i] = y;
                    if (R != nullptr) R[i] = y;
                }
                os.antiDenormal();
                act.store (lim (pk, 0.0f, 1.0f), std::memory_order_relaxed);
            }
        private:
            Sobre4 os;
            Bq hpPre, midPre, lpPre4, entre4, dc, t1, t2, t3, t4, post, tight;
            std::array<float, maxControls> ultim {};
            float gSuau = -1.0f;
        };

        // ---------------------------------------------------------------------------- Compressor
        class PedalComp : public Pedal
        {
        public:
            using Pedal::Pedal;
        protected:
            void preparaMotor (double, int) override { ultim.fill (-999.0f); buida(); }
            void buida() noexcept override { env = 0.0f; gr = 1.0f; to.neteja(); to2.neteja(); }
            void processa (float* L, float* R, int n) noexcept override
            {
                const int v = model().variant;
                float thr = -30, ratio = 4, atk = 5, rel = 250, make = 0, sec = 0, tone = 0, entrada = 0;
                switch (v)
                {
                    case 0: thr = -12 - valor (1) * 3.4f; ratio = 8; atk = 4; rel = 300; make = (valor (0) - 5) * 2.4f; tone = -2.0f; break;
                    case 1: thr = -10 - valor (3) * 3.0f; ratio = 6; atk = 1 + valor (2) * 3.0f; rel = 220; make = (valor (0) - 5) * 2.4f; tone = (valor (1) - 5) * 1.2f; break;
                    case 2: thr = -10 - valor (0) * 3.0f; ratio = 4; atk = 10; rel = 260; make = (valor (1) - 5) * 2.4f; sec = 1.0f - valor (2) / 10.0f; tone = (valor (3) - 5) * 1.2f; break;
                    default:
                    {
                        static const float rs[4] = { 4, 8, 12, 20 };
                        entrada = (valor (0) - 5) * 3.0f;
                        thr = -24; ratio = rs[std::clamp ((int) valor (4), 0, 3)];
                        atk = 0.05f * std::pow (40.0f, valor (2) / 10.0f);
                        rel = 50.0f * std::pow (22.0f, valor (3) / 10.0f);
                        make = (valor (1) - 5) * 2.4f;
                        sec = valor (5) / 10.0f;
                        break;
                    }
                }
                if (tone != ultimTo) { ultimTo = tone; prestatge (to, 3000.0, tone, fs, true); prestatge (to2, 3000.0, tone, fs, true); }
                const float aA = coefSuau (fs, atk), aR = coefSuau (fs, rel);
                const float autoMake = -thr * (1.0f - 1.0f / ratio) * 0.45f;
                const float gMake = dbLin (make + autoMake), gIn = dbLin (entrada);
                float minG = 1.0f;
                for (int i = 0; i < n; ++i)
                {
                    const float xl = L[i] * gIn, xr = R != nullptr ? R[i] * gIn : xl;
                    const float niv = std::max (std::abs (xl), std::abs (xr));
                    env += (niv - env) * (niv > env ? aA : aR);
                    const float eDb = 20.0f * std::log10 (env + 1e-9f);
                    float over = eDb - thr, red = 0.0f;
                    if (over > -3.0f) red = over < 3.0f ? (1.0f - 1.0f / ratio) * (over + 3.0f) * (over + 3.0f) / 12.0f : (1.0f - 1.0f / ratio) * over;
                    const float g = dbLin (-red);
                    minG = std::min (minG, g);
                    const float yl = to (xl * g * gMake), yr = R != nullptr ? to2 (xr * g * gMake) : yl;
                    L[i] = yl * (1.0f - sec) + xl * sec;
                    if (R != nullptr) R[i] = yr * (1.0f - sec) + xr * sec;
                }
                act.store (lim (1.0f - minG, 0.0f, 1.0f) * 2.0f, std::memory_order_relaxed);
            }
        private:
            float env = 0.0f, gr = 1.0f, ultimTo = -99.0f;
            Bq to, to2;
            std::array<float, maxControls> ultim {};
        };

        // ---------------------------------------------------------------------------- Chorus i Flanger (línies modulades)
        class PedalChorus : public Pedal
        {
        public:
            using Pedal::Pedal;
        protected:
            void preparaMotor (double f, int) override { lin.prepara ((int) (f * 0.03)); lp (lpW, 5500, 0.7, f); buida(); }
            void buida() noexcept override { lin.neteja(); lpW.neteja(); fb = 0.0f; }
            void processa (float* L, float* R, int n) noexcept override
            {
                const bool flanger = model().motor == MFlanger;
                float rate, centre, swing, res = 0.0f, mixW = 0.5f;
                if (flanger)
                {
                    centre = 0.0006f + 0.0045f * valor (0) / 10.0f;
                    swing = centre * 0.9f * valor (1) / 10.0f;
                    rate = 0.05f * std::pow (120.0f, valor (2) / 10.0f);
                    res = 0.85f * valor (3) / 10.0f;
                }
                else
                {
                    rate = 0.1f * std::pow (60.0f, valor (0) / 10.0f);
                    const bool clone = model().variant == 1;
                    centre = clone ? 0.008f : 0.0065f;
                    swing = clone ? (valor (1) > 0.5f ? 0.0055f : 0.0028f) : 0.0045f * valor (1) / 10.0f;
                }
                const double inc = rate / fs;
                for (int i = 0; i < n; ++i)
                {
                    const float x = R != nullptr ? 0.5f * (L[i] + R[i]) : L[i];
                    fase += inc; if (fase >= 1.0) fase -= 1.0;
                    const float tri = (float) (fase < 0.5 ? 4.0 * fase - 1.0 : 3.0 - 4.0 * fase);
                    const float d = (float) fs * (centre + swing * 0.5f * tri);
                    lin.escriu (x + fb * res);
                    float w = lin.llegeix (std::max (1.0f, d));
                    if (! flanger) w = lpW (w);
                    fb = w;
                    if (R != nullptr) { L[i] = (L[i] + w) * 0.72f; R[i] = (R[i] - w) * 0.72f; }
                    else L[i] = (x + w) * mixW * 1.4f;
                }
                act.store (0.5f + 0.5f * (float) std::sin (2 * pi * fase), std::memory_order_relaxed);
            }
        private:
            Linia lin;
            Bq lpW;
            double fase = 0.0;
            float fb = 0.0f;
        };

        // ---------------------------------------------------------------------------- Phaser i Uni-Vibe
        class PedalPhaser : public Pedal
        {
        public:
            using Pedal::Pedal;
        protected:
            void preparaMotor (double, int) override { buida(); }
            void buida() noexcept override { std::fill (std::begin (z), std::end (z), 0.0f); fb = 0.0f; }
            void processa (float* L, float* R, int n) noexcept override
            {
                const int v = model().variant;
                float rate, fMin, fMax, fbk = 0.0f, mixW = 0.5f, vol = 1.0f;
                bool vibe = v == 2;
                if (v == 0) { rate = 0.08f * std::pow (100.0f, valor (0) / 10.0f); fMin = 220; fMax = 1800; fbk = 0.12f; }
                else if (v == 1) { rate = 0.06f * std::pow (120.0f, valor (0) / 10.0f); fMin = 120; fMax = 2600; fbk = valor (1) > 0.5f ? 0.55f : 0.0f; }
                else
                {
                    rate = 0.5f * std::pow (20.0f, valor (0) / 10.0f);
                    const float ints = valor (1) / 10.0f;
                    fMin = 120; fMax = 120.0f + 1100.0f * ints;
                    mixW = valor (2) > 0.5f ? 1.0f : 0.5f;
                    vol = dbLin ((valor (3) - 5.0f) * 1.5f);
                }
                const double inc = rate / fs;
                static const float esgl[4] = { 1.0f, 1.9f, 3.4f, 6.2f };
                for (int i = 0; i < n; ++i)
                {
                    const float x = R != nullptr ? 0.5f * (L[i] + R[i]) : L[i];
                    fase += inc; if (fase >= 1.0) fase -= 1.0;
                    if ((comptador++ & 15) == 0)
                    {
                        float lfo = (float) (0.5 - 0.5 * std::cos (2 * pi * fase));
                        if (vibe) lfo = std::pow (lfo, 1.8f);   // la bombeta s'encén ràpid i s'apaga a poc a poc
                        const float f0 = fMin * std::pow (fMax / fMin, lfo);
                        for (int s = 0; s < 4; ++s)
                        {
                            const float fs1 = std::min ((float) fs * 0.45f, f0 * (vibe ? esgl[s] : 1.0f));
                            const float t = std::tan ((float) pi * fs1 / (float) fs);
                            coef[s] = (t - 1.0f) / (t + 1.0f);
                        }
                    }
                    float u = x + fb * fbk;
                    for (int s = 0; s < 4; ++s)
                    {
                        const float y = coef[s] * u + z[s];
                        z[s] = u - coef[s] * y;
                        u = y;
                    }
                    fb = u;
                    const float out = (x * (1.0f - mixW) + u * mixW) * (mixW < 1.0f ? 1.35f : 1.0f) * vol;
                    if (R != nullptr) { L[i] = out; R[i] = (R[i] * (1.0f - mixW) + u * mixW) * (mixW < 1.0f ? 1.35f : 1.0f) * vol; }
                    else L[i] = out;
                }
                act.store ((float) (0.5 - 0.5 * std::cos (2 * pi * fase)), std::memory_order_relaxed);
            }
        private:
            double fase = 0.0;
            unsigned comptador = 0;
            float z[4] {}, coef[4] {}, fb = 0.0f;
        };

        // ---------------------------------------------------------------------------- Trèmolo
        class PedalTremolo : public Pedal
        {
        public:
            using Pedal::Pedal;
        protected:
            void preparaMotor (double, int) override { buida(); }
            void processa (float* L, float* R, int n) noexcept override
            {
                const float rate = 0.8f * std::pow (15.0f, valor (0) / 10.0f);
                const float forma = valor (1) / 10.0f, depth = valor (2) / 10.0f;
                const double inc = rate / fs;
                const float kS = coefSuau (fs, 2.0);
                for (int i = 0; i < n; ++i)
                {
                    fase += inc; if (fase >= 1.0) fase -= 1.0;
                    const float tri = (float) (fase < 0.5 ? 4.0 * fase - 1.0 : 3.0 - 4.0 * fase);
                    const float sh = tanhR (tri * (1.0f + forma * 8.0f)) / tanhR (1.0f + forma * 8.0f);
                    const float obj = 1.0f - depth * (0.5f + 0.5f * sh);
                    g += (obj - g) * kS;
                    L[i] *= g;
                    if (R != nullptr) R[i] *= g;
                }
                act.store (g, std::memory_order_relaxed);
            }
        private:
            double fase = 0.0;
            float g = 1.0f;
        };

        // ---------------------------------------------------------------------------- base dels que en porten un de Mibase a dins
        struct Estereo
        {
            std::vector<float> r;
            void prepara (int n) { r.assign ((size_t) std::max (1, n), 0.0f); }
        };

        // ---------------------------------------------------------------------------- Delay (fa servir el Delay de Mibase)
        class PedalDelay : public Pedal
        {
        public:
            using Pedal::Pedal;
            void posaTempo (const std::atomic<double>* t) override { dl.posaTempo (t); }
        protected:
            void preparaMotor (double f, int maxBloc) override
            {
                ultim.fill (-999.0f);
                es.prepara (maxBloc);
                configura (false);
                dl.prepara (f, maxBloc);
            }
            void buida() noexcept override { dl.buida(); }
            void configura (bool estereo) noexcept
            {
                using namespace mibase::delay;
                const int v = model().variant;
                auto P = [this] (int p, float x) { dl.posa (p, x); };
                P (Freeze, 0); P (DuckAmt, 0); P (Difusio, 0); P (Desfas, 0); P (delay::Sortida, 0); P (Amplada, 100);
                P (EncaminamentP, estereo ? (int) delay::Estereo : (int) delay::Mono);
                P (Sync, 0); P (LowCut, 60); P (HighCut, 12000); P (Drive, 0); P (ModDepth, 0); P (ModRate, 0.7f);
                switch (v)
                {
                    case 0: P (CaracterP, Digital); P (Mix, valor (0) * 5.0f); P (Feedback, valor (1) * 9.0f); P (Temps, valor (2)); break;
                    case 1:
                    {
                        const int m = (int) valor (3);
                        P (CaracterP, m == 1 ? Analogic : Digital); P (Mix, valor (0) * 5.0f); P (Feedback, valor (1) * 9.0f); P (Temps, valor (2));
                        if (m == 1) { P (HighCut, 3800); P (Drive, 20); }
                        if (m == 2) { P (ModDepth, 30); P (ModRate, 0.9f); }
                        break;
                    }
                    case 2:
                        P (CaracterP, Analogic); P (delay::Sortida, (valor (0) - 5.0f) * 1.5f); P (Mix, valor (1) * 5.0f); P (Feedback, valor (2) * 10.5f);
                        P (Temps, valor (3)); P (HighCut, 2800); P (Drive, 25); P (ModDepth, valor (5) * 7.0f); P (ModRate, valor (4) > 0.5f ? 3.5f : 0.8f);
                        break;
                    case 3:
                        P (CaracterP, Analogic); P (Feedback, valor (0) * 10.0f); P (Mix, valor (1) * 5.0f); P (Temps, valor (2)); P (HighCut, 2500); P (Drive, 15);
                        if (valor (3) > 0.5f) { P (ModDepth, 25); P (ModRate, 1.2f); }
                        break;
                    default:
                    {
                        static const int car[5] = { Digital, Cinta, Analogic, Digital, LoFi };
                        const int t = std::clamp ((int) valor (0), 0, 4);
                        P (CaracterP, car[t]);
                        if (t == 3 && estereo) P (EncaminamentP, PingPong);
                        P (Temps, valor (1)); P (Feedback, valor (2) * 9.5f); P (Mix, valor (3) * 5.0f);
                        P (HighCut, 1500.0f * std::pow (12.0f, valor (4) / 10.0f)); P (Drive, valor (5) * 10.0f); P (ModDepth, valor (6) * 6.0f);
                        static const int notes[6] = { -1, 8, 7, 5, 6, 2 };
                        const int k = std::clamp ((int) valor (7), 0, 5);
                        if (k > 0) { P (Sync, 1); P (Nota, (float) notes[k]); }
                        break;
                    }
                }
            }
            void processa (float* L, float* R, int n) noexcept override
            {
                const bool estereo = R != nullptr;
                bool canvi = estereo != eraEstereo;
                for (int c = 0; c < numControls(); ++c) if (valor (c) != ultim[(size_t) c]) { ultim[(size_t) c] = valor (c); canvi = true; }
                if (canvi) { eraEstereo = estereo; configura (estereo); }
                if (estereo) dl.processa (L, R, n);
                else
                {
                    n = std::min (n, (int) es.r.size());
                    std::copy (L, L + n, es.r.data());
                    dl.processa (L, es.r.data(), n);
                    for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + es.r[(size_t) i]);
                }
            }
        private:
            delay::Delay dl;
            Estereo es;
            std::array<float, maxControls> ultim {};
            bool eraEstereo = false;
        };

        // ---------------------------------------------------------------------------- Reverb (fa servir la Reverb de Mibase)
        class PedalReverb : public Pedal
        {
        public:
            using Pedal::Pedal;
        protected:
            void preparaMotor (double f, int maxBloc) override
            {
                ultim.fill (-999.0f);
                es.prepara (maxBloc);
                configura();
                rv.prepara (f, maxBloc);
            }
            void buida() noexcept override { rv.buida(); }
            void configura() noexcept
            {
                using namespace mibase::reverb;
                auto P = [this] (int p, float x) { if (rv.valor (p) != x) rv.posa (p, x); };
                const int v = model().variant;
                P (ModDepth, 25); P (ModRate, 0.6f); P (DuckOn, 0); P (ShimOn, 0); P (FreezeOn, 0); P (GateOn, 0); P (PreSync, 0);
                P (LowCut, 90); P (Width, 100); P (Diffusion, 75); P (Size, 60); P (PreDelay, 10);
                switch (v)
                {
                    case 0:
                    {
                        static const int alg[7] = { reverb::Room, Hall, Plate, Spring, Hall, Shimmer, Hall };
                        const int m = std::clamp ((int) valor (3), 0, 6);
                        P (Alg, (float) alg[m]); P (Mix, valor (0) * 6.0f);
                        P (Decay, 0.3f * std::pow (30.0f, valor (2) / 10.0f));
                        P (HighCut, 2000.0f * std::pow (8.0f, valor (1) / 10.0f));
                        if (m == 4) { P (ModDepth, 80); P (ModRate, 0.9f); }
                        if (m == 6) { P (DuckOn, 1); P (DuckAmt, 12); }
                        break;
                    }
                    case 1:
                    {
                        const int m = std::clamp ((int) valor (1), 0, 2);
                        P (Alg, (float) (m == 0 ? Spring : Hall)); P (Mix, valor (0) * 6.0f); P (Decay, m == 0 ? 2.2f : 2.8f); P (HighCut, 9000);
                        if (m == 2) { P (ModDepth, 100); P (ModRate, 0.45f); }
                        break;
                    }
                    case 2:
                    {
                        static const int alg[7] = { reverb::Room, Hall, Plate, Spring, Ambient, Ambient, Shimmer };
                        const int m = std::clamp ((int) valor (0), 0, 6);
                        P (Alg, (float) alg[m]);
                        P (Decay, 0.4f * std::pow (50.0f, valor (1) / 10.0f) * (m == 5 ? 1.8f : 1.0f));
                        P (PreDelay, valor (2)); P (Mix, valor (3) * 7.0f);
                        P (HighCut, 2000.0f * std::pow (8.0f, valor (4) / 10.0f)); P (ModDepth, valor (5) * 10.0f);
                        if (m == 5) { P (Diffusion, 100); P (Size, 100); }
                        if (m == 4) P (Size, 90);
                        break;
                    }
                    case 3:
                        P (Alg, Spring); P (Decay, 1.4f + valor (0) * 0.3f); P (Mix, valor (1) * 7.0f); P (HighCut, 2500.0f * std::pow (4.0f, valor (2) / 10.0f));
                        break;
                    default:
                        P (Alg, Plate); P (Decay, 0.5f * std::pow (12.0f, valor (0) / 10.0f)); P (PreDelay, valor (1)); P (Mix, valor (2) * 6.0f); P (Damping, valor (3) * 10.0f);
                        break;
                }
            }
            void processa (float* L, float* R, int n) noexcept override
            {
                bool canvi = false;
                for (int c = 0; c < numControls(); ++c) if (valor (c) != ultim[(size_t) c]) { ultim[(size_t) c] = valor (c); canvi = true; }
                if (canvi) configura();
                if (R != nullptr) rv.processa (L, R, n);
                else
                {
                    n = std::min (n, (int) es.r.size());
                    std::copy (L, L + n, es.r.data());
                    rv.processa (L, es.r.data(), n);
                    for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + es.r[(size_t) i]);
                }
            }
        private:
            reverb::Reverb rv;
            Estereo es;
            std::array<float, maxControls> ultim {};
        };

        // ---------------------------------------------------------------------------- Eco de cinta
        class PedalCinta : public Pedal
        {
        public:
            using Pedal::Pedal;
        protected:
            void preparaMotor (double f, int maxBloc) override
            {
                lin.prepara ((int) (f * 1.6));
                hpF.neteja(); lpF.neteja();
                hp (hpF, 120, 0.7, f); lp (lpF, 4200, 0.7, f);
                es.prepara (maxBloc);
                wl.assign ((size_t) std::max (1, maxBloc), 0.0f);
                bl.assign ((size_t) std::max (1, maxBloc), 0.0f);
                rv.posa (reverb::Alg, reverb::Spring); rv.posa (reverb::Mix, 100); rv.posa (reverb::Decay, 1.8f); rv.posa (reverb::PreDelay, 0);
                rv.prepara (f, maxBloc);
                ultBass = ultTreble = -99.0f;
                buida();
            }
            void buida() noexcept override { lin.neteja(); fb = 0.0f; rv.buida(); hpF.neteja(); lpF.neteja(); sb.neteja(); st.neteja(); }
            void processa (float* L, float* R, int n) noexcept override
            {
                static const bool caps[7][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 0, 1, 1 }, { 1, 1, 0 }, { 1, 0, 1 }, { 1, 1, 1 } };
                const float d1 = (float) fs * (0.06f + (10.0f - valor (0)) * 0.025f);
                const float inten = valor (1) / 10.0f * 1.15f, eco = valor (2) / 10.0f * 1.2f;
                const int m = std::clamp ((int) valor (3), 0, 6);
                if (valor (4) != ultBass) { ultBass = valor (4); prestatge (sb, 200, (ultBass - 5.0f) * 1.6f, fs, false); }
                if (valor (5) != ultTreble) { ultTreble = valor (5); prestatge (st, 2500, (ultTreble - 5.0f) * 1.6f, fs, true); }
                const float rvG = valor (6) / 10.0f * 0.5f;
                const float actius = (float) (caps[m][0] + caps[m][1] + caps[m][2]);
                n = std::min (n, (int) es.r.size());
                for (int i = 0; i < n; ++i)
                {
                    const float x = R != nullptr ? 0.5f * (L[i] + R[i]) : L[i];
                    fw += 0.55 / fs; if (fw >= 1.0) fw -= 1.0;
                    ff += 6.5 / fs; if (ff >= 1.0) ff -= 1.0;
                    const float mod = 1.0f + 0.0025f * (float) std::sin (2 * pi * fw) + 0.0007f * (float) std::sin (2 * pi * ff);
                    float s = 0.0f;
                    const float rel[3] = { 1.0f, 1.9f, 2.8f };
                    for (int h = 0; h < 3; ++h) if (caps[m][h]) s += lin.llegeix (std::max (2.0f, d1 * rel[h] * mod));
                    s /= std::sqrt (actius);
                    fb = tanhR (lpF (hpF (s)) * inten);
                    lin.escriu (tanhR (x * 0.9f + fb));
                    const float w = st (sb (s)) * eco;
                    wl[(size_t) i] = x + w;
                }
                const bool ambRv = rvG > 0.001f && (int) bl.size() >= n;
                if (ambRv)
                {
                    for (int i = 0; i < n; ++i) { bl[(size_t) i] = wl[(size_t) i]; es.r[(size_t) i] = wl[(size_t) i]; }
                    rv.processa (bl.data(), es.r.data(), n);
                }
                for (int i = 0; i < n; ++i)
                {
                    const float rl = ambRv ? bl[(size_t) i] * rvG : 0.0f, rr = ambRv ? es.r[(size_t) i] * rvG : 0.0f;
                    if (R != nullptr) { L[i] = wl[(size_t) i] + rl; R[i] = wl[(size_t) i] + rr; }
                    else L[i] = wl[(size_t) i] + 0.5f * (rl + rr);
                }
                act.store (lim (std::abs (fb), 0.0f, 1.0f), std::memory_order_relaxed);
            }
        private:
            Linia lin;
            Bq hpF, lpF, sb, st;
            reverb::Reverb rv;
            Estereo es;
            std::vector<float> wl, bl;
            float fb = 0.0f, ultBass = -99.0f, ultTreble = -99.0f;
            double fw = 0.0, ff = 0.0;
        };

        // ---------------------------------------------------------------------------- Wah i filtres
        class PedalWah : public Pedal
        {
        public:
            using Pedal::Pedal;
        protected:
            void preparaMotor (double, int) override { buida(); }
            void buida() noexcept override { ic1 = ic2 = ic1b = ic2b = 0.0f; env = 0.0f; pS = -1.0f; }
            void processa (float* L, float* R, int n) noexcept override
            {
                const int v = model().variant;
                float fMin = 350, fMax = 2200, q = 4.0f, sens = 0.0f, pos = 0.5f;
                int modeEnv = 0, sortida = 1;   // sortida: 0 LP, 1 BP, 2 HP
                bool avall = false;
                float rate = 1.0f;
                if (v == 0) pos = valor (0) / 100.0f;
                else if (v == 1) { fMin = 450; fMax = 2000; q = 3.2f; pos = valor (0) / 100.0f; }
                else if (v == 2)
                {
                    sens = valor (0) / 10.0f; fMin = 300; fMax = 300.0f + 300.0f * std::pow (10.0f, valor (1) / 10.0f);
                    q = 1.5f + valor (2) * 0.8f; modeEnv = valor (3) > 0.5f ? 2 : 1; rate = 0.2f * std::pow (40.0f, valor (4) / 10.0f);
                }
                else
                {
                    sens = valor (0) / 10.0f; q = 0.8f + valor (1) * 1.1f; modeEnv = 1;
                    if (valor (2) > 0.5f) { fMin = 400; fMax = 3000; } else { fMin = 200; fMax = 1200; }
                    sortida = std::clamp ((int) valor (3), 0, 2); avall = valor (4) > 0.5f;
                }
                const float aA = coefSuau (fs, 4.0), aR = coefSuau (fs, 140.0), kP = coefSuau (fs, 12.0);
                if (pS < 0.0f) pS = pos;
                const float k = 1.0f / q;
                for (int i = 0; i < n; ++i)
                {
                    const float xl = L[i], xr = R != nullptr ? R[i] : xl;
                    float obj = pos;
                    if (modeEnv == 1)
                    {
                        const float a = std::abs (0.5f * (xl + xr));
                        env += (a - env) * (a > env ? aA : aR);
                        obj = lim (env * (2.0f + sens * 30.0f), 0.0f, 1.0f);
                        if (avall) obj = 1.0f - obj;
                    }
                    else if (modeEnv == 2)
                    {
                        lfo += rate / fs; if (lfo >= 1.0) lfo -= 1.0;
                        obj = (float) (0.5 - 0.5 * std::cos (2 * pi * lfo)) * (0.3f + 0.7f * sens);
                    }
                    pS += (obj - pS) * kP;
                    if ((cnt++ & 7) == 0)
                    {
                        const float fc = std::min ((float) fs * 0.45f, fMin * std::pow (fMax / fMin, pS));
                        g = std::tan ((float) pi * fc / (float) fs);
                        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
                    }
                    auto svf = [&] (float x, float& c1, float& c2) noexcept
                    {
                        const float v3 = x - c2, v1 = a1 * c1 + a2 * v3, v2 = c2 + a2 * c1 + a3 * v3;
                        c1 = 2.0f * v1 - c1; c2 = 2.0f * v2 - c2;
                        if (v < 2) return v1 * k * 2.4f + x * 0.04f;
                        if (sortida == 0) return v2 * 1.2f;
                        if (sortida == 2) return (x - k * v1 - v2) * 1.2f;
                        return v1 * k * 2.0f;
                    };
                    L[i] = svf (xl, ic1, ic2);
                    if (R != nullptr) R[i] = svf (xr, ic1b, ic2b);
                }
                act.store (pS, std::memory_order_relaxed);
            }
        private:
            float ic1 = 0, ic2 = 0, ic1b = 0, ic2b = 0, env = 0, pS = -1.0f, g = 0.1f, a1 = 1, a2 = 0, a3 = 0;
            double lfo = 0.0;
            unsigned cnt = 0;
        };

        // ---------------------------------------------------------------------------- Octavadors i pitch
        class PedalPitch : public Pedal
        {
        public:
            using Pedal::Pedal;
        protected:
            void preparaMotor (double f, int) override
            {
                const int v = model().variant;
                for (auto& d : des) d.prepara (f, v == 1 ? 45.0 : 32.0);
                lp (lpT1, 250, 0.7, f); lp (lpT2, 250, 0.7, f); lp1 (lpS, 900, f); lp1 (lpS2, 600, f); hp1 (dcS, 20, f);
                buida();
            }
            void buida() noexcept override
            {
                for (auto& d : des) d.neteja();
                for (Bq* b : { &lpT1, &lpT2, &lpS, &lpS2, &dcS }) b->neteja();
                env = 0.0f; q1 = q2 = 1.0f; alt = false; ratioS = -1.0f;
            }
            void processa (float* L, float* R, int n) noexcept override
            {
                const int v = model().variant;
                const float aE = coefSuau (fs, 3.0), rE = coefSuau (fs, 60.0), kR = coefSuau (fs, 20.0);
                for (int i = 0; i < n; ++i)
                {
                    const float x = R != nullptr ? 0.5f * (L[i] + R[i]) : L[i];
                    float y;
                    if (v == 0)
                    {
                        const float t = lpT2 (lpT1 (x));
                        const float a = std::abs (t);
                        env += (a - env) * (a > env ? aE : rE);
                        const float h = env * 0.25f;
                        if (! alt && t > h) { alt = true; q1 = -q1; if (q1 > 0.0f) q2 = -q2; }
                        else if (alt && t < -h) alt = false;
                        const float e = env * 3.0f;
                        const float s1 = lpS (q1 * e), s2 = lpS2 (q2 * e);
                        y = dcS (x * valor (2) / 10.0f + s1 * valor (0) / 10.0f * 1.4f + s2 * valor (1) / 10.0f * 1.4f);
                    }
                    else if (v == 1)
                    {
                        y = x * valor (0) / 10.0f;
                        if (valor (1) > 0.01f) y += des[0].passa (x, 0.5f) * valor (1) / 10.0f; else des[0].l.escriu (x);
                        if (valor (2) > 0.01f) y += des[1].passa (x, 2.0f) * valor (2) / 10.0f; else des[1].l.escriu (x);
                        if (valor (3) > 0.01f) y += des[2].passa (x, 4.0f) * valor (3) / 10.0f * 0.8f; else des[2].l.escriu (x);
                    }
                    else
                    {
                        const float p = valor (0) / 100.0f;
                        const int m = std::clamp ((int) valor (1), 0, 6);
                        static const float semis[7] = { 24, 12, 7, -12, -24, 12, 0.25f };
                        const float obj = std::pow (2.0f, semis[m] * (m == 6 ? -1.0f : 1.0f) * p / 12.0f);
                        if (ratioS < 0.0f) ratioS = obj;
                        ratioS += (obj - ratioS) * kR;
                        const float s = des[0].passa (x, ratioS);
                        y = m >= 5 ? (x + s) * 0.7f : s;
                    }
                    L[i] = y;
                    if (R != nullptr) R[i] = y;
                }
                act.store (lim (env * 4.0f, 0.0f, 1.0f), std::memory_order_relaxed);
            }
        private:
            std::array<Desplacador, 3> des;
            Bq lpT1, lpT2, lpS, lpS2, dcS;
            float env = 0.0f, q1 = 1.0f, q2 = 1.0f, ratioS = -1.0f;
            bool alt = false;
        };

        // ---------------------------------------------------------------------------- Utilitats: noise gate, EQ gràfic, volum
        class PedalUtil : public Pedal
        {
        public:
            using Pedal::Pedal;
        protected:
            void preparaMotor (double, int) override { ultim.fill (-999.0f); buida(); }
            void buida() noexcept override { env = 0.0f; gGate = 1.0f; for (auto& b : eq) b.neteja(); for (auto& b : eqR) b.neteja(); }
            void processa (float* L, float* R, int n) noexcept override
            {
                const int v = model().variant;
                if (v == 0)
                {
                    const float thr = dbLin (valor (0)), thrT = thr * 0.5f;
                    const float aE = coefSuau (fs, 1.0), rE = coefSuau (fs, 30.0), kO = coefSuau (fs, 1.0), kT = coefSuau (fs, valor (1));
                    for (int i = 0; i < n; ++i)
                    {
                        const float a = R != nullptr ? std::max (std::abs (L[i]), std::abs (R[i])) : std::abs (L[i]);
                        env += (a - env) * (a > env ? aE : rE);
                        if (env > thr) obert = true; else if (env < thrT) obert = false;
                        gGate += ((obert ? 1.0f : 0.0f) - gGate) * (obert ? kO : kT);
                        L[i] *= gGate;
                        if (R != nullptr) R[i] *= gGate;
                    }
                    act.store (gGate, std::memory_order_relaxed);
                }
                else if (v == 1)
                {
                    static const float fr[7] = { 100, 200, 400, 800, 1600, 3200, 6400 };
                    bool canvi = false;
                    for (int c = 0; c < 8; ++c) if (valor (c) != ultim[(size_t) c]) { ultim[(size_t) c] = valor (c); canvi = true; }
                    if (canvi) for (int b = 0; b < 7; ++b) { pic (eq[(size_t) b], fr[b], 1.4, valor (b), fs); eqR[(size_t) b] = eq[(size_t) b]; eqR[(size_t) b].neteja(); }
                    const float g = dbLin (valor (7));
                    for (int i = 0; i < n; ++i)
                    {
                        float y = L[i];
                        for (auto& b : eq) y = b (y);
                        L[i] = y * g;
                        if (R != nullptr) { float z = R[i]; for (auto& b : eqR) z = b (z); R[i] = z * g; }
                    }
                }
                else
                {
                    const float p = valor (0) / 100.0f, mn = valor (1) / 10.0f;
                    const float obj = mn + (1.0f - mn) * p * p;
                    const float k = coefSuau (fs, 8.0);
                    for (int i = 0; i < n; ++i)
                    {
                        gGate += (obj - gGate) * k;
                        L[i] *= gGate;
                        if (R != nullptr) R[i] *= gGate;
                    }
                    act.store (gGate, std::memory_order_relaxed);
                }
            }
        private:
            float env = 0.0f, gGate = 1.0f;
            bool obert = true;
            std::array<Bq, 7> eq, eqR;
            std::array<float, maxControls> ultim {};
        };
    }

    std::shared_ptr<Pedal> creaPedal (const std::string& clau)
    {
        const int i = indexPedal (clau);
        if (i < 0) return nullptr;
        switch (pedals()[(size_t) i].motor)
        {
            case MDrive: return std::make_shared<PedalDrive> (i);
            case MComp: return std::make_shared<PedalComp> (i);
            case MChorus: case MFlanger: return std::make_shared<PedalChorus> (i);
            case MPhaser: return std::make_shared<PedalPhaser> (i);
            case MTremolo: return std::make_shared<PedalTremolo> (i);
            case MDelay: return std::make_shared<PedalDelay> (i);
            case MCinta: return std::make_shared<PedalCinta> (i);
            case MReverb: return std::make_shared<PedalReverb> (i);
            case MWah: return std::make_shared<PedalWah> (i);
            case MPitch: return std::make_shared<PedalPitch> (i);
            default: return std::make_shared<PedalUtil> (i);
        }
    }
}

// ======================================================================================== ampli, pantalla i rack
namespace mibase::guitarrack
{
    namespace
    {
        constexpr float sortidaAmpli = 0.0f;   // dB comuns de l'ampli (els de cada canal són a Veu::nivell)

        // ---------------------------------------------------------------------------- l'amplificador
        struct Ampli
        {
            double fs = 48000.0;
            Sobre4 os;
            Bq hpIn, bright, midV, preHp, brillant;
            std::array<Bq, 4> hpE, lpE;
            PilaTons pila;
            Components comp {};
            double norma = 1.0;
            Bq pres4, res4;
            Bq hpOut, lpOut, lpOut2, cut, gvMid, gvLow, gvHigh, toneShift;
            float env = 0.0f, sagA = 0.0f, sagR = 0.0f;
            std::array<float, numAmpCtl> k {};
            bool primer = true;
            int model = -1, canal = 0, inter = 0;
            reverb::Reverb rv;
            std::vector<float> rvL, rvR;
            float ultRv = -1.0f;
            // fil d'àudio (suavitzats)
            float gPreS = 1.0f, gPowS = 1.0f, lvlS = 1.0f;

            const ModelAmp& m() const { return amplis()[(size_t) std::clamp (model, 0, (int) amplis().size() - 1)]; }
            const Veu& veu() const { const auto& a = m(); return a.veus[(size_t) std::clamp (canal, 0, (int) a.veus.size() - 1)]; }

            void prepara (double f, int maxBloc)
            {
                fs = f;
                os.prepara (f);
                rvL.assign ((size_t) std::max (1, maxBloc), 0.0f);
                rvR.assign ((size_t) std::max (1, maxBloc), 0.0f);
                rv.posa (reverb::Alg, reverb::Spring); rv.posa (reverb::Mix, 100); rv.posa (reverb::Decay, 2.0f); rv.posa (reverb::PreDelay, 0);
                rv.posa (reverb::LowCut, 180); rv.posa (reverb::HighCut, 5500);
                rv.prepara (f, maxBloc);
                sagA = coefSuau (f * 4.0, 4.0); sagR = coefSuau (f * 4.0, 140.0);
                if (model >= 0) configura (model, canal, inter);
            }
            void neteja() noexcept
            {
                os.neteja();
                for (Bq* b : { &hpIn, &bright, &midV, &preHp, &brillant, &pres4, &res4, &hpOut, &lpOut, &lpOut2, &cut, &gvMid, &gvLow, &gvHigh, &toneShift }) b->neteja();
                for (auto& b : hpE) b.neteja();
                for (auto& b : lpE) b.neteja();
                pila.neteja();
                env = 0.0f;
                rv.buida();
            }
            /** Canvi d'estructura (model, canal, interruptor): fora de la fosa, amb el so apagat. */
            void configura (int mo, int ca, int in) noexcept
            {
                model = std::clamp (mo, 0, (int) amplis().size() - 1);
                canal = std::clamp (ca, 0, (int) m().veus.size() - 1);
                inter = in;
                const auto& a = m();
                const auto& u = veu();
                const double f4 = fs * 4.0;
                comp = components (a.pila);
                {
                    double bb[4], aa[4];
                    PilaTons::analog (comp, 0.5, 0.5, corbaGreus (0.5), bb, aa);
                    norma = 1.0 / std::max (1e-4, PilaTons::modul (bb, aa, 1000.0));
                }
                hp1 (hpIn, u.hpEntrada, fs);
                pic (midV, u.midF, 0.8, u.midDb, fs);
                const bool interOn = in != 0;
                bright.pla(); preHp.pla(); toneShift.pla();
                if (interOn)
                {
                    switch (a.interruptor)
                    {
                        case Interruptor::Bright: prestatge (bright, 2500.0, 5.0, fs, true); break;
                        case Interruptor::Boost: hp1 (preHp, 120.0, fs); break;
                        case Interruptor::Tight: hp1 (preHp, 150.0, fs); break;
                        case Interruptor::ToneShift: pic (toneShift, 650.0, 0.8, -8.0, fs); break;
                        default: break;
                    }
                }
                for (int s = 0; s < 4; ++s)
                {
                    hp1 (hpE[(size_t) s], s == 0 ? 20.0 : u.hpEtapa, f4);
                    lp1 (lpE[(size_t) s], u.lpEtapa, f4);
                }
                hp (hpOut, u.hpSortida, 0.7, fs);
                lp (lpOut, u.lpSortida, 0.7, fs);
                lp (lpOut2, std::min (14000.0, u.lpSortida * 1.6), 0.6, fs);
                primer = true;
                neteja();
            }
            float knob (int c) const noexcept { return k[(size_t) c]; }
            void recalcula() noexcept
            {
                const auto& a = m();
                const auto& u = veu();
                const double f4 = fs * 4.0;
                const double t = knob (ATreble) / 10.0, mm = a.etiqueta[AMiddle].empty() ? 0.5 : knob (AMiddle) / 10.0, l = corbaGreus (knob (ABass) / 10.0);
                pila.calcula (comp, t, mm, l, f4, norma);
                const bool esCut = a.etiqueta[APresence] == "Cut";
                if (! a.etiqueta[APresence].empty() && ! esCut) prestatge (pres4, 3500.0, (knob (APresence) - 5.0f) * 1.6f, f4, true); else pres4.pla();
                if (esCut) lp1 (cut, 12000.0 * std::pow (2500.0 / 12000.0, knob (APresence) / 10.0), fs); else cut.pla();
                if (a.extra == Extra::Resonance || a.extra == Extra::Depth) pic (res4, 90.0, 0.9, (knob (AExtra) - 5.0f) * 1.6f + 1.0f, f4); else res4.pla();
                if (a.extra == Extra::GraficV)
                {
                    const float v = knob (AExtra);
                    pic (gvMid, 750.0, 0.7, -v * 1.1f, fs);
                    prestatge (gvLow, 120.0, v * 0.6f, fs, false);
                    prestatge (gvHigh, 4000.0, v * 0.5f, fs, true);
                }
                else { gvMid.pla(); gvLow.pla(); gvHigh.pla(); }
                const float g01 = knob (AGain) / 10.0f;
                prestatge (brillant, 2000.0, u.brillant * (1.0f - 0.7f * g01), fs, true);
            }
            /** m: mono, al lloc. obj: els poms (0..10) que es volen. */
            void processa (float* x, int n, const std::array<float, numAmpCtl>& obj, float& pic) noexcept
            {
                const auto& a = m();
                const auto& u = veu();
                // poms suavitzats (≈ 30 ms)
                const float kk = std::min (1.0f, (float) n / (float) (0.03 * fs));
                bool canvi = primer;
                for (int c = 0; c < numAmpCtl; ++c)
                {
                    float& v = k[(size_t) c];
                    const float o = obj[(size_t) c];
                    if (primer) v = o;
                    else if (std::abs (o - v) > 1e-4f) { v += (o - v) * kk; if (std::abs (o - v) < 0.002f) v = o; canvi = true; }
                }
                if (canvi) recalcula();
                const float g01 = knob (AGain) / 10.0f;
                const float preDb = u.guanyMin + (u.guanyMax - u.guanyMin) * g01 + (inter != 0 && a.interruptor == Interruptor::Boost ? 8.0f : 0.0f);
                const float m01 = a.teMaster ? knob (AMaster) / 10.0f : g01;
                const float driveDb = u.potencia * m01;
                const float lvl = (a.teMaster ? dbLin (-24.0f * (1.0f - m01) * (1.0f - m01)) : dbLin (-8.0f * (1.0f - g01) * (1.0f - g01)))
                                * dbLin (-0.5f * driveDb + u.nivell + sortidaAmpli);
                const float gPre = dbLin (preDb), gPow = dbLin (driveDb) * 0.8f;
                if (primer) { gPreS = gPre; gPowS = gPow; lvlS = lvl; primer = false; }
                const float kS = coefSuau (fs, 20.0);
                const int et = std::clamp (u.etapes, 1, 4);
                const int posP = std::clamp (u.posPila, 0, et);
                const float k1 = std::clamp ((u.duresa - 1.0f) * 0.5f, 0.0f, 1.0f);
                const float b = u.biaix, sag = u.sag;
                // repartiment del guany entre etapes (dB → lineal, la primera s'endú la part grossa)
                float frac[4] = { 1, 0, 0, 0 };
                if (et > 1) { frac[0] = 0.45f; for (int s = 1; s < et; ++s) frac[s] = 0.55f / (float) (et - 1); }
                const float rvAmt = a.etiqueta[AReverb].empty() ? 0.0f : knob (AReverb) / 10.0f * 0.6f;
                for (int i = 0; i < n; ++i)
                {
                    gPreS += (gPre - gPreS) * kS; gPowS += (gPow - gPowS) * kS; lvlS += (lvl - lvlS) * kS;
                    float gs[4];
                    if ((i & 15) == 0)
                    {
                        const float preDbS = 20.0f * std::log10 (std::max (1e-6f, gPreS));
                        for (int s = 0; s < et; ++s) gEt[s] = dbLin (preDbS * frac[s]);
                    }
                    for (int s = 0; s < et; ++s) gs[s] = gEt[s];
                    float v = brillant (preHp (bright (midV (hpIn (x[i])))));
                    const float gp = gPowS;
                    v = os.passa (v, [&] (float s) noexcept
                    {
                        for (int e = 0; e < et; ++e)
                        {
                            if (e == posP) s = pila (s);
                            s = valvulaC (s * gs[e], b, k1);
                            s = lpE[(size_t) e] (hpE[(size_t) e] (s));
                        }
                        if (posP >= et) s = pila (s);
                        s = pres4 (res4 (s));
                        const float as = std::abs (s);
                        env += (as - env) * (as > env ? sagA : sagR);
                        return tanhR (s * gp / (1.0f + sag * 1.5f * env));
                    });
                    v = toneShift (gvHigh (gvLow (gvMid (cut (lpOut2 (lpOut (hpOut (v))))))));
                    x[i] = v * lvlS;
                }
                os.antiDenormal();
                if (rvAmt > 0.003f && (int) rvL.size() >= n)
                {
                    std::copy (x, x + n, rvL.data());
                    std::copy (x, x + n, rvR.data());
                    rv.processa (rvL.data(), rvR.data(), n);
                    for (int i = 0; i < n; ++i) x[i] += 0.5f * (rvL[(size_t) i] + rvR[(size_t) i]) * rvAmt;
                }
                for (int i = 0; i < n; ++i) pic = std::max (pic, std::abs (x[i]));
            }
            float gEt[4] { 1, 1, 1, 1 };
        };

        // ---------------------------------------------------------------------------- la pantalla i els micròfons
        struct Pantalla
        {
            double fs = 48000.0;
            struct Mic
            {
                std::array<Bq, 12> f;
                Linia dist;
                float pos = -1, dis = -1, ang = -1;
                int quin = -1;
                void neteja() { for (auto& b : f) b.neteja(); dist.neteja(); }
            };
            Mic A, B;
            int cab = 0, alt = 0;
            reverb::Reverb sala;
            std::vector<float> sL, sR;
            float ultSala = -1.0f;
            conv::Convolucio ir;   // 1.6

            void prepara (double f, int maxBloc)
            {
                fs = f;
                A.dist.prepara ((int) (f * 0.006)); B.dist.prepara ((int) (f * 0.006));
                sL.assign ((size_t) std::max (1, maxBloc), 0.0f);
                sR.assign ((size_t) std::max (1, maxBloc), 0.0f);
                using namespace mibase::reverb;
                sala.posa (Alg, reverb::Room); sala.posa (Mix, 100); sala.posa (Size, 25); sala.posa (PreDelay, 3); sala.posa (Width, 130);
                sala.posa (LowCut, 150); sala.posa (HighCut, 7000); sala.posa (Decay, 0.55f); sala.posa (Diffusion, 70);
                sala.prepara (f, maxBloc);
                conv::Opcions o;
                o.normalitza = true;
                ir.posaOpcions (o);
                ir.prepara (f);
                A.pos = B.pos = -1;
            }
            void configura (int c, int a, int ma, int mb) noexcept
            {
                cab = std::clamp (c, 0, (int) cabs().size() - 1);
                alt = std::clamp (a, 0, (int) altaveus().size() - 1);
                A.quin = std::clamp (ma, 0, (int) micros().size() - 1);
                B.quin = std::clamp (mb, 0, (int) micros().size() - 1);
                A.pos = B.pos = -1;   // es recalculen
                A.neteja(); B.neteja();
                sala.buida();
                ir.buida();
            }
            void recalcula (Mic& mc, float pos, float dis, float ang) noexcept
            {
                const auto& c = cabs()[(size_t) cab];
                const auto& s = altaveus()[(size_t) alt];
                const auto& m = micros()[(size_t) mc.quin];
                auto& f = mc.f;
                hp (f[0], c.hp, 0.7, fs);
                pic (f[1], c.resF, c.resQ, c.resDb * (1.0f - 0.4f * dis), fs);
                pic (f[2], 400.0, 0.7, c.cosDb, fs);
                pic (f[3], s.pic1F * (1.0f - 0.25f * pos), s.pic1Q, s.pic1Db * (1.0f - 0.5f * pos), fs);
                pic (f[4], s.pic2F, s.pic2Q, s.pic2Db, fs);
                prestatge (f[5], 150.0, s.greusDb + m.proximitat * (1.0f - dis) * (1.0f - dis) - 1.0f - (c.obert ? 1.5f : 0.0f), fs, false);
                lp (f[6], s.lp * (1.25f - 0.55f * pos), 0.9, fs);
                lp (f[7], std::min (16000.0f, s.lp * 1.6f * (1.2f - 0.5f * pos)), 0.6, fs);
                pic (f[8], m.presF, m.presQ, m.presDb * (1.0f - 0.3f * dis), fs);
                prestatge (f[9], 5000.0, m.agutsDb - ang / 45.0f * 7.0f - dis * 2.0f, fs, true);
                hp1 (f[10], m.hp, fs);
                lp (f[11], m.lp, 0.7, fs);
                mc.pos = pos; mc.dis = dis; mc.ang = ang;
            }
            inline float filtra (Mic& mc, float x) noexcept
            {
                for (auto& b : mc.f) x = b (x);
                return x;
            }
            void processa (const float* x, float* L, float* R, int n, float posA, float disA, float angA, float posB, float disB, float angB,
                           float mescla, float room, float mesclaIR) noexcept
            {
                const bool ambIR = mesclaIR > 0.0005f && ir.longitudActiva() > 0;
                const bool nomesIR = ambIR && mesclaIR >= 0.9995f;
                if (posA != A.pos || disA != A.dis || angA != A.ang) recalcula (A, posA, disA, angA);
                const bool ambB = mescla > 0.001f;
                if (ambB && (posB != B.pos || disB != B.dis || angB != B.ang)) recalcula (B, posB, disB, angB);
                const float dA = (float) fs * (0.0002f + disA * 0.0012f), dB = (float) fs * (0.0002f + disB * 0.0012f);
                const float rA = (float) fs * (0.0006f + disA * 0.003f), rB = (float) fs * (0.0006f + disB * 0.003f);
                const float gA = 1.0f - mescla * 0.4f, gB = mescla * 0.8f;
                for (int i = 0; i < n; ++i)
                {
                    float y = 0.0f;
                    if (! nomesIR)
                    {
                        float ya = filtra (A, x[i]);
                        A.dist.escriu (ya);
                        y = (A.dist.llegeix (dA) + 0.3f * disA * A.dist.llegeix (dA + rA)) * gA;
                        if (ambB)
                        {
                            const float yb = filtra (B, x[i]);
                            B.dist.escriu (yb);
                            y += (B.dist.llegeix (dB) + 0.3f * disB * B.dist.llegeix (dB + rB)) * gB;
                        }
                    }
                    if (ambIR)
                    {
                        float oL = 0.0f, oR = 0.0f;
                        ir.mostra (x[i], x[i], oL, oR);
                        L[i] = y * (1.0f - mesclaIR) + oL * mesclaIR * 2.0f;   // la IR ve normalitzada (energia 0,35): així sona com la simulació
                        R[i] = y * (1.0f - mesclaIR) + oR * mesclaIR * 2.0f;
                    }
                    else L[i] = R[i] = y;
                }
                for (auto& b : A.f) b.antiDenormal();
                if (room > 0.001f && (int) sL.size() >= n)
                {
                    const float dec = 0.4f + room * 0.9f;
                    if (std::abs (dec - ultSala) > 0.01f) { ultSala = dec; sala.posa (reverb::Decay, dec); }
                    std::copy (L, L + n, sL.data());
                    std::copy (R, R + n, sR.data());
                    sala.processa (sL.data(), sR.data(), n);
                    const float g = room * 0.55f;
                    for (int i = 0; i < n; ++i) { L[i] += sL[(size_t) i] * g; R[i] += sR[(size_t) i] * g; }
                }
            }
        };
    }

    // ==================================================================================== GuitarRack
    struct GuitarRack::Imp
    {
        Ampli amp;
        Pantalla pant;
        std::vector<float> m;
        struct Config
        {
            Placa* placa = nullptr;
            int amp = -1, canal = -1, inter = -1, cab = -1, alt = -1, micA = -1, micB = -1, ampOn = -1, cabOn = -1;
            bool operator== (const Config&) const = default;
        };
        Config aplicada;
        int estatFosa = 0;   // 0 normal, 1 baixant, 2 pujant
        float fosa = 1.0f;
        bool primer = true;
        float gIn = 1.0f, gOut = 1.0f, env = 0.0f, gGate = 1.0f;
        bool gateObert = true;
    };

    namespace
    {
        /** 1.6: els Guitar Rack vius, perquè els CC del teclat els arribin (fil del MIDI ↔ interfície; mai l'àudio). */
        std::mutex& mtxRegistre() { static std::mutex m; return m; }
        std::vector<GuitarRack*>& registre() { static std::vector<GuitarRack*> v; return v; }
    }

    void GuitarRack::ccPerATots (int numCC, int valor)
    {
        std::lock_guard<std::mutex> l (mtxRegistre());
        for (auto* r : registre()) r->ccMidi (numCC, valor);
    }

    bool GuitarRack::ccMidi (int numCC, int valor) noexcept
    {
        if (numCC < 0 || numCC > 127) return false;
        bool fet = false;
        if (const int a = aprenent.exchange (-1); a >= 0 && a < numParams)
        {
            for (auto& x : cc) if (x.load() == numCC) x.store (-1);   // un CC mou un sol paràmetre
            cc[(size_t) a].store (numCC);
            ++canvis;
            fet = true;
        }
        const float v01 = (float) std::clamp (valor, 0, 127) / 127.0f;
        for (int p = 0; p < numParams; ++p)
            if (cc[(size_t) p].load (std::memory_order_relaxed) == numCC) { posaParametre (p, v01); fet = true; }
        return fet;
    }

    bool GuitarRack::carregaIR (const std::string& referencia, std::string& error)
    {
        refIR = referencia;
        auto ir = conv::carrega (conv::resol (referencia), error);
        imp->pant.ir.posaIR (ir);
        if (ir != nullptr) { posa (IrOn, 1.0f); if (valor (IrMescla) < 1.0f) posa (IrMescla, 100.0f); }
        ++canvis;
        return ir != nullptr;
    }

    void GuitarRack::treuIR()
    {
        refIR.clear();
        imp->pant.ir.posaIR (nullptr);
        posa (IrOn, 0.0f);
        ++canvis;
    }

    std::string GuitarRack::nomIR() const
    {
        if (auto ir = imp->pant.ir.ir()) return ir->nom;
        if (refIR.empty()) return {};
        return std::filesystem::path (conv::resol (refIR)).stem().string();
    }

    bool GuitarRack::irFalta() const { return ! refIR.empty() && imp->pant.ir.ir() == nullptr; }

    GuitarRack::GuitarRack() : imp (std::make_unique<Imp>())
    {
        for (auto& x : cc) x.store (-1);
        {
            std::lock_guard<std::mutex> l (mtxRegistre());
            registre().push_back (this);
        }
        for (int p = 0; p < numFixos; ++p) fix[(size_t) p].store (infoFix (p).def);
        for (auto& s : slot) s.store (nullptr);
        ui = std::make_shared<Placa>();
        actual.store (ui.get());
        posaAmpli (std::max (0, indexAmpli ("plexi")));
    }

    GuitarRack::~GuitarRack()
    {
        std::lock_guard<std::mutex> l (mtxRegistre());
        auto& v = registre();
        v.erase (std::remove (v.begin(), v.end(), this), v.end());
    }

    void GuitarRack::posaTempo (const std::atomic<double>* t)
    {
        tempo = t;
        for (auto& p : ui->pedals) p->posaTempo (t);
    }

    void GuitarRack::posa (int p, float v) noexcept
    {
        if (p < 0 || p >= numFixos || ! std::isfinite (v)) return;
        const auto& i = infoFix (p);
        v = lim (v, i.mn, i.mx);
        if (i.passos > 1) v = std::round (v);
        fix[(size_t) p].store (v, std::memory_order_relaxed);
    }

    void GuitarRack::posaAmpli (int i, bool ambPoms)
    {
        i = std::clamp (i, 0, (int) amplis().size() - 1);
        iAmp.store (i);
        if (ambPoms)
        {
            const auto& a = amplis()[(size_t) i];
            for (int c = 0; c < numAmpCtl; ++c) posa (AmpGain + c, a.def[(size_t) c]);
            posa (Canal, (float) a.canalDef);
            posa (BrightP, 0.0f);
        }
        ++canvis;
    }

    void GuitarRack::posaCab (int i, bool ambAltaveu)
    {
        i = std::clamp (i, 0, (int) cabs().size() - 1);
        iCab.store (i);
        if (ambAltaveu) iAlt.store (std::max (0, indexAltaveu (cabs()[(size_t) i].altaveu)));
        ++canvis;
    }

    void GuitarRack::posaAltaveu (int i)
    {
        iAlt.store (std::clamp (i, 0, (int) altaveus().size() - 1));
        ++canvis;
    }

    namespace
    {
        /** Primer els d'abans de l'ampli, després els del loop (sense canviar l'ordre dins de cada grup). */
        void ordena (Placa& p)
        {
            std::vector<std::shared_ptr<Pedal>> a, b;
            for (auto& x : p.pedals) (x->loop.load() ? b : a).push_back (x);
            p.pedals = a;
            p.pedals.insert (p.pedals.end(), b.begin(), b.end());
            p.loop.clear();
            for (auto& x : p.pedals) p.loop.push_back (x->loop.load() ? 1 : 0);
        }
    }

    void GuitarRack::publica (std::shared_ptr<Placa> p)
    {
        ordena (*p);
        auto vell = ui;
        ui = p;
        for (int k = 0; k < maxPedals; ++k) slot[(size_t) k].store (k < (int) p->pedals.size() ? p->pedals[(size_t) k].get() : nullptr);
        actual.store (p.get());
        guardades.push_back ({ vell, epoca.load() });
        ++canvis;
        recull();
    }

    void GuitarRack::recull()
    {
        const auto ara = epoca.load();
        Placa* enUs = usA.load();
        Placa* act = actual.load();
        guardades.erase (std::remove_if (guardades.begin(), guardades.end(), [&] (const Guardada& g)
        {
            if (g.p.get() == enUs || g.p.get() == act) return false;
            return ! preparat || ara >= g.retirada + 2;
        }), guardades.end());
    }

    int GuitarRack::afegeix (const std::string& clau, int pos, bool aLoop)
    {
        if (numPedals() >= maxPedals) return -1;
        auto p = creaPedal (clau);
        if (p == nullptr) return -1;
        p->loop.store (aLoop);
        p->posaTempo (tempo);
        if (preparat) p->prepara (fs, maxBloc);
        auto c = copia();
        // la posició: dins del seu grup
        int nPre = 0;
        for (auto& x : c->pedals) if (! x->loop.load()) ++nPre;
        if (pos < 0) pos = aLoop ? (int) c->pedals.size() : nPre;
        pos = aLoop ? std::clamp (pos, nPre, (int) c->pedals.size()) : std::clamp (pos, 0, nPre);
        c->pedals.insert (c->pedals.begin() + pos, p);
        publica (c);
        for (int k = 0; k < numPedals(); ++k) if (ui->pedals[(size_t) k] == p) return k;
        return -1;
    }

    void GuitarRack::treu (int k)
    {
        if (k < 0 || k >= numPedals()) return;
        auto c = copia();
        c->pedals.erase (c->pedals.begin() + k);
        publica (c);
    }

    void GuitarRack::mou (int de, int a, bool aLoop)
    {
        if (de < 0 || de >= numPedals()) return;
        auto c = copia();
        auto p = c->pedals[(size_t) de];
        c->pedals.erase (c->pedals.begin() + de);
        p->loop.store (aLoop);
        a = std::clamp (a, 0, (int) c->pedals.size());
        c->pedals.insert (c->pedals.begin() + a, p);
        publica (c);
    }

    void GuitarRack::posaLoop (int k, bool aLoop)
    {
        if (k < 0 || k >= numPedals()) return;
        auto c = copia();
        c->pedals[(size_t) k]->loop.store (aLoop);
        publica (c);
    }

    void GuitarRack::buidaPlaca() { publica (std::make_shared<Placa>()); }

    void GuitarRack::prepara (double f, int mb)
    {
        fs = f;
        maxBloc = std::max (16, mb);
        auto& I = *imp;
        I.amp.prepara (f, maxBloc);
        I.pant.prepara (f, maxBloc);
        I.m.assign ((size_t) maxBloc, 0.0f);
        for (auto& p : ui->pedals) p->prepara (f, maxBloc);
        I.primer = true;
        preparat = true;
    }

    void GuitarRack::processa (float* L, float* R, int n) noexcept
    {
        if (! preparat) return;
#ifdef MIBASE_GR_FTZ
        const unsigned int csr = _mm_getcsr();
        _mm_setcsr (csr | 0x8040);   // flush-to-zero i denormals-are-zero (les cues dels filtres no costen CPU)
#endif
        for (int a = 0; a < n; a += maxBloc) processaBloc (L + a, R + a, std::min (maxBloc, n - a));
#ifdef MIBASE_GR_FTZ
        _mm_setcsr (csr);
#endif
        epoca.fetch_add (1);
    }

    void GuitarRack::processaBloc (float* L, float* R, int n) noexcept
    {
        auto& I = *imp;
        if (n <= 0) return;
        if (demanaBuidar.exchange (false))
        {
            I.amp.neteja();
            I.pant.A.neteja(); I.pant.B.neteja(); I.pant.sala.buida();
            if (auto* p = I.aplicada.placa) for (auto& x : p->pedals) x->buida();
        }
        Imp::Config vol;
        vol.placa = actual.load();
        vol.amp = iAmp.load (std::memory_order_relaxed);
        vol.canal = (int) valor (Canal);
        vol.inter = valor (BrightP) > 0.5f ? 1 : 0;
        vol.cab = iCab.load (std::memory_order_relaxed);
        vol.alt = iAlt.load (std::memory_order_relaxed);
        vol.micA = (int) valor (MicA);
        vol.micB = (int) valor (MicB);
        vol.ampOn = valor (AmpOn) > 0.5f ? 1 : 0;
        vol.cabOn = valor (CabOn) > 0.5f ? 1 : 0;
        auto aplica = [&] (const Imp::Config& c)
        {
            if (c.amp != I.aplicada.amp || c.canal != I.aplicada.canal || c.inter != I.aplicada.inter || c.ampOn != I.aplicada.ampOn)
                I.amp.configura (c.amp, c.canal, c.inter);
            if (c.cab != I.aplicada.cab || c.alt != I.aplicada.alt || c.micA != I.aplicada.micA || c.micB != I.aplicada.micB || c.cabOn != I.aplicada.cabOn)
                I.pant.configura (c.cab, c.alt, c.micA, c.micB);
            I.aplicada = c;
            usA.store (c.placa);
        };
        if (I.primer) { aplica (vol); I.primer = false; I.estatFosa = 0; I.fosa = 1.0f; I.gIn = dbLin (valor (Entrada)); I.gOut = dbLin (valor (Sortida)); }
        else if (I.estatFosa == 0 && ! (vol == I.aplicada)) I.estatFosa = 1;
        else if (I.estatFosa == 1 && I.fosa <= 0.0f) { aplica (vol); I.estatFosa = 2; }
        else if (I.estatFosa == 2 && ! (vol == I.aplicada)) I.estatFosa = 1;

        Placa* pl = I.aplicada.placa;
        float* m = I.m.data();
        n = std::min (n, (int) I.m.size());
        // entrada (mono) i gate
        const float gInO = dbLin (valor (Entrada)), gOutO = dbLin (valor (Sortida));
        const float kS = coefSuau (fs, 20.0);
        const float thrDb = valor (Gate);
        const bool ambGate = thrDb > -95.0f;
        const float thr = dbLin (thrDb), thrT = dbLin (thrDb - 6.0f);
        const float aE = coefSuau (fs, 0.5), rE = coefSuau (fs, 40.0), kO = coefSuau (fs, 0.8), kT = coefSuau (fs, 70.0);
        float pIn = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            I.gIn += (gInO - I.gIn) * kS;
            float x = 0.5f * (L[i] + R[i]) * I.gIn;
            pIn = std::max (pIn, std::abs (x));
            if (ambGate)
            {
                const float a = std::abs (x);
                I.env += (a - I.env) * (a > I.env ? aE : rE);
                if (I.env > thr) I.gateObert = true; else if (I.env < thrT) I.gateObert = false;
                I.gGate += ((I.gateObert ? 1.0f : 0.0f) - I.gGate) * (I.gateObert ? kO : kT);
                x *= I.gGate;
            }
            else I.gGate = 1.0f;
            m[i] = x;
        }
        // pedals d'abans de l'ampli
        if (pl != nullptr)
            for (size_t k = 0; k < pl->pedals.size(); ++k)
                if (k >= pl->loop.size() || ! pl->loop[k]) pl->pedals[k]->passa (m, nullptr, n);
        // ampli
        float pAmp = 0.0f;
        if (I.aplicada.ampOn)
        {
            std::array<float, numAmpCtl> obj {};
            for (int c = 0; c < numAmpCtl; ++c) obj[(size_t) c] = valor (AmpGain + c);
            I.amp.processa (m, n, obj, pAmp);
        }
        // pantalla, micròfons i sala
        if (I.aplicada.cabOn)
            I.pant.processa (m, L, R, n, valor (PosA), valor (DistA), valor (AngleA), valor (PosB), valor (DistB), valor (AngleB),
                             valor (MesclaB) / 100.0f, valor (Room) / 100.0f, valor (IrOn) > 0.5f ? valor (IrMescla) / 100.0f : 0.0f);
        else
            for (int i = 0; i < n; ++i) L[i] = R[i] = m[i] * 0.5f;
        // loop d'efectes
        if (pl != nullptr)
            for (size_t k = 0; k < pl->pedals.size(); ++k)
                if (k < pl->loop.size() && pl->loop[k]) pl->pedals[k]->passa (L, R, n);
        // sortida, protecció i fosa
        const float pas = I.estatFosa == 1 ? -1.0f / (float) (0.005 * fs) : (I.estatFosa == 2 ? 1.0f / (float) (0.008 * fs) : 0.0f);
        float pOut = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            I.gOut += (gOutO - I.gOut) * kS;
            if (pas != 0.0f) I.fosa = lim (I.fosa + pas, 0.0f, 1.0f);
            const float g = I.gOut * I.fosa;
            float l = L[i] * g, r = R[i] * g;
            auto protegeix = [] (float v) noexcept
            {
                const float a = std::abs (v);
                if (a <= 0.9f) return v;
                const float s = 0.9f + 0.1f * tanhR ((a - 0.9f) * 10.0f);
                return v < 0.0f ? -s : s;
            };
            l = protegeix (l); r = protegeix (r);
            if (! std::isfinite (l)) l = 0.0f;
            if (! std::isfinite (r)) r = 0.0f;
            L[i] = l; R[i] = r;
            pOut = std::max (pOut, std::max (std::abs (l), std::abs (r)));
        }
        if (I.estatFosa == 2 && I.fosa >= 1.0f) I.estatFosa = 0;
        auto pujaPic = [] (std::atomic<float>& a, float v) { if (v > a.load (std::memory_order_relaxed)) a.store (v, std::memory_order_relaxed); };
        pujaPic (mes.entrada, pIn);
        pujaPic (mes.sortida, pOut);
        pujaPic (mes.ampli, pAmp);
        mes.gate.store (I.gGate, std::memory_order_relaxed);
    }

    // ---------------------------------------------------------------------------- paràmetres (automatització)
    InfoParametre GuitarRack::infoParametre (int i) const
    {
        InfoParametre r;
        if (i < 0 || i >= numParams) return r;
        if (i < numFixos)
        {
            const auto& f = infoFix (i);
            r.id = f.clau;
            r.nom = f.nom;
            r.passos = f.passos;
            if (i >= AmpGain && i <= AmpExtra)
            {
                const auto& et = amplis()[(size_t) ampli()].etiqueta[(size_t) (i - AmpGain)];
                r.nom = "Ampli · " + (et.empty() ? std::string (f.nom) + " (aquest ampli no en té)" : et);
                r.automatitzable = ! et.empty();
            }
            if (i == Canal) r.passos = 4;
            return r;
        }
        const int k = (i - numFixos) / paramsPerPedal, c = (i - numFixos) % paramsPerPedal - 1;
        r.id = "p" + std::to_string (k + 1) + (c < 0 ? std::string ("on") : "c" + std::to_string (c + 1));
        const Pedal* p = pedal (k);
        if (p == nullptr) { r.nom = "Pedal " + std::to_string (k + 1) + " (buit)"; r.automatitzable = false; return r; }
        r.nom = "Pedal " + std::to_string (k + 1) + " · " + p->model().nom + " · ";
        if (c < 0) { r.nom += "On"; r.passos = 2; return r; }
        if (c >= p->numControls()) { r.nom += "(sense control)"; r.automatitzable = false; return r; }
        const auto& ct = p->model().controls[(size_t) c];
        r.nom += ct.nom;
        if (! ct.opcions.empty()) r.passos = (int) ct.opcions.size();
        return r;
    }

    void GuitarRack::posaParametre (int i, float v01) noexcept
    {
        if (i < 0 || i >= numParams || ! std::isfinite (v01)) return;
        v01 = lim (v01, 0.0f, 1.0f);
        if (i < numFixos) { const auto& f = infoFix (i); posa (i, f.mn + v01 * (f.mx - f.mn)); return; }
        const int k = (i - numFixos) / paramsPerPedal, c = (i - numFixos) % paramsPerPedal - 1;
        Pedal* p = slot[(size_t) k].load();
        if (p == nullptr) return;
        if (c < 0) { p->on.store (v01 > 0.5f); return; }
        if (c >= p->numControls()) return;
        p->posa (c, Pedal::deNormal (p->model().controls[(size_t) c], v01));
    }

    float GuitarRack::parametre (int i) const noexcept
    {
        if (i < 0 || i >= numParams) return 0.0f;
        if (i < numFixos) { const auto& f = infoFix (i); return f.mx > f.mn ? (valor (i) - f.mn) / (f.mx - f.mn) : 0.0f; }
        const int k = (i - numFixos) / paramsPerPedal, c = (i - numFixos) % paramsPerPedal - 1;
        const Pedal* p = slot[(size_t) k].load();
        if (p == nullptr) return 0.0f;
        if (c < 0) return p->on.load() ? 1.0f : 0.0f;
        if (c >= p->numControls()) return 0.0f;
        return Pedal::normal (p->model().controls[(size_t) c], p->valor (c));
    }

    std::string GuitarRack::textParametre (int i, float v01) const
    {
        char b[48];
        if (i < 0 || i >= numParams) return {};
        if (i < numFixos)
        {
            const auto& f = infoFix (i);
            const float v = f.mn + lim (v01, 0, 1) * (f.mx - f.mn);
            if (i == Entrada || i == Sortida) std::snprintf (b, sizeof b, "%+.1f dB", (double) v);
            else if (i == Gate) { if (v <= -95.0f) return "Apagat"; std::snprintf (b, sizeof b, "%.0f dB", (double) v); }
            else if (f.passos == 2) return v > 0.5f ? "On" : "Off";
            else if (i == Canal)
            {
                const auto& a = amplis()[(size_t) ampli()];
                const int c = (int) std::lround (v);
                return c < (int) a.canals.size() ? a.canals[(size_t) c] : std::to_string (c + 1);
            }
            else if (i == MicA || i == MicB) return micros()[(size_t) std::clamp ((int) std::lround (v), 0, 3)].nom;
            else if (i == AngleA || i == AngleB) std::snprintf (b, sizeof b, "%.0f°", (double) v);
            else if (i == MesclaB || i == Room || i == IrMescla) std::snprintf (b, sizeof b, "%.0f %%", (double) v);
            else if (i == PosA || i == PosB || i == DistA || i == DistB) std::snprintf (b, sizeof b, "%.0f %%", (double) v * 100.0);
            else std::snprintf (b, sizeof b, "%.1f", (double) v);
            std::string s = b;
            for (auto& ch : s) if (ch == '.') ch = ',';
            return s;
        }
        const int k = (i - numFixos) / paramsPerPedal, c = (i - numFixos) % paramsPerPedal - 1;
        const Pedal* p = pedal (k);
        if (p == nullptr) return "-";
        if (c < 0) return v01 > 0.5f ? "On" : "Off";
        if (c >= p->numControls()) return "-";
        return p->text (c, Pedal::deNormal (p->model().controls[(size_t) c], v01));
    }

    // ---------------------------------------------------------------------------- estat
    std::string GuitarRack::estat() const
    {
        nlohmann::json j;
        j["v"] = 1;
        j["amp"] = amplis()[(size_t) ampli()].clau;
        j["cab"] = cabs()[(size_t) cab()].clau;
        j["alt"] = altaveus()[(size_t) altaveu()].clau;
        auto& f = j["fix"];
        f = nlohmann::json::object();
        for (int p = 0; p < numFixos; ++p) f[infoFix (p).clau] = valor (p);
        auto& ps = j["pedals"];
        ps = nlohmann::json::array();
        for (auto& p : ui->pedals)
        {
            nlohmann::json x;
            x["m"] = p->model().clau;
            x["on"] = p->on.load();
            x["loop"] = p->loop.load();
            auto& c = x["c"];
            c = nlohmann::json::object();
            for (int k = 0; k < p->numControls(); ++k) c[p->model().controls[(size_t) k].clau] = p->valor (k);
            ps.push_back (x);
        }
        if (! presetNom.empty()) { j["preset"] = presetNom; j["mod"] = presetMod; }
        if (! refIR.empty()) j["ir"] = refIR;   // 1.6
        nlohmann::json m = nlohmann::json::object();
        for (int p = 0; p < numParams; ++p)
            if (const int c = ccDe (p); c >= 0) m[infoParametre (p).id] = c;
        if (! m.empty()) j["cc"] = m;
        return j.dump();
    }

    void GuitarRack::posaEstat (const std::string& s)
    {
        if (s.empty()) { if (&s != &estatInicial()) posaEstat (estatInicial()); return; }
        nlohmann::json j = nlohmann::json::parse (s, nullptr, false);
        if (j.is_discarded() || ! j.is_object()) return;
        const int a = indexAmpli (j.value ("amp", std::string()));
        if (a >= 0) posaAmpli (a, true);
        const int c = indexCab (j.value ("cab", std::string()));
        if (c >= 0) posaCab (c, true);
        const int al = indexAltaveu (j.value ("alt", std::string()));
        if (al >= 0) posaAltaveu (al);
        if (auto f = j.find ("fix"); f != j.end() && f->is_object())
            for (int p = 0; p < numFixos; ++p)
                if (auto v = f->find (infoFix (p).clau); v != f->end() && v->is_number()) posa (p, v->get<float>());
        auto nova = std::make_shared<Placa>();
        if (auto ps = j.find ("pedals"); ps != j.end() && ps->is_array())
            for (const auto& x : *ps)
            {
                if ((int) nova->pedals.size() >= maxPedals) break;
                auto p = creaPedal (x.value ("m", std::string()));
                if (p == nullptr) continue;
                p->on.store (x.value ("on", true));
                p->loop.store (x.value ("loop", false));
                if (auto cc = x.find ("c"); cc != x.end() && cc->is_object())
                    for (int k = 0; k < p->numControls(); ++k)
                        if (auto v = cc->find (p->model().controls[(size_t) k].clau); v != cc->end() && v->is_number()) p->posa (k, v->get<float>());
                p->posaTempo (tempo);
                if (preparat) p->prepara (fs, maxBloc);
                nova->pedals.push_back (p);
            }
        publica (nova);
        // 1.6: la IR de pantalla i el MIDI learn
        if (const auto r = j.value ("ir", std::string()); r != refIR)
        {
            const float on = valor (IrOn), mes = valor (IrMescla);
            if (r.empty()) treuIR();
            else { std::string err; carregaIR (r, err); }
            posa (IrOn, on); posa (IrMescla, mes);
        }
        for (auto& x : cc) x.store (-1);
        if (auto m = j.find ("cc"); m != j.end() && m->is_object())
            for (auto it = m->begin(); it != m->end(); ++it)
                if (it.value().is_number_integer())
                    if (const int p = indexParametre (it.key()); p >= 0) cc[(size_t) p].store (std::clamp (it.value().get<int>(), 0, 127));
        presetNom = j.value ("preset", std::string());
        presetMod = j.value ("mod", false);
        ++canvis;
    }
}

// ======================================================================================== 1.6: presets de pedals i amplis
namespace mibase::guitarrack
{
    std::string estatPedal (const Pedal& p)
    {
        nlohmann::json j = nlohmann::json::object();
        for (int k = 0; k < p.numControls(); ++k) j[p.model().controls[(size_t) k].clau] = p.valor (k);
        return j.dump();
    }
    void posaEstatPedal (Pedal& p, const std::string& s)
    {
        auto j = nlohmann::json::parse (s, nullptr, false);
        if (! j.is_object()) return;
        for (int k = 0; k < p.numControls(); ++k)
            if (auto v = j.find (p.model().controls[(size_t) k].clau); v != j.end() && v->is_number()) p.posa (k, v->get<float>());
    }
    std::string estatAmpli (const GuitarRack& r)
    {
        nlohmann::json j;
        j["canal"] = r.valor (Canal);
        j["inter"] = r.valor (BrightP);
        auto& v = j["poms"];
        v = nlohmann::json::array();
        for (int c = 0; c < numAmpCtl; ++c) v.push_back (r.valor (AmpGain + c));
        return j.dump();
    }
    void posaEstatAmpli (GuitarRack& r, const std::string& s)
    {
        auto j = nlohmann::json::parse (s, nullptr, false);
        if (! j.is_object()) return;
        r.posa (Canal, j.value ("canal", 0.0f));
        r.posa (BrightP, j.value ("inter", 0.0f));
        if (auto v = j.find ("poms"); v != j.end() && v->is_array())
            for (int c = 0; c < numAmpCtl && c < (int) v->size(); ++c) if ((*v)[(size_t) c].is_number()) r.posa (AmpGain + c, (*v)[(size_t) c].get<float>());
    }

    std::vector<std::pair<std::string, std::string>> presetsFabricaPedal (const std::string& clau)
    {
        std::vector<std::pair<std::string, std::string>> v;
        std::set<std::string> vistos;
        for (const auto& pr : presetsFabrica())
        {
            GuitarRack r;
            r.posaEstat (pr.estat);
            for (int k = 0; k < r.numPedals(); ++k)
                if (r.pedal (k)->model().clau == clau)
                {
                    const auto e = estatPedal (*r.pedal (k));
                    if (vistos.insert (e).second) v.push_back ({ pr.nom, e });
                }
        }
        return v;
    }
    std::vector<std::pair<std::string, std::string>> presetsFabricaAmpli (const std::string& clau)
    {
        std::vector<std::pair<std::string, std::string>> v;
        std::set<std::string> vistos;
        for (const auto& pr : presetsFabrica())
        {
            GuitarRack r;
            r.posaEstat (pr.estat);
            if (amplis()[(size_t) r.ampli()].clau != clau) continue;
            const auto e = estatAmpli (r);
            if (vistos.insert (e).second) v.push_back ({ pr.nom, e });
        }
        return v;
    }

    bool PresetsPeces::carrega (const std::filesystem::path& f)
    {
        v.clear();
        std::ifstream in (f);
        if (! in) return false;
        auto j = nlohmann::json::parse (in, nullptr, false);
        if (! j.is_object()) return false;
        for (auto it = j.begin(); it != j.end(); ++it)
            if (it.value().is_array())
                for (const auto& x : it.value())
                    if (x.is_object()) v[it.key()].push_back ({ x.value ("nom", std::string()), x.value ("estat", std::string()) });
        return true;
    }
    bool PresetsPeces::desa (const std::filesystem::path& f) const
    {
        nlohmann::json j = nlohmann::json::object();
        for (const auto& [k, l] : v)
        {
            auto& a = j[k];
            a = nlohmann::json::array();
            for (const auto& [n, e] : l) a.push_back ({ { "nom", n }, { "estat", e } });
        }
        std::error_code ec;
        std::filesystem::create_directories (f.parent_path(), ec);
        std::ofstream out (f);
        if (! out) return false;
        out << j.dump (1);
        return (bool) out;
    }
    void PresetsPeces::posa (const std::string& clau, const std::string& nom, const std::string& estat)
    {
        auto& l = v[clau];
        for (auto& [n, e] : l) if (n == nom) { e = estat; return; }
        l.push_back ({ nom, estat });
    }
    bool PresetsPeces::esborra (const std::string& clau, const std::string& nom)
    {
        auto it = v.find (clau);
        if (it == v.end()) return false;
        auto& l = it->second;
        const auto n0 = l.size();
        l.erase (std::remove_if (l.begin(), l.end(), [&] (const auto& x) { return x.first == nom; }), l.end());
        return l.size() != n0;
    }
    const std::vector<std::pair<std::string, std::string>>& PresetsPeces::de (const std::string& clau) const
    {
        static const std::vector<std::pair<std::string, std::string>> buit;
        auto it = v.find (clau);
        return it != v.end() ? it->second : buit;
    }
}
