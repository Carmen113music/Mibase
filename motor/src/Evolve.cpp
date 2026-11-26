#include "mibase/Evolve.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <random>

namespace mibase::evolve
{
    namespace
    {
        constexpr double pi = 3.14159265358979323846;
        using S = saturacio::Saturacio;
        inline float lim (float x, float a, float b) { return x < a ? a : (x > b ? b : x); }
        inline float dB (float d) { return std::pow (10.0f, d / 20.0f); }

        const InfoParam infos[numParams] = {
            { "evolution", "Evolution", 0, 100, 35, false, false, "El pom gran: de 0 % (so original, sense processar) a 100 % (transformació extrema)." },
            { "drywet", "Dry/Wet", 0, 100, 100, false, false, "Barreja del so processat amb l'original (a 0 % d'Evolution, sempre l'original)." },
            { "entrada", "Input", -24, 12, 0, false, false, "Nivell que entra als efectes (més nivell = el Screamer i el Filter treballen més)." },
            { "sortida", "Output", -24, 12, 0, false, false, "Nivell de sortida del so processat." },
            { "hold", "Hold", 0, 1, 0, true, false, "Manté l'Evolution d'ara: mentre està encès, l'automatització i el pom no el mouen." },
            { "vista", "Vista", 0, 1, 0, true, false, "Simple o Advanced (només la finestra)." },

            { "scron", "Screamer", 0, 1, 1, true, false, "Encén o apaga el Screamer." },
            { "scrmode", "Tipus", 0, 4, 0, true, false, "Analògic (saturació suau), Overdrive, Distorsió, Fuzz o Plec (waveshaping que doblega l'ona)." },
            { "scrdrive", "Drive", 0, 100, 0, false, false, "Quanta distorsió (fins a +36 dB d'empenta). El volum es compensa sol." },
            { "scrtone", "Tone", 0, 100, 50, false, false, "Fosc ↔ brillant." },
            { "scrmix", "Mix", 0, 100, 0, false, false, "Quant Screamer se sent (0 % = res)." },
            { "scrout", "Output", -24, 12, 0, false, false, "Volum de sortida del Screamer." },

            { "filon", "Filter", 0, 1, 1, true, false, "Encén o apaga el Filter." },
            { "filtipus", "Tipus", 0, 3, 0, true, false, "LP (treu aguts), HP (treu greus), BP (només una banda) o Notch (treu una banda)." },
            { "filfreq", "Cutoff", 20, 20000, 20000, false, true, "Freqüència del filtre." },
            { "filreso", "Resonance", 0, 100, 10, false, false, "Ressonància: realça la zona del tall (molta = xiulet)." },
            { "fildrive", "Drive", 0, 100, 0, false, false, "Satura el so abans del filtre." },
            { "fillfodiv", "LFO", 0, 13, 5, true, false, "Velocitat del moviment del filtre, amb el tempo de la cançó." },
            { "fillfo", "Sweep", 0, 100, 0, false, false, "Quant es mou el filtre amb l'LFO (fins a 3 octaves amunt i avall)." },
            { "filmix", "Mix", 0, 100, 0, false, false, "Quant Filter se sent (0 % = res)." },

            { "pmpon", "Pumper", 0, 1, 1, true, false, "Encén o apaga el Pumper." },
            { "pmpmode", "Tipus", 0, 3, 0, true, false, "Ducking (bombeig com un sidechain), Trèmolo, Gate rítmic (patrons) o Pan (d'un costat a l'altre)." },
            { "pmpdiv", "Ritme", 0, 13, 5, true, false, "Cada quant bomba, amb el tempo de la cançó (amb punt i tresets)." },
            { "pmpdepth", "Depth", 0, 100, 0, false, false, "Quant baixa el volum a cada pulsació (0 % = res)." },
            { "pmpforma", "Forma", 0, 100, 50, false, false, "Ducking: com de ràpid torna. Trèmolo: rodó ↔ quadrat. Gate: tall sec ↔ suau." },
            { "pmppatro", "Patró", 0, 7, 0, true, false, "El patró de 16 passos del Gate rítmic." },

            { "shmon", "Shimmer", 0, 1, 1, true, false, "Encén o apaga el Shimmer." },
            { "shmtipus", "Tipus", 0, 2, 0, true, false, "Shimmer (cua que puja de to), Hall dens o Ambient." },
            { "shmpitch", "Pitch", 0, 3, 0, true, false, "Transposició de la cua: +12, +7, +5 o -12 semitons." },
            { "shmamt", "Shimmer", 0, 100, 40, false, false, "Quanta part de la cua es transposa." },
            { "shmdecay", "Decay", 0.3f, 20.0f, 5.0f, false, true, "Quant dura la cua." },
            { "shmsize", "Size", 0, 100, 70, false, false, "Mida de l'espai." },
            { "shmdiff", "Diffusion", 0, 100, 80, false, false, "Densitat de la cua: poca = ecos; molta = llisa." },
            { "shmmod", "Modulation", 0, 100, 40, false, false, "Moviment de la cua (cor suau)." },
            { "shmdamp", "Damping", 0, 100, 40, false, false, "Com de fosca és la cua." },
            { "shmfreeze", "Freeze", 0, 1, 0, true, false, "Congela la cua: queda sonant sense parar." },
            { "shmmix", "Mix", 0, 100, 0, false, false, "Quant Shimmer se sent (0 % = res; 100 % = només la cua)." },
        };

        struct Div { const char* nom; double negres; };
        const Div divs[] = {
            { "1/1", 4.0 }, { "1/2.", 3.0 }, { "1/2", 2.0 }, { "1/4.", 1.5 }, { "1/2T", 4.0 / 3.0 }, { "1/4", 1.0 }, { "1/8.", 0.75 },
            { "1/4T", 2.0 / 3.0 }, { "1/8", 0.5 }, { "1/16.", 0.375 }, { "1/8T", 1.0 / 3.0 }, { "1/16", 0.25 }, { "1/16T", 1.0 / 6.0 }, { "1/32", 0.125 },
        };
        struct Pat { const char* nom; const char* passos; };
        const Pat pats[] = {
            { "Corxeres", "x.x.x.x.x.x.x.x." }, { "Semis", "xxxxxxxxxxxxxxx." }, { "Galop", "x.xxx.xxx.xxx.xx" }, { "Trencat", "xx.x.xx.x..xx.x." },
            { "Contratemps", ".x.x.x.x.x.x.x.x" }, { "Tresillo", "x..x..x.x..x..x." }, { "Stutter", "xxxx....xx.x...." }, { "Esquitx", "x.......x.x.xxxx" },
        };
        float fraccio (double x) { return (float) (x - std::floor (x)); }
        float protegeix (float x) noexcept
        {
            const float a = std::fabs (x);
            if (a <= 0.9f) return x;
            const float y = 0.9f + 0.099f * std::tanh ((a - 0.9f) / 0.099f);
            return x < 0.0f ? -y : y;
        }
    }

    const InfoParam& info (int p) { return infos[std::clamp (p, 0, numParams - 1)]; }
    int paramDeClau (const std::string& c)
    {
        for (int i = 0; i < numParams; ++i) if (c == infos[i].clau) return i;
        return -1;
    }

    const char* nomModul (int m)
    {
        static const char* n[numModuls] = { "SCREAMER", "FILTER", "PUMPER", "SHIMMER" };
        return n[std::clamp (m, 0, numModuls - 1)];
    }
    const char* descripcioModul (int m)
    {
        static const char* n[numModuls] = { "Distorsió creativa: de saturació subtil a destrucció.", "Filtratge creatiu amb ressonància i moviment al tempo.",
                                            "Moviment rítmic: bombeig, trèmolo, gate i panoràmica.", "Espai: cues llargues que pugen de to." };
        return n[std::clamp (m, 0, numModuls - 1)];
    }
    int paramOn (int m) { static const int p[numModuls] = { ScrOn, FilOn, PmpOn, ShmOn }; return p[std::clamp (m, 0, numModuls - 1)]; }
    int paramMix (int m) { static const int p[numModuls] = { ScrMix, FilMix, PmpDepth, ShmMix }; return p[std::clamp (m, 0, numModuls - 1)]; }
    int primerParam (int m) { return paramOn (m); }
    int finalParam (int m) { return m + 1 < numModuls ? paramOn (m + 1) : numParams; }
    int modulDeParam (int p)
    {
        for (int m = 0; m < numModuls; ++m) if (p >= primerParam (m) && p < finalParam (m)) return m;
        return -1;
    }
    const char* nomModeScreamer (int m) { static const char* n[] = { "Analògic", "Overdrive", "Distorsió", "Fuzz", "Plec" }; return n[std::clamp (m, 0, 4)]; }
    const char* nomTipusFiltre (int t) { static const char* n[] = { "LP", "HP", "BP", "Notch" }; return n[std::clamp (t, 0, 3)]; }
    const char* nomModePumper (int m) { static const char* n[] = { "Ducking", "Trèmolo", "Gate", "Pan" }; return n[std::clamp (m, 0, 3)]; }
    const char* nomTipusShimmer (int t) { static const char* n[] = { "Shimmer", "Hall dens", "Ambient" }; return n[std::clamp (t, 0, 2)]; }
    int numDivisions() { return (int) (sizeof divs / sizeof divs[0]); }
    const char* nomDivisio (int i) { return divs[std::clamp (i, 0, numDivisions() - 1)].nom; }
    double divisioEnNegres (int i) { return divs[std::clamp (i, 0, numDivisions() - 1)].negres; }
    int numPatrons() { return (int) (sizeof pats / sizeof pats[0]); }
    const char* nomPatro (int i) { return pats[std::clamp (i, 0, numPatrons() - 1)].nom; }
    const char* patro (int i) { return pats[std::clamp (i, 0, numPatrons() - 1)].passos; }
    const char* nomCorba (int c) { static const char* n[] = { "Lineal", "Suau", "Ràpida", "En S" }; return n[std::clamp (c, 0, numCorbes - 1)]; }

    std::string textValor (int p, float v)
    {
        char b[48];
        switch (p)
        {
            case Entrada: case Sortida: case ScrOut: std::snprintf (b, sizeof b, "%+.1f dB", (double) v); return b;
            case FilFreq: if (v >= 1000.0f) std::snprintf (b, sizeof b, "%.1f kHz", (double) v / 1000.0); else std::snprintf (b, sizeof b, "%.0f Hz", (double) v); return b;
            case ShmDecay: std::snprintf (b, sizeof b, "%.1f s", (double) v); return b;
            case ScrMode: return nomModeScreamer ((int) std::lround (v));
            case FilTipus: return nomTipusFiltre ((int) std::lround (v));
            case PmpMode: return nomModePumper ((int) std::lround (v));
            case ShmTipus: return nomTipusShimmer ((int) std::lround (v));
            case FilLfoDiv: case PmpDiv: return nomDivisio ((int) std::lround (v));
            case PmpPatro: return nomPatro ((int) std::lround (v));
            case ShmPitch: { const int t = reverb::transposicio ((int) std::lround (v)); std::snprintf (b, sizeof b, "%+d", t); return b; }
            case Vista: return v > 0.5f ? "Advanced" : "Simple";
            case Hold: case ScrOn: case FilOn: case PmpOn: case ShmOn: case ShmFreeze: return v > 0.5f ? "Sí" : "No";
            default: std::snprintf (b, sizeof b, "%.0f %%", (double) v); return b;
        }
    }

    float aplicaCorba (int c, float t) noexcept
    {
        t = lim (t, 0.0f, 1.0f);
        switch (c)
        {
            case Suau: return t * t;                          // entra a poc a poc i s'accelera
            case Rapida: return std::sqrt (t);                // entra de cop i després s'alenteix
            case EnS: return t * t * (3.0f - 2.0f * t);
            default: return t;
        }
    }
    float valorEnEvolution (const Ruta& r, float base, float evo) noexcept
    {
        float t;
        if (r.a <= r.de + 1e-3f) t = evo >= r.de ? 1.0f : 0.0f;
        else t = (evo - r.de) / (r.a - r.de);
        const float f = aplicaCorba (r.corba, t);
        if (f <= 0.0f) return base;
        if (f >= 1.0f) return r.valor;
        const auto& in = info (r.param);
        if (in.log && base > 0.0f && r.valor > 0.0f)
            return std::exp (std::log (base) + (std::log (r.valor) - std::log (base)) * f);
        return base + (r.valor - base) * f;
    }

    // ---------------------------------------------------------------------------------------------
    Evolve::Evolve() : ctx (&ContextMusical::global())
    {
        for (int i = 0; i < numParams; ++i) par[(size_t) i].store (infos[i].def);
        for (int m = 0; m < numModuls; ++m) ord[(size_t) m].store (m);
        rev = std::make_unique<reverb::Reverb>();
        // Per defecte: la guitarra (o el que sigui) va guanyant espai, moviment i color.
        posa (ShmTipus, 0); posa (ShmPitch, 0); posa (FilTipus, 0); posa (PmpMode, 1); posa (PmpDiv, 8);
        posaRutes ({ { ShmMix, 10, 70, 45, Suau }, { ShmDecay, 30, 100, 12, Lineal }, { ShmAmt, 40, 100, 70, Lineal },
                     { FilMix, 20, 50, 100, Rapida }, { FilFreq, 50, 100, 1800, EnS }, { FilLfo, 60, 100, 45, Lineal },
                     { PmpDepth, 55, 95, 55, Suau }, { ScrMix, 70, 100, 45, Suau }, { ScrDrive, 70, 100, 55, Lineal } });
    }
    Evolve::~Evolve() = default;

    void Evolve::posaTempo (const std::atomic<double>* t) noexcept { tempo = t; if (rev != nullptr) rev->posaTempo (t); }

    InfoParametre Evolve::infoParametre (int i) const
    {
        const auto& f = info (i);
        InfoParametre r;
        r.id = f.clau;
        r.nom = f.nom;
        if (const int m = modulDeParam (i); m >= 0 && i != paramOn (m))
        {
            std::string n = nomModul (m);
            for (size_t k = 1; k < n.size(); ++k) n[k] = (char) std::tolower ((unsigned char) n[k]);
            r.nom = n + " · " + f.nom;
        }
        r.passos = f.enter ? (int) (f.mx - f.mn) + 1 : 0;
        r.automatitzable = i != Vista;
        return r;
    }
    void Evolve::posaParametre (int i, float v01) noexcept { posa (i, deNormal (i, v01)); }
    float Evolve::parametre (int i) const noexcept { return normal (i, valor (i)); }
    std::string Evolve::textParametre (int i, float v01) const
    {
        float v = deNormal (i, v01);
        if (info (i).enter) v = (float) std::lround (v);
        return textValor (i, v);
    }
    float Evolve::normal (int p, float v) noexcept
    {
        const auto& f = info (p);
        if (f.log) return lim ((float) (std::log (v / f.mn) / std::log (f.mx / f.mn)), 0.0f, 1.0f);
        return lim ((v - f.mn) / (f.mx - f.mn), 0.0f, 1.0f);
    }
    float Evolve::deNormal (int p, float x) noexcept
    {
        const auto& f = info (p);
        x = lim (x, 0.0f, 1.0f);
        return f.log ? f.mn * std::pow (f.mx / f.mn, x) : f.mn + (f.mx - f.mn) * x;
    }
    void Evolve::posa (int p, float v) noexcept
    {
        if (p < 0 || p >= numParams || ! std::isfinite (v)) return;
        v = lim (v, infos[p].mn, infos[p].mx);
        if (infos[p].enter) v = (float) std::lround (v);
        par[(size_t) p].store (v, std::memory_order_relaxed);
    }
    float Evolve::valor (int p) const noexcept { return p >= 0 && p < numParams ? par[(size_t) p].load (std::memory_order_relaxed) : 0.0f; }

    float Evolve::efectiu (int p, float evo) const noexcept
    {
        if (p < 0 || p >= numParams) return 0.0f;
        float v = valor (p);
        const int n = std::min (nRutes.load (std::memory_order_acquire), maxRutes);
        for (int i = 0; i < n; ++i)
            if (rt[(size_t) i].param.load (std::memory_order_relaxed) == p)
            {
                const Ruta r { p, rt[(size_t) i].de.load (std::memory_order_relaxed), rt[(size_t) i].a.load (std::memory_order_relaxed),
                               rt[(size_t) i].valor.load (std::memory_order_relaxed), rt[(size_t) i].corba.load (std::memory_order_relaxed) };
                v = valorEnEvolution (r, v, evo);
            }
        v = lim (v, infos[p].mn, infos[p].mx);
        if (infos[p].enter) v = (float) std::lround (v);
        return v;
    }

    std::vector<Ruta> Evolve::rutes() const
    {
        std::vector<Ruta> v;
        const int n = std::min (nRutes.load (std::memory_order_acquire), maxRutes);
        for (int i = 0; i < n; ++i)
            v.push_back ({ rt[(size_t) i].param.load(), rt[(size_t) i].de.load(), rt[(size_t) i].a.load(), rt[(size_t) i].valor.load(), rt[(size_t) i].corba.load() });
        return v;
    }
    void Evolve::posaRutes (const std::vector<Ruta>& v)
    {
        nRutes.store (0, std::memory_order_release);   // el fil d'àudio deixa de llegir-les mentre canvien
        int k = 0;
        for (const auto& r : v)
        {
            if (k >= maxRutes) break;
            if (r.param < 0 || r.param >= numParams || r.param == Evolution || r.param == Hold || r.param == Vista) continue;
            auto& d = rt[(size_t) k++];
            d.param.store (r.param);
            d.de.store (lim (r.de, 0.0f, 100.0f));
            d.a.store (lim (r.a, 0.0f, 100.0f));
            d.valor.store (lim (r.valor, infos[r.param].mn, infos[r.param].mx));
            d.corba.store (std::clamp (r.corba, 0, numCorbes - 1));
        }
        nRutes.store (k, std::memory_order_release);
    }
    std::array<int, numModuls> Evolve::ordre() const noexcept
    {
        std::array<int, numModuls> o {};
        for (int i = 0; i < numModuls; ++i) o[(size_t) i] = ord[(size_t) i].load (std::memory_order_relaxed);
        return o;
    }
    void Evolve::posaOrdre (const std::array<int, numModuls>& o) noexcept
    {
        // ha de ser una permutació; si no, l'ordre de sempre
        bool vist[numModuls] {};
        bool bo = true;
        for (int m : o) { if (m < 0 || m >= numModuls || vist[m]) { bo = false; break; } vist[m] = true; }
        for (int i = 0; i < numModuls; ++i) ord[(size_t) i].store (bo ? o[(size_t) i] : i, std::memory_order_relaxed);
    }

    std::string Evolve::estat() const
    {
        std::string s = "mibase-evolve=1";
        char b[96];
        for (int i = 0; i < numParams; ++i)
        {
            std::snprintf (b, sizeof b, ";%s=%.6g", infos[i].clau, (double) valor (i));
            s += b;
        }
        s += ";ordre=";
        const auto o = ordre();
        for (int i = 0; i < numModuls; ++i) s += (i > 0 ? "," : "") + std::to_string (o[(size_t) i]);
        s += ";rutes=";
        bool primera = true;
        for (const auto& r : rutes())
        {
            std::snprintf (b, sizeof b, "%s%s:%.6g:%.6g:%.6g:%d", primera ? "" : "|", infos[r.param].clau, (double) r.de, (double) r.a, (double) r.valor, r.corba);
            s += b;
            primera = false;
        }
        return s;
    }

    void Evolve::posaEstat (const std::string& s)
    {
        if (s.rfind ("mibase-evolve=", 0) != 0) return;
        for (int i = 0; i < numParams; ++i) posa (i, infos[i].def);
        std::array<int, numModuls> o {};
        for (int i = 0; i < numModuls; ++i) o[(size_t) i] = i;
        std::vector<Ruta> rs;
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
                if (clau == "ordre")
                {
                    size_t c = 0; int k = 0;
                    while (c <= val.size() && k < numModuls)
                    {
                        size_t d = val.find (',', c);
                        if (d == std::string::npos) d = val.size();
                        if (d > c) o[(size_t) k++] = std::atoi (val.substr (c, d - c).c_str());
                        c = d + 1;
                    }
                }
                else if (clau == "rutes")
                {
                    size_t c = 0;
                    while (c < val.size())
                    {
                        size_t d = val.find ('|', c);
                        if (d == std::string::npos) d = val.size();
                        const auto t = val.substr (c, d - c);
                        const auto p1 = t.find (':');
                        if (p1 != std::string::npos)
                        {
                            Ruta r;
                            r.param = paramDeClau (t.substr (0, p1));
                            float x[3] { 0, 100, 0 }; int corba = 0;
                            std::sscanf (t.c_str() + p1 + 1, "%f:%f:%f:%d", &x[0], &x[1], &x[2], &corba);
                            r.de = x[0]; r.a = x[1]; r.valor = x[2]; r.corba = corba;
                            if (r.param >= 0) rs.push_back (r);
                        }
                        c = d + 1;
                    }
                }
                else if (const int p = paramDeClau (clau); p >= 0)
                    posa (p, (float) std::strtod (val.c_str(), nullptr));
            }
            a = b + 1;
        }
        posaOrdre (o);
        posaRutes (rs);
    }

    // ---------------------------------------------------------------------------------------------
    void Evolve::prepara (double nouFs, int)
    {
        fs = nouFs > 0.0 ? nouFs : 48000.0;
        rev->prepara (fs, subBloc);
        for (auto& c : scr.ch)
        {
            c = Screamer::Canal {};
            static const double qs[4] = { 0.5098, 0.6013, 0.9000, 2.5629 };
            for (int k = 0; k < 4; ++k) { S::passaBaixos (c.up[k], 0.45 * fs, qs[k], fs * 4.0); c.down[k] = c.up[k]; }
            S::passaAlts (c.dc, 10.0, 0.7071, fs * 4.0);
        }
        scr.toneVist = -1.0f; scr.modeVist = -1;
        shmMixVist = -1.0f;
        primer = true;
        neteja();
    }

    void Evolve::neteja() noexcept
    {
        for (auto& c : scr.ch)
            for (Bq* b : { &c.up[0], &c.up[1], &c.up[2], &c.up[3], &c.down[0], &c.down[1], &c.down[2], &c.down[3], &c.dc, &c.tilt, &c.lp })
                b->neteja();
        scr.mix = 0.0f; scr.eIn = scr.eOut = 0.0f; scr.comp = -1.0f;   // -1: s'estima en començar
        for (int c = 0; c < 2; ++c) fil.s1[c] = fil.s2[c] = 0.0f;
        fil.mix = 0.0f;
        pmp.gL = pmp.gR = pmp.gate = 1.0f; pmp.depth = 0.0f;
        if (rev != nullptr) rev->buida();
    }

    void Evolve::calculaEfectius (float evo) noexcept
    {
        for (int p = 0; p < numParams; ++p) ef[(size_t) p] = par[(size_t) p].load (std::memory_order_relaxed);
        const int n = std::min (nRutes.load (std::memory_order_acquire), maxRutes);
        for (int i = 0; i < n; ++i)
        {
            const int p = rt[(size_t) i].param.load (std::memory_order_relaxed);
            if (p < 0 || p >= numParams) continue;
            const Ruta r { p, rt[(size_t) i].de.load (std::memory_order_relaxed), rt[(size_t) i].a.load (std::memory_order_relaxed),
                           rt[(size_t) i].valor.load (std::memory_order_relaxed), rt[(size_t) i].corba.load (std::memory_order_relaxed) };
            ef[(size_t) p] = valorEnEvolution (r, ef[(size_t) p], evo);
        }
        for (int p = 0; p < numParams; ++p)
        {
            float v = lim (ef[(size_t) p], infos[p].mn, infos[p].mx);
            if (infos[p].enter) v = (float) std::lround (v);
            ef[(size_t) p] = v;
        }
    }

    void Evolve::processa (float* L, float* R, int n) noexcept
    {
        if (n <= 0) return;
        mes.blocs.fetch_add (1, std::memory_order_relaxed);
        if (demanaBuidar.exchange (false)) neteja();
        double bpm = tempo != nullptr ? tempo->load (std::memory_order_relaxed) : 120.0;
        if (! (bpm > 1.0)) bpm = 120.0;
        // on és la cançó (en negres): si sona, del transport; si no (o exportant), el comptador propi continua
        if (ctx != nullptr && ctx->sonant.load (std::memory_order_relaxed)) negresLliure = ctx->segons.load (std::memory_order_relaxed) * bpm / 60.0;
        double negres = negresLliure;
        const double negresPerMostra = bpm / 60.0 / fs;
        float picE = 0.0f, picS = 0.0f, prot = 0.0f;
        std::array<float, numModuls> act {};

        for (int o = 0; o < n; o += subBloc)
        {
            const int m = std::min (subBloc, n - o);
            float* l = L + o;
            float* r = R + o;
            for (int i = 0; i < m; ++i) picE = std::max (picE, std::max (std::fabs (l[i]), std::fabs (r[i])));

            // Evolution que sona: la del pom (o la retinguda pel Hold), suavitzada
            const bool hold = valor (Hold) > 0.5f;
            if (hold && ! holdAbans) evoHold = evoS;   // es queda el que sona ara
            holdAbans = hold;
            const float evoObj = hold ? evoHold : valor (Evolution);
            const float kObj = evoObj > 0.0f ? 1.0f : 0.0f;
            if (primer) { evoS = evoObj; kBy = kObj; primer = false; }
            evoS += (evoObj - evoS) * (1.0f - std::exp (-(float) m / (float) (0.04 * fs)));
            if (std::fabs (evoObj - evoS) < 1e-4f) evoS = evoObj;

            if (kBy <= 0.0f && kObj <= 0.0f)
            {
                // 0 %: bypass real, el so no es toca
                brut = true;
                for (int i = 0; i < m; ++i) picS = std::max (picS, std::max (std::fabs (l[i]), std::fabs (r[i])));
                negres += m * negresPerMostra;
                continue;
            }
            if (brut) { neteja(); brut = false; }

            calculaEfectius (evoS);
            float dl[subBloc], dr[subBloc];
            std::copy (l, l + m, dl);
            std::copy (r, r + m, dr);

            const float gIn0 = gIn, gIn1 = dB (ef[Entrada]);
            for (int i = 0; i < m; ++i)
            {
                const float g = gIn0 + (gIn1 - gIn0) * (float) (i + 1) / (float) m;
                l[i] *= g; r[i] *= g;
            }
            gIn = gIn1;

            const auto o2 = ordre();
            for (int k = 0; k < numModuls; ++k)
            {
                const int mod = o2[(size_t) k];
                const bool on = ef[(size_t) paramOn (mod)] > 0.5f;
                act[(size_t) mod] = std::max (act[(size_t) mod], on ? ef[(size_t) paramMix (mod)] / 100.0f : 0.0f);
                // un mòdul amb Mix a 0 (i que ja no sona) no es calcula: no gasta CPU
                const bool vol = on && ef[(size_t) paramMix (mod)] > 0.0f;
                switch (mod)
                {
                    case MScreamer: if (vol || scr.mix > 0.0f) processaScreamer (l, r, m); break;
                    case MFiltre: if (vol || fil.mix > 0.0f) processaFiltre (l, r, m, negres, bpm); break;
                    case MPumper: if (vol || pmp.depth > 0.0f || pmp.gL != 1.0f || pmp.gR != 1.0f) processaPumper (l, r, m, negres, bpm); break;
                    default: if (vol || shmMixVist > 0.0f) processaShimmer (l, r, m); break;
                }
            }

            // Dry/Wet, sortida, protecció i el pas suau cap al bypass
            const float dw0 = dw, dw1 = ef[DryWet] * 0.01f, gO0 = gOut, gO1 = dB (ef[Sortida]);
            const float pasK = (float) (1.0 / (0.01 * fs));
            for (int i = 0; i < m; ++i)
            {
                const float t = (float) (i + 1) / (float) m;
                const float w = dw0 + (dw1 - dw0) * t, go = gO0 + (gO1 - gO0) * t;
                kBy = kObj > kBy ? std::min (kObj, kBy + pasK) : std::max (kObj, kBy - pasK);
                float pl = dl[i] * (1.0f - w) + l[i] * go * w;
                float pr = dr[i] * (1.0f - w) + r[i] * go * w;
                if (! std::isfinite (pl)) pl = 0.0f;
                if (! std::isfinite (pr)) pr = 0.0f;
                const float ql = protegeix (pl), qr = protegeix (pr);
                if (ql != pl || qr != pr) prot = 1.0f;
                l[i] = dl[i] + kBy * (ql - dl[i]);
                r[i] = dr[i] + kBy * (qr - dr[i]);
                picS = std::max (picS, std::max (std::fabs (l[i]), std::fabs (r[i])));
            }
            dw = dw1; gOut = gO1;
            negres += m * negresPerMostra;
        }
        negresLliure = negres;

        mes.entrada.store (std::max (picE, mes.entrada.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
        mes.sortida.store (std::max (picS, mes.sortida.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
        mes.evolution.store (evoS, std::memory_order_relaxed);
        mes.bypass.store (1.0f - kBy, std::memory_order_relaxed);
        mes.protegeix.store (std::max (prot, mes.protegeix.load (std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
        for (int k = 0; k < numModuls; ++k) mes.activitat[(size_t) k].store (kBy > 0.0f ? act[(size_t) k] : 0.0f, std::memory_order_relaxed);
    }

    // ---- SCREAMER ----
    void Evolve::processaScreamer (float* L, float* R, int n) noexcept
    {
        const bool on = ef[ScrOn] > 0.5f;
        const int mode = (int) ef[ScrMode];
        const float tone = ef[ScrTone];
        if (mode != scr.modeVist || std::fabs (tone - scr.toneVist) > 0.25f)
        {
            scr.modeVist = mode; scr.toneVist = tone;
            const double fLp = std::min (0.45 * fs, (mode == 3 ? 2500.0 : 3500.0) * std::pow (5.0, tone / 100.0));
            for (auto& c : scr.ch)
            {
                S::prestatgeAlt (c.tilt, 2500.0, (tone - 50.0f) * 0.2f, fs);
                S::passaBaixos (c.lp, fLp, 0.7071, fs);
            }
        }
        const float gObj = dB (ef[ScrDrive] * 0.36f);
        const float mixObj = on ? ef[ScrMix] * 0.01f : 0.0f, outObj = dB (ef[ScrOut]);
        const float g0 = scr.g, m0 = scr.mix, o0 = scr.out;
        if (scr.comp < 0.0f) scr.comp = std::pow (std::max (gObj, 1.0f), -0.6f);
        // Volum automàtic: el so distorsionat surt amb la mateixa energia que el que entra (mesurada a ~300 ms).
        const float aEnv = 1.0f - std::exp (-1.0f / (0.3f * (float) fs));
        float* io[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) (i + 1) / (float) n;
            const float g = g0 + (gObj - g0) * t, mx = m0 + (mixObj - m0) * t, ou = o0 + (outObj - o0) * t;
            float y2[2];
            for (int c = 0; c < 2; ++c)
            {
                auto& k = scr.ch[c];
                const float v = io[c][i] * g;
                float y = 0.0f;
                for (int s = 0; s < 4; ++s)
                {
                    float u = s == 0 ? v * 4.0f : 0.0f;
                    for (auto& f : k.up) u = f (u);
                    switch (mode)
                    {
                        case 0: u = std::tanh (u); break;
                        case 1: u = drive::corba (drive::Overdrive, u); break;
                        case 2: u = drive::corba (drive::Distorsio, u); break;
                        case 3: u = drive::corba (drive::Fuzz, u); break;
                        default: u = std::sin (u); break;   // plec: passat de ±π/2 l'ona es doblega
                    }
                    u = k.dc (u);
                    for (auto& f : k.down) u = f (u);
                    if (s == 0) y = u;
                }
                y2[c] = k.lp (k.tilt (y));
            }
            const float xe = 0.5f * (io[0][i] * io[0][i] + io[1][i] * io[1][i]), ye = 0.5f * (y2[0] * y2[0] + y2[1] * y2[1]);
            scr.eIn += (xe - scr.eIn) * aEnv;
            scr.eOut += (ye - scr.eOut) * aEnv;
            const float objAuto = scr.eOut > 1e-9f ? lim (std::sqrt (scr.eIn / scr.eOut), 0.05f, 2.0f) : scr.comp;
            scr.comp += (objAuto - scr.comp) * aEnv * 4.0f;
            for (int c = 0; c < 2; ++c)
            {
                const float x = io[c][i];
                io[c][i] = x + mx * (y2[c] * scr.comp * ou - x);
            }
        }
        scr.g = gObj; scr.mix = mixObj; scr.out = outObj;
    }

    // ---- FILTER ----
    void Evolve::processaFiltre (float* L, float* R, int n, double negres, double bpm) noexcept
    {
        const bool on = ef[FilOn] > 0.5f;
        const int tipus = (int) ef[FilTipus];
        const double len = divisioEnNegres ((int) ef[FilLfoDiv]);
        const float oct = ef[FilLfo] * 0.03f;
        const double negresFi = negres + n * bpm / 60.0 / fs;
        const float lfo = std::sin ((float) (2.0 * pi) * fraccio (negresFi / len));
        const float f = lim (ef[FilFreq] * std::pow (2.0f, oct * lfo), 20.0f, (float) (0.45 * fs));
        const float gObj = std::tan ((float) pi * f / (float) fs);
        const float kObj = 2.0f - 1.96f * ef[FilReso] * 0.01f;
        const float mixObj = on ? ef[FilMix] * 0.01f : 0.0f;
        const float drvObj = 1.0f + ef[FilDrive] * 0.15f;
        const float g0 = fil.g, k0 = fil.k, m0 = fil.mix, d0 = fil.drv;
        float* io[2] = { L, R };
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) (i + 1) / (float) n;
            const float g = g0 + (gObj - g0) * t, k = k0 + (kObj - k0) * t, mx = m0 + (mixObj - m0) * t, gd = d0 + (drvObj - d0) * t;
            const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const float comp = std::pow (std::min (1.0f, k / 1.4142f), 0.75f);
            const float normD = std::pow (gd, -0.75f);
            for (int c = 0; c < 2; ++c)
            {
                const float x = io[c][i];
                const float xd = gd > 1.001f ? std::tanh (x * gd) * normD : x;
                const float v3 = xd - fil.s2[c];
                const float v1 = a1 * fil.s1[c] + a2 * v3;
                const float v2 = fil.s2[c] + a2 * fil.s1[c] + a3 * v3;
                fil.s1[c] = 2.0f * v1 - fil.s1[c];
                fil.s2[c] = 2.0f * v2 - fil.s2[c];
                float y;
                switch (tipus)
                {
                    case 1: y = (xd - k * v1 - v2) * comp; break;
                    case 2: y = v1 * k; break;
                    case 3: y = xd - k * v1; break;
                    default: y = v2 * comp; break;
                }
                io[c][i] = x + mx * (y - x);
            }
        }
        for (int c = 0; c < 2; ++c)
        {
            if (std::fabs (fil.s1[c]) < 1e-20f || ! std::isfinite (fil.s1[c])) fil.s1[c] = 0.0f;
            if (std::fabs (fil.s2[c]) < 1e-20f || ! std::isfinite (fil.s2[c])) fil.s2[c] = 0.0f;
        }
        fil.g = gObj; fil.k = kObj; fil.mix = mixObj; fil.drv = drvObj;
    }

    // ---- PUMPER ----
    void Evolve::processaPumper (float* L, float* R, int n, double negres, double bpm) noexcept
    {
        const bool on = ef[PmpOn] > 0.5f;
        const int mode = (int) ef[PmpMode];
        const double len = divisioEnNegres ((int) ef[PmpDiv]);
        const float forma = ef[PmpForma] * 0.01f;
        const char* pat = patro ((int) ef[PmpPatro]);
        const float dObj = on ? ef[PmpDepth] * 0.01f : 0.0f, d0 = pmp.depth;
        const double pas = bpm / 60.0 / fs;
        const float msSuau = mode == 2 ? 0.5f + forma * 20.0f : 1.5f;
        const float cS = 1.0f - std::exp (-1.0f / (msSuau * 0.001f * (float) fs));
        const float expo = 1.0f + forma * 8.0f;
        for (int i = 0; i < n; ++i)
        {
            const float d = d0 + (dObj - d0) * (float) (i + 1) / (float) n;
            const double b = negres + i * pas;
            const float ph = fraccio (b / len);
            float gl = 1.0f, gr = 1.0f;
            switch (mode)
            {
                case 0: gl = gr = 1.0f - d * std::pow (1.0f - ph, expo); break;   // ducking: cau al començament i torna
                case 1:
                {
                    float s = 0.5f - 0.5f * std::cos ((float) (2.0 * pi) * ph);
                    const float dur = 1.0f + forma * 12.0f;
                    s = 0.5f + 0.5f * std::tanh ((s - 0.5f) * dur) / std::tanh (0.5f * dur);
                    gl = gr = 1.0f - d * s;
                    break;
                }
                case 2:
                {
                    const long long pasN = (long long) std::floor (b / len);
                    const int k = (int) (((pasN % 16) + 16) % 16);
                    gl = gr = pat[k] == 'x' ? 1.0f : 1.0f - d;
                    break;
                }
                default:
                {
                    const float s = std::sin ((float) (2.0 * pi) * ph);
                    gl = 1.0f - d * std::max (0.0f, s);
                    gr = 1.0f - d * std::max (0.0f, -s);
                    break;
                }
            }
            pmp.gL += (gl - pmp.gL) * cS;
            pmp.gR += (gr - pmp.gR) * cS;
            L[i] *= pmp.gL;
            R[i] *= pmp.gR;
        }
        pmp.depth = dObj;
        if (dObj <= 0.0f && std::fabs (pmp.gL - 1.0f) < 1e-4f && std::fabs (pmp.gR - 1.0f) < 1e-4f) pmp.gL = pmp.gR = 1.0f;
    }

    // ---- SHIMMER (la Reverb de Mibase per dins) ----
    void Evolve::processaShimmer (float* L, float* R, int n) noexcept
    {
        const bool on = ef[ShmOn] > 0.5f;
        static const int algs[3] = { reverb::Shimmer, reverb::Hall, reverb::Ambient };
        const float alg = (float) algs[std::clamp ((int) ef[ShmTipus], 0, 2)];
        auto posaSiCal = [this] (int p, float v) { if (rev->valor (p) != v) rev->posa (p, v); };
        posaSiCal (reverb::Alg, alg);
        posaSiCal (reverb::ShimOn, 1.0f);
        posaSiCal (reverb::ShimPitch, ef[ShmPitch]);
        posaSiCal (reverb::ShimAmt, ef[ShmAmt]);
        posaSiCal (reverb::Decay, ef[ShmDecay]);
        posaSiCal (reverb::Size, ef[ShmSize]);
        posaSiCal (reverb::Diffusion, ef[ShmDiff]);
        posaSiCal (reverb::ModDepth, ef[ShmMod]);
        posaSiCal (reverb::Damping, ef[ShmDamp]);
        posaSiCal (reverb::FreezeOn, ef[ShmFreeze] > 0.5f ? 1.0f : 0.0f);
        posaSiCal (reverb::LowCut, 120.0f);
        posaSiCal (reverb::PreDelay, 15.0f);
        const float mix = on ? ef[ShmMix] : 0.0f;
        posaSiCal (reverb::Mix, mix);
        rev->processa (L, R, n);
        shmMixVist = mix;
    }

    // ---- Atzar musical ----
    std::string Evolve::estatAtzar (unsigned llavor) const
    {
        std::mt19937 rng (llavor);
        auto u = [&] (float a, float b) { return std::uniform_real_distribution<float> (a, b) (rng); };
        auto e = [&] (int a, int b) { return std::uniform_int_distribution<int> (a, b) (rng); };
        Evolve x;
        x.posa (Evolution, valor (Evolution));
        x.posa (DryWet, 100.0f);
        for (int m = 0; m < numModuls; ++m) x.posa (paramOn (m), 0.0f);
        // 2–4 mòduls, el Shimmer o el Filter gairebé sempre (perquè soni a «transformació» i no a soroll)
        bool usa[numModuls] {};
        usa[e (0, 1) == 0 ? MShimmer : MFiltre] = true;
        const int quants = e (2, 4);
        while ((int) (usa[0] + usa[1] + usa[2] + usa[3]) < quants) usa[e (0, numModuls - 1)] = true;
        std::vector<Ruta> rs;
        float entra = u (0.0f, 25.0f);
        auto tram = [&] { const float de = entra; const float a = std::min (100.0f, de + u (25.0f, 55.0f)); entra = std::min (75.0f, entra + u (10.0f, 25.0f)); return std::pair<float, float> (de, a); };
        if (usa[MFiltre])
        {
            x.posa (FilOn, 1); const int t = e (0, 2); x.posa (FilTipus, (float) t);
            x.posa (FilReso, u (10.0f, 45.0f)); x.posa (FilLfoDiv, (float) e (2, 11));
            x.posa (FilFreq, t == 1 ? 20.0f : 20000.0f);
            const auto [de, a] = tram();
            rs.push_back ({ FilMix, de, std::min (100.0f, de + 10.0f), 100.0f, Rapida });
            rs.push_back ({ FilFreq, de, a, t == 1 ? u (300.0f, 1500.0f) : u (500.0f, 3000.0f), EnS });
            rs.push_back ({ FilLfo, a * 0.7f, 100.0f, u (20.0f, 70.0f), Lineal });
            rs.push_back ({ FilReso, a * 0.7f, 100.0f, u (40.0f, 70.0f), Suau });
        }
        if (usa[MScreamer])
        {
            x.posa (ScrOn, 1); x.posa (ScrMode, (float) e (0, 4)); x.posa (ScrTone, u (30.0f, 70.0f));
            const auto [de, a] = tram();
            rs.push_back ({ ScrMix, de, a, u (40.0f, 100.0f), Suau });
            rs.push_back ({ ScrDrive, de, 100.0f, u (35.0f, 85.0f), Lineal });
        }
        if (usa[MPumper])
        {
            x.posa (PmpOn, 1); x.posa (PmpMode, (float) e (0, 3)); x.posa (PmpDiv, (float) e (3, 11));
            x.posa (PmpForma, u (20.0f, 80.0f)); x.posa (PmpPatro, (float) e (0, numPatrons() - 1));
            const auto [de, a] = tram();
            rs.push_back ({ PmpDepth, de, a, u (40.0f, 85.0f), Suau });
        }
        if (usa[MShimmer])
        {
            x.posa (ShmOn, 1); x.posa (ShmTipus, (float) e (0, 2)); x.posa (ShmPitch, (float) (e (0, 3) == 3 ? 3 : e (0, 1)));
            x.posa (ShmSize, u (50.0f, 95.0f)); x.posa (ShmDamp, u (20.0f, 60.0f)); x.posa (ShmMod, u (20.0f, 70.0f)); x.posa (ShmDecay, u (1.5f, 4.0f));
            const auto [de, a] = tram();
            rs.push_back ({ ShmMix, de, a, u (25.0f, 55.0f), Suau });
            rs.push_back ({ ShmDecay, de, 100.0f, u (6.0f, 18.0f), Lineal });
            rs.push_back ({ ShmAmt, de, 100.0f, u (30.0f, 80.0f), Lineal });
        }
        x.posaRutes (rs);
        // ordre: gairebé sempre distorsió → filtre → moviment → espai; de tant en tant el filtre primer
        std::array<int, numModuls> o { MScreamer, MFiltre, MPumper, MShimmer };
        if (e (0, 3) == 0) std::swap (o[0], o[1]);
        if (e (0, 4) == 0) std::swap (o[2], o[3]);
        x.posaOrdre (o);
        return x.estat();
    }
}
