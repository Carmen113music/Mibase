#pragma once

#include "mibase/Instrument.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

/** 0.66: «Saturació (Mibase)», efecte intern d'insert («intern:saturacio»). Base dels futurs amplis i pedals.

    Inspirada en el Decapitator: cinc estils analògics (A cinta, E consola britànica, N preamp de transistors,
    T vàlvula triode, P vàlvula pèntode) i dos de pedal (Overdrive i Fuzz). Drive, Punish (+20 dB), Low Cut amb
    Thump (cop de greus al tall), Tone (fosc ↔ brillant), High Cut amb Steep, Mix, Output i Auto (el volum de sortida
    es queda igual encara que pugis el drive). La distorsió es fa a 4 vegades la freqüència de mostreig perquè no
    surtin harmònics fantasma.

    Mateixes normes que la resta d'efectes interns: paràmetres atòmics, prepara() fora del fil d'àudio,
    processa() sense memòria ni esperes. */
namespace mibase::saturacio
{
    inline constexpr const char* idPlugin = "intern:saturacio";
    inline constexpr const char* nomPlugin = "Saturació (Mibase)";

    enum Estil : int { A, E, N, T, P, Overdrive, Fuzz, numEstils };
    const char* nomEstil (int e);          // «A», «E»… «Overdrive», «Fuzz»
    const char* nomLlargEstil (int e);     // «A · Cinta»
    const char* descripcioEstil (int e);

    enum Param : int { EstilP, Drive, Punish, LowCut, Thump, Tone, HighCut, Steep, Mix, Sortida, Auto, numParams };

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

    /** La corba de l'estil (sense filtres), per dibuixar-la: entrada (lineal) → sortida, amb el drive en dB. */
    float corba (int estil, float x, float driveDb) noexcept;

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
        std::atomic<float> nivellCorba { 0.0f };                  // pic a l'entrada de la corba (lineal, amb el drive)
        std::atomic<float> autoDb { 0.0f };                       // compensació d'Auto ara (dB)
        std::atomic<std::uint32_t> blocs { 0 };
    };

    class Saturacio : public EfecteIntern
    {
    public:
        static constexpr int sobre = 4;   // sobremostreig

        Saturacio();
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

        /** Drive total en dB (Drive + Punish). */
        float driveDb() const noexcept;
        const Mesures& mesures() const noexcept { return mes; }

    private:
        std::array<std::atomic<float>, numParams> par;
        std::atomic<bool> demanaBuidar { false };
        Mesures mes;
        double fs = 48000.0;

        struct Bq
        {
            float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
            float operator() (float x) noexcept { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
            void neteja() noexcept { z1 = z2 = 0.0f; }
        };
        static void passaAlts (Bq& b, double f, double q, double fs) noexcept;
        static void passaBaixos (Bq& b, double f, double q, double fs) noexcept;
        static void campana (Bq& b, double f, double q, double db, double fs) noexcept;
        static void prestatgeAlt (Bq& b, double f, double db, double fs) noexcept;
        static void prestatgeBaix (Bq& b, double f, double db, double fs) noexcept;

        // per canal
        struct Canal
        {
            Bq hp, preA, preB;              // abans de la corba (a la freqüència normal)
            Bq up[4], down[4];              // sobremostreig: filtres d'imatge i d'alias (8è ordre)
            Bq dc;                          // treu la continua després de la corba (a fs × 4)
            Bq postA, postB, toneLo, toneHi, lp1, lp2;
        };
        Canal ch[2];
        float darrer[numParams] {};
        bool cal = true;
        float sDrive = 1.0f, sMix = 0.0f, sOut = 1.0f, sAuto = 1.0f;
        float rmsIn = 0.0f, rmsOut = 0.0f;
        void recalcula() noexcept;
    };
}
