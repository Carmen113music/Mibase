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
                if (p.tipus == Proposta::PassaAlts) { b.tipus = eq::HighPass; b.pendent = 1; b.q = eq::qDefecte; }   // 12 dB/oct
                else
                {
                    b.tipus = p.tipus == Proposta::Campana ? eq::Bell : p.tipus == Proposta::PrestatgeGreus ? eq::LowShelf : eq::HighShelf;
                    b.guany = std::clamp (p.guany, -eq::guanyMax, eq::guanyMax);
                    b.q = p.tipus == Proposta::Campana ? std::clamp (p.q, eq::qMin, eq::qMax) : eq::qDefecte;
                }
                e.posaBanda (0, b);
                c.plugin = eq::idPlugin;
                c.nom = "EQ Pro · Mix Assistant";
                c.estat = e.estat();
                c.resum = p.tipus == Proposta::PassaAlts ? "EQ Pro: passa-alts a " + hz (b.freq)
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
                    c.nom = "De-esser · Mix Assistant";
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
                    c.nom = "Compressor · Mix Assistant";
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
                c.nom = "Estèreo · Mix Assistant";
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
