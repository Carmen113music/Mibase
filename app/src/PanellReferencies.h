#pragma once

#include <memory>
#include <juce_gui_basics/juce_gui_basics.h>
#include <mibase/Rack.h>

/** 1.01: les referències del Mastering Studio.
    - `carregaReferencia`: llegeix una cançó (WAV, AIFF, FLAC, MP3, OGG), la passa a estèreo i n'analitza tota la cançó
      (espectre, loudness, estèreo). Es guarda en memòria: tornar-la a demanar no la rellegeix.
    - `carregaReferenciaActiva`: posa al rack l'àudio de la referència activa (si encara no el té).
    - `creaFinestraReferencies`: la finestra (llista, habituals, inici, volum igualat, comparació i EQ matching). */
std::shared_ptr<const mibase::rack::Referencia> carregaReferencia (const juce::File& f, juce::String& error);
bool carregaReferenciaActiva (mibase::rack::Rack& r);
std::unique_ptr<juce::DocumentWindow> creaFinestraReferencies (std::shared_ptr<mibase::rack::Rack> rk, std::function<void()> enTancar);
