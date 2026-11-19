#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mibase
{
    /** Un missatge MIDI per a un instrument, amb la mostra del bloc on ha de sonar (0..n-1). */
    struct EventInstrument
    {
        int offset = 0;
        std::uint8_t estat = 0, dada1 = 0, dada2 = 0;
    };

    /** 0.56: un paràmetre que un plugin deixa controlar des de fora (automatització, finestres genèriques).
        «id» no canvia mai (es desa al projecte); el valor que es mou és sempre normalitzat 0..1. */
    struct InfoParametre
    {
        std::string id, nom;
        bool automatitzable = true;
        int passos = 0;                  // > 1: valors discrets (selectors, interruptors): l'automatització va a salts
    };

    /** 0.56: base comuna de paràmetres de tots els plugins (interns i VST3, efectes i instruments).
        Sobre aquesta base s'endollen l'automatització i les finestres. Normes de fils:
        - numParametres(), infoParametre(), textParametre(): fil de la interfície.
        - posaParametre(): qualsevol fil; al fil d'àudio no pot demanar memòria ni esperar-se.
        - parametre(): qualsevol fil. */
    class Parametritzable
    {
    public:
        virtual ~Parametritzable() = default;
        virtual int numParametres() const { return 0; }
        virtual InfoParametre infoParametre (int) const { return {}; }
        virtual void posaParametre (int, float) noexcept {}
        virtual float parametre (int) const noexcept { return 0.0f; }
        /** El valor amb les seves unitats («2,4 s», «-12 dB»). */
        virtual std::string textParametre (int i, float v01) const;
        /** Índex d'un paràmetre pel seu id (-1 si no hi és). */
        int indexParametre (const std::string& id) const
        {
            for (int i = 0, n = numParametres(); i < n; ++i) if (infoParametre (i).id == id) return i;
            return -1;
        }
    };

    inline std::string Parametritzable::textParametre (int, float v01) const
    {
        const int pc = (int) (v01 * 100.0f + 0.5f);
        return std::to_string (pc) + " %";
    }

    /** El que fa sonar una pista MIDI. És el punt d'endoll comú: el sinte intern de Mibase ho és,
        i més endavant els plugins VST3 hi entraran amb un adaptador que compleixi aquesta mateixa interfície.
        El motor (sense JUCE) només coneix aquesta classe.

        Fils: es crea i es prepara fora del fil d'àudio; processa() i apagaTot() només des del fil d'àudio. */
    class Instrument : public virtual Parametritzable
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
    class Efecte : public virtual Parametritzable
    {
    public:
        virtual ~Efecte() = default;
        virtual void prepara (double fs, int maxBloc) = 0;
        /** Fil d'àudio: transforma n mostres de L i R. No pot demanar memòria ni esperar-se. */
        virtual void processa (float* L, float* R, int n) noexcept = 0;
        virtual int latencia() const noexcept { return 0; }

        // ---- 0.56: sidechain ----
        /** Té entrada de sidechain (una altra pista en pot controlar la dinàmica). */
        virtual bool admetSidechain() const noexcept { return false; }
        /** Fil d'àudio, just abans de processa(): el so de la cadena lateral per a les n mostres que venen
            (nullptr: cap font connectada; l'efecte fa servir el seu propi so, com sempre). */
        virtual void posaSidechain (const float*, const float*) noexcept {}
    };

    /** 0.56: un efecte intern de Mibase (Reverb, Dinàmica): els ajustos són text i es poden buidar les cues. */
    class EfecteIntern : public Efecte
    {
    public:
        virtual std::string estat() const = 0;
        virtual void posaEstat (const std::string& s) = 0;
        virtual void buida() noexcept {}
    };
}
