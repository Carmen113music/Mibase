#include "PantallaDiagnostic.h"

#include <cmath>

namespace
{
    constexpr int marge = 12;
    constexpr int alcadaTitol = 30;
    constexpr int alcadaFila = 30;

    float aDecibels (float pic) { return pic > 0.0f ? 20.0f * std::log10 (pic) : -100.0f; }

    // Posició 0..1 d'un nivell a la barra (de -60 dB a 0 dB).
    float posicioBarra (float pic) { return juce::jlimit (0.0f, 1.0f, (aDecibels (pic) + 60.0f) / 60.0f); }

    void preparaEtiqueta (juce::Label& l, const juce::String& text, juce::Colour color = tema::suau)
    {
        l.setText (text, juce::dontSendNotification);
        l.setColour (juce::Label::textColourId, color);
        l.setFont (juce::FontOptions (14.0f));
    }

    juce::String nomsActius (const juce::StringArray& noms, const juce::BigInteger& actius)
    {
        juce::StringArray r;
        for (int i = 0; i < noms.size(); ++i)
            if (actius[i])
                r.add (noms[i]);
        return r.isEmpty() ? juce::String ("cap") : r.joinIntoString (", ");
    }

    juce::String ms (int mostres, double fs)
    {
        return fs > 0 ? juce::String (1000.0 * mostres / fs, 1) + " ms" : juce::String ("?");
    }
}

//==============================================================================
void Vumetres::setCanals (const juce::StringArray& noms)
{
    if ((int) canals.size() == noms.size())
    {
        bool iguals = true;
        for (int i = 0; i < noms.size(); ++i)
            iguals = iguals && canals[(size_t) i].nom == noms[i];
        if (iguals)
            return;
    }

    canals.clear();
    for (auto& n : noms)
        canals.push_back ({ n });
    repaint();
}

void Vumetres::setNivell (int canal, float pic, bool saturat)
{
    if (canal < 0 || canal >= (int) canals.size())
        return;

    auto& c = canals[(size_t) canal];
    // Puja de cop i baixa a poc a poc.
    c.nivell = pic > c.nivell ? pic : c.nivell * 0.85f;
    if (pic >= c.retingut) { c.retingut = pic; c.retencio = 45; }
    else if (--c.retencio <= 0) c.retingut *= 0.95f;
    if (saturat) c.vermell = 60;
    else if (c.vermell > 0) --c.vermell;
}

void Vumetres::paint (juce::Graphics& g)
{
    auto zona = getLocalBounds();

    if (canals.empty())
    {
        g.setColour (tema::suau);
        g.setFont (juce::FontOptions (14.0f));
        g.drawText ("No hi ha cap entrada activa.", zona, juce::Justification::centredLeft);
        return;
    }

    const int alcada = juce::jmin (26, zona.getHeight() / (int) canals.size());

    for (auto& c : canals)
    {
        auto fila = zona.removeFromTop (alcada);
        auto nom = fila.removeFromLeft (150);
        g.setColour (tema::text);
        g.setFont (juce::FontOptions (13.0f));
        g.drawFittedText (c.nom, nom.reduced (2, 0), juce::Justification::centredLeft, 1);

        auto barra = fila.reduced (4, 6).toFloat();
        g.setColour (tema::superficie2);
        g.fillRoundedRectangle (barra, 3.0f);

        const float x = posicioBarra (c.nivell);
        auto ple = barra.withWidth (barra.getWidth() * x);
        const auto color = c.vermell > 0 ? tema::error : (aDecibels (c.nivell) > -6.0f ? tema::guitarra : tema::sinte);
        g.setColour (color);
        g.fillRoundedRectangle (ple, 3.0f);

        // Marques de -48, -24, -12, -6 dB.
        g.setColour (tema::fons.withAlpha (0.7f));
        for (float db : { -48.0f, -24.0f, -12.0f, -6.0f })
        {
            const float px = barra.getX() + barra.getWidth() * (db + 60.0f) / 60.0f;
            g.drawVerticalLine ((int) px, barra.getY(), barra.getBottom());
        }

        // Pic retingut.
        const float pr = barra.getX() + barra.getWidth() * posicioBarra (c.retingut);
        g.setColour (tema::text);
        g.fillRect (juce::Rectangle<float> (pr - 1.0f, barra.getY(), 2.0f, barra.getHeight()));

        g.setColour (c.vermell > 0 ? tema::error : tema::suau);
        g.setFont (juce::FontOptions (12.0f));
        const float db = aDecibels (c.retingut);
        g.drawText (db > -99.0f ? juce::String (db, 1) + " dB" : juce::String ("-∞"_ca),
                    fila.removeFromRight (70).withX (getWidth() - 70), juce::Justification::centredRight);
    }
}

//==============================================================================
void LlistaMidi::afegeix (const juce::String& text)
{
    linies.insert (0, text);
    if (linies.size() > 40)
        linies.removeRange (40, linies.size() - 40);
    repaint();
}

void LlistaMidi::paint (juce::Graphics& g)
{
    g.setColour (tema::superficie2);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);

    auto zona = getLocalBounds().reduced (8, 6);
    g.setFont (juce::FontOptions (13.0f));

    if (linies.isEmpty())
    {
        g.setColour (tema::suau);
        g.drawText ("Toca una tecla, un pad o un potenciòmetre del teclat MIDI."_ca, zona, juce::Justification::topLeft);
        return;
    }

    for (int i = 0; i < linies.size() && zona.getHeight() >= 18; ++i)
    {
        g.setColour (i == 0 ? tema::midi : tema::text.withAlpha (juce::jmax (0.35f, 1.0f - i * 0.06f)));
        g.drawText (linies[i], zona.removeFromTop (19), juce::Justification::centredLeft);
    }
}

//==============================================================================
PantallaDiagnostic::PantallaDiagnostic (ServeiAudio& s, Historial& h)
    : servei (s), historial (h)
{
    preparaEtiqueta (etTipus, "Controlador");
    preparaEtiqueta (etSortida, "Targeta");
    preparaEtiqueta (etEntrada, "Entrada");
    preparaEtiqueta (etFreq, "Freqüència"_ca);
    preparaEtiqueta (etBuffer, "Buffer");

    for (auto* c : { &etTipus, &etSortida, &etEntrada, &etFreq, &etBuffer })
        addAndMakeVisible (c);

    for (auto* c : { &cbTipus, &cbSortida, &cbEntrada, &cbFreq, &cbBuffer })
    {
        c->setTextWhenNothingSelected ("(tria'n un)"_ca);
        c->setTextWhenNoChoicesAvailable ("(no n'hi ha cap)"_ca);
        addAndMakeVisible (c);
    }

    cbTipus.onChange   = [this] { if (! refrescant) triaTipus(); };
    cbSortida.onChange = [this] { if (! refrescant) triaDispositiu(); };
    cbEntrada.onChange = [this] { if (! refrescant) triaDispositiu(); };
    cbFreq.onChange    = [this] { if (! refrescant) triaFreqOBuffer(); };
    cbBuffer.onChange  = [this] { if (! refrescant) triaFreqOBuffer(); };

    addAndMakeVisible (btPanell);
    btPanell.onClick = [this]
    {
        if (auto* d = servei.dispositius().getCurrentAudioDevice(); d != nullptr && d->hasControlPanel())
        {
            historial.afegeix ("Obert el panell del controlador.");
            if (d->showControlPanel())
                servei.dispositius().restartLastAudioDevice();
        }
    };

    addAndMakeVisible (btBusca);
    btBusca.onClick = [this]
    {
        for (auto* t : servei.dispositius().getAvailableDeviceTypes())
            t->scanForDevices();
        servei.actualitzaMidi();
        historial.afegeix ("Dispositius tornats a buscar.");
        refrescaControls();
    };

    // Proves de so.
    addAndMakeVisible (btClic);
    btClic.setClickingTogglesState (true);
    btClic.onClick = [this]
    {
        servei.motor().clicActiu = btClic.getToggleState();
        historial.afegeix (btClic.getToggleState() ? "Clic de prova engegat." : "Clic de prova aturat.");
    };

    addAndMakeVisible (btSinte);
    btSinte.setClickingTogglesState (true);
    btSinte.setToggleState (true, juce::dontSendNotification);
    btSinte.onClick = [this] { servei.motor().sinteActiu = btSinte.getToggleState(); };

    preparaEtiqueta (etTempo, "Tempo");
    addAndMakeVisible (etTempo);
    addAndMakeVisible (tempo);
    tempo.setSliderStyle (juce::Slider::LinearHorizontal);
    tempo.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 24);
    tempo.setRange (40.0, 240.0, 1.0);
    tempo.setValue (120.0, juce::dontSendNotification);
    tempo.setTextValueSuffix (" bpm");
    tempo.onValueChange = [this] { servei.motor().tempoClic = tempo.getValue(); };

    // Latència.
    preparaEtiqueta (etAjudaLatencia,
        "Connecta un cable de la sortida 1 de la targeta a l'entrada 1 (o posa el micròfon "_ca
        "davant d'un altaveu, amb el volum baix). Abaixa els auriculars: sonarà un soroll curt 5 vegades."_ca);
    etAjudaLatencia.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (etAjudaLatencia);

    addAndMakeVisible (btLatencia);
    btLatencia.onClick = [this]
    {
        if (servei.motor().latencia().estaMesurant())
        {
            servei.motor().latencia().cancella();
            historial.afegeix ("Mesura de latència cancel·lada."_ca);
        }
        else
        {
            comencaLatencia();
        }
    };

    preparaEtiqueta (etResultatLatencia, {}, tema::text);
    etResultatLatencia.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (etResultatLatencia);

    addAndMakeVisible (vumetres);

    preparaEtiqueta (etTeclats, {});
    addAndMakeVisible (etTeclats);
    addAndMakeVisible (llistaMidi);

    preparaEtiqueta (etEstat, {}, tema::text);
    etEstat.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (etEstat);

    historialText.setMultiLine (true);
    historialText.setReadOnly (true);
    historialText.setScrollbarsShown (true);
    historialText.setCaretVisible (false);
    historialText.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (historialText);

    addAndMakeVisible (btCopia);
    btCopia.onClick = [this]
    {
        juce::SystemClipboard::copyTextToClipboard (textDiagnostic());
        historial.afegeix ("Diagnòstic copiat. Ja el pots enganxar."_ca);
    };

    servei.dispositius().addChangeListener (this);
    refrescaControls();
    actualitzaEstat();
    startTimerHz (30);
}

PantallaDiagnostic::~PantallaDiagnostic()
{
    servei.dispositius().removeChangeListener (this);
}

//==============================================================================
void PantallaDiagnostic::paint (juce::Graphics& g)
{
    g.fillAll (tema::fons);

    auto seccio = [&] (juce::Rectangle<int> z, const juce::String& titol, juce::Colour color)
    {
        g.setColour (tema::superficie);
        g.fillRoundedRectangle (z.toFloat(), 8.0f);
        g.setColour (tema::linia);
        g.drawRoundedRectangle (z.toFloat().reduced (0.5f), 8.0f, 1.0f);

        auto t = z.reduced (14, 0).removeFromTop (alcadaTitol);
        g.setColour (color);
        g.fillRect (t.getX(), t.getCentreY() - 6, 3, 12);
        g.setColour (tema::text);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawText (titol, t.withTrimmedLeft (10), juce::Justification::centredLeft);
    };

    seccio (zonaAudio,     "Targeta d'àudio"_ca,          tema::accent);
    seccio (zonaProves,    "Proves de so",             tema::sinte);
    seccio (zonaLatencia,  "Latència real"_ca,            tema::guitarra);
    seccio (zonaEntrades,  "Entrades",                 tema::veu);
    seccio (zonaMidi,      "Teclat MIDI",              tema::midi);
    seccio (zonaEstat,     "Estat",                    tema::bateria);
    seccio (zonaHistorial, "Historial (el més recent a dalt)"_ca, tema::suau);

    // Barra de progrés de la mesura de latència.
    if (servei.motor().latencia().estaMesurant())
    {
        auto barra = btLatencia.getBounds().translated (0, btLatencia.getHeight() + 4).withHeight (4).toFloat();
        g.setColour (tema::superficie2);
        g.fillRoundedRectangle (barra, 2.0f);
        g.setColour (tema::guitarra);
        g.fillRoundedRectangle (barra.withWidth (barra.getWidth() * servei.motor().latencia().progres()), 2.0f);
    }
}

void PantallaDiagnostic::resized()
{
    auto zona = getLocalBounds().reduced (marge);

    zonaHistorial = zona.removeFromBottom (170);
    zona.removeFromBottom (marge);

    auto esquerra = zona.removeFromLeft (juce::jmax (480, zona.getWidth() * 45 / 100));
    zona.removeFromLeft (marge);
    auto dreta = zona;

    // Columna esquerra.
    zonaAudio = esquerra.removeFromTop (alcadaTitol + 6 * (alcadaFila + 6) + 14);
    esquerra.removeFromTop (marge);
    zonaProves = esquerra.removeFromTop (alcadaTitol + 2 * (alcadaFila + 6) + 14);
    esquerra.removeFromTop (marge);
    zonaLatencia = esquerra;

    {
        auto z = zonaAudio.reduced (14, 0).withTrimmedTop (alcadaTitol + 4);
        auto fila = [&] (juce::Label& l, juce::ComboBox& c)
        {
            auto f = z.removeFromTop (alcadaFila);
            z.removeFromTop (6);
            l.setBounds (f.removeFromLeft (100));
            c.setBounds (f);
        };
        fila (etTipus, cbTipus);
        fila (etSortida, cbSortida);
        fila (etEntrada, cbEntrada);
        fila (etFreq, cbFreq);
        fila (etBuffer, cbBuffer);
        auto f = z.removeFromTop (alcadaFila).withTrimmedLeft (100);
        btPanell.setBounds (f.removeFromLeft (f.getWidth() / 2 - 4));
        f.removeFromLeft (8);
        btBusca.setBounds (f);
    }
    {
        auto z = zonaProves.reduced (14, 0).withTrimmedTop (alcadaTitol + 4);
        auto f1 = z.removeFromTop (alcadaFila);
        z.removeFromTop (6);
        btClic.setBounds (f1.removeFromLeft (150));
        f1.removeFromLeft (14);
        etTempo.setBounds (f1.removeFromLeft (56));
        tempo.setBounds (f1);
        auto f2 = z.removeFromTop (alcadaFila);
        btSinte.setBounds (f2.removeFromLeft (150));
    }
    {
        auto z = zonaLatencia.reduced (14, 0).withTrimmedTop (alcadaTitol + 2).withTrimmedBottom (10);
        etAjudaLatencia.setBounds (z.removeFromTop (64));
        z.removeFromTop (4);
        btLatencia.setBounds (z.removeFromTop (alcadaFila).removeFromLeft (200));
        z.removeFromTop (12);
        etResultatLatencia.setBounds (z);
    }

    // Columna dreta.
    zonaEntrades = dreta.removeFromTop (alcadaTitol + 8 * 26 + 14);
    dreta.removeFromTop (marge);
    zonaEstat = dreta.removeFromBottom (150);
    dreta.removeFromBottom (marge);
    zonaMidi = dreta;

    vumetres.setBounds (zonaEntrades.reduced (14, 0).withTrimmedTop (alcadaTitol).withTrimmedBottom (10));
    {
        auto z = zonaMidi.reduced (14, 0).withTrimmedTop (alcadaTitol).withTrimmedBottom (12);
        etTeclats.setBounds (z.removeFromTop (24));
        z.removeFromTop (4);
        llistaMidi.setBounds (z);
    }
    etEstat.setBounds (zonaEstat.reduced (14, 0).withTrimmedTop (alcadaTitol).withTrimmedBottom (8));

    {
        auto z = zonaHistorial.reduced (14, 0).withTrimmedBottom (12);
        auto titol = z.removeFromTop (alcadaTitol);
        btCopia.setBounds (titol.removeFromRight (190).reduced (0, 2));
        historialText.setBounds (z);
    }
}

//==============================================================================
void PantallaDiagnostic::refrescaControls()
{
    const juce::ScopedValueSetter<bool> svs (refrescant, true);
    auto& gestor = servei.dispositius();

    // Tipus de controlador.
    cbTipus.clear (juce::dontSendNotification);
    nomsTipus.clear();
    for (auto* t : gestor.getAvailableDeviceTypes())
    {
        nomsTipus.add (t->getTypeName());
        cbTipus.addItem (ServeiAudio::nomCatalaTipus (t->getTypeName()), nomsTipus.size());
    }
    cbTipus.setSelectedId (nomsTipus.indexOf (gestor.getCurrentAudioDeviceType()) + 1, juce::dontSendNotification);

    auto* tipus = gestor.getCurrentDeviceTypeObject();
    const auto setup = gestor.getAudioDeviceSetup();

    cbSortida.clear (juce::dontSendNotification);
    cbEntrada.clear (juce::dontSendNotification);

    if (tipus != nullptr)
    {
        const auto sortides = tipus->getDeviceNames (false);
        cbSortida.addItemList (sortides, 1);
        cbSortida.setSelectedId (sortides.indexOf (setup.outputDeviceName) + 1, juce::dontSendNotification);

        if (tipus->hasSeparateInputsAndOutputs())
        {
            const auto entrades = tipus->getDeviceNames (true);
            cbEntrada.addItem ("(cap)", 1);
            cbEntrada.addItemList (entrades, 2);
            cbEntrada.setSelectedId (setup.inputDeviceName.isEmpty() ? 1 : entrades.indexOf (setup.inputDeviceName) + 2,
                                     juce::dontSendNotification);
            cbEntrada.setEnabled (true);
        }
        else
        {
            cbEntrada.addItem ("(la mateixa targeta)", 1);
            cbEntrada.setSelectedId (1, juce::dontSendNotification);
            cbEntrada.setEnabled (false);
        }
    }

    cbFreq.clear (juce::dontSendNotification);
    cbBuffer.clear (juce::dontSendNotification);

    auto* d = gestor.getCurrentAudioDevice();
    if (d != nullptr)
    {
        for (auto f : d->getAvailableSampleRates())
            cbFreq.addItem (juce::String ((int) f) + " Hz", (int) f);
        cbFreq.setSelectedId ((int) d->getCurrentSampleRate(), juce::dontSendNotification);

        const double fs = d->getCurrentSampleRate();
        for (auto b : d->getAvailableBufferSizes())
            cbBuffer.addItem (juce::String (b) + " mostres  (" + ms (b, fs) + ")", b);
        cbBuffer.setSelectedId (d->getCurrentBufferSizeSamples(), juce::dontSendNotification);
    }

    btPanell.setEnabled (d != nullptr && d->hasControlPanel());
    cbFreq.setEnabled (d != nullptr);
    cbBuffer.setEnabled (d != nullptr);

    // Vúmetres: noms de les entrades actives.
    juce::StringArray nomsEntrades;
    if (d != nullptr)
    {
        const auto noms = d->getInputChannelNames();
        const auto actives = d->getActiveInputChannels();
        for (int i = 0; i < noms.size() && nomsEntrades.size() < mibase::MotorDiagnostic::maxEntrades; ++i)
            if (actives[i])
                nomsEntrades.add (noms[i]);
    }
    vumetres.setCanals (nomsEntrades);

    const auto desada = servei.getLatenciaDesada();
    if (ultimResultatLatencia.isEmpty())
        etResultatLatencia.setText (desada.isNotEmpty() ? "Última mesura amb aquesta configuració: "_ca + desada
                                                        : "Encara no s'ha mesurat amb aquesta configuració."_ca,
                                    juce::dontSendNotification);
}

void PantallaDiagnostic::aplica (juce::AudioDeviceManager::AudioDeviceSetup setup, const juce::String& que)
{
    setup.useDefaultInputChannels = true;
    setup.useDefaultOutputChannels = true;

    const auto error = servei.dispositius().setAudioDeviceSetup (setup, true);
    if (error.isNotEmpty())
        historial.afegeix ("No s'ha pogut canviar " + que + ": " + error);

    ultimResultatLatencia.clear();
    refrescaControls();
}

void PantallaDiagnostic::triaTipus()
{
    const auto nom = nomsTipus[cbTipus.getSelectedId() - 1];
    if (nom.isEmpty() || nom == servei.dispositius().getCurrentAudioDeviceType())
        return;

    historial.afegeix ("Canvi de controlador a " + ServeiAudio::nomCatalaTipus (nom) + ".");
    servei.dispositius().setCurrentAudioDeviceType (nom, true);
    ultimResultatLatencia.clear();
    refrescaControls();
}

void PantallaDiagnostic::triaDispositiu()
{
    auto setup = servei.dispositius().getAudioDeviceSetup();
    setup.outputDeviceName = cbSortida.getText();

    if (auto* t = servei.dispositius().getCurrentDeviceTypeObject(); t != nullptr && t->hasSeparateInputsAndOutputs())
        setup.inputDeviceName = cbEntrada.getSelectedId() <= 1 ? juce::String() : cbEntrada.getText();
    else
        setup.inputDeviceName = setup.outputDeviceName;

    aplica (setup, "la targeta");
}

void PantallaDiagnostic::triaFreqOBuffer()
{
    auto setup = servei.dispositius().getAudioDeviceSetup();
    if (cbFreq.getSelectedId() > 0)   setup.sampleRate = cbFreq.getSelectedId();
    if (cbBuffer.getSelectedId() > 0) setup.bufferSize = cbBuffer.getSelectedId();
    aplica (setup, "la freqüència o el buffer"_ca);
}

//==============================================================================
void PantallaDiagnostic::comencaLatencia()
{
    auto* d = servei.dispositius().getCurrentAudioDevice();
    if (d == nullptr || d->getActiveInputChannels().isZero())
    {
        etResultatLatencia.setText ("Cal una targeta amb alguna entrada activa per mesurar.", juce::dontSendNotification);
        return;
    }

    if (servei.motor().latencia().demana())
    {
        historial.afegeix ("Mesurant la latència..."_ca);
        btLatencia.setButtonText ("Cancel·la la mesura"_ca);
        etResultatLatencia.setText ("Mesurant...", juce::dontSendNotification);
    }
}

void PantallaDiagnostic::acabaLatencia()
{
    auto* d = servei.dispositius().getCurrentAudioDevice();
    const auto r = servei.motor().latencia().calcula();
    btLatencia.setButtonText ("Mesura la latència"_ca);

    juce::String text;
    if (r.correcte)
    {
        text << "Anada i tornada mesurada: " << juce::String (r.ms, 1) << " ms (" << r.mostres << " mostres).\n";

        if (d != nullptr)
        {
            const int informat = d->getInputLatencyInSamples() + d->getOutputLatencyInSamples();
            const double fs = d->getCurrentSampleRate();
            text << "El controlador diu: " << ms (informat, fs) << " (" << informat << " mostres). "
                 << "Diferència: "_ca << (r.mostres - informat) << " mostres.\n";
        }

        text << juce::String::fromUTF8 (r.missatge.c_str());
        servei.desaLatencia (r.mostres, r.ms);
        historial.afegeix ("Latència mesurada: "_ca + juce::String (r.ms, 1) + " ms (" + juce::String (r.mostres) + " mostres).");
    }
    else
    {
        text = juce::String::fromUTF8 (r.missatge.c_str());
        historial.afegeix ("La mesura de latència no ha funcionat."_ca);
    }

    ultimResultatLatencia = text;
    etResultatLatencia.setText (text, juce::dontSendNotification);
}

//==============================================================================
void PantallaDiagnostic::timerCallback()
{
    auto& motor = servei.motor();
    auto* d = servei.dispositius().getCurrentAudioDevice();

    // Vúmetres.
    const int numActives = d != nullptr ? juce::jmin (d->getActiveInputChannels().countNumberOfSetBits(),
                                                      mibase::MotorDiagnostic::maxEntrades) : 0;
    for (int c = 0; c < numActives; ++c)
    {
        auto& m = motor.mesurador (c);
        const float pic = m.llegeixIReinicia();
        const bool saturat = m.getSaturacions() > 0;
        m.reiniciaSaturacions();
        vumetres.setNivell (c, pic, saturat);
    }
    vumetres.repaint();

    // MIDI rebut.
    mibase::EsdevenimentMidi e;
    int maxim = 64;
    while (maxim-- > 0 && servei.treuMidiPerMostrar (e))
        llistaMidi.afegeix (descriuMidi (e));

    // Mesura de latència.
    if (motor.latencia().teDades())
        acabaLatencia();
    if (motor.latencia().estaMesurant() || btLatencia.getButtonText() != "Mesura la latència"_ca)
    {
        if (! motor.latencia().estaMesurant() && ! motor.latencia().teDades())
            btLatencia.setButtonText ("Mesura la latència"_ca);
        repaint (zonaLatencia);
    }

    // Cada mig segon: estat; cada 2 segons: teclats MIDI.
    if (++comptador % 15 == 0)
        actualitzaEstat();
    if (comptador % 60 == 0)
        servei.actualitzaMidi();

    if (historial.getVersio() != versioHistorial)
    {
        versioHistorial = historial.getVersio();
        historialText.setText (historial.getLinies().joinIntoString ("\n"), false);
        historialText.moveCaretToTop (false);
    }
}

void PantallaDiagnostic::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (! refrescant)
        refrescaControls();
}

void PantallaDiagnostic::actualitzaEstat()
{
    auto& gestor = servei.dispositius();
    auto* d = gestor.getCurrentAudioDevice();

    juce::String text;
    if (d == nullptr)
    {
        text << "Àudio aturat."_ca;
        if (servei.getUltimError().isNotEmpty())
            text << "\nÚltim error: "_ca << servei.getUltimError();
    }
    else
    {
        const double fs = d->getCurrentSampleRate();
        const int b = d->getCurrentBufferSizeSamples();
        text << ServeiAudio::nomCatalaTipus (d->getTypeName()) << "  ·  "_ca << d->getName() << "\n"
             << (int) fs << " Hz  ·  buffer "_ca << b << " mostres (" << ms (b, fs) << ")  ·  "_ca
             << d->getActiveInputChannels().countNumberOfSetBits() << " entrades, "
             << d->getActiveOutputChannels().countNumberOfSetBits() << " sortides\n"
             << "Retard segons el controlador: entrada " << ms (d->getInputLatencyInSamples(), fs)
             << ", sortida " << ms (d->getOutputLatencyInSamples(), fs) << "\n"
             << "Càrrega de l'àudio: "_ca << juce::String (gestor.getCpuUsage() * 100.0, 0) << " %";

        const int talls = d->getXRunCount();
        text << "  ·  Talls: "_ca << (talls >= 0 ? juce::String (talls) : juce::String ("el controlador no ho diu"));
    }
    etEstat.setText (text, juce::dontSendNotification);

    const auto teclats = servei.getNomsMidi();
    etTeclats.setText (teclats.isEmpty() ? "Cap teclat MIDI connectat." : "Connectats: " + teclats.joinIntoString (", "),
                       juce::dontSendNotification);
}

//==============================================================================
juce::String PantallaDiagnostic::nomNota (int nota)
{
    static const char* noms[] = { "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" };
    return juce::String (noms[nota % 12]) + juce::String (nota / 12 - 1);
}

juce::String PantallaDiagnostic::descriuMidi (const mibase::EsdevenimentMidi& e)
{
    const int canal = (e.estat & 0x0F) + 1;
    const int tipus = e.estat & 0xF0;
    const juce::String c = "   canal " + juce::String (canal);

    switch (tipus)
    {
        case 0x90:
            if (e.dada2 > 0)
                return "Nota   " + nomNota (e.dada1) + " (" + juce::String (e.dada1) + ")   força "_ca + juce::String (e.dada2) + c;
            [[fallthrough]];
        case 0x80:
            return "Deixa  " + nomNota (e.dada1) + " (" + juce::String (e.dada1) + ")" + c;
        case 0xB0:
            return "Control " + juce::String (e.dada1) + " = " + juce::String (e.dada2) + c;
        case 0xE0:
            return "Inflexió (pitch bend) "_ca + juce::String (((e.dada2 << 7) | e.dada1) - 8192) + c;
        case 0xC0:
            return "Canvi de programa " + juce::String (e.dada1 + 1) + c;
        case 0xD0:
            return "Pressió "_ca + juce::String (e.dada1) + c;
        case 0xA0:
            return "Pressió de nota "_ca + nomNota (e.dada1) + " = " + juce::String (e.dada2) + c;
        default:
            return "Missatge " + juce::String::toHexString ((int) e.estat);
    }
}

juce::String PantallaDiagnostic::textDiagnostic()
{
    using SS = juce::SystemStats;
    auto& gestor = servei.dispositius();
    auto* d = gestor.getCurrentAudioDevice();

    juce::String t;
    t << "=== Diagnòstic de Mibase "_ca MIBASE_VERSIO " ===\n"
      << "Data: " << juce::Time::getCurrentTime().formatted ("%d/%m/%Y %H:%M:%S") << "\n"
      << "Sistema: " << SS::getOperatingSystemName() << (SS::isOperatingSystem64Bit() ? " (64 bits)" : "") << "\n"
      << "Processador: " << SS::getCpuModel() << " · "_ca << SS::getNumPhysicalCpus() << " nuclis físics, "_ca
      << SS::getNumCpus() << " fils\n"
      << "Memòria: "_ca << SS::getMemorySizeInMegabytes() << " MB\n\n";

    t << "-- Àudio --\n"_ca
      << "Controladors disponibles: ";
    juce::StringArray tipus;
    for (auto* ty : gestor.getAvailableDeviceTypes())
        tipus.add (ty->getTypeName() + " [" + ty->getDeviceNames (false).joinIntoString (" | ") + "]");
    t << tipus.joinIntoString ("; ") << "\n";

    if (d == nullptr)
    {
        t << "Cap dispositiu obert. Últim error: "_ca << servei.getUltimError() << "\n";
    }
    else
    {
        const double fs = d->getCurrentSampleRate();
        t << "Controlador: " << d->getTypeName() << "\n"
          << "Dispositiu: " << d->getName() << "\n"
          ;
        if (d->getTypeName().startsWith ("Windows Audio"))
            t << "Entrada: " << gestor.getAudioDeviceSetup().inputDeviceName << "\n";
        t << "Freqüència: "_ca << fs << " Hz (possibles: ";
        juce::StringArray f;
        for (auto x : d->getAvailableSampleRates()) f.add (juce::String ((int) x));
        t << f.joinIntoString (", ") << ")\n"
          << "Buffer: " << d->getCurrentBufferSizeSamples() << " (per defecte " << d->getDefaultBufferSize() << "; possibles: ";
        const auto buffers = d->getAvailableBufferSizes();
        juce::StringArray b;
        if (buffers.size() > 12)
            b.add (juce::String (buffers.getFirst()) + " a " + juce::String (buffers.getLast())
                   + " (" + juce::String (buffers.size()) + " mides)");
        else
            for (auto x : buffers) b.add (juce::String (x));
        t << b.joinIntoString (", ") << ")\n"
          << "Bits: " << d->getCurrentBitDepth() << "\n"
          << "Entrades actives: " << nomsActius (d->getInputChannelNames(), d->getActiveInputChannels())
          << "  (de " << d->getInputChannelNames().size() << ")\n"
          << "Sortides actives: " << nomsActius (d->getOutputChannelNames(), d->getActiveOutputChannels())
          << "  (de " << d->getOutputChannelNames().size() << ")\n"
          << "Retard segons el controlador: entrada " << d->getInputLatencyInSamples() << ", sortida "
          << d->getOutputLatencyInSamples() << " mostres\n"
          << "Càrrega: "_ca << juce::String (gestor.getCpuUsage() * 100.0, 1) << " %  ·  Talls: "_ca
          << (d->getXRunCount() >= 0 ? juce::String (d->getXRunCount()) : juce::String ("el controlador no ho diu"))
          << "  ·  Blocs massa grans: "_ca << servei.motor().blocsMassaGrans.load() << "\n"
          << "Panell del controlador: " << (d->hasControlPanel() ? "sí"_ca : "no") << "\n";
    }

    t << "Latència mesurada desada: "_ca << servei.getLatenciaDesada() << "\n";
    if (ultimResultatLatencia.isNotEmpty())
        t << "Última mesura d'aquesta sessió:\n"_ca << ultimResultatLatencia << "\n";

    t << "\n-- MIDI --\n";
    for (const auto& m : juce::MidiInput::getAvailableDevices())
        t << m.name << (gestor.isMidiInputDeviceEnabled (m.identifier) ? " (activat)" : " (no activat)") << "\n";
    if (juce::MidiInput::getAvailableDevices().isEmpty())
        t << "Cap teclat MIDI.\n";

    t << "\n-- Historial (el més recent a dalt) --\n"_ca;
    const auto& linies = historial.getLinies();
    for (int i = 0; i < juce::jmin (40, linies.size()); ++i)
        t << linies[i] << "\n";

    return t;
}
