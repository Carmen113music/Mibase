#pragma once

#include "mibase/FFT.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

/** 0.60: analitzador d'espectre (per a l'EQ Pro i els efectes que vinguin).

    El fil d'àudio només copia mostres a un anell (sense demanar memòria ni esperar). La interfície llegeix les
    últimes mostres, en fa la FFT i les passa a columnes de la pantalla. Tot el càlcul és aquí (al motor) perquè
    es pugui provar sense finestres. */
namespace mibase::espectre
{
    constexpr int midaAnell = 16384;
    constexpr float pis = -120.0f;    // dB: el que hi ha per sota no es dibuixa

    /** Anell de mostres: escriu un sol fil (l'àudio) i llegeix un sol fil (la interfície). */
    class Anell
    {
    public:
        void escriu (const float* x, int n) noexcept;
        void escriuUna (float x) noexcept;
        /** Copia les últimes n mostres (n <= midaAnell), de la més vella a la més nova. */
        void llegeix (float* desti, int n) const noexcept;
        void buida() noexcept;
        /** Quantes mostres s'hi han escrit en total (per saber si n'han arribat de noves). */
        std::uint32_t posicio() const noexcept { return pos.load (std::memory_order_acquire); }
    private:
        std::array<float, midaAnell> b {};
        std::atomic<std::uint32_t> pos { 0 };
    };

    /** FFT amb finestra de Hann. Un sinus a escala plena dona 0 dB al seu bin. */
    class Analitzador
    {
    public:
        explicit Analitzador (int mida = 4096);
        int mida() const noexcept { return n; }
        /** Nivell en dB de cada bin (0 .. mida/2) de les `mida` mostres de x. */
        const std::vector<float>& analitza (const float* x);
    private:
        int n;
        fft::FFT f;
        std::vector<float> finestra, db;
        std::vector<fft::C> buf;
    };

    /** Mida de FFT recomanada per a una freqüència de mostreig (uns 85 ms de so). */
    int midaPer (double fs);

    /** Passa els bins a columnes (freqs = una freqüència per columna, creixent). Si una columna té diversos bins,
        en pren el més alt (no s'amaguen pics estrets); si en té menys d'un, interpola. */
    void aColumnes (const std::vector<float>& binsDb, double fs, const std::vector<float>& freqs, std::vector<float>& out);

    /** dB que s'afegeixen a f amb una inclinació de dbOct per octava (0 dB a 1 kHz). */
    float inclinacio (float f, float dbOct);

    enum Mode : int { Pic, Mitjana };
    enum Velocitat : int { Lenta, Normal, Rapida };
    /** Suavitzat en el temps. Pic: puja de cop i baixa a poc a poc (dB/s segons la velocitat).
        Mitjana: mitjana de la potència amb una constant de temps segons la velocitat. dt = segons des de la vegada anterior.
        Si `mostrat` és buit o d'una altra mida, hi copia `nou`. */
    void suavitza (std::vector<float>& mostrat, const std::vector<float>& nou, int mode, int velocitat, double dt);
    float caigudaPic (int velocitat);       // dB/s
    double tempsMitjana (int velocitat);    // s

    /** El pic (màxim local) més alt entre f/2^oct i f·2^oct. Torna l'índex de la columna o -1. */
    int picProper (const std::vector<float>& col, const std::vector<float>& freqs, float f, float oct);
}
