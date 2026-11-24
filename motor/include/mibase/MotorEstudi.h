#pragma once

#include <bitset>

#include "mibase/Automatitzacio.h"
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
            /** 0.31.1: els clips de dalt que el tapen amb un fade (en segons absoluts: comença, acaba el fade in,
                comença el fade out, acaba). Mentre el de dalt puja, aquest baixa igual (crossfade complet). */
            struct Tapa { double a0 = 0, a1 = 0, b0 = 0, b1 = 0; };
            std::vector<Tapa> tapes;
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
            std::vector<char> encesAmbRM;                  // 0.69: per a cada insert (mateix ordre): RM no l'apaga
            std::vector<Nota> notes;
            std::vector<Event> events;
            // ---- 0.33: busos i enviaments ----
            int bus = -1;            // és un bus: quin buffer d'entrada té (0..maxBusos-1); -1 = pista normal
            int desti = -1;          // buffer del bus on va el so; -1 = a la sortida
            bool armada = false;     // 0.68: té el botó de gravar encès (amb «Retard mínim», sense plugins que retarden)
            int escoltaEntrada = -1; // 0.71: armada i amb «escolta»: la primera entrada de la targeta que se sent (-1 = cap)
            bool obert = true;       // false: només sonen els seus enviaments (alimenta un retorn en solo)
            struct Envia { int desti = 0; float guanyL = 1.0f, guanyR = 1.0f; int mode = 1; };
            std::vector<Envia> envia;   // els encesos, per ordre
            // ---- 0.56: sidechain ----
            /** Per a cada insert (mateix ordre que «inserts»): buffer de sidechain que rep (-1: cap font). */
            std::vector<int> scInserts;
            /** Còpies cap als buffers de sidechain d'altres efectes. punt: 0 abans dels efectes, 1 després (abans del
                fader), 2 després del fader. */
            struct EnviaSc { int buf = 0; float guany = 1.0f; int punt = 1; };
            std::vector<EnviaSc> scEnvia;
            bool scSempre = false;   // té enviaments de sidechain abans del fader: es calcula encara que estigui callada
            // ---- 0.56: automatització ----
            automatitzacio::Corba autoVolum, autoPan;
            std::shared_ptr<std::atomic<bool>> lliureVolum, lliurePan;   // 0.57: s'hi està gravant: mana el control
            struct AutoParam
            {
                Parametritzable* obj = nullptr;   // viu gràcies a «inserts» o «instrument»
                int index = 0;
                bool instrument = false;
                automatitzacio::Corba corba;
                mutable float ultim = -1.0f;     // el darrer valor enviat (no es torna a enviar si no canvia)
                std::shared_ptr<std::atomic<bool>> lliure;   // 0.57
            };
            std::vector<AutoParam> autoParams;
            bool autoInserts = false;            // algun paràmetre d'un insert: els efectes es calculen a trossos petits
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
        std::vector<char> encesAmbRMMaster;                   // 0.69: RM no l'apaga (mateix ordre)
        std::vector<int> scMaster;                            // 0.56: buffer de sidechain de cada insert del màster (-1: cap)
        int numSidechain = 0;                                 // 0.56: buffers de sidechain en ús
        std::vector<Pista> pistes;
        /** 0.33: en quin ordre s'han de calcular les pistes (índexs de «pistes»): cada una abans dels busos on va.
            Si és buit, per ordre. */
        std::vector<int> ordre;
        int numBusos = 0;
        std::vector<std::shared_ptr<const AudioPresa>> audios;   // perquè no s'esborrin mentre sonen
    };

    constexpr int maxSlots = 128;
    constexpr int maxBusos = 32;        // 0.33: busos (de grup i retorns) que poden sonar alhora
    constexpr int maxSidechain = 32;    // 0.56: efectes amb sidechain que poden sonar alhora

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

    /** 0.91: una pista MIDI que segueix els acords però no té cap nota escrita (ni patró) toca sola l'acord de cada
        moment de la pista d'Acords, sostingut (de cap a fi de l'acord). Aquestes són aquestes notes (en segons). */
    /** 0.94: per clips. Pista sense cap clip MIDI: els acords a tota la cançó. Amb clips, cada clip es val per ell
        mateix: un clip MIDI buit (sense notes ni patrons) toca els acords del seu tros (retallats als seus límits i
        tapats pels clips de sobre); un clip amb notes o patrons fa el seu; un clip silenciat, res; fora dels clips, res.
        tocaAcordsSols: si en algun lloc de la pista sonen els acords sols. */
    bool tocaAcordsSols (const Pista& t);
    std::vector<Pla::Nota> notesAcordsSols (const Projecte& p, const Pista& t);
    /** 0.94: per a «Info → Pista triada»: quants clips MIDI toquen els acords, quants tenen notes i quants patrons. */
    struct ResumAcordsClips { int clips = 0, acords = 0, notes = 0, patrons = 0, muts = 0; };
    ResumAcordsClips resumAcordsClips (const Pista& t);

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
        /** 0.64: per trobar on es perd el so (el fil d'àudio escriu, la interfície llegeix). Els «pic» baixen sols
            (uns 0,5 s) per veure el que passa ara mateix. */
        struct Sonda
        {
            std::atomic<unsigned int> evsAturat { 0 };     // missatges MIDI que han arribat als instruments amb la cançó aturada
            std::atomic<float> picInstrument { 0.0f };     // el que surt dels instruments (abans dels efectes)
            std::atomic<float> picEfectes { 0.0f };        // les pistes d'instrument després dels seus efectes
            std::atomic<float> picMaster { 0.0f };         // la sortida, després de la cadena del màster
            std::atomic<unsigned int> noValids { 0 };      // trossos amb valors no vàlids (NaN, infinit o desmesurats) que s'han silenciat
            std::atomic<int> slotNoValid { -1 };           // l'últim slot que els ha donat (-2 = el màster)
            // 0.68: compensació del retard dels plugins (PDC)
            std::atomic<int> pdcMaxim { 0 };               // la pista que s'avança més (mostres)
            std::atomic<int> pdcSaltats { 0 };             // plugins que retarden apagats ara per «Retard mínim»
        };
        /** 0.68: «Retard mínim»: els plugins que retarden s'apaguen a les pistes que es graven o es toquen en directe,
            als busos i al màster (per no sentir retard als auriculars). */
        void setRetardMinim (bool b) noexcept { retardMinim.store (b, std::memory_order_relaxed); }
        /** 0.69: RM només apaga els plugins que retarden més d'aquestes mostres (Preferències: 5 ms per defecte). */
        void setLlindarRM (int mostres) noexcept { llindarRM.store (std::max (0, mostres), std::memory_order_relaxed); }
        bool getRetardMinim() const noexcept { return retardMinim.load (std::memory_order_relaxed); }
        /** 0.68: la pista (slot) que toca el teclat ara (-1: cap); la posa el motor. */
        void setSlotDirecte (int s) noexcept { slotDirecte = s; }
        /** 0.71: les entrades de la targeta d'aquest tros (per escoltar-se mentre es grava). «mapa»: entrada lògica →
            canal de la targeta; «offset»: on comença el tros dins dels buffers. */
        void setEntrades (const float* const* e, int num, const int* mapa, int numMapa, int offset) noexcept
        {
            entrades = e; numEntrades = num; mapaEnt = mapa; numMapaEnt = numMapa; offsetEnt = offset;
        }
        /** 0.68: quantes mostres s'avança una pista: el retard dels seus plugins, del bus (o busos) on va i del màster. */
        static int retardDe (const std::vector<std::shared_ptr<Efecte>>& v, bool salta, int* saltats = nullptr,
                             const std::vector<char>* fixos = nullptr, int llindar = 0) noexcept;
        Sonda sonda;
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
        /** 0.71: tornar a l'inici del bucle (o saltar de tram): com reinicia(), però les notes que sonen s'apaguen una a
            una (note-off) en lloc d'«apaga-ho tot». Alguns VST3 apliquen l'«apaga-ho tot» després de les notes del mateix
            instant i es menjaven les notes del començament del bucle. */
        void reiniciaBucle() noexcept;
        /** 0.98: el bucle que sona ara, en mostres ([ini, fi); fi <= ini: cap). Les pistes que llegeixen avançat per la
            compensació de retard (PDC) han de tornar a l'inici del bucle abans que el capçal: si no, les notes i el so
            dels primers mil·lisegons del bucle no es llegien mai (es llegia el que hi ha després del final). */
        void setBucle (long long ini, long long fi) noexcept { bucleIniS = ini; bucleFiS = fi; }
        /** Pistes (slots) que no sonen al tram de l'estructura que sona ara (0.15). */
        void setSilenci (const std::bitset<128>& s) noexcept { silenciAra = s; }
        /** 0.83: exportant, la cua final: no comença res de nou (ni clips ni notes), però els efectes continuen sonant. */
        void setNomesCua (bool c) noexcept { nomesCua = c; }
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
        bool nomesCua = false;   // 0.83
        struct NotaActiva { long long fi = 0; std::uint8_t to = 0, canal = 0; };
        struct EstatSlot
        {
            const Instrument* inst = nullptr;
            std::array<NotaActiva, maxNotesActives> act {};
            int n = 0;
            bool apagar = false;
            bool apagarTou = false;   // 0.71
            bool tornat = false;      // 0.98: la lectura avançada (PDC) ja ha tornat a l'inici del bucle
            bool senseMarge = false;  // 0.98: després del salt real, no cal tornar a llegir el mil·lisegon d'abans
        };
        long long bucleIniS = -1, bucleFiS = -1, posReal = 0;   // 0.98
        int escriuDes = 0;                          // 0.98: on escriu sonaTram dins el bloc (per partir-lo en dos)
        /** 0.98: llegeix les notes i els missatges de [pos, pos + n) i els posa a partir de l'offset «base». */
        void llegeixMidi (const Pla::Pista& t, EstatSlot& es, long long pos, int n, int base, bool reproduint, long long marge) noexcept;
        /** 0.98: apaga les notes que sonen (nota per nota) a l'offset «off». */
        void apagaTou (EstatSlot& es, int off) noexcept;
        bool margeInici = false, margeAra = false;
        const float* const* entrades = nullptr;   // 0.71
        int numEntrades = 0, numMapaEnt = 0, offsetEnt = 0;
        const int* mapaEnt = nullptr;   // 0.71: després d'un salt, les notes que cauen una mica abans també
        void instrumentPista (const Pla::Pista& t, long long pos, int n, bool reproduint, const EventDirecte* directe, int numDirecte) noexcept;
        void afegeixEvent (int offset, std::uint8_t estat, std::uint8_t d1, std::uint8_t d2) noexcept;

        void sonaClip (const Pla::Clip& c, int canalsPista, long long pos, int n) noexcept;
        void sonaTram (const Pla::Clip& c, double de, double fins, int canalsPista, long long pos, int n) noexcept;

        double fs = 48000.0;
        std::vector<float> pL, pR;
        // Volums suavitzats per slot: porta (mut/solo), fader, panorama i sortida oberta (0.33: separats, perquè
        // els enviaments surten d'entremig).
        std::array<float, maxSlots> gPorta {}, gFader {}, gPanL {}, gPanR {}, gObert {};
        std::array<bool, maxSlots> gFet {};
        static constexpr int maxEnviaSuau = 8;   // enviaments per pista amb el nivell suavitzat (la resta, a salts)
        std::vector<float> gEnvia;               // maxSlots * maxEnviaSuau * 2 (L, R); negatiu = encara no
        std::vector<float> busL, busR;           // maxBusos * maxN: el que arriba a cada bus en aquest bloc
        int maxN = 16;
        void envia (const Pla::Pista& t, int mode, const float* xL, const float* xR, int n, int nb) noexcept;
        // 0.56
        void enviaSc (const Pla::Pista& t, int punt, const float* xL, const float* xR, int n, int ns) noexcept;
        std::atomic<bool> retardMinim { false };
        std::atomic<int> llindarRM { 0 };
        int slotDirecte = -1;
        void efectes (const Pla::Pista& t, const std::vector<std::shared_ptr<Efecte>>& v, const std::vector<int>& sc, float* xL, float* xR,
                      long long pos, int n, int ns, bool salta = false, const std::vector<char>* fixos = nullptr, int llindar = 0) noexcept;
        std::vector<float> scL, scR;             // maxSidechain * maxN
        Pla::Pista pistaBuida;                   // per als efectes del màster (sense automatització)
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
        /** 0.72 (Punch In): si està sonant, comença a gravar ara mateix, sense aturar ni tornar enrere. «entrada» < 0:
            sense àudio (pista MIDI). Si la pregravació és de la mateixa entrada, el que ha entrat just abans (fins a
            «segonsPregravacio», sense salts del capçal) va al davant del fitxer: és marge per recuperar, fora del clip.
            La gravadora ja ha d'estar engegada (si és d'àudio). */
        void entra (int entrada, int canals);
        /** 0.72 (Punch Out): deixa de gravar (o de comptar) i continua sonant. */
        void surt();
        /** 0.72: l'entrada de la pista armada, que es va guardant mentre sona per a la pregravació del Punch In (-1: cap). */
        void setPregravacio (int entrada, int canals) noexcept
        {
            preEntrada.store (entrada, std::memory_order_relaxed);
            preCanals.store (canals == 2 ? 2 : 1, std::memory_order_relaxed);
        }
        static constexpr double segonsPregravacio = 4.0;
        /** 0.72: de la darrera gravació: on comença el fitxer, on es va començar a gravar de debò (el punch in o el final
            del pre-roll) i on es va deixar de gravar (punch out), en segons de la línia de temps. Els escriu el fil d'àudio. */
        double getIniciFitxer() const noexcept { return iniciFitxer.load (std::memory_order_acquire); }
        double getPuntGravacio() const noexcept { return puntGravacio.load (std::memory_order_acquire); }
        double getFiGravacio() const noexcept { return fiGravacio.load (std::memory_order_acquire); }
        /** 0.81: gravació d'àudio en bucle (el bucle encès mentre es grava): el capçal torna a l'inici del bucle i el fitxer
            continua. Quantes vegades ha tornat, on (segons dins el fitxer) ho va fer el primer cop, quant dura cada volta
            i on comença el bucle. Sense voltes: getVoltes() == 0. */
        int getVoltes() const noexcept { return voltesGrav.load (std::memory_order_acquire); }
        double getPrimeraVolta() const noexcept { return primeraVolta.load (std::memory_order_acquire); }
        double getLlargVolta() const noexcept { return llargVolta.load (std::memory_order_acquire); }
        double getIniciVolta() const noexcept { return iniciVolta.load (std::memory_order_acquire); }

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
        /** 0.62.1 (diagnòstic): el fil d'àudio té un pla per fer sonar? */
        bool tePla() const noexcept { return ambPla.load (std::memory_order_relaxed); }

    private:
        struct Ordre
        {
            enum class Tipus { Reprodueix, Atura, Situa, Grava, Entra, Surt } tipus = Tipus::Atura;
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
    public:
        Mesclador mesclador;   // 0.64: públic per a la sonda del diagnòstic
    private:
        std::vector<float> mixL, mixR;
        std::array<int, maxEntrades> mapaEntrades {};

        Pla* pla = nullptr;    // el que sona (només el toca el fil d'àudio)
        std::atomic<bool> ambPla { false };
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

        // 0.72: pregravació (anell amb l'entrada de la pista armada mentre sona)
        void guardaPregravacio (const float* const* entrades, int numEntrades, int fet, int m) noexcept;
        void escriuPregravacio (long long frames) noexcept;
        std::vector<float> preL, preR;
        long long preMida = 0, preEscriu = 0, preContinus = 0;
        int preEntradaVista = -1, preCanalsVists = 1;
        std::atomic<int> preEntrada { -1 }, preCanals { 1 };
        std::atomic<double> iniciFitxer { 0.0 }, puntGravacio { 0.0 }, fiGravacio { 0.0 };
        std::atomic<int> voltesGrav { 0 };                 // 0.81
        std::atomic<double> primeraVolta { -1.0 }, llargVolta { 0.0 }, iniciVolta { 0.0 };
        long long mostresFitxer = 0;                       // fil d'àudio: mostres escrites al fitxer d'aquesta gravació

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
    /** 0.83: com exportaMescla, amb opcions. Si el pla té estructura (pla.ruta), en segueix els trams per ordre (desDe i
        finsA no compten); si no, de desDe a finsA. «cua»: segons de més al final perquè s'apaguin reverbs i notes (el
        silenci del final es treu, però mai abans de finsA o del final de l'estructura). bits: 16 (amb dither) o 24. */
    bool exportaPla (const Pla& pla, double fs, double desDe, double finsA, int bits, double cua, const std::filesystem::path& desti,
                     std::string& error, const std::function<void (double)>& progres = {}, bool retallaCua = true);
    /** 0.83: opcions de la finestra d'exportar. */
    struct OpcionsExport
    {
        enum Tros { Canco = 0, Bucle = 1, Part = 2 };
        int tros = Canco;
        std::string estructura;   // amb Canco: "" lineal, o l'id d'una estructura
        std::string part;         // amb Part: l'id de la part
        bool efectes = true;      // false: sense inserts (pistes i busos) ni enviaments (el Vocal Editor es queda)
        bool master = true;       // false: sense la cadena del màster (el volum del màster es queda)
    };
    /** 0.83: el projecte a punt per exportar: només sonen «pistes» (índexs; buit = tot, tal com sona ara, amb mutes i
        solos), amb o sense efectes i màster, i el tros [desDe, finsA] que toca ({0, 0} si no hi ha res). */
    Projecte projectePerExportar (const Projecte& p, const OpcionsExport& o, const std::vector<int>& pistes, std::pair<double, double>& tros);
    /** 0.83: quant dura el que s'exportarà (amb estructura, la suma dels trams). */
    double duradaExportacio (const Pla& pla, double desDe, double finsA);
}
