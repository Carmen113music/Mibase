#pragma once

#include "mibase/Instrument.h"
#include <array>
#include <atomic>
#include <string>

/** 0.56: «Dinàmica (Mibase)», efecte intern amb entrada de sidechain: compressor, gate, expander i de-esser.
    Mateixes normes que la Reverb: paràmetres atòmics (qualsevol fil), processa() sense memòria ni esperes. */
namespace mibase::dinamica
{
    inline constexpr const char* idPlugin = "intern:dinamica";
    inline constexpr const char* nomPlugin = "Dinàmica (Mibase)";

    enum Mode : int { Compressor, Gate, Expander, DeEsser, numModes };
    const char* nomMode (int m);

    enum Param : int { ModeP, Llindar, Ratio, Atac, Alliberament, Genoll, Rang, Guany, Detector, FiltreGreus, FiltreAguts, FreqDeEsser,
                       Mix, Escolta, numParams };

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

    struct Mesures
    {
        std::atomic<float> reduccio { 0.0f };    // dB (positiu)
        std::atomic<float> entrada { 0.0f };     // pic del detector (lineal)
        std::atomic<float> sortida { 0.0f };
        std::atomic<bool> sidechain { false };
    };

    class Dinamica : public EfecteIntern
    {
    public:
        Dinamica();
        void prepara (double fs, int maxBloc) override;
        void processa (float* L, float* R, int n) noexcept override;

        bool admetSidechain() const noexcept override { return true; }
        void posaSidechain (const float* l, const float* r) noexcept override { scL = l; scR = r; }

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
        /** Corba del càlcul de guany (per dibuixar-la): nivell d'entrada en dB → reducció en dB. */
        float reduccioPer (float nivellDb) const noexcept;

    private:
        std::array<std::atomic<float>, numParams> par;
        std::atomic<bool> demanaBuidar { false };
        Mesures mes;
        const float* scL = nullptr;
        const float* scR = nullptr;
        double fs = 48000.0;
        // Estat del fil d'àudio
        struct Bq { float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0; float operator() (float x) noexcept; };
        Bq dHP, dLP, dsHP[2];
        float fHP = -1.0f, fLP = -1.0f, fDs = -1.0f;
        float env = 0.0f, ms = 0.0f, gr = 0.0f, hold = 0.0f;
        float sGuany = 1.0f, sMix = 1.0f;
        bool primer = true;
        float reduccio (float nivellDb, int mode) const noexcept;
    };
}
