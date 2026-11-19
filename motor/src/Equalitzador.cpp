#include "mibase/Equalitzador.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#if defined(__SSE__) || defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__)
 #include <xmmintrin.h>
 #define MIBASE_EQ_FTZ 1
#endif

namespace mibase::eq
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;

        template <typename T> T lim (T v, T a, T b) { return v < a ? a : (v > b ? b : v); }

        /** Una secció analògica normalitzada (s dividida per la freqüència de la banda) i com s'ha de passar a digital. */
        struct Analog
        {
            enum Com { General, PassaAlts2, Rebuig, PassaTot, Primer, BaixosBil2, BaixosBil1 } com = General;
            double n2 = 0, n1 = 0, n0 = 1, d2 = 1, d1 = 1, d0 = 1;   // (n2 s² + n1 s + n0) / (d2 s² + d1 s + d0); primer ordre: d2 = n2 = 0
            double q = qDefecte;
        };

        double mag2 (const Analog& a, double w)   // |H(jw)|², w normalitzada
        {
            const double w2 = w * w;
            const double nr = a.n0 - a.n2 * w2, ni = a.n1 * w;
            const double dr = a.d0 - a.d2 * w2, di = a.d1 * w;
            const double d = dr * dr + di * di;
            return d > 0.0 ? (nr * nr + ni * ni) / d : 0.0;
        }

        /** Punts on s'ajusta la corba: de 15 Hz (a 48 kHz) fins a tocar dalt de tot, en escala logarítmica. */
        constexpr int numPunts = 40;
        double punt (int i, double)
        {
            const double lo = 0.002, hi = 0.995 * pi;
            return lo * std::pow (hi / lo, (double) i / (numPunts - 1));
        }

        int analogiques (const Banda& b, Analog* out)
        {
            const double A = std::pow (10.0, (double) b.guany / 40.0);
            const double q = lim ((double) b.q, (double) qMin, (double) qMax);
            switch (b.tipus)
            {
                case Bell:
                    out[0] = { Analog::General, 1.0, A / q, 1.0, 1.0, 1.0 / (A * q), 1.0, q };
                    return 1;
                case LowShelf:
                {
                    const double sA = std::sqrt (A);
                    out[0] = { Analog::General, A, A * sA / q, A * A, A, sA / q, 1.0, q };
                    return 1;
                }
                case HighShelf:
                {
                    const double sA = std::sqrt (A);
                    out[0] = { Analog::General, A * A, A * sA / q, A, 1.0, sA / q, A, q };
                    return 1;
                }
                case Notch:
                    out[0] = { Analog::Rebuig, 1.0, 0.0, 1.0, 1.0, 1.0 / q, 1.0, q };
                    return 1;
                case BandPass:
                    out[0] = { Analog::General, 0.0, 1.0 / q, 0.0, 1.0, 1.0 / q, 1.0, q };
                    return 1;
                case AllPass:
                    out[0] = { Analog::PassaTot, 1.0, -1.0 / q, 1.0, 1.0, 1.0 / q, 1.0, q };
                    return 1;
                case Tilt:
                {
                    const double G = std::pow (10.0, (double) b.guany / 20.0), sG = std::sqrt (G);
                    // √G (s + 1/√G) / (s + √G): -guany/2 al greu, 0 dB al centre, +guany/2 a dalt
                    out[0] = { Analog::Primer, 0.0, sG, 1.0, 0.0, 1.0, sG, q };
                    return 1;
                }
                case HighPass:
                case LowPass:
                {
                    const bool alts = b.tipus == HighPass;
                    const int N = ordre (b.pendent);
                    int n = 0;
                    const int parells = N / 2;
                    for (int k = 1; k <= parells; ++k)
                    {
                        double qk = 1.0 / (2.0 * std::cos ((2.0 * k - 1.0) * pi / (2.0 * N)));
                        if (k == parells) qk *= q / (double) qDefecte;   // la Q de la banda fa la ressonància
                        qk = lim (qk, 0.025, 60.0);
                        if (alts) out[n++] = { Analog::PassaAlts2, 1.0, 0.0, 0.0, 1.0, 1.0 / qk, 1.0, qk };
                        else out[n++] = { N >= 4 ? Analog::BaixosBil2 : Analog::General, 0.0, 0.0, 1.0, 1.0, 1.0 / qk, 1.0, qk };
                    }
                    if (N % 2 == 1)
                    {
                        if (alts) out[n++] = { Analog::Primer, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, q };
                        else out[n++] = { N >= 4 ? Analog::BaixosBil1 : Analog::Primer, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, q };
                    }
                    return n;
                }
                default: return 0;
            }
        }

        /** Pols de segon ordre «ajustats» (invariància a l'impuls) per a una freqüència wp (rad/mostra) i una Q. */
        void pols (double wp, double qp, double& a1, double& a2)
        {
            wp = lim (wp, 1.0e-6, 0.97 * pi);
            const double z = 1.0 / (2.0 * qp);
            const double e = std::exp (-z * wp);
            a2 = e * e;
            a1 = z <= 1.0 ? -2.0 * e * std::cos (wp * std::sqrt (1.0 - z * z))
                          : -2.0 * e * std::cosh (wp * std::sqrt (z * z - 1.0));
        }

        double magA2 (double a1, double a2, double phi1)   // |A(e^jw)|² amb phi1 = sin²(w/2)
        {
            const double phi0 = 1.0 - phi1, phi2 = 4.0 * phi0 * phi1;
            return (1.0 + a1 + a2) * (1.0 + a1 + a2) * phi0 + (1.0 - a1 + a2) * (1.0 - a1 + a2) * phi1 - 4.0 * a2 * phi2;
        }

        /** Numerador de fase mínima a partir de |B|² = B0 phi0 + B1 phi1 + B2 phi2. */
        void numerador (double B0, double B1, double B2, Seccio& s)
        {
            const double r0 = std::sqrt (std::max (0.0, B0)), r1 = std::sqrt (std::max (0.0, B1));
            const double W = 0.5 * (r0 + r1);
            s.b1 = 0.5 * (r0 - r1);
            const double d = std::max (0.0, W * W + B2);
            s.b0 = 0.5 * (W + std::sqrt (d));
            s.b2 = W - s.b0;
        }

        Seccio digital (const Analog& a, double w0)
        {
            Seccio s;
            if (a.com == Analog::BaixosBil2 || a.com == Analog::BaixosBil1)
            {
                // Passa-baixos drets (24 dB/oct o més): transformada bilineal. A prop de dalt de tot cau més que l'analògic
                // (mai menys), i la zona que passa queda plana; l'ajust analògic aquí faria una panxa a la zona que passa.
                const double K = std::tan (0.5 * std::min (w0, 0.97 * pi));
                if (a.com == Analog::BaixosBil1)
                {
                    const double nr = 1.0 / (1.0 + K);
                    s.b0 = K * nr; s.b1 = s.b0; s.b2 = 0.0; s.a1 = (K - 1.0) * nr; s.a2 = 0.0;
                    return s;
                }
                const double nr = 1.0 / (1.0 + K / a.q + K * K);
                s.b0 = K * K * nr; s.b1 = 2.0 * s.b0; s.b2 = s.b0;
                s.a1 = 2.0 * (K * K - 1.0) * nr; s.a2 = (1.0 - K / a.q + K * K) * nr;
                return s;
            }
            if (a.com == Analog::Primer)
            {
                // pol a -d0/d1 (normalitzat); zeros ajustats al greu i a dalt de tot
                const double wp = lim (w0 * a.d0 / a.d1, 1.0e-7, 0.97 * pi);
                const double p = std::exp (-wp);
                s.a1 = -p;
                s.a2 = 0.0;
                // |B|² = C0 cos²(w/2) + C1 sin²(w/2): C0 fa el greu exacte; C1, el millor ajust (relatiu) fins a dalt
                const double C0 = mag2 (a, 0.0) * (1.0 - p) * (1.0 - p);
                double C1 = mag2 (a, pi / w0) * (1.0 + p) * (1.0 + p);
                {
                    double num = 0.0, den = 0.0, tmax = 0.0;
                    double tw[numPunts], c2[numPunts], s2[numPunts];
                    for (int i = 0; i < numPunts; ++i)
                    {
                        const double w = punt (i, w0);
                        c2[i] = std::pow (std::cos (0.5 * w), 2.0);
                        s2[i] = 1.0 - c2[i];
                        tw[i] = mag2 (a, w / w0) * (1.0 + p * p - 2.0 * p * std::cos (w));
                        tmax = std::max (tmax, tw[i]);
                    }
                    for (int i = 0; i < numPunts; ++i)
                    {
                        const double pes = 1.0 / std::max (tw[i], 1.0e-3 * tmax);
                        num += pes * pes * s2[i] * (tw[i] - C0 * c2[i]);
                        den += pes * pes * s2[i] * s2[i];
                    }
                    if (den > 0.0 && num > 0.0) C1 = num / den;
                }
                const double dc = std::sqrt (C0), ny = std::sqrt (C1);
                s.b0 = 0.5 * (dc + ny);
                s.b1 = 0.5 * (dc - ny);
                s.b2 = 0.0;
                return s;
            }
            const double wp = w0 * std::sqrt (a.d0 / a.d2);
            const double qp = std::sqrt (a.d0 * a.d2) / a.d1;
            pols (wp, qp, s.a1, s.a2);
            const double w0c = std::min (w0, 0.97 * pi);
            const double phiC = std::pow (std::sin (0.5 * w0c), 2.0);
            switch (a.com)
            {
                case Analog::PassaTot:
                    s.b0 = s.a2; s.b1 = s.a1; s.b2 = 1.0;
                    return s;
                case Analog::PassaAlts2:
                {
                    // zero doble al greu; |H| = Q al centre (com l'analògic)
                    s.b0 = a.q * std::sqrt (std::max (0.0, magA2 (s.a1, s.a2, phiC))) / (4.0 * phiC);
                    s.b1 = -2.0 * s.b0;
                    s.b2 = s.b0;
                    return s;
                }
                case Analog::Rebuig:
                {
                    // zeros just a la freqüència (rebuig total) i el greu igual que l'analògic
                    const double c = std::cos (w0c);
                    const double k = (1.0 + s.a1 + s.a2) / (2.0 - 2.0 * c);
                    s.b0 = k; s.b1 = -2.0 * c * k; s.b2 = k;
                    return s;
                }
                default: break;
            }
            const double A0 = (1.0 + s.a1 + s.a2) * (1.0 + s.a1 + s.a2);
            const double A1 = (1.0 - s.a1 + s.a2) * (1.0 - s.a1 + s.a2);
            const double B0 = mag2 (a, 0.0) * A0;
            const double B1 = mag2 (a, pi / w0) * A1;
            const double wm = std::min (w0, 0.8 * pi);
            const double phi1 = std::pow (std::sin (0.5 * wm), 2.0), phi0 = 1.0 - phi1, phi2 = 4.0 * phi0 * phi1;
            const double B2 = (mag2 (a, wm / w0) * magA2 (s.a1, s.a2, phi1) - B0 * phi0 - B1 * phi1) / phi2;

            // Millor ajust: B0 fix (greu exacte); B1 i B2 per mínims quadrats relatius de 15 Hz fins a dalt, amb més pes al centre.
            double m11 = 0, m12 = 0, m22 = 0, v1 = 0, v2 = 0, tmax = 0;
            double t[numPunts + 1], f1[numPunts + 1], f2[numPunts + 1], f0[numPunts + 1], pes[numPunts + 1];
            for (int i = 0; i <= numPunts; ++i)
            {
                const double w = i < numPunts ? punt (i, w0) : std::min (w0, 0.97 * pi);
                const double ph1 = std::pow (std::sin (0.5 * w), 2.0), ph0 = 1.0 - ph1;
                f0[i] = ph0; f1[i] = ph1; f2[i] = 4.0 * ph0 * ph1;
                t[i] = mag2 (a, w / w0) * magA2 (s.a1, s.a2, ph1);
                tmax = std::max (tmax, t[i]);
                pes[i] = i < numPunts ? 1.0 : 4.0;
            }
            for (int i = 0; i <= numPunts; ++i)
            {
                const double k = pes[i] / std::max (t[i], 1.0e-3 * tmax), k2 = k * k;
                const double y = t[i] - B0 * f0[i];
                m11 += k2 * f1[i] * f1[i]; m12 += k2 * f1[i] * f2[i]; m22 += k2 * f2[i] * f2[i];
                v1 += k2 * f1[i] * y; v2 += k2 * f2[i] * y;
            }
            const double det = m11 * m22 - m12 * m12;
            if (std::abs (det) > 1.0e-300)
            {
                const double L1 = (v1 * m22 - v2 * m12) / det, L2 = (m11 * v2 - m12 * v1) / det;
                // vàlid només si |B|² no és negatiu enlloc
                bool bo = L1 >= 0.0;
                for (int i = 0; i <= 64 && bo; ++i)
                {
                    const double x = i / 64.0;
                    bo = B0 * (1.0 - x) + L1 * x + 4.0 * L2 * x * (1.0 - x) >= -1.0e-12 * (B0 + L1 + 1.0e-30);
                }
                const double r0 = std::sqrt (std::max (0.0, B0)), r1 = std::sqrt (std::max (0.0, L1));
                const double W = 0.5 * (r0 + r1);
                if (bo && W * W + L2 >= -1.0e-9 * (W * W + 1.0e-30)) { numerador (B0, L1, L2, s); return s; }
            }
            numerador (B0, B1, B2, s);
            return s;
        }

        float lin (float db) { return std::pow (10.0f, db / 20.0f); }

        std::string num (double v, int dec)
        {
            char b[48];
            std::snprintf (b, sizeof b, "%.*f", dec, v);
            std::string s (b);
            for (auto& c : s) if (c == '.') c = ',';
            return s;
        }

        double llegeixNum (const std::string& s, size_t& p)
        {
            const char* c = s.c_str() + p;
            char* fi = nullptr;
            const double v = std::strtod (c, &fi);
            p += (size_t) (fi - c);
            if (p < s.size() && s[p] == ',') ++p;
            return v;
        }
    }

    // ---- Noms i capacitats ----
    const char* nomTipus (int t)
    {
        static const char* n[] = { "Bell", "Low Shelf", "High Shelf", "High Pass", "Low Pass", "Notch", "Band Pass", "Tilt", "All Pass" };
        return t >= 0 && t < numTipus ? n[t] : "";
    }
    const char* nomCanal (int c)
    {
        static const char* n[] = { "Estèreo", "Esquerre", "Dret", "Mid", "Side" };
        return c >= 0 && c < numCanals ? n[c] : "";
    }
    const char* curtCanal (int c)
    {
        static const char* n[] = { "ST", "L", "R", "M", "S" };
        return c >= 0 && c < numCanals ? n[c] : "";
    }
    int dbPerOctava (int p) { static const int v[] = { 6, 12, 18, 24, 36, 48, 72, 96 }; return v[lim (p, 0, numPendents - 1)]; }
    int ordre (int p) { return dbPerOctava (p) / 6; }
    bool teGuany (int t) { return t == Bell || t == LowShelf || t == HighShelf || t == Tilt; }
    bool tePendent (int t) { return t == HighPass || t == LowPass; }
    bool teQ (int t, int p)
    {
        if (t == Tilt) return false;
        if (tePendent (t)) return ordre (p) >= 2;
        return true;
    }

    int dissenya (const Banda& b, double fs, Seccio* out)
    {
        if (fs <= 0.0) return 0;
        Analog a[maxSeccions + 1];
        const int n = analogiques (b, a);
        const double f = lim ((double) b.freq, (double) freqMin, 0.49 * fs);
        const double w0 = 2.0 * pi * f / fs;
        for (int i = 0; i < n; ++i) out[i] = digital (a[i], w0);
        return n;
    }

    std::complex<double> resposta (const Seccio* s, int n, double freqHz, double fs)
    {
        const double w = 2.0 * pi * freqHz / fs;
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
        std::complex<double> h (1.0, 0.0);
        for (int i = 0; i < n; ++i)
            h *= (s[i].b0 + s[i].b1 * z1 + s[i].b2 * z2) / (1.0 + s[i].a1 * z1 + s[i].a2 * z2);
        return h;
    }

    double magnitudAnalogica (const Banda& b, double freqHz)
    {
        Analog a[maxSeccions + 1];
        const int n = analogiques (b, a);
        const double w = freqHz / std::max (1.0e-6, (double) b.freq);
        double m = 1.0;
        for (int i = 0; i < n; ++i) m *= std::sqrt (mag2 (a[i], w));
        return m;
    }

    Seccio dissenyaSolo (const Banda& b, double fs)
    {
        Seccio s;
        if (fs <= 0.0) return s;
        const double f = lim ((double) b.freq, 20.0, 0.45 * fs);
        const double w = 2.0 * pi * f / fs, cs = std::cos (w), sn = std::sin (w);
        const bool baixos = b.tipus == LowShelf || b.tipus == HighPass;
        const bool alts = b.tipus == HighShelf || b.tipus == LowPass;
        if (baixos || alts)
        {
            const double al = sn / (2.0 * 0.7071), a0 = 1.0 + al;
            const double k = baixos ? (1.0 - cs) * 0.5 : (1.0 + cs) * 0.5;
            s.b0 = k / a0;
            s.b1 = (baixos ? 1.0 - cs : -(1.0 + cs)) / a0;
            s.b2 = k / a0;
            s.a1 = -2.0 * cs / a0;
            s.a2 = (1.0 - al) / a0;
            return s;
        }
        const double q = b.tipus == Tilt ? 0.5 : lim ((double) b.q, 0.3, 30.0);
        const double al = sn / (2.0 * q), a0 = 1.0 + al;
        s.b0 = al / a0; s.b1 = 0.0; s.b2 = -al / a0;
        s.a1 = -2.0 * cs / a0;
        s.a2 = (1.0 - al) / a0;
        return s;
    }

    int canalOposat (int c)
    {
        switch (c)
        {
            case Mid: return Side;
            case Side: return Mid;
            case Esquerre: return Dret;
            case Dret: return Esquerre;
            default: return c;
        }
    }

    // ---- Equalitzador ----
    Equalitzador::Equalitzador() = default;

    void Equalitzador::prepara (double nouFs, int)
    {
        fs = nouFs > 0.0 ? nouFs : 48000.0;
        fsAtom.store (fs, std::memory_order_relaxed);
        kSuau = 1.0 - std::exp (-(double) tros / (0.025 * fs));   // uns 25 ms
        pasFosa = 1.0 / (0.012 * fs);                               // 12 ms
        for (auto& v : vi) v = Viva {};
        dinsTros = 0;
        gSort = lin (sortidaDb.load());
        wet = byp.load() ? 0.0 : 1.0;
        apagatDelTot = false;
        demanaBuidar.store (false);
        soloViu = -1;
        soloMix = 0.0;
        for (auto& c : soloZ) c[0] = c[1] = 0.0;
    }

    Banda Equalitzador::banda (int k) const noexcept
    {
        Banda b;
        if (k < 0 || k >= maxBandes) return b;
        const auto& a = at[(size_t) k];
        b.usada = a.usada.load (std::memory_order_relaxed);
        b.activa = a.activa.load (std::memory_order_relaxed);
        b.tipus = a.tipus.load (std::memory_order_relaxed);
        b.pendent = a.pendent.load (std::memory_order_relaxed);
        b.canal = a.canal.load (std::memory_order_relaxed);
        b.freq = a.freq.load (std::memory_order_relaxed);
        b.guany = a.guany.load (std::memory_order_relaxed);
        b.q = a.q.load (std::memory_order_relaxed);
        b.dinamica = a.dinamica.load (std::memory_order_relaxed);
        b.sidechain = a.sidechain.load (std::memory_order_relaxed);
        b.llindar = a.llindar.load (std::memory_order_relaxed);
        b.ratio = a.ratio.load (std::memory_order_relaxed);
        b.rang = a.rang.load (std::memory_order_relaxed);
        b.atac = a.atac.load (std::memory_order_relaxed);
        b.retorn = a.retorn.load (std::memory_order_relaxed);
        return b;
    }

    void Equalitzador::posaBanda (int k, const Banda& b) noexcept
    {
        if (k < 0 || k >= maxBandes) return;
        auto& a = at[(size_t) k];
        a.tipus.store (lim (b.tipus, 0, numTipus - 1), std::memory_order_relaxed);
        a.pendent.store (lim (b.pendent, 0, numPendents - 1), std::memory_order_relaxed);
        a.canal.store (lim (b.canal, 0, numCanals - 1), std::memory_order_relaxed);
        a.freq.store (std::isfinite (b.freq) ? lim (b.freq, freqMin, freqMax) : 1000.0f, std::memory_order_relaxed);
        a.guany.store (std::isfinite (b.guany) ? lim (b.guany, -guanyMax, guanyMax) : 0.0f, std::memory_order_relaxed);
        a.q.store (std::isfinite (b.q) ? lim (b.q, qMin, qMax) : qDefecte, std::memory_order_relaxed);
        a.activa.store (b.activa, std::memory_order_relaxed);
        auto fin = [] (float v, float mn, float mx, float def) { return std::isfinite (v) ? lim (v, mn, mx) : def; };
        a.llindar.store (fin (b.llindar, llindarMin, llindarMax, -24.0f), std::memory_order_relaxed);
        a.ratio.store (fin (b.ratio, ratioMin, ratioMax, 3.0f), std::memory_order_relaxed);
        a.rang.store (fin (b.rang, -rangMax, rangMax, -6.0f), std::memory_order_relaxed);
        a.atac.store (fin (b.atac, atacMin, atacMax, 10.0f), std::memory_order_relaxed);
        a.retorn.store (fin (b.retorn, retornMin, retornMax, 120.0f), std::memory_order_relaxed);
        a.sidechain.store (b.sidechain, std::memory_order_relaxed);
        a.dinamica.store (b.dinamica && potSerDinamica (lim (b.tipus, 0, numTipus - 1)), std::memory_order_relaxed);
        a.usada.store (b.usada, std::memory_order_release);
        toca();
    }

    int Equalitzador::bandaLliure() const noexcept
    {
        for (int k = 0; k < maxBandes; ++k) if (! at[(size_t) k].usada.load (std::memory_order_relaxed)) return k;
        return -1;
    }

    void Equalitzador::esborra (int k) noexcept
    {
        if (k < 0 || k >= maxBandes) return;
        at[(size_t) k].usada.store (false, std::memory_order_release);
        toca();
    }

    void Equalitzador::posaSortida (float db) noexcept
    {
        if (! std::isfinite (db)) return;
        sortidaDb.store (lim (db, -sortidaMax, sortidaMax), std::memory_order_relaxed);
        toca();
    }

    // ---- Paràmetres ----
    InfoParametre Equalitzador::infoParametre (int i) const
    {
        InfoParametre r;
        if (i == PSortida) { r.id = "out"; r.nom = "Sortida"; return r; }
        if (i == PBypass) { r.id = "bypass"; r.nom = "Bypass"; r.passos = 2; return r; }
        if (i < numGlobals || i >= numParams) { r.automatitzable = false; return r; }
        const int k = (i - numGlobals) / paramsPerBanda, p = (i - numGlobals) % paramsPerBanda;
        char id[16];
        std::snprintf (id, sizeof id, "b%02d.", k + 1);
        static const char* ids[] = { "on", "freq", "gain", "q" };
        static const char* noms[] = { "Encesa", "Freqüència", "Guany", "Q" };
        r.id = std::string (id) + ids[p];
        r.nom = "Banda " + std::to_string (k + 1) + " · " + noms[p];
        r.passos = p == PActiva ? 2 : 0;
        r.automatitzable = at[(size_t) k].usada.load (std::memory_order_relaxed);
        return r;
    }

    void Equalitzador::posaParametre (int i, float v) noexcept
    {
        if (! std::isfinite (v)) return;
        v = lim (v, 0.0f, 1.0f);
        if (i == PSortida) { sortidaDb.store (-sortidaMax + 2.0f * sortidaMax * v, std::memory_order_relaxed); toca(); return; }
        if (i == PBypass) { byp.store (v >= 0.5f, std::memory_order_relaxed); toca(); return; }
        if (i < numGlobals || i >= numParams) return;
        const int k = (i - numGlobals) / paramsPerBanda, p = (i - numGlobals) % paramsPerBanda;
        auto& a = at[(size_t) k];
        switch (p)
        {
            case PActiva: a.activa.store (v >= 0.5f, std::memory_order_relaxed); break;
            case PFreq: a.freq.store (freqMin * std::pow (freqMax / freqMin, v), std::memory_order_relaxed); break;
            case PGuany: a.guany.store (-guanyMax + 2.0f * guanyMax * v, std::memory_order_relaxed); break;
            case PQ: a.q.store (qMin * std::pow (qMax / qMin, v), std::memory_order_relaxed); break;
            default: break;
        }
        toca();
    }

    float Equalitzador::parametre (int i) const noexcept
    {
        if (i == PSortida) return (sortidaDb.load (std::memory_order_relaxed) + sortidaMax) / (2.0f * sortidaMax);
        if (i == PBypass) return byp.load (std::memory_order_relaxed) ? 1.0f : 0.0f;
        if (i < numGlobals || i >= numParams) return 0.0f;
        const int k = (i - numGlobals) / paramsPerBanda, p = (i - numGlobals) % paramsPerBanda;
        const auto& a = at[(size_t) k];
        switch (p)
        {
            case PActiva: return a.activa.load (std::memory_order_relaxed) ? 1.0f : 0.0f;
            case PFreq: return lim (std::log (a.freq.load (std::memory_order_relaxed) / freqMin) / std::log (freqMax / freqMin), 0.0f, 1.0f);
            case PGuany: return (a.guany.load (std::memory_order_relaxed) + guanyMax) / (2.0f * guanyMax);
            case PQ: return lim (std::log (a.q.load (std::memory_order_relaxed) / qMin) / std::log (qMax / qMin), 0.0f, 1.0f);
            default: return 0.0f;
        }
    }

    std::string Equalitzador::textParametre (int i, float v) const
    {
        v = lim (v, 0.0f, 1.0f);
        if (i == PSortida) return num (-sortidaMax + 2.0f * sortidaMax * v, 1) + " dB";
        if (i == PBypass) return v >= 0.5f ? "Bypass" : "Actiu";
        if (i < numGlobals || i >= numParams) return {};
        switch ((i - numGlobals) % paramsPerBanda)
        {
            case PActiva: return v >= 0.5f ? "Encesa" : "Apagada";
            case PFreq:
            {
                const double f = freqMin * std::pow ((double) freqMax / freqMin, (double) v);
                return f >= 1000.0 ? num (f / 1000.0, f >= 10000.0 ? 1 : 2) + " kHz" : num (f, f >= 100.0 ? 0 : 1) + " Hz";
            }
            case PGuany: return num (-guanyMax + 2.0f * guanyMax * v, 1) + " dB";
            case PQ: return "Q " + num (qMin * std::pow ((double) qMax / qMin, (double) v), 2);
            default: return {};
        }
    }

    // ---- Estat: «mibase-eq=1;out=..;byp=..;b3=tipus,freq,guany,q,pendent,canal,activa» ----
    std::string Equalitzador::estat() const
    {
        std::string s = "mibase-eq=1";
        char b[160];
        std::snprintf (b, sizeof b, ";out=%.6g;byp=%d", (double) sortidaDb.load(), byp.load() ? 1 : 0);
        s += b;
        for (int k = 0; k < maxBandes; ++k)
        {
            const auto x = banda (k);
            if (! x.usada) continue;
            std::snprintf (b, sizeof b, ";b%d=%d,%.7g,%.6g,%.6g,%d,%d,%d", k, x.tipus, (double) x.freq, (double) x.guany, (double) x.q,
                           x.pendent, x.canal, x.activa ? 1 : 0);
            s += b;
            const Banda def;
            if (x.dinamica || x.sidechain || x.llindar != def.llindar || x.ratio != def.ratio || x.rang != def.rang || x.atac != def.atac || x.retorn != def.retorn)
            {
                // 0.62: la dinàmica (els estats sense aquests camps la tenen apagada)
                std::snprintf (b, sizeof b, ",%d,%d,%.6g,%.6g,%.6g,%.6g,%.6g", x.dinamica ? 1 : 0, x.sidechain ? 1 : 0, (double) x.llindar, (double) x.ratio,
                               (double) x.rang, (double) x.atac, (double) x.retorn);
                s += b;
            }
        }
        return s;
    }

    void Equalitzador::posaEstat (const std::string& s)
    {
        if (s.rfind ("mibase-eq=", 0) != 0) return;
        for (int k = 0; k < maxBandes; ++k) at[(size_t) k].usada.store (false);
        sortidaDb.store (0.0f);
        byp.store (false);
        size_t a = 0;
        while (a < s.size())
        {
            size_t f = s.find (';', a);
            if (f == std::string::npos) f = s.size();
            const auto parell = s.substr (a, f - a);
            a = f + 1;
            const auto eq = parell.find ('=');
            if (eq == std::string::npos) continue;
            const auto clau = parell.substr (0, eq);
            const auto val = parell.substr (eq + 1);
            if (clau == "out") posaSortida ((float) std::strtod (val.c_str(), nullptr));
            else if (clau == "byp") byp.store (std::atoi (val.c_str()) != 0);
            else if (clau.size() > 1 && clau[0] == 'b')
            {
                const int k = std::atoi (clau.c_str() + 1);
                if (k < 0 || k >= maxBandes) continue;
                size_t p = 0;
                Banda x;
                x.usada = true;
                x.tipus = (int) llegeixNum (val, p);
                x.freq = (float) llegeixNum (val, p);
                x.guany = (float) llegeixNum (val, p);
                x.q = (float) llegeixNum (val, p);
                x.pendent = (int) llegeixNum (val, p);
                x.canal = (int) llegeixNum (val, p);
                x.activa = p < val.size() ? llegeixNum (val, p) != 0.0 : true;
                if (p < val.size())   // 0.62: dinàmica
                {
                    x.dinamica = llegeixNum (val, p) != 0.0;
                    x.sidechain = p < val.size() && llegeixNum (val, p) != 0.0;
                    if (p < val.size()) x.llindar = (float) llegeixNum (val, p);
                    if (p < val.size()) x.ratio = (float) llegeixNum (val, p);
                    if (p < val.size()) x.rang = (float) llegeixNum (val, p);
                    if (p < val.size()) x.atac = (float) llegeixNum (val, p);
                    if (p < val.size()) x.retorn = (float) llegeixNum (val, p);
                }
                posaBanda (k, x);
            }
        }
        toca();
    }

    // ---- Fil d'àudio ----
    void Equalitzador::actualitza() noexcept
    {
        if (demanaBuidar.exchange (false, std::memory_order_acq_rel))
            for (auto& v : vi) for (auto& c : v.z) for (auto& s : c) s[0] = s[1] = 0.0;

        for (int k = 0; k < maxBandes; ++k)
        {
            auto& a = at[(size_t) k];
            auto& v = vi[(size_t) k];
            const bool on = a.usada.load (std::memory_order_acquire) && a.activa.load (std::memory_order_relaxed);
            const int t = a.tipus.load (std::memory_order_relaxed), p = a.pendent.load (std::memory_order_relaxed),
                      c = a.canal.load (std::memory_order_relaxed);
            const double lf = std::log ((double) a.freq.load (std::memory_order_relaxed));
            const double g = (double) a.guany.load (std::memory_order_relaxed);
            const double lq = std::log ((double) a.q.load (std::memory_order_relaxed));
            const bool mateixa = t == v.tipus && p == v.pendent && c == v.canal;

            if (! v.viva)
            {
                if (! on) continue;
                // s'encén: comença sense memòria i ja amb els valors finals (sense lliscar des dels vells)
                v.viva = true;
                v.tipus = t; v.pendent = p; v.canal = c;
                v.lf = lf; v.g = g; v.lq = lq;
                v.pres = 0.0;
                v.coefsBons = false;
                for (auto& ch : v.z) for (auto& s : ch) s[0] = s[1] = 0.0;
            }
            else if (! mateixa && v.pres <= 0.0)
            {
                // ja s'ha apagat del tot: canvia d'estructura i torna a sonar
                v.tipus = t; v.pendent = p; v.canal = c;
                v.lf = lf; v.g = g; v.lq = lq;
                v.coefsBons = false;
                for (auto& ch : v.z) for (auto& s : ch) s[0] = s[1] = 0.0;
            }
            else if (! on && v.pres <= 0.0)
            {
                v.viva = false;
                continue;
            }

            // valors suavitzats
            v.interp = false;
            bool canvia = ! v.coefsBons;
            auto suau = [&] (double& x, double obj, double llindar)
            {
                const double d = obj - x;
                if (std::abs (d) < llindar) { if (d != 0.0) { x = obj; canvia = true; } }
                else { x += d * kSuau; canvia = true; }
            };
            suau (v.lf, lf, 1.0e-5);
            suau (v.g, g, 1.0e-4);
            suau (v.lq, lq, 1.0e-5);
            // 0.62: dinàmica (el detector ha calculat v.dyn al tros anterior)
            const bool dinamica = a.dinamica.load (std::memory_order_relaxed);
            if (! dinamica && v.dyn != 0.0) { v.dyn = 0.0; v.env = 0.0; dynAra[(size_t) k].store (0.0f, std::memory_order_relaxed); }
            if (dinamica && v.det.b0 == 1.0 && v.det.b2 == 0.0) canvia = true;   // encara no té el filtre del detector
            if (std::abs (v.dyn - v.dynDis) > 1.0e-3 || (v.dyn == 0.0 && v.dynDis != 0.0)) canvia = true;
            if (canvia)
            {
                Banda b;
                b.tipus = v.tipus; b.pendent = v.pendent; b.canal = v.canal;
                b.freq = (float) std::exp (v.lf); b.guany = (float) (v.g + v.dyn); b.q = (float) std::exp (v.lq);
                v.dynDis = v.dyn;
                if (dinamica) { auto bd = b; bd.guany = 0.0f; v.det = dissenyaSolo (bd, fs); }
                const bool teAnt = v.coefsBons;
                const int nAnt = v.n;
                for (int i = 0; i < nAnt; ++i) v.sAnt[i] = v.s[i];
                v.n = dissenya (b, fs, v.s);
                v.interp = teAnt && nAnt == v.n;   // el pas d'un joc a l'altre es fa mostra a mostra (sense esglaons)
                v.coefsBons = true;
            }
        }

        // escolta d'una banda: el filtre segueix els valors suavitzats de la banda (sense salts)
        const int sObj = solo.load (std::memory_order_relaxed);
        if (sObj >= 0 && at[(size_t) sObj].usada.load (std::memory_order_relaxed))
        {
            if (soloViu != sObj && soloMix <= 0.0) for (auto& c : soloZ) c[0] = c[1] = 0.0;
            soloViu = sObj;
        }
        if (soloViu >= 0)
        {
            const auto& a = at[(size_t) soloViu];
            const auto& v = vi[(size_t) soloViu];
            Banda b;
            b.tipus = a.tipus.load (std::memory_order_relaxed);
            b.canal = a.canal.load (std::memory_order_relaxed);
            b.freq = (float) (v.viva ? std::exp (v.lf) : (double) a.freq.load (std::memory_order_relaxed));
            b.q = (float) (v.viva ? std::exp (v.lq) : (double) a.q.load (std::memory_order_relaxed));
            soloS = dissenyaSolo (b, fs);
        }
    }

    void Equalitzador::tros16 (float* L, float* R, int n, int ofs, const float* sl, const float* sr) noexcept
    {
        double l[tros], r[tros], dl[tros], dr[tros], x[tros], y[tros];
        float picI = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            dl[i] = l[i] = (double) L[i];
            dr[i] = r[i] = (double) R[i];
            picI = std::max (picI, std::max (std::abs (L[i]), std::abs (R[i])));
        }

        const double wetObj = byp.load (std::memory_order_relaxed) ? 0.0 : 1.0;
        const double gObj = (double) lin (sortidaDb.load (std::memory_order_relaxed));
        const bool estalvia = wet <= 0.0 && wetObj <= 0.0;
        bool algunaBanda = false;

        if (! estalvia)
        {
            for (int k = 0; k < maxBandes; ++k)
            {
                auto& v = vi[(size_t) k];
                if (! v.viva) continue;
                const auto& a = at[(size_t) k];
                const bool on = a.usada.load (std::memory_order_relaxed) && a.activa.load (std::memory_order_relaxed);
                const bool mateixa = a.tipus.load (std::memory_order_relaxed) == v.tipus && a.pendent.load (std::memory_order_relaxed) == v.pendent
                                     && a.canal.load (std::memory_order_relaxed) == v.canal;
                const double obj = on && mateixa ? 1.0 : 0.0;
                if (v.pres <= 0.0 && obj <= 0.0) continue;
                algunaBanda = true;

                // 0.62: el detector de la dinàmica (la zona de la banda, del so propi o del sidechain)
                if (on && a.dinamica.load (std::memory_order_relaxed) && v.det.b0 != 1.0)
                {
                    const bool ext = a.sidechain.load (std::memory_order_relaxed) && sl != nullptr && sr != nullptr;
                    const double ka = std::exp (-1.0 / (std::max (0.05, (double) a.atac.load (std::memory_order_relaxed)) * 0.001 * fs));
                    const double kr = std::exp (-1.0 / (std::max (1.0, (double) a.retorn.load (std::memory_order_relaxed)) * 0.001 * fs));
                    const auto& c = v.det;
                    double env = v.env;
                    for (int i = 0; i < n; ++i)
                    {
                        double x;
                        if (ext) x = v.canal == Side ? 0.5 * ((double) sl[i] - sr[i]) : (v.canal == Esquerre ? sl[i] : v.canal == Dret ? sr[i] : 0.5 * ((double) sl[i] + sr[i]));
                        else x = v.canal == Side ? 0.5 * (dl[i] - dr[i]) : (v.canal == Esquerre ? dl[i] : v.canal == Dret ? dr[i] : 0.5 * (dl[i] + dr[i]));
                        const double y = c.b0 * x + v.zd[0];
                        v.zd[0] = c.b1 * x - c.a1 * y + v.zd[1];
                        v.zd[1] = c.b2 * x - c.a2 * y;
                        const double ay = std::abs (y);
                        env = ay > env ? ay + ka * (env - ay) : ay + kr * (env - ay);
                    }
                    if (! std::isfinite (env) || ! std::isfinite (v.zd[0]) || ! std::isfinite (v.zd[1])) { env = 0.0; v.zd[0] = v.zd[1] = 0.0; }
                    v.env = env;
                    const double nivell = 20.0 * std::log10 (std::max (1.0e-9, env));
                    const double sobre = nivell - (double) a.llindar.load (std::memory_order_relaxed);
                    const double rt = std::max (1.0, (double) a.ratio.load (std::memory_order_relaxed)), rg = (double) a.rang.load (std::memory_order_relaxed);
                    const double mou = sobre > 0.0 ? sobre * (1.0 - 1.0 / rt) : 0.0;
                    v.dyn = rg < 0.0 ? -std::min (mou, -rg) : std::min (mou, rg);
                    dynAra[(size_t) k].store ((float) v.dyn, std::memory_order_relaxed);
                }

                auto filtra = [&] (double* buf, int ch)
                {
                    for (int i = 0; i < n; ++i) x[i] = buf[i];
                    for (int s = 0; s < v.n; ++s)
                    {
                        const auto& c = v.s[s];
                        double z1 = v.z[ch][s][0], z2 = v.z[ch][s][1];
                        if (v.interp)
                        {
                            const auto& a0 = v.sAnt[s];
                            const double db0 = c.b0 - a0.b0, db1 = c.b1 - a0.b1, db2 = c.b2 - a0.b2, da1 = c.a1 - a0.a1, da2 = c.a2 - a0.a2;
                            for (int i = 0; i < n; ++i)
                            {
                                const double f = (double) (ofs + i + 1) / (double) tros;
                                const double in = x[i];
                                const double o = (a0.b0 + f * db0) * in + z1;
                                z1 = (a0.b1 + f * db1) * in - (a0.a1 + f * da1) * o + z2;
                                z2 = (a0.b2 + f * db2) * in - (a0.a2 + f * da2) * o;
                                x[i] = o;
                            }
                        }
                        else
                        for (int i = 0; i < n; ++i)
                        {
                            const double in = x[i];
                            const double o = c.b0 * in + z1;
                            z1 = c.b1 * in - c.a1 * o + z2;
                            z2 = c.b2 * in - c.a2 * o;
                            x[i] = o;
                        }
                        if (! std::isfinite (z1) || ! std::isfinite (z2)) { z1 = z2 = 0.0; for (int i = 0; i < n; ++i) x[i] = buf[i]; }
                        v.z[ch][s][0] = z1; v.z[ch][s][1] = z2;
                    }
                    // fosa: so original + pres · (filtrat - original)
                    double p = v.pres;
                    const double pas = obj > p ? pasFosa : (obj < p ? -pasFosa : 0.0);
                    for (int i = 0; i < n; ++i)
                    {
                        if (pas != 0.0) p = pas > 0.0 ? std::min (obj, p + pas) : std::max (obj, p + pas);
                        const double e = p * p * (3.0 - 2.0 * p);   // fosa en S (sense arestes)
                        y[i] = p >= 1.0 ? x[i] : buf[i] + e * (x[i] - buf[i]);
                    }
                    for (int i = 0; i < n; ++i) buf[i] = y[i];
                    return p;
                };

                double pFinal = v.pres;
                switch (v.canal)
                {
                    case Esquerre: pFinal = filtra (l, 0); break;
                    case Dret: pFinal = filtra (r, 1); break;
                    case Mid:
                    case Side:
                    {
                        double m[tros], s[tros];
                        for (int i = 0; i < n; ++i) { m[i] = 0.5 * (l[i] + r[i]); s[i] = 0.5 * (l[i] - r[i]); }
                        pFinal = filtra (v.canal == Mid ? m : s, 0);
                        for (int i = 0; i < n; ++i) { l[i] = m[i] + s[i]; r[i] = m[i] - s[i]; }
                        break;
                    }
                    default:
                        filtra (l, 0);
                        pFinal = filtra (r, 1);
                        break;
                }
                v.pres = pFinal;
            }
        }

        // sortida i bypass (amb fosa)
        float picO = 0.0f;
        const bool igual = estalvia || (! algunaBanda && gSort == 1.0 && gObj == 1.0 && wet >= 1.0 && wetObj >= 1.0);
        if (igual)
        {
            if (estalvia) for (auto& v : vi) { v.viva = false; v.pres = 0.0; }
            picO = picI;
        }
        else
        {
            const double kg = 1.0 - std::exp (-1.0 / (0.02 * fs));
            const double pasW = 1.0 / (0.01 * fs);
            for (int i = 0; i < n; ++i)
            {
                gSort += (gObj - gSort) * kg;
                if (std::abs (gObj - gSort) < 1.0e-7) gSort = gObj;
                if (wet != wetObj) wet = wetObj > wet ? std::min (wetObj, wet + pasW) : std::max (wetObj, wet - pasW);
                const double e = wet * wet * (3.0 - 2.0 * wet);
                const double ol = dl[i] + e * (l[i] * gSort - dl[i]);
                const double orr = dr[i] + e * (r[i] * gSort - dr[i]);
                L[i] = (float) ol;
                R[i] = (float) orr;
                picO = std::max (picO, std::max (std::abs (L[i]), std::abs (R[i])));
            }
        }
        if (estalvia) { gSort = gObj; }

        // escolta d'una banda (amb fosa d'entrada i de sortida)
        {
            const int sObj = solo.load (std::memory_order_relaxed);
            const double obj = sObj >= 0 && sObj == soloViu ? 1.0 : 0.0;
            if (soloMix > 0.0 || obj > 0.0)
            {
                const int canal = soloViu >= 0 ? at[(size_t) soloViu].canal.load (std::memory_order_relaxed) : Estereo;
                const double pasS = 1.0 / (0.015 * fs);
                const auto& c = soloS;
                auto biquad = [&c] (double in, double* z)
                {
                    const double o = c.b0 * in + z[0];
                    z[0] = c.b1 * in - c.a1 * o + z[1];
                    z[1] = c.b2 * in - c.a2 * o;
                    return o;
                };
                picO = 0.0f;
                for (int i = 0; i < n; ++i)
                {
                    const double l0 = L[i], r0 = R[i];
                    double sl, sr;
                    switch (canal)
                    {
                        case Esquerre: sl = sr = biquad (l0, soloZ[0]); break;
                        case Dret: sl = sr = biquad (r0, soloZ[0]); break;
                        case Mid: sl = sr = biquad (0.5 * (l0 + r0), soloZ[0]); break;
                        case Side: { const double x = biquad (0.5 * (l0 - r0), soloZ[0]); sl = x; sr = -x; break; }
                        default: sl = biquad (l0, soloZ[0]); sr = biquad (r0, soloZ[1]); break;
                    }
                    if (soloMix != obj) soloMix = obj > soloMix ? std::min (obj, soloMix + pasS) : std::max (obj, soloMix - pasS);
                    const double e = soloMix * soloMix * (3.0 - 2.0 * soloMix);
                    L[i] = (float) (l0 + e * (sl - l0));
                    R[i] = (float) (r0 + e * (sr - r0));
                    picO = std::max (picO, std::max (std::abs (L[i]), std::abs (R[i])));
                }
                for (auto& z : soloZ) for (auto& x : z) if (! std::isfinite (x)) z[0] = z[1] = 0.0;
                if (soloMix <= 0.0 && obj <= 0.0) { soloViu = -1; for (auto& z : soloZ) z[0] = z[1] = 0.0; }
            }
        }

        // analitzador: entrada i sortida (mitjana de L i R)
        if (captura.load (std::memory_order_relaxed))
            for (int i = 0; i < n; ++i)
            {
                anellIn.escriuUna ((float) (0.5 * (dl[i] + dr[i])));
                anellOut.escriuUna (0.5f * (L[i] + R[i]));
            }

        if (picI > picIn.load (std::memory_order_relaxed)) picIn.store (picI, std::memory_order_relaxed);
        if (picO > picOut.load (std::memory_order_relaxed)) picOut.store (picO, std::memory_order_relaxed);
    }

    void Equalitzador::processa (float* L, float* R, int n) noexcept
    {
        if (n <= 0 || L == nullptr || R == nullptr) return;
#ifdef MIBASE_EQ_FTZ
        const unsigned int csr = _mm_getcsr();
        _mm_setcsr (csr | 0x8040);   // flush-to-zero i denormals-are-zero
#endif
        int i = 0;
        while (i < n)
        {
            if (dinsTros <= 0) { actualitza(); dinsTros = tros; }
            const int m = std::min (dinsTros, n - i);
            tros16 (L + i, R + i, m, tros - dinsTros, scL != nullptr ? scL + i : nullptr, scR != nullptr ? scR + i : nullptr);
            dinsTros -= m;
            i += m;
        }
#ifdef MIBASE_EQ_FTZ
        _mm_setcsr (csr);
#endif
        scConnectat.store (scL != nullptr, std::memory_order_relaxed);
        scL = scR = nullptr;   // el sidechain val per a aquest bloc
    }
}
