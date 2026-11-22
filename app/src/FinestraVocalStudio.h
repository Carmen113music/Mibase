#pragma once

#include <functional>
#include <memory>
#include <juce_gui_basics/juce_gui_basics.h>
#include <mibase/Rack.h>
#include <mibase/Harmonitzador.h>

/** 0.70: la finestra del «Vocal Studio (Mibase)»: presets, 8 macros, entrada i sortida, i el rack de mòduls (encès, Mix,
    ordre, treure, afegir) amb la finestra de sempre de cada mòdul. */
std::unique_ptr<juce::DocumentWindow> creaFinestraVocalStudio (std::shared_ptr<mibase::rack::Rack> rack, const juce::String& titol,
                                                               std::function<void()> enTancar,
                                                               std::function<int (int)> accioPistes = {});
/** 0.80: la finestra del Tune quan va sol, com un insert més. */
std::unique_ptr<juce::DocumentWindow> creaFinestraTune (std::shared_ptr<mibase::tune::Tune> tune, const juce::String& titol, std::function<void()> enTancar);   // 0.78: 0 = quants busos «VS ·», 1 = implementa, 2 = treu; 0.79: 3 = la pista és en solo?, 4 = commuta el solo
