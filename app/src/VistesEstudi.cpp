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

    juce::String textCanals (const Pista& t) { return t.canals == 2 ? juce::String::fromUTF8 ("estèreo") : juce::String ("mono"); }
}

//==============================================================================
// Dibuix compartit
//==============================================================================
void dibuixEstudi::teclaPetita (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, bool encesa, juce::Colour color, bool rodona)
{
    auto cara = vintage::tecla (g, r.toFloat(), encesa, false, juce::Colours::transparentBlack, true, false).expanded (3.0f, 2.0f);
    if (encesa)
    {
        g.setColour (color.withAlpha (0.9f));
        g.fillRoundedRectangle (cara.reduced (1.0f), 3.0f);
    }
    if (rodona)
    {
        g.setColour (encesa ? juce::Colour (0xff1a0d0b) : tema::error.withAlpha (0.85f));
        g.fillEllipse (cara.withSizeKeepingCentre (9.0f, 9.0f));
    }
    else
    {
        g.setColour (encesa ? juce::Colour (0xff15151a) : tema::textMig);
        g.setFont (dibuix::lletra (11.5f, true));
        g.drawText (text, cara, juce::Justification::centred);
    }
}

juce::Rectangle<float> dibuixEstudi::carrilVertical (juce::Rectangle<float> r)
{
    return { r.getCentreX() + 6.0f - 3.0f, r.getY() + 14.0f, 6.0f, juce::jmax (20.0f, r.getHeight() - 28.0f) };
}

juce::Rectangle<float> dibuixEstudi::faderVertical (juce::Graphics& g, juce::Rectangle<float> r, double db, juce::Colour color, bool actiu)
{
    const auto carril = carrilVertical (r);
    vintage::ranura (g, carril);
    vintage::escalaVertical (g, carril, carril.getX() - 34.0f, [] (double d) { return EstatEstudi::posFader (d); });
    const float pos = EstatEstudi::posFader (db);
    const float y = carril.getBottom() - carril.getHeight() * pos;
    vintage::capFader (g, juce::Rectangle<float> (carril.getCentreX() - 15.0f, y - 9.0f, 30.0f, 18.0f), color, true, actiu);
    return carril;
}

void dibuixEstudi::faderHoritzontal (juce::Graphics& g, juce::Rectangle<float> r, double db, juce::Colour color, bool actiu)
{
    auto carril = r.withSizeKeepingCentre (r.getWidth(), 6.0f);
    vintage::ranura (g, carril);
    // Marques.
    g.setColour (tema::linia2);
    for (double d : { -24.0, -12.0, -6.0, 0.0, 6.0 })
    {
        const float x = carril.getX() + carril.getWidth() * EstatEstudi::posFader (d);
        g.fillRect (juce::Rectangle<float> (x - 0.5f, r.getY() - 1.0f, 1.0f, std::abs (d) < 0.01 ? 4.0f : 3.0f));
    }
    const float x = carril.getX() + carril.getWidth() * EstatEstudi::posFader (db);
    vintage::capFader (g, juce::Rectangle<float> (x - 7.0f, r.getCentreY() - 12.0f, 14.0f, 24.0f), color, false, actiu);
}

//==============================================================================
// Regla
//==============================================================================
void Regla::paint (juce::Graphics& g)
{
    const auto& p = estat.projecte;
    const float w = (float) getWidth(), h = (float) getHeight();
    vintage::panell (g, getLocalBounds().toFloat());

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
        if (t % p.compasNum == 0)
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

    const float xc = estat.x (estat.capcal);
    juce::Path tri;
    tri.addTriangle (xc - 6.0f, h - 9.0f, xc + 6.0f, h - 9.0f, xc, h);
    g.setColour (tema::text);
    g.fillPath (tri);
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
    ed::marcaBucle (estat.projecte, compas0, ed::compasA (estat.projecte, estat.t ((float) e.x)));
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
juce::MouseCursor VistaPistes::cursor (Eina e)
{
    if (e == Eina::Fletxa)
        return juce::MouseCursor::NormalCursor;

    static std::map<int, juce::MouseCursor> fets;
    if (auto it = fets.find ((int) e); it != fets.end())
        return it->second;

    const dibuix::Icona icones[] = { dibuix::Icona::Fletxa2, dibuix::Icona::Llapis, dibuix::Icona::Goma,
                                     dibuix::Icona::Tisores, dibuix::Icona::Cola, dibuix::Icona::Silenci };
    const juce::Point<int> punts[] = { { 6, 3 }, { 4, 25 }, { 8, 23 }, { 14, 14 }, { 14, 26 }, { 14, 14 } };

    juce::Image img (juce::Image::ARGB, 28, 28, true);
    {
        juce::Graphics g (img);
        const auto r = juce::Rectangle<float> (1.0f, 1.0f, 26.0f, 26.0f);
        auto forma = dibuix::icona (icones[(int) e], r);
        g.setColour (juce::Colours::black);
        g.strokePath (forma, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white);
        g.strokePath (forma, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    const auto p = punts[(int) e];
    return fets[(int) e] = juce::MouseCursor (img, p.x, p.y);
}

double VistaPistes::tEina (double t) const
{
    return juce::jmax (0.0, ed::enganxa (estat.projecte, t, estat.projecte.snap));
}

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

    if (estat.eina == Eina::Fletxa)
    {
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
    }

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
    if (darrer - primer > 20000) return;

    for (int t = primer; t <= darrer; ++t)
    {
        const bool compas = t % p.compasNum == 0;
        if (! compas && ! temps) continue;
        g.setColour (juce::Colours::white.withAlpha (compas ? 0.10f : 0.04f));
        g.fillRect (juce::Rectangle<float> (estat.x (t * spt), 0.0f, 1.0f, h));
    }
}

void VistaPistes::pintaOna (juce::Graphics& g, const Clip& c, const Presa& s, OnaPresa& ona, juce::Rectangle<float> r, juce::Colour color)
{
    const float mig = r.getCentreY();
    const float amp = (r.getHeight() - 14.0f) / 2.0f;
    const double guany = std::pow (10.0, c.guanyDb / 20.0);

    // Línia del zero (amb molt de zoom s'hi veu on creua l'ona).
    g.setColour (color.withAlpha (0.35f));
    g.fillRect (juce::Rectangle<float> (r.getX(), mig - 0.5f, r.getWidth(), 1.0f));

    auto envolupant = [&c] (double tf)
    {
        double env = 1.0;
        if (c.fadeIn > 0.0 && tf < c.inici + c.fadeIn) env = juce::jmax (0.0, (tf - c.inici) / c.fadeIn);
        if (c.fadeOut > 0.0 && tf > c.fi - c.fadeOut) env = juce::jmin (env, juce::jmax (0.0, (c.fi - tf) / c.fadeOut));
        return env;
    };

    // Part visible, dins el fitxer.
    const double tIni = juce::jmax (c.inici, 0.0, estat.t (juce::jmax (r.getX(), 0.0f)) - c.pos);
    const double tFi = juce::jmin (c.fi, s.durada, estat.t (juce::jmin (r.getRight(), (float) getWidth())) - c.pos);
    if (tFi <= tIni || ona.longitud <= 0) return;

    const double mostresPerPx = ona.sr / estat.pxPerSegon;
    const auto colorOna = color.withAlpha (0.9f);

    if (mostresPerPx < 1.0)
    {
        // Molt de zoom: es veu cada mostra, unides amb una línia (i un puntet a cada mostra si hi cap).
        const auto a = (juce::int64) std::floor (tIni * ona.sr), b = (juce::int64) std::ceil (tFi * ona.sr);
        const int n = (int) juce::jmin ((juce::int64) 200000, b - a + 1);
        const float* m = ona.mostres (a, n);
        if (m == nullptr) return;
        juce::Path linia;
        for (int i = 0; i < n; ++i)
        {
            const double tf = (double) (a + i) / ona.sr;
            const float x = estat.x (c.pos + tf);
            const float y = mig - (float) (m[i] * guany * envolupant (tf)) * amp;
            if (i == 0) linia.startNewSubPath (x, y); else linia.lineTo (x, y);
        }
        g.setColour (colorOna);
        g.strokePath (linia, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (1.0 / mostresPerPx >= 7.0)
            for (int i = 0; i < n; ++i)
            {
                const double tf = (double) (a + i) / ona.sr;
                g.fillEllipse (estat.x (c.pos + tf) - 2.0f, mig - (float) (m[i] * guany * envolupant (tf)) * amp - 2.0f, 4.0f, 4.0f);
            }
        return;
    }

    // Lluny o a mig zoom: forma plena (màxims a dalt, mínims a baix), com una ona analògica.
    const float x0 = estat.x (c.pos + tIni), x1 = estat.x (c.pos + tFi);
    const int px = juce::jmax (1, (int) std::ceil (x1 - x0));
    std::vector<float> dalt ((size_t) px + 1), baix ((size_t) px + 1);

    const float* cru = nullptr;
    juce::int64 cruIni = 0;
    if (mostresPerPx < OnaPresa::bloc)
    {
        cruIni = (juce::int64) std::floor (tIni * ona.sr);
        const int n = (int) juce::jmin ((juce::int64) 2000000, (juce::int64) std::ceil ((tFi - tIni) * ona.sr) + 2);
        cru = ona.mostres (cruIni, n);
    }

    for (int i = 0; i <= px; ++i)
    {
        const double ta = tIni + (double) i / estat.pxPerSegon;
        const double tb = juce::jmin (tFi, ta + 1.0 / estat.pxPerSegon);
        auto sa = (juce::int64) (ta * ona.sr), sb = juce::jmax (sa + 1, (juce::int64) (tb * ona.sr));
        float mn = 0.0f, mx = 0.0f;
        if (cru != nullptr)
        {
            for (auto k = sa; k < sb; ++k)
            {
                const auto idx = k - cruIni;
                if (idx < 0) continue;
                const float v = cru[idx];
                mn = juce::jmin (mn, v); mx = juce::jmax (mx, v);
            }
        }
        else
        {
            const auto ba = (size_t) (sa / OnaPresa::bloc), bb = juce::jmin (ona.mins.size(), (size_t) (sb / OnaPresa::bloc) + 1);
            for (auto k = ba; k < bb; ++k) { mn = juce::jmin (mn, ona.mins[k]); mx = juce::jmax (mx, ona.maxs[k]); }
        }
        const float e = (float) (guany * envolupant (ta));
        dalt[(size_t) i] = mig - juce::jmax (0.5f, mx * e * amp);
        baix[(size_t) i] = mig - juce::jmin (-0.5f, mn * e * amp);
    }

    juce::Path forma;
    forma.startNewSubPath (x0, dalt[0]);
    for (int i = 1; i <= px; ++i) forma.lineTo (x0 + (float) i, dalt[(size_t) i]);
    for (int i = px; i >= 0; --i) forma.lineTo (x0 + (float) i, baix[(size_t) i]);
    forma.closeSubPath();
    g.setColour (color.withAlpha (0.75f));
    g.fillPath (forma);
    g.setColour (colorOna);
    g.strokePath (forma, juce::PathStrokeType (0.8f));
}

void VistaPistes::pintaClip (juce::Graphics& g, int pista, int indexClip, const Clip& c)
{
    const auto& p = estat.projecte;
    const auto& t = p.pistes[(size_t) pista];
    auto r = rectClip (pista, c);
    if (r.getRight() < -40.0f || r.getX() > (float) getWidth() + 40.0f) return;

    const bool triat = estat.clipTriat (pista, indexClip);
    const bool manetes = triat && estat.clipsTriats.size() == 1 && estat.eina == Eina::Fletxa;
    const bool actiu = sona (p, t) && ! c.mut;
    const auto color = actiu ? EstatEstudi::colorPista (t) : tema::suau;
    const float rad = 6.0f;
    const auto* s = ed::presa (p, c.presa);

    g.setColour (tema::fons);
    g.fillRoundedRectangle (r, rad);
    g.setColour (color.withAlpha (triat ? 0.26f : 0.15f));
    g.fillRoundedRectangle (r, rad);

    {
        juce::Graphics::ScopedSaveState estatG (g);
        g.reduceClipRegion (r.toNearestInt());

        if (s == nullptr)
        {
            // Finestra buida: ratlles en diagonal.
            g.setColour (color.withAlpha (0.12f));
            for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += 10.0f)
                g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), 1.0f);
        }
        else if (auto it = estat.ones.find (c.presa); it != estat.ones.end() && it->second != nullptr)
            pintaOna (g, c, *s, *it->second, r, color);

        const float yg = yGuany (c, r);
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

        if (c.guanyDb < -0.05 || manetes)
        {
            g.setColour (juce::Colours::white.withAlpha (manetes ? 0.8f : 0.45f));
            g.fillRect (juce::Rectangle<float> (r.getX(), yg - 0.5f, r.getWidth(), 1.0f));
        }

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
            juce::String nom = s != nullptr ? dibuix::utf8 (s->nom) : juce::String ("Finestra buida");
            if (c.mut) nom = "(silenciat) " + nom;
            const bool banderaAprop = ed::banderaVisible (p, c) && std::abs (estat.x (c.pos + s->primer) - xn) < 14.0f;
            g.drawText (nom, juce::Rectangle<float> (xn + (banderaAprop ? 12.0f : 0.0f), r.getY() + 3.0f, r.getRight() - xn - 6.0f, 16.0f),
                        juce::Justification::centredLeft, true);
        }

        // Silenciat: una X grossa i suau.
        if (c.mut)
        {
            g.setColour (tema::error.withAlpha (0.35f));
            g.drawLine (r.getX(), r.getY(), r.getRight(), r.getBottom(), 1.5f);
            g.drawLine (r.getX(), r.getBottom(), r.getRight(), r.getY(), 1.5f);
        }
    }

    // Vora.
    g.setColour (triat ? tema::text : color.withAlpha (0.65f));
    g.drawRoundedRectangle (r.reduced (0.5f), rad, triat ? 2.0f : 1.0f);

    // Tisores: on tallarà.
    if (estat.eina == Eina::Tisores && tTisores > c.iniciVisible() && tTisores < c.fiVisible() && sobre.pista == pista && sobre.clip == indexClip)
    {
        const float x = estat.x (tTisores);
        g.setColour (tema::accent);
        const float guions[] = { 4.0f, 3.0f };
        g.drawDashedLine (juce::Line<float> (x, r.getY() - 3.0f, x, r.getBottom() + 3.0f), guions, 2, 2.0f);
    }

    if (manetes)
    {
        const float yg = yGuany (c, r);
        const float xfi = estat.x (c.iniciVisible() + c.fadeIn), xfo = estat.x (c.fiVisible() - c.fadeOut);
        const auto m = sobre.pista == pista && sobre.clip == indexClip ? sobre.maneta : Maneta::Cap;
        auto maneta = [&] (juce::Rectangle<float> z, bool destacada)
        {
            g.setColour (destacada ? tema::accent : tema::text);
            g.fillRect (z);
        };
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

    for (int i = 0; i < estat.numPistes(); ++i)
    {
        const int y = estat.yPista (i);
        if (y > getHeight() || y + estat.altPista < 0) continue;
        g.setColour (i == estat.pistaTriada ? tema::triat : (i % 2 == 0 ? tema::fons : juce::Colour (0xff19191e)));
        g.fillRect (0, y, getWidth(), estat.altPista);
        g.setColour (tema::linia.withAlpha (0.6f));
        g.fillRect (0, y + estat.altPista - 1, getWidth(), 1);
    }

    // Abans del començament (quan el zoom centrat hi arriba): més fosc.
    if (estat.iniciVista < 0.0)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect (juce::Rectangle<float> (0.0f, 0.0f, estat.x (0.0), h));
    }

    pintaGraella (g);
    pintaBucleSobre (g, estat, h);

    for (int i = 0; i < estat.numPistes(); ++i)
    {
        const auto& t = estat.projecte.pistes[(size_t) i];
        for (int k = 0; k < (int) t.clips.size(); ++k) if (! estat.clipTriat (i, k)) pintaClip (g, i, k, t.clips[(size_t) k]);
        for (int k = 0; k < (int) t.clips.size(); ++k) if (estat.clipTriat (i, k)) pintaClip (g, i, k, t.clips[(size_t) k]);
    }

    // Llapis: la finestra que es crearà.
    if (mode == Mode::Llapis && lloc0.pista >= 0)
    {
        const double a = juce::jmin (tLlapis, t0), b = juce::jmax (tLlapis, t0);
        const double fi = b - a < 1e-6 ? a + ed::segonsPerCompas (estat.projecte) : b;
        auto r = juce::Rectangle<float> (estat.x (a), (float) estat.yPista (lloc0.pista) + 5.0f, estat.x (fi) - estat.x (a), (float) estat.altPista - 10.0f);
        g.setColour (tema::accent.withAlpha (0.18f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (tema::accent);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.5f);
    }

    // Cola: línia del primer clip al ratolí.
    if (mode == Mode::Cola)
    {
        g.setColour (tema::accent);
        g.drawArrow (juce::Line<float> (inici.toFloat(), puntCola.toFloat()), 2.0f, 10.0f, 10.0f);
    }

    {
        const int y = estat.yPista (estat.numPistes());
        auto r = juce::Rectangle<float> (8.0f, (float) y + 6.0f, juce::jmin (w - 16.0f, 300.0f), 32.0f);
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
    const double tt = estat.eina == Eina::Tisores ? tEina (estat.t ((float) e.x)) : -1.0;
    if (l.pista != sobre.pista || l.clip != sobre.clip || l.maneta != sobre.maneta || l.pistaNova != sobre.pistaNova || std::abs (tt - tTisores) > 1e-9)
    {
        sobre = l;
        tTisores = tt;
        repaint();
    }

    if (estat.eina != Eina::Fletxa)
    {
        setMouseCursor (l.pistaNova ? juce::MouseCursor::PointingHandCursor : cursor (estat.eina));
        return;
    }

    switch (l.maneta)
    {
        case Maneta::RetallInici:
        case Maneta::RetallFi:
        case Maneta::Bandera:
        case Maneta::FadeIn:
        case Maneta::FadeOut:   setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); break;
        case Maneta::Guany:     setMouseCursor (juce::MouseCursor::UpDownResizeCursor); break;
        case Maneta::Cap:       setMouseCursor (l.clip >= 0 ? juce::MouseCursor::DraggingHandCursor
                                                            : (l.pistaNova ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor)); break;
    }
}

void VistaPistes::menuPistaNova (juce::Component& on, std::function<void (const juce::String&, int)> tria)
{
    juce::PopupMenu m;
    m.addSectionHeader ("Pista nova");
    m.addItem (1, "Àudio mono"_ca);
    m.addItem (2, "Àudio estèreo"_ca);
    m.addSeparator();
    m.addItem (10, "Instrument (MIDI amb VST o sinte)  · aviat"_ca, false);
    m.addItem (11, "Bateria  · més endavant"_ca, false);
    m.addItem (12, "Acords  · més endavant"_ca, false);
    m.addItem (13, "Bus d'efectes  · més endavant"_ca, false);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&on).withMousePosition(), [tria] (int r)
    {
        if (r == 1) tria ("audio", 1);
        else if (r == 2) tria ("audio", 2);
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
    m.addItem (6, clip0.mut ? "Torna a fer-lo sonar" : "Silencia'l   (eina X)");
    m.addItem (7, "Talla aquí   (tisores)"_ca, unic && t > clip0.iniciVisible() && t < clip0.fiVisible());
    m.addSeparator();
    m.addItem (3, "Posa la bandera del primer temps aquí"_ca, unic && s != nullptr);
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
            case 6:
            {
                const bool nou = ! c.mut;
                for (int i : estat.clipsTriats) estat.projecte.pistes[(size_t) pista].clips[(size_t) i].mut = nou;
                break;
            }
            case 7: ed::talla (estat.projecte, pista, clip, tEina (t)); estat.clipsTriats.clear(); break;
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
        menuPistaNova (*this, [this] (const juce::String& tipus, int canals) { if (onPistaNova) onPistaNova (tipus, canals); });
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
            const double on = tEina (t0);
            juce::PopupMenu m;
            m.addItem (1, "Importa àudio aquí…"_ca);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this, fila, on] (int r)
            {
                if (r == 1 && onDemanaImportar) onDemanaImportar (fila, on);
            });
        }
        return;
    }

    switch (estat.eina)
    {
        case Eina::Fletxa:
            mouseDownFletxa (e);
            return;

        case Eina::Llapis:
            if (lloc0.pista < 0) return;
            estat.pistaTriada = lloc0.pista;
            estat.clipsTriats.clear();
            mode = Mode::Llapis;
            t0 = tLlapis = tEina (t0);
            estat.avisa();
            return;

        case Eina::Goma:
            comencaCanvi();
            mode = Mode::Goma;
            mouseDrag (e);
            return;

        case Eina::Tisores:
            if (lloc0.clip < 0) return;
            comencaCanvi();
            restaura();
            if (! ed::talla (estat.projecte, lloc0.pista, lloc0.clip, tEina (t0)))
                canviFet = false;
            estat.pistaTriada = lloc0.pista;
            estat.clipsTriats.clear();
            acabaCanvi();
            return;

        case Eina::Cola:
            if (lloc0.clip < 0) return;
            estat.pistaTriada = lloc0.pista;
            estat.clipsTriats = { lloc0.clip };
            mode = Mode::Cola;
            puntCola = inici;
            estat.info = "Arrossega fins al clip que vols enganxar-hi";
            estat.avisa();
            return;

        case Eina::Silenci:
            if (lloc0.clip < 0) return;
            comencaCanvi();
            restaura();
            {
                auto& c = estat.projecte.pistes[(size_t) lloc0.pista].clips[(size_t) lloc0.clip];
                c.mut = ! c.mut;
                estat.info = c.mut ? "Clip silenciat" : "Clip sonant";
            }
            acabaCanvi();
            return;
    }
}

void VistaPistes::mouseDownFletxa (const juce::MouseEvent& e)
{
    if (lloc0.pista < 0)
    {
        estat.clipsTriats.clear();
        mode = Mode::Capcal;
        estat.capcal = tEina (t0);
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
            return;
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

    estat.pistaTriada = lloc0.pista;
    estat.clipsTriats.clear();
    mode = Mode::Capcal;
    estat.capcal = tEina (t0);
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
            estat.capcal = tEina (t);
            estat.avisa();
            return;

        case Mode::Llapis:
            tLlapis = tEina (t);
            estat.info = "Finestra: "_ca + posicio (p, juce::jmin (t0, tLlapis)) + " a " + posicio (p, juce::jmax (t0, tLlapis));
            repaint();
            estat.avisa();
            return;

        case Mode::Goma:
        {
            const auto l = llocA (e.getPosition());
            if (l.clip >= 0 && l.pista >= 0)
            {
                ed::treuClips (p, l.pista, { l.clip });
                canviFet = true;
                estat.clipsTriats.clear();
                estat.info = "Esborrat";
                estat.avisa();
            }
            return;
        }

        case Mode::Cola:
            puntCola = e.getPosition();
            repaint();
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
                    if (const auto* s = ed::presa (p, c.presa))
                        estat.info = "Bandera: "_ca + posicio (p, c.pos + s->primer, true);
                    break;
                case Mode::Res: case Mode::Capcal: case Mode::Decidint: case Mode::MouTemps: case Mode::MouPista:
                case Mode::Llapis: case Mode::Goma: case Mode::Cola:
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

    switch (mode)
    {
        case Mode::Decidint:
            if (! e.mods.isCommandDown() && ! e.mods.isShiftDown())
                estat.clipsTriats = { lloc0.clip };
            canviFet = false;
            estat.avisa();
            break;

        case Mode::Llapis:
        {
            comencaCanvi();
            restaura();
            double a = juce::jmin (t0, tLlapis), b = juce::jmax (t0, tLlapis);
            if (b - a < 1e-6) b = a + ed::segonsPerCompas (estat.projecte);   // sense arrossegar: un compàs
            const int nou = ed::creaFinestra (estat.projecte, lloc0.pista, a, b);
            estat.pistaTriada = lloc0.pista;
            estat.clipsTriats = { nou };
            acabaCanvi();
            break;
        }

        case Mode::Goma:
            acabaCanvi();
            break;

        case Mode::Cola:
        {
            const auto l = llocA (e.getPosition());
            estat.info.clear();
            if (l.clip >= 0 && l.pista == lloc0.pista && l.clip != lloc0.clip)
            {
                comencaCanvi();
                restaura();
                const auto r = ed::enganxaClips (estat.projecte, lloc0.pista, lloc0.clip, l.clip);
                if (r.fet)
                {
                    estat.clipsTriats = { r.clip };
                    acabaCanvi();
                }
                else
                {
                    canviFet = false;
                    if (r.calConsolidar && onConsolida) onConsolida (lloc0.pista, lloc0.clip, l.clip);
                }
            }
            else if (l.clip >= 0 && l.pista != lloc0.pista)
                estat.info = "Només es poden enganxar clips de la mateixa pista"_ca;
            estat.avisa();
            break;
        }

        case Mode::Res:
        case Mode::Capcal:
            break;

        case Mode::MouTemps: case Mode::MouPista: case Mode::RetallInici: case Mode::RetallFi:
        case Mode::FadeIn: case Mode::FadeOut: case Mode::Guany: case Mode::Bandera:
            acabaCanvi();
            break;
    }
    triatsOriginals.clear();
    mode = Mode::Res;
    repaint();
}

void VistaPistes::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto l = llocA (e.getPosition());
    if (l.pista < 0 && ! l.pistaNova && e.y > estat.yPista (estat.numPistes()))
        menuPistaNova (*this, [this] (const juce::String& tipus, int canals) { if (onPistaNova) onPistaNova (tipus, canals); });
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
    if (onImporta) onImporta (files, (fila >= 0 && fila < estat.numPistes()) ? fila : -1, tEina (estat.t ((float) x)));
    repaint();
}

//==============================================================================
// Capcaleres
//==============================================================================
juce::Rectangle<int> Capcaleres::rectTecla (int pista, int quina) const
{
    const int y = estat.yPista (pista);
    const int baix = y + estat.altPista - 9;
    return { 14 + quina * 31, baix - 24, 28, 24 };
}

Capcaleres::Tecla Capcaleres::teclaA (int pista, juce::Point<int> p) const
{
    if (rectTecla (pista, 0).contains (p)) return Tecla::Mut;
    if (rectTecla (pista, 1).contains (p)) return Tecla::Solo;
    if (rectTecla (pista, 2).contains (p)) return Tecla::Grava;
    return Tecla::Cap;
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
        g.setColour (sona (p, t) ? EstatEstudi::colorPista (t) : tema::suau);
        g.fillRect (r.getX(), r.getY() + 4, 4, r.getHeight() - 8);

        auto text = r.withTrimmedLeft (14).withTrimmedRight (8).withTrimmedTop (7);
        auto fila = text.removeFromTop (20);
        g.setColour (tema::suau);
        g.setFont (dibuix::mono (12.0f));
        g.drawText (juce::String (i + 1), fila.removeFromLeft (22), juce::Justification::centredLeft);
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (14.0f, true));
        g.drawText (dibuix::utf8 (t.nom), fila, juce::Justification::centredLeft, true);

        dibuixEstudi::teclaPetita (g, rectTecla (i, 0), "M", t.mut, vintage::ledBlau);
        dibuixEstudi::teclaPetita (g, rectTecla (i, 1), "S", t.solo, vintage::ledGroc);
        dibuixEstudi::teclaPetita (g, rectTecla (i, 2), {}, t.grava, vintage::ledVermell, true);

        const auto zonaCanals = juce::Rectangle<int> (rectTecla (i, 2).getRight() + 8, rectTecla (i, 2).getY(), 60, 24);
        if (zonaCanals.getRight() < getWidth() - 4)
        {
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (11.0f));
            g.drawText (textCanals (t), zonaCanals, juce::Justification::centredLeft);
        }

        g.setColour (tema::linia.withAlpha (0.6f));
        g.fillRect (0, r.getBottom() - 1, getWidth(), 1);
    }

    {
        const int y = estat.yPista (estat.numPistes());
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (13.0f));
        g.drawText ("+ Pista nova", juce::Rectangle<int> (14, y + 6, getWidth() - 20, 32), juce::Justification::centredLeft, true);
    }

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
    const auto& t = estat.projecte.pistes[(size_t) pista];
    juce::PopupMenu colors;
    for (int c = 0; c < ed::colorsPista; ++c)
        colors.addColouredItem (20 + c, EstatEstudi::nomColor (c), EstatEstudi::colorPista (c), true, t.color == c);

    juce::PopupMenu m;
    m.addItem (1, "Canvia el nom…"_ca);
    m.addSubMenu ("Color", colors);
    m.addItem (3, t.canals == 2 ? "Fes-la mono" : "Fes-la estèreo"_ca);
    m.addSeparator();
    m.addItem (2, "Esborra la pista");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this, pista] (int r)
    {
        if (r == 0 || pista >= estat.numPistes()) return;
        if (r == 1) { if (onCanviaNom) onCanviaNom (pista); return; }
        comencaCanvi();
        restaura();
        auto& tt = estat.projecte.pistes[(size_t) pista];
        juce::String evolucio;
        if (r == 2)
        {
            evolucio = "Pista esborrada: «"_ca + dibuix::utf8 (tt.nom) + "»"_ca;
            ed::esborraPista (estat.projecte, pista);
            estat.clipsTriats.clear();
        }
        else if (r == 3) tt.canals = tt.canals == 2 ? 1 : 2;
        else if (r >= 20) tt.color = r - 20;
        acabaCanvi (evolucio);
    });
}

void Capcaleres::mouseDown (const juce::MouseEvent& e)
{
    const int fila = estat.pistaA (e.y);
    arrossegant = desti = -1;

    if (fila == estat.numPistes() && e.y < estat.yPista (fila) + 44)
    {
        VistaPistes::menuPistaNova (*this, [this] (const juce::String& tipus, int canals) { if (onPistaNova) onPistaNova (tipus, canals); });
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

    const auto b = teclaA (fila, e.getPosition());
    if (b != Tecla::Cap)
    {
        comencaCanvi();
        restaura();
        auto& t = estat.projecte.pistes[(size_t) fila];
        if (b == Tecla::Mut) t.mut = ! t.mut;
        else if (b == Tecla::Solo) t.solo = ! t.solo;
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
    if (fila >= 0 && fila < estat.numPistes() && teclaA (fila, e.getPosition()) == Tecla::Cap && onCanviaNom)
        onCanviaNom (fila);
}

//==============================================================================
// Faders (horitzontals, un per pista)
//==============================================================================
juce::Rectangle<float> Faders::rectFader (int pista) const
{
    const float y = (float) estat.yPista (pista);
    const float mig = y + (float) estat.altPista * 0.5f + 8.0f;
    return { 16.0f, mig - 12.0f, juce::jmax (30.0f, (float) getWidth() - 32.0f), 24.0f };
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
        g.setColour (tema::suau);
        g.setFont (dibuix::mono (11.5f));
        g.drawText (EstatEstudi::textDb (t.faderDb), juce::Rectangle<float> (r.getX(), (float) y + 7.0f, r.getWidth(), 16.0f),
                    juce::Justification::centredLeft);
        dibuixEstudi::faderHoritzontal (g, r, t.faderDb, EstatEstudi::colorPista (t), sona (p, t));
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
    if (std::abs (db) < 0.6) db = 0.0;
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
// ColumnaEsquerra
//==============================================================================
ColumnaEsquerra::ColumnaEsquerra (EstatEstudi& e) : SeccioEstudi (e)
{
    for (auto* b : { &btTreball, &btMescles })
    {
        addAndMakeVisible (b);
        b->setMidaText (12.0f);
        b->setLlum (vintage::ledTaronja);
    }
    btTreball.onClick = [this] { estat.vistaMescles = false; estat.avisa(); };
    btMescles.onClick = [this] { estat.vistaMescles = true; estat.avisa(); };
    changeListenerCallback (nullptr);
}

void ColumnaEsquerra::changeListenerCallback (juce::ChangeBroadcaster*)
{
    btTreball.setToggleState (! estat.vistaMescles, juce::dontSendNotification);
    btMescles.setToggleState (estat.vistaMescles, juce::dontSendNotification);
    repaint();
}

void ColumnaEsquerra::resized()
{
    auto r = getLocalBounds().reduced (8, 10);
    btTreball.setBounds (r.removeFromTop (44));
    r.removeFromTop (6);
    btMescles.setBounds (r.removeFromTop (44));
    r.removeFromTop (16);
    r.removeFromTop (44);   // nom de la pista
    zonaTecles = r.removeFromBottom (30);
    r.removeFromBottom (26);   // valor
    zonaFader = r;
}

void ColumnaEsquerra::paint (juce::Graphics& g)
{
    vintage::panell (g, getLocalBounds().toFloat());
    g.setColour (tema::linia);
    g.fillRect (getWidth() - 1, 0, 1, getHeight());

    auto* t = estat.pista (estat.pistaTriada);
    auto cap = juce::Rectangle<int> (10, btMescles.getBottom() + 14, getWidth() - 20, 40);
    if (t == nullptr)
    {
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (12.0f));
        g.drawFittedText ("Tria una pista", cap, juce::Justification::centredLeft, 2);
        return;
    }

    g.setColour (EstatEstudi::colorPista (*t));
    g.fillRect (cap.getX(), cap.getY() + 2, 4, 34);
    g.setColour (tema::text);
    g.setFont (dibuix::lletra (13.0f, true));
    g.drawFittedText (dibuix::utf8 (t->nom), cap.withTrimmedLeft (10).removeFromTop (20), juce::Justification::centredLeft, 1, 0.8f);
    g.setColour (tema::suau);
    g.setFont (dibuix::lletra (11.0f));
    g.drawText ("Pista " + juce::String (estat.pistaTriada + 1) + " · "_ca + textCanals (*t), cap.withTrimmedLeft (10).withTrimmedTop (20),
                juce::Justification::centredLeft, true);

    dibuixEstudi::faderVertical (g, zonaFader.toFloat(), t->faderDb, EstatEstudi::colorPista (*t), sona (estat.projecte, *t));

    vintage::lcd (g, juce::Rectangle<float> ((float) zonaFader.getX() + 6.0f, (float) zonaFader.getBottom() + 2.0f, (float) zonaFader.getWidth() - 12.0f, 22.0f),
                  EstatEstudi::textDb (t->faderDb), {});

    const int ample = 28, sep = 4;
    auto tecles = zonaTecles.withSizeKeepingCentre (ample * 3 + sep * 2, 26);
    dibuixEstudi::teclaPetita (g, tecles.removeFromLeft (ample), "M", t->mut, vintage::ledBlau);
    tecles.removeFromLeft (sep);
    dibuixEstudi::teclaPetita (g, tecles.removeFromLeft (ample), "S", t->solo, vintage::ledGroc);
    tecles.removeFromLeft (sep);
    dibuixEstudi::teclaPetita (g, tecles.removeFromLeft (ample), {}, t->grava, vintage::ledVermell, true);
}

void ColumnaEsquerra::mouseDown (const juce::MouseEvent& e)
{
    auto* t = estat.pista (estat.pistaTriada);
    if (t == nullptr) return;

    auto tecles = zonaTecles.withSizeKeepingCentre (28 * 3 + 8, 26);
    for (int i = 0; i < 3; ++i)
    {
        if (juce::Rectangle<int> (tecles.getX() + i * 32, tecles.getY(), 28, 26).contains (e.getPosition()))
        {
            comencaCanvi();
            restaura();
            auto& tt = estat.projecte.pistes[(size_t) estat.pistaTriada];
            if (i == 0) tt.mut = ! tt.mut;
            else if (i == 1) tt.solo = ! tt.solo;
            else ed::triaGravacio (estat.projecte, estat.pistaTriada);
            acabaCanvi();
            return;
        }
    }

    movent = zonaFader.contains (e.getPosition());
    if (! movent) return;
    pos0 = EstatEstudi::posFader (t->faderDb);
    comencaCanvi();
}

void ColumnaEsquerra::mouseDrag (const juce::MouseEvent& e)
{
    if (! movent) return;
    restaura();
    auto* t = estat.pista (estat.pistaTriada);
    if (t == nullptr) return;
    const auto carril = dibuixEstudi::carrilVertical (zonaFader.toFloat());
    double db = EstatEstudi::dbFader (pos0 - (float) e.getDistanceFromDragStartY() / carril.getHeight());
    if (std::abs (db) < 0.6) db = 0.0;
    t->faderDb = db;
    estat.info = "Volum de «"_ca + dibuix::utf8 (t->nom) + "»: "_ca + EstatEstudi::textDb (db);
    estat.avisa();
}

void ColumnaEsquerra::mouseUp (const juce::MouseEvent&)
{
    if (movent) acabaCanvi();
    movent = false;
}

void ColumnaEsquerra::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (estat.pista (estat.pistaTriada) == nullptr || ! zonaFader.contains (e.getPosition())) return;
    comencaCanvi();
    restaura();
    estat.pista (estat.pistaTriada)->faderDb = 0.0;
    acabaCanvi();
}

//==============================================================================
// TaulaMescles
//==============================================================================
juce::Rectangle<int> TaulaMescles::rectTecla (int i, int quina) const
{
    auto r = rectTira (i);
    const int ample = 26, sep = 3;
    const int x0 = r.getCentreX() - (ample * 3 + sep * 2) / 2;
    return { x0 + quina * (ample + sep), 46, ample, 24 };
}

juce::Rectangle<float> TaulaMescles::zonaFader (int i) const
{
    auto r = rectTira (i);
    return juce::Rectangle<int> (r.getX() + 6, 82, r.getWidth() - 12, juce::jmax (80, getHeight() - 82 - 64)).toFloat();
}

void TaulaMescles::paint (juce::Graphics& g)
{
    g.fillAll (tema::fons);
    const auto& p = estat.projecte;

    if (estat.numPistes() == 0)
    {
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (14.0f));
        g.drawText ("Encara no hi ha pistes. Crea'n una a la taula de treball.", getLocalBounds().reduced (20), juce::Justification::topLeft);
        return;
    }

    for (int i = 0; i < estat.numPistes(); ++i)
    {
        auto r = rectTira (i);
        if (r.getRight() < 0 || r.getX() > getWidth()) continue;
        const auto& t = p.pistes[(size_t) i];
        const auto color = EstatEstudi::colorPista (t);

        vintage::panell (g, r.reduced (2, 0).toFloat(), 6.0f);
        if (i == estat.pistaTriada)
        {
            g.setColour (tema::accent.withAlpha (0.6f));
            g.drawRoundedRectangle (r.reduced (2, 0).toFloat().reduced (0.5f), 6.0f, 1.5f);
        }
        g.setColour (color);
        g.fillRect (r.getX() + 8, 8, r.getWidth() - 16, 3);
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (12.0f, true));
        g.drawFittedText (dibuix::utf8 (t.nom), juce::Rectangle<int> (r.getX() + 6, 16, r.getWidth() - 12, 24), juce::Justification::centred, 1, 0.75f);

        dibuixEstudi::teclaPetita (g, rectTecla (i, 0), "M", t.mut, vintage::ledBlau);
        dibuixEstudi::teclaPetita (g, rectTecla (i, 1), "S", t.solo, vintage::ledGroc);
        dibuixEstudi::teclaPetita (g, rectTecla (i, 2), {}, t.grava, vintage::ledVermell, true);

        const auto zf = zonaFader (i);
        dibuixEstudi::faderVertical (g, zf, t.faderDb, color, sona (p, t));

        vintage::lcd (g, juce::Rectangle<float> ((float) r.getX() + 8.0f, zf.getBottom() + 6.0f, (float) r.getWidth() - 16.0f, 22.0f),
                      EstatEstudi::textDb (t.faderDb), {});
        g.setColour (tema::suau);
        g.setFont (dibuix::mono (12.0f));
        g.drawText (juce::String (i + 1), juce::Rectangle<int> (r.getX(), getHeight() - 26, r.getWidth(), 20), juce::Justification::centred);
    }
}

void TaulaMescles::mouseDown (const juce::MouseEvent& e)
{
    const int i = tiraA (e.x);
    tiraFader = -1;
    if (i < 0 || i >= estat.numPistes()) return;
    if (estat.pistaTriada != i) estat.clipsTriats.clear();
    estat.pistaTriada = i;

    for (int k = 0; k < 3; ++k)
    {
        if (rectTecla (i, k).contains (e.getPosition()))
        {
            comencaCanvi();
            restaura();
            auto& t = estat.projecte.pistes[(size_t) i];
            if (k == 0) t.mut = ! t.mut;
            else if (k == 1) t.solo = ! t.solo;
            else ed::triaGravacio (estat.projecte, i);
            acabaCanvi();
            return;
        }
    }

    if (zonaFader (i).contains (e.position))
    {
        tiraFader = i;
        pos0 = EstatEstudi::posFader (estat.projecte.pistes[(size_t) i].faderDb);
        comencaCanvi();
    }
    estat.avisa();
}

void TaulaMescles::mouseDrag (const juce::MouseEvent& e)
{
    if (tiraFader < 0) return;
    restaura();
    auto& t = estat.projecte.pistes[(size_t) tiraFader];
    const auto carril = dibuixEstudi::carrilVertical (zonaFader (tiraFader));
    double db = EstatEstudi::dbFader (pos0 - (float) e.getDistanceFromDragStartY() / carril.getHeight());
    if (std::abs (db) < 0.6) db = 0.0;
    t.faderDb = db;
    estat.info = "Volum de «"_ca + dibuix::utf8 (t.nom) + "»: "_ca + EstatEstudi::textDb (db);
    estat.avisa();
}

void TaulaMescles::mouseUp (const juce::MouseEvent&)
{
    if (tiraFader >= 0) acabaCanvi();
    tiraFader = -1;
}

void TaulaMescles::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int i = tiraA (e.x);
    if (i < 0 || i >= estat.numPistes() || ! zonaFader (i).contains (e.position)) return;
    comencaCanvi();
    restaura();
    estat.projecte.pistes[(size_t) i].faderDb = 0.0;
    acabaCanvi();
}

void TaulaMescles::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float d = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
    const int maxim = juce::jmax (0, estat.numPistes() * ampleTira - getWidth());
    despl = juce::jlimit (0, maxim, despl - juce::roundToInt (d * 200.0f));
    repaint();
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
    auto botons = getLocalBounds().reduced (18, 16).withTrimmedTop (230).withHeight (36);
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
    auto info = r.withHeight (220);
    auto* c = estat.clipUnicTriat();
    const auto& p = estat.projecte;

    dibuix::pintaEtiquetaSeccio (g, c != nullptr ? "Clip triat" : "Clips", info.removeFromTop (20));
    info.removeFromTop (4);
    if (c != nullptr)
    {
        const auto* s = ed::presa (p, c->presa);
        auto linia = [&] (const juce::String& etiqueta, const juce::String& valor)
        {
            auto fila = info.removeFromTop (24);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (12.0f));
            g.drawText (etiqueta, fila.removeFromLeft (74), juce::Justification::centredLeft);
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (12.5f));
            g.drawText (valor, fila, juce::Justification::centredLeft, true);
        };
        linia ("Presa", s ? dibuix::utf8 (s->nom) : juce::String ("finestra buida"));
        linia ("Fitxer", s ? dibuix::utf8 (s->fitxer).fromLastOccurrenceOf ("/", false, false) : juce::String ("-"));
        linia ("Comença"_ca, posicio (p, c->iniciVisible(), true));
        linia ("Llargada", juce::String (c->longitud(), 2).replaceCharacter ('.', ',') + " s");
        linia ("Fades", ms (c->fadeIn) + " / " + ms (c->fadeOut));
        linia ("Volum", EstatEstudi::textDb (c->guanyDb) + (c->mut ? " · silenciat"_ca : ""));
        linia ("Bandera", s && ed::banderaVisible (p, *c) ? posicio (p, c->pos + s->primer, true)
                                                          : (s && s->primer >= 0 ? juce::String ("fora del clip") : juce::String ("no en té"_ca)));
    }
    else
    {
        g.setColour (estat.clipsTriats.size() > 1 ? tema::textMig : tema::suau);
        g.setFont (dibuix::lletra (12.5f));
        g.drawFittedText (estat.clipsTriats.size() > 1 ? juce::String (estat.clipsTriats.size()) + " clips triats"
                                                       : "Clica un clip (amb la fletxa) per veure'n les dades."_ca,
                          info.removeFromTop (40), juce::Justification::topLeft, 2);
    }

    r.removeFromTop (290);
    dibuix::pintaEtiquetaSeccio (g, "Eines", r.removeFromTop (20));
    const char* linies[] = {
        "Fletxa (1): tria i mou. Cantonades de baix: retallar; de dalt: fades; punt del mig: volum.",
        "Llapis (2): clica i arrossega per fer una finestra; sense arrossegar, d'un compàs.",
        "Goma (3): clica o passa per sobre dels clips per esborrar-los.",
        "Tisores (4): talla el clip on cliques (segons la graella).",
        "Cola (5): clica un clip i arrossega fins al del costat: en fa una sola finestra.",
        "X (6): silencia o torna a fer sonar un clip.",
        "Roda: amunt i avall · Maj: de costat · Ctrl: zoom · Alt: alçada.",
    };
    g.setFont (dibuix::lletra (12.0f));
    for (auto* l : linies)
    {
        if (r.getHeight() < 34) break;
        g.setColour (tema::suau);
        g.drawFittedText (juce::String::fromUTF8 (l), r.removeFromTop (34), juce::Justification::topLeft, 2, 0.9f);
    }
}
