#include "mibase/SubBass.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace mibase::subbass
{
    namespace
    {
        constexpr float pi = 3.14159265358979f;
        constexpr double dpi = 3.14159265358979323846;
        const char* const onesOsc = "Sinus|Triangle|Serra|Quadrada|Pols";
        const char* const noSi = "No|Sí";

        const InfoParam infos[numParams] = {
            { "o1w", "Ona", SOsc, 0, 4, 2, Enter, onesOsc, "Forma d'ona de l'oscil·lador 1" },
            { "o1o", "Octava", SOsc, -3, 2, 0, Enter, nullptr, "Octava de l'oscil·lador 1" },
            { "o1s", "Semitò", SOsc, -12, 12, 0, Enter, nullptr, "Semitons de l'oscil·lador 1" },
            { "o1f", "Fi", SOsc, -100, 100, 0, Lineal, nullptr, "Afinació fina de l'oscil·lador 1 (centèsimes)" },
            { "o1p", "PW", SOsc, 0.05f, 0.95f, 0.5f, Lineal, nullptr, "Amplada del pols de l'oscil·lador 1 (ona «Pols»)" },
            { "o1n", "Nivell", SOsc, 0, 1, 0.85f, Lineal, nullptr, "Volum de l'oscil·lador 1" },
            { "o2w", "Ona", SOsc, 0, 4, 2, Enter, onesOsc, "Forma d'ona de l'oscil·lador 2" },
            { "o2o", "Octava", SOsc, -3, 2, 0, Enter, nullptr, "Octava de l'oscil·lador 2" },
            { "o2s", "Semitò", SOsc, -12, 12, 0, Enter, nullptr, "Semitons de l'oscil·lador 2 (7 = una quinta)" },
            { "o2f", "Detune", SOsc, -100, 100, 7, Lineal, nullptr, "Desafinació de l'oscil·lador 2 respecte de l'1 (centèsimes): batec i gruix" },
            { "o2p", "PW", SOsc, 0.05f, 0.95f, 0.5f, Lineal, nullptr, "Amplada del pols de l'oscil·lador 2 (ona «Pols»)" },
            { "o2n", "Nivell", SOsc, 0, 1, 0, Lineal, nullptr, "Volum de l'oscil·lador 2" },
            { "sync", "Sync", SOsc, 0, 1, 0, Enter, noSi, "L'oscil·lador 2 torna a començar cada cicle de l'1 (so tallant; mou l'afinació del 2)" },
            { "fm", "FM", SOsc, 0, 1, 0, Lineal, nullptr, "L'oscil·lador 1 modula el 2 (FM): de rodó a metàl·lic" },
            { "uni", "Unison", SOsc, 1, 7, 1, Enter, nullptr, "Quantes còpies de cada oscil·lador sonen alhora" },
            { "unid", "Detune", SOsc, 0, 1, 0.25f, Lineal, nullptr, "Com de desafinades estan les còpies de l'unison" },
            { "unia", "Obertura", SOsc, 0, 1, 0.5f, Lineal, nullptr, "Com s'obren les còpies de l'unison a esquerra i dreta" },
            { "fase", "Fase", SOsc, 0, 360, 0, Lineal, nullptr, "On comença l'ona a cada nota (si la fase no és lliure): transitori sempre igual" },
            { "lliure", "Lliure", SOsc, 0, 1, 0, Enter, noSi, "Fase lliure: els oscil·ladors no es reinicien (més analògic, cada nota una mica diferent)" },
            { "clic", "Clic", SOsc, 0, 1, 0, Lineal, nullptr, "Clic d'atac: un cop curt al començament de la nota perquè el baix es retalli a la mescla" },
            { "subw", "Ona", SSub, 0, 2, 0, Enter, "Sinus|Triangle|Quadrada", "Forma d'ona del sub" },
            { "subo", "Octava", SSub, 0, 1, 0, Enter, "-1 oct|-2 oct", "Una o dues octaves per sota de la nota" },
            { "subn", "Nivell", SSub, 0, 1, 0.5f, Lineal, nullptr, "Volum del sub-oscil·lador" },
            { "subf", "Pel filtre", SSub, 0, 1, 0, Enter, noSi, "Sí: el sub passa pel filtre i el drive. No: va net per sota (greu estable)" },
            { "nz", "Soroll", SSub, 0, 1, 0, Lineal, nullptr, "Soroll: aire i textura" },
            { "nzc", "Color", SSub, 0, 1, 0.3f, Lineal, nullptr, "Color del soroll: blanc (0) o fosc (1)" },
            { "flt", "Model", SFiltre, 0, 3, 0, Enter, "Ladder|SVF|Diode|MS-20", "Ladder: rodó i greu (Moog). SVF: net i precís. Diode: el «squelch» àcid (303). MS-20: agressiu i cridaner" },
            { "pend", "Pendent", SFiltre, 0, 1, 1, Enter, "12 dB|24 dB", "Pendent del filtre (Ladder i SVF): 12 dB més obert, 24 dB més rodó" },
            { "cut", "Tall", SFiltre, 20, 18000, 900, Log, nullptr, "Freqüència de tall del filtre passa-baixos" },
            { "res", "Ressonància", SFiltre, 0, 1, 0.2f, Lineal, nullptr, "Ressonància: realça el tall; al màxim xiula (autooscil·la)" },
            { "fenv", "Envolupant", SFiltre, -1, 1, 0.45f, Lineal, nullptr, "Quant obre (o tanca) el filtre l'envolupant del filtre" },
            { "key", "Seguiment", SFiltre, 0, 1, 0.3f, Lineal, nullptr, "Seguiment de teclat: les notes agudes obren més el filtre" },
            { "fdrv", "Saturació", SFiltre, 0, 1, 0.1f, Lineal, nullptr, "Saturació a l'entrada del filtre" },
            { "aa", "A", SEnv, 0.3f, 5000, 1.0f, Log, nullptr, "Atac del volum" },
            { "ad", "D", SEnv, 5, 8000, 400, Log, nullptr, "Caiguda del volum" },
            { "as", "S", SEnv, 0, 1, 0.8f, Lineal, nullptr, "Sostingut del volum" },
            { "ar", "R", SEnv, 3, 8000, 120, Log, nullptr, "Alliberament del volum" },
            { "fa", "A", SEnv, 0.3f, 5000, 1.0f, Log, nullptr, "Atac del filtre" },
            { "fd", "D", SEnv, 5, 8000, 280, Log, nullptr, "Caiguda del filtre" },
            { "fs", "S", SEnv, 0, 1, 0.2f, Lineal, nullptr, "Sostingut del filtre" },
            { "fr", "R", SEnv, 3, 8000, 150, Log, nullptr, "Alliberament del filtre" },
            { "lw", "Ona", SLfo, 0, 4, 0, Enter, "Sinus|Triangle|Serra|Quadrada|Atzar", "Forma de l'LFO" },
            { "lr", "Velocitat", SLfo, 0.05f, 30, 2, Log, nullptr, "Velocitat de l'LFO (si no va amb el tempo)" },
            { "ls", "Tempo", SLfo, 0, 11, 0, Enter, "Lliure|1/1|1/2|1/4|1/8|1/16|1/32|1/4T|1/8T|1/16T|1/4.|1/8.", "L'LFO va lliure o amb el tempo del projecte (wobble)" },
            { "ld", "Destí", SLfo, 0, 4, 0, Enter, "Tall|To|PW|Volum|FM", "Què mou l'LFO" },
            { "lq", "Quantitat", SLfo, 0, 1, 0, Lineal, nullptr, "Quant mou l'LFO" },
            { "lret", "Reinicia", SLfo, 0, 1, 1, Enter, noSi, "L'LFO torna a començar a cada nota nova" },
            { "dt", "Tipus", SDrive, 0, 3, 0, Enter, "Suau|Dur|Fuzz|Plec", "Suau: vàlvula. Dur: transistor asimètric. Fuzz: trencat. Plec: ona plegada (growl)" },
            { "dq", "Drive", SDrive, 0, 1, 0, Lineal, nullptr, "Quanta distorsió" },
            { "dto", "To", SDrive, 0, 1, 0.7f, Lineal, nullptr, "To de la distorsió: fosc o brillant" },
            { "dmix", "Mix", SDrive, 0, 1, 1, Lineal, nullptr, "Barreja del so net i el distorsionat" },
            { "dpos", "Lloc", SDrive, 0, 1, 1, Enter, "Abans del filtre|Després del filtre", "On va la distorsió: abans del filtre (el filtre l'arrodoneix) o després (més aspre)" },
            { "mwc", "Roda → Tall", SMod, -1, 1, 0.3f, Lineal, nullptr, "La roda de modulació obre (o tanca) el filtre" },
            { "mwl", "Roda → LFO", SMod, 0, 1, 0, Lineal, nullptr, "La roda de modulació afegeix LFO" },
            { "vv", "Vel → Volum", SMod, 0, 1, 0.5f, Lineal, nullptr, "Com canvia el volum amb la força de la nota" },
            { "vf", "Vel → Filtre", SMod, 0, 1, 0.3f, Lineal, nullptr, "Com obre el filtre la força de la nota" },
            { "bend", "Bend", SMod, 0, 12, 2, Enter, nullptr, "Rang del pitch bend (semitons)" },
            { "pe", "Env. de to", SMod, -24, 24, 0, Lineal, nullptr, "La nota comença més aguda (o greu) i baixa al to: el «punch» dels 808" },
            { "pet", "Temps de to", SMod, 2, 1000, 40, Log, nullptr, "Quant triga l'envolupant de to a arribar a la nota" },
            { "mode", "Mode", SSortida, 0, 2, 0, Enter, "Mono|Legato|Poli", "Mono: una nota i torna a atacar. Legato: les notes lligades no ataquen i llisquen. Poli: acords (8 veus)" },
            { "glide", "Glide", SSortida, 0, 2000, 0, Cub, nullptr, "Temps del portament entre notes" },
            { "vol", "Volum", SSortida, -24, 6, -3, Lineal, nullptr, "Volum de sortida" },
            { "width", "Amplada", SSortida, 0, 1.5f, 1, Lineal, nullptr, "Amplada estèreo (el sub sempre és mono)" },
            { "lim", "Límit", SSortida, 0, 1, 1, Enter, noSi, "Limitador suau a la sortida" },
        };

        inline float tanhR (float x) noexcept
        {
            if (x > 3.0f) return 1.0f;
            if (x < -3.0f) return -1.0f;
            const float x2 = x * x;
            return x * (27.0f + x2) / (27.0f + 9.0f * x2);
        }
        inline float blep (double t, double dt) noexcept
        {
            if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
            if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
            return 0.0f;
        }
        inline float formaOna (int w, double p, double dt, float pw, bool ambBlep) noexcept
        {
            switch (w)
            {
                case 0: return std::sin (2.0f * pi * (float) p);
                case 1: return 1.0f - 4.0f * std::abs ((float) p - 0.5f);
                case 2: return (float) (2.0 * p - 1.0) - (ambBlep ? blep (p, dt) : 0.0f);
                default:
                {
                    const double w2 = w == 3 ? 0.5 : (double) pw;
                    float v = p < w2 ? 1.0f : -1.0f;
                    if (ambBlep)
                    {
                        v += blep (p, dt);
                        double q = p + 1.0 - w2;
                        if (q >= 1.0) q -= 1.0;
                        v -= blep (q, dt);
                    }
                    return v;
                }
            }
        }
        inline float coef (double temps, double fs) noexcept { return temps <= 0.0 ? 1.0f : (float) (1.0 - std::exp (-1.0 / (temps * fs))); }

        std::string num (double v, int dec)
        {
            char b[32];
            std::snprintf (b, sizeof b, "%.*f", dec, v);
            std::string s (b);
            for (auto& c : s) if (c == '.') c = ',';
            if (s == "-0" || s == "-0,0") s = s.substr (1);
            return s;
        }
        std::string ms (double v) { return v < 10.0 ? num (v, 1) + " ms" : v < 1000.0 ? num (v, 0) + " ms" : num (v / 1000.0, 2) + " s"; }

        // filtre de mitja banda (2× → 1×)
        constexpr int tapsHb = 47;
        const float* coefHb()
        {
            static float h[tapsHb];
            static bool fet = false;
            if (! fet)
            {
                double suma = 0.0;
                const int c = tapsHb / 2;
                for (int i = 0; i < tapsHb; ++i)
                {
                    const int d = i - c;
                    double v = d == 0 ? 0.5 : (d % 2 == 0 ? 0.0 : std::sin (dpi * d / 2.0) / (dpi * d));
                    const double w = 0.42 - 0.5 * std::cos (2.0 * dpi * i / (tapsHb - 1)) + 0.08 * std::cos (4.0 * dpi * i / (tapsHb - 1));
                    h[i] = (float) (v * w);
                    suma += h[i];
                }
                for (auto& x : h) x = (float) (x / suma);
                fet = true;
            }
            return h;
        }
    }

    std::atomic<double>& tempo() { static std::atomic<double> t { 120.0 }; return t; }

    const InfoParam& info (int p) { return infos[std::clamp (p, 0, numParams - 1)]; }

    std::vector<std::string> opcions (int p)
    {
        std::vector<std::string> r;
        const char* o = info (p).opcions;
        if (o == nullptr) return r;
        std::string s (o);
        size_t a = 0;
        while (true)
        {
            const auto b = s.find ('|', a);
            r.push_back (s.substr (a, b == std::string::npos ? std::string::npos : b - a));
            if (b == std::string::npos) break;
            a = b + 1;
        }
        return r;
    }

    double negresRitme (int r) noexcept
    {
        static const double d[] = { 0.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 1.5, 0.75 };
        return d[std::clamp (r, 0, 11)];
    }

    float normal (int p, float v) noexcept
    {
        const auto& i = infos[p];
        v = std::clamp (v, i.mn, i.mx);
        switch (i.escala)
        {
            case Log: return (float) (std::log (v / i.mn) / std::log (i.mx / i.mn));
            case Cub: return (float) std::cbrt ((v - i.mn) / (i.mx - i.mn));
            default: return (v - i.mn) / (i.mx - i.mn);
        }
    }

    float deNormal (int p, float x) noexcept
    {
        const auto& i = infos[p];
        x = std::clamp (x, 0.0f, 1.0f);
        switch (i.escala)
        {
            case Log: return (float) (i.mn * std::pow (i.mx / i.mn, (double) x));
            case Cub: return i.mn + (i.mx - i.mn) * x * x * x;
            case Enter: return std::round (i.mn + (i.mx - i.mn) * x);
            default: return i.mn + (i.mx - i.mn) * x;
        }
    }

    std::string textValor (int p, float v)
    {
        const auto& i = infos[std::clamp (p, 0, numParams - 1)];
        if (i.opcions != nullptr)
        {
            const auto o = opcions (p);
            const int k = (int) std::lround (v - i.mn);
            return o[(size_t) std::clamp (k, 0, (int) o.size() - 1)];
        }
        switch (p)
        {
            case O1Oct: case O2Oct: return (v > 0 ? "+" : "") + num (v, 0) + " oct";
            case O1Semi: case O2Semi: case BendRang: return (v > 0 && p != BendRang ? "+" : "") + num (v, 0) + " st";
            case O1Fi: case O2Fi: return (v > 0 ? "+" : "") + num (v, 0) + " ct";
            case PEnvQuant: return (v > 0 ? "+" : "") + num (v, 1) + " st";
            case UniDetune: return num (v * v * 60.0, 0) + " ct";
            case Unison: return v < 1.5f ? std::string ("Off") : num (v, 0) + " veus";
            case Fase: return num (v, 0) + "°";
            case FTall: return v < 1000.0f ? num (v, 0) + " Hz" : num (v / 1000.0, v < 10000.0f ? 2 : 1) + " kHz";
            case LVel: return v < 10.0f ? num (v, 2) + " Hz" : num (v, 1) + " Hz";
            case AA: case AD: case AR: case FA: case FD: case FR: case PEnvTemps: case Glide: return v < 0.05f && p == Glide ? std::string ("Off") : ms (v);
            case Volum: return (v > 0 ? "+" : "") + num (v, 1) + " dB";
            case FEnv: case RodaTall: return (v > 0 ? "+" : "") + num (v * 100.0, 0) + " %";
            default: return num (v * 100.0, 0) + " %";
        }
    }

    //==========================================================================
    SubBass::SubBass()
    {
        for (int i = 0; i < numParams; ++i) par[(size_t) i].store (infos[i].def);
        for (auto& c : ccMapa) c.store (-1);
        for (auto& n : notesOn) n.store (0);
        for (auto& o : ona) o.store (0.0f);
        ccMapa[74].store (FTall);
        ccMapa[71].store (FRes);
        const float* h = coefHb();
        for (int i = 0; i < tapsHb; ++i) hb[i] = h[i];
        numHb = tapsHb;
    }

    void SubBass::prepara (double f, int)
    {
        fs = f > 1000.0 ? f : 48000.0;
        fsI = fs * sobre;
        for (auto& v : veus) { v = Veu(); }
        std::memset (histL, 0, sizeof histL);
        std::memset (histR, 0, sizeof histR);
        numPila = 0;
        dcL = dcR = dcxL = dcxR = 0.0f;
    }

    InfoParametre SubBass::infoParametre (int i) const
    {
        InfoParametre r;
        const auto& x = infos[std::clamp (i, 0, numParams - 1)];
        static const char* seccions[] = { "Osc", "Sub", "Filtre", "Env", "LFO", "Drive", "Mod", "Sortida" };
        r.id = x.clau;
        r.nom = std::string (seccions[x.seccio]) + " · " + x.nom;
        if (i >= O1Ona && i <= O1Niv) r.nom = std::string ("Osc 1 · ") + x.nom;
        else if (i >= O2Ona && i <= O2Niv) r.nom = std::string ("Osc 2 · ") + x.nom;
        else if (i >= AA && i <= AR) r.nom = std::string ("Env volum · ") + x.nom;
        else if (i >= FA && i <= FR) r.nom = std::string ("Env filtre · ") + x.nom;
        r.passos = x.escala == Enter ? (int) (x.mx - x.mn) + 1 : 0;
        return r;
    }

    void SubBass::posaParametre (int i, float v01) noexcept { if (i >= 0 && i < numParams) posa (i, deNormal (i, v01)); }
    float SubBass::parametre (int i) const noexcept { return i >= 0 && i < numParams ? normal (i, valor (i)) : 0.0f; }
    std::string SubBass::textParametre (int i, float v01) const { return textValor (i, deNormal (i, v01)); }

    void SubBass::posa (int p, float v) noexcept
    {
        if (p < 0 || p >= numParams) return;
        const auto& i = infos[p];
        v = std::clamp (v, i.mn, i.mx);
        if (i.escala == Enter) v = std::round (v);
        par[(size_t) p].store (v, std::memory_order_relaxed);
    }

    // ---- estat ----
    std::string SubBass::estatSo() const
    {
        std::string s = "mibase-subbass=1";
        char b[64];
        for (int i = 0; i < numParams; ++i)
        {
            std::snprintf (b, sizeof b, ";%s=%.6g", infos[i].clau, (double) valor (i));
            s += b;
        }
        return s;
    }

    std::string SubBass::estat() const
    {
        auto s = estatSo();
        std::string cc;
        for (int c = 0; c < 128; ++c)
            if (const int p = paramDeCC (c); p >= 0)
                cc += (cc.empty() ? "" : ",") + std::to_string (c) + ">" + infos[p].clau;
        s += ";cc=" + cc;
        if (! nomPreset.empty())
        {
            std::string n = nomPreset;
            for (auto& c : n) if (c == ';' || c == '=') c = ',';
            s += ";pnom=" + n;
        }
        return s;
    }

    void SubBass::posaEstat (const std::string& s)
    {
        if (s.rfind ("mibase-subbass=", 0) != 0) return;
        // primer es llegeix tot i després es posa: el fil d'àudio no veu mai valors a mitges
        float v[numParams];
        for (int i = 0; i < numParams; ++i) v[i] = infos[i].def;
        std::string nom;
        bool ambCC = false;
        int cc[128];
        for (auto& c : cc) c = -1;
        size_t a = 0;
        while (a < s.size())
        {
            size_t b = s.find (';', a);
            if (b == std::string::npos) b = s.size();
            const auto parell = s.substr (a, b - a);
            const auto eq = parell.find ('=');
            if (eq != std::string::npos)
            {
                const auto clau = parell.substr (0, eq), val = parell.substr (eq + 1);
                if (clau == "pnom") nom = val;
                else if (clau == "cc")
                {
                    ambCC = true;
                    size_t x = 0;
                    while (x < val.size())
                    {
                        size_t y = val.find (',', x);
                        if (y == std::string::npos) y = val.size();
                        const auto e = val.substr (x, y - x);
                        const auto g = e.find ('>');
                        if (g != std::string::npos)
                        {
                            const int n = std::atoi (e.substr (0, g).c_str());
                            const auto cl = e.substr (g + 1);
                            for (int i = 0; i < numParams; ++i)
                                if (cl == infos[i].clau && n >= 0 && n < 128 && ! ccReservat (n)) cc[n] = i;
                        }
                        x = y + 1;
                    }
                }
                else
                    for (int i = 0; i < numParams; ++i)
                        if (clau == infos[i].clau) v[i] = (float) std::strtod (val.c_str(), nullptr);
            }
            a = b + 1;
        }
        for (int i = 0; i < numParams; ++i) posa (i, v[i]);
        nomPreset = nom;
        if (ambCC)   // sense «cc=» (un preset) les assignacions no es toquen
            for (int n = 0; n < 128; ++n) ccMapa[(size_t) n].store (cc[n]);
    }

    void SubBass::assignaCC (int cc, int param) noexcept
    {
        if (cc < 0 || cc >= 128 || ccReservat (cc) || param < 0 || param >= numParams) return;
        treuCC (param);
        ccMapa[(size_t) cc].store (param);
    }
    void SubBass::treuCC (int param) noexcept
    {
        for (auto& c : ccMapa) if (c.load() == param) c.store (-1);
    }
    int SubBass::ccDeParam (int param) const noexcept
    {
        for (int c = 0; c < 128; ++c) if (ccMapa[(size_t) c].load() == param) return c;
        return -1;
    }

    void SubBass::copiaOna (float* d, int n) const noexcept
    {
        n = std::min (n, midaOna);
        const int pos = posOna.load (std::memory_order_acquire);
        for (int i = 0; i < n; ++i)
            d[i] = ona[(size_t) ((pos - n + i + midaOna * 2) % midaOna)].load (std::memory_order_relaxed);
    }

    void SubBass::marcaNota (int n, bool on) noexcept
    {
        if (n < 0 || n > 127) return;
        auto& w = notesOn[(size_t) (n >> 6)];
        const std::uint64_t bit = 1ull << (n & 63);
        if (on) w.fetch_or (bit, std::memory_order_relaxed); else w.fetch_and (~bit, std::memory_order_relaxed);
    }

    bool SubBass::fentSo() const noexcept
    {
        for (const auto& v : veus) if (v.viva) return true;
        return false;
    }

    void SubBass::apagaTot() noexcept
    {
        for (auto& v : veus) if (v.viva) { v.etapaA = 4; v.tecla = false; v.etapaF = 3; }
        numPila = 0;
        pedal = false;
        for (auto& s : sostingudes) s = false;
        for (auto& n : notesOn) n.store (0);
    }

    // ---- notes ----
    void SubBass::dispara (Veu& v, int nota, float vel, bool retriga, bool glide) noexcept
    {
        const bool estava = v.viva && v.envA > 1e-4f;
        v.nota = nota;
        v.objectiu = nota;
        if (! glide || ! v.viva) v.semis = nota;
        v.vel = vel;
        v.tecla = true;
        v.ordre = ++comptaOrdre;
        if (retriga)
        {
            v.etapaA = 1;
            v.etapaF = 1;
            v.tPEnv = 0.0f;
            v.clic = 1.0f;
            if (! estava)
            {
                v.envA = 0.0f;
                v.envF = 0.0f;
                if (valor (FaseLliure) < 0.5f)
                {
                    const double f0 = valor (Fase) / 360.0;
                    for (int k = 0; k < maxUnison; ++k)
                    {
                        double ph = f0 + k * 0.1373;
                        ph -= std::floor (ph);
                        v.f1[k] = ph;
                        v.f2[k] = ph;
                    }
                    v.fSub = f0;
                }
                else
                    for (int k = 0; k < maxUnison; ++k) { v.f1[k] = 0.5 + 0.5 * aleat(); v.f2[k] = 0.5 + 0.5 * aleat(); }
                for (auto& f : v.fl) f.buida();
                v.drvTo[0] = v.drvTo[1] = 0.0f;
                v.compta = 0;
            }
        }
        v.viva = true;
        hzUltima.store ((float) (440.0 * std::pow (2.0, (nota - 69) / 12.0)), std::memory_order_relaxed);
        if (valor (LRetrig) > 0.5f && retriga && numPila <= 1) fase = 0.0;
    }

    void SubBass::allibera (Veu& v) noexcept
    {
        v.tecla = false;
        if (v.etapaA != 0 && v.etapaA != 4) v.etapaA = 3;
        if (v.etapaF != 0) v.etapaF = 3;
    }

    void SubBass::noteOn (int nota, int vel) noexcept
    {
        marcaNota (nota, true);
        sostingudes[nota] = false;
        const float vf = std::clamp (vel / 127.0f, 0.0f, 1.0f);
        const int mode = (int) valor (Mode);
        const bool ambGlide = valor (Glide) > 0.5f;
        if (mode == 2)
        {
            // poli: una veu lliure; si no n'hi ha, la que fa més temps que s'ha deixat anar (o la més vella)
            Veu* tria = nullptr;
            for (auto& v : veus) if (! v.viva) { tria = &v; break; }
            if (tria == nullptr)
                for (auto& v : veus) if (! v.tecla && (tria == nullptr || v.envA < tria->envA)) tria = &v;
            if (tria == nullptr)
                for (auto& v : veus) if (tria == nullptr || v.ordre < tria->ordre) tria = &v;
            const bool g = ambGlide && darreraNota >= 0.0;
            if (g && ! tria->viva) { tria->semis = darreraNota; tria->viva = true; tria->envA = 0.0f; tria->etapaA = 0; }
            dispara (*tria, nota, vf, true, g);
            darreraNota = nota;
            return;
        }
        // mono i legato: una sola veu i la pila de notes
        for (int i = 0; i < numPila; ++i)
            if (pila[i] == nota) { std::memmove (pila + i, pila + i + 1, sizeof (int) * (size_t) (numPila - i - 1)); --numPila; break; }
        const bool lligada = numPila > 0;
        if (numPila < 32) pila[numPila++] = nota;
        for (int i = 1; i < maxVeus; ++i) if (veus[i].viva) { veus[i].etapaA = 4; veus[i].tecla = false; }
        auto& v = veus[0];
        if (mode == 1) dispara (v, nota, lligada ? v.vel : vf, ! lligada, lligada || (ambGlide && v.viva && v.envA > 1e-3f && lligada));
        else dispara (v, nota, vf, true, ambGlide && v.viva && v.envA > 1e-4f);
        darreraNota = nota;
    }

    void SubBass::noteOff (int nota) noexcept
    {
        marcaNota (nota, false);
        if (pedal) { sostingudes[nota] = true; return; }
        const int mode = (int) valor (Mode);
        if (mode == 2)
        {
            for (auto& v : veus) if (v.viva && v.tecla && v.nota == nota) allibera (v);
            return;
        }
        for (int i = 0; i < numPila; ++i)
            if (pila[i] == nota) { std::memmove (pila + i, pila + i + 1, sizeof (int) * (size_t) (numPila - i - 1)); --numPila; break; }
        auto& v = veus[0];
        if (v.nota != nota) return;
        if (numPila > 0)
        {
            // torna a la nota que encara es prem (sense atacar en legato; en mono, ataca)
            const int n = pila[numPila - 1];
            const bool g = mode == 1 || valor (Glide) > 0.5f;
            dispara (v, n, v.vel, mode == 0, g);
        }
        else allibera (v);
    }

    void SubBass::controla (int cc, int val) noexcept
    {
        const int apr = aprenent.load (std::memory_order_relaxed);
        if (apr >= 0 && ! ccReservat (cc))
        {
            for (auto& c : ccMapa) if (c.load (std::memory_order_relaxed) == apr) c.store (-1, std::memory_order_relaxed);
            ccMapa[(size_t) cc].store (apr, std::memory_order_relaxed);
            aprenent.store (-1);
        }
        if (cc == 1) { rodaMod = val / 127.0f; rodaV.store (rodaMod, std::memory_order_relaxed); return; }
        if (cc == 64)
        {
            pedal = val >= 64;
            if (! pedal)
                for (int n = 0; n < 128; ++n)
                    if (sostingudes[n]) { sostingudes[n] = false; noteOff (n); }
            return;
        }
        if (cc == 120 || cc == 123) { apagaTot(); return; }
        const int p = ccMapa[(size_t) cc].load (std::memory_order_relaxed);
        if (p >= 0) posa (p, deNormal (p, val / 127.0f));
    }

    // ---- so ----
    void SubBass::processa (const EventInstrument* events, int numEvents, float* L, float* R, int n) noexcept
    {
        const auto t0 = std::chrono::steady_clock::now();
        int fet = 0, e = 0;
        while (fet < n)
        {
            while (e < numEvents && events[e].offset <= fet)
            {
                const auto& ev = events[e++];
                const int tipus = ev.estat & 0xF0;
                if (tipus == 0x90 && ev.dada2 > 0) noteOn (ev.dada1 & 127, ev.dada2);
                else if (tipus == 0x80 || (tipus == 0x90 && ev.dada2 == 0)) noteOff (ev.dada1 & 127);
                else if (tipus == 0xB0) controla (ev.dada1 & 127, ev.dada2);
                else if (tipus == 0xE0)
                {
                    const float b = (float) (((int) ev.dada2 << 7 | ev.dada1) - 8192) / 8192.0f;
                    bendV.store (b, std::memory_order_relaxed);
                    bendSemis = b;
                }
            }
            int fins = e < numEvents ? std::min (n, std::max (fet + 1, events[e].offset)) : n;
            fins = std::min (fins, fet + tros);
            const int m = fins - fet;
            renderitza (m);
            // delmat 2× → 1× amb el filtre de mitja banda
            const int nI = m * sobre, h = numHb - 1;
            float wL[tros * sobre + 64], wR[tros * sobre + 64];
            std::memcpy (wL, histL, sizeof (float) * (size_t) h);
            std::memcpy (wR, histR, sizeof (float) * (size_t) h);
            std::memcpy (wL + h, bufL, sizeof (float) * (size_t) nI);
            std::memcpy (wR + h, bufR, sizeof (float) * (size_t) nI);
            const float volObj = std::pow (10.0f, valor (Volum) / 20.0f);
            const float amp = valor (Ample);
            const bool lim = valor (Limit) > 0.5f;
            float pic = nivellV.load (std::memory_order_relaxed) * 0.97f;
            int pos = posOna.load (std::memory_order_relaxed);
            for (int i = 0; i < m; ++i)
            {
                float yl = 0.0f, yr = 0.0f;
                const float* xl = wL + 2 * i + 1;
                const float* xr = wR + 2 * i + 1;
                for (int j = 0; j < numHb; j += 2) { yl += hb[j] * xl[j]; yr += hb[j] * xr[j]; }
                // els coeficients no nuls són els parells i el del centre (senar)
                const int c = numHb / 2;
                yl += hb[c] * xl[c]; yr += hb[c] * xr[c];
                // tallar la continua
                const float ol = yl - dcxL + 0.9995f * dcL; dcxL = yl; dcL = ol;
                const float orr = yr - dcxR + 0.9995f * dcR; dcxR = yr; dcR = orr;
                volSuau += (volObj - volSuau) * 0.002f;
                float mid = 0.5f * (ol + orr), sid = 0.5f * (ol - orr) * amp;
                float a = (mid + sid) * volSuau, b = (mid - sid) * volSuau;
                if (lim)
                {
                    auto sl = [] (float x) { const float ax = std::abs (x); return ax <= 0.8f ? x : std::copysign (0.8f + 0.2f * tanhR ((ax - 0.8f) / 0.2f), x); };
                    a = sl (a); b = sl (b);
                }
                L[fet + i] = a;
                R[fet + i] = b;
                pic = std::max (pic, std::max (std::abs (a), std::abs (b)));
                ona[(size_t) pos].store (0.5f * (a + b), std::memory_order_relaxed);
                pos = (pos + 1) % midaOna;
            }
            posOna.store (pos, std::memory_order_release);
            nivellV.store (pic, std::memory_order_relaxed);
            std::memcpy (histL, wL + nI, sizeof (float) * (size_t) h);
            std::memcpy (histR, wR + nI, sizeof (float) * (size_t) h);
            fet = fins;
        }
        while (e < numEvents) { const auto& ev = events[e++]; if ((ev.estat & 0xF0) == 0x80 || ((ev.estat & 0xF0) == 0x90 && ev.dada2 == 0)) noteOff (ev.dada1 & 127); }
        int act = 0;
        for (const auto& v : veus) act += v.viva ? 1 : 0;
        actives.store (act, std::memory_order_relaxed);
        const double usat = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        const float carrega = (float) (usat / std::max (1e-6, n / fs));
        const float ant = cpu.load (std::memory_order_relaxed);
        cpu.store (ant + (carrega - ant) * 0.05f, std::memory_order_relaxed);
    }

    void SubBass::renderitza (int n) noexcept
    {
        const int nI = n * sobre;
        std::memset (bufL, 0, sizeof (float) * (size_t) nI);
        std::memset (bufR, 0, sizeof (float) * (size_t) nI);
        constexpr int sub = 16;
        // LFO global
        const int lw = (int) valor (LOna);
        const int ritme = (int) valor (LRitme);
        double hz = valor (LVel);
        if (ritme > 0)
        {
            const double t = std::max (20.0, tempo().load (std::memory_order_relaxed));
            hz = t / 60.0 / negresRitme (ritme);
        }
        for (int s = 0; s < nI; s += sub)
        {
            const int m = std::min (sub, nI - s);
            const double ph0 = fase;
            fase += hz * m / fsI;
            if (fase >= 1.0)
            {
                fase -= std::floor (fase);
                lfoSH = aleat();
            }
            float l;
            switch (lw)
            {
                case 0: l = std::sin (2.0f * pi * (float) ph0); break;
                case 1: l = 1.0f - 4.0f * std::abs ((float) ph0 - 0.5f); break;
                case 2: l = 1.0f - 2.0f * (float) ph0; break;
                case 3: l = ph0 < 0.5 ? 1.0f : -1.0f; break;
                default: l = lfoSH; break;
            }
            lfoVal += (l - lfoVal) * (lw == 3 || lw == 4 ? 0.5f : 1.0f);
            lfoV.store (lfoVal, std::memory_order_relaxed);
            for (auto& v : veus)
                if (v.viva) renderitzaVeu (v, s, m, lfoVal);
        }
    }

    void SubBass::renderitzaVeu (Veu& v, int s0, int m, float lfo) noexcept
    {
        const int fi = s0 + m;
        if (m <= 0) return;
        const double dtS = (double) m / fsI;

        // ---- paràmetres del tros ----
        const int w1 = (int) valor (O1Ona), w2 = (int) valor (O2Ona);
        const float n1 = valor (O1Niv), n2 = valor (O2Niv);
        const int nu = std::clamp ((int) valor (Unison), 1, maxUnison);
        const bool sync = valor (Sync) > 0.5f;
        const float lfoQ = std::clamp (valor (LQuant) + valor (RodaLfo) * rodaMod, 0.0f, 1.0f);
        const int desti = (int) valor (LDesti);
        const float lfoM = lfo * lfoQ;
        float fm = valor (Fm);
        if (desti == 4) fm = std::clamp (fm + lfoQ * (0.5f + 0.5f * lfo), 0.0f, 1.0f);
        float pw1 = valor (O1Pw), pw2 = valor (O2Pw);
        if (desti == 2) { pw1 = std::clamp (pw1 + lfoM * 0.45f, 0.03f, 0.97f); pw2 = std::clamp (pw2 + lfoM * 0.45f, 0.03f, 0.97f); }

        // glide
        const float gl = valor (Glide);
        if (gl < 0.5f) v.semis = v.objectiu;
        else
        {
            const double c = 1.0 - std::exp (-dtS / (gl / 1000.0 / 3.0));
            v.semis += (v.objectiu - v.semis) * c;
        }
        // envolupant de to
        const float pe = valor (PEnvQuant);
        double semis = v.semis + bendSemis * valor (BendRang);
        if (std::abs (pe) > 1e-3f) semis += pe * std::exp (-v.tPEnv / (valor (PEnvTemps) / 1000.0f));
        v.tPEnv += (float) dtS;
        if (desti == 1) semis += lfoM * lfoQ * 12.0f;
        const double hzNota = 440.0 * std::pow (2.0, (semis - 69.0) / 12.0);
        const double hz1 = hzNota * std::pow (2.0, valor (O1Oct) + valor (O1Semi) / 12.0 + valor (O1Fi) / 1200.0);
        const double hz2 = hzNota * std::pow (2.0, valor (O2Oct) + valor (O2Semi) / 12.0 + valor (O2Fi) / 1200.0);
        const double hzSub = hzNota * (valor (SubOct) > 0.5f ? 0.25 : 0.5);
        const float uniD = valor (UniDetune), uniA = valor (UniAmple);
        double d1[maxUnison], d2[maxUnison];
        float gL[maxUnison], gR[maxUnison];
        const float gu = 1.0f / std::sqrt ((float) nu);
        for (int k = 0; k < nu; ++k)
        {
            const float x = nu == 1 ? 0.0f : (float) k / (nu - 1) * 2.0f - 1.0f;
            const double det = std::pow (2.0, x * uniD * uniD * 60.0 / 1200.0);
            d1[k] = std::min (0.45, hz1 * det / fsI);
            d2[k] = std::min (0.45, hz2 * det / fsI);
            const float pan = x * uniA;
            gL[k] = gu * std::sqrt (0.5f * (1.0f - pan));
            gR[k] = gu * std::sqrt (0.5f * (1.0f + pan));
        }
        const double dSub = std::min (0.45, hzSub / fsI);
        const int wSub = (int) valor (SubOna);
        const float nSub = valor (SubNiv);
        const bool subPelFiltre = valor (SubFiltre) > 0.5f;
        const float nz = valor (SorollNiv), nzC = 1.0f - 0.97f * valor (SorollColor);
        const float clicN = valor (Clic);
        const float cClic = (float) std::exp (-1.0 / (0.0018 * fsI));

        // envolupants (coeficients)
        auto cA = [&] (int p) { return coef (valor (p) / 1000.0 / 1.609, fsI); };
        auto cDR = [&] (int p) { return coef (valor (p) / 1000.0 / 4.0, fsI); };
        const float caA = cA (AA), cdA = cDR (AD), crA = cDR (AR), sA = valor (AS);
        const float caF = cA (FA), cdF = cDR (FD), crF = cDR (FR), sF = valor (FS);
        const float crRapid = coef (0.004, fsI);

        // filtre (coeficients del tros, amb l'envolupant al principi del tros)
        const int model = (int) valor (FModel);
        const bool p24 = valor (FPendent) > 0.5f;
        double oct = valor (FEnv) * 7.0 * v.envF + valor (FKey) * (semis - 48.0) / 12.0
                   + valor (VelFiltre) * 4.0 * (v.vel - 0.6) + valor (RodaTall) * rodaMod * 4.0;
        if (desti == 0) oct += lfoM * 4.0;
        const double fc = std::clamp (valor (FTall) * std::pow (2.0, oct), 16.0, std::min (20000.0, fsI * 0.45));
        const float g = (float) std::tan (dpi * fc / fsI);
        const float res = valor (FRes);
        int N = 4; float k = 0.0f;
        if (model == 0) { N = p24 ? 4 : 2; k = res * (p24 ? 4.15f : 2.6f); }
        else if (model == 2) { N = 3; k = res * 8.4f; }
        else if (model == 1) k = 2.0f - 1.97f * res;
        else k = 2.0f - 2.02f * res;
        const float G = g / (1.0f + g);
        const float GN = N == 4 ? G * G * G * G : N == 3 ? G * G * G : G * G;
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float kb = 1.0f;   // la segona etapa de l'SVF de 24 dB, suau
        const float b1 = 1.0f / (1.0f + g * (g + kb)), b2 = g * b1, b3 = g * b2;
        const float fDrv = 1.0f + 3.0f * valor (FDrive);
        const float compensa = (model == 0 ? 1.0f + k * 0.28f : model == 2 ? 1.0f + k * 0.45f : 1.0f) / (1.0f + 0.5f * valor (FDrive));
        const float cHp = (float) (1.0 - std::exp (-2.0 * dpi * 45.0 / fsI));

        // drive
        const int dt = (int) valor (DTipus);
        const float dq = valor (DQuant), dMix = dq < 1e-4f ? 0.0f : valor (DMix);
        const bool dAbans = valor (DLloc) < 0.5f;
        const float dA = 1.0f + dq * dq * 30.0f;
        const float dMake = 1.0f / std::pow (dA, 0.33f);
        const float cTo = (float) (1.0 - std::exp (-2.0 * dpi * (800.0 * std::pow (22.0, (double) valor (DTo))) / fsI));
        auto drive = [&] (float x, int c) noexcept
        {
            if (dMix <= 0.0f) return x;
            float y;
            const float xa = x * dA;
            switch (dt)
            {
                case 0: y = tanhR (xa); break;
                case 1: { const float z = xa + 0.25f; y = (z > 0.0f ? tanhR (z * 1.4f) : std::max (-0.75f, z * 1.2f)) - 0.17f; break; }
                case 2: { const float t = tanhR (xa * 1.5f); y = std::clamp (0.6f * t + 0.6f * std::abs (t) - 0.25f, -1.0f, 1.0f); break; }
                default: y = std::sin (std::clamp (x * (1.0f + dq * 7.0f), -12.0f, 12.0f) * 0.5f * pi); break;
            }
            y *= dt == 3 ? 0.8f : dMake;
            v.drvTo[c] += (y - v.drvTo[c]) * cTo;
            return x + (v.drvTo[c] - x) * dMix;
        };

        // volum
        float vca = 1.0f - valor (VelVol) * (1.0f - v.vel);
        if (desti == 3) vca *= 1.0f - lfoQ * (0.5f - 0.5f * lfo);
        const bool stereo = nu > 1 && uniA > 0.001f;
        const float n1e = n1 * 0.6f, n2e = n2 * 0.6f;

        for (int i = s0; i < fi; ++i)
        {
            // envolupants
            switch (v.etapaA)
            {
                case 1: v.envA += (1.25f - v.envA) * caA; if (v.envA >= 1.0f) { v.envA = 1.0f; v.etapaA = 2; } break;
                case 2: v.envA += (sA - v.envA) * cdA; break;
                case 3: v.envA += (0.0f - v.envA) * crA; break;
                case 4: v.envA += (0.0f - v.envA) * crRapid; break;
                default: break;
            }
            switch (v.etapaF)
            {
                case 1: v.envF += (1.25f - v.envF) * caF; if (v.envF >= 1.0f) { v.envF = 1.0f; v.etapaF = 2; } break;
                case 2: v.envF += (sF - v.envF) * cdF; break;
                case 3: v.envF += (0.0f - v.envF) * crF; break;
                default: break;
            }
            // oscil·ladors
            float aL = 0.0f, aR = 0.0f;
            for (int u = 0; u < nu; ++u)
            {
                double p1 = v.f1[u];
                const float o1 = formaOna (w1, p1, d1[u], pw1, true);
                p1 += d1[u];
                bool volta = false; double frac = 0.0;
                if (p1 >= 1.0) { p1 -= 1.0; volta = true; frac = p1 / d1[u]; }
                v.f1[u] = p1;
                float o2 = 0.0f;
                if (n2 > 0.0f || fm > 0.0f)
                {
                    double p2 = v.f2[u];
                    if (sync && volta) p2 = frac * d2[u];
                    if (fm > 0.001f)
                    {
                        double ph = p2 + fm * fm * 1.6 * o1;
                        ph -= std::floor (ph);
                        o2 = formaOna (w2, ph, d2[u], pw2, false);
                    }
                    else o2 = formaOna (w2, p2, d2[u], pw2, true);
                    p2 += d2[u];
                    if (p2 >= 1.0) p2 -= std::floor (p2);
                    v.f2[u] = p2;
                }
                // amb FM, l'oscil·lador 2 és el portador: se sent encara que el seu nivell sigui 0
                const float sMix = o1 * n1e + o2 * (fm > 0.001f ? std::max (n2e, 0.6f * fm) : n2e);
                aL += sMix * gL[u];
                aR += sMix * gR[u];
            }
            // sub
            float sb = formaOna (wSub == 2 ? 3 : wSub, v.fSub, dSub, 0.5f, true) * nSub * 0.7f;
            v.fSub += dSub;
            if (v.fSub >= 1.0) v.fSub -= 1.0;
            // soroll
            float soroll = 0.0f;
            if (nz > 0.0f) { v.soroll += (aleat() - v.soroll) * nzC; soroll = v.soroll * nz * (1.0f + (1.0f - nzC) * 2.0f) * 0.5f; }
            float xL = aL + soroll, xR = (stereo ? aR : aL) + soroll;
            if (! stereo) xL = (aL + aR) * 0.7071f + soroll;
            if (subPelFiltre) { xL += sb; xR += sb; }
            if (! stereo) xR = xL;
            // filtre (i drive abans)
            float y[2];
            for (int c = 0; c < (stereo ? 2 : 1); ++c)
            {
                float x = c == 0 ? xL : xR;
                if (dAbans) x = drive (x, c);
                x = x * fDrv + 1e-20f;
                auto& f = v.fl[c];
                float out;
                if (model == 0 || model == 2)
                {
                    if (model == 2) { f.hp += (x - f.hp) * cHp; x -= f.hp; }
                    float S = 0.0f;
                    for (int st = 0; st < N; ++st) S = S * G + (1.0f - G) * f.s[st];
                    const float yN = (GN * x + S) / (1.0f + k * GN);
                    float u = x - k * yN;
                    u = model == 2 ? tanhR (u + 0.18f * u * u) : tanhR (u);
                    for (int st = 0; st < N; ++st)
                    {
                        const float vv = (u - f.s[st]) * G;
                        const float yy = vv + f.s[st];
                        f.s[st] = yy + vv;
                        u = yy;
                    }
                    out = u;
                }
                else
                {
                    const float v3 = tanhR (x * 0.7f) * 1.4286f - f.ic2;
                    float v1 = a1 * f.ic1 + a2 * v3;
                    if (model == 3) v1 = tanhR (v1 * 1.2f) / 1.2f;
                    const float v2 = model == 3 ? f.ic2 + g * v1 : f.ic2 + a2 * f.ic1 + a3 * v3;
                    f.ic1 = 2.0f * v1 - f.ic1;
                    f.ic2 = 2.0f * v2 - f.ic2;
                    out = v2;
                    if (model == 1 && p24)
                    {
                        const float w3 = out - f.jc2;
                        const float w1 = b1 * f.jc1 + b2 * w3;
                        const float wv2 = f.jc2 + b2 * f.jc1 + b3 * w3;
                        f.jc1 = 2.0f * w1 - f.jc1;
                        f.jc2 = 2.0f * wv2 - f.jc2;
                        out = wv2;
                    }
                }
                out *= compensa;
                if (! dAbans) out = drive (out, c);
                y[c] = out;
            }
            if (! stereo) y[1] = y[0];
            if (! subPelFiltre) { y[0] += sb; y[1] += sb; }
            // clic d'atac
            if (v.clic > 1e-4f)
            {
                if (clicN > 0.0f) { const float c = v.clic * clicN * (0.55f * aleat() + 0.6f); y[0] += c; y[1] += c; }
                v.clic *= cClic;
            }
            const float a = v.envA * vca;
            bufL[i] += y[0] * a;
            bufR[i] += y[1] * a;
        }
        if ((v.etapaA == 3 || v.etapaA == 4) && v.envA < 1e-5f) { v.viva = false; v.etapaA = 0; v.envA = 0.0f; }
        (void) stereo;
    }

    //==========================================================================
    float previsualitza (const std::string& estat, float* sortida, int n, int nota)
    {
        auto sp = std::make_unique<SubBass>();
        auto& s = *sp;
        s.posaEstat (estat.empty() ? estatInicial() : estat);
        s.posa (Volum, 0.0f);
        s.posa (Ample, 0.0f);
        s.posa (LQuant, 0.0f);   // l'LFO no ajuda a veure la forma
        const double fs = 48000.0;
        s.prepara (fs, 512);
        EventInstrument on { 0, 0x90, (std::uint8_t) nota, 110 };
        std::vector<float> L (512), R (512);
        // a la sortida hi ha prou cicles un cop passat l'atac (o 120 ms, si l'atac és llarg)
        const int espera = (int) (fs * std::clamp ((double) s.valor (AA) / 1000.0 + 0.06, 0.06, 0.4));
        int fet = 0;
        s.processa (&on, 1, L.data(), R.data(), 512);
        fet = 512;
        while (fet < espera) { s.processa (nullptr, 0, L.data(), R.data(), 512); fet += 512; }
        const double hz = 440.0 * std::pow (2.0, (nota - 69) / 12.0);
        const int periode = (int) std::lround (fs / hz);
        std::vector<float> buf;
        for (int k = 0; k < 4 * periode / 512 + 2; ++k) { s.processa (nullptr, 0, L.data(), R.data(), 512); buf.insert (buf.end(), L.begin(), L.end()); }
        // comença on l'ona creua el zero pujant
        int ini = 0;
        for (int i = 1; i < periode && i + 1 < (int) buf.size(); ++i) if (buf[(size_t) i - 1] < 0.0f && buf[(size_t) i] >= 0.0f) { ini = i; break; }
        float pic = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const double x = ini + (double) i / std::max (1, n - 1) * 2.0 * periode;
            const int a = std::min ((int) buf.size() - 2, (int) x);
            const float t = (float) (x - a);
            sortida[i] = buf[(size_t) a] * (1.0f - t) + buf[(size_t) a + 1] * t;
            pic = std::max (pic, std::abs (sortida[i]));
        }
        return pic;
    }
}
