#pragma once

#include "Historial.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_data_structures/juce_data_structures.h>
#include <mibase/MotorDiagnostic.h>
#include <mibase/MotorEstudi.h>
#include <mibase/TeclatAcords.h>

/** Connecta el motor amb la targeta d'àudio i els teclats MIDI (via JUCE),
    i desa els ajustos entre sessions. */
class ServeiAudio : private juce::AudioIODeviceCallback,
                    private juce::MidiInputCallback,
                    private juce::ChangeListener
{
public:
    explicit ServeiAudio (Historial& historial);
    ~ServeiAudio() override;

    juce::AudioDeviceManager& dispositius() noexcept { return gestor; }
    mibase::MotorDiagnostic& motor() noexcept { return motorDiag; }
    /** El motor de l'estudi: fa sonar el projecte i grava. */
    mibase::MotorEstudi& estudi() noexcept { return motorEstudi; }

    /** Hi ha la targeta en marxa? */
    bool enMarxa() { return gestor.getCurrentAudioDevice() != nullptr && gestor.getCurrentAudioDevice()->isPlaying(); }
    /** Noms de les entrades de la targeta («Entrada 1», «Input 2»…). */
    juce::StringArray getNomsEntrades();
    /** Retard d'anada i tornada per alinear les gravacions, en segons: el mesurat si n'hi ha; si no, el que diu la targeta.
        «mesurat» diu d'on surt. */
    double retardGravacio (bool& mesurat);

    /** Activa tots els teclats MIDI connectats i anota els nous a l'historial. */
    void actualitzaMidi();
    juce::StringArray getNomsMidi() const;
    juce::Array<juce::MidiDeviceInfo> getTeclatsMidi() const { return juce::MidiInput::getAvailableDevices(); }

    /** Teclats assignats a una pista concreta (identificador → canal de la pista) i, per a la pista triada, quin teclat
        la toca (buit: qualsevol que no estigui assignat a una altra). Fil de la interfície. */
    void setMapaTeclats (const std::vector<std::pair<juce::String, int>>& mapa, const juce::String& teclatTriada);

    /** Chord Pad al teclat (0.10): les tecles que van a la pista «slot» toquen l'acord del pad de la seva nota
        (acords[0] = C … acords[11] = B). slot -1: desactivat. Fil de la interfície. */
    void setChordPadTeclat (int slot, const std::array<std::vector<int>, 12>& acords);
    /** 0.17: escala del projecte per al teclat (0 visual, 1 assistit, 2 bloquejat). */
    void setEscalaTeclat (int mode, const std::array<bool, 12>& escala);

    /** Missatges MIDI per mostrar a la pantalla (fil de la interfície). */
    bool treuMidiPerMostrar (mibase::EsdevenimentMidi& e) noexcept { return cuaMostrar.treu (e); }

    /** Desa la configuració de l'àudio. */
    void desaAjustos();

    /** Latència mesurada desada per a la configuració actual (o buit). */
    juce::String getLatenciaDesada();
    void desaLatencia (int mostres, double ms);

    juce::String getUltimError() const { return ultimError; }

    /** Ajustos del programa (es desen sols). */
    juce::PropertiesFile& getAjustos() noexcept { return *ajustos; }

    /** «ASIO · Focusrite USB ASIO · 48 kHz · 128 mostres» (o buit si l'àudio és aturat). */
    juce::String resumDispositiu();

    /** Retard mesurat per a la configuració actual, en ms (o negatiu si no s'ha mesurat). */
    double latenciaMsDesada();

    static juce::String nomCatalaTipus (const juce::String& tipus);

private:
    void audioDeviceIOCallbackWithContext (const float* const* entrades, int numEntrades,
                                           float* const* sortides, int numSortides, int numMostres,
                                           const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* dispositiu) override;
    void audioDeviceStopped() override;
    void audioDeviceError (const juce::String& error) override;

    void handleIncomingMidiMessage (juce::MidiInput* origen, const juce::MidiMessage& m) override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    juce::String clauConfiguracio();

    Historial& historial;
    juce::AudioDeviceManager gestor;
    mibase::MotorDiagnostic motorDiag;
    mibase::MotorEstudi motorEstudi;
    std::unique_ptr<juce::PropertiesFile> ajustos;

    juce::SpinLock midiEntrant;   // només entre fils de MIDI i la interfície (mai al fil d'àudio)
    std::vector<std::pair<juce::String, int>> mapaTeclats;   // protegit per midiEntrant
    juce::String teclatTriada;
    int slotChordPad = -1;                                   // protegit per midiEntrant
    std::array<std::vector<int>, 12> acordsChordPad;
    mibase::TeclatAcords teclatAcords;                       // protegit per midiEntrant
    mibase::EscalaTeclat escalaTeclat;                       // protegit per midiEntrant
    mibase::CuaSenseBloqueig<mibase::EsdevenimentMidi, 512> cuaMostrar;
    juce::StringArray midiConeguts;
    juce::String ultimError, ultimaDescripcio;
    std::shared_ptr<bool> viu = std::make_shared<bool> (true);
};
