#include "Plugins.h"
#include "EscGlobal.h"
#include "FinestraDelay.h"
#include "FinestraSaturacio.h"
#include "FinestraDrive.h"
#include "FinestraLimitador.h"
#include "FinestraVocalStudio.h"
#include "FinestraFlotant.h"
#include "FinestraDinamica.h"
#include "FinestraEq.h"
#include "FinestraReverb.h"
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
    // 0.56: si és un efecte amb entrada lateral (sidechain), s'encén (estèreo o, si no pot, mono).
    canalSc = -1;
    if (! plugin->getPluginDescription().isInstrument && plugin->getBusCount (true) > 1)
        if (auto* b = plugin->getBus (true, 1))
        {
            if (b->setCurrentLayout (juce::AudioChannelSet::stereo()) || b->setCurrentLayout (juce::AudioChannelSet::mono()))
                b->enable (true);
            if (b->isEnabled() && b->getNumberOfChannels() > 0)
                canalSc = plugin->getChannelIndexInProcessBlockBuffer (true, 1, 0);
        }
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
    float pe = 0.0f;
    for (int i = 0; i < n; ++i) pe = juce::jmax (pe, std::abs (L[i]), std::abs (R[i]));
    for (int fet = 0; fet < n;)
    {
        const int m = juce::jmin (n - fet, maxBloc);
        if (m <= 0) return;
        buf.clear();
        std::copy (L + fet, L + fet + m, buf.getWritePointer (0));
        std::copy (R + fet, R + fet + m, buf.getWritePointer (1));
        if (canalSc >= 2 && scL != nullptr && canalSc < canals)   // 0.56: sidechain a l'entrada lateral
        {
            std::copy (scL + fet, scL + fet + m, buf.getWritePointer (canalSc));
            if (canalSc + 1 < canals && plugin->getChannelCountOfBus (true, 1) > 1)
                std::copy (scR + fet, scR + fet + m, buf.getWritePointer (canalSc + 1));
        }
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
    float ps = 0.0f;
    for (int i = 0; i < n; ++i) ps = juce::jmax (ps, std::abs (L[i]), std::abs (R[i]));
    picEntrada.store (pe, std::memory_order_relaxed);
    picSortida.store (ps, std::memory_order_relaxed);
    crides.fetch_add (1, std::memory_order_relaxed);
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

// 0.56: paràmetres del plugin (automatització)
int AdaptadorPlugin::numParametres() const { return plugin->getParameters().size(); }

mibase::InfoParametre AdaptadorPlugin::infoParametre (int i) const
{
    mibase::InfoParametre r;
    const auto& ps = plugin->getParameters();
    if (i < 0 || i >= ps.size()) return r;
    auto* p = ps[i];
    if (auto* h = dynamic_cast<juce::HostedAudioProcessorParameter*> (p)) r.id = h->getParameterID().toStdString();
    if (r.id.empty()) r.id = "#" + std::to_string (i);
    r.nom = p->getName (80).toStdString();
    r.automatitzable = p->isAutomatable();
    r.passos = p->isDiscrete() ? juce::jmin (p->getNumSteps(), 1000) : 0;
    return r;
}

void AdaptadorPlugin::posaParametre (int i, float v01) noexcept
{
    const auto& ps = plugin->getParameters();
    if (i >= 0 && i < ps.size()) ps[i]->setValue (juce::jlimit (0.0f, 1.0f, v01));
}

float AdaptadorPlugin::parametre (int i) const noexcept
{
    const auto& ps = plugin->getParameters();
    return i >= 0 && i < ps.size() ? ps[i]->getValue() : 0.0f;
}

std::string AdaptadorPlugin::textParametre (int i, float v01) const
{
    const auto& ps = plugin->getParameters();
    if (i < 0 || i >= ps.size()) return {};
    auto t = ps[i]->getText (v01, 40);
    const auto u = ps[i]->getLabel();
    if (u.isNotEmpty() && ! t.endsWith (u)) t << " " << u;
    return t.toStdString();
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
        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }

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
    ultimFs = fs;
    ultimBloc = maxBloc;
    if (esIntern (x.plugin))
    {
        auto& r = reverbs[x.id];
        if (r == nullptr)
        {
            r = creaIntern (x);
            r->prepara (fs, maxBloc);
        }
        else if (sensePreparar.erase (x.id) > 0) r->prepara (fs, maxBloc);   // encara no és a cap pla: es pot preparar
        else if (adormides.erase (x.id) > 0) r->buida();
        return r;
    }
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
    if (esIntern (x.plugin))
    {
        auto it = reverbs.find (x.id);
        mibase::Insert y = x;
        if (it != reverbs.end()) y.estat = it->second->estat();
        auto r = creaIntern (y);
        if (auto* rv = dynamic_cast<mibase::reverb::Reverb*> (r.get()))
            rv->posa (mibase::reverb::FreezeOn, 0.0f);   // exportant no té sentit començar congelat
        r->prepara (fs, bloc);
        return r;
    }
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
    auto desaReverb = [&] (mibase::Insert& x)
    {
        auto it = reverbs.find (x.id);
        if (it == reverbs.end() || ! esIntern (x.plugin)) return false;
        auto e = it->second->estat();
        if (e != x.estat) { x.estat = std::move (e); canvi = true; }
        return true;
    };
    for (auto& x : p.insertsMaster) if (! desaReverb (x)) desa (x.id, x.estat);
    for (auto& t : p.pistes)
    {
        for (auto& x : t.inserts) if (! desaReverb (x)) desa (x.id, x.estat);
        if (esVst3 (t.instrument)) desa ("inst:" + t.id, t.estatInstrument);
    }
    return canvi;
}

void GestorPlugins::buidaCues()
{
    for (auto& [clau, r] : reverbs) r->buida();
    for (auto& [clau, v] : vius)
    {
        if (v.a == nullptr) continue;
        auto& p = v.a->instancia();
        p.suspendProcessing (true);    // espera que el fil d'àudio acabi el bloc i, mentrestant, en surt silenci
        p.reset();                     // oblida les cues (reverb, delay…) i el que tenia a dins
        p.suspendProcessing (false);
    }
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
    for (auto it = reverbs.begin(); it != reverbs.end();)
    {
        auto h = hiSon.find (it->first);
        if (h == hiSon.end() || ! esIntern (h->second))
        {
            finestres.erase (it->first);
            adormides.erase (it->first);
            sensePreparar.erase (it->first);
            it = reverbs.erase (it);   // el pla vell la manté viva mentre calgui
        }
        else ++it;
    }
    auto marca = [this] (const mibase::Insert& x) { if (! x.actiu && reverbs.count (x.id)) adormides.insert (x.id); };
    for (const auto& x : p.insertsMaster) marca (x);
    for (const auto& t : p.pistes) for (const auto& x : t.inserts) marca (x);
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
    if (auto r = reverbs.find (clau); r != reverbs.end())
    {
        if (auto rv = std::dynamic_pointer_cast<mibase::reverb::Reverb> (r->second))
            finestres[clau] = creaFinestraReverb (rv, titol, [this, clau] { finestres.erase (clau); }, sidechain, clau);
        else if (auto d = std::dynamic_pointer_cast<mibase::dinamica::Dinamica> (r->second))
            finestres[clau] = creaFinestraDinamica (d, titol, [this, clau] { finestres.erase (clau); }, sidechain, clau);
        else if (auto q = std::dynamic_pointer_cast<mibase::eq::Equalitzador> (r->second))
            finestres[clau] = creaFinestraEq (q, titol, [this, clau] { finestres.erase (clau); });
        else if (auto dl = std::dynamic_pointer_cast<mibase::delay::Delay> (r->second))   // 0.65
            finestres[clau] = creaFinestraDelay (dl, titol, [this, clau] { finestres.erase (clau); });
        else if (auto st = std::dynamic_pointer_cast<mibase::saturacio::Saturacio> (r->second))   // 0.66
            finestres[clau] = creaFinestraSaturacio (st, titol, [this, clau] { finestres.erase (clau); });
        else if (auto dv = std::dynamic_pointer_cast<mibase::drive::Drive> (r->second))   // 0.67
            finestres[clau] = creaFinestraDrive (dv, titol, [this, clau] { finestres.erase (clau); });
        else if (auto lm = std::dynamic_pointer_cast<mibase::limitador::Limitador> (r->second))   // 0.69
            finestres[clau] = creaFinestraLimitador (lm, titol, [this, clau] { finestres.erase (clau); });
        else if (auto rk = std::dynamic_pointer_cast<mibase::rack::Rack> (r->second))   // 0.70
            finestres[clau] = creaFinestraVocalStudio (rk, titol, [this, clau] { finestres.erase (clau); });
        if (auto f = finestres.find (clau); f != finestres.end() && f->second != nullptr) quedaSobreMibase (*f->second);   // 0.70
        return;
    }
    auto* a = viu (clau);
    if (a == nullptr) return;
    finestres[clau] = std::make_unique<FinestraPlugin> (titol, a->instancia(), [this, clau] { finestres.erase (clau); }, lateral ? lateral() : nullptr);
    quedaSobreMibase (*finestres[clau]);   // 0.70: no s'amaga darrere de Mibase en clicar-hi
}

std::shared_ptr<mibase::EfecteIntern> GestorPlugins::creaIntern (const mibase::Insert& x)
{
    std::shared_ptr<mibase::EfecteIntern> r;
    if (x.plugin == mibase::dinamica::idPlugin) r = std::make_shared<mibase::dinamica::Dinamica>();
    else if (x.plugin == mibase::eq::idPlugin) r = std::make_shared<mibase::eq::Equalitzador>();
    else if (x.plugin == mibase::saturacio::idPlugin) r = std::make_shared<mibase::saturacio::Saturacio>();   // 0.66
    else if (x.plugin == mibase::drive::idPlugin) r = std::make_shared<mibase::drive::Drive>();               // 0.67
    else if (x.plugin == mibase::limitador::idPlugin) r = std::make_shared<mibase::limitador::Limitador>();   // 0.69
    else if (x.plugin == mibase::rack::idPlugin)   // 0.70
    {
        auto rk = std::make_shared<mibase::rack::Rack>();
        rk->posaTempo (&RellotgePlugins::global().tempo);
        r = rk;
    }
    else if (x.plugin == mibase::delay::idPlugin)   // 0.65
    {
        auto dl = std::make_shared<mibase::delay::Delay>();
        dl->posaTempo (&RellotgePlugins::global().tempo);
        r = dl;
    }
    else
    {
        auto rv = std::make_shared<mibase::reverb::Reverb>();
        rv->posaTempo (&RellotgePlugins::global().tempo);
        r = rv;
    }
    r->posaEstat (x.estat);
    return r;
}

void GestorPlugins::obreIntern (const mibase::Insert& x, const juce::String& titol)
{
    if (! esIntern (x.plugin)) return;
    if (auto f = finestres.find (x.id); f != finestres.end()) { f->second->toFront (true); return; }
    auto& r = reverbs[x.id];
    if (r == nullptr)
    {
        r = creaIntern (x);
        sensePreparar.insert (x.id);
    }
    obreFinestra (x.id, titol);
}

mibase::Parametritzable* GestorPlugins::parametres (const mibase::Insert& x)
{
    if (esIntern (x.plugin))
    {
        auto& r = reverbs[x.id];
        if (r == nullptr) { r = creaIntern (x); sensePreparar.insert (x.id); }
        return r.get();
    }
    if (auto* a = viu (x.id)) return a;
    if (! x.actiu) return nullptr;
    return dynamic_cast<AdaptadorPlugin*> (efecte (x, ultimFs, ultimBloc).get());
}

mibase::Parametritzable* GestorPlugins::parametresInstrument (const std::string& idPista)
{
    return viu ("inst:" + idPista);
}

bool GestorPlugins::admetSidechain (const mibase::Insert& x)
{
    if (esIntern (x.plugin)) return true;
    if (auto* a = viu (x.id)) return a->admetSidechain();
    return false;
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

juce::String GestorPlugins::informe (const mibase::Projecte& p) const
{
    auto dB = [] (float v) { return v < 1e-6f ? juce::String ("-inf") : juce::String (20.0f * std::log10 (v), 1); };
    juce::String t;
    auto linia = [&] (const juce::String& on, const mibase::Insert& x)
    {
        t << "  " << on << " · " << juce::String::fromUTF8 (x.nom.c_str()) << (x.actiu ? " (encès)"_ca : " (apagat)"_ca) << ": ";
        if (mibase::esInsertVeu (x)) { t << "intern (Vocal Editor)\n"; return; }
        if (x.plugin == mibase::dinamica::idPlugin)
        {
            auto r = reverbs.find (x.id);
            auto* d = r != reverbs.end() ? dynamic_cast<mibase::dinamica::Dinamica*> (r->second.get()) : nullptr;
            if (d == nullptr) { t << "intern (Dinàmica), encara no creada\n"_ca; return; }
            t << "intern (Dinàmica) · "_ca << mibase::dinamica::nomMode ((int) d->valor (mibase::dinamica::ModeP)) << " · reducció "_ca
              << juce::String (d->mesures().reduccio.load(), 1) << " dB" << (d->mesures().sidechain.load() ? " · amb sidechain" : "") << "\n";
            return;
        }
        if (x.plugin == mibase::rack::idPlugin)   // 0.70
        {
            auto r = reverbs.find (x.id);
            auto* d = r != reverbs.end() ? dynamic_cast<mibase::rack::Rack*> (r->second.get()) : nullptr;
            if (d == nullptr) { t << "intern (Vocal Studio), encara no creat\n"_ca; return; }
            t << "intern (Vocal Studio) · "_ca << d->numModuls() << " mòduls:"_ca;
            for (int i = 0; i < d->numModuls(); ++i)
                t << " " << juce::String::fromUTF8 (mibase::rack::nomModul (d->modul (i)->clau, d->modul (i)->fx.get()).c_str()) << (d->modul (i)->on.load() ? "" : " (apagat)"_ca);
            t << " · latència "_ca << d->latencia() << " mostres\n";
            return;
        }
        if (x.plugin == mibase::limitador::idPlugin)   // 0.69
        {
            auto r = reverbs.find (x.id);
            auto* d = r != reverbs.end() ? dynamic_cast<mibase::limitador::Limitador*> (r->second.get()) : nullptr;
            if (d == nullptr) { t << "intern (Limitador), encara no creat\n"_ca; return; }
            t << "intern (Limitador) · "_ca << juce::String::fromUTF8 (mibase::limitador::nomEstil ((int) d->valor (mibase::limitador::EstilP)))
              << " · reducció " << juce::String (d->mesures().reduccio.load(), 1) << " dB · " << juce::String (d->mesures().lufsCurt.load(), 1)
              << " LUFS · latència " << d->latencia() << " mostres\n";
            return;
        }
        if (x.plugin == mibase::drive::idPlugin)   // 0.67
        {
            auto r = reverbs.find (x.id);
            auto* d = r != reverbs.end() ? dynamic_cast<mibase::drive::Drive*> (r->second.get()) : nullptr;
            if (d == nullptr) { t << "intern (Drive), encara no creat\n"_ca; return; }
            t << "intern (Drive) · "_ca << juce::String::fromUTF8 (mibase::drive::nomMode ((int) d->valor (mibase::drive::ModeP)))
              << " · gain " << juce::String (d->valor (mibase::drive::Gain), 1) << " · sortida " << dB (d->mesures().sortida.load()) << " dB\n";
            return;
        }
        if (x.plugin == mibase::saturacio::idPlugin)   // 0.66
        {
            auto r = reverbs.find (x.id);
            auto* s = r != reverbs.end() ? dynamic_cast<mibase::saturacio::Saturacio*> (r->second.get()) : nullptr;
            if (s == nullptr) { t << "intern (Decapitor), encara no creat\n"_ca; return; }
            t << "intern (Decapitor) · "_ca << juce::String::fromUTF8 (mibase::saturacio::nomLlargEstil ((int) s->valor (mibase::saturacio::EstilP)))
              << " · drive " << juce::String (s->driveDb(), 1) << " dB · sortida " << dB (s->mesures().sortida.load()) << " dB\n";
            return;
        }
        if (x.plugin == mibase::delay::idPlugin)   // 0.65
        {
            auto r = reverbs.find (x.id);
            auto* d = r != reverbs.end() ? dynamic_cast<mibase::delay::Delay*> (r->second.get()) : nullptr;
            if (d == nullptr) { t << "intern (Delay), encara no creat\n"_ca; return; }
            t << "intern (Delay) · "_ca << mibase::delay::nomCaracter ((int) d->valor (mibase::delay::CaracterP)) << " · "
              << mibase::delay::nomEncaminament ((int) d->valor (mibase::delay::EncaminamentP)) << " · "
              << juce::String (d->tempsL() * 1000.0, 0) << " ms · eco " << dB (d->mesures().eco.load()) << " dB"
              << (d->valor (mibase::delay::Freeze) > 0.5f ? " · freeze" : "") << "\n";
            return;
        }
        if (x.plugin == mibase::eq::idPlugin)
        {
            auto r = reverbs.find (x.id);
            auto* q = r != reverbs.end() ? dynamic_cast<mibase::eq::Equalitzador*> (r->second.get()) : nullptr;
            if (q == nullptr) { t << "intern (EQ Pro), encara no creat\n"_ca; return; }
            int n = 0;
            for (int k = 0; k < mibase::eq::maxBandes; ++k) if (q->banda (k).usada) ++n;
            t << "intern (EQ Pro) · "_ca << n << " bandes · sortida "_ca << juce::String (q->sortida(), 1) << " dB" << (q->bypass() ? " · bypass"_ca : juce::String())
              << " · "_ca << juce::String (q->frequenciaMostreig(), 0) << " Hz\n";
            return;
        }
        if (x.plugin == mibase::reverb::idPlugin)
        {
            auto r = reverbs.find (x.id);
            auto* rv = r != reverbs.end() ? dynamic_cast<mibase::reverb::Reverb*> (r->second.get()) : nullptr;
            if (rv == nullptr) { t << "intern (Reverb), encara no creada\n"; return; }
            const auto& m = rv->mesures();
            const auto p = rv->params();
            t << "intern (Reverb) · " << mibase::reverb::nomAlgorisme ((int) p[mibase::reverb::Alg]) << " · qualitat "_ca
              << juce::String::fromUTF8 (mibase::reverb::textValor (mibase::reverb::Qualitat, p[mibase::reverb::Qualitat]).c_str())
              << " · blocs " << (juce::int64) m.blocs.load() << " · entra " << dB (m.entrada.load()) << " dB · surt " << dB (m.sortida.load()) << " dB"
              << (adormides.count (x.id) ? " · apagada" : "") << (m.sidechain.load() ? " · ducking amb sidechain" : "")
              << (rv->referenciaIR().empty() ? juce::String() : " · IR " + juce::String::fromUTF8 (rv->referenciaIR().c_str()) + (rv->irFalta() ? " (NO ES TROBA)" : ""))
              << "\n";
            return;
        }
        auto it = vius.find (x.id);
        if (it != vius.end() && it->second.a != nullptr)
        {
            auto& a = *it->second.a;
            auto& in = const_cast<AdaptadorPlugin&> (a).instancia();
            t << "obert · E/S " << in.getTotalNumInputChannels() << "/" << in.getTotalNumOutputChannels()
              << " · latència "_ca << in.getLatencySamples() << " · " << (in.isSuspended() ? "SUSPÈS · "_ca : juce::String())
              << "crides " << (juce::int64) a.crides.load() << " · entra " << dB (a.picEntrada.load()) << " dB · surt " << dB (a.picSortida.load()) << " dB\n";
        }
        else if (auto f = fallats.find (x.id); f != fallats.end()) t << "NO S'HA POGUT OBRIR\n";
        else t << (x.actiu ? "encara no s'ha creat\n" : "no es crea (apagat)\n");
    };
    for (auto& x : p.insertsMaster) linia ("Màster"_ca, x);
    for (auto& tr : p.pistes)
        for (auto& x : tr.inserts) linia (juce::String::fromUTF8 (tr.nom.c_str()), x);
    if (t.isEmpty()) t << "  (cap efecte al projecte)\n";
    if (ultimError.isNotEmpty()) t << "  Últim error: "_ca << ultimError << "\n";
    return t;
}

int GestorPlugins::latencia (const std::string& idInsert) const
{
    // 0.68: per mostrar el retard de cada efecte (els VST3 el diuen; els interns de Mibase no en tenen)
    if (auto r = reverbs.find (idInsert); r != reverbs.end() && r->second != nullptr) return std::max (0, r->second->latencia());
    if (auto v = vius.find (idInsert); v != vius.end() && v->second.a != nullptr) return std::max (0, v->second.a->latencia());
    return 0;
}
