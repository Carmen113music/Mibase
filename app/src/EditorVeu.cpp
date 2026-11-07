#include "EditorVeu.h"
#include "Inserts.h"

#include "mibase/Edicio.h"
#include "mibase/Harmonia.h"
#include "mibase/Midi.h"

namespace vv = mibase::veu;
using mibase::NotaVeu;

namespace
{
    /** Color d'una nota segons com d'afinada queda (respecte a la nota més propera). */
    juce::Colour colorAfinacio (double centreFinal)
    {
        const double c = std::abs (centreFinal - std::round (centreFinal)) * 100.0;
        if (c < 8.0) return tema::sinte;          // afinada
        if (c < 25.0) return tema::guitarra;      // una mica
        return tema::accent;                      // desafinada
    }

    juce::String num (double v, int dec)
    {
        auto s = juce::String (v, dec).replaceCharacter ('.', ',');
        return s;
    }
}

juce::String EditorVeu::nomEina (int e)
{
    switch (e)
    {
        case Fletxa:  return "Afinació: arrossega les notes amunt i avall (s'enganxen als semitons; Alt = lliure). Doble clic = afina-la."_ca;
        case Vibrato: return "Vibrato: arrossega amunt per més vibrato, avall per menys. Doble clic = l'original."_ca;
        case Deriva:  return "Deriva: arrossega avall per aplanar la nota (sense tocar el vibrato). Doble clic = l'original."_ca;
        case Formant: return "Formant: arrossega per canviar el timbre sense canviar la nota. Doble clic = l'original."_ca;
        case Volum:   return "Volum: arrossega per pujar o baixar el volum de la nota. Doble clic = l'original."_ca;
        default: return {};
    }
}

juce::String EditorVeu::nomNota (double midi)
{
    const int n = (int) std::lround (midi);
    return juce::String::fromUTF8 (mibase::harmonia::nomTo (((n % 12) + 12) % 12).c_str()) + juce::String (n / 12 - 1);
}

juce::String EditorVeu::textCents (double c)
{
    const int v = (int) std::lround (c);
    return (v > 0 ? "+" : "") + juce::String (v) + " cents";
}

EditorVeu::EditorVeu (EstatEstudi& e, ServeiVeu& s) : SeccioEstudi (e), servei (s)
{
    setWantsKeyboardFocus (true);
    for (int i = 0; i < numEines; ++i)
    {
        auto& b = btEines[i];
        addAndMakeVisible (b);
        b.setTooltip (nomEina (i) + " (tecla "_ca + juce::String (i + 1) + ")");
        b.onClick = [this, i] { estat.veu.eina = i; actualitzaBotons(); repaint(); };
    }
    for (auto* b : { &btOna, &btEscala, &btAB, &btAjusta, &btTanca }) addAndMakeVisible (b);
    btOna.setTooltip ("Mostra l'ona de la gravació de fons"_ca);
    btOna.onClick = [this] { estat.veu.ona = ! estat.veu.ona; actualitzaBotons(); repaint(); };
    btEscala.setTooltip ("Marca les notes de l'escala de la cançó"_ca);
    btEscala.onClick = [this] { estat.veu.escala = ! estat.veu.escala; actualitzaBotons(); repaint(); };
    btAB.setTooltip ("Encès: sona la veu corregida. Apagat: sona l'original (comparació A/B). Tecla B."_ca);
    btAB.onClick = [this] { if (onCommutaAB) onCommutaAB(); };
    btAjusta.setTooltip ("Encaixa la vista a les notes"_ca);
    btAjusta.onClick = [this] { ajustaVista(); };
    btTanca.setTooltip ("Amaga el Vocal Editor"_ca);
    btTanca.onClick = [this] { if (onTanca) onTanca(); };
    actualitzaBotons();
}

void EditorVeu::actualitzaBotons()
{
    for (int i = 0; i < numEines; ++i) btEines[i].setToggleState (estat.veu.eina == i, juce::dontSendNotification);
    btOna.setToggleState (estat.veu.ona, juce::dontSendNotification);
    btEscala.setToggleState (estat.veu.escala, juce::dontSendNotification);
    bool actiu = false;
    if (auto* t = estat.pista (estat.veu.pista)) actiu = mibase::veuActiva (*t);
    btAB.setToggleState (actiu, juce::dontSendNotification);
    btAB.setButtonText (actiu ? "Corregit" : "Original");
}

void EditorVeu::changeListenerCallback (juce::ChangeBroadcaster*)
{
    actualitzaBotons();
    repaint();
}

void EditorVeu::resized()
{
    auto r = zonaBarra().reduced (8, 5);
    btTanca.setBounds (r.removeFromRight (28));
    r.removeFromRight (6);
    btAjusta.setBounds (r.removeFromRight (70));
    r.removeFromRight (6);
    btAB.setBounds (r.removeFromRight (84));
    r.removeFromRight (6);
    btEscala.setBounds (r.removeFromRight (66));
    r.removeFromRight (4);
    btOna.setBounds (r.removeFromRight (52));
    r.removeFromLeft (150);   // el títol
    for (int i = 0; i < numEines; ++i)
    {
        btEines[i].setBounds (r.removeFromLeft (76));
        r.removeFromLeft (3);
    }
}

const mibase::Clip* EditorVeu::clip()
{
    auto* c = estat.clip ({ estat.veu.pista, estat.veu.clip });
    if (c != nullptr && c->presa == estat.veu.presa && ! c->midi) return c;
    if (auto* t = estat.pista (estat.veu.pista))
        for (int k = 0; k < (int) t->clips.size(); ++k)
            if (t->clips[(size_t) k].presa == estat.veu.presa) { estat.veu.clip = k; return &t->clips[(size_t) k]; }
    return nullptr;
}

const vv::Corbes* EditorVeu::corbes()
{
    auto* s = presa();
    auto a = s != nullptr ? servei.analisi (s->id) : nullptr;
    if (a == nullptr || ! s->veu.analitzada) return nullptr;
    if (! corbesValides || a != analisiCorbes || ! (s->veu == veuCorbes))
    {
        cacheCorbes = vv::corbes (*a, s->veu);
        analisiCorbes = a;
        veuCorbes = s->veu;
        corbesValides = true;
        rmsMax = 1e-6f;
        for (auto v : a->rms) rmsMax = std::max (rmsMax, v);
    }
    return &cacheCorbes;
}

void EditorVeu::ajustaVista()
{
    auto* s = presa();
    const auto* c = clip();
    if (s == nullptr || c == nullptr || getWidth() <= ampleTeclat) return;
    const double ini = c->iniciVisible(), fi = c->fiVisible();
    t0 = ini - 0.05 * (fi - ini);
    pxs = juce::jlimit (5.0, 4000.0, (double) zonaNotes().getWidth() / juce::jmax (0.1, (fi - ini) * 1.1));
    double lo = 200.0, hi = -200.0;
    for (auto& n : s->veu.notes)
    {
        if (c->pos + n.fi < ini || c->pos + n.ini > fi) continue;
        lo = std::min ({ lo, n.centre, vv::centreFinal (n) });
        hi = std::max ({ hi, n.centre, vv::centreFinal (n) });
    }
    if (lo > hi) { lo = 55.0; hi = 72.0; }
    const double files = juce::jmax (1.0, zonaNotes().getHeight() / (double) altSemi);
    if (hi - lo + 6.0 > files) altSemi = (float) juce::jlimit (6.0, 40.0, zonaNotes().getHeight() / (hi - lo + 6.0));
    midiDalt = (hi + lo) * 0.5 + zonaNotes().getHeight() / (double) altSemi * 0.5;
    vistaFeta = true;
    repaint();
}

void EditorVeu::tic()
{
    auto* s = presa();
    if (s == nullptr) return;
    if (! vistaFeta && s->veu.analitzada) ajustaVista();
    if (estat.capcal != ultimCapcal || servei.progres (s->id) >= 0.0f || servei.treballant (s->id))
    {
        ultimCapcal = estat.capcal;
        repaint();
    }
}

//==============================================================================
juce::Rectangle<float> EditorVeu::rectNota (const mibase::Clip& c, const NotaVeu& n) const
{
    const float x0 = xDe (c.pos + n.ini), x1 = xDe (c.pos + n.fi);
    const float y = yDe (vv::centreFinal (n));
    const float h = juce::jmax (8.0f, altSemi * 1.1f);
    return { x0, y - h * 0.5f, juce::jmax (2.0f, x1 - x0), h };
}

bool EditorVeu::triada (int k) const
{
    return std::find (estat.veu.triades.begin(), estat.veu.triades.end(), k) != estat.veu.triades.end();
}

void EditorVeu::triaSola (int k) { estat.veu.triades = { k }; }

void EditorVeu::commuta (int k)
{
    auto& t = estat.veu.triades;
    if (auto it = std::find (t.begin(), t.end(), k); it != t.end()) t.erase (it);
    else t.push_back (k);
}

int EditorVeu::notaA (juce::Point<int> p)
{
    auto* s = presa();
    const auto* c = clip();
    if (s == nullptr || c == nullptr) return -1;
    for (int k = (int) s->veu.notes.size() - 1; k >= 0; --k)
        if (rectNota (*c, s->veu.notes[(size_t) k]).expanded (0.0f, 3.0f).contains (p.toFloat())) return k;
    return -1;
}

//==============================================================================
void EditorVeu::paint (juce::Graphics& g)
{
    g.fillAll (tema::fons);
    // Barra.
    {
        auto b = zonaBarra();
        g.setColour (tema::lateral);
        g.fillRect (b);
        g.setColour (tema::linia);
        g.fillRect (b.getX(), b.getBottom() - 1, b.getWidth(), 1);
        g.setColour (tema::veu);
        g.setFont (dibuix::lletra (13.0f, true));
        auto t = b.reduced (12, 0).removeFromLeft (150);
        g.drawText ("Vocal Editor", t, juce::Justification::centredLeft);
    }

    auto* s = presa();
    const auto* c = clip();
    const auto zona = zonaNotes();
    if (s == nullptr || c == nullptr)
    {
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (13.0f));
        g.drawFittedText ("Fes doble clic en un clip d'una pista amb el Vocal Editor (o obre'l des dels inserts de la pista)."_ca,
                          zona.reduced (20), juce::Justification::centred, 3);
        return;
    }

    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (zona);
        pintaGraella (g, *c);
        if (estat.veu.ona) pintaOna (g, *c);
        pintaNotes (g, *c);
        pintaCorba (g, *c);
        // Capçal.
        const float xc = xDe (estat.capcal);
        g.setColour (tema::accent);
        g.fillRect (xc - 0.5f, (float) zona.getY(), 1.5f, (float) zona.getHeight());
        // Requadre.
        if (gest == Gest::Requadre)
        {
            const auto r = juce::Rectangle<int> (inici, ratoli).toFloat();
            g.setColour (tema::accent.withAlpha (0.12f));
            g.fillRect (r);
            g.setColour (tema::accent.withAlpha (0.6f));
            g.drawRect (r, 1.0f);
        }
    }
    pintaRegla (g);
    pintaTeclat (g);

    // Estat de l'anàlisi.
    const float p = servei.progres (s->id);
    if (p >= 0.0f || ! s->veu.analitzada)
    {
        auto r = zona.withSizeKeepingCentre (300, 54);
        g.setColour (tema::superficie2.withAlpha (0.95f));
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (13.0f));
        g.drawText ("Analitzant la veu… "_ca + (p >= 0.0f ? juce::String ((int) (p * 100.0f)) + " %" : juce::String()),
                    r.removeFromTop (32), juce::Justification::centred);
        auto barra = r.reduced (20, 6).removeFromTop (6).toFloat();
        g.setColour (tema::linia);
        g.fillRoundedRectangle (barra, 3.0f);
        g.setColour (tema::veu);
        g.fillRoundedRectangle (barra.withWidth (barra.getWidth() * juce::jlimit (0.0f, 1.0f, p)), 3.0f);
    }
    else if (s->veu.notes.empty())
    {
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (13.0f));
        g.drawText ("No s'hi ha trobat cap nota cantada."_ca, zona, juce::Justification::centred);
    }
}

void EditorVeu::pintaGraella (juce::Graphics& g, const mibase::Clip& c)
{
    const auto zona = zonaNotes();
    const auto ton = mibase::harmonia::tonalitatEfectiva (estat.projecte);
    const int nDalt = (int) std::ceil (midiDalt), nBaix = (int) std::floor (midiDe ((float) zona.getBottom()));
    for (int n = nBaix; n <= nDalt; ++n)
    {
        const float y0 = yDe (n + 0.5), y1 = yDe (n - 0.5);
        const bool negra = mibase::midi::esNegra (n);
        juce::Colour col = negra ? tema::fons.darker (0.25f) : tema::fons;
        if (estat.veu.escala && mibase::harmonia::aEscala (n, ton)) col = col.brighter (0.12f);
        g.setColour (col);
        g.fillRect ((float) zona.getX(), y0, (float) zona.getWidth(), y1 - y0);
        g.setColour (juce::Colours::white.withAlpha ((((n % 12) + 12) % 12) == 0 ? 0.10f : 0.03f));
        g.fillRect ((float) zona.getX(), y1 - 0.5f, (float) zona.getWidth(), 1.0f);
    }
    // Compassos i temps.
    const double spc = mibase::edicio::segonsPerCompas (estat.projecte);
    const double spt = spc / juce::jmax (1, estat.projecte.compasNum);
    const double tf = tDe ((float) zona.getRight());
    for (double t = std::floor (t0 / spt) * spt; t <= tf; t += spt)
    {
        const bool compas = std::abs (std::remainder (t, spc)) < spt * 0.25;
        if (! compas && spt * pxs < 12.0) continue;
        g.setColour (juce::Colours::white.withAlpha (compas ? 0.12f : 0.04f));
        g.fillRect (xDe (t), (float) zona.getY(), 1.0f, (float) zona.getHeight());
    }
    // Fora de la finestra del clip: més fosc.
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    const float xa = xDe (c.iniciVisible()), xb = xDe (c.fiVisible());
    if (xa > zona.getX()) g.fillRect ((float) zona.getX(), (float) zona.getY(), xa - zona.getX(), (float) zona.getHeight());
    if (xb < zona.getRight()) g.fillRect (xb, (float) zona.getY(), zona.getRight() - xb, (float) zona.getHeight());
}

void EditorVeu::pintaOna (juce::Graphics& g, const mibase::Clip& c)
{
    auto it = estat.ones.find (estat.veu.presa);
    if (it == estat.ones.end() || it->second == nullptr) return;
    const auto& ona = *it->second;
    const auto zona = zonaNotes();
    const float mig = (float) zona.getY() + zona.getHeight() * 0.5f, alt = zona.getHeight() * 0.45f;
    juce::Path p;
    juce::Array<juce::Point<float>> baix;
    for (int x = zona.getX(); x < zona.getRight(); x += 2)
    {
        const double ta = tDe ((float) x) - c.pos, tb = tDe ((float) x + 2.0f) - c.pos;
        if (ta < c.inici || tb > c.fi) continue;
        const auto b0 = (size_t) juce::jmax (0.0, ta * ona.sr / OnaPresa::bloc), b1 = (size_t) juce::jmax (0.0, tb * ona.sr / OnaPresa::bloc);
        float mn = 0.0f, mx = 0.0f;
        for (size_t b = b0; b <= b1 && b < ona.maxs.size(); ++b) { mn = juce::jmin (mn, ona.mins[b]); mx = juce::jmax (mx, ona.maxs[b]); }
        if (p.isEmpty()) p.startNewSubPath ((float) x, mig - mx * alt);
        else p.lineTo ((float) x, mig - mx * alt);
        baix.add ({ (float) x, mig - mn * alt });
    }
    for (int i = baix.size() - 1; i >= 0; --i) p.lineTo (baix[i]);
    if (! p.isEmpty())
    {
        p.closeSubPath();
        g.setColour (tema::veu.withAlpha (0.10f));
        g.fillPath (p);
    }
}

void EditorVeu::pintaNotes (juce::Graphics& g, const mibase::Clip& c)
{
    auto* s = presa();
    auto a = servei.analisi (s->id);
    const auto zona = zonaNotes();
    for (int k = 0; k < (int) s->veu.notes.size(); ++k)
    {
        const auto& n = s->veu.notes[(size_t) k];
        const auto r = rectNota (c, n);
        if (r.getRight() < zona.getX() || r.getX() > zona.getRight()) continue;
        if (c.pos + n.fi < c.inici + c.pos || c.pos + n.ini > c.fi + c.pos) continue;
        const bool tri = triada (k);
        const double cf = vv::centreFinal (n);
        const auto col = colorAfinacio (cf);
        // On era abans de moure-la.
        if (std::abs (n.desplac) > 0.005)
        {
            auto r0 = r.withY (yDe (n.centre) - r.getHeight() * 0.5f);
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            juce::Path pr; pr.addRoundedRectangle (r0, 4.0f);
            float guions[] { 3.0f, 3.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (pr, pr, guions, 2);
            g.fillPath (pr);
        }
        // Blob: l'alçada segueix el volum de la nota.
        juce::Path blob;
        if (a != nullptr && r.getWidth() > 6.0f)
        {
            const int i0 = juce::jmax (0, (int) std::ceil (n.ini / vv::pas)), i1 = juce::jmin (a->punts(), (int) std::ceil (n.fi / vv::pas));
            const float yc = r.getCentreY(), hMax = r.getHeight() * 0.5f;
            const float guany = (float) std::pow (10.0, n.guanyDb / 20.0);
            std::vector<juce::Point<float>> dalt;
            const int pasI = juce::jmax (1, (int) (1.5 / (vv::pas * pxs)));
            for (int i = i0; i < i1; i += pasI)
            {
                const float v = std::sqrt (juce::jlimit (0.0f, 1.0f, a->rms[(size_t) i] * guany / rmsMax));
                dalt.push_back ({ xDe (c.pos + i * vv::pas), yc - (2.0f + v * (hMax - 2.0f)) });
            }
            if (dalt.size() >= 2)
            {
                blob.startNewSubPath (r.getX(), yc);
                for (auto& pt : dalt) blob.lineTo (pt);
                blob.lineTo (r.getRight(), yc);
                for (auto it = dalt.rbegin(); it != dalt.rend(); ++it) blob.lineTo (it->x, 2.0f * yc - it->y);
                blob.closeSubPath();
                blob = blob.createPathWithRoundedCorners (3.0f);
            }
        }
        if (blob.isEmpty()) blob.addRoundedRectangle (r, 4.0f);
        g.setColour (col.withAlpha (tri ? 0.85f : 0.55f));
        g.fillPath (blob);
        g.setColour (tri ? tema::text : col.brighter (0.3f));
        g.strokePath (blob, juce::PathStrokeType (tri ? 2.0f : 1.0f));
        // Nom i cents, si hi ha lloc.
        if (r.getWidth() > 44.0f)
        {
            g.setColour (tema::text.withAlpha (0.85f));
            g.setFont (dibuix::lletra (10.5f, tri));
            const double cents = (cf - std::round (cf)) * 100.0;
            g.drawText (nomNota (cf) + (std::abs (cents) >= 1.0 ? "  " + textCents (cents).upToFirstOccurrenceOf (" ", false, false) : juce::String()),
                        r.withY (r.getY() - 14.0f).withHeight (13.0f).toNearestInt(), juce::Justification::centredLeft, false);
        }
    }
}

void EditorVeu::pintaCorba (juce::Graphics& g, const mibase::Clip& c)
{
    const auto* cb = corbes();
    if (cb == nullptr) return;
    const auto zona = zonaNotes();
    const int n = (int) cb->editada.size();
    const int i0 = juce::jmax (0, (int) std::floor ((tDe ((float) zona.getX()) - c.pos) / vv::pas));
    const int i1 = juce::jmin (n, (int) std::ceil ((tDe ((float) zona.getRight()) - c.pos) / vv::pas) + 1);
    const int iIni = (int) std::ceil (c.inici / vv::pas), iFi = (int) std::floor (c.fi / vv::pas);
    auto traça = [&] (const std::vector<float>& v, bool nomesDiferent)
    {
        juce::Path p;
        bool dins = false;
        for (int i = juce::jmax (i0, iIni); i < juce::jmin (i1, iFi); ++i)
        {
            const float m = v[(size_t) i];
            const bool ok = ! std::isnan (m) && (! nomesDiferent || std::abs (cb->canvi[(size_t) i]) > 0.02f);
            if (! ok) { dins = false; continue; }
            const juce::Point<float> pt (xDe (c.pos + i * vv::pas), yDe (m));
            if (! dins) p.startNewSubPath (pt); else p.lineTo (pt);
            dins = true;
        }
        return p;
    };
    g.setColour (juce::Colours::white.withAlpha (0.28f));
    g.strokePath (traça (cb->original, true), juce::PathStrokeType (1.0f));
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.strokePath (traça (cb->editada, false), juce::PathStrokeType (1.4f, juce::PathStrokeType::curved));
}

void EditorVeu::pintaRegla (juce::Graphics& g)
{
    auto r = zonaRegla();
    g.setColour (tema::lateral);
    g.fillRect (r);
    g.setColour (tema::linia);
    g.fillRect (r.getX(), r.getBottom() - 1, r.getWidth(), 1);
    const double spc = mibase::edicio::segonsPerCompas (estat.projecte);
    int cada = 1;
    while (spc * cada * pxs < 50.0) cada *= 2;
    g.setFont (dibuix::mono (10.5f));
    const double tf = tDe ((float) r.getRight());
    for (long long k = (long long) std::floor (t0 / spc); k * spc <= tf; ++k)
    {
        if (k % cada != 0) continue;
        const float x = xDe (k * spc);
        g.setColour (tema::linia2);
        g.fillRect (x, (float) r.getY() + 8.0f, 1.0f, (float) r.getHeight() - 8.0f);
        g.setColour (tema::textMig);
        const auto text = juce::String (mibase::edicio::textPosicio (estat.projecte, k * spc)).upToFirstOccurrenceOf (".", false, false);
        g.drawText (text, juce::Rectangle<float> (x + 3.0f, (float) r.getY(), 40.0f, (float) r.getHeight()), juce::Justification::centredLeft);
    }
    const float xc = xDe (estat.capcal);
    if (xc >= r.getX())
    {
        g.setColour (tema::accent);
        juce::Path tri;
        tri.addTriangle (xc - 5.0f, (float) r.getY() + 4.0f, xc + 5.0f, (float) r.getY() + 4.0f, xc, (float) r.getBottom() - 2.0f);
        g.fillPath (tri);
    }
}

void EditorVeu::pintaTeclat (juce::Graphics& g)
{
    auto r = zonaTeclat();
    g.setColour (tema::lateral);
    g.fillRect (getLocalBounds().withTrimmedTop (altBarra).removeFromTop (altRegla).removeFromLeft (ampleTeclat));
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (r);
    const int nDalt = (int) std::ceil (midiDalt), nBaix = (int) std::floor (midiDe ((float) r.getBottom()));
    for (int n = nBaix; n <= nDalt; ++n)
    {
        const float y0 = yDe (n + 0.5), y1 = yDe (n - 0.5);
        const bool negra = mibase::midi::esNegra (n);
        g.setColour (negra ? juce::Colour (0xff202026) : juce::Colour (0xffd9d7de));
        g.fillRect ((float) r.getX(), y0, (float) r.getWidth() - (negra ? 18.0f : 0.0f), y1 - y0);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect ((float) r.getX(), y1 - 0.5f, (float) r.getWidth(), 0.5f);
        if ((((n % 12) + 12) % 12) == 0 && altSemi >= 8.0f)
        {
            g.setColour (juce::Colour (0xff333338));
            g.setFont (dibuix::lletra (juce::jmin (11.0f, altSemi)));
            g.drawText ("C" + juce::String (n / 12 - 1), juce::Rectangle<float> ((float) r.getX(), y0, (float) r.getWidth() - 4.0f, y1 - y0),
                        juce::Justification::centredRight);
        }
    }
    g.setColour (tema::linia);
    g.fillRect (r.getRight() - 1, r.getY(), 1, r.getHeight());
}

//==============================================================================
void EditorVeu::mouseMove (const juce::MouseEvent& e)
{
    const int k = zonaNotes().contains (e.getPosition()) ? notaA (e.getPosition()) : -1;
    setMouseCursor (k < 0 ? juce::MouseCursor::NormalCursor
                          : estat.veu.eina == Fletxa ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::PointingHandCursor);
}

void EditorVeu::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    estat.focusEditor = true;
    inici = ratoli = e.getPosition();
    gest = Gest::Res;
    auto* s = presa();
    if (zonaRegla().contains (e.getPosition()) || (zonaTeclat().contains (e.getPosition()) && false))
    {
        gest = Gest::Capcal;
        estat.mouCapcal (juce::jmax (0.0, tDe ((float) e.x)), true);
        return;
    }
    if (s == nullptr || ! zonaNotes().contains (e.getPosition()) || ! s->veu.analitzada) return;
    const int k = notaA (e.getPosition());
    if (k < 0)
    {
        if (! e.mods.isCommandDown() && ! e.mods.isShiftDown()) estat.veu.triades.clear();
        gest = Gest::Requadre;
        estat.avisa();
        return;
    }
    if (e.mods.isCommandDown()) commuta (k);
    else if (e.mods.isShiftDown()) { if (! triada (k)) estat.veu.triades.push_back (k); }
    else if (! triada (k)) triaSola (k);
    if (! triada (k)) { estat.avisa(); return; }
    gest = Gest::Notes;
    notaSota = k;
    notesInici = s->veu.notes;
    comencaCanvi();
    estat.avisa();
}

void EditorVeu::mouseDrag (const juce::MouseEvent& e)
{
    ratoli = e.getPosition();
    if (gest == Gest::Capcal) { estat.mouCapcal (juce::jmax (0.0, tDe ((float) e.x)), true); return; }
    if (gest == Gest::Requadre)
    {
        auto* s = presa();
        const auto* c = clip();
        if (s == nullptr || c == nullptr) return;
        const auto r = juce::Rectangle<int> (inici, ratoli).toFloat();
        std::vector<int> t;
        for (int k = 0; k < (int) s->veu.notes.size(); ++k)
            if (rectNota (*c, s->veu.notes[(size_t) k]).intersects (r)) t.push_back (k);
        estat.veu.triades = t;
        estat.avisa();
        return;
    }
    if (gest == Gest::Notes && e.getDistanceFromDragStart() > 2) aplicaArrossegament (e);
}

void EditorVeu::aplicaArrossegament (const juce::MouseEvent& e)
{
    restaura();
    auto* s = presa();
    if (s == nullptr) return;
    const double dy = (double) e.getDistanceFromDragStartY();
    juce::String info;
    for (int k : estat.veu.triades)
    {
        if (k < 0 || k >= (int) s->veu.notes.size() || k >= (int) notesInici.size()) continue;
        auto& n = s->veu.notes[(size_t) k];
        const auto& n0 = notesInici[(size_t) k];
        switch (estat.veu.eina)
        {
            case Fletxa:
            {
                double desti = vv::centreFinal (n0) - dy / altSemi;
                if (! e.mods.isAltDown()) desti = std::round (desti);
                n.desplac = juce::jlimit (-24.0, 24.0, desti - n.centre);
                if (k == notaSota) info = nomNota (desti) + "  " + textCents ((desti - std::round (desti)) * 100.0);
                break;
            }
            case Vibrato:
                n.modulacio = juce::jlimit (0.0, 2.0, n0.modulacio - dy / 100.0);
                if (k == notaSota) info = "Vibrato " + juce::String ((int) std::lround (n.modulacio * 100.0)) + " %";
                break;
            case Deriva:
                n.deriva = juce::jlimit (0.0, 1.0, n0.deriva - dy / 150.0);
                if (k == notaSota) info = "Deriva " + juce::String ((int) std::lround (n.deriva * 100.0)) + " %";
                break;
            case Formant:
                n.formant = juce::jlimit (-12.0, 12.0, std::round ((n0.formant - dy / 15.0) * 10.0) / 10.0);
                if (k == notaSota) info = "Formant " + num (n.formant, 1) + " st";
                break;
            case Volum:
                n.guanyDb = juce::jlimit (-30.0, 12.0, std::round ((n0.guanyDb - dy / 6.0) * 10.0) / 10.0);
                if (k == notaSota) info = "Volum " + num (n.guanyDb, 1) + " dB";
                break;
            default: break;
        }
    }
    estat.info = info;
    estat.avisa();
}

void EditorVeu::mouseUp (const juce::MouseEvent&)
{
    if (gest == Gest::Notes) acabaCanvi ("Vocal Editor: notes editades"_ca);
    gest = Gest::Res;
    notaSota = -1;
    repaint();
}

void EditorVeu::restableix (int k)
{
    auto* s = presa();
    if (s == nullptr || k < 0 || k >= (int) s->veu.notes.size()) return;
    auto& n = s->veu.notes[(size_t) k];
    switch (estat.veu.eina)
    {
        case Fletxa:
        {
            const auto ton = mibase::harmonia::tonalitatEfectiva (estat.projecte);
            n.desplac = vv::notaObjectiu (n.centre, estat.veu.corrDesti == 1 ? vv::Desti::Escala : vv::Desti::Cromatic, ton) - n.centre;
            break;
        }
        case Vibrato: n.modulacio = 1.0; break;
        case Deriva: n.deriva = 1.0; break;
        case Formant: n.formant = 0.0; break;
        case Volum: n.guanyDb = 0.0; break;
        default: break;
    }
}

void EditorVeu::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int k = notaA (e.getPosition());
    if (k < 0) return;
    if (! triada (k)) triaSola (k);
    comencaCanvi();
    restaura();
    for (int t : estat.veu.triades) restableix (t);
    acabaCanvi (estat.veu.eina == Fletxa ? "Vocal Editor: nota afinada"_ca : "Vocal Editor: valor original"_ca);
}

void EditorVeu::mouTriades (double semitons, bool lliure, const juce::String& evolucio)
{
    auto* s = presa();
    if (s == nullptr || estat.veu.triades.empty()) return;
    comencaCanvi();
    restaura();
    for (int k : estat.veu.triades)
    {
        if (k < 0 || k >= (int) s->veu.notes.size()) continue;
        auto& n = s->veu.notes[(size_t) k];
        double desti = vv::centreFinal (n) + semitons;
        if (! lliure) desti = std::round (desti);
        n.desplac = juce::jlimit (-24.0, 24.0, desti - n.centre);
    }
    acabaCanvi (evolucio);
}

void EditorVeu::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float d = std::abs (w.deltaY) > std::abs (w.deltaX) ? w.deltaY : w.deltaX;
    if (e.mods.isCommandDown())
    {
        const double t = tDe ((float) e.x);
        pxs = juce::jlimit (5.0, 4000.0, pxs * std::pow (1.25, d * 4.0));
        t0 = t - (e.x - zonaNotes().getX()) / pxs;
    }
    else if (e.mods.isAltDown())
    {
        const double m = midiDe ((float) e.y);
        altSemi = juce::jlimit (5.0f, 48.0f, altSemi * (float) std::pow (1.2, d * 4.0));
        midiDalt = m + (e.y - zonaNotes().getY()) / (double) altSemi;
    }
    else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
        t0 -= d * 300.0 / pxs;
    else
        midiDalt = juce::jlimit (30.0, 110.0, midiDalt + d * 12.0);
    repaint();
}

bool EditorVeu::keyPressed (const juce::KeyPress& k)
{
    const int codi = k.getKeyCode();
    const auto mods = k.getModifiers();
    auto* s = presa();
    if (s == nullptr) return false;
    if (codi >= '1' && codi < '1' + numEines && ! mods.isAnyModifierKeyDown()) { estat.veu.eina = codi - '1'; actualitzaBotons(); repaint(); return true; }
    if (codi == juce::KeyPress::upKey || codi == juce::KeyPress::downKey)
    {
        const double d = (codi == juce::KeyPress::upKey ? 1.0 : -1.0) * (mods.isAltDown() ? 0.1 : 1.0);
        mouTriades (d, mods.isAltDown(), "Vocal Editor: notes mogudes"_ca);
        return true;
    }
    if (mods.isCommandDown() && (codi == 'A' || codi == 'a'))
    {
        estat.veu.triades.clear();
        for (int i = 0; i < (int) s->veu.notes.size(); ++i) estat.veu.triades.push_back (i);
        estat.avisa();
        return true;
    }
    if (codi == juce::KeyPress::escapeKey) { estat.veu.triades.clear(); estat.avisa(); return true; }
    if ((codi == 'B' || codi == 'b') && ! mods.isAnyModifierKeyDown()) { if (onCommutaAB) onCommutaAB(); return true; }
    if (codi == juce::KeyPress::deleteKey || codi == juce::KeyPress::backspaceKey)
    {
        if (estat.veu.triades.empty()) return true;
        comencaCanvi();
        restaura();
        vv::restaura (s->veu, estat.veu.triades);
        acabaCanvi ("Vocal Editor: notes a l'original"_ca);
        return true;
    }
    return false;
}

//==============================================================================
// PanellVeu
//==============================================================================
PanellVeu::Camp::Camp (const juce::String& etiqueta, double minim, double maxim, double p, int decimals, const juce::String& u)
    : text (etiqueta), unitat (u), mn (minim), mx (maxim), pas (p), dec (decimals)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void PanellVeu::Camp::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    const float alfa = isEnabled() ? 1.0f : 0.4f;
    g.setColour (tema::suau.withAlpha (alfa));
    g.setFont (dibuix::lletra (12.0f));
    g.drawText (text, r.removeFromLeft (r.getWidth() - 96), juce::Justification::centredLeft, true);
    auto caixa = r.reduced (0, 2).toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.35f * alfa));
    g.fillRoundedRectangle (caixa, 3.0f);
    g.setColour ((arrossega ? tema::accent : tema::linia2).withAlpha (alfa));
    g.drawRoundedRectangle (caixa.reduced (0.5f), 3.0f, 1.0f);
    if (editor != nullptr) return;
    g.setColour (tema::text.withAlpha (alfa));
    g.setFont (dibuix::mono (12.0f));
    const auto v = igual && isEnabled() ? num (valor, dec) + " " + unitat : juce::String::fromUTF8 ("—");
    g.drawText (v, caixa.reduced (6.0f, 0.0f).toNearestInt(), juce::Justification::centredRight);
}

void PanellVeu::Camp::resized()
{
    if (editor != nullptr) editor->setBounds (getLocalBounds().removeFromRight (96).reduced (1, 3));
}

void PanellVeu::Camp::mouseDown (const juce::MouseEvent&) { valor0 = valor; arrossega = false; }

void PanellVeu::Camp::mouseDrag (const juce::MouseEvent& e)
{
    if (! arrossega && e.getDistanceFromDragStart() < 3) return;
    if (! arrossega) { arrossega = true; if (onComenca) onComenca(); }
    const double fi = e.mods.isShiftDown() ? 0.1 : 1.0;
    double v = valor0 - e.getDistanceFromDragStartY() * pas * fi * (pas < 0.05 ? 2.0 : 0.5);
    v = juce::jlimit (mn, mx, std::round (v / (pas * fi)) * pas * fi);
    valor = v;
    igual = true;
    if (onCanvi) onCanvi (v);
    repaint();
}

void PanellVeu::Camp::mouseUp (const juce::MouseEvent&)
{
    if (arrossega) { arrossega = false; if (onAcaba) onAcaba(); repaint(); }
}

void PanellVeu::Camp::mouseDoubleClick (const juce::MouseEvent&)
{
    if (! isEnabled()) return;
    editor = std::make_unique<juce::TextEditor>();
    editor->setFont (dibuix::mono (12.0f));
    editor->setJustification (juce::Justification::centredRight);
    editor->setText (num (valor, dec), false);
    editor->selectAll();
    addAndMakeVisible (*editor);
    resized();
    editor->grabKeyboardFocus();
    juce::Component::SafePointer<Camp> jo (this);
    auto tanca = [jo] (bool aplica)
    {
        juce::MessageManager::callAsync ([jo, aplica]
        {
            if (jo == nullptr || jo->editor == nullptr) return;
            const auto t = jo->editor->getText().replaceCharacter (',', '.').trim();
            jo->editor.reset();
            if (aplica && t.isNotEmpty() && t.containsAnyOf ("0123456789"))
            {
                const double v = juce::jlimit (jo->mn, jo->mx, t.getDoubleValue());
                jo->valor = v; jo->igual = true;
                if (jo->onComenca) jo->onComenca();
                if (jo->onCanvi) jo->onCanvi (v);
                if (jo->onAcaba) jo->onAcaba();
            }
            jo->repaint();
        });
    };
    editor->onReturnKey = [tanca] { tanca (true); };
    editor->onEscapeKey = [tanca] { tanca (false); };
    editor->onFocusLost = [tanca] { tanca (true); };
    repaint();
}

//==============================================================================
PanellVeu::PanellVeu (EstatEstudi& e, ServeiVeu& s, EditorVeu& ed) : SeccioEstudi (e), servei (s), editor (ed)
{
    auto objectiu = [this] (const NotaVeu& n)
    {
        return vv::notaObjectiu (n.centre, estat.veu.corrDesti == 1 ? vv::Desti::Escala : vv::Desti::Cromatic,
                                 mibase::harmonia::tonalitatEfectiva (estat.projecte));
    };
    struct Def { Camp* c; std::function<void (NotaVeu&, double)> f; };
    for (auto& d : { Def { &cDesplac, [] (NotaVeu& n, double v) { n.desplac = v; } },
                     Def { &cCentre, [objectiu] (NotaVeu& n, double v) { n.desplac = (objectiu (n) - n.centre) * v / 100.0; } },
                     Def { &cModulacio, [] (NotaVeu& n, double v) { n.modulacio = v / 100.0; } },
                     Def { &cDeriva, [] (NotaVeu& n, double v) { n.deriva = v / 100.0; } },
                     Def { &cFormant, [] (NotaVeu& n, double v) { n.formant = v; } },
                     Def { &cVolum, [] (NotaVeu& n, double v) { n.guanyDb = v; } } })
    {
        addAndMakeVisible (d.c);
        auto f = d.f;
        d.c->onComenca = [this] { enCampComenca(); };
        d.c->onCanvi = [this, f] (double v) { enCampCanvi ([f, v] (NotaVeu& n) { f (n, v); }); };
        d.c->onAcaba = [this] { acabaCanvi ("Vocal Editor: valors de nota"_ca); };
    }
    for (auto* c : { &aCentre, &aDeriva, &aVibrato, &aTransicio }) addAndMakeVisible (c);
    aCentre.onCanvi = [this] (double v) { estat.veu.corrCentre = v / 100.0; };
    aDeriva.onCanvi = [this] (double v) { estat.veu.corrDeriva = v / 100.0; };
    aVibrato.onCanvi = [this] (double v) { estat.veu.corrVibrato = v / 100.0; };
    aTransicio.onComenca = [this] { comencaCanvi(); };
    aTransicio.onCanvi = [this] (double v)
    {
        restaura();
        if (auto* p = estat.presaVeu()) p->veu.transicio = v / 1000.0;
        estat.avisa();
    };
    aTransicio.onAcaba = [this] { acabaCanvi ("Vocal Editor: transició entre notes"_ca); };

    for (auto* b : { &btDesti, &btCorregeix, &btRestaura, &btAB }) addAndMakeVisible (b);
    btDesti.setTooltip ("On van les notes: al semitò més proper (cromàtic) o a la nota més propera de l'escala de la cançó"_ca);
    btDesti.onClick = [this] { estat.veu.corrDesti = estat.veu.corrDesti == 1 ? 0 : 1; actualitzaCamps(); };
    btCorregeix.setTooltip ("Aplica la correcció a les notes triades (o a totes, si no n'hi ha cap de triada)"_ca);
    btCorregeix.onClick = [this]
    {
        auto* p = estat.presaVeu();
        if (p == nullptr) return;
        vv::Correccio c;
        c.centre = estat.veu.corrCentre;
        c.deriva = estat.veu.corrDeriva;
        c.vibrato = estat.veu.corrVibrato;
        c.desti = estat.veu.corrDesti == 1 ? vv::Desti::Escala : vv::Desti::Cromatic;
        c.tonalitat = mibase::harmonia::tonalitatEfectiva (estat.projecte);
        comencaCanvi();
        restaura();
        p = estat.presaVeu();
        if (p != nullptr) vv::corregeix (p->veu, triades(), c);
        acabaCanvi ("Vocal Editor: correcció automàtica"_ca);
    };
    btRestaura.setTooltip ("Torna les notes triades (o totes) a com es van cantar"_ca);
    btRestaura.onClick = [this]
    {
        comencaCanvi();
        restaura();
        if (auto* p = estat.presaVeu()) vv::restaura (p->veu, triades());
        acabaCanvi ("Vocal Editor: tornat a l'original"_ca);
    };
    btAB.setTooltip ("Escolta la gravació original sense cap correcció (tecla B a l'editor)"_ca);
    btAB.onClick = [this] { if (onCommutaAB) onCommutaAB(); };
    actualitzaCamps();
}

std::vector<int> PanellVeu::triades()
{
    std::vector<int> t;
    auto* p = estat.presaVeu();
    if (p == nullptr) return t;
    for (int k : estat.veu.triades) if (k >= 0 && k < (int) p->veu.notes.size()) t.push_back (k);
    return t;
}

void PanellVeu::enCampComenca() { comencaCanvi(); }

void PanellVeu::enCampCanvi (const std::function<void (NotaVeu&)>& f)
{
    restaura();
    auto* p = estat.presaVeu();
    if (p == nullptr) return;
    for (int k : triades()) f (p->veu.notes[(size_t) k]);
    estat.avisa();
}

void PanellVeu::changeListenerCallback (juce::ChangeBroadcaster*)
{
    actualitzaCamps();
    repaint();
}

void PanellVeu::actualitzaCamps()
{
    auto* p = estat.presaVeu();
    const auto t = triades();
    const bool hiHa = p != nullptr && ! t.empty();
    for (auto* c : { &cDesplac, &cCentre, &cModulacio, &cDeriva, &cFormant, &cVolum }) c->setEnabled (hiHa);
    if (hiHa)
    {
        const auto ton = mibase::harmonia::tonalitatEfectiva (estat.projecte);
        auto valor = [&] (Camp& c, const std::function<double (const NotaVeu&)>& f)
        {
            const double v0 = f (p->veu.notes[(size_t) t[0]]);
            bool igual = true;
            for (int k : t) if (std::abs (f (p->veu.notes[(size_t) k]) - v0) > 1e-6) igual = false;
            c.setValor (v0, igual);
        };
        valor (cDesplac, [] (const NotaVeu& n) { return n.desplac; });
        valor (cCentre, [&] (const NotaVeu& n)
        {
            const double obj = vv::notaObjectiu (n.centre, estat.veu.corrDesti == 1 ? vv::Desti::Escala : vv::Desti::Cromatic, ton) - n.centre;
            return std::abs (obj) < 1e-6 ? 100.0 : juce::jlimit (0.0, 100.0, std::round (n.desplac / obj * 100.0));
        });
        valor (cModulacio, [] (const NotaVeu& n) { return std::round (n.modulacio * 100.0); });
        valor (cDeriva, [] (const NotaVeu& n) { return std::round (n.deriva * 100.0); });
        valor (cFormant, [] (const NotaVeu& n) { return n.formant; });
        valor (cVolum, [] (const NotaVeu& n) { return n.guanyDb; });
    }
    aCentre.setValor (estat.veu.corrCentre * 100.0);
    aDeriva.setValor (estat.veu.corrDeriva * 100.0);
    aVibrato.setValor (estat.veu.corrVibrato * 100.0);
    aTransicio.setEnabled (p != nullptr);
    if (p != nullptr) aTransicio.setValor (p->veu.transicio * 1000.0);
    const auto nomTon = juce::String::fromUTF8 (mibase::harmonia::nomTonalitat (mibase::harmonia::tonalitatEfectiva (estat.projecte)).c_str());
    btDesti.setButtonText (estat.veu.corrDesti == 1 ? "Destí: escala ("_ca + nomTon + ")" : "Destí: semitò més proper"_ca);
    btCorregeix.setButtonText (t.empty() ? "Corregeix-les totes"_ca : "Corregeix les triades"_ca);
    btRestaura.setButtonText (t.empty() ? "Totes a l'original"_ca : "Triades a l'original"_ca);
    bool actiu = false;
    if (auto* tr = estat.pista (estat.veu.pista)) actiu = mibase::veuActiva (*tr);
    btAB.setToggleState (! actiu, juce::dontSendNotification);
}

void PanellVeu::resized()
{
    auto r = getLocalBounds().reduced (14, 12);
    zonaInfo = r.removeFromTop (62);
    r.removeFromTop (6);
    zonaInspector = r.removeFromTop (20 + 6 * 16 + 4);
    for (auto* c : { &cDesplac, &cCentre, &cModulacio, &cDeriva, &cFormant, &cVolum }) c->setBounds (r.removeFromTop (26));
    r.removeFromTop (10);
    zonaAuto = r.removeFromTop (22);
    for (auto* c : { &aCentre, &aDeriva, &aVibrato, &aTransicio }) c->setBounds (r.removeFromTop (26));
    r.removeFromTop (4);
    btDesti.setBounds (r.removeFromTop (26));
    r.removeFromTop (6);
    btCorregeix.setBounds (r.removeFromTop (30));
    r.removeFromTop (6);
    btRestaura.setBounds (r.removeFromTop (26));
    r.removeFromTop (12);
    btAB.setBounds (r.removeFromTop (28));
}

void PanellVeu::paint (juce::Graphics& g)
{
    g.fillAll (tema::lateral);
    g.setColour (tema::linia);
    g.fillRect (0, 0, 1, getHeight());

    auto* p = estat.presaVeu();
    auto r = zonaInfo;
    dibuix::pintaEtiquetaSeccio (g, "Vocal Editor", r.removeFromTop (20));
    g.setFont (dibuix::lletra (12.5f));
    g.setColour (tema::text);
    g.drawText (p != nullptr ? dibuix::utf8 (p->nom) : juce::String(), r.removeFromTop (20), juce::Justification::centredLeft, true);
    juce::String estatText;
    if (p != nullptr)
    {
        const float pr = servei.progres (p->id);
        bool actiu = false;
        if (auto* t = estat.pista (estat.veu.pista)) actiu = mibase::veuActiva (*t);
        if (pr >= 0.0f) estatText = "Analitzant… "_ca + juce::String ((int) (pr * 100.0f)) + " %";
        else if (! p->veu.analitzada) estatText = "Preparant l'anàlisi…"_ca;
        else if (servei.treballant (p->id)) estatText = juce::String (p->veu.notes.size()) + " notes · preparant el so…"_ca;
        else estatText = juce::String (p->veu.notes.size()) + " notes · "_ca + (actiu ? "sona corregit"_ca : "sona l'original (apagat)"_ca);
    }
    g.setColour (tema::suau);
    g.setFont (dibuix::lletra (11.5f));
    g.drawText (estatText, r.removeFromTop (18), juce::Justification::centredLeft, true);

    // Inspector.
    auto z = zonaInspector;
    const auto t = triades();
    dibuix::pintaEtiquetaSeccio (g, t.size() > 1 ? juce::String ((int) t.size()) + " notes triades"_ca : juce::String ("Nota triada"), z.removeFromTop (20));
    auto linia = [&] (const juce::String& a, const juce::String& b)
    {
        auto f = z.removeFromTop (16);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.5f));
        g.drawText (a, f.removeFromLeft (92), juce::Justification::centredLeft);
        g.setColour (tema::text);
        g.drawText (b, f, juce::Justification::centredLeft, true);
    };
    if (p == nullptr || t.empty())
    {
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.5f));
        g.drawFittedText ("Clica una nota (Ctrl o Maj per triar-ne més, o fes un requadre). Ctrl+A les tria totes."_ca,
                          z, juce::Justification::topLeft, 4);
    }
    else
    {
        const auto& n = p->veu.notes[(size_t) t[0]];
        const double cf = vv::centreFinal (n);
        const auto* c = estat.clip ({ estat.veu.pista, estat.veu.clip });
        const double pos = (c != nullptr ? c->pos : 0.0) + n.ini;
        if (t.size() == 1)
        {
            linia ("Nota cantada"_ca, EditorVeu::nomNota (n.centre) + "  (" + EditorVeu::textCents ((n.centre - std::round (n.centre)) * 100.0) + ")");
            linia ("Freqüència"_ca, num (vv::hzDeMidi (n.centre), 1) + " Hz");
            linia ("Sonarà"_ca, EditorVeu::nomNota (cf) + "  (" + EditorVeu::textCents ((cf - std::round (cf)) * 100.0) + ")");
            linia ("Objectiu"_ca, EditorVeu::nomNota (vv::notaObjectiu (n.centre, estat.veu.corrDesti == 1 ? vv::Desti::Escala : vv::Desti::Cromatic,
                                                                         mibase::harmonia::tonalitatEfectiva (estat.projecte))));
            linia ("Posició"_ca, juce::String (mibase::edicio::textPosicio (estat.projecte, pos, true)) + "  ·  "_ca + num (pos, 2) + " s");
            linia ("Durada"_ca, num (n.fi - n.ini, 2) + " s");
        }
        else
        {
            double ini = 1e9, fi = -1e9;
            for (int k : t) { ini = std::min (ini, p->veu.notes[(size_t) k].ini); fi = std::max (fi, p->veu.notes[(size_t) k].fi); }
            linia ("Des de"_ca, juce::String (mibase::edicio::textPosicio (estat.projecte, (c != nullptr ? c->pos : 0.0) + ini, true)));
            linia ("Durada"_ca, num (fi - ini, 2) + " s");
        }
    }

    dibuix::pintaEtiquetaSeccio (g, "Correcció automàtica"_ca, zonaAuto);

    // Ajuda de baix.
    auto ajuda = getLocalBounds().reduced (14, 12).withTop (btAB.getBottom() + 12);
    if (ajuda.getHeight() > 40)
    {
        g.setColour (tema::suau.withAlpha (0.8f));
        g.setFont (dibuix::lletra (11.0f));
        g.drawFittedText ("Eines: 1 afinació · 2 vibrato · 3 deriva · 4 formant · 5 volum. Fletxes amunt/avall = un semitò (Alt = 10 cents). "
                          "Supr = la nota torna a l'original. Doble clic en un valor per escriure'l."_ca,
                          ajuda, juce::Justification::topLeft, 6);
    }
}
