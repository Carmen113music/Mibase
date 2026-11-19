#pragma once

#include <cmath>
#include <utility>
#include <vector>

/** 0.60: FFT complexa de base 2 compartida (abans era només dins de la convolució; el codi és el mateix).
    Iterativa, amb les taules fetes en crear-la: fa() no demana memòria. */
namespace mibase::fft
{
    struct C { float r = 0.0f, i = 0.0f; };

    class FFT
    {
    public:
        explicit FFT (int mida) : n (mida), tw ((size_t) mida / 2), rev ((size_t) mida)
        {
            constexpr double pi = 3.14159265358979323846;
            int bits = 0;
            while ((1 << bits) < n) ++bits;
            for (int i = 0; i < n; ++i)
            {
                int r = 0;
                for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b);
                rev[(size_t) i] = r;
            }
            for (int k = 0; k < n / 2; ++k)
                tw[(size_t) k] = { (float) std::cos (-2.0 * pi * k / n), (float) std::sin (-2.0 * pi * k / n) };
        }
        void fa (C* x, bool inversa) const noexcept
        {
            for (int i = 0; i < n; ++i)
                if (i < rev[(size_t) i]) std::swap (x[i], x[rev[(size_t) i]]);
            for (int len = 2; len <= n; len <<= 1)
            {
                const int mig = len / 2, pas = n / len;
                for (int i = 0; i < n; i += len)
                    for (int j = 0; j < mig; ++j)
                    {
                        const C w = tw[(size_t) (j * pas)];
                        const float wi = inversa ? -w.i : w.i;
                        C& a = x[i + j];
                        C& b = x[i + j + mig];
                        const C v { b.r * w.r - b.i * wi, b.r * wi + b.i * w.r };
                        b = { a.r - v.r, a.i - v.i };
                        a = { a.r + v.r, a.i + v.i };
                    }
            }
        }
        int mida() const noexcept { return n; }
    private:
        int n;
        std::vector<C> tw;
        std::vector<int> rev;
    };
}
