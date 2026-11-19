#pragma once

#include "mibase/Instrument.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

/** 0.65: «Delay (Mibase)», efecte intern d'insert («intern:delay»).

    Quatre caràcters (Digital, Cinta, Analògic, Lo-fi), tres maneres de repartir els ecos (Estèreo, Ping-pong, Mono),
    temps en ms o sincronitzat amb el tempo, filtres i saturació dins de la repetició, modulació (wow i flutter),
    difusió, ducking (els ecos s'abaixen mentre toques), amplada estèreo i freeze.

    Mateixes normes que la Reverb i la Dinàmica: paràmetres atòmics (qualsevol fil), prepara() fora del fil d'àudio
    (és l'única que demana memòria), processa() sense memòria ni esperes. */
namespace mibase::delay
{
    inline constexpr const char* idPlugin = "intern:delay";
    inline constexpr const char* nomPlugin = "Delay (Mibase)";

    enum Caracter : int { Digital, Cinta, Analogic, LoFi, numCaracters };
    enum Encaminament : int { Estereo, PingPong, Mono, numEncaminaments };
    const char* nomCaracter (int c);
    const char* descripcioCaracter (int c);
    const char* nomEncaminament (int e);

    /** L'ordre no es pot canviar a la lleugera: les claus de text són les que es desen. */
    enum Param : int
    {
        CaracterP, EncaminamentP, Sync, Temps, Nota, Desfas, Feedback,
        LowCut, HighCut, Drive, ModRate, ModDepth, Difusio,
        DuckAmt, DuckThr, DuckRel,
        Amplada, Mix, Sortida, Freeze,
        numParams
    };

    struct InfoParam
    {
        const char* clau;
        const char* nom;
        float mn, mx, def;
        bool enter, log;
        const char* ajuda;
    };
    const InfoParam& info (int p);
    std::string textValor (int p, float v);

    /** Valors de nota per sincronitzar amb el tempo (de més curt a més llarg). */
    int numNotes();
    const char* nomNota (int i);
    double notaEnNegres (int i);   // 1 = una negra

    constexpr double maxSegons = 6.0;   // el temps més llarg d'un eco (també sincronitzat)

    /** Presets (de fàbrica i de l'usuari), com els de la Dinàmica. «estat» és el text de Delay::estat(). */
    struct Preset { std::string nom, categoria, descripcio, estat; };
    const std::vector<std::string>& categories();
    const std::vector<Preset>& presetsFabrica();
    class BibliotecaPresets
    {
    public:
        std::vector<Preset> usuari;
        std::vector<std::string> favorits;     // «f:Nom» (fàbrica) o «u:Nom» (usuari)
        bool carrega (const std::filesystem::path& fitxer);
        bool desa (const std::filesystem::path& fitxer) const;
        void desaPreset (const Preset& p);
        bool esborra (const std::string& nom);
        bool reanomena (const std::string& nom, const std::string& nou);
        bool mou (const std::string& nom, const std::string& carpeta);
        const Preset* busca (const std::string& nom) const;
        std::vector<std::string> carpetes() const;
        static std::string clauFabrica (const std::string& nom) { return "f:" + nom; }
        static std::string clauUsuari (const std::string& nom) { return "u:" + nom; }
        bool esFavorit (const std::string& clau) const;
        void commutaFavorit (const std::string& clau);
        std::string nomLliure (const std::string& base) const;
    };

    struct Mesures
    {
        std::atomic<float> entrada { 0.0f }, sortida { 0.0f };   // pics (lineal)
        std::atomic<float> eco { 0.0f };                          // nivell dels ecos (pic, lineal)
        std::atomic<float> ducking { 0.0f };                      // reducció ara (dB, positiu)
        std::atomic<std::uint32_t> blocs { 0 };
    };

    /** Un eco per dibuixar: quan arriba (s) i amb quin volum a cada costat (0..1). */
    struct Eco { double temps; float l, r; };

    class Delay : public EfecteIntern
    {
    public:
        Delay();
        void prepara (double fs, int maxBloc) override;
        void processa (float* L, float* R, int n) noexcept override;

        int numParametres() const override { return numParams; }
        InfoParametre infoParametre (int i) const override;
        void posaParametre (int i, float v01) noexcept override;
        float parametre (int i) const noexcept override;
        std::string textParametre (int i, float v01) const override;
        static float normal (int p, float v) noexcept;
        static float deNormal (int p, float v01) noexcept;

        void posa (int p, float v) noexcept;
        float valor (int p) const noexcept;
        std::string estat() const override;
        void posaEstat (const std::string& s) override;
        void buida() noexcept override { demanaBuidar.store (true); }

        /** D'on llegeix el tempo (bpm) per a la sincronització. nullptr = 120. */
        void posaTempo (const std::atomic<double>* t) noexcept { tempo = t; }
        double bpm() const noexcept { const double b = tempo != nullptr ? tempo->load (std::memory_order_relaxed) : 120.0; return b > 1.0 ? b : 120.0; }
        /** Temps de l'eco esquerre i del dret (s), amb el tempo d'ara. */
        double tempsL() const noexcept;
        double tempsR() const noexcept;
        /** Els ecos que se sentiran (per a la finestra; no escolta el so): com a molt «maxEcos» o fins a «finsA» segons. */
        std::vector<Eco> ecos (int maxEcos = 24, double finsA = 4.0) const;
        const Mesures& mesures() const noexcept { return mes; }
        double frequenciaMostreig() const noexcept { return fs; }

    private:
        std::array<std::atomic<float>, numParams> par;
        std::atomic<bool> demanaBuidar { false };
        const std::atomic<double>* tempo = nullptr;
        Mesures mes;
        double fs = 48000.0;

        // ---- estat del fil d'àudio ----
        struct Bq
        {
            float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
            float operator() (float x) noexcept { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
            void passaBaixos (double f, double fs) noexcept;
            void passaAlts (double f, double fs) noexcept;
            void neteja() noexcept { z1 = z2 = 0.0f; }
        };
        struct AllPass { int pos = 0, len = 1; float* buf = nullptr; float operator() (float x, float g) noexcept; };

        std::vector<float> buf[2];
        int mascara = 0, escriu = 0;
        std::vector<float> difBuf;
        AllPass dif[2][4];
        Bq hp[2], lp[2], lpCar[2];
        // temps (en mostres): actual, el de destí i, per al caràcter Digital, el fos entre dos temps
        double dAct[2] {}, dVell[2] {}, dNou[2] {};
        float fos = 1.0f;          // 1 = sense fos en curs
        float faseLfo = 0.0f, faseFlutter = 0.0f;
        float sFb = 0.0f, sMix = 0.0f, sOut = 1.0f, sDuck = 1.0f, envDuck = 0.0f;
        float sh[2] {}; int shComptador = 0;   // lo-fi: mostra i aguanta
        float darrerHp = -1.0f, darrerLp = -1.0f, darrerCar = -1.0f;
        int darrerCaracter = -1;
        bool primer = true;
        float llegeix (int c, double d) const noexcept;
    };
}
