#pragma once

class ServeiSidechain;
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <mibase/Reverb.h>

/** 0.55: la finestra de la Reverb de Mibase (mode Simple i Avançat, visualització, presets, A/B, desfer).
    Treballa directament sobre la reverb viva (paràmetres atòmics): el so canvia al moment, sense refer el pla. */
std::unique_ptr<juce::DocumentWindow> creaFinestraReverb (std::shared_ptr<mibase::reverb::Reverb> reverb, const juce::String& titol,
                                                          std::function<void()> enTancar,
                                                          ServeiSidechain* sc = nullptr, const std::string& idInsert = {});
