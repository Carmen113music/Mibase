#pragma once

#include <cstdint>
#include <string>

namespace mibase
{
    /** Un missatge MIDI per a un instrument, amb la mostra del bloc on ha de sonar (0..n-1). */
    struct EventInstrument
    {
        int offset = 0;
        std::uint8_t estat = 0, dada1 = 0, dada2 = 0;
    };

    /** El que fa sonar una pista MIDI. És el punt d'endoll comú: el sinte intern de Mibase ho és,
        i més endavant els plugins VST3 hi entraran amb un adaptador que compleixi aquesta mateixa interfície.
        El motor (sense JUCE) només coneix aquesta classe.

        Fils: es crea i es prepara fora del fil d'àudio; processa() i apagaTot() només des del fil d'àudio. */
    class Instrument
    {
    public:
        virtual ~Instrument() = default;

        /** Fora del fil d'àudio (abans de sonar o quan canvia la targeta). Pot demanar memòria. */
        virtual void prepara (double fs, int maxBloc) = 0;

        /** Fil d'àudio. Els esdeveniments arriben ordenats per «offset». ESCRIU (no suma) n mostres a L i R.
            No pot demanar memòria ni esperar-se. */
        virtual void processa (const EventInstrument* events, int numEvents, float* L, float* R, int n) noexcept = 0;

        /** Fil d'àudio: totes les notes s'apaguen ara (amb una caiguda curta, sense clic). */
        virtual void apagaTot() noexcept = 0;

        /** Fil d'àudio: encara fa so? (si no en fa i no rep res, el mesclador se'l pot estalviar). */
        virtual bool fentSo() const noexcept { return true; }

        /** Retard propi, en mostres (els VST3 en poden tenir; el sinte intern, cap). */
        virtual int latencia() const noexcept { return 0; }
    };

    /** 0.19: un efecte d'un insert (un plugin VST3 a la pràctica). Mateixes normes de fils que l'Instrument.
        El so és sempre estèreo i es transforma al lloc. */
    class Efecte
    {
    public:
        virtual ~Efecte() = default;
        virtual void prepara (double fs, int maxBloc) = 0;
        /** Fil d'àudio: transforma n mostres de L i R. No pot demanar memòria ni esperar-se. */
        virtual void processa (float* L, float* R, int n) noexcept = 0;
        virtual int latencia() const noexcept { return 0; }
    };
}
