#pragma once

namespace mibase
{
    /** Clic de metrònom (el primer temps del compàs més fort i més agut), amb quatre sons:
        0 digital (pip), 1 fusta (bloc de fusta), 2 analògic (claus de sintetitzador), 3 campana (esquella).
        Tot es calcula al moment: no demana memòria mentre sona. */
    class ClicDeProva
    {
    public:
        void prepara (double freqMostreig) noexcept;

        /** Torna a començar pel primer temps. */
        void reinicia() noexcept;

        void setTempo (double bpm) noexcept;
        double getTempo() const noexcept { return tempo; }

        static constexpr int numSons = 4;
        void setSo (int so) noexcept;
        void setVolum (float v) noexcept { volum = v < 0.0f ? 0.0f : (v > 1.5f ? 1.5f : v); }
        void setTempsPerCompas (int n) noexcept { tempsPerCompas = n < 1 ? 1 : n; }

        /** Suma el clic al buffer (mono), amb els temps que es compta sol. */
        void processa (float* sortida, int numMostres) noexcept;

        /** Per al motor de l'estudi, que decideix ell quan sona cada temps:
            comença un cop ara mateix (el primer temps del compàs, amb accent)… */
        void dispara (bool ambAccent) noexcept;
        /** …i suma al buffer només el cop que ja sona (sense comptar temps). */
        void processaSo (float* sortida, int numMostres) noexcept;

    private:
        double fs = 48000.0;
        double tempo = 120.0;

        long long mostresFinsAlTemps = 0;   // quan sona el proper temps
        int temps = 0;                      // dins el compàs
        int tempsPerCompas = 4;
        int posicioClic = -1;               // -1 = no sona
        int durada = 0;                     // mostres que dura el clic
        int so = 0;
        float volum = 1.0f;
        bool accent = false;
        double fase = 0.0, fase2 = 0.0, freq1 = 0.0, freq2 = 0.0;
        float amplitud = 0.0f;
        unsigned int soroll = 12345u;

        float mostra (double t) noexcept;
        void sonaUna (float* sortida, int i) noexcept;
    };
}
