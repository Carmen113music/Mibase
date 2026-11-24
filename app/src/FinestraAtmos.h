#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <mibase/Atmos.h>

class EstatEstudi;
class GestorPlugins;

/** 0.90: la finestra de «Mibase ATMOS». Mode SIMPLE (els 8 macros, la forma d'ona, l'evolució del timbre, el moviment
    estèreo i un resum dels efectes) i mode AVANÇAT (tots els paràmetres: 4 oscil·ladors, textura i granular, filtre,
    envolupants i evolució, 3 LFO, matriu de modulació de 8 vies, rack d'efectes, macros i veu). Presets, favorits,
    desar, importar/exportar, atzar musical per tipus de pad, desfer, MIDI learn, arpegiador i teclat. */
std::unique_ptr<juce::DocumentWindow> creaFinestraAtmos (std::shared_ptr<mibase::atmos::Atmos> sinte, EstatEstudi& estat,
                                                            GestorPlugins* plugins, const std::string& idPista, const juce::String& titol,
                                                            std::function<void (std::uint8_t, std::uint8_t, std::uint8_t)> directe,
                                                            std::function<void()> enTancar);
