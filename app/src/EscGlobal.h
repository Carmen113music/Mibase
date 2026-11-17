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

/** 0.46: les tecles de l'estudi (espai = play/atura, R = grava, L = bucle…) també des de les finestres a part
    (guitarra virtual, sinte, kits, plugins). La posa l'estudi; torna si l'ha fet servir. */
#include <juce_gui_basics/juce_gui_basics.h>
inline std::function<bool (const juce::KeyPress&)>& teclaGlobal()
{
    static std::function<bool (const juce::KeyPress&)> f;
    return f;
}
/** Per al keyPressed d'una finestra a part: Esc ho tanca tot; la resta va a l'estudi. */
inline bool cridaTeclaGlobal (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) return cridaEscGlobal();
    return teclaGlobal() ? teclaGlobal() (k) : false;
}
