#pragma once

#include <bitset>

#include "mibase/ClicDeProva.h"
#include "mibase/Edicio.h"
#include "mibase/CuaSenseBloqueig.h"
#include "mibase/Gravadora.h"
#include "mibase/Instrument.h"
#include "mibase/Mesurador.h"
#include "mibase/Projecte.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace mibase
{
    /** L'àudio d'una presa a la memòria, per sonar. Un cop fet no es toca mai més. */
    struct AudioPresa
    {
        double fs = 48000.0;
        int canals = 1;                  // 1 o 2 (si el fitxer en té més, només els dos primers)
        long long longitud = 0;          // frames
        std::vector<float> dades[2];     // un vector per canal

        double durada() const noexcept { return fs > 0.0 ? (double) longitud / fs : 0.0; }
    };

    using MapaAudios = std::map<std::string, std::shared_ptr<const AudioPresa>>;

    /** El que el fil d'àudio necessita per fer sonar el projecte. El fil de la interfície en fa un de nou cada
        cop que canvia alguna cosa i l'envia al motor; el fil d'àudio només el llegeix. Tot en segons. */
    struct Pla
    {
        struct Clip
        {
            const AudioPresa* audio = nullptr;
            double pos = 0.0, inici = 0.0, fi = 0.0, fadeIn = 0.0, fadeOut = 0.0;
            float guany = 1.0f;
            /** Trossos de la línia de temps on sona de debò: un clip posat més amunt (més tard a la llista) el tapa. */
            std::vector<std::pair<double, double>> trams;
        };

        /** Una nota MIDI, ja en segons absoluts i retallada a la finestra del clip (i al que no tapa un altre clip). */
        struct Nota
        {
            double ini = 0.0, fi = 0.0;
            std::uint8_t to = 60, vel = 100, canal = 0;
        };

        /** Un altre missatge MIDI (controladors, pitch bend…), en segons absoluts. */
        struct Event
        {
            double t = 0.0;
            std::uint8_t estat = 0xB0, dada1 = 0, dada2 = 0;
        };

        struct Pista
        {
            int slot = 0;            // canal de la taula (per suavitzar el volum i per al vúmetre)
            int canals = 1;
            float guany = 1.0f;      // fader, lineal
            float pan = 0.0f;        // -1..1
            bool sona = true;        // ni silenciada ni tapada per un solo
            std::vector<Clip> clips;

            /** Pistes MIDI: l'instrument (el fa servir el fil d'àudio; la interfície el manté viu) i les notes ordenades. */
            std::shared_ptr<Instrument> instrument;
            std::vector<std::shared_ptr<Efecte>> inserts;   // 0.19: els actius, per ordre
            std::vector<Nota> notes;
            std::vector<Event> events;
        };

        double tempo = 120.0;
        std::vector<edicio::TramTempo> mapaTempo;   // 0.28: per a la claqueta (si hi ha canvis de tempo per parts)
        int compasNum = 4;
        bool clic = true;
        float clicVolum = 1.0f;
        int clicSo = 0;
        bool bucle = false;
        double bucleIni = 0.0, bucleFi = 0.0;
        /** 0.15: estructura (si no és buida): els trams de la línia de temps que sonen, per ordre, i les pistes
            (slots) que no sonen a cada tram. El capçal salta d'un tram al següent; al final, s'atura. */
        struct Tram { double ini = 0.0, fi = 0.0; std::bitset<128> silenci; };
        std::vector<Tram> ruta;
        float master = 1.0f;
        int sortidaMaster = 0;   // primer canal = 2 * sortidaMaster
        std::vector<std::shared_ptr<Efecte>> insertsMaster;   // 0.19: cadena de màstering (abans del volum del màster)
        std::vector<Pista> pistes;
        std::vector<std::shared_ptr<const AudioPresa>> audios;   // perquè no s'esborrin mentre sonen
    };

    constexpr int maxSlots = 128;

    /** Canal de la taula de cada pista (a partir del seu identificador, que no canvia). */
    int slotDePista (const Pista& t) noexcept;

    /** Qui dona l'instrument de cada pista MIDI (fil de la interfície). Ha de tornar sempre el mateix objecte per a la mateixa
        pista mentre no canviï el so, perquè les notes que sonen no es tallin. nullptr: la pista no sona. */
    using ProveidorInstruments = std::function<std::shared_ptr<Instrument> (const Pista&)>;
    /** 0.19: qui dona el plugin viu de cada insert (nullptr si no s'ha pogut obrir: el so hi passa de llarg). */
    using ProveidorEfectes = std::function<std::shared_ptr<Efecte> (const Insert&)>;

    /** Fa el pla del projecte (fil de la interfície). Els clips sense àudio a la memòria no sonen. */
    /** «corregides» (0.22): preses passades pel Vocal Editor; les fan servir les pistes amb el Vocal Editor encès. */
    std::unique_ptr<Pla> construeixPla (const Projecte& p, const MapaAudios& audios, const ProveidorInstruments& instruments = {},
                                     const ProveidorEfectes& efectes = {}, const MapaAudios* corregides = nullptr);

    /** Un missatge MIDI en directe (del piano roll o del teclat) per a l'instrument d'una pista. */
    struct EventDirecte
    {
        int slot = -1;
        std::uint8_t estat = 0, dada1 = 0, dada2 = 0;
    };

    /** Guany lineal d'un valor en dB (-60 o menys = silenci). */
    float guanyDb (double db) noexcept;

    /** Mescla un pla: suma pistes i claqueta. El fan servir el motor (en directe) i l'exportació (fora de línia). */
    class Mesclador
    {
    public:
        void prepara (double fs, int maxBloc);
        /** Suma a L i R «n» mostres a partir de la mostra «pos» de la línia de temps (pot ser negativa).
            «vus»: un Mesurador per slot i canal (2 * maxSlots), o nullptr.
            «reproduint»: false quan el transport és aturat (només sonen els instruments, amb el que es toca en directe).
            «directe»: missatges en directe per als instruments (sonen al començament del bloc). */
        void processa (const Pla& pla, long long pos, int n, float* L, float* R, bool ambClic, Mesurador* vus,
                       bool reproduint = true, const EventDirecte* directe = nullptr, int numDirecte = 0) noexcept;
        /** Torna a començar (el volum salta directament al valor, sense pujar a poc a poc) i apaga les notes que sonen
            (play, situar el capçal, tornar a l'inici del bucle). */
        void reinicia() noexcept;
        /** Pistes (slots) que no sonen al tram de l'estructura que sona ara (0.15). */
        void setSilenci (const std::bitset<128>& s) noexcept { silenciAra = s; }
        /** Apaga les notes MIDI que sonen (en aturar). */
        void apagaNotes() noexcept;
        /** Fil d'àudio, just abans de canviar de pla: els instruments que ja no hi seran (o han canviat) s'apaguen
            i s'obliden les seves notes. */
        void canviDePla (const Pla* vell, const Pla* nou) noexcept;
        /** Quantes notes té enceses ara mateix l'instrument d'una pista (per a les proves). */
        int notesEnceses (int slot) const noexcept { return slots.empty() ? 0 : slots[(size_t) (slot % maxSlots)].n; }

        static constexpr int maxNotesActives = 128;
        static constexpr int maxEventsBloc = 1024;

    private:
        std::bitset<128> silenciAra;
        struct NotaActiva { long long fi = 0; std::uint8_t to = 0, canal = 0; };
        struct EstatSlot
        {
            const Instrument* inst = nullptr;
            std::array<NotaActiva, maxNotesActives> act {};
            int n = 0;
            bool apagar = false;
        };
        void instrumentPista (const Pla::Pista& t, long long pos, int n, bool reproduint, const EventDirecte* directe, int numDirecte) noexcept;
        void afegeixEvent (int offset, std::uint8_t estat, std::uint8_t d1, std::uint8_t d2) noexcept;

        void sonaClip (const Pla::Clip& c, int canalsPista, long long pos, int n) noexcept;
        void sonaTram (const Pla::Clip& c, double de, double fins, int canalsPista, long long pos, int n) noexcept;

        double fs = 48000.0;
        std::vector<float> pL, pR;
        std::array<float, maxSlots> gL {}, gR {};
        std::array<bool, maxSlots> gFet {};
        float gMaster = 1.0f;
        bool masterFet = false;
        ClicDeProva clic;
        std::vector<EstatSlot> slots;
        std::vector<EventInstrument> evs;
        int numEvs = 0;
    };

    /** El motor de l'estudi: fa sonar el projecte, compta abans de gravar i grava.
        No depèn de JUCE ni de Windows: es prova sol. */
    class MotorEstudi
    {
    public:
        static constexpr int maxEntrades = 32;
        enum class Estat { Aturat = 0, Sonant, Comptant, Gravant };

        MotorEstudi();
        ~MotorEstudi();

        // ---- Fora del fil d'àudio, abans que soni (quan arrenca la targeta) ----
        void prepara (double fs, int maxBloc);
        /** Per a cada entrada de la targeta (0 = entrada 1), quina posició té a la crida d'àudio (o -1 si està apagada). */
        void setMapaEntrades (const std::vector<int>& mapa);
        double getFs() const noexcept { return fs; }

        // ---- Fil de la interfície ----
        /** El pla nou que ha de sonar. El motor se'n fa càrrec. */
        void publica (std::unique_ptr<Pla> pla);
        /** Esborra els plans que el fil d'àudio ja no fa servir. Cal cridar-ho sovint. */
        void recull();
        void reprodueix (double desDe);
        void atura();
        void situa (double t);
        /** Comença a sonar a «desDe» i a gravar a la vegada. Fins a «cursor» és el compte d'entrada
            (la claqueta sona encara que estigui apagada). La gravadora ja ha d'estar engegada. */
        void grava (double desDe, double cursor, int entrada, int canals);
        Gravadora& gravadora() noexcept { return grav; }

        // ---- MIDI en directe ----
        /** Fil de la interfície: una nota (o un altre missatge) per a l'instrument d'una pista, ara (piano roll, teclat de pantalla). */
        bool enviaDirecte (const EventDirecte& e) noexcept { return directeUI.posa (e); }
        /** A quina pista (slot) va el teclat MIDI de fora. -1: a cap (el teclat fa sonar el so de prova del Diagnòstic). */
        void setSlotTeclat (int slot) noexcept { slotTeclat.store (slot, std::memory_order_relaxed); }
        int getSlotTeclat() const noexcept { return slotTeclat.load (std::memory_order_relaxed); }
        /** Fil del MIDI (un sol productor alhora): un missatge del teclat de fora. Retorna false si no hi ha cap pista que l'esperi.
            Els note-off van sempre a la mateixa pista que el seu note-on, encara que mentrestant s'hagi canviat de pista. */
        /** «desti»: -2 = la pista del teclat (setSlotTeclat); -1 = cap (al Diagnòstic); >= 0 = aquesta pista (un teclat
            assignat a una pista concreta). */
        bool enviaTeclat (std::uint8_t estat, std::uint8_t d1, std::uint8_t d2, int desti = -2) noexcept;

        /** Un missatge del teclat gravat (temps en segons de la línia de temps). */
        struct MidiGravat
        {
            double t = 0.0;
            std::uint8_t estat = 0, dada1 = 0, dada2 = 0;
        };
        /** Grava el MIDI que arriba del teclat a la pista «slot» (mentre el transport grava). -1: no. */
        void setSlotGravaMidi (int slot) noexcept { slotGravaMidi.store (slot, std::memory_order_relaxed); }
        /** Fil de la interfície: el que s'ha gravat del teclat. */
        bool treuMidiGravat (MidiGravat& m) noexcept { return gravats.treu (m); }

        Estat getEstat() const noexcept { return (Estat) estat.load (std::memory_order_acquire); }
        double getPosicio() const noexcept { return posicio.load (std::memory_order_relaxed); }
        /** Peça de l'estructura que sona (-1: lineal o aturat). */
        int getTramRuta() const noexcept { return tramPublic.load (std::memory_order_relaxed); }

        Mesurador& vuPista (int slot, int canal) noexcept { return vus[(size_t) ((slot % maxSlots) * 2 + (canal & 1))]; }
        Mesurador& vuSortida (int canal) noexcept { return vusSortida[(size_t) (canal & 1)]; }
        Mesurador& vuEntrada (int entrada) noexcept { return vusEntrada[(size_t) std::clamp (entrada, 0, maxEntrades - 1)]; }

        // ---- Fil d'àudio ----
        /** SUMA el so a les sortides (no les esborra). No demana memòria ni s'espera mai. */
        void processa (const float* const* entrades, int numEntrades, float* const* sortides, int numSortides, int n) noexcept;

    private:
        struct Ordre
        {
            enum class Tipus { Reprodueix, Atura, Situa, Grava } tipus = Tipus::Atura;
            double t = 0.0, t2 = 0.0;
            int a = 0, b = 0;
        };

        void ordres() noexcept;
        void sumaSortida (float* const* sortides, int numSortides, int fet, int m) noexcept;
        void entraARuta() noexcept;
        int tramRuta = -1;           // tram de l'estructura que sona (-1: lineal)
        std::atomic<int> tramPublic { -1 };

        double fs = 48000.0;
        int maxBloc = 512;
        Mesclador mesclador;
        std::vector<float> mixL, mixR;
        std::array<int, maxEntrades> mapaEntrades {};

        Pla* pla = nullptr;    // el que sona (només el toca el fil d'àudio)
        CuaSenseBloqueig<Pla*, 64> nous;
        CuaSenseBloqueig<Pla*, 1024> vells;
        CuaSenseBloqueig<Ordre, 64> cuaOrdres;
        std::vector<std::unique_ptr<Pla>> pendents;   // fil de la interfície: plans que no han cabut a la cua

        std::atomic<int> estat { 0 };
        std::atomic<double> posicio { 0.0 };
        long long pos = 0;           // mostra de la línia de temps (fil d'àudio)
        long long compteFins = 0;    // fins aquí, compte d'entrada
        int entradaGrava = 0, canalsGrava = 1;

        Gravadora grav;

        CuaSenseBloqueig<EventDirecte, 512> directeUI, directeTeclat;
        std::array<EventDirecte, 512> directeBloc {};
        int numDirecte = 0;
        std::atomic<int> slotTeclat { -1 }, slotGravaMidi { -1 };
        std::array<int, 128> slotNotaTeclat {};     // fil del MIDI: a quina pista ha anat cada nota del teclat
        CuaSenseBloqueig<MidiGravat, 4096> gravats;

        std::array<Mesurador, maxSlots * 2> vus;
        std::array<Mesurador, 2> vusSortida;
        std::array<Mesurador, maxEntrades> vusEntrada;
    };

    /** Tros que s'exporta: el bucle si és encès; si no, del començament del primer clip que sona al final del darrer. */
    std::pair<double, double> trosPerExportar (const Projecte& p);

    /** Exporta la mescla de [desDe, finsA) a un WAV estèreo de 24 bits (sense la claqueta). */
    bool exportaMescla (const Pla& pla, double fs, double desDe, double finsA, const std::filesystem::path& desti, std::string& error,
                        const std::function<void (double)>& progres = {});
}
