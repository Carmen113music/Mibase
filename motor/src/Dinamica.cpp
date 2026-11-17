#include "mibase/Dinamica.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace mibase::dinamica
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;
        const char* const noms[numModes] = { "Compressor", "Gate", "Expander", "De-esser" };
        const InfoParam infos[numParams] = {
            { "mode", "Mode", 0, numModes - 1, 0, true, false, "Compressor, gate, expander o de-esser." },
            { "llindar", "Threshold", -60, 0, -18, false, false, "A partir d'on actua (dB)." },
            { "ratio", "Ratio", 1, 20, 4, false, true, "Com de fort actua: 4 = per cada 4 dB que passen del llindar, en surt 1." },
            { "atac", "Attack", 0.1f, 100, 5, false, true, "Com de ràpid reacciona (ms)." },
            { "alliberament", "Release", 5, 2000, 150, false, true, "Com de ràpid torna (ms)." },
            { "genoll", "Knee", 0, 12, 3, false, false, "Entrada suau al llindar (dB)." },
            { "rang", "Range", 0, 80, 40, false, false, "Gate i expander: com a molt, quants dB s'abaixa." },
            { "guany", "Makeup", -12, 24, 0, false, false, "Guany de sortida (dB)." },
            { "detector", "Detector", 0, 1, 0, true, false, "Peak: cada cop. RMS: el volum mitjà." },
            { "hpf", "SC Low Cut", 20, 2000, 20, false, true, "El detector no escolta els greus per sota d'aquí." },
            { "lpf", "SC High Cut", 500, 20000, 20000, false, true, "El detector no escolta els aguts per sobre d'aquí." },
            { "freq", "Freq. de-esser", 2000, 12000, 6000, false, true, "De-esser: a partir d'on hi ha les sibilants (S, X, Z)." },
            { "mix", "Mix", 0, 100, 100, false, false, "100 %: només el so processat. Menys: compressió en paral·lel." },
            { "escolta", "Escolta SC", 0, 1, 0, true, false, "Escolta el que sent el detector (per ajustar els filtres)." },
        };
        inline float lim (float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
        inline float aDb (float g) { return g > 1.0e-9f ? 20.0f * std::log10 (g) : -180.0f; }
        inline float deDb (float d) { return std::pow (10.0f, d / 20.0f); }
        std::string num (double x, int d)
        {
            char b[32];
            std::snprintf (b, sizeof b, "%.*f", d, x);
            std::string s = b;
            for (auto& c : s) if (c == '.') c = ',';
            return s;
        }
        void coefs (float& b0, float& b1, float& b2, float& a1, float& a2, bool alt, double f, double fs)
        {
            const double w = 2 * pi * std::min (f, fs * 0.45) / fs, c = std::cos (w), al = std::sin (w) / (2 * 0.7071), a0 = 1 + al;
            if (alt) { b0 = (float) ((1 + c) / 2 / a0); b1 = (float) (-(1 + c) / a0); b2 = b0; }
            else { b0 = (float) ((1 - c) / 2 / a0); b1 = (float) ((1 - c) / a0); b2 = b0; }
            a1 = (float) (-2 * c / a0);
            a2 = (float) ((1 - al) / a0);
        }
    }

    const char* nomMode (int m) { return noms[std::clamp (m, 0, numModes - 1)]; }
    const InfoParam& info (int p) { return infos[std::clamp (p, 0, numParams - 1)]; }

    std::string textValor (int p, float v)
    {
        switch (p)
        {
            case ModeP: return nomMode ((int) std::lround (v));
            case Llindar: case Genoll: return num (v, 1) + " dB";
            case Guany: return (v > 0.05f ? "+" : "") + num (v, 1) + " dB";
            case Rang: return "-" + num (v, 0) + " dB";
            case Ratio: return num (v, v < 10 ? 1 : 0) + ":1";
            case Atac: case Alliberament: return v < 10 ? num (v, 1) + " ms" : num (v, 0) + " ms";
            case Detector: return v > 0.5f ? "RMS" : "Peak";
            case FiltreGreus: return v <= 20.5f ? "Off" : (v >= 1000 ? num (v / 1000.0, 1) + " kHz" : num (v, 0) + " Hz");
            case FiltreAguts: return v >= 19999.0f ? "Off" : (v >= 1000 ? num (v / 1000.0, 1) + " kHz" : num (v, 0) + " Hz");
            case FreqDeEsser: return num (v / 1000.0, 1) + " kHz";
            case Mix: return num (v, 0) + " %";
            default: return v > 0.5f ? "Sí" : "No";
        }
    }

    float Dinamica::Bq::operator() (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    Dinamica::Dinamica()
    {
        for (int i = 0; i < numParams; ++i) par[(size_t) i].store (infos[i].def);
    }

    void Dinamica::prepara (double nouFs, int)
    {
        fs = nouFs > 0.0 ? nouFs : 48000.0;
        fHP = fLP = fDs = -1.0f;
        env = ms = gr = hold = 0.0f;
        dHP = dLP = Bq {};
        dsHP[0] = dsHP[1] = Bq {};
        primer = true;
    }

    void Dinamica::posa (int p, float v) noexcept
    {
        if (p < 0 || p >= numParams || ! std::isfinite (v)) return;
        v = lim (v, infos[p].mn, infos[p].mx);
        if (infos[p].enter) v = (float) std::lround (v);
        par[(size_t) p].store (v, std::memory_order_relaxed);
    }

    float Dinamica::valor (int p) const noexcept { return p >= 0 && p < numParams ? par[(size_t) p].load (std::memory_order_relaxed) : 0.0f; }

    float Dinamica::normal (int p, float v) noexcept
    {
        const auto& f = info (p);
        if (f.log) return lim ((float) (std::log (v / f.mn) / std::log (f.mx / f.mn)), 0.0f, 1.0f);
        return lim ((v - f.mn) / (f.mx - f.mn), 0.0f, 1.0f);
    }

    float Dinamica::deNormal (int p, float x) noexcept
    {
        const auto& f = info (p);
        x = lim (x, 0.0f, 1.0f);
        return f.log ? f.mn * std::pow (f.mx / f.mn, x) : f.mn + (f.mx - f.mn) * x;
    }

    InfoParametre Dinamica::infoParametre (int i) const
    {
        const auto& f = info (i);
        InfoParametre r;
        r.id = f.clau;
        r.nom = f.nom;
        r.passos = f.enter ? (int) (f.mx - f.mn) + 1 : 0;
        r.automatitzable = i != Escolta;
        return r;
    }

    void Dinamica::posaParametre (int i, float v01) noexcept { posa (i, deNormal (i, v01)); }
    float Dinamica::parametre (int i) const noexcept { return normal (i, valor (i)); }
    std::string Dinamica::textParametre (int i, float v01) const
    {
        float v = deNormal (i, v01);
        if (info (i).enter) v = (float) std::lround (v);
        return textValor (i, v);
    }

    std::string Dinamica::estat() const
    {
        std::string s = "mibase-dinamica=1";
        char b[64];
        for (int i = 0; i < numParams; ++i)
        {
            std::snprintf (b, sizeof b, ";%s=%.6g", infos[i].clau, (double) valor (i));
            s += b;
        }
        return s;
    }

    void Dinamica::posaEstat (const std::string& s)
    {
        if (s.rfind ("mibase-dinamica=", 0) != 0) return;
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

    float Dinamica::reduccio (float x, int mode) const noexcept
    {
        const float thr = valor (Llindar), ratio = std::max (1.0f, valor (Ratio)), knee = valor (Genoll), rang = valor (Rang);
        switch (mode)
        {
            case Compressor: case DeEsser:
            {
                const float over = x - thr;
                const float pend = 1.0f - 1.0f / ratio;
                if (knee > 0.01f && std::abs (over) <= knee * 0.5f)
                {
                    const float t = over + knee * 0.5f;
                    return pend * t * t / (2.0f * knee);
                }
                return over > 0.0f ? pend * over : 0.0f;
            }
            case Expander:
            {
                const float under = thr - x;
                return under > 0.0f ? std::min (rang, (ratio - 1.0f) * under) : 0.0f;
            }
            default:   // gate
                return x < thr ? rang : 0.0f;
        }
    }

    float Dinamica::reduccioPer (float nivellDb) const noexcept { return reduccio (nivellDb, (int) valor (ModeP)); }

    void Dinamica::processa (float* L, float* R, int n) noexcept
    {
        if (n <= 0) return;
        if (demanaBuidar.exchange (false)) { env = ms = gr = hold = 0.0f; dHP.z1 = dHP.z2 = dLP.z1 = dLP.z2 = 0.0f; }
        const float* kL = scL;
        const float* kR = scR;
        mes.sidechain.store (kL != nullptr, std::memory_order_relaxed);
        const int mode = (int) valor (ModeP);
        const float fsf = (float) fs;
        // Filtres del detector (només es recalculen si canvien)
        const float hp = mode == DeEsser ? valor (FreqDeEsser) : valor (FiltreGreus), lp = mode == DeEsser ? 20000.0f : valor (FiltreAguts);
        const bool ambHP = hp > 20.5f, ambLP = lp < 19999.0f;
        if (hp != fHP) { fHP = hp; if (ambHP) coefs (dHP.b0, dHP.b1, dHP.b2, dHP.a1, dHP.a2, true, hp, fs); }
        if (lp != fLP) { fLP = lp; if (ambLP) coefs (dLP.b0, dLP.b1, dLP.b2, dLP.a1, dLP.a2, false, lp, fs); }
        const float fd = valor (FreqDeEsser);
        if (mode == DeEsser && fd != fDs)
        {
            fDs = fd;
            for (auto& b : dsHP) coefs (b.b0, b.b1, b.b2, b.a1, b.a2, false, fd, fs);   // passa-baix: els aguts són el que en queda (x - baix)
        }
        const float aA = std::exp (-1.0f / (std::max (0.05f, valor (Atac)) * 0.001f * fsf));
        const float aR = std::exp (-1.0f / (std::max (1.0f, valor (Alliberament)) * 0.001f * fsf));
        const float aEnv = std::exp (-1.0f / (0.004f * fsf));      // el pic cau en 4 ms (l'atac i el retorn són del guany)
        const float aRms = 1.0f - std::exp (-1.0f / (0.01f * fsf));
        const bool rms = valor (Detector) > 0.5f, escolta = valor (Escolta) > 0.5f;
        const float holdGate = 0.025f * fsf;
        const float gObj = deDb (valor (Guany)), mObj = valor (Mix) / 100.0f;
        if (primer) { sGuany = gObj; sMix = mObj; primer = false; }
        const float g0 = sGuany, m0 = sMix;
        const float aS = 1.0f - std::exp (-(float) n / (0.02f * fsf));
        sGuany += (gObj - sGuany) * aS;
        sMix += (mObj - sMix) * aS;
        const bool expansio = mode == Gate || mode == Expander;
        float maxRed = 0.0f, picE = 0.0f, picS = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) (i + 1) / (float) n;
            const float xL = L[i], xR = R[i];
            float k = kL != nullptr ? 0.5f * (kL[i] + kR[i]) : 0.5f * (xL + xR);
            if (! std::isfinite (k)) k = 0.0f;
            if (ambHP) k = dHP (k);
            if (ambLP) k = dLP (k);
            float nivell;
            const float a = std::fabs (k);
            if (rms) { ms += (k * k - ms) * aRms; nivell = std::sqrt (ms); }
            else { env = a > env ? a : env * aEnv; nivell = env; }
            picE = std::max (picE, a);
            float obj = reduccio (aDb (nivell), mode);
            if (mode == Gate)
            {
                if (obj < 0.5f) hold = holdGate;
                else if (hold > 0.0f) { hold -= 1.0f; obj = 0.0f; }
            }
            // Més reducció: el compressor fa servir l'atac; el gate/expander, el retorn (i a l'inrevés)
            const bool baixa = obj > gr;
            const float c = expansio ? (baixa ? aR : aA) : (baixa ? aA : aR);
            gr = obj + (gr - obj) * c;
            maxRed = std::max (maxRed, gr);
            const float g = deDb (-gr);
            float yL, yR;
            if (escolta) { yL = yR = k; }
            else if (mode == DeEsser)
            {
                yL = xL - (1.0f - g) * (xL - dsHP[0] (xL));
                yR = xR - (1.0f - g) * (xR - dsHP[1] (xR));
            }
            else { yL = xL * g; yR = xR * g; }
            const float gm = g0 + (sGuany - g0) * t, mx = m0 + (sMix - m0) * t;
            if (! escolta)
            {
                yL = (yL * gm) * mx + xL * (1.0f - mx);
                yR = (yR * gm) * mx + xR * (1.0f - mx);
            }
            L[i] = yL;
            R[i] = yR;
            picS = std::max ({ picS, std::fabs (yL), std::fabs (yR) });
        }
        if (std::fabs (env) < 1.0e-20f) env = 0.0f;
        if (std::fabs (ms) < 1.0e-20f) ms = 0.0f;
        mes.reduccio.store (maxRed, std::memory_order_relaxed);
        mes.entrada.store (picE, std::memory_order_relaxed);
        mes.sortida.store (picS, std::memory_order_relaxed);
    }
}
