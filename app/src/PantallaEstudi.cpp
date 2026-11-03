#include "PantallaEstudi.h"

using namespace mibase;
namespace ed = mibase::edicio;

namespace
{
    constexpr int alcadaBarra = 60;
    constexpr int alcadaRegla = 34;
    constexpr int gruixBarres = 10;
}

PantallaEstudi::PantallaEstudi (const juce::File& f, const Projecte& p, juce::PropertiesFile& a, Historial& h)
    : ajustos (a), historial (h)
{
    setWantsKeyboardFocus (true);
    formats.registerBasicFormats();

    estat.projecte = p;
    estat.fitxer = f;
    estat.carpeta = f.getParentDirectory();
    estat.pxPerSegon = ajustos.getDoubleValue ("estudi.zoom", 48.0);
    estat.altPista = ajustos.getIntValue ("estudi.altPista", 76);
    if (! estat.projecte.pistes.empty()) estat.pistaTriada = 0;
    estat.addChangeListener (this);
    estat.onCanviat = [this] (const juce::String& evolucio)
    {
        if (evolucio.isNotEmpty())
            disc::afegeixEvolucio (dibuix::cami (estat.carpeta), dibuix::araText().toStdString(), evolucio.toStdString());
        calDesar = true;
        startTimer (700);
    };

    for (auto& s : estat.projecte.preses)
        carregaOna (s);

    ampleNoms = ajustos.getIntValue ("estudi.ampleNoms", 170);
    ampleFaders = ajustos.getIntValue ("estudi.ampleFaders", 150);
    amplePanell = ajustos.getIntValue ("estudi.amplePanell", 300);

    // Barra de dalt.
    for (auto* b : { &btPrincipi, &btPlay, &btGrava, &btClic, &btTempo, &btDesfes, &btRefes, &btImporta })
        addAndMakeVisible (b);
    addAndMakeVisible (snap);

    btPrincipi.setTooltip ("Capçal a l'inici (Inici)"_ca);
    btPrincipi.onClick = [this] { estat.capcal = 0.0; estat.iniciVista = 0.0; estat.avisa(); };
    btPlay.setTooltip ("Reproduir: arriba a la versió 0.4"_ca);
    btPlay.setEnabled (false);
    btGrava.setTooltip ("Gravar: arriba a la versió 0.4"_ca);
    btGrava.setEnabled (false);
    btClic.setClickingTogglesState (false);
    btClic.setTooltip ("Metrònom encès o apagat (sonarà a la 0.4)"_ca);
    btClic.onClick = [this] { estat.projecte.clic = ! estat.projecte.clic; estat.avisa(); calDesar = true; startTimer (700); };
    btTempo.setTooltip ("Canvia el tempo"_ca);
    btTempo.onClick = [this] { canviaTempo(); };
    snap.setTriat ((int) estat.projecte.snap);
    snap.onCanvi = [this] (int i) { estat.projecte.snap = (Snap) i; estat.avisa(); calDesar = true; startTimer (700); };
    btDesfes.setTooltip ("Desfés (Ctrl+Z)"_ca);
    btDesfes.onClick = [this] { desfes (false); };
    btRefes.setTooltip ("Refés (Ctrl+Y)"_ca);
    btRefes.onClick = [this] { desfes (true); };
    btImporta.setTooltip ("Importa un o més fitxers d'àudio a la pista triada (o a pistes noves)"_ca);
    btImporta.onClick = [this] { demanaImportar (estat.pistaTriada, ed::enganxa (estat.projecte, estat.capcal, estat.projecte.snap)); };

    // Seccions.
    for (auto* s : std::initializer_list<SeccioEstudi*> { &capcaleres, &faders, &regla, &pistes, &panell })
    {
        addAndMakeVisible (s);
        s->onRoda = [this] (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) { roda (e, w); };
    }
    capcaleres.onCanviaNom = [this] (int i) { canviaNom (i); };
    capcaleres.onPistaNova = [this] (const juce::String& tipus) { pistaNova (tipus); };
    pistes.onPistaNova = [this] (int, const juce::String& tipus) { pistaNova (tipus); };
    pistes.onImporta = [this] (const juce::StringArray& fitxers, int pista, double on) { importa (fitxers, pista, on); };
    pistes.onDemanaImportar = [this] (int pista, double on) { demanaImportar (pista, on); };
    panell.onDuplica = [this] { duplica(); };
    panell.onTreu = [this] { treu(); };

    for (auto* d : { &divNoms, &divFaders, &divPanell })
    {
        addAndMakeVisible (d);
        d->setTooltip ("Arrossega per fer més ample o més estret. Doble clic: mida inicial."_ca);
        d->onAcaba = [this]
        {
            ajustos.setValue ("estudi.ampleNoms", ampleNoms);
            ajustos.setValue ("estudi.ampleFaders", ampleFaders);
            ajustos.setValue ("estudi.amplePanell", amplePanell);
            ajustos.saveIfNeeded();
        };
    }
    divNoms.onComenca = [this] { ampleInici = capcaleres.getWidth(); };
    divNoms.onArrossega = [this] (int dx) { ampleNoms = juce::jmax (110, ampleInici + dx); colocaSeccions(); };
    divNoms.onDobleClic = [this] { ampleNoms = 170; colocaSeccions(); divNoms.onAcaba(); };
    divFaders.onComenca = [this] { ampleInici = faders.getWidth(); };
    divFaders.onArrossega = [this] (int dx) { ampleFaders = juce::jmax (90, ampleInici + dx); colocaSeccions(); };
    divFaders.onDobleClic = [this] { ampleFaders = 150; colocaSeccions(); divFaders.onAcaba(); };
    divPanell.onComenca = [this] { ampleInici = panell.getWidth(); };
    divPanell.onArrossega = [this] (int dx) { amplePanell = juce::jmax (220, ampleInici - dx); colocaSeccions(); };
    divPanell.onDobleClic = [this] { amplePanell = 300; colocaSeccions(); divPanell.onAcaba(); };

    for (auto* b : { &barraV, &barraH })
    {
        addAndMakeVisible (b);
        b->addListener (this);
        b->setAutoHide (false);
        b->setColour (juce::ScrollBar::thumbColourId, tema::linia2);
    }

    ultimaCopia = 0;   // el primer desament de la sessió fa còpia; després, com a molt una cada 10 minuts
    changeListenerCallback (nullptr);
}

PantallaEstudi::~PantallaEstudi()
{
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
    const bool ferCopia = ara - ultimaCopia > 10 * 60 * 1000;   // una còpia cada 10 minuts, com a molt
    const auto r = disc::desa (dibuix::cami (estat.fitxer), estat.projecte, dibuix::araIso().toStdString(),
                               dibuix::araPerNom().toStdString(), ferCopia);
    if (r.correcte)
    {
        calDesar = false;
        if (ferCopia) ultimaCopia = ara;
        // El que hi ha al disc és ara l'original (per conservar camps desconeguts).
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
    if (snap.getTriat() != (int) p.snap) snap.setTriat ((int) p.snap);
    actualitzaBarres();
    repaint (zonaBarra);
}

//==============================================================================
void PantallaEstudi::paint (juce::Graphics& g)
{
    g.fillAll (tema::fons);

    // Barra de dalt.
    g.setColour (tema::superficie);
    g.fillRect (zonaBarra);
    g.setColour (tema::linia);
    g.fillRect (zonaBarra.getX(), zonaBarra.getBottom() - 1, zonaBarra.getWidth(), 1);

    // Posició.
    {
        auto r = zonaPosicio.toFloat();
        g.setColour (tema::fons);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (tema::linia);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (10.5f, true));
        g.drawText ("COMPÀS"_ca, zonaPosicio.reduced (10, 4).removeFromTop (12), juce::Justification::centredLeft);
        g.setColour (tema::text);
        g.setFont (dibuix::mono (18.0f));
        g.drawText (dibuix::utf8 (ed::textPosicio (estat.projecte, estat.capcal)), zonaPosicio.reduced (10, 2).withTrimmedTop (12),
                    juce::Justification::centredLeft);
    }

    // Metrònom encès: en groc.
    if (estat.projecte.clic)
    {
        g.setColour (tema::guitarra.withAlpha (0.18f));
        g.fillRoundedRectangle (btClic.getBounds().toFloat(), 8.0f);
    }

    // Text d'informació (el que s'encongeix: els botons no es mouen mai).
    if (! zonaInfo.isEmpty())
    {
        g.setColour (estat.info.isNotEmpty() ? tema::accent : tema::suau);
        g.setFont (dibuix::lletra (13.0f));
        const auto text = estat.info.isNotEmpty() ? estat.info
                        : juce::String (estat.numPistes()) + (estat.numPistes() == 1 ? " pista" : " pistes")
                          + " · Ctrl+roda: zoom · Alt+roda: alçada"_ca;
        g.drawFittedText (text, zonaInfo, juce::Justification::centredLeft, 1, 0.8f);
    }

    // Títols de les columnes, a l'altura de la regla.
    auto titol = [&] (juce::Rectangle<int> r, const juce::String& t)
    {
        g.setColour (tema::lateral);
        g.fillRect (r);
        g.setColour (tema::linia);
        g.fillRect (r.getX(), r.getBottom() - 1, r.getWidth(), 1);
        g.fillRect (r.getRight() - 1, r.getY(), 1, r.getHeight());
        dibuix::pintaEtiquetaSeccio (g, t, r.withTrimmedLeft (14));
    };
    titol (zonaTitolNoms, "Pistes");
    titol (zonaTitolFaders, "Volum");
}

void PantallaEstudi::colocaSeccions()
{
    auto r = getLocalBounds().withTrimmedTop (alcadaBarra);

    // Si no hi cap tot, s'encongeix primer el panell i després els noms i els faders (la línia de temps, mínim 320).
    int noms = ampleNoms, fad = ampleFaders, pan = amplePanell;
    int sobra = noms + fad + pan + 320 - r.getWidth();
    auto encongeix = [&sobra] (int& v, int minim) { if (sobra > 0) { const int t = juce::jmin (sobra, v - minim); if (t > 0) { v -= t; sobra -= t; } } };
    encongeix (pan, 220);
    encongeix (fad, 90);
    encongeix (noms, 110);

    auto colNoms = r.removeFromLeft (noms);
    auto colFaders = r.removeFromLeft (fad);
    auto colPanell = r.removeFromRight (pan);
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
    panell.setBounds (colPanell);

    divNoms.setBounds (capcaleres.getRight() - 4, alcadaBarra, 8, getHeight() - alcadaBarra);
    divFaders.setBounds (faders.getRight() - 4, alcadaBarra, 8, getHeight() - alcadaBarra);
    divPanell.setBounds (panell.getX() - 4, alcadaBarra, 8, getHeight() - alcadaBarra);
    for (auto* d : { &divNoms, &divFaders, &divPanell }) d->toFront (false);

    actualitzaBarres();
    repaint();
}

void PantallaEstudi::resized()
{
    zonaBarra = getLocalBounds().removeFromTop (alcadaBarra);
    auto b = zonaBarra.reduced (14, 10);

    btPrincipi.setBounds (b.removeFromLeft (40));
    b.removeFromLeft (2);
    btPlay.setBounds (b.removeFromLeft (40));
    b.removeFromLeft (2);
    btGrava.setBounds (b.removeFromLeft (40));
    b.removeFromLeft (12);
    zonaPosicio = b.removeFromLeft (112);
    b.removeFromLeft (12);
    btClic.setBounds (b.removeFromLeft (40));
    btTempo.setBounds (b.removeFromLeft (btTempo.ampladaIdeal() + 10));
    b.removeFromLeft (12);
    snap.setBounds (b.removeFromLeft (snap.ampladaIdeal() + 6));
    b.removeFromLeft (12);
    btDesfes.setBounds (b.removeFromLeft (40));
    btRefes.setBounds (b.removeFromLeft (40));

    btImporta.setBounds (b.removeFromRight (btImporta.ampladaIdeal()));
    b.removeFromRight (12);
    b.removeFromLeft (16);
    zonaInfo = b;

    colocaSeccions();
}

//==============================================================================
void PantallaEstudi::actualitzaBarres()
{
    const double visibles = pistes.getWidth() / estat.pxPerSegon;
    const double total = juce::jmax (ed::final (estat.projecte) + 30.0, estat.iniciVista + visibles, 120.0);
    barraH.setRangeLimits (0.0, total, juce::dontSendNotification);
    barraH.setCurrentRange (estat.iniciVista, visibles, juce::dontSendNotification);

    const int contingut = (estat.numPistes() + 1) * estat.altPista + 60;
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
        // Zoom del temps, al voltant del ratolí.
        const float x = (float) e.getEventRelativeTo (&pistes).x;
        const double tRatoli = estat.t (x);
        estat.pxPerSegon = juce::jlimit (2.0, 2000.0, estat.pxPerSegon * std::pow (1.6, (double) d * 2.0));
        estat.iniciVista = juce::jmax (0.0, tRatoli - x / estat.pxPerSegon);
    }
    else if (e.mods.isAltDown())
    {
        estat.altPista = juce::jlimit (40, 240, estat.altPista + juce::roundToInt (d * 60.0f));
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
void PantallaEstudi::pistaNova (const juce::String& tipus)
{
    estat.desfes.abans (estat.projecte);
    const auto nom = ed::nomPistaLliure (estat.projecte, EstatEstudi::nomTipus (tipus.toStdString()).toStdString());
    ed::afegeixPista (estat.projecte, nom, tipus.toStdString());
    estat.pistaTriada = estat.numPistes() - 1;
    estat.clipsTriats.clear();
    estat.canviat ("Pista nova: «"_ca + dibuix::utf8 (nom) + "»"_ca);
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
        });
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
            estat.canviat ("Tempo de "_ca + juce::String (abans, 0) + " a " + juce::String (bpm, 0) + " bpm");
        });
    DialegText::mostra (*this, dialeg);
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

juce::File PantallaEstudi::fitxerPresa (const Presa& s) const
{
    auto f = estat.carpeta;
    juce::StringArray trossos;
    trossos.addTokens (dibuix::utf8 (s.fitxer), "/", {});
    for (auto& t : trossos)
        if (t.isNotEmpty()) f = f.getChildFile (t);
    return f;
}

void PantallaEstudi::carregaOna (const Presa& s)
{
    std::unique_ptr<juce::AudioFormatReader> lector (formats.createReaderFor (fitxerPresa (s)));
    if (lector == nullptr || lector->sampleRate <= 0.0)
    {
        estat.ones.erase (s.id);
        return;
    }

    const auto mostresPerPic = juce::jmax ((juce::int64) 1, (juce::int64) (lector->sampleRate / 100.0));   // un pic cada 10 ms
    const auto total = lector->lengthInSamples;
    std::vector<float> pics ((size_t) ((total + mostresPerPic - 1) / mostresPerPic), 0.0f);

    const int bloc = (int) mostresPerPic * 256;
    juce::AudioBuffer<float> buffer ((int) juce::jmax (1u, lector->numChannels), bloc);
    for (juce::int64 pos = 0; pos < total; pos += bloc)
    {
        const int n = (int) juce::jmin ((juce::int64) bloc, total - pos);
        lector->read (&buffer, 0, n, pos, true, true);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float* d = buffer.getReadPointer (ch);
            for (int i = 0; i < n; ++i)
            {
                auto& pic = pics[(size_t) ((pos + i) / mostresPerPic)];
                pic = juce::jmax (pic, std::abs (d[i]));
            }
        }
    }
    estat.ones[s.id] = std::move (pics);
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
        ed::afegeixPista (estat.projecte, nom, "audio");
        pista = estat.numPistes() - 1;
    }

    auto& t = estat.projecte.pistes[(size_t) pista];
    auto audios = estat.carpeta.getChildFile (juce::String::fromUTF8 (disc::carpetaAudios));
    audios.createDirectory();
    const auto nomDesti = dibuix::utf8 (disc::nomPresa (dibuix::cami (audios), t.nom));
    const auto desti = audios.getChildFile (nomDesti);

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
        // Es converteix a WAV de 24 bits.
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
        int p = pista;   // -1: cada fitxer, a una pista nova
        double final = posicio;
        juce::String error;
        if (importaUn (juce::File (cami), p, posicio, final, error))
        {
            ++fets;
            ultimaPista = p;
            // Si van a la mateixa pista, un darrere l'altre.
            if (pista >= 0) posicio = ed::enganxa (estat.projecte, final, Snap::Compas) < final
                                    ? ed::enganxa (estat.projecte, final, Snap::Compas) + ed::segonsPerCompas (estat.projecte)
                                    : ed::enganxa (estat.projecte, final, Snap::Compas);
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
    if (codi == juce::KeyPress::escapeKey) { estat.clipsTriats.clear(); estat.avisa(); return true; }
    if (codi == juce::KeyPress::homeKey) { estat.capcal = 0.0; estat.iniciVista = 0.0; estat.avisa(); return true; }
    if (codi == juce::KeyPress::spaceKey)
    {
        estat.info = "Reproduir arriba a la versió 0.4"_ca;
        estat.avisa();
        return true;
    }
    return false;
}
