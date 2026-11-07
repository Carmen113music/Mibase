#include "Plugins.h"
#include "Tema.h"
#include "Text.h"

//==============================================================================
// AdaptadorPlugin
//==============================================================================
AdaptadorPlugin::AdaptadorPlugin (std::unique_ptr<juce::AudioPluginInstance> p) : plugin (std::move (p))
{
    plugin->setPlayHead (&RellotgePlugins::global());
}

AdaptadorPlugin::~AdaptadorPlugin()
{
    plugin->releaseResources();
}

void AdaptadorPlugin::prepara (double fs, int nouMaxBloc)
{
    // Estèreo si el plugin ho accepta (si no, el que porti).
    for (bool entrada : { true, false })
        if (auto* b = plugin->getBus (entrada, 0))
            b->setCurrentLayout (juce::AudioChannelSet::stereo());
    plugin->setRateAndBufferSizeDetails (fs, nouMaxBloc);
    plugin->prepareToPlay (fs, nouMaxBloc);
    maxBloc = nouMaxBloc;
    canals = juce::jmax (2, plugin->getTotalNumInputChannels(), plugin->getTotalNumOutputChannels());
    buf.setSize (canals, maxBloc);
    midi.ensureSize (8192);
}

void AdaptadorPlugin::corre (int n) noexcept
{
    juce::AudioBuffer<float> b (buf.getArrayOfWritePointers(), canals, n);
    const juce::ScopedLock l (plugin->getCallbackLock());
    if (plugin->isSuspended()) { b.clear(); return; }
    plugin->processBlock (b, midi);
}

void AdaptadorPlugin::processa (float* L, float* R, int n) noexcept
{
    for (int fet = 0; fet < n;)
    {
        const int m = juce::jmin (n - fet, maxBloc);
        if (m <= 0) return;
        buf.clear();
        std::copy (L + fet, L + fet + m, buf.getWritePointer (0));
        std::copy (R + fet, R + fet + m, buf.getWritePointer (1));
        midi.clear();
        const bool passaDeLlarg = plugin->getTotalNumInputChannels() == 0;
        corre (m);
        if (! passaDeLlarg)
        {
            const int sortides = plugin->getTotalNumOutputChannels();
            std::copy (buf.getReadPointer (0), buf.getReadPointer (0) + m, L + fet);
            std::copy (buf.getReadPointer (sortides >= 2 ? 1 : 0), buf.getReadPointer (sortides >= 2 ? 1 : 0) + m, R + fet);
        }
        fet += m;
    }
}

void AdaptadorPlugin::processa (const mibase::EventInstrument* ev, int num, float* L, float* R, int n) noexcept
{
    n = juce::jmin (n, maxBloc);
    buf.clear();
    midi.clear();
    if (apagar.exchange (false))
        for (int c = 0; c < 16; ++c)
        {
            const juce::uint8 a[3] = { (juce::uint8) (0xB0 | c), 123, 0 }, s[3] = { (juce::uint8) (0xB0 | c), 120, 0 };
            midi.addEvent (a, 3, 0);
            midi.addEvent (s, 3, 0);
        }
    for (int i = 0; i < num; ++i)
    {
        const auto& e = ev[i];
        const int tipus = e.estat & 0xF0;
        const juce::uint8 d[3] = { e.estat, e.dada1, e.dada2 };
        midi.addEvent (d, (tipus == 0xC0 || tipus == 0xD0) ? 2 : 3, juce::jlimit (0, juce::jmax (0, n - 1), e.offset));
    }
    corre (n);
    const int sortides = plugin->getTotalNumOutputChannels();
    std::copy (buf.getReadPointer (0), buf.getReadPointer (0) + n, L);
    std::copy (buf.getReadPointer (sortides >= 2 ? 1 : 0), buf.getReadPointer (sortides >= 2 ? 1 : 0) + n, R);
}

std::string AdaptadorPlugin::estat() const
{
    juce::MemoryBlock m;
    plugin->getStateInformation (m);
    return m.toBase64Encoding().toStdString();
}

void AdaptadorPlugin::posaEstat (const std::string& base64)
{
    if (base64.empty()) return;
    juce::MemoryBlock m;
    if (m.fromBase64Encoding (juce::String (base64)))
        plugin->setStateInformation (m.getData(), (int) m.getSize());
}

//==============================================================================
// Finestra d'un plugin
//==============================================================================
namespace
{
    class FinestraPlugin : public juce::DocumentWindow
    {
    public:
        FinestraPlugin (const juce::String& titol, juce::AudioProcessor& p, std::function<void()> enTancar, std::unique_ptr<juce::Component> lateral)
            : juce::DocumentWindow (titol, tema::fons, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
              tancar (std::move (enTancar))
        {
            setUsingNativeTitleBar (true);
            juce::AudioProcessorEditor* ed = p.hasEditor() ? p.createEditorIfNeeded() : nullptr;
            if (ed == nullptr) ed = new juce::GenericAudioProcessorEditor (p);
            const bool redim = ed->isResizable();
            if (lateral != nullptr) setContentOwned (new AmbLateral (ed, std::move (lateral)), true);
            else setContentOwned (ed, true);
            setResizable (redim, false);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::MessageManager::callAsync ([t = tancar] { t(); }); }

    private:
        std::function<void()> tancar;
    };
}

//==============================================================================
// GestorPlugins
//==============================================================================
GestorPlugins::GestorPlugins (const juce::KnownPluginList& l) : plugins (l)
{
    formats.addFormat (std::make_unique<juce::VST3PluginFormat>());
}

GestorPlugins::~GestorPlugins()
{
    finestres.clear();   // els editors, abans que els plugins
}

std::shared_ptr<AdaptadorPlugin> GestorPlugins::crea (const std::string& id, const std::string& estat, double fs, int maxBloc)
{
    auto desc = plugins.getTypeForIdentifierString (juce::String (id));
    if (desc == nullptr)
    {
        ultimError = "No es troba el plugin a la llista (busca'l a Preferències)"_ca;
        return nullptr;
    }
    juce::String error;
    auto inst = formats.createPluginInstance (*desc, fs, maxBloc, error);
    if (inst == nullptr)
    {
        ultimError = "No s'ha pogut obrir " + desc->name + ": " + error;
        return nullptr;
    }
    auto a = std::make_shared<AdaptadorPlugin> (std::move (inst));
    a->prepara (fs, maxBloc);
    a->posaEstat (estat);
    return a;
}

AdaptadorPlugin* GestorPlugins::viu (const std::string& clau)
{
    auto it = vius.find (clau);
    return it != vius.end() ? it->second.a.get() : nullptr;
}

std::shared_ptr<mibase::Efecte> GestorPlugins::efecte (const mibase::Insert& x, double fs, int maxBloc)
{
    auto& v = vius[x.id];
    if (v.a == nullptr || v.plugin != x.plugin)
    {
        if (fallats.count (x.id) && fallats[x.id] == x.plugin) return nullptr;
        finestres.erase (x.id);
        v.a = crea (x.plugin, x.estat, fs, maxBloc);
        v.plugin = x.plugin;
        if (v.a == nullptr) { fallats[x.id] = x.plugin; vius.erase (x.id); return nullptr; }
    }
    return v.a;
}

std::shared_ptr<mibase::Instrument> GestorPlugins::instrument (const mibase::Pista& t, double fs, int maxBloc)
{
    if (! esVst3 (t.instrument)) return nullptr;
    const auto clau = "inst:" + t.id;
    const auto id = t.instrument.substr (5);
    auto& v = vius[clau];
    if (v.a == nullptr || v.plugin != id)
    {
        if (fallats.count (clau) && fallats[clau] == id) return nullptr;
        finestres.erase (clau);
        v.a = crea (id, t.estatInstrument, fs, maxBloc);
        v.plugin = id;
        if (v.a == nullptr) { fallats[clau] = id; vius.erase (clau); return nullptr; }
    }
    return v.a;
}

std::shared_ptr<mibase::Efecte> GestorPlugins::efecteNou (const mibase::Insert& x, double fs, int bloc)
{
    auto* a = viu (x.id);
    return crea (x.plugin, a != nullptr ? a->estat() : x.estat, fs, bloc);
}

std::shared_ptr<mibase::Instrument> GestorPlugins::instrumentNou (const mibase::Pista& t, double fs, int bloc)
{
    if (! esVst3 (t.instrument)) return nullptr;
    auto* a = viu ("inst:" + t.id);
    return crea (t.instrument.substr (5), a != nullptr ? a->estat() : t.estatInstrument, fs, bloc);
}

bool GestorPlugins::desaEstats (mibase::Projecte& p)
{
    bool canvi = false;
    auto desa = [&] (const std::string& clau, std::string& desti)
    {
        if (auto* a = viu (clau))
        {
            auto e = a->estat();
            if (e != desti) { desti = std::move (e); canvi = true; }
        }
    };
    for (auto& x : p.insertsMaster) desa (x.id, x.estat);
    for (auto& t : p.pistes)
    {
        for (auto& x : t.inserts) desa (x.id, x.estat);
        if (esVst3 (t.instrument)) desa ("inst:" + t.id, t.estatInstrument);
    }
    return canvi;
}

void GestorPlugins::neteja (const mibase::Projecte& p)
{
    std::map<std::string, std::string> hiSon;   // clau → plugin
    for (const auto& x : p.insertsMaster) hiSon[x.id] = x.plugin;
    for (const auto& t : p.pistes)
    {
        for (const auto& x : t.inserts) hiSon[x.id] = x.plugin;
        if (esVst3 (t.instrument)) hiSon["inst:" + t.id] = t.instrument.substr (5);
    }
    for (auto it = vius.begin(); it != vius.end();)
    {
        auto h = hiSon.find (it->first);
        if (h == hiSon.end() || h->second != it->second.plugin)
        {
            finestres.erase (it->first);
            it = vius.erase (it);   // el pla vell el manté viu mentre calgui
        }
        else ++it;
    }
}

void GestorPlugins::obreFinestra (const std::string& clau, const juce::String& titol, std::function<std::unique_ptr<juce::Component>()> lateral)
{
    if (auto f = finestres.find (clau); f != finestres.end())
    {
        f->second->toFront (true);
        return;
    }
    auto* a = viu (clau);
    if (a == nullptr) return;
    finestres[clau] = std::make_unique<FinestraPlugin> (titol, a->instancia(), [this, clau] { finestres.erase (clau); }, lateral ? lateral() : nullptr);
}

juce::PopupMenu GestorPlugins::menu (bool instruments, int base, juce::Array<juce::PluginDescription>& tipus) const
{
    std::map<juce::String, juce::PopupMenu> perFabricant;
    for (const auto& t : plugins.getTypes())
    {
        if (t.isInstrument != instruments) continue;
        tipus.add (t);
        perFabricant[t.manufacturerName.isEmpty() ? "(sense fabricant)"_ca : t.manufacturerName].addItem (base + tipus.size() - 1, t.name);
    }
    juce::PopupMenu m;
    for (auto& [f, sub] : perFabricant) m.addSubMenu (f, sub);
    if (perFabricant.empty())
        m.addItem (base - 1, "Cap plugin: busca'ls a Fitxer > Preferències"_ca, false);
    return m;
}
