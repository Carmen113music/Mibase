#pragma once

#include <functional>
#include <juce_events/juce_events.h>

/** 0.27: la tecla Esc, estiguis on estiguis (també a les finestres a part dels plugins, del sinte o dels kits),
    tanca les finestres obertes i torna a la taula de treball. La posa l'estudi. */
inline std::function<void()>& escGlobal()
{
    static std::function<void()> f;
    return f;
}
inline bool cridaEscGlobal()
{
    if (! escGlobal()) return false;
    auto f = escGlobal();
    juce::MessageManager::callAsync ([f] { f(); });   // fora de la finestra que es tancarà
    return true;
}
