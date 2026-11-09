#include "PantallaEstudi.h"
#include "Inserts.h"

using namespace mibase;
namespace ed = mibase::edicio;

namespace
{
    constexpr int alcadaBarra = 58;
    constexpr int alcadaRegla = 34;
    constexpr int gruixBarres = 10;

    /** Fader petit horitzontal per al volum de la claqueta (0 a 150 %). */
    class FaderPetit : public juce::Component
    {
    public:
        std::function<void (double)> onCanvi;
        double valor = 1.0;

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            auto text = r.removeFromRight (48.0f);
            auto carril = r.reduced (8.0f, 0.0f).withSizeKeepingCentre (r.getWidth() - 16.0f, 6.0f);
            vintage::ranura (g, carril);
            g.setColour (tema::linia2);
            const float x100 = carril.getX() + carril.getWidth() / 1.5f;
            g.fillRect (juce::Rectangle<float> (x100 - 0.5f, r.getY() + 2.0f, 1.0f, 5.0f));
            const float x = carril.getX() + carril.getWidth() * (float) (valor / 1.5);
            vintage::capFader (g, juce::Rectangle<float> (x - 7.0f, r.getCentreY() - 11.0f, 14.0f, 22.0f), vintage::ledGroc, false);
            g.setColour (tema::textMig);
            g.setFont (dibuix::mono (12.0f));
            g.drawText (juce::String (juce::roundToInt (valor * 100.0)) + " %", text, juce::Justification::centredRight);
        }

        void mouseDown (const juce::MouseEvent& e) override { mouseDrag (e); }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            const float ample = (float) getWidth() - 48.0f - 16.0f;
            valor = juce::jlimit (0.0, 1.5, (double) (e.position.x - 8.0f) / ample * 1.5);
            if (std::abs (valor - 1.0) < 0.04) valor = 1.0;
            repaint();
            if (onCanvi) onCanvi (valor);
        }
        void mouseDoubleClick (const juce::MouseEvent&) override { valor = 1.0; repaint(); if (onCanvi) onCanvi (valor); }
    };

    /** Finestreta de la claqueta: encesa, volum, so i «escolta». */
    class PanellClic : public juce::Component
    {
    public:
        PanellClic (bool encesa, double volum, int so, std::function<void (bool, double, int)> canvi, std::function<void (bool)> escolta)
            : onCanvi (std::move (canvi)), onEscolta (std::move (escolta))
        {
            addAndMakeVisible (btEncesa);
            btEncesa.setLlum (vintage::ledGroc);
            btEncesa.setToggleState (encesa, juce::dontSendNotification);
            btEncesa.onClick = [this] { btEncesa.setToggleState (! btEncesa.getToggleState(), juce::dontSendNotification); avisa(); };

            addAndMakeVisible (volum_);
            volum_.valor = volum;
            volum_.onCanvi = [this] (double) { avisa(); };

            addAndMakeVisible (sons);
            sons.setTriat (so);
            sons.setLlum (vintage::ledGroc);
            sons.onCanvi = [this] (int) { avisa(); };

            addAndMakeVisible (btEscolta);
            btEscolta.setLlum (vintage::ledVerd);
            btEscolta.onClick = [this]
            {
                btEscolta.setToggleState (! btEscolta.getToggleState(), juce::dontSendNotification);
                if (onEscolta) onEscolta (btEscolta.getToggleState());
            };
            setSize (380, 214);
        }

        ~PanellClic() override { if (onEscolta) onEscolta (false); }

        void paint (juce::Graphics& g) override
        {
            vintage::panell (g, getLocalBounds().toFloat(), 8.0f);
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (15.0f, true));
            g.drawText (juce::String::fromUTF8 ("Claqueta"), 16, 10, 200, 26, juce::Justification::centredLeft);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (12.0f));
            g.drawText ("Volum", 16, volum_.getY() - 18, 200, 16, juce::Justification::centredLeft);
            g.drawText ("So", 16, sons.getY() - 18, 200, 16, juce::Justification::centredLeft);
        }

        void resized() override
        {
            btEncesa.setBounds (getWidth() - 16 - 100, 10, 100, 40);
            volum_.setBounds (16, 70, getWidth() - 32, 28);
            sons.setBounds (16, 124, getWidth() - 32, 40);
            btEscolta.setBounds (16, 172, 150, 34);
        }

    private:
        void avisa() { if (onCanvi) onCanvi (btEncesa.getToggleState(), volum_.valor, sons.getTriat()); }

        std::function<void (bool, double, int)> onCanvi;
        std::function<void (bool)> onEscolta;
        Boto btEncesa { juce::String::fromUTF8 ("Encesa"), std::nullopt, Boto::Estil::Tecla };
        FaderPetit volum_;
        Segmentat sons { { "Digital", "Fusta", juce::String::fromUTF8 ("Analògic"), "Campana" } };
        Boto btEscolta { "Escolta-la", dibuix::Icona::Play, Boto::Estil::Tecla };
    };
}

//==============================================================================
PantallaEstudi::PantallaEstudi (const juce::File& f, const Projecte& p, juce::PropertiesFile& a, Historial& h, ServeiAudio& s)
    : ajustos (a), historial (h), servei (s)
{
    setWantsKeyboardFocus (true);
    formats.registerBasicFormats();

    estat.projecte = p;
    estat.fitxer = f;
    estat.carpeta = f.getParentDirectory();
    estat.pxPerSegon = ajustos.getDoubleValue ("estudi.zoom", 48.0);
    estat.altPista = juce::jmax (VistaPistes::alcadaMinima, ajustos.getIntValue ("estudi.altPista", 80));
    if (! estat.projecte.pistes.empty()) estat.pistaTriada = 0;
    estat.addChangeListener (this);
    estat.onCanviat = [this] (const juce::String& evolucio)
    {
        if (evolucio.isNotEmpty())
            disc::afegeixEvolucio (dibuix::cami (estat.carpeta), dibuix::araText().toStdString(), evolucio.toStdString());
        calDesar = true;
        startTimer (700);
    };

    for (auto& presa : estat.projecte.preses)
        carregaOna (presa);
    sincronitzaVeu();   // 0.22: les preses corregides es preparen encara que no s'obri el Vocal Editor

    estat.nomsEntrades = servei.getNomsEntrades();
    estat.onCapcalMogut = [this] (double t) { if (estat.transport == 1) servei.estudi().situa (t); };
    rellotge.tic = [this] { tic(); };
    rellotge.startTimerHz (30);

    ampleEsquerra = ajustos.getIntValue ("estudi.ampleEsquerra", 132);
    ampleNoms = ajustos.getIntValue ("estudi.ampleNoms2", 190);
    amplePanell = ajustos.getIntValue ("estudi.amplePanell", 270);

    // Caixa d'eines.
    struct E { dibuix::Icona icona; const char* ajuda; };
    const E llista[] = {
        { dibuix::Icona::Fletxa2, "Fletxa (1): tria, mou, retalla, fades i volum" },
        { dibuix::Icona::Llapis,  "Llapis (2): fa una finestra nova (arrossegant, o d'un compàs)" },
        { dibuix::Icona::Goma,    "Goma (3): esborra clips" },
        { dibuix::Icona::Tisores, "Tisores (4): talla un clip on cliques" },
        { dibuix::Icona::Cola,    "Cola (5): ajunta dos clips en una sola finestra" },
        { dibuix::Icona::Silenci, "X (6): silencia o fa sonar un clip" },
    };
    for (int i = 0; i < 6; ++i)
    {
        auto* b = eines.add (new Boto ({}, llista[i].icona, Boto::Estil::Tecla));
        b->setLlum (vintage::ledTaronja);
        b->setTooltip (juce::String::fromUTF8 (llista[i].ajuda));
        b->onClick = [this, i] { triaEina ((Eina) i); };
        addAndMakeVisible (b);
    }

    for (auto* b : { &btClic, &btClicMenu, &btTempo, &btDesfes, &btRefes, &btRequadre, &btInsereix })
        addAndMakeVisible (b);
    addAndMakeVisible (btColor);
    estat.modeRang = ajustos.getBoolValue ("estudi.modeRang", false);
    estat.insereix = ajustos.getBoolValue ("estudi.insereix", false);
    btRequadre.setLlum (vintage::ledTaronja);
    btRequadre.onClick = [this] { canviaModeRang (! estat.modeRang); };
    btInsereix.setLlum (vintage::ledTaronja);
    btInsereix.onClick = [this] { canviaInsereix (! estat.insereix); };
    btColor.setTooltip ("Color dels clips triats o, si no n'hi ha, de la pista"_ca);
    btColor.onClick = [this] { triaColor(); };
    addAndMakeVisible (snap);

    btClic.setLlum (vintage::ledGroc);
    btClic.setTooltip ("Claqueta encesa o apagada"_ca);
    btClic.onClick = [this]
    {
        estat.projecte.clic = ! estat.projecte.clic;
        estat.avisa();
        calDesar = true;
        startTimer (700);
    };
    btClicMenu.setTooltip ("Volum i so de la claqueta"_ca);
    btClicMenu.onClick = [this] { menuClaqueta(); };
    btTempo.setTooltip ("Canvia el tempo"_ca);
    btTempo.onClick = [this] { canviaTempo(); };
    snap.setTriat ((int) estat.projecte.snap);
    snap.onCanvi = [this] (int i) { estat.projecte.snap = (Snap) i; estat.avisa(); calDesar = true; startTimer (700); };
    btDesfes.setTooltip ("Desfés (Ctrl+Z)"_ca);
    btDesfes.onClick = [this] { desfes (false); };
    btRefes.setTooltip ("Refés (Ctrl+Y)"_ca);
    btRefes.onClick = [this] { desfes (true); };

    // Seccions.
    for (auto* sec : std::initializer_list<SeccioEstudi*> { &esquerra, &capcaleres, &regla, &pistes, &mescles, &panell })
    {
        addAndMakeVisible (sec);
        sec->onRoda = [this] (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) { roda (e, w); };
    }
    mescles.onRoda = nullptr;
    capcaleres.onCanviaNom = [this] (int i) { canviaNom (i); };
    capcaleres.onPistaNova = [this] (const juce::String& tipus, int canals) { pistaNova (tipus, canals); };
    pistes.onPistaNova = [this] (const juce::String& tipus, int canals) { pistaNova (tipus, canals); };
    pistes.onImporta = [this] (const juce::StringArray& fitxers, int pista, double on) { importa (fitxers, pista, on); };
    pistes.onDemanaImportar = [this] (int pista, double on) { demanaImportar (pista, on); };
    pistes.onConsolida = [this] (int pista, int clipA, int clipB) { consolida (pista, clipA, clipB); };
    pistes.onObreMidi = [this] (int pista, int clip) { obrePianoRoll (pista, clip); };
    pistes.onObreVeu = [this] (int pista, int clip) { obreVeu (pista, clip); };
    inserts::obreVeu() = [this] (int pista) { obreVeuDePista (pista); };
    informePlugins() = [this] { return plugins.informe (estat.projecte); };
    addChildComponent (editorVeu);
    editorVeu.onTanca = [this] { tancaVeu(); };
    editorVeu.onCommutaAB = [this] { commutaVeuAB(); };
    editorVeu.onDesfes = [this] (bool refer) { desfes (refer); };
    editorVeu.onSolo = [this] { commutaSoloEditor(); };
    // 0.26: samplejador
    addChildComponent (editorSampler);
    editorSampler.onTanca = [this] { tancaSampler(); };
    editorSampler.onCarrega = [this] { demanaSample(); };
    editorSampler.onFitxer = [this] (const juce::File& fitxerSample) { carregaSample (estat.sampler.pista, fitxerSample); };
    editorSampler.onNota = [this] (std::uint8_t e, std::uint8_t n, std::uint8_t v)
    {
        for (auto& t : estat.projecte.pistes)
            if (t.id == estat.sampler.pista) { servei.estudi().enviaDirecte ({ slotDePista (t), e, n, v }); return; }
    };
    editorSampler.onEscolta = [this] (double seg)
    {
        if (auto* sm = instruments.samplejador (estat.sampler.pista)) { if (seg >= 0.0) sm->escolta (seg); else sm->aturaEscolta(); }
    };
    editorSampler.posicioSo = [this]() -> double
    {
        auto* sm = instruments.samplejador (estat.sampler.pista);
        const auto* pr = sm != nullptr ? sm->preparada() : nullptr;
        if (pr == nullptr) return -1.0;
        const double pos = sm->posicioActual();
        if (pos < 0.0 || pr->m.fs <= 0.0) return -1.0;
        const double seg = pos / (pr->m.fs * pr->factorTemps);
        return pr->girada ? pr->duradaOriginal - seg : seg;
    };
    editorSampler.mostra = [this]() -> std::shared_ptr<const mibase::sampler::Mostra>
    {
        for (auto& t : estat.projecte.pistes)
            if (t.id == estat.sampler.pista && mibase::esSampler (t) && ! t.sampler.fitxer.empty())
                return Instruments::mostraSample (Instruments::fitxerSample (estat.carpeta, t.sampler));
        return nullptr;
    };
    editorSampler.onTeclaNoUsada = [this] (const juce::KeyPress& k) { return keyPressed (k); };
    panell.onDuplica = [this] { duplica(); };
    panell.onTreu = [this] { treu(); };

    // Carpetes de pistes (0.11).
    addChildComponent (carpetes);
    addChildComponent (taulaEstructura);
    taulaEstructura.onTram = [this] { return servei.estudi().getTramRuta(); };
    addChildComponent (divCarpetes);
    estat.panellCarpetes = ajustos.getBoolValue ("estudi.panellCarpetes", false);
    ampleCarpetes = juce::jlimit (130, 400, ajustos.getIntValue ("estudi.ampleCarpetes", 190));
    btCarpetes.setLlum (vintage::ledGroc);
    btCarpetes.setTooltip ("Carpetes de pistes: mostra o amaga el panell per agrupar les pistes"_ca);
    btCarpetes.onClick = [this]
    {
        estat.panellCarpetes = ! estat.panellCarpetes;
        ajustos.setValue ("estudi.panellCarpetes", estat.panellCarpetes);
        colocaSeccions();
    };
    addAndMakeVisible (btCarpetes);
    carpetes.onCarpetaNova = [this] (std::vector<int> v) { demanaNomCarpeta (std::move (v), {}); };
    carpetes.onCanviaNom = [this] (std::string id) { demanaNomCarpeta ({}, id); };
    divCarpetes.setTooltip ("Arrossega per fer més ample o més estret. Doble clic: mida inicial."_ca);
    divCarpetes.onComenca = [this] { ampleInici = carpetes.getWidth(); };
    divCarpetes.onArrossega = [this] (int dx) { ampleCarpetes = juce::jlimit (130, 400, ampleInici + dx); colocaSeccions(); };
    divCarpetes.onDobleClic = [this] { ampleCarpetes = 190; colocaSeccions(); divCarpetes.onAcaba(); };
    divCarpetes.onAcaba = [this] { ajustos.setValue ("estudi.ampleCarpetes", ampleCarpetes); ajustos.saveIfNeeded(); };

    for (auto* d : { &divEsquerra, &divNoms, &divPanell })
    {
        addAndMakeVisible (d);
        d->setTooltip ("Arrossega per fer més ample o més estret. Doble clic: mida inicial."_ca);
        d->onAcaba = [this]
        {
            ajustos.setValue ("estudi.ampleEsquerra", ampleEsquerra);
            ajustos.setValue ("estudi.ampleEsquerraEditor", ampleEsquerraEditor);
            ajustos.setValue ("estudi.ampleNoms2", ampleNoms);
            ajustos.setValue ("estudi.amplePanell", amplePanell);
            ajustos.saveIfNeeded();
        };
    }
    divEsquerra.onComenca = [this] { ampleInici = esquerra.getWidth(); };
    divEsquerra.onArrossega = [this] (int dx)
    {
        if (esquerra.ambEditor()) ampleEsquerraEditor = juce::jlimit (230, 420, ampleInici + dx);
        else ampleEsquerra = juce::jlimit (110, 260, ampleInici + dx);
        colocaSeccions();
    };
    divEsquerra.onDobleClic = [this] { if (esquerra.ambEditor()) ampleEsquerraEditor = 270; else ampleEsquerra = 132; colocaSeccions(); divEsquerra.onAcaba(); };
    divNoms.onComenca = [this] { ampleInici = capcaleres.getWidth(); };
    divNoms.onArrossega = [this] (int dx) { ampleNoms = juce::jmax (120, ampleInici + dx); colocaSeccions(); };
    divNoms.onDobleClic = [this] { ampleNoms = 190; colocaSeccions(); divNoms.onAcaba(); };
    divPanell.onComenca = [this] { ampleInici = panell.getWidth(); };
    divPanell.onArrossega = [this] (int dx) { amplePanell = juce::jmax (200, ampleInici - dx); colocaSeccions(); };
    divPanell.onDobleClic = [this] { amplePanell = 270; colocaSeccions(); divPanell.onAcaba(); };

    // Piano Roll: a baix (amb un separador per fer-lo més alt) o en una finestra a part.
    addChildComponent (zonaEditors);
    esquerra.setPanellEditor (&panellEditor);
    esquerra.opcions.onObreKits = [this] { obreKits(); };
    esquerra.opcions.onObreSampler = [this] { obreSampler (estat.pistaTriada); };
    panellHarmonia.onObreSampler = [this]
    {
        if (auto* pr = zonaEditors.actiu()) obreSampler (pr->pistaDeLEditor());
    };
    panellBateria.onObreKits = [this] { obreKits(); };
    capcaleres.onObreInstrument = [this] (int fila)
    {
        auto* t = estat.pista (fila);
        if (t == nullptr) return;
        if (t->tipus == "bateria") obreKits();
        else if (mibase::esSampler (*t)) obreSampler (fila);
        else esquerra.opcions.obreInstrument();   // la pista ja és la triada
    };
    esquerra.opcions.teclats = [this] { return servei.getTeclatsMidi(); };
    esquerra.opcions.setPlugins (&plugins);
    mescles.plugins = &plugins;
    esquerra.opcions.sortides = [this]
    {
        juce::StringArray noms;
        if (auto* d = servei.dispositius().getCurrentAudioDevice())
        {
            const auto tots = d->getOutputChannelNames();
            const auto actives = d->getActiveOutputChannels();
            for (int i = 0; i < tots.size(); ++i)   // el motor rep només les actives, en ordre
                if (actives[i]) noms.add (tots[i]);
        }
        return noms;
    };
    focusEditor.f = [this] { if (! estat.focusEditor) { estat.focusEditor = true; estat.avisa(); } };
    focusPistes.f = [this] { if (estat.focusEditor && estat.pianoRoll) { estat.focusEditor = false; estat.avisa(); } };
    zonaEditors.addMouseListener (&focusEditor, true);
    pistes.addMouseListener (&focusPistes, true);
    capcaleres.addMouseListener (&focusPistes, true);
    ampleEsquerraEditor = ajustos.getIntValue ("estudi.ampleEsquerraEditor", 270);
    addChildComponent (divPianoRoll);
    altPianoRoll = ajustos.getIntValue ("estudi.altPianoRoll", 430);
    divPianoRoll.setTooltip ("Arrossega per fer el Piano Roll més alt o més baix. Doble clic: mida inicial."_ca);
    divPianoRoll.onComenca = [this] { altInici = estat.veu.obert ? editorVeu.getHeight() : estat.sampler.obert ? editorSampler.getHeight() : zonaEditors.getHeight(); };
    divPianoRoll.onArrossega = [this] (int dy)
    {
        altPianoRoll = juce::jmax (160, altInici - dy - (estat.veu.obert || estat.sampler.obert ? 0 : (juce::jmax (1, zonaEditors.quants()) - 1) * 250));
        colocaSeccions();
    };
    divPianoRoll.onDobleClic = [this] { altPianoRoll = 430; colocaSeccions(); divPianoRoll.onAcaba(); };
    divPianoRoll.onAcaba = [this] { ajustos.setValue ("estudi.altPianoRoll", altPianoRoll); ajustos.saveIfNeeded(); };

    for (auto* b : { &barraV, &barraH })
    {
        addAndMakeVisible (b);
        b->addListener (this);
        b->setAutoHide (false);
        b->setColour (juce::ScrollBar::thumbColourId, tema::linia2);
    }

    ultimaCopia = 0;   // el primer desament de la sessió fa còpia; després, com a molt una cada 10 minuts
    triaEina (Eina::Fletxa);
    changeListenerCallback (nullptr);
}

PantallaEstudi::~PantallaEstudi()
{
    inserts::obreVeu() = nullptr;
    informePlugins() = nullptr;
    rellotge.stopTimer();
    zonaEditors.removeMouseListener (&focusEditor);
    pistes.removeMouseListener (&focusPistes);
    capcaleres.removeMouseListener (&focusPistes);
    if (finestraPianoRoll != nullptr)
    {
        ajustos.setValue ("pianoRoll.finestra", finestraPianoRoll->getWindowStateAsString());
        finestraPianoRoll->clearContentComponent();
        finestraPianoRoll.reset();
    }
    auto& motor = servei.estudi();
    motor.setSlotTeclat (-1);
    motor.setSlotGravaMidi (-1);
    if (estat.transport != 0 || motor.getEstat() != MotorEstudi::Estat::Aturat)
    {
        motor.atura();
        for (int i = 0; i < 50 && motor.getEstat() != MotorEstudi::Estat::Aturat; ++i)
            juce::Thread::sleep (10);
    }
    if (motor.gravadora().activa())
        acabaGravacio();
    if (gravacio.midi && gravacio.pista >= 0)
        acabaGravacioMidi();
    motor.publica (std::make_unique<Pla>());   // ja no ha de sonar res d'aquest projecte
    previsualitzaClic (false);
    estat.removeChangeListener (this);
    if (plugins.desaEstats (estat.projecte)) calDesar = true;   // 0.19: ajustos dels plugins
    if (calDesar) desaAra();
    ajustos.setValue ("estudi.zoom", estat.pxPerSegon);
    ajustos.setValue ("estudi.altPista", estat.altPista);
    ajustos.saveIfNeeded();
}

void PantallaEstudi::desaAra()
{
    stopTimer();
    const auto ara = juce::Time::currentTimeMillis();
    const bool ferCopia = ara - ultimaCopia > 10 * 60 * 1000;
    plugins.desaEstats (estat.projecte);
    const auto r = disc::desa (dibuix::cami (estat.fitxer), estat.projecte, dibuix::araIso().toStdString(),
                               dibuix::araPerNom().toStdString(), ferCopia);
    if (r.correcte)
    {
        calDesar = false;
        if (ferCopia) ultimaCopia = ara;
        if (auto llegit = disc::llegeix (dibuix::cami (estat.fitxer)))
            estat.projecte.jsonOriginal = llegit->jsonOriginal;
    }
    else
    {
        estat.info = "No s'ha pogut desar: "_ca + dibuix::utf8 (r.error);
        historial.afegeix ("No s'ha pogut desar el projecte: "_ca + dibuix::utf8 (r.error));
        estat.avisa();
    }
}

void PantallaEstudi::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // Els editors només canvien de clip amb doble clic (obrePianoRoll), no en triar un clip (0.10).
    // El teclat MIDI de fora toca la pista triada (si és MIDI) o la que grava.
    {
        int slot = -1;
        if (gravacio.midi && gravacio.pista >= 0) { if (auto* t = estat.pista (gravacio.pista)) slot = slotDePista (*t); }
        else if (auto* t = estat.pista (estat.pistaTriada); t != nullptr && mibase::esMidi (*t)) slot = slotDePista (*t);
        servei.estudi().setSlotTeclat (slot);
        // Teclats assignats a pistes concretes; i el de la pista triada (si en té un d'assignat, només aquell la toca).
        std::vector<std::pair<juce::String, int>> mapa;
        for (auto& t : estat.projecte.pistes)
            if (mibase::esMidi (t) && ! t.entradaMidi.empty()) mapa.push_back ({ juce::String (t.entradaMidi), slotDePista (t) });
        juce::String teclatTriada;
        if (auto* t = estat.pista (estat.pistaTriada); t != nullptr && mibase::esMidi (*t)) teclatTriada = juce::String (t->entradaMidi);
        servei.setMapaTeclats (mapa, teclatTriada);

        // Chord Pad (0.10): el de l'editor actiu (o el primer que el tingui obert) fa que el teclat de la seva pista toqui acords.
        int slotPads = -1;
        std::array<std::vector<int>, 12> acords;
        for (int k = -1; k < zonaEditors.quants() && slotPads < 0; ++k)
        {
            auto* pr = zonaEditors.editor (k < 0 ? estat.editorActiu : k);
            if (pr == nullptr || ! pr->chordPadActiu()) continue;
            if (auto* t = estat.pista (pr->pistaDeLEditor())) { slotPads = slotDePista (*t); acords = pr->acordsTeclat(); }
        }
        servei.setChordPadTeclat (slotPads, acords);

        // 0.20: bateria amb els pads per ordre de files (la triada, o la que grava).
        {
            int slotBat = -1;
            std::vector<int> notesFiles;
            const auto* tb = gravacio.midi && gravacio.pista >= 0 ? estat.pista (gravacio.pista) : estat.pista (estat.pistaTriada);
            if (tb != nullptr && tb->tipus == "bateria" && tb->padsPerOrdre)
            {
                slotBat = slotDePista (*tb);
                for (auto& f : tb->files) notesFiles.push_back (f.nota);
            }
            servei.setMapaBateria (slotBat, notesFiles);
        }

        // 0.17: el teclat respecta l'escala (assistit o bloquejat).
        {
            const auto t = harmonia::tonalitatEfectiva (estat.projecte);
            std::array<bool, 12> esc {};
            for (int n = 0; n < 12; ++n) esc[(size_t) n] = harmonia::aEscala (n, t);
            // 0.20: una pista que toca lliure (o la bateria) no fa cas de l'escala.
            const auto* tt = estat.pista (estat.pistaTriada);
            const bool lliure = tt != nullptr && (! tt->escalaTeclat || tt->tipus == "bateria");
            servei.setEscalaTeclat (lliure ? 0 : (int) (tt != nullptr ? mibase::modeEscalaDe (estat.projecte, *tt) : estat.projecte.modeEscala), esc);
        }
    }
    {
        const bool volVisible = estat.pianoRoll && finestraPianoRoll == nullptr && ! estat.foraTreball();
        if (volVisible != zonaEditors.isVisible() && finestraPianoRoll == nullptr) colocaSeccions();
        else if (esquerra.ambEditor() != (esquerra.getWidth() == ampleEsquerraEditor)) colocaSeccions();
    }

    const auto& p = estat.projecte;
    btTempo.setButtonText (juce::String ((int) std::round (p.tempo)) + " bpm · "_ca + juce::String (p.compasNum) + "/" + juce::String (p.compasDen));
    btClic.setToggleState (p.clic, juce::dontSendNotification);
    btRequadre.setToggleState (estat.modeRang, juce::dontSendNotification);
    btRequadre.setTooltip (estat.modeRang ? "Requadre: tria només el que queda dins (encès). Clica per triar clips sencers."_ca
                                          : "Requadre: tria els clips sencers que toca (apagat). Clica per triar només el que queda dins."_ca);
    btInsereix.setToggleState (estat.insereix, juce::dontSendNotification);
    btInsereix.setTooltip (estat.insereix ? "Enganxar inserint: aparta el que hi ha després del capçal (encès). Tecla Insert."_ca
                                          : "Enganxar trepitjant el que hi ha (apagat). Clica o Insert per inserir."_ca);
    if (! estat.triats.empty())
    {
        if (auto* c = estat.clip (estat.triats[0])) btColor.setColor (estat.colorClip (estat.triats[0].pista, *c));
    }
    else if (auto* t = estat.pista (estat.pistaTriada))
        btColor.setColor (EstatEstudi::colorPista (*t));
    btDesfes.setEnabled (estat.desfes.potDesfer());
    btRefes.setEnabled (estat.desfes.potRefer());
    for (int i = 0; i < eines.size(); ++i)
        eines[i]->setToggleState ((int) estat.eina == i, juce::dontSendNotification);
    if (snap.getTriat() != (int) p.snap) snap.setTriat ((int) p.snap);

    if (mescles.isVisible() != estat.vistaMescles || taulaEstructura.isVisible() != estat.vistaEstructura)
        colocaSeccions();
    actualitzaBarres();
    repaint (zonaBarra);
    sincronitzaVeu();
    seguixSoloEditor();
    publicaPla();
}

void PantallaEstudi::triaEina (Eina e)
{
    estat.eina = e;
    if (e != Eina::Fletxa && (estat.triats.size() > 1 || estat.rang.actiu)) estat.capTriat();
    estat.avisa();
}

//==============================================================================
void PantallaEstudi::paint (juce::Graphics& g)
{
    g.fillAll (tema::fons);
    vintage::panell (g, zonaBarra.toFloat());

    // Calaix de la caixa d'eines.
    auto calaix = zonaEines.toFloat().expanded (5.0f, 4.0f);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (calaix, 7.0f);

    if (! zonaInfo.isEmpty())
    {
        g.setColour (estat.info.isNotEmpty() ? tema::accent : tema::suau);
        g.setFont (dibuix::lletra (13.0f));
        const auto text = estat.info.isNotEmpty() ? estat.info
                        : juce::String (estat.numPistes()) + (estat.numPistes() == 1 ? " pista" : " pistes")
                          + " · Espai: play/atura · R: grava · Ctrl+roda: zoom · Alt+roda: alçada"_ca;
        g.drawFittedText (text, zonaInfo, juce::Justification::centredLeft, 1, 0.8f);
    }

    if (! estat.foraTreball())
    {
        auto titol = [&] (juce::Rectangle<int> r, const juce::String& t)
        {
            vintage::panell (g, r.toFloat());
            g.setColour (tema::linia);
            g.fillRect (r.getRight() - 1, r.getY(), 1, r.getHeight());
            dibuix::pintaEtiquetaSeccio (g, t, r.withTrimmedLeft (14));
        };
        titol (zonaTitolNoms, "Pistes");
    }
}

void PantallaEstudi::colocaSeccions()
{
    auto r = getLocalBounds().withTrimmedTop (alcadaBarra);
    const bool m = estat.foraTreball();

    const bool ambEditor = estat.pianoRoll && ! m;   // amb un editor obert, la columna esquerra té el panell d'harmonia
    int esq = ambEditor ? ampleEsquerraEditor : ampleEsquerra, noms = ampleNoms, pan = amplePanell;
    const bool ambCarpetes = estat.panellCarpetes && ! m;
    int carp = ambCarpetes ? ampleCarpetes : 0;
    int sobra = esq + (m ? 0 : noms + carp) + pan + 320 - r.getWidth();
    auto encongeix = [&sobra] (int& v, int minim) { if (sobra > 0) { const int t = juce::jmin (sobra, v - minim); if (t > 0) { v -= t; sobra -= t; } } };
    encongeix (pan, 200);
    if (! m) encongeix (noms, 140);
    encongeix (esq, ambEditor ? 230 : 110);
    if (ambCarpetes) encongeix (carp, 120);

    esquerra.setBounds (r.removeFromLeft (esq));
    panell.setBounds (r.removeFromRight (pan));
    // 0.26: el samplejador, a baix (sense tocar el panell de la dreta, on anirà la biblioteca)
    const bool samplerBaix = estat.sampler.obert && ! m && ! estat.veu.obert;
    editorSampler.setVisible (samplerBaix);
    if (samplerBaix)
    {
        const int alt = juce::jlimit (330, juce::jmax (330, r.getHeight() - 140), juce::jmax (altPianoRoll, 470));
        auto zona = r.removeFromBottom (alt);
        editorSampler.setBounds (zona.withTrimmedTop (4));
        divPianoRoll.setBounds (zona.getX(), zona.getY() - 3, zona.getWidth(), 8);
    }

    // Piano Roll a baix (si és obert i no és en una finestra a part).
    const bool veuBaix = estat.veu.obert && ! m;   // 0.22: o el Vocal Editor, al mateix lloc
    const bool prBaix = estat.pianoRoll && finestraPianoRoll == nullptr && ! m && ! veuBaix && ! samplerBaix;
    divPianoRoll.setVisible (prBaix || veuBaix || samplerBaix);
    if (finestraPianoRoll == nullptr) zonaEditors.setVisible (prBaix);
    editorVeu.setVisible (veuBaix);
    if (veuBaix)
    {
        const int alt = juce::jlimit (300, juce::jmax (300, r.getHeight() - 140), juce::jmax (altPianoRoll, 380));
        auto zona = r.removeFromBottom (alt);
        editorVeu.setBounds (zona.withTrimmedTop (4));
        divPianoRoll.setBounds (zona.getX(), zona.getY() - 3, zona.getWidth(), 8);
    }
    if (prBaix)
    {
        // Amb diversos editors, la zona creix (cada un ha de tenir lloc per treballar); les pistes en conserven una mica.
        const int n = juce::jmax (1, zonaEditors.quants());
        const int alt = juce::jlimit (160, juce::jmax (160, r.getHeight() - 140), altPianoRoll + (n - 1) * 250);
        auto zona = r.removeFromBottom (alt);
        zonaEditors.setBounds (zona.withTrimmedTop (4));
        divPianoRoll.setBounds (zona.getX(), zona.getY() - 3, zona.getWidth(), 8);
    }
    const int baixSeccions = r.getBottom();

    mescles.setVisible (estat.vistaMescles);
    taulaEstructura.setVisible (estat.vistaEstructura);
    panell.setVisible (! estat.vistaEstructura);
    for (auto* c : std::initializer_list<juce::Component*> { &capcaleres, &regla, &pistes, &barraH, &barraV, &divNoms, &btCarpetes })
        c->setVisible (! m);
    carpetes.setVisible (ambCarpetes);
    divCarpetes.setVisible (ambCarpetes);
    btCarpetes.setToggleState (estat.panellCarpetes, juce::dontSendNotification);

    if (m)
    {
        mescles.setBounds (r);
        // La taula d'estructura també ocupa el lloc del panell d'info (hi van les parts per arrossegar).
        taulaEstructura.ampleDreta = panell.getWidth();
        taulaEstructura.setBounds (r.withRight (panell.getRight()));
        zonaTitolNoms = {};
    }
    else
    {
        if (ambCarpetes)
        {
            carpetes.setBounds (r.removeFromLeft (carp).withTrimmedBottom (gruixBarres));
            divCarpetes.setBounds (carpetes.getRight() - 4, alcadaBarra, 8, baixSeccions - alcadaBarra);
        }
        auto colNoms = r.removeFromLeft (noms);
        auto temps = r;
        zonaTitolNoms = colNoms.removeFromTop (alcadaRegla);
        btCarpetes.setBounds (zonaTitolNoms.getRight() - 34, zonaTitolNoms.getY() + 1, 30, zonaTitolNoms.getHeight() - 2);
        colNoms.removeFromBottom (gruixBarres);
        capcaleres.setBounds (colNoms);
        regla.setBounds (temps.removeFromTop (alcadaRegla));
        barraH.setBounds (temps.removeFromBottom (gruixBarres).withTrimmedRight (gruixBarres));
        barraV.setBounds (temps.removeFromRight (gruixBarres));
        pistes.setBounds (temps);
        divNoms.setBounds (capcaleres.getRight() - 4, alcadaBarra, 8, baixSeccions - alcadaBarra);
    }

    divEsquerra.setBounds (esquerra.getRight() - 4, alcadaBarra, 8, getHeight() - alcadaBarra);
    divPanell.setBounds (panell.getX() - 4, alcadaBarra, 8, getHeight() - alcadaBarra);
    for (auto* d : { &divEsquerra, &divNoms, &divPanell, &divPianoRoll, &divCarpetes }) d->toFront (false);

    actualitzaBarres();
    repaint();
}

void PantallaEstudi::resized()
{
    zonaBarra = getLocalBounds().removeFromTop (alcadaBarra);
    auto b = zonaBarra.reduced (14, 8);

    zonaEines = b.removeFromLeft (6 * 40 + 5 * 3);
    {
        auto e = zonaEines;
        for (auto* t : eines)
        {
            t->setBounds (e.removeFromLeft (40));
            e.removeFromLeft (3);
        }
    }
    b.removeFromLeft (20);
    btClic.setBounds (b.removeFromLeft (44));
    b.removeFromLeft (2);
    btClicMenu.setBounds (b.removeFromLeft (26));
    b.removeFromLeft (6);
    btTempo.setBounds (b.removeFromLeft (btTempo.ampladaIdeal() + 10));
    b.removeFromLeft (16);
    snap.setBounds (b.removeFromLeft (snap.ampladaIdeal() + 10));
    b.removeFromLeft (4);
    btRequadre.setBounds (b.removeFromLeft (40));
    b.removeFromLeft (3);
    btInsereix.setBounds (b.removeFromLeft (40));
    b.removeFromLeft (16);
    btColor.setBounds (b.removeFromLeft (44));
    b.removeFromLeft (16);
    btDesfes.setBounds (b.removeFromLeft (40));
    b.removeFromLeft (3);
    btRefes.setBounds (b.removeFromLeft (40));
    b.removeFromLeft (16);
    zonaInfo = b;

    colocaSeccions();
}

//==============================================================================
void PantallaEstudi::actualitzaBarres()
{
    if (pistes.getWidth() <= 0) return;
    const double visibles = pistes.getWidth() / estat.pxPerSegon;
    const double total = juce::jmax (ed::final (estat.projecte) + 30.0, estat.iniciVista + visibles, 120.0);
    barraH.setRangeLimits (0.0, total, juce::dontSendNotification);
    barraH.setCurrentRange (estat.iniciVista, visibles, juce::dontSendNotification);

    const int contingut = estat.altTotal() + estat.altPista + 60;
    const int maxim = juce::jmax (0, contingut - pistes.getHeight());
    if (estat.desplY > maxim) estat.desplY = maxim;
    barraV.setRangeLimits (0.0, juce::jmax (contingut, pistes.getHeight()), juce::dontSendNotification);
    barraV.setCurrentRange (estat.desplY, pistes.getHeight(), juce::dontSendNotification);
}

void PantallaEstudi::scrollBarMoved (juce::ScrollBar* barra, double inici)
{
    if (barra == &barraH) estat.iniciVista = juce::jmax (0.0, inici);
    else estat.desplY = juce::jmax (0, (int) inici);
    estat.avisa();
}

void PantallaEstudi::roda (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float d = std::abs (w.deltaY) >= std::abs (w.deltaX) ? w.deltaY : w.deltaX;
    if (std::abs (d) < 1.0e-6f) return;

    if (e.mods.isCommandDown())
    {
        // Zoom del temps a partir del punt del ratolí, que després queda centrat a la pantalla.
        // Si el ratolí no s'ha mogut des de l'últim zoom, es continua fent zoom sobre el mateix punt.
        const auto ara = juce::Time::currentTimeMillis();
        if (e.getScreenPosition() != posZoom || ara - tempsZoom > 2000)
            anclaZoom = estat.t ((float) e.getEventRelativeTo (&pistes).x);
        posZoom = e.getScreenPosition();
        tempsZoom = ara;
        estat.pxPerSegon = juce::jlimit (2.0, 800000.0, estat.pxPerSegon * (d > 0.0f ? 1.35 : 1.0 / 1.35));   // cada pas de roda, el mateix
        const double meitat = (pistes.getWidth() / 2.0) / estat.pxPerSegon;
        estat.iniciVista = juce::jmax (0.0, anclaZoom - meitat);   // mai abans del començament
    }
    else if (e.mods.isAltDown())
    {
        estat.altPista = juce::jlimit (VistaPistes::alcadaMinima, 260, estat.altPista + juce::roundToInt (d * 60.0f));
    }
    else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
    {
        estat.iniciVista = juce::jmax (0.0, estat.iniciVista - d * 300.0 / estat.pxPerSegon);
    }
    else
    {
        estat.desplY = juce::jmax (0, estat.desplY - juce::roundToInt (d * 200.0f));
    }
    actualitzaBarres();
    estat.avisa();
}

//==============================================================================
void PantallaEstudi::menuClaqueta()
{
    auto panellClic = std::make_unique<PanellClic> (estat.projecte.clic, estat.projecte.clicVolum, estat.projecte.clicSo,
        [this] (bool encesa, double volum, int so)
        {
            estat.projecte.clic = encesa;
            estat.projecte.clicVolum = volum;
            estat.projecte.clicSo = so;
            auto& m = servei.motor();
            m.volumClic = (float) volum;
            m.soClic = so;
            estat.avisa();
            calDesar = true;
            startTimer (700);
        },
        [this] (bool escolta) { previsualitzaClic (escolta); });
    juce::CallOutBox::launchAsynchronously (std::move (panellClic), btClicMenu.getScreenBounds(), nullptr);
}

void PantallaEstudi::previsualitzaClic (bool encesa)
{
    auto& m = servei.motor();
    m.tempoClic = estat.projecte.tempo;
    m.soClic = estat.projecte.clicSo;
    m.volumClic = (float) estat.projecte.clicVolum;
    m.tempsPerCompasClic = estat.projecte.compasNum;
    m.clicActiu = encesa;
}

//==============================================================================
void PantallaEstudi::transport (int accio)
{
    if (accio == 3) { atura(); return; }
    if (accio == 4) { if (estat.transport == 0) reprodueix(); return; }
    if (accio == 5) { if (estat.transport == 0) grava(); return; }
    if (estat.transport >= 2) return;   // mentre es grava, el capçal no es toca

    const double compas = ed::segonsPerCompas (estat.projecte);
    double t = estat.capcal;
    switch (accio)
    {
        case 0: t = 0.0; estat.iniciVista = 0.0; break;
        case 1: t = juce::jmax (0.0, std::ceil (t / compas - 1e-6) * compas - compas); break;
        case 2: t = std::floor (t / compas + 1e-6) * compas + compas; break;
        default: break;
    }
    estat.mouCapcal (t, true);
    // Que el capçal es vegi.
    const double visibles = pistes.getWidth() / estat.pxPerSegon;
    if (estat.capcal < estat.iniciVista || estat.capcal > estat.iniciVista + visibles * 0.9)
        estat.iniciVista = juce::jmax (0.0, estat.capcal - visibles * 0.25);
    estat.avisa();
}

//==============================================================================
void PantallaEstudi::publicaPla()
{
    instruments.neteja (estat.projecte);
    instruments.carpeta = estat.carpeta;
    plugins.neteja (estat.projecte);
    RellotgePlugins::global().tempo.store (estat.projecte.tempo > 1.0 ? estat.projecte.tempo : 120.0);
    RellotgePlugins::global().num.store (estat.projecte.compasNum);
    RellotgePlugins::global().den.store (estat.projecte.compasDen);
    const double fs = servei.estudi().getFs();
    auto interns = instruments.proveidor (fs, 2048);
    const auto errorAbans = plugins.getUltimError();
    servei.estudi().publica (construeixPla (estat.projecte, audioPreses,
        [this, fs, interns] (const mibase::Pista& t) { return GestorPlugins::esVst3 (t.instrument) ? plugins.instrument (t, fs, 2048) : interns (t); },
        [this, fs] (const mibase::Insert& x) { return plugins.efecte (x, fs, 2048); }, &serveiVeu.corregides()));
    if (instruments.avis.isNotEmpty())
    {
        historial.afegeix (instruments.avis);
        estat.info = instruments.avis;
        instruments.avis.clear();
    }
    if (plugins.getUltimError() != errorAbans && plugins.getUltimError().isNotEmpty())
    {
        historial.afegeix (plugins.getUltimError());
        estat.info = plugins.getUltimError();
    }
}

void PantallaEstudi::avisDeTargeta (const juce::String& titol, const juce::String& text)
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, titol, text);
}

void PantallaEstudi::reprodueix()
{
    if (! servei.enMarxa())
    {
        avisDeTargeta ("No pot sonar", "La targeta d'àudio està aturada. Obre Fitxer → Preferències i tria-la."_ca);
        return;
    }
    publicaPla();
    iniciPlay = estat.capcal;
    servei.estudi().reprodueix (estat.capcal);
    estat.transport = 1;
    estatEsperat = 1;
    esperantMotor = true;
    tempsOrdre = juce::Time::currentTimeMillis();
    estat.avisa();
}

void PantallaEstudi::atura()
{
    if (estat.transport == 0)
    {
        // Ja aturat: a l'inici.
        transport (0);
        return;
    }
    const int abans = estat.transport;
    if (gravacio.midi) gravacio.fins = juce::jmax (gravacio.cursor, servei.estudi().getPosicio());   // fins on s'ha gravat
    servei.estudi().atura();
    estat.transport = 0;
    estatEsperat = 0;
    esperantMotor = true;
    tempsOrdre = juce::Time::currentTimeMillis();
    // Torna on havies començat (per tornar-ho a escoltar o a gravar).
    estat.capcal = abans >= 2 ? gravacio.cursor : iniciPlay;
    estat.avisa();
}

void PantallaEstudi::grava()
{
    int pista = -1;
    for (int i = 0; i < estat.numPistes(); ++i)
        if (estat.projecte.pistes[(size_t) i].grava) pista = i;
    if (pista < 0)
    {
        estat.info = "Per gravar, encén el botó vermell d'una pista"_ca;
        estat.avisa();
        return;
    }
    if (! servei.enMarxa())
    {
        avisDeTargeta ("No es pot gravar", "La targeta d'àudio està aturada. Obre Fitxer → Preferències i tria-la."_ca);
        return;
    }

    const auto& t = estat.projecte.pistes[(size_t) pista];
    auto& motor = servei.estudi();

    if (mibase::esMidi (t))
    {
        // Pista MIDI: es grava el que es toca al teclat (no la targeta).
        publicaPla();
        gravacio.pista = pista;
        gravacio.midi = true;
        gravacio.missatges.clear();
        gravacio.cursor = estat.capcal;
        gravacio.desDe = estat.capcal - ed::segonsPerCompas (estat.projecte);
        gravacio.retard = 0.0;
        gravacio.fitxer = juce::File();
        MotorEstudi::MidiGravat buit;
        while (motor.treuMidiGravat (buit)) {}
        const int slot = slotDePista (t);
        motor.setSlotTeclat (slot);
        motor.setSlotGravaMidi (slot);
        motor.grava (gravacio.desDe, gravacio.cursor, 0, 1);
        estat.cursorGravacio = gravacio.cursor;
        estat.transport = 2;
        estatEsperat = 2;
        esperantMotor = true;
        tempsOrdre = juce::Time::currentTimeMillis();
        estat.info = "Compte d'entrada… (toca el teclat MIDI)"_ca;
        estat.avisa();
        return;
    }

    estat.nomsEntrades = servei.getNomsEntrades();
    const auto& noms = estat.nomsEntrades;
    const int darrera = t.entrada + (t.canals == 2 ? 1 : 0);
    if (darrera >= noms.size() || noms[t.entrada].isEmpty() || noms[darrera].isEmpty())
    {
        avisDeTargeta ("No es pot gravar",
                       "La pista «"_ca + dibuix::utf8 (t.nom) + "» grava de l'entrada "_ca + EstatEstudi::textEntrada (t).substring (1)
                       + ", que no està activa.\n\nTria'n una altra (botó dret al nom de la pista → Entrada per gravar) o activa-la a Fitxer → Preferències."_ca);
        return;
    }

    auto carpetaAudios = audios();
    carpetaAudios.createDirectory();
    const auto nomFitxer = dibuix::utf8 (disc::nomPresa (dibuix::cami (carpetaAudios), t.nom));
    gravacio.fitxer = carpetaAudios.getChildFile (nomFitxer);
    if (! motor.gravadora().comenca (dibuix::cami (gravacio.fitxer), t.canals, motor.getFs()))
    {
        avisDeTargeta ("No es pot gravar", "No s'ha pogut crear el fitxer:\n"_ca + gravacio.fitxer.getFullPathName());
        return;
    }

    publicaPla();
    gravacio.pista = pista;
    gravacio.cursor = estat.capcal;
    gravacio.desDe = estat.capcal - ed::segonsPerCompas (estat.projecte);   // un compàs de compte
    gravacio.retard = servei.retardGravacio (gravacio.retardMesurat);
    motor.grava (gravacio.desDe, gravacio.cursor, t.entrada, t.canals);
    estat.cursorGravacio = gravacio.cursor;
    estat.transport = 2;
    estatEsperat = 2;
    esperantMotor = true;
    tempsOrdre = juce::Time::currentTimeMillis();
    estat.info = "Compte d'entrada…"_ca;
    estat.avisa();
}

void PantallaEstudi::acabaGravacio()
{
    auto& motor = servei.estudi();
    const auto r = motor.gravadora().acaba();
    const double fs = motor.getFs();
    const auto fitxer = gravacio.fitxer;
    const int pista = gravacio.pista;
    gravacio.pista = -1;
    if (estat.pista (pista) == nullptr)
    {
        fitxer.deleteFile();
        return;
    }
    if (! r.correcte)
    {
        fitxer.deleteFile();
        avisDeTargeta ("La gravació ha fallat", "No s'ha pogut escriure l'àudio al disc."_ca);
        return;
    }

    const auto copia = estat.projecte;
    const auto nomPista = dibuix::utf8 (estat.projecte.pistes[(size_t) pista].nom);
    const auto rel = (juce::String::fromUTF8 (disc::carpetaAudios) + "/" + fitxer.getFileName()).toStdString();
    const int clip = ed::afegeixGravacio (estat.projecte, pista, rel, fitxer.getFileNameWithoutExtension().toStdString(),
                                          (double) r.frames / fs, gravacio.desDe, gravacio.cursor, gravacio.retard);
    if (clip < 0)
    {
        fitxer.deleteFile();
        estat.info = "No s'ha gravat res (s'ha aturat durant el compte)"_ca;
        estat.avisa();
        return;
    }

    estat.desfes.abans (copia);
    carregaOna (estat.projecte.preses.back());
    estat.triaClip (pista, clip);
    const auto retard = juce::String (gravacio.retard * 1000.0, 1).replaceCharacter ('.', ',') + " ms";
    estat.info = "Gravat: "_ca + fitxer.getFileName() + " (retard corregit: " + retard + (gravacio.retardMesurat ? ", mesurat)" : ", el que diu la targeta)");
    historial.afegeix ("Gravada la presa «"_ca + fitxer.getFileName() + "» (" + juce::String ((double) r.frames / fs, 1) + " s, retard corregit "
                       + retard + ")");
    estat.canviat ("Gravada «"_ca + fitxer.getFileName() + "» a la pista «"_ca + nomPista + "»"_ca);
    if (r.perdudes > 0)
        avisDeTargeta ("Compte: s'han perdut mostres",
                       "El disc no ha anat prou de pressa i s'han perdut "_ca + juce::String (r.perdudes) + " mostres de la gravació.");
}

void PantallaEstudi::tic()
{
    auto& motor = servei.estudi();
    motor.recull();

    // Estat del motor.
    const int ara = (int) motor.getEstat();
    if (esperantMotor)
    {
        const bool fet = estatEsperat == 0 ? ara == 0 : ara != 0;
        if (fet) esperantMotor = false;
        else if (juce::Time::currentTimeMillis() - tempsOrdre > 1500)
        {
            esperantMotor = false;
            if (estatEsperat != 0)
            {
                estat.transport = 0;
                avisDeTargeta ("La targeta no respon", "S'ha demanat sonar però la targeta d'àudio no ha respost. Mira Fitxer → Preferències."_ca);
            }
            estat.avisa();
        }
    }
    else if (ara != estat.transport)
    {
        if (ara == 3 && estat.transport == 2) estat.info = "Gravant…"_ca;
        if (ara == 0 && estat.transport == 1) estat.capcal = iniciPlay;   // la targeta s'ha aturat sola
        estat.transport = ara;
        estat.avisa();
    }
    if (estat.transport == 0 && ! esperantMotor && motor.gravadora().activa())
        acabaGravacio();
    if (gravacio.midi && gravacio.pista >= 0)
    {
        recullMidiGravat();
        if (estat.transport == 0 && ! esperantMotor) acabaGravacioMidi();
    }

    // Capçal: mentre sona, el del motor (i la vista el segueix).
    const float xAbans = estat.x (estat.capcal);
    if (estat.transport != 0 && ! esperantMotor)
    {
        estat.capcal = motor.getPosicio();
        const double visibles = pistes.getWidth() / estat.pxPerSegon;
        if (estat.capcal > estat.iniciVista + visibles * 0.95 || estat.capcal < estat.iniciVista)
        {
            estat.iniciVista = juce::jmax (0.0, estat.capcal - visibles * 0.05);
            actualitzaBarres();
            estat.avisa();
        }
        else
        {
            const float xAra = estat.x (estat.capcal);
            const int x0 = (int) juce::jmin (xAbans, xAra) - 6, x1 = (int) juce::jmax (xAbans, xAra) + 6;
            if (estat.transport == 3) pistes.repaint();
            else pistes.repaint (x0, 0, x1 - x0, pistes.getHeight());
            regla.repaint (x0, 0, x1 - x0, regla.getHeight());
        }
        zonaEditors.ticTransport();
        if (taulaEstructura.isVisible()) taulaEstructura.repaint();
    }
    recullVeu();
    if (editorVeu.isVisible()) editorVeu.tic();
    if (editorSampler.isVisible()) editorSampler.tic();


    // Vúmetres: el pic de cada canal, amb caiguda suau.
    auto cau = [] (float& v, float nou) { v = juce::jmax (nou, v * 0.82f); if (v < 1.0e-4f) v = 0.0f; };
    bool canvi = false;
    for (int i = 0; i < maxSlots * 2; ++i)
    {
        const float abans = estat.nivells[(size_t) i];
        cau (estat.nivells[(size_t) i], motor.vuPista (i / 2, i % 2).llegeixIReinicia());
        canvi = canvi || std::abs (abans - estat.nivells[(size_t) i]) > 1.0e-6f;
    }
    for (int e = 0; e < 32; ++e)
    {
        const float abans = estat.nivellsEntrada[(size_t) e];
        cau (estat.nivellsEntrada[(size_t) e], motor.vuEntrada (e).llegeixIReinicia());
        canvi = canvi || std::abs (abans - estat.nivellsEntrada[(size_t) e]) > 1.0e-6f;
    }
    for (int c = 0; c < 2; ++c)
    {
        const float abans = estat.nivellsSortida[(size_t) c];
        cau (estat.nivellsSortida[(size_t) c], motor.vuSortida (c).llegeixIReinicia());
        canvi = canvi || std::abs (abans - estat.nivellsSortida[(size_t) c]) > 1.0e-6f;
    }
    if (canvi)
    {
        if (capcaleres.isVisible()) capcaleres.repaint (capcaleres.getWidth() - 20, 0, 20, capcaleres.getHeight());
        esquerra.repaint();
        if (mescles.isVisible()) mescles.repaint();
    }

    // De tant en tant, les entrades de la targeta (per si s'ha canviat al Diagnòstic).
    static int comptador = 0;
    if (++comptador % 30 == 0)
    {
        auto noms = servei.getNomsEntrades();
        if (noms != estat.nomsEntrades) { estat.nomsEntrades = noms; capcaleres.repaint(); }
    }
}

void PantallaEstudi::exportaMescla()
{
    desaAra();
    const auto tros = trosPerExportar (estat.projecte);
    if (tros.second <= tros.first)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Exporta la mescla", "No hi ha res que soni per exportar."_ca);
        return;
    }
    const auto proposta = estat.carpeta.getChildFile (dibuix::utf8 (estat.projecte.nom) + " - mescla.wav");
    selector = std::make_unique<juce::FileChooser> ("Exporta la mescla (WAV estèreo, 24 bits)"_ca, proposta, "*.wav");
    selector->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                               | juce::FileBrowserComponent::warnAboutOverwriting,
                           [this, tros] (const juce::FileChooser& fc)
    {
        auto desti = fc.getResult();
        if (desti == juce::File()) return;
        if (! desti.hasFileExtension ("wav")) desti = desti.withFileExtension ("wav");
        const double fs = servei.estudi().getFs() > 0.0 ? servei.estudi().getFs() : 48000.0;
        std::string error;
        juce::MouseCursor::showWaitCursor();
        auto interns = Instruments::perExportar (fs, 4096, estat.carpeta);
        const bool fet = mibase::exportaMescla (*construeixPla (estat.projecte, audioPreses,
                                                    [this, fs, interns] (const mibase::Pista& t) { return GestorPlugins::esVst3 (t.instrument) ? plugins.instrumentNou (t, fs, 4096) : interns (t); },
                                                    [this, fs] (const mibase::Insert& x) { return x.actiu ? plugins.efecteNou (x, fs, 4096) : nullptr; },
                                                    &serveiVeu.corregides()),
                                                fs, tros.first, tros.second, dibuix::cami (desti), error);
        juce::MouseCursor::hideWaitCursor();
        if (! fet)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "No s'ha pogut exportar", dibuix::utf8 (error));
            return;
        }
        const auto que = estat.projecte.bucle.marcat && estat.projecte.bucle.actiu ? juce::String ("el bucle") : juce::String ("del primer al darrer clip");
        disc::afegeixEvolucio (dibuix::cami (estat.carpeta), dibuix::araText().toStdString(),
                               ("Mescla exportada a " + desti.getFullPathName()).toStdString());
        historial.afegeix ("Mescla exportada a "_ca + desti.getFullPathName());
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Mescla exportada",
            "S'ha desat "_ca + que + " (" + juce::String (tros.second - tros.first, 1).replaceCharacter ('.', ',') + " s) a:\n" + desti.getFullPathName()
            + "\n\nSona tal com ho sents (volums, panorama, silencis i solos), sense la claqueta."_ca);
        desti.revealToUser();
    });
}

std::pair<juce::String, juce::String> PantallaEstudi::textPosicio() const
{
    const double t = estat.capcal;
    const int min = (int) (t / 60.0);
    const double seg = t - min * 60.0;
    return { dibuix::utf8 (ed::textPosicio (estat.projecte, t)),
             juce::String::formatted ("%02d:%06.3f", min, seg) };
}

void PantallaEstudi::obreKits()
{
    if (finestraKits != nullptr) { finestraKits->toFront (true); return; }
    finestraKits = std::make_unique<FinestraKits> (estat, servei, [this] { finestraKits.reset(); });
}

void PantallaEstudi::pistaNova (const juce::String& tipus, int canals)
{
    estat.desfes.abans (estat.projecte);
    if (tipus == "bateria")
    {
        // 0.20: pista de bateria amb el kit intern (o el darrer que s'ha fet servir) i un clip de 4 compassos amb un ritme bàsic.
        const auto nom = ed::nomPistaLliure (estat.projecte, "Bateria");
        auto& t = ed::afegeixPista (estat.projecte, nom, "bateria");
        t.canals = 2;
        t.instrument = "intern:bateria";
        kits::posaKit (t, kits::ultimKit());
        estat.pistaTriada = estat.numPistes() - 1;
        estat.capTriat();
        estat.info = "Pista de bateria: fes un clip amb el llapis (2) i fes-hi doble clic per obrir el Drumpad"_ca;
        estat.canviat ("Pista nova: «"_ca + dibuix::utf8 (nom) + "» (bateria)"_ca);
        return;
    }
    if (tipus == "sampler")
    {
        // 0.26: pista d'instrument amb el samplejador; s'obre l'editor per carregar-hi un sample.
        const auto nom = ed::nomPistaLliure (estat.projecte, "Sampler");
        auto& t = ed::afegeixPista (estat.projecte, nom, "midi");
        t.canals = 2;
        t.instrument = mibase::instrumentSampler;
        estat.pistaTriada = estat.numPistes() - 1;
        estat.capTriat();
        estat.canviat ("Pista nova: «"_ca + dibuix::utf8 (nom) + "» (samplejador)"_ca);
        obreSampler (estat.pistaTriada);
        estat.info = "Samplejador: carrega un sample (o arrossega'l a l'ona) i toca'l amb el teclat"_ca;
        estat.avisa();
        return;
    }
    if (tipus == "midi")
    {
        const auto nom = ed::nomPistaLliure (estat.projecte, "Piano");
        auto& t = ed::afegeixPista (estat.projecte, nom, "midi");
        t.canals = 2;
        t.instrument = "intern:piano";
        estat.pistaTriada = estat.numPistes() - 1;
        estat.capTriat();
        estat.info = "Pista d'instrument: fes un clip amb el llapis (2) i dibuixa-hi notes, o grava amb el teclat MIDI"_ca;
        estat.canviat ("Pista nova: «"_ca + dibuix::utf8 (nom) + "» (instrument MIDI)"_ca);
        return;
    }
    const auto nom = ed::nomPistaLliure (estat.projecte, (canals == 2 ? juce::String::fromUTF8 ("Àudio estèreo") : juce::String::fromUTF8 ("Àudio")).toStdString());
    auto& t = ed::afegeixPista (estat.projecte, nom, tipus.toStdString());
    t.canals = canals;
    estat.pistaTriada = estat.numPistes() - 1;
    estat.capTriat();
    estat.canviat ("Pista nova: «"_ca + dibuix::utf8 (nom) + "» ("_ca + (canals == 2 ? "estèreo" : "mono") + ")"_ca);
}

void PantallaEstudi::canviaNom (int pista)
{
    if (estat.pista (pista) == nullptr) return;
    const auto actual = dibuix::utf8 (estat.projecte.pistes[(size_t) pista].nom);
    dialeg = std::make_unique<DialegText> ("Nom de la pista", "Els àudios que ja hi ha conserven el seu nom."_ca, "Canvia'l"_ca,
        [] (const juce::String& t) -> juce::String { return t.trim().isEmpty() ? juce::String ("Escriu un nom.") : juce::String(); },
        [this, pista, actual] (const juce::String& t)
        {
            if (estat.pista (pista) == nullptr || t.trim() == actual) return;
            estat.desfes.abans (estat.projecte);
            estat.projecte.pistes[(size_t) pista].nom = t.trim().toStdString();
            estat.canviat ("Pista «"_ca + actual + "» ara es diu «"_ca + t.trim() + "»"_ca);
        }, actual);
    DialegText::mostra (*this, dialeg);
}

void PantallaEstudi::canviaTempo()
{
    dialeg = std::make_unique<DialegText> ("Tempo", "En bpm (de 30 a 300). Els clips d'àudio no es mouen; els MIDI segueixen els compassos."_ca, "Canvia'l"_ca,
        [] (const juce::String& t) -> juce::String
        {
            const auto v = t.trim().replaceCharacter (',', '.');
            if (! v.containsOnly ("0123456789.")) return "Escriu només el número."_ca;
            const double bpm = v.getDoubleValue();
            return (bpm < 30.0 || bpm > 300.0) ? juce::String ("Ha de ser entre 30 i 300.") : juce::String();
        },
        [this] (const juce::String& t)
        {
            const double bpm = t.trim().replaceCharacter (',', '.').getDoubleValue();
            if (std::abs (bpm - estat.projecte.tempo) < 0.001) return;
            estat.desfes.abans (estat.projecte);
            const double abans = estat.projecte.tempo;
            midi::canviaTempo (estat.projecte, bpm);   // els clips MIDI queden als mateixos compassos
            servei.motor().tempoClic = bpm;
            estat.canviat ("Tempo de "_ca + juce::String (abans, 0) + " a " + juce::String (bpm, 0) + " bpm");
        }, juce::String (estat.projecte.tempo, 0));
    DialegText::mostra (*this, dialeg);
}

void PantallaEstudi::novaVersio()
{
    desaAra();
    const auto actual = dibuix::utf8 (estat.projecte.nom);
    const auto carpeta = estat.carpeta;
    dialeg = std::make_unique<DialegText> ("Nova versió a partir d'aquí"_ca,
        "Es desa una còpia del projecte tal com és ara, a la mateixa carpeta (comparteix els àudios). Després s'obre la versió nova."_ca,
        "Crea-la"_ca,
        [carpeta] (const juce::String& t) -> juce::String
        {
            const auto v = disc::validaNom (t.trim().toStdString());
            if (! v.correcte) return dibuix::utf8 (v.missatge);
            if (carpeta.getChildFile (t.trim() + ".mibase").exists()) return "Ja hi ha una versió amb aquest nom."_ca;
            return {};
        },
        [this, actual] (const juce::String& t)
        {
            const auto r = disc::novaVersio (dibuix::cami (estat.fitxer), estat.projecte, t.trim().toStdString(),
                                             dibuix::araIso().toStdString(), dibuix::araText().toStdString());
            if (! r.correcte)
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "No s'ha creat la versió"_ca, dibuix::utf8 (r.error));
                return;
            }
            historial.afegeix ("Nova versió: "_ca + t.trim() + " (a partir de " + actual + ")");
            const auto f = dibuix::fitxer (r.fitxer);
            juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PantallaEstudi> (this), f] { if (sp && sp->onObre) sp->onObre (f); });
        }, actual + " v2");
    DialegText::mostra (*this, dialeg);
}

void PantallaEstudi::exportaProjecte()
{
    desaAra();
    selector = std::make_unique<juce::FileChooser> ("On vols exportar el projecte? (s'hi crearà una carpeta)"_ca,
                                                    juce::File::getSpecialLocation (juce::File::userDesktopDirectory));
    selector->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                           [this] (const juce::FileChooser& fc)
    {
        const auto desti = fc.getResult();
        if (! desti.isDirectory()) return;
        const auto r = disc::recopila (dibuix::cami (estat.carpeta), estat.projecte, dibuix::cami (desti),
                                       dibuix::araIso().toStdString(), dibuix::araText().toStdString());
        if (! r.correcte)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "No s'ha pogut exportar"_ca, dibuix::utf8 (r.error));
            return;
        }
        juce::String text = "S'ha creat la carpeta amb el projecte i només els àudios que fa servir:\n"_ca + dibuix::fitxer (r.fitxer).getParentDirectory().getFullPathName();
        for (auto& a : r.avisos) text << "\n" << dibuix::utf8 (a);
        disc::afegeixEvolucio (dibuix::cami (estat.carpeta), dibuix::araText().toStdString(),
                               ("Exportat a " + dibuix::fitxer (r.fitxer).getParentDirectory().getFullPathName()).toStdString());
        historial.afegeix ("Projecte exportat a "_ca + dibuix::fitxer (r.fitxer).getParentDirectory().getFullPathName());
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Projecte exportat", text);
        dibuix::fitxer (r.fitxer).revealToUser();
    });
}

void PantallaEstudi::importaDeMenu()
{
    demanaImportar (estat.pistaTriada, juce::jmax (0.0, ed::enganxa (estat.projecte, estat.capcal, estat.projecte.snap)));
}

void PantallaEstudi::demanaImportar (int pista, double on)
{
    selector = std::make_unique<juce::FileChooser> ("Importa àudio"_ca, juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                    "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    selector->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                               | juce::FileBrowserComponent::canSelectMultipleItems,
                           [this, pista, on] (const juce::FileChooser& fc)
    {
        juce::StringArray fitxers;
        for (auto& f : fc.getResults()) fitxers.add (f.getFullPathName());
        if (! fitxers.isEmpty()) importa (fitxers, pista, on);
    });
}

juce::File PantallaEstudi::audios() const
{
    return estat.carpeta.getChildFile (juce::String::fromUTF8 (disc::carpetaAudios));
}

juce::File PantallaEstudi::fitxerPresa (const Presa& s) const
{
    return dibuix::fitxer (disc::camiRelatiu (dibuix::cami (estat.carpeta), s.fitxer));
}

void PantallaEstudi::carregaOna (const Presa& s)
{
    auto ona = std::make_shared<OnaPresa>();
    ona->lector.reset (formats.createReaderFor (fitxerPresa (s)));
    if (ona->lector == nullptr || ona->lector->sampleRate <= 0.0)
    {
        estat.ones.erase (s.id);
        audioPreses.erase (s.id);
        return;
    }

    auto& lector = *ona->lector;
    ona->sr = lector.sampleRate;
    ona->longitud = lector.lengthInSamples;
    ona->canals = (int) lector.numChannels;
    const auto blocs = (size_t) ((ona->longitud + OnaPresa::bloc - 1) / OnaPresa::bloc);
    ona->mins.assign (blocs, 0.0f);
    ona->maxs.assign (blocs, 0.0f);

    // L'àudio sencer a la memòria (per sonar) i, alhora, l'ona per dibuixar.
    auto audio = std::make_shared<AudioPresa>();
    audio->fs = lector.sampleRate;
    audio->canals = lector.numChannels > 1 ? 2 : 1;
    audio->longitud = ona->longitud;
    for (int c = 0; c < audio->canals; ++c)
        audio->dades[c].assign ((size_t) audio->longitud, 0.0f);

    const int tros = OnaPresa::bloc * 512;
    juce::AudioBuffer<float> buffer ((int) juce::jmax (1u, lector.numChannels), tros);
    const float factor = 1.0f / (float) buffer.getNumChannels();
    for (juce::int64 pos = 0; pos < ona->longitud; pos += tros)
    {
        const int n = (int) juce::jmin ((juce::int64) tros, ona->longitud - pos);
        lector.read (&buffer, 0, n, pos, true, true);
        for (int c = 0; c < audio->canals; ++c)
            std::copy (buffer.getReadPointer (c), buffer.getReadPointer (c) + n, audio->dades[c].data() + pos);
        for (int i = 0; i < n; ++i)
        {
            float v = 0.0f;
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch) v += buffer.getReadPointer (ch)[i];
            v *= factor;
            const auto b = (size_t) ((pos + i) / OnaPresa::bloc);
            ona->mins[b] = juce::jmin (ona->mins[b], v);
            ona->maxs[b] = juce::jmax (ona->maxs[b], v);
        }
    }
    estat.ones[s.id] = std::move (ona);
    audioPreses[s.id] = std::move (audio);
}

bool PantallaEstudi::importaUn (const juce::File& f, int& pista, double on, double& final, juce::String& error)
{
    std::unique_ptr<juce::AudioFormatReader> lector (formats.createReaderFor (f));
    if (lector == nullptr || lector->sampleRate <= 0.0 || lector->lengthInSamples <= 0)
    {
        error = "«"_ca + f.getFileName() + "» no és un àudio que Mibase sàpiga llegir."_ca;
        return false;
    }

    if (pista < 0 || pista >= estat.numPistes())
    {
        auto base = f.getFileNameWithoutExtension().substring (0, 40).trim();
        if (base.isEmpty()) base = juce::String::fromUTF8 ("Àudio");
        const auto nom = ed::nomPistaLliure (estat.projecte, base.toStdString());
        ed::afegeixPista (estat.projecte, nom, "audio").canals = lector->numChannels > 1 ? 2 : 1;
        pista = estat.numPistes() - 1;
    }

    auto& t = estat.projecte.pistes[(size_t) pista];
    auto carpetaAudios = audios();
    carpetaAudios.createDirectory();
    const auto nomDesti = dibuix::utf8 (disc::nomPresa (dibuix::cami (carpetaAudios), t.nom));
    const auto desti = carpetaAudios.getChildFile (nomDesti);

    if (f.hasFileExtension ("wav"))
    {
        if (! f.copyFileTo (desti))
        {
            error = "No s'ha pogut copiar «"_ca + f.getFileName() + "» a Àudios."_ca;
            return false;
        }
    }
    else
    {
        std::unique_ptr<juce::OutputStream> sortida = desti.createOutputStream();
        juce::WavAudioFormat wav;
        auto escriptor = sortida != nullptr ? wav.createWriterFor (sortida, juce::AudioFormatWriterOptions{}
                                                                            .withSampleRate (lector->sampleRate)
                                                                            .withNumChannels ((int) lector->numChannels)
                                                                            .withBitsPerSample (24))
                                            : nullptr;
        if (escriptor == nullptr || ! escriptor->writeFromAudioReader (*lector, 0, -1))
        {
            escriptor.reset();
            desti.deleteFile();
            error = "No s'ha pogut convertir «"_ca + f.getFileName() + "» a WAV."_ca;
            return false;
        }
    }

    const double durada = (double) lector->lengthInSamples / lector->sampleRate;
    const auto fitxerRelatiu = (juce::String::fromUTF8 (disc::carpetaAudios) + "/" + nomDesti).toStdString();
    const auto idPresa = ed::afegeixPresa (estat.projecte, fitxerRelatiu, f.getFileName().toStdString(), durada).id;
    ed::afegeixClip (estat.projecte, pista, idPresa, on);
    final = on + durada;
    carregaOna (*ed::presa (estat.projecte, idPresa));
    disc::afegeixEvolucio (dibuix::cami (estat.carpeta), dibuix::araText().toStdString(),
                           ("Importat «"_ca + f.getFileName() + "» a la pista «"_ca + dibuix::utf8 (t.nom) + "» ("_ca + nomDesti + ")").toStdString());
    return true;
}

void PantallaEstudi::importa (const juce::StringArray& fitxers, int pista, double on)
{
    const auto copia = estat.projecte;
    juce::StringArray errors;
    int fets = 0;
    double posicio = juce::jmax (0.0, on);
    int ultimaPista = pista;

    for (auto& cami : fitxers)
    {
        int p = pista;
        double final = posicio;
        juce::String error;
        if (importaUn (juce::File (cami), p, posicio, final, error))
        {
            ++fets;
            ultimaPista = p;
            if (pista >= 0)
            {
                const double compas = ed::segonsPerCompas (estat.projecte);
                posicio = std::ceil (final / compas - 1e-6) * compas;
            }
        }
        else
            errors.add (error);
    }

    if (fets > 0)
    {
        estat.desfes.abans (copia);
        estat.pistaTriada = ultimaPista;
        estat.capTriat();
        estat.canviat();
        historial.afegeix ("Importats "_ca + juce::String (fets) + " fitxers d'àudio.");
    }
    if (! errors.isEmpty())
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Algun fitxer no s'ha pogut importar", errors.joinIntoString ("\n"));
}

void PantallaEstudi::consolida (int pista, int ia, int ib)
{
    // Ajunta dos clips en un fitxer nou: es llegeix el que sona de cada un (amb fades i volum), es barreja i es desa.
    auto* t = estat.pista (pista);
    if (t == nullptr) return;
    const auto a = t->clips[(size_t) ia], b = t->clips[(size_t) ib];

    double sr = 0.0;
    int canals = 1;
    for (const auto* c : { &a, &b })
    {
        if (c->presa.empty()) continue;
        auto it = estat.ones.find (c->presa);
        if (it == estat.ones.end() || it->second == nullptr || it->second->lector == nullptr)
        {
            estat.info = "No es troba l'àudio d'un dels clips"_ca;
            estat.avisa();
            return;
        }
        if (sr > 0.0 && std::abs (sr - it->second->sr) > 0.5)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "No es poden enganxar",
                                                    "Els dos àudios tenen freqüències de mostreig diferents."_ca);
            return;
        }
        sr = it->second->sr;
        canals = juce::jmax (canals, it->second->canals > 1 ? 2 : 1);
    }
    if (sr <= 0.0) return;

    const double ini = juce::jmin (a.iniciVisible(), b.iniciVisible());
    const double fi = juce::jmax (a.fiVisible(), b.fiVisible());
    const int total = (int) std::ceil ((fi - ini) * sr);
    juce::AudioBuffer<float> mescla (canals, total);
    mescla.clear();

    for (const auto* c : { &a, &b })
    {
        if (c->presa.empty() || c->mut) continue;
        auto& ona = *estat.ones[c->presa];
        const double guany = std::pow (10.0, c->guanyDb / 20.0);
        // Tros del fitxer que sona: [max(inici, 0), min(fi, durada)].
        const double fa = juce::jmax (c->inici, 0.0), fb = juce::jmin (c->fi, (double) ona.longitud / sr);
        if (fb <= fa) continue;
        const auto s0 = (juce::int64) std::round (fa * sr);
        const int n = (int) std::round ((fb - fa) * sr);
        juce::AudioBuffer<float> llegit ((int) juce::jmax (1u, ona.lector->numChannels), n);
        ona.lector->read (&llegit, 0, n, s0, true, true);
        const int desti0 = (int) std::round ((c->pos + fa - ini) * sr);
        for (int i = 0; i < n; ++i)
        {
            const double tf = fa + (double) i / sr;
            double env = 1.0;
            if (c->fadeIn > 0.0 && tf < c->inici + c->fadeIn) env = juce::jmax (0.0, (tf - c->inici) / c->fadeIn);
            if (c->fadeOut > 0.0 && tf > c->fi - c->fadeOut) env = juce::jmin (env, juce::jmax (0.0, (c->fi - tf) / c->fadeOut));
            const int k = desti0 + i;
            if (k < 0 || k >= total) continue;
            for (int ch = 0; ch < canals; ++ch)
            {
                const int origen = juce::jmin (ch, llegit.getNumChannels() - 1);
                mescla.addSample (ch, k, (float) (llegit.getSample (origen, i) * guany * env));
            }
        }
    }

    auto carpetaAudios = audios();
    carpetaAudios.createDirectory();
    const auto nomDesti = dibuix::utf8 (disc::nomPresa (dibuix::cami (carpetaAudios), t->nom));
    const auto desti = carpetaAudios.getChildFile (nomDesti);
    {
        std::unique_ptr<juce::OutputStream> sortida = desti.createOutputStream();
        juce::WavAudioFormat wav;
        auto escriptor = sortida != nullptr ? wav.createWriterFor (sortida, juce::AudioFormatWriterOptions{}
                                                                            .withSampleRate (sr).withNumChannels (canals).withBitsPerSample (24))
                                            : nullptr;
        if (escriptor == nullptr || ! escriptor->writeFromAudioSampleBuffer (mescla, 0, total))
        {
            escriptor.reset();
            desti.deleteFile();
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "No es poden enganxar", "No s'ha pogut escriure el fitxer nou.");
            return;
        }
    }

    estat.desfes.abans (estat.projecte);
    const auto* sa = ed::presa (estat.projecte, a.presa);
    const auto* sb = ed::presa (estat.projecte, b.presa);
    const auto nomPresa = juce::String ("Enganxat: ") + (sa ? dibuix::utf8 (sa->nom) : juce::String ("buida")) + " + "
                        + (sb ? dibuix::utf8 (sb->nom) : juce::String ("buida"));
    const auto idPresa = ed::afegeixPresa (estat.projecte, (juce::String::fromUTF8 (disc::carpetaAudios) + "/" + nomDesti).toStdString(),
                                           nomPresa.toStdString(), (double) total / sr).id;
    ed::treuClips (estat.projecte, pista, { ia, ib });
    ed::afegeixClip (estat.projecte, pista, idPresa, ini);
    carregaOna (*ed::presa (estat.projecte, idPresa));
    estat.triaClip (pista, (int) estat.projecte.pistes[(size_t) pista].clips.size() - 1);
    estat.info = "Enganxats en un fitxer nou: "_ca + nomDesti;
    estat.canviat ("Enganxats dos clips a «"_ca + dibuix::utf8 (estat.projecte.pistes[(size_t) pista].nom) + "» en un fitxer nou ("_ca + nomDesti + ")");
}

//==============================================================================
void PantallaEstudi::duplica()
{
    if (estat.triats.empty()) return;
    estat.desfes.abans (estat.projecte);
    estat.triats = ed::duplicaTriats (estat.projecte, estat.triats);
    estat.canviat();
}

void PantallaEstudi::treu()
{
    if (estat.rang.actiu)
    {
        // Buida el tros triat.
        estat.desfes.abans (estat.projecte);
        for (int i = estat.rang.pistaDe; i <= estat.rang.pistaFins; ++i)
            ed::buidaRang (estat.projecte, i, estat.rang.t0, estat.rang.t1);
        estat.info = "Tros buidat";
        estat.capTriat();
        estat.canviat();
        return;
    }
    if (estat.triats.empty()) return;
    estat.desfes.abans (estat.projecte);
    ed::treuTriats (estat.projecte, estat.triats);
    estat.capTriat();
    estat.canviat();
}

void PantallaEstudi::copia (bool retalla)
{
    if (estat.rang.actiu)
        estat.portaretalls = ed::copiaRang (estat.projecte, estat.rang.pistaDe, estat.rang.pistaFins, estat.rang.t0, estat.rang.t1);
    else if (! estat.triats.empty())
        estat.portaretalls = ed::copiaClips (estat.projecte, estat.triats);
    else
    {
        estat.info = "No hi ha res triat per copiar";
        estat.avisa();
        return;
    }
    const auto n = (int) estat.portaretalls.clips.size();
    estat.info = (retalla ? "Retallat: "_ca : "Copiat: "_ca) + juce::String (n) + (n == 1 ? " clip" : " clips")
               + " · Ctrl+V l'enganxa al capçal"_ca;
    if (retalla) treu();
    else estat.avisa();
}

void PantallaEstudi::enganxa()
{
    if (estat.portaretalls.buit())
    {
        estat.info = "No hi ha res copiat (Ctrl+C)";
        estat.avisa();
        return;
    }
    estat.desfes.abans (estat.projecte);
    estat.triats = ed::enganxaRetall (estat.projecte, estat.portaretalls, estat.capcal, estat.insereix);
    estat.rang.actiu = false;
    if (! estat.triats.empty()) estat.pistaTriada = estat.triats.back().pista;
    estat.info = estat.insereix ? "Enganxat inserint: el que hi havia s'ha apartat"_ca : "Enganxat trepitjant el que hi havia"_ca;
    estat.canviat();
}

void PantallaEstudi::canviaModeRang (bool rang)
{
    estat.modeRang = rang;
    ajustos.setValue ("estudi.modeRang", rang);
    estat.info = rang ? "Requadre: només el que queda dins"_ca : "Requadre: clips sencers"_ca;
    estat.avisa();
}

void PantallaEstudi::canviaInsereix (bool ins)
{
    estat.insereix = ins;
    ajustos.setValue ("estudi.insereix", ins);
    estat.info = ins ? "Enganxar: insereix (aparta el que hi ha després del capçal, a totes les pistes)"_ca
                     : "Enganxar: trepitja el que hi ha"_ca;
    estat.avisa();
}

void PantallaEstudi::silenciaTriat()
{
    estat.desfes.abans (estat.projecte);
    if (! estat.triats.empty())
    {
        const bool nou = ! estat.clip (estat.triats[0])->mut;
        for (auto& r : estat.triats) if (auto* c = estat.clip (r)) c->mut = nou;
        estat.info = nou ? "Silenciat" : "Sonant";
    }
    else if (const auto v = estat.indexsTriades(); v.size() > 1)
    {
        // Diverses pistes triades (o una carpeta): totes alhora.
        bool totes = true;
        for (int i : v) totes &= estat.projecte.pistes[(size_t) i].mut;
        for (int i : v) estat.projecte.pistes[(size_t) i].mut = ! totes;
        estat.info = ! totes ? "Pistes silenciades" : "Pistes sonant";
    }
    else if (auto* t = estat.pista (estat.pistaTriada))
    {
        t->mut = ! t->mut;
        estat.info = t->mut ? "Pista silenciada" : "Pista sonant";
    }
    else return;
    estat.canviat();
}

void PantallaEstudi::demanaNomCarpeta (std::vector<int> v, const std::string& idACanviar)
{
    auto* existent = ed::carpeta (estat.projecte, idACanviar);
    const auto inicial = existent != nullptr ? dibuix::utf8 (existent->nom)
                                             : "Carpeta "_ca + juce::String ((int) estat.projecte.carpetes.size() + 1);
    dialeg = std::make_unique<DialegText> (existent != nullptr ? "Canvia el nom de la carpeta"_ca : "Carpeta nova"_ca,
        existent != nullptr ? juce::String() : (v.empty() ? "Després hi podràs arrossegar pistes."_ca
                                                          : juce::String ((int) v.size()) + (v.size() == 1 ? " pista hi anirà."_ca : " pistes hi aniran."_ca)),
        existent != nullptr ? "Canvia"_ca : "Crea"_ca,
        [] (const juce::String& t) { return t.trim().isEmpty() ? "Escriu un nom"_ca : juce::String(); },
        [this, v, idACanviar] (const juce::String& t)
        {
            estat.desfes.abans (estat.projecte);
            const auto nom = t.trim().toStdString();
            if (auto* c = ed::carpeta (estat.projecte, idACanviar)) c->nom = nom;
            else
            {
                ed::creaCarpeta (estat.projecte, nom, v);
                estat.panellCarpetes = true;
                colocaSeccions();
            }
            estat.canviat ("Carpeta «"_ca + t.trim() + "»");
        }, inicial);
    DialegText::mostra (*this, dialeg);
}

void PantallaEstudi::fesCrossfade()
{
    // 0.14: X amb un tros triat (requadre «només el tros») on dos clips d'àudio se sobreposen.
    auto copia = estat.projecte;
    int fets = 0;
    for (int i = estat.rang.pistaDe; i <= estat.rang.pistaFins; ++i)
        if (ed::crossfade (copia, i, estat.rang.t0, estat.rang.t1)) ++fets;
    if (fets == 0)
    {
        estat.info = "Per fer un crossfade, tria (en Lliure) el tros on se sobreposen dos clips d'àudio i prem X"_ca;
        estat.avisa();
        return;
    }
    const int ms = juce::roundToInt ((estat.rang.t1 - estat.rang.t0) * 1000.0);
    estat.desfes.abans (estat.projecte);
    estat.projecte = std::move (copia);
    estat.rang = {};
    estat.canviat ("Crossfade de "_ca + juce::String (ms) + " ms");
}

void PantallaEstudi::commutaBucle()
{
    auto& b = estat.projecte.bucle;
    estat.desfes.abans (estat.projecte);
    if (! b.marcat)
    {
        const int c = ed::compasA (estat.projecte, estat.capcal);
        ed::marcaBucle (estat.projecte, c, c + 3);
        estat.projecte.bucle.marcat = estat.projecte.bucle.actiu = true;
    }
    else b.actiu = ! b.actiu;
    estat.info = b.actiu ? "Bucle encès"_ca : "Bucle apagat"_ca;
    estat.canviat();
}

void PantallaEstudi::zoomA (double a, double b, int pistaDe, int pistaFins)
{
    if (b <= a || pistes.getWidth() <= 0) return;
    estat.pxPerSegon = juce::jlimit (2.0, 800000.0, pistes.getWidth() * 0.94 / (b - a));
    estat.iniciVista = juce::jmax (0.0, a - (pistes.getWidth() * 0.03) / estat.pxPerSegon);
    const int n = juce::jmax (1, pistaFins - pistaDe + 1);
    estat.altPista = juce::jlimit (VistaPistes::alcadaMinima, 260, (pistes.getHeight() - 8) / n);
    estat.desplY = 0;
    estat.desplY = juce::jmax (0, estat.yPista (pistaDe));
    actualitzaBarres();
    estat.avisa();
}

void PantallaEstudi::zoomTot()
{
    const double fi = juce::jmax (ed::final (estat.projecte), ed::segonsPerCompas (estat.projecte) * 4.0);
    zoomA (0.0, fi, 0, estat.numPistes() - 1);
    estat.info = "Tot el projecte";
}

void PantallaEstudi::zoomTriat()
{
    if (estat.rang.actiu)
    {
        zoomA (estat.rang.t0, estat.rang.t1, estat.rang.pistaDe, estat.rang.pistaFins);
        return;
    }
    if (estat.triats.empty()) { estat.info = "No hi ha res triat"; estat.avisa(); return; }
    double a = 1e300, b = -1e300;
    int pa = estat.numPistes(), pb = 0;
    for (auto& r : estat.triats)
        if (auto* c = estat.clip (r))
        {
            a = juce::jmin (a, c->iniciVisible());
            b = juce::jmax (b, c->fiVisible());
            pa = juce::jmin (pa, r.pista);
            pb = juce::jmax (pb, r.pista);
        }
    zoomA (a, b, pa, pb);
}

void PantallaEstudi::triaColor()
{
    const bool clips = ! estat.triats.empty();
    if (! clips && estat.pista (estat.pistaTriada) == nullptr)
    {
        estat.info = "Tria una pista o uns clips per pintar-los";
        estat.avisa();
        return;
    }
    const auto actual = clips ? estat.colorClip (estat.triats[0].pista, *estat.clip (estat.triats[0]))
                              : EstatEstudi::colorPista (*estat.pista (estat.pistaTriada));
    const auto que = clips ? (estat.triats.size() == 1 ? juce::String ("el clip triat") : juce::String (estat.triats.size()) + " clips triats")
                           : "la pista «"_ca + dibuix::utf8 (estat.pista (estat.pistaTriada)->nom) + "»"_ca;
    auto primer = std::make_shared<bool> (true);
    auto quins = estat.triats;
    const int pista = estat.pistaTriada;
    auto panellColor = std::make_unique<PanellColor> (actual, que, [this, primer, quins, pista, clips] (juce::Colour c)
    {
        if (*primer) { estat.desfes.abans (estat.projecte); *primer = false; }   // un sol Desfés per a tota la finestreta
        if (clips) { for (auto& r : quins) if (auto* k = estat.clip (r)) k->rgb = c.getARGB(); }
        else if (auto* t = estat.pista (pista)) t->rgb = c.getARGB();
        estat.canviat();
    });
    juce::CallOutBox::launchAsynchronously (std::move (panellColor), btColor.getScreenBounds(), nullptr);
}

void PantallaEstudi::desfes (bool refer)
{
    const bool fet = refer ? estat.desfes.refes (estat.projecte) : estat.desfes.desfes (estat.projecte);
    if (! fet) return;
    for (auto& s : estat.projecte.preses)
        if (estat.ones.find (s.id) == estat.ones.end())
            carregaOna (s);
    estat.info = refer ? "Refet" : "Desfet";
    estat.canviat();
}

bool PantallaEstudi::keyPressed (const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
    const int codi = k.getKeyCode();

    const bool cap = ! mods.isAnyModifierKeyDown();
    auto lletra = [codi] (char c) { return codi == c || codi == c + 32; };
    if (codi == juce::KeyPress::deleteKey || codi == juce::KeyPress::backspaceKey) { treu(); return true; }
    if (mods.isCommandDown() && lletra ('C')) { copia (false); return true; }
    if (mods.isCommandDown() && lletra ('X')) { copia (true); return true; }
    if (cap && lletra ('X') && estat.rang.actiu) { fesCrossfade(); return true; }
    if (mods.isCommandDown() && lletra ('V')) { enganxa(); return true; }
    if (codi == juce::KeyPress::insertKey) { canviaInsereix (! estat.insereix); return true; }
    // 0.24: / i * del teclat numèric (a Linux arriben com a «/» i «*»).
    if (codi == juce::KeyPress::numberPadDivide || (codi == '/' && ! mods.isCommandDown())) { commutaBucle(); return true; }
    if (codi == juce::KeyPress::numberPadMultiply || (codi == '*' && ! mods.isCommandDown())) { commutaSoloEditor(); return true; }
    if (cap && lletra ('M')) { silenciaTriat(); return true; }
    if (mods.isCommandDown() && lletra ('G')) { demanaNomCarpeta (estat.indexsTriades(), {}); return true; }
    if (cap && lletra ('L')) { commutaBucle(); return true; }
    if (cap && lletra ('P')) { mostraPianoRoll (! estat.pianoRoll); return true; }
    if (! mods.isCommandDown() && ! mods.isAltDown() && lletra ('Z'))
    {
        // 0.12: si el ratolí és sobre un Piano Roll, el zoom és el del Piano Roll.
        auto* sota = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse();
        auto* pr = sota != nullptr ? (dynamic_cast<PianoRoll*> (sota) != nullptr ? dynamic_cast<PianoRoll*> (sota) : sota->findParentComponentOfClass<PianoRoll>()) : nullptr;
        if (pr != nullptr) return pr->keyPressed (k);
    }
    if (! mods.isCommandDown() && ! mods.isAltDown() && lletra ('Z')) { if (mods.isShiftDown()) zoomTriat(); else zoomTot(); return true; }
    if (cap && lletra ('Q')) { estat.projecte.snap = Snap::Compas; estat.info = "Graella: compàs"_ca; estat.avisa(); calDesar = true; startTimer (700); return true; }
    if (cap && lletra ('W')) { estat.projecte.snap = Snap::Temps; estat.info = "Graella: temps"; estat.avisa(); calDesar = true; startTimer (700); return true; }
    if (cap && lletra ('E')) { estat.projecte.snap = Snap::Lliure; estat.info = "Graella: lliure"; estat.avisa(); calDesar = true; startTimer (700); return true; }
    if (mods.isCommandDown() && (codi == 'D' || codi == 'd')) { duplica(); return true; }
    if (mods.isCommandDown() && (codi == 'Z' || codi == 'z')) { desfes (mods.isShiftDown()); return true; }
    if (mods.isCommandDown() && (codi == 'Y' || codi == 'y')) { desfes (true); return true; }
    if (mods.isCommandDown() && (codi == 'S' || codi == 's')) { desaAra(); estat.info = "Desat"; estat.avisa(); return true; }
    if (mods.isCommandDown() && (codi == 'I' || codi == 'i')) { importaDeMenu(); return true; }
    if (! mods.isAnyModifierKeyDown() && codi >= '1' && codi <= '6') { triaEina ((Eina) (codi - '1')); return true; }
    if (codi == juce::KeyPress::escapeKey) { estat.triantFantasma = -1; estat.capTriat(); triaEina (Eina::Fletxa); return true; }
    if (codi == juce::KeyPress::homeKey) { transport (0); return true; }
    if (codi == juce::KeyPress::spaceKey) { if (estat.transport == 0) reprodueix(); else atura(); return true; }
    if (! mods.isAnyModifierKeyDown() && (codi == 'R' || codi == 'r')) { if (estat.transport == 0) grava(); else atura(); return true; }
    return false;
}

//==============================================================================
// Piano Roll i MIDI
//==============================================================================
void PantallaEstudi::configuraEditor (PianoRoll& pr)
{
    juce::Component::SafePointer<PianoRoll> sp (&pr);
    pr.onDirecte = [this] (int pista, std::uint8_t e, std::uint8_t d1, std::uint8_t d2)
    {
        if (auto* t = estat.pista (pista)) servei.estudi().enviaDirecte ({ slotDePista (*t), e, d1, d2 });
    };
    pr.onDesfes = [this] (bool refer) { desfes (refer); };
    pr.onSolo = [this] { commutaSoloEditor(); };
    pr.onTeclaNoUsada = [this] (const juce::KeyPress& k) { return keyPressed (k); };
    pr.onDesenganxa = [this]
    {
        const bool desenganxa = finestraPianoRoll == nullptr;
        ajustos.setValue ("pianoRoll.desenganxat", desenganxa);
        desenganxaPianoRoll (desenganxa);
    };
    juce::Component::SafePointer<PantallaEstudi> jo (this);
    pr.onTanca = [jo, sp]
    {
        if (sp == nullptr) return;
        const int i = sp->getIndex();
        juce::MessageManager::callAsync ([jo, i] { if (jo != nullptr) jo->tancaEditor (i); });   // no es pot esborrar l'editor des de dins seu
    };
    pr.onVistaCanviada = [this] { zonaEditors.repintaTots(); };
    pr.onActiu = [this] { zonaEditors.repintaTots(); esquerra.repaint(); estat.avisa(); };
}

void PantallaEstudi::tancaEditor (int index)
{
    if (index < 0 || index >= (int) estat.editors.size()) return;
    estat.editors.erase (estat.editors.begin() + index);
    estat.editorActiu = juce::jlimit (0, juce::jmax (0, (int) estat.editors.size() - 1), estat.editorActiu >= index ? estat.editorActiu - 1 : estat.editorActiu);
    zonaEditors.sincronitza();
    if (estat.editors.empty()) mostraPianoRoll (false);
    else { colocaSeccions(); estat.avisa(); }
}

void PantallaEstudi::obrePianoRoll (int pista, int clip)
{
    auto* c = estat.clip ({ pista, clip });
    if (c == nullptr || ! c->midi) return;
    estat.focusEditor = true;
    estat.triaClip (pista, clip);
    int i = estat.editorDe ({ pista, clip });
    if (i < 0)
    {
        // Un editor no bloquejat (primer l'actiu); si tots ho estan, un de nou a sota.
        if (estat.editorActiu < (int) estat.editors.size() && ! estat.editors[(size_t) estat.editorActiu].bloquejat) i = estat.editorActiu;
        for (int k = 0; k < (int) estat.editors.size() && i < 0; ++k)
            if (! estat.editors[(size_t) k].bloquejat) i = k;
        if (i < 0)
        {
            if ((int) estat.editors.size() >= EstatEstudi::maxEditors)
            {
                estat.info = "Ja hi ha 3 editors bloquejats: desbloqueja'n un per obrir aquest clip"_ca;
                estat.avisa();
                return;
            }
            estat.editors.push_back ({});
            i = (int) estat.editors.size() - 1;
        }
        estat.editorMostra (i, { pista, clip });
    }
    estat.editorActiu = i;
    zonaEditors.sincronitza();
    mostraPianoRoll (true);
    if (auto* pr = zonaEditors.editor (i))
    {
        pr->actualitza();
        pr->grabKeyboardFocus();
    }
    estat.avisa();
}

void PantallaEstudi::mostraPianoRoll (bool visible)
{
    if (visible && estat.veu.obert) estat.veu.obert = false;   // 0.22: comparteixen el lloc de baix
    if (visible && estat.sampler.obert) estat.sampler.obert = false;   // 0.26
    if (visible && estat.editors.empty())
    {
        // Un editor (amb el clip MIDI triat, si n'hi ha un).
        estat.editors.push_back ({});
        if (estat.triats.size() == 1)
            if (auto* c = estat.clip (estat.triats[0]); c != nullptr && c->midi) estat.editorMostra (0, estat.triats[0]);
        estat.editorActiu = 0;
        zonaEditors.sincronitza();
    }
    estat.pianoRoll = visible;
    if (visible && finestraPianoRoll == nullptr && ajustos.getBoolValue ("pianoRoll.desenganxat", false))
        desenganxaPianoRoll (true);
    else if (! visible && finestraPianoRoll != nullptr)
        desenganxaPianoRoll (false);
    colocaSeccions();
    estat.avisa();
    if (! visible) grabKeyboardFocus();
}

void PantallaEstudi::desenganxaPianoRoll (bool desenganxa)
{
    if (desenganxa)
    {
        if (finestraPianoRoll != nullptr) return;
        removeChildComponent (&zonaEditors);
        zonaEditors.setVisible (true);
        zonaEditors.setDesenganxat (true);
        juce::Component::SafePointer<PantallaEstudi> jo (this);
        finestraPianoRoll = std::make_unique<FinestraPianoRoll> (zonaEditors, [jo]
        {
            // En tancar la finestra, torna a baix (més tard: no es pot esborrar la finestra des de dins seu).
            juce::MessageManager::callAsync ([jo]
            {
                if (jo == nullptr) return;
                jo->ajustos.setValue ("pianoRoll.desenganxat", false);
                jo->desenganxaPianoRoll (false);
            });
        });
        const auto estatFinestra = ajustos.getValue ("pianoRoll.finestra");
        if (estatFinestra.isNotEmpty()) finestraPianoRoll->restoreWindowStateFromString (estatFinestra);
        estat.pianoRoll = true;
        historial.afegeix ("Piano Roll en una finestra a part"_ca);
    }
    else
    {
        if (finestraPianoRoll == nullptr) return;
        ajustos.setValue ("pianoRoll.finestra", finestraPianoRoll->getWindowStateAsString());
        finestraPianoRoll->clearContentComponent();
        finestraPianoRoll.reset();
        addChildComponent (zonaEditors);
        zonaEditors.setDesenganxat (false);
    }
    ajustos.saveIfNeeded();
    colocaSeccions();
    estat.avisa();
    if (auto* pr = zonaEditors.actiu()) pr->grabKeyboardFocus();
}

void PantallaEstudi::recullMidiGravat()
{
    MotorEstudi::MidiGravat m;
    while (servei.estudi().treuMidiGravat (m))
        gravacio.missatges.push_back (m);
}

void PantallaEstudi::acabaGravacioMidi()
{
    auto& motor = servei.estudi();
    recullMidiGravat();
    motor.setSlotGravaMidi (-1);
    const int pista = gravacio.pista;
    const double cursorRec = gravacio.cursor;
    double fins = juce::jmax (gravacio.fins, cursorRec);
    gravacio.pista = -1;
    gravacio.midi = false;
    gravacio.fins = 0.0;
    auto missatges = std::move (gravacio.missatges);
    gravacio.missatges.clear();
    estat.info.clear();

    auto* t = estat.pista (pista);
    if (t == nullptr || ! mibase::esMidi (*t)) return;

    // Notes: cada encesa amb la seva apagada (les que encara sonaven en aturar, s'acaben on s'ha aturat).
    const auto& p = estat.projecte;
    const double spt = midi::segonsPerTick (p);
    auto tick = [&] (double tt) { return (long long) std::llround ((tt - cursorRec) / spt); };
    Clip c = midi::clipNou (cursorRec, cursorRec + ed::segonsPerCompas (p));
    std::array<double, 16 * 128> obertes;
    std::array<int, 16 * 128> velocitats {};
    obertes.fill (-1.0);
    for (auto& m : missatges) fins = juce::jmax (fins, m.t);
    auto tanca = [&] (int canal, int to, double fi)
    {
        const size_t k = (size_t) (canal * 128 + to);
        if (obertes[k] < 0.0) return;
        Nota n;
        n.ini = tick (obertes[k]);
        n.dur = juce::jmax (1LL, tick (fi) - n.ini);
        n.to = to;
        n.vel = juce::jlimit (1, 127, velocitats[k]);
        n.canal = canal;
        c.notes.push_back (n);
        obertes[k] = -1.0;
    };
    for (auto& m : missatges)
    {
        const int tipus = m.estat & 0xF0, canal = m.estat & 0x0F;
        if (tipus == 0x90 && m.dada2 > 0)
        {
            tanca (canal, m.dada1 & 127, m.t);
            obertes[(size_t) (canal * 128 + (m.dada1 & 127))] = m.t;
            velocitats[(size_t) (canal * 128 + (m.dada1 & 127))] = m.dada2;
        }
        else if (tipus == 0x80 || tipus == 0x90)
            tanca (canal, m.dada1 & 127, m.t);
        else if (tipus == 0xB0 || tipus == 0xE0 || tipus == 0xD0 || tipus == 0xA0 || tipus == 0xC0)
        {
            EventMidi e;
            e.tick = tick (m.t);
            e.tipus = tipus;
            e.canal = canal;
            if (tipus == 0xE0) e.valor = (m.dada1 & 127) | ((m.dada2 & 127) << 7);
            else if (tipus == 0xD0 || tipus == 0xC0) e.valor = m.dada1;
            else { e.num = m.dada1; e.valor = m.dada2; }
            c.events.push_back (e);
        }
    }
    for (int k = 0; k < 16 * 128; ++k)
        if (obertes[(size_t) k] >= 0.0) tanca (k / 128, k % 128, fins);

    if (c.notes.empty())
    {
        estat.info = "No s'ha gravat cap nota"_ca;
        estat.avisa();
        return;
    }
    std::vector<int> cap;
    midi::ordena (c.notes, cap);
    c.fi = juce::jmax (c.fi, fins - cursorRec);
    midi::encabeix (p, c);
    c.inici = 0.0;   // el que s'ha tocat durant el compte queda amagat (no s'esborra)

    estat.desfes.abans (estat.projecte);
    auto& clips = estat.projecte.pistes[(size_t) pista].clips;
    clips.push_back (c);
    const int idx = (int) clips.size() - 1;
    const auto nomPista = dibuix::utf8 (estat.projecte.pistes[(size_t) pista].nom);
    const int quantes = (int) c.notes.size();
    estat.info = "Gravades "_ca + juce::String (quantes) + (quantes == 1 ? " nota" : " notes") + " a «"_ca + nomPista + "»"_ca;
    historial.afegeix (estat.info);
    estat.canviat ("Gravat MIDI ("_ca + juce::String (quantes) + " notes) a la pista «"_ca + nomPista + "»"_ca);
    obrePianoRoll (pista, idx);
}

//==============================================================================
// 0.22: Vocal Editor
//==============================================================================
void PantallaEstudi::obreVeu (int pista, int clip)
{
    auto* t = estat.pista (pista);
    auto* c = estat.clip ({ pista, clip });
    if (t == nullptr || c == nullptr || c->midi || c->presa.empty() || mibase::esMidi (*t)) return;
    if (audioPreses.find (c->presa) == audioPreses.end())
    {
        estat.info = "Aquest clip no té l'àudio carregat"_ca;
        estat.avisa();
        return;
    }
    if (! mibase::teVeu (*t))
    {
        // Es posa el Vocal Editor al primer insert de la pista (es pot desfer).
        estat.desfes.abans (estat.projecte);
        mibase::Insert x;
        x.id = "i" + std::to_string (++estat.projecte.comptador);
        x.plugin = mibase::pluginVeu;
        x.nom = "Vocal Editor";
        t->inserts.insert (t->inserts.begin(), x);
        estat.canviat ("Vocal Editor a la pista "_ca + dibuix::utf8 (t->nom));
        t = estat.pista (pista);
        c = estat.clip ({ pista, clip });
        if (t == nullptr || c == nullptr) return;
    }
    if (estat.pianoRoll) mostraPianoRoll (false);
    estat.sampler.obert = false;
    const bool mateixa = estat.veu.obert && estat.veu.presa == c->presa;
    estat.veu.obert = true;
    estat.veu.pista = pista;
    estat.veu.clip = clip;
    estat.veu.presa = c->presa;
    if (! mateixa)
    {
        estat.veu.triades.clear();
        editorVeu.novaPresa();
    }
    estat.triaClip (pista, clip);
    colocaSeccions();
    sincronitzaVeu();
    estat.avisa();
    editorVeu.grabKeyboardFocus();
}

void PantallaEstudi::obreVeuDePista (int pista)
{
    auto* t = estat.pista (pista);
    if (t == nullptr) return;
    for (auto& r : estat.triats)
        if (r.pista == pista)
            if (auto* c = estat.clip (r); c != nullptr && ! c->presa.empty()) { obreVeu (pista, r.clip); return; }
    for (int k = 0; k < (int) t->clips.size(); ++k)
        if (! t->clips[(size_t) k].presa.empty()) { obreVeu (pista, k); return; }
    estat.info = "Aquesta pista encara no té cap gravació per editar"_ca;
    estat.avisa();
}

void PantallaEstudi::tancaVeu()
{
    estat.veu.obert = false;
    colocaSeccions();
    estat.avisa();
    grabKeyboardFocus();
}

void PantallaEstudi::commutaVeuAB()
{
    auto* t = estat.pista (estat.veu.pista);
    if (t == nullptr) return;
    for (int k = 0; k < (int) t->inserts.size(); ++k)
        if (mibase::esInsertVeu (t->inserts[(size_t) k]))
        {
            inserts::commuta (estat, &plugins, estat.veu.pista, k);
            estat.info = mibase::veuActiva (estat.projecte.pistes[(size_t) estat.veu.pista]) ? "Sona la veu corregida"_ca : "Sona la veu original"_ca;
            estat.avisa();
            return;
        }
}

void PantallaEstudi::sincronitzaVeu()
{
    for (auto& t : estat.projecte.pistes)
    {
        if (mibase::esMidi (t) || ! mibase::teVeu (t)) continue;
        for (auto& c : t.clips)
        {
            if (c.presa.empty()) continue;
            auto* s = mibase::edicio::presa (estat.projecte, c.presa);
            auto it = audioPreses.find (c.presa);
            if (s == nullptr || it == audioPreses.end()) continue;
            // Només les preses que ja s'han obert al Vocal Editor (o la que s'hi obre ara).
            if (! s->veu.analitzada && ! (estat.veu.obert && estat.veu.presa == s->id)) continue;
            serveiVeu.actualitza (s->id, it->second, juce::File (fitxerPresa (*s).getFullPathName() + ".veu"), s->veu);
        }
    }
}

void PantallaEstudi::recullVeu()
{
    std::vector<std::string> fetes;
    if (! serveiVeu.recull (fetes)) return;
    bool notesNoves = false;
    for (auto& id : fetes)
    {
        auto* s = mibase::edicio::presa (estat.projecte, id);
        auto a = serveiVeu.analisi (id);
        if (s == nullptr || a == nullptr || s->veu.analitzada) continue;
        s->veu.notes = mibase::veu::detectaNotes (*a);
        s->veu.analitzada = true;
        historial.afegeix ("Vocal Editor: "_ca + juce::String ((int) s->veu.notes.size()) + " notes a "_ca + dibuix::utf8 (s->nom));
        notesNoves = true;
    }
    if (notesNoves) estat.canviat ("Vocal Editor: notes detectades"_ca);
    else publicaPla();
    editorVeu.repaint();
}

//==============================================================================
// 0.24: solo dels editors
//==============================================================================
std::string PantallaEstudi::pistaEditor() const
{
    if (estat.veu.obert && estat.veu.pista >= 0 && estat.veu.pista < (int) estat.projecte.pistes.size())
        return estat.projecte.pistes[(size_t) estat.veu.pista].id;
    if (estat.pianoRoll && estat.editorActiu >= 0 && estat.editorActiu < (int) estat.editors.size())
        return estat.editors[(size_t) estat.editorActiu].idPista;
    return {};
}

void PantallaEstudi::restauraSolos()
{
    for (auto& [id, solo] : estat.soloEditor.abans)
        for (auto& t : estat.projecte.pistes)
            if (t.id == id) t.solo = solo;
    estat.soloEditor = {};
}

void PantallaEstudi::commutaSoloEditor()
{
    auto& s = estat.soloEditor;
    if (s.actiu)
    {
        restauraSolos();
        estat.info = "Solo de l'editor apagat: les pistes tornen com estaven"_ca;
        estat.avisa();
        return;
    }
    const auto id = pistaEditor();
    if (id.empty())
    {
        estat.info = "Obre un editor (Piano Roll, Drumpad o Vocal Editor) per fer-ne solo"_ca;
        estat.avisa();
        return;
    }
    s.abans.clear();
    for (auto& t : estat.projecte.pistes) s.abans.push_back ({ t.id, t.solo });
    for (auto& t : estat.projecte.pistes) t.solo = t.id == id;
    s.pista = id;
    s.actiu = true;
    for (auto& t : estat.projecte.pistes) if (t.id == id) estat.info = "Solo de l'editor: "_ca + dibuix::utf8 (t.nom);
    estat.avisa();
}

void PantallaEstudi::seguixSoloEditor()
{
    auto& s = estat.soloEditor;
    if (! s.actiu) return;
    const auto id = pistaEditor();
    if (id.empty()) { restauraSolos(); return; }   // l'editor s'ha tancat
    if (id == s.pista) return;
    for (auto& t : estat.projecte.pistes) t.solo = t.id == id;   // l'editor ha canviat de pista: el solo la segueix
    s.pista = id;
}

//==============================================================================
// 0.26: samplejador
//==============================================================================
void PantallaEstudi::obreSampler (int pista)
{
    auto* t = estat.pista (pista);
    if (t == nullptr || ! mibase::esSampler (*t)) return;
    if (estat.pianoRoll) mostraPianoRoll (false);
    estat.veu.obert = false;
    if (estat.sampler.pista != t->id) { estat.sampler.pxPerSeg = 0.0; estat.sampler.iniciVista = 0.0; }
    estat.sampler.obert = true;
    estat.sampler.pista = t->id;
    estat.pistaTriada = pista;
    colocaSeccions();
    estat.avisa();
    editorSampler.grabKeyboardFocus();
}

void PantallaEstudi::tancaSampler()
{
    estat.sampler.obert = false;
    colocaSeccions();
    estat.avisa();
    grabKeyboardFocus();
}

void PantallaEstudi::demanaSample()
{
    const auto id = estat.sampler.pista;
    selector = std::make_unique<juce::FileChooser> ("Carrega un sample"_ca, juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                    "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    selector->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                           [this, id] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f.existsAsFile()) carregaSample (id, f);
    });
}

void PantallaEstudi::carregaSample (const std::string& idPista, const juce::File& origen)
{
    mibase::Pista* t = nullptr;
    for (auto& q : estat.projecte.pistes) if (q.id == idPista && mibase::esSampler (q)) t = &q;
    if (t == nullptr) return;
    // Es copia a «Samples» dins del projecte (els que no són WAV es converteixen a WAV de 24 bits). L'original no es toca.
    const auto carpetaSamples = estat.carpeta.getChildFile ("Samples");
    carpetaSamples.createDirectory();
    const auto base = juce::File::createLegalFileName (origen.getFileNameWithoutExtension());
    auto desti = carpetaSamples.getChildFile (base + ".wav");
    for (int k = 2; desti.existsAsFile(); ++k)
    {
        if (origen.hasFileExtension ("wav") && desti.getSize() == origen.getSize()) break;   // ja hi és
        desti = carpetaSamples.getChildFile (base + " " + juce::String (k) + ".wav");
    }
    juce::String error;
    if (! desti.existsAsFile())
    {
        if (origen.hasFileExtension ("wav"))
        {
            if (! origen.copyFileTo (desti)) error = "No s'ha pogut copiar el fitxer"_ca;
        }
        else
        {
            std::unique_ptr<juce::AudioFormatReader> r (kits::formats().createReaderFor (origen));
            if (r == nullptr) error = "No sé llegir aquest fitxer"_ca;
            else
            {
                juce::WavAudioFormat wav;
                std::unique_ptr<juce::OutputStream> sortida = desti.createOutputStream();
                auto w = sortida != nullptr ? wav.createWriterFor (sortida, juce::AudioFormatWriterOptions{}
                                                                       .withSampleRate (r->sampleRate)
                                                                       .withNumChannels ((int) juce::jmin (2u, r->numChannels))
                                                                       .withBitsPerSample (24))
                                            : nullptr;
                if (w == nullptr || ! w->writeFromAudioReader (*r, 0, -1)) error = "No s'ha pogut escriure el WAV"_ca;
            }
        }
    }
    if (error.isNotEmpty() || Instruments::mostraSample (desti) == nullptr)
    {
        if (error.isEmpty()) error = "El fitxer no té àudio o no es pot llegir"_ca;
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "No s'ha pogut carregar el sample"_ca, error + "\n" + origen.getFullPathName());
        return;
    }
    estat.desfes.abans (estat.projecte);
    auto& p = t->sampler;
    p.fitxer = ("Samples/" + desti.getFileName()).toStdString();
    p.nom = desti.getFileNameWithoutExtension().toStdString();
    p.inici = 0.0; p.fi = -1.0;
    p.bucleDe = p.bucleA = -1.0;
    p.fadeIn = 0.0;
    p.bpm = 0.0;
    p.speed = 1.0;
    p.reversa = false;
    estat.sampler.pxPerSeg = 0.0;
    // 0.26.1: la nota del sample es busca sola i es posa com a Root (així, aquella tecla el toca tal qual).
    const int root = mibase::sampler::detectaRoot (*Instruments::mostraSample (desti));
    p.root = root >= 0 ? root : 60;
    estat.info = "Sample carregat: "_ca + desti.getFileName()
                 + (root >= 0 ? ". És un "_ca + EditorSampler::nomNota (root) + ": aquesta tecla el toca tal qual"_ca
                              : ". No té una nota clara: el toca la C4"_ca);
    estat.canviat ("Samplejador: «"_ca + desti.getFileName() + "» a la pista «"_ca + dibuix::utf8 (t->nom) + "»"_ca);
}
