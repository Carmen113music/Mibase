#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <map>
#include <memory>
#include <mibase/Instrument.h>
#include <mibase/Projecte.h>

/** 0.19: el temps de la cançó per als plugins (tempo, posició, sonant), perquè els delays i similars vagin a tempo.
    El fil d'àudio el posa al dia a cada bloc (ServeiAudio); els plugins el llegeixen. */
struct RellotgePlugins : public juce::AudioPlayHead
{
    std::atomic<double> tempo { 120.0 }, segons { 0.0 };
    std::atomic<bool> sonant { false };
    std::atomic<int> num { 4 }, den { 4 };
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        const double t = tempo.load(), s = segons.load();
        p.setBpm (t);
        p.setTimeInSeconds (s);
        p.setPpqPosition (s * t / 60.0 * (4.0 / juce::jmax (1, den.load())));
        p.setTimeSignature (juce::AudioPlayHead::TimeSignature { num.load(), den.load() });
        p.setIsPlaying (sonant.load());
        return p;
    }
    static RellotgePlugins& global() { static RellotgePlugins r; return r; }
};

/** 0.20: una finestra d'instrument amb un panell enganxat a la dreta (l'escala del teclat). El contingut principal
    (l'editor del plugin o el panell del sinte intern) mana l'alçada; si canvia de mida, tot s'hi adapta. */
class AmbLateral : public juce::Component, private juce::ComponentListener
{
public:
    static constexpr int ampleLateral = 236;
    AmbLateral (juce::Component* principalPropi, std::unique_ptr<juce::Component> lat)
        : principal (principalPropi), lateral (std::move (lat))
    {
        addAndMakeVisible (*principal);
        if (lateral != nullptr) addAndMakeVisible (*lateral);
        principal->addComponentListener (this);
        ajusta();
    }
    ~AmbLateral() override { if (principal != nullptr) principal->removeComponentListener (this); }
    juce::Component* getPrincipal() const noexcept { return principal.get(); }
    void resized() override
    {
        const int l = lateral != nullptr ? ampleLateral : 0;
        if (lateral != nullptr) lateral->setBounds (getWidth() - l, 0, l, getHeight());
        if (! dinsAjusta)
        {
            const juce::ScopedValueSetter<bool> s (dinsAjusta, true);
            principal->setBounds (0, 0, juce::jmax (40, getWidth() - l), getHeight());
        }
    }
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff1b1b20)); }

private:
    void componentMovedOrResized (juce::Component&, bool, bool wasResized) override { if (wasResized && ! dinsAjusta) ajusta(); }
    void ajusta()
    {
        const juce::ScopedValueSetter<bool> s (dinsAjusta, true);
        const int l = lateral != nullptr ? ampleLateral : 0;
        setSize (principal->getWidth() + l, juce::jmax (principal->getHeight(), lateral != nullptr ? 330 : 0));
        if (lateral != nullptr) lateral->setBounds (getWidth() - l, 0, l, getHeight());
    }
    std::unique_ptr<juce::Component> principal, lateral;
    bool dinsAjusta = false;
};

/** 0.19: un plugin VST3 vist pel motor. Fa d'efecte (insert) o d'instrument (pista MIDI): el motor només coneix
    mibase::Efecte i mibase::Instrument. El buffer i el MIDI es preparen fora del fil d'àudio. */
class AdaptadorPlugin : public mibase::Efecte, public mibase::Instrument
{
public:
    explicit AdaptadorPlugin (std::unique_ptr<juce::AudioPluginInstance> p);
    ~AdaptadorPlugin() override;

    void prepara (double fs, int maxBloc) override;
    void processa (float* L, float* R, int n) noexcept override;                                   // efecte
    void processa (const mibase::EventInstrument* ev, int num, float* L, float* R, int n) noexcept override;   // instrument
    void apagaTot() noexcept override { apagar.store (true); }
    int latencia() const noexcept override { return plugin->getLatencySamples(); }

    juce::AudioPluginInstance& instancia() noexcept { return *plugin; }
    std::string estat() const;
    void posaEstat (const std::string& base64);

    /** 0.25: per al diagnòstic (el fil d'àudio ho apunta sense bloquejos): quantes vegades s'ha cridat com a efecte
        i el pic que hi entra i en surt (l'últim bloc). */
    std::atomic<long long> crides { 0 };
    std::atomic<float> picEntrada { 0.0f }, picSortida { 0.0f };

private:
    void corre (int n) noexcept;

    std::unique_ptr<juce::AudioPluginInstance> plugin;
    juce::AudioBuffer<float> buf;
    juce::MidiBuffer midi;
    int canals = 2, maxBloc = 0;
    std::atomic<bool> apagar { false };
};

/** 0.25: el diagnòstic demana l'informe dels plugins del projecte obert (el posa la pantalla de l'estudi). */
inline std::function<juce::String()>& informePlugins() { static std::function<juce::String()> f; return f; }

/** Els plugins vius del projecte obert: un per insert (per id) i un per pista d'instrument VST3.
    Fil de la interfície. També obre i tanca les seves finestres. */
class GestorPlugins
{
public:
    explicit GestorPlugins (const juce::KnownPluginList& llista);
    ~GestorPlugins();

    /** El plugin viu d'un insert (el mateix mentre no canviï de plugin). nullptr si no s'ha pogut obrir. */
    std::shared_ptr<mibase::Efecte> efecte (const mibase::Insert& x, double fs, int maxBloc);
    /** L'instrument VST3 d'una pista («vst3:<id>»). */
    std::shared_ptr<mibase::Instrument> instrument (const mibase::Pista& t, double fs, int maxBloc);
    /** Còpies noves (per exportar sense tocar els que sonen), amb els ajustos d'ara. */
    std::shared_ptr<mibase::Efecte> efecteNou (const mibase::Insert& x, double fs, int bloc);
    std::shared_ptr<mibase::Instrument> instrumentNou (const mibase::Pista& t, double fs, int bloc);

    /** Copia els ajustos dels plugins vius al projecte. Torna cert si n'ha canviat algun. */
    bool desaEstats (mibase::Projecte& p);
    /** 0.27: tanca totes les finestres dels plugins (Esc). */
    void tancaFinestres() { finestres.clear(); }
    /** Oblida (i tanca) els que ja no són al projecte. */
    void neteja (const mibase::Projecte& p);

    /** Obre (o porta davant) la finestra del plugin d'un insert (clau = id) o d'una pista (clau = «inst:» + id). */
    /** 0.20: «lateral» (si n'hi ha) fa el panell que va enganxat a la dreta de l'editor (l'escala del teclat). */
    void obreFinestra (const std::string& clau, const juce::String& titol, std::function<std::unique_ptr<juce::Component>()> lateral = {});

    const juce::KnownPluginList& llista() const noexcept { return plugins; }
    /** Menú dels plugins (instruments o efectes), per fabricant. Els ids dels ítems comencen a «base». */
    juce::PopupMenu menu (bool instruments, int base, juce::Array<juce::PluginDescription>& tipus) const;

    static bool esVst3 (const std::string& instrument) { return instrument.rfind ("vst3:", 0) == 0; }
    juce::String getUltimError() const { return ultimError; }
    /** 0.25: com estan els efectes i instruments VST3 del projecte (per al diagnòstic). */
    juce::String informe (const mibase::Projecte& p) const;

private:
    std::shared_ptr<AdaptadorPlugin> crea (const std::string& id, const std::string& estat, double fs, int maxBloc);
    AdaptadorPlugin* viu (const std::string& clau);

    const juce::KnownPluginList& plugins;
    juce::AudioPluginFormatManager formats;
    struct Viu { std::string plugin; std::shared_ptr<AdaptadorPlugin> a; };
    std::map<std::string, Viu> vius;
    std::map<std::string, std::string> fallats;   // clau → plugin que no s'ha pogut obrir (no es torna a provar)
    std::map<std::string, std::unique_ptr<juce::DocumentWindow>> finestres;
    juce::String ultimError;
};
