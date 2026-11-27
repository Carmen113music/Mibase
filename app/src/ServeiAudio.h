#pragma once

#include <functional>

#include "Historial.h"
#include "EscanerPlugins.h"
#include "Plugins.h"

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_data_structures/juce_data_structures.h>
#include <mibase/MotorDiagnostic.h>
#include <mibase/MotorEstudi.h>
#include <mibase/TeclatAcords.h>
#include <mibase/Bateria.h>
#include <mibase/FontInspiracio.h>
#include <atomic>

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
    juce::StringArray getNomsSortides();   // 1.17: totes les sortides (buit: apagada)
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

    /** 0.20: fa sonar un so (per escoltar els sons dels kits). Fil de la interfície; es guarda els últims perquè no
        s'esborrin mentre sonen. nullptr: atura'l. */
    void escoltaSo (std::shared_ptr<const mibase::bateria::Mostra> m);
    /** 0.97: el que sona de la Font d'inspiració (a part del transport) i a quin parell de sortides va (el del màster). */
    mibase::font::Reproductor& font() noexcept { return reproductorFont; }
    std::atomic<int> parellFont { 0 };

    /** 0.20: pads per ordre de files a la pista de bateria «slot»: la nota 36 + k toca la fila k (notesFiles[k]).
        slot -1: desactivat. Fil de la interfície. */
    void setMapaBateria (int slot, const std::vector<int>& notesFiles);

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

    /** Plugins VST3 de l'ordinador (0.18). */
    EscanerPlugins& escaner() noexcept { return *plugins; }

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
public:
    // 0.62.1: per al diagnòstic del so que es perd (els escriu el fil d'àudio)
    std::atomic<unsigned long long> blocsAudio { 0 };
    std::atomic<unsigned int> msUltimBloc { 0 };
    std::atomic<float> picSortidaAudio { 0.0f };
    // 1.13: per al diagnòstic MIDI: quants missatges han arribat del teclat i on ha anat el darrer
    // (>= 0: el slot d'una pista; -1: so de prova; -4: només l'Harmonitzador; -3: encara cap)
    std::atomic<unsigned int> midiRebuts { 0 };
    std::atomic<int> midiDarrerDesti { -3 };
    /** 1.13: el nom de la pista d'un slot (el posa la pantalla de l'estudi; fil de la interfície). */
    std::function<juce::String (int)> nomSlotMidi;
    // 1.10: calcular les pistes amb diversos nuclis (es desa a les preferències; apagat per defecte)
    bool getDiversosNuclis() const;
    void setDiversosNuclis (bool b);
    // 1.8: quant triga cada bloc d'àudio, comparat amb el temps que hi ha (el buffer).
    std::atomic<float> pitjorBlocMs { 0.0f };        // des de l'última lectura de la línia d'estat
    std::atomic<float> pitjorBlocTotalMs { 0.0f };   // des que s'ha engegat la targeta
    std::atomic<float> margeBlocMs { 0.0f };         // el temps que hi ha per a cada bloc
    std::atomic<unsigned int> blocsAlLimit { 0 };    // blocs que han fet servir més del 80 % del temps
    std::atomic<unsigned int> blocsTard { 0 };       // blocs que han trigat més del temps que hi ha (= clic segur)
private:
    juce::AudioDeviceManager gestor;
    mibase::MotorDiagnostic motorDiag;
    mibase::MotorEstudi motorEstudi;
    mibase::font::Reproductor reproductorFont;   // 0.97
    std::unique_ptr<juce::PropertiesFile> ajustos;
    std::unique_ptr<EscanerPlugins> plugins;

    juce::SpinLock midiEntrant;   // només entre fils de MIDI i la interfície (mai al fil d'àudio)
    std::vector<std::pair<juce::String, int>> mapaTeclats;   // protegit per midiEntrant
    juce::String teclatTriada;
    int slotChordPad = -1;                                   // protegit per midiEntrant
    std::array<std::vector<int>, 12> acordsChordPad;
    mibase::TeclatAcords teclatAcords;                       // protegit per midiEntrant
    mibase::EscalaTeclat escalaTeclat;                       // protegit per midiEntrant
    int slotBateria = -1;                                    // protegit per midiEntrant
    std::array<int, 128> mapaBateria {};
    // Escoltar sons (0.20): el fil d'àudio llegeix el punter; la interfície manté vius els darrers.
    std::atomic<const mibase::bateria::Mostra*> previa { nullptr };
    std::atomic<int> previaGen { 0 };
    std::array<std::shared_ptr<const mibase::bateria::Mostra>, 4> previesVives;
    int previaIdx = 0, previaVista = 0;
    double previaPos = 0.0, fsDispositiu = 48000.0;
    mibase::CuaSenseBloqueig<mibase::EsdevenimentMidi, 512> cuaMostrar;
    juce::StringArray midiConeguts;
    juce::String ultimError, ultimaDescripcio;
    std::shared_ptr<bool> viu = std::make_shared<bool> (true);
};
