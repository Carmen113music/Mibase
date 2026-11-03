#include "PantallaInici.h"

namespace
{

    juce::String textTempo (const mibase::Projecte& p)
    {
        return juce::String ((int) std::round (p.tempo)) + " bpm · "_ca + juce::String (p.compasNum) + "/" + juce::String (p.compasDen);
    }
}

//==============================================================================
void TargetaProjecte::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (3.0f);
    const bool sobre = isMouseOver (true);

    if (triada)
    {
        g.setColour (tema::accent.withAlpha (0.15f));
        g.drawRoundedRectangle (r.expanded (1.5f), 14.0f, 3.0f);
    }
    g.setColour (triada ? tema::triat : (sobre ? tema::sobre : tema::superficie));
    g.fillRoundedRectangle (r, 12.0f);
    g.setColour (triada ? tema::accent : (hasKeyboardFocus (false) ? tema::linia2 : tema::linia));
    g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);

    auto c = r.reduced (12.0f).toNearestInt();

    // Vista general (de moment, sense ona: encara no hi ha gravacions).
    auto ona = c.removeFromTop (54).toFloat();
    g.setColour (tema::fons);
    g.fillRoundedRectangle (ona, 8.0f);
    if (! info.existeix)
    {
        g.setColour (tema::errorText);
        g.setFont (dibuix::lletra (12.0f));
        g.drawText ("No es troba el projecte", ona, juce::Justification::centred);
    }
    else
    {
        g.setColour (tema::linia2);
        g.fillRect (ona.reduced (12.0f, 0).withHeight (1.0f).withCentre (ona.getCentre()));
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.5f));
        const auto t = info.preses == 0 ? juce::String ("Sense gravacions")
                                        : juce::String (info.preses) + (info.preses == 1 ? " presa" : " preses");
        auto etiqueta = ona.withSizeKeepingCentre (juce::jmin (ona.getWidth() - 20.0f, 130.0f), 20.0f);
        g.setColour (tema::fons);
        g.fillRect (etiqueta);
        g.setColour (tema::suau);
        g.drawText (t, etiqueta, juce::Justification::centred);
    }
    c.removeFromTop (10);

    auto fila = c.removeFromTop (20);
    g.setFont (dibuix::lletra (12.0f));
    g.setColour (tema::suau);
    g.drawText (info.quan, fila.removeFromRight (110), juce::Justification::centredRight);
    g.setColour (tema::text);
    g.setFont (dibuix::lletra (15.0f, true));
    g.drawText (info.nom, fila, juce::Justification::centredLeft, true);

    c.removeFromTop (4);
    g.setColour (tema::suau);
    g.setFont (dibuix::lletra (12.0f));
    g.drawText (info.lloc, c.removeFromTop (18), juce::Justification::centredLeft, true);

    c.removeFromTop (8);
    auto xips = c.removeFromTop (22);
    {
        const juce::String t = info.pistes == 0 ? juce::String ("Sense pistes") : juce::String (info.pistes) + " pistes";
        auto xip = xips.removeFromLeft (juce::jmin (xips.getWidth(), 22 + (int) t.length() * 7)).toFloat();
        g.setColour (tema::superficie2);
        g.fillRoundedRectangle (xip, 6.0f);
        g.setColour (tema::linia2);
        g.fillEllipse (xip.getX() + 8.0f, xip.getCentreY() - 3.5f, 7.0f, 7.0f);
        g.setColour (tema::textMig);
        g.setFont (dibuix::lletra (11.5f));
        g.drawText (t, xip.withTrimmedLeft (20.0f), juce::Justification::centredLeft);
    }

    c.removeFromTop (8);
    g.setColour (tema::textMig);
    g.setFont (dibuix::mono (12.0f));
    g.drawText (info.tempo, c.removeFromTop (18), juce::Justification::centredLeft, true);
}

//==============================================================================
PanellDetalls::PanellDetalls()
{
    addAndMakeVisible (btObre);
    addAndMakeVisible (btMostra);
    addChildComponent (btTreu);
    btObre.setMidaText (15.0f);
    btObre.onClick = [this] { if (onObre) onObre(); };
    btMostra.onClick = [this] { if (info) (info->existeix ? info->fitxer : info->carpeta.getParentDirectory()).revealToUser(); };
    btTreu.onClick = [this] { if (onTreu) onTreu(); };
}

void PanellDetalls::setInfo (const InfoProjecte* i)
{
    if (i) info = *i; else info.reset();
    btObre.setVisible (info && info->existeix);
    btMostra.setVisible (info.has_value());
    btTreu.setVisible (info && ! info->existeix);
    resized();
    repaint();
}

void PanellDetalls::resized()
{
    auto r = getLocalBounds().reduced (22, 24);
    btMostra.setBounds (r.removeFromBottom (38));
    r.removeFromTop (110);
    btObre.setBounds (r.removeFromTop (48));
    btTreu.setBounds (btObre.getBounds());
    r.removeFromTop (16);
    zonaStats = r.removeFromTop (64);
    r.removeFromTop (20);
    zonaEvolucio = r.withTrimmedBottom (12);
}

void PanellDetalls::paint (juce::Graphics& g)
{
    g.fillAll (tema::lateral);
    auto r = getLocalBounds().reduced (22, 24);

    if (! info)
    {
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (14.0f));
        g.drawFittedText ("Tria un projecte per veure'n els detalls."_ca, r.removeFromTop (60), juce::Justification::topLeft, 3);
        return;
    }

    dibuix::pintaEtiquetaSeccio (g, "Projecte triat", r.removeFromTop (18));
    r.removeFromTop (8);
    g.setColour (tema::text);
    g.setFont (dibuix::lletra (21.0f, true));
    g.drawText (info->nom, r.removeFromTop (30), juce::Justification::centredLeft, true);
    g.setColour (tema::suau);
    g.setFont (dibuix::mono (12.0f));
    g.drawFittedText (info->carpeta.getFullPathName(), r.removeFromTop (48), juce::Justification::topLeft, 3, 0.8f);

    if (! info->existeix)
    {
        g.setColour (tema::errorText);
        g.setFont (dibuix::lletra (13.0f));
        g.drawFittedText ("No es troba el fitxer del projecte. Potser s'ha mogut, s'ha canviat de nom o és en un disc desconnectat."_ca,
                          zonaStats.withHeight (80), juce::Justification::topLeft, 4);
        return;
    }

    // Tres caixetes.
    const juce::String valors[] = { juce::String (info->pistes), juce::String (info->preses), info->tempo.upToFirstOccurrenceOf (" ", false, false) };
    const char* noms[] = { "pistes", "preses", "bpm" };
    const int ample = (zonaStats.getWidth() - 16) / 3;
    for (int i = 0; i < 3; ++i)
    {
        auto c = juce::Rectangle<int> (zonaStats.getX() + i * (ample + 8), zonaStats.getY(), ample, zonaStats.getHeight());
        g.setColour (tema::superficie);
        g.fillRoundedRectangle (c.toFloat(), 10.0f);
        g.setColour (tema::linia);
        g.drawRoundedRectangle (c.toFloat().reduced (0.5f), 10.0f, 1.0f);
        auto t = c.reduced (12, 10);
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (19.0f, true));
        g.drawText (valors[i], t.removeFromTop (26), juce::Justification::centredLeft);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.5f));
        g.drawText (noms[i], t, juce::Justification::centredLeft);
    }

    // Evolució.
    auto e = zonaEvolucio;
    auto titol = e.removeFromTop (20);
    dibuix::pintaIcona (g, dibuix::Icona::Rellotge, titol.removeFromLeft (16).toFloat().withSizeKeepingCentre (15, 15), tema::suau, 2.0f);
    titol.removeFromLeft (8);
    dibuix::pintaEtiquetaSeccio (g, "Evolució del projecte"_ca, titol);
    e.removeFromTop (6);

    if (info->evolucio.empty())
    {
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (13.0f));
        g.drawText ("Encara no hi ha res anotat.", e.removeFromTop (30), juce::Justification::centredLeft);
    }

    for (const auto& entrada : info->evolucio)
    {
        if (e.getHeight() < 50) break;
        auto fila = e.removeFromTop (52);
        const auto text = dibuix::utf8 (entrada.text);
        const auto color = text.startsWith ("Projecte creat") ? tema::sinte
                         : text.startsWith ("Gravada") ? tema::guitarra : tema::suau;
        g.setColour (color);
        g.fillEllipse ((float) fila.getX(), (float) fila.getY() + 11.0f, 8.0f, 8.0f);
        auto t = fila.withTrimmedLeft (18).reduced (0, 6);
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (13.0f));
        g.drawFittedText (text, t.removeFromTop (20), juce::Justification::centredLeft, 1, 0.85f);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.5f));
        g.drawText (dibuix::utf8 (entrada.quan), t, juce::Justification::topLeft);
        g.setColour (tema::superficie2);
        g.fillRect (fila.getX(), fila.getBottom() - 1, fila.getWidth(), 1);
    }
}

//==============================================================================
PantallaInici::PantallaInici (juce::PropertiesFile& a, Recents& r, Historial& h)
    : ajustos (a), recents (r), historial (h), navegador (a)
{
    ampleEsq = ajustos.getIntValue ("inici.ampleEsq", 300);
    ampleDre = ajustos.getIntValue ("inici.ampleDre", 340);

    addAndMakeVisible (navegador);
    navegador.onCanvi = [this] { if (filtre.getTriat() == 1) ompleTargetes(); };

    for (auto* d : { &divisorEsq, &divisorDre })
    {
        addAndMakeVisible (d);
        d->setTooltip ("Arrossega per fer més ample o més estret. Doble clic: mida inicial."_ca);
        d->onAcaba = [this]
        {
            ajustos.setValue ("inici.ampleEsq", ampleEsq);
            ajustos.setValue ("inici.ampleDre", ampleDre);
            ajustos.saveIfNeeded();
        };
    }
    // En començar, es parteix de l'amplada que es veu (pot ser més petita que la desada si la finestra és estreta).
    divisorEsq.onComenca = [this] { ampleInici = navegador.getWidth(); ampleDre = detalls.getWidth(); };
    divisorEsq.onArrossega = [this] (int dx) { ampleEsq = juce::jlimit (220, juce::jmax (220, getWidth() - ampleDre - 560), ampleInici + dx); resized(); repaint(); };
    divisorEsq.onDobleClic = [this] { ampleEsq = 300; resized(); repaint(); divisorEsq.onAcaba(); };
    divisorDre.onComenca = [this] { ampleInici = detalls.getWidth(); ampleEsq = navegador.getWidth(); };
    divisorDre.onArrossega = [this] (int dx) { ampleDre = juce::jlimit (260, juce::jmax (260, getWidth() - ampleEsq - 560), ampleInici - dx); resized(); repaint(); };
    divisorDre.onDobleClic = [this] { ampleDre = 340; resized(); repaint(); divisorDre.onAcaba(); };

    addAndMakeVisible (btAltre);
    btAltre.onClick = [this] { obreAltre(); };
    addAndMakeVisible (btNou);
    btNou.setMidaText (14.5f);
    btNou.onClick = [this] { obreNou(); };

    cerca.setTextToShowWhenEmpty ("Busca un projecte", tema::suau);
    cerca.setFont (dibuix::lletra (14.0f));
    cerca.setIndents (34, 11);
    // El fons i la lupa es dibuixen a sota (paint): el quadre de text és transparent.
    cerca.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    cerca.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    cerca.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    cerca.onFocusLost = [this] { repaint(); };
    cerca.onReturnKey = [this] { if (! targetes.isEmpty()) tria (targetes[0]->info.fitxer); };
    cerca.onTextChange = [this] { ompleTargetes(); };
    addAndMakeVisible (cerca);

    filtre.onCanvi = [this] (int) { ompleTargetes(); };
    addAndMakeVisible (filtre);

    vistaGraella.setViewedComponent (&graella, false);
    vistaGraella.setScrollBarsShown (true, false);
    vistaGraella.setScrollBarThickness (8);
    addAndMakeVisible (vistaGraella);

    addAndMakeVisible (detalls);
    detalls.onObre = [this] { if (triat != juce::File() && onObre) onObre (triat); };
    detalls.onTreu = [this]
    {
        recents.treu (triat);
        historial.afegeix ("Tret de la llista de recents: "_ca + triat.getFileNameWithoutExtension());
        triat = juce::File();
        refresca();
    };

    refresca();
}

PantallaInici::~PantallaInici() = default;

InfoProjecte PantallaInici::llegeixInfo (const Recents::Entrada& e) const
{
    InfoProjecte i;
    i.fitxer = e.fitxer;
    i.carpeta = e.fitxer.getParentDirectory();
    i.obert = e.obert;
    i.quan = dibuix::quan (e.obert);
    i.nom = e.fitxer.getFileNameWithoutExtension();
    i.lloc = i.carpeta.getParentDirectory().getParentDirectory().getFileName() + " › "_ca + i.carpeta.getParentDirectory().getFileName();
    i.existeix = e.fitxer.existsAsFile();

    if (i.existeix)
    {
        std::string error;
        if (auto p = mibase::disc::llegeix (dibuix::cami (e.fitxer), &error))
        {
            i.nom = dibuix::utf8 (p->nom);
            i.tempo = textTempo (*p);
            i.pistes = (int) p->pistes.size();
        }
        else
        {
            i.error = dibuix::utf8 (error);
            i.tempo = "No es pot llegir";
        }
        i.preses = mibase::disc::comptaPreses (dibuix::cami (i.carpeta));
        i.evolucio = mibase::disc::llegeixEvolucio (dibuix::cami (i.carpeta), 12);
    }
    return i;
}

void PantallaInici::refresca()
{
    navegador.refresca();

    infos.clear();
    for (auto& e : recents.llista())
        infos.push_back (llegeixInfo (e));

    if (triat == juce::File() && ! infos.empty())
        triat = infos.front().fitxer;

    ompleTargetes();
}

void PantallaInici::ompleTargetes()
{
    targetes.clear();
    const auto text = cerca.getText().trim();
    const auto carpeta = navegador.getCarpeta();

    for (auto& i : infos)
    {
        if (text.isNotEmpty() && ! i.nom.containsIgnoreCase (text))
            continue;
        if (filtre.getTriat() == 1 && i.carpeta.getParentDirectory() != carpeta)
            continue;

        auto* t = targetes.add (new TargetaProjecte (i));
        t->triada = (i.fitxer == triat);
        const auto f = i.fitxer;
        t->onTria = [this, f] { tria (f); };
        t->onObre = [this, f] { tria (f); if (onObre) onObre (f); };
        graella.addAndMakeVisible (t);
    }

    const InfoProjecte* sel = nullptr;
    for (auto& i : infos)
        if (i.fitxer == triat)
            sel = &i;
    detalls.setInfo (sel);

    colocaTargetes();
    repaint();
}

void PantallaInici::tria (const juce::File& f)
{
    triat = f;
    for (auto* t : targetes)
    {
        const bool abans = t->triada;
        t->triada = (t->info.fitxer == f);
        if (abans != t->triada) t->repaint();
    }
    for (auto& i : infos)
        if (i.fitxer == f)
            detalls.setInfo (&i);
}

void PantallaInici::colocaTargetes()
{
    const int ample = juce::jmax (200, vistaGraella.getMaximumVisibleWidth());
    const int columnes = juce::jmax (1, (ample + 14) / (250 + 14));
    const int ampleTargeta = (ample - (columnes - 1) * 14) / columnes;
    const int alt = 196;

    for (int i = 0; i < targetes.size(); ++i)
        targetes[i]->setBounds ((i % columnes) * (ampleTargeta + 14), (i / columnes) * (alt + 14), ampleTargeta, alt);

    const int files = (targetes.size() + columnes - 1) / columnes;
    graella.setSize (ample, juce::jmax (vistaGraella.getHeight(), files * (alt + 14)));
}

void PantallaInici::obreNou()
{
    finestraNou = std::make_unique<FinestraProjecteNou> (navegador.getCarpeta());
    finestraNou->onTanca = [this] { juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PantallaInici> (this)] { if (sp) sp->tancaNou(); }); };
    finestraNou->onCrea = [this] (const mibase::Projecte& p)
    {
        const auto carpeta = navegador.getCarpeta();
        const auto r = mibase::disc::creaProjecte (dibuix::cami (carpeta), p,
                                                   dibuix::araIso().toStdString(), dibuix::araText().toStdString());
        if (! r.correcte)
        {
            finestraNou->setError (dibuix::utf8 (r.error));
            historial.afegeix ("No s'ha pogut crear el projecte: "_ca + dibuix::utf8 (r.error));
            return;
        }

        const auto fitxer = dibuix::fitxer (r.fitxer);
        historial.afegeix ("Projecte creat: "_ca + fitxer.getFullPathName());
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PantallaInici> (this), fitxer]
        {
            if (sp == nullptr) return;
            sp->tancaNou();
            sp->triat = fitxer;
            if (sp->onCreat) sp->onCreat (fitxer);
        });
    };
    addAndMakeVisible (*finestraNou);
    finestraNou->setBounds (getLocalBounds());
    finestraNou->toFront (true);
    finestraNou->enfocaNom();
}

void PantallaInici::tancaNou()
{
    finestraNou = nullptr;
}

void PantallaInici::obreAltre()
{
    selector = std::make_unique<juce::FileChooser> ("Obre un projecte de Mibase", navegador.getCarpeta(), "*.mibase");
    selector->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                           [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f.existsAsFile() && onObre)
            onObre (f);
    });
}

void PantallaInici::resized()
{
    auto r = getLocalBounds();

    // El centre sempre té com a mínim 560 px: si no hi caben, s'encongeixen els laterals (primer el de la dreta).
    const int minimCentre = 560;
    int esq = ampleEsq, dre = ampleDre;
    int sobra = esq + dre + minimCentre - r.getWidth();
    if (sobra > 0) { const int t = juce::jmin (sobra, dre - 260); dre -= juce::jmax (0, t); sobra -= juce::jmax (0, t); }
    if (sobra > 0) { const int t = juce::jmin (sobra, esq - 220); esq -= juce::jmax (0, t); }

    navegador.setBounds (r.removeFromLeft (esq));
    detalls.setBounds (r.removeFromRight (dre));
    divisorEsq.setBounds (navegador.getRight() - 4, r.getY(), 8, r.getHeight());
    divisorDre.setBounds (detalls.getX() - 4, r.getY(), 8, r.getHeight());

    zonaCentre = r;
    auto c = r.reduced (32, 28);
    const bool estret = c.getWidth() < 720;   // si no hi cap tot en una fila, els botons van a sota del títol
    zonaCapcalera = c.removeFromTop (52);
    {
        auto h = estret ? c.removeFromTop (52) : zonaCapcalera;
        btNou.setBounds (h.removeFromRight (btNou.ampladaIdeal() + 6).withTrimmedTop (8));
        h.removeFromRight (12);
        btAltre.setBounds (h.removeFromRight (btAltre.ampladaIdeal()).withTrimmedTop (8));
    }
    zonaTitol = estret ? zonaCapcalera : zonaCapcalera.withRight (btAltre.getX() - 12);
    c.removeFromTop (20);
    auto fila = c.removeFromTop (40);
    cerca.setBounds (fila.removeFromLeft (juce::jmin (420, fila.getWidth() / 2)));
    fila.removeFromLeft (12);
    filtre.setBounds (fila.removeFromLeft (juce::jmin (fila.getWidth(), filtre.ampladaIdeal() + 10)));
    c.removeFromTop (20);
    vistaGraella.setBounds (c.withTrimmedRight (-8));

    colocaTargetes();
    divisorEsq.toFront (false);
    divisorDre.toFront (false);

    if (finestraNou)
        finestraNou->setBounds (getLocalBounds());
}

void PantallaInici::paint (juce::Graphics& g)
{
    g.fillAll (tema::fons);

    auto h = zonaTitol;
    g.setColour (tema::text);
    g.setFont (dibuix::lletra (26.0f, true));
    g.drawText ("Projectes recents", h.removeFromTop (32), juce::Justification::centredLeft, true);
    g.setColour (tema::suau);
    g.setFont (dibuix::lletra (14.0f));
    g.drawText ("Continua on ho vas deixar, o comença una idea nova."_ca, h, juce::Justification::centredLeft, true);

    // Cercador: fons, vora i lupa.
    {
        auto c = cerca.getBounds().toFloat();
        g.setColour (tema::superficie);
        g.fillRoundedRectangle (c, 10.0f);
        g.setColour (cerca.hasKeyboardFocus (true) ? tema::linia2 : tema::linia);
        g.drawRoundedRectangle (c.reduced (0.5f), 10.0f, 1.0f);
    }
    dibuix::pintaIcona (g, dibuix::Icona::Lupa, cerca.getBounds().toFloat().removeFromLeft (34).withSizeKeepingCentre (16, 16), tema::suau, 2.0f);

    if (targetes.isEmpty())
    {
        auto z = vistaGraella.getBounds().withHeight (120).toFloat();
        juce::Path p;
        p.addRoundedRectangle (z.reduced (0.5f), 12.0f);
        juce::Path discontinu;
        const float guions[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (discontinu, p, guions, 2);
        g.setColour (tema::linia2);
        g.fillPath (discontinu);
        g.setColour (tema::textMig);
        g.setFont (dibuix::lletra (15.0f, true));
        g.drawText (infos.empty() ? juce::String ("Encara no hi ha projectes recents") : juce::String ("Cap projecte coincideix"),
                    z.withTrimmedBottom (z.getHeight() / 2).withTrimmedTop (22), juce::Justification::centred);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (13.0f));
        g.drawText (infos.empty() ? "Tria a l'esquerra on el vols guardar i prem «Projecte nou»."_ca
                                  : "Prova amb una altra paraula o mira «Tots»."_ca,
                    z.withTrimmedTop (z.getHeight() / 2).withTrimmedBottom (22), juce::Justification::centred);
    }
}
