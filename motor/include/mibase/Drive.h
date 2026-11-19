#pragma once

#include "mibase/Saturacio.h"

/** 0.67: «Drive (Mibase)», pedal de guitarra intern («intern:drive»): Crunch, Overdrive, Distorsió i Fuzz.
    Poms de pedal: Gain, Tone i Level; a més, Bass, Mix (per barrejar amb el so net, útil al baix), Gate (talla
    el brunzit entre notes) i Tight (treu greus abans de distorsionar, per a riffs ràpids i afinats baixos).
    És el primer pedal de la futura col·lecció d'amplis i pedals. Com la Saturació, la distorsió es fa a ×4.
    Mateixes normes de fils que la resta d'efectes interns. */
namespace mibase::drive
{
    inline constexpr const char* idPlugin = "intern:drive";
    inline constexpr const char* nomPlugin = "Drive (Mibase)";

    enum Mode : int { Crunch, Overdrive, Distorsio, Fuzz, numModes };
    const char* nomMode (int m);
    const char* descripcioMode (int m);

    enum Param : int { ModeP, Gain, Tone, Level, Bass, Mix, Gate, Tight, numParams };

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
    /** Guany (dB) que fa el pom Gain en aquest mode. */
    float guanyDb (int mode, float gain) noexcept;
    /** La corba del mode (per dibuixar-la i per a les proves), amb l'entrada ja amplificada. */
    float corba (int mode, float x) noexcept;

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
        std::atomic<float> entrada { 0.0f }, sortida { 0.0f };
        std::atomic<float> gate { 1.0f };   // 0 tancat … 1 obert
        std::atomic<std::uint32_t> blocs { 0 };
    };

    class Drive : public EfecteIntern
    {
    public:
        static constexpr int sobre = 4;
        using Bq = saturacio::Saturacio::Bq;

        Drive();
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
        const Mesures& mesures() const noexcept { return mes; }

    private:
        std::array<std::atomic<float>, numParams> par;
        std::atomic<bool> demanaBuidar { false };
        Mesures mes;
        double fs = 48000.0;
        struct Canal { Bq tight, preA, preB, up[4], down[4], dc, postA, postB, bass, tone1, tone2; };
        Canal ch[2];
        float darrer[numParams] {};
        bool cal = true;
        float sGain = 1.0f, sMix = 1.0f, sOut = 1.0f, env = 0.0f, gGate = 1.0f;
        void recalcula() noexcept;
    };
}
