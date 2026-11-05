#pragma once

#include "mibase/Harmonia.h"

#include <array>
#include <string>
#include <vector>

/** Presets dels pads d'acords (Chord Pad): seqüències de 8 acords de cançons pop, rock, indie i funk
    (adaptacions pràctiques per tenir 8 canvis; no són transcripcions literals). */
namespace mibase::pads
{
    /** 12 pads com les tecles d'una octava de piano (0.10): l'índex és la nota, de C (0) a B (11).
        Les blanques (C D E F G A B) a baix, les negres (C# D# F# G# A#) a dalt. Una tecla del teclat MIDI toca el pad
        de la seva nota (en qualsevol octava). */
    constexpr int numPads = 12;
    constexpr bool esNegre (int i) noexcept { const int n = ((i % 12) + 12) % 12; return n == 1 || n == 3 || n == 6 || n == 8 || n == 10; }

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

    /** Els 12 pads d'un preset: a les blanques, els acords de la seqüència en ordre (sense repetir) i, si en sobra lloc,
        els de la seva tonalitat; a les negres, els de color de la tonalitat (V7, préstecs…) que no hi siguin. */
    std::vector<std::string> padsDePreset (int preset);
    /** Els 12 pads «de la tonalitat»: a les blanques, els graus I a vii (C = I, D = ii…); a les negres, acords de color.
        Major: C# V7, D# ii7, F# iv, G# bVI, A# bVII. Menor: C# V7, D# im7, F# IV, G# V, A# bII. */
    std::vector<std::string> padsDeTonalitat (const Tonalitat& t);
    /** Adapta uns pads desats amb una altra disposició (la 0.9 en tenia 16) a la de 12. */
    std::vector<std::string> adapta (const std::vector<std::string>& pads, const std::string& preset);
}
