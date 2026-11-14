#pragma once

#include "mibase/Instrument.h"
#include "mibase/Projecte.h"
#include <atomic>
#include <cstdint>

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

    // ---- 0.43: sons de qualsevol .sf2 (les guitarres d'en Roger) ----
    /** L'instrument d'una pista pot ser «sf2:<fitxer>|<banc>|<programa>|<nom>»: un so concret d'un .sf2 de la carpeta
        de guitarres. El nom hi va per poder-lo ensenyar sense obrir el fitxer. */
    struct InfoPreset { std::string nom; int banc = 0, programa = 0; };
    struct RefSf2 { std::string fitxer, nom; int banc = 0, programa = 0; };
    /** Els sons que porta un .sf2, sense carregar-ne les mostres (només en llegeix la llista). */
    std::vector<InfoPreset> presetsDeFitxer (const std::string& cami);
    bool llegeixRef (const std::string& instrument, RefSf2& r);
    std::string instrumentSf2 (const RefSf2& r);

    /** Nom en català dels 128 programes i de les 16 famílies (8 programes cadascuna). */
    const char* nomPrograma (int programa);
    const char* nomFamilia (int familia);

    /** L'instrument d'una pista. Totes les notes van al mateix programa (no fa cas del canal MIDI).
        Respon a velocitat, sustain (CC64), volum (CC7), expressió (CC11), panorama (CC10) i pitch bend.
        Sense banc sona en silenci. No demana memòria mentre sona (veus reservades a prepara()). */
    class SinteGM : public Instrument
    {
    public:
        SinteGM (std::shared_ptr<Banc> banc, int programa, int bancMidi = 0);   // 0.43: «bancMidi», per als .sf2 amb diversos bancs
        ~SinteGM() override;

        void prepara (double fs, int maxBloc) override;
        void processa (const EventInstrument* events, int numEvents, float* L, float* R, int n) noexcept override;
        void apagaTot() noexcept override;
        bool fentSo() const noexcept override;

        int getPrograma() const noexcept { return prog; }
        /** 0.44: controls de la guitarra virtual (es poden canviar mentre sona; fil de la interfície). */
        void posaParams (const ParamsGuitarra& p) noexcept;

    private:
        void renderitza (float* L, float* R, int ini, int fi) noexcept;

        std::shared_ptr<Banc> banc;
        void* sinte = nullptr;     // tsf* (una còpia del banc: comparteix les mostres)
        int prog = 0, bancMidi = 0;
        int maxBloc = 0;
        std::vector<float> entrellacat;
        std::atomic<float> pVolum { 1.0f }, pTo { 1.0f }, pAtac { 0.0f }, pSustain { 0.5f }, pAfina { 0.0f };
        std::atomic<int> pOctava { 0 };
        std::uint8_t teclaSonant[128] {};   // amb quina tecla (després de l'octava) s'ha encès cada nota
        float volumAra = 1.0f, afinaAra = 0.0f, lpL = 0.0f, lpR = 0.0f;
        double fsAra = 48000.0;
    };
}
