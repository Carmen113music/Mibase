#include "mibase/Transitoris.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace mibase::transitoris
{
    namespace
    {
        const char* const nomsFocus[numFocus] = { "Tot", "Greus (bombo)", "Mitjos (caixa)", "Aguts (plats)" };
        const InfoParam infos[numParams] = {
            { "atac", "Atac", -100, 100, 0, "Positiu: més cop al principi de cada so. Negatiu: el suavitza (menys punxant)." },
            { "sustain", "Sustain", -100, 100, 0, "Positiu: allarga la cua (més ambient i cos). Negatiu: l'escurça (més sec i ajustat)." },
            { "velocitat", "Velocitat", 0, 100, 50, "Com de curt és l'atac que es detecta: més alt, només els cops més ràpids." },
            { "focus", "Focus", 0, numFocus - 1, Tot, "Quina zona escolta el detector: tot, el bombo, la caixa o els plats. El canvi s'aplica a tot el so." },
            { "mix", "Mix", 0, 100, 100, "Barreja amb el so original." },
            { "sortida", "Sortida", -12, 12, 0, "Volum de sortida (dB)." },
        };
        inline float lim (float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
        constexpr double pi = 3.14159265358979323846;
    }

    const char* nomFocus (int f) { return nomsFocus[std::clamp (f, 0, numFocus - 1)]; }
    const InfoParam& info (int p) { return infos[std::clamp (p, 0, numParams - 1)]; }

    Transitoris::Transitoris() { for (int i = 0; i < numParams; ++i) par[(size_t) i].store (infos[i].def); }

    void Transitoris::prepara (double f, int)
    {
        fs = f > 1000.0 ? f : 48000.0;
        focusFet = -1;
        aFast = aSlow = sFast = sSlow = 0.0f;
        primer = true;
    }

    void Transitoris::dissenyaFocus (int f) noexcept
    {
        auto bq = [this] (Bq& b, bool passaAlts, double freq)
        {
            const double w = 2.0 * pi * freq / fs, c = std::cos (w), al = std::sin (w) / (2.0 * 0.7071), a0 = 1.0 + al;
            b = {};
            if (passaAlts) { b.b0 = (float) ((1.0 + c) / 2.0 / a0); b.b1 = (float) (-(1.0 + c) / a0); }
            else { b.b0 = (float) ((1.0 - c) / 2.0 / a0); b.b1 = (float) ((1.0 - c) / a0); }
            b.b2 = b.b0; b.a1 = (float) (-2.0 * c / a0); b.a2 = (float) ((1.0 - al) / a0);
        };
        hp = {}; lp = {};
        if (f == Greus) bq (lp, false, 150.0);
        else if (f == Mitjos) { bq (hp, true, 150.0); bq (lp, false, 3000.0); }
        else if (f == Aguts) bq (hp, true, 5000.0);
        focusFet = f;
    }

    void Transitoris::processa (float* L, float* R, int n) noexcept
    {
        if (demanaBuidar.exchange (false)) { aFast = aSlow = sFast = sSlow = 0.0f; hp.z1 = hp.z2 = lp.z1 = lp.z2 = 0.0f; }
        if (n <= 0) return;
        const int focus = (int) std::lround (valor (FocusP));
        if (focus != focusFet) dissenyaFocus (focus);
        const float atac = valor (Atac) / 100.0f, sus = valor (Sustain) / 100.0f;
        const float tOut = std::pow (10.0f, valor (Sortida) / 20.0f), tMix = valor (Mix) / 100.0f;
        if (primer) { gOut = tOut; gMix = tMix; primer = false; }
        auto coef = [this] (double ms) { return (float) (1.0 - std::exp (-1.0 / (0.001 * ms * fs))); };
        const float kAf = coef (0.5), kAs = coef (40.0 - 35.0 * valor (Velocitat) / 100.0), kAr = coef (60.0);
        const float kSa = coef (1.0), kSf = coef (40.0), kSs = coef (300.0);
        const float kG = coef (1.0);
        const bool filtraHp = focus == Mitjos || focus == Aguts, filtraLp = focus == Greus || focus == Mitjos;
        const float inv = 1.0f / (float) n, dOut = (tOut - gOut) * inv, dMix = (tMix - gMix) * inv;
        float gMax = 0.0f, gUltim = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            gOut += dOut; gMix += dMix;
            float d = 0.5f * (L[i] + R[i]);
            if (filtraHp) d = hp (d);
            if (filtraLp) d = lp (d);
            const float x = d * d + 1.0e-12f;
            aFast += (x - aFast) * (x > aFast ? kAf : kAr);
            aSlow += (x - aSlow) * (x > aSlow ? kAs : kAr);
            sFast += (x - sFast) * (x > sFast ? kSa : kSf);
            sSlow += (x - sSlow) * (x > sSlow ? kSa : kSs);
            const float dAtk = std::max (0.0f, 10.0f * std::log10 (aFast / aSlow));
            const float dSus = std::max (0.0f, 10.0f * std::log10 (sSlow / sFast));
            const float gDb = lim (atac * dAtk + 0.6f * sus * dSus, -15.0f, 15.0f);
            const float g = std::pow (10.0f, gDb / 20.0f);
            gSuau += (g - gSuau) * kG;
            const float l = L[i], r = R[i];
            L[i] = (l + (l * gSuau - l) * gMix) * gOut;
            R[i] = (r + (r * gSuau - r) * gMix) * gOut;
            gUltim = gDb;
            if (std::abs (gDb) > std::abs (gMax)) gMax = gDb;
        }
        gOut = tOut; gMix = tMix;
        mes.guany.store (gUltim, std::memory_order_relaxed);
        if (std::abs (gMax) > std::abs (mes.guanyMax.load (std::memory_order_relaxed))) mes.guanyMax.store (gMax, std::memory_order_relaxed);
    }

    InfoParametre Transitoris::infoParametre (int i) const
    {
        InfoParametre r;
        r.id = info (i).clau;
        r.nom = info (i).nom;
        if (i == FocusP) r.passos = numFocus;
        return r;
    }
    float Transitoris::normal (int p, float v) noexcept { const auto& f = info (p); return lim ((v - f.mn) / (f.mx - f.mn), 0.0f, 1.0f); }
    float Transitoris::deNormal (int p, float v01) noexcept { const auto& f = info (p); return f.mn + lim (v01, 0.0f, 1.0f) * (f.mx - f.mn); }
    void Transitoris::posaParametre (int i, float v01) noexcept { posa (i, deNormal (i, v01)); }
    float Transitoris::parametre (int i) const noexcept { return normal (i, valor (i)); }
    std::string Transitoris::textParametre (int i, float v01) const
    {
        const float v = deNormal (i, v01);
        char b[48];
        switch (i)
        {
            case FocusP: return nomFocus ((int) std::lround (v));
            case Sortida: std::snprintf (b, sizeof b, "%+.1f dB", (double) v); break;
            case Atac: case Sustain: std::snprintf (b, sizeof b, "%+d %%", (int) std::lround (v)); break;
            default: std::snprintf (b, sizeof b, "%d %%", (int) std::lround (v)); break;
        }
        std::string s (b);
        for (auto& c : s) if (c == '.') c = ',';
        return s;
    }
    void Transitoris::posa (int p, float v) noexcept
    {
        if (p < 0 || p >= numParams || ! std::isfinite (v)) return;
        if (p == FocusP) v = std::round (v);
        par[(size_t) p].store (lim (v, infos[p].mn, infos[p].mx), std::memory_order_relaxed);
    }
    float Transitoris::valor (int p) const noexcept { return p >= 0 && p < numParams ? par[(size_t) p].load (std::memory_order_relaxed) : 0.0f; }
    std::string Transitoris::estat() const
    {
        std::string s = "mibase-transitoris=1";
        char b[64];
        for (int i = 0; i < numParams; ++i) { std::snprintf (b, sizeof b, ";%s=%.6g", infos[i].clau, (double) valor (i)); s += b; }
        return s;
    }
    void Transitoris::posaEstat (const std::string& s)
    {
        if (s.rfind ("mibase-transitoris=", 0) != 0) return;
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
