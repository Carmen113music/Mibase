#pragma once

#include "mibase/Saturacio.h"

/** 0.69: «Limitador (Mibase)», efecte intern («intern:limitador»): limitador / maximitzador per al màster i els busos.

    Mira endavant 5 ms (la latència és sempre la mateixa i el PDC la compensa): el guany baixa just a temps i cap
    pic passa del sostre. Quatre estils: Transparent (atac suau, mira 5 ms), Modern (2 ms), Punch (1,5 ms i un
    retallador suau abans, que deixa passar el cop) i Agressiu (1 ms i retallador més dur). Gain (puja el volum),
    Ceiling (sostre), Release o Auto (s'adapta al material), Link estèreo, True Peak (vigila els pics entre mostres)
    i «Escolta igualat» (abaixa el que has pujat, per comparar només el so). Mesura LUFS (volum percebut, 3 s).

    Mateixes normes de fils que la resta d'efectes interns. */
namespace mibase::limitador
{
    inline constexpr const char* idPlugin = "intern:limitador";
    inline constexpr const char* nomPlugin = "Limitador (Mibase)";

    enum Estil : int { Transparent, Modern, Punch, Agressiu, numEstils };
    const char* nomEstil (int e);
    const char* descripcioEstil (int e);

    enum Param : int { EstilP, Gain, Ceiling, Release, AutoRel, Link, TruePeak, Igualat, numParams };

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

    constexpr double lookaheadMs = 5.0;   // la latència (fixa) del limitador

    struct Preset { std::string nom, categoria, descripcio, estat; };
    const std::vector<std::string>& categories();
    const std::vector<Preset>& presetsFabrica();
    class BibliotecaPresets
    {
    public:
        std::vector<Preset> usuari;
        std::vector<std::string> favorits;
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
        std::atomic<float> reduccio { 0.0f };                     // ara (dB, positiu)
        std::atomic<float> reduccioMax { 0.0f };                  // la màxima des de l'última lectura de la finestra (dB)
        std::atomic<float> lufsCurt { -100.0f };                  // volum percebut dels últims 3 s (LUFS)
        static constexpr int numHist = 240;
        std::array<std::atomic<float>, numHist> hist {};          // reducció (dB) cada ~50 ms, per al gràfic
        std::atomic<int> posHist { 0 };
        std::atomic<std::uint32_t> blocs { 0 };
        std::atomic<std::uint32_t> retallades { 0 };   // mostres que ha hagut d'aturar la seguretat (hauria de ser gairebé 0)
    };

    class Limitador : public EfecteIntern
    {
    public:
        using Bq = saturacio::Saturacio::Bq;
        Limitador();
        void prepara (double fs, int maxBloc) override;
        void processa (float* L, float* R, int n) noexcept override;
        int latencia() const noexcept override { return lat; }

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
        Mesures& mesures() noexcept { return mes; }

    private:
        std::array<std::atomic<float>, numParams> par;
        std::atomic<bool> demanaBuidar { false };
        Mesures mes;
        double fs = 48000.0;
        int lat = 240;            // mostres de retard (5 ms)
        int mida = 512, mascara = 511;
        std::vector<float> audio[2];   // retard del so
        std::vector<float> req[2];     // guany necessari (per a la finestra del mínim)
        std::vector<float> suau[2];    // per a la mitjana (atac suau)
        std::vector<float> gDelay[2];  // retard del guany (perquè el so i el guany quadrin amb qualsevol estil)
        double suma[2] {};
        float relEnv[2] { 1.0f, 1.0f };
        float hist4[2][3] {};          // les darreres mostres (true peak aproximat)
        int w = 0;
        float sGain = 1.0f, sCeil = 1.0f, grMitja = 0.0f;
        int finAnterior = -1;
        // mínim de la finestra amb una cua ordenada (cada mostra entra i surt un sol cop)
        std::vector<long long> dqT[2];
        std::vector<float> dqV[2];
        long long dqCap[2] {}, dqCua[2] {};
        long long t = 0;   // si canvia la finestra (o es buida), es torna a sumar
        bool primer = true;
        // LUFS: filtre K i 30 trossos de 100 ms
        Bq k1[2], k2[2];
        double energia = 0.0; int comptaE = 0;
        std::array<double, 30> trossos {}; int posTros = 0, numTrossos = 0;
        float maxHist = 0.0f; int comptaHist = 0;
        void neteja() noexcept;
    };
}
