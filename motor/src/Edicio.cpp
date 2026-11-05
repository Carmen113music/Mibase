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

    double enganxa (const Projecte& p, double t, Snap snap)
    {
        if (snap == Snap::Lliure)
            return t;
        const double pas = snap == Snap::Compas ? segonsPerCompas (p) : segonsPerTemps (p);
        return std::round (t / pas) * pas;
    }

    std::string textPosicio (const Projecte& p, double t, bool ambMs)
    {
        const double pt = segonsPerTemps (p);
        const double totalTemps = std::floor (t / pt + 1e-9);
        const int compas = (int) std::floor (totalTemps / p.compasNum);
        const int temps = (int) (totalTemps - (double) compas * p.compasNum) + 1;

        char text[64];
        if (ambMs)
        {
            const double resta = (t - totalTemps * pt) * 1000.0;
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
        p.pistes.push_back (t);
        return p.pistes.back();
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
        c.midi = p.pistes[(size_t) pista].tipus == "midi";   // a una pista MIDI, el llapis fa un clip MIDI buit
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
        if (pista >= 0 && pista < (int) p.pistes.size())
            p.pistes.erase (p.pistes.begin() + pista);
    }

    void mouPista (Projecte& p, int de, int a)
    {
        const int n = (int) p.pistes.size();
        if (de < 0 || de >= n || a < 0 || a >= n || de == a)
            return;
        auto t = p.pistes[(size_t) de];
        p.pistes.erase (p.pistes.begin() + de);
        p.pistes.insert (p.pistes.begin() + a, t);
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
        return (int) std::floor (std::max (0.0, t) / segonsPerCompas (p));
    }

    void marcaBucle (Projecte& p, int a, int b)
    {
        p.bucle.marcat = true;
        p.bucle.actiu = true;
        p.bucle.de = std::max (0, std::min (a, b));
        p.bucle.a = std::max (a, b) + 1;
    }

    double iniciBucle (const Projecte& p) { return p.bucle.de * segonsPerCompas (p); }
    double fiBucle (const Projecte& p)    { return p.bucle.a * segonsPerCompas (p); }

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
