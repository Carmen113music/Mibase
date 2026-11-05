#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mibase
{
    /** Graella on s'enganxen els clips. */
    enum class Snap { Compas = 0, Temps = 1, Lliure = 2 };

    /** Resolució del MIDI: ticks per negra. Les notes es guarden en ticks (temps musical):
        si canvia el tempo, les notes el segueixen. */
    constexpr int ticksPerNegra = 960;

    /** Una nota MIDI d'un clip. Temps en ticks, des del començament del clip (Clip::pos). */
    struct Nota
    {
        long long ini = 0;       // ticks
        long long dur = 240;     // ticks (mínim 1)
        int to = 60;             // 0..127 (60 = C4, el do central)
        int vel = 100;           // 1..127
        int canal = 0;           // 0..15
        bool operator== (const Nota&) const = default;
    };

    /** Un altre esdeveniment MIDI d'un clip (controladors, pitch bend, aftertouch…), en ticks des de Clip::pos.
        «tipus» és el nibble alt de l'estat MIDI: 0xB0 controlador, 0xE0 pitch bend (valor 0..16383, centre 8192),
        0xD0 aftertouch de canal, 0xA0 aftertouch polifònic (num = nota), 0xC0 canvi de programa.
        Es conserven (per exemple, d'un MIDI importat) i sonen; l'editor d'expressió arribarà més endavant. */
    struct EventMidi
    {
        long long tick = 0;
        int tipus = 0xB0;
        int num = 0;
        int valor = 0;
        int canal = 0;
        bool operator== (const EventMidi&) const = default;
    };

    /** Una gravació (un fitxer d'Àudios). Tots els temps, en segons. */
    struct Presa
    {
        std::string id;          // «s1», «s2»…
        std::string fitxer;      // relatiu a la carpeta del projecte: «Àudios/Guitarra - presa 01.wav»
        std::string nom;         // el que es mostra (no canvia el nom del fitxer)
        double durada = 0.0;
        double primer = -1.0;    // bandera del primer temps, dins el fitxer (negatiu: no en té)
    };

    /** Un tros d'una presa col·locat a la línia de temps (una «finestra»). No toca mai el fitxer.
        Sense presa (presa buida) és una finestra buida, feta amb el llapis. */
    struct Clip
    {
        std::string presa;
        bool mut = false;        // silenciat amb l'eina X
        double pos = 0.0;        // on cau l'inici del FITXER a la línia de temps
        double inici = 0.0;      // finestra visible dins el fitxer (pot sortir de 0..durada: és silenci)
        double fi = 0.0;
        double fadeIn = 0.0, fadeOut = 0.0;
        double guanyDb = 0.0;    // de -24 a 0
        std::uint32_t rgb = 0;   // color propi (0xAARRGGBB); 0 = el de la pista

        // ---- Clip MIDI (pistes de tipus «midi») ----
        /** És un clip MIDI: les notes són a «notes» (ticks des de «pos») i «inici»/«fi» és la finestra que sona.
            Com l'àudio, no destructiu: el que queda fora de la finestra no sona però no s'esborra. */
        bool midi = false;
        std::vector<Nota> notes;
        std::vector<EventMidi> events;

        double iniciVisible() const noexcept { return pos + inici; }
        double fiVisible() const noexcept    { return pos + fi; }
        double longitud() const noexcept     { return fi - inici; }
    };

    struct Pista
    {
        std::string id;          // «t1», «t2»…
        std::string nom;
        std::string tipus = "audio";   // audio, midi (més endavant: bateria, acords, bus)
        /** Pistes MIDI: qui fa el so. «intern:<so>» (sinte intern de Mibase, per exemple «intern:piano»);
            més endavant «vst3:<identificador>». */
        std::string instrument;
        int canals = 1;                 // 1 mono, 2 estèreo
        int color = 0;                  // índex de la paleta de colors de pista
        std::uint32_t rgb = 0;          // color triat a mà (0xAARRGGBB); 0 = el de la paleta
        std::vector<Clip> clips;
        bool mut = false, solo = false, grava = false;
        double faderDb = 0.0;    // de -60 (silenci) a +6
        double pan = 0.0;        // de -1 (tot a l'esquerra) a +1 (tot a la dreta)
        int entrada = 0;         // primera entrada de la targeta per gravar (0 = entrada 1); estèreo: aquesta i la següent
    };

    /** Tonalitat: nota fonamental (0 = C … 11 = B) i escala (índex de harmonia::escales()). */
    struct Tonalitat
    {
        int to = 0;
        int escala = 0;
        bool operator== (const Tonalitat&) const = default;
    };

    /** Un canvi de tonalitat a partir d'un compàs (intern). Preparat per a més endavant: ara només compta la tonalitat del projecte. */
    struct CanviTonalitat
    {
        int compas = 0;
        Tonalitat tonalitat;
    };

    /** Com treballa el Piano Roll amb l'escala: 0 visual (només ressalta), 1 assistit (ajusta a la nota de l'escala
        més propera), 2 bloquejat (no deixa posar notes fora de l'escala). */
    enum class ModeEscala { Visual = 0, Assistit = 1, Bloquejat = 2 };

    /** Bucle per compassos sencers (índexs interns, des de 0): [de, a). */
    struct Bucle
    {
        bool marcat = false, actiu = false;
        int de = 0, a = 0;
    };

    struct Projecte
    {
        std::string nom;
        double tempo = 120.0;
        int compasNum = 4;
        int compasDen = 4;
        std::string plantilla = "buit";
        std::string creat;       // ISO: 2026-11-03T09:45:00
        std::string modificat;

        std::vector<Presa> preses;
        std::vector<Pista> pistes;
        Bucle bucle;
        Snap snap = Snap::Compas;
        bool clic = true;
        double clicVolum = 1.0;  // 0 a 1,5
        int clicSo = 0;          // 0 digital, 1 fusta, 2 analògic, 3 campana
        double masterDb = 0.0;   // volum de la sortida (de -60 a +6)
        int comptador = 0;       // per fer identificadors únics

        // ---- Harmonia (0.8) ----
        /** true: la tonalitat es dedueix de les notes MIDI (si n'hi ha prou); false: l'ha triada en Roger. */
        bool tonalitatAuto = true;
        Tonalitat tonalitat;                      // la triada (o la darrera detectada)
        ModeEscala modeEscala = ModeEscala::Visual;
        bool ressaltaAcord = true;                // pinta les notes de l'acord que sona al capçal
        std::vector<CanviTonalitat> canvisTonalitat;   // per a més endavant (es desen i es llegeixen)

        /** El JSON tal com s'ha llegit: els camps que aquesta versió no coneix es conserven en desar. */
        std::string jsonOriginal;
    };
}
