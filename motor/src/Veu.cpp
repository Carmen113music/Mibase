#include "mibase/Veu.h"
#include "mibase/Harmonia.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <numeric>

namespace mibase::veu
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;
        constexpr double fsAnalisi = 16000.0;
        constexpr int saltA = 80;           // 5 ms a 16 kHz
        constexpr int finestraA = 512;      // 32 ms
        constexpr int midaFFT = 1024;
        constexpr double fMin = 55.0, fMax = 1100.0;
        constexpr double midiMin = 33.0;    // ~55 Hz
        constexpr double ampleBin = 0.2;    // semitons per casella del camí
        constexpr int nBins = 265;          // fins a ~MIDI 86
        constexpr int salt = 13;            // caselles que pot saltar el camí d'un punt al següent
        const float nan = std::numeric_limits<float>::quiet_NaN();

        using cpx = std::complex<double>;

        void fft (std::vector<cpx>& a, bool inversa)
        {
            const size_t n = a.size();
            for (size_t i = 1, j = 0; i < n; ++i)
            {
                size_t bit = n >> 1;
                for (; j & bit; bit >>= 1) j ^= bit;
                j ^= bit;
                if (i < j) std::swap (a[i], a[j]);
            }
            for (size_t len = 2; len <= n; len <<= 1)
            {
                const double ang = 2.0 * pi / (double) len * (inversa ? 1.0 : -1.0);
                const cpx wl (std::cos (ang), std::sin (ang));
                for (size_t i = 0; i < n; i += len)
                {
                    cpx w (1.0, 0.0);
                    for (size_t j = 0; j < len / 2; ++j)
                    {
                        const cpx u = a[i + j], v = a[i + j + len / 2] * w;
                        a[i + j] = u + v;
                        a[i + j + len / 2] = u - v;
                        w *= wl;
                    }
                }
            }
            if (inversa)
                for (auto& x : a) x /= (double) n;
        }

        /** Passa el senyal a 16 kHz (filtre sinc amb finestra). */
        std::vector<float> a16k (const float* x, long long n, double fs)
        {
            const double rati = fs / fsAnalisi;
            const long long m = (long long) std::floor ((double) n / rati);
            std::vector<float> y ((size_t) std::max (0LL, m));
            if (std::abs (rati - 1.0) < 1e-9) { std::copy (x, x + m, y.begin()); return y; }
            const double fc = std::min (0.45 * fsAnalisi / fs, 0.45);   // cicles per mostra d'entrada
            const int mig = (int) std::ceil (4.0 / fc);
            for (long long k = 0; k < m; ++k)
            {
                const double u = (double) k * rati;
                const long long c = (long long) std::floor (u);
                double s = 0.0, sw = 0.0;
                for (long long j = c - mig + 1; j <= c + mig; ++j)
                {
                    if (j < 0 || j >= n) continue;
                    const double d = u - (double) j;
                    const double arg = 2.0 * fc * d;
                    const double sinc = std::abs (arg) < 1e-9 ? 1.0 : std::sin (pi * arg) / (pi * arg);
                    const double w = 0.5 + 0.5 * std::cos (pi * d / (mig + 1));
                    const double h = sinc * w;
                    s += h * x[j];
                    sw += h;
                }
                y[(size_t) k] = (float) (sw > 1e-9 ? s / sw : 0.0);
            }
            return y;
        }

        struct Candidat { double midi; double p; };

        double betaPdf (double x, double a, double b) { return std::pow (x, a - 1.0) * std::pow (1.0 - x, b - 1.0); }

        /** Mitjana mòbil centrada (ignora els NaN). */
        std::vector<float> suavitza (const std::vector<float>& v, int ample)
        {
            if (ample <= 1) return v;
            const int n = (int) v.size(), m = ample / 2;
            std::vector<double> s ((size_t) n + 1, 0.0), c ((size_t) n + 1, 0.0);
            for (int i = 0; i < n; ++i)
            {
                const bool ok = ! std::isnan (v[(size_t) i]);
                s[(size_t) i + 1] = s[(size_t) i] + (ok ? v[(size_t) i] : 0.0);
                c[(size_t) i + 1] = c[(size_t) i] + (ok ? 1.0 : 0.0);
            }
            std::vector<float> r ((size_t) n);
            for (int i = 0; i < n; ++i)
            {
                const int a = std::max (0, i - m), b = std::min (n, i + m + 1);
                const double k = c[(size_t) b] - c[(size_t) a];
                r[(size_t) i] = std::isnan (v[(size_t) i]) || k <= 0.0 ? v[(size_t) i] : (float) ((s[(size_t) b] - s[(size_t) a]) / k);
            }
            return r;
        }

        std::vector<float> mediana5 (const std::vector<float>& v)
        {
            std::vector<float> r = v;
            const int n = (int) v.size();
            for (int i = 0; i < n; ++i)
            {
                float w[5]; int k = 0;
                for (int j = std::max (0, i - 2); j <= std::min (n - 1, i + 2); ++j) w[k++] = v[(size_t) j];
                std::nth_element (w, w + k / 2, w + k);
                r[(size_t) i] = w[k / 2];
            }
            return r;
        }

        /** Treu el vibrato d'un tros amb to (sense NaN): on es detecta una oscil·lació de 3,5–9 Hz, fa la mitjana
            d'un període sencer. Torna la corba «sense vibrato». */
        std::vector<float> senseVibrato (const std::vector<float>& q)
        {
            const int n = (int) q.size();
            std::vector<float> c = q;
            if (n < 50) return c;
            constexpr int mig = 40, lMin = 22, lMax = 57;
            std::vector<int> periode ((size_t) n, 0);
            std::vector<double> e;
            for (int i = 0; i < n; i += 2)
            {
                // Finestra de 80 punts (a les vores, encabida dins el tros).
                const int a = n >= 2 * mig ? std::clamp (i - mig, 0, n - 2 * mig) : 0, b = std::min (n, a + 2 * mig);
                const int L = b - a;
                if (L < 50) continue;
                // Treu la tendència (recta).
                double sx = 0, sy = 0, sxx = 0, sxy = 0;
                for (int j = a; j < b; ++j) { const double x = j - a, y = q[(size_t) j]; sx += x; sy += y; sxx += x * x; sxy += x * y; }
                const double den = L * sxx - sx * sx;
                const double m = den != 0.0 ? (L * sxy - sx * sy) / den : 0.0, k0 = (sy - m * sx) / L;
                e.assign ((size_t) L, 0.0);
                double en = 0.0;
                for (int j = 0; j < L; ++j) { e[(size_t) j] = q[(size_t) (a + j)] - (k0 + m * j); en += e[(size_t) j] * e[(size_t) j]; }
                const double rmsE = std::sqrt (en / L);
                if (rmsE < 0.1) continue;
                // El vibrato oscil·la al voltant d'un centre: si la tendència és gran, és una escala de notes.
                if (std::abs (m) * L > 2.5 * rmsE) continue;
                // I té forma de sinus: una oscil·lació «quadrada» (notes que es repeteixen, trinat) no ho és.
                double pk = 0.0; for (auto v : e) pk = std::max (pk, std::abs (v));
                if (pk > 0.0 && rmsE / pk > 0.82) continue;
                auto corr = [&] (int l)
                {
                    double s = 0, s1 = 0, s2 = 0;
                    for (int j = 0; j + l < L; ++j) { s += e[(size_t) j] * e[(size_t) (j + l)]; s1 += e[(size_t) j] * e[(size_t) j]; s2 += e[(size_t) (j + l)] * e[(size_t) (j + l)]; }
                    return s1 > 0 && s2 > 0 ? s / std::sqrt (s1 * s2) : 0.0;
                };
                int millor = 0; double rMillor = 0.5;
                for (int l = lMin; l <= std::min (lMax, L - 20); ++l)
                    if (const double r = corr (l); r > rMillor) { rMillor = r; millor = l; }
                if (millor > 0 && corr (millor / 2) < -0.2)
                {
                    periode[(size_t) i] = millor;
                    if (i + 1 < n) periode[(size_t) i + 1] = millor;
                }
            }
            for (int i = 0; i < n; ++i)
            {
                const int p = periode[(size_t) i];
                if (p == 0) continue;
                const int a = std::max (0, i - p / 2), b = std::min (n, a + p);
                double s = 0; for (int j = a; j < b; ++j) s += q[(size_t) j];
                c[(size_t) i] = (float) (s / (b - a));
            }
            return c;
        }

        /** Segmenta una corba (sense NaN) en trossos que s'ajusten bé a una recta (una nota pot tenir deriva lenta):
            minimitza l'error + «lambda» per nota. Torna els començaments. */
        std::vector<int> segmenta (const std::vector<float>& y, double lambda, int minim)
        {
            const int n = (int) y.size();
            if (n < 2 * minim) return { 0 };
            std::vector<double> S1 ((size_t) n + 1, 0), S2 ((size_t) n + 1, 0), Y ((size_t) n + 1, 0), Y2 ((size_t) n + 1, 0), XY ((size_t) n + 1, 0);
            for (int i = 0; i < n; ++i)
            {
                const double x = i, v = y[(size_t) i];
                S1[(size_t) i + 1] = S1[(size_t) i] + x; S2[(size_t) i + 1] = S2[(size_t) i] + x * x;
                Y[(size_t) i + 1] = Y[(size_t) i] + v; Y2[(size_t) i + 1] = Y2[(size_t) i] + v * v; XY[(size_t) i + 1] = XY[(size_t) i] + x * v;
            }
            auto cost = [&] (int a, int b)
            {
                const double k = b - a;
                const double sx = S1[(size_t) b] - S1[(size_t) a], sxx = S2[(size_t) b] - S2[(size_t) a];
                const double sy = Y[(size_t) b] - Y[(size_t) a], syy = Y2[(size_t) b] - Y2[(size_t) a], sxy = XY[(size_t) b] - XY[(size_t) a];
                const double vx = sxx - sx * sx / k, cv = sxy - sx * sy / k;
                double r = syy - sy * sy / k - (vx > 1e-12 ? cv * cv / vx : 0.0);
                return std::max (0.0, r);
            };
            const double inf = std::numeric_limits<double>::infinity();
            std::vector<double> best ((size_t) n + 1, inf);
            std::vector<int> d ((size_t) n + 1, 0);
            best[0] = 0.0;
            constexpr int maxNota = 2000;   // 10 s
            for (int j = minim; j <= n; ++j)
                for (int i = std::max (0, j - maxNota); i <= j - minim; ++i)
                {
                    if (best[(size_t) i] == inf) continue;
                    const double v = best[(size_t) i] + cost (i, j) + lambda;
                    if (v < best[(size_t) j]) { best[(size_t) j] = v; d[(size_t) j] = i; }
                }
            std::vector<int> inicis;
            for (int j = n; j > 0; j = d[(size_t) j]) inicis.push_back (d[(size_t) j]);
            std::reverse (inicis.begin(), inicis.end());
            if (inicis.empty()) inicis.push_back (0);
            return inicis;
        }

        float interpola (const std::vector<float>& v, double u)
        {
            if (v.empty()) return 0.0f;
            const int n = (int) v.size();
            if (u <= 0.0) return v[0];
            const int i = (int) std::floor (u);
            if (i >= n - 1) return v[(size_t) n - 1];
            const double f = u - i;
            return (float) (v[(size_t) i] * (1.0 - f) + v[(size_t) i + 1] * f);
        }
    }

    double midiDeHz (double hz) noexcept { return 69.0 + 12.0 * std::log2 (hz / 440.0); }
    double hzDeMidi (double midi) noexcept { return 440.0 * std::pow (2.0, (midi - 69.0) / 12.0); }

    //==============================================================================
    Analisi analitza (const float* x, long long n, double fs, const std::function<bool (float)>& progres)
    {
        Analisi r;
        r.fs = fs;
        r.mostres = n;
        if (x == nullptr || n <= 0 || fs <= 0.0) return r;
        const auto y = a16k (x, n, fs);
        const int m = (int) y.size();
        const int punts = (int) std::ceil ((double) n / fs / pas);
        const int maxLag = (int) std::ceil (fsAnalisi / fMin), minLag = (int) std::floor (fsAnalisi / fMax);
        const int mig = (finestraA + maxLag) / 2;

        // Llindars de YIN amb probabilitats (distribució beta amb mitjana 0,2).
        constexpr int nLl = 100;
        double prior[nLl]; double sp = 0.0;
        for (int k = 0; k < nLl; ++k) { prior[k] = betaPdf ((k + 1) * 0.01 - 0.005, 2.0, 8.0); sp += prior[k]; }
        for (auto& p : prior) p /= sp;

        std::vector<std::vector<Candidat>> cands ((size_t) punts);
        r.rms.assign ((size_t) punts, 0.0f);
        std::vector<cpx> A (midaFFT), B (midaFFT);
        std::vector<double> b2 ((size_t) finestraA + maxLag + 1), d ((size_t) maxLag + 1), dn ((size_t) maxLag + 1);
        for (int i = 0; i < punts; ++i)
        {
            if (progres && (i % 400) == 0 && ! progres (0.8f * (float) i / (float) punts)) return Analisi { fs, n, {}, {}, {} };
            const int s = i * saltA - mig;
            auto val = [&] (int k) { const int j = s + k; return j >= 0 && j < m ? (double) y[(size_t) j] : 0.0; };
            double e1 = 0.0;
            for (int k = 0; k < midaFFT; ++k)
            {
                const double v = k < finestraA + maxLag ? val (k) : 0.0;
                B[(size_t) k] = v;
                A[(size_t) k] = k < finestraA ? v : 0.0;
                if (k < finestraA) e1 += v * v;
            }
            r.rms[(size_t) i] = (float) std::sqrt (e1 / finestraA);
            if (e1 < 1e-9) continue;
            fft (A, false); fft (B, false);
            for (int k = 0; k < midaFFT; ++k) A[(size_t) k] = std::conj (A[(size_t) k]) * B[(size_t) k];
            fft (A, true);
            b2[0] = 0.0;
            for (int k = 0; k < finestraA + maxLag; ++k) { const double v = val (k); b2[(size_t) k + 1] = b2[(size_t) k] + v * v; }
            double acum = 0.0;
            dn[0] = 1.0;
            for (int t = 1; t <= maxLag; ++t)
            {
                const double e2 = b2[(size_t) (t + finestraA)] - b2[(size_t) t];
                d[(size_t) t] = std::max (0.0, e1 + e2 - 2.0 * A[(size_t) t].real());
                acum += d[(size_t) t];
                dn[(size_t) t] = acum > 0.0 ? d[(size_t) t] * t / acum : 1.0;
            }
            auto freqDe = [&] (int t)
            {
                double tt = t;
                if (t > 1 && t < maxLag)
                {
                    const double a = dn[(size_t) t - 1], b = dn[(size_t) t], c = dn[(size_t) t + 1], den = a - 2.0 * b + c;
                    if (std::abs (den) > 1e-12) tt = t + 0.5 * (a - c) / den;
                }
                return fsAnalisi / tt;
            };
            int tg = minLag;
            for (int t = minLag; t < maxLag; ++t) if (dn[(size_t) t] < dn[(size_t) tg]) tg = t;
            std::vector<std::pair<int, double>> pt;
            auto afegeix = [&] (int t, double p) { for (auto& x2 : pt) if (x2.first == t) { x2.second += p; return; } pt.push_back ({ t, p }); };
            for (int k = 0; k < nLl; ++k)
            {
                const double llindar = (k + 1) * 0.01;
                int t = minLag;
                while (t < maxLag && dn[(size_t) t] >= llindar) ++t;
                if (t < maxLag)
                {
                    while (t + 1 < maxLag && dn[(size_t) t + 1] < dn[(size_t) t]) ++t;
                    afegeix (t, prior[k]);
                }
                else afegeix (tg, prior[k] * 0.01);
            }
            // Si un múltiple del període encaixa molt millor, el període de debò és aquell (quan un harmònic parell
            // domina, YIN troba la meitat del període: una octava massa amunt).
            auto periodeReal = [&] (int t)
            {
                for (bool canvi = true; canvi;)
                {
                    canvi = false;
                    for (int k = 2; k <= 6; ++k)
                    {
                        const int c = k * t;
                        if (c + 3 >= maxLag) break;
                        int mm = c;
                        for (int u = c - 3; u <= c + 3; ++u) if (dn[(size_t) u] < dn[(size_t) mm]) mm = u;
                        if (dn[(size_t) mm] < 0.5 * dn[(size_t) t] && dn[(size_t) t] > 0.04) { t = mm; canvi = true; break; }
                    }
                }
                return t;
            };
            for (auto& [t, p] : pt)
            {
                const double midi = midiDeHz (freqDe (periodeReal (t)));
                bool fet = false;
                for (auto& c : cands[(size_t) i]) if (std::abs (c.midi - midi) < 0.05) { c.p += p; fet = true; }
                if (! fet) cands[(size_t) i].push_back ({ midi, p });
            }
        }

        // Silencis i sons molt fluixos: sense to.
        float maxRms = 0.0f;
        for (auto v : r.rms) maxRms = std::max (maxRms, v);
        for (int i = 0; i < punts; ++i)
        {
            const double rel = 20.0 * std::log10 (std::max (1e-9f, r.rms[(size_t) i]) / std::max (1e-9f, maxRms));
            const double factor = r.rms[(size_t) i] < 1e-4f ? 0.0 : std::clamp ((rel + 45.0) / 10.0, 0.0, 1.0);
            for (auto& c : cands[(size_t) i]) c.p *= factor;
        }

        // Camí més probable (Viterbi): estats amb to (una casella per cada 0,2 semitons) i sense to.
        double lt[2 * salt + 1]; double st = 0.0;
        for (int k = -salt; k <= salt; ++k) { lt[k + salt] = (double) (salt + 1 - std::abs (k)); st += lt[k + salt]; }
        for (auto& v : lt) v = std::log (v / st);
        const double lQueda = std::log (0.99), lCanvia = std::log (0.01);
        std::vector<double> V ((size_t) nBins, 0.0), U ((size_t) nBins, 0.0), V2 ((size_t) nBins), U2 ((size_t) nBins);
        std::vector<std::int8_t> enrere ((size_t) punts * 2 * nBins, 0);
        std::vector<double> oV ((size_t) nBins);
        for (int i = 0; i < punts; ++i)
        {
            if (progres && (i % 2000) == 0 && ! progres (0.8f + 0.2f * (float) i / (float) punts)) return Analisi { fs, n, {}, {}, {} };
            double pv = 0.0;
            std::fill (oV.begin(), oV.end(), 0.0);
            for (auto& c : cands[(size_t) i])
            {
                const int b = std::clamp ((int) std::lround ((c.midi - midiMin) / ampleBin), 0, nBins - 1);
                oV[(size_t) b] += c.p;
                pv += c.p;
            }
            pv = std::min (pv, 1.0);
            const double lU = std::log (std::max (1e-12, 0.5 * (1.0 - pv)));
            for (int b = 0; b < nBins; ++b)
            {
                double mv = -1e300, mu = -1e300; int bv = 0, bu = 0;
                for (int k = -salt; k <= salt; ++k)
                {
                    const int a = b + k;
                    if (a < 0 || a >= nBins) continue;
                    const double l = lt[k + salt];
                    if (const double v = V[(size_t) a] + l + lQueda; v > mv) { mv = v; bv = k + salt; }
                    if (const double v = U[(size_t) a] + l + lCanvia; v > mv) { mv = v; bv = k + salt + 32; }
                    if (const double v = U[(size_t) a] + l + lQueda; v > mu) { mu = v; bu = k + salt + 32; }
                    if (const double v = V[(size_t) a] + l + lCanvia; v > mu) { mu = v; bu = k + salt; }
                }
                V2[(size_t) b] = mv + std::log (std::max (1e-12, oV[(size_t) b]));
                U2[(size_t) b] = mu + lU;
                enrere[((size_t) i * 2 + 0) * nBins + (size_t) b] = (std::int8_t) bv;   // de V: +32 si venia de U
                enrere[((size_t) i * 2 + 1) * nBins + (size_t) b] = (std::int8_t) bu;   // de U: +32 si venia de U
            }
            double mx = -1e300;
            for (int b = 0; b < nBins; ++b) mx = std::max ({ mx, V2[(size_t) b], U2[(size_t) b] });
            for (int b = 0; b < nBins; ++b) { V[(size_t) b] = V2[(size_t) b] - mx; U[(size_t) b] = U2[(size_t) b] - mx; }
        }
        r.f0.assign ((size_t) punts, 0.0f);
        r.prob.assign ((size_t) punts, 0.0f);
        int b = 0; bool sonor = false; double mx = -1e300;
        for (int k = 0; k < nBins; ++k)
        {
            if (V[(size_t) k] > mx) { mx = V[(size_t) k]; b = k; sonor = true; }
            if (U[(size_t) k] > mx) { mx = U[(size_t) k]; b = k; sonor = false; }
        }
        for (int i = punts - 1; i >= 0; --i)
        {
            double pv = 0.0;
            for (auto& c : cands[(size_t) i]) pv += c.p;
            r.prob[(size_t) i] = (float) std::min (1.0, pv);
            if (sonor)
            {
                // L'afinació fina: el candidat més proper a la casella del camí.
                const double centreBin = midiMin + b * ampleBin;
                double millor = centreBin, dist = 1e9;
                for (auto& c : cands[(size_t) i])
                    if (std::abs (c.midi - centreBin) < dist) { dist = std::abs (c.midi - centreBin); millor = c.midi; }
                if (dist > 1.0) millor = centreBin;
                r.f0[(size_t) i] = (float) hzDeMidi (millor);
            }
            const int e = enrere[((size_t) i * 2 + (sonor ? 0 : 1)) * nBins + (size_t) b];
            const bool deU = e >= 32;
            b += (e & 31) - salt;
            b = std::clamp (b, 0, nBins - 1);
            sonor = ! deU;
        }
        // Trossos amb to massa curts (menys de 30 ms): sense to.
        for (int i = 0; i < punts;)
        {
            if (r.f0[(size_t) i] <= 0.0f) { ++i; continue; }
            int j = i; while (j < punts && r.f0[(size_t) j] > 0.0f) ++j;
            if (j - i < 6) for (int k = i; k < j; ++k) r.f0[(size_t) k] = 0.0f;
            i = j;
        }
        if (progres) progres (1.0f);
        return r;
    }

    //==============================================================================
    bool desaAnalisi (const Analisi& a, const std::filesystem::path& cami)
    {
        std::ofstream f (cami, std::ios::binary | std::ios::trunc);
        if (! f) return false;
        const char cap[8] = { 'M', 'B', 'V', 'E', 'U', '0', '0', '1' };
        f.write (cap, 8);
        const std::int64_t mostres = a.mostres; const std::int32_t punts = a.punts();
        f.write (reinterpret_cast<const char*> (&a.fs), sizeof (double));
        f.write (reinterpret_cast<const char*> (&mostres), sizeof (mostres));
        f.write (reinterpret_cast<const char*> (&punts), sizeof (punts));
        for (auto* v : { &a.f0, &a.prob, &a.rms })
            f.write (reinterpret_cast<const char*> (v->data()), (std::streamsize) (v->size() * sizeof (float)));
        return (bool) f;
    }

    bool llegeixAnalisi (const std::filesystem::path& cami, Analisi& a)
    {
        std::ifstream f (cami, std::ios::binary);
        if (! f) return false;
        char cap[8];
        f.read (cap, 8);
        if (! f || std::memcmp (cap, "MBVEU001", 8) != 0) return false;
        std::int64_t mostres = 0; std::int32_t punts = 0; double fs = 0.0;
        f.read (reinterpret_cast<char*> (&fs), sizeof (double));
        f.read (reinterpret_cast<char*> (&mostres), sizeof (mostres));
        f.read (reinterpret_cast<char*> (&punts), sizeof (punts));
        if (! f || punts < 0 || punts > 50'000'000) return false;
        Analisi r; r.fs = fs; r.mostres = mostres;
        for (auto* v : { &r.f0, &r.prob, &r.rms })
        {
            v->resize ((size_t) punts);
            f.read (reinterpret_cast<char*> (v->data()), (std::streamsize) ((size_t) punts * sizeof (float)));
        }
        if (! f) return false;
        a = std::move (r);
        return true;
    }

    //==============================================================================
    std::vector<NotaVeu> detectaNotes (const Analisi& a)
    {
        std::vector<NotaVeu> notes;
        const int n = a.punts();
        int i = 0;
        while (i < n)
        {
            if (a.f0[(size_t) i] <= 0.0f) { ++i; continue; }
            int j = i; while (j < n && a.f0[(size_t) j] > 0.0f) ++j;
            if (j - i >= 8)
            {
                std::vector<float> p;
                for (int k = i; k < j; ++k) p.push_back ((float) midiDeHz (a.f0[(size_t) k]));
                const auto q = mediana5 (p);
                const auto c = senseVibrato (q);
                auto inicis = segmenta (c, 1.2, 10);
                inicis.push_back (j - i);
                // Lliscaments curts (de nota a nota) que han quedat sols: es reparteixen entre les notes del costat.
                for (size_t s = 1; s + 1 < inicis.size();)
                {
                    const int a0 = inicis[s - 1], a1 = inicis[s], a2 = inicis[s + 1];
                    (void) a0;
                    const int len = a2 - a1;
                    const double pendent = (c[(size_t) a2 - 1] - c[(size_t) a1]) / std::max (1, len - 1) / pas;
                    if (len < 24 && std::abs (pendent) > 6.0 && s + 2 < inicis.size())
                    {
                        inicis[s] = a1 + len / 2;            // la primera meitat, amb la d'abans
                        inicis.erase (inicis.begin() + (long) s + 1);   // la segona, amb la de després
                        continue;
                    }
                    ++s;
                }
                // Un tros curt (menys de 100 ms) a la vora (l'atac o la caiguda) va amb la nota del costat.
                auto mitjanaTros = [&] (int x0, int x1) { double t = 0; for (int k = x0; k < x1; ++k) t += c[(size_t) k]; return t / std::max (1, x1 - x0); };
                while (inicis.size() > 2 && inicis[1] - inicis[0] < 20
                       && std::abs (mitjanaTros (inicis[0], inicis[1]) - mitjanaTros (inicis[1], inicis[2])) < 3.0)
                    inicis.erase (inicis.begin() + 1);
                while (inicis.size() > 2 && inicis[inicis.size() - 1] - inicis[inicis.size() - 2] < 20
                       && std::abs (mitjanaTros (inicis[inicis.size() - 2], inicis[inicis.size() - 1]) - mitjanaTros (inicis[inicis.size() - 3], inicis[inicis.size() - 2])) < 3.0)
                    inicis.erase (inicis.end() - 2);
                for (size_t s = 0; s + 1 < inicis.size(); ++s)
                {
                    const int a0 = inicis[s], a1 = inicis[s + 1];
                    NotaVeu nv;
                    nv.ini = std::max (0.0, (i + a0 - 0.5) * pas);
                    nv.fi = (i + a1 - 0.5) * pas;
                    // Centre: mitjana ponderada pel volum, sense els extrems (on hi ha les transicions).
                    const int len = a1 - a0, retall = len / 5;
                    double s1 = 0.0, sw = 0.0;
                    for (int k = a0 + retall; k < a1 - retall; ++k)
                    {
                        const double w = std::max (1e-6f, a.rms[(size_t) (i + k)]);
                        s1 += w * c[(size_t) k]; sw += w;
                    }
                    nv.centre = sw > 0.0 ? s1 / sw : c[(size_t) a0];
                    notes.push_back (nv);
                }
            }
            i = j;
        }
        // Una nota tallada per una caiguda curta del to: s'ajunten si tenen el mateix centre.
        for (size_t k = 1; k < notes.size();)
        {
            auto& x = notes[k - 1]; const auto& y = notes[k];
            if (y.ini - x.fi < 0.04 && std::abs (y.centre - x.centre) < 0.5)
            {
                const double dx = x.fi - x.ini, dy = y.fi - y.ini;
                x.centre = (x.centre * dx + y.centre * dy) / (dx + dy);
                x.fi = y.fi;
                notes.erase (notes.begin() + (long) k);
            }
            else ++k;
        }
        return notes;
    }

    //==============================================================================
    Corbes corbes (const Analisi& a, const VeuPresa& v)
    {
        Corbes c;
        const int n = a.punts();
        c.original.assign ((size_t) n, nan);
        c.nota.assign ((size_t) n, -1);
        for (int i = 0; i < n; ++i)
            if (a.f0[(size_t) i] > 0.0f) c.original[(size_t) i] = (float) midiDeHz (a.f0[(size_t) i]);
        for (int k = 0; k < (int) v.notes.size(); ++k)
        {
            const auto& nv = v.notes[(size_t) k];
            const int i0 = std::max (0, (int) std::ceil (nv.ini / pas - 1e-9)), i1 = std::min (n, (int) std::ceil (nv.fi / pas - 1e-9));
            for (int i = i0; i < i1; ++i) c.nota[(size_t) i] = k;
        }
        // Per a cada punt: desplaçament de la nota (es suavitza amb la transició), la resta (deriva i vibrato) i formant.
        std::vector<float> D ((size_t) n, 0.0f), R ((size_t) n, 0.0f), F ((size_t) n, 0.0f), G ((size_t) n, 0.0f);
        for (int k = 0; k < (int) v.notes.size(); ++k)
        {
            const auto& nv = v.notes[(size_t) k];
            std::vector<int> idx;
            for (int i = 0; i < n; ++i) if (c.nota[(size_t) i] == k) idx.push_back (i);
            if (idx.empty()) continue;
            std::vector<float> o; for (int i : idx) o.push_back (c.original[(size_t) i]);
            const auto L = suavitza (suavitza (o, 41), 41);
            for (size_t t = 0; t < idx.size(); ++t)
            {
                const int i = idx[t];
                if (! std::isnan (o[t]))
                    R[(size_t) i] = (float) ((nv.deriva - 1.0) * (L[t] - nv.centre) + (nv.modulacio - 1.0) * (o[t] - L[t]));
            }
        }
        // Desplaçament, formant i guany de les notes; fora de les notes, el de la nota més propera (el guany, 0 dB).
        int ultima = -1;
        std::vector<int> mes ((size_t) n, -1);
        for (int i = 0; i < n; ++i) { if (c.nota[(size_t) i] >= 0) ultima = c.nota[(size_t) i]; mes[(size_t) i] = ultima; }
        int seguent = -1;
        for (int i = n - 1; i >= 0; --i)
        {
            if (c.nota[(size_t) i] >= 0) seguent = c.nota[(size_t) i];
            int k = c.nota[(size_t) i];
            if (k < 0) k = mes[(size_t) i] >= 0 ? mes[(size_t) i] : seguent;
            if (k < 0) continue;
            const auto& nv = v.notes[(size_t) k];
            D[(size_t) i] = (float) nv.desplac;
            F[(size_t) i] = (float) nv.formant;
            G[(size_t) i] = c.nota[(size_t) i] >= 0 ? (float) nv.guanyDb : 0.0f;
        }
        const int T = std::max (1, (int) std::lround (v.transicio / pas)) | 1;
        const auto Ds = suavitza (D, T), Fs = suavitza (F, T), Gs = suavitza (G, 5), Rs = suavitza (R, 3);
        c.canvi.resize ((size_t) n); c.formant.resize ((size_t) n); c.guany.resize ((size_t) n); c.editada.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            c.canvi[(size_t) i] = Ds[(size_t) i] + Rs[(size_t) i];
            c.formant[(size_t) i] = Fs[(size_t) i];
            c.guany[(size_t) i] = (float) std::pow (10.0, Gs[(size_t) i] / 20.0);
            c.editada[(size_t) i] = std::isnan (c.original[(size_t) i]) ? nan : c.original[(size_t) i] + c.canvi[(size_t) i];
        }
        return c;
    }

    //==============================================================================
    namespace
    {
        bool puntAmbCanvi (const Corbes& c, int i)
        {
            const bool sonor = ! std::isnan (c.original[(size_t) i]);
            return (sonor && (std::abs (c.canvi[(size_t) i]) > 0.003f || std::abs (c.formant[(size_t) i]) > 0.01f))
                || std::abs (c.guany[(size_t) i] - 1.0f) > 1e-4f;
        }

        std::vector<std::pair<int, int>> rangs (const std::vector<char>& marca, int marge, int ajunta)
        {
            std::vector<std::pair<int, int>> r;
            const int n = (int) marca.size();
            for (int i = 0; i < n;)
            {
                if (! marca[(size_t) i]) { ++i; continue; }
                int j = i; while (j < n && marca[(size_t) j]) ++j;
                const int a = std::max (0, i - marge), b = std::min (n, j + marge);
                if (! r.empty() && a <= r.back().second + ajunta) r.back().second = b;
                else r.push_back ({ a, b });
                i = j;
            }
            return r;
        }

        Tros aMostres (const Analisi& a, std::pair<int, int> r)
        {
            return { std::max (0LL, (long long) std::floor (r.first * pas * a.fs)), std::min (a.mostres, (long long) std::ceil (r.second * pas * a.fs)) };
        }
    }

    std::vector<Tros> trossosAmbCanvis (const Analisi& a, const Corbes& c)
    {
        const int n = std::min (a.punts(), (int) c.canvi.size());
        std::vector<char> m ((size_t) n, 0);
        for (int i = 0; i < n; ++i) m[(size_t) i] = puntAmbCanvi (c, i);
        std::vector<Tros> t;
        for (auto r : rangs (m, 12, 20)) t.push_back (aMostres (a, r));
        return t;
    }

    std::vector<Tros> trossosPerRefer (const Analisi& a, const Corbes& abans, const Corbes& ara)
    {
        const int n = std::min (a.punts(), (int) ara.canvi.size());
        const bool tot = (int) abans.canvi.size() != n;
        std::vector<char> m ((size_t) n, 0);
        for (int i = 0; i < n; ++i)
        {
            if (tot) { m[(size_t) i] = puntAmbCanvi (ara, i); continue; }
            m[(size_t) i] = std::abs (abans.canvi[(size_t) i] - ara.canvi[(size_t) i]) > 1e-5f
                         || std::abs (abans.formant[(size_t) i] - ara.formant[(size_t) i]) > 1e-5f
                         || std::abs (abans.guany[(size_t) i] - ara.guany[(size_t) i]) > 1e-6f;
        }
        auto r = rangs (m, 12, 20);
        // Cada tros s'ha d'estendre als trossos amb canvis (d'abans i d'ara) que toca: s'han de refer sencers.
        std::vector<std::pair<int, int>> amb;
        for (const auto* c : { &abans, &ara })
        {
            if ((int) c->canvi.size() != n) continue;
            std::vector<char> mm ((size_t) n, 0);
            for (int i = 0; i < n; ++i) mm[(size_t) i] = puntAmbCanvi (*c, i);
            for (auto x : rangs (mm, 12, 20)) amb.push_back (x);
        }
        for (bool canvi = true; canvi;)
        {
            canvi = false;
            for (auto& x : r)
                for (auto& y : amb)
                    if (y.first < x.second && y.second > x.first && (y.first < x.first || y.second > x.second))
                    {
                        x.first = std::min (x.first, y.first); x.second = std::max (x.second, y.second); canvi = true;
                    }
            std::sort (r.begin(), r.end());
            for (size_t k = 1; k < r.size();)
                if (r[k].first <= r[k - 1].second) { r[k - 1].second = std::max (r[k - 1].second, r[k].second); r.erase (r.begin() + (long) k); canvi = true; }
                else ++k;
        }
        std::vector<Tros> t;
        for (auto x : r) t.push_back (aMostres (a, x));
        return t;
    }

    //==============================================================================
    namespace
    {
        struct Marca { double pos; double periode; bool sonor; };

        void psola (const float* const* x, int canals, long long n, double fs, const Analisi& a, const Corbes& c,
                    float* const* y, long long de, long long fins)
        {
            const double puntsPerMostra = 1.0 / (pas * fs);
            auto f0a = [&] (double t) -> double
            {
                const double u = t * puntsPerMostra;
                const int i = std::clamp ((int) std::floor (u), 0, a.punts() - 1), j = std::min (i + 1, a.punts() - 1);
                const double fi = a.f0[(size_t) i], fj = a.f0[(size_t) j];
                if (fi > 0.0 && fj > 0.0) { const double f = u - i; return std::exp (std::log (fi) * (1.0 - f) + std::log (fj) * f); }
                const int k = std::clamp ((int) std::lround (u), 0, a.punts() - 1);
                return a.f0[(size_t) k];
            };
            auto corba = [&] (const std::vector<float>& v, double t) { return interpola (v, t * puntsPerMostra); };
            const double Tu = 0.004 * fs;
            const double maxP = fs / fMin;

            // Senyal mono filtrat (per trobar els polsos de cada període).
            const long long a0 = std::max (0LL, de - (long long) (2 * maxP)), a1 = std::min (n, fins + (long long) (2 * maxP));
            std::vector<float> xl ((size_t) (a1 - a0));
            {
                const double k = 1.0 - std::exp (-2.0 * pi * 900.0 / fs);
                double s1 = 0.0, s2 = 0.0;
                for (long long t = a0; t < a1; ++t)
                {
                    double v = 0.0; for (int ch = 0; ch < canals; ++ch) v += x[ch][t];
                    s1 += k * (v - s1); s2 += k * (s1 - s2);
                    xl[(size_t) (t - a0)] = (float) s2;
                }
            }
            auto pic = [&] (double desDe, double finsA)
            {
                long long b0 = std::max (a0, (long long) std::ceil (desDe)), b1 = std::min (a1 - 1, (long long) std::floor (finsA));
                long long millor = b0; float mx = -1e30f;
                for (long long t = b0; t <= b1; ++t) if (xl[(size_t) (t - a0)] > mx) { mx = xl[(size_t) (t - a0)]; millor = t; }
                return (double) millor;
            };

            // Marques d'anàlisi.
            std::vector<Marca> marques;
            double m = (double) de;
            bool sonor = f0a (m) > 0.0;
            while (m < (double) fins + maxP)
            {
                const double f = f0a (m);
                sonor = f > 0.0;
                const double P = sonor ? fs / f : Tu;
                marques.push_back ({ m, P, sonor });
                const double cand = m + P;
                const double fc = f0a (cand);
                double seg;
                if (sonor && fc > 0.0) seg = pic (cand - 0.15 * P, cand + 0.15 * P);
                else if (! sonor && fc > 0.0) seg = pic (cand, cand + fs / fc);
                else seg = cand;
                m = std::max (seg, m + 0.5 * std::min (P, sonor ? P : Tu));
            }
            if (marques.empty()) return;

            // Síntesi: grans finestrats posats al ritme del període nou.
            const long long L = fins - de;
            std::vector<double> acc ((size_t) canals * (size_t) L, 0.0), suma ((size_t) L, 0.0);
            double t = (double) de, ultim = -1e30;
            size_t j = 0;
            auto mostra = [&] (int ch, double p)
            {
                const long long i = (long long) std::floor (p);
                if (i < 0 || i + 1 >= n) return i >= 0 && i < n ? (double) x[ch][i] : 0.0;
                const double f = p - (double) i;
                return x[ch][i] * (1.0 - f) + x[ch][i + 1] * f;
            };
            while (t < (double) fins)
            {
                while (j + 1 < marques.size() && std::abs (marques[j + 1].pos - t) <= std::abs (marques[j].pos - t)) ++j;
                const auto& mk = marques[j];
                const double r = mk.sonor ? std::pow (2.0, corba (c.canvi, mk.pos) / 12.0) : 1.0;
                const double kf = mk.sonor ? std::pow (2.0, corba (c.formant, mk.pos) / 12.0) : 1.0;
                const double g = corba (c.guany, t);
                const bool identitat = std::abs (r - 1.0) < 1e-5 && std::abs (kf - 1.0) < 1e-5;
                double centre = t, pasS = mk.periode / r;
                if (identitat && mk.pos > ultim)
                {
                    centre = mk.pos;
                    pasS = j + 1 < marques.size() ? marques[j + 1].pos - mk.pos : mk.periode;
                }
                const double H = std::max (mk.periode, mk.periode / r);
                const long long i0 = (long long) std::ceil (centre - H), i1 = (long long) std::floor (centre + H);
                for (long long i = std::max (i0, de); i <= std::min (i1, fins - 1); ++i)
                {
                    const double d = (double) i - centre;
                    const double w = 0.5 + 0.5 * std::cos (pi * d / H);
                    suma[(size_t) (i - de)] += w;
                    for (int ch = 0; ch < canals; ++ch)
                        acc[(size_t) ch * (size_t) L + (size_t) (i - de)] += g * w * mostra (ch, mk.pos + d * kf);
                }
                ultim = centre;
                t = centre + std::max (1.0, pasS);
            }
            // Normalitza i fa una fosa curta amb l'original als dos extrems (allà no hi ha canvis).
            const long long fosa = std::min ((long long) (0.004 * fs), L / 4);
            for (long long i = 0; i < L; ++i)
            {
                double mescla = 1.0;
                if (i < fosa) mescla = (double) i / (double) fosa;
                else if (i >= L - fosa) mescla = (double) (L - 1 - i) / (double) fosa;
                for (int ch = 0; ch < canals; ++ch)
                {
                    const double o = x[ch][de + i];
                    const double s = suma[(size_t) i] > 1e-6 ? acc[(size_t) ch * (size_t) L + (size_t) i] / suma[(size_t) i] : o;
                    y[ch][de + i] = (float) (o * (1.0 - mescla) + s * mescla);
                }
            }
        }
    }

    void renderitza (const float* const* entrada, int canals, long long n, double fs, const Analisi& a, const Corbes& c,
                     float* const* sortida, long long de, long long fins)
    {
        de = std::max (0LL, de); fins = std::min (n, fins);
        if (fins <= de || a.buida()) return;
        for (int ch = 0; ch < canals; ++ch) std::copy (entrada[ch] + de, entrada[ch] + fins, sortida[ch] + de);
        for (auto tr : trossosAmbCanvis (a, c))
        {
            const long long d0 = std::max (de, tr.de), d1 = std::min (fins, tr.a);
            if (d1 > d0) psola (entrada, canals, n, fs, a, c, sortida, d0, d1);
        }
    }

    //==============================================================================
    int notaObjectiu (double midi, Desti d, const Tonalitat& t)
    {
        const int r = (int) std::lround (midi);
        if (d == Desti::Cromatic) return r;
        int millor = r; double dist = 1e9;
        for (int k = r - 6; k <= r + 6; ++k)
            if (harmonia::aEscala (k, t) && std::abs (k - midi) < dist) { dist = std::abs (k - midi); millor = k; }
        return millor;
    }

    void corregeix (VeuPresa& v, const std::vector<int>& quines, const Correccio& c)
    {
        auto fes = [&] (NotaVeu& n)
        {
            n.desplac = (notaObjectiu (n.centre, c.desti, c.tonalitat) - n.centre) * std::clamp (c.centre, 0.0, 1.0);
            n.deriva = 1.0 - std::clamp (c.deriva, 0.0, 1.0);
            n.modulacio = std::clamp (c.vibrato, 0.0, 2.0);
        };
        if (quines.empty()) for (auto& n : v.notes) fes (n);
        else for (int k : quines) if (k >= 0 && k < (int) v.notes.size()) fes (v.notes[(size_t) k]);
    }

    bool teCanvis (const VeuPresa& v) noexcept
    {
        for (auto& n : v.notes)
            if (n.desplac != 0.0 || n.modulacio != 1.0 || n.deriva != 1.0 || n.formant != 0.0 || n.guanyDb != 0.0) return true;
        return false;
    }

    void restaura (VeuPresa& v, const std::vector<int>& quines)
    {
        auto fes = [] (NotaVeu& n) { n.desplac = 0.0; n.modulacio = 1.0; n.deriva = 1.0; n.formant = 0.0; n.guanyDb = 0.0; };
        if (quines.empty()) for (auto& n : v.notes) fes (n);
        else for (int k : quines) if (k >= 0 && k < (int) v.notes.size()) fes (v.notes[(size_t) k]);
    }
}
