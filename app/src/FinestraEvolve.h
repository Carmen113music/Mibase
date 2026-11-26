#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <mibase/Evolve.h>

/** 1.13: la finestra d'«EVOLVE (Mibase)», mode Simple: el pom EVOLUTION al mig, presets i categories, Randomize, Reset,
    Hold, A/B, Input, Dry/Wet, Output i la cadena de mòduls (encesos i quant treballen). */
std::unique_ptr<juce::DocumentWindow> creaFinestraEvolve (std::shared_ptr<mibase::evolve::Evolve> e, const juce::String& titol,
                                                          std::function<void()> enTancar);
