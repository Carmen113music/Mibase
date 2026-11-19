#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <mibase/Saturacio.h>

/** 0.66: la finestra de «Saturació (Mibase)»: estil, corba, poms, interruptors i presets. */
std::unique_ptr<juce::DocumentWindow> creaFinestraSaturacio (std::shared_ptr<mibase::saturacio::Saturacio> s, const juce::String& titol,
                                                             std::function<void()> enTancar);
