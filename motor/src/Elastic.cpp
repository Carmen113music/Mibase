#include "mibase/Elastic.h"
#include "mibase/Edicio.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace mibase::elastic
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;
        constexpr double tocant = 1.0e-6;   // dos temps «iguals» (segons)

        /** Que el primer punt (el començament del fitxer) caigui al temps 0 del clip, sense moure res a la línia de temps. */
        void normalitza (Clip& c)
        {
            if (c.warp.empty()) return;
            const double d = c.warp.front().desti;
            if (std::abs (d) < 1.0e-12) return;
            for (auto& p : c.warp) p.desti -= d;
            c.inici -= d;
            c.fi -= d;
            c.pos += d;
        }

        void ajustaFades (Clip& c)
        {
            const double l = std::max (0.0, c.fi - c.inici);
            c.fadeIn = std::clamp (c.fadeIn, 0.0, l);
            c.fadeOut = std::clamp (c.fadeOut, 0.0, std::max (0.0, l - c.fadeIn));
        }

        /** Treu els punts de dins que no canvien res (són a la recta dels veïns). Si queda l'original, buida. */
        void simplifica (Clip& c)
        {
            auto& w = c.warp;
            for (size_t i = 1; i + 1 < w.size();)
            {
                const double r1 = ratioTros (w, i - 1), r2 = ratioTros (w, i);
                if (std::abs (r1 - r2) < 1.0e-9) w.erase (w.begin() + (long) i);
                else ++i;
            }
            if (w.size() == 2 && std::abs (w[0].desti) < 1.0e-9 && std::abs (w[1].desti - w[1].font) < 1.0e-9)
                w.clear();
        }

        int insereix (std::vector<PuntWarp>& w, PuntWarp p)
        {
            auto it = std::upper_bound (w.begin(), w.end(), p.desti, [] (double d, const PuntWarp& x) { return d < x.desti; });
            const auto i = it - w.begin();
            w.insert (it, p);
            return (int) i;
        }

        bool hiHaPuntA (const std::vector<PuntWarp>& w, double desti, double marge)
        {
            return std::any_of (w.begin(), w.end(), [&] (const PuntWarp& x) { return std::abs (x.desti - desti) < marge; });
        }
    }

    const char* nomMode (int mode)
    {
        switch (mode)
        {
            case Ritme: return "Ritme (bateria)";
            case Veu:   return "Veu / solista";
            default:    return "General";
        }
    }

    bool actiu (const Clip& c) noexcept { return c.warp.size() >= 2; }

    double aClip (const std::vector<PuntWarp>& w, double f) noexcept
    {
        if (w.empty()) return f;
        if (f <= w.front().font) return w.front().desti + (f - w.front().font);
        if (f >= w.back().font) return w.back().desti + (f - w.back().font);
        auto it = std::upper_bound (w.begin(), w.end(), f, [] (double v, const PuntWarp& x) { return v < x.font; });
        const auto& b = *it;
        const auto& a = *(it - 1);
        const double df = b.font - a.font;
        return df > 0.0 ? a.desti + (f - a.font) / df * (b.desti - a.desti) : a.desti;
    }

    double aFont (const std::vector<PuntWarp>& w, double d) noexcept
    {
        if (w.empty()) return d;
        if (d <= w.front().desti) return w.front().font + (d - w.front().desti);
        if (d >= w.back().desti) return w.back().font + (d - w.back().desti);
        auto it = std::upper_bound (w.begin(), w.end(), d, [] (double v, const PuntWarp& x) { return v < x.desti; });
        const auto& b = *it;
        const auto& a = *(it - 1);
        const double dd = b.desti - a.desti;
        return dd > 0.0 ? a.font + (d - a.desti) / dd * (b.font - a.font) : a.font;
    }

    double duradaClip (const Clip& c, double duradaFitxer) noexcept
    {
        return actiu (c) ? aClip (c.warp, duradaFitxer) : duradaFitxer;
    }

    double ratioTros (const std::vector<PuntWarp>& w, size_t i) noexcept
    {
        if (i + 1 >= w.size()) return 1.0;
        const double df = w[i + 1].font - w[i].font;
        return df > 0.0 ? (w[i + 1].desti - w[i].desti) / df : 1.0;
    }

    void activa (Clip& c, double durada)
    {
        if (actiu (c)) return;
        durada = std::max (durada, 0.01);
        c.warp = { { 0.0, 0.0 }, { durada, durada } };
    }

    double estiraVora (Clip& c, double durada, bool dreta, double vora)
    {
        activa (c, durada);
        const double l = c.fi - c.inici;
        if (l <= 0.0) return 1.0;
        double k = dreta ? (vora - c.pos - c.inici) / l : (c.pos + c.fi - vora) / l;
        double kMin = 0.0, kMax = 1.0e9;
        for (size_t i = 0; i + 1 < c.warp.size(); ++i)
        {
            const double r = ratioTros (c.warp, i);
            kMin = std::max (kMin, ratioMin / r);
            kMax = std::min (kMax, ratioMax / r);
        }
        kMin = std::max (kMin, edicio::longitudMinima / l);
        k = std::clamp (k, kMin, std::max (kMin, kMax));
        const double a = dreta ? c.inici : c.fi;
        for (auto& p : c.warp) p.desti = a + (p.desti - a) * k;
        if (dreta) c.fi = a + l * k;
        else c.inici = a - l * k;
        ajustaFades (c);
        normalitza (c);
        simplifica (c);
        return k;
    }

    bool esVora (const Clip& c, size_t i) noexcept
    {
        if (i >= c.warp.size()) return false;
        const double d = c.warp[i].desti;
        return std::abs (d - c.inici) < tocant * 10.0 || std::abs (d - c.fi) < tocant * 10.0;
    }

    std::vector<int> marcadors (const Clip& c)
    {
        std::vector<int> r;
        for (size_t i = 1; i + 1 < c.warp.size(); ++i)
        {
            const double d = c.warp[i].desti;
            if (d > c.inici && d < c.fi && ! esVora (c, i)) r.push_back ((int) i);
        }
        return r;
    }

    int afegeixMarcador (Clip& c, double durada, double t)
    {
        const double u = t - c.pos;
        if (u <= c.inici + distanciaMinima * 0.5 || u >= c.fi - distanciaMinima * 0.5) return -1;
        activa (c, durada);
        const double fd = c.warp.back().font;
        if (hiHaPuntA (c.warp, u, distanciaMinima)) return -1;
        const double f = aFont (c.warp, u);
        if (f <= 0.0 || f >= fd) return -1;
        // Les vores de la finestra queden fixes (marcadors 1 i 3): així el que es mou és només el que hi ha a dins.
        for (double vora : { c.inici, c.fi })
        {
            const double fv = aFont (c.warp, vora);
            if (fv > tocant && fv < fd - tocant && ! hiHaPuntA (c.warp, vora, tocant * 10.0))
                insereix (c.warp, { fv, vora });
        }
        return insereix (c.warp, { f, u });
    }

    double mouMarcador (Clip& c, int i, double t)
    {
        auto& w = c.warp;
        if (i <= 0 || i + 1 >= (int) w.size()) return t;
        const auto& a = w[(size_t) i - 1];
        const auto& b = w[(size_t) i + 1];
        const double f = w[(size_t) i].font;
        const double lo = std::max (a.desti + std::max (trosMinim, ratioMin * (f - a.font)), b.desti - ratioMax * (b.font - f));
        const double hi = std::min (a.desti + ratioMax * (f - a.font), b.desti - std::max (trosMinim, ratioMin * (b.font - f)));
        if (lo > hi) return c.pos + w[(size_t) i].desti;
        w[(size_t) i].desti = std::clamp (t - c.pos, lo, hi);
        return c.pos + w[(size_t) i].desti;
    }

    bool treuMarcador (Clip& c, int i)
    {
        if (i <= 0 || i + 1 >= (int) c.warp.size()) return false;
        c.warp.erase (c.warp.begin() + i);
        simplifica (c);
        return true;
    }

    void treuMarcadors (Clip& c, double durada)
    {
        if (! actiu (c)) return;
        const double f0 = aFont (c.warp, c.inici), f1 = aFont (c.warp, c.fi);
        if (f1 - f0 <= 1.0e-9) { restaura (c); return; }
        const double r = std::clamp ((c.fi - c.inici) / (f1 - f0), ratioMin, ratioMax);
        durada = std::max (durada, c.warp.back().font);
        c.warp = { { 0.0, c.inici - r * f0 }, { durada, c.inici + r * (durada - f0) } };
        normalitza (c);
        simplifica (c);
    }

    void restaura (Clip& c)
    {
        if (c.warp.empty()) return;
        const double t0 = c.pos + c.inici;
        const double f0 = aFont (c.warp, c.inici), f1 = aFont (c.warp, c.fi);
        c.warp.clear();
        c.inici = f0;
        c.fi = std::max (f1, f0 + edicio::longitudMinima);
        c.pos = t0 - f0;
        ajustaFades (c);
    }

    //==========================================================================
    std::vector<double> cops (const AudioPresa& a, double sens)
    {
        std::vector<double> r;
        if (a.longitud <= 0 || a.fs <= 0.0) return r;
        sens = std::clamp (sens, 0.0, 1.0);
        const long long n = a.longitud;
        std::vector<float> m ((size_t) n);
        for (long long i = 0; i < n; ++i)
        {
            float v = 0.0f;
            for (int ch = 0; ch < a.canals; ++ch) v += a.dades[ch][(size_t) i];
            m[(size_t) i] = v / (float) std::max (1, a.canals);
        }
        // Energia del senyal derivat (dona més pes als aguts: els atacs) en trossets de 5 ms.
        const int hop = std::max (32, (int) std::lround (a.fs * 0.005));
        const long long frames = n / hop;
        if (frames < 4) return r;
        std::vector<double> e ((size_t) frames);
        double maxim = -200.0;
        for (long long k = 0; k < frames; ++k)
        {
            double s = 0.0;
            const long long i0 = k * hop, i1 = std::min (n, i0 + 2 * hop);
            for (long long i = std::max (1LL, i0); i < i1; ++i)
            {
                const double d = (double) m[(size_t) i] - (double) m[(size_t) i - 1];
                s += d * d;
            }
            e[(size_t) k] = 10.0 * std::log10 (s / (double) std::max (1LL, i1 - i0) + 1.0e-14);
            maxim = std::max (maxim, e[(size_t) k]);
        }
        const double puja = 14.0 - 10.0 * sens;        // dB que ha de pujar en 30 ms
        const double terra = maxim - (30.0 + 20.0 * sens);
        auto deteccio = [&] (long long k)
        {
            double mn = e[(size_t) k];
            for (long long j = std::max (0LL, k - 6); j < k; ++j) mn = std::min (mn, e[(size_t) j]);
            return e[(size_t) k] - mn;
        };
        double ultim = -1.0;
        for (long long k = 1; k + 1 < frames; ++k)
        {
            const double o = deteccio (k);
            if (o < puja || e[(size_t) k] < terra) continue;
            if (o < deteccio (k - 1) || o <= deteccio (k + 1)) continue;   // el punt més fort de la pujada
            // Afina a la mostra: on el senyal arriba per primer cop a un quart del pic del voltant.
            const long long i0 = std::max (0LL, (k - 3) * hop), i1 = std::min (n, (k + 3) * hop);
            float pic = 0.0f;
            for (long long i = i0; i < i1; ++i) pic = std::max (pic, std::abs (m[(size_t) i]));
            long long on = i0;
            for (long long i = i0; i < i1; ++i)
                if (std::abs (m[(size_t) i]) >= 0.25f * pic) { on = i; break; }
            const double t = std::max (0.0, (double) on / a.fs - 0.001);
            if (ultim >= 0.0 && t - ultim < 0.05) continue;
            r.push_back (t);
            ultim = t;
        }
        return r;
    }

    int marcadorsAlsCops (Clip& c, double durada, const std::vector<double>& copsFitxer)
    {
        activa (c, durada);
        int n = 0;
        for (double f : copsFitxer)
        {
            const double u = aClip (c.warp, f);
            if (u <= c.inici + distanciaMinima || u >= c.fi - distanciaMinima) continue;
            if (afegeixMarcador (c, durada, c.pos + u) >= 0) ++n;
        }
        if (n == 0) simplifica (c);
        return n;
    }

    int quantitza (Clip& c, const Projecte& p, double forca)
    {
        int n = 0;
        for (int i : marcadors (c))
        {
            const double t = c.pos + c.warp[(size_t) i].desti;
            const double g = edicio::enganxa (p, t, Snap::Temps);
            const double desti = t + (g - t) * std::clamp (forca, 0.0, 1.0);
            if (std::abs (desti - t) < 1.0e-5) continue;
            const double fet = mouMarcador (c, i, desti);
            if (std::abs (fet - t) > 1.0e-5) ++n;
        }
        return n;
    }

    std::string clau (const Clip& c, bool corregida)
    {
        std::string s = "~e|" + c.presa + (corregida ? "|v|" : "|o|") + std::to_string (c.elastic);
        for (auto& p : c.warp)
            s += "|" + std::to_string (std::llround (p.font * 1.0e5)) + ":" + std::to_string (std::llround (p.desti * 1.0e5));
        return s;
    }

    //==========================================================================
    // El so: WSOLA amb un mapa de temps variable, tros a tros entre cops.
    namespace
    {
        struct Font
        {
            int canals = 1, marge = 0;
            long long len = 0;
            std::vector<float> x[2], guia;   // amb «marge» zeros a cada banda
            float mostra (int ch, long long i) const noexcept { return x[ch][(size_t) (i + marge)]; }
        };

        double correlacio (const Font& f, long long a, long long b, int n, int pas)
        {
            const float* g = f.guia.data() + f.marge;
            double s = 0.0;
            for (int i = 0; i < n; i += pas) s += (double) g[a + i] * g[b + i];
            return s;
        }

        /** Un tros: de la mostra «sa» del fitxer, que cau a «wa» de la sortida, fins a «wb» (sortida). El tros es
            renderitza a [inici, wb) amb fades d'«xIn» abans de «wa» i d'«xOut» abans de «wb», i se suma a «out».
            No llegeix res del fitxer a partir de «sb» (el cop següent): si no, l'atac sonaria abans d'hora. */
        void tros (const Font& f, const std::vector<PuntWarp>& w, double fs, long long sa, long long sb, long long wa, long long wb,
                   long long inici, long long xIn, long long xOut, int nFin, std::vector<float>* out, long long outLen)
        {
            const int hs = nFin / 2, p = nFin / 2, tol = nFin / 4;
            static thread_local std::vector<float> fin;
            if ((int) fin.size() != nFin)
            {
                fin.assign ((size_t) nFin, 0.0f);
                for (int i = 0; i < nFin; ++i) fin[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * pi * i / nFin));
            }
            const long long lim = (long long) f.marge - nFin;   // fins on pot anar una finestra fora del fitxer
            auto dins = [&] (long long c) { return std::clamp (c, -lim + p, f.len + lim - p); };

            const long long ini = std::max (0LL, inici), fi = std::min (outLen, wb);
            if (fi <= ini) return;
            std::vector<float> acc[2];
            for (int ch = 0; ch < f.canals; ++ch) acc[ch].assign ((size_t) (fi - ini), 0.0f);

            long long anterior = 0;
            for (long long k = -3;; ++k)
            {
                const long long o = wa + k * hs;   // centre de la finestra a la sortida
                if (o - p >= fi) break;
                long long c;
                if (k <= 0) c = dins (sa + k * hs);   // abans del cop: tal com és (l'atac surt intacte)
                else
                {
                    const long long nat = anterior + hs;
                    const long long obj = dins (std::llround (aFont (w, (double) o / fs) * fs));
                    long long millor = obj;   // si tot és igual (silenci), es queda on toca
                    double maxim = correlacio (f, obj - p, nat - p, nFin, 4);
                    for (long long d = -tol; d <= tol; d += 4)
                    {
                        const long long c0 = dins (obj + d);
                        const double v = correlacio (f, c0 - p, nat - p, nFin, 4);
                        if (v > maxim) { maxim = v; millor = c0; }
                    }
                    const long long grossa = millor;
                    maxim = correlacio (f, grossa - p, nat - p, nFin, 2);
                    for (long long d = -3; d <= 3; ++d)
                    {
                        const long long c0 = dins (grossa + d);
                        const double v = correlacio (f, c0 - p, nat - p, nFin, 2);
                        if (v > maxim) { maxim = v; millor = c0; }
                    }
                    c = millor;
                }
                anterior = c;
                const long long o0 = o - p;
                const int i0 = (int) std::max (0LL, ini - o0), i1 = (int) std::min ((long long) nFin, fi - o0);
                // Fins on es pot llegir: el cop següent (amb una rampa d'1 ms just abans).
                const long long rampa = std::max (8LL, (long long) (0.001 * fs));
                const int i2 = (int) std::clamp (sb - (c - p), 0LL, (long long) nFin);
                for (int ch = 0; ch < f.canals; ++ch)
                {
                    float* a = acc[ch].data() + (o0 - ini);
                    const float* x = f.x[ch].data() + f.marge + (c - p);
                    const int fins = std::min (i1, i2);
                    for (int i = i0; i < fins; ++i)
                    {
                        const long long resta = sb - (c - p + i);
                        const float g = resta >= rampa ? 1.0f : (float) resta / (float) rampa;
                        a[i] += x[i] * fin[(size_t) i] * g;
                    }
                }
            }
            for (int ch = 0; ch < f.canals; ++ch)
                for (long long o = ini; o < fi; ++o)
                {
                    double g = 1.0;
                    if (xIn > 0 && o < wa) g = (double) (o - (wa - xIn)) / (double) xIn;
                    if (xOut > 0 && o >= wb - xOut) g = std::min (g, (double) (wb - o) / (double) xOut);
                    if (g <= 0.0) continue;
                    out[ch][(size_t) o] += (float) (acc[ch][(size_t) (o - ini)] * g);
                }
        }

        /** El tros [sa, sb) del fitxer només es desplaça (sense estirar): es copia tal qual. */
        bool nomesDesplacat (const std::vector<PuntWarp>& w, double a, double b)
        {
            for (size_t i = 0; i + 1 < w.size(); ++i)
            {
                if (w[i + 1].font <= a || w[i].font >= b) continue;
                if (std::abs (ratioTros (w, i) - 1.0) > 1.0e-9) return false;
            }
            return true;
        }
    }

    std::shared_ptr<AudioPresa> renderitza (const AudioPresa& o, const std::vector<PuntWarp>& w, int mode,
                                            const std::function<bool (float)>& progres)
    {
        auto r = std::make_shared<AudioPresa>();
        r->fs = o.fs;
        r->canals = std::clamp (o.canals, 1, 2);
        const double fs = o.fs > 0.0 ? o.fs : 48000.0;
        const double durada = (double) o.longitud / fs;
        const long long outLen = std::max (1LL, (long long) std::llround (aClip (w, durada) * fs));
        r->longitud = outLen;
        for (int ch = 0; ch < r->canals; ++ch) r->dades[ch].assign ((size_t) outLen, 0.0f);
        if (o.longitud <= 0) return r;

        const double segons = mode == Ritme ? 0.020 : mode == Veu ? 0.030 : 0.040;
        const int nFin = std::max (256, (int) std::lround (segons * fs) / 2 * 2);

        Font f;
        f.canals = r->canals;
        f.len = o.longitud;
        f.marge = nFin * 3 + 16;
        for (int ch = 0; ch < f.canals; ++ch)
        {
            const auto& d = o.dades[std::min (ch, o.canals - 1)];
            f.x[ch].assign ((size_t) (f.len + 2 * f.marge), 0.0f);
            std::copy (d.begin(), d.begin() + (long) std::min<long long> ((long long) d.size(), f.len), f.x[ch].begin() + f.marge);
        }
        f.guia.assign (f.x[0].size(), 0.0f);
        for (size_t i = 0; i < f.guia.size(); ++i)
            f.guia[i] = f.canals > 1 ? 0.5f * (f.x[0][i] + f.x[1][i]) : f.x[0][i];

        // Els talls: on comença cada cop (el mode Veu no talla: les notes lligades no s'han de partir).
        std::vector<long long> talls { 0 };
        if (mode != Veu)
            for (double t : cops (o, mode == Ritme ? 0.7 : 0.35))
            {
                const long long s = std::llround (t * fs);
                if (s > talls.back() + (long long) (0.03 * fs) && s < f.len - (long long) (0.01 * fs)) talls.push_back (s);
            }

        const long long xf = std::max (16LL, (long long) std::lround (0.003 * fs));
        for (size_t j = 0; j < talls.size(); ++j)
        {
            if (progres && ! progres ((float) j / (float) talls.size())) return nullptr;
            const long long sa = talls[j], sb = j + 1 < talls.size() ? talls[j + 1] : f.len;
            const long long wa = std::llround (aClip (w, (double) sa / fs) * fs);
            const long long wb = j + 1 < talls.size() ? std::llround (aClip (w, (double) sb / fs) * fs) : outLen;
            if (wb <= wa) continue;
            const long long xIn = j > 0 ? std::min (xf, (wb - wa) / 2) : 0;
            const long long xOut = j + 1 < talls.size() ? std::min (xf, (wb - wa) / 2) : 0;
            const long long inici = j > 0 ? wa - xIn : 0;
            const double marge = (double) nFin / fs;
            if (nomesDesplacat (w, (double) sa / fs - marge, (double) sb / fs + marge))
            {
                const long long desp = wa - sa;
                for (int ch = 0; ch < f.canals; ++ch)
                    for (long long q = std::max (0LL, inici); q < std::min (outLen, wb); ++q)
                    {
                        double g = 1.0;
                        if (xIn > 0 && q < wa) g = (double) (q - (wa - xIn)) / (double) xIn;
                        if (xOut > 0 && q >= wb - xOut) g = std::min (g, (double) (wb - q) / (double) xOut);
                        const long long src = q - desp;
                        if (g > 0.0 && src >= 0 && src < f.len) r->dades[ch][(size_t) q] += (float) (f.mostra (ch, src) * g);
                    }
                continue;
            }
            tros (f, w, fs, sa, j + 1 < talls.size() ? sb : f.len + f.marge, wa, wb, inici, xIn, xOut, nFin, r->dades, outLen);
        }
        if (progres) progres (1.0f);
        return r;
    }
}
