#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>

namespace mibase::guitarrack { class GuitarRack; }

/** 1.5: la finestra del «Guitar Rack (Mibase)»: l'ampli, la pantalla amb els micròfons, la pedalera amb cables i la
    biblioteca de pedals (arrossegar, cercar, preferits). Un clic en un pedal l'amplia amb els seus controls. */
std::unique_ptr<juce::DocumentWindow> creaFinestraGuitarRack (std::shared_ptr<mibase::guitarrack::GuitarRack> g, const juce::String& titol,
                                                              std::function<void()> enTancar);
