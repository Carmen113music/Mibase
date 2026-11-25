#include "mibase/Multibanda.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace mibase::multibanda
{
    namespace
    {
        const InfoParam globals[numGlobals] = {
            { "bandes", "Bandes", 2, 4, 4, "Quantes bandes: 2 (greus / aguts), 3 o 4." },
            { "x1", "Encreuament 1", 40, 500, 120, "On acaba la primera banda (Hz): els greus." },
            { "x2", "Encreuament 2", 200, 4000, 800, "On acaba la segona banda (Hz)." },
            { "x3", "Encreuament 3", 1500, 12000, 5000, "On acaba la tercera banda (Hz): per sobre, els aguts." },
            { "genoll", "Genoll", 0, 12, 6, "Com de suau comença a comprimir (dB): més, més musical." },
            { "mix", "Mix", 0, 100, 100, "Barreja amb el so sense comprimir (compressió en paral·lel)." },
            { "sortida", "Sortida", -12, 12, 0, "Volum de sortida (dB)." },
        };
        struct PB { const char* clau; const char* nom; float mn, mx; const char* ajuda; };
        const PB perBanda[paramsPerBanda] = {
            { "llindar", "Llindar", -48, 0, "A partir d'on comprimeix aquesta banda (dB)." },
            { "ratio", "Ratio", 1, 10, "Quant comprimeix: 2:1 és suau; més de 4:1 ja es nota molt." },
            { "atac", "Atac", 0.1f, 200, "Com de ràpid reacciona (ms): més lent deixa passar el cop." },
            { "alliberament", "Alliberament", 10, 1000, "Com de ràpid deixa de comprimir (ms)." },
            { "guany", "Guany", -12, 12, "Volum de la banda després de comprimir (dB)." },
            { "solo", "Solo", 0, 1, "Escolta només aquesta banda." },
            { "mute", "Mute", 0, 1, "Silencia aquesta banda." },
        };
        // valors per defecte per banda (greus més lents, aguts més ràpids)
        const float defBanda[maxBandes][paramsPerBanda] = {
            { -20, 2.0f, 30, 160, 0, 0, 0 },
            { -20, 2.0f, 20, 130, 0, 0, 0 },
            { -22, 2.0f, 10, 100, 0, 0, 0 },
            { -24, 2.0f, 5, 80, 0, 0, 0 },
        };
        std::array<std::string, maxBandes * paramsPerBanda> claus = []
        {
            std::array<std::string, maxBandes * paramsPerBanda> c;
            for (int b = 0; b < maxBandes; ++b)
                for (int k = 0; k < paramsPerBanda; ++k) c[(size_t) (b * paramsPerBanda + k)] = std::string (perBanda[k].clau) + std::to_string (b + 1);
            return c;
        }();
        inline float lim (float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
        constexpr double pi = 3.14159265358979323846;
        bool esLog (int p) { if (p < numGlobals) return p >= X1 && p <= X3; const int k = (p - numGlobals) % paramsPerBanda; return k == Atac || k == Alliberament; }
    }

    InfoParam info (int p)
    {
        p = std::clamp (p, 0, numParams - 1);
        if (p < numGlobals) return globals[p];
        const int b = (p - numGlobals) / paramsPerBanda, k = (p - numGlobals) % paramsPerBanda;
        const auto& f = perBanda[k];
        return { claus[(size_t) (p - numGlobals)].c_str(), f.nom, f.mn, f.mx, defBanda[b][k], f.ajuda };
    }

    const char* nomBanda (int b, int n)
    {
        static const char* const d2[2] = { "Greus", "Aguts" };
        static const char* const d3[3] = { "Greus", "Mitjos", "Aguts" };
        static const char* const d4[4] = { "Greus", "Mitjos greus", "Mitjos aguts", "Aguts" };
        if (n <= 2) return d2[std::clamp (b, 0, 1)];
        if (n == 3) return d3[std::clamp (b, 0, 2)];
        return d4[std::clamp (b, 0, 3)];
    }

    Multibanda::Multibanda() { for (int i = 0; i < numParams; ++i) par[(size_t) i].store (info (i).def); }

    void Multibanda::buidaAra() noexcept
    {
        for (auto& c : cr) for (auto& x : c) for (auto* b : { &x.lp[0], &x.lp[1], &x.hp[0], &x.hp[1] }) b->z1 = b->z2 = 0.0f;
        for (auto& c : ap) for (auto& b : c) b.z1 = b.z2 = 0.0f;
        env.fill (0.0f); gr.fill (0.0f);
    }

    void Multibanda::prepara (double f, int)
    {
        fs = f > 1000.0 ? f : 48000.0;
        fFet = { -1, -1, -1 };
        buidaAra();
        primer = true;
    }

    int Multibanda::numBandes() const noexcept { return std::clamp ((int) std::lround (valor (Bandes)), 2, maxBandes); }

    void Multibanda::calculaFiltres() noexcept
    {
        // els encreuaments, sempre en ordre i separats almenys mitja octava
        std::array<float, 3> f { valor (X1), valor (X2), valor (X3) };
        f[1] = std::max (f[1], f[0] * 1.5f);
        f[2] = std::max (f[2], f[1] * 1.5f);
        for (int k = 0; k < 3; ++k)
        {
            if (f[(size_t) k] == fFet[(size_t) k]) continue;
            fFet[(size_t) k] = f[(size_t) k];
            const double w = 2.0 * pi * std::min ((double) f[(size_t) k], fs * 0.45) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * 0.70710678);
            const double a0 = 1.0 + al;
            auto posa = [&] (Bq& b, int tipus)   // 0 LP, 1 HP, 2 AP
            {
                const float z1 = b.z1, z2 = b.z2;
                if (tipus == 0) { b.b0 = (float) ((1.0 - c) / 2.0 / a0); b.b1 = (float) ((1.0 - c) / a0); b.b2 = b.b0; }
                else if (tipus == 1) { b.b0 = (float) ((1.0 + c) / 2.0 / a0); b.b1 = (float) (-(1.0 + c) / a0); b.b2 = b.b0; }
                else { b.b0 = (float) ((1.0 - al) / a0); b.b1 = (float) (-2.0 * c / a0); b.b2 = 1.0f; }
                b.a1 = (float) (-2.0 * c / a0); b.a2 = (float) ((1.0 - al) / a0);
                b.z1 = z1; b.z2 = z2;
            };
            for (int ch = 0; ch < 2; ++ch)
            {
                auto& x = cr[(size_t) ch][(size_t) k];
                posa (x.lp[0], 0); posa (x.lp[1], 0); posa (x.hp[0], 1); posa (x.hp[1], 1);
                if (k == 1) posa (ap[(size_t) ch][0], 2);
                if (k == 2) { posa (ap[(size_t) ch][1], 2); posa (ap[(size_t) ch][2], 2); }
            }
        }
    }

    void Multibanda::processa (float* L, float* R, int n) noexcept
    {
        if (demanaBuidar.exchange (false)) buidaAra();
        if (n <= 0) return;
        calculaFiltres();
        const int nb = numBandes();
        const float knee = valor (Genoll);
        const float tOut = std::pow (10.0f, valor (Sortida) / 20.0f), tMix = valor (Mix) / 100.0f;
        if (primer) { gOut = tOut; gMix = tMix; primer = false; }
        float thr[maxBandes], rat[maxBandes], kA[maxBandes], kR[maxBandes], mk[maxBandes];
        bool actiu[maxBandes];
        bool solo = false;
        for (int b = 0; b < nb; ++b) solo = solo || valor (index (b, Solo)) >= 0.5f;
        for (int b = 0; b < nb; ++b)
        {
            thr[b] = valor (index (b, Llindar));
            rat[b] = valor (index (b, Ratio));
            kA[b] = 1.0f - std::exp (-1.0f / (0.001f * valor (index (b, Atac)) * (float) fs));
            kR[b] = 1.0f - std::exp (-1.0f / (0.001f * valor (index (b, Alliberament)) * (float) fs));
            mk[b] = valor (index (b, Guany));
            actiu[b] = solo ? valor (index (b, Solo)) >= 0.5f : valor (index (b, Mute)) < 0.5f;
        }
        const float kDet = 1.0f - std::exp (-1.0f / (0.005f * (float) fs));
        const float inv = 1.0f / (float) n, dOut = (tOut - gOut) * inv, dMix = (tMix - gMix) * inv;
        float grMax[maxBandes] {};
        for (int i = 0; i < n; ++i)
        {
            gOut += dOut; gMix += dMix;
            float bl[maxBandes] {}, br[maxBandes] {};
            for (int ch = 0; ch < 2; ++ch)
            {
                float* bnd = ch == 0 ? bl : br;
                const float x = ch == 0 ? L[i] : R[i];
                auto& c = cr[(size_t) ch];
                auto& a = ap[(size_t) ch];
                float b0 = c[0].lp[1] (c[0].lp[0] (x));
                float r = c[0].hp[1] (c[0].hp[0] (x));
                if (nb >= 3) b0 = a[0] (b0);
                if (nb == 4) b0 = a[1] (b0);
                bnd[0] = b0;
                if (nb == 2) { bnd[1] = r; continue; }
                float b1 = c[1].lp[1] (c[1].lp[0] (r));
                const float r2 = c[1].hp[1] (c[1].hp[0] (r));
                if (nb == 4) b1 = a[2] (b1);
                bnd[1] = b1;
                if (nb == 3) { bnd[2] = r2; continue; }
                bnd[2] = c[2].lp[1] (c[2].lp[0] (r2));
                bnd[3] = c[2].hp[1] (c[2].hp[0] (r2));
            }
            float wl = 0, wr = 0, dl = 0, dr = 0;
            for (int b = 0; b < nb; ++b)
            {
                dl += bl[b]; dr += br[b];
                const float e = std::max (bl[b] * bl[b], br[b] * br[b]);
                env[(size_t) b] += (e - env[(size_t) b]) * kDet;
                const float lvl = 10.0f * std::log10 (env[(size_t) b] + 1.0e-12f);
                const float over = lvl - thr[b];
                float t = 0.0f;
                if (2.0f * over <= -knee) t = 0.0f;
                else if (2.0f * std::abs (over) <= knee && knee > 0.0f) t = (1.0f / rat[b] - 1.0f) * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
                else t = (1.0f / rat[b] - 1.0f) * over;
                auto& g = gr[(size_t) b];
                g += (t - g) * (t < g ? kA[b] : kR[b]);
                grMax[b] = std::min (grMax[b], g);
                if (! actiu[b]) continue;
                const float lin = std::pow (10.0f, (g + mk[b]) / 20.0f);
                wl += bl[b] * lin; wr += br[b] * lin;
            }
            const float m = solo ? 1.0f : gMix;
            L[i] = (dl + (wl - dl) * m) * gOut;
            R[i] = (dr + (wr - dr) * m) * gOut;
        }
        gOut = tOut; gMix = tMix;
        for (int b = 0; b < maxBandes; ++b)
        {
            mes.reduccio[(size_t) b].store (b < nb ? -grMax[b] : 0.0f, std::memory_order_relaxed);
            mes.nivell[(size_t) b].store (b < nb ? 10.0f * std::log10 (env[(size_t) b] + 1.0e-12f) : -120.0f, std::memory_order_relaxed);
        }
    }

    InfoParametre Multibanda::infoParametre (int i) const
    {
        InfoParametre r;
        const auto f = info (i);
        r.id = f.clau;
        r.nom = f.nom;
        if (i >= numGlobals) r.nom = std::string (f.nom) + " " + std::to_string ((i - numGlobals) / paramsPerBanda + 1);
        if (i == Bandes) r.passos = 3;
        if (i >= numGlobals) { const int k = (i - numGlobals) % paramsPerBanda; if (k == Solo || k == Mute) r.passos = 2; }
        return r;
    }
    float Multibanda::normal (int p, float v) noexcept
    {
        const auto f = info (p);
        if (esLog (p)) return lim (std::log (std::max (v, f.mn) / f.mn) / std::log (f.mx / f.mn), 0.0f, 1.0f);
        return lim ((v - f.mn) / (f.mx - f.mn), 0.0f, 1.0f);
    }
    float Multibanda::deNormal (int p, float v01) noexcept
    {
        const auto f = info (p);
        v01 = lim (v01, 0.0f, 1.0f);
        if (esLog (p)) return f.mn * std::pow (f.mx / f.mn, v01);
        return f.mn + v01 * (f.mx - f.mn);
    }
    void Multibanda::posaParametre (int i, float v01) noexcept { posa (i, deNormal (i, v01)); }
    float Multibanda::parametre (int i) const noexcept { return normal (i, valor (i)); }
    std::string Multibanda::textParametre (int i, float v01) const
    {
        const float v = deNormal (i, v01);
        char b[48];
        if (i < numGlobals)
            switch (i)
            {
                case Bandes: std::snprintf (b, sizeof b, "%d", (int) std::lround (v)); break;
                case X1: case X2: case X3: if (v >= 1000.0f) std::snprintf (b, sizeof b, "%.1f kHz", (double) v / 1000.0); else std::snprintf (b, sizeof b, "%d Hz", (int) std::lround (v)); break;
                case Mix: std::snprintf (b, sizeof b, "%d %%", (int) std::lround (v)); break;
                case Sortida: std::snprintf (b, sizeof b, "%+.1f dB", (double) v); break;
                default: std::snprintf (b, sizeof b, "%.1f dB", (double) v); break;
            }
        else
            switch ((i - numGlobals) % paramsPerBanda)
            {
                case Ratio: std::snprintf (b, sizeof b, "%.1f:1", (double) v); break;
                case Atac: case Alliberament: std::snprintf (b, sizeof b, v < 10.0f ? "%.1f ms" : "%.0f ms", (double) v); break;
                case Guany: std::snprintf (b, sizeof b, "%+.1f dB", (double) v); break;
                case Solo: case Mute: return v >= 0.5f ? "sí" : "no";
                default: std::snprintf (b, sizeof b, "%.1f dB", (double) v); break;
            }
        std::string s (b);
        for (auto& c : s) if (c == '.') c = ',';
        return s;
    }
    void Multibanda::posa (int p, float v) noexcept
    {
        if (p < 0 || p >= numParams || ! std::isfinite (v)) return;
        const auto f = info (p);
        if (p == Bandes) v = std::round (v);
        if (p >= numGlobals) { const int k = (p - numGlobals) % paramsPerBanda; if (k == Solo || k == Mute) v = std::round (v); }
        par[(size_t) p].store (lim (v, f.mn, f.mx), std::memory_order_relaxed);
    }
    float Multibanda::valor (int p) const noexcept { return p >= 0 && p < numParams ? par[(size_t) p].load (std::memory_order_relaxed) : 0.0f; }
    std::string Multibanda::estat() const
    {
        std::string s = "mibase-multibanda=1";
        char b[64];
        for (int i = 0; i < numParams; ++i) { std::snprintf (b, sizeof b, ";%s=%.6g", info (i).clau, (double) valor (i)); s += b; }
        return s;
    }
    void Multibanda::posaEstat (const std::string& s)
    {
        if (s.rfind ("mibase-multibanda=", 0) != 0) return;
        for (int i = 0; i < numParams; ++i) posa (i, info (i).def);
        size_t a = 0;
        while (a < s.size())
        {
            size_t b = s.find (';', a);
            if (b == std::string::npos) b = s.size();
            const auto parell = s.substr (a, b - a);
            const auto eq = parell.find ('=');
            if (eq != std::string::npos)
                for (int i = 0; i < numParams; ++i)
                {
                    const auto f = info (i);
                    if (parell.compare (0, eq, f.clau) == 0 && std::string (f.clau).size() == eq) { posa (i, (float) std::strtod (parell.c_str() + eq + 1, nullptr)); break; }
                }
            a = b + 1;
        }
    }
}
