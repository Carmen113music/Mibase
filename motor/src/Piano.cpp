#include "mibase/Piano.h"
#include <optional>
#include "mibase/Midi.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <sstream>

namespace mibase::piano
{
    namespace
    {
        std::vector<std::string> paraules (const std::string& s)
        {
            std::vector<std::string> v;
            std::istringstream f (s);
            for (std::string w; f >> w;) v.push_back (w);
            return v;
        }

        // Atzar petit i repetible (mateixa llavor, mateixes notes).
        double atzar (unsigned llavor, unsigned quin)
        {
            unsigned long long z = ((unsigned long long) llavor << 32) ^ (quin * 0x9E3779B97F4A7C15ull) ^ 0xD1B54A32D192ED03ull;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
            z ^= z >> 31;
            return (double) (z >> 11) / (double) (1ull << 53);
        }

        struct Acord
        {
            std::vector<int> dreta;      // l'acord tancat a l'octava del do central, de greu a aguda
            int arrel = 60;              // la fonamental de la mà dreta
            int tercera = 4, quinta = 7, setena = 10;
            int baix = 36;               // la fonamental de la mà esquerra (o la nota de baix, si l'acord en té)
            std::vector<int> fonamental, desplaca;   // 0.61: l'acord en posició fonamental i quant es mou cada nota amb la inversió
        };

        /** 0.61: una nota de la mà dreta (calculada en posició fonamental) a la seva lloc a la inversió: es mou el mateix
            que la nota de l'acord de la seva classe (o, si no és de l'acord, que la nota de l'acord més propera). */
        int aInversio (const Acord& a, int n)
        {
            if (a.desplaca.empty()) return n;
            int millor = 0, dist = 99;
            for (size_t j = 0; j < a.fonamental.size(); ++j)
            {
                const int f = a.fonamental[j];
                if (((n - f) % 12 + 12) % 12 == 0) return n + a.desplaca[j];
                const int d = std::min (((n - f) % 12 + 12) % 12, ((f - n) % 12 + 12) % 12);
                if (d < dist) { dist = d; millor = a.desplaca[j]; }
            }
            return n + millor;
        }

        Acord prepara (const harmonia::AcordPad& a)
        {
            Acord r;
            auto b = a;
            b.inversio = 0;
            b.baix = -1;
            r.dreta = harmonia::notesPad (b, 4, harmonia::Voicing::Tancat);
            std::sort (r.dreta.begin(), r.dreta.end());
            if (r.dreta.empty()) return r;
            r.arrel = r.dreta.front();
            // Si l'acord queda molt amunt (La, Si…), una octava avall: la mà dreta es queda al voltant del do central.
            if (r.arrel > 65) { for (auto& n : r.dreta) n -= 12; r.arrel -= 12; }
            if (r.dreta.size() > 1) r.tercera = r.dreta[1] - r.arrel;
            if (r.dreta.size() > 2) r.quinta = r.dreta[2] - r.arrel;
            r.setena = r.dreta.size() > 3 ? r.dreta[3] - r.arrel : (r.tercera == 3 ? 10 : 11);
            r.fonamental = r.dreta;
            const int pc = a.baix >= 0 ? a.baix : a.arrel;
            r.baix = 36 + ((pc % 12) + 12) % 12;
            if (r.baix > 43) r.baix -= 12;   // de Sol# en amunt, una octava avall (entre el Sol#1 i el Sol2)
            return r;
        }

        int grau (const Acord& a, int g)
        {
            switch (g)
            {
                case 1:  return 0;
                case 2:  return 2;
                case 3:  return a.tercera;
                case 4:  return 5;
                case 5:  return a.quinta;
                case 6:  return 9;
                case 7:  return a.setena;
                case 8:  return 12;
                case 9:  return 14;
                case 10: return 12 + a.tercera;
                default: return 0;
            }
        }

        /** Les notes d'una paraula de la mà dreta (buit = «.» o no s'entén). «tipus»: 0 acord, 1 nota d'arpegi, 2 agudes, 3 polze. */
        std::vector<int> notesDreta (const Acord& a, const std::string& w, int& tipus)
        {
            tipus = 0;
            if (w == "C") return a.dreta;
            if (w == "T" || w == "T8")
            {
                tipus = 2;
                std::vector<int> v (a.dreta.end() - (std::ptrdiff_t) std::min<size_t> (2, a.dreta.size()), a.dreta.end());
                if (w == "T8") for (auto& n : v) n += 12;
                return v;
            }
            if (w == "P") { tipus = 3; return { aInversio (a, a.arrel) }; }
            if (w == "Cs4" || w == "Cs2")
            {
                auto v = a.fonamental;
                if (v.size() > 1) v[1] = a.arrel + (w == "Cs4" ? 5 : 2);
                for (auto& n : v) n = aInversio (a, n);
                std::sort (v.begin(), v.end());
                return v;
            }
            if (! w.empty() && std::isdigit ((unsigned char) w[0])) { tipus = 1; return { aInversio (a, a.arrel + grau (a, std::atoi (w.c_str()))) }; }
            return {};
        }

        std::vector<int> notesEsquerra (const Acord& a, const std::string& w, bool plena)
        {
            std::vector<int> v;
            std::istringstream f (w);
            for (std::string part; std::getline (f, part, '+');)
            {
                if (part == "R") v.push_back (a.baix);
                else if (! part.empty() && std::isdigit ((unsigned char) part[0])) v.push_back (a.baix + grau (a, std::atoi (part.c_str())));
            }
            if (plena && v.size() == 1 && w == "R") v.push_back (a.baix + 12);   // variació «ple»: la fonamental, amb l'octava
            return v;
        }

        std::map<std::string, Patro>& cau() { static std::map<std::string, Patro> m; return m; }
    }

    long long ticksCompas (const std::string& compas)
    {
        const auto on = compas.find ('/');
        if (on == std::string::npos) return 4 * ticksPerNegra;
        const int num = std::max (1, std::atoi (compas.substr (0, on).c_str())), den = std::max (1, std::atoi (compas.substr (on + 1).c_str()));
        return (long long) num * (4 * ticksPerNegra / den);
    }

    const std::vector<Patro>& patrons()
    {
        static const std::vector<Patro> llista = []
        {
            struct Def { const char* id; const char* grup; const char* nom; const char* compas; const char* dreta; long long pasD; const char* esquerra; long long pasE; double gate; int pedal; int vel; };
            static const Def defs[] = {
#include "PianoPatrons.inc"
            };
            std::vector<Patro> v;
            for (auto& d : defs)
            {
                Patro p;
                p.id = d.id; p.grup = d.grup; p.nom = d.nom; p.compas = d.compas;
                p.dreta = d.dreta; p.pasDreta = d.pasD; p.esquerra = d.esquerra; p.pasEsquerra = d.pasE;
                p.gate = d.gate; p.pedal = d.pedal; p.vel = d.vel;
                v.push_back (std::move (p));
            }
            return v;
        }();
        return llista;
    }

    std::vector<Patro> variacions (const Patro& p)
    {
        // 0.48: variacions que se senten de seguida (abans eren massa semblants al patró).
        std::vector<Patro> v;
        const long long compas = ticksCompas (p.compas);
        const long long temps = p.compas.size() > 2 && p.compas.substr (p.compas.size() - 2) == "/8" ? 1440 : 960;
        auto junta = [] (const std::vector<std::string>& w) { std::string s; for (auto& x : w) s += (s.empty() ? "" : " ") + x; return s; };
        auto esNum = [] (const std::string& x) { return ! x.empty() && std::isdigit ((unsigned char) x[0]); };
        auto fes = [&] (const char* sufix, auto canvia)
        {
            Patro q = p;
            q.id = p.id + "~" + sufix;
            q.nom = p.nom + " \xc2\xb7 " + sufix;
            canvia (q);
            if (q.dreta == p.dreta && q.esquerra == p.esquerra && q.octavaDreta == p.octavaDreta) return;
            for (auto& x : v) if (x.dreta == q.dreta && x.esquerra == q.esquerra && x.pasDreta == q.pasDreta && x.octavaDreta == q.octavaDreta) return;
            v.push_back (std::move (q));
        };
        const auto wd = paraules (p.dreta);
        int acords = 0, arpegi = 0;
        for (auto& x : wd) { if (esNum (x)) ++arpegi; else if (x != ".") ++acords; }

        if (acords >= arpegi)
            fes ("arpegi", [&] (Patro& q)
            {
                // els acords es desfan en un arpegi seguit de corxeres (o del pas del patró, si és més curt)
                const long long pas = std::min (480LL, q.pasDreta);
                const char* volta[] = { "1", "3", "5", "8", "5", "3" };
                std::vector<std::string> w;
                int k = 0;
                for (long long t = 0; t < compas; t += pas)
                    w.push_back (volta[k++ % 6]);   // puja i baixa: 1 3 5 8 5 3…
                q.dreta = junta (w); q.pasDreta = pas; q.gate = std::max (q.gate, 0.95);
            });
        else
            fes ("blocs", [&] (Patro& q)
            {
                // l'arpegi es torna acords sencers a cada temps
                std::vector<std::string> w;
                for (long long t = 0; t < compas; t += q.pasDreta) w.push_back (t % temps == 0 ? "C" : ".");
                q.dreta = junta (w);
            });

        if (p.pasDreta >= 240 && p.pasDreta % 2 == 0)
            fes ("doble", [&] (Patro& q)
            {
                // la mà dreta, el doble de ràpid (dues voltes per compàs)
                auto w = wd; w.insert (w.end(), wd.begin(), wd.end());
                q.dreta = junta (w); q.pasDreta = p.pasDreta / 2;
            });

        fes ("ple", [&] (Patro& q)
        {
            // gran: acords sencers a la dreta també als temps buits, i baixos en octaves
            auto w = wd;
            for (size_t k = 0; k < w.size(); ++k)
            {
                const bool aTemps = ((long long) k * q.pasDreta) % temps == 0;
                if (w[k] == "T" || w[k] == "P" || (w[k] == "." && aTemps)) w[k] = "C";
            }
            auto e = paraules (q.esquerra);
            for (size_t k = 0; k < e.size(); ++k)
            {
                if (e[k] == "R") e[k] = "R+8";
                else if (e[k] == "." && ((long long) k * q.pasEsquerra) % temps == 0) e[k] = "R";
            }
            q.dreta = junta (w); q.esquerra = junta (e);
            q.esquerraPlena = true; q.vel = std::min (118, q.vel + 12);
        });

        fes ("espaiat", [&] (Patro& q)
        {
            // de balada: només el que cau al principi i a mig compàs, i deixant sonar
            auto buida = [&] (const std::string& fila, long long pas, bool dreta)
            {
                auto w = paraules (fila);
                bool primer = true;
                for (size_t k = 0; k < w.size(); ++k)
                {
                    const long long t = (long long) k * pas;
                    const bool queda = t == 0 || (compas >= 2 * temps && t == (compas / (2 * temps)) * temps);
                    if (! queda) { w[k] = "."; continue; }
                    if (w[k] == ".") w[k] = dreta ? "C" : "R";
                    else if (dreta && esNum (w[k]) && primer) w[k] = "C";
                    primer = false;
                }
                return junta (w);
            };
            q.dreta = buida (q.dreta, q.pasDreta, true); q.esquerra = buida (q.esquerra, q.pasEsquerra, false);
            q.gate = 1.0; q.suau = true; q.vel = std::max (40, q.vel - 14);
        });

        fes ("8va", [] (Patro& q) { q.octavaDreta = 1; });   // la mà dreta, una octava amunt
        return v;
    }

    const Patro* patro (const std::string& id)
    {
        for (auto& p : patrons()) if (p.id == id) return &p;
        if (const auto on = id.rfind ('~'); on != std::string::npos)
        {
            auto& c = cau();
            if (auto it = c.find (id); it != c.end()) return &it->second;
            if (const auto* base = patro (id.substr (0, on)))
            {
                for (auto& v : variacions (*base)) c[v.id] = v;
                for (int k = 1; k < numPartsCanco; ++k) { auto v = variacioPart (*base, (PartCanco) k); c[v.id] = v; }
            }
            if (auto it = c.find (id); it != c.end()) return &it->second;
        }
        return nullptr;
    }

    Patro variacioPart (const Patro& p, PartCanco v)
    {
        if (v == PartCanco::Original) return p;
        const long long compas = std::max (1LL, ticksCompas (p.compas));
        const long long temps = p.compas.size() > 2 && p.compas.substr (p.compas.size() - 2) == "/8" ? 1440 : 960;
        auto junta = [] (const std::vector<std::string>& w) { std::string s; for (auto& x : w) s += (s.empty() ? "" : " ") + x; return s; };
        auto de = [&] (const char* sufix) -> std::optional<Patro>
        {
            for (auto& x : variacions (p)) if (x.id == p.id + "~" + sufix) return x;
            return std::nullopt;
        };
        Patro q = p;
        switch (v)
        {
            case PartCanco::Estrofa:
            {
                // suau, i el baix només al principi i a mig compàs (deixant sonar)
                auto e = paraules (q.esquerra);
                for (size_t k = 0; k < e.size(); ++k)
                {
                    const long long t = ((long long) k * q.pasEsquerra) % compas;
                    if (! (t == 0 || (compas >= 2 * temps && t == (compas / (2 * temps)) * temps))) e[k] = ".";
                }
                q.esquerra = junta (e);
                q.suau = true; q.vel = std::max (40, q.vel - 12);
                break;
            }
            case PartCanco::PreTornada:
            {
                // el baix a cada temps (empeny cap a la tornada), una mica més fort
                auto e = paraules (q.esquerra);
                for (size_t k = 0; k < e.size(); ++k)
                    if (((long long) k * q.pasEsquerra) % temps == 0 && e[k] == ".") e[k] = "R";
                q.esquerra = junta (e);
                q.vel = std::min (118, q.vel + 6);
                break;
            }
            case PartCanco::Tornada:
                if (const auto x = de ("ple")) q = *x;
                else { q.esquerraPlena = true; q.vel = std::min (118, q.vel + 12); }
                break;
            case PartCanco::Pont:
                if (const auto x = de ("espaiat")) q = *x;
                else { q.suau = true; q.gate = 1.0; q.vel = std::max (40, q.vel - 14); }
                break;
            case PartCanco::Original: break;
        }
        q.id = p.id + "~" + sufixPart (v);
        q.nom = p.nom + " \xc2\xb7 " + nomPart (v);
        return q;
    }

    std::string dibuix (const Patro& p)
    {
        auto net = [] (std::string s) { for (size_t k; (k = s.find ('.')) != std::string::npos;) s.replace (k, 1, "\xc2\xb7"); return s; };
        return "D: " + net (p.dreta) + "   E: " + net (p.esquerra);
    }

    std::vector<int> invertida (const std::vector<int>& fonamental, int inversio)
    {
        // 0.62.1 (demanat per en Roger): no és un cicle; cada pas amunt puja la nota més greu una octava i cada pas
        // avall baixa la més aguda, de manera que es va pujant (o baixant) d'octava inversió a inversió.
        auto v = fonamental;
        std::sort (v.begin(), v.end());
        if (v.size() < 2) return v;
        for (int i = 0; i < inversio; ++i) { v.front() += 12; std::sort (v.begin(), v.end()); }
        for (int i = 0; i > inversio; --i) { v.back() -= 12; std::sort (v.begin(), v.end()); }
        return v;
    }

    std::vector<Nota> toca (const harmonia::AcordPad& acord, const Patro& p, long long durada, const Opcions& o)
    {
        std::vector<Nota> r;
        if (acord.buit() || durada <= 0) return r;
        Acord a = prepara (acord);
        if (a.dreta.empty()) return r;
        if (o.inversio != 0 && a.dreta.size() > 1)   // 0.61: inversió de la mà dreta
        {
            a.dreta = invertida (a.fonamental, o.inversio);
            // quant s'ha mogut cada nota de l'acord (per a les notes d'arpegi, el polze…)
            a.desplaca.assign (a.fonamental.size(), 0);
            for (size_t j = 0; j < a.fonamental.size(); ++j)
                for (int n : a.dreta)
                    if (((n - a.fonamental[j]) % 12 + 12) % 12 == 0) { a.desplaca[j] = n - a.fonamental[j]; break; }
        }
        const int base = std::clamp (o.vel > 0 ? o.vel : p.vel, 20, 122);
        const double ticksPerMs = std::max (20.0, o.tempo) * (double) ticksPerNegra / 60000.0;
        const double var = std::clamp (o.variacio, 0.0, 1.0);
        unsigned quin = 0;

        auto ma = [&] (const std::string& fila, long long pas, bool dreta)
        {
            const auto w = paraules (fila);
            if (w.empty() || pas <= 0) return;
            const long long n = (long long) w.size(), total = (durada + pas - 1) / pas;
            for (long long k = 0; k < total; ++k)
            {
                const std::string& par = w[(size_t) (k % n)];
                if (par == ".") continue;
                int tipus = 0;
                auto notes = dreta ? notesDreta (a, par, tipus) : notesEsquerra (a, par, p.esquerraPlena);
                if (notes.empty()) continue;
                // Dura fins al proper cop de la mateixa mà (o el final), per la part que diu el gate.
                long long seg = k + 1;
                while (seg < total && w[(size_t) (seg % n)] == ".") ++seg;
                const long long ini = k * pas, fi = std::min (seg * pas, durada);
                long long dur = std::max (60LL, (long long) std::llround ((double) (fi - ini) * std::clamp (p.gate, 0.1, 1.0)));
                dur = std::min (dur, durada - ini);
                if (dur <= 0) continue;
                const bool aTemps = ini % ticksPerNegra == 0, aCompas = ini % std::max (1LL, ticksCompas (p.compas)) == 0;
                int vel = base + (aCompas ? 6 : aTemps ? 2 : -3);
                if (dreta) vel += tipus == 1 ? -5 : tipus == 3 ? -14 : tipus == 2 ? -2 : 0;
                else vel -= 8;
                if (p.suau) vel -= dreta ? 0 : 6;
                // Humanització: una mica de força i de temps (mai el primer cop del bloc), i l'acord una mica arpegiat.
                vel += (int) std::llround ((atzar (o.llavor, ++quin) - 0.5) * 2.0 * (3.0 + 9.0 * var));
                long long mou = ini == 0 ? 0 : (long long) std::llround ((atzar (o.llavor, ++quin) - 0.5) * 2.0 * 14.0 * var);
                const double obre = notes.size() > 1 ? (dreta ? 11.0 : 6.0) * ticksPerMs : 0.0;   // ~11 ms entre notes
                std::sort (notes.begin(), notes.end());
                for (size_t j = 0; j < notes.size(); ++j)
                {
                    const long long t = std::max (0LL, ini + mou + (long long) std::llround (obre * (double) j));
                    const int to = notes[j] + (dreta ? 12 * p.octavaDreta : 0);
                    if (to < 0 || to > 127 || t >= durada) continue;
                    const int v = std::clamp (vel + (dreta && j + 1 == notes.size() ? 4 : 0), 1, 127);   // la de dalt, una mica més
                    r.push_back ({ t, std::max (30LL, std::min (dur, durada - t)), to, v, 0 });
                }
            }
        };
        ma (p.dreta, p.pasDreta, true);
        ma (p.esquerra, p.pasEsquerra, false);
        std::sort (r.begin(), r.end(), [] (const Nota& x, const Nota& y) { return x.ini != y.ini ? x.ini < y.ini : x.to < y.to; });
        return r;
    }

    BlocPatro blocDe (const Patro* p, long long ini, long long dur, double variacio, unsigned llavor, int vel)
    {
        BlocPatro b;
        b.tipus = "piano";
        b.ini = ini; b.dur = dur; b.variacio = variacio; b.llavor = llavor; b.vel = vel;
        if (p != nullptr)
        {
            b.patro = p->id; b.nom = p->nom;
            b.passos = p->dreta; b.pasTicks = p->pasDreta;
            b.passosEsq = p->esquerra; b.pasTicksEsq = p->pasEsquerra;
            b.gate = p->gate; b.pedal = p->pedal; b.compas = p->compas;
            if (vel <= 0) b.vel = p->vel;
        }
        return b;
    }

    Patro patroDeBloc (const BlocPatro& b)
    {
        Patro p;
        if (const auto* base = patro (b.patro)) p = *base;   // per a les variacions (octava, ple, suau)
        p.id = b.patro; p.nom = b.nom; p.compas = b.compas.empty() ? std::string ("4/4") : b.compas;
        p.dreta = b.passos; p.pasDreta = b.pasTicks;
        p.esquerra = b.passosEsq; p.pasEsquerra = b.pasTicksEsq;
        p.gate = b.gate; p.pedal = b.pedal; p.vel = b.vel;
        return p;
    }
}
