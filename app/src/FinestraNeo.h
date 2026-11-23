#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <mibase/Neo.h>

class EstatEstudi;
class GestorPlugins;

/** 0.93: la finestra de «Mibase NEO». Les 8 macros sempre a la vista i tres pàgines: SÍNTESI (3 oscil·ladors amb la
    wavetable en 3D i el morph animat, mescla, unison, granular, plec i drive, filtre i ressonadors), MODULACIÓ (3
    envolupants, 4 LFO, seqüenciador de passos i matriu de 12 vies amb llumetes d'activitat) i EFECTES (rack de 7 efectes,
    assignació de les macros i veu). Presets, favorits, atzar per tipus, variacions, desfer, MIDI learn, arpegiador i teclat. */
std::unique_ptr<juce::DocumentWindow> creaFinestraNeo (std::shared_ptr<mibase::neo::Neo> sinte, EstatEstudi& estat,
                                                         GestorPlugins* plugins, const std::string& idPista, const juce::String& titol,
                                                         std::function<void (std::uint8_t, std::uint8_t, std::uint8_t)> directe,
                                                         std::function<void()> enTancar);
