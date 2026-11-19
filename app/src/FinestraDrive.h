#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <mibase/Drive.h>

/** 0.67: la finestra de «Drive (Mibase)», amb aspecte de pedal: mode, Gain / Tone / Level, Bass, Mix, Gate, Tight i presets. */
std::unique_ptr<juce::DocumentWindow> creaFinestraDrive (std::shared_ptr<mibase::drive::Drive> d, const juce::String& titol,
                                                         std::function<void()> enTancar);
