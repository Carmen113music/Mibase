#include "mibase/Doubler.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace mibase::doubler
{
    namespace
    {
        const InfoParam infos[numParams] = {
            { "quantitat", "Quantitat", 0, 100, 60, "Com de fort sonen les dues còpies al costat de la veu (100 %: tan fort com la veu; la veu no es toca)." },
            { "desafina", "Desafina", 0, 30, 12, "Quants cèntims puja una còpia i baixa l'altra: 5–12 és el doblat natural; més, més ample." },
            { "retard", "Retard", 3, 60, 22, "Quant arriben tard les còpies (ms): 20–30 és el doblat clàssic; més, més separades de la veu." },
            { "amplada", "Amplada", 0, 100, 100, "0 %: les còpies al mig. 100 %: una a cada costat." },
            { "color", "Color", 0, 100, 15, "Enfosqueix les còpies perquè no competeixin amb la veu." },
            { "variacio", "Variació", 0, 100, 60, "Com de diferent de la veu va cada còpia: s'avança i s'endarrereix uns mil·lisegons a l'atzar, com una segona presa de debò." },
            { "veu", "Veu", 0, 100, 100, "La veu original. A 0 %, només sonen les còpies (per fer servir el Doubler en un retorn)." },
        };
        constexpr double finestraMs = 30.0;
        inline float lim (float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
    }

    const InfoParam& info (int p) { return infos[std::clamp (p, 0, numParams - 1)]; }

    Doubler::Doubler()
    {
        for (int i = 0; i < numParams; ++i) par[(size_t) i].store (infos[i].def);
        linia.assign ((size_t) mida, 0.0f);
    }

    void Doubler::prepara (double f, int)
    {
        fs = f > 1000.0 ? f : 48000.0;
        const int cal = (int) std::ceil ((60.0 + 5.0 + finestraMs + 4.0) * fs / 1000.0) + 4;   // 0.75: fins a 60 ms + la variació
        mida = 1;
        while (mida < cal) mida <<= 1;
        mascara = mida - 1;
        linia.assign ((size_t) mida, 0.0f);
        w = 0;
        fase[0] = 0.0; fase[1] = 0.5;
        lpEstat[0] = lpEstat[1] = 0.0f;
        for (int v = 0; v < 2; ++v) { derivaDe[v] = 0.0; derivaA[v] = 2.0 * atzar() - 1.0; derivaFase[v] = 0.0; derivaPas[v] = 1.0 / ((0.4 + 0.5 * atzar()) * fs); }
        primer = true;
    }

    void Doubler::processa (float* L, float* R, int n) noexcept
    {
        if (demanaBuidar.exchange (false))
        {
            std::fill (linia.begin(), linia.end(), 0.0f);
            lpEstat[0] = lpEstat[1] = 0.0f;
        }
        const float quant = valor (Quantitat) / 100.0f;   // 0.75: fins a tan fort com la veu (abans 0,75)
        if (primer) { sQuant = quant; primer = false; }
        if (quant < 1.0e-5f && sQuant < 1.0e-5f)
        {
            // no se sent: només omple la línia (perquè en pujar-ho no surti res vell)
            const float gv = valor (Veu) / 100.0f;
            for (int i = 0; i < n; ++i) { linia[(size_t) w] = 0.5f * (L[i] + R[i]); w = (w + 1) & mascara; L[i] *= gv; R[i] *= gv; }
            return;
        }
        const double cents = valor (Desafina);
        const double rAmunt = std::pow (2.0, cents / 1200.0), rAvall = std::pow (2.0, -cents / 1200.0);
        const double fin = finestraMs * fs / 1000.0;
        const double base = valor (Retard) * fs / 1000.0;
        // per mostra, la fase avança (1 - r) / finestra: amb r > 1 el retard s'escurça i l'afinació puja
        const double pas[2] { (1.0 - rAmunt) / fin, (1.0 - rAvall) / fin };
        const float amp = valor (Amplada) / 100.0f;
        const float a = 0.5f + 0.5f * amp, b = 0.5f - 0.5f * amp;
        const double tall = 18000.0 * std::pow (3000.0 / 18000.0, valor (Color) / 100.0);
        const float k = (float) (1.0 - std::exp (-2.0 * 3.141592653589793 * std::min (tall, fs * 0.45) / fs));
        const float dq = (quant - sQuant) / (float) std::max (1, n);
        const double variacio = valor (Variacio) / 100.0 * 4.0 * fs / 1000.0;   // fins a ±4 ms
        const float gVeu = valor (Veu) / 100.0f;   // 0.76
        auto llegeix = [this] (double ret) noexcept
        {
            double p = (double) w - ret;
            while (p < 0.0) p += mida;
            const int i0 = (int) p;
            const float fr = (float) (p - i0);
            const float x0 = linia[(size_t) (i0 & mascara)], x1 = linia[(size_t) ((i0 + 1) & mascara)];
            return x0 + (x1 - x0) * fr;
        };
        for (int i = 0; i < n; ++i)
        {
            linia[(size_t) w] = 0.5f * (L[i] + R[i]);
            float veu[2];
            for (int v = 0; v < 2; ++v)
            {
                double f0 = fase[v] + pas[v];
                f0 -= std::floor (f0);
                fase[v] = f0;
                double f1 = f0 + 0.5;
                if (f1 >= 1.0) f1 -= 1.0;
                const float g0 = (float) (1.0 - std::abs (2.0 * f0 - 1.0)), g1 = (float) (1.0 - std::abs (2.0 * f1 - 1.0));
                // 0.75: la deriva lenta (corba suau entre punts a l'atzar cada 0,4–0,9 s)
                derivaFase[v] += derivaPas[v];
                if (derivaFase[v] >= 1.0)
                {
                    derivaFase[v] -= 1.0;
                    derivaDe[v] = derivaA[v];
                    derivaA[v] = 2.0 * atzar() - 1.0;
                    derivaPas[v] = 1.0 / ((0.4 + 0.5 * atzar()) * fs);
                }
                const double c = 0.5 - 0.5 * std::cos (3.141592653589793 * derivaFase[v]);
                const double ret = base + variacio * (derivaDe[v] + (derivaA[v] - derivaDe[v]) * c);
                const float x = g0 * llegeix (ret + f0 * fin + 1.0) + g1 * llegeix (ret + f1 * fin + 1.0);
                lpEstat[v] += k * (x - lpEstat[v]);
                veu[v] = lpEstat[v];
            }
            sQuant += dq;
            L[i] = gVeu * L[i] + sQuant * (a * veu[0] + b * veu[1]);
            R[i] = gVeu * R[i] + sQuant * (b * veu[0] + a * veu[1]);
            w = (w + 1) & mascara;
        }
        sQuant = quant;
    }

    InfoParametre Doubler::infoParametre (int i) const
    {
        InfoParametre r;
        r.id = info (i).clau;
        r.nom = info (i).nom;
        return r;
    }
    float Doubler::normal (int p, float v) noexcept
    {
        const auto& f = info (p);
        return lim ((v - f.mn) / (f.mx - f.mn), 0.0f, 1.0f);
    }
    float Doubler::deNormal (int p, float v01) noexcept
    {
        const auto& f = info (p);
        return f.mn + lim (v01, 0.0f, 1.0f) * (f.mx - f.mn);
    }
    void Doubler::posaParametre (int i, float v01) noexcept { posa (i, deNormal (i, v01)); }
    float Doubler::parametre (int i) const noexcept { return normal (i, valor (i)); }
    std::string Doubler::textParametre (int i, float v01) const
    {
        const float v = deNormal (i, v01);
        char b[48];
        switch (i)
        {
            case Desafina: std::snprintf (b, sizeof b, "%.1f cèntims", (double) v); break;
            case Retard:   std::snprintf (b, sizeof b, "%.1f ms", (double) v); break;
            default:       std::snprintf (b, sizeof b, "%d %%", (int) std::lround (v)); break;
        }
        std::string s (b);
        for (auto& c : s) if (c == '.') c = ',';
        return s;
    }
    void Doubler::posa (int p, float v) noexcept
    {
        if (p < 0 || p >= numParams || ! std::isfinite (v)) return;
        par[(size_t) p].store (lim (v, infos[p].mn, infos[p].mx), std::memory_order_relaxed);
    }
    float Doubler::valor (int p) const noexcept
    {
        return p >= 0 && p < numParams ? par[(size_t) p].load (std::memory_order_relaxed) : 0.0f;
    }
    std::string Doubler::estat() const
    {
        std::string s = "mibase-doubler=1";
        char b[64];
        for (int i = 0; i < numParams; ++i) { std::snprintf (b, sizeof b, ";%s=%.6g", infos[i].clau, (double) valor (i)); s += b; }
        return s;
    }
    void Doubler::posaEstat (const std::string& s)
    {
        if (s.rfind ("mibase-doubler=", 0) != 0) return;
        for (int i = 0; i < numParams; ++i) posa (i, infos[i].def);
        size_t a = 0;
        while (a < s.size())
        {
            size_t b = s.find (';', a);
            if (b == std::string::npos) b = s.size();
            const auto parell = s.substr (a, b - a);
            const auto eq = parell.find ('=');
            if (eq != std::string::npos)
                for (int i = 0; i < numParams; ++i)
                    if (parell.compare (0, eq, infos[i].clau) == 0 && std::string (infos[i].clau).size() == eq)
                        posa (i, (float) std::strtod (parell.c_str() + eq + 1, nullptr));
            a = b + 1;
        }
    }
}
