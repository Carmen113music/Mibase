#pragma once

#include "mibase/ClicDeProva.h"
#include "mibase/CuaSenseBloqueig.h"
#include "mibase/Gravadora.h"
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

        struct Pista
        {
            int slot = 0;            // canal de la taula (per suavitzar el volum i per al vúmetre)
            int canals = 1;
            float guany = 1.0f;      // fader, lineal
            float pan = 0.0f;        // -1..1
            bool sona = true;        // ni silenciada ni tapada per un solo
            std::vector<Clip> clips;
        };

        double tempo = 120.0;
        int compasNum = 4;
        bool clic = true;
        float clicVolum = 1.0f;
        int clicSo = 0;
        bool bucle = false;
        double bucleIni = 0.0, bucleFi = 0.0;
        float master = 1.0f;
        std::vector<Pista> pistes;
        std::vector<std::shared_ptr<const AudioPresa>> audios;   // perquè no s'esborrin mentre sonen
    };

    constexpr int maxSlots = 128;

    /** Canal de la taula de cada pista (a partir del seu identificador, que no canvia). */
    int slotDePista (const Pista& t) noexcept;

    /** Fa el pla del projecte (fil de la interfície). Els clips sense àudio a la memòria no sonen. */
    std::unique_ptr<Pla> construeixPla (const Projecte& p, const MapaAudios& audios);

    /** Guany lineal d'un valor en dB (-60 o menys = silenci). */
    float guanyDb (double db) noexcept;

    /** Mescla un pla: suma pistes i claqueta. El fan servir el motor (en directe) i l'exportació (fora de línia). */
    class Mesclador
    {
    public:
        void prepara (double fs, int maxBloc);
        /** Suma a L i R «n» mostres a partir de la mostra «pos» de la línia de temps (pot ser negativa).
            «vus»: un Mesurador per slot i canal (2 * maxSlots), o nullptr. */
        void processa (const Pla& pla, long long pos, int n, float* L, float* R, bool ambClic, Mesurador* vus) noexcept;
        /** Torna a començar (el volum salta directament al valor, sense pujar a poc a poc). */
        void reinicia() noexcept;

    private:
        void sonaClip (const Pla::Clip& c, int canalsPista, long long pos, int n) noexcept;
        void sonaTram (const Pla::Clip& c, double de, double fins, int canalsPista, long long pos, int n) noexcept;

        double fs = 48000.0;
        std::vector<float> pL, pR;
        std::array<float, maxSlots> gL {}, gR {};
        std::array<bool, maxSlots> gFet {};
        float gMaster = 1.0f;
        bool masterFet = false;
        ClicDeProva clic;
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

        Estat getEstat() const noexcept { return (Estat) estat.load (std::memory_order_acquire); }
        double getPosicio() const noexcept { return posicio.load (std::memory_order_relaxed); }

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
