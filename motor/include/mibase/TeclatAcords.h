#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace mibase
{
    /** Chord Pad tocat amb un teclat (0.10): cada tecla toca l'acord del pad de la seva nota (C a B, en qualsevol octava).

        - Les notes que comparteixen dos acords que sonen alhora només s'apaguen quan s'amollen totes dues tecles.
        - L'apagada d'una tecla apaga exactament el que va encendre la seva encesa, encara que mentrestant s'hagin
          canviat els acords o s'hagi desactivat el Chord Pad.
        - tradueix() no demana memòria (es crida des del fil del MIDI). setAcord() sí: cal que qui el fa servir ho
          protegeixi (ServeiAudio ho fa amb el mateix bloqueig que el MIDI d'entrada). */
    class TeclatAcords
    {
    public:
        static constexpr int maxNotes = 12;

        void setAcord (int nota, const std::vector<int>& notes)
        {
            auto& a = acords[(size_t) (((nota % 12) + 12) % 12)];
            a.quantes = 0;
            for (int n : notes)
                if (n >= 0 && n <= 127 && a.quantes < maxNotes) a.n[(size_t) a.quantes++] = (std::uint8_t) n;
        }
        void buida() { for (auto& a : acords) a.quantes = 0; }

        /** Tradueix un missatge del teclat. Amb «actiu», les notes es converteixen en acords; si no, passen igual
            (però una apagada d'una tecla que havia fet un acord apaga l'acord). envia(estat, d1, d2). */
        template <typename Envia>
        void tradueix (std::uint8_t estat, std::uint8_t d1, std::uint8_t d2, bool actiu, Envia&& envia)
        {
            const int tipus = estat & 0xF0, canal = estat & 0x0F;
            const int tecla = d1 & 127;
            const bool apaga = tipus == 0x80 || (tipus == 0x90 && d2 == 0);
            if (apaga)
            {
                auto& s = sonant[(size_t) tecla];
                if (s.quantes > 0)
                {
                    for (int k = 0; k < s.quantes; ++k)
                    {
                        const int n = s.n[(size_t) k];
                        if (compte[(size_t) n] > 0 && --compte[(size_t) n] == 0) envia ((std::uint8_t) (0x80 | canal), (std::uint8_t) n, (std::uint8_t) 0);
                    }
                    s.quantes = 0;
                    return;
                }
                if (directa[(size_t) tecla]) { directa[(size_t) tecla] = false; envia (estat, d1, d2); }
                return;
            }
            if (tipus == 0x90)
            {
                if (sonant[(size_t) tecla].quantes > 0 || directa[(size_t) tecla])   // tornada a prémer sense amollar
                    tradueix ((std::uint8_t) (0x80 | canal), d1, 0, actiu, envia);
                if (! actiu) { directa[(size_t) tecla] = true; envia (estat, d1, d2); return; }
                const auto& a = acords[(size_t) (tecla % 12)];
                auto& s = sonant[(size_t) tecla];
                s = a;
                for (int k = 0; k < a.quantes; ++k)
                {
                    const int n = a.n[(size_t) k];
                    if (compte[(size_t) n]++ == 0) envia (estat, (std::uint8_t) n, d2);
                }
                return;
            }
            if (tipus == 0xB0 && (d1 == 120 || d1 == 123))   // tot apagat
            {
                compte.fill (0);
                directa.fill (false);
                for (auto& s : sonant) s.quantes = 0;
            }
            envia (estat, d1, d2);
        }

        /** Quantes tecles fan sonar un acord ara mateix. */
        int teclesAmbAcord() const noexcept
        {
            int r = 0;
            for (auto& s : sonant) if (s.quantes > 0) ++r;
            return r;
        }

    private:
        struct Acord { std::array<std::uint8_t, maxNotes> n {}; int quantes = 0; };
        std::array<Acord, 12> acords {};
        std::array<Acord, 128> sonant {};       // per tecla: les notes que va encendre
        std::array<int, 128> compte {};         // per nota de sortida: quantes tecles la mantenen
        std::array<bool, 128> directa {};       // tecla que va passar tal qual (sense acord)
    };
}
