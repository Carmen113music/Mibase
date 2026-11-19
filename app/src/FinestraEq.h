#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <mibase/Equalitzador.h>

/** 0.59: la finestra de «EQ Pro (Mibase)»: gràfica amb les bandes (nodes que s'arrosseguen), llista de bandes i panell
    de la banda triada. */
std::unique_ptr<juce::DocumentWindow> creaFinestraEq (std::shared_ptr<mibase::eq::Equalitzador> eq, const juce::String& titol,
                                                      std::function<void()> enTancar);
