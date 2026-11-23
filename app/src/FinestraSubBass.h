#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <mibase/SubBass.h>

class EstatEstudi;
class GestorPlugins;

/** 0.88: la finestra de «Mibase SUB & BASS»: les 8 seccions (Oscil·ladors, Sub, Filtre, Envolupants, LFO, Drive,
    Modulació, Sortida), presets (navegador, favorits, desar, importar/exportar), atzar, desfer/refer, MIDI learn,
    oscil·loscopi, CPU i teclat de pantalla amb rodes. «directe» fa sonar notes a la pista (teclat de pantalla). */
std::unique_ptr<juce::DocumentWindow> creaFinestraSubBass (std::shared_ptr<mibase::subbass::SubBass> sinte, EstatEstudi& estat,
                                                           GestorPlugins* plugins, const std::string& idPista, const juce::String& titol,
                                                           std::function<void (std::uint8_t, std::uint8_t, std::uint8_t)> directe,
                                                           std::function<void()> enTancar);
