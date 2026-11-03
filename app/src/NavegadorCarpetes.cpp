#include "NavegadorCarpetes.h"

namespace
{
    juce::String nomVisible (const juce::File& f)
    {
        auto n = f.getFileName();
        return n.isEmpty() ? f.getFullPathName() : n;
    }

    juce::String midaLliure (juce::int64 bytes)
    {
        if (bytes <= 0) return {};
        const double gb = (double) bytes / (1024.0 * 1024.0 * 1024.0);
        return gb >= 10 ? juce::String ((int) gb) + " GB lliures" : juce::String (gb, 1) + " GB lliures";
    }

    struct Lloc { juce::String nom; juce::File f; };

    std::vector<Lloc> llocs()
    {
        using F = juce::File;
        return {
            { "Escriptori", F::getSpecialLocation (F::userDesktopDirectory) },
            { "Documents",  F::getSpecialLocation (F::userDocumentsDirectory) },
            { "Música"_ca,     F::getSpecialLocation (F::userMusicDirectory) },
            { "Baixades",   F::getSpecialLocation (F::userHomeDirectory).getChildFile ("Downloads") },
        };
    }
}

NavegadorCarpetes::NavegadorCarpetes (juce::PropertiesFile& a) : ajustos (a)
{
    const auto musica = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    auto perDefecte = musica.getChildFile ("Mibase");
    if (! perDefecte.isDirectory())
        perDefecte.createDirectory();

    arrel  = juce::File (ajustos.getValue ("navegador.arrel", musica.getFullPathName()));
    triada = juce::File (ajustos.getValue ("navegador.triada", perDefecte.getFullPathName()));
    if (! arrel.isDirectory())  arrel = musica;
    if (! triada.isDirectory()) triada = perDefecte.isDirectory() ? perDefecte : arrel;

    juce::StringArray o;
    o.addLines (ajustos.getValue ("navegador.oberts", arrel.getFullPathName() + "\n" + perDefecte.getFullPathName()));
    for (auto& s : o)
        if (s.isNotEmpty())
            oberts.insert (s);

    vista.setViewedComponent (&llista, false);
    vista.setScrollBarsShown (true, false);
    vista.setScrollBarThickness (8);
    addAndMakeVisible (vista);

    addAndMakeVisible (btNova);
    btNova.onClick = [this] { carpetaNova(); };

    refresca();
}

NavegadorCarpetes::~NavegadorCarpetes() = default;

void NavegadorCarpetes::desa()
{
    ajustos.setValue ("navegador.arrel", arrel.getFullPathName());
    ajustos.setValue ("navegador.triada", triada.getFullPathName());
    juce::StringArray o;
    for (auto& s : oberts) o.add (s);
    ajustos.setValue ("navegador.oberts", o.joinIntoString ("\n"));
    ajustos.saveIfNeeded();
}

void NavegadorCarpetes::refresca()
{
    memoriaProjectes.clear();
    construeix();
}

juce::Array<juce::File> NavegadorCarpetes::subcarpetes (const juce::File& f) const
{
    juce::Array<juce::File> r;
    for (const auto& e : juce::RangedDirectoryIterator (f, false, "*", juce::File::findDirectories))
    {
        const auto& c = e.getFile();
        if (e.isHidden() || c.getFileName().startsWithChar ('.') || c.getFileName().startsWithChar ('$'))
            continue;
        if (mibase::disc::esCarpetaDeProjecte (dibuix::cami (c)))
            continue;
        r.add (c);
    }
    std::sort (r.begin(), r.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getFileName().compareNatural (b.getFileName()) < 0;
    });
    return r;
}

int NavegadorCarpetes::projectesDe (const juce::File& f)
{
    const auto clau = f.getFullPathName();
    if (auto it = memoriaProjectes.find (clau); it != memoriaProjectes.end())
        return it->second;
    const int n = mibase::disc::comptaProjectes (dibuix::cami (f));
    memoriaProjectes[clau] = n;
    return n;
}

void NavegadorCarpetes::afegeixCarpeta (const juce::File& f, int nivell)
{
    const bool obert = oberts.count (f.getFullPathName()) > 0;
    const auto fills = obert || nivell < 8 ? subcarpetes (f) : juce::Array<juce::File>();

    Fila fila;
    fila.tipus = Fila::Tipus::Carpeta;
    fila.f = f;
    fila.nom = nomVisible (f);
    fila.nivell = nivell;
    fila.obert = obert;
    fila.teFills = ! fills.isEmpty();
    fila.projectes = projectesDe (f);
    files.push_back (fila);

    if (obert && nivell < 8)
        for (auto& c : fills)
            afegeixCarpeta (c, nivell + 1);
}

void NavegadorCarpetes::construeix()
{
    files.clear();

    auto titol = [this] (const juce::String& t)
    {
        Fila f; f.tipus = Fila::Tipus::Titol; f.nom = t; f.alcada = files.empty() ? 26 : 40;
        files.push_back (f);
    };

    titol ("Llocs");
    for (auto& l : llocs())
    {
        if (! l.f.isDirectory()) continue;
        Fila f; f.tipus = Fila::Tipus::Lloc; f.f = l.f; f.nom = l.nom;
        files.push_back (f);
    }

    titol ("Carpetes de projectes");
    afegeixCarpeta (arrel, 0);

    titol ("Unitats");
    juce::Array<juce::File> unitats;
    juce::File::findFileSystemRoots (unitats);
    for (auto& u : unitats)
    {
        Fila f; f.tipus = Fila::Tipus::Unitat; f.f = u;
        const auto etiqueta = u.getVolumeLabel();
        const auto lletra = u.getFullPathName().trimCharactersAtEnd ("\\/");
        f.nom = (etiqueta.isNotEmpty() ? etiqueta : juce::String ("Disc")) + " (" + (lletra.isEmpty() ? juce::String ("/") : lletra) + ")";
        f.extra = midaLliure (u.getBytesFreeOnVolume());
        files.push_back (f);
    }

    int total = 0;
    for (auto& f : files) total += f.alcada;
    llista.setSize (juce::jmax (100, vista.getMaximumVisibleWidth()), total + 8);
    llista.repaint();
    repaint();
}

void NavegadorCarpetes::tria (const juce::File& f)
{
    triada = f;
    desa();
    construeix();
    if (onCanvi) onCanvi();
}

void NavegadorCarpetes::obreOTanca (const juce::File& f)
{
    const auto clau = f.getFullPathName();
    if (oberts.count (clau)) oberts.erase (clau);
    else oberts.insert (clau);
    desa();
    construeix();
}

void NavegadorCarpetes::triaArrel (const juce::File& f)
{
    arrel = f;
    oberts.insert (f.getFullPathName());
    tria (f);
}

void NavegadorCarpetes::carpetaNova()
{
    const auto mare = triada;
    dialeg = std::make_unique<DialegText> ("Carpeta nova"_ca, "Es crearà dins de: "_ca + mare.getFullPathName(), "Crea-la"_ca,
        [mare] (const juce::String& nom) -> juce::String
        {
            const auto v = mibase::disc::validaNom (nom.toStdString());
            if (! v.correcte) return dibuix::utf8 (v.missatge);
            if (mare.getChildFile (nom).exists()) return "Ja n'hi ha una que es diu així."_ca;
            return {};
        },
        [this, mare] (const juce::String& nom)
        {
            auto nova = mare.getChildFile (nom);
            if (nova.createDirectory().failed())
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "No s'ha creat la carpeta"_ca,
                                                        "Windows no ha deixat crear-la en aquest lloc."_ca);
                return;
            }
            oberts.insert (mare.getFullPathName());
            memoriaProjectes.clear();
            tria (nova);
        });
    DialegText::mostra (*this, dialeg);
}

void NavegadorCarpetes::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff141417));

    auto caixa = zonaCami.toFloat();
    g.setColour (tema::superficie);
    g.fillRoundedRectangle (caixa, 10.0f);
    g.setColour (tema::linia);
    g.drawRoundedRectangle (caixa.reduced (0.5f), 10.0f, 1.0f);

    auto t = zonaCami.reduced (12, 10);
    g.setColour (tema::suau);
    g.setFont (dibuix::lletra (12.0f));
    g.drawText ("Els projectes nous es crearan a"_ca, t.removeFromTop (18), juce::Justification::centredLeft);
    g.setColour (tema::text);
    g.setFont (dibuix::mono (12.5f));
    g.drawFittedText (triada.getFullPathName(), t, juce::Justification::topLeft, 3, 0.8f);
}

void NavegadorCarpetes::resized()
{
    auto r = getLocalBounds().reduced (10, 12);
    zonaCami = r.removeFromBottom (86);
    r.removeFromBottom (10);
    btNova.setBounds (r.removeFromBottom (36));
    r.removeFromBottom (8);
    vista.setBounds (r);
    construeix();
}

//==============================================================================
int NavegadorCarpetes::Llista::filaA (int y) const
{
    int acumulat = 0;
    for (int i = 0; i < (int) nav.files.size(); ++i)
    {
        acumulat += nav.files[(size_t) i].alcada;
        if (y < acumulat)
            return i;
    }
    return -1;
}

void NavegadorCarpetes::Llista::paint (juce::Graphics& g)
{
    int y = 0;
    const int ample = getWidth();

    for (int i = 0; i < (int) nav.files.size(); ++i)
    {
        const auto& f = nav.files[(size_t) i];
        juce::Rectangle<int> r (0, y, ample, f.alcada);
        y += f.alcada;

        if (f.tipus == Fila::Tipus::Titol)
        {
            dibuix::pintaEtiquetaSeccio (g, f.nom, r.withTrimmedLeft (10).withTrimmedTop (r.getHeight() - 24));
            continue;
        }

        const bool esTriada = (f.tipus == Fila::Tipus::Carpeta || f.tipus == Fila::Tipus::Lloc) && f.f == nav.triada
                              && f.tipus == Fila::Tipus::Carpeta;
        const bool esArrel = f.tipus != Fila::Tipus::Carpeta && f.f == nav.arrel;
        auto fons = r.reduced (0, 1).toFloat();

        if (esTriada)
        {
            g.setColour (tema::accent.withAlpha (0.12f));
            g.fillRoundedRectangle (fons, 8.0f);
            g.setColour (tema::accent.withAlpha (0.45f));
            g.drawRoundedRectangle (fons.reduced (0.5f), 8.0f, 1.0f);
        }
        else if (esArrel)
        {
            g.setColour (tema::linia);
            g.fillRoundedRectangle (fons, 8.0f);
        }
        else if (i == sobre)
        {
            g.setColour (tema::superficie2);
            g.fillRoundedRectangle (fons, 8.0f);
        }

        int x = 10 + f.nivell * 16;
        const auto colorText = (esTriada || esArrel || i == sobre) ? tema::text : juce::Colour (0xffc9c8d1);

        if (f.tipus == Fila::Tipus::Carpeta)
        {
            if (f.teFills)
                dibuix::pintaIcona (g, f.obert ? dibuix::Icona::FletxaAvall : dibuix::Icona::Fletxa,
                                    juce::Rectangle<float> ((float) x - 3.0f, (float) r.getCentreY() - 7.0f, 14.0f, 14.0f), tema::suau, 2.2f);
            x += 14;
        }

        const auto icona = f.tipus == Fila::Tipus::Unitat ? dibuix::Icona::Disc : dibuix::Icona::Carpeta;
        dibuix::pintaIcona (g, icona, juce::Rectangle<float> ((float) x, (float) r.getCentreY() - 9.0f, 18.0f, 18.0f),
                            esTriada ? tema::accent : tema::suau, 1.8f,
                            esTriada ? tema::accent.withAlpha (0.18f) : juce::Colours::transparentBlack);
        x += 26;

        auto zonaText = juce::Rectangle<int> (x, r.getY(), ample - x - 8, r.getHeight());

        if (f.tipus == Fila::Tipus::Carpeta && f.projectes > 0)
        {
            const juce::String n (f.projectes);
            auto pastilla = zonaText.removeFromRight (16 + 7 * n.length()).withSizeKeepingCentre (16 + 7 * n.length(), 18).toFloat();
            g.setColour (tema::linia);
            g.fillRoundedRectangle (pastilla, 9.0f);
            g.setColour (juce::Colour (0xffc9c8d1));
            g.setFont (dibuix::lletra (11.5f));
            g.drawText (n, pastilla, juce::Justification::centred);
            zonaText.removeFromRight (6);
        }
        else if (f.extra.isNotEmpty())
        {
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (11.5f));
            g.drawText (f.extra, zonaText.removeFromRight (90), juce::Justification::centredRight);
        }

        g.setColour (colorText);
        g.setFont (dibuix::lletra (14.0f));
        g.drawText (f.nom, zonaText, juce::Justification::centredLeft, true);
    }
}

void NavegadorCarpetes::Llista::mouseMove (const juce::MouseEvent& e)
{
    const int i = filaA (e.y);
    const int nou = (i >= 0 && nav.files[(size_t) i].tipus != Fila::Tipus::Titol) ? i : -1;
    if (nou != sobre) { sobre = nou; repaint(); }
    setMouseCursor (nou >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void NavegadorCarpetes::Llista::mouseDown (const juce::MouseEvent& e)
{
    const int i = filaA (e.y);
    if (i < 0) return;
    const auto f = nav.files[(size_t) i];   // còpia: construeix() canvia la llista

    switch (f.tipus)
    {
        case Fila::Tipus::Lloc:
        case Fila::Tipus::Unitat:
            nav.triaArrel (f.f);
            break;
        case Fila::Tipus::Carpeta:
        {
            const int xFletxa = 10 + f.nivell * 16;
            if (f.teFills && e.x >= xFletxa - 6 && e.x < xFletxa + 14)
                nav.obreOTanca (f.f);
            else
                nav.tria (f.f);
            break;
        }
        case Fila::Tipus::Titol:
        case Fila::Tipus::Buit:
            break;
    }
}

void NavegadorCarpetes::Llista::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int i = filaA (e.y);
    if (i >= 0 && nav.files[(size_t) i].tipus == Fila::Tipus::Carpeta && nav.files[(size_t) i].teFills)
        nav.obreOTanca (nav.files[(size_t) i].f);
}
