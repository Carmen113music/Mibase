#pragma once

#include "mibase/Instrument.h"
#include <array>
#include <atomic>
#include <string>
#include <vector>

/** 0.96: «Clipper (Mibase)» («intern:clipper»), per posar just abans del limitador al màster.

    Retalla els pics més curts (el cop de la caixa, el bombo) perquè el limitador no hagi de baixar tot el volum per
    culpa seva: més volum amb menys bombeig. Cinc formes: Soft (corba suau a partir del genoll), Hard (talla en sec),
    Analog (saturació suau de tota l'ona, com una cinta), Smooth (genoll ample i rodó) i Modern (gairebé sec, amb una
    mica de genoll). Drive (dB que entren), Ceiling (on talla), Shape (on comença el genoll a Soft), Oversampling x4
    (menys aspror: els harmònics no es dobleguen), Link estèreo (el mateix retall als dos canals), Mix i Sortida.
    Mesura quant retalla (dB) i quantes mostres toca. Amb l'oversampling, 12 mostres de retard (el PDC ho compensa). */
namespace mibase::clipper
{
    inline constexpr const char* idPlugin = "intern:clipper";
    inline constexpr const char* nomPlugin = "Clipper (Mibase)";

    enum Mode : int { Soft, Hard, Analog, Smooth, Modern, numModes };
    const char* nomMode (int m);

    enum Param : int { ModeP, Drive, Ceiling, Shape, Oversampling, Link, Mix, Sortida, numParams };
    struct InfoParam { const char* clau; const char* nom; float mn, mx, def; const char* ajuda; };
    const InfoParam& info (int p);

    constexpr int latenciaOs = 12;   // mostres amb l'oversampling (filtres de 49 coeficients a x4)

    struct Mesures
    {
        std::atomic<float> retall { 0.0f };      // dB que treu al pic més alt de l'últim bloc
        std::atomic<float> retallMax { 0.0f };   // el màxim des de l'última lectura (la finestra el reinicia)
        std::atomic<float> percent { 0.0f };     // % de mostres retallades (suavitzat)
    };

    /** La corba (sense drive): x i el sostre «t» en lineal. */
    float corba (int mode, float x, float t, float shape01) noexcept;

    class Clipper : public EfecteIntern
    {
    public:
        Clipper();
        void prepara (double fs, int maxBloc) override;
        void processa (float* L, float* R, int n) noexcept override;
        int latencia() const noexcept override { return valor (Oversampling) >= 0.5f ? latenciaOs : 0; }

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
        static constexpr int taps = 49;
        std::array<float, taps> fir {};
        // oversampling: historial d'entrada (base) i de la senyal sobremostrejada (per al filtre de baixada)
        std::array<std::array<float, taps>, 2> hUp {}, hDown {};
        int pUp = 0, pDown = 0;
        // el sec retardat (per a Mix amb oversampling)
        std::array<std::array<float, 16>, 2> sec {};
        int pSec = 0;
        float gDrive = 1.0f, gOut = 1.0f, gMix = 1.0f, percentSuau = 0.0f;
        bool primer = true;
        void buidaAra() noexcept;
    };
}
