// 0.78: «Implementa a pistes reals»: els retorns del Vocal Studio passen a ser busos d'efectes de debò.
#include "mibase/Rack.h"
#include "mibase/Edicio.h"
#include "mibase/Projecte.h"
#include <algorithm>
#include <cmath>

namespace mibase::rack
{
    namespace
    {
        const TipusModul* tipusDe (const std::string& clau)
        {
            for (const auto& t : tipus()) if (clau == t.clau) return &t;
            return nullptr;
        }
        /** Els que es poden posar sols com a insert d'una pista (els altres van dins d'un Vocal Studio petit). */
        bool potAnarSol (const std::string& clau)
        {
            return clau != "doubler" && clau != "harmonitzador" && clau != "octava";
        }
        bool comencaPer (const std::string& s, const std::string& p) { return s.rfind (p, 0) == 0; }
    }

    std::vector<int> busosDesplegats (const Projecte& p, int pista)
    {
        std::vector<int> r;
        if (pista < 0 || pista >= (int) p.pistes.size()) return r;
        for (const auto& e : p.pistes[(size_t) pista].enviaments)
        {
            const int b = edicio::pistaPerId (p, e.desti);
            if (b >= 0 && esBus (p.pistes[(size_t) b]) && comencaPer (p.pistes[(size_t) b].nom, prefixDesplega)
                && std::find (r.begin(), r.end(), b) == r.end())
                r.push_back (b);
        }
        std::sort (r.begin(), r.end());
        return r;
    }

    int treuDesplegament (Projecte& p, int pista)
    {
        auto bs = busosDesplegats (p, pista);
        std::vector<std::string> carpetes;
        for (int b : bs)
        {
            const auto& c = p.pistes[(size_t) b].carpeta;
            if (! c.empty() && std::find (carpetes.begin(), carpetes.end(), c) == carpetes.end()) carpetes.push_back (c);
        }
        for (auto it = bs.rbegin(); it != bs.rend(); ++it) edicio::esborraPista (p, *it);
        for (const auto& c : carpetes)
            if (edicio::pistesDeCarpeta (p, c).empty())
                if (auto* f = edicio::carpeta (p, c); f != nullptr && comencaPer (f->nom, prefixDesplega)) edicio::esborraCarpeta (p, c);
        return (int) bs.size();
    }

    ResultatDesplega desplega (Projecte& p, int pista, const std::string& idInsert)
    {
        ResultatDesplega res;
        if (pista < 0 || pista >= (int) p.pistes.size()) { res.error = "No hi ha cap pista."; return res; }
        int ins = -1;
        for (int i = 0; i < (int) p.pistes[(size_t) pista].inserts.size(); ++i)
            if (p.pistes[(size_t) pista].inserts[(size_t) i].id == idInsert) ins = i;
        if (ins < 0 || p.pistes[(size_t) pista].inserts[(size_t) ins].plugin != idPlugin) { res.error = "No és un Vocal Studio."; return res; }

        Rack r;
        r.posaEstat (p.pistes[(size_t) pista].inserts[(size_t) ins].estat);
        bool hiHa = false;
        for (int k = 0; k < maxRetorns; ++k) hiHa = hiHa || r.retorn (k).actiu;
        if (! hiHa) { res.error = "Aquest Vocal Studio no té cap retorn."; return res; }

        const std::string nomPista = p.pistes[(size_t) pista].nom;
        const std::string idPista = p.pistes[(size_t) pista].id;
        const std::string idCarpeta = edicio::creaCarpeta (p, std::string (prefixDesplega) + nomPista);
        res.carpeta = idCarpeta;
        auto nouInsert = [&p] (const std::string& plugin, const std::string& nom, const std::string& estat)
        {
            Insert x;
            x.id = "i" + std::to_string (++p.comptador);
            x.plugin = plugin;
            x.nom = nom;
            x.estat = estat;
            return x;
        };

        for (int k = 0; k < maxRetorns; ++k)
        {
            if (! r.retorn (k).actiu) continue;
            const std::string nomRet = r.retorn (k).nom.empty() ? "Retorn " + std::to_string (k + 1) : r.retorn (k).nom;
            const std::string idBus = edicio::afegeixBus (p, std::string (prefixDesplega) + nomPista + " · " + nomRet, true).id;
            int b = edicio::pistaPerId (p, idBus);
            // efectes del retorn, per ordre; els que no poden anar sols, junts dins d'un Vocal Studio petit
            std::vector<Insert> inserts;
            std::shared_ptr<Rack> petit;
            auto tancaPetit = [&]
            {
                if (petit == nullptr) return;
                inserts.push_back (nouInsert (idPlugin, nomPlugin, petit->estat()));
                petit.reset();
            };
            for (int i = 0; i < r.numModuls(); ++i)
            {
                if (r.cami (i) != k + 1) continue;
                const auto* m = r.modul (i);
                const auto* t = tipusDe (m->clau);
                if (t == nullptr) continue;
                if (potAnarSol (m->clau) && m->mix.load() >= 0.999f)
                {
                    tancaPetit();
                    auto x = nouInsert (t->plugin, std::string (t->nom) + " (Mibase)", m->fx->estat());
                    x.actiu = m->on.load();
                    inserts.push_back (x);
                }
                else
                {
                    if (petit == nullptr) { petit = std::make_shared<Rack>(); petit->posaEstat (R"({"mibase-rack":1,"moduls":[]})"); }
                    const int j = petit->afegeix (m->clau);
                    if (j >= 0)
                    {
                        petit->modul (j)->fx->posaEstat (m->fx->estat());
                        petit->posaMix (j, m->mix.load());
                        petit->posaOn (j, m->on.load());
                    }
                }
            }
            tancaPetit();
            // ducking: una Dinàmica amb la pista com a sidechain (baixa uns «ducking» dB quan la veu va forta)
            const float duck = r.valor (Ducking1 + k);
            if (duck > 0.05f)
            {
                dinamica::Dinamica d;
                d.posa (dinamica::ModeP, dinamica::Compressor);
                d.posa (dinamica::Llindar, -30.0f);
                d.posa (dinamica::Ratio, std::clamp (24.0f / std::max (1.0f, 24.0f - std::min (duck, 20.0f)), 1.0f, 20.0f));
                d.posa (dinamica::Atac, 5.0f);
                d.posa (dinamica::Alliberament, 250.0f);
                auto x = nouInsert (dinamica::idPlugin, "Ducking (Mibase)", d.estat());
                FontSidechain f;
                f.pista = idPista;
                f.punt = 1;
                x.sidechain.push_back (f);
                inserts.push_back (x);
            }
            b = edicio::pistaPerId (p, idBus);
            auto& bus = p.pistes[(size_t) b];
            bus.inserts = std::move (inserts);
            const float ret = r.valor (Retorn1 + k);
            bus.faderDb = std::clamp ((double) ret, -60.0, 6.0);
            bus.pan = std::clamp (r.valor (PanRetorn1 + k) / 100.0, -1.0, 1.0);
            edicio::posaACarpeta (p, b, idCarpeta);
            // l'enviament des de la pista
            const int pi = edicio::pistaPerId (p, idPista);
            const int e = edicio::afegeixEnviament (p, pi, idBus);
            if (e >= 0)
            {
                auto& env = p.pistes[(size_t) pi].enviaments[(size_t) e];
                const float en = r.valor (Envia1 + k);
                env.nivellDb = en <= minDbRetorn + 0.05f ? -60.0 : std::clamp ((double) en, -60.0, 6.0);
                env.mode = 1;
            }
            ++res.busos;
        }

        // al Vocal Studio de la pista hi queda només la cadena principal
        for (int i = r.numModuls() - 1; i >= 0; --i) if (r.cami (i) > 0) r.treu (i);
        for (int k = 0; k < maxRetorns; ++k) r.activaRetorn (k, false);
        const int pi = edicio::pistaPerId (p, idPista);
        for (auto& x : p.pistes[(size_t) pi].inserts) if (x.id == idInsert) x.estat = r.estat();
        res.correcte = true;
        return res;
    }
}
