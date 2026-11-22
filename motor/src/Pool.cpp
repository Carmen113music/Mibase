// 0.85: el Pool: tots els àudios del projecte, on es fan servir i els que sobren.
#include "mibase/Edicio.h"

#include <algorithm>

namespace mibase::edicio
{
    UsPresa usPresa (const Projecte& p, const std::string& id)
    {
        UsPresa u;
        for (const auto& t : p.pistes)
        {
            for (const auto& c : t.clips) if (c.presa == id) ++u.clips;
            for (const auto& c : t.takes) if (c.presa == id) ++u.takes;
        }
        return u;
    }

    std::vector<RefClip> llocsPresa (const Projecte& p, const std::string& id)
    {
        std::vector<RefClip> r;
        for (int i = 0; i < (int) p.pistes.size(); ++i)
            for (int k = 0; k < (int) p.pistes[(size_t) i].clips.size(); ++k)
                if (p.pistes[(size_t) i].clips[(size_t) k].presa == id) r.push_back ({ i, k });
        std::sort (r.begin(), r.end(), [&p] (RefClip a, RefClip b)
        {
            const double ta = p.pistes[(size_t) a.pista].clips[(size_t) a.clip].iniciVisible();
            const double tb = p.pistes[(size_t) b.pista].clips[(size_t) b.clip].iniciVisible();
            return ta != tb ? ta < tb : a.pista < b.pista;
        });
        return r;
    }

    std::vector<std::string> presesSenseUs (const Projecte& p)
    {
        std::vector<std::string> r;
        for (const auto& s : p.preses)
            if (! usPresa (p, s.id).enUs()) r.push_back (s.id);
        return r;
    }

    bool treuPresa (Projecte& p, const std::string& id)
    {
        if (usPresa (p, id).enUs()) return false;
        const auto abans = p.preses.size();
        p.preses.erase (std::remove_if (p.preses.begin(), p.preses.end(), [&id] (const Presa& s) { return s.id == id; }), p.preses.end());
        return p.preses.size() != abans;
    }

    void canviaFitxerPresa (Projecte& p, const std::string& id, const std::string& fitxer, const std::string& nom)
    {
        if (auto* s = presa (p, id))
        {
            s->fitxer = fitxer;
            if (! nom.empty()) s->nom = nom;
        }
    }
}
