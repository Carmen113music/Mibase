#include "mibase/FontInspiracio.h"
#include "mibase/Guitarra.h"
#include "mibase/Harmonia.h"
#include "mibase/Piano.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>

namespace mibase::font
{
    using harmonia::AcordPad;

    // ---------------------------------------------------------------- la idea
    std::string textIdea (const Idea& i)
    {
        nlohmann::json j;
        auto& a = j["acords"] = nlohmann::json::array();
        for (auto& k : i.acords) a.push_back ({ { "acord", k.acord }, { "temps", k.temps } });
        j["instrument"] = (int) i.instrument;
        j["patroPiano"] = i.patroPiano;
        j["patroGuitarra"] = i.patroGuitarra;
        j["bucle"] = i.bucle;
        j["tePropia"] = i.tePropia;
        j["to"] = i.tonalitat.to;
        j["escala"] = i.tonalitat.escala;
        return j.dump();
    }

    Idea llegeixIdea (const std::string& text)
    {
        Idea i;
        if (text.empty()) return i;
        try
        {
            const auto j = nlohmann::json::parse (text);
            if (j.contains ("acords") && j["acords"].is_array())
                for (auto& k : j["acords"])
                {
                    AcordIdea a;
                    a.acord = k.value ("acord", std::string());
                    a.temps = std::clamp (k.value ("temps", 4.0), tempsMinim, 64.0);
                    if (! a.acord.empty() && harmonia::llegeixAcord (a.acord).buit()) continue;
                    i.acords.push_back (a);
                }
            i.instrument = j.value ("instrument", 0) == 1 ? Instr::Guitarra : Instr::Piano;
            i.patroPiano = std::clamp (j.value ("patroPiano", 0), 0, 3);
            i.patroGuitarra = std::clamp (j.value ("patroGuitarra", 0), 0, 3);
            i.bucle = j.value ("bucle", true);
            i.tePropia = j.value ("tePropia", false);
            i.tonalitat.to = std::clamp (j.value ("to", 0), 0, 11);
            i.tonalitat.escala = std::max (0, j.value ("escala", 0));
            if (i.tonalitat.escala >= (int) harmonia::escales().size()) i.tonalitat.escala = 0;
        }
        catch (...) { return Idea(); }
        return i;
    }

    double inici (const Idea& i, int index)
    {
        double t = 0.0;
        for (int k = 0; k < index && k < (int) i.acords.size(); ++k) t += i.acords[(size_t) k].temps;
        return t;
    }

    double durada (const Idea& i) { return inici (i, (int) i.acords.size()); }

    void afegeix (Idea& i, const std::string& acord, double temps)
    {
        i.acords.push_back ({ acord, std::max (tempsMinim, temps) });
    }

    void treu (Idea& i, int index)
    {
        if (index >= 0 && index < (int) i.acords.size()) i.acords.erase (i.acords.begin() + index);
    }

    bool mouInici (Idea& i, int index, double delta)
    {
        if (index <= 0 || index >= (int) i.acords.size() || delta == 0.0) return false;
        auto& ant = i.acords[(size_t) index - 1];
        auto& ara = i.acords[(size_t) index];
        const double a = ant.temps + delta, b = ara.temps - delta;
        if (a < tempsMinim - 1e-9 || b < tempsMinim - 1e-9) return false;
        ant.temps = a; ara.temps = b;
        return true;
    }

    bool canviaDurada (Idea& i, int index, double delta, int tempsCompas)
    {
        if (index < 0 || index >= (int) i.acords.size() || delta == 0.0) return false;
        auto& k = i.acords[(size_t) index];
        const double n = std::clamp (k.temps + delta, tempsMinim, 8.0 * std::max (1, tempsCompas));
        if (std::abs (n - k.temps) < 1e-9) return false;
        k.temps = n;
        return true;
    }

    // ---------------------------------------------------------------- suggeriments
    bool esMenor (const Tonalitat& t)
    {
        const auto n = harmonia::notesEscala (t);
        for (int pc : n) if (((pc - t.to) % 12 + 12) % 12 == 3) return true;
        return false;
    }

    namespace
    {
        struct Cand { int rel; const char* sufix; double base; Nivell nivell; };

        // rel: semitons sobre la tònica. Les bases fan l'ordre «per defecte»; el context hi suma.
        const std::vector<Cand>& candidatsMajor()
        {
            static const std::vector<Cand> v {
                { 0, "", 100, Nivell::Habitual }, { 7, "", 96, Nivell::Habitual }, { 5, "", 95, Nivell::Habitual },
                { 9, "m", 92, Nivell::Habitual }, { 2, "m", 82, Nivell::Habitual }, { 4, "m", 74, Nivell::Habitual },
                { 7, "7", 60, Nivell::Color }, { 5, "maj7", 54, Nivell::Color }, { 0, "maj7", 52, Nivell::Color },
                { 2, "m7", 52, Nivell::Color }, { 9, "m7", 50, Nivell::Color }, { 7, "sus4", 46, Nivell::Color },
                { 0, "add9", 46, Nivell::Color }, { 0, "sus2", 44, Nivell::Color }, { 0, "sus4", 42, Nivell::Color },
                { 4, "m7", 40, Nivell::Color }, { 5, "add9", 40, Nivell::Color }, { 11, "dim", 34, Nivell::Color },
                { 11, "m7b5", 30, Nivell::Color },
                { 2, "7", 48, Nivell::Atrevit },   // V/V
                { 10, "", 47, Nivell::Atrevit },   // bVII (manllevat)
                { 4, "7", 46, Nivell::Atrevit },   // V/vi
                { 5, "m", 45, Nivell::Atrevit },   // iv (manllevat)
                { 8, "", 43, Nivell::Atrevit },    // bVI (manllevat)
                { 9, "7", 40, Nivell::Atrevit },   // V/ii
                { 2, "", 38, Nivell::Atrevit },    // II (lidi)
                { 0, "7", 36, Nivell::Atrevit },   // V/IV
                { 3, "", 36, Nivell::Atrevit },    // bIII (manllevat)
                { 7, "m", 34, Nivell::Atrevit },   // v (mixolidi)
                { 11, "7", 30, Nivell::Atrevit },  // V/iii
            };
            return v;
        }

        const std::vector<Cand>& candidatsMenor()
        {
            static const std::vector<Cand> v {
                { 0, "m", 100, Nivell::Habitual }, { 8, "", 93, Nivell::Habitual }, { 10, "", 91, Nivell::Habitual },
                { 5, "m", 90, Nivell::Habitual }, { 3, "", 86, Nivell::Habitual }, { 7, "", 80, Nivell::Habitual },
                { 7, "m", 62, Nivell::Color }, { 7, "7", 60, Nivell::Color }, { 8, "maj7", 52, Nivell::Color },
                { 0, "m7", 48, Nivell::Color }, { 5, "m7", 46, Nivell::Color }, { 0, "sus2", 44, Nivell::Color },
                { 3, "maj7", 44, Nivell::Color }, { 2, "m7b5", 42, Nivell::Color }, { 0, "sus4", 40, Nivell::Color },
                { 10, "sus4", 38, Nivell::Color }, { 2, "dim", 34, Nivell::Color },
                { 5, "", 46, Nivell::Atrevit },    // IV (dòric)
                { 10, "7", 42, Nivell::Atrevit },  // V/III
                { 0, "7", 38, Nivell::Atrevit },   // V/iv
                { 1, "", 36, Nivell::Atrevit },    // bII (napolità)
                { 3, "7", 36, Nivell::Atrevit },   // V/VI
                { 0, "", 34, Nivell::Atrevit },    // I major (picarda)
                { 11, "dim", 32, Nivell::Atrevit },
                { 2, "7", 30, Nivell::Atrevit },   // V/V
            };
            return v;
        }

        /** Un acord llegit: l'arrel i què és (només el que cal per enllaçar). */
        struct Info { int arrel = -1; bool dominant = false, major = false; std::string nom; bool buit() const { return arrel < 0; } };
        Info info (const std::string& nom)
        {
            Info r;
            const auto a = harmonia::llegeixAcord (nom);
            if (a.buit()) return r;
            r.arrel = a.arrel;
            r.nom = nom;
            const auto barra = nom.find ('/');
            const std::string cos = nom.substr (0, barra);
            r.dominant = cos.size() >= 2 && cos.back() == '7' && cos.find ("maj7") == std::string::npos && cos.find ("m7") == std::string::npos;
            r.major = a.tipus == harmonia::llegeixAcord (harmonia::nomTo (a.arrel)).tipus && a.tensions == 0;
            return r;
        }

        /** Com de natural és anar de «de» a «a». */
        double enllac (const Info& de, const Info& a, int tonica)
        {
            if (de.buit() || a.buit()) return 0.0;
            const int iv = ((a.arrel - de.arrel) % 12 + 12) % 12;
            double s = 0.0;
            if (iv == 0) return de.nom == a.nom ? -150.0 : -25.0;
            switch (iv)
            {
                case 5:  s += 30; break;              // quinta avall (V → I)
                case 7:  s += 15; break;              // quinta amunt (I → V)
                case 2:  s += 12; break;              // pas amunt (IV → V)
                case 10: s += 10; break;              // pas avall
                case 9:  s += 14; break;              // tercera avall (I → vi)
                case 3: case 4: case 8: s += 8; break;
                default: break;
            }
            // una dominant (o un acord major que en fa) vol anar a la seva tònica
            if (iv == 5 && de.dominant) s += 45;
            if (iv == 5 && de.major && ((de.arrel - tonica + 12) % 12) == 7) s += 12;
            if (a.arrel == tonica && ((de.arrel - tonica + 12) % 12) == 5) s += 8;   // plagal
            return s;
        }
    }

    std::vector<Suggeriment> suggeriments (const Tonalitat& t, const std::vector<AcordIdea>& fila, int index)
    {
        const bool menor = esMenor (t);
        const auto& cands = menor ? candidatsMenor() : candidatsMajor();
        const int n = (int) fila.size();
        index = std::clamp (index, 0, n);
        Info abans, despres;
        for (int k = index - 1; k >= 0; --k) if (! fila[(size_t) k].acord.empty()) { abans = info (fila[(size_t) k].acord); break; }
        if (index + 1 < n) despres = info (fila[(size_t) index + 1].acord);

        std::vector<Suggeriment> v;
        for (auto& c : cands)
        {
            // els graus rebaixats (bII, bIII, bVI, bVII) s'escriuen amb bemolls: «Bb», no «A#»
            static const char* const bemolls[] = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
            const int pc = (t.to + c.rel) % 12;
            const bool rebaixat = c.rel == 1 || c.rel == 3 || c.rel == 8 || c.rel == 10;
            const Info a = info ((rebaixat ? std::string (bemolls[pc]) : harmonia::nomTo (pc, t)) + c.sufix);
            if (a.buit()) continue;
            Suggeriment s;
            s.acord = a.nom;
            s.nivell = c.nivell;
            double p = c.base;
            if (abans.buit())
                p += (c.rel == 0 && c.nivell == Nivell::Habitual) ? 40.0 : 0.0;
            else
                p += enllac (abans, a, t.to);
            if (! despres.buit())
            {
                p += 0.6 * enllac (a, despres, t.to);
                if (a.dominant && ((despres.arrel - a.arrel + 12) % 12) == 5) p += 35;   // dominant secundària que resol
            }
            if (index > 0 && (index + 1) % 4 == 0 && c.rel == 7) p += 12;    // final de frase: la dominant
            if (index > 0 && index % 4 == 0 && c.rel == 0) p += 8;           // començament de frase: la tònica
            for (auto& k : fila) if (k.acord == s.acord) { p += 6; break; }  // els que ja hi són lliguen
            s.puntuacio = p;
            v.push_back (s);
        }
        // els sis primers: els habituals; després, tot per puntuació
        std::stable_sort (v.begin(), v.end(), [] (const Suggeriment& x, const Suggeriment& y)
        {
            const bool hx = x.nivell == Nivell::Habitual, hy = y.nivell == Nivell::Habitual;
            if (hx != hy) return hx;
            return x.puntuacio > y.puntuacio;
        });
        // sense repetits (mateix nom)
        std::vector<Suggeriment> r;
        for (auto& s : v)
            if (std::none_of (r.begin(), r.end(), [&s] (const Suggeriment& q) { return q.acord == s.acord; })) r.push_back (s);
        r.push_back ({ std::string(), Nivell::Atrevit, -1000.0 });   // el silenci, al final
        return r;
    }

    std::string mesProbable (const Tonalitat& t, const std::vector<AcordIdea>& fila, int index)
    {
        std::string anterior;
        if (index > 0 && index - 1 < (int) fila.size()) anterior = fila[(size_t) index - 1].acord;
        for (auto& s : suggeriments (t, fila, index))
            if (! s.acord.empty() && s.acord != anterior) return s.acord;
        return harmonia::nomTo (t.to, t) + (esMenor (t) ? "m" : "");
    }

    // ---------------------------------------------------------------- notes
    const std::array<OpcioPatro, 4>& opcionsPatrons (Instr i)
    {
        static const std::array<OpcioPatro, 4> piano {{
            { "pd03", "Arpegi ondulat" }, { "pd01", "Arpegi Alberti" }, { "pa01", "Pop a negres" }, { "pa25", "Rock a corxeres" } }};
        static const std::array<OpcioPatro, 4> guitarra {{
            { "f01", "Arpegi" }, { "m36", "Travis picking" }, { "m01", "Rasgueig pop" }, { "m04", "Indie rock" } }};
        return i == Instr::Guitarra ? guitarra : piano;
    }

    long long ticksTemps (int den) { return 960LL * 4 / std::max (1, den); }

    namespace
    {
        double alcadaDreta (const std::vector<Nota>& v)
        {
            double s = 0.0; int k = 0;
            for (auto& x : v) if (x.to >= 52) { s += x.to; ++k; }
            return k > 0 ? s / k : 64.0;
        }
    }

    std::vector<Nota> notes (const Idea& idea, int num, int den, double tempo, int nomes)
    {
        std::vector<Nota> r;
        const long long tt = ticksTemps (den);
        const auto& op = opcionsPatrons (idea.instrument)[(size_t) std::clamp (idea.instrument == Instr::Guitarra ? idea.patroGuitarra : idea.patroPiano, 0, 3)];
        const piano::Patro* pp = idea.instrument == Instr::Piano ? piano::patro (op.id) : nullptr;
        const guitarra::Patro* pg = idea.instrument == Instr::Guitarra ? guitarra::patro (op.id) : nullptr;
        (void) num;
        double altura = 66.0;   // on era la mà dreta de l'acord anterior
        long long ini = 0;
        for (int i = 0; i < (int) idea.acords.size(); ++i)
        {
            const auto& k = idea.acords[(size_t) i];
            const long long dur = std::max<long long> (1, std::llround (k.temps * (double) tt));
            std::vector<Nota> v;
            const auto a = harmonia::llegeixAcord (k.acord);
            if (! a.buit())
            {
                if (pp != nullptr)
                {
                    // la inversió més propera a l'acord d'abans (perquè els acords s'enllacin i no salti tot)
                    piano::Opcions o; o.tempo = tempo; o.llavor = (unsigned) i + 1;
                    double dmin = 1e9; int triada = 0;
                    for (int inv = -3; inv <= 3; ++inv)
                    {
                        o.inversio = inv;
                        const double d = std::abs (alcadaDreta (piano::toca (a, *pp, std::min<long long> (dur, tt * 4), o)) - altura) + 0.3 * std::abs (inv);
                        if (d < dmin) { dmin = d; triada = inv; }
                    }
                    o.inversio = triada;
                    v = piano::toca (a, *pp, dur, o);
                    altura = 0.5 * altura + 0.5 * alcadaDreta (v);
                    altura = std::clamp (altura, 58.0, 74.0);
                }
                else if (pg != nullptr)
                {
                    guitarra::Opcions o; o.tempo = tempo; o.variacio = 0.15; o.llavor = (unsigned) i + 1; o.vel = 92;
                    v = guitarra::rasgueja (guitarra::notes (a), *pg, dur, o);
                }
            }
            if (nomes < 0 || nomes == i)
                for (auto n : v)
                {
                    if (n.ini >= dur) continue;
                    n.dur = std::max<long long> (1, std::min (n.dur, dur - n.ini + (pg != nullptr ? tt / 2 : 0)));
                    n.ini += nomes < 0 ? ini : 0;
                    r.push_back (n);
                }
            ini += dur;
        }
        std::stable_sort (r.begin(), r.end(), [] (const Nota& x, const Nota& y) { return x.ini < y.ini; });
        return r;
    }

    std::shared_ptr<const Programa> programa (std::vector<Nota> n, long long llargada, bool bucle, double tempo)
    {
        auto p = std::make_shared<Programa>();
        std::stable_sort (n.begin(), n.end(), [] (const Nota& x, const Nota& y) { return x.ini < y.ini; });
        p->notes = std::move (n);
        p->llargada = std::max<long long> (1, llargada);
        p->bucle = bucle;
        p->tempo = std::clamp (tempo, 20.0, 400.0);
        return p;
    }

    // ---------------------------------------------------------------- reproductor
    void Reproductor::prepara (double f, int mb)
    {
        std::lock_guard<std::mutex> l (mutex);
        fs = f > 0 ? f : 48000.0;
        maxBloc = std::max (64, mb);
        fsAudio.store (fs);
        tmpL.assign ((size_t) maxBloc, 0.0f);
        tmpR.assign ((size_t) maxBloc, 0.0f);
        for (auto& i : instrumentsVius) if (i != nullptr) i->prepara (fs, maxBloc);
    }

    void Reproductor::posaInstrument (std::shared_ptr<Instrument> i)
    {
        std::lock_guard<std::mutex> l (mutex);
        if (i != nullptr) i->prepara (fs, maxBloc);
        iInstr = (iInstr + 1) % (int) instrumentsVius.size();
        instrumentsVius[(size_t) iInstr] = i;
        instrNou.store (i.get(), std::memory_order_release);
    }

    void Reproductor::toca (std::shared_ptr<const Programa> p, bool desDelPrincipi, double desDe)
    {
        std::lock_guard<std::mutex> l (mutex);
        iProg = (iProg + 1) % (int) programesVius.size();
        programesVius[(size_t) iProg] = p;
        reinicia.store (desDelPrincipi, std::memory_order_relaxed);
        desDeNou.store (std::max (0.0, desDe), std::memory_order_relaxed);
        parar.store (false, std::memory_order_relaxed);
        progNou.store (p.get(), std::memory_order_relaxed);
        genProg.fetch_add (1, std::memory_order_release);
        if (p != nullptr) { actiu.store (true); bucleAra.store (p->bucle); }
    }

    void Reproductor::atura()
    {
        parar.store (true, std::memory_order_release);
    }

    void Reproductor::apagaTotes (int& numEv, int offset) noexcept
    {
        for (auto& s : sonen)
            if (s.to >= 0)
            {
                if (numEv < (int) events.size()) events[(size_t) numEv++] = { offset, 0x80, (std::uint8_t) s.to, 0 };
                s.to = -1;
            }
    }

    void Reproductor::processa (float* L, float* R, int n) noexcept
    {
        if (n <= 0) return;
        if (auto* ni = instrNou.load (std::memory_order_acquire); ni != instr)
        {
            if (instr != nullptr) instr->apagaTot();
            for (auto& s : sonen) s.to = -1;
            instr = ni;
        }
        int numEv = 0;
        if (const int g = genProg.load (std::memory_order_acquire); g != genVista)
        {
            genVista = g;
            prog = progNou.load (std::memory_order_relaxed);
            if (reinicia.load (std::memory_order_relaxed) || prog == nullptr) { apagaTotes (numEv, 0); pos = prog != nullptr ? std::min (desDeNou.load (std::memory_order_relaxed), (double) prog->llargada) : 0.0; }
            else if (prog->llargada > 0 && pos >= (double) prog->llargada) pos = prog->bucle ? std::fmod (pos, (double) prog->llargada) : (double) prog->llargada;
        }
        if (parar.exchange (false, std::memory_order_acq_rel))
        {
            apagaTotes (numEv, 0);
            prog = nullptr;
            actiu.store (false, std::memory_order_relaxed);
        }

        const double f = fsAudio.load (std::memory_order_relaxed);
        if (prog != nullptr && instr != nullptr)
        {
            const double mostresTick = f * 60.0 / (prog->tempo * 960.0);
            const double llarg = (double) prog->llargada;
            double ara = pos;
            int fet = 0;
            // el bloc pot travessar el final (i tornar a començar si és en bucle)
            while (fet < n && prog != nullptr)
            {
                const double queden = (double) (n - fet) / mostresTick;   // ticks que falten del bloc
                const double fi = std::min (ara + queden, llarg);
                for (auto& nt : prog->notes)
                {
                    if ((double) nt.ini < ara) continue;
                    if ((double) nt.ini >= fi) break;
                    const int off = std::clamp (fet + (int) std::floor (((double) nt.ini - ara) * mostresTick + 1e-6), 0, n - 1);
                    // la mateixa nota sonant: s'apaga abans
                    for (auto& s : sonen) if (s.to == nt.to) { if (numEv < (int) events.size()) events[(size_t) numEv++] = { off, 0x80, (std::uint8_t) s.to, 0 }; s.to = -1; }
                    for (auto& s : sonen)
                        if (s.to < 0)
                        {
                            s.to = nt.to;
                            s.queden = (double) off + (double) nt.dur * mostresTick;
                            break;
                        }
                    if (numEv < (int) events.size())
                        events[(size_t) numEv++] = { off, 0x90, (std::uint8_t) std::clamp (nt.to, 0, 127), (std::uint8_t) std::clamp (nt.vel, 1, 127) };
                }
                const int usades = (int) std::ceil ((fi - ara) * mostresTick - 1e-9);
                fet += std::max (0, std::min (usades, n - fet));
                ara = fi;
                if (ara >= llarg - 1e-9)
                {
                    if (prog->bucle) ara = 0.0;
                    else { prog = nullptr; break; }
                }
                if (usades <= 0 && ara > 0.0) break;
            }
            pos = ara;
            if (prog == nullptr)
            {
                // s'ha acabat: les que queden sonen fins al seu final
                bool cap = true;
                for (auto& s : sonen) if (s.to >= 0) cap = false;
                if (cap) actiu.store (false, std::memory_order_relaxed);
            }
        }
        // les que s'acaben en aquest bloc
        bool encara = false;
        for (auto& s : sonen)
        {
            if (s.to < 0) continue;
            if (s.queden < (double) n)
            {
                if (numEv < (int) events.size()) events[(size_t) numEv++] = { std::clamp ((int) s.queden, 0, n - 1), 0x80, (std::uint8_t) s.to, 0 };
                s.to = -1;
            }
            else { s.queden -= (double) n; encara = true; }
        }
        if (prog == nullptr && ! encara) actiu.store (false, std::memory_order_relaxed);
        posAra.store (prog != nullptr ? pos : 0.0, std::memory_order_relaxed);

        if (instr == nullptr) return;
        if (numEv == 0 && ! fentSo && ! instr->fentSo()) return;
        std::stable_sort (events.begin(), events.begin() + numEv, [] (const EventInstrument& a, const EventInstrument& b) { return a.offset < b.offset; });
        for (int k = 0; k < n; k += maxBloc)
        {
            const int m = std::min (maxBloc, n - k);
            // els esdeveniments d'aquest tros
            int a = 0;
            while (a < numEv && events[(size_t) a].offset < k) ++a;
            int b = a;
            while (b < numEv && events[(size_t) b].offset < k + m) { events[(size_t) b].offset -= k; ++b; }
            if ((int) tmpL.size() < m) return;   // no preparat
            instr->processa (events.data() + a, b - a, tmpL.data(), tmpR.data(), m);
            for (int i = 0; i < m; ++i)
            {
                if (L != nullptr) L[k + i] += guany * tmpL[(size_t) i];
                if (R != nullptr) R[k + i] += guany * tmpR[(size_t) i];
            }
        }
        fentSo = instr->fentSo();
    }
}
