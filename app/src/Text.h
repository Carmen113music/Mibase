#pragma once

#include <juce_core/juce_core.h>

/** Text en català amb accents: "Diagnòstic"_ca.
    (JUCE llegeix els textos normals com si no tinguessin accents.) */
inline juce::String operator""_ca (const char* text, std::size_t mida)
{
    return juce::String::fromUTF8 (text, (int) mida);
}
