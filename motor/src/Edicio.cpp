#include "mibase/Edicio.h"
#include "mibase/Midi.h"

#include <map>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace mibase::edicio
{
    double segonsPerTemps (const Projecte& p)
    {
        return 60.0 / std::max (20.0, p.tempo);
    }

    double segonsPerCompas (const Projecte& p)
    {
        return segonsPerTemps (p) * std::max (1, p.compasNum);
    }

    //==========================================================================
    // 0.28: mapa de tempo
    bool teCanvisTempo (const Projecte& p)
    {
        for (auto& x : p.parts) if ((x.tempo > 0.0 || x.compasNum > 0) && x.a > x.de) return true;
        return false;
    }

    std::vector<TramTempo> mapaTempo (const Projecte& p)
    {
        TramTempo base;
        base.tempo = std::max (20.0, p.tempo);
        base.num = std::max (1, p.compasNum);
        base.den = std::max (1, p.compasDen);
        std::vector<TramTempo> v { base };
        if (! teCanvisTempo (p)) return v;
        struct Punt { double compas; int ordre; double tempo; int num, den; };
        std::vector<Punt> punts;
        for (auto& x : p.parts)
        {
            if (! ((x.tempo > 0.0 || x.compasNum > 0) && x.a > x.de)) continue;
            const double de = std::floor (x.de + 1e-9), a = std::ceil (x.a - 1e-9);   // els canvis, a compassos sencers
            punts.push_back ({ a, 0, base.tempo, base.num, base.den });                 // al final, el de la cançó
            punts.push_back ({ de, 1, x.tempo > 0.0 ? std::max (20.0, x.tempo) : base.tempo,
                               x.compasNum > 0 ? x.compasNum : base.num, x.compasNum > 0 && x.compasDen > 0 ? x.compasDen : base.den });
        }
        std::stable_sort (punts.begin(), punts.end(), [] (const Punt& a, const Punt& b) { return a.compas < b.compas || (a.compas == b.compas && a.ordre < b.ordre); });
        for (auto& pt : punts)
        {
            if (pt.compas < 0.0) continue;
            auto& darrer = v.back();
            const double dc = pt.compas - darrer.compas;
            if (dc < 1e-9) { if (v.size() > 1 || pt.compas <= 1e-9) { darrer.tempo = pt.tempo; darrer.num = pt.num; darrer.den = pt.den; } continue; }
            if (std::abs (pt.tempo - darrer.tempo) < 1e-9 && pt.num == darrer.num && pt.den == darrer.den) continue;
            TramTempo t;
            t.compas = pt.compas;
            t.segons = darrer.segons + dc * darrer.segonsPerCompas();
            t.tick = darrer.tick + dc * (double) ticksPerNegra * darrer.num;
            t.tempo = pt.tempo; t.num = pt.num; t.den = pt.den;
            v.push_back (t);
        }
        return v;
    }

    namespace
    {
        template <typename F> const TramTempo& busca (const std::vector<TramTempo>& v, F esDespres)
        {
            size_t i = 0;
            while (i + 1 < v.size() && ! esDespres (v[i + 1])) ++i;
            return v[i];
        }
    }

    double segonsDeCompas (const Projecte& p, double compas)
    {
        if (! teCanvisTempo (p)) return compas * segonsPerCompas (p);
        const auto v = mapaTempo (p);
        const auto& t = busca (v, [compas] (const TramTempo& x) { return x.compas > compas + 1e-9; });
        return t.segons + (compas - t.compas) * t.segonsPerCompas();
    }

    double compasDeSegons (const Projecte& p, double s)
    {
        if (! teCanvisTempo (p)) return s / segonsPerCompas (p);
        const auto v = mapaTempo (p);
        const auto& t = busca (v, [s] (const TramTempo& x) { return x.segons > s + 1e-9; });
        return t.compas + (s - t.segons) / t.segonsPerCompas();
    }

    TramTempo tramA (const Projecte& p, double s)
    {
        const auto v = mapaTempo (p);
        return busca (v, [s] (const TramTempo& x) { return x.segons > s + 1e-9; });
    }
    double segonsPerCompasA (const Projecte& p, double s) { return tramA (p, s).segonsPerCompas(); }
    double segonsPerTempsA (const Projecte& p, double s) { return tramA (p, s).segonsPerTemps(); }

    double enganxa (const Projecte& p, double t, Snap snap)
    {
        if (snap == Snap::Lliure)
            return t;
        if (snap == Snap::Compas) return segonsDeCompas (p, std::round (compasDeSegons (p, t)));
        const auto tr = tramA (p, t);
        const double pas = tr.segonsPerTemps();
        return tr.segons + std::round ((t - tr.segons) / pas) * pas;
    }

    std::string textPosicio (const Projecte& p, double t, bool ambMs)
    {
        const auto tr = tramA (p, t);
        const double pt = tr.segonsPerTemps();
        const int num = std::max (1, tr.num);
        const double totalTemps = std::floor ((t - tr.segons) / pt + 1e-9);
        const int compas = (int) std::floor (tr.compas + 1e-9) + (int) std::floor (totalTemps / num);
        const int temps = (int) (totalTemps - std::floor (totalTemps / num) * num) + 1;

        char text[64];
        if (ambMs)
        {
            const double resta = (t - tr.segons - totalTemps * pt) * 1000.0;
            if (resta >= 1.0)
            {
                std::snprintf (text, sizeof (text), "%d.%d +%d ms", numCompas (compas), temps, (int) std::lround (resta));
                return text;
            }
        }
        std::snprintf (text, sizeof (text), "%d.%d", numCompas (compas), temps);
        return text;
    }

    Presa* presa (Projecte& p, const std::string& id)
    {
        for (auto& s : p.preses)
            if (s.id == id)
                return &s;
        return nullptr;
    }

    const Presa* presa (const Projecte& p, const std::string& id)
    {
        for (auto& s : p.preses)
            if (s.id == id)
                return &s;
        return nullptr;
    }

    bool banderaVisible (const Projecte& p, const Clip& c)
    {
        const auto* s = presa (p, c.presa);
        return s != nullptr && s->primer >= 0.0 && s->primer >= c.inici && s->primer <= c.fi;
    }

    double puntSnap (const Projecte& p, const Clip& c)
    {
        if (banderaVisible (p, c))
            return c.pos + presa (p, c.presa)->primer;   // banderaVisible ja comprova que la presa existeix
        return c.iniciVisible();
    }

    std::string idNou (Projecte& p, char prefix)
    {
        return std::string (1, prefix) + std::to_string (++p.comptador);
    }

    Pista& afegeixPista (Projecte& p, const std::string& nom, const std::string& tipus)
    {
        Pista t;
        t.id = idNou (p, 't');
        t.nom = nom;
        t.tipus = tipus;
        t.color = colorLliure (p);
        // 0.34: els grups i retorns sempre van a baix de tot: les pistes noves, abans del primer bus
        const auto lloc = std::find_if (p.pistes.begin(), p.pistes.end(), [] (const Pista& q) { return esBus (q); });
        return *p.pistes.insert (lloc, t);
    }

    int resAbansDelZero (Projecte& p)
    {
        int n = 0;
        for (auto& t : p.pistes)
            for (auto& c : t.clips)
                if (c.iniciVisible() < 0.0) { c.pos -= c.iniciVisible(); ++n; }
        return n;
    }

    void busosABaix (Projecte& p)
    {
        std::stable_partition (p.pistes.begin(), p.pistes.end(), [] (const Pista& q) { return ! esBus (q); });
    }

    Presa& afegeixPresa (Projecte& p, const std::string& fitxer, const std::string& nom, double durada)
    {
        Presa s;
        s.id = idNou (p, 's');
        s.fitxer = fitxer;
        s.nom = nom;
        s.durada = std::max (0.0, durada);
        p.preses.push_back (s);
        return p.preses.back();
    }

    Clip& afegeixClip (Projecte& p, int pista, const std::string& idPresa, double on)
    {
        Clip c;
        c.presa = idPresa;
        const auto* s = presa (p, idPresa);
        c.inici = 0.0;
        c.fi = s != nullptr ? std::max (longitudMinima, s->durada) : 1.0;
        c.pos = on;
        auto& clips = p.pistes[(size_t) pista].clips;
        clips.push_back (c);
        return clips.back();
    }

    int afegeixGravacio (Projecte& p, int pista, const std::string& fitxer, const std::string& nom, double durada,
                         double iniciFitxer, double cursor, double latencia)
    {
        if (pista < 0 || pista >= (int) p.pistes.size())
            return -1;
        const double pos = iniciFitxer - std::max (0.0, latencia);
        const double inici = std::max (0.0, cursor - pos);
        if (durada - inici < longitudMinima)
            return -1;

        auto& s = afegeixPresa (p, fitxer, nom, durada);
        s.primer = inici;
        Clip c;
        c.presa = s.id;
        c.pos = pos;
        c.inici = inici;
        c.fi = durada;
        auto& clips = p.pistes[(size_t) pista].clips;
        clips.push_back (c);
        return (int) clips.size() - 1;
    }

    int colorLliure (const Projecte& p)
    {
        int usos[colorsPista] = {};
        for (auto& t : p.pistes)
            if (t.color >= 0 && t.color < colorsPista) ++usos[t.color];
        int millor = 0;
        for (int i = 1; i < colorsPista; ++i)
            if (usos[i] < usos[millor]) millor = i;
        // En empat, el següent a l'última pista (perquè les pistes seguides tinguin colors diferents).
        if (! p.pistes.empty())
        {
            const int seguent = (p.pistes.back().color + 1) % colorsPista;
            if (usos[seguent] == usos[millor]) millor = seguent;
        }
        return millor;
    }

    int creaFinestra (Projecte& p, int pista, double de, double a)
    {
        if (pista < 0 || pista >= (int) p.pistes.size()) return -1;
        Clip c;
        c.pos = std::max (0.0, std::min (de, a));
        c.inici = 0.0;
        c.fi = std::max (longitudMinima, std::abs (a - de));
        c.midi = esMidi (p.pistes[(size_t) pista]);   // a una pista MIDI, el llapis fa un clip MIDI buit
        auto& clips = p.pistes[(size_t) pista].clips;
        clips.push_back (c);
        return (int) clips.size() - 1;
    }

    bool talla (Projecte& p, int pista, int clip, double t)
    {
        if (pista < 0 || pista >= (int) p.pistes.size()) return false;
        auto& clips = p.pistes[(size_t) pista].clips;
        if (clip < 0 || clip >= (int) clips.size()) return false;

        auto a = clips[(size_t) clip];
        if (t <= a.iniciVisible() + longitudMinima || t >= a.fiVisible() - longitudMinima) return false;

        auto b = a;
        a.fi = t - a.pos;
        b.inici = t - b.pos;
        a.fadeOut = 0.0;
        b.fadeIn = 0.0;
        a.fadeIn = std::min (a.fadeIn, a.longitud());
        b.fadeOut = std::min (b.fadeOut, b.longitud());
        clips[(size_t) clip] = a;
        clips.insert (clips.begin() + clip + 1, b);
        return true;
    }

    bool crossfade (Projecte& p, int pista, double t0, double t1)
    {
        if (pista < 0 || pista >= (int) p.pistes.size() || t1 - t0 < 0.0005) return false;
        auto& cl = p.pistes[(size_t) pista].clips;
        auto toca = [&] (const Clip& c) { return ! c.midi && ! c.presa.empty() && c.iniciVisible() < t1 && c.fiVisible() > t0; };
        int u = -1, l = -1;
        for (int i = (int) cl.size() - 1; i >= 0; --i)
            if (toca (cl[(size_t) i])) { if (u < 0) u = i; else { l = i; break; } }
        if (u < 0 || l < 0) return false;
        const double len = t1 - t0;
        const auto& U = cl[(size_t) u];
        const auto& L = cl[(size_t) l];
        const bool entra = U.iniciVisible() > L.iniciVisible() + 1e-9
                        && (U.fiVisible() >= L.fiVisible() - 1e-9 || std::abs (t0 - U.iniciVisible()) <= std::abs (t1 - U.fiVisible()));
        if (entra)
        {
            // El tros de sota que hi ha després només es conserva (partint) si torna a sonar quan el de dalt s'acaba.
            if (L.fiVisible() > U.fiVisible() + longitudMinima && talla (p, pista, l, t1) && u > l) ++u;
            auto& lo = cl[(size_t) l];
            lo.fi = t1 - lo.pos;
            lo.fadeOut = std::min (len, lo.longitud());
            auto& up = cl[(size_t) u];
            up.inici = t0 - up.pos;
            up.fadeIn = std::min (len, up.longitud());
        }
        else
        {
            int peca = l;
            if (L.iniciVisible() < U.iniciVisible() - longitudMinima && talla (p, pista, l, t0)) { peca = l + 1; if (u > l) ++u; }
            auto& lo = cl[(size_t) peca];
            lo.inici = t0 - lo.pos;
            lo.fadeIn = std::min (len, lo.longitud());
            auto& up = cl[(size_t) u];
            up.fi = t1 - up.pos;
            up.fadeOut = std::min (len, up.longitud());
        }
        return true;
    }

    namespace
    {
        /** Retalla la finestra del clip a [a, b) (absolut). Retorna false si no en queda res. */
        bool retallaA (Clip& c, double a, double b)
        {
            const double ini = std::max (c.inici, a - c.pos), fi = std::min (c.fi, b - c.pos);
            if (fi - ini < longitudMinima) return false;
            if (ini > c.inici + 1e-12) c.fadeIn = 0.0;
            if (fi < c.fi - 1e-12) c.fadeOut = 0.0;
            c.inici = ini;
            c.fi = fi;
            c.fadeIn = std::min (c.fadeIn, c.longitud());
            c.fadeOut = std::min (c.fadeOut, c.longitud() - c.fadeIn);
            return true;
        }

        std::map<int, std::vector<int>> perPista (const std::vector<RefClip>& triats)
        {
            std::map<int, std::vector<int>> m;
            for (auto& r : triats) m[r.pista].push_back (r.clip);
            return m;
        }
    }

    Retall copiaClips (const Projecte& p, const std::vector<RefClip>& triats)
    {
        Retall r;
        double a = 1e300, b = -1e300;
        for (auto& t : triats)
        {
            if (t.pista < 0 || t.pista >= (int) p.pistes.size()) continue;
            const auto& clips = p.pistes[(size_t) t.pista].clips;
            if (t.clip < 0 || t.clip >= (int) clips.size()) continue;
            r.clips.push_back ({ t.pista, clips[(size_t) t.clip] });
            a = std::min (a, clips[(size_t) t.clip].iniciVisible());
            b = std::max (b, clips[(size_t) t.clip].fiVisible());
        }
        if (r.clips.empty()) return {};
        // L'ordre de la pista es manté (el de dalt continua a dalt).
        std::stable_sort (r.clips.begin(), r.clips.end(), [] (auto& x, auto& y) { return x.first < y.first; });
        for (auto& c : r.clips) c.second.pos -= a;
        r.durada = b - a;
        return r;
    }

    Retall copiaRang (const Projecte& p, int pistaDe, int pistaFins, double t0, double t1)
    {
        Retall r;
        if (t1 <= t0) return r;
        r.durada = t1 - t0;
        for (int i = std::max (0, pistaDe); i <= std::min (pistaFins, (int) p.pistes.size() - 1); ++i)
            for (auto c : p.pistes[(size_t) i].clips)
                if (retallaA (c, t0, t1))
                {
                    c.pos -= t0;
                    r.clips.push_back ({ i, c });
                }
        if (r.clips.empty()) r.durada = 0.0;
        return r;
    }

    void buidaRang (Projecte& p, int pista, double t0, double t1)
    {
        if (pista < 0 || pista >= (int) p.pistes.size() || t1 <= t0) return;
        auto& clips = p.pistes[(size_t) pista].clips;
        std::vector<Clip> nous;
        for (auto& c : clips)
        {
            if (c.fiVisible() <= t0 || c.iniciVisible() >= t1) { nous.push_back (c); continue; }
            auto esquerra = c, dreta = c;
            if (retallaA (esquerra, c.iniciVisible(), t0)) nous.push_back (esquerra);
            if (retallaA (dreta, t1, c.fiVisible())) nous.push_back (dreta);
        }
        clips = std::move (nous);
    }

    void insereixEspai (Projecte& p, double t, double durada)
    {
        if (durada <= 0.0) return;
        for (int i = 0; i < (int) p.pistes.size(); ++i)
        {
            auto& clips = p.pistes[(size_t) i].clips;
            for (int k = 0; k < (int) clips.size(); ++k)
                if (clips[(size_t) k].iniciVisible() < t - 1e-9 && clips[(size_t) k].fiVisible() > t + 1e-9)
                    talla (p, i, k, t);   // el tros de la dreta queda just després i es mourà
            for (auto& c : clips)
                if (c.iniciVisible() >= t - 1e-9)
                    c.pos += durada;
        }
        if (p.bucle.marcat)
        {
            const double spc = segonsPerCompas (p);
            if (iniciBucle (p) >= t - 1e-9)
            {
                const int n = (int) std::lround (durada / spc);
                if (std::abs (n * spc - durada) < 1e-6) { p.bucle.de += n; p.bucle.a += n; }
            }
        }
    }

    std::vector<RefClip> enganxaRetall (Projecte& p, const Retall& r, double on, bool insereix)
    {
        std::vector<RefClip> nous;
        if (r.buit()) return nous;
        auto potAnar = [&p] (int pista, const Clip& c)
        {
            return pista >= 0 && pista < (int) p.pistes.size() && midi::compatible (p.pistes[(size_t) pista], c);
        };
        if (insereix) insereixEspai (p, on, r.durada);
        else
            for (auto& [pista, c] : r.clips)
                if (potAnar (pista, c))
                    buidaRang (p, pista, on + c.iniciVisible(), on + c.fiVisible());
        for (auto& [pista, c] : r.clips)
        {
            if (! potAnar (pista, c)) continue;   // un clip MIDI no va a una pista d'àudio (ni al revés)
            auto c2 = c;
            c2.pos += on;
            auto& clips = p.pistes[(size_t) pista].clips;
            clips.push_back (c2);
            nous.push_back ({ pista, (int) clips.size() - 1 });
        }
        return nous;
    }

    std::vector<RefClip> duplicaTriats (Projecte& p, const std::vector<RefClip>& triats)
    {
        std::vector<RefClip> nous;
        for (auto& [pista, idx] : perPista (triats))
            for (int k : duplica (p, pista, idx))
                nous.push_back ({ pista, k });
        return nous;
    }

    void treuTriats (Projecte& p, const std::vector<RefClip>& triats)
    {
        for (auto& [pista, idx] : perPista (triats))
            treuClips (p, pista, idx);
    }

    std::vector<RefClip> mouDePista (Projecte& p, const std::vector<RefClip>& triats, int d)
    {
        const int n = (int) p.pistes.size();
        int minP = n, maxP = -1;
        for (auto& r : triats) { minP = std::min (minP, r.pista); maxP = std::max (maxP, r.pista); }
        if (maxP < 0) return triats;
        d = std::clamp (d, -minP, n - 1 - maxP);
        // Només on tots hi poden anar (MIDI amb MIDI, àudio amb àudio): si no, tan lluny com es pugui.
        auto caben = [&] (int dd)
        {
            for (auto& r : triats)
            {
                if (r.pista < 0 || r.pista >= n) continue;
                const auto& clips = p.pistes[(size_t) r.pista].clips;
                if (r.clip < 0 || r.clip >= (int) clips.size()) continue;
                if (! midi::compatible (p.pistes[(size_t) (r.pista + dd)], clips[(size_t) r.clip])) return false;
            }
            return true;
        };
        while (d != 0 && ! caben (d)) d += d > 0 ? -1 : 1;
        if (d == 0) return triats;

        std::vector<std::pair<int, Clip>> moguts;
        for (auto& [pista, idx] : perPista (triats))
            for (int k : idx)
                if (k >= 0 && k < (int) p.pistes[(size_t) pista].clips.size())
                    moguts.push_back ({ pista + d, p.pistes[(size_t) pista].clips[(size_t) k] });
        treuTriats (p, triats);
        std::vector<RefClip> nous;
        for (auto& [pista, c] : moguts)
        {
            auto& clips = p.pistes[(size_t) pista].clips;
            clips.push_back (c);
            nous.push_back ({ pista, (int) clips.size() - 1 });
        }
        return nous;
    }

    ResultatCola enganxaClips (Projecte& p, int pista, int ia, int ib)
    {
        ResultatCola r;
        if (pista < 0 || pista >= (int) p.pistes.size() || ia == ib) return r;
        auto& clips = p.pistes[(size_t) pista].clips;
        if (ia < 0 || ib < 0 || ia >= (int) clips.size() || ib >= (int) clips.size()) return r;

        if (clips[(size_t) ib].iniciVisible() < clips[(size_t) ia].iniciVisible()) std::swap (ia, ib);
        const auto a = clips[(size_t) ia];
        const auto b = clips[(size_t) ib];
        const double ini = std::min (a.iniciVisible(), b.iniciVisible());
        const double fi = std::max (a.fiVisible(), b.fiVisible());

        Clip resultat;
        bool simple = false;

        if (a.midi || b.midi)
        {
            if (a.midi != b.midi) return r;
            resultat = midi::ajunta (p, a, b);
            simple = true;
        }
        else if (a.presa.empty() && b.presa.empty())
        {
            resultat = a;
            resultat.pos = ini; resultat.inici = 0.0; resultat.fi = fi - ini;
            simple = true;
        }
        else if (a.presa.empty() != b.presa.empty())
        {
            // Una finestra buida i un àudio: la finestra de l'àudio s'allarga (la part nova és silenci).
            resultat = a.presa.empty() ? b : a;
            const double inici = ini - resultat.pos, final = fi - resultat.pos;
            // Només es pot si no es destapa àudio amagat on hi havia la finestra buida (ha de quedar silenci).
            const auto* s = presa (p, resultat.presa);
            const bool descobreix = s != nullptr && ((inici < resultat.inici && resultat.inici > 0.0)
                                                    || (final > resultat.fi && resultat.fi < s->durada));
            if (! descobreix)
            {
                resultat.inici = inici; resultat.fi = final;
                if (a.presa.empty()) resultat.fadeIn = 0.0; else resultat.fadeOut = 0.0;
                simple = true;
            }
        }
        else if (a.presa == b.presa && std::abs (a.pos - b.pos) < 1e-6 && std::abs (a.guanyDb - b.guanyDb) < 1e-6
                 && a.mut == b.mut && std::abs (a.fiVisible() - b.iniciVisible()) < 0.001
                 && a.fadeOut <= 0.0 && b.fadeIn <= 0.0)
        {
            resultat = a;
            resultat.fi = b.fi;
            resultat.fadeOut = b.fadeOut;
            simple = true;
        }

        if (! simple)
        {
            r.calConsolidar = true;
            return r;
        }

        clips[(size_t) ia] = resultat;
        clips.erase (clips.begin() + ib);
        r.fet = true;
        r.clip = ib < ia ? ia - 1 : ia;
        return r;
    }

    void esborraPista (Projecte& p, int pista)
    {
        if (pista < 0 || pista >= (int) p.pistes.size()) return;
        const std::string id = p.pistes[(size_t) pista].id;
        p.pistes.erase (p.pistes.begin() + pista);
        // 0.33: el que anava a aquest bus passa a la sortida, i els enviaments que hi apuntaven s'esborren.
        for (auto& t : p.pistes)
        {
            if (t.sortida == id) t.sortida.clear();
            std::erase_if (t.enviaments, [&] (const Enviament& e) { return e.desti == id; });
        }
    }

    Pista& afegeixBus (Projecte& p, const std::string& nom, bool fx)
    {
        Pista t;
        t.id = idNou (p, 't');
        t.nom = nom;
        t.tipus = fx ? "fx" : "bus";
        t.canals = 2;
        p.pistes.push_back (t);
        return p.pistes.back();
    }

    int pistaPerId (const Projecte& p, const std::string& id) noexcept
    {
        if (! id.empty())
            for (int i = 0; i < (int) p.pistes.size(); ++i)
                if (p.pistes[(size_t) i].id == id) return i;
        return -1;
    }

    bool potConnectar (const Projecte& p, const std::string& origen, const std::string& desti)
    {
        const int d = pistaPerId (p, desti);
        if (d < 0 || ! esBus (p.pistes[(size_t) d]) || origen == desti || pistaPerId (p, origen) < 0) return false;
        // Des de «desti», seguint sortides i enviaments, no s'ha de poder tornar a «origen».
        std::vector<char> vist (p.pistes.size(), 0);
        std::vector<int> cua { d };
        vist[(size_t) d] = 1;
        while (! cua.empty())
        {
            const auto& t = p.pistes[(size_t) cua.back()];
            cua.pop_back();
            if (t.id == origen) return false;
            auto segueix = [&] (const std::string& id)
            {
                const int j = pistaPerId (p, id);
                if (j >= 0 && ! vist[(size_t) j]) { vist[(size_t) j] = 1; cua.push_back (j); }
            };
            segueix (t.sortida);
            for (auto& e : t.enviaments) segueix (e.desti);
        }
        return true;
    }

    bool posaSortida (Projecte& p, int pista, const std::string& bus)
    {
        if (pista < 0 || pista >= (int) p.pistes.size()) return false;
        auto& t = p.pistes[(size_t) pista];
        if (! bus.empty() && ! potConnectar (p, t.id, bus)) return false;
        t.sortida = bus;
        return true;
    }

    int afegeixEnviament (Projecte& p, int pista, const std::string& bus)
    {
        if (pista < 0 || pista >= (int) p.pistes.size()) return -1;
        if (! potConnectar (p, p.pistes[(size_t) pista].id, bus)) return -1;
        // 0.34: els enviaments només van als retorns d'efectes; als grups s'hi va amb la sortida
        if (const int d = pistaPerId (p, bus); d < 0 || p.pistes[(size_t) d].tipus != "fx") return -1;
        Enviament e;
        e.id = idNou (p, 'v');
        e.desti = bus;
        auto& t = p.pistes[(size_t) pista];
        t.enviaments.push_back (e);
        return (int) t.enviaments.size() - 1;
    }

    bool destiEnviament (Projecte& p, int pista, int quin, const std::string& bus)
    {
        if (pista < 0 || pista >= (int) p.pistes.size()) return false;
        auto& t = p.pistes[(size_t) pista];
        if (quin < 0 || quin >= (int) t.enviaments.size() || ! potConnectar (p, t.id, bus)) return false;
        if (const int d = pistaPerId (p, bus); d < 0 || p.pistes[(size_t) d].tipus != "fx") return false;   // 0.34
        t.enviaments[(size_t) quin].desti = bus;
        return true;
    }

    void treuEnviament (Projecte& p, int pista, int quin)
    {
        if (pista < 0 || pista >= (int) p.pistes.size()) return;
        auto& v = p.pistes[(size_t) pista].enviaments;
        if (quin >= 0 && quin < (int) v.size()) v.erase (v.begin() + quin);
    }

    std::vector<int> estatSolo (const Projecte& p)
    {
        const int n = (int) p.pistes.size();
        std::vector<int> r ((size_t) n, 2);
        const bool hiHaSolo = std::any_of (p.pistes.begin(), p.pistes.end(), [] (const Pista& t) { return t.solo; });
        if (hiHaSolo)
        {
            std::vector<int> surt ((size_t) n, -1);
            std::vector<std::vector<int>> envia ((size_t) n);
            for (int i = 0; i < n; ++i)
            {
                const auto& t = p.pistes[(size_t) i];
                if (const int j = pistaPerId (p, t.sortida); j >= 0 && esBus (p.pistes[(size_t) j])) surt[(size_t) i] = j;
                for (auto& e : t.enviaments)
                    if (e.actiu && ! e.mut)
                        if (const int j = pistaPerId (p, e.desti); j >= 0 && esBus (p.pistes[(size_t) j])) envia[(size_t) i].push_back (j);
            }
            std::vector<char> font ((size_t) n, 0);   // busos que s'han de sentir: el que els alimenta ha de sonar
            for (int i = 0; i < n; ++i)
            {
                r[(size_t) i] = p.pistes[(size_t) i].solo ? 2 : 0;
                font[(size_t) i] = p.pistes[(size_t) i].solo ? 1 : 0;
            }
            // Cap amunt: el que alimenta un bus en solo.
            for (bool canvi = true; canvi;)
            {
                canvi = false;
                for (int c = 0; c < n; ++c)
                {
                    int vol = 0;
                    if (surt[(size_t) c] >= 0 && font[(size_t) surt[(size_t) c]]) vol = 2;
                    else
                        for (int e : envia[(size_t) c]) if (font[(size_t) e]) vol = 1;
                    if (vol == 0) continue;
                    if (r[(size_t) c] < vol) { r[(size_t) c] = vol; canvi = true; }
                    if (! font[(size_t) c]) { font[(size_t) c] = 1; canvi = true; }
                }
            }
            // Cap avall: els busos per on passa el que sona i els retorns on envia.
            for (bool canvi = true; canvi;)
            {
                canvi = false;
                for (int c = 0; c < n; ++c)
                {
                    if (r[(size_t) c] != 2) continue;
                    auto obre = [&] (int j) { if (j >= 0 && r[(size_t) j] < 2) { r[(size_t) j] = 2; canvi = true; } };
                    obre (surt[(size_t) c]);
                    for (int e : envia[(size_t) c]) obre (e);
                }
            }
        }
        for (int i = 0; i < n; ++i)
            if (p.pistes[(size_t) i].mut) r[(size_t) i] = 0;
        return r;
    }

    void mouPista (Projecte& p, int de, int a)
    {
        const int n = (int) p.pistes.size();
        if (de < 0 || de >= n || a < 0 || a >= n || de == a)
            return;
        // 0.34: les pistes no passen per sota dels busos ni els busos per sobre de les pistes
        const int normals = (int) std::count_if (p.pistes.begin(), p.pistes.end(), [] (const Pista& q) { return ! esBus (q); });
        if (esBus (p.pistes[(size_t) de])) a = std::max (a, normals);
        else a = std::min (a, normals - 1);
        if (a == de) return;
        auto t = p.pistes[(size_t) de];
        p.pistes.erase (p.pistes.begin() + de);
        p.pistes.insert (p.pistes.begin() + a, t);
    }

    std::string creaPart (Projecte& p, const std::string& nom, double de, double a, std::uint32_t rgb)
    {
        Part x;
        x.id = idNou (p, 'm');
        x.nom = nom;
        x.de = std::max (0.0, std::min (de, a));
        x.a = std::max (de, a);
        x.rgb = rgb;
        p.parts.push_back (x);
        return x.id;
    }

    Part* part (Projecte& p, const std::string& id)
    {
        for (auto& x : p.parts) if (x.id == id && ! id.empty()) return &x;
        return nullptr;
    }

    void esborraPart (Projecte& p, const std::string& id)
    {
        p.parts.erase (std::remove_if (p.parts.begin(), p.parts.end(), [&id] (const Part& x) { return x.id == id; }), p.parts.end());
    }

    int partA (const Projecte& p, double t)
    {
        const double c = compasDeSegons (p, t);
        for (int i = (int) p.parts.size() - 1; i >= 0; --i)
            if (c >= p.parts[(size_t) i].de && c < p.parts[(size_t) i].a) return i;
        return -1;
    }

    const Estructura* estructuraActiva (const Projecte& p)
    {
        for (auto& e : p.estructures)
            if (! p.estructuraActiva.empty() && e.id == p.estructuraActiva) return e.peces.empty() ? nullptr : &e;
        return nullptr;
    }

    Estructura* estructura (Projecte& p, const std::string& id)
    {
        for (auto& e : p.estructures) if (e.id == id && ! id.empty()) return &e;
        return nullptr;
    }

    std::string creaEstructura (Projecte& p, const std::string& nom)
    {
        Estructura e;
        e.id = idNou (p, 'e');
        e.nom = nom;
        p.estructures.push_back (e);
        return e.id;
    }

    Carpeta* carpeta (Projecte& p, const std::string& id)
    {
        for (auto& c : p.carpetes) if (c.id == id && ! id.empty()) return &c;
        return nullptr;
    }

    std::string creaCarpeta (Projecte& p, const std::string& nom, const std::vector<int>& pistes)
    {
        Carpeta c;
        c.id = idNou (p, 'f');
        c.nom = nom;
        p.carpetes.push_back (c);
        // Per ordre: la primera on és; la resta, seguides darrere seu.
        std::vector<std::string> ids;
        for (int i : pistes) if (i >= 0 && i < (int) p.pistes.size()) ids.push_back (p.pistes[(size_t) i].id);
        std::vector<std::pair<int, std::string>> ordre;
        for (auto& id : ids)
            for (int i = 0; i < (int) p.pistes.size(); ++i)
                if (p.pistes[(size_t) i].id == id) ordre.push_back ({ i, id });
        std::sort (ordre.begin(), ordre.end());
        for (auto& [i0, id] : ordre)
            for (int i = 0; i < (int) p.pistes.size(); ++i)
                if (p.pistes[(size_t) i].id == id) { posaACarpeta (p, i, c.id); break; }
        return c.id;
    }

    int posaACarpeta (Projecte& p, int pista, const std::string& id)
    {
        if (pista < 0 || pista >= (int) p.pistes.size()) return pista;
        if (! id.empty() && carpeta (p, id) == nullptr) return pista;
        auto& t = p.pistes[(size_t) pista];
        if (t.carpeta == id) return pista;
        t.carpeta = id;
        if (id.empty()) return pista;
        int ultima = -1;
        for (int i = 0; i < (int) p.pistes.size(); ++i)
            if (i != pista && p.pistes[(size_t) i].carpeta == id) ultima = i;
        if (ultima < 0) return pista;                      // la primera: es queda on és
        const int desti = ultima < pista ? ultima + 1 : ultima;
        mouPista (p, pista, desti);
        return desti;
    }

    void esborraCarpeta (Projecte& p, const std::string& id)
    {
        for (auto& t : p.pistes) if (t.carpeta == id) t.carpeta.clear();
        p.carpetes.erase (std::remove_if (p.carpetes.begin(), p.carpetes.end(), [&id] (const Carpeta& c) { return c.id == id; }),
                          p.carpetes.end());
    }

    std::vector<int> pistesDeCarpeta (const Projecte& p, const std::string& id)
    {
        std::vector<int> r;
        for (int i = 0; i < (int) p.pistes.size(); ++i) if (! id.empty() && p.pistes[(size_t) i].carpeta == id) r.push_back (i);
        return r;
    }

    std::vector<int> filesVisibles (const Projecte& p)
    {
        std::vector<int> r (p.pistes.size(), 1);
        for (auto& c : p.carpetes)
        {
            if (! c.plegada && ! c.amagada) continue;
            bool primera = true;
            for (int i : pistesDeCarpeta (p, c.id))
            {
                r[(size_t) i] = (c.amagada || ! primera) ? 0 : 2;
                primera = false;
            }
        }
        // 0.34: els busos també es veuen a la taula de treball (a baix de tot, després d'una ratlla)
        return r;
    }

    void triaGravacio (Projecte& p, int pista)
    {
        for (int i = 0; i < (int) p.pistes.size(); ++i)
            p.pistes[(size_t) i].grava = (i == pista) ? ! p.pistes[(size_t) i].grava : false;
    }

    std::string nomPistaLliure (const Projecte& p, const std::string& base)
    {
        auto existeix = [&] (const std::string& n)
        {
            return std::any_of (p.pistes.begin(), p.pistes.end(), [&] (const Pista& t) { return t.nom == n; });
        };
        if (! existeix (base))
            return base;
        for (int i = 2;; ++i)
            if (! existeix (base + " " + std::to_string (i)))
                return base + " " + std::to_string (i);
    }

    void mouClipA (Projecte& p, Clip& c, double desti)
    {
        c.pos += desti - puntSnap (p, c);
    }

    bool mouAmbBanderaFixa (Projecte& p, Clip& c, double posNova, double banderaAbs)
    {
        auto* s = presa (p, c.presa);
        if (s == nullptr)
            return false;
        const double primer = banderaAbs - posNova;
        if (primer < c.inici || primer > c.fi || primer < 0.0 || primer > s->durada)
            return false;
        c.pos = posNova;
        s->primer = primer;
        return true;
    }

    void retallaInici (Clip& c, double iniciAbs)
    {
        const double finestra = c.fi;
        c.inici = std::clamp (iniciAbs - c.pos, -allargaMaxim, finestra - longitudMinima);
        c.fadeIn = std::min (c.fadeIn, c.longitud() - c.fadeOut);
        c.fadeIn = std::max (0.0, c.fadeIn);
    }

    void retallaFi (Clip& c, double fiAbs)
    {
        c.fi = std::clamp (fiAbs - c.pos, c.inici + longitudMinima, allargaMaxim + 3600.0);
        c.fadeOut = std::max (0.0, std::min (c.fadeOut, c.longitud() - c.fadeIn));
    }

    void fadeIn (Clip& c, double fiFadeAbs)
    {
        c.fadeIn = std::clamp (fiFadeAbs - c.iniciVisible(), 0.0, std::max (0.0, c.longitud() - c.fadeOut));
    }

    void fadeOut (Clip& c, double iniciFadeAbs)
    {
        c.fadeOut = std::clamp (c.fiVisible() - iniciFadeAbs, 0.0, std::max (0.0, c.longitud() - c.fadeIn));
    }

    void guany (Clip& c, double db)
    {
        c.guanyDb = std::clamp (db, guanyMinDb, 0.0);
    }

    std::vector<int> duplica (Projecte& p, int pista, std::vector<int> clips)
    {
        std::vector<int> noves;
        if (pista < 0 || pista >= (int) p.pistes.size() || clips.empty())
            return noves;

        auto& llista = p.pistes[(size_t) pista].clips;
        std::sort (clips.begin(), clips.end());

        // El grup es copia sencer, just després del final del grup.
        double ini = 1e300, fi = -1e300;
        for (int i : clips)
        {
            ini = std::min (ini, llista[(size_t) i].iniciVisible());
            fi = std::max (fi, llista[(size_t) i].fiVisible());
        }
        const double salt = fi - ini;

        std::vector<Clip> copies;
        for (int i : clips)
        {
            auto c = llista[(size_t) i];
            c.pos += salt;
            copies.push_back (c);
        }
        for (auto& c : copies)
        {
            llista.push_back (c);
            noves.push_back ((int) llista.size() - 1);
        }
        return noves;
    }

    void treuClips (Projecte& p, int pista, std::vector<int> clips)
    {
        if (pista < 0 || pista >= (int) p.pistes.size())
            return;
        auto& llista = p.pistes[(size_t) pista].clips;
        std::sort (clips.rbegin(), clips.rend());
        for (int i : clips)
            if (i >= 0 && i < (int) llista.size())
                llista.erase (llista.begin() + i);
    }

    std::vector<int> passaAPista (Projecte& p, int de, std::vector<int> clips, int a)
    {
        std::vector<int> noves;
        if (de == a || a < 0 || a >= (int) p.pistes.size() || de < 0 || de >= (int) p.pistes.size())
            return clips;

        std::sort (clips.begin(), clips.end());
        for (int i : clips)
            if (! midi::compatible (p.pistes[(size_t) a], p.pistes[(size_t) de].clips[(size_t) i]))
                return clips;
        std::vector<Clip> moguts;
        for (int i : clips)
            moguts.push_back (p.pistes[(size_t) de].clips[(size_t) i]);
        treuClips (p, de, clips);
        for (auto& c : moguts)
        {
            p.pistes[(size_t) a].clips.push_back (c);
            noves.push_back ((int) p.pistes[(size_t) a].clips.size() - 1);
        }
        return noves;
    }

    void posaBandera (Projecte& p, const Clip& c, double abs)
    {
        if (auto* s = presa (p, c.presa))
            s->primer = abs < 0.0 ? -1.0 : std::clamp (abs - c.pos, 0.0, s->durada);
    }

    int compasA (const Projecte& p, double t)
    {
        return (int) std::floor (compasDeSegons (p, std::max (0.0, t)) + 1e-9);
    }

    void marcaBucle (Projecte& p, int a, int b)
    {
        p.bucle.marcat = true;
        p.bucle.actiu = true;
        p.bucle.de = std::max (0, std::min (a, b));
        p.bucle.a = std::max (a, b) + 1;
    }

    double iniciBucle (const Projecte& p) { return segonsDeCompas (p, p.bucle.de); }
    double fiBucle (const Projecte& p)    { return segonsDeCompas (p, p.bucle.a); }

    double final (const Projecte& p)
    {
        double f = 0.0;
        for (auto& t : p.pistes)
            for (auto& c : t.clips)
                f = std::max (f, c.fiVisible());
        return f;
    }

    void Desfes::abans (const Projecte& p)
    {
        enrere.push_back (p);
        if ((int) enrere.size() > maxim)
            enrere.erase (enrere.begin());
        endavant.clear();
    }

    bool Desfes::desfes (Projecte& p)
    {
        if (enrere.empty())
            return false;
        endavant.push_back (p);
        p = enrere.back();
        enrere.pop_back();
        return true;
    }

    bool Desfes::refes (Projecte& p)
    {
        if (endavant.empty())
            return false;
        enrere.push_back (p);
        p = endavant.back();
        endavant.pop_back();
        return true;
    }
}
