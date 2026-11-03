#pragma once

#include "Historial.h"
#include "ServeiAudio.h"
#include "Tema.h"

#include <juce_gui_basics/juce_gui_basics.h>

/** Vúmetres de les entrades actives. */
class Vumetres : public juce::Component
{
public:
    void setCanals (const juce::StringArray& noms);
    void setNivell (int canal, float pic, bool saturat);
    void paint (juce::Graphics& g) override;

private:
    struct Canal { juce::String nom; float nivell = 0.0f, retingut = 0.0f; int retencio = 0; int vermell = 0; };
    std::vector<Canal> canals;
};

/** Últims missatges MIDI rebuts, el més recent a dalt. */
class LlistaMidi : public juce::Component
{
public:
    void afegeix (const juce::String& text);
    void paint (juce::Graphics& g) override;

private:
    juce::StringArray linies;
};

/** Pantalla de diagnòstic: triar la targeta, provar el so, el MIDI i mesurar la latència. */
class PantallaDiagnostic : public juce::Component,
                           private juce::Timer,
                           private juce::ChangeListener
{
public:
    explicit PantallaDiagnostic (ServeiAudio& servei, Historial& historial);
    ~PantallaDiagnostic() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    void refrescaControls();
    void triaTipus();
    void triaDispositiu();
    void triaFreqOBuffer();
    void aplica (juce::AudioDeviceManager::AudioDeviceSetup setup, const juce::String& que);

    void comencaLatencia();
    void acabaLatencia();
    void actualitzaEstat();
    juce::String textDiagnostic();
    static juce::String descriuMidi (const mibase::EsdevenimentMidi& e);
    static juce::String nomNota (int nota);

    ServeiAudio& servei;
    Historial& historial;

    // Seccions (per dibuixar-ne el fons i el títol).
    juce::Rectangle<int> zonaAudio, zonaProves, zonaLatencia, zonaEntrades, zonaMidi, zonaEstat, zonaHistorial;

    juce::Label etTipus, etSortida, etEntrada, etFreq, etBuffer;
    juce::ComboBox cbTipus, cbSortida, cbEntrada, cbFreq, cbBuffer;
    juce::TextButton btPanell { "Panell del controlador" }, btBusca { "Torna a buscar" };

    juce::TextButton btClic { "Clic de prova" }, btSinte { "So del teclat" };
    juce::Slider tempo;
    juce::Label etTempo;

    juce::TextButton btLatencia { "Mesura la latència"_ca };
    juce::Label etAjudaLatencia, etResultatLatencia;

    Vumetres vumetres;
    juce::Label etTeclats;
    LlistaMidi llistaMidi;

    juce::Label etEstat;
    juce::TextEditor historialText;
    juce::TextButton btCopia { "Copia el diagnòstic"_ca };

    juce::StringArray nomsTipus;   // noms reals dels tipus (en el mateix ordre que el desplegable)
    juce::String ultimResultatLatencia;
    int versioHistorial = -1;
    int comptador = 0;
    bool refrescant = false;
};
