#pragma once

#include "mibase/Instrument.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace mibase
{
    /** El sinte intern de Mibase per a les pistes MIDI: 16 veus, dos oscil·ladors (o FM), filtre i envolupants.
        Té uns quants sons preparats (els de midi::sonsInterns()). Respon a velocitat, sustain (CC64),
        modulació (CC1), volum i expressió (CC7, CC11), brillantor (CC74) i pitch bend (±2 semitons).
        No demana memòria mentre sona. */
    class SinteIntern : public Instrument
    {
    public:
        static constexpr int numVeus = 16;

        explicit SinteIntern (int so = 0);

        /** Canvia el so (es pot cridar des de qualsevol fil; les notes noves ja el fan servir). */
        void setSo (int so) noexcept;
        int getSo() const noexcept { return so.load (std::memory_order_relaxed); }

        void prepara (double fs, int maxBloc) override;
        void processa (const EventInstrument* events, int numEvents, float* L, float* R, int n) noexcept override;
        void apagaTot() noexcept override;
        bool fentSo() const noexcept override { return actives > 0; }

        int veusSonant() const noexcept { return actives; }

    private:
        struct Parametres
        {
            int ona1 = 1, ona2 = 0;          // 0 sinus, 1 serra, 2 quadrada, 3 triangle, 4 FM (sinus modulat)
            float mix2 = 0.0f, cents2 = 0.0f, octava2 = 0.0f;
            float fmRati = 1.0f, fmIndex = 0.0f, fmCaiguda = 1.0f;   // FM: l'índex cau amb el temps (segons)
            float a = 0.005f, d = 0.3f, s = 0.7f, r = 0.2f;          // segons; s de 0 a 1
            float tall = 4000.0f, ressonancia = 0.1f, envFiltre = 0.0f, caigudaFiltre = 0.3f;   // Hz, 0..1, octaves, segons
            float sensVel = 0.7f, vibrato = 0.0f, volum = 0.5f;
        };
        static const Parametres& parametres (int so) noexcept;

        enum class Fase : std::uint8_t { Lliure, Atac, Caiguda, Sosteniment, Alliberament };

        struct Veu
        {
            Fase fase = Fase::Lliure;
            int nota = -1, so = 0;
            bool sostinguda = false;          // ha rebut el note-off però el pedal la manté
            float vel = 1.0f, nivell = 0.0f, envF = 0.0f, fm = 0.0f, pan = 0.0f;
            double fase1 = 0.0, fase2 = 0.0, faseM = 0.0;
            float ic1 = 0.0f, ic2 = 0.0f;     // estat del filtre
            float alliberamentRapid = 0.0f;   // > 0: s'apaga de pressa (apagaTot)
            std::uint32_t ordre = 0;
        };

        void notaOn (int nota, int vel) noexcept;
        void notaOff (int nota) noexcept;
        void missatge (const EventInstrument& e) noexcept;
        void renderitza (float* L, float* R, int de, int fins) noexcept;
        void allibera (Veu& v) noexcept;

        std::atomic<int> so { 0 };
        std::array<Veu, numVeus> veus {};
        double fs = 48000.0;
        std::uint32_t comptador = 0;
        int actives = 0;
        bool pedal = false;
        float bend = 0.0f;                 // semitons
        float modulacio = 0.0f, volumCC = 1.0f, expressio = 1.0f, brillantor = 1.0f;
        double faseVibrato = 0.0;
        float guanySuau = 1.0f;
    };
}
