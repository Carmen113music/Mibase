#pragma once

#include <atomic>
#include <string>
#include <vector>

namespace mibase
{
    /** Mesura el retard real d'anada i tornada (sortida -> cable -> entrada).

        Com funciona:
        1. La interfície crida demana().
        2. El fil d'àudio fa sonar un soroll curt i conegut 5 vegades (cada 0,5 s)
           i grava l'entrada a la vegada (en memòria preparada abans).
        3. Quan acaba, la interfície crida calcula(): busca on apareix el soroll a
           cada gravació i en treu el valor del mig.

        Durant la mesura, la resta de sons s'han de silenciar. */
    class MesuraLatencia
    {
    public:
        static constexpr int repeticions = 5;
        static constexpr int longitudSenyal = 512;

        struct Resultat
        {
            bool correcte = false;
            int mostres = 0;        // retard en mostres
            double ms = 0.0;        // retard en mil·lisegons
            int dispersio = 0;      // diferència entre la mesura més llarga i la més curta
            int mesuresValides = 0;
            double qualitat = 0.0;  // 0..1 (com s'assembla el que ha tornat al que ha sortit)
            std::string missatge;
        };

        /** Prepara la memòria. Cal cridar-ho fora del fil d'àudio (en arrencar el dispositiu). */
        void prepara (double freqMostreig);

        /** Demana una mesura nova (des de la interfície). */
        bool demana() noexcept;

        /** Atura la mesura en curs (des de qualsevol fil). */
        void cancella() noexcept;

        bool estaMesurant() const noexcept;
        bool teDades() const noexcept;

        /** Fil d'àudio. Escriu el senyal de prova a 'sortida' (mono, el substitueix)
            i grava la suma de les entrades. Retorna true si està mesurant
            (llavors la resta de sons s'han de silenciar). */
        bool processa (const float* const* entrades, int numEntrades,
                       float* sortida, int numMostres) noexcept;

        /** Calcula el resultat (des de la interfície, quan teDades()). */
        Resultat calcula();

        double getFreqMostreig() const noexcept { return fs; }

        /** Progrés de 0 a 1 mentre mesura. */
        float progres() const noexcept;

    private:
        enum Estat : int { Inactiu = 0, Demanat = 1, Mesurant = 2, Fet = 3 };

        double fs = 0.0;
        int longitudCicle = 0;
        std::vector<float> senyal, gravacio;
        std::atomic<int> estat { Inactiu };
        std::atomic<int> posicio { 0 };
    };
}
