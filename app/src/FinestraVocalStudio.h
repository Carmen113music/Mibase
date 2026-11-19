#pragma once

#include <functional>
#include <memory>
#include <juce_gui_basics/juce_gui_basics.h>
#include <mibase/Rack.h>

/** 0.70: la finestra del «Vocal Studio (Mibase)»: presets, 8 macros, entrada i sortida, i el rack de mòduls (encès, Mix,
    ordre, treure, afegir) amb la finestra de sempre de cada mòdul. */
std::unique_ptr<juce::DocumentWindow> creaFinestraVocalStudio (std::shared_ptr<mibase::rack::Rack> rack, const juce::String& titol,
                                                               std::function<void()> enTancar);
