#pragma once

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <mibase/Midi.h>
#include <mibase/MotorEstudi.h>
#include <mibase/Sampler.h>
#include <mibase/SubBass.h>
#include <mibase/Lead.h>
#include <mibase/Atmos.h>
#include <mibase/Neo.h>
#include <mibase/Vintage.h>
#include "Kits.h"
#include "BancGM.h"
#include "Guitarres.h"
#include <mibase/GeneralMidi.h>

/** Els instruments de les pistes MIDI del projecte obert (un per pista, viu mentre la pista hi és).
    És l'únic lloc que sap crear-los: ara el sinte intern; quan arribin els VST3, aquí es crearà
    l'adaptador del plugin (també és un mibase::Instrument) i la resta del programa no canviarà. */
class Instruments
{
public:
    /** El de la pista (el mateix objecte mentre no canviï el tipus d'instrument). Fil de la interfície. */
    std::shared_ptr<mibase::Instrument> obte (const mibase::Pista& t, double fs, int maxBloc)
    {
        if (mibase::esSampler (t))
        {
            // 0.26: el samplejador. Si canvia el so (fitxer, reversa, mono, estirament), un de nou; la resta, sense parar.
            auto& e = llista[t.id];
            const auto sig = std::string ("sampler|") + mibase::sampler::signatura (t.sampler);
            auto* s = dynamic_cast<mibase::sampler::Samplejador*> (e.instrument.get());
            if (s == nullptr || e.tipus != sig)
            {
                e.instrument = creaSampler (t, carpeta, fs, maxBloc, &avis);
                e.tipus = sig;
                e.fs = fs;
            }
            else s->posaParams (t.sampler);
            return e.instrument;
        }
        if (mibase::esKit (t))
        {
            // 0.20: el sampler de bateria. Si canvien els sons, un de nou; si només canvien volums, afinació… s'hi posen sense parar.
            auto& e = llista[t.id];
            const auto sig = mibase::bateria::SamplerBateria::signatura (t);
            auto* s = dynamic_cast<mibase::bateria::SamplerBateria*> (e.instrument.get());
            if (s == nullptr || e.tipus != sig)
            {
                e.instrument = kits::sampler (t, fs, maxBloc);
                e.tipus = sig;
                e.fs = fs;
            }
            else s->posaParams (t.files);
            return e.instrument;
        }
        if (t.tipus != "midi") return nullptr;
        if (mibase::subbass::esSubBass (t) || mibase::lead::esLead (t) || mibase::atmos::esAtmos (t) || mibase::neo::esNeo (t) || mibase::analogic::esVintage (t))
        {
            // 0.88 / 0.89: els sintes de Mibase. El mateix objecte mentre la pista el tingui; si el projecte porta uns
            // ajustos nous (desfer, obrir), s'hi posen sense parar el so.
            const std::string tipus = mibase::analogic::esVintage (t) ? "vintage" : mibase::neo::esNeo (t) ? "neo" : mibase::atmos::esAtmos (t) ? "atmos" : mibase::lead::esLead (t) ? "lead" : "subbass";
            auto& e = llista[t.id];
            if (e.tipus != tipus || e.instrument == nullptr)
            {
                e.instrument = creaSinte (t, fs, maxBloc);
                e.tipus = tipus;
                e.fs = fs;
                e.ultimEstat = t.estatInstrument;
            }
            else if (t.estatInstrument != e.ultimEstat)
            {
                if (auto* s = dynamic_cast<mibase::sinte::Base*> (e.instrument.get()))
                    if (s->estat() != t.estatInstrument && ! t.estatInstrument.empty())
                        s->posaEstat (t.estatInstrument);
                e.ultimEstat = t.estatInstrument;
            }
            e.fs = fs;
            return e.instrument;
        }
        if (mibase::gm::RefSf2 ref; mibase::gm::llegeixRef (t.instrument, ref))
        {
            // 0.43: una guitarra d'un .sf2 (el banc es carrega el primer cop que una pista el fa servir)
            auto& e = llista[t.id];
            const auto sig = t.instrument + "|" + guitarres::signatura (ref.fitxer);
            if (e.instrument == nullptr || e.tipus != sig || e.fs != fs)
            {
                e.instrument = creaSf2 (ref, fs, maxBloc);
                e.tipus = sig;
                e.fs = fs;
            }
            if (auto* sg = dynamic_cast<mibase::gm::SinteGM*> (e.instrument.get())) sg->posaParams (t.guitarra);   // 0.44
            return e.instrument;
        }
        if (const int prog = mibase::gm::programa (t.instrument); prog >= 0)
        {
            // 0.31: General MIDI. Si canvia el so o el banc, un de nou (és lleuger: comparteix les mostres del banc).
            auto& e = llista[t.id];
            const auto sig = t.instrument + "|" + bancgm::signatura();
            if (e.instrument == nullptr || e.tipus != sig)
            {
                e.instrument = creaGM (prog, fs, maxBloc);
                e.tipus = sig;
                e.fs = fs;
            }
            return e.instrument;
        }
        const auto desc = descripcio (t);
        auto& e = llista[t.id];
        const bool intern = mibase::midi::soIntern (desc) >= 0;
        if (e.instrument == nullptr || e.tipus != (intern ? "intern" : desc))
        {
            e.instrument = crea (desc, fs, maxBloc);
            e.tipus = intern ? "intern" : desc;
            e.fs = fs;
        }
        else if (std::abs (e.fs - fs) > 0.5)
        {
            // Ha canviat la targeta: el motor ja l'ha preparat; només ens ho apuntem.
            e.fs = fs;
        }
        return e.instrument;
    }

    /** Proveïdor per al pla que sona. */
    mibase::ProveidorInstruments proveidor (double fs, int maxBloc)
    {
        return [this, fs, maxBloc] (const mibase::Pista& t) { return obte (t, fs, maxBloc); };
    }

    /** Proveïdor per exportar: instruments nous (no toquen els que sonen). */
    static mibase::ProveidorInstruments perExportar (double fs, int bloc, const juce::File& carpetaProjecte)
    {
        return [fs, bloc, carpetaProjecte] (const mibase::Pista& t) -> std::shared_ptr<mibase::Instrument>
        {
            if (mibase::esSampler (t)) return creaSampler (t, carpetaProjecte, fs, bloc, nullptr);
            if (mibase::esKit (t)) return kits::sampler (t, fs, bloc);
            if (t.tipus != "midi") return nullptr;
            if (mibase::subbass::esSubBass (t) || mibase::lead::esLead (t) || mibase::atmos::esAtmos (t) || mibase::neo::esNeo (t) || mibase::analogic::esVintage (t)) return creaSinte (t, fs, bloc);   // 0.88 / 0.89
            if (mibase::gm::RefSf2 ref; mibase::gm::llegeixRef (t.instrument, ref))
            {
                auto s = creaSf2 (ref, fs, bloc);
                if (auto* sg = dynamic_cast<mibase::gm::SinteGM*> (s.get())) sg->posaParams (t.guitarra);
                return s;
            }
            if (const int prog = mibase::gm::programa (t.instrument); prog >= 0) return creaGM (prog, fs, bloc);
            return crea (descripcio (t), fs, bloc);
        };
    }

    /** Oblida els de les pistes que ja no hi són (el pla vell els manté vius mentre calgui). */
    void neteja (const mibase::Projecte& p)
    {
        for (auto it = llista.begin(); it != llista.end();)
        {
            const bool hiEs = std::any_of (p.pistes.begin(), p.pistes.end(), [&] (const mibase::Pista& t) { return t.id == it->first && mibase::esMidi (t); });
            it = hiEs ? std::next (it) : llista.erase (it);
        }
    }

    /** 0.88 / 0.89: el sinte de Mibase viu d'una pista (SUB & BASS o LEAD; nullptr si no en té o encara no sona). */
    std::shared_ptr<mibase::sinte::Base> sinte (const std::string& idPista)
    {
        auto it = llista.find (idPista);
        if (it == llista.end()) return nullptr;
        return std::dynamic_pointer_cast<mibase::sinte::Base> (it->second.instrument);
    }

    /** Copia els ajustos dels sintes vius al projecte (abans de desar, desfer o exportar). */
    bool desaEstats (mibase::Projecte& p)
    {
        bool canvi = false;
        for (auto& t : p.pistes)
            if (mibase::subbass::esSubBass (t) || mibase::lead::esLead (t) || mibase::atmos::esAtmos (t) || mibase::neo::esNeo (t) || mibase::analogic::esVintage (t))
                if (auto s = sinte (t.id))
                {
                    if ((mibase::lead::esLead (t) && dynamic_cast<mibase::lead::Lead*> (s.get()) == nullptr)
                        || (mibase::atmos::esAtmos (t) && dynamic_cast<mibase::atmos::Atmos*> (s.get()) == nullptr)
                        || (mibase::neo::esNeo (t) && dynamic_cast<mibase::neo::Neo*> (s.get()) == nullptr)
                        || (mibase::analogic::esVintage (t) && dynamic_cast<mibase::analogic::Vintage*> (s.get()) == nullptr)
                        || (mibase::subbass::esSubBass (t) && dynamic_cast<mibase::subbass::SubBass*> (s.get()) == nullptr)) continue;
                    auto e = s->estat();
                    if (e != t.estatInstrument) { t.estatInstrument = e; canvi = true; }
                    llista[t.id].ultimEstat = t.estatInstrument;
                }
        return canvi;
    }

    /** 0.26: el samplejador d'una pista, si n'hi ha un de viu (per a l'editor: on és el capçal del so). */
    mibase::sampler::Samplejador* samplejador (const std::string& idPista)
    {
        auto it = llista.find (idPista);
        return it == llista.end() ? nullptr : dynamic_cast<mibase::sampler::Samplejador*> (it->second.instrument.get());
    }

    /** 0.26: la mostra original d'un sample del projecte (llegida un cop; la mateixa mentre el fitxer no canviï). */
    static std::shared_ptr<const mibase::sampler::Mostra> mostraSample (const juce::File& f)
    {
        static std::map<juce::String, std::shared_ptr<const mibase::sampler::Mostra>> cau;
        if (! f.existsAsFile()) return nullptr;
        const auto clau = f.getFullPathName() + "|" + juce::String (f.getLastModificationTime().toMilliseconds());
        if (auto it = cau.find (clau); it != cau.end()) return it->second;
        auto m = kits::llegeix (f, 600.0);
        cau[clau] = m;
        return m;
    }

    static juce::File fitxerSample (const juce::File& carpetaProjecte, const mibase::ParamsSampler& p)
    {
        return carpetaProjecte.getChildFile (juce::String::fromUTF8 (p.fitxer.c_str()));
    }

    juce::File carpeta;       // la carpeta del projecte (per trobar-hi els samples)
    juce::String avis;        // l'últim avís (un sample que falta), per a l'historial

    static std::string descripcio (const mibase::Pista& t) { return t.instrument.empty() ? std::string ("intern:piano") : t.instrument; }

private:
    static std::shared_ptr<mibase::Instrument> creaSampler (const mibase::Pista& t, const juce::File& carpetaProjecte, double fs, int bloc,
                                                            juce::String* avis)
    {
        std::shared_ptr<const mibase::sampler::Mostra> m;
        if (! t.sampler.fitxer.empty())
        {
            const auto f = fitxerSample (carpetaProjecte, t.sampler);
            m = mostraSample (f);
            if (m == nullptr && avis != nullptr)
                *avis = juce::String::fromUTF8 ("Pista «") + juce::String::fromUTF8 (t.nom.c_str())
                        + juce::String::fromUTF8 ("»: no trobo el sample ") + f.getFullPathName() + juce::String::fromUTF8 (" (sona en silenci)");
        }
        auto s = std::make_shared<mibase::sampler::Samplejador> (mibase::sampler::prepara (m, t.sampler));
        s->posaParams (t.sampler);
        s->prepara (fs, bloc);
        return s;
    }

    /** 0.88 / 0.89: un sinte nou de Mibase. Sense ajustos, el primer preset «Analog Bass» (SUB & BASS) o «Classic Lead» (LEAD). */
    static std::shared_ptr<mibase::Instrument> creaSinte (const mibase::Pista& t, double fs, int bloc)
    {
        std::shared_ptr<mibase::sinte::Base> s;
        std::string inicial;
        if (mibase::analogic::esVintage (t))   // 1.4
        {
            s = std::make_shared<mibase::analogic::Vintage>();
            for (const auto& p : mibase::analogic::presetsFabrica()) if (p.nom == "Cinc veus") { inicial = p.estat + ";pnom=" + p.nom; break; }
        }
        else if (mibase::neo::esNeo (t))   // 0.93
        {
            s = std::make_shared<mibase::neo::Neo>();
            for (const auto& p : mibase::neo::presetsFabrica()) if (p.categoria == "Future Pop") { inicial = p.estat + ";pnom=" + p.nom; break; }
        }
        else if (mibase::atmos::esAtmos (t))   // 0.90
        {
            s = std::make_shared<mibase::atmos::Atmos>();
            for (const auto& p : mibase::atmos::presetsFabrica()) if (p.categoria == "Warm Pads") { inicial = p.estat + ";pnom=" + p.nom; break; }
        }
        else if (mibase::lead::esLead (t))
        {
            s = std::make_shared<mibase::lead::Lead>();
            for (const auto& p : mibase::lead::presetsFabrica()) if (p.categoria == "Classic Lead") { inicial = p.estat + ";pnom=" + p.nom; break; }
        }
        else
        {
            s = std::make_shared<mibase::subbass::SubBass>();
            for (const auto& p : mibase::subbass::presetsFabrica()) if (p.categoria == "Analog Bass") { inicial = p.estat + ";pnom=" + p.nom; break; }
        }
        s->posaEstat (t.estatInstrument.empty() || ! s->esEstatMeu (t.estatInstrument) ? inicial : t.estatInstrument);
        s->prepara (fs, bloc);
        return s;
    }

    static std::shared_ptr<mibase::Instrument> creaSf2 (const mibase::gm::RefSf2& ref, double fs, int maxBloc)
    {
        auto s = std::make_shared<mibase::gm::SinteGM> (guitarres::banc (ref.fitxer), ref.programa, ref.banc);
        s->prepara (fs, maxBloc);
        return s;
    }

    static std::shared_ptr<mibase::Instrument> creaGM (int prog, double fs, int maxBloc)
    {
        auto s = std::make_shared<mibase::gm::SinteGM> (bancgm::banc(), prog);
        s->prepara (fs, maxBloc);
        return s;
    }

    static std::shared_ptr<mibase::Instrument> crea (const std::string& desc, double fs, int maxBloc)
    {
        // 0.89: el sinte bàsic ja no hi és: un so antic («intern:piano»…) sona amb el LEAD o el SUB & BASS
        // (normalment el projecte ja s'ha passat en obrir-lo: lead::migraSintesAntics)
        mibase::Pista t;
        t.tipus = "midi";
        const auto id = desc.rfind ("intern:", 0) == 0 ? desc.substr (7) : std::string ("piano");
        t.estatInstrument = mibase::lead::estatPerSoAntic (id, t.instrument);
        return creaSinte (t, fs, maxBloc);
    }

    struct Entrada
    {
        std::shared_ptr<mibase::Instrument> instrument;
        std::string tipus;
        double fs = 0.0;
        std::string ultimEstat;   // 0.88: els ajustos del projecte que ja té l'instrument
    };
    std::map<std::string, Entrada> llista;
};
