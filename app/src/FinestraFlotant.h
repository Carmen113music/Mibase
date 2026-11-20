#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** 0.70: les finestres dels efectes, plugins, sintes i kits es queden sempre per sobre de la finestra de Mibase,
    encara que cliquis a l'estudi (així pots clicar la taula de treball i fer play amb l'Espai sense que desapareguin).
    Només es tanquen amb la X (o amb Esc). No queden per sobre d'altres programes: a Windows, la finestra de
    Mibase n'és la «propietària» (si minimitzes Mibase, se'n van amb ella). */
inline juce::Component::SafePointer<juce::Component>& finestraMibase()
{
    static juce::Component::SafePointer<juce::Component> f;
    return f;
}

/** Crida-ho just després de crear la finestra (quan ja és visible). */
void quedaSobreMibase (juce::Component& finestra);
