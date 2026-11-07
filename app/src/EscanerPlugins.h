#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/** Busca els plugins VST3 de l'ordinador (0.18).
    Cada plugin s'obre en un procés a part (el mateix Mibase amb «--escaneja-plugin»): si un plugin peta o es penja,
    només cau aquell procés i el plugin queda a la llista d'errors. La llista es desa a «Plugins.xml» i només es tornen
    a mirar els fitxers nous o canviats (o tots, si es demana). */
class EscanerPlugins : private juce::Thread, private juce::AsyncUpdater
{
public:
    explicit EscanerPlugins (const juce::File& fitxerLlista);
    ~EscanerPlugins() override;

    /** Comença a buscar (fil de la interfície). tot: torna a mirar també els que ja coneix. */
    void comenca (bool tot);
    void atura();
    bool buscant() const { return isThreadRunning(); }

    /** Text del que està fent ara («Mirant 3 de 40: Serum…») o buit. */
    juce::String progres() const;

    struct Error { juce::String fitxer, motiu; };
    const juce::KnownPluginList& llista() const noexcept { return plugins; }
    const std::vector<Error>& errors() const noexcept { return errs; }
    int numInstruments() const;
    int numEfectes() const;

    /** Avís (fil de la interfície) quan canvia el progrés o la llista. */
    std::function<void()> onCanvi;

    /** Mode procés fill: obre un plugin i n'escriu la descripció a «sortida». Torna el codi de sortida. */
    static int escanejaUn (const juce::String& fitxer, const juce::String& sortida);
    static constexpr const char* argument = "--escaneja-plugin";

private:
    void run() override;
    void handleAsyncUpdate() override;
    void carrega();
    void desa();

    juce::File fitxer;
    juce::KnownPluginList plugins;
    std::vector<Error> errs;
    bool tornaTot = false;
    juce::StringArray fitxersErrors;   // còpia per al fil

    // Resultats del fil (es passen a la llista al fil de la interfície).
    juce::CriticalSection seccio;
    juce::String textProgres;
    juce::OwnedArray<juce::PluginDescription> nous;
    std::vector<Error> errsNous;
    juce::StringArray mirats;
    bool acabat = false;
};
