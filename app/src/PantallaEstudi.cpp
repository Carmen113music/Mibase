#include "PantallaEstudi.h"

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

    ampleEsquerra = ajustos.getIntValue ("estudi.ampleEsquerra", 132);
    ampleNoms = ajustos.getIntValue ("estudi.ampleNoms", 170);
    ampleFaders = ajustos.getIntValue ("estudi.ampleFaders", 150);
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

    for (auto* b : { &btClic, &btClicMenu, &btTempo, &btDesfes, &btRefes })
        addAndMakeVisible (b);
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
    for (auto* sec : std::initializer_list<SeccioEstudi*> { &esquerra, &capcaleres, &faders, &regla, &pistes, &mescles, &panell })
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
    panell.onDuplica = [this] { duplica(); };
    panell.onTreu = [this] { treu(); };

    for (auto* d : { &divEsquerra, &divNoms, &divFaders, &divPanell })
    {
        addAndMakeVisible (d);
        d->setTooltip ("Arrossega per fer més ample o més estret. Doble clic: mida inicial."_ca);
        d->onAcaba = [this]
        {
            ajustos.setValue ("estudi.ampleEsquerra", ampleEsquerra);
            ajustos.setValue ("estudi.ampleNoms", ampleNoms);
            ajustos.setValue ("estudi.ampleFaders", ampleFaders);
            ajustos.setValue ("estudi.amplePanell", amplePanell);
            ajustos.saveIfNeeded();
        };
    }
    divEsquerra.onComenca = [this] { ampleInici = esquerra.getWidth(); };
    divEsquerra.onArrossega = [this] (int dx) { ampleEsquerra = juce::jlimit (110, 260, ampleInici + dx); colocaSeccions(); };
    divEsquerra.onDobleClic = [this] { ampleEsquerra = 132; colocaSeccions(); divEsquerra.onAcaba(); };
    divNoms.onComenca = [this] { ampleInici = capcaleres.getWidth(); };
    divNoms.onArrossega = [this] (int dx) { ampleNoms = juce::jmax (120, ampleInici + dx); colocaSeccions(); };
    divNoms.onDobleClic = [this] { ampleNoms = 170; colocaSeccions(); divNoms.onAcaba(); };
    divFaders.onComenca = [this] { ampleInici = faders.getWidth(); };
    divFaders.onArrossega = [this] (int dx) { ampleFaders = juce::jmax (90, ampleInici + dx); colocaSeccions(); };
    divFaders.onDobleClic = [this] { ampleFaders = 150; colocaSeccions(); divFaders.onAcaba(); };
    divPanell.onComenca = [this] { ampleInici = panell.getWidth(); };
    divPanell.onArrossega = [this] (int dx) { amplePanell = juce::jmax (200, ampleInici - dx); colocaSeccions(); };
    divPanell.onDobleClic = [this] { amplePanell = 270; colocaSeccions(); divPanell.onAcaba(); };

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
    previsualitzaClic (false);
    estat.removeChangeListener (this);
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
    const auto& p = estat.projecte;
    btTempo.setButtonText (juce::String ((int) std::round (p.tempo)) + " bpm · "_ca + juce::String (p.compasNum) + "/" + juce::String (p.compasDen));
    btClic.setToggleState (p.clic, juce::dontSendNotification);
    btDesfes.setEnabled (estat.desfes.potDesfer());
    btRefes.setEnabled (estat.desfes.potRefer());
    for (int i = 0; i < eines.size(); ++i)
        eines[i]->setToggleState ((int) estat.eina == i, juce::dontSendNotification);
    if (snap.getTriat() != (int) p.snap) snap.setTriat ((int) p.snap);

    const bool mesclesVisible = mescles.isVisible();
    if (mesclesVisible != estat.vistaMescles)
        colocaSeccions();
    actualitzaBarres();
    repaint (zonaBarra);
}

void PantallaEstudi::triaEina (Eina e)
{
    estat.eina = e;
    if (e != Eina::Fletxa && estat.clipsTriats.size() > 1) estat.clipsTriats.clear();
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
                          + " · Ctrl+roda: zoom · Alt+roda: alçada"_ca;
        g.drawFittedText (text, zonaInfo, juce::Justification::centredLeft, 1, 0.8f);
    }

    if (! estat.vistaMescles)
    {
        auto titol = [&] (juce::Rectangle<int> r, const juce::String& t)
        {
            vintage::panell (g, r.toFloat());
            g.setColour (tema::linia);
            g.fillRect (r.getRight() - 1, r.getY(), 1, r.getHeight());
            dibuix::pintaEtiquetaSeccio (g, t, r.withTrimmedLeft (14));
        };
        titol (zonaTitolNoms, "Pistes");
        titol (zonaTitolFaders, "Volum");
    }
}

void PantallaEstudi::colocaSeccions()
{
    auto r = getLocalBounds().withTrimmedTop (alcadaBarra);
    const bool m = estat.vistaMescles;

    int esq = ampleEsquerra, noms = ampleNoms, fad = ampleFaders, pan = amplePanell;
    int sobra = esq + (m ? 0 : noms + fad) + pan + 320 - r.getWidth();
    auto encongeix = [&sobra] (int& v, int minim) { if (sobra > 0) { const int t = juce::jmin (sobra, v - minim); if (t > 0) { v -= t; sobra -= t; } } };
    encongeix (pan, 200);
    if (! m) { encongeix (fad, 90); encongeix (noms, 120); }
    encongeix (esq, 110);

    esquerra.setBounds (r.removeFromLeft (esq));
    panell.setBounds (r.removeFromRight (pan));

    mescles.setVisible (m);
    for (auto* c : std::initializer_list<juce::Component*> { &capcaleres, &faders, &regla, &pistes, &barraH, &barraV, &divNoms, &divFaders })
        c->setVisible (! m);

    if (m)
    {
        mescles.setBounds (r);
        zonaTitolNoms = zonaTitolFaders = {};
    }
    else
    {
        auto colNoms = r.removeFromLeft (noms);
        auto colFaders = r.removeFromLeft (fad);
        auto temps = r;
        zonaTitolNoms = colNoms.removeFromTop (alcadaRegla);
        zonaTitolFaders = colFaders.removeFromTop (alcadaRegla);
        colNoms.removeFromBottom (gruixBarres);
        colFaders.removeFromBottom (gruixBarres);
        capcaleres.setBounds (colNoms);
        faders.setBounds (colFaders);
        regla.setBounds (temps.removeFromTop (alcadaRegla));
        barraH.setBounds (temps.removeFromBottom (gruixBarres).withTrimmedRight (gruixBarres));
        barraV.setBounds (temps.removeFromRight (gruixBarres));
        pistes.setBounds (temps);
        divNoms.setBounds (capcaleres.getRight() - 4, alcadaBarra, 8, getHeight() - alcadaBarra);
        divFaders.setBounds (faders.getRight() - 4, alcadaBarra, 8, getHeight() - alcadaBarra);
    }

    divEsquerra.setBounds (esquerra.getRight() - 4, alcadaBarra, 8, getHeight() - alcadaBarra);
    divPanell.setBounds (panell.getX() - 4, alcadaBarra, 8, getHeight() - alcadaBarra);
    for (auto* d : { &divEsquerra, &divNoms, &divFaders, &divPanell }) d->toFront (false);

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
    barraH.setRangeLimits (juce::jmin (0.0, estat.iniciVista), total, juce::dontSendNotification);
    barraH.setCurrentRange (estat.iniciVista, visibles, juce::dontSendNotification);

    const int contingut = (estat.numPistes() + 1) * estat.altPista + 60;
    const int maxim = juce::jmax (0, contingut - pistes.getHeight());
    if (estat.desplY > maxim) estat.desplY = maxim;
    barraV.setRangeLimits (0.0, juce::jmax (contingut, pistes.getHeight()), juce::dontSendNotification);
    barraV.setCurrentRange (estat.desplY, pistes.getHeight(), juce::dontSendNotification);
}

void PantallaEstudi::scrollBarMoved (juce::ScrollBar* barra, double inici)
{
    if (barra == &barraH) estat.iniciVista = inici;
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
        estat.iniciVista = juce::jmax (-meitat, anclaZoom - meitat);
    }
    else if (e.mods.isAltDown())
    {
        estat.altPista = juce::jlimit (VistaPistes::alcadaMinima, 260, estat.altPista + juce::roundToInt (d * 60.0f));
    }
    else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
    {
        estat.iniciVista = juce::jmax (-(pistes.getWidth() / 2.0) / estat.pxPerSegon, estat.iniciVista - d * 300.0 / estat.pxPerSegon);
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
    const double compas = ed::segonsPerCompas (estat.projecte);
    switch (accio)
    {
        case 0: estat.capcal = 0.0; estat.iniciVista = 0.0; break;
        case 1: estat.capcal = juce::jmax (0.0, std::ceil (estat.capcal / compas - 1e-6) * compas - compas); break;
        case 2: estat.capcal = std::floor (estat.capcal / compas + 1e-6) * compas + compas; break;
        default: break;
    }
    // Que el capçal es vegi.
    const double visibles = pistes.getWidth() / estat.pxPerSegon;
    if (estat.capcal < estat.iniciVista || estat.capcal > estat.iniciVista + visibles * 0.9)
        estat.iniciVista = juce::jmax (0.0, estat.capcal - visibles * 0.25);
    estat.avisa();
}

std::pair<juce::String, juce::String> PantallaEstudi::textPosicio() const
{
    const double t = estat.capcal;
    const int min = (int) (t / 60.0);
    const double seg = t - min * 60.0;
    return { dibuix::utf8 (ed::textPosicio (estat.projecte, t)),
             juce::String::formatted ("%02d:%06.3f", min, seg) };
}

void PantallaEstudi::pistaNova (const juce::String& tipus, int canals)
{
    estat.desfes.abans (estat.projecte);
    const auto nom = ed::nomPistaLliure (estat.projecte, (canals == 2 ? juce::String::fromUTF8 ("Àudio estèreo") : juce::String::fromUTF8 ("Àudio")).toStdString());
    auto& t = ed::afegeixPista (estat.projecte, nom, tipus.toStdString());
    t.canals = canals;
    estat.pistaTriada = estat.numPistes() - 1;
    estat.clipsTriats.clear();
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
    dialeg = std::make_unique<DialegText> ("Tempo", "En bpm (de 30 a 300). Els clips no es mouen."_ca, "Canvia'l"_ca,
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
            estat.projecte.tempo = bpm;
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
        return;
    }

    auto& lector = *ona->lector;
    ona->sr = lector.sampleRate;
    ona->longitud = lector.lengthInSamples;
    ona->canals = (int) lector.numChannels;
    const auto blocs = (size_t) ((ona->longitud + OnaPresa::bloc - 1) / OnaPresa::bloc);
    ona->mins.assign (blocs, 0.0f);
    ona->maxs.assign (blocs, 0.0f);

    const int tros = OnaPresa::bloc * 512;
    juce::AudioBuffer<float> buffer ((int) juce::jmax (1u, lector.numChannels), tros);
    const float factor = 1.0f / (float) buffer.getNumChannels();
    for (juce::int64 pos = 0; pos < ona->longitud; pos += tros)
    {
        const int n = (int) juce::jmin ((juce::int64) tros, ona->longitud - pos);
        lector.read (&buffer, 0, n, pos, true, true);
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
        estat.clipsTriats.clear();
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
    estat.pistaTriada = pista;
    estat.clipsTriats = { (int) estat.projecte.pistes[(size_t) pista].clips.size() - 1 };
    estat.info = "Enganxats en un fitxer nou: "_ca + nomDesti;
    estat.canviat ("Enganxats dos clips a «"_ca + dibuix::utf8 (estat.projecte.pistes[(size_t) pista].nom) + "» en un fitxer nou ("_ca + nomDesti + ")");
}

//==============================================================================
void PantallaEstudi::duplica()
{
    if (estat.pista (estat.pistaTriada) == nullptr || estat.clipsTriats.empty()) return;
    estat.desfes.abans (estat.projecte);
    estat.clipsTriats = ed::duplica (estat.projecte, estat.pistaTriada, estat.clipsTriats);
    estat.canviat();
}

void PantallaEstudi::treu()
{
    if (estat.pista (estat.pistaTriada) == nullptr || estat.clipsTriats.empty()) return;
    estat.desfes.abans (estat.projecte);
    ed::treuClips (estat.projecte, estat.pistaTriada, estat.clipsTriats);
    estat.clipsTriats.clear();
    estat.canviat();
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

    if (codi == juce::KeyPress::deleteKey || codi == juce::KeyPress::backspaceKey) { treu(); return true; }
    if (mods.isCommandDown() && (codi == 'D' || codi == 'd')) { duplica(); return true; }
    if (mods.isCommandDown() && (codi == 'Z' || codi == 'z')) { desfes (mods.isShiftDown()); return true; }
    if (mods.isCommandDown() && (codi == 'Y' || codi == 'y')) { desfes (true); return true; }
    if (mods.isCommandDown() && (codi == 'S' || codi == 's')) { desaAra(); estat.info = "Desat"; estat.avisa(); return true; }
    if (mods.isCommandDown() && (codi == 'I' || codi == 'i')) { importaDeMenu(); return true; }
    if (! mods.isAnyModifierKeyDown() && codi >= '1' && codi <= '6') { triaEina ((Eina) (codi - '1')); return true; }
    if (codi == juce::KeyPress::escapeKey) { estat.clipsTriats.clear(); triaEina (Eina::Fletxa); return true; }
    if (codi == juce::KeyPress::homeKey) { transport (0); return true; }
    if (codi == juce::KeyPress::spaceKey)
    {
        estat.info = "Reproduir arriba a la versió 0.5"_ca;
        estat.avisa();
        return true;
    }
    return false;
}
