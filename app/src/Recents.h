#pragma once

#include "Text.h"

#include <juce_data_structures/juce_data_structures.h>

/** Llista de projectes oberts fa poc (el més recent primer), desada als ajustos. */
class Recents
{
public:
    struct Entrada
    {
        juce::File fitxer;
        juce::int64 obert = 0;   // mil·lisegons
    };

    explicit Recents (juce::PropertiesFile& a) : ajustos (a) { carrega(); }

    const juce::Array<Entrada>& llista() const noexcept { return entrades; }

    void afegeix (const juce::File& f)
    {
        treuSenseDesar (f);
        entrades.insert (0, { f, juce::Time::currentTimeMillis() });
        if (entrades.size() > maxim)
            entrades.removeRange (maxim, entrades.size() - maxim);
        desa();
    }

    void treu (const juce::File& f)
    {
        treuSenseDesar (f);
        desa();
    }

private:
    void treuSenseDesar (const juce::File& f)
    {
        for (int i = entrades.size(); --i >= 0;)
            if (entrades.getReference (i).fitxer == f)
                entrades.remove (i);
    }

    void carrega()
    {
        entrades.clear();
        juce::StringArray linies;
        linies.addLines (ajustos.getValue ("recents"));
        for (auto& l : linies)
        {
            const auto ms = l.upToFirstOccurrenceOf ("|", false, false).getLargeIntValue();
            const auto cami = l.fromFirstOccurrenceOf ("|", false, false);
            if (cami.isNotEmpty() && juce::File::isAbsolutePath (cami))
                entrades.add ({ juce::File (cami), ms });
        }
    }

    void desa()
    {
        juce::StringArray linies;
        for (auto& e : entrades)
            linies.add (juce::String (e.obert) + "|" + e.fitxer.getFullPathName());
        ajustos.setValue ("recents", linies.joinIntoString ("\n"));
        ajustos.saveIfNeeded();
    }

    static constexpr int maxim = 40;
    juce::PropertiesFile& ajustos;
    juce::Array<Entrada> entrades;
};
