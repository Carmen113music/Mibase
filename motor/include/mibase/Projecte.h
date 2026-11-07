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

    /** 0.22: una nota del Vocal Editor dins d'una presa: on és (segons dins el fitxer), què s'hi ha detectat i què
        s'hi ha canviat. L'àudio original no es toca mai: aquests valors diuen com s'ha de transformar. */
    struct NotaVeu
    {
        double ini = 0.0, fi = 0.0;   // segons dins el fitxer
        double centre = 60.0;         // afinació detectada (MIDI amb decimals: 60,25 = C4 + 25 cents)
        double desplac = 0.0;         // semitons sumats al centre (Pitch Center)
        double modulacio = 1.0;       // vibrato: 0 = treure'l, 1 = l'original, 2 = el doble (Pitch Modulation)
        double deriva = 1.0;          // deriva lenta dins la nota: 0 = recta, 1 = l'original (Pitch Drift)
        double formant = 0.0;         // semitons (0 = es conserven els formants originals)
        double guanyDb = 0.0;         // volum de la nota
        bool operator== (const NotaVeu&) const = default;
    };

    /** 0.22: el que el Vocal Editor sap i ha canviat d'una presa. */
    struct VeuPresa
    {
        bool analitzada = false;          // les notes ja s'han detectat (l'anàlisi fina és a «<fitxer>.veu»)
        double transicio = 0.06;          // segons que tarda a passar d'una nota corregida a la següent
        int modeTon = 0;                  // 0.23: on van les notes: 0 tonalitat de la cançó, 1 cromàtic, 2 la triada aquí
        int to = 0, escala = 0;           // 0.23: tonalitat triada (modeTon = 2): to 0..11 i índex de harmonia::escales()
        std::vector<NotaVeu> notes;       // per ordre de temps
        bool operator== (const VeuPresa&) const = default;
    };

    /** Una gravació (un fitxer d'Àudios). Tots els temps, en segons. */
    struct Presa
    {
        std::string id;          // «s1», «s2»…
        std::string fitxer;      // relatiu a la carpeta del projecte: «Àudios/Guitarra - presa 01.wav»
        std::string nom;         // el que es mostra (no canvia el nom del fitxer)
        double durada = 0.0;
        double primer = -1.0;    // bandera del primer temps, dins el fitxer (negatiu: no en té)
        VeuPresa veu;            // 0.22: Vocal Editor
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

    /** 0.19: un plugin d'efecte en un insert (d'una pista o del màster). */
    struct Insert
    {
        std::string id;          // «i1», «i2»… (no canvia: serveix per retrobar el plugin viu)
        std::string plugin;      // identificador del plugin (el de la llista de plugins)
        std::string nom;         // per mostrar-lo encara que no s'hagi pogut obrir
        bool actiu = true;       // apagat: el so hi passa de llarg
        std::string estat;       // ajustos del plugin (base64)
    };

    /** 0.20: una fila del Drumpad (pista de bateria): un so del kit tocat per una nota MIDI. */
    struct FilaBateria
    {
        std::string tipus = "altres";   // kick, snare, hihat… (bateria::tipus())
        std::string so;                 // «intern:kick» o camí relatiu a la carpeta de kits («Kit rock/Kick 01.wav»)
        std::string nom;                // el que es mostra a la fila
        int nota = 36;                  // nota MIDI que la toca (General MIDI)
        double volumDb = 0.0;           // de -40 a +6
        double pan = 0.0;               // de -1 a +1
        double afina = 0.0;             // semitons, de -12 a +12
        double decai = 1.0;             // 0,05..1: quina part del so sona (1 = sencer)
        int talla = 0;                  // grup de tall (0 = cap): un cop d'un grup talla els altres sons del mateix grup
        bool mut = false;
        bool operator== (const FilaBateria&) const = default;
    };

    struct Pista
    {
        std::string id;          // «t1», «t2»…
        std::string nom;
        std::string tipus = "audio";   // audio, midi, bateria (0.20; és MIDI amb el sampler de bateria). Més endavant: bus
        /** Pistes MIDI: qui fa el so. «intern:<so>» (sinte intern de Mibase, per exemple «intern:piano»);
            més endavant «vst3:<identificador>». */
        std::string instrument;
        /** Pistes MIDI: quin teclat MIDI la toca (identificador del dispositiu; buit = el que toca la pista triada)
            i el seu nom (per mostrar-lo encara que ara no estigui connectat). */
        std::string entradaMidi, entradaMidiNom;
        /** Pads d'acords (Chord Pad) de la pista: noms d'acord («Am7», «G/B»…; buit = pad buit) i el preset d'on surten.
            Si no n'hi ha cap, es mostren els acords de la tonalitat. */
        std::vector<std::string> pads;
        std::string presetPads;
        int canals = 1;                 // 1 mono, 2 estèreo
        int color = 0;                  // índex de la paleta de colors de pista
        std::uint32_t rgb = 0;          // color triat a mà (0xAARRGGBB); 0 = el de la paleta
        std::vector<Clip> clips;
        bool mut = false, solo = false, grava = false;
        double faderDb = 0.0;    // de -60 (silenci) a +6
        double pan = 0.0;        // de -1 (tot a l'esquerra) a +1 (tot a la dreta)
        int entrada = 0;         // primera entrada de la targeta per gravar (0 = entrada 1); estèreo: aquesta i la següent
        std::string carpeta;     // 0.11: id de la carpeta on és (buit: cap)
        std::vector<Insert> inserts;           // 0.19: efectes, per ordre
        std::string nomInstrument, estatInstrument;   // 0.19: instrument VST3 («vst3:…»): nom i ajustos (base64)
        // ---- 0.20 ----
        bool escalaTeclat = true;                  // el teclat MIDI d'aquesta pista segueix l'escala (assistit/bloquejat)
        int modeEscala = -1;                       // 0.25: Visual/Assistit/Bloquejat d'aquesta pista (-1: el del projecte, com abans)
        std::vector<FilaBateria> files;            // bateria: les files del Drumpad (de dalt a baix)
        std::string kit;                           // bateria: nom del kit d'on surten els sons
        bool padsPerOrdre = false;                 // bateria: els pads del teclat (notes 36…) toquen les files per ordre
    };

    /** 0.20: és una pista de notes MIDI (instrument o bateria). */
    inline bool esMidi (const Pista& t) noexcept { return t.tipus == "midi" || t.tipus == "bateria"; }

    /** 0.22: identificador de l'insert intern del Vocal Editor. */
    inline constexpr const char* pluginVeu = "intern:veu";
    inline bool esInsertVeu (const Insert& x) noexcept { return x.plugin == pluginVeu; }
    /** La pista té el Vocal Editor encès (fa sonar les preses corregides). */
    inline bool veuActiva (const Pista& t) noexcept
    {
        for (auto& x : t.inserts) if (esInsertVeu (x)) return x.actiu;
        return false;
    }
    inline bool teVeu (const Pista& t) noexcept
    {
        for (auto& x : t.inserts) if (esInsertVeu (x)) return true;
        return false;
    }

    /** Estructura de la cançó (0.15): una llista de parts (peces) per provar ordres diferents sense tocar la línia de temps.
        A cada peça hi pot haver pistes que no sonen. */
    struct PecaEstructura
    {
        std::string part;                    // id de la Part
        std::vector<std::string> silencis;   // ids de les pistes que no sonen en aquesta peça
    };
    struct Estructura
    {
        std::string id;                      // «eN»
        std::string nom;
        std::vector<PecaEstructura> peces;
    };

    /** Carpeta de pistes (0.11): agrupa pistes (bateria, guitarres…). Plegada: a la taula de treball les seves pistes
        es veuen com una sola fila. Amagada: no es veuen a la taula de treball (però sonen). */
    struct Carpeta
    {
        std::string id;          // «fN»
        std::string nom;
        bool plegada = false, amagada = false;
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
        double de = 0, a = 0;    // en compassos interns (0.13: amb decimals, per a seleccions a Temps o Lliure)
    };

    /** Part de la cançó (0.13): intro, estrofa, tornada… marcada a la regla. En compassos interns. */
    struct Part
    {
        std::string id;          // «mN»
        std::string nom;
        double de = 0, a = 0;
        std::uint32_t rgb = 0;
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
        std::vector<Carpeta> carpetes;
        std::vector<Part> parts;
        std::vector<Estructura> estructures;
        std::string estructuraActiva;        // id; buit = lineal
        Bucle bucle;
        Snap snap = Snap::Compas;
        bool clic = true;
        double clicVolum = 1.0;  // 0 a 1,5
        int clicSo = 0;          // 0 digital, 1 fusta, 2 analògic, 3 campana
        double masterDb = 0.0;   // volum de la sortida (de -60 a +6)
        std::vector<Insert> insertsMaster;   // 0.19: cadena de màstering
        int sortidaMaster = 0;   // 0.17: parell de sortides del màster (0 = 1/2, 1 = 3/4…)
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

    /** 0.25: el mode d'escala d'una pista (cada editor té el seu; si la pista no en té, el del projecte). */
    inline ModeEscala modeEscalaDe (const Projecte& p, const Pista& t) noexcept
    {
        return t.modeEscala >= 0 ? (ModeEscala) t.modeEscala : p.modeEscala;
    }
}
