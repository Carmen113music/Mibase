#include "mibase/Midi.h"
#include "mibase/Edicio.h"

#include <algorithm>
#include "mibase/GeneralMidi.h"
#include <cmath>
#include <numeric>

namespace mibase::midi
{
    double segonsPerTick (const Projecte& p) { return edicio::segonsPerTemps (p) / ticksPerNegra; }
    // 0.28: amb el mapa de tempo (tick absolut de la cançó ↔ segons).
    double segons (const Projecte& p, double t)
    {
        if (! edicio::teCanvisTempo (p)) return t * segonsPerTick (p);
        const auto v = edicio::mapaTempo (p);
        size_t i = 0;
        while (i + 1 < v.size() && v[i + 1].tick <= t + 1e-9) ++i;
        return v[i].segons + (t - v[i].tick) * 60.0 / v[i].tempo / ticksPerNegra;
    }
    double ticks (const Projecte& p, double s)
    {
        if (! edicio::teCanvisTempo (p)) return s / segonsPerTick (p);
        const auto v = edicio::mapaTempo (p);
        size_t i = 0;
        while (i + 1 < v.size() && v[i + 1].segons <= s + 1e-9) ++i;
        return v[i].tick + (s - v[i].segons) * v[i].tempo / 60.0 * ticksPerNegra;
    }
    long long ticksPerCompas (const Projecte& p) { return (long long) ticksPerNegra * std::max (1, p.compasNum); }
    double segonsAbs (const Projecte& p, const Clip& c, double tick) { return c.pos + segons (p, tick); }
    double tickDe (const Projecte& p, const Clip& c, double s) { return ticks (p, s - c.pos); }

    std::string nomNota (int to)
    {
        static const char* noms[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        to = std::clamp (to, 0, 127);
        return std::string (noms[to % 12]) + std::to_string (to / 12 - 1);
    }

    bool esNegra (int to)
    {
        const int n = ((to % 12) + 12) % 12;
        return n == 1 || n == 3 || n == 6 || n == 8 || n == 10;
    }

    const std::vector<ValorGraella>& valorsGraella()
    {
        static const std::vector<ValorGraella> v {
            { "Lliure", 0 },
            { "1/1", ticksPerNegra * 4 },
            { "1/2", ticksPerNegra * 2 },
            { "1/4", ticksPerNegra },
            { "1/8", ticksPerNegra / 2 },
            { "1/16", ticksPerNegra / 4 },
            { "1/32", ticksPerNegra / 8 },
            { "1/4 T", ticksPerNegra * 2 / 3 },
            { "1/8 T", ticksPerNegra / 3 },
            { "1/16 T", ticksPerNegra / 6 },
        };
        return v;
    }

    long long enganxa (long long t, long long pas)
    {
        if (pas <= 0) return t;
        return (long long) std::llround ((double) t / (double) pas) * pas;
    }

    long long enganxaAvall (long long t, long long pas)
    {
        if (pas <= 0) return t;
        return (long long) std::floor ((double) t / (double) pas) * pas;
    }

    //==========================================================================
    void ordena (std::vector<Nota>& notes, std::vector<int>& triades)
    {
        std::vector<int> ordre (notes.size());
        std::iota (ordre.begin(), ordre.end(), 0);
        std::stable_sort (ordre.begin(), ordre.end(), [&] (int a, int b)
        {
            const auto& x = notes[(size_t) a];
            const auto& y = notes[(size_t) b];
            return x.ini != y.ini ? x.ini < y.ini : x.to < y.to;
        });
        std::vector<int> nouIndex (notes.size());
        std::vector<Nota> noves;
        noves.reserve (notes.size());
        for (size_t k = 0; k < ordre.size(); ++k)
        {
            nouIndex[(size_t) ordre[k]] = (int) k;
            noves.push_back (notes[(size_t) ordre[k]]);
        }
        notes = std::move (noves);
        std::vector<int> t;
        for (int i : triades)
            if (i >= 0 && i < (int) nouIndex.size())
                t.push_back (nouIndex[(size_t) i]);
        std::sort (t.begin(), t.end());
        t.erase (std::unique (t.begin(), t.end()), t.end());
        triades = std::move (t);
    }

    static Nota neta (Nota n)
    {
        n.to = std::clamp (n.to, 0, 127);
        n.vel = std::clamp (n.vel, 1, 127);
        n.canal = std::clamp (n.canal, 0, 15);
        n.dur = std::max (1LL, n.dur);
        return n;
    }

    static std::vector<int> valids (const Clip& c, const std::vector<int>& idx)
    {
        std::vector<int> v;
        for (int i : idx)
            if (i >= 0 && i < (int) c.notes.size() && std::find (v.begin(), v.end(), i) == v.end())
                v.push_back (i);
        return v;
    }

    int afegeix (Clip& c, const Nota& n)
    {
        c.notes.push_back (neta (n));
        std::vector<int> t { (int) c.notes.size() - 1 };
        ordena (c.notes, t);
        return t.empty() ? -1 : t[0];
    }

    void esborra (Clip& c, std::vector<int> idx)
    {
        idx = valids (c, idx);
        std::sort (idx.rbegin(), idx.rend());
        // 0.31: amb les notes també se'n van els controladors (expressió, modulació, pitch bend, aftertouch…) del seu tros,
        // si cap altra nota que es queda no hi sona. Sense cap nota, no en queda cap (els canvis de programa sí).
        std::vector<Nota> treure;
        for (int i : idx) treure.push_back (c.notes[(size_t) i]);
        for (int i : idx)
            c.notes.erase (c.notes.begin() + i);
        auto dins = [] (const Nota& n, long long t) { return t >= n.ini && t < n.ini + std::max (1LL, n.dur); };
        std::erase_if (c.events, [&] (const EventMidi& e)
        {
            if (e.tipus == 0xC0) return false;
            if (c.notes.empty()) return true;
            bool delTros = false;
            for (auto& n : treure)
                if (dins (n, e.tick) && (e.tipus != 0xA0 || e.num == n.to)) { delTros = true; break; }
            if (! delTros) return false;
            if (e.tipus == 0xA0)   // aftertouch d'una nota concreta
                return std::none_of (c.notes.begin(), c.notes.end(), [&] (const Nota& n) { return n.to == e.num && dins (n, e.tick); });
            return std::none_of (c.notes.begin(), c.notes.end(), [&] (const Nota& n) { return dins (n, e.tick); });
        });
    }

    void mou (Clip& c, std::vector<int>& idx, long long dT, int dTo, long long minTick)
    {
        idx = valids (c, idx);
        if (idx.empty()) return;
        int toMin = 127, toMax = 0;
        long long iniMin = 0;
        bool primer = true;
        for (int i : idx)
        {
            const auto& n = c.notes[(size_t) i];
            toMin = std::min (toMin, n.to);
            toMax = std::max (toMax, n.to);
            iniMin = primer ? n.ini : std::min (iniMin, n.ini);
            primer = false;
        }
        dTo = std::clamp (dTo, -toMin, 127 - toMax);
        if (iniMin + dT < minTick) dT = std::min (0LL, minTick - iniMin);
        for (int i : idx)
        {
            auto& n = c.notes[(size_t) i];
            n.ini += dT;
            n.to += dTo;
        }
        ordena (c.notes, idx);
    }

    void canviaDurada (Clip& c, std::vector<int>& idx, long long d, bool perInici, long long durMinima)
    {
        durMinima = std::max (1LL, durMinima);
        idx = valids (c, idx);
        for (int i : idx)
        {
            auto& n = c.notes[(size_t) i];
            if (perInici)
            {
                const long long fi = n.ini + n.dur;
                long long ini = std::min (n.ini + d, fi - durMinima);
                ini = std::max (0LL, ini);
                n.ini = ini;
                n.dur = fi - ini;
            }
            else
                n.dur = std::max (durMinima, n.dur + d);
        }
        if (perInici) ordena (c.notes, idx);
    }

    void fixaDurada (Clip& c, const std::vector<int>& idx, long long dur)
    {
        for (int i : valids (c, idx))
            c.notes[(size_t) i].dur = std::max (1LL, dur);
    }

    void quantitza (Clip& c, std::vector<int>& idx, long long pas, double forca, bool tambeFinal)
    {
        idx = valids (c, idx);
        if (pas <= 0 || idx.empty()) return;
        forca = std::clamp (forca, 0.0, 1.0);
        for (int i : idx)
        {
            auto& n = c.notes[(size_t) i];
            const long long fi = n.ini + n.dur;
            const long long ini = n.ini + (long long) std::llround ((double) (enganxa (n.ini, pas) - n.ini) * forca);
            long long nouFi = fi + (ini - n.ini);   // si no es quantitza el final, la durada es manté
            if (tambeFinal)
            {
                nouFi = fi + (long long) std::llround ((double) (enganxa (fi, pas) - fi) * forca);
                if (nouFi <= ini) nouFi = ini + pas;
            }
            n.ini = std::max (0LL, ini);
            n.dur = std::max (1LL, nouFi - n.ini);
        }
        ordena (c.notes, idx);
    }

    void transposa (Clip& c, std::vector<int>& idx, int semitons) { mou (c, idx, 0, semitons, -(1LL << 50)); }

    void velocitat (Clip& c, const std::vector<int>& idx, int vel)
    {
        for (int i : valids (c, idx))
            c.notes[(size_t) i].vel = std::clamp (vel, 1, 127);
    }

    void sumaVelocitat (Clip& c, const std::vector<int>& idx, int d)
    {
        for (int i : valids (c, idx))
            c.notes[(size_t) i].vel = std::clamp (c.notes[(size_t) i].vel + d, 1, 127);
    }

    std::vector<int> duplica (Clip& c, const std::vector<int>& idx0, long long pas)
    {
        auto idx = valids (c, idx0);
        if (idx.empty()) return {};
        long long a = c.notes[(size_t) idx[0]].ini, b = a;
        for (int i : idx)
        {
            a = std::min (a, c.notes[(size_t) i].ini);
            b = std::max (b, c.notes[(size_t) i].ini + c.notes[(size_t) i].dur);
        }
        long long salt = b - a;
        if (pas > 0) salt = std::max (pas, (long long) std::ceil ((double) salt / (double) pas - 1e-9) * pas);
        std::vector<Nota> copies;
        for (int i : idx)
        {
            auto n = c.notes[(size_t) i];
            n.ini += salt;
            copies.push_back (n);
        }
        std::vector<int> noves;
        for (auto& n : copies)
        {
            c.notes.push_back (n);
            noves.push_back ((int) c.notes.size() - 1);
        }
        ordena (c.notes, noves);
        return noves;
    }

    RetallNotes copia (const Clip& c, const std::vector<int>& idx0)
    {
        RetallNotes r;
        auto idx = valids (c, idx0);
        if (idx.empty()) return r;
        long long a = c.notes[(size_t) idx[0]].ini, b = a;
        for (int i : idx)
        {
            a = std::min (a, c.notes[(size_t) i].ini);
            b = std::max (b, c.notes[(size_t) i].ini + c.notes[(size_t) i].dur);
        }
        for (int i : idx)
        {
            auto n = c.notes[(size_t) i];
            n.ini -= a;
            r.notes.push_back (n);
        }
        r.durada = b - a;
        return r;
    }

    std::vector<int> enganxa (Clip& c, const RetallNotes& r, long long tick)
    {
        std::vector<int> noves;
        for (auto n : r.notes)
        {
            n.ini += tick;
            c.notes.push_back (neta (n));
            noves.push_back ((int) c.notes.size() - 1);
        }
        ordena (c.notes, noves);
        return noves;
    }

    std::vector<int> notesDins (const Clip& c, long long t0, long long t1, int toMin, int toMax)
    {
        std::vector<int> v;
        if (t1 < t0) std::swap (t0, t1);
        if (toMax < toMin) std::swap (toMin, toMax);
        for (int i = 0; i < (int) c.notes.size(); ++i)
        {
            const auto& n = c.notes[(size_t) i];
            if (n.to < toMin || n.to > toMax) continue;
            if (n.ini + n.dur > t0 && n.ini < t1) v.push_back (i);   // la toca
        }
        return v;
    }

    std::vector<bool> trepitjades (const std::vector<Nota>& notes)
    {
        std::vector<bool> r (notes.size(), false);
        // Les notes estan ordenades per inici: per a cada to, l'última que encara sona.
        long long fins[128];
        int qui[128];
        for (int k = 0; k < 128; ++k) { fins[k] = -1; qui[k] = -1; }
        for (size_t i = 0; i < notes.size(); ++i)
        {
            const auto& n = notes[i];
            const int to = std::clamp (n.to, 0, 127);
            if (qui[to] >= 0 && n.ini < fins[to])
            {
                r[i] = true;
                r[(size_t) qui[to]] = true;
            }
            if (n.ini + n.dur > fins[to]) { fins[to] = n.ini + n.dur; qui[to] = (int) i; }
        }
        return r;
    }

    long long finalNotes (const Clip& c)
    {
        long long f = 0;
        for (auto& n : c.notes) f = std::max (f, n.ini + n.dur);
        return f;
    }

    //==========================================================================
    Clip clipNou (double de, double a)
    {
        Clip c;
        c.midi = true;
        c.pos = de;
        c.inici = 0.0;
        c.fi = std::max (edicio::longitudMinima, a - de);
        return c;
    }

    void encabeix (const Projecte& p, Clip& c)
    {
        if (! c.midi || c.notes.empty()) return;
        long long a = c.notes.front().ini, b = 0;
        for (auto& n : c.notes) { a = std::min (a, n.ini); b = std::max (b, n.ini + n.dur); }
        c.fi = std::max (c.fi, segons (p, (double) b));
        c.inici = std::min (c.inici, segons (p, (double) a));
    }

    Clip ajunta (const Projecte& p, const Clip& a, const Clip& b)
    {
        Clip r = a;
        r.notes.clear();
        r.events.clear();
        const double ini = std::min (a.iniciVisible(), b.iniciVisible());
        const double fi = std::max (a.fiVisible(), b.fiVisible());
        r.inici = ini - a.pos;
        r.fi = fi - a.pos;
        r.fadeIn = r.fadeOut = 0.0;

        auto afegeixDe = [&] (const Clip& c)
        {
            const double t0 = ticks (p, c.inici), t1 = ticks (p, c.fi);
            const long long salt = (long long) std::llround (ticks (p, c.pos - a.pos));
            for (auto n : c.notes)
            {
                if ((double) n.ini < t0 - 1e-6 || (double) n.ini >= t1 - 1e-6) continue;
                n.dur = std::max (1LL, std::min (n.dur, (long long) std::floor (t1 - (double) n.ini)));
                n.ini += salt;
                r.notes.push_back (n);
            }
            for (auto e : c.events)
            {
                if ((double) e.tick < t0 - 1e-6 || (double) e.tick >= t1 - 1e-6) continue;
                e.tick += salt;
                r.events.push_back (e);
            }
        };
        afegeixDe (a);
        afegeixDe (b);
        std::vector<int> cap;
        ordena (r.notes, cap);
        std::stable_sort (r.events.begin(), r.events.end(), [] (const EventMidi& x, const EventMidi& y) { return x.tick < y.tick; });
        return r;
    }

    void canviaTempo (Projecte& p, double nou)
    {
        nou = std::clamp (nou, 20.0, 400.0);
        const double f = std::max (20.0, p.tempo) / nou;
        for (auto& t : p.pistes)
            for (auto& c : t.clips)
                if (c.midi)
                {
                    c.pos *= f;
                    c.inici *= f;
                    c.fi *= f;
                }
        p.tempo = nou;
    }

    bool compatible (const Pista& t, const Clip& c) { return ! esBus (t) && c.midi == esMidi (t); }   // 0.34: als busos no hi van clips

    //==========================================================================
    const std::vector<SoIntern>& sonsInterns()
    {
        static const std::vector<SoIntern> v {
            { "piano",  "Piano" },
            { "epiano", "Piano elèctric" },
            { "orgue",  "Orgue" },
            { "pad",    "Pad" },
            { "baix",   "Baix" },
            { "pluck",  "Pinçat" },
            { "lead",   "Solista" },
            { "cordes", "Cordes" },
        };
        return v;
    }

    int soIntern (const std::string& instrument)
    {
        const std::string prefix = "intern:";
        if (instrument.rfind (prefix, 0) != 0) return -1;
        const auto id = instrument.substr (prefix.size());
        const auto& v = sonsInterns();
        for (size_t i = 0; i < v.size(); ++i)
            if (id == v[i].id) return (int) i;
        return 0;
    }

    std::string nomInstrument (const std::string& instrument)
    {
        const int i = soIntern (instrument);
        if (i >= 0) return sonsInterns()[(size_t) i].nom;
        if (instrument.empty()) return "Cap";
        if (instrument == instrumentSampler) return "Samplejador";
        if (const int p = gm::programa (instrument); p >= 0) return std::string ("GM · ") + gm::nomPrograma (p);
        return instrument;
    }
}
