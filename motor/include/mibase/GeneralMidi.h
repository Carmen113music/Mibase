#pragma once

#include "mibase/Instrument.h"

#include <memory>
#include <string>
#include <vector>

namespace mibase::gm
{
    /** 0.31: instruments General MIDI (els 128 sons genèrics) amb un banc de sons SoundFont (.sf2).
        L'instrument d'una pista és «gm:<programa>» (0..127, la numeració GM). Fa servir TinySoundFont (MIT). */

    /** Un banc .sf2 llegit a memòria. Es llegeix un sol cop i el comparteixen totes les pistes. */
    class Banc;

    /** Llegeix un .sf2 (pot trigar un segon o dos: fora del fil d'àudio). nullptr si no es pot llegir. */
    std::shared_ptr<Banc> llegeix (const std::string& cami);
    std::shared_ptr<Banc> llegeixMemoria (const void* dades, int mida);

    /** Quants sons té el banc (presets). */
    int numPresets (const Banc& b);

    /** «gm:12» → 12; qualsevol altra cosa → -1. */
    int programa (const std::string& instrument);
    std::string instrument (int programa);

    /** Nom en català dels 128 programes i de les 16 famílies (8 programes cadascuna). */
    const char* nomPrograma (int programa);
    const char* nomFamilia (int familia);

    /** L'instrument d'una pista. Totes les notes van al mateix programa (no fa cas del canal MIDI).
        Respon a velocitat, sustain (CC64), volum (CC7), expressió (CC11), panorama (CC10) i pitch bend.
        Sense banc sona en silenci. No demana memòria mentre sona (veus reservades a prepara()). */
    class SinteGM : public Instrument
    {
    public:
        SinteGM (std::shared_ptr<Banc> banc, int programa);
        ~SinteGM() override;

        void prepara (double fs, int maxBloc) override;
        void processa (const EventInstrument* events, int numEvents, float* L, float* R, int n) noexcept override;
        void apagaTot() noexcept override;
        bool fentSo() const noexcept override;

        int getPrograma() const noexcept { return prog; }

    private:
        void renderitza (float* L, float* R, int ini, int fi) noexcept;

        std::shared_ptr<Banc> banc;
        void* sinte = nullptr;     // tsf* (una còpia del banc: comparteix les mostres)
        int prog = 0;
        int maxBloc = 0;
        std::vector<float> entrellacat;
    };
}
