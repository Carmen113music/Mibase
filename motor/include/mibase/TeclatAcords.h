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

    /** El teclat amb l'escala del projecte (0.17). Bloquejat: les notes de fora de l'escala no sonen. Assistit: van a la
        més propera de l'escala en el sentit que porta la mà (si l'anterior era més aguda, la de sota; si era més greu, la
        de sobre). L'apagada apaga la nota que va sonar. tradueix() no demana memòria. */
    class EscalaTeclat
    {
    public:
        /** mode: 0 = res (visual), 1 = assistit, 2 = bloquejat. escala[n]: la nota n (0 = C … 11 = B) és de l'escala. */
        void set (int m, const std::array<bool, 12>& e) noexcept { mode = m; escala = e; }

        template <typename Envia>
        void tradueix (std::uint8_t estat, std::uint8_t d1, std::uint8_t d2, Envia&& envia)
        {
            const int tipus = estat & 0xF0, canal = estat & 0x0F;
            const int tecla = d1 & 127;
            const bool apaga = tipus == 0x80 || (tipus == 0x90 && d2 == 0);
            if (apaga)
            {
                const int n = sona[(size_t) tecla];
                sona[(size_t) tecla] = -2;
                if (n == -2) { envia (estat, d1, d2); return; }   // no l'havíem vista: tal qual
                if (n >= 0 && compte[(size_t) n] > 0 && --compte[(size_t) n] == 0) envia ((std::uint8_t) (0x80 | canal), (std::uint8_t) n, (std::uint8_t) 0);
                return;
            }
            if (tipus == 0x90)
            {
                if (sona[(size_t) tecla] != -2) tradueix ((std::uint8_t) (0x80 | canal), d1, 0, envia);
                int n = tecla;
                if (mode != 0 && ! escala[(size_t) (tecla % 12)])
                {
                    if (mode == 2) n = -1;
                    else
                    {
                        const int sentit = ultima > tecla ? -1 : 1;
                        n = -1;
                        for (int k = 1; k < 12; ++k)
                        {
                            const int x = tecla + sentit * k;
                            if (x >= 0 && x <= 127 && escala[(size_t) (x % 12)]) { n = x; break; }
                        }
                    }
                }
                ultima = tecla;
                sona[(size_t) tecla] = n;
                if (n >= 0 && compte[(size_t) n]++ == 0) envia (estat, (std::uint8_t) n, d2);
                return;
            }
            if (tipus == 0xB0 && (d1 == 120 || d1 == 123)) { compte.fill (0); sona.fill (-2); }
            envia (estat, d1, d2);
        }

    private:
        int mode = 0, ultima = -1;
        std::array<bool, 12> escala { true, true, true, true, true, true, true, true, true, true, true, true };
        std::array<int, 128> sona = [] { std::array<int, 128> a {}; a.fill (-2); return a; }();   // per tecla: la nota que sona (-1: cap; -2: no premuda)
        std::array<int, 128> compte {};
    };
}
