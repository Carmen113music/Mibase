#pragma once

#include <memory>
#include <juce_gui_basics/juce_gui_basics.h>
#include <mibase/Rack.h>

/** 1.01: les referències del Mastering Studio.
    - `carregaReferencia`: llegeix una cançó (WAV, AIFF, FLAC, MP3, OGG), la passa a estèreo i n'analitza tota la cançó
      (espectre, loudness, estèreo). Es guarda en memòria: tornar-la a demanar no la rellegeix.
    - `carregaReferenciaActiva`: posa al rack l'àudio de la referència activa (si encara no el té).
    - `creaFinestraReferencies`: la finestra (llista, habituals, inici, volum igualat, comparació i EQ matching). */
/** 1.12: «fsDesti» (la del projecte): la referència es converteix a aquesta freqüència en carregar-la, amb un bon convertidor
    (abans es reproduïa amb interpolació lineal i, si les freqüències no coincidien, sonava més apagada). 0 = no la converteix. */
std::shared_ptr<const mibase::rack::Referencia> carregaReferencia (const juce::File& f, juce::String& error, double fsDesti = 0.0);
bool carregaReferenciaActiva (mibase::rack::Rack& r);
std::unique_ptr<juce::DocumentWindow> creaFinestraReferencies (std::shared_ptr<mibase::rack::Rack> rk, std::function<void()> enTancar);
