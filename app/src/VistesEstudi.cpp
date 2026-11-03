#include "VistesEstudi.h"

using namespace mibase;
namespace ed = mibase::edicio;

namespace
{
    constexpr float mida = 7.0f;   // mida de les manetes

    juce::String ms (double segons) { return juce::String (juce::roundToInt (segons * 1000.0)) + " ms"; }

    juce::String posicio (const Projecte& p, double t, bool ambMs = false)
    {
        return dibuix::utf8 (ed::textPosicio (p, t, ambMs));
    }

    bool sona (const Projecte& p, const Pista& t)
    {
        const bool hiHaSolo = std::any_of (p.pistes.begin(), p.pistes.end(), [] (const Pista& x) { return x.solo; });
        return ! t.mut && (! hiHaSolo || t.solo);
    }

    void pintaCapcal (juce::Graphics& g, float x, float alt)
    {
        g.setColour (tema::text);
        g.fillRect (juce::Rectangle<float> (x - 0.75f, 0.0f, 1.5f, alt));
    }

    void pintaBucleSobre (juce::Graphics& g, const EstatEstudi& e, float alt)
    {
        const auto& b = e.projecte.bucle;
        if (! b.marcat) return;
        const float xa = e.x (ed::iniciBucle (e.projecte)), xb = e.x (ed::fiBucle (e.projecte));
        const auto color = b.actiu ? tema::guitarra : tema::suau;
        g.setColour (color.withAlpha (b.actiu ? 0.07f : 0.04f));
        g.fillRect (juce::Rectangle<float> (xa, 0.0f, xb - xa, alt));
        g.setColour (color.withAlpha (b.actiu ? 0.6f : 0.3f));
        g.fillRect (juce::Rectangle<float> (xa - 0.75f, 0.0f, 1.5f, alt));
        g.fillRect (juce::Rectangle<float> (xb - 0.75f, 0.0f, 1.5f, alt));
    }

    void pintaBotoPetit (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, bool actiu, juce::Colour colorActiu, bool rodo = false)
    {
        auto f = r.toFloat();
        g.setColour (actiu ? colorActiu : tema::superficie2);
        g.fillRoundedRectangle (f, 5.0f);
        g.setColour (actiu ? colorActiu : tema::linia2);
        g.drawRoundedRectangle (f.reduced (0.5f), 5.0f, 1.0f);
        if (rodo)
        {
            g.setColour (actiu ? tema::fons : tema::error.withAlpha (0.85f));
            g.fillEllipse (f.withSizeKeepingCentre (9.0f, 9.0f));
        }
        else
        {
            g.setColour (actiu ? tema::fons : tema::textMig);
            g.setFont (dibuix::lletra (12.0f, true));
            g.drawText (text, r, juce::Justification::centred);
        }
    }
}

//==============================================================================
// Regla
//==============================================================================
void Regla::paint (juce::Graphics& g)
{
    const auto& p = estat.projecte;
    const float w = (float) getWidth(), h = (float) getHeight();
    g.fillAll (tema::superficie);

    // Bucle.
    if (p.bucle.marcat)
    {
        const float xa = estat.x (ed::iniciBucle (p)), xb = estat.x (ed::fiBucle (p));
        const auto color = p.bucle.actiu ? tema::guitarra : tema::suau;
        g.setColour (color.withAlpha (p.bucle.actiu ? 0.28f : 0.14f));
        g.fillRect (juce::Rectangle<float> (xa, 0.0f, xb - xa, h * 0.45f));
        g.setColour (color.withAlpha (0.8f));
        g.fillRect (juce::Rectangle<float> (xa, h * 0.45f - 2.0f, xb - xa, 2.0f));
    }

    const double spt = ed::segonsPerTemps (p), spc = ed::segonsPerCompas (p);
    const double pxCompas = spc * estat.pxPerSegon;
    int cada = 1;
    while (pxCompas * cada < 36.0) cada *= 2;

    const int primerTemps = juce::jmax (0, (int) std::floor (estat.t (0.0f) / spt));
    const int darrerTemps = (int) std::ceil (estat.t (w) / spt);

    for (int t = primerTemps; t <= darrerTemps; ++t)
    {
        const float x = estat.x (t * spt);
        const bool esCompas = t % p.compasNum == 0;
        if (esCompas)
        {
            const int c = t / p.compasNum;
            const bool numera = c % cada == 0;
            g.setColour (numera ? tema::linia2 : tema::linia);
            g.fillRect (juce::Rectangle<float> (x, numera ? h * 0.35f : h * 0.6f, 1.0f, h));
            if (numera)
            {
                const int n = ed::numCompas (c);
                g.setColour (n == 0 ? tema::accent : tema::textMig);
                g.setFont (dibuix::mono (12.0f));
                g.drawText (juce::String (n), juce::Rectangle<float> (x + 4.0f, h * 0.3f, 40.0f, h * 0.45f), juce::Justification::centredLeft);
            }
        }
        else if (spt * estat.pxPerSegon >= 7.0)
        {
            g.setColour (tema::linia);
            g.fillRect (juce::Rectangle<float> (x, h * 0.75f, 1.0f, h * 0.25f));
        }
    }

    // Capçal.
    const float xc = estat.x (estat.capcal);
    juce::Path tri;
    tri.addTriangle (xc - 6.0f, h - 9.0f, xc + 6.0f, h - 9.0f, xc, h);
    g.setColour (tema::text);
    g.fillPath (tri);

    g.setColour (tema::linia);
    g.fillRect (0.0f, h - 1.0f, w, 1.0f);
}

void Regla::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        auto& b = estat.projecte.bucle;
        juce::PopupMenu m;
        m.addItem (1, b.actiu ? "Apaga el bucle" : "Encén el bucle"_ca, b.marcat);
        m.addItem (2, "Treu el bucle", b.marcat);
        m.addSeparator();
        m.addItem (3, "Capçal a l'inici"_ca);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this] (int r)
        {
            if (r == 1 || r == 2)
            {
                comencaCanvi();
                restaura();
                if (r == 1) estat.projecte.bucle.actiu = ! estat.projecte.bucle.actiu;
                else estat.projecte.bucle = {};
                acabaCanvi();
            }
            else if (r == 3)
            {
                estat.capcal = 0.0;
                estat.avisa();
            }
        });
        return;
    }

    compas0 = ed::compasA (estat.projecte, estat.t ((float) e.x));
    marcant = false;
    comencaCanvi();
}

void Regla::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    if (! marcant && std::abs (e.getDistanceFromDragStartX()) < 5) return;

    marcant = true;
    restaura();
    const int c1 = ed::compasA (estat.projecte, estat.t ((float) e.x));
    ed::marcaBucle (estat.projecte, compas0, c1);
    const auto& b = estat.projecte.bucle;
    estat.info = "Bucle: compassos " + juce::String (ed::numCompas (b.de)) + " a " + juce::String (ed::numCompas (b.a - 1));
    estat.avisa();
}

void Regla::mouseUp (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    if (marcant)
    {
        acabaCanvi();
        marcant = false;
        return;
    }
    canviFet = false;
    estat.capcal = juce::jmax (0.0, ed::enganxa (estat.projecte, estat.t ((float) e.x), Snap::Temps));
    estat.avisa();
}

void Regla::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto& p = estat.projecte;
    const double t = estat.t ((float) e.x);
    if (p.bucle.marcat && t >= ed::iniciBucle (p) && t < ed::fiBucle (p))
    {
        comencaCanvi();
        restaura();
        estat.projecte.bucle.actiu = ! estat.projecte.bucle.actiu;
        acabaCanvi();
    }
}

//==============================================================================
// VistaPistes
//==============================================================================
juce::Rectangle<float> VistaPistes::rectClip (int pista, const Clip& c) const
{
    const float y = (float) estat.yPista (pista);
    const float xa = estat.x (c.iniciVisible()), xb = estat.x (c.fiVisible());
    return { xa, y + 5.0f, juce::jmax (2.0f, xb - xa), (float) estat.altPista - 10.0f };
}

float VistaPistes::yGuany (const Clip& c, juce::Rectangle<float> r) const
{
    return r.getY() + 3.0f + (float) (-c.guanyDb / -ed::guanyMinDb) * (r.getHeight() * 0.8f);
}

VistaPistes::Lloc VistaPistes::llocA (juce::Point<int> p) const
{
    Lloc l;
    const int fila = estat.pistaA (p.y);
    if (fila == estat.numPistes() && p.y >= estat.yPista (fila) && p.y < estat.yPista (fila) + 44)
    {
        l.pistaNova = true;
        return l;
    }
    if (fila < 0 || fila >= estat.numPistes())
        return l;

    l.pista = fila;
    const auto& t = estat.projecte.pistes[(size_t) fila];
    const auto pf = p.toFloat();

    // Manetes del clip triat (només si n'hi ha un).
    if (fila == estat.pistaTriada && estat.clipsTriats.size() == 1)
    {
        const int ci = estat.clipsTriats[0];
        if (ci >= 0 && ci < (int) t.clips.size())
        {
            const auto& c = t.clips[(size_t) ci];
            const auto r = rectClip (fila, c);
            const float xfi = estat.x (c.iniciVisible() + c.fadeIn), xfo = estat.x (c.fiVisible() - c.fadeOut);
            const float yg = yGuany (c, r);
            auto prova = [&] (juce::Rectangle<float> z, Maneta m) { if (z.expanded (4.0f).contains (pf)) { l.clip = ci; l.maneta = m; return true; } return false; };

            if (prova ({ (r.getX() + r.getRight()) / 2.0f - mida, yg - mida, mida * 2, mida * 2 }, Maneta::Guany)) return l;
            if (prova ({ r.getX(), r.getBottom() - mida * 2, mida * 1.4f, mida * 2 }, Maneta::RetallInici)) return l;
            if (prova ({ r.getRight() - mida * 1.4f, r.getBottom() - mida * 2, mida * 1.4f, mida * 2 }, Maneta::RetallFi)) return l;
            if (prova ({ xfi - mida * 0.7f, r.getY(), mida * 1.4f, mida * 1.4f }, Maneta::FadeIn)) return l;
            if (prova ({ xfo - mida * 0.7f, r.getY(), mida * 1.4f, mida * 1.4f }, Maneta::FadeOut)) return l;
        }
    }

    // Banderes (es poden arrossegar).
    for (int i = (int) t.clips.size(); --i >= 0;)
    {
        const auto& c = t.clips[(size_t) i];
        if (! ed::banderaVisible (estat.projecte, c)) continue;
        const auto r = rectClip (fila, c);
        const float xb = estat.x (c.pos + ed::presa (estat.projecte, c.presa)->primer);
        if (juce::Rectangle<float> (xb - 4.0f, r.getY(), 14.0f, 12.0f).contains (pf))
        {
            l.clip = i;
            l.maneta = Maneta::Bandera;
            return l;
        }
    }

    // Cos dels clips (els triats tenen preferència, perquè es dibuixen a sobre).
    for (int passada = 0; passada < 2; ++passada)
        for (int i = (int) t.clips.size(); --i >= 0;)
            if ((passada == 0) == estat.clipTriat (fila, i) && rectClip (fila, t.clips[(size_t) i]).contains (pf))
            {
                l.clip = i;
                return l;
            }

    return l;
}

void VistaPistes::pintaGraella (juce::Graphics& g)
{
    const auto& p = estat.projecte;
    const double spt = ed::segonsPerTemps (p);
    const float h = (float) getHeight();
    const int primer = juce::jmax (0, (int) std::floor (estat.t (0.0f) / spt));
    const int darrer = (int) std::ceil (estat.t ((float) getWidth()) / spt);
    const bool temps = spt * estat.pxPerSegon >= 9.0;

    for (int t = primer; t <= darrer; ++t)
    {
        const bool compas = t % p.compasNum == 0;
        if (! compas && ! temps) continue;
        g.setColour (juce::Colours::white.withAlpha (compas ? 0.10f : 0.04f));
        g.fillRect (juce::Rectangle<float> (estat.x (t * spt), 0.0f, 1.0f, h));
    }
}

void VistaPistes::pintaClip (juce::Graphics& g, int pista, int indexClip, const Clip& c)
{
    const auto& p = estat.projecte;
    const auto& t = p.pistes[(size_t) pista];
    auto r = rectClip (pista, c);
    if (r.getRight() < -40.0f || r.getX() > (float) getWidth() + 40.0f) return;

    const bool triat = estat.clipTriat (pista, indexClip);
    const bool manetes = triat && estat.clipsTriats.size() == 1;
    const bool actiu = sona (p, t);
    const auto color = actiu ? EstatEstudi::colorTipus (t.tipus) : tema::suau;
    const float rad = 6.0f;

    g.setColour (tema::fons);
    g.fillRoundedRectangle (r, rad);
    g.setColour (color.withAlpha (triat ? 0.30f : 0.18f));
    g.fillRoundedRectangle (r, rad);

    // Ona (amb el volum i els fades del clip). Fora del so, no n'hi ha (és silenci).
    const auto* s = ed::presa (p, c.presa);
    auto it = estat.ones.find (c.presa);
    if (s != nullptr && it != estat.ones.end() && ! it->second.empty())
    {
        const auto& pics = it->second;
        const double guany = std::pow (10.0, c.guanyDb / 20.0);
        const float mig = r.getCentreY();
        const float altMax = r.getHeight() - 12.0f;
        g.setColour (color.withAlpha (actiu ? 0.9f : 0.5f));
        const float pas = 2.0f;
        for (float x = juce::jmax (r.getX() + 1.0f, 0.0f); x < juce::jmin (r.getRight() - 1.0f, (float) getWidth()); x += pas)
        {
            const double ta = estat.t (x) - c.pos, tb = estat.t (x + pas) - c.pos;   // dins el fitxer
            if (tb < 0.0 || ta > s->durada) continue;
            const int a = juce::jlimit (0, (int) pics.size() - 1, (int) (ta * 100.0));
            const int b = juce::jlimit (a + 1, (int) pics.size(), (int) std::ceil (tb * 100.0));
            float v = 0.0f;
            for (int q = a; q < b; ++q) v = juce::jmax (v, pics[(size_t) q]);

            double env = 1.0;
            if (c.fadeIn > 0.0 && ta < c.inici + c.fadeIn) env = juce::jmax (0.0, (ta - c.inici) / c.fadeIn);
            if (c.fadeOut > 0.0 && ta > c.fi - c.fadeOut) env = juce::jmin (env, juce::jmax (0.0, (c.fi - ta) / c.fadeOut));

            const float hb = juce::jmax (1.0f, (float) (v * guany * env) * altMax);
            g.fillRect (juce::Rectangle<float> (x, mig - hb / 2.0f, pas * 0.6f, hb));
        }
    }

    const float yg = yGuany (c, r);

    // Fades: zona enfosquida i línia (de silenci a dalt de tot = 0 dB).
    {
    juce::Graphics::ScopedSaveState estatG (g);
    g.reduceClipRegion (r.toNearestInt());
    if (c.fadeIn > 0.0)
    {
        const float xf = estat.x (c.iniciVisible() + c.fadeIn);
        juce::Path zona;
        zona.addTriangle (r.getX(), r.getBottom(), r.getX(), r.getY(), xf, r.getY());
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillPath (zona);
        g.setColour (tema::text);
        g.drawLine (r.getX(), r.getBottom(), xf, r.getY(), 1.5f);
    }
    if (c.fadeOut > 0.0)
    {
        const float xf = estat.x (c.fiVisible() - c.fadeOut);
        juce::Path zona;
        zona.addTriangle (r.getRight(), r.getBottom(), r.getRight(), r.getY(), xf, r.getY());
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillPath (zona);
        g.setColour (tema::text);
        g.drawLine (xf, r.getY(), r.getRight(), r.getBottom(), 1.5f);
    }

    // Línia del volum del clip.
    if (c.guanyDb < -0.05 || manetes)
    {
        g.setColour (juce::Colours::white.withAlpha (manetes ? 0.8f : 0.45f));
        g.fillRect (juce::Rectangle<float> (r.getX(), yg - 0.5f, r.getWidth(), 1.0f));
    }

    // Bandera del primer temps.
    if (ed::banderaVisible (p, c))
    {
        const float xb = estat.x (c.pos + s->primer);
        g.setColour (tema::accent);
        g.fillRect (juce::Rectangle<float> (xb - 0.75f, r.getY(), 1.5f, r.getHeight()));
        juce::Path tri;
        tri.addTriangle (xb, r.getY(), xb + 10.0f, r.getY() + 4.5f, xb, r.getY() + 9.0f);
        g.fillPath (tri);
    }

    // Nom.
    g.setColour (actiu ? tema::text : tema::suau);
    g.setFont (dibuix::lletra (12.0f, true));
    const float xn = juce::jmax (r.getX(), 0.0f) + 8.0f;
    if (xn < r.getRight() - 24.0f)
    {
        const auto nom = s != nullptr ? dibuix::utf8 (s->nom) : juce::String ("?");
        g.drawText (nom, juce::Rectangle<float> (xn + (ed::banderaVisible (p, c) && std::abs (estat.x (c.pos + s->primer) - xn) < 14.0f ? 12.0f : 0.0f),
                                                  r.getY() + 3.0f, r.getRight() - xn - 6.0f, 16.0f),
                    juce::Justification::centredLeft, true);
    }

    }

    // Vora.
    g.setColour (triat ? tema::text : color.withAlpha (0.65f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad, triat ? 2.0f : 1.0f);

    // Manetes.
    if (manetes)
    {
        const float xfi = estat.x (c.iniciVisible() + c.fadeIn), xfo = estat.x (c.fiVisible() - c.fadeOut);
        auto maneta = [&] (juce::Rectangle<float> z, bool destacada)
        {
            g.setColour (destacada ? tema::accent : tema::text);
            g.fillRect (z);
        };
        const auto m = sobre.pista == pista && sobre.clip == indexClip ? sobre.maneta : Maneta::Cap;
        maneta ({ r.getX(), r.getBottom() - mida * 2, mida * 1.4f, mida * 2 }, m == Maneta::RetallInici);
        maneta ({ r.getRight() - mida * 1.4f, r.getBottom() - mida * 2, mida * 1.4f, mida * 2 }, m == Maneta::RetallFi);
        maneta ({ xfi - mida * 0.7f, r.getY(), mida * 1.4f, mida * 1.4f }, m == Maneta::FadeIn);
        maneta ({ xfo - mida * 0.7f, r.getY(), mida * 1.4f, mida * 1.4f }, m == Maneta::FadeOut);
        const float xm = (r.getX() + r.getRight()) / 2.0f;
        g.setColour (m == Maneta::Guany ? tema::accent : tema::text);
        g.fillEllipse (xm - mida * 0.85f, yg - mida * 0.85f, mida * 1.7f, mida * 1.7f);
        g.setColour (tema::fons);
        g.fillEllipse (xm - mida * 0.35f, yg - mida * 0.35f, mida * 0.7f, mida * 0.7f);
    }
}

void VistaPistes::paint (juce::Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();
    g.fillAll (tema::fons);

    // Files de les pistes.
    for (int i = 0; i < estat.numPistes(); ++i)
    {
        const int y = estat.yPista (i);
        if (y > getHeight() || y + estat.altPista < 0) continue;
        g.setColour (i == estat.pistaTriada ? tema::triat : (i % 2 == 0 ? tema::fons : juce::Colour (0xff19191e)));
        g.fillRect (0, y, getWidth(), estat.altPista);
        g.setColour (tema::linia.withAlpha (0.6f));
        g.fillRect (0, y + estat.altPista - 1, getWidth(), 1);
    }

    pintaGraella (g);
    pintaBucleSobre (g, estat, h);

    // Clips: primer els no triats, després els triats (a sobre).
    for (int i = 0; i < estat.numPistes(); ++i)
    {
        const auto& t = estat.projecte.pistes[(size_t) i];
        for (int k = 0; k < (int) t.clips.size(); ++k) if (! estat.clipTriat (i, k)) pintaClip (g, i, k, t.clips[(size_t) k]);
        for (int k = 0; k < (int) t.clips.size(); ++k) if (estat.clipTriat (i, k)) pintaClip (g, i, k, t.clips[(size_t) k]);
    }

    // Pista nova.
    {
        const int y = estat.yPista (estat.numPistes());
        auto r = juce::Rectangle<float> (8.0f, (float) y + 6.0f, juce::jmin (w - 16.0f, 260.0f), 32.0f);
        const bool destacat = sobre.pistaNova || filaDeixar >= estat.numPistes();
        juce::Path p;
        p.addRoundedRectangle (r, 8.0f);
        juce::Path discontinu;
        const float guions[] = { 4.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (discontinu, p, guions, 2);
        g.setColour (destacat ? tema::accent : tema::linia2);
        g.fillPath (discontinu);
        g.setColour (destacat ? tema::text : tema::suau);
        g.setFont (dibuix::lletra (13.0f));
        g.drawText (estat.numPistes() == 0 ? "+ Pista nova  (o arrossega aquí un fitxer d'àudio)"_ca : "+ Pista nova"_ca,
                    r.reduced (12.0f, 0.0f), juce::Justification::centredLeft, true);
    }

    // Fitxers que s'arrosseguen a sobre.
    if (filaDeixar >= -1)
    {
        const int fila = juce::jlimit (0, estat.numPistes(), filaDeixar);
        g.setColour (tema::accent.withAlpha (0.12f));
        g.fillRect (0, estat.yPista (fila), getWidth(), fila < estat.numPistes() ? estat.altPista : 44);
    }

    pintaCapcal (g, estat.x (estat.capcal), h);
}

void VistaPistes::mouseMove (const juce::MouseEvent& e)
{
    const auto l = llocA (e.getPosition());
    if (l.pista != sobre.pista || l.clip != sobre.clip || l.maneta != sobre.maneta || l.pistaNova != sobre.pistaNova)
    {
        sobre = l;
        repaint();
    }

    switch (l.maneta)
    {
        case Maneta::RetallInici:
        case Maneta::RetallFi:
        case Maneta::Bandera:   setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); break;
        case Maneta::FadeIn:
        case Maneta::FadeOut:   setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); break;
        case Maneta::Guany:     setMouseCursor (juce::MouseCursor::UpDownResizeCursor); break;
        case Maneta::Cap:       setMouseCursor (l.clip >= 0 ? juce::MouseCursor::DraggingHandCursor
                                                            : (l.pistaNova ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor)); break;
    }
}

void VistaPistes::menuPistaNova (juce::Component& on, std::function<void (const juce::String&)> tria)
{
    juce::PopupMenu m;
    m.addSectionHeader ("Pista nova");
    m.addItem (1, "Guitarra");
    m.addItem (2, "Veu");
    m.addItem (3, "Àudio"_ca);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&on).withMousePosition(), [tria] (int r)
    {
        if (r == 1) tria ("guitarra");
        else if (r == 2) tria ("veu");
        else if (r == 3) tria ("audio");
    });
}

void VistaPistes::menuClip (int pista, int clip, double t)
{
    const auto& clip0 = estat.projecte.pistes[(size_t) pista].clips[(size_t) clip];
    const auto* s = ed::presa (estat.projecte, clip0.presa);
    const bool unic = estat.clipsTriats.size() == 1;

    juce::PopupMenu m;
    m.addItem (1, estat.clipsTriats.size() > 1 ? "Duplica els clips   (Ctrl+D)" : "Duplica   (Ctrl+D)");
    m.addItem (2, estat.clipsTriats.size() > 1 ? "Treu els clips   (Supr)" : "Treu   (Supr)");
    m.addSeparator();
    m.addItem (3, "Posa la bandera del primer temps aquí"_ca, unic);
    m.addItem (4, "Treu la bandera", unic && s != nullptr && s->primer >= 0.0);
    m.addSeparator();
    m.addItem (5, "Treu els fades i el volum del clip", unic);

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this, pista, clip, t] (int r)
    {
        if (r == 0 || pista >= estat.numPistes() || clip >= (int) estat.projecte.pistes[(size_t) pista].clips.size()) return;
        comencaCanvi();
        restaura();
        auto& c = estat.projecte.pistes[(size_t) pista].clips[(size_t) clip];
        switch (r)
        {
            case 1: estat.clipsTriats = ed::duplica (estat.projecte, pista, estat.clipsTriats); break;
            case 2: ed::treuClips (estat.projecte, pista, estat.clipsTriats); estat.clipsTriats.clear(); break;
            case 3: ed::posaBandera (estat.projecte, c, t); break;
            case 4: ed::posaBandera (estat.projecte, c, -1.0); break;
            case 5: c.fadeIn = c.fadeOut = 0.0; c.guanyDb = 0.0; break;
            default: break;
        }
        acabaCanvi();
    });
}

void VistaPistes::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    inici = e.getPosition();
    lloc0 = llocA (inici);
    t0 = estat.t ((float) e.x);
    mode = Mode::Res;

    if (lloc0.pistaNova)
    {
        menuPistaNova (*this, [this] (const juce::String& tipus) { if (onPistaNova) onPistaNova (-1, tipus); });
        return;
    }

    if (e.mods.isPopupMenu())
    {
        if (lloc0.clip >= 0)
        {
            if (! estat.clipTriat (lloc0.pista, lloc0.clip))
            {
                estat.pistaTriada = lloc0.pista;
                estat.clipsTriats = { lloc0.clip };
                estat.avisa();
            }
            menuClip (lloc0.pista, lloc0.clip, t0);
        }
        else if (lloc0.pista >= 0)
        {
            estat.pistaTriada = lloc0.pista;
            estat.clipsTriats.clear();
            estat.avisa();
            const int fila = lloc0.pista;
            const double on = juce::jmax (0.0, ed::enganxa (estat.projecte, t0, estat.projecte.snap));
            juce::PopupMenu m;
            m.addItem (1, "Importa àudio aquí…"_ca);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this, fila, on] (int r)
            {
                if (r == 1 && onDemanaImportar) onDemanaImportar (fila, on);
            });
        }
        return;
    }

    if (lloc0.pista < 0)
    {
        estat.clipsTriats.clear();
        mode = Mode::Capcal;
        estat.capcal = juce::jmax (0.0, ed::enganxa (estat.projecte, t0, estat.projecte.snap));
        estat.avisa();
        return;
    }

    auto& t = estat.projecte.pistes[(size_t) lloc0.pista];

    if (lloc0.maneta != Maneta::Cap)
    {
        auto& c = t.clips[(size_t) lloc0.clip];
        comencaCanvi();
        switch (lloc0.maneta)
        {
            case Maneta::RetallInici: mode = Mode::RetallInici; break;
            case Maneta::RetallFi:    mode = Mode::RetallFi; break;
            case Maneta::FadeIn:      mode = Mode::FadeIn; break;
            case Maneta::FadeOut:     mode = Mode::FadeOut; break;
            case Maneta::Guany:       mode = Mode::Guany; guany0 = c.guanyDb; break;
            case Maneta::Bandera:
                mode = Mode::Bandera;
                estat.pistaTriada = lloc0.pista;
                if (! estat.clipTriat (lloc0.pista, lloc0.clip)) estat.clipsTriats = { lloc0.clip };
                break;
            case Maneta::Cap: break;
        }
        return;
    }

    if (lloc0.clip >= 0)
    {
        // Triar.
        if (e.mods.isCommandDown())
        {
            if (estat.pistaTriada != lloc0.pista) { estat.pistaTriada = lloc0.pista; estat.clipsTriats = { lloc0.clip }; }
            else
            {
                auto& v = estat.clipsTriats;
                auto it = std::find (v.begin(), v.end(), lloc0.clip);
                if (it != v.end()) v.erase (it); else v.push_back (lloc0.clip);
            }
        }
        else if (e.mods.isShiftDown() && estat.pistaTriada == lloc0.pista && ! estat.clipsTriats.empty())
        {
            const double a = t.clips[(size_t) estat.clipsTriats[0]].iniciVisible();
            const double b = t.clips[(size_t) lloc0.clip].iniciVisible();
            estat.clipsTriats.clear();
            for (int k = 0; k < (int) t.clips.size(); ++k)
            {
                const double x = t.clips[(size_t) k].iniciVisible();
                if (x >= juce::jmin (a, b) - 1e-9 && x <= juce::jmax (a, b) + 1e-9)
                    estat.clipsTriats.push_back (k);
            }
        }
        else if (! estat.clipTriat (lloc0.pista, lloc0.clip))
        {
            estat.pistaTriada = lloc0.pista;
            estat.clipsTriats = { lloc0.clip };
        }

        if (! estat.clipTriat (lloc0.pista, lloc0.clip))
        {
            estat.avisa();
            return;   // Ctrl+clic l'ha tret de la selecció
        }

        const auto& c = t.clips[(size_t) lloc0.clip];
        punt0 = ed::puntSnap (estat.projecte, c);
        bandera0 = ed::banderaVisible (estat.projecte, c) ? c.pos + ed::presa (estat.projecte, c.presa)->primer : -1.0;
        comencaCanvi();
        triatsOriginals = estat.clipsTriats;
        mode = Mode::Decidint;
        estat.avisa();
        return;
    }

    // Espai buit d'una pista: tria la pista i posa el capçal.
    estat.pistaTriada = lloc0.pista;
    estat.clipsTriats.clear();
    mode = Mode::Capcal;
    estat.capcal = juce::jmax (0.0, ed::enganxa (estat.projecte, t0, estat.projecte.snap));
    estat.avisa();
}

void VistaPistes::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    auto& p = estat.projecte;
    const double t = estat.t ((float) e.x);

    switch (mode)
    {
        case Mode::Res:
            return;

        case Mode::Capcal:
            estat.capcal = juce::jmax (0.0, ed::enganxa (p, t, p.snap));
            estat.avisa();
            return;

        case Mode::Decidint:
        {
            const int dx = std::abs (e.x - inici.x), dy = std::abs (e.y - inici.y);
            if (juce::jmax (dx, dy) < 5) return;
            mode = dx >= dy ? Mode::MouTemps : Mode::MouPista;   // el primer moviment decideix el sentit
            mouseDrag (e);
            return;
        }

        case Mode::MouTemps:
        {
            restaura();
            auto& clips = p.pistes[(size_t) estat.pistaTriada].clips;
            const auto& copia = copiaInici.pistes[(size_t) estat.pistaTriada].clips;
            const double dt = t - t0;

            if (p.snap == Snap::Lliure && estat.clipsTriats.size() == 1 && bandera0 >= 0.0)
            {
                auto& c = clips[(size_t) estat.clipsTriats[0]];
                const double posNova = copia[(size_t) estat.clipsTriats[0]].pos + dt;
                if (ed::mouAmbBanderaFixa (p, c, posNova, bandera0))
                    estat.info = "Bandera fixa · àudio "_ca + (dt >= 0 ? "+" : "") + ms (dt);
                else
                {
                    c.pos = posNova;
                    estat.info = "La bandera quedaria fora del clip: es mou amb l'àudio"_ca;
                }
            }
            else
            {
                double desti = ed::enganxa (p, punt0 + dt, p.snap);
                double delta = desti - punt0;
                double minInici = 1e300;
                for (int i : estat.clipsTriats) minInici = juce::jmin (minInici, copia[(size_t) i].iniciVisible());
                if (minInici + delta < 0.0) { delta = -minInici; desti = punt0 + delta; }
                for (int i : estat.clipsTriats) clips[(size_t) i].pos = copia[(size_t) i].pos + delta;
                estat.info = (estat.clipsTriats.size() > 1 ? juce::String (estat.clipsTriats.size()) + " clips · "_ca : juce::String())
                           + "Compàs "_ca + posicio (p, desti, p.snap == Snap::Lliure);
            }
            estat.avisa();
            return;
        }

        case Mode::MouPista:
        {
            restaura();
            const int de = lloc0.pista;
            const int a = juce::jlimit (0, estat.numPistes() - 1, de + (int) std::round ((double) (e.y - inici.y) / estat.altPista));
            if (a != de)
            {
                estat.clipsTriats = ed::passaAPista (p, de, triatsOriginals, a);
                estat.pistaTriada = a;
            }
            else
            {
                estat.clipsTriats = triatsOriginals;
                estat.pistaTriada = de;
                canviFet = false;
            }
            estat.info = "Pista: "_ca + dibuix::utf8 (p.pistes[(size_t) estat.pistaTriada].nom);
            estat.avisa();
            return;
        }

        case Mode::RetallInici:
        case Mode::RetallFi:
        case Mode::FadeIn:
        case Mode::FadeOut:
        case Mode::Guany:
        case Mode::Bandera:
        {
            restaura();
            auto& c = p.pistes[(size_t) lloc0.pista].clips[(size_t) lloc0.clip];
            switch (mode)
            {
                case Mode::RetallInici:
                    ed::retallaInici (c, ed::enganxa (p, t, p.snap));
                    estat.info = "Inici: "_ca + posicio (p, c.iniciVisible(), p.snap == Snap::Lliure);
                    break;
                case Mode::RetallFi:
                    ed::retallaFi (c, ed::enganxa (p, t, p.snap));
                    estat.info = "Final: "_ca + posicio (p, c.fiVisible(), p.snap == Snap::Lliure);
                    break;
                case Mode::FadeIn:
                    ed::fadeIn (c, t);
                    estat.info = "Fade in: " + ms (c.fadeIn);
                    break;
                case Mode::FadeOut:
                    ed::fadeOut (c, t);
                    estat.info = "Fade out: " + ms (c.fadeOut);
                    break;
                case Mode::Guany:
                {
                    const double alt = (estat.altPista - 10) * 0.8;
                    ed::guany (c, guany0 - (e.y - inici.y) * (-ed::guanyMinDb) / alt);
                    estat.info = "Volum del clip: " + EstatEstudi::textDb (c.guanyDb);
                    break;
                }
                case Mode::Bandera:
                    ed::posaBandera (p, c, t);
                    estat.info = "Bandera: "_ca + posicio (p, c.pos + ed::presa (p, c.presa)->primer, true);
                    break;
                case Mode::Res: case Mode::Capcal: case Mode::Decidint: case Mode::MouTemps: case Mode::MouPista:
                    break;
            }
            estat.avisa();
            return;
        }
    }
}

void VistaPistes::mouseUp (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { mode = Mode::Res; return; }

    if (mode == Mode::Decidint)
    {
        // Un clic sense moure's sobre un clip d'un grup el deixa triat sol.
        if (! e.mods.isCommandDown() && ! e.mods.isShiftDown())
            estat.clipsTriats = { lloc0.clip };
        canviFet = false;
        estat.avisa();
    }
    else if (mode != Mode::Res && mode != Mode::Capcal)
    {
        acabaCanvi();
    }
    triatsOriginals.clear();
    mode = Mode::Res;
}

void VistaPistes::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto l = llocA (e.getPosition());
    if (l.pista < 0 && ! l.pistaNova && e.y > estat.yPista (estat.numPistes()))
        menuPistaNova (*this, [this] (const juce::String& tipus) { if (onPistaNova) onPistaNova (-1, tipus); });
}

bool VistaPistes::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg"))
            return true;
    return false;
}

void VistaPistes::filesDropped (const juce::StringArray& files, int x, int y)
{
    filaDeixar = -2;
    const int fila = estat.pistaA (y);
    const double on = juce::jmax (0.0, ed::enganxa (estat.projecte, estat.t ((float) x), estat.projecte.snap));
    if (onImporta) onImporta (files, (fila >= 0 && fila < estat.numPistes()) ? fila : -1, on);
    repaint();
}

//==============================================================================
// Capcaleres
//==============================================================================
juce::Rectangle<int> Capcaleres::rectBoto (int pista, int quin) const
{
    const int y = estat.yPista (pista);
    const int baix = y + estat.altPista - 10;
    return { 14 + quin * 30, baix - 22, 26, 22 };
}

Capcaleres::Boto Capcaleres::botoA (int pista, juce::Point<int> p) const
{
    if (estat.altPista < 54) return Boto::Cap;
    if (rectBoto (pista, 0).contains (p)) return Boto::Mut;
    if (rectBoto (pista, 1).contains (p)) return Boto::Solo;
    if (rectBoto (pista, 2).contains (p)) return Boto::Grava;
    return Boto::Cap;
}

void Capcaleres::paint (juce::Graphics& g)
{
    g.fillAll (tema::lateral);
    const auto& p = estat.projecte;

    for (int i = 0; i < estat.numPistes(); ++i)
    {
        const auto& t = p.pistes[(size_t) i];
        const int y = estat.yPista (i);
        if (y > getHeight() || y + estat.altPista < 0) continue;
        auto r = juce::Rectangle<int> (0, y, getWidth(), estat.altPista);

        if (i == estat.pistaTriada)
        {
            g.setColour (tema::triat);
            g.fillRect (r);
        }
        const auto color = sona (p, t) ? EstatEstudi::colorTipus (t.tipus) : tema::suau;
        g.setColour (color);
        g.fillRect (r.getX(), r.getY() + 4, 4, r.getHeight() - 8);

        auto text = r.withTrimmedLeft (14).withTrimmedRight (8).withTrimmedTop (8);
        auto fila = text.removeFromTop (20);
        g.setColour (tema::suau);
        g.setFont (dibuix::mono (12.0f));
        g.drawText (juce::String (i + 1), fila.removeFromLeft (22), juce::Justification::centredLeft);
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (14.0f, true));
        g.drawText (dibuix::utf8 (t.nom), fila, juce::Justification::centredLeft, true);

        if (estat.altPista >= 54)
        {
            pintaBotoPetit (g, rectBoto (i, 0), "M", t.mut, tema::midi);
            pintaBotoPetit (g, rectBoto (i, 1), "S", t.solo, tema::guitarra);
            pintaBotoPetit (g, rectBoto (i, 2), {}, t.grava, tema::error, true);
        }

        g.setColour (tema::linia.withAlpha (0.6f));
        g.fillRect (0, r.getBottom() - 1, getWidth(), 1);
    }

    // Pista nova.
    {
        const int y = estat.yPista (estat.numPistes());
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (13.0f));
        g.drawText ("+ Pista nova", juce::Rectangle<int> (14, y + 6, getWidth() - 20, 32), juce::Justification::centredLeft, true);
    }

    // On anirà la pista que s'arrossega.
    if (arrossegant >= 0 && desti >= 0 && desti != arrossegant)
    {
        const int y = desti > arrossegant ? estat.yPista (desti) + estat.altPista : estat.yPista (desti);
        g.setColour (tema::accent);
        g.fillRect (0, y - 1, getWidth(), 3);
    }

    g.setColour (tema::linia);
    g.fillRect (getWidth() - 1, 0, 1, getHeight());
}

void Capcaleres::menu (int pista)
{
    const auto tipus = estat.projecte.pistes[(size_t) pista].tipus;
    juce::PopupMenu tipusMenu;
    tipusMenu.addItem (10, "Guitarra", true, tipus == "guitarra");
    tipusMenu.addItem (11, "Veu", true, tipus == "veu");
    tipusMenu.addItem (12, "Àudio"_ca, true, tipus == "audio");

    juce::PopupMenu m;
    m.addItem (1, "Canvia el nom…"_ca);
    m.addSubMenu ("Tipus (color)", tipusMenu);
    m.addSeparator();
    m.addItem (2, "Esborra la pista");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this, pista] (int r)
    {
        if (r == 0 || pista >= estat.numPistes()) return;
        if (r == 1) { if (onCanviaNom) onCanviaNom (pista); return; }
        comencaCanvi();
        restaura();
        juce::String evolucio;
        if (r == 2)
        {
            evolucio = "Pista esborrada: «"_ca + dibuix::utf8 (estat.projecte.pistes[(size_t) pista].nom) + "»"_ca;
            ed::esborraPista (estat.projecte, pista);
            estat.clipsTriats.clear();
        }
        else
        {
            estat.projecte.pistes[(size_t) pista].tipus = r == 10 ? "guitarra" : r == 11 ? "veu" : "audio";
        }
        acabaCanvi (evolucio);
    });
}

void Capcaleres::mouseDown (const juce::MouseEvent& e)
{
    const int fila = estat.pistaA (e.y);
    arrossegant = desti = -1;

    if (fila == estat.numPistes() && e.y < estat.yPista (fila) + 44)
    {
        VistaPistes::menuPistaNova (*this, [this] (const juce::String& tipus) { if (onPistaNova) onPistaNova (tipus); });
        return;
    }
    if (fila < 0 || fila >= estat.numPistes()) return;

    if (estat.pistaTriada != fila) estat.clipsTriats.clear();
    estat.pistaTriada = fila;

    if (e.mods.isPopupMenu())
    {
        estat.avisa();
        menu (fila);
        return;
    }

    const auto b = botoA (fila, e.getPosition());
    if (b != Boto::Cap)
    {
        comencaCanvi();
        restaura();
        auto& t = estat.projecte.pistes[(size_t) fila];
        if (b == Boto::Mut) t.mut = ! t.mut;
        else if (b == Boto::Solo) t.solo = ! t.solo;
        else ed::triaGravacio (estat.projecte, fila);
        acabaCanvi();
        return;
    }

    arrossegant = fila;
    inici = e.getPosition();
    estat.avisa();
}

void Capcaleres::mouseDrag (const juce::MouseEvent& e)
{
    if (arrossegant < 0 || std::abs (e.y - inici.y) < 6) return;
    desti = juce::jlimit (0, estat.numPistes() - 1, estat.pistaA (e.y));
    estat.info = "Mou la pista a la posició "_ca + juce::String (desti + 1);
    estat.avisa();
}

void Capcaleres::mouseUp (const juce::MouseEvent&)
{
    if (arrossegant >= 0 && desti >= 0 && desti != arrossegant)
    {
        comencaCanvi();
        restaura();
        ed::mouPista (estat.projecte, arrossegant, desti);
        estat.pistaTriada = desti;
        acabaCanvi();
    }
    arrossegant = desti = -1;
    estat.info.clear();
    estat.avisa();
}

void Capcaleres::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int fila = estat.pistaA (e.y);
    if (fila >= 0 && fila < estat.numPistes() && botoA (fila, e.getPosition()) == Boto::Cap && onCanviaNom)
        onCanviaNom (fila);
}

//==============================================================================
// Faders
//==============================================================================
juce::Rectangle<float> Faders::rectFader (int pista) const
{
    const float y = (float) estat.yPista (pista);
    const float mig = y + (float) estat.altPista * 0.5f + 6.0f;
    return { 14.0f, mig - 9.0f, juce::jmax (30.0f, (float) getWidth() - 28.0f), 18.0f };
}

void Faders::pintaFaderHoritzontal (juce::Graphics& g, juce::Rectangle<float> r, double db, juce::Colour color, bool actiu)
{
    auto pista = r.withSizeKeepingCentre (r.getWidth(), 6.0f);
    g.setColour (tema::superficie2);
    g.fillRoundedRectangle (pista, 3.0f);
    const float pos = EstatEstudi::posFader (db);
    g.setColour (color.withAlpha (actiu ? 0.85f : 0.4f));
    g.fillRoundedRectangle (pista.withWidth (pista.getWidth() * pos), 3.0f);
    // Marca de 0 dB.
    g.setColour (tema::linia2);
    g.fillRect (juce::Rectangle<float> (r.getX() + r.getWidth() * 0.75f - 0.5f, r.getY(), 1.0f, r.getHeight()));
    // Botó.
    const float x = r.getX() + r.getWidth() * pos;
    auto boto = juce::Rectangle<float> (x - 5.0f, r.getY(), 10.0f, r.getHeight());
    g.setColour (tema::text);
    g.fillRoundedRectangle (boto, 3.0f);
    g.setColour (tema::fons);
    g.fillRect (boto.withSizeKeepingCentre (2.0f, r.getHeight() - 8.0f));
}

void Faders::paint (juce::Graphics& g)
{
    g.fillAll (tema::lateral);
    const auto& p = estat.projecte;
    for (int i = 0; i < estat.numPistes(); ++i)
    {
        const auto& t = p.pistes[(size_t) i];
        const int y = estat.yPista (i);
        if (y > getHeight() || y + estat.altPista < 0) continue;
        if (i == estat.pistaTriada)
        {
            g.setColour (tema::triat);
            g.fillRect (0, y, getWidth(), estat.altPista);
        }
        const auto r = rectFader (i);
        if (estat.altPista >= 50)
        {
            g.setColour (tema::suau);
            g.setFont (dibuix::mono (11.5f));
            g.drawText (EstatEstudi::textDb (t.faderDb), juce::Rectangle<float> (r.getX(), r.getY() - 22.0f, r.getWidth(), 16.0f),
                        juce::Justification::centredLeft);
        }
        pintaFaderHoritzontal (g, r, t.faderDb, EstatEstudi::colorTipus (t.tipus), sona (p, t));
        g.setColour (tema::linia.withAlpha (0.6f));
        g.fillRect (0, y + estat.altPista - 1, getWidth(), 1);
    }
    g.setColour (tema::linia);
    g.fillRect (getWidth() - 1, 0, 1, getHeight());
}

void Faders::mouseDown (const juce::MouseEvent& e)
{
    const int fila = estat.pistaA (e.y);
    pistaFader = (fila >= 0 && fila < estat.numPistes()) ? fila : -1;
    if (pistaFader < 0) return;
    if (estat.pistaTriada != fila) estat.clipsTriats.clear();
    estat.pistaTriada = fila;
    pos0 = EstatEstudi::posFader (estat.projecte.pistes[(size_t) fila].faderDb);
    comencaCanvi();
    estat.avisa();
}

void Faders::mouseDrag (const juce::MouseEvent& e)
{
    if (pistaFader < 0) return;
    restaura();
    auto& t = estat.projecte.pistes[(size_t) pistaFader];
    double db = EstatEstudi::dbFader (pos0 + (float) e.getDistanceFromDragStartX() / rectFader (pistaFader).getWidth());
    if (std::abs (db) < 0.6) db = 0.0;   // s'enganxa a 0 dB
    t.faderDb = db;
    estat.info = "Volum de «"_ca + dibuix::utf8 (t.nom) + "»: "_ca + EstatEstudi::textDb (db);
    estat.avisa();
}

void Faders::mouseUp (const juce::MouseEvent&)
{
    if (pistaFader >= 0) acabaCanvi();
    pistaFader = -1;
}

void Faders::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int fila = estat.pistaA (e.y);
    if (fila < 0 || fila >= estat.numPistes()) return;
    comencaCanvi();
    restaura();
    estat.projecte.pistes[(size_t) fila].faderDb = 0.0;
    acabaCanvi();
}

//==============================================================================
// PanellPista
//==============================================================================
PanellPista::PanellPista (EstatEstudi& e) : SeccioEstudi (e)
{
    addChildComponent (btDuplica);
    addChildComponent (btTreu);
    btDuplica.onClick = [this] { if (onDuplica) onDuplica(); };
    btTreu.onClick = [this] { if (onTreu) onTreu(); };
}

void PanellPista::changeListenerCallback (juce::ChangeBroadcaster*)
{
    const bool hiHaClips = estat.pista (estat.pistaTriada) != nullptr && ! estat.clipsTriats.empty();
    btDuplica.setVisible (hiHaClips);
    btTreu.setVisible (hiHaClips);
    repaint();
}

void PanellPista::resized()
{
    auto r = getLocalBounds().reduced (18, 16);
    r.removeFromTop (60);
    zonaFader = r.removeFromTop (220).removeFromLeft (64);
    auto botons = getLocalBounds().reduced (18, 16).withTrimmedTop (60 + 220 + 16 + 170).withHeight (36);
    btDuplica.setBounds (botons.removeFromLeft (botons.getWidth() / 2 - 5));
    botons.removeFromLeft (10);
    btTreu.setBounds (botons);
}

void PanellPista::paint (juce::Graphics& g)
{
    g.fillAll (tema::lateral);
    g.setColour (tema::linia);
    g.fillRect (0, 0, 1, getHeight());

    auto r = getLocalBounds().reduced (18, 16);
    auto* t = estat.pista (estat.pistaTriada);

    auto ajuda = [&] (juce::Rectangle<int> z)
    {
        dibuix::pintaEtiquetaSeccio (g, "Com funciona", z.removeFromTop (20));
        const char* linies[] = {
            "Arrossega un clip de costat per moure'l en el temps, o amunt i avall per passar-lo a una altra pista.",
            "Cantonades de baix: retallen. Cantonades de dalt: fades. Punt del mig: volum del clip.",
            "Mode Lliure: si el clip té bandera, la bandera es queda quieta i l'àudio llisca per sota.",
            "Ctrl+clic: tria'n uns quants. Supr: treu. Ctrl+D: duplica. Ctrl+Z: desfés.",
            "Roda: amunt i avall. Maj+roda: de costat. Ctrl+roda: zoom. Alt+roda: alçada de les pistes.",
            "Regla: clic posa el capçal; arrossega-hi per marcar el bucle.",
        };
        g.setFont (dibuix::lletra (12.0f));
        for (auto* l : linies)
        {
            if (z.getHeight() < 34) break;
            g.setColour (tema::suau);
            g.drawFittedText (juce::String::fromUTF8 (l), z.removeFromTop (34), juce::Justification::topLeft, 2, 0.9f);
        }
    };

    if (t == nullptr)
    {
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (14.0f));
        g.drawFittedText ("Tria una pista per veure-la aquí."_ca, r.removeFromTop (40), juce::Justification::topLeft, 2);
        r.removeFromTop (20);
        ajuda (r);
        return;
    }

    const auto color = EstatEstudi::colorTipus (t->tipus);
    auto cap = r.removeFromTop (60);
    g.setColour (color);
    g.fillEllipse ((float) cap.getX(), (float) cap.getY() + 8.0f, 10.0f, 10.0f);
    g.setColour (tema::text);
    g.setFont (dibuix::lletra (18.0f, true));
    g.drawText (dibuix::utf8 (t->nom), cap.withTrimmedLeft (18).removeFromTop (26), juce::Justification::centredLeft, true);
    g.setColour (tema::suau);
    g.setFont (dibuix::lletra (12.5f));
    g.drawText (EstatEstudi::nomTipus (t->tipus) + " · pista "_ca + juce::String (estat.pistaTriada + 1) + " · "_ca
                + juce::String ((int) t->clips.size()) + ((int) t->clips.size() == 1 ? " clip" : " clips"),
                cap.withTrimmedLeft (18).withTrimmedTop (28).withHeight (18), juce::Justification::centredLeft, true);

    // Fader gran.
    {
        auto f = zonaFader.toFloat();
        auto carril = f.withSizeKeepingCentre (8.0f, f.getHeight() - 30.0f).withY (f.getY());
        g.setColour (tema::superficie2);
        g.fillRoundedRectangle (carril, 4.0f);
        const float pos = EstatEstudi::posFader (t->faderDb);
        auto ple = carril.withTrimmedTop (carril.getHeight() * (1.0f - pos));
        g.setColour (color.withAlpha (0.85f));
        g.fillRoundedRectangle (ple, 4.0f);
        g.setColour (tema::linia2);
        g.fillRect (juce::Rectangle<float> (carril.getX() - 8.0f, carril.getBottom() - carril.getHeight() * 0.75f, carril.getWidth() + 16.0f, 1.0f));
        auto boto = juce::Rectangle<float> (carril.getCentreX() - 16.0f, carril.getBottom() - carril.getHeight() * pos - 6.0f, 32.0f, 12.0f);
        g.setColour (tema::text);
        g.fillRoundedRectangle (boto, 3.0f);
        g.setColour (tema::fons);
        g.fillRect (boto.withSizeKeepingCentre (18.0f, 2.0f));
        g.setColour (tema::textMig);
        g.setFont (dibuix::mono (12.0f));
        g.drawText (EstatEstudi::textDb (t->faderDb), juce::Rectangle<float> (f.getX() - 10.0f, f.getBottom() - 22.0f, f.getWidth() + 20.0f, 20.0f),
                    juce::Justification::centred);
    }

    // Clip triat.
    auto info = juce::Rectangle<int> (zonaFader.getRight() + 16, zonaFader.getY(), r.getRight() - zonaFader.getRight() - 16, 220);
    auto* c = estat.clipUnicTriat();
    if (c != nullptr)
    {
        const auto* s = ed::presa (estat.projecte, c->presa);
        dibuix::pintaEtiquetaSeccio (g, "Clip triat", info.removeFromTop (20));
        auto linia = [&] (const juce::String& etiqueta, const juce::String& valor)
        {
            auto fila = info.removeFromTop (22);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (12.0f));
            g.drawText (etiqueta, fila.removeFromLeft (70), juce::Justification::centredLeft);
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (12.5f));
            g.drawText (valor, fila, juce::Justification::centredLeft, true);
        };
        const auto& p = estat.projecte;
        linia ("Presa", s ? dibuix::utf8 (s->nom) : juce::String ("?"));
        linia ("Fitxer", s ? dibuix::utf8 (s->fitxer).fromLastOccurrenceOf ("/", false, false) : juce::String());
        linia ("Comença"_ca, posicio (p, c->iniciVisible(), true));
        linia ("Llargada", juce::String (c->longitud(), 2).replaceCharacter ('.', ',') + " s");
        linia ("Fades", ms (c->fadeIn) + " / " + ms (c->fadeOut));
        linia ("Volum", EstatEstudi::textDb (c->guanyDb));
        linia ("Bandera", ed::banderaVisible (p, *c) ? posicio (p, c->pos + s->primer, true)
                                                     : (s && s->primer >= 0 ? juce::String ("fora del clip") : juce::String ("no en té"_ca)));
    }
    else if (estat.clipsTriats.size() > 1)
    {
        g.setColour (tema::textMig);
        g.setFont (dibuix::lletra (13.0f));
        g.drawText (juce::String (estat.clipsTriats.size()) + " clips triats", info.removeFromTop (22), juce::Justification::centredLeft);
    }
    else
    {
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (12.5f));
        g.drawFittedText ("Clica un clip per veure'n les dades."_ca, info.removeFromTop (40), juce::Justification::topLeft, 2);
    }

    r.removeFromTop (220 + 16 + 170 + 36 + 20);
    ajuda (r);
}

void PanellPista::mouseDown (const juce::MouseEvent& e)
{
    movent = estat.pista (estat.pistaTriada) != nullptr && zonaFader.expanded (10).contains (e.getPosition());
    if (! movent) return;
    pos0 = EstatEstudi::posFader (estat.pista (estat.pistaTriada)->faderDb);
    comencaCanvi();
}

void PanellPista::mouseDrag (const juce::MouseEvent& e)
{
    if (! movent) return;
    restaura();
    auto* t = estat.pista (estat.pistaTriada);
    if (t == nullptr) return;
    double db = EstatEstudi::dbFader (pos0 - (float) e.getDistanceFromDragStartY() / (float) (zonaFader.getHeight() - 30));
    if (std::abs (db) < 0.6) db = 0.0;
    t->faderDb = db;
    estat.info = "Volum de «"_ca + dibuix::utf8 (t->nom) + "»: "_ca + EstatEstudi::textDb (db);
    estat.avisa();
}

void PanellPista::mouseUp (const juce::MouseEvent&)
{
    if (movent) acabaCanvi();
    movent = false;
}

void PanellPista::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (estat.pista (estat.pistaTriada) == nullptr || ! zonaFader.expanded (10).contains (e.getPosition())) return;
    comencaCanvi();
    restaura();
    estat.pista (estat.pistaTriada)->faderDb = 0.0;
    acabaCanvi();
}
