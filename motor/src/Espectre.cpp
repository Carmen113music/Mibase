#include "mibase/Espectre.h"
#include <algorithm>
#include <cmath>

namespace mibase::espectre
{
    void Anell::escriu (const float* x, int n) noexcept
    {
        auto p = pos.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i) b[(size_t) ((p + (std::uint32_t) i) & (midaAnell - 1))] = x[i];
        pos.store (p + (std::uint32_t) n, std::memory_order_release);
    }

    void Anell::escriuUna (float x) noexcept
    {
        const auto p = pos.load (std::memory_order_relaxed);
        b[(size_t) (p & (midaAnell - 1))] = x;
        pos.store (p + 1, std::memory_order_release);
    }

    void Anell::llegeix (float* desti, int n) const noexcept
    {
        n = std::clamp (n, 0, (int) midaAnell);
        const auto p = pos.load (std::memory_order_acquire);
        const std::uint32_t ini = p - (std::uint32_t) n;
        for (int i = 0; i < n; ++i) desti[i] = b[(size_t) ((ini + (std::uint32_t) i) & (midaAnell - 1))];
    }

    void Anell::buida() noexcept
    {
        b.fill (0.0f);
    }

    Analitzador::Analitzador (int mida) : n (mida), f (mida), finestra ((size_t) mida), db ((size_t) mida / 2 + 1), buf ((size_t) mida)
    {
        constexpr double pi = 3.14159265358979323846;
        for (int i = 0; i < n; ++i) finestra[(size_t) i] = (float) (0.5 - 0.5 * std::cos (2.0 * pi * i / n));
    }

    const std::vector<float>& Analitzador::analitza (const float* x)
    {
        for (int i = 0; i < n; ++i) buf[(size_t) i] = { x[i] * finestra[(size_t) i], 0.0f };
        f.fa (buf.data(), false);
        const double esc = 4.0 / n;   // Hann: guany coherent 0,5 · un sinus d'amplitud A dona A·N/2
        for (int k = 0; k <= n / 2; ++k)
        {
            const auto& c = buf[(size_t) k];
            const double m = std::sqrt ((double) c.r * c.r + (double) c.i * c.i) * esc;
            db[(size_t) k] = (float) std::max ((double) pis, 20.0 * std::log10 (std::max (1.0e-12, m)));
        }
        return db;
    }

    int midaPer (double fs)
    {
        return fs > 60000.0 ? 8192 : 4096;
    }

    void aColumnes (const std::vector<float>& binsDb, double fs, const std::vector<float>& freqs, std::vector<float>& out)
    {
        const size_t nc = freqs.size();
        out.assign (nc, pis);
        if (binsDb.size() < 2 || nc == 0) return;
        const int nb = (int) binsDb.size() - 1;               // l'últim bin és la meitat de fs
        const double hzBin = fs * 0.5 / nb;
        for (size_t c = 0; c < nc; ++c)
        {
            const double f = freqs[c];
            // límits de la columna: a mig camí de les veïnes
            const double fa = c > 0 ? std::sqrt (f * freqs[c - 1]) : f / std::sqrt (freqs.size() > 1 ? freqs[1] / f : 1.0);
            const double fb = c + 1 < nc ? std::sqrt (f * freqs[c + 1]) : f * std::sqrt (nc > 1 ? f / freqs[c - 1] : 1.0);
            const int ka = (int) std::ceil (fa / hzBin), kb = (int) std::floor (fb / hzBin);
            if (kb >= ka && ka <= nb)
            {
                float m = pis;
                for (int k = std::max (1, ka); k <= std::min (nb, kb); ++k) m = std::max (m, binsDb[(size_t) k]);
                out[c] = m;
            }
            else
            {
                const double x = f / hzBin;
                if (x >= nb) { out[c] = binsDb[(size_t) nb]; continue; }
                const int k = std::max (0, (int) x);
                const double t = x - k;
                out[c] = (float) (binsDb[(size_t) k] + t * (binsDb[(size_t) k + 1] - binsDb[(size_t) k]));
            }
        }
    }

    float inclinacio (float f, float dbOct)
    {
        return dbOct * (float) std::log2 (std::max (1.0f, f) / 1000.0f);
    }

    float caigudaPic (int v)
    {
        return v == Lenta ? 8.0f : (v == Rapida ? 45.0f : 18.0f);
    }

    double tempsMitjana (int v)
    {
        return v == Lenta ? 1.2 : (v == Rapida ? 0.08 : 0.3);
    }

    void suavitza (std::vector<float>& m, const std::vector<float>& nou, int mode, int velocitat, double dt)
    {
        if (m.size() != nou.size()) { m = nou; return; }
        dt = std::clamp (dt, 0.0, 1.0);
        if (mode == Pic)
        {
            const float baixa = caigudaPic (velocitat) * (float) dt;
            for (size_t i = 0; i < m.size(); ++i) m[i] = std::max (nou[i], std::max (pis, m[i] - baixa));
        }
        else
        {
            const double k = 1.0 - std::exp (-dt / tempsMitjana (velocitat));
            for (size_t i = 0; i < m.size(); ++i)
            {
                const double a = std::pow (10.0, m[i] / 10.0), b = std::pow (10.0, nou[i] / 10.0);
                m[i] = (float) std::max ((double) pis, 10.0 * std::log10 (std::max (1.0e-13, a + (b - a) * k)));
            }
        }
    }

    int picProper (const std::vector<float>& col, const std::vector<float>& freqs, float f, float oct)
    {
        const float fa = f / std::exp2 (oct), fb = f * std::exp2 (oct);
        int millor = -1;
        for (size_t c = 1; c + 1 < col.size() && c < freqs.size(); ++c)
        {
            if (freqs[c] < fa || freqs[c] > fb) continue;
            if (col[c] <= pis + 1.0f) continue;
            if (col[c] >= col[c - 1] && col[c] >= col[c + 1] && (millor < 0 || col[c] > col[(size_t) millor])) millor = (int) c;
        }
        return millor;
    }
}
