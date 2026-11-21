#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

/** 0.73: el que els efectes de veu (l'Harmonitzador) han de saber de la cançó i del teclat, sense dependre de l'app.

    - On és la cançó i si sona: ho posa el fil d'àudio a cada bloc (ServeiAudio).
    - La tonalitat (tònica i escala com a màscara de 12 bits): la posa la interfície quan canvia el projecte.
    - Els acords de la pista d'Acords (trams de temps amb la màscara de les notes): la interfície els publica en dos
      buffers alternats; el fil d'àudio en llegeix un sense bloquejar-se.
    - Les notes premudes del teclat MIDI (128 bits): les posa el fil del MIDI.
    Tot és atòmic o sense bloqueig: es pot llegir des del fil d'àudio. */
namespace mibase
{
    class ContextMusical
    {
    public:
        static ContextMusical& global();

        std::atomic<double> segons { 0.0 };
        std::atomic<bool> sonant { false };
        std::atomic<int> tonica { 0 };
        std::atomic<int> mascaraEscala { 0xAB5 };   // major

        // ---- Teclat MIDI ----
        void notaOn (int n) noexcept;
        void notaOff (int n) noexcept;
        void totesOff() noexcept;
        bool nota (int n) const noexcept;
        /** Les notes premudes, de greu a agut (com a molt «max»). */
        int notesPremudes (int* sortida, int max) const noexcept;
        /** Quants harmonitzadors fan servir el teclat ara (si n'hi ha, el teclat no fa sonar el so de prova). */
        std::atomic<int> escoltantTeclat { 0 };

        // ---- Acords ----
        struct Tram { double de = 0.0, a = 0.0; int mascara = 0; int baix = -1; };
        static constexpr int maxTrams = 1024;
        /** Fil de la interfície. Els trams han d'anar ordenats per «de». */
        void publicaAcords (const std::vector<Tram>& t) noexcept;
        /** Qualsevol fil: l'acord que sona al temps «t» (fals si no n'hi ha cap). */
        bool acordA (double t, Tram& sortida) const noexcept;

    private:
        std::array<std::atomic<std::uint32_t>, 4> bits {};
        Tram trams[2][maxTrams];
        std::atomic<int> nTrams[2] { 0, 0 };
        std::atomic<int> actiu { 0 };
    };
}
