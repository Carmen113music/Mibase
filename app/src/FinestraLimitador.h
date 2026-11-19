#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <mibase/Limitador.h>

/** 0.69: la finestra de «Limitador (Mibase)»: estil, gràfic de la reducció, LUFS, poms i presets. */
std::unique_ptr<juce::DocumentWindow> creaFinestraLimitador (std::shared_ptr<mibase::limitador::Limitador> l, const juce::String& titol,
                                                             std::function<void()> enTancar);
