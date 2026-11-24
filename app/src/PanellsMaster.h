#pragma once

#include <memory>
#include <juce_gui_basics/juce_gui_basics.h>
#include <mibase/Estereo.h>
#include <mibase/Loudness.h>
#include <mibase/Rack.h>

/** 0.94: les peces de la interfície del Mastering Studio.
    - El panell lateral del màster (A/B amb volum igualat, objectiu de loudness, mesures del mix i del màster,
      historial i una lectura de les mesures amb avisos explicats).
    - La finestra del mòdul Estèreo (amplada per bandes, mono als greus, correlació).
    - La finestra del mòdul Loudness Meter. */
juce::Component* creaPanellMaster (std::shared_ptr<mibase::rack::Rack> rack);
juce::Component* creaPanellEstereo (std::shared_ptr<mibase::estereo::Estereo> e);
juce::Component* creaPanellLoudness (std::shared_ptr<mibase::loudness::Modul> m);
/** 0.96: les finestres del Clipper i del Transient Shaper (poms, mesura de quant actua i un consell). */
juce::Component* creaPanellClipper (std::shared_ptr<mibase::EfecteIntern> c);
juce::Component* creaPanellTransitoris (std::shared_ptr<mibase::EfecteIntern> t);
/** 0.99: les finestres del Multibanda (una columna per banda) i del Low End Focus. */
juce::Component* creaPanellMultibanda (std::shared_ptr<mibase::EfecteIntern> m);
juce::Component* creaPanellLowEnd (std::shared_ptr<mibase::EfecteIntern> g);

/** Un avís curt i explicat si el mòdul està en una zona perillosa (buit si no n'hi ha). «master»: llindars de màster. */
juce::String avisModul (const mibase::rack::Modul& m, bool master);
/** Quant actua ara el mòdul, en text curt («−2,1 dB GR», «+1,4 dB»…), i de 0 a 1 per a la barreta. */
juce::String activitatModul (const mibase::rack::Modul& m, float& quant);
