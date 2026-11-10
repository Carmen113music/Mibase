#pragma once

#include "Dibuix.h"
#include "mibase/MotorEstudi.h"
#include "mibase/Veu.h"

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <map>
#include <mutex>
#include <set>

/** 0.22: Vocal Editor — feina pesant en un fil a part (mai al fil d'àudio):
    analitza les preses (o llegeix l'anàlisi desada a «<fitxer>.veu») i refà el so corregit, només els trossos que
    canvien. La interfície hi envia l'estat de cada presa amb actualitza() i en recull els resultats amb recull()
    (des del seu Timer). Les preses corregides es donen al pla i sonen en lloc de les originals. */
class ServeiVeu : private juce::Thread
{
public:
    ServeiVeu() : juce::Thread ("Mibase veu") { startThread (juce::Thread::Priority::low); }
    ~ServeiVeu() override
    {
        signalThreadShouldExit();
        senyal.signal();
        stopThread (20000);
    }

    /** Fil de la interfície: que la presa «id» estigui analitzada i que el so corregit correspongui a «v».
        No fa res si ja és així (es pot cridar cada cop que canvia el projecte). */
    void actualitza (const std::string& id, std::shared_ptr<const mibase::AudioPresa> original, const juce::File& cau,
                     const mibase::VeuPresa& v)
    {
        if (original == nullptr) return;
        {
            std::lock_guard<std::mutex> l (mutex);
            auto& f = feines[id];
            if (f.original == original && f.veu == v && f.enviada) return;
            f.original = std::move (original);
            f.cau = cau;
            f.veu = v;
            f.enviada = true;
            f.pendent = true;
        }
        senyal.signal();
    }

    /** Fil de la interfície: treu els resultats nous. «analitzades» = preses que acaben de tenir anàlisi.
        Torna true si ha canviat alguna cosa (cal refer el pla i repintar). */
    bool recull (std::vector<std::string>& analitzades)
    {
        std::lock_guard<std::mutex> l (mutex);
        if (sortides.empty()) return false;
        for (auto& s : sortides)
        {
            if (s.analisi != nullptr) { analisis[s.id] = s.analisi; analitzades.push_back (s.id); }
            if (s.teSo)
            {
                if (s.so != nullptr) corregidesUI[s.id] = s.so;
                else corregidesUI.erase (s.id);
            }
        }
        sortides.clear();
        return true;
    }

    std::shared_ptr<const mibase::veu::Analisi> analisi (const std::string& id) const
    {
        auto it = analisis.find (id);
        return it != analisis.end() ? it->second : nullptr;
    }

    /** 0..1 mentre s'analitza; -1 si no. */
    float progres (const std::string& id) const
    {
        std::lock_guard<std::mutex> l (mutex);
        auto it = feines.find (id);
        return it != feines.end() ? it->second.progres : -1.0f;
    }

    /** Hi ha feina pendent per a aquesta presa (el so encara no està al dia). */
    bool treballant (const std::string& id) const
    {
        std::lock_guard<std::mutex> l (mutex);
        auto it = feines.find (id);
        return it != feines.end() && (it->second.pendent || it->second.enCurs);
    }

    const mibase::MapaAudios& corregides() const noexcept { return corregidesUI; }

    /** En tancar el projecte. */
    void buida()
    {
        std::lock_guard<std::mutex> l (mutex);
        for (auto& [id, f] : feines) f.oblidada = true;
        feines.clear();
        analisis.clear();
        corregidesUI.clear();
        sortides.clear();
        cancela = true;
    }

private:
    struct Feina
    {
        std::shared_ptr<const mibase::AudioPresa> original;
        juce::File cau;
        mibase::VeuPresa veu;
        bool enviada = false, pendent = false, enCurs = false, oblidada = false;
        float progres = -1.0f;
    };
    struct Sortida
    {
        std::string id;
        std::shared_ptr<const mibase::veu::Analisi> analisi;   // nova anàlisi (o nullptr)
        bool teSo = false;                                     // canvia el so corregit
        std::shared_ptr<const mibase::AudioPresa> so;          // nullptr: sona l'original
    };
    /** El que el fil de treball sap de cada presa (només el toca ell). */
    struct Treball
    {
        std::shared_ptr<const mibase::AudioPresa> original;
        std::shared_ptr<const mibase::veu::Analisi> analisi;
        std::vector<float> buf[2];
        mibase::veu::Corbes ultimes;
        bool ambSo = false;
    };

    void run() override
    {
        while (! threadShouldExit())
        {
            senyal.wait (500);
            for (;;)
            {
                if (threadShouldExit()) return;
                std::string id;
                Feina f;
                {
                    std::lock_guard<std::mutex> l (mutex);
                    if (cancela) { treballs.clear(); cancela = false; }
                    for (auto& [k, v] : feines)
                        if (v.pendent) { id = k; f = v; v.pendent = false; v.enCurs = true; break; }
                }
                if (id.empty()) break;
                fes (id, f);
                std::lock_guard<std::mutex> l (mutex);
                if (auto it = feines.find (id); it != feines.end()) { it->second.enCurs = false; it->second.progres = -1.0f; }
            }
        }
    }

    void fes (const std::string& id, const Feina& f)
    {
        auto& t = treballs[id];
        const auto& o = *f.original;
        if (t.original != f.original || t.analisi == nullptr)
        {
            t = Treball {};
            t.original = f.original;
            auto a = std::make_shared<mibase::veu::Analisi>();
            const auto cami = dibuix::cami (f.cau);
            const bool llegida = f.cau.existsAsFile() && mibase::veu::llegeixAnalisi (cami, *a)
                                 && a->mostres == o.longitud && std::abs (a->fs - o.fs) < 0.5;
            if (! llegida)
            {
                std::vector<float> mono ((size_t) o.longitud);
                for (long long i = 0; i < o.longitud; ++i)
                {
                    float v = 0.0f;
                    for (int ch = 0; ch < o.canals; ++ch) v += o.dades[ch][(size_t) i];
                    mono[(size_t) i] = v / (float) o.canals;
                }
                *a = mibase::veu::analitza (mono.data(), o.longitud, o.fs, [this, &id] (float p)
                {
                    std::lock_guard<std::mutex> l (mutex);
                    auto it = feines.find (id);
                    if (it == feines.end() || cancela || threadShouldExit()) return false;
                    it->second.progres = p;
                    return true;
                });
                if (a->buida()) { treballs.erase (id); return; }   // cancel·lada
                mibase::veu::desaAnalisi (*a, cami);
            }
            t.analisi = a;
            std::lock_guard<std::mutex> l (mutex);
            if (! feines.count (id)) return;
            sortides.push_back ({ id, a, false, nullptr });
        }
        if (! f.veu.analitzada) return;   // la interfície hi posarà les notes i tornarà a demanar

        const auto c = mibase::veu::corbes (*t.analisi, f.veu);
        if (! mibase::veu::teCanvis (f.veu))
        {
            t.ultimes = c;
            const bool teniaSo = t.ambSo;
            t.ambSo = false;
            t.buf[0].clear(); t.buf[1].clear();
            if (teniaSo) { std::lock_guard<std::mutex> l (mutex); sortides.push_back ({ id, nullptr, true, nullptr }); }
            return;
        }
        if (! t.ambSo)
        {
            for (int ch = 0; ch < o.canals; ++ch) t.buf[ch] = o.dades[ch];
            t.ultimes = {};
        }
        const auto trossos = mibase::veu::trossosPerRefer (*t.analisi, t.ultimes, c);
        if (trossos.empty() && t.ambSo) { t.ultimes = c; return; }
        const float* in[2] { o.dades[0].data(), o.canals > 1 ? o.dades[1].data() : nullptr };
        float* out[2] { t.buf[0].data(), o.canals > 1 ? t.buf[1].data() : nullptr };
        for (auto tr : trossos)
            mibase::veu::renderitza (in, o.canals, o.longitud, o.fs, *t.analisi, c, out, tr.de, tr.a);
        t.ultimes = c;
        t.ambSo = true;
        auto so = std::make_shared<mibase::AudioPresa>();
        so->fs = o.fs;
        so->canals = o.canals;
        so->longitud = o.longitud;
        for (int ch = 0; ch < o.canals; ++ch) so->dades[ch] = t.buf[ch];
        std::lock_guard<std::mutex> l (mutex);
        if (! feines.count (id)) return;
        sortides.push_back ({ id, nullptr, true, std::move (so) });
    }

    mutable std::mutex mutex;
    juce::WaitableEvent senyal;
    std::map<std::string, Feina> feines;
    std::vector<Sortida> sortides;
    bool cancela = false;
    std::map<std::string, Treball> treballs;   // només el fil de treball

    // Fil de la interfície.
    std::map<std::string, std::shared_ptr<const mibase::veu::Analisi>> analisis;
    mibase::MapaAudios corregidesUI;
};
