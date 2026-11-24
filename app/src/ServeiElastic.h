#pragma once

#include "mibase/Elastic.h"
#include "mibase/MotorEstudi.h"

#include <juce_core/juce_core.h>

#include <map>
#include <mutex>
#include <set>

/** 0.86: Elastic Audio — el so estirat de cada clip es calcula en un fil a part (mai al fil d'àudio).
    La interfície demana el so de cada clau (elastic::clau) amb demana() i en recull els resultats amb recull() (des del
    seu Timer). Mentre un so no està fet, el pla fa sonar l'anterior del mateix clip (o l'original). */
class ServeiElastic : private juce::Thread
{
public:
    ServeiElastic() : juce::Thread ("Mibase elastic") { startThread (juce::Thread::Priority::low); }
    ~ServeiElastic() override
    {
        signalThreadShouldExit();
        senyal.signal();
        stopThread (20000);
    }

    /** Fil de la interfície: que hi hagi el so «clau» fet a partir de «font». No fa res si ja hi és o s'hi treballa. */
    void demana (const std::string& clau, std::shared_ptr<const mibase::AudioPresa> font, const std::vector<mibase::PuntWarp>& w, int mode)
    {
        if (font == nullptr || fet (clau, font) != nullptr) return;
        {
            std::lock_guard<std::mutex> l (mutex);
            auto& f = feines[clau];
            if (f.font == font && (f.pendent || f.enCurs)) return;
            f.font = std::move (font);
            f.warp = w;
            f.mode = mode;
            f.pendent = true;
            ++f.generacio;
        }
        senyal.signal();
    }

    /** Fil de la interfície: el so «clau», si ja està fet (i a partir de la mateixa font, si se'n dona una). */
    std::shared_ptr<const mibase::AudioPresa> fet (const std::string& clau, const std::shared_ptr<const mibase::AudioPresa>& font = nullptr) const
    {
        auto it = fets.find (clau);
        if (it == fets.end()) return nullptr;
        if (font != nullptr && it->second.font != font) return nullptr;
        return it->second.so;
    }

    /** Fil de la interfície: el fa ara mateix si no hi és (per exportar o fer un Print: no es pot esperar). */
    std::shared_ptr<const mibase::AudioPresa> ara (const std::string& clau, std::shared_ptr<const mibase::AudioPresa> font,
                                                   const std::vector<mibase::PuntWarp>& w, int mode)
    {
        if (font == nullptr) return nullptr;
        if (auto so = fet (clau, font)) return so;
        std::shared_ptr<const mibase::AudioPresa> so = mibase::elastic::renderitza (*font, w, mode);
        fets[clau] = { font, so };
        return so;
    }

    /** Fil de la interfície: treu els resultats nous. Torna true si n'hi ha (cal refer el pla i repintar). */
    bool recull()
    {
        std::lock_guard<std::mutex> l (mutex);
        if (sortides.empty()) return false;
        for (auto& s : sortides) fets[s.clau] = { s.font, s.so };
        sortides.clear();
        return true;
    }

    bool treballant() const
    {
        std::lock_guard<std::mutex> l (mutex);
        for (auto& [k, f] : feines) if (f.pendent || f.enCurs) return true;
        return false;
    }

    /** Fil de la interfície: oblida els sons que ja no fa servir cap clip (i deixa de calcular-los). */
    void oblida (const std::set<std::string>& vius)
    {
        for (auto it = fets.begin(); it != fets.end();)
            it = vius.count (it->first) ? std::next (it) : fets.erase (it);
        std::lock_guard<std::mutex> l (mutex);
        for (auto it = feines.begin(); it != feines.end();)
            it = vius.count (it->first) || it->second.enCurs ? std::next (it) : feines.erase (it);
    }

    /** En tancar el projecte. */
    void buida()
    {
        fets.clear();
        std::lock_guard<std::mutex> l (mutex);
        feines.clear();
        sortides.clear();
    }

private:
    struct Feina
    {
        std::shared_ptr<const mibase::AudioPresa> font;
        std::vector<mibase::PuntWarp> warp;
        int mode = 0;
        bool pendent = false, enCurs = false;
        int generacio = 0;
    };
    struct Sortida
    {
        std::string clau;
        std::shared_ptr<const mibase::AudioPresa> font, so;
    };
    struct Fet
    {
        std::shared_ptr<const mibase::AudioPresa> font, so;
    };

    void run() override
    {
        while (! threadShouldExit())
        {
            senyal.wait (500);
            for (;;)
            {
                if (threadShouldExit()) return;
                std::string clau;
                Feina f;
                {
                    std::lock_guard<std::mutex> l (mutex);
                    for (auto& [k, v] : feines)
                        if (v.pendent) { clau = k; f = v; v.pendent = false; v.enCurs = true; break; }
                }
                if (clau.empty()) break;
                // Es para si la feina canvia (s'ha tornat a demanar amb uns altres punts) o ja no cal.
                auto segueix = [this, &clau, gen = f.generacio] (float)
                {
                    if (threadShouldExit()) return false;
                    std::lock_guard<std::mutex> l (mutex);
                    auto it = feines.find (clau);
                    return it != feines.end() && it->second.generacio == gen;
                };
                std::shared_ptr<const mibase::AudioPresa> so = mibase::elastic::renderitza (*f.font, f.warp, f.mode, segueix);
                std::lock_guard<std::mutex> l (mutex);
                auto it = feines.find (clau);
                if (it == feines.end()) continue;
                it->second.enCurs = false;
                if (so != nullptr && it->second.generacio == f.generacio)
                {
                    sortides.push_back ({ clau, f.font, so });
                    feines.erase (it);
                }
            }
        }
    }

    mutable std::mutex mutex;
    juce::WaitableEvent senyal;
    std::map<std::string, Feina> feines;
    std::vector<Sortida> sortides;

    std::map<std::string, Fet> fets;   // només el fil de la interfície
};
