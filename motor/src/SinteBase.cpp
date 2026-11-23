#include "mibase/SinteBase.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace mibase::sinte
{
    float normal (const InfoParam& i, float v) noexcept
    {
        v = std::clamp (v, i.mn, i.mx);
        switch (i.escala)
        {
            case Log: return (float) (std::log (v / i.mn) / std::log (i.mx / i.mn));
            case Cub: return (float) std::cbrt ((v - i.mn) / (i.mx - i.mn));
            default: return (v - i.mn) / (i.mx - i.mn);
        }
    }

    float deNormal (const InfoParam& i, float x) noexcept
    {
        x = std::clamp (x, 0.0f, 1.0f);
        switch (i.escala)
        {
            case Log: return (float) (i.mn * std::pow (i.mx / i.mn, (double) x));
            case Cub: return i.mn + (i.mx - i.mn) * x * x * x;
            case Enter: return std::round (i.mn + (i.mx - i.mn) * x);
            default: return i.mn + (i.mx - i.mn) * x;
        }
    }

    std::vector<std::string> opcions (const InfoParam& i)
    {
        std::vector<std::string> r;
        if (i.opcions == nullptr) return r;
        std::string s (i.opcions);
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

    std::string num (double v, int dec)
    {
        char b[32];
        std::snprintf (b, sizeof b, "%.*f", dec, v);
        std::string s (b);
        for (auto& c : s) if (c == '.') c = ',';
        if (s == "-0" || s == "-0,0" || s == "-0,00") s = s.substr (1);
        return s;
    }
    std::string ms (double v) { return v < 10.0 ? num (v, 1) + " ms" : v < 1000.0 ? num (v, 0) + " ms" : num (v / 1000.0, 2) + " s"; }

    std::atomic<double>& tempo() { static std::atomic<double> t { 120.0 }; return t; }

    double negresRitme (int r) noexcept
    {
        static const double d[] = { 0.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0, 1.5, 0.75 };
        return d[std::clamp (r, 0, 11)];
    }
    double negresArp (int r) noexcept
    {
        static const double d[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        return d[std::clamp (r, 0, 5)];
    }

    const std::vector<Patro>& patrons()
    {
        // «R» = silenci; «a» = accent; «l» = lligat (llisca a la següent en mode legato)
        static const std::vector<Patro> v = []
        {
            struct D { const char* nom; const char* passos; };
            const D d[] = {
                { "Octaves", "0 12" },
                { "Arrel i quinta", "0 7 12 7" },
                { "Berlinesa", "0 0 12 0 7 0 10 0" },
                { "Galop", "0a R 0 0" },
                { "Àcid 1", "0 0a 12l 0 3 0 10a 0l 7 0 12 0a 5l 0 3 0" },
                { "Àcid 2", "0a 12 0 0l 15 0 12a 0 7l 5 0 0 10 0a 12l 0" },
                { "Trance", "0 12 7 12 0 12 10 12" },
                { "Synthwave", "0 0 12 0 0 0 12 0 -2 -2 10 -2 -4 -4 8 -4" },
                { "Arpegi menor", "0 3 7 12 15 12 7 3" },
                { "Arpegi major", "0 4 7 12 16 12 7 4" },
                { "Pulsació", "0a R 0 0 0a R 0 R" },
                { "Funk", "0a R 12 0 R 0 10 R 0a R 7 R 12l 10 7 R" },
                { "Techno", "0a R 0 R 0 0 R 0" },
                { "Escala", "0 2 3 5 7 8 10 12" },
                { "Hook", "0 R 7 R 12l 10 7 R 0 R 7 R 5 3 2 R" },
                { "Repeteix", "0a 0 0 0 0 0 0a 0 0 0 0 0 0a 0 0 0" },
            };
            std::vector<Patro> r;
            for (const auto& x : d)
            {
                Patro p { x.nom, {} };
                std::string s (x.passos);
                size_t a = 0;
                while (a < s.size())
                {
                    size_t b = s.find (' ', a);
                    if (b == std::string::npos) b = s.size();
                    const auto t = s.substr (a, b - a);
                    PasPatro pp;
                    if (t == "R") pp.silenci = true;
                    else
                    {
                        pp.to = std::atoi (t.c_str());
                        pp.accent = t.find ('a') != std::string::npos;
                        pp.lligat = t.find ('l') != std::string::npos;
                    }
                    p.passos.push_back (pp);
                    a = b + 1;
                }
                r.push_back (p);
            }
            return r;
        }();
        return v;
    }

    std::string opcionsPatrons()
    {
        std::string s;
        for (const auto& p : patrons()) s += (s.empty() ? "" : "|") + std::string (p.nom);
        return s;
    }

    // ---- delmador ----
    namespace
    {
        const float* coefHb()
        {
            static float h[Delmador::taps];
            static bool fet = false;
            if (! fet)
            {
                constexpr double dpi = 3.14159265358979323846;
                double suma = 0.0;
                const int c = Delmador::taps / 2;
                for (int i = 0; i < Delmador::taps; ++i)
                {
                    const int d = i - c;
                    const double v = d == 0 ? 0.5 : (d % 2 == 0 ? 0.0 : std::sin (dpi * d / 2.0) / (dpi * d));
                    const double w = 0.42 - 0.5 * std::cos (2.0 * dpi * i / (Delmador::taps - 1)) + 0.08 * std::cos (4.0 * dpi * i / (Delmador::taps - 1));
                    h[i] = (float) (v * w);
                    suma += h[i];
                }
                for (auto& x : h) x = (float) (x / suma);
                fet = true;
            }
            return h;
        }
        const float* const coefInicial = coefHb();
    }

    void Delmador::processa (const float* in, float* out, int n) noexcept
    {
        const float* hb = coefHb();
        constexpr int h = taps - 1, c = taps / 2;
        float w[1024 + taps];
        std::memcpy (w, hist, sizeof (float) * h);
        std::memcpy (w + h, in, sizeof (float) * (size_t) (2 * n));
        for (int i = 0; i < n; ++i)
        {
            const float* x = w + 2 * i + 1;
            float y = hb[c] * x[c];
            for (int j = 0; j < taps; j += 2) y += hb[j] * x[j];
            out[i] = y;
        }
        std::memcpy (hist, w + 2 * n, sizeof (float) * h);
    }

    //==========================================================================
    Base::Base (const InfoParam* t, int n, std::string pre, IndexArp arp)
        : taula (t), numP (n), prefix (std::move (pre)), ia (arp), par ((size_t) n), ona ((size_t) midaOna)
    {
        (void) coefInicial;
        for (int i = 0; i < numP; ++i) par[(size_t) i].store (taula[i].def);
        for (auto& c : ccMapa) c.store (-1);
        for (auto& x : notesOn) x.store (0);
        for (auto& o : ona) o.store (0.0f);
    }

    InfoParametre Base::infoParametre (int i) const
    {
        InfoParametre r;
        const auto& x = info (i);
        r.id = x.clau;
        r.nom = nomComplet (i);
        r.passos = x.escala == Enter ? (int) (x.mx - x.mn) + 1 : 0;
        return r;
    }
    std::string Base::nomComplet (int p) const { return info (p).nom; }
    void Base::posaParametre (int i, float v01) noexcept { if (i >= 0 && i < numP) posa (i, deNormal (taula[i], v01)); }
    float Base::parametre (int i) const noexcept { return i >= 0 && i < numP ? normal (taula[i], valor (i)) : 0.0f; }
    std::string Base::textParametre (int i, float v01) const { return textValor (i, deNormal (info (i), v01)); }

    std::string Base::textValor (int p, float v) const
    {
        const auto& i = info (p);
        if (i.opcions != nullptr)
        {
            const auto o = opcions (i);
            return o[(size_t) std::clamp ((int) std::lround (v - i.mn), 0, (int) o.size() - 1)];
        }
        if (i.escala == Log && i.mx > 1000.0f) return v < 1000.0f ? num (v, 0) + " Hz" : num (v / 1000.0, v < 10000.0f ? 2 : 1) + " kHz";
        return num (v * 100.0, 0) + " %";
    }

    void Base::posa (int p, float v) noexcept
    {
        if (p < 0 || p >= numP) return;
        const auto& i = taula[p];
        v = std::clamp (v, i.mn, i.mx);
        if (i.escala == Enter) v = std::round (v);
        par[(size_t) p].store (v, std::memory_order_relaxed);
    }

    std::string Base::estatSo() const
    {
        std::string s = prefix + "=1";
        char b[64];
        for (int i = 0; i < numP; ++i)
        {
            std::snprintf (b, sizeof b, ";%s=%.6g", taula[i].clau, (double) valor (i));
            s += b;
        }
        return s;
    }

    std::string Base::estat() const
    {
        auto s = estatSo();
        std::string cc;
        for (int c = 0; c < 128; ++c)
            if (const int p = paramDeCC (c); p >= 0)
                cc += (cc.empty() ? "" : ",") + std::to_string (c) + ">" + taula[p].clau;
        s += ";cc=" + cc;
        if (! nomPreset.empty())
        {
            std::string n = nomPreset;
            for (auto& c : n) if (c == ';' || c == '=') c = ',';
            s += ";pnom=" + n;
        }
        return s;
    }

    void Base::posaEstat (const std::string& s)
    {
        if (! esEstatMeu (s)) return;
        std::vector<float> v ((size_t) numP);
        for (int i = 0; i < numP; ++i) v[(size_t) i] = taula[i].def;
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
                            for (int i = 0; i < numP; ++i)
                                if (cl == taula[i].clau && n >= 0 && n < 128 && ! ccReservat (n)) cc[n] = i;
                        }
                        x = y + 1;
                    }
                }
                else
                    for (int i = 0; i < numP; ++i)
                        if (clau == taula[i].clau) v[(size_t) i] = (float) std::strtod (val.c_str(), nullptr);
            }
            a = b + 1;
        }
        for (int i = 0; i < numP; ++i) posa (i, v[(size_t) i]);
        nomPreset = nom;
        if (ambCC)
            for (int n = 0; n < 128; ++n) ccMapa[(size_t) n].store (cc[n]);
    }

    void Base::assignaCC (int cc, int param) noexcept
    {
        if (cc < 0 || cc >= 128 || ccReservat (cc) || param < 0 || param >= numP) return;
        treuCC (param);
        ccMapa[(size_t) cc].store (param);
    }
    void Base::treuCC (int param) noexcept { for (auto& c : ccMapa) if (c.load() == param) c.store (-1); }
    int Base::ccDeParam (int param) const noexcept
    {
        for (int c = 0; c < 128; ++c) if (ccMapa[(size_t) c].load() == param) return c;
        return -1;
    }

    void Base::copiaOna (float* d, int n) const noexcept
    {
        n = std::min (n, midaOna);
        const int pos = posOna.load (std::memory_order_acquire);
        for (int i = 0; i < n; ++i)
            d[i] = ona[(size_t) ((pos - n + i + midaOna * 2) % midaOna)].load (std::memory_order_relaxed);
    }

    void Base::marcaNota (int n, bool on) noexcept
    {
        if (n < 0 || n > 127) return;
        auto& w = notesOn[(size_t) (n >> 6)];
        const std::uint64_t bit = 1ull << (n & 63);
        if (on) w.fetch_or (bit, std::memory_order_relaxed); else w.fetch_and (~bit, std::memory_order_relaxed);
    }

    void Base::prepara (double f, int)
    {
        fs = f > 1000.0 ? f : 48000.0;
        nHeld = 0;
        arpEnMarxa = false;
        nSonants = 0;
        preparaSo (fs);
    }

    bool Base::fentSo() const noexcept { return veusSonen() || (arpActiu() && nHeld > 0); }

    void Base::apagaTot() noexcept
    {
        nHeld = 0;
        nSonants = 0;
        arpEnMarxa = false;
        pasArpV.store (-1);
        pedal = false;
        for (auto& s : sostingudes) s = false;
        for (auto& n : notesOn) n.store (0);
        apagaVeus();
    }

    // ---- entrada de notes (reals) ----
    void Base::entraNota (int nota, int vel) noexcept
    {
        marcaNota (nota, true);
        sostingudes[nota] = false;
        for (int i = 0; i < nHeld; ++i) if (held[i].nota == nota) { std::memmove (held + i, held + i + 1, sizeof (Tecla) * (size_t) (nHeld - i - 1)); --nHeld; break; }
        if (nHeld < 32) held[nHeld++] = { nota, vel, ++comptaOrdre };
        if (arpActiu())
        {
            velArp = vel;
            if (! arpEnMarxa) { arpEnMarxa = true; pas = 0; finsPas = 0.0; }
            return;
        }
        notaOn (nota, vel);
    }

    void Base::surtNota (int nota) noexcept
    {
        marcaNota (nota, false);
        if (pedal) { sostingudes[nota] = true; return; }
        for (int i = 0; i < nHeld; ++i) if (held[i].nota == nota) { std::memmove (held + i, held + i + 1, sizeof (Tecla) * (size_t) (nHeld - i - 1)); --nHeld; break; }
        if (arpEnMarxa)
        {
            if (nHeld == 0) arpAtura();
            return;
        }
        notaOff (nota);
    }

    void Base::controla (int cc, int val) noexcept
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
                    if (sostingudes[n]) { sostingudes[n] = false; surtNota (n); }
            return;
        }
        if (cc == 120 || cc == 123) { apagaTot(); return; }
        const int p = ccMapa[(size_t) cc].load (std::memory_order_relaxed);
        if (p >= 0) posa (p, deNormal (taula[p], val / 127.0f));
    }

    // ---- arpegiador ----
    double Base::mostresPas() const noexcept
    {
        const double t = std::max (20.0, tempo().load (std::memory_order_relaxed));
        double len = fs * 60.0 / t * negresArp ((int) valor (ia.ritme));
        if (ia.swing >= 0)
        {
            const double sw = valor (ia.swing);
            len *= (pas % 2 == 0) ? 1.0 + sw : 1.0 - sw;
        }
        return std::max (16.0, len);
    }

    void Base::arpOffs() noexcept
    {
        for (int i = 0; i < nSonants; ++i) notaOff (sonants[i]);
        nSonants = 0;
        finsOff = -1.0;
    }

    void Base::arpAtura() noexcept
    {
        arpOffs();
        arpEnMarxa = false;
        pasArpV.store (-1, std::memory_order_relaxed);
    }

    void Base::arpPas() noexcept
    {
        if (nHeld == 0) { arpAtura(); return; }
        const int mode = (int) valor (ia.mode);
        const int octs = ia.octaves >= 0 ? std::clamp ((int) valor (ia.octaves), 1, 4) : 1;
        // les notes premudes, per to (i per ordre per al mode «Ordre»)
        int per[32], n = nHeld;
        for (int i = 0; i < n; ++i) per[i] = held[i].nota;
        if (mode != 5) std::sort (per, per + n);
        int noves[32]; int nn = 0;
        bool accent = false, lligat = false;
        const int total = n * octs;
        auto llista = [&] (int k) { return per[k % n] + 12 * (k / n); };
        switch (mode)
        {
            case 1: case 5: noves[nn++] = llista (pas % total); break;
            case 2: noves[nn++] = llista (total - 1 - pas % total); break;
            case 3:
            {
                const int cicle = total > 1 ? 2 * total - 2 : 1;
                int k = pas % cicle;
                if (k >= total) k = cicle - k;
                noves[nn++] = llista (k);
                break;
            }
            case 4: noves[nn++] = llista (std::clamp ((int) ((aleat() * 0.5f + 0.5f) * total), 0, total - 1)); break;
            case 6: { const int o = (pas % octs) * 12; for (int i = 0; i < n && nn < 32; ++i) noves[nn++] = per[i] + o; break; }
            default:
            {
                const auto& pts = patrons();
                const auto& pt = pts[(size_t) std::clamp (ia.patro >= 0 ? (int) valor (ia.patro) : 0, 0, (int) pts.size() - 1)];
                const int len = (int) pt.passos.size();
                const auto& pp = pt.passos[(size_t) (pas % len)];
                const int o = ((pas / len) % octs) * 12;
                if (! pp.silenci) noves[nn++] = per[0] + pp.to + o;
                accent = pp.accent;
                lligat = pp.lligat;
                break;
            }
        }
        const double len = mostresPas();
        const float gate = ia.gate >= 0 ? valor (ia.gate) : 0.5f;
        const bool lliga = lligat || gate >= 0.99f;
        // primer les noves (en legato llisquen) i després s'apaguen les velles
        int velles[32]; const int nv = nSonants;
        for (int i = 0; i < nv; ++i) velles[i] = sonants[i];
        nSonants = 0;
        const int vel = std::clamp (accent ? velArp + 30 : velArp, 1, 127);
        for (int i = 0; i < nn; ++i)
        {
            const int x = std::clamp (noves[i], 0, 127);
            bool jaSona = false;
            for (int k = 0; k < nv; ++k) if (velles[k] == x) { jaSona = true; velles[k] = -1; }
            if (jaSona) notaOff (x);   // la mateixa nota: s'ha de tornar a atacar
            notaOn (x, vel);
            sonants[nSonants++] = x;
        }
        for (int k = 0; k < nv; ++k) if (velles[k] >= 0) notaOff (velles[k]);
        finsOff = lliga ? -1.0 : std::max (8.0, len * gate);
        finsPas = len;
        pasArpV.store (pas, std::memory_order_relaxed);
        ++pas;
    }

    // ---- so ----
    void Base::processa (const EventInstrument* events, int numEvents, float* L, float* R, int n) noexcept
    {
        const auto t0 = std::chrono::steady_clock::now();
        if (arpEnMarxa && ! arpActiu())
        {
            // l'han apagat mentre sonava: les tecles que encara es premen sonen normals
            arpAtura();
            for (int i = 0; i < nHeld; ++i) notaOn (held[i].nota, held[i].vel);
        }
        int fet = 0, e = 0;
        float pic = nivellV.load (std::memory_order_relaxed) * 0.97f;
        int pos = posOna.load (std::memory_order_relaxed);
        while (fet < n)
        {
            while (e < numEvents && events[e].offset <= fet)
            {
                const auto& ev = events[e++];
                const int tipus = ev.estat & 0xF0;
                if (tipus == 0x90 && ev.dada2 > 0) entraNota (ev.dada1 & 127, ev.dada2);
                else if (tipus == 0x80 || (tipus == 0x90 && ev.dada2 == 0)) surtNota (ev.dada1 & 127);
                else if (tipus == 0xB0) controla (ev.dada1 & 127, ev.dada2);
                else if (tipus == 0xE0)
                {
                    bendCru = (float) (((int) ev.dada2 << 7 | ev.dada1) - 8192) / 8192.0f;
                    bendV.store (bendCru, std::memory_order_relaxed);
                }
                else if (tipus == 0xD0 || tipus == 0xA0)
                {
                    pressioAra = (tipus == 0xD0 ? ev.dada1 : ev.dada2) / 127.0f;
                    pressioV.store (pressioAra, std::memory_order_relaxed);
                }
            }
            if (arpEnMarxa)
            {
                if (finsOff >= 0.0 && finsOff < 1.0) arpOffs();
                if (finsPas < 1.0) arpPas();
            }
            int fins = e < numEvents ? std::min (n, std::max (fet + 1, events[e].offset)) : n;
            fins = std::min (fins, fet + tros);
            if (arpEnMarxa)
            {
                fins = std::min (fins, fet + std::max (1, (int) std::ceil (finsPas)));
                if (finsOff >= 0.0) fins = std::min (fins, fet + std::max (1, (int) std::ceil (finsOff)));
            }
            const int m = fins - fet;
            renderitza (L + fet, R + fet, m);
            if (arpEnMarxa) { finsPas -= m; if (finsOff >= 0.0) finsOff -= m; }
            for (int i = fet; i < fins; ++i)
            {
                pic = std::max (pic, std::max (std::abs (L[i]), std::abs (R[i])));
                ona[(size_t) pos].store (0.5f * (L[i] + R[i]), std::memory_order_relaxed);
                pos = (pos + 1) % midaOna;
            }
            fet = fins;
        }
        while (e < numEvents) { const auto& ev = events[e++]; if ((ev.estat & 0xF0) == 0x80 || ((ev.estat & 0xF0) == 0x90 && ev.dada2 == 0)) surtNota (ev.dada1 & 127); }
        posOna.store (pos, std::memory_order_release);
        nivellV.store (pic, std::memory_order_relaxed);
        actives.store (numVeusActives(), std::memory_order_relaxed);
        const double usat = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        const float carrega = (float) (usat / std::max (1e-6, n / fs));
        const float ant = cpu.load (std::memory_order_relaxed);
        cpu.store (ant + (carrega - ant) * 0.05f, std::memory_order_relaxed);
    }
}
