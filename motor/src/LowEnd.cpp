#include "mibase/LowEnd.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace mibase::lowend
{
    namespace
    {
        const InfoParam infos[numParams] = {
            { "freq", "Freqüència", 60, 250, 120, "Fins on arriben els greus que es treballen (Hz)." },
            { "sub", "Sub", -6, 6, 0, "Puja o baixa el subgreu, al voltant de 45 Hz (dB). Vigila: als altaveus petits no se sent i gasta volum." },
            { "punch", "Punch", 0, 100, 0, "Més cop als atacs dels greus (el bombo)." },
            { "tight", "Tight", 0, 100, 0, "Escurça la cua dels greus: un baix més ajustat i menys bola." },
            { "escalfor", "Escalfor", 0, 100, 0, "Harmònics suaus als greus: el baix es sent millor als altaveus petits." },
            { "fang", "Fang", 0, 100, 0, "Treu acumulació al voltant de 250 Hz (fins a -6 dB): menys so tapat." },
            { "mono", "Mono", 0, 200, 0, "Per sota d'aquesta freqüència, tot al mig (Hz). 0 = apagat. Habitual: 100–150 Hz." },
            { "sortida", "Sortida", -12, 12, 0, "Volum de sortida (dB)." },
        };
        inline float lim (float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
        constexpr double pi = 3.14159265358979323846;
    }

    const InfoParam& info (int p) { return infos[std::clamp (p, 0, numParams - 1)]; }

    LowEnd::LowEnd() { for (int i = 0; i < numParams; ++i) par[(size_t) i].store (infos[i].def); }

    void LowEnd::prepara (double f, int)
    {
        fs = f > 1000.0 ? f : 48000.0;
        fFet = -1; subFet = fangFet = -100; monoFet = -1;
        for (int c = 0; c < 2; ++c) { lp[c][0] = lp[c][1] = hp[c][0] = hp[c][1] = {}; sub[c] = fang[c] = mono[c] = {}; }
        aF = aS = sF = sS = 0.0f; gCop = 1.0f; eIn = eOut = 0.0;
        primer = true;
    }

    void LowEnd::calcula() noexcept
    {
        auto rbj = [this] (Bq& b, int tipus, double f, double q, double gDb)   // 0 LP, 1 HP, 2 campana
        {
            const double w = 2.0 * pi * std::min (f, fs * 0.45) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
            const float z1 = b.z1, z2 = b.z2;
            if (tipus == 2)
            {
                const double A = std::pow (10.0, gDb / 40.0), a0 = 1.0 + al / A;
                b.b0 = (float) ((1.0 + al * A) / a0); b.b1 = (float) (-2.0 * c / a0); b.b2 = (float) ((1.0 - al * A) / a0);
                b.a1 = (float) (-2.0 * c / a0); b.a2 = (float) ((1.0 - al / A) / a0);
            }
            else
            {
                const double a0 = 1.0 + al;
                if (tipus == 0) { b.b0 = (float) ((1.0 - c) / 2.0 / a0); b.b1 = (float) ((1.0 - c) / a0); }
                else { b.b0 = (float) ((1.0 + c) / 2.0 / a0); b.b1 = (float) (-(1.0 + c) / a0); }
                b.b2 = b.b0; b.a1 = (float) (-2.0 * c / a0); b.a2 = (float) ((1.0 - al) / a0);
            }
            b.z1 = z1; b.z2 = z2;
        };
        const float f = valor (Freq), s = valor (Sub), fa = -6.0f * valor (Fang) / 100.0f, mo = valor (Mono);
        if (f != fFet) { for (int c = 0; c < 2; ++c) { rbj (lp[c][0], 0, f, 0.7071, 0); rbj (lp[c][1], 0, f, 0.7071, 0); rbj (hp[c][0], 1, f, 0.7071, 0); rbj (hp[c][1], 1, f, 0.7071, 0); } fFet = f; }
        if (s != subFet) { for (auto& b : sub) rbj (b, 2, 45.0, 0.9, s); subFet = s; }
        if (fa != fangFet) { for (auto& b : fang) rbj (b, 2, 250.0, 1.0, fa); fangFet = fa; }
        if (mo != monoFet) { if (mo >= 10.0f) { rbj (mono[0], 1, mo, 0.7071, 0); rbj (mono[1], 1, mo, 0.7071, 0); } monoFet = mo; }
    }

    void LowEnd::processa (float* L, float* R, int n) noexcept
    {
        if (demanaBuidar.exchange (false))
        {
            for (int c = 0; c < 2; ++c) for (auto* b : { &lp[c][0], &lp[c][1], &hp[c][0], &hp[c][1], &sub[c], &fang[c], &mono[c] }) b->z1 = b->z2 = 0.0f;
            aF = aS = sF = sS = 0.0f;
        }
        if (n <= 0) return;
        calcula();
        const float punch = valor (Punch) / 100.0f, tight = valor (Tight) / 100.0f, esc = valor (Escalfor) / 100.0f;
        const bool ambSub = std::abs (subFet) > 0.01f, ambFang = fangFet < -0.01f, ambMono = monoFet >= 10.0f;
        const float tOut = std::pow (10.0f, valor (Sortida) / 20.0f);
        if (primer) { gOut = tOut; primer = false; }
        auto coef = [this] (double ms) { return (float) (1.0 - std::exp (-1.0 / (0.001 * ms * fs))); };
        const float kAf = coef (1.0), kAs = coef (25.0), kAr = coef (80.0), kSa = coef (2.0), kSf = coef (50.0), kSs = coef (300.0), kG = coef (2.0);
        const float drive = 1.0f + 4.0f * esc;
        const float dOut = (tOut - gOut) / (float) n;
        double eI = 0, eO = 0;
        float gUltim = 0;
        for (int i = 0; i < n; ++i)
        {
            gOut += dOut;
            float lo[2], hi[2];
            for (int c = 0; c < 2; ++c)
            {
                const float x = c == 0 ? L[i] : R[i];
                lo[c] = lp[c][1] (lp[c][0] (x));
                hi[c] = hp[c][1] (hp[c][0] (x));
            }
            eI += (double) lo[0] * lo[0] + (double) lo[1] * lo[1];
            // cop i cua dels greus (detector a la suma dels dos canals)
            float gDb = 0.0f;
            if (punch > 0.0f || tight > 0.0f)
            {
                const float d = 0.5f * (lo[0] + lo[1]), x = d * d + 1.0e-12f;
                aF += (x - aF) * (x > aF ? kAf : kAr);
                aS += (x - aS) * (x > aS ? kAs : kAr);
                sF += (x - sF) * (x > sF ? kSa : kSf);
                sS += (x - sS) * (x > sS ? kSa : kSs);
                const float dAtk = std::max (0.0f, 10.0f * std::log10 (aF / aS)), dSus = std::max (0.0f, 10.0f * std::log10 (sS / sF));
                gDb = lim (punch * dAtk - 0.8f * tight * dSus, -12.0f, 12.0f);
            }
            gCop += (std::pow (10.0f, gDb / 20.0f) - gCop) * kG;
            gUltim = gDb;
            for (int c = 0; c < 2; ++c)
            {
                float y = lo[c] * gCop;
                if (ambSub) y = sub[c] (y);
                if (esc > 0.0f) y += esc * 0.6f * (std::tanh (drive * y) / drive - y);   // harmònics senars suaus
                lo[c] = y;
            }
            eO += (double) lo[0] * lo[0] + (double) lo[1] * lo[1];
            float l = lo[0] + hi[0], r = lo[1] + hi[1];
            if (ambFang) { l = fang[0] (l); r = fang[1] (r); }
            if (ambMono)
            {
                const float m = 0.5f * (l + r), s = mono[1] (mono[0] (0.5f * (l - r)));
                l = m + s; r = m - s;
            }
            L[i] = l * gOut; R[i] = r * gOut;
        }
        gOut = tOut;
        const double a = 1.0 - std::exp (-(double) n / (0.3 * fs));
        eIn += (eI / (2.0 * n) - eIn) * a;
        eOut += (eO / (2.0 * n) - eOut) * a;
        mes.greusIn.store (eIn > 1e-12 ? (float) (10.0 * std::log10 (eIn)) : -120.0f, std::memory_order_relaxed);
        mes.greusOut.store (eOut > 1e-12 ? (float) (10.0 * std::log10 (eOut)) : -120.0f, std::memory_order_relaxed);
        mes.canviCop.store (gUltim, std::memory_order_relaxed);
    }

    InfoParametre LowEnd::infoParametre (int i) const { InfoParametre r; r.id = info (i).clau; r.nom = info (i).nom; return r; }
    float LowEnd::normal (int p, float v) noexcept { const auto& f = info (p); return lim ((v - f.mn) / (f.mx - f.mn), 0.0f, 1.0f); }
    float LowEnd::deNormal (int p, float v01) noexcept { const auto& f = info (p); return f.mn + lim (v01, 0.0f, 1.0f) * (f.mx - f.mn); }
    void LowEnd::posaParametre (int i, float v01) noexcept { posa (i, deNormal (i, v01)); }
    float LowEnd::parametre (int i) const noexcept { return normal (i, valor (i)); }
    std::string LowEnd::textParametre (int i, float v01) const
    {
        const float v = deNormal (i, v01);
        char b[48];
        switch (i)
        {
            case Freq: std::snprintf (b, sizeof b, "%d Hz", (int) std::lround (v)); break;
            case Mono: if (v < 10.0f) return "apagat"; std::snprintf (b, sizeof b, "%d Hz", (int) std::lround (v)); break;
            case Sub: case Sortida: std::snprintf (b, sizeof b, "%+.1f dB", (double) v); break;
            default: std::snprintf (b, sizeof b, "%d %%", (int) std::lround (v)); break;
        }
        std::string s (b);
        for (auto& c : s) if (c == '.') c = ',';
        return s;
    }
    void LowEnd::posa (int p, float v) noexcept
    {
        if (p < 0 || p >= numParams || ! std::isfinite (v)) return;
        par[(size_t) p].store (lim (v, infos[p].mn, infos[p].mx), std::memory_order_relaxed);
    }
    float LowEnd::valor (int p) const noexcept { return p >= 0 && p < numParams ? par[(size_t) p].load (std::memory_order_relaxed) : 0.0f; }
    std::string LowEnd::estat() const
    {
        std::string s = "mibase-lowend=1";
        char b[64];
        for (int i = 0; i < numParams; ++i) { std::snprintf (b, sizeof b, ";%s=%.6g", infos[i].clau, (double) valor (i)); s += b; }
        return s;
    }
    void LowEnd::posaEstat (const std::string& s)
    {
        if (s.rfind ("mibase-lowend=", 0) != 0) return;
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
