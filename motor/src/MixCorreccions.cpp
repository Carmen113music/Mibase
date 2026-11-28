#include "mibase/MixAssistant.h"
#include "mibase/Dinamica.h"
#include "mibase/Equalitzador.h"
#include "mibase/Estereo.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

/** 1.25: Mix Assistant, fase 3: de la proposta a un efecte intern preparat (EQ Pro, Dinàmica o Estèreo) i el seguiment
    entre dues anàlisis. Tot fora del fil d'àudio: els efectes es creen només per treure'n l'estat (el text que es desa). */
namespace mibase::mix
{
    namespace
    {
        std::string num1 (double v)
        {
            char b[32];
            std::snprintf (b, sizeof b, "%.1f", v);
            std::string s (b);
            for (auto& c : s) if (c == '.') c = ',';
            return s;
        }
        std::string hz (double f) { return f >= 1000.0 ? num1 (f / 1000.0) + " kHz" : std::to_string ((int) std::lround (f)) + " Hz"; }
        std::string dbs (double v) { return std::string (v > 0 ? "+" : v < 0 ? "−" : "") + num1 (std::abs (v)) + " dB"; }

        /** El llindar (dBFS, abans del fader) perquè les parts fortes baixin uns «red» dB amb aquesta ratio. */
        float llindarPer (const AnalisiPista* a, float guanyDb, float red, float ratio, float perDefecte)
        {
            if (a == nullptr || ! a->valid || a->nivellFortDb < -90.0f) return perDefecte;
            const float g = std::isnan (guanyDb) ? 0.0f : guanyDb;
            const float fort = a->nivellFortDb - g;
            const float t = fort - red / std::max (0.1f, 1.0f - 1.0f / std::max (1.01f, ratio));
            return std::clamp (t, -50.0f, -3.0f);
        }
    }

    bool espotPreparar (const Incidencia& i)
    {
        switch (i.proposta.tipus)
        {
            case Proposta::PassaAlts: case Proposta::Campana: case Proposta::PrestatgeGreus: case Proposta::PrestatgeAguts:
            case Proposta::DeEsser: case Proposta::Compressor: case Proposta::MonoGreus:
                return ! i.pista.empty() && ! i.deMescla;
            default: return false;
        }
    }

    Correccio preparaCorreccio (const Incidencia& i, const AnalisiPista* a, float guanyDb)
    {
        Correccio c;
        if (! espotPreparar (i)) return c;
        c.pista = i.pista;
        const auto& p = i.proposta;
        switch (p.tipus)
        {
            case Proposta::PassaAlts: case Proposta::Campana: case Proposta::PrestatgeGreus: case Proposta::PrestatgeAguts:
            {
                eq::Equalitzador e;
                eq::Banda b;
                b.usada = true;
                b.freq = std::clamp (p.freq, eq::freqMin, eq::freqMax);
                if (p.tipus == Proposta::PassaAlts) { b.tipus = eq::HighPass; b.pendent = std::clamp (p.pendent, 0, eq::numPendents - 1); b.q = eq::qDefecte; }
                else
                {
                    b.tipus = p.tipus == Proposta::Campana ? eq::Bell : p.tipus == Proposta::PrestatgeGreus ? eq::LowShelf : eq::HighShelf;
                    b.guany = std::clamp (p.guany, -eq::guanyMax, eq::guanyMax);
                    b.q = p.tipus == Proposta::Campana ? std::clamp (p.q, eq::qMin, eq::qMax) : eq::qDefecte;
                }
                e.posaBanda (0, b);
                c.plugin = eq::idPlugin;
                c.nom = nomEqMix;
                c.estat = e.estat();
                c.resum = p.tipus == Proposta::PassaAlts ? "EQ Pro: passa-alts a " + hz (b.freq) + " (" + std::to_string (eq::dbPerOctava (b.pendent)) + " dB/oct)"
                        : "EQ Pro: " + dbs (b.guany) + " a " + hz (b.freq) + (p.tipus == Proposta::Campana ? " (Q " + num1 (b.q) + ")" : "");
                break;
            }
            case Proposta::DeEsser: case Proposta::Compressor:
            {
                dinamica::Dinamica d;
                if (p.tipus == Proposta::DeEsser)
                {
                    const float red = p.reduccioDb > 0.0f ? p.reduccioDb : 4.0f;
                    d.posa (dinamica::ModeP, (float) dinamica::DeEsser);
                    d.posa (dinamica::FreqDeEsser, std::clamp (p.freq * 0.85f, 2000.0f, 12000.0f));   // una mica per sota del pic
                    d.posa (dinamica::Ratio, 4.0f);
                    // les esses són més fortes a la seva zona que la veu sencera: el llindar, per sota de les parts fortes
                    d.posa (dinamica::Llindar, llindarPer (a, guanyDb, red + 6.0f, 4.0f, -30.0f));
                    d.posa (dinamica::Atac, 1.0f);
                    d.posa (dinamica::Alliberament, 60.0f);
                    c.nom = nomDeEsserMix;
                    c.resum = "De-esser a partir de " + hz (std::clamp (p.freq * 0.85f, 2000.0f, 12000.0f)) + " (que abaixi uns " + num1 (red) + " dB les esses)";
                }
                else
                {
                    const float ratio = p.ratio > 1.0f ? p.ratio : 3.0f, red = p.reduccioDb > 0.0f ? p.reduccioDb : 4.0f;
                    d.posa (dinamica::ModeP, (float) dinamica::Compressor);
                    d.posa (dinamica::Ratio, ratio);
                    d.posa (dinamica::Llindar, llindarPer (a, guanyDb, red, ratio, -24.0f));
                    d.posa (dinamica::Atac, p.atacMs > 0.0f ? p.atacMs : 10.0f);
                    d.posa (dinamica::Alliberament, p.alliberamentMs > 0.0f ? p.alliberamentMs : 120.0f);
                    d.posa (dinamica::Genoll, 6.0f);
                    d.posa (dinamica::Detector, 1.0f);   // RMS: segueix el volum, no cada pic
                    d.posa (dinamica::Guany, std::round (red * 0.5f * 2.0f) / 2.0f);   // recupera una part del volum
                    c.nom = nomCompressorMix;
                    c.resum = "Compressor " + num1 (ratio) + ":1 (que redueixi uns " + num1 (red) + " dB a les parts fortes)";
                }
                c.plugin = dinamica::idPlugin;
                c.estat = d.estat();
                break;
            }
            case Proposta::MonoGreus:
            {
                estereo::Estereo e;
                e.posa (estereo::MonoGreus, std::clamp (p.freq, 40.0f, 300.0f));
                c.plugin = estereo::idPlugin;
                c.nom = nomEstereoMix;
                c.estat = e.estat();
                c.resum = "Estèreo: greus al mig per sota de " + hz (p.freq);
                break;
            }
            default: break;
        }
        return c;
    }

    const char* nomEvolucio (Evolucio::Estat e)
    {
        switch (e)
        {
            case Evolucio::Solucionat: return "Solucionat";
            case Evolucio::Millorat: return "Ha millorat";
            case Evolucio::Continua: return "Continua";
            case Evolucio::Empitjorat: return "Ha empitjorat";
            default: return "Nou";
        }
    }

    std::vector<Evolucio> compara (const std::vector<Incidencia>& abans, const std::vector<Incidencia>& ara)
    {
        std::vector<Evolucio> v;
        std::map<std::string, const Incidencia*> ja;
        for (const auto& i : ara) ja[i.id] = &i;
        for (const auto& i : abans)
        {
            Evolucio e;
            e.id = i.id; e.pista = i.pista; e.titol = i.titol;
            const auto it = ja.find (i.id);
            if (it == ja.end()) e.estat = Evolucio::Solucionat;
            else if (it->second->gravetat < i.gravetat - 0.05f) e.estat = Evolucio::Millorat;
            else if (it->second->gravetat > i.gravetat + 0.05f) e.estat = Evolucio::Empitjorat;
            else e.estat = Evolucio::Continua;
            v.push_back (e);
        }
        std::map<std::string, bool> abansIds;
        for (const auto& i : abans) abansIds[i.id] = true;
        for (const auto& i : ara)
            if (abansIds.find (i.id) == abansIds.end())
            {
                Evolucio e;
                e.id = i.id; e.pista = i.pista; e.titol = i.titol;
                e.estat = Evolucio::Nou;
                v.push_back (e);
            }
        std::stable_sort (v.begin(), v.end(), [] (const Evolucio& a, const Evolucio& b) { return (int) a.estat < (int) b.estat; });
        return v;
    }
}

// =====================================================================================================================
// 1.26: un sol efecte de cada mena per pista
#include "mibase/Projecte.h"

namespace mibase::mix
{
    namespace
    {
        const char* const ordreMix[] = { nomEqMix, nomCompressorMix, nomDeEsserMix, nomEstereoMix };
        int posMix (const std::string& nom)
        {
            for (int k = 0; k < 4; ++k) if (nom == ordreMix[k]) return k;
            return -1;
        }
        Pista* pistaDe (Projecte& p, const std::string& id)
        {
            for (auto& t : p.pistes) if (t.id == id) return &t;
            return nullptr;
        }
        Insert* insertDe (Pista& t, const std::string& id)
        {
            for (auto& x : t.inserts) if (x.id == id) return &x;
            return nullptr;
        }
        bool acabaEn (const std::string& s, const std::string& f) { return s.size() >= f.size() && s.compare (s.size() - f.size(), f.size(), f) == 0; }
    }

    bool esEfecteMix (const Insert& x)
    {
        return posMix (x.nom) >= 0 || x.nom.find ("Mix Assistant") != std::string::npos;
    }

    void ordenaEfectesMix (Pista& t)
    {
        std::stable_sort (t.inserts.begin(), t.inserts.end(), [] (const Insert& a, const Insert& b)
        {
            const int pa = posMix (a.nom), pb = posMix (b.nom);
            return (pa < 0 ? -1 : pa) < (pb < 0 ? -1 : pb);   // els d'en Roger (-1) primer, sense moure'ls entre ells
        });
    }

    LlocCorreccio posaCorreccio (Projecte& p, int pista, const Correccio& c, bool encesa)
    {
        LlocCorreccio l;
        if (pista < 0 || pista >= (int) p.pistes.size() || ! c.valida()) return l;
        auto& t = p.pistes[(size_t) pista];
        l.pista = t.id;
        const std::string nom = c.nom;
        Insert* x = nullptr;
        for (auto& y : t.inserts) if (y.nom == nom) x = &y;
        if (x == nullptr)
        {
            Insert n;
            n.id = "i" + std::to_string (++p.comptador);
            n.plugin = c.plugin;
            n.nom = nom;
            n.actiu = true;
            t.inserts.push_back (n);
            x = &t.inserts.back();
        }
        if (c.plugin == eq::idPlugin)
        {
            eq::Equalitzador e, nova;
            if (! x->estat.empty()) e.posaEstat (x->estat);
            nova.posaEstat (c.estat);
            auto b = nova.banda (0);
            b.activa = encesa;
            const int k = e.bandaLliure();
            if (k < 0) return {};   // l'EQ és ple (24 bandes)
            e.posaBanda (k, b);
            x->estat = e.estat();
            x->actiu = true;
            l.banda = k;
        }
        else
        {
            x->estat = c.estat;   // el compressor (o el de-esser, o l'Estèreo) d'abans es reajusta amb la proposta nova
            x->actiu = encesa;
        }
        l.insert = x->id;
        ordenaEfectesMix (t);
        return l;
    }

    int estatCorreccio (const Projecte& p, const LlocCorreccio& l)
    {
        for (const auto& t : p.pistes)
        {
            if (t.id != l.pista) continue;
            for (const auto& x : t.inserts)
            {
                if (x.id != l.insert) continue;
                if (l.banda < 0) return x.actiu ? 1 : 0;
                eq::Equalitzador e;
                e.posaEstat (x.estat);
                const auto b = e.banda (l.banda);
                if (! b.usada) return -1;
                return b.activa && x.actiu ? 1 : 0;
            }
        }
        return -1;
    }

    bool encenCorreccio (Projecte& p, const LlocCorreccio& l, bool encesa)
    {
        auto* t = pistaDe (p, l.pista);
        auto* x = t != nullptr ? insertDe (*t, l.insert) : nullptr;
        if (x == nullptr) return false;
        if (l.banda < 0) { x->actiu = encesa; return true; }
        eq::Equalitzador e;
        e.posaEstat (x->estat);
        auto b = e.banda (l.banda);
        if (! b.usada) return false;
        b.activa = encesa;
        e.posaBanda (l.banda, b);
        x->estat = e.estat();
        if (encesa) x->actiu = true;
        return true;
    }

    bool treuCorreccio (Projecte& p, const LlocCorreccio& l)
    {
        auto* t = pistaDe (p, l.pista);
        if (t == nullptr) return false;
        auto it = std::find_if (t->inserts.begin(), t->inserts.end(), [&] (const Insert& x) { return x.id == l.insert; });
        if (it == t->inserts.end()) return false;
        if (l.banda >= 0)
        {
            eq::Equalitzador e;
            e.posaEstat (it->estat);
            e.esborra (l.banda);
            bool queda = false;
            for (int k = 0; k < eq::maxBandes; ++k) queda = queda || e.banda (k).usada;
            if (queda) { it->estat = e.estat(); return true; }
        }
        t->inserts.erase (it);
        return true;
    }

    int ajuntaEqsMix (Projecte& p)
    {
        int n = 0;
        for (auto& t : p.pistes)
        {
            std::vector<size_t> eqs;
            for (size_t k = 0; k < t.inserts.size(); ++k)
            {
                const auto& x = t.inserts[k];
                if (x.plugin == eq::idPlugin && (x.nom == nomEqMix || x.nom == std::string (nomEqMix) + sufixProva)) eqs.push_back (k);
            }
            if (eqs.empty()) continue;
            if (eqs.size() == 1 && t.inserts[eqs[0]].nom == nomEqMix && t.inserts[eqs[0]].actiu) continue;   // ja està bé
            eq::Equalitzador tot;
            for (size_t k : eqs)
            {
                const auto& x = t.inserts[k];
                eq::Equalitzador e;
                e.posaEstat (x.estat);
                for (int b = 0; b < eq::maxBandes; ++b)
                {
                    auto bb = e.banda (b);
                    if (! bb.usada) continue;
                    bb.activa = bb.activa && x.actiu;   // una prova apagada queda com una banda apagada
                    const int lliure = tot.bandaLliure();
                    if (lliure >= 0) tot.posaBanda (lliure, bb);
                }
            }
            Insert nou = t.inserts[eqs[0]];
            nou.nom = nomEqMix;
            nou.actiu = true;
            nou.estat = tot.estat();
            for (auto k = eqs.rbegin(); k != eqs.rend(); ++k) t.inserts.erase (t.inserts.begin() + (long) *k);
            t.inserts.push_back (nou);
            ordenaEfectesMix (t);
            n += (int) eqs.size();
        }
        return n;
    }

    bool reforcaCorreccio (Projecte& p, const LlocCorreccio& l, const Correccio& nova, int* bandaNova)
    {
        if (bandaNova != nullptr) *bandaNova = -1;
        auto* t = pistaDe (p, l.pista);
        auto* x = t != nullptr ? insertDe (*t, l.insert) : nullptr;
        if (x == nullptr || ! nova.valida()) return false;
        if (l.banda < 0)
        {
            x->estat = nova.estat;
            x->actiu = true;
            return true;
        }
        eq::Equalitzador e, n;
        e.posaEstat (x->estat);
        n.posaEstat (nova.estat);
        auto b = e.banda (l.banda);
        const auto bn = n.banda (0);
        if (! b.usada || ! bn.usada) return false;
        if (b.tipus == eq::HighPass)
        {
            b.freq = std::max (b.freq, bn.freq);
            b.pendent = std::min (eq::numPendents - 1, std::max (b.pendent, bn.pendent) + (bn.freq <= b.freq ? 1 : 0));
        }
        else if (std::abs (std::log2 (bn.freq / std::max (1.0f, b.freq))) > 1.0 / 3.0)
        {
            // el que falta és a un altre lloc: una banda nova (encesa) al mateix EQ
            auto nb = bn;
            nb.activa = true;
            const int k = e.bandaLliure();
            if (k < 0) return false;
            e.posaBanda (k, nb);
            x->estat = e.estat();
            x->actiu = true;
            if (bandaNova != nullptr) *bandaNova = k;
            return true;
        }
        else
        {
            b.guany = std::clamp (b.guany + bn.guany, -24.0f, 24.0f);
            if (b.tipus == eq::Bell && bn.tipus == eq::Bell) b.q = std::max (b.q, bn.q);
        }
        b.activa = true;
        e.posaBanda (l.banda, b);
        x->estat = e.estat();
        x->actiu = true;
        return true;
    }
}
