#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include "PianoRoll.h"

#include <mibase/Harmonia.h>

/** Panell de la columna esquerra quan hi ha un editor MIDI obert (en lloc del fader de la pista):
    la configuració de la pista (instrument, so, teclat MIDI) i tot el que cal per treballar la tonalitat de la cançó.
    Cada secció es plega clicant-ne el títol (es recorda).
    - Tonalitat: la detectada (si n'hi ha prou notes) o la triada a mà, l'escala i les seves notes.
    - Mode de l'escala al Piano Roll: Visual, Assistit o Bloquejat; ressaltar l'acord que sona.
    - Acords de la tonalitat: clic = escoltar-lo i posar-lo al Piano Roll amb un altre clic.
    - Constructor d'acords: fonamental, tipus, inversió, voicing, octava; sobre les notes triades: fer-ne acords,
      inversions i enllaçar-los amb el mínim moviment.
    - Progressions de 2, 4, 8 o 16 compassos: escoltar-les i inserir-les al clip.
    Treballa sobre l'editor actiu (el darrer on s'ha clicat). Tot el que posa són notes MIDI normals. */
class PanellHarmonia : public juce::Component, private juce::ChangeListener, private juce::Timer
{
public:
    PanellHarmonia (EstatEstudi& e, std::function<PianoRoll*()> editorActiu, juce::PropertiesFile& ajustos,
                    std::function<juce::Array<juce::MidiDeviceInfo>()> teclats);
    ~PanellHarmonia() override;

    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    class Contingut;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    EstatEstudi& estat;
    juce::Viewport vista;
    std::unique_ptr<Contingut> contingut;
};
