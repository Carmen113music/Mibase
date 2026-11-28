// 0.81: preses alternatives (takes), comping i gravació en bucle.
//
// Idea: les preses es guarden senceres a Pista::takes i no sonen. El que sona és la pista de sempre: els seus clips
// són finestres d'aquestes preses al mateix lloc (el «comp»). Així el motor de so, el dibuix dels clips, el desfer i el
// desar no canvien: un comp és una llista de clips normals, i una pista sense preses és exactament com abans.
#include "mibase/Edicio.h"

#include <algorithm>
#include <cmath>

namespace mibase::edicio
{
    namespace
    {
        bool esAudio (const Clip& c) noexcept { return ! c.presa.empty() && ! c.midi && ! c.esAcord(); }

        bool trepitja (const Clip& a, const Clip& b) noexcept
        {
            return std::min (a.fiVisible(), b.fiVisible()) - std::max (a.iniciVisible(), b.iniciVisible()) > longitudMinima;
        }

        double duradaPresa (const Projecte& p, const std::string& id)
        {
            const auto* s = presa (p, id);
            return s != nullptr ? s->durada : 0.0;
        }

        /** Treu de [a, b) els trossos tapats per [c, d). */
        void resta (std::vector<std::pair<double, double>>& v, double c, double d)
        {
            std::vector<std::pair<double, double>> r;
            for (auto [a, b] : v)
            {
                if (d <= a || c >= b) { r.push_back ({ a, b }); continue; }
                if (c > a) r.push_back ({ a, c });
                if (d < b) r.push_back ({ d, b });
            }
            v = std::move (r);
        }
    }

    bool esDeTake (const Clip& c, const Clip& take) noexcept
    {
        return esAudio (c) && c.presa == take.presa && std::abs (c.pos - take.pos) < 1.0e-6;
    }

    int takeDe (const Pista& t, const Clip& c) noexcept
    {
        for (int k = 0; k < (int) t.takes.size(); ++k)
            if (esDeTake (c, t.takes[(size_t) k])) return k;
        return -1;
    }

    std::vector<int> afegeixVoltes (Projecte& p, int pista, int clip, double iniBucle, double primeraVolta, double llargVolta,
                                    int voltes, double latencia)
    {
        std::vector<int> r;
        if (pista < 0 || pista >= (int) p.pistes.size()) return r;
        auto& clips = p.pistes[(size_t) pista].clips;
        if (clip < 0 || clip >= (int) clips.size()) return r;
        r.push_back (clip);
        if (voltes <= 0 || llargVolta <= longitudMinima || primeraVolta <= 0.0) return r;
        const double lat = std::max (0.0, latencia);
        const Clip base = clips[(size_t) clip];
        const double durada = duradaPresa (p, base.presa);
        // La primera volta acaba on el capçal va tornar a l'inici del bucle (el so arriba «lat» tard)
        {
            auto& c = clips[(size_t) clip];
            c.fi = std::min (c.fi, primeraVolta + lat);
            if (c.fi - c.inici < longitudMinima) c.fi = std::min (durada, c.inici + longitudMinima);
            c.fadeOut = 0.0;
        }
        for (int k = 1; k <= voltes; ++k)
        {
            const double ini = primeraVolta + (k - 1) * llargVolta + lat;
            const double fi = std::min (durada, ini + llargVolta);
            if (fi - ini < voltaMinima) break;   // s'ha aturat just després de tornar a començar
            Clip c = base;
            c.inici = ini;
            c.fi = fi;
            c.pos = iniBucle - ini;
            c.fadeIn = c.fadeOut = 0.0;
            clips.push_back (c);
            r.push_back ((int) clips.size() - 1);
        }
        return r;
    }

    bool organitzaTakes (Projecte& p, int pista, const std::vector<int>& nous)
    {
        if (pista < 0 || pista >= (int) p.pistes.size()) return false;
        auto& t = p.pistes[(size_t) pista];
        if (esMidi (t) || esBus (t) || esAcords (t)) return ! t.takes.empty();

        std::vector<Clip> nousClips;
        for (int i : nous)
            if (i >= 0 && i < (int) t.clips.size() && esAudio (t.clips[(size_t) i])) nousClips.push_back (t.clips[(size_t) i]);
        if (nousClips.empty()) return ! t.takes.empty();

        auto esNou = [&] (int i) { return std::find (nous.begin(), nous.end(), i) != nous.end(); };
        // Les gravacions d'abans que competeixen amb alguna de les noves (al mateix tros de la cançó)
        std::vector<Clip> velles;
        for (int i = 0; i < (int) t.clips.size(); ++i)
        {
            const auto& c = t.clips[(size_t) i];
            if (esNou (i) || ! esAudio (c) || c.mut) continue;
            for (const auto& n : nousClips)
                if (trepitja (c, n)) { velles.push_back (c); break; }
        }
        bool ambTake = false;
        for (const auto& n : nousClips)
            for (const auto& k : t.takes)
                if (trepitja (n, k)) ambTake = true;
        bool entreElles = false;   // en bucle: les voltes es trepitgen entre elles
        for (size_t a = 0; a < nousClips.size(); ++a)
            for (size_t b = a + 1; b < nousClips.size(); ++b)
                if (trepitja (nousClips[a], nousClips[b])) entreElles = true;
        if (velles.empty() && ! ambTake && ! entreElles) return ! t.takes.empty();   // una gravació sola: com sempre

        const bool primeres = t.takes.empty();
        for (const auto& c : velles)
            if (takeDe (t, c) < 0)
            {
                auto k = c;
                k.fadeIn = k.fadeOut = 0.0;
                t.takes.push_back (k);
            }
        for (const auto& n : nousClips)
            if (takeDe (t, n) < 0)
            {
                auto k = n;
                k.fadeIn = k.fadeOut = 0.0;
                t.takes.push_back (k);
            }
        if (primeres) t.takesObertes = true;

        // El comp: on hi ha gravació nova, sona la més nova (com abans, quan la de dalt tapava la de sota), però net:
        // sense clips amagats a sota i amb crossfades curts als canvis.
        std::vector<int> ordre (nous.begin(), nous.end());
        std::sort (ordre.rbegin(), ordre.rend());
        std::vector<Clip> perComp;
        for (int i : ordre)
            if (i >= 0 && i < (int) t.clips.size() && esAudio (t.clips[(size_t) i]))
            {
                perComp.insert (perComp.begin(), t.clips[(size_t) i]);
                t.clips.erase (t.clips.begin() + i);
            }
        for (const auto& n : perComp)
            if (const int k = takeDe (t, n); k >= 0)
                compTake (p, pista, k, n.iniciVisible(), n.fiVisible());
        return true;
    }

    bool compTake (Projecte& p, int pista, int take, double a, double b)
    {
        if (pista < 0 || pista >= (int) p.pistes.size()) return false;
        auto& t = p.pistes[(size_t) pista];
        if (take < 0 || take >= (int) t.takes.size()) return false;
        const Clip tk = t.takes[(size_t) take];
        if (a > b) std::swap (a, b);
        a = std::max (a, tk.iniciVisible());
        b = std::min (b, tk.fiVisible());
        if (b - a < longitudMinima) return false;

        const double xf = std::min (crossfadeComp, (b - a) / 4.0);
        // Els veïns es queden fins al mig del crossfade; la presa nova hi entra (i en surt) amb un fade curt.
        buidaRang (p, pista, a + xf / 2.0, b - xf / 2.0);
        const double durada = duradaPresa (p, tk.presa);
        Clip c = tk;
        c.mut = false;
        c.inici = std::max (0.0, a - xf / 2.0 - c.pos);
        c.fi = std::max (c.inici + longitudMinima, std::min (durada > 0.0 ? durada : 1.0e9, b + xf / 2.0 - c.pos));
        c.fadeIn = std::min (xf, c.longitud() / 4.0);
        c.fadeOut = std::min (xf, c.longitud() / 4.0);

        // Si toca un altre tros de la mateixa presa, s'hi ajunta (és el mateix àudio al mateix lloc)
        auto& clips = t.clips;
        for (int i = (int) clips.size() - 1; i >= 0; --i)
        {
            const auto& d = clips[(size_t) i];
            if (! esDeTake (d, tk) || d.mut) continue;
            if (d.fiVisible() < c.iniciVisible() - 1.0e-6 || d.iniciVisible() > c.fiVisible() + 1.0e-6) continue;
            if (d.inici < c.inici) { c.inici = d.inici; c.fadeIn = d.fadeIn; }
            if (d.fi > c.fi) { c.fi = d.fi; c.fadeOut = d.fadeOut; }
            clips.erase (clips.begin() + i);
        }
        clips.push_back (c);
        return true;
    }

    void treuDelComp (Projecte& p, int pista, double a, double b)
    {
        if (pista < 0 || pista >= (int) p.pistes.size()) return;
        if (a > b) std::swap (a, b);
        buidaRang (p, pista, a, b);
        for (auto& c : p.pistes[(size_t) pista].clips)
        {
            if (! esAudio (c)) continue;
            if (std::abs (c.fiVisible() - a) < 1.0e-6 && c.fadeOut <= 0.0) c.fadeOut = std::min (crossfadeComp, c.longitud() / 4.0);
            if (std::abs (c.iniciVisible() - b) < 1.0e-6 && c.fadeIn <= 0.0) c.fadeIn = std::min (crossfadeComp, c.longitud() / 4.0);
        }
    }

    bool compTakeSencera (Projecte& p, int pista, int take)
    {
        if (pista < 0 || pista >= (int) p.pistes.size()) return false;
        const auto& t = p.pistes[(size_t) pista];
        if (take < 0 || take >= (int) t.takes.size()) return false;
        const auto& k = t.takes[(size_t) take];
        return compTake (p, pista, take, k.iniciVisible(), k.fiVisible());
    }

    std::vector<std::vector<std::pair<double, double>>> zonesTakes (const Projecte& p, int pista)
    {
        std::vector<std::vector<std::pair<double, double>>> r;
        if (pista < 0 || pista >= (int) p.pistes.size()) return r;
        const auto& t = p.pistes[(size_t) pista];
        r.resize (t.takes.size());
        const auto& clips = t.clips;
        for (size_t i = 0; i < clips.size(); ++i)
        {
            const auto& c = clips[i];
            if (c.mut || ! esAudio (c)) continue;
            const int k = takeDe (t, c);
            if (k < 0) continue;
            // dins un crossfade tots dos sonen: la vora es posa al mig, on es creuen
            std::vector<std::pair<double, double>> v { { c.iniciVisible() + std::min (c.fadeIn, c.longitud()) / 2.0,
                                                         c.fiVisible() - std::min (c.fadeOut, c.longitud()) / 2.0 } };
            for (size_t j = i + 1; j < clips.size(); ++j)   // els de més amunt de la llista el tapen
                if (! clips[j].mut && ! clips[j].midi)
                {
                    const double vora = std::min (clips[j].fadeIn, clips[j].longitud()) / 2.0;
                    const double vora2 = std::min (clips[j].fadeOut, clips[j].longitud()) / 2.0;
                    resta (v, clips[j].iniciVisible() + vora, clips[j].fiVisible() - vora2);
                }
            for (auto& z : v)
                if (z.second - z.first > 1.0e-4) r[(size_t) k].push_back (z);
        }
        for (auto& z : r) std::sort (z.begin(), z.end());
        return r;
    }

    void treuTake (Projecte& p, int pista, int take)
    {
        if (pista < 0 || pista >= (int) p.pistes.size()) return;
        auto& t = p.pistes[(size_t) pista];
        if (take < 0 || take >= (int) t.takes.size()) return;
        const Clip tk = t.takes[(size_t) take];
        t.clips.erase (std::remove_if (t.clips.begin(), t.clips.end(), [&] (const Clip& c) { return esDeTake (c, tk); }), t.clips.end());
        t.takes.erase (t.takes.begin() + take);
        if (t.takes.size() <= 1) t.takes.clear();   // ja no hi ha alternatives: pista normal (el comp queda com està)
    }

    int treuTotesLesTakes (Projecte& p, int pista)
    {
        if (pista < 0 || pista >= (int) p.pistes.size()) return 0;
        auto& t = p.pistes[(size_t) pista];
        const int n = (int) t.takes.size();
        t.takes.clear();
        return n;
    }

    int treuTakesSenseComp (Projecte& p)
    {
        int n = 0;
        for (int i = 0; i < (int) p.pistes.size(); ++i)
            if (! p.pistes[(size_t) i].takes.empty() && p.pistes[(size_t) i].clips.empty()) n += treuTotesLesTakes (p, i);
        return n;
    }

    int creaPistaComp (Projecte& p, int pista, const std::string& idPresa, double pos, double inici, double fi)
    {
        if (pista < 0 || pista >= (int) p.pistes.size()) return -1;
        Pista q = p.pistes[(size_t) pista];
        q.id = idNou (p, 't');
        q.nom = "Comp - " + q.nom;
        q.takes.clear();
        q.takesObertes = true;
        q.mut = q.solo = q.grava = false;
        Clip c;
        c.presa = idPresa;
        c.pos = pos;
        c.inici = inici;
        c.fi = std::max (inici + longitudMinima, fi);
        q.clips = { c };
        // Els mateixos efectes (amb identificadors nous: cada insert és un plugin viu). El Vocal Editor no: el Print ja
        // porta la veu corregida.
        std::vector<std::pair<std::string, std::string>> canvis;
        std::vector<Insert> ins;
        for (auto x : q.inserts)
        {
            if (esInsertVeu (x)) continue;
            const auto nou = idNou (p, 'i');
            canvis.push_back ({ x.id, nou });
            x.id = nou;
            ins.push_back (x);
        }
        q.inserts = std::move (ins);
        for (auto& e : q.enviaments) e.id = idNou (p, 'v');
        std::vector<Automatitzacio> autos;
        for (auto a : q.autos)
        {
            if (a.objectiu.rfind ("insert:", 0) == 0)
            {
                const auto resta = a.objectiu.substr (7);
                const auto dos = resta.find (':');
                const auto vell = resta.substr (0, dos);
                bool trobat = false;
                for (auto& [de, a2] : canvis)
                    if (de == vell) { a.objectiu = "insert:" + a2 + (dos == std::string::npos ? std::string() : resta.substr (dos)); trobat = true; }
                if (! trobat) continue;   // era del Vocal Editor
            }
            a.id = idNou (p, 'a');
            autos.push_back (a);
        }
        q.autos = std::move (autos);
        p.pistes[(size_t) pista].mut = true;
        p.pistes.insert (p.pistes.begin() + pista + 1, q);
        return pista + 1;
    }

    int creaPistaPrint (Projecte& p, int pista, const std::string& idPresa, double pos, double inici, double fi)
    {
        if (pista < 0 || pista >= (int) p.pistes.size()) return -1;
        const Pista& o = p.pistes[(size_t) pista];
        Pista q;
        q.id = idNou (p, 't');
        q.nom = "Print - " + o.nom;
        q.tipus = "audio";
        q.canals = 2;                    // els efectes (i els instruments) surten en estèreo
        q.color = o.color;
        q.rgb = o.rgb;
        q.carpeta = o.carpeta;
        q.faderDb = o.faderDb;
        q.pan = o.pan;
        q.sortida = o.sortida;
        q.sortidaTargeta = o.sortidaTargeta;   // 1.17
        q.canalsTargeta = o.canalsTargeta;
        q.entrada = o.entrada;
        q.escolta = o.escolta;
        q.enviaments = o.enviaments;
        for (auto& e : q.enviaments) e.id = idNou (p, 'v');
        for (const auto& a : o.autos)    // només el volum i el panorama: els efectes ja són dins l'àudio
            if (a.objectiu == "volum" || a.objectiu == "pan")
            {
                auto b = a;
                b.id = idNou (p, 'a');
                q.autos.push_back (b);
            }
        Clip c;
        c.presa = idPresa;
        c.pos = pos;
        c.inici = inici;
        c.fi = std::max (inici + longitudMinima, fi);
        q.clips = { c };
        p.pistes[(size_t) pista].mut = true;
        p.pistes.insert (p.pistes.begin() + pista + 1, q);
        return pista + 1;
    }

    std::pair<double, double> rangClips (const Pista& t)
    {
        double a = 1.0e300, b = -1.0e300;
        for (const auto& c : t.clips)
            if (! c.mut && ! c.esAcord() && (c.midi || ! c.presa.empty())) { a = std::min (a, c.iniciVisible()); b = std::max (b, c.fiVisible()); }
        if (b <= a) return { 0.0, 0.0 };
        return { a, b };
    }

    std::pair<double, double> rangComp (const Pista& t)
    {
        double a = 1.0e300, b = -1.0e300;
        for (const auto& c : t.clips)
            if (esAudio (c) && ! c.mut) { a = std::min (a, c.iniciVisible()); b = std::max (b, c.fiVisible()); }
        if (b <= a) return { 0.0, 0.0 };
        return { a, b };
    }
}
