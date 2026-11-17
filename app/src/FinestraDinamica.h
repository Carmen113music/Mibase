#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <mibase/Dinamica.h>

/** 0.56: la finestra de «Dinàmica (Mibase)»: mode, poms, corba, reducció i d'on escolta (sidechain o so propi). */
std::unique_ptr<juce::DocumentWindow> creaFinestraDinamica (std::shared_ptr<mibase::dinamica::Dinamica> d, const juce::String& titol,
                                                            std::function<void()> enTancar);
