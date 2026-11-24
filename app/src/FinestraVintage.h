#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <mibase/Vintage.h>

class EstatEstudi;
class GestorPlugins;

/** 1.4: la finestra de «Mibase VINTAGE». Aspecte d'aparell analògic dels 70: panell negre serigrafiat, galtes de fusta,
    poms amb faldilla, faders per a les envolupants, el nom del preset en una etiqueta de paper i un vúmetre d'agulla
    (cap pantalla de colors). Vista SIMPLE (oscil·ladors, filtre, envolupant, caràcter, efectes i veu essencials) i
    AVANÇAT (tot: modulació, mesclador, LFO, roda, aftertouch, unison…). Presets, favorits, cerca, atzar per tipus,
    variacions, desfer, MIDI learn, import/export, arpegiador i teclat de pantalla. */
std::unique_ptr<juce::DocumentWindow> creaFinestraVintage (std::shared_ptr<mibase::analogic::Vintage> sinte, EstatEstudi& estat,
                                                             GestorPlugins* plugins, const std::string& idPista, const juce::String& titol,
                                                             std::function<void (std::uint8_t, std::uint8_t, std::uint8_t)> directe,
                                                             std::function<void()> enTancar);
