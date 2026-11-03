#pragma once

#include "Text.h"

#include <juce_core/juce_core.h>

/** Historial del que passa al programa, el més recent a dalt.
    Només s'hi escriu des del fil de la interfície. */
class Historial
{
public:
    void afegeix (const juce::String& text)
    {
        const auto ara = juce::Time::getCurrentTime().formatted ("%H:%M:%S");
        linies.insert (0, ara + "  " + text);
        if (linies.size() > 300)
            linies.removeRange (300, linies.size() - 300);
        ++versio;
    }

    const juce::StringArray& getLinies() const noexcept { return linies; }
    int getVersio() const noexcept { return versio; }

private:
    juce::StringArray linies;
    int versio = 0;
};
