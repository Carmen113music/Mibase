#include "mibase/Harmonia.h"
#include "mibase/Midi.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <set>

namespace mibase::harmonia
{
    namespace
    {
        int pc (int n) { return ((n % 12) + 12) % 12; }

        const char* nomsSostinguts[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const char* nomsBemolls[]    = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

        bool teTerceraMajor (const Escala& e) { return std::find (e.intervals.begin(), e.intervals.end(), 4) != e.intervals.end(); }

        bool ambBemolls (const Tonalitat& t)
        {
            const auto& e = escales()[(size_t) std::clamp (t.escala, 0, (int) escales().size() - 1)];
            const int major = teTerceraMajor (e) ? pc (t.to) : pc (t.to + 3);   // la major relativa
            return major == 5 || major == 10 || major == 3 || major == 8 || major == 1;
        }

        const Escala& escala (const Tonalitat& t) { return escales()[(size_t) std::clamp (t.escala, 0, (int) escales().size() - 1)]; }
    }

    //==========================================================================
    const std::vector<Escala>& escales()
    {
        static const std::vector<Escala> v {
            { "major",          "Major",              { 0, 2, 4, 5, 7, 9, 11 } },
            { "menor",          "Menor natural",      { 0, 2, 3, 5, 7, 8, 10 } },
            { "menorHarmonica", "Menor harmònica",    { 0, 2, 3, 5, 7, 8, 11 } },
            { "menorMelodica",  "Menor melòdica",     { 0, 2, 3, 5, 7, 9, 11 } },
            { "dorica",         "Dòrica",             { 0, 2, 3, 5, 7, 9, 10 } },
            { "frigia",         "Frígia",             { 0, 1, 3, 5, 7, 8, 10 } },
            { "lidia",          "Lídia",              { 0, 2, 4, 6, 7, 9, 11 } },
            { "mixolidia",      "Mixolídia",          { 0, 2, 4, 5, 7, 9, 10 } },
            { "eolica",         "Eòlica",             { 0, 2, 3, 5, 7, 8, 10 } },
            { "locria",         "Lòcria",             { 0, 1, 3, 5, 6, 8, 10 } },
            { "pentaMajor",     "Pentatònica major",  { 0, 2, 4, 7, 9 } },
            { "pentaMenor",     "Pentatònica menor",  { 0, 3, 5, 7, 10 } },
            { "blues",          "Blues",              { 0, 3, 5, 6, 7, 10 } },
        };
        return v;
    }

    int escalaPerId (const std::string& id)
    {
        const auto& v = escales();
        for (size_t i = 0; i < v.size(); ++i) if (id == v[i].id) return (int) i;
        return -1;
    }

    std::string nomTo (int n) { return nomsSostinguts[pc (n)]; }
    std::string nomTo (int n, const Tonalitat& t) { return ambBemolls (t) ? nomsBemolls[pc (n)] : nomsSostinguts[pc (n)]; }
    std::string nomTonalitat (const Tonalitat& t) { return nomTo (t.to, t) + " " + escala (t).nom; }

    bool aEscala (int nota, const Tonalitat& t)
    {
        const int r = pc (nota - t.to);
        const auto& iv = escala (t).intervals;
        return std::find (iv.begin(), iv.end(), r) != iv.end();
    }

    std::vector<int> notesEscala (const Tonalitat& t)
    {
        std::vector<int> v;
        for (int i : escala (t).intervals) v.push_back (pc (t.to + i));
        return v;
    }

    int ajusta (int nota, const Tonalitat& t, int direccio)
    {
        nota = std::clamp (nota, 0, 127);
        if (aEscala (nota, t)) return nota;
        for (int d = 1; d < 12; ++d)
        {
            const bool amunt = nota + d <= 127 && aEscala (nota + d, t);
            const bool avall = nota - d >= 0 && aEscala (nota - d, t);
            if (amunt && avall) return direccio > 0 ? nota + d : nota - d;
            if (avall) return nota - d;
            if (amunt) return nota + d;
        }
        return nota;
    }

    int grau (int nota, const Tonalitat& t, int passos)
    {
        int n = ajusta (nota, t, passos);
        const int pas = passos > 0 ? 1 : -1;
        for (int k = 0; k < std::abs (passos); ++k)
        {
            int m = n + pas;
            while (m >= 0 && m <= 127 && ! aEscala (m, t)) m += pas;
            if (m < 0 || m > 127) break;
            n = m;
        }
        return n;
    }

    //==========================================================================
    const std::vector<TipusAcord>& tipusAcords()
    {
        static const std::vector<TipusAcord> v {
            { "maj",  "",     "Major",            { 0, 4, 7 } },
            { "min",  "m",    "Menor",            { 0, 3, 7 } },
            { "sus2", "sus2", "Sus2",             { 0, 2, 7 } },
            { "sus4", "sus4", "Sus4",             { 0, 5, 7 } },
            { "dim",  "dim",  "Disminuït",        { 0, 3, 6 } },
            { "aug",  "aug",  "Augmentat",        { 0, 4, 8 } },
            { "maj7", "maj7", "Major 7a",         { 0, 4, 7, 11 } },
            { "m7",   "m7",   "Menor 7a",         { 0, 3, 7, 10 } },
            { "7",    "7",    "7a de dominant",   { 0, 4, 7, 10 } },
            { "m7b5", "m7b5", "Semidisminuït",    { 0, 3, 6, 10 } },
            { "9",    "9",    "9a",               { 0, 4, 7, 10, 14 } },
            { "maj9", "maj9", "Major 9a",         { 0, 4, 7, 11, 14 } },
            { "m9",   "m9",   "Menor 9a",         { 0, 3, 7, 10, 14 } },
            { "11",   "11",   "11a",              { 0, 7, 10, 14, 17 } },
            { "13",   "13",   "13a",              { 0, 4, 10, 14, 21 } },
            { "add9", "add9", "Add9",             { 0, 4, 7, 14 } },
            { "6",    "6",    "6a",               { 0, 4, 7, 9 } },
            { "m6",   "m6",   "Menor 6a",         { 0, 3, 7, 9 } },
        };
        return v;
    }

    int tipusPerSufix (const std::string& s)
    {
        const auto& v = tipusAcords();
        for (size_t i = 0; i < v.size(); ++i) if (s == v[i].sufix) return (int) i;
        return -1;
    }

    std::vector<int> acord (int fonamental, int tipus, int inversio, Voicing voicing, int octaves, bool baix)
    {
        const auto& tp = tipusAcords()[(size_t) std::clamp (tipus, 0, (int) tipusAcords().size() - 1)];
        std::vector<int> n;
        for (int i : tp.intervals) n.push_back (fonamental + i);
        std::sort (n.begin(), n.end());
        inversio = std::clamp (inversio, 0, (int) n.size() - 1);
        for (int k = 0; k < inversio; ++k)
        {
            n.front() += 12;
            std::sort (n.begin(), n.end());
        }
        if (voicing == Voicing::Obert && n.size() >= 3)
        {
            n[n.size() - 2] -= 12;   // drop 2
            std::sort (n.begin(), n.end());
        }
        if (octaves >= 2)
        {
            const auto base = n;
            for (int x : base) n.push_back (x + 12);
        }
        if (baix)
        {
            int b = fonamental;
            while (b >= n.front()) b -= 12;
            n.push_back (b);
        }
        std::vector<int> r;
        for (int x : n) if (x >= 0 && x <= 127) r.push_back (x);
        std::sort (r.begin(), r.end());
        r.erase (std::unique (r.begin(), r.end()), r.end());
        return r;
    }

    std::string nomAcord (int pcF, int tipus, int pcBaix)
    {
        const auto& tp = tipusAcords()[(size_t) std::clamp (tipus, 0, (int) tipusAcords().size() - 1)];
        std::string s = nomTo (pcF) + tp.sufix;
        if (pcBaix >= 0 && pc (pcBaix) != pc (pcF)) s += "/" + nomTo (pcBaix);
        return s;
    }

    std::vector<AcordGrau> acordsDeTonalitat (const Tonalitat& t, bool setenes)
    {
        std::vector<int> iv = escala (t).intervals;
        if (iv.size() != 7) iv = teTerceraMajor (escala (t)) ? escales()[0].intervals : escales()[1].intervals;
        static const char* romans[] = { "I", "II", "III", "IV", "V", "VI", "VII" };
        std::vector<AcordGrau> r;
        for (int g = 0; g < 7; ++g)
        {
            const int arrel = iv[(size_t) g];
            auto rel = [&] (int k) { return pc (iv[(size_t) ((g + k) % 7)] - arrel); };
            std::vector<int> conj { 0, rel (2), rel (4) };
            if (setenes) conj.push_back (rel (6));
            std::sort (conj.begin(), conj.end());
            int tipus = -1;
            for (int k = 0; k < (int) tipusAcords().size() && tipus < 0; ++k)
            {
                std::vector<int> x;
                for (int i : tipusAcords()[(size_t) k].intervals) x.push_back (pc (i));
                std::sort (x.begin(), x.end());
                if (x == conj) tipus = k;
            }
            if (tipus < 0)
            {
                // Per exemple, la 7a menor-major de la menor harmònica: es queda en tríada.
                std::vector<int> tri { 0, rel (2), rel (4) };
                std::sort (tri.begin(), tri.end());
                for (int k = 0; k < 6 && tipus < 0; ++k)
                {
                    std::vector<int> x (tipusAcords()[(size_t) k].intervals);
                    if (x == tri) tipus = k;
                }
                if (tipus < 0) tipus = 0;
            }
            AcordGrau a;
            a.grau = g;
            a.pc = pc (t.to + arrel);
            a.tipus = tipus;
            const std::string id = tipusAcords()[(size_t) tipus].id;
            std::string rom = romans[g];
            const bool menor = id == "min" || id == "dim" || id == "m7" || id == "m7b5";
            if (menor) for (auto& c : rom) c = (char) std::tolower ((unsigned char) c);
            if (id == "dim") rom += "°";
            else if (id == "aug") rom += "+";
            else if (id == "7" || id == "m7") rom += "7";
            else if (id == "maj7") rom += "maj7";
            else if (id == "m7b5") rom += "ø7";
            a.roma = rom;
            a.nom = nomTo (a.pc, t) + tipusAcords()[(size_t) tipus].sufix;
            r.push_back (a);
        }
        return r;
    }

    Reconegut reconeix (const std::vector<int>& notes)
    {
        Reconegut r;
        if (notes.empty()) return r;
        std::set<int> pcs;
        int baix = notes.front();
        for (int n : notes) { pcs.insert (pc (n)); baix = std::min (baix, n); }
        if (pcs.size() < 2) return r;
        const int pcBaix = pc (baix);

        // Ordre de preferència (els més corrents primer).
        static const int ordre[] = { 0, 1, 8, 7, 6, 4, 5, 3, 2, 9, 16, 17, 15, 10, 11, 12, 13, 14 };
        int millor = -1, millorPc = 0, millorPunts = -1000000;
        for (int exacte = 1; exacte >= 0 && millor < 0; --exacte)
            for (int pos = 0; pos < (int) (sizeof (ordre) / sizeof (ordre[0])); ++pos)
                for (int arrel : pcs)
                {
                    const int k = ordre[pos];
                    std::set<int> conj;
                    for (int i : tipusAcords()[(size_t) k].intervals) conj.insert (pc (arrel + i));
                    bool val;
                    if (exacte) val = conj == pcs;
                    else
                    {
                        // Notes de menys (per exemple, sense la quinta): totes les que hi ha són de l'acord i hi ha la tercera.
                        if (k > 1) continue;
                        val = std::includes (conj.begin(), conj.end(), pcs.begin(), pcs.end())
                              && pcs.count (pc (arrel + tipusAcords()[(size_t) k].intervals[1])) > 0;
                    }
                    if (! val) continue;
                    const int punts = (arrel == pcBaix ? 100 : 0) - pos;
                    if (punts > millorPunts) { millorPunts = punts; millor = k; millorPc = arrel; }
                }
        if (millor < 0) return r;
        r.trobat = true;
        r.pc = millorPc;
        r.tipus = millor;
        r.baix = pcBaix;
        r.nom = nomAcord (millorPc, millor, pcBaix);
        return r;
    }

    //==========================================================================
    Deteccio detecta (const Projecte& p)
    {
        Deteccio d;
        std::array<double, 12> h {};
        const double spt = midi::segonsPerTick (p);
        const bool hiHaSolo = std::any_of (p.pistes.begin(), p.pistes.end(), [] (const Pista& t) { return t.solo; });
        for (const auto& t : p.pistes)
        {
            if (t.tipus != "midi" || t.mut || (hiHaSolo && ! t.solo)) continue;
            for (const auto& c : t.clips)
            {
                if (! c.midi || c.mut) continue;
                for (const auto& n : c.notes)
                {
                    const double a = c.pos + spt * (double) n.ini;
                    if (a < c.iniciVisible() - 1e-9 || a >= c.fiVisible()) continue;
                    const double dur = std::min (c.fiVisible(), a + spt * (double) n.dur) - a;
                    h[(size_t) pc (n.to)] += std::max (0.05, dur);
                    ++d.notes;
                }
            }
        }
        int diferents = 0;
        for (double x : h) if (x > 0.0) ++diferents;

        static const double major[] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        static const double menor[] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
        auto correl = [&h] (const double* perfil, int to)
        {
            double mx = 0.0, my = 0.0;
            for (int i = 0; i < 12; ++i) { mx += h[(size_t) i]; my += perfil[pc (i - to)]; }
            mx /= 12.0; my /= 12.0;
            double sxy = 0.0, sxx = 0.0, syy = 0.0;
            for (int i = 0; i < 12; ++i)
            {
                const double x = h[(size_t) i] - mx, y = perfil[pc (i - to)] - my;
                sxy += x * y; sxx += x * x; syy += y * y;
            }
            return (sxx <= 0.0 || syy <= 0.0) ? 0.0 : sxy / std::sqrt (sxx * syy);
        };
        double millor = -2.0;
        for (int to = 0; to < 12; ++to)
        {
            const double a = correl (major, to), b = correl (menor, to);
            if (a > millor) { millor = a; d.tonalitat = { to, 0 }; }
            if (b > millor) { millor = b; d.tonalitat = { to, 1 }; }
        }
        d.confianca = std::clamp (millor, 0.0, 1.0);
        d.fiable = d.notes >= 8 && diferents >= 4 && millor >= 0.5;
        return d;
    }

    Tonalitat tonalitatEfectiva (const Projecte& p)
    {
        if (! p.tonalitatAuto) return p.tonalitat;
        const auto d = detecta (p);
        return d.fiable ? d.tonalitat : p.tonalitat;
    }

    std::vector<int> notesSonant (const Projecte& p, double t)
    {
        std::vector<int> r;
        const double spt = midi::segonsPerTick (p);
        const bool hiHaSolo = std::any_of (p.pistes.begin(), p.pistes.end(), [] (const Pista& x) { return x.solo; });
        for (const auto& pista : p.pistes)
        {
            if (pista.tipus != "midi" || pista.mut || (hiHaSolo && ! pista.solo)) continue;
            for (const auto& c : pista.clips)
            {
                if (! c.midi || c.mut || t < c.iniciVisible() || t >= c.fiVisible()) continue;
                for (const auto& n : c.notes)
                {
                    const double a = c.pos + spt * (double) n.ini, b = c.pos + spt * (double) (n.ini + n.dur);
                    if (a < c.iniciVisible() - 1e-9) continue;
                    if (a <= t + 1e-9 && b > t) r.push_back (n.to);
                }
            }
        }
        std::sort (r.begin(), r.end());
        r.erase (std::unique (r.begin(), r.end()), r.end());
        return r;
    }

    //==========================================================================
    std::vector<int> enllaca (const std::vector<int>& anterior, const std::vector<int>& acordNou)
    {
        if (anterior.empty() || acordNou.empty()) return acordNou;
        auto tancat = acordNou;
        std::sort (tancat.begin(), tancat.end());
        const int n = (int) tancat.size();
        double centreAnt = 0.0;
        for (int x : anterior) centreAnt += x;
        centreAnt /= (double) anterior.size();

        auto cost = [&anterior] (const std::vector<int>& c)
        {
            double s = 0.0;
            for (int x : c)
            {
                int m = 1000;
                for (int y : anterior) m = std::min (m, std::abs (x - y));
                s += m;
            }
            for (int y : anterior)
            {
                int m = 1000;
                for (int x : c) m = std::min (m, std::abs (x - y));
                s += m;
            }
            return s;
        };

        std::vector<int> millor = tancat;
        double millorCost = 1e18;
        for (int inv = 0; inv < n; ++inv)
        {
            auto c = tancat;
            for (int k = 0; k < inv; ++k) { c.front() += 12; std::sort (c.begin(), c.end()); }
            double centre = 0.0;
            for (int x : c) centre += x;
            centre /= n;
            const int salt = (int) std::lround ((centreAnt - centre) / 12.0);
            for (int o = salt - 1; o <= salt + 1; ++o)
            {
                auto d = c;
                for (auto& x : d) x += 12 * o;
                if (d.front() < 24 || d.back() > 108) continue;
                double cc = cost (d);
                double centreD = 0.0;
                for (int x : d) centreD += x;
                cc += std::abs (centreD / n - centreAnt) * 0.01;   // a igualtat, el més a prop
                if (cc < millorCost) { millorCost = cc; millor = d; }
            }
        }
        return millor;
    }

    const std::vector<Progressio>& progressions()
    {
        static const std::vector<Progressio> v {
            { "Pop: I – V – vi – IV",                 { 0, 4, 5, 3 } },
            { "Balada: vi – IV – I – V",              { 5, 3, 0, 4 } },
            { "Anys 50: I – vi – IV – V",             { 0, 5, 3, 4 } },
            { "Rock: I – IV – V – IV",                { 0, 3, 4, 3 } },
            { "Indie: I – iii – IV – IV",             { 0, 2, 3, 3 } },
            { "Folk: I – IV – I – V",                 { 0, 3, 0, 4 } },
            { "Jazz: ii – V – I – I",                 { 1, 4, 0, 0 } },
            { "Èpica (menor): i – VI – III – VII",    { 0, 5, 2, 6 }, true },
            { "Andalusa (menor): i – VII – VI – V",   { 0, 6, 5, 4 }, true, true },
            { "Trista (menor): i – iv – v – i",       { 0, 3, 4, 0 }, true },
            { "Blues de 12 compassos",                { 0, 0, 0, 0, 3, 3, 0, 0, 4, 3, 0, 4 } },
        };
        return v;
    }

    Tonalitat relativaMajor (const Tonalitat& t)
    {
        const auto pcs = notesEscala (t);
        const int preferida = teTerceraMajor (escala (t)) ? pc (t.to) : pc (t.to + 3);
        int millor = preferida, punts = -1;
        for (int k = 0; k < 12; ++k)
        {
            const int r = pc (preferida + k);   // comença per la preferida: a igualtat, es queda
            int n = 0;
            for (int x : pcs) if (aEscala (x, { r, 0 })) ++n;
            if (n > punts) { punts = n; millor = r; }
        }
        return { millor, 0 };
    }

    std::vector<AcordGrau> acordsProgressio (const Tonalitat& t, int prog, bool setenes)
    {
        const auto& p = progressions()[(size_t) std::clamp (prog, 0, (int) progressions().size() - 1)];
        const auto major = relativaMajor (t);
        const Tonalitat base = p.menor ? Tonalitat { pc (major.to + 9), 1 } : major;
        const auto acords = acordsDeTonalitat (base, setenes);
        std::vector<AcordGrau> r;
        for (int g : p.graus)
        {
            auto a = acords[(size_t) g];
            if (p.menor && p.vMajor && g == 4)
            {
                a.tipus = setenes ? 8 : 0;   // V (o V7) major
                a.nom = nomTo (a.pc, base) + tipusAcords()[(size_t) a.tipus].sufix;
                a.roma = setenes ? "V7" : "V";
            }
            r.push_back (a);
        }
        return r;
    }

    std::vector<std::vector<int>> generaProgressio (const Tonalitat& t, int prog, int compassos, bool setenes, Voicing v,
                                                    bool movimentMinim, int octava)
    {
        std::vector<std::vector<int>> r;
        const auto acords = acordsProgressio (t, prog, setenes);
        std::vector<int> anterior;
        for (int i = 0; i < std::max (1, compassos); ++i)
        {
            const auto& a = acords[(size_t) i % acords.size()];
            int arrel = 12 * (octava + 1) + a.pc;
            if (a.pc > 7) arrel -= 12;   // que no pugi massa (A, Bb, B a sota del C)
            auto notes = acord (arrel, a.tipus, 0, Voicing::Tancat);
            if (movimentMinim) notes = enllaca (anterior, notes);
            anterior = notes;
            if (v == Voicing::Obert && notes.size() >= 3)
            {
                notes[notes.size() - 2] -= 12;
                std::sort (notes.begin(), notes.end());
            }
            r.push_back (notes);
        }
        return r;
    }
}

namespace mibase::harmonia
{
    namespace
    {
        /** Els índexs triats, agrupats per inici (cada grup, un acord). */
        std::vector<std::vector<int>> grups (const Clip& c, const std::vector<int>& idx)
        {
            std::vector<int> v;
            for (int i : idx) if (i >= 0 && i < (int) c.notes.size()) v.push_back (i);
            std::sort (v.begin(), v.end(), [&c] (int a, int b) { return c.notes[(size_t) a].ini < c.notes[(size_t) b].ini; });
            std::vector<std::vector<int>> g;
            for (int i : v)
            {
                if (g.empty() || std::abs (c.notes[(size_t) g.back().front()].ini - c.notes[(size_t) i].ini) > ticksPerNegra / 16)
                    g.push_back ({});
                g.back().push_back (i);
            }
            for (auto& x : g)
                std::sort (x.begin(), x.end(), [&c] (int a, int b) { return c.notes[(size_t) a].to < c.notes[(size_t) b].to; });
            return g;
        }
    }

    std::vector<int> afegeixAcord (Clip& c, const std::vector<int>& notes, long long ini, long long dur, int vel)
    {
        std::vector<int> noves;
        for (int n : notes)
        {
            c.notes.push_back ({ ini, std::max (1LL, dur), std::clamp (n, 0, 127), std::clamp (vel, 1, 127), 0 });
            noves.push_back ((int) c.notes.size() - 1);
        }
        midi::ordena (c.notes, noves);
        return noves;
    }

    void inverteix (Clip& c, std::vector<int>& idx, bool amunt)
    {
        for (auto& g : grups (c, idx))
        {
            if (g.size() < 2) continue;
            auto& n = amunt ? c.notes[(size_t) g.front()] : c.notes[(size_t) g.back()];
            const int nou = n.to + (amunt ? 12 : -12);
            if (nou >= 0 && nou <= 127) n.to = nou;
        }
        midi::ordena (c.notes, idx);
    }

    void enllacaTriades (Clip& c, std::vector<int>& idx)
    {
        std::vector<int> anterior;
        for (auto& g : grups (c, idx))
        {
            std::vector<int> notes;
            for (int i : g) notes.push_back (c.notes[(size_t) i].to);
            const auto nou = enllaca (anterior, notes);
            if (nou.size() == g.size())
                for (size_t k = 0; k < g.size(); ++k) c.notes[(size_t) g[k]].to = nou[k];
            anterior = nou;
        }
        midi::ordena (c.notes, idx);
    }

    std::vector<int> ferAcords (Clip& c, const std::vector<int>& idx, const Tonalitat& t, int tipus, bool deTonalitat,
                                int inversio, Voicing v)
    {
        std::vector<Nota> noves;
        const auto diatonics = acordsDeTonalitat (t, false);
        std::vector<int> fets;
        for (int i : idx)
        {
            if (i < 0 || i >= (int) c.notes.size()) continue;
            const auto base = c.notes[(size_t) i];
            int tp = tipus;
            if (deTonalitat)
            {
                const int pcN = pc (base.to);
                tp = -1;
                for (auto& a : diatonics) if (a.pc == pcN) tp = a.tipus;
                if (tp < 0) tp = 0;   // nota fora de l'escala: acord major
            }
            const auto notes = acord (base.to, tp, inversio, v);
            for (int n : notes)
                if (n != base.to || inversio != 0 || v != Voicing::Tancat)
                {
                    auto x = base;
                    x.to = n;
                    noves.push_back (x);
                }
            if (inversio != 0 || v != Voicing::Tancat) fets.push_back (i);   // la nota original es substitueix
        }
        midi::esborra (c, fets);
        std::vector<int> r;
        for (auto& n : noves) { c.notes.push_back (n); r.push_back ((int) c.notes.size() - 1); }
        midi::ordena (c.notes, r);
        return r;
    }
}
