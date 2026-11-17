#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <mibase/Instrument.h>
#include <mibase/Projecte.h>

/** 0.20: la bateria (Drumpad). Una pista de bateria és una pista MIDI: cada cop és una nota (General MIDI:
    36 bombo, 38 caixa, 42 hihat…) i el so el fa el SamplerBateria amb els sons d'un kit.
    Aquí hi ha els tipus de so, la classificació dels fitxers (pel nom i, si no, pel so), els kits al disc,
    les eines d'edició (humanitzar, swing, redobles, flams), els ritmes de partida i els «fills». */
namespace mibase::bateria
{
    /** Un tipus de so de bateria. */
    struct Tipus
    {
        const char* id;      // «kick»
        const char* nom;     // «Bombo»
        int nota;            // General MIDI
        int talla;           // grup de tall per defecte (1 = hihats)
    };
    const std::vector<Tipus>& tipus();
    const Tipus* tipusPerId (const std::string& id);
    std::string nomTipus (const std::string& id);
    int notaDeTipus (const std::string& id);

    /** Pel nom del fitxer (sense la carpeta): «kick», «snare»… o buit si el nom no ho diu. */
    std::string classificaNom (const std::string& nomFitxer);
    /** Pel so (mono): mira la llargada, on hi ha l'energia (greus, aguts) i el soroll. Sempre en torna un. */
    std::string classificaSo (const float* mostres, int n, double fs);

    // ---- Kits al disc ----
    struct SoKit
    {
        std::string fitxer;  // dins la carpeta del kit
        std::string nom;     // per mostrar (el nom del fitxer sense extensió)
        std::string tipus;
        std::string original;   // d'on es va importar (informatiu)
    };
    struct Kit
    {
        std::string nom;
        std::string carpeta;    // camí sencer (UTF-8)
        std::vector<SoKit> sons;
        /** 0.31: files desades amb el kit («Desa el kit actual»): l'ordre, les notes i els ajustos de cada fila.
            El camp «so» és el fitxer dins la carpeta del kit. Si n'hi ha, posar el kit a una pista posa exactament aquestes files. */
        std::vector<FilaBateria> files;
    };
    /** El kit d'una carpeta (llegeix «kit.json»; si no n'hi ha, el fa amb els WAV que hi hagi, classificats pel nom). */
    std::optional<Kit> llegeixKit (const std::string& carpeta);
    bool desaKit (const Kit& k);
    /** Tots els kits de dins la carpeta dels kits (una subcarpeta per kit), per ordre alfabètic. */
    std::vector<Kit> llegeixKits (const std::string& arrel);
    /** Nom del kit intern (sons sintetitzats, sempre disponible). */
    constexpr const char* kitIntern = "Intern";

    /** Files per a una pista amb aquest kit. Una per tipus que el kit tingui (el primer so de cada tipus), per l'ordre de tipus().
        Si hi havia files («actuals»), es mantenen (volum, afinació…) i només canvien de so; les que el kit nou no té es queden. */
    std::vector<FilaBateria> filesDeKit (const Kit& k, const std::vector<FilaBateria>& actuals = {});
    /** Les files del kit intern. */
    std::vector<FilaBateria> filesInternes();
    /** Camí relatiu a la carpeta dels kits: «Kit/so.wav». */
    std::string camiSo (const Kit& k, const SoKit& s);

    // ---- So ----
    /** Un so carregat a la memòria (estèreo; si és mono, R és buit). */
    struct Mostra
    {
        std::vector<float> L, R;
        double fs = 44100.0;
    };
    /** Sons del kit intern, sintetitzats («intern:kick»…). nullptr si el tipus no hi és. */
    std::shared_ptr<const Mostra> mostraInterna (const std::string& so, double fs);

    /** El que fa sonar una pista de bateria. Cada nota té el seu so (fixat en crear-lo) i uns paràmetres
        (volum, pan, afinació, decaïment, tall, silenci) que es poden canviar mentre sona (atòmics).
        Una nota sona sencera (one-shot): les apagades no fan res. Un cop d'un grup de tall apaga (amb 5 ms) els
        altres sons del mateix grup: el hihat tancat talla l'obert. */
    class SamplerBateria : public Instrument
    {
    public:
        struct So
        {
            int nota = 36;
            std::shared_ptr<const Mostra> mostra;
        };
        explicit SamplerBateria (std::vector<So> sons);
        /** Fora del fil d'àudio o des d'ell: paràmetres d'una nota. */
        void posaParams (const FilaBateria& f) noexcept;
        void posaParams (const std::vector<FilaBateria>& files) noexcept { for (auto& f : files) posaParams (f); }

        void prepara (double fs, int maxBloc) override;
        void processa (const EventInstrument* ev, int num, float* L, float* R, int n) noexcept override;
        void apagaTot() noexcept override;
        bool fentSo() const noexcept override { return actives > 0; }

        /** Signatura dels sons (nota i so de cada fila): si canvia, cal un sampler nou. */
        static std::string signatura (const Pista& t);

    private:
        struct Params
        {
            std::atomic<float> guany { 1.0f }, pan { 0.0f }, afina { 0.0f }, decai { 1.0f };
            std::atomic<int> talla { 0 };
            std::atomic<bool> mut { false };
        };
        struct Veu
        {
            int nota = -1;
            const Mostra* m = nullptr;
            double pos = 0.0, pas = 1.0;
            float gL = 0, gR = 0, env = 1.0f, mult = 1.0f, fade = 1.0f, fadePas = 0.0f;
            int talla = 0;
        };
        void dispara (int nota, int vel) noexcept;
        std::array<std::shared_ptr<const Mostra>, 128> mostres;
        std::array<Params, 128> params;
        static constexpr int maxVeus = 48;
        std::array<Veu, maxVeus> veus;
        int actives = 0;
        double fsSortida = 48000.0;
    };

    // ---- Edició (sobre les notes d'un clip; «idx» = les triades; buit = totes) ----
    /** Petits desplaçaments a l'atzar de temps (± ms) i de velocitat (± %). Sempre igual per a la mateixa llavor.
        fixaPrimer: els cops que cauen just al primer temps d'un compàs no es mouen de lloc (sí de velocitat).
        desplTicks: on cau el començament del clip (ticks absoluts), per saber on són els compassos. */
    void humanitza (Clip& c, std::vector<int>& idx, double msTemps, double pctVel, std::uint32_t llavor, bool fixaPrimer,
                    double tempo, long long ticksCompas, long long desplTicks);
    /** Swing: els cops de les posicions parelles-senars d'una graella «pas» (corxeres o semicorxeres) es retarden.
        pct = 50 recte, 66 tresets, fins a 75. Només mou els que són (gairebé) a la graella. */
    void swing (Clip& c, std::vector<int>& idx, double pct, long long pas, long long desplTicks);
    /** Redoble: cada cop es converteix en «cops» cops dins la casella «pas», amb la velocitat que baixa («caiguda» 0..1). */
    void redoble (Clip& c, std::vector<int>& idx, int cops, long long pas, double caiguda);
    /** Flam: un cop de gràcia una mica abans (avanc ticks), més fluix (velPct 0..1). */
    void flam (Clip& c, std::vector<int>& idx, long long avanc, double velPct);

    // ---- Ritmes de partida i fills ----
    struct Ritme
    {
        const char* nom;
        const char* grup;
        int num;               // temps per compàs (4 = 4/4, 3 = 3/4, 6 = 6/8, 12 = 12/8)
        int den;
        int caselles;          // caselles per compàs
        /** tipus → caselles: «X» fort, «x» normal, «o» fluix, «-» res. */
        std::vector<std::pair<const char*, const char*>> files;
    };
    const std::vector<Ritme>& ritmes();
    /** Notes del ritme per a «compassos» compassos a partir de «iniTicks» (ticks del clip). Fa servir la nota de la fila del
        tipus (o la General MIDI si la pista no en té). */
    std::vector<Nota> notesDeRitme (const Ritme& r, int compassos, long long ticksCompas, const std::vector<FilaBateria>& files, long long iniTicks);
    /** Fill al darrer temps (o mig compàs, si «llarg») de cada «cada» compassos de la cançó dins [iniTicks, fiTicks) del clip,
        i plat i bombo al començament del compàs següent. Treu els cops que hi havia en aquell tros (menys el bombo).
        estil: 0 caixa, 1 toms, 2 caixa i toms. Torna quants fills ha posat. */
    int fill (Clip& c, const std::vector<FilaBateria>& files, long long ticksCompas, long long ticksTemps, long long desplTicks,
              long long iniTicks, long long fiTicks, int cada, int estil, bool llarg, std::uint32_t llavor);
}
