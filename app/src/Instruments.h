#pragma once

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <string>
#include <mibase/Midi.h>
#include <mibase/MotorEstudi.h>
#include <mibase/SinteIntern.h>

/** Els instruments de les pistes MIDI del projecte obert (un per pista, viu mentre la pista hi és).
    És l'únic lloc que sap crear-los: ara el sinte intern; quan arribin els VST3, aquí es crearà
    l'adaptador del plugin (també és un mibase::Instrument) i la resta del programa no canviarà. */
class Instruments
{
public:
    /** El de la pista (el mateix objecte mentre no canviï el tipus d'instrument). Fil de la interfície. */
    std::shared_ptr<mibase::Instrument> obte (const mibase::Pista& t, double fs, int maxBloc)
    {
        if (t.tipus != "midi") return nullptr;
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
        if (intern)
            if (auto* s = dynamic_cast<mibase::SinteIntern*> (e.instrument.get()))
                s->setSo (mibase::midi::soIntern (desc));   // canviar de so no talla res
        return e.instrument;
    }

    /** Proveïdor per al pla que sona. */
    mibase::ProveidorInstruments proveidor (double fs, int maxBloc)
    {
        return [this, fs, maxBloc] (const mibase::Pista& t) { return obte (t, fs, maxBloc); };
    }

    /** Proveïdor per exportar: instruments nous (no toquen els que sonen). */
    static mibase::ProveidorInstruments perExportar (double fs, int bloc)
    {
        return [fs, bloc] (const mibase::Pista& t) -> std::shared_ptr<mibase::Instrument>
        {
            if (t.tipus != "midi") return nullptr;
            return crea (descripcio (t), fs, bloc);
        };
    }

    /** Oblida els de les pistes que ja no hi són (el pla vell els manté vius mentre calgui). */
    void neteja (const mibase::Projecte& p)
    {
        for (auto it = llista.begin(); it != llista.end();)
        {
            const bool hiEs = std::any_of (p.pistes.begin(), p.pistes.end(), [&] (const mibase::Pista& t) { return t.id == it->first && t.tipus == "midi"; });
            it = hiEs ? std::next (it) : llista.erase (it);
        }
    }

    static std::string descripcio (const mibase::Pista& t) { return t.instrument.empty() ? std::string ("intern:piano") : t.instrument; }

private:
    static std::shared_ptr<mibase::Instrument> crea (const std::string& desc, double fs, int maxBloc)
    {
        // Més endavant: «vst3:…» → adaptador del plugin.
        auto s = std::make_shared<mibase::SinteIntern> (std::max (0, mibase::midi::soIntern (desc)));
        s->prepara (fs, maxBloc);
        return s;
    }

    struct Entrada
    {
        std::shared_ptr<mibase::Instrument> instrument;
        std::string tipus;
        double fs = 0.0;
    };
    std::map<std::string, Entrada> llista;
};
