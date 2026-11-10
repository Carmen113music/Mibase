#include "mibase/Guitarra.h"
#include "mibase/Midi.h"
#include <algorithm>
#include <cmath>

namespace mibase::guitarra
{
    namespace
    {
        int pc (int n) { return ((n % 12) + 12) % 12; }

        // Els acords oberts de tota la vida (i quatre celletes molt fetes servir), tal com es toquen.
        struct Obert { const char* acord; const char* pos; };
        constexpr Obert oberts[] = {
            { "C", "x32010" }, { "D", "xx0232" }, { "E", "022100" }, { "G", "320003" }, { "A", "x02220" },
            { "Am", "x02210" }, { "Dm", "xx0231" }, { "Em", "022000" },
            { "C7", "x32310" }, { "D7", "xx0212" }, { "E7", "020100" }, { "G7", "320001" }, { "A7", "x02020" }, { "B7", "x21202" },
            { "Am7", "x02010" }, { "Dm7", "xx0211" }, { "Em7", "022030" },
            { "Cmaj7", "x32000" }, { "Dmaj7", "xx0222" }, { "Amaj7", "x02120" }, { "Fmaj7", "xx3210" }, { "Gmaj7", "320002" }, { "Emaj7", "021100" },
            { "Dsus4", "xx0233" }, { "Dsus2", "xx0230" }, { "Asus4", "x02230" }, { "Asus2", "x02200" }, { "Esus4", "022200" },
            { "F", "133211" }, { "Fm", "133111" }, { "Bm", "x24432" }, { "B", "x24442" },
            { "Cadd9", "x32030" }, { "E9", "020102" }, { "A6", "x02222" }, { "E6", "022120" }, { "D6", "xx0202" }, { "C6", "x32210" },
            { "G6", "320000" }, { "Am6", "x02212" }, { "Em6", "022020" },
        };

        /** Les classes de to de l'acord (0..11, relatives a la fonamental). */
        std::vector<int> intervals (const harmonia::AcordPad& a)
        {
            auto b = a;
            b.inversio = 0;
            b.baix = -1;
            std::vector<int> r;
            for (int n : harmonia::notesPad (b))
            {
                const int i = pc (n - a.arrel);
                if (std::find (r.begin(), r.end(), i) == r.end()) r.push_back (i);
            }
            return r;
        }
    }

    bool Posicio::buida() const noexcept
    {
        for (int t : trast) if (t >= 0) return false;
        return true;
    }

    std::vector<int> Posicio::notes() const
    {
        std::vector<int> r;
        for (int s = 0; s < numCordes; ++s)
            if (trast[(size_t) s] >= 0) r.push_back (afinacio[(size_t) s] + trast[(size_t) s]);
        return r;
    }

    std::string Posicio::text() const
    {
        bool llarg = false;
        for (int t : trast) if (t > 9) llarg = true;
        std::string r;
        for (int s = 0; s < numCordes; ++s)
        {
            if (llarg && s > 0) r += '-';
            const int t = trast[(size_t) s];
            r += t < 0 ? std::string ("x") : std::to_string (t);
        }
        return r;
    }

    Posicio llegeixPosicio (const std::string& text)
    {
        Posicio p;
        int s = 0;
        for (size_t i = 0; i < text.size() && s < numCordes; ++i)
        {
            const char c = text[i];
            if (c == '-') continue;
            if (c == 'x' || c == 'X') { p.trast[(size_t) s++] = -1; continue; }
            if (c < '0' || c > '9') continue;
            int v = c - '0';
            if (text.find ('-') != std::string::npos)
                while (i + 1 < text.size() && text[i + 1] >= '0' && text[i + 1] <= '9') v = v * 10 + (text[++i] - '0');
            p.trast[(size_t) s++] = v;
        }
        return p;
    }

    Posicio posicio (const harmonia::AcordPad& a)
    {
        if (a.buit()) return {};
        const int baix = a.baix >= 0 ? a.baix : a.arrel;

        if (baix == a.arrel)
        {
            auto net = a;
            net.inversio = 0;
            net.baix = -1;
            for (auto& o : oberts)
                if (harmonia::llegeixAcord (o.acord) == net) return llegeixPosicio (o.pos);
        }

        // Les notes que hi han de ser i les que es poden deixar (amb 5 notes o més, la quinta; amb 6 o més, també la 9a i l'11a).
        const auto iv = intervals (a);
        std::vector<int> cal = iv;
        auto treu = [&cal] (int i) { cal.erase (std::remove (cal.begin(), cal.end(), i), cal.end()); };
        if (iv.size() >= 5) treu (7);
        if (cal.size() > 4) treu (2);
        if (cal.size() > 4) treu (5);
        bool esDe[12] = {}, esCal[12] = {};
        for (int i : iv) esDe[pc (a.arrel + i)] = true;
        for (int i : cal) esCal[pc (a.arrel + i)] = true;
        esDe[baix] = true;

        // Fora dels acords oberts de la taula, cordes a l'aire només si l'acord porta una altra nota greu (D/F#, G/B…):
        // la resta surten amb celleta, que és com es toquen.
        const bool permetAire = baix != a.arrel;
        Posicio millor;
        double millorPunts = 1e9;
        std::array<std::vector<int>, numCordes> opcions;
        for (int base = 1; base <= 10; ++base)
        {
            for (int s = 0; s < numCordes; ++s)
            {
                auto& o = opcions[(size_t) s];
                o.assign (1, -1);
                if (permetAire && base <= 2 && esDe[pc (afinacio[(size_t) s])]) o.push_back (0);
                for (int t = base; t <= base + 3; ++t)
                    if (esDe[pc (afinacio[(size_t) s] + t)]) o.push_back (t);
            }
            std::array<size_t, numCordes> k {};
            for (;;)
            {
                Posicio p;
                for (int s = 0; s < numCordes; ++s) p.trast[(size_t) s] = opcions[(size_t) s][k[(size_t) s]];

                // Vàlida? Cordes seguides, la greu fa el baix, hi són les notes que cal, i es pot posar amb 4 dits.
                int primera = -1, ultima = -1, quantes = 0, trepitjades = 0, minT = 99, maxT = 0, aire = 0;
                bool forat = false, te[12] = {};
                for (int s = 0; s < numCordes; ++s)
                {
                    const int t = p.trast[(size_t) s];
                    if (t < 0) continue;
                    if (primera < 0) primera = s;
                    if (ultima >= 0 && s != ultima + 1) forat = true;
                    ultima = s;
                    ++quantes;
                    te[pc (afinacio[(size_t) s] + t)] = true;
                    if (t == 0) ++aire;
                    else { ++trepitjades; minT = std::min (minT, t); maxT = std::max (maxT, t); }
                }
                bool val = quantes >= 4 && ! forat && pc (afinacio[(size_t) primera] + p.trast[(size_t) primera]) == baix;
                for (int n = 0; val && n < 12; ++n) if (esCal[n] && ! te[n]) val = false;
                int dits = trepitjades;
                if (val && aire == 0 && trepitjades > 0)
                {
                    dits = 1;   // la celleta
                    for (int t : p.trast) if (t > minT) ++dits;
                }
                if (val && dits > 4) val = false;
                if (val)
                {
                    int falten = 0;
                    for (int i : iv) if (! te[pc (a.arrel + i)]) ++falten;
                    // Com més avall del mànec, més cordes i menys dits, millor; les posicions molt obertes (4 trasts) costen.
                    const double punts = (trepitjades > 0 ? 1.5 * minT + (minT > 7 ? 4.0 : 0.0) : 0.0) - 3.0 * quantes + 1.5 * dits + 3.0 * falten + 1.0 * aire
                                         + (trepitjades > 0 && maxT - minT >= 3 ? 8.0 : 0.0) + (ultima < numCordes - 1 ? 2.0 : 0.0);
                    if (punts < millorPunts - 1e-9) { millorPunts = punts; millor = p; }
                }

                int s = numCordes - 1;
                while (s >= 0 && ++k[(size_t) s] >= opcions[(size_t) s].size()) k[(size_t) s--] = 0;
                if (s < 0) break;
            }
        }
        return millor;
    }

    std::vector<int> notes (const harmonia::AcordPad& a)
    {
        const auto p = posicio (a);
        if (! p.buida()) return p.notes();
        return harmonia::notesPad (a);
    }

    //==========================================================================
    std::vector<Pas> llegeixPassos (const std::string& text)
    {
        std::vector<Pas> r;
        for (size_t i = 0; i < text.size(); ++i)
        {
            Pas p;
            switch (text[i])
            {
                case 'D': p.cop = Cop::Avall; break;
                case 'U': p.cop = Cop::Amunt; break;
                case 'X': p.cop = Cop::MutAvall; break;
                case 'x': p.cop = Cop::MutAmunt; break;
                case '_': p.cop = Cop::Silenci; break;
                case '.': p.cop = Cop::Res; break;
                default: continue;
            }
            if (i + 1 < text.size() && text[i + 1] == '>') { p.accent = true; ++i; }
            r.push_back (p);
        }
        return r;
    }

    const std::vector<Patro>& patrons()
    {
        static const std::vector<Patro> llista = []
        {
            struct Def { const char* id; const char* nom; long long pas; const char* passos; };
            const Def defs[] = {
                { "un",         "Un sol rasgueig",                 0,   "D" },
                { "negres",     "Negres avall",                    480, "D> . D . D . D ." },
                { "corxeres",   "Corxeres avall",                  480, "D> D D D D> D D D" },
                { "seguit",     "Avall i amunt seguit",            480, "D> U D U D> U D U" },
                { "folk",       "Folk / pop",                      480, "D> . D U . U D U" },
                { "balada",     "Balada",                          480, "D> . D . . U D U" },
                { "percussiu",  "Percussiu",                       480, "D> . X U . U X U" },
                { "rock",       "Rock apagat",                     480, "D> D X U D> D X U" },
                { "contratemps","Contratemps",                     480, "_ D> _ D> _ D> _ D>" },
                { "balada16",   "Balada 1/16",                     240, "D> . . . D . D U . U D U D . D U" },
                { "funk",       "Funk 1/16",                       240, "D> . X U X U D . X U D> U X U X U" },
                { "vals",       "Vals (3/4)",                      480, "D> . D U D U" },
            };
            std::vector<Patro> v;
            for (auto& d : defs)
            {
                Patro p;
                p.id = d.id;
                p.nom = d.nom;
                p.pasTicks = d.pas;
                p.passos = llegeixPassos (d.passos);
                for (auto& s : p.passos)
                {
                    if (! p.dibuix.empty()) p.dibuix += ' ';
                    switch (s.cop)
                    {
                        case Cop::Avall:    p.dibuix += "\xe2\x86\x93"; break;   // ↓
                        case Cop::Amunt:    p.dibuix += "\xe2\x86\x91"; break;   // ↑
                        case Cop::MutAvall:
                        case Cop::MutAmunt: p.dibuix += "x"; break;
                        case Cop::Silenci:  p.dibuix += "-"; break;
                        case Cop::Res:      p.dibuix += "\xc2\xb7"; break;       // ·
                    }
                    if (s.accent) p.dibuix += ">";
                }
                v.push_back (std::move (p));
            }
            return v;
        }();
        return llista;
    }

    const Patro* patro (const std::string& id)
    {
        for (auto& p : patrons()) if (p.id == id) return &p;
        return nullptr;
    }

    std::vector<Nota> rasgueja (const std::vector<int>& cordes, const Patro& p, long long durada, const Opcions& o)
    {
        std::vector<Nota> r;
        const int n = (int) cordes.size();
        if (n == 0 || durada <= 0 || p.passos.empty()) return r;

        const double ticksPerMs = std::max (20.0, o.tempo) / 60.0 * (double) ticksPerNegra / 1000.0;
        const long long pas = p.pasTicks > 0 ? p.pasTicks : durada;
        // El rasgueig sencer no ha d'ocupar més de mig pas (a tempos molt ràpids s'estreny).
        long long delta = std::max (1LL, (long long) std::llround (o.msPerCorda * ticksPerMs));
        delta = std::max (1LL, std::min (delta, pas / (2 * (long long) std::max (1, n))));
        const long long durMut = std::max (10LL, std::min ((long long) std::llround (45.0 * ticksPerMs), pas / 2));

        std::vector<int> sonant ((size_t) n, -1);   // per corda: la nota que encara sona (índex a r)
        auto apaga = [&] (int s, long long t)
        {
            const int i = sonant[(size_t) s];
            if (i >= 0) r[(size_t) i].dur = std::max (1LL, t - r[(size_t) i].ini);
            sonant[(size_t) s] = -1;
        };
        auto variacio = [] (long long k, int s) { return (int) (((k * 7 + s * 13 + 3) * 2654435761u >> 16) % 7) - 3; };   // -3..+3, sempre igual

        for (long long k = 0; k * pas < durada; ++k)
        {
            if (p.pasTicks <= 0 && k > 0) break;
            const Pas& ps = p.passos[(size_t) (k % (long long) p.passos.size())];
            const long long t = k * pas;
            if (ps.cop == Cop::Res) continue;
            const bool mut = ps.cop == Cop::MutAvall || ps.cop == Cop::MutAmunt;
            if (ps.cop == Cop::Silenci || mut)
                for (int s = 0; s < n; ++s) apaga (s, t);
            if (ps.cop == Cop::Silenci) continue;

            const bool avall = ps.cop == Cop::Avall || ps.cop == Cop::MutAvall;
            const int quantes = avall ? n : std::min (n, 4);   // amunt, la pua només arriba a les agudes
            for (int j = 0; j < quantes; ++j)
            {
                const int s = avall ? j : n - 1 - j;
                const long long ini = t + j * delta;
                if (ini >= durada) break;
                apaga (s, ini);
                int vel;
                if (mut) vel = 44 - 2 * j + (ps.accent ? 8 : 0);
                else
                {
                    vel = o.vel + (ps.accent ? 22 : 0);
                    vel += avall ? (j == 0 ? 10 : -2 * j) : (j == 0 ? -2 : -8 - 3 * j);
                }
                vel += variacio (k, s);
                Nota nota;
                nota.ini = ini;
                nota.dur = mut ? std::max (1LL, std::min (durMut, durada - ini)) : std::max (1LL, durada - ini);
                nota.to = std::clamp (cordes[(size_t) s], 0, 127);
                nota.vel = std::clamp (vel, 1, 127);
                r.push_back (nota);
                if (! mut) sonant[(size_t) s] = (int) r.size() - 1;
            }
        }
        std::stable_sort (r.begin(), r.end(), [] (const Nota& a, const Nota& b) { return a.ini < b.ini; });
        return r;
    }

    std::vector<int> afegeix (Clip& c, const std::vector<Nota>& notes, long long ini)
    {
        std::vector<Nota> noves;
        for (auto n : notes)
        {
            n.ini += ini;
            noves.push_back (n);
            c.notes.push_back (n);
        }
        std::vector<int> cap;
        midi::ordena (c.notes, cap);
        std::vector<int> r;
        for (int i = 0; i < (int) c.notes.size(); ++i)
            for (size_t j = 0; j < noves.size(); ++j)
                if (noves[j].to >= 0 && c.notes[(size_t) i] == noves[j]) { r.push_back (i); noves[j].to = -1; break; }
        return r;
    }
}
