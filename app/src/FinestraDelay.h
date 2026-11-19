#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <mibase/Delay.h>

/** 0.65: la finestra de «Delay (Mibase)»: caràcter, ecos, temps (ms o nota), poms, ecos dibuixats i presets. */
std::unique_ptr<juce::DocumentWindow> creaFinestraDelay (std::shared_ptr<mibase::delay::Delay> d, const juce::String& titol,
                                                         std::function<void()> enTancar);
