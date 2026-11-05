#pragma once

#include "mibase/Harmonia.h"

#include <array>
#include <string>
#include <vector>

/** Presets dels pads d'acords (Chord Pad): seqüències de 8 acords de cançons pop, rock, indie i funk
    (adaptacions pràctiques per tenir 8 canvis; no són transcripcions literals). */
namespace mibase::pads
{
    constexpr int numPads = 16;   // dues files de 8

    struct Preset
    {
        const char* titol;
        const char* artista;
        const char* tonalitatText;
        const char* grup;
        int to = 0, escala = 0;   // tonalitat de referència (per a la fila de baix)
        std::array<const char*, 8> acords;
    };
    const std::vector<Preset>& presets();

    /** Els 16 pads d'un preset: la fila de dalt, la seqüència; la de baix, els acords de la seva tonalitat (amb setenes)
        i el V7. */
    std::vector<std::string> padsDePreset (int preset);
    /** Els 16 pads «de la tonalitat»: a dalt les tríades (I a vii i la I una octava amunt no: el V7), a baix les setenes. */
    std::vector<std::string> padsDeTonalitat (const Tonalitat& t);
}
