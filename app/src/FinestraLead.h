#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <mibase/Lead.h>

class EstatEstudi;
class GestorPlugins;

/** 0.89: la finestra de «Mibase LEAD»: Oscil·ladors, Mix (amb la forma d'ona), Filtre, Envolupants, LFO, FX, Modulació
    i Veu; presets (navegador amb recents i atzar, favorits, desar, importar/exportar), desfer, MIDI learn, CPU, arpegiador
    i teclat de pantalla amb rodes. */
std::unique_ptr<juce::DocumentWindow> creaFinestraLead (std::shared_ptr<mibase::lead::Lead> sinte, EstatEstudi& estat,
                                                           GestorPlugins* plugins, const std::string& idPista, const juce::String& titol,
                                                           std::function<void (std::uint8_t, std::uint8_t, std::uint8_t)> directe,
                                                           std::function<void()> enTancar);
