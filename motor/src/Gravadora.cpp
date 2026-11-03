#include "mibase/Gravadora.h"

#include <algorithm>
#include <chrono>

namespace mibase
{
    namespace { constexpr size_t midaAnell = (size_t) 1 << 21; }   // 2 M mostres: uns 20 s en mono a 48 kHz

    bool Gravadora::comenca (const std::filesystem::path& cami, int nCanals, double fs)
    {
        acaba();
        canals = std::clamp (nCanals, 1, 2);
        if (! wav.obre (cami, canals, fs, 24))
            return false;
        if (anell.size() != midaAnell)
            anell.assign (midaAnell, 0.0f);
        mascara = midaAnell - 1;
        posEscriu.store (0);
        posLlegeix.store (0);
        perdudes.store (0);
        escrits.store (0);
        errorDisc = false;
        seguir.store (true);
        fil = std::thread ([this] { bucle(); });
        actiu.store (true, std::memory_order_release);
        return true;
    }

    void Gravadora::escriu (const float* const* entrada, int n) noexcept
    {
        if (! actiu.load (std::memory_order_acquire) || n <= 0)
            return;
        const size_t cal = (size_t) n * (size_t) canals;
        const auto e = posEscriu.load (std::memory_order_relaxed);
        const auto l = posLlegeix.load (std::memory_order_acquire);
        if (anell.size() - (e - l) < cal)
        {
            perdudes.fetch_add (n, std::memory_order_relaxed);
            return;
        }
        size_t k = e;
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < canals; ++c)
                anell[(k++) & mascara] = entrada[c] != nullptr ? entrada[c][i] : 0.0f;
        posEscriu.store (k, std::memory_order_release);
    }

    size_t Gravadora::buida (std::vector<float>& tros)
    {
        const auto l = posLlegeix.load (std::memory_order_relaxed);
        const auto e = posEscriu.load (std::memory_order_acquire);
        size_t n = e - l;
        n -= n % (size_t) canals;
        if (n == 0) return 0;
        tros.resize (n);
        for (size_t i = 0; i < n; ++i)
            tros[i] = anell[(l + i) & mascara];
        posLlegeix.store (l + n, std::memory_order_release);
        if (! wav.escriu (tros.data(), (long long) (n / (size_t) canals)))
            errorDisc = true;
        escrits.store (wav.getFrames(), std::memory_order_relaxed);
        return n;
    }

    void Gravadora::bucle()
    {
        std::vector<float> tros;
        tros.reserve (midaAnell);
        while (seguir.load (std::memory_order_acquire))
        {
            if (buida (tros) == 0)
                std::this_thread::sleep_for (std::chrono::milliseconds (10));
        }
        while (buida (tros) > 0) {}
    }

    Gravadora::Resultat Gravadora::acaba()
    {
        Resultat r;
        if (! fil.joinable())
            return r;
        actiu.store (false, std::memory_order_release);
        seguir.store (false, std::memory_order_release);
        fil.join();
        const bool tancat = wav.tanca();
        r.frames = wav.getFrames();
        r.perdudes = perdudes.load();
        r.correcte = tancat && ! errorDisc;
        return r;
    }
}
