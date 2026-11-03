#pragma once

namespace mibase
{
    /** Clic de metrònom senzill (4 temps, el primer més agut).
        Tot es calcula al moment: no demana memòria mentre sona. */
    class ClicDeProva
    {
    public:
        void prepara (double freqMostreig) noexcept;

        /** Torna a començar pel primer temps. */
        void reinicia() noexcept;

        void setTempo (double bpm) noexcept;
        double getTempo() const noexcept { return tempo; }

        /** Suma el clic al buffer (mono). */
        void processa (float* sortida, int numMostres) noexcept;

    private:
        double fs = 48000.0;
        double tempo = 120.0;

        long long mostresFinsAlTemps = 0;   // quan sona el proper temps
        int temps = 0;                      // 0..3 dins el compàs
        int posicioClic = -1;               // -1 = no sona
        int durada = 0;                     // mostres que dura el clic
        double fase = 0.0, incrementFase = 0.0;
        float amplitud = 0.0f;
    };
}
