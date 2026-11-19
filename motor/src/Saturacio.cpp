#include "mibase/Saturacio.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace mibase::saturacio
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;
        const char* const curts[numEstils] = { "A", "E", "N", "T", "P", "Suau" };
        const char* const llargs[numEstils] = { "A · Cinta", "E · Consola", "N · Preamp", "T · Triode", "P · Pèntode", "Suau" };
        const char* const descs[numEstils] = {
            "A: com un preamp de cinta. Càlid i rodó, greus plens, aguts suaus.",
            "E: com una consola britànica dels 60. Brillant i aspre, molt present.",
            "N: com un preamp de transistors clàssic. Gruixut als mitjos, harmònics parells.",
            "T: com una vàlvula triode. Càlid, asimètric; amb drive alt, brut i gros.",
            "P: com una vàlvula pèntode. Agressiu i mossegador, el més dur dels cinc.",
            "Suau: la saturació de sempre, neutra i rodona. No canvia el to, només afegeix escalfor i cos.",
        };
        // clau, nom, min, max, defecte, enter, log, ajuda
        const InfoParam infos[numParams] = {
            { "estil", "Style", 0, numEstils - 1, 0, true, false, "El caràcter de la saturació: A, E, N, T, P (estils analògics) o Suau. Per a pedals de guitarra, el Drive." },
            { "drive", "Drive", 0, 10, 3, false, false, "Quanta saturació: de 0 (gairebé net) a 10 (+36 dB a l'entrada de la corba)." },
            { "punish", "Punish", 0, 1, 0, true, false, "Afegeix +20 dB de drive: per destrossar el so." },
            { "lowcut", "Low Cut", 20, 1000, 20, false, true, "Treu greus abans de saturar (menys fang)." },
            { "thump", "Thump", 0, 1, 0, true, false, "Un cop de greus just al tall del Low Cut." },
            { "tone", "Tone", -10, 10, 0, false, false, "Després de saturar: fosc (-) o brillant (+)." },
            { "highcut", "High Cut", 1000, 20000, 20000, false, true, "Treu aguts al final (suavitza l'aspror)." },
            { "steep", "Steep", 0, 1, 0, true, false, "El High Cut talla més fort (24 dB/oct en lloc de 12)." },
            { "mix", "Mix", 0, 100, 100, false, false, "Barreja amb el so original (saturació en paral·lel)." },
            { "sortida", "Output", -24, 12, 0, false, false, "Guany de sortida (dB)." },
            { "auto", "Auto", 0, 1, 1, true, false, "Manté el volum de sortida encara que pugis el drive (per comparar només el color)." },
        };
        inline float lim (float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
        std::string num (double x, int d)
        {
            char b[32];
            std::snprintf (b, sizeof b, "%.*f", d, x);
            std::string s = b;
            for (auto& c : s) if (c == '.') c = ',';
            return s;
        }
        std::string hz (double v) { return v >= 1000 ? num (v / 1000.0, 1) + " kHz" : num (v, 0) + " Hz"; }
    }

    const char* nomEstil (int e) { return curts[std::clamp (e, 0, numEstils - 1)]; }
    const char* nomLlargEstil (int e) { return llargs[std::clamp (e, 0, numEstils - 1)]; }
    const char* descripcioEstil (int e) { return descs[std::clamp (e, 0, numEstils - 1)]; }
    const InfoParam& info (int p) { return infos[std::clamp (p, 0, numParams - 1)]; }

    std::string textValor (int p, float v)
    {
        switch (p)
        {
            case EstilP: return nomLlargEstil ((int) std::lround (v));
            case Drive: return num (v, 1);
            case LowCut: return v <= 20.5f ? "Off" : hz (v);
            case HighCut: return v >= 19999.0f ? "Off" : hz (v);
            case Tone: return std::fabs (v) < 0.05f ? "0" : (v > 0 ? "+" : "") + num (v, 1);
            case Mix: return num (v, 0) + " %";
            case Sortida: return (v > 0.05f ? "+" : "") + num (v, 1) + " dB";
            default: return v > 0.5f ? "Sí" : "No";
        }
    }

    namespace
    {
        /** Harmònics parells que no desapareixen amb el drive alt (com el biaix d'una vàlvula): depenen del so abans del drive. */
        inline float parells (int estil, float u) noexcept
        {
            static const float k[numEstils] = { 0.06f, 0.0f, 0.18f, 0.3f, 0.0f, 0.04f };
            const float kk = k[std::clamp (estil, 0, numEstils - 1)];
            if (kk == 0.0f) return 0.0f;
            const float t = std::tanh (2.0f * u);
            return -kk * t * t;
        }
        float corbaSola (int estil, float x) noexcept;
    }

    float corba (int estil, float x, float driveDb) noexcept
    {
        return corbaSola (estil, x * std::pow (10.0f, driveDb / 20.0f)) + parells (estil, x) - parells (estil, 0.0f);
    }

    namespace
    {
    float corbaSola (int estil, float x) noexcept
    {
        switch (estil)
        {
            case A: { constexpr float b = 0.08f; return std::tanh (x + b) - std::tanh (b); }
            case E: { const float c = lim (x, -1.5f, 1.5f); return (c - c * c * c / 6.75f) * 0.9f + 0.1f * std::tanh (x); }   // cúbica (aspra) i una mica de tanh
            case N: { constexpr float b = 0.25f; return std::tanh (x + b) - std::tanh (b); }
            case T:
            {
                constexpr float kp = 1.4f, kn = 0.7f;
                return x >= 0.0f ? (1.0f - std::exp (-kp * x)) / kp : -(1.0f - std::exp (kn * x)) / kn * 0.75f + 0.25f * std::tanh (x);
            }
            case P: return x / std::sqrt (std::sqrt (1.0f + x * x * x * x));
            default: return std::tanh (x);   // Suau
        }
    }
    }

    // ---- filtres (RBJ) ----
    void Saturacio::passaAlts (Bq& b, double f, double q, double fs) noexcept
    {
        const double w = 2 * pi * std::min (f, fs * 0.45) / fs, c = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al;
        b.b0 = (float) ((1 + c) / 2 / a0); b.b1 = (float) (-(1 + c) / a0); b.b2 = b.b0; b.a1 = (float) (-2 * c / a0); b.a2 = (float) ((1 - al) / a0);
    }
    void Saturacio::passaBaixos (Bq& b, double f, double q, double fs) noexcept
    {
        const double w = 2 * pi * std::min (f, fs * 0.45) / fs, c = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al;
        b.b0 = (float) ((1 - c) / 2 / a0); b.b1 = (float) ((1 - c) / a0); b.b2 = b.b0; b.a1 = (float) (-2 * c / a0); b.a2 = (float) ((1 - al) / a0);
    }
    void Saturacio::campana (Bq& b, double f, double q, double db, double fs) noexcept
    {
        const double A = std::pow (10.0, db / 40.0), w = 2 * pi * std::min (f, fs * 0.45) / fs, c = std::cos (w), al = std::sin (w) / (2 * q), a0 = 1 + al / A;
        b.b0 = (float) ((1 + al * A) / a0); b.b1 = (float) (-2 * c / a0); b.b2 = (float) ((1 - al * A) / a0); b.a1 = b.b1; b.a2 = (float) ((1 - al / A) / a0);
    }
    void Saturacio::prestatgeAlt (Bq& b, double f, double db, double fs) noexcept
    {
        const double A = std::pow (10.0, db / 40.0), w = 2 * pi * std::min (f, fs * 0.45) / fs, c = std::cos (w), s = std::sin (w), al = s / 2 * std::sqrt (2.0), sq = 2 * std::sqrt (A) * al;
        const double a0 = (A + 1) - (A - 1) * c + sq;
        b.b0 = (float) (A * ((A + 1) + (A - 1) * c + sq) / a0); b.b1 = (float) (-2 * A * ((A - 1) + (A + 1) * c) / a0); b.b2 = (float) (A * ((A + 1) + (A - 1) * c - sq) / a0);
        b.a1 = (float) (2 * ((A - 1) - (A + 1) * c) / a0); b.a2 = (float) (((A + 1) - (A - 1) * c - sq) / a0);
    }
    void Saturacio::prestatgeBaix (Bq& b, double f, double db, double fs) noexcept
    {
        const double A = std::pow (10.0, db / 40.0), w = 2 * pi * std::min (f, fs * 0.45) / fs, c = std::cos (w), s = std::sin (w), al = s / 2 * std::sqrt (2.0), sq = 2 * std::sqrt (A) * al;
        const double a0 = (A + 1) + (A - 1) * c + sq;
        b.b0 = (float) (A * ((A + 1) - (A - 1) * c + sq) / a0); b.b1 = (float) (2 * A * ((A - 1) - (A + 1) * c) / a0); b.b2 = (float) (A * ((A + 1) - (A - 1) * c - sq) / a0);
        b.a1 = (float) (-2 * ((A - 1) + (A + 1) * c) / a0); b.a2 = (float) (((A + 1) + (A - 1) * c - sq) / a0);
    }

    Saturacio::Saturacio()
    {
        for (int i = 0; i < numParams; ++i) par[(size_t) i].store (infos[i].def);
    }

    void Saturacio::prepara (double nouFs, int)
    {
        fs = nouFs > 0.0 ? nouFs : 48000.0;
        for (auto& c : ch)
        {
            c = Canal {};
            // 8è ordre Butterworth a 0,45 × fs (a la freqüència sobremostrejada)
            static const double qs[4] = { 0.5098, 0.6013, 0.9000, 2.5629 };
            for (int k = 0; k < 4; ++k) { passaBaixos (c.up[k], 0.45 * fs, qs[k], fs * sobre); c.down[k] = c.up[k]; }
            passaAlts (c.dc, 8.0, 0.7071, fs * sobre);
        }
        cal = true;
        rmsIn = rmsOut = 0.0f;
        sAuto = 1.0f;
    }

    void Saturacio::recalcula() noexcept
    {
        const int e = (int) valor (EstilP);
        for (auto& c : ch)
        {
            const float lc = valor (LowCut);
            if (lc > 20.5f) passaAlts (c.hp, lc, valor (Thump) > 0.5f ? 2.0 : 0.7071, fs);
            else c.hp = Bq {};
            // abans i després de la corba, segons l'estil (el que dona el color)
            c.preA = c.preB = c.postA = c.postB = Bq {};
            switch (e)
            {
                case A: prestatgeBaix (c.preA, 100, 2.0, fs); passaBaixos (c.postA, 15000, 0.6, fs); prestatgeAlt (c.postB, 8000, -1.5, fs); break;
                case E: prestatgeAlt (c.preA, 2500, 5.0, fs); prestatgeAlt (c.postA, 2500, -3.0, fs); break;
                case N: campana (c.preA, 700, 0.8, 3.0, fs); campana (c.postA, 700, 0.8, -2.0, fs); prestatgeBaix (c.postB, 120, 1.0, fs); break;
                case T: prestatgeBaix (c.preA, 200, 1.5, fs); passaBaixos (c.postA, 13000, 0.6, fs); break;
                case P: campana (c.preA, 1800, 0.9, 4.0, fs); passaBaixos (c.postA, 12000, 0.7071, fs); break;
                default: break;   // Suau: sense colors
            }
            const float t = valor (Tone) * 0.6f;
            prestatgeBaix (c.toneLo, 900, -t, fs);
            prestatgeAlt (c.toneHi, 900, t, fs);
            const float hc = valor (HighCut);
            if (hc < 19999.0f)
            {
                passaBaixos (c.lp1, hc, valor (Steep) > 0.5f ? 0.5412 : 0.7071, fs);
                if (valor (Steep) > 0.5f) passaBaixos (c.lp2, hc, 1.3066, fs); else c.lp2 = Bq {};
            }
            else c.lp1 = c.lp2 = Bq {};
        }
    }

    void Saturacio::posa (int p, float v) noexcept
    {
        if (p < 0 || p >= numParams || ! std::isfinite (v)) return;
        v = lim (v, infos[p].mn, infos[p].mx);
        if (infos[p].enter) v = (float) std::lround (v);
        par[(size_t) p].store (v, std::memory_order_relaxed);
    }
    float Saturacio::valor (int p) const noexcept { return p >= 0 && p < numParams ? par[(size_t) p].load (std::memory_order_relaxed) : 0.0f; }
    float Saturacio::normal (int p, float v) noexcept
    {
        const auto& f = info (p);
        if (f.log) return lim ((float) (std::log (v / f.mn) / std::log (f.mx / f.mn)), 0.0f, 1.0f);
        return lim ((v - f.mn) / (f.mx - f.mn), 0.0f, 1.0f);
    }
    float Saturacio::deNormal (int p, float x) noexcept
    {
        const auto& f = info (p);
        x = lim (x, 0.0f, 1.0f);
        return f.log ? f.mn * std::pow (f.mx / f.mn, x) : f.mn + (f.mx - f.mn) * x;
    }
    InfoParametre Saturacio::infoParametre (int i) const
    {
        const auto& f = info (i);
        InfoParametre r;
        r.id = f.clau;
        r.nom = f.nom;
        r.passos = f.enter ? (int) (f.mx - f.mn) + 1 : 0;
        r.automatitzable = true;
        return r;
    }
    void Saturacio::posaParametre (int i, float v01) noexcept { posa (i, deNormal (i, v01)); }
    float Saturacio::parametre (int i) const noexcept { return normal (i, valor (i)); }
    std::string Saturacio::textParametre (int i, float v01) const
    {
        float v = deNormal (i, v01);
        if (info (i).enter) v = (float) std::lround (v);
        return textValor (i, v);
    }
    std::string Saturacio::estat() const
    {
        std::string s = "mibase-saturacio=1";
        char b[64];
        for (int i = 0; i < numParams; ++i)
        {
            std::snprintf (b, sizeof b, ";%s=%.6g", infos[i].clau, (double) valor (i));
            s += b;
        }
        return s;
    }
    void Saturacio::posaEstat (const std::string& s)
    {
        if (s.rfind ("mibase-saturacio=", 0) != 0) return;
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
    float Saturacio::driveDb() const noexcept { return valor (Drive) * 3.6f + (valor (Punish) > 0.5f ? 20.0f : 0.0f); }

    void Saturacio::processa (float* L, float* R, int n) noexcept
    {
        if (n <= 0) return;
        mes.blocs.fetch_add (1, std::memory_order_relaxed);
        bool canvi = cal;
        for (int p = 0; p < numParams; ++p)
            if (p != Drive && p != Mix && p != Sortida && p != Auto && p != Punish && valor (p) != darrer[p]) canvi = true;
        if (canvi)
        {
            for (int p = 0; p < numParams; ++p) darrer[p] = valor (p);
            recalcula();
            cal = false;
        }
        if (demanaBuidar.exchange (false))
            for (auto& c : ch)
                for (Bq* b : { &c.hp, &c.preA, &c.preB, &c.up[0], &c.up[1], &c.up[2], &c.up[3], &c.down[0], &c.down[1], &c.down[2], &c.down[3],
                               &c.dc, &c.postA, &c.postB, &c.toneLo, &c.toneHi, &c.lp1, &c.lp2 })
                    b->neteja();

        const int estil = (int) valor (EstilP);
        const float dDb = driveDb();
        const float gObj = std::pow (10.0f, dDb / 20.0f);
        const float mixObj = valor (Mix) * 0.01f, outObj = std::pow (10.0f, valor (Sortida) / 20.0f);
        const bool autoOn = valor (Auto) > 0.5f;
        const float sm = 1.0f - std::exp (-1.0f / (float) (0.02 * fs));
        const float smRms = 1.0f - std::exp (-1.0f / (float) (0.3 * fs));
        const float smAuto = 1.0f - std::exp (-1.0f / (float) (0.15 * fs));
        float picE = 0.0f, picS = 0.0f, picC = 0.0f;
        float* io[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            sDrive += (gObj - sDrive) * sm;
            sMix += (mixObj - sMix) * sm;
            sOut += (outObj - sOut) * sm;
            float hum[2], sec[2];
            for (int c = 0; c < 2; ++c)
            {
                auto& k = ch[c];
                const float x = io[c][i];
                sec[c] = x;
                picE = std::max (picE, std::fabs (x));
                float v = k.preB (k.preA (k.hp (x))) * sDrive;
                picC = std::max (picC, std::fabs (v));
                // ×4: zeros entre mostres, filtre d'imatge, corba, filtre d'alias i se'n queda una de cada quatre
                float y = 0.0f;
                for (int s = 0; s < sobre; ++s)
                {
                    float u = s == 0 ? v * (float) sobre : 0.0f;
                    for (auto& f : k.up) u = f (u);
                    u = k.dc (corbaSola (estil, u) + parells (estil, u / sDrive));
                    for (auto& f : k.down) u = f (u);
                    if (s == 0) y = u;
                }
                y = k.toneHi (k.toneLo (k.postB (k.postA (y))));
                y = k.lp2 (k.lp1 (y));
                hum[c] = y;
            }
            // Auto: el so saturat sona tan fort com l'original
            const float pe = 0.5f * (sec[0] * sec[0] + sec[1] * sec[1]), ps = 0.5f * (hum[0] * hum[0] + hum[1] * hum[1]);
            rmsIn += (pe - rmsIn) * smRms;
            rmsOut += (ps - rmsOut) * smRms;
            float autoObj = 1.0f;
            if (autoOn && rmsIn > 1e-8f && rmsOut > 1e-10f) autoObj = lim (std::sqrt (rmsIn / rmsOut), 0.0158f, 4.0f);   // -36 … +12 dB
            if (! autoOn || rmsIn > 1e-8f) sAuto += (autoObj - sAuto) * smAuto;
            for (int c = 0; c < 2; ++c)
            {
                float o = (sec[c] * (1.0f - sMix) + hum[c] * sAuto * sMix) * sOut;
                if (! std::isfinite (o)) o = 0.0f;
                io[c][i] = o;
                picS = std::max (picS, std::fabs (o));
            }
        }
        if (! std::isfinite (rmsIn) || ! std::isfinite (rmsOut)) { rmsIn = rmsOut = 0.0f; sAuto = 1.0f; }
        mes.entrada.store (std::max (picE, mes.entrada.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
        mes.sortida.store (std::max (picS, mes.sortida.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
        mes.nivellCorba.store (std::max (picC, mes.nivellCorba.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
        mes.autoDb.store (20.0f * std::log10 (std::max (1e-6f, sAuto)), std::memory_order_relaxed);
    }
}
