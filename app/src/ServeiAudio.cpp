#include "ServeiAudio.h"

namespace
{
    juce::PropertiesFile::Options opcionsAjustos()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "Mibase";
        o.folderName = "Mibase";
        o.filenameSuffix = ".ajustos";
        o.osxLibrarySubFolder = "Application Support";
        o.storageFormat = juce::PropertiesFile::storeAsXML;
        return o;
    }
}

ServeiAudio::ServeiAudio (Historial& h) : historial (h)
{
    ajustos = std::make_unique<juce::PropertiesFile> (opcionsAjustos());
    plugins = std::make_unique<EscanerPlugins> (ajustos->getFile().getSiblingFile ("Plugins.xml"));

    // Fins a 32 entrades i 2 sortides (0.18; les altres s'activen a Preferències). Si hi ha ajustos desats, es fan servir.
    auto desat = ajustos->getXmlValue ("dispositiu");
        const auto error = gestor.initialise (32, 2, desat.get(), true);

    if (error.isNotEmpty())
    {
        ultimError = error;
        historial.afegeix ("No s'ha pogut obrir l'àudio: "_ca + error);
    }

    gestor.addChangeListener (this);
    gestor.addAudioCallback (this);
    gestor.addMidiInputDeviceCallback ({}, this);   // tots els teclats activats

    historial.afegeix ("Mibase " MIBASE_VERSIO " en marxa.");
    actualitzaMidi();
    changeListenerCallback (nullptr);
}

ServeiAudio::~ServeiAudio()
{
    *viu = false;
    gestor.removeMidiInputDeviceCallback ({}, this);
    gestor.removeAudioCallback (this);
    gestor.removeChangeListener (this);
    desaAjustos();
}

juce::String ServeiAudio::nomCatalaTipus (const juce::String& tipus)
{
    if (tipus == "ASIO")                             return "ASIO (el més ràpid)"_ca;
    if (tipus == "Windows Audio")                    return "Windows Audio (compartit)";
    if (tipus == "Windows Audio (Exclusive Mode)")   return "Windows Audio (exclusiu)";
    if (tipus == "Windows Audio (Low Latency Mode)") return "Windows Audio (baix retard)";
    if (tipus == "ALSA")                             return "ALSA (Linux)";
    return tipus;
}

void ServeiAudio::actualitzaMidi()
{
    for (const auto& d : juce::MidiInput::getAvailableDevices())
    {
        if (! gestor.isMidiInputDeviceEnabled (d.identifier))
            gestor.setMidiInputDeviceEnabled (d.identifier, true);

        if (! midiConeguts.contains (d.identifier))
        {
            midiConeguts.add (d.identifier);
            historial.afegeix ("Teclat MIDI connectat: " + d.name);
        }
    }

    // Els que s'han desconnectat.
    juce::StringArray ara;
    for (const auto& d : juce::MidiInput::getAvailableDevices())
        ara.add (d.identifier);

    for (int i = midiConeguts.size(); --i >= 0;)
    {
        if (! ara.contains (midiConeguts[i]))
        {
            historial.afegeix ("Teclat MIDI desconnectat.");
            midiConeguts.remove (i);
        }
    }
}

juce::StringArray ServeiAudio::getNomsMidi() const
{
    juce::StringArray noms;
    for (const auto& d : juce::MidiInput::getAvailableDevices())
        noms.add (d.name);
    return noms;
}

void ServeiAudio::desaAjustos()
{
    if (auto xml = gestor.createStateXml())
        ajustos->setValue ("dispositiu", xml.get());
    ajustos->saveIfNeeded();
}

juce::String ServeiAudio::clauConfiguracio()
{
    auto* d = gestor.getCurrentAudioDevice();
    if (d == nullptr)
        return {};

    return "latencia|" + d->getTypeName() + "|" + d->getName() + "|"
         + juce::String ((int) d->getCurrentSampleRate()) + "|" + juce::String (d->getCurrentBufferSizeSamples());
}

juce::String ServeiAudio::getLatenciaDesada()
{
    const auto clau = clauConfiguracio();
    return clau.isEmpty() ? juce::String() : ajustos->getValue (clau);
}

void ServeiAudio::desaLatencia (int mostres, double ms)
{
    const auto clau = clauConfiguracio();
    if (clau.isEmpty())
        return;

    ajustos->setValue (clau, juce::String (mostres) + " mostres (" + juce::String (ms, 1) + " ms), "
                                 + juce::Time::getCurrentTime().formatted ("%d/%m/%Y %H:%M"));
    ajustos->saveIfNeeded();
}

juce::String ServeiAudio::resumDispositiu()
{
    auto* d = gestor.getCurrentAudioDevice();
    if (d == nullptr)
        return {};

    auto tipus = d->getTypeName();
    if (tipus.startsWith ("Windows Audio"))
        tipus = "WASAPI";

    const double fs = d->getCurrentSampleRate();
    const auto freq = std::fmod (fs, 1000.0) < 0.5 ? juce::String ((int) (fs / 1000.0)) : juce::String (fs / 1000.0, 1);
    return tipus + " · "_ca + d->getName() + " · "_ca + freq + " kHz · "_ca
         + juce::String (d->getCurrentBufferSizeSamples()) + " mostres";
}

double ServeiAudio::latenciaMsDesada()
{
    const auto t = getLatenciaDesada();
    if (t.isEmpty())
        return -1.0;
    // «123 mostres (2,6 ms), …» o «123 mostres (2.6 ms), …»
    return t.fromFirstOccurrenceOf ("(", false, false).upToFirstOccurrenceOf (" ms", false, false)
            .replaceCharacter (',', '.').getDoubleValue();
}

//==============================================================================
void ServeiAudio::audioDeviceIOCallbackWithContext (const float* const* entrades, int numEntrades,
                                                    float* const* sortides, int numSortides, int numMostres,
                                                    const juce::AudioIODeviceCallbackContext&)
{
    // Fil d'àudio: només el motor, que no demana memòria ni s'espera mai.
    motorDiag.processa (entrades, numEntrades, sortides, numSortides, numMostres);
    {
        // 0.19: on és la cançó, per als plugins (tempo i compàs els posa la pantalla de l'estudi).
        auto& r = RellotgePlugins::global();
        const auto e = motorEstudi.getEstat();
        r.sonant.store (e == mibase::MotorEstudi::Estat::Sonant || e == mibase::MotorEstudi::Estat::Gravant, std::memory_order_relaxed);
        r.segons.store (motorEstudi.getPosicio(), std::memory_order_relaxed);
    }
    motorEstudi.processa (entrades, numEntrades, sortides, numSortides, numMostres);
}

void ServeiAudio::audioDeviceAboutToStart (juce::AudioIODevice* d)
{
    motorDiag.prepara (d->getCurrentSampleRate(), d->getCurrentBufferSizeSamples());
    motorEstudi.prepara (d->getCurrentSampleRate(), juce::jmax (d->getCurrentBufferSizeSamples(), 2048));

    // Les entrades actives arriben juntes a la crida d'àudio: per a cada entrada, la seva posició.
    std::vector<int> mapa;
    const auto actives = d->getActiveInputChannels();
    int k = 0;
    for (int e = 0; e < juce::jmax (actives.getHighestBit() + 1, 0) && e < mibase::MotorEstudi::maxEntrades; ++e)
        mapa.push_back (actives[e] ? k++ : -1);
    motorEstudi.setMapaEntrades (mapa);
}

juce::StringArray ServeiAudio::getNomsEntrades()
{
    juce::StringArray noms;
    if (auto* d = gestor.getCurrentAudioDevice())
    {
        const auto actives = d->getActiveInputChannels();
        const auto tots = d->getInputChannelNames();
        for (int e = 0; e < tots.size() && e < mibase::MotorEstudi::maxEntrades; ++e)
            noms.add (actives[e] ? tots[e] : juce::String());   // buit: entrada apagada
    }
    return noms;
}

double ServeiAudio::retardGravacio (bool& mesurat)
{
    const double ms = latenciaMsDesada();
    mesurat = ms > 0.0;
    if (mesurat) return ms / 1000.0;
    if (auto* d = gestor.getCurrentAudioDevice())
    {
        const double fs = d->getCurrentSampleRate();
        if (fs > 0.0)
            return (d->getInputLatencyInSamples() + d->getOutputLatencyInSamples()) / fs;
    }
    return 0.0;
}

void ServeiAudio::audioDeviceStopped()
{
    motorDiag.latencia().cancella();
}

void ServeiAudio::audioDeviceError (const juce::String& error)
{
    std::weak_ptr<bool> encaraViu = viu;
    juce::MessageManager::callAsync ([this, error, encaraViu]
    {
        if (auto v = encaraViu.lock(); v == nullptr || ! *v)
            return;
        ultimError = error;
        historial.afegeix ("Error de l'àudio: "_ca + error);
    });
}

void ServeiAudio::setMapaTeclats (const std::vector<std::pair<juce::String, int>>& mapa, const juce::String& triada)
{
    const juce::SpinLock::ScopedLockType bloqueig (midiEntrant);
    if (mapa == mapaTeclats && triada == teclatTriada) return;
    mapaTeclats = mapa;
    teclatTriada = triada;
}

void ServeiAudio::setEscalaTeclat (int mode, const std::array<bool, 12>& escala)
{
    const juce::SpinLock::ScopedLockType bloqueig (midiEntrant);
    escalaTeclat.set (mode, escala);
}

void ServeiAudio::setChordPadTeclat (int slot, const std::array<std::vector<int>, 12>& acords)
{
    const juce::SpinLock::ScopedLockType bloqueig (midiEntrant);
    if (slot == slotChordPad && acords == acordsChordPad) return;
    slotChordPad = slot;
    acordsChordPad = acords;
    for (int i = 0; i < 12; ++i) teclatAcords.setAcord (i, acords[(size_t) i]);
}

void ServeiAudio::handleIncomingMidiMessage (juce::MidiInput* origen, const juce::MidiMessage& m)
{
    if (m.getRawDataSize() > 3 || m.isMidiClock() || m.isActiveSense())
        return;

    const auto* dades = m.getRawData();
    mibase::EsdevenimentMidi e;
    e.estat = dades[0];
    e.dada1 = m.getRawDataSize() > 1 ? dades[1] : 0;
    e.dada2 = m.getRawDataSize() > 2 ? dades[2] : 0;

    // Pot haver-hi més d'un teclat (un fil per teclat): un sol productor alhora.
    const juce::SpinLock::ScopedLockType bloqueig (midiEntrant);
    // 0.17: mana la pista MIDI triada (si no té assignat un altre teclat); si no, la pista a què el teclat està
    // assignat; si no n'hi ha cap, el so de prova de les Preferències.
    const auto id = origen != nullptr ? origen->getIdentifier() : juce::String();
    int desti = -2;
    const bool triadaAccepta = motorEstudi.getSlotTeclat() >= 0 && (teclatTriada.isEmpty() || teclatTriada == id);
    if (! triadaAccepta)
    {
        desti = -1;
        for (auto& [teclat, slot] : mapaTeclats)
            if (teclat == id) desti = slot;
    }
    // Chord Pad: si la pista on va el teclat el té obert, cada tecla toca l'acord del seu pad.
    const int slot = desti == -2 ? motorEstudi.getSlotTeclat() : desti;
    const bool acords = slotChordPad >= 0 && slot == slotChordPad;
    auto envia = [this, desti] (std::uint8_t es, std::uint8_t d1, std::uint8_t d2)
    {
        if (! motorEstudi.enviaTeclat (es, d1, d2, desti))
            motorDiag.enviaMidi ({ es, d1, d2 });
    };
    // Primer el Chord Pad; el que hi passa tal qual, per l'escala (bloquejat / assistit).
    teclatAcords.tradueix (e.estat, e.dada1, e.dada2, acords, [this, &envia] (std::uint8_t es, std::uint8_t d1, std::uint8_t d2)
    {
        escalaTeclat.tradueix (es, d1, d2, envia);
    });
    cuaMostrar.posa (e);
}

void ServeiAudio::changeListenerCallback (juce::ChangeBroadcaster*)
{
    auto* d = gestor.getCurrentAudioDevice();

    juce::String descripcio;
    if (d == nullptr)
        descripcio = "Àudio aturat (cap dispositiu obert)."_ca;
    else
        descripcio = "Àudio: "_ca + nomCatalaTipus (d->getTypeName()) + " · "_ca + d->getName() + " · "_ca
                   + juce::String ((int) d->getCurrentSampleRate()) + " Hz · buffer "_ca
                   + juce::String (d->getCurrentBufferSizeSamples());

    if (descripcio != ultimaDescripcio)
    {
        ultimaDescripcio = descripcio;
        historial.afegeix (descripcio);
        desaAjustos();
    }
}
