#include "mibase/Drive.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace mibase::drive
{
    namespace
    {
        using S = saturacio::Saturacio;
        constexpr double pi = 3.14159265358979323846;
        const char* const noms[numModes] = { "Crunch", "Overdrive", "Distorsió", "Fuzz" };
        const char* const descs[numModes] = {
            "Crunch: un ampli de vàlvules que comença a trencar. Respon a com toques: suau, net; fort, brut.",
            "Overdrive: com els pedals verds clàssics. Mitjos endavant, greus nets i molt sustain.",
            "Distorsió: dura i densa, per a riffs de rock i metal. Mitjos una mica buidats.",
            "Fuzz: quadrat, gruixut i salvatge, com els pedals dels 60 i 70.",
        };
        // clau, nom, min, max, defecte, enter, log, ajuda
        const InfoParam infos[numParams] = {
            { "mode", "Mode", 0, numModes - 1, 1, true, false, "Crunch, Overdrive, Distorsió o Fuzz." },
            { "gain", "Gain", 0, 10, 5, false, false, "Quanta distorsió." },
            { "tone", "Tone", 0, 10, 5, false, false, "Fosc (0) o brillant (10)." },
            { "level", "Level", -24, 12, 0, false, false, "Volum de sortida del pedal (dB)." },
            { "bass", "Bass", -10, 10, 0, false, false, "Greus després de distorsionar (dB)." },
            { "mix", "Mix", 0, 100, 100, false, false, "Barreja amb el so net (al baix, per no perdre els greus)." },
            { "gate", "Gate", -80, -30, -80, false, false, "Talla el brunzit quan no toques (dB). A -80: apagat." },
            { "tight", "Tight", 0, 1, 0, true, false, "Treu greus abans de distorsionar: més definit en riffs ràpids." },
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
        // guany mínim i màxim (dB) de cada mode, i el volum de sortida que el deixa més o menys a 0 dB
        const float gMin[numModes] = { 0, 6, 12, 6 }, gMax[numModes] = { 30, 42, 52, 50 };
        const float compensa[numModes] = { 0.25f, 0.12f, 0.3f, 0.3f };
    }

    const char* nomMode (int m) { return noms[std::clamp (m, 0, numModes - 1)]; }
    const char* descripcioMode (int m) { return descs[std::clamp (m, 0, numModes - 1)]; }
    const InfoParam& info (int p) { return infos[std::clamp (p, 0, numParams - 1)]; }
    float guanyDb (int mode, float gain) noexcept { mode = std::clamp (mode, 0, numModes - 1); return gMin[mode] + (gMax[mode] - gMin[mode]) * lim (gain, 0, 10) / 10.0f; }

    std::string textValor (int p, float v)
    {
        switch (p)
        {
            case ModeP: return nomMode ((int) std::lround (v));
            case Gain: case Tone: return num (v, 1);
            case Level: return (v > 0.05f ? "+" : "") + num (v, 1) + " dB";
            case Bass: return std::fabs (v) < 0.05f ? "0 dB" : (v > 0 ? "+" : "") + num (v, 1) + " dB";
            case Mix: return num (v, 0) + " %";
            case Gate: return v <= -79.5f ? "Off" : num (v, 0) + " dB";
            default: return v > 0.5f ? "Sí" : "No";
        }
    }

    float corba (int mode, float x) noexcept
    {
        switch (mode)
        {
            case Crunch: { constexpr float b = 0.15f; return std::tanh (x + b) - std::tanh (b) - 0.12f * std::tanh (1.5f * x) * std::tanh (1.5f * x); }
            case Overdrive: return (float) (2.0 / pi) * std::atan (x * (float) (pi / 2.0));
            case Distorsio: { const float x2 = x * x, x4 = x2 * x2; return x / std::sqrt (std::sqrt (std::sqrt (1.0f + x4 * x4))); }
            default: { constexpr float b = 0.3f; return std::tanh (x + b) - std::tanh (b); }   // Fuzz (asimètric: la polarització fa el so)
        }
    }

    Drive::Drive()
    {
        for (int i = 0; i < numParams; ++i) par[(size_t) i].store (infos[i].def);
    }

    void Drive::prepara (double nouFs, int)
    {
        fs = nouFs > 0.0 ? nouFs : 48000.0;
        for (auto& c : ch)
        {
            c = Canal {};
            static const double qs[4] = { 0.5098, 0.6013, 0.9000, 2.5629 };
            for (int k = 0; k < 4; ++k) { S::passaBaixos (c.up[k], 0.45 * fs, qs[k], fs * sobre); c.down[k] = c.up[k]; }
            S::passaAlts (c.dc, 10.0, 0.7071, fs * sobre);
        }
        cal = true;
        env = 0.0f; gGate = 1.0f;
    }

    void Drive::recalcula() noexcept
    {
        const int m = (int) valor (ModeP);
        const float to = valor (Tone);
        for (auto& c : ch)
        {
            S::passaAlts (c.tight, valor (Tight) > 0.5f ? 200.0 : 35.0, 0.7071, fs);
            c.preA = c.preB = c.postA = c.postB = Bq {};
            switch (m)
            {
                case Crunch: S::prestatgeBaix (c.preA, 300, -3.0, fs); S::campana (c.postA, 2500, 0.8, 2.0, fs); break;
                case Overdrive: S::prestatgeBaix (c.preA, 720, -12.0, fs); S::campana (c.preB, 900, 0.7, 4.0, fs); S::prestatgeBaix (c.postA, 720, 7.0, fs); break;
                case Distorsio: S::passaAlts (c.preA, 80, 0.7071, fs); S::campana (c.preB, 2000, 0.7, 3.0, fs); S::campana (c.postA, 500, 0.8, -5.0, fs); break;
                default: S::prestatgeBaix (c.preA, 150, 3.0, fs); S::campana (c.postA, 1000, 0.6, 2.0, fs); break;
            }
            // Tone: talla aguts (com el pom d'un pedal) i inclina una mica
            const double fTone = (m == Fuzz ? 1500.0 : 2000.0) * std::pow (6.0, to / 10.0);   // 2k … 12k (fuzz: 1,5k … 9k)
            S::passaBaixos (c.tone1, fTone, 0.7071, fs);
            S::prestatgeAlt (c.tone2, 3000, (to - 5.0f) * 0.8f, fs);
            S::prestatgeBaix (c.bass, 120, valor (Bass), fs);
            // un altaveu de guitarra no passa d'uns 7-8 kHz: un tall suau perquè no soni «de mosquit»
            S::passaBaixos (c.postB, 9000, 0.6, fs);
        }
    }

    void Drive::posa (int p, float v) noexcept
    {
        if (p < 0 || p >= numParams || ! std::isfinite (v)) return;
        v = lim (v, infos[p].mn, infos[p].mx);
        if (infos[p].enter) v = (float) std::lround (v);
        par[(size_t) p].store (v, std::memory_order_relaxed);
    }
    float Drive::valor (int p) const noexcept { return p >= 0 && p < numParams ? par[(size_t) p].load (std::memory_order_relaxed) : 0.0f; }
    float Drive::normal (int p, float v) noexcept
    {
        const auto& f = info (p);
        if (f.log) return lim ((float) (std::log (v / f.mn) / std::log (f.mx / f.mn)), 0.0f, 1.0f);
        return lim ((v - f.mn) / (f.mx - f.mn), 0.0f, 1.0f);
    }
    float Drive::deNormal (int p, float x) noexcept
    {
        const auto& f = info (p);
        x = lim (x, 0.0f, 1.0f);
        return f.log ? f.mn * std::pow (f.mx / f.mn, x) : f.mn + (f.mx - f.mn) * x;
    }
    InfoParametre Drive::infoParametre (int i) const
    {
        const auto& f = info (i);
        InfoParametre r;
        r.id = f.clau;
        r.nom = f.nom;
        r.passos = f.enter ? (int) (f.mx - f.mn) + 1 : 0;
        r.automatitzable = true;
        return r;
    }
    void Drive::posaParametre (int i, float v01) noexcept { posa (i, deNormal (i, v01)); }
    float Drive::parametre (int i) const noexcept { return normal (i, valor (i)); }
    std::string Drive::textParametre (int i, float v01) const
    {
        float v = deNormal (i, v01);
        if (info (i).enter) v = (float) std::lround (v);
        return textValor (i, v);
    }
    std::string Drive::estat() const
    {
        std::string s = "mibase-drive=1";
        char b[64];
        for (int i = 0; i < numParams; ++i)
        {
            std::snprintf (b, sizeof b, ";%s=%.6g", infos[i].clau, (double) valor (i));
            s += b;
        }
        return s;
    }
    void Drive::posaEstat (const std::string& s)
    {
        if (s.rfind ("mibase-drive=", 0) != 0) return;
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

    void Drive::processa (float* L, float* R, int n) noexcept
    {
        if (n <= 0) return;
        mes.blocs.fetch_add (1, std::memory_order_relaxed);
        bool canvi = cal;
        for (int p : { (int) ModeP, (int) Tone, (int) Bass, (int) Tight }) if (valor (p) != darrer[p]) canvi = true;
        if (canvi)
        {
            for (int p = 0; p < numParams; ++p) darrer[p] = valor (p);
            recalcula();
            cal = false;
        }
        if (demanaBuidar.exchange (false))
            for (auto& c : ch)
                for (Bq* b : { &c.tight, &c.preA, &c.preB, &c.up[0], &c.up[1], &c.up[2], &c.up[3], &c.down[0], &c.down[1], &c.down[2], &c.down[3],
                               &c.dc, &c.postA, &c.postB, &c.bass, &c.tone1, &c.tone2 })
                    b->neteja();
        const int m = (int) valor (ModeP);
        const float gObj = std::pow (10.0f, guanyDb (m, valor (Gain)) / 20.0f);
        const float outObj = std::pow (10.0f, valor (Level) / 20.0f) * compensa[std::clamp (m, 0, numModes - 1)];
        const float mixObj = valor (Mix) * 0.01f;
        const float thr = valor (Gate);
        const bool ambGate = thr > -79.5f;
        const float thrLin = std::pow (10.0f, thr / 20.0f);
        const float sm = 1.0f - std::exp (-1.0f / (float) (0.02 * fs));
        const float atk = std::exp (-1.0f / (float) (0.001 * fs)), rel = std::exp (-1.0f / (float) (0.05 * fs));
        const float obre = 1.0f - std::exp (-1.0f / (float) (0.001 * fs)), tanca = 1.0f - std::exp (-1.0f / (float) (0.08 * fs));
        float picE = 0.0f, picS = 0.0f;
        float* io[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            sGain += (gObj - sGain) * sm;
            sOut += (outObj - sOut) * sm;
            sMix += (mixObj - sMix) * sm;
            // gate: escolta el so que entra (abans de distorsionar)
            const float a = std::max (std::fabs (L[i]), std::fabs (R[i]));
            picE = std::max (picE, a);
            env = a > env ? a + (env - a) * atk : a + (env - a) * rel;
            const float gObjGate = ! ambGate ? 1.0f : (env > thrLin ? 1.0f : (env > thrLin * 0.5f ? (env - thrLin * 0.5f) / (thrLin * 0.5f) : 0.0f));
            gGate += (gObjGate - gGate) * (gObjGate > gGate ? obre : tanca);
            if (gObjGate <= 0.0f && gGate < 1e-3f) gGate = 0.0f;   // tancat del tot
            for (int c = 0; c < 2; ++c)
            {
                auto& k = ch[c];
                const float x = io[c][i];
                float v = k.preB (k.preA (k.tight (x))) * sGain;
                float y = 0.0f;
                for (int s = 0; s < sobre; ++s)
                {
                    float u = s == 0 ? v * (float) sobre : 0.0f;
                    for (auto& f : k.up) u = f (u);
                    u = k.dc (corba (m, u));
                    for (auto& f : k.down) u = f (u);
                    if (s == 0) y = u;
                }
                y = k.bass (k.tone2 (k.tone1 (k.postB (k.postA (y)))));
                float o = (x * (1.0f - sMix) + y * sOut * sMix) * gGate;
                if (! std::isfinite (o)) o = 0.0f;
                io[c][i] = o;
                picS = std::max (picS, std::fabs (o));
            }
        }
        mes.entrada.store (std::max (picE, mes.entrada.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
        mes.sortida.store (std::max (picS, mes.sortida.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
        mes.gate.store (gGate, std::memory_order_relaxed);
    }
}
