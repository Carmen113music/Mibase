#include "FinestraProjecteNou.h"

namespace
{
    constexpr int ampleMaxim = 980;
}

FinestraProjecteNou::FinestraProjecteNou (const juce::File& carpetaMare) : carpeta (carpetaMare)
{
    setWantsKeyboardFocus (true);

    nom.setFont (dibuix::lletra (16.0f));
    nom.setTextToShowWhenEmpty ("Per exemple: Tornada amb slide"_ca, tema::suau);
    nom.setIndents (14, 14);
    nom.setColour (juce::TextEditor::backgroundColourId, tema::fons);
    nom.setColour (juce::TextEditor::focusedOutlineColourId, tema::accent);
    nom.setColour (juce::TextEditor::outlineColourId, tema::linia2);
    nom.addListener (this);
    addAndMakeVisible (nom);

    addAndMakeVisible (btTanca);
    btTanca.setTitle ("Tanca");
    btTanca.onClick = [this] { if (onTanca) onTanca(); };

    addAndMakeVisible (btCanvia);
    btCanvia.setTooltip ("Tanca aquesta finestra per triar una altra carpeta a l'esquerra"_ca);
    btCanvia.onClick = [this] { if (onTanca) onTanca(); };

    for (auto* b : { &btMenys, &btMes })
    {
        addAndMakeVisible (b);
        b->setMidaText (20.0f);
        b->setRepeatSpeed (400, 60);
    }
    btMenys.setTitle ("Més lent"_ca);
    btMes.setTitle ("Més ràpid"_ca);
    btMenys.onClick = [this] { canviaTempo (-1); };
    btMes.onClick = [this] { canviaTempo (1); };

    addAndMakeVisible (compas);
    compas.setTriat (2);

    addAndMakeVisible (btCancella);
    btCancella.onClick = [this] { if (onTanca) onTanca(); };
    addAndMakeVisible (btCrea);
    btCrea.onClick = [this] { crea(); };

    valida();
}

void FinestraProjecteNou::canviaTempo (int delta)
{
    tempo = juce::jlimit (40, 240, tempo + delta);
    repaint (zonaTempo);
}

void FinestraProjecteNou::valida()
{
    const auto text = nom.getText();
    const auto v = mibase::disc::validaNom (text.toStdString());

    if (! v.correcte)
    {
        avis = dibuix::utf8 (v.missatge);
        error = text.isNotEmpty();   // buit: encara no és un error, però no es pot crear
        btCrea.setEnabled (false);
    }
    else if (carpeta.getChildFile (text).exists())
    {
        avis = "Aquí ja hi ha una carpeta que es diu així."_ca;
        error = true;
        btCrea.setEnabled (false);
    }
    else
    {
        avis = "Es crearà una carpeta amb aquest nom."_ca;
        error = false;
        btCrea.setEnabled (true);
    }

    nom.setColour (juce::TextEditor::outlineColourId, error ? tema::error : tema::linia2);
    repaint();
}

void FinestraProjecteNou::setError (const juce::String& e)
{
    avis = e;
    error = true;
    repaint();
}

void FinestraProjecteNou::crea()
{
    if (! btCrea.isEnabled())
        return;

    static const int nums[] = { 2, 3, 4, 6 };
    static const int dens[] = { 4, 4, 4, 8 };

    mibase::Projecte p;
    p.nom = nom.getText().toStdString();
    p.tempo = tempo;
    p.compasNum = nums[compas.getTriat()];
    p.compasDen = dens[compas.getTriat()];
    p.plantilla = "buit";
    if (onCrea) onCrea (p);
}

bool FinestraProjecteNou::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { if (onTanca) onTanca(); return true; }
    if (k == juce::KeyPress::returnKey) { crea(); return true; }
    return true;   // la finestra és modal: les tecles no passen a sota
}

void FinestraProjecteNou::mouseDown (const juce::MouseEvent& e)
{
    // Clicar fora del panell el tanca (com «Cancel·la»).
    if (! panell.contains (e.getPosition()) && onTanca)
        onTanca();
}

void FinestraProjecteNou::resized()
{
    auto tot = getLocalBounds();
    const int ample = juce::jmin (ampleMaxim, tot.getWidth() - 40);
    const int alt = juce::jmin (640, tot.getHeight() - 40);
    panell = tot.withSizeKeepingCentre (ample, alt);

    auto r = panell;
    auto capcalera = r.removeFromTop (68);
    btTanca.setBounds (capcalera.removeFromRight (60).withSizeKeepingCentre (36, 36));

    peu = r.removeFromBottom (76);
    {
        auto p = peu.reduced (24, 16);
        btCrea.setBounds (p.removeFromRight (btCrea.ampladaIdeal()));
        p.removeFromRight (10);
        btCancella.setBounds (p.removeFromRight (btCancella.ampladaIdeal()));
    }

    columnaDreta = r.removeFromRight (juce::jmax (300, ample * 36 / 100));
    auto e = r.reduced (24, 22);

    e.removeFromTop (22);   // etiqueta «Nom del projecte»
    nom.setBounds (e.removeFromTop (48));
    e.removeFromTop (30);   // avís

    e.removeFromTop (22);   // «On es crearà»
    zonaCami = e.removeFromTop (44);
    btCanvia.setBounds (zonaCami.withTrimmedLeft (zonaCami.getWidth() - 86).reduced (6, 6));
    e.removeFromTop (22);

    auto fila = e.removeFromTop (22 + 44);
    zonaTempo = fila.removeFromLeft (170).withTrimmedTop (22);
    btMenys.setBounds (zonaTempo.withWidth (44));
    btMes.setBounds (zonaTempo.withTrimmedLeft (zonaTempo.getWidth() - 44));
    fila.removeFromLeft (24);
    compas.setBounds (fila.withTrimmedTop (22).withWidth (compas.ampladaIdeal() + 20));
    e.removeFromTop (22);

    zonaPlantilles = e;
    auto t = zonaPlantilles.withTrimmedTop (24);
    const int ampleTargeta = (t.getWidth() - 10) / 2;
    targetaBuit = t.removeFromLeft (ampleTargeta).withHeight (juce::jmin (112, t.getHeight()));
    t.removeFromLeft (10);
    targetaBuida = t.withHeight (targetaBuit.getHeight());
}

void FinestraProjecteNou::paint (juce::Graphics& g)
{
    // Fons enfosquit.
    g.fillAll (juce::Colour (0xa0060608));

    // Panell.
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (panell.toFloat().translated (0, 12).expanded (6), 20.0f);
    g.setColour (tema::superficie);
    g.fillRoundedRectangle (panell.toFloat(), 16.0f);

    // Columna dreta, més fosca (només el tros de dins del panell).
    {
        juce::Graphics::ScopedSaveState s (g);
        juce::Path forma;
        forma.addRoundedRectangle (panell.toFloat(), 16.0f);
        g.reduceClipRegion (forma);
        g.setColour (tema::lateral);
        g.fillRect (columnaDreta);
        g.setColour (tema::linia);
        g.fillRect (columnaDreta.getX(), columnaDreta.getY(), 1, columnaDreta.getHeight());
        g.fillRect (panell.getX(), panell.getY() + 68, panell.getWidth(), 1);
        g.fillRect (panell.getX(), peu.getY(), panell.getWidth(), 1);
    }
    g.setColour (tema::linia2);
    g.drawRoundedRectangle (panell.toFloat().reduced (0.5f), 16.0f, 1.0f);

    // Títol.
    g.setColour (tema::text);
    g.setFont (dibuix::lletra (20.0f, true));
    g.drawText ("Projecte nou", panell.getX() + 24, panell.getY(), 400, 68, juce::Justification::centredLeft);

    auto etiqueta = [&] (const juce::String& t, juce::Rectangle<int> r)
    {
        g.setColour (tema::textMig);
        g.setFont (dibuix::lletra (13.0f, true));
        g.drawText (t, r, juce::Justification::topLeft);
    };

    etiqueta ("Nom del projecte", nom.getBounds().translated (0, -22).withHeight (20));

    g.setColour (error ? tema::errorText : tema::suau);
    g.setFont (dibuix::lletra (12.5f));
    g.drawText (avis, nom.getBounds().translated (0, 52).withHeight (20), juce::Justification::centredLeft);

    // On es crearà.
    etiqueta ("On es crearà"_ca, zonaCami.translated (0, -22).withHeight (20));
    g.setColour (tema::fons);
    g.fillRoundedRectangle (zonaCami.toFloat(), 10.0f);
    g.setColour (tema::linia);
    g.drawRoundedRectangle (zonaCami.toFloat().reduced (0.5f), 10.0f, 1.0f);
    dibuix::pintaIcona (g, dibuix::Icona::Carpeta, juce::Rectangle<float> ((float) zonaCami.getX() + 12, (float) zonaCami.getCentreY() - 9, 18, 18),
                        tema::accent, 1.8f, tema::accent.withAlpha (0.18f));
    g.setColour (tema::text);
    g.setFont (dibuix::mono (12.5f));
    g.drawFittedText (carpeta.getFullPathName(), zonaCami.withTrimmedLeft (40).withTrimmedRight (96), juce::Justification::centredLeft, 1, 0.7f);

    // Tempo.
    etiqueta ("Tempo", zonaTempo.translated (0, -22).withHeight (20));
    g.setColour (tema::fons);
    g.fillRoundedRectangle (zonaTempo.toFloat(), 10.0f);
    g.setColour (tema::linia);
    g.drawRoundedRectangle (zonaTempo.toFloat().reduced (0.5f), 10.0f, 1.0f);
    g.setColour (tema::text);
    g.setFont (dibuix::mono (15.0f));
    g.drawText (juce::String (tempo) + " bpm", zonaTempo, juce::Justification::centred);

    etiqueta ("Compàs"_ca, compas.getBounds().translated (0, -22).withHeight (20));

    // Amb què es comença.
    etiqueta ("Comença amb"_ca, zonaPlantilles.withHeight (20));
    {
        auto r = targetaBuit.toFloat();
        g.setColour (tema::triat);
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (tema::accent.withAlpha (0.15f));
        g.drawRoundedRectangle (r.expanded (2.0f), 14.0f, 3.0f);
        g.setColour (tema::accent);
        g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);

        auto t = targetaBuit.reduced (14, 12);
        g.setColour (tema::linia2);
        g.fillRoundedRectangle (juce::Rectangle<float> ((float) t.getX(), (float) t.getY() + 8, 8, 14), 2.0f);
        t.removeFromTop (30);
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (14.0f, true));
        g.drawText ("Projecte buit", t.removeFromTop (20), juce::Justification::centredLeft);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (12.0f));
        g.drawFittedText ("Sense pistes. Comences amb «+ Pista nova»."_ca, t, juce::Justification::topLeft, 2);
    }
    {
        auto r = targetaBuida.toFloat().reduced (0.5f);
        juce::Path p;
        p.addRoundedRectangle (r, 12.0f);
        juce::Path discontinu;
        const float guions[] = { 4.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (discontinu, p, guions, 2);
        g.setColour (tema::linia2);
        g.fillPath (discontinu);

        auto t = targetaBuida.reduced (14, 14);
        g.setColour (tema::textMig);
        g.setFont (dibuix::lletra (13.0f, true));
        g.drawText ("Encara no tens plantilles", t.removeFromTop (20), juce::Justification::centredLeft);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (12.0f));
        g.drawFittedText ("Des de l'estudi podràs desar un projecte com a plantilla (pistes, sons, busos i efectes)."_ca,
                          t, juce::Justification::topLeft, 3);
    }

    // Què es crearà.
    auto d = columnaDreta.reduced (24, 22);
    dibuix::pintaEtiquetaSeccio (g, "Es crearà això"_ca, d.removeFromTop (20));
    d.removeFromTop (10);

    const auto text = nom.getText().trim();
    const juce::String nomProjecte = text.isNotEmpty() ? text : juce::String ("Nom del projecte");

    struct Element { int nivell; bool esCarpeta; juce::String nom, desc; juce::Colour color, ple; bool destacat, exemple; };
    const Element elements[] = {
        { 0, true,  carpeta.getFileName() + "\\", "La carpeta triada a l'esquerra"_ca, tema::suau, {}, false, false },
        { 1, true,  nomProjecte + "\\", "Tot el projecte en una carpeta"_ca, tema::accent, tema::accent.withAlpha (0.18f), true, false },
        { 2, false, nomProjecte + ".mibase", "El projecte: pistes, clips, tempo… Es desa sol."_ca, tema::text, {}, true, false },
        { 2, false, "Evolució.txt"_ca, "Diari del projecte, el més recent a dalt"_ca, tema::textMig, {}, false, false },
        { 2, true,  "Àudios\\"_ca, "Les gravacions, amb el nom de la pista i el número de presa"_ca, tema::guitarra, tema::guitarra.withAlpha (0.15f), false, false },
        { 3, false, "Guitarra - presa 01.wav", "Exemple: la primera gravació a «Guitarra»"_ca, tema::suau, {}, false, true },
        { 3, false, "Veu - presa 03.wav", "Exemple: la tercera presa a «Veu»"_ca, tema::suau, {}, false, true },
        { 2, true,  "Còpies\\"_ca, "Còpies automàtiques del fitxer del projecte (les 10 últimes)"_ca, tema::suau, {}, false, false },
    };

    for (auto& el : elements)
    {
        auto fila = d.removeFromTop (44);
        fila.removeFromLeft (el.nivell * 18);
        dibuix::pintaIcona (g, el.esCarpeta ? dibuix::Icona::Carpeta : dibuix::Icona::Fitxer,
                            juce::Rectangle<float> ((float) fila.getX(), (float) fila.getY() + 4, 16, 16), el.color, 1.8f, el.ple);
        fila.removeFromLeft (24);
        g.setColour (el.exemple ? tema::suau : (el.destacat ? tema::text : tema::textMig));
        g.setFont (el.exemple ? dibuix::mono (12.5f).italicised() : dibuix::mono (12.5f));
        g.drawText (el.nom, fila.removeFromTop (20), juce::Justification::centredLeft, true);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.5f));
        g.drawFittedText (el.desc, fila, juce::Justification::topLeft, 1, 0.85f);
    }

    // Peu.
    g.setColour (tema::suau);
    g.setFont (dibuix::lletra (12.5f));
    g.drawText ("El tempo i el compàs es poden canviar després."_ca, peu.reduced (24, 0).withRight (btCancella.getX() - 10),
                juce::Justification::centredLeft, true);
}
