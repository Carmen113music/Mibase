#include "mibase/Estereo.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace mibase::estereo
{
    namespace
    {
        const InfoParam infos[numParams] = {
            { "amplada", "Amplada", 0, 200, 100, "L'amplada de tot (el Side): 0 % = mono, 100 % = tal com és, 200 % = el doble d'ample." },
            { "ampgreus", "Amplada greus", 0, 200, 100, "L'amplada només dels greus (per sota del primer encreuament). Als greus, millor tancar que obrir." },
            { "ampmitjos", "Amplada mitjos", 0, 200, 100, "L'amplada dels mitjos (guitarres, teclats)." },
            { "ampaguts", "Amplada aguts", 0, 200, 100, "L'amplada dels aguts (pads, plats, reverbs): és on obrir sona més natural." },
            { "creu1", "Greus / mitjos", 80, 600, 250, "On acaben els greus i comencen els mitjos (Hz)." },
            { "creu2", "Mitjos / aguts", 1500, 10000, 4000, "On acaben els mitjos i comencen els aguts (Hz)." },
            { "monogreus", "Mono als greus", 0, 300, 0, "Per sota d'aquesta freqüència, tot al mig (mono). 0 = apagat. Habitual: 80–150 Hz." },
            { "mid", "Mid", -12, 12, 0, "El volum del que és al mig (dB): veu, bombo, baix i caixa." },
            { "side", "Side", -12, 12, 0, "El volum del que és als costats (dB)." },
            { "balanc", "Balanç", -100, 100, 0, "Cap a l'esquerra o cap a la dreta." },
            { "mono", "Escolta en mono", 0, 1, 0, "Per comprovar com sona en un mòbil o un altaveu d'un sol con." },
            { "sortida", "Sortida", -12, 12, 0, "Volum de sortida (dB)." },
        };
        inline float lim (float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
        inline float dbALin (float db) { return std::pow (10.0f, db / 20.0f); }
        constexpr double pi = 3.14159265358979323846;
    }

    const InfoParam& info (int p) { return infos[std::clamp (p, 0, numParams - 1)]; }

    Estereo::Estereo() { for (int i = 0; i < numParams; ++i) par[(size_t) i].store (infos[i].def); }

    void Estereo::prepara (double f, int)
    {
        fs = f > 1000.0 ? f : 48000.0;
        fC1 = fC2 = -1; fMono = -2;
        lp1 = {}; hp2 = {}; hpMono[0] = hpMono[1] = {};
        for (int q = 0; q < 2; ++q) ll[q] = rr[q] = lr[q] = mm[q] = ss[q] = 0.0;
        primer = true;
    }

    void Estereo::calcula() noexcept
    {
        auto biquad = [this] (Bq& b, bool passaAlts, float f)
        {
            const double w = 2.0 * pi * std::clamp ((double) f, 10.0, fs * 0.45) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * 0.70710678);
            const double a0 = 1.0 + al;
            const float z1 = b.z1, z2 = b.z2;
            if (passaAlts) { b.b0 = (float) ((1.0 + c) / 2.0 / a0); b.b1 = (float) (-(1.0 + c) / a0); b.b2 = b.b0; }
            else { b.b0 = (float) ((1.0 - c) / 2.0 / a0); b.b1 = (float) ((1.0 - c) / a0); b.b2 = b.b0; }
            b.a1 = (float) (-2.0 * c / a0); b.a2 = (float) ((1.0 - al) / a0);
            b.z1 = z1; b.z2 = z2;
        };
        const float c1 = valor (Creu1), c2 = std::max (valor (Creu2), c1 * 1.5f), mo = valor (MonoGreus);
        if (c1 != fC1) { biquad (lp1, false, c1); fC1 = c1; }
        if (c2 != fC2) { biquad (hp2, true, c2); fC2 = c2; }
        if (mo != fMono) { if (mo >= 10.0f) { biquad (hpMono[0], true, mo); biquad (hpMono[1], true, mo); } fMono = mo; }
    }

    void Estereo::processa (float* L, float* R, int n) noexcept
    {
        if (demanaBuidar.exchange (false)) { for (auto* b : { &lp1, &hp2, &hpMono[0], &hpMono[1] }) b->z1 = b->z2 = 0.0f; }
        if (n <= 0) return;
        calcula();
        const float amp = valor (Amplada) / 100.0f;
        const float t[6] = { amp * valor (AmpGreus) / 100.0f, amp * valor (AmpMitjos) / 100.0f, amp * valor (AmpAguts) / 100.0f,
                             dbALin (valor (Mid)), dbALin (valor (Side)), dbALin (valor (Sortida)) };
        const float b = valor (Balanc) / 100.0f;
        const float tbl = b > 0.0f ? 1.0f - b : 1.0f, tbr = b < 0.0f ? 1.0f + b : 1.0f;
        const bool mono = valor (Mono) >= 0.5f, monoGreus = fMono >= 10.0f;
        if (primer) { for (int k = 0; k < 6; ++k) g[k] = t[k]; bl = tbl; br = tbr; primer = false; }
        float d[6];
        const float inv = 1.0f / (float) n;
        for (int k = 0; k < 6; ++k) d[k] = (t[k] - g[k]) * inv;
        const float dbl = (tbl - bl) * inv, dbr = (tbr - br) * inv;
        double s0[5] {}, s1[5] {};
        for (int i = 0; i < n; ++i)
        {
            for (int k = 0; k < 6; ++k) g[k] += d[k];
            bl += dbl; br += dbr;
            const float l = L[i], r = R[i];
            s0[0] += (double) l * l; s0[1] += (double) r * r; s0[2] += (double) l * r;
            const float m = 0.5f * (l + r), s = 0.5f * (l - r);
            s0[3] += (double) m * m; s0[4] += (double) s * s;
            const float sG = lp1 (s), resta = s - sG, sA = hp2 (resta), sM = resta - sA;
            float s2 = (sG * g[0] + sM * g[1] + sA * g[2]) * g[4];
            if (monoGreus) s2 = hpMono[1] (hpMono[0] (s2));
            const float m2 = m * g[3];
            float lo = (m2 + s2) * bl * g[5], ro = (m2 - s2) * br * g[5];
            if (mono) lo = ro = 0.5f * (lo + ro);
            L[i] = lo; R[i] = ro;
            s1[0] += (double) lo * lo; s1[1] += (double) ro * ro; s1[2] += (double) lo * ro;
            const float mo = 0.5f * (lo + ro), so = 0.5f * (lo - ro);
            s1[3] += (double) mo * mo; s1[4] += (double) so * so;
        }
        for (int k = 0; k < 6; ++k) g[k] = t[k];
        bl = tbl; br = tbr;
        // mesures: mitjana exponencial d'uns 300 ms
        const double a = 1.0 - std::exp (-(double) n / (0.3 * fs));
        for (int q = 0; q < 2; ++q)
        {
            const double* s = q == 0 ? s0 : s1;
            ll[q] += (s[0] - ll[q]) * a; rr[q] += (s[1] - rr[q]) * a; lr[q] += (s[2] - lr[q]) * a;
            mm[q] += (s[3] - mm[q]) * a; ss[q] += (s[4] - ss[q]) * a;
            const double den = std::sqrt (ll[q] * rr[q]);
            const float c = den > 1.0e-12 ? (float) std::clamp (lr[q] / den, -1.0, 1.0) : 1.0f;
            const float sm = mm[q] > 1.0e-12 ? (float) (10.0 * std::log10 (std::max (ss[q], 1.0e-14) / mm[q])) : -100.0f;
            (q == 0 ? mes.correlacioIn : mes.correlacioOut).store (c, std::memory_order_relaxed);
            (q == 0 ? mes.sideMidIn : mes.sideMidOut).store (sm, std::memory_order_relaxed);
        }
    }

    InfoParametre Estereo::infoParametre (int i) const
    {
        InfoParametre r;
        r.id = info (i).clau;
        r.nom = info (i).nom;
        if (i == Mono) r.passos = 2;
        return r;
    }
    float Estereo::normal (int p, float v) noexcept { const auto& f = info (p); return lim ((v - f.mn) / (f.mx - f.mn), 0.0f, 1.0f); }
    float Estereo::deNormal (int p, float v01) noexcept { const auto& f = info (p); return f.mn + lim (v01, 0.0f, 1.0f) * (f.mx - f.mn); }
    void Estereo::posaParametre (int i, float v01) noexcept { posa (i, deNormal (i, v01)); }
    float Estereo::parametre (int i) const noexcept { return normal (i, valor (i)); }
    std::string Estereo::textParametre (int i, float v01) const
    {
        const float v = deNormal (i, v01);
        char b[48];
        switch (i)
        {
            case Creu1: case Creu2: std::snprintf (b, sizeof b, "%d Hz", (int) std::lround (v)); break;
            case MonoGreus: if (v < 10.0f) return "apagat"; std::snprintf (b, sizeof b, "%d Hz", (int) std::lround (v)); break;
            case Mid: case Side: case Sortida: std::snprintf (b, sizeof b, "%+.1f dB", (double) v); break;
            case Balanc: { const int p = (int) std::lround (v); if (p == 0) return "C"; std::snprintf (b, sizeof b, "%d %s", std::abs (p), p < 0 ? "E" : "D"); break; }
            case Mono: return v >= 0.5f ? "sí" : "no";
            default: std::snprintf (b, sizeof b, "%d %%", (int) std::lround (v)); break;
        }
        std::string s (b);
        for (auto& c : s) if (c == '.') c = ',';
        return s;
    }
    void Estereo::posa (int p, float v) noexcept
    {
        if (p < 0 || p >= numParams || ! std::isfinite (v)) return;
        par[(size_t) p].store (lim (v, infos[p].mn, infos[p].mx), std::memory_order_relaxed);
    }
    float Estereo::valor (int p) const noexcept { return p >= 0 && p < numParams ? par[(size_t) p].load (std::memory_order_relaxed) : 0.0f; }
    std::string Estereo::estat() const
    {
        std::string s = "mibase-estereo=1";
        char b[64];
        for (int i = 0; i < numParams; ++i) { std::snprintf (b, sizeof b, ";%s=%.6g", infos[i].clau, (double) valor (i)); s += b; }
        return s;
    }
    void Estereo::posaEstat (const std::string& s)
    {
        if (s.rfind ("mibase-estereo=", 0) != 0) return;
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
