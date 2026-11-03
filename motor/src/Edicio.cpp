#include "mibase/Edicio.h"

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
            return c.pos + presa (p, c.presa)->primer;
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
