#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace mibase
{
    /** Escriu un fitxer WAV (PCM de 24 bits o de 16). No és per al fil d'àudio: escriu al disc. */
    class EscriptorWav
    {
    public:
        ~EscriptorWav() { tanca(); }

        bool obre (const std::filesystem::path& cami, int canals, double freqMostreig, int bits = 24);
        /** Mostres intercalades (L R L R…), de -1 a 1 (el que en surt es retalla). */
        bool escriu (const float* intercalades, long long frames);
        /** Acaba la capçalera i tanca. Retorna false si hi ha hagut cap error. */
        bool tanca();

        bool obert() const noexcept { return fitxer.is_open(); }
        long long getFrames() const noexcept { return frames; }

    private:
        std::ofstream fitxer;
        int canals = 1, bits = 24;
        double fs = 48000.0;
        long long frames = 0;
        bool error = false;
        std::vector<char> tmp;
    };

    /** Un WAV llegit sencer (per a les proves i per a coses petites): mostres intercalades. */
    struct WavLlegit
    {
        bool correcte = false;
        int canals = 0;
        double fs = 0.0;
        std::vector<float> mostres;
        long long frames() const noexcept { return canals > 0 ? (long long) mostres.size() / canals : 0; }
    };

    /** Llegeix WAV PCM de 16 o 24 bits o float de 32. */
    WavLlegit llegeixWav (const std::filesystem::path& cami);
}
