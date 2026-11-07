#include "EscanerPlugins.h"
#include "Text.h"

EscanerPlugins::EscanerPlugins (const juce::File& f) : juce::Thread ("Mibase: busca plugins"), fitxer (f)
{
    carrega();
}

EscanerPlugins::~EscanerPlugins()
{
    atura();
}

void EscanerPlugins::atura()
{
    stopThread (5000);
}

int EscanerPlugins::escanejaUn (const juce::String& fitxerPlugin, const juce::String& sortida)
{
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> tipus;
    format.findAllTypesForFile (tipus, fitxerPlugin);
    juce::XmlElement arrel ("TIPUS");
    for (auto* t : tipus)
        arrel.addChildElement (t->createXml().release());
    return arrel.writeTo (juce::File (sortida)) ? 0 : 2;
}

void EscanerPlugins::comenca (bool tot)
{
    if (isThreadRunning()) return;
    if (tot) { plugins.clear(); errs.clear(); }
    {
        const juce::ScopedLock l (seccio);
        nous.clear(); errsNous.clear(); mirats.clear(); acabat = false;
        textProgres = "Buscant plugins…"_ca;
    }
    fitxersErrors.clear();
    for (const auto& e : errs) fitxersErrors.add (e.fitxer);
    startThread();
    if (onCanvi) onCanvi();
}

juce::String EscanerPlugins::progres() const
{
    const juce::ScopedLock l (seccio);
    return isThreadRunning() ? textProgres : juce::String();
}

int EscanerPlugins::numInstruments() const
{
    int n = 0;
    for (const auto& t : plugins.getTypes()) n += t.isInstrument ? 1 : 0;
    return n;
}

int EscanerPlugins::numEfectes() const
{
    return plugins.getNumTypes() - numInstruments();
}

void EscanerPlugins::run()
{
    juce::VST3PluginFormat format;
    const auto fitxers = format.searchPathsForPlugins (format.getDefaultLocationsToSearch(), true, false);

    // Ja coneguts i sense canvis (o amb error de la vegada anterior): no cal tornar-los a obrir.
    juce::StringArray saltar;
    for (const auto& t : plugins.getTypes())
        if (juce::File (t.fileOrIdentifier).getLastModificationTime() == t.lastFileModTime)
            saltar.addIfNotAlreadyThere (t.fileOrIdentifier);
    saltar.addArray (fitxersErrors);

    const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName();
    const auto temporal = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("mibase-plugin.xml");

    int k = 0;
    for (const auto& f : fitxers)
    {
        ++k;
        if (threadShouldExit()) break;
        if (saltar.contains (f)) { const juce::ScopedLock l (seccio); mirats.add (f); continue; }
        {
            const juce::ScopedLock l (seccio);
            textProgres = "Mirant " + juce::String (k) + " de " + juce::String (fitxers.size()) + ": " + juce::File (f).getFileNameWithoutExtension();
        }
        triggerAsyncUpdate();

        temporal.deleteFile();
        juce::ChildProcess fill;
        juce::String motiu;
        juce::OwnedArray<juce::PluginDescription> trobats;
        if (! fill.start (juce::StringArray { exe, argument, f, temporal.getFullPathName() }, 0))
            motiu = "No s'ha pogut engegar la prova";
        else
        {
            const auto inici = juce::Time::getMillisecondCounter();
            while (fill.isRunning() && juce::Time::getMillisecondCounter() - inici < 60000 && ! threadShouldExit())
                wait (50);
            if (fill.isRunning())
            {
                fill.kill();
                motiu = threadShouldExit() ? juce::String() : "No respon (més d'un minut)"_ca;
            }
            else if (fill.getExitCode() != 0)
                motiu = "Ha petat en obrir-lo"_ca;
            else if (auto xml = juce::parseXML (temporal))
            {
                for (auto* e : xml->getChildIterator())
                {
                    auto d = std::make_unique<juce::PluginDescription>();
                    if (d->loadFromXml (*e)) trobats.add (d.release());
                }
                if (trobats.isEmpty()) motiu = "No s'ha pogut obrir (no és un VST3 vàlid?)"_ca;
            }
            else
                motiu = "No ha tornat cap resposta";
        }
        if (threadShouldExit() && motiu.isEmpty() && trobats.isEmpty()) break;

        const juce::ScopedLock l (seccio);
        mirats.add (f);
        if (motiu.isNotEmpty()) errsNous.push_back ({ f, motiu });
        for (auto* d : trobats) nous.add (new juce::PluginDescription (*d));
    }
    temporal.deleteFile();

    {
        const juce::ScopedLock l (seccio);
        acabat = true;
        // Els que ja no hi són (desinstal·lats) es treuen de la llista.
        if (! threadShouldExit())
            for (const auto& t : plugins.getTypes())
                if (! fitxers.contains (t.fileOrIdentifier))
                    errsNous.push_back ({ t.fileOrIdentifier, "__treu" });
        if (! threadShouldExit())
            for (const auto& f : fitxersErrors)
                if (! fitxers.contains (f))
                    errsNous.push_back ({ f, "__treu" });
    }
    triggerAsyncUpdate();
}

void EscanerPlugins::handleAsyncUpdate()
{
    bool desar = false;
    {
        const juce::ScopedLock l (seccio);
        for (auto* d : nous)
        {
            for (const auto& t : plugins.getTypes())
                if (t.fileOrIdentifier == d->fileOrIdentifier && t.uniqueId == d->uniqueId) plugins.removeType (t);
            plugins.addType (*d);
        }
        for (const auto& e : errsNous)
        {
            errs.erase (std::remove_if (errs.begin(), errs.end(), [&e] (const Error& x) { return x.fitxer == e.fitxer; }), errs.end());
            if (e.motiu == "__treu")
            {
                for (const auto& t : plugins.getTypes())
                    if (t.fileOrIdentifier == e.fitxer) plugins.removeType (t);
            }
            else
                errs.push_back (e);
        }
        desar = ! nous.isEmpty() || ! errsNous.empty() || acabat;
        nous.clear(); errsNous.clear(); mirats.clear();
        acabat = false;
    }
    if (desar) desa();
    if (onCanvi) onCanvi();
}

void EscanerPlugins::carrega()
{
    auto xml = juce::parseXML (fitxer);
    if (xml == nullptr) return;
    if (auto* l = xml->getChildByName ("KNOWNPLUGINS"))
        plugins.recreateFromXml (*l);
    if (auto* e = xml->getChildByName ("ERRORS"))
        for (auto* c : e->getChildIterator())
            errs.push_back ({ c->getStringAttribute ("fitxer"), c->getStringAttribute ("motiu") });
}

void EscanerPlugins::desa()
{
    juce::XmlElement arrel ("MIBASE_PLUGINS");
    if (auto l = plugins.createXml())
        arrel.addChildElement (l.release());
    auto* e = arrel.createNewChildElement ("ERRORS");
    for (const auto& x : errs)
    {
        auto* c = e->createNewChildElement ("ERROR");
        c->setAttribute ("fitxer", x.fitxer);
        c->setAttribute ("motiu", x.motiu);
    }
    fitxer.getParentDirectory().createDirectory();
    arrel.writeTo (fitxer);
}
