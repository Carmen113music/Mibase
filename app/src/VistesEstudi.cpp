#include "VistesEstudi.h"
#include "BancGM.h"
#include "TempoPart.h"

#include <mibase/Midi.h>

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

    void pintaCapcal (juce::Graphics& g, float x, float alt, int transport = 0)
    {
        g.setColour (transport >= 2 ? vintage::ledVermell : (transport == 1 ? vintage::ledVerd : tema::text));
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

    juce::String textCanals (const Pista& t)
    {
        if (t.tipus == "bateria") return "Bateria";
        if (t.tipus == "midi") return "MIDI";
        return t.canals == 2 ? juce::String::fromUTF8 ("estèreo") : juce::String ("mono");
    }

    /** «sortida 1/2» (0.17). */
    juce::String textSortidaMaster (const Projecte& p)
    {
        return "sortida " + juce::String (2 * p.sortidaMaster + 1) + "/" + juce::String (2 * p.sortidaMaster + 2);
    }

    /** «MIDI · Piano» o «mono · E1». */
    juce::String textPistaCurt (const Pista& t)
    {
        if (t.tipus == "bateria") return juce::String::fromUTF8 (("Bateria · " + (t.kit.empty() ? std::string ("Intern") : t.kit)).c_str());
        if (t.tipus == "midi") return juce::String::fromUTF8 (("MIDI · " + midi::nomInstrument (t.instrument.empty() ? "intern:piano" : t.instrument)).c_str());
        return textCanals (t) + juce::String::fromUTF8 (" · ") + EstatEstudi::textEntrada (t);
    }
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
    return { r.getX() + 41.0f, r.getY() + 16.0f, 6.0f, juce::jmax (20.0f, r.getHeight() - 32.0f) };
}

void dibuixEstudi::vumetres (juce::Graphics& g, juce::Rectangle<float> r, float l, float d)
{
    const auto pos = [] (double db) { return EstatEstudi::posFader (db); };
    const float ample = juce::jmax (3.0f, (r.getWidth() - 2.0f) / 2.0f);
    vintage::vumetre (g, r.withWidth (ample), l, pos);
    vintage::vumetre (g, r.withTrimmedLeft (r.getWidth() - ample), d, pos);
}

juce::Rectangle<float> dibuixEstudi::faderVertical (juce::Graphics& g, juce::Rectangle<float> r, double db, juce::Colour color, bool actiu,
                                                    float nivellL, float nivellR)
{
    const auto carril = carrilVertical (r);
    vintage::ranura (g, carril);
    vintage::escalaVertical (g, carril, carril.getX() - 34.0f, [] (double d) { return EstatEstudi::posFader (d); });
    if (nivellL >= 0.0f)
        vumetres (g, juce::Rectangle<float> (r.getRight() - 13.0f, carril.getY(), 12.0f, carril.getHeight()), nivellL, nivellR);
    const float pos = EstatEstudi::posFader (db);
    const float y = carril.getBottom() - carril.getHeight() * pos;
    vintage::capFader (g, juce::Rectangle<float> (carril.getCentreX() - 17.0f, y - 13.0f, 34.0f, 26.0f), color, true, actiu);
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
namespace
{
    // Colors de les parts de la cançó (0.13).
    const juce::Colour coloresParts[] = { juce::Colour (0xffe8743b), juce::Colour (0xff59d0d6), juce::Colour (0xffb18cff), juce::Colour (0xff7fd67a),
                                          juce::Colour (0xffff8fc4), juce::Colour (0xfff5b445), juce::Colour (0xff6fa8ff), juce::Colour (0xffd9534f) };
    juce::Colour colorPart (const mibase::Part& x, int i)
    {
        return x.rgb != 0 ? juce::Colour (0xff000000u | x.rgb) : coloresParts[(size_t) (i % 8)];
    }
}

double Regla::compas (double t) const { return ed::compasDeSegons (estat.projecte, t); }

namespace
{
    /** 0.28: arrodoneix cap avall (o amunt) a la graella, amb el mapa de tempo. */
    double graellaAvall (const mibase::Projecte& p, double t, bool amunt)
    {
        if (p.snap == Snap::Lliure) return t;
        if (p.snap == Snap::Compas)
        {
            const double c = ed::compasDeSegons (p, t);
            return ed::segonsDeCompas (p, amunt ? std::ceil (c - 1e-9) : std::floor (c + 1e-9));
        }
        const auto tr = ed::tramA (p, t);
        const double pas = tr.segonsPerTemps(), k = (t - tr.segons) / pas;
        return tr.segons + (amunt ? std::ceil (k - 1e-9) : std::floor (k + 1e-9)) * pas;
    }
}

/** Una selecció de la regla ajustada a la graella: a Compàs, compassos sencers; a Temps, temps; a Lliure, exacta. */
std::pair<double, double> Regla::seleccio (double ta, double tb) const
{
    const auto& p = estat.projecte;
    double a = juce::jmax (0.0, juce::jmin (ta, tb)), b = juce::jmax (0.0, juce::jmax (ta, tb));
    if (p.snap != Snap::Lliure)
    {
        a = graellaAvall (p, a, false);
        b = graellaAvall (p, b, true);
        if (b <= a + 1e-9) b = graellaAvall (p, a + 1e-6, true);
    }
    return { compas (a), compas (b) };
}

double Regla::vora (double t) const
{
    const auto& p = estat.projecte;
    if (p.snap == Snap::Lliure) return compas (juce::jmax (0.0, t));
    return compas (juce::jmax (0.0, ed::enganxa (p, t, p.snap)));
}

void Regla::paint (juce::Graphics& g)
{
    const auto& p = estat.projecte;
    const float w = (float) getWidth(), h = (float) getHeight();
    const float mig = h * 0.5f;
    const double spc = ed::segonsPerCompas (p);
    vintage::panell (g, getLocalBounds().toFloat());
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    g.fillRect (0.0f, 0.0f, w, mig);

    // Meitat de dalt: parts de la cançó i, a sobre, el bucle.
    for (int i = 0; i < (int) p.parts.size(); ++i)
    {
        const auto& x = p.parts[(size_t) i];
        const float xa = estat.x (ed::segonsDeCompas (p, x.de)), xb = estat.x (ed::segonsDeCompas (p, x.a));
        if (xb < 0.0f || xa > w) continue;
        const auto col = colorPart (x, i);
        auto r = juce::Rectangle<float> (xa, 2.0f, xb - xa, mig - 4.0f);
        g.setColour (col.withAlpha (0.55f));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (col);
        g.drawRoundedRectangle (r, 3.0f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.setFont (dibuix::lletra (11.5f, true));
        g.drawText (dibuix::utf8 (x.nom), r.reduced (5.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, true);
    }
    if (p.bucle.marcat)
    {
        const float xa = estat.x (ed::iniciBucle (p)), xb = estat.x (ed::fiBucle (p));
        const auto color = p.bucle.actiu ? tema::guitarra : tema::suau;
        g.setColour (color.withAlpha (p.bucle.actiu ? 0.30f : 0.14f));
        g.fillRect (juce::Rectangle<float> (xa, 0.0f, xb - xa, mig));
        g.setColour (color.withAlpha (0.9f));
        g.fillRect (juce::Rectangle<float> (xa, mig - 2.0f, xb - xa, 2.0f));
        // Tiradors a les vores.
        g.fillRect (juce::Rectangle<float> (xa, 0.0f, 3.0f, mig));
        g.fillRect (juce::Rectangle<float> (xb - 3.0f, 0.0f, 3.0f, mig));
    }

    // Meitat de baix: compassos, temps i el capçal (0.28: compàs a compàs, amb el mapa de tempo).
    const double pxCompas = spc * estat.pxPerSegon;
    int cada = 1;
    while (pxCompas * cada < 36.0) cada *= 2;
    const auto mapa = ed::mapaTempo (p);
    const int primer = juce::jmax (0, (int) std::floor (ed::compasDeSegons (p, juce::jmax (0.0, estat.t (0.0f)))));
    const int darrer = (int) std::ceil (ed::compasDeSegons (p, juce::jmax (0.0, estat.t (w))));
    for (int c = primer; c <= darrer && c - primer < 20000; ++c)
    {
        const double sc = ed::segonsDeCompas (p, (double) c);
        const float x = estat.x (sc);
        const auto tr = ed::tramA (p, sc + 1e-6);
        const bool numera = c % cada == 0;
        g.setColour (numera ? tema::linia2 : tema::linia);
        g.fillRect (juce::Rectangle<float> (x, numera ? mig : h * 0.72f, 1.0f, h));
        if (numera)
        {
            const int n = ed::numCompas (c);
            g.setColour (n == 0 ? tema::accent : tema::textMig);
            g.setFont (dibuix::mono (11.5f));
            g.drawText (juce::String (n), juce::Rectangle<float> (x + 4.0f, mig, 40.0f, h - mig - 2.0f), juce::Justification::centredLeft);
        }
        const double spt = tr.segonsPerTemps();
        if (spt * estat.pxPerSegon >= 7.0)
            for (int k = 1; k < tr.num; ++k)
            {
                g.setColour (tema::linia);
                g.fillRect (juce::Rectangle<float> (estat.x (sc + k * spt), h * 0.8f, 1.0f, h * 0.2f));
            }
    }
    // 0.28: on canvia el tempo o el compàs, ho diu
    if (mapa.size() > 1)
        for (size_t i = 0; i < mapa.size(); ++i)
        {
            const float x = estat.x (mapa[i].segons);
            if (x < -60.0f || x > w) continue;
            const auto txt = juce::String (mapa[i].tempo, 0) + " · " + juce::String (mapa[i].num) + "/" + juce::String (mapa[i].den);
            g.setFont (dibuix::mono (10.0f));
            const float tw = (float) juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), txt) + 6.0f;
            auto r = juce::Rectangle<float> (x + 22.0f, mig + 2.0f, tw, 12.0f);
            g.setColour (tema::guitarra.withAlpha (0.85f));
            g.fillRoundedRectangle (r, 2.0f);
            g.setColour (juce::Colours::black);
            g.drawText (txt, r, juce::Justification::centred);
        }

    const float xc = estat.x (estat.capcal);
    juce::Path tri;
    tri.addTriangle (xc - 6.0f, h - 10.0f, xc + 6.0f, h - 10.0f, xc, h);
    g.setColour (tema::text);
    g.fillPath (tri);
    g.fillRect (xc - 0.5f, mig, 1.0f, h - mig - 9.0f);
}

void Regla::mouseMove (const juce::MouseEvent& e)
{
    const bool dalt = e.y < getHeight() / 2;
    const auto q = queHiHa (e.getPosition());
    setMouseCursor (q.vora != 0 ? juce::MouseCursor::LeftRightResizeCursor
                    : ! dalt && std::abs ((float) e.x - estat.x (estat.capcal)) <= 7.0f ? juce::MouseCursor::DraggingHandCursor
                                                                                       : juce::MouseCursor::NormalCursor);
}

/** Què hi ha a la meitat de dalt en aquest punt: una vora del bucle o d'una part (per allargar) o una part. */
Regla::Lloc Regla::queHiHa (juce::Point<int> pt) const
{
    Lloc l;
    if (pt.y >= getHeight() / 2) return l;
    const auto& p = estat.projecte;
    const float x = (float) pt.x;
    auto prop = [x] (float v) { return std::abs (x - v) <= 5.0f; };
    if (p.bucle.marcat)
    {
        if (prop (estat.x (ed::iniciBucle (p)))) { l.vora = -1; l.part = -1; return l; }
        if (prop (estat.x (ed::fiBucle (p)))) { l.vora = 1; l.part = -1; return l; }
    }
    for (int i = (int) p.parts.size() - 1; i >= 0; --i)
    {
        const auto& q = p.parts[(size_t) i];
        if (prop (estat.x (ed::segonsDeCompas (p, q.de)))) { l.vora = -1; l.part = i; return l; }
        if (prop (estat.x (ed::segonsDeCompas (p, q.a)))) { l.vora = 1; l.part = i; return l; }
    }
    l.part = ed::partA (p, estat.t (x));
    return l;
}

void Regla::mouseDown (const juce::MouseEvent& e)
{
    inici = e.getPosition();
    accio = Accio::Res;
    const bool dalt = e.y < getHeight() / 2;
    const auto q = queHiHa (e.getPosition());

    if (e.mods.isPopupMenu()) { menu (e, q); return; }

    if (! dalt)
    {
        // Meitat de baix: el capçal. Arrossegant, lliure; un clic, al temps més proper.
        accio = Accio::Capcal;
        return;
    }
    comencaCanvi();
    if (q.vora != 0)
    {
        accio = Accio::Vora;
        llocVora = q;
        return;
    }
    accio = Accio::Seleccio;
    partClicada = q.part;
}

void Regla::mouseDrag (const juce::MouseEvent& e)
{
    const double t = estat.t ((float) e.x);
    switch (accio)
    {
        case Accio::Capcal:
            if (e.getDistanceFromDragStart() < 3) return;
            estat.mouCapcal (juce::jmax (0.0, t), true);
            estat.avisa();
            return;

        case Accio::Vora:
        {
            restaura();
            auto& p = estat.projecte;
            const double v = vora (t);
            double* de = llocVora.part < 0 ? &p.bucle.de : &p.parts[(size_t) llocVora.part].de;
            double* a = llocVora.part < 0 ? &p.bucle.a : &p.parts[(size_t) llocVora.part].a;
            const double minim = p.snap == Snap::Lliure ? 0.05 : p.snap == Snap::Temps ? 1.0 / juce::jmax (1, p.compasNum) : 1.0;
            if (llocVora.vora < 0) *de = juce::jlimit (0.0, *a - minim, v);
            else *a = juce::jmax (*de + minim, v);
            estat.info = llocVora.part < 0 ? "Bucle: "_ca + textRang (*de, *a)
                                           : dibuix::utf8 (p.parts[(size_t) llocVora.part].nom) + ": " + textRang (*de, *a);
            estat.avisa();
            return;
        }

        case Accio::Seleccio:
        {
            if (std::abs (e.getDistanceFromDragStartX()) < 5) return;
            restaura();
            const auto [a, b] = seleccio (estat.t ((float) inici.x), t);
            auto& bu = estat.projecte.bucle;
            bu.marcat = bu.actiu = true;
            bu.de = a;
            bu.a = b;
            estat.info = "Bucle: "_ca + textRang (a, b) + " (botó dret: fes-ne una part de la cançó)"_ca;
            canviFet = true;
            accio = Accio::Seleccionant;
            estat.avisa();
            return;
        }

        case Accio::Seleccionant:
        {
            restaura();
            const auto [a, b] = seleccio (estat.t ((float) inici.x), t);
            auto& bu = estat.projecte.bucle;
            bu.marcat = bu.actiu = true;
            bu.de = a;
            bu.a = b;
            estat.info = "Bucle: "_ca + textRang (a, b) + " (botó dret: fes-ne una part de la cançó)"_ca;
            estat.avisa();
            return;
        }

        case Accio::Res: return;
    }
}

void Regla::mouseUp (const juce::MouseEvent& e)
{
    const auto acc = accio;
    accio = Accio::Res;
    if (acc == Accio::Capcal)
    {
        if (e.getDistanceFromDragStart() < 3)
            estat.mouCapcal (ed::enganxa (estat.projecte, estat.t ((float) e.x), Snap::Temps), true);
        estat.avisa();
        return;
    }
    if (acc == Accio::Seleccio && partClicada >= 0 && partClicada < (int) estat.projecte.parts.size())
    {
        // Un clic a una part: en fa el bucle.
        restaura();
        const auto& x = estat.projecte.parts[(size_t) partClicada];
        estat.projecte.bucle = { true, true, x.de, x.a };
        estat.info = "Bucle: "_ca + dibuix::utf8 (x.nom);
        canviFet = true;
    }
    if (acc == Accio::Vora || acc == Accio::Seleccionant) canviFet = true;
    if (acc != Accio::Res) acabaCanvi();
}

void Regla::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto& p = estat.projecte;
    const double t = estat.t ((float) e.x);
    if (e.y < getHeight() / 2 && p.bucle.marcat && t >= ed::iniciBucle (p) && t < ed::fiBucle (p))
    {
        comencaCanvi();
        restaura();
        estat.projecte.bucle.actiu = ! estat.projecte.bucle.actiu;
        acabaCanvi();
    }
}

juce::String Regla::textRang (double de, double a) const
{
    auto txt = [this] (double c)
    {
        const int sencer = (int) std::floor (c + 1e-9);
        const double resta = c - sencer;
        const int temps = (int) std::floor (resta * estat.projecte.compasNum + 1e-6) + 1;
        return juce::String (ed::numCompas (sencer)) + "." + juce::String (temps);
    };
    return txt (de) + " - " + txt (a);
}

void Regla::menu (const juce::MouseEvent& e, Lloc q)
{
    auto& p = estat.projecte;
    const double t = estat.t ((float) e.x);
    const bool dinsBucle = p.bucle.marcat && t >= ed::iniciBucle (p) && t < ed::fiBucle (p);
    const int part = q.part;
    juce::PopupMenu m;
    if (e.y < getHeight() / 2 && part >= 0)
    {
        m.addSectionHeader (dibuix::utf8 (p.parts[(size_t) part].nom));
        m.addItem (10, "Fes-ne el bucle");
        m.addItem (11, "Canvia el nom i el color…"_ca);
        juce::PopupMenu colors;
        for (int i = 0; i < 8; ++i) colors.addColouredItem (20 + i, "Color " + juce::String (i + 1), coloresParts[i]);
        m.addSubMenu ("Color", colors);
        m.addItem (12, "Esborra la part");
        m.addSeparator();
        tempoPart::afegeix (m, p, p.parts[(size_t) part]);   // 0.28
        m.addSeparator();
    }
    if (dinsBucle) m.addItem (4, "Fes-ne una part de la cançó (nom i color)…"_ca);
    m.addItem (1, p.bucle.actiu ? "Apaga el bucle" : "Encén el bucle"_ca, p.bucle.marcat);
    m.addItem (2, "Treu el bucle", p.bucle.marcat);
    m.addSeparator();
    m.addItem (3, "Capçal a l'inici"_ca);
    const std::string idPart = part >= 0 ? p.parts[(size_t) part].id : std::string();
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this, idPart] (int r)
    {
        auto& pp = estat.projecte;
        if (tempoPart::tracta (r, *this, estat, idPart)) return;   // 0.28
        if (r == 3) { estat.mouCapcal (0.0, true); estat.avisa(); return; }
        // El diàleg s'obre un cop el menú s'ha tancat del tot (si no, el menú es queda el teclat).
        if (r == 4 || r == 11)
        {
            juce::Component::SafePointer<Regla> jo (this);
            const auto id = r == 4 ? std::string() : idPart;
            juce::MessageManager::callAsync ([jo, id] { if (jo != nullptr) jo->demanaNom (id); });
            return;
        }
        if (r == 0) return;
        comencaCanvi();
        restaura();
        auto* x = ed::part (pp, idPart);
        if (r == 1) pp.bucle.actiu = ! pp.bucle.actiu;
        else if (r == 2) pp.bucle = {};
        else if (r == 10 && x != nullptr) pp.bucle = { true, true, x->de, x->a };
        else if (r == 12) ed::esborraPart (pp, idPart);
        else if (r >= 20 && r < 28 && x != nullptr) x->rgb = coloresParts[r - 20].getARGB() & 0xffffffu;
        acabaCanvi();
    });
}

void Regla::demanaNom (const std::string& idPart)
{
    auto* x = ed::part (estat.projecte, idPart);
    static const char* suggeriments[] = { "Intro", "Estrofa", "Tornada", "Pont", "Final" };
    const auto inicial = x != nullptr ? dibuix::utf8 (x->nom)
                                      : juce::String (suggeriments[juce::jmin (4, (int) estat.projecte.parts.size())]);
    auto* pare = getParentComponent();
    if (pare == nullptr) return;
    dialeg = std::make_unique<DialegText> (x != nullptr ? "Canvia el nom de la part"_ca : "Part de la cançó"_ca,
        x != nullptr ? juce::String() : "Intro, estrofa, tornada… Es veurà a la regla i es desa amb el projecte."_ca,
        x != nullptr ? "Canvia"_ca : "Crea"_ca,
        [] (const juce::String& s) { return s.trim().isEmpty() ? "Escriu un nom"_ca : juce::String(); },
        [this, idPart] (const juce::String& s)
        {
            comencaCanvi();
            restaura();
            auto& p = estat.projecte;
            const auto rgb = coloresParts[juce::jlimit (0, 7, dialeg != nullptr ? dialeg->colorTriat : 0)].getARGB() & 0xffffffu;
            if (auto* y = ed::part (p, idPart)) { y->nom = s.trim().toStdString(); y->rgb = rgb; }
            else ed::creaPart (p, s.trim().toStdString(), p.bucle.de, p.bucle.a, rgb);
            canviFet = true;
            acabaCanvi();
        }, inicial);
    // 0.16: el color es tria a la mateixa finestra.
    int colorInicial = (int) estat.projecte.parts.size() % 8;
    if (x != nullptr)
        for (int i = 0; i < 8; ++i)
            if ((coloresParts[i].getARGB() & 0xffffffu) == x->rgb) colorInicial = i;
    dialeg->ambColors (std::vector<juce::Colour> (std::begin (coloresParts), std::end (coloresParts)), colorInicial);
    DialegText::mostra (*pare, dialeg);
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

    if (estat.filaCarpeta (fila)) return l;   // fila d'una carpeta plegada: no s'hi edita (doble clic la desplega)

    l.pista = fila;
    const auto& t = estat.projecte.pistes[(size_t) fila];
    const auto pf = p.toFloat();

    if (estat.eina == Eina::Fletxa)
    {
        // Manetes del clip triat (només si n'hi ha un).
        if (estat.triats.size() == 1 && estat.triats[0].pista == fila)
        {
            const int ci = estat.triats[0].clip;
            if (ci >= 0 && ci < (int) t.clips.size())
            {
                const auto& c = t.clips[(size_t) ci];
                const auto r = rectClip (fila, c);
                const float xfi = estat.x (c.iniciVisible() + c.fadeIn), xfo = estat.x (c.fiVisible() - c.fadeOut);
                const float yg = yGuany (c, r);
                auto prova = [&] (juce::Rectangle<float> z, Maneta m) { if (z.expanded (4.0f).contains (pf)) { l.clip = ci; l.maneta = m; return true; } return false; };

                if (c.midi)
                {
                    // Clips MIDI: només retallar (ni fades ni volum: la dinàmica és la velocitat de les notes).
                    if (prova ({ r.getX(), r.getBottom() - mida * 2, mida * 1.4f, mida * 2 }, Maneta::RetallInici)) return l;
                    if (prova ({ r.getRight() - mida * 1.4f, r.getBottom() - mida * 2, mida * 1.4f, mida * 2 }, Maneta::RetallFi)) return l;
                }
                else
                {
                if (prova ({ (r.getX() + r.getRight()) / 2.0f - mida, yg - mida, mida * 2, mida * 2 }, Maneta::Guany)) return l;
                if (prova ({ r.getX(), r.getBottom() - mida * 2, mida * 1.4f, mida * 2 }, Maneta::RetallInici)) return l;
                if (prova ({ r.getRight() - mida * 1.4f, r.getBottom() - mida * 2, mida * 1.4f, mida * 2 }, Maneta::RetallFi)) return l;
                if (prova ({ xfi - mida * 0.7f, r.getY(), mida * 1.4f, mida * 1.4f }, Maneta::FadeIn)) return l;
                if (prova ({ xfo - mida * 0.7f, r.getY(), mida * 1.4f, mida * 1.4f }, Maneta::FadeOut)) return l;
                }
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
    const float h = (float) getHeight();
    // 0.28: compàs a compàs, amb el mapa de tempo
    const int primer = juce::jmax (0, (int) std::floor (ed::compasDeSegons (p, juce::jmax (0.0, estat.t (0.0f)))));
    const int darrer = (int) std::ceil (ed::compasDeSegons (p, juce::jmax (0.0, estat.t ((float) getWidth()))));
    if (darrer - primer > 5000) return;
    for (int c = primer; c <= darrer; ++c)
    {
        const double sc = ed::segonsDeCompas (p, (double) c);
        const auto tr = ed::tramA (p, sc + 1e-6);
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.fillRect (juce::Rectangle<float> (estat.x (sc), 0.0f, 1.0f, h));
        if (tr.segonsPerTemps() * estat.pxPerSegon < 9.0) continue;
        g.setColour (juce::Colours::white.withAlpha (0.075f));
        for (int k = 1; k < tr.num; ++k) g.fillRect (juce::Rectangle<float> (estat.x (sc + k * tr.segonsPerTemps()), 0.0f, 1.0f, h));
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
    const bool manetes = triat && estat.triats.size() == 1 && estat.eina == Eina::Fletxa;
    const bool actiu = sona (p, t) && ! c.mut;
    const auto color = actiu ? estat.colorClip (pista, c) : tema::suau;
    const float rad = 6.0f;
    const auto* s = ed::presa (p, c.presa);

    g.setColour (tema::fons);
    g.fillRoundedRectangle (r, rad);
    g.setColour (color.withAlpha (triat ? 0.26f : 0.15f));
    g.fillRoundedRectangle (r, rad);

    {
        juce::Graphics::ScopedSaveState estatG (g);
        g.reduceClipRegion (r.toNearestInt());

        if (c.midi)
        {
            // Clip MIDI: les notes en petit (només les que cauen dins la finestra sonen; la resta, apagades).
            int toMin = 127, toMax = 0;
            for (auto& n : c.notes) { toMin = juce::jmin (toMin, n.to); toMax = juce::jmax (toMax, n.to); }
            if (! c.notes.empty())
            {
                const int rang = juce::jmax (12, toMax - toMin + 1);
                const int baix = juce::jmax (0, (toMin + toMax) / 2 - rang / 2);
                const float zonaY = r.getY() + 18.0f, zonaH = juce::jmax (8.0f, r.getHeight() - 22.0f);
                const float hNota = juce::jlimit (1.5f, 5.0f, zonaH / (float) rang);
                const double tick0 = midi::ticks (p, c.pos);   // 0.28: amb el mapa de tempo
                const bool mapa = ed::teCanvisTempo (p);
                const double spt = midi::segonsPerTick (p);
                for (auto& n : c.notes)
                {
                    const double a = mapa ? midi::segons (p, tick0 + (double) n.ini) : c.pos + spt * (double) n.ini;
                    const double b = mapa ? midi::segons (p, tick0 + (double) (n.ini + n.dur)) : c.pos + spt * (double) (n.ini + n.dur);
                    const bool dins = a >= c.iniciVisible() - 1e-9 && a < c.fiVisible();
                    const float xa = estat.x (a), xb = juce::jmax (xa + 1.5f, estat.x (juce::jmin (b, dins ? c.fiVisible() : b)));
                    if (xb < 0.0f || xa > (float) getWidth()) continue;
                    const float y = zonaY + zonaH - (float) (n.to - baix + 1) * zonaH / (float) rang;
                    g.setColour (dins ? color.withAlpha (0.55f + 0.45f * (float) n.vel / 127.0f) : color.withAlpha (0.18f));
                    g.fillRect (juce::Rectangle<float> (xa, y, xb - xa, hNota));
                }
            }
        }
        else if (s == nullptr)
        {
            // Finestra buida: ratlles en diagonal.
            g.setColour (color.withAlpha (0.12f));
            for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += 10.0f)
                g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), 1.0f);
        }
        else if (auto it = estat.ones.find (c.presa); it != estat.ones.end() && it->second != nullptr)
            pintaOna (g, c, *s, *it->second, r, color);

        const float yg = yGuany (c, r);
        if (c.fadeIn > 0.0 && ! c.midi)
        {
            const float xf = estat.x (c.iniciVisible() + c.fadeIn);
            juce::Path zona;
            zona.addTriangle (r.getX(), r.getBottom(), r.getX(), r.getY(), xf, r.getY());
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillPath (zona);
            g.setColour (tema::text);
            g.drawLine (r.getX(), r.getBottom(), xf, r.getY(), 1.5f);
        }
        if (c.fadeOut > 0.0 && ! c.midi)
        {
            const float xf = estat.x (c.fiVisible() - c.fadeOut);
            juce::Path zona;
            zona.addTriangle (r.getRight(), r.getBottom(), r.getRight(), r.getY(), xf, r.getY());
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillPath (zona);
            g.setColour (tema::text);
            g.drawLine (xf, r.getY(), r.getRight(), r.getBottom(), 1.5f);
        }

        if ((c.guanyDb < -0.05 || manetes) && ! c.midi)
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
            juce::String nom = c.midi && t.tipus == "bateria" ? "Bateria · "_ca + juce::String ((int) c.notes.size()) + (c.notes.size() == 1 ? " cop" : " cops")
                             : c.midi ? "MIDI · "_ca + juce::String ((int) c.notes.size()) + (c.notes.size() == 1 ? " nota" : " notes")
                             : (s != nullptr ? dibuix::utf8 (s->nom) : juce::String ("Finestra buida"));
            if (c.midi && estat.pianoRoll && estat.editorDe ({ pista, indexClip }) >= 0) nom += "  (Piano Roll)";
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
        if (c.midi) return;
        maneta ({ xfi - mida * 0.7f, r.getY(), mida * 1.4f, mida * 1.4f }, m == Maneta::FadeIn);
        maneta ({ xfo - mida * 0.7f, r.getY(), mida * 1.4f, mida * 1.4f }, m == Maneta::FadeOut);
        const float xm = (r.getX() + r.getRight()) / 2.0f;
        g.setColour (m == Maneta::Guany ? tema::accent : tema::text);
        g.fillEllipse (xm - mida * 0.85f, yg - mida * 0.85f, mida * 1.7f, mida * 1.7f);
        g.setColour (tema::fons);
        g.fillEllipse (xm - mida * 0.35f, yg - mida * 0.35f, mida * 0.7f, mida * 0.7f);
    }
}

void VistaPistes::pintaFilaCarpeta (juce::Graphics& g, int fila)
{
    // Carpeta plegada (0.11): una sola fila amb els clips de totes les seves pistes, en franges fines.
    const auto& p = estat.projecte;
    const auto idCarpeta = p.pistes[(size_t) fila].carpeta;
    const auto membres = ed::pistesDeCarpeta (p, idCarpeta);
    if (membres.empty()) return;
    const float y0 = (float) estat.yPista (fila) + 5.0f, alt = (float) estat.altPista - 10.0f;
    const float franja = alt / (float) membres.size();
    for (int m = 0; m < (int) membres.size(); ++m)
    {
        const auto& t = p.pistes[(size_t) membres[(size_t) m]];
        const auto col = EstatEstudi::colorPista (t);
        for (auto& c : t.clips)
        {
            auto r = juce::Rectangle<float> (estat.x (c.iniciVisible()), y0 + franja * (float) m,
                                             juce::jmax (2.0f, estat.x (c.fiVisible()) - estat.x (c.iniciVisible())), juce::jmax (2.0f, franja - 1.0f));
            if (r.getRight() < 0.0f || r.getX() > (float) getWidth()) continue;
            g.setColour (col.withAlpha (c.mut || t.mut ? 0.18f : 0.55f));
            g.fillRoundedRectangle (r, 2.0f);
        }
    }
    g.setColour (tema::suau);
    g.setFont (dibuix::lletra (11.0f));
    if (auto* c = ed::carpeta (const_cast<Projecte&> (p), idCarpeta))
        g.drawText (dibuix::utf8 (c->nom) + " · " + juce::String ((int) membres.size()) + " pistes (doble clic per desplegar)",
                    8, (int) y0, 400, 14, juce::Justification::centredLeft, true);
}

void VistaPistes::paint (juce::Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();
    g.fillAll (tema::fons);

    // 0.17: fila del màster (no té clips).
    {
        // 0.27: diferenciat de les pistes: metall fosc amb ratlles daurades i el text «sortida general»
        const int y = estat.yMaster();
        const auto r = juce::Rectangle<int> (0, y, getWidth(), estat.altPista);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff24221c), 0.0f, (float) y, juce::Colour (0xff16150f), 0.0f, (float) r.getBottom(), false));
        g.fillRect (r);
        if (estat.masterTriat()) { g.setColour (tema::triat.withAlpha (0.5f)); g.fillRect (r); }
        g.setColour (tema::guitarra.withAlpha (0.05f));
        for (int x = -estat.altPista; x < getWidth(); x += 14)
            g.drawLine ((float) x, (float) r.getBottom(), (float) (x + estat.altPista), (float) y, 5.0f);
        g.setColour (tema::guitarra.withAlpha (0.55f));
        g.fillRect (0, y, getWidth(), 1);
        g.fillRect (0, y + estat.altPista - 2, getWidth(), 2);
        g.setColour (tema::guitarra.withAlpha (0.35f));
        g.setFont (dibuix::lletra (11.0f, true));
        g.drawText (dibuix::majuscules ("Sortida general"_ca), r.withTrimmedLeft (12), juce::Justification::centredLeft, false);
    }

    for (int i = 0; i < estat.numPistes(); ++i)
    {
        const int y = estat.yPista (i);
        if (estat.altDe (i) == 0 || y > getHeight() || y + estat.altPista < 0) continue;
        g.setColour (estat.pistaEsTriada (i) ? tema::triat : (i % 2 == 0 ? tema::fons : juce::Colour (0xff19191e)));
        if (estat.filaCarpeta (i)) g.setColour (juce::Colour (0xff1d1d23));
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
        if (estat.altDe (i) == 0) continue;
        if (estat.filaCarpeta (i)) { pintaFilaCarpeta (g, i); continue; }
        // En ordre: el de més amunt de la llista és el que sona i es dibuixa a sobre.
        for (int k = 0; k < (int) t.clips.size(); ++k) pintaClip (g, i, k, t.clips[(size_t) k]);

        // On dos clips es trepitgen: ratllat.
        for (int a = 0; a < (int) t.clips.size(); ++a)
            for (int b = a + 1; b < (int) t.clips.size(); ++b)
            {
                const double ta = juce::jmax (t.clips[(size_t) a].iniciVisible(), t.clips[(size_t) b].iniciVisible());
                const double tb = juce::jmin (t.clips[(size_t) a].fiVisible(), t.clips[(size_t) b].fiVisible());
                if (tb - ta <= 1.0e-6) continue;
                auto r = juce::Rectangle<float> (estat.x (ta), (float) estat.yPista (i) + 5.0f, estat.x (tb) - estat.x (ta), (float) estat.altPista - 10.0f);
                if (r.getRight() < 0.0f || r.getX() > w) continue;
                juce::Graphics::ScopedSaveState guarda (g);
                g.reduceClipRegion (r.toNearestInt());
                g.setColour (juce::Colours::black.withAlpha (0.18f));
                g.fillRect (r);
                g.setColour (juce::Colours::white.withAlpha (0.28f));
                for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += 7.0f)
                    g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), 1.2f);
                g.setColour (juce::Colours::white.withAlpha (0.6f));
                g.fillRect (r.getX(), r.getY(), 1.0f, r.getHeight());
                g.fillRect (r.getRight() - 1.0f, r.getY(), 1.0f, r.getHeight());
            }
    }

    // Tros triat (requadre en mode «només el que queda dins»).
    if (estat.rang.actiu)
    {
        const auto& rg = estat.rang;
        auto r = juce::Rectangle<float> (estat.x (rg.t0), (float) estat.yPista (rg.pistaDe), estat.x (rg.t1) - estat.x (rg.t0),
                                         (float) (estat.yPista (rg.pistaFins + 1) - estat.yPista (rg.pistaDe)));
        g.setColour (tema::accent.withAlpha (0.16f));
        g.fillRect (r);
        g.setColour (tema::accent);
        g.drawRect (r, 1.5f);
    }

    // Requadre mentre s'arrossega.
    if (mode == Mode::Requadre && puntRequadre != inici)
    {
        const auto r = juce::Rectangle<int> (inici, puntRequadre).toFloat();
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRect (r);
        juce::Path p;
        p.addRectangle (r);
        juce::Path discontinu;
        const float guions[] = { 4.0f, 3.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (discontinu, p, guions, 2);
        g.setColour (tema::text.withAlpha (0.8f));
        g.fillPath (discontinu);
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

    // Gravant: la zona que s'està gravant, a la pista amb el botó encès.
    if (estat.transport == 3 && estat.capcal > estat.cursorGravacio)
    {
        for (int i = 0; i < estat.numPistes(); ++i)
        {
            if (! estat.projecte.pistes[(size_t) i].grava || estat.altDe (i) == 0) continue;
            auto r = juce::Rectangle<float> (estat.x (estat.cursorGravacio), (float) estat.yPista (i) + 5.0f,
                                             estat.x (estat.capcal) - estat.x (estat.cursorGravacio), (float) estat.altPista - 10.0f);
            g.setColour (vintage::ledVermell.withAlpha (0.18f));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (vintage::ledVermell.withAlpha (0.8f));
            g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.5f);
            g.setFont (dibuix::lletra (12.0f, true));
            g.drawText ("Gravant", r.reduced (10.0f, 4.0f), juce::Justification::topLeft, false);
        }
    }

    pintaCapcal (g, estat.x (estat.capcal), h, estat.transport);
}

void VistaPistes::mouseMove (const juce::MouseEvent& e)
{
    if (estat.triantFantasma >= 0)
    {
        static const juce::MouseCursor cursorFantasma = []
        {
            juce::Image im (juce::Image::ARGB, 28, 28, true);
            juce::Graphics g (im);
            dibuix::pintaIcona (g, dibuix::Icona::Fantasma, { 2.0f, 2.0f, 24.0f, 24.0f }, vintage::ledBlau, 2.0f, juce::Colours::black.withAlpha (0.5f));
            return juce::MouseCursor (im, 14, 14);
        }();
        setMouseCursor (cursorFantasma);
        return;
    }
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
        case Maneta::Cap:       setMouseCursor (l.clip >= 0 && estat.clipTriat (l.pista, l.clip) ? juce::MouseCursor::DraggingHandCursor
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
    m.addItem (10, "Instrument MIDI (sinte intern o VST3)"_ca);
    m.addItem (11, "Bateria (Drumpad)"_ca);
    m.addItem (14, "Samplejador"_ca);
    m.addItem (12, "Acords  · més endavant"_ca, false);
    m.addItem (13, "Bus d'efectes  · més endavant"_ca, false);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&on).withMousePosition(), [tria] (int r)
    {
        if (r == 1) tria ("audio", 1);
        else if (r == 2) tria ("audio", 2);
        else if (r == 10) tria ("midi", 2);
        else if (r == 11) tria ("bateria", 2);
        else if (r == 14) tria ("sampler", 2);
    });
}

void VistaPistes::menuClip (int pista, int clip, double t)
{
    const auto& clip0 = estat.projecte.pistes[(size_t) pista].clips[(size_t) clip];
    const auto* s = ed::presa (estat.projecte, clip0.presa);
    const bool unic = estat.triats.size() == 1;
    const bool molts = estat.triats.size() > 1;

    juce::PopupMenu m;
    if (clip0.midi)
    {
        m.addItem (20, "Obre al Piano Roll   (doble clic)"_ca);
        m.addSeparator();
    }
    else if (s != nullptr)
    {
        m.addItem (21, "Obre al Vocal Editor"_ca + (mibase::teVeu (estat.projecte.pistes[(size_t) pista]) ? "   (doble clic)"_ca : juce::String()));
        // 0.28: el tros (el clip o, si n'hi ha, el que hi ha dins el requadre) com un sample nou
        const bool ambRang = estat.rang.actiu && pista >= estat.rang.pistaDe && pista <= estat.rang.pistaFins;
        m.addItem (22, ambRang ? "Envia el tros triat al samplejador (pista nova)"_ca : "Envia al samplejador (pista nova)"_ca);
        m.addItem (23, ambRang ? "Envia el tros triat a la biblioteca"_ca : "Envia a la biblioteca"_ca);
        m.addSeparator();
    }
    m.addItem (8, "Copia   (Ctrl+C)");
    m.addItem (1, molts ? "Duplica els clips   (Ctrl+D)" : "Duplica   (Ctrl+D)");
    m.addItem (2, molts ? "Treu els clips   (Supr)" : "Treu   (Supr)");
    m.addItem (6, clip0.mut ? "Torna a fer sonar   (M)" : "Silencia   (M)");
    m.addItem (7, "Talla aquí   (tisores)"_ca, unic && t > clip0.iniciVisible() && t < clip0.fiVisible());
    m.addSeparator();
    if (! clip0.midi)
    {
        m.addItem (3, "Posa la bandera del primer temps aquí"_ca, unic && s != nullptr);
        m.addItem (4, "Treu la bandera", unic && s != nullptr && s->primer >= 0.0);
        m.addSeparator();
        m.addItem (5, "Treu els fades i el volum del clip", unic);
    }
    m.addItem (9, "Treu el color propi", clip0.rgb != 0);

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this, pista, clip, t] (int r)
    {
        if (r == 0 || pista >= estat.numPistes() || clip >= (int) estat.projecte.pistes[(size_t) pista].clips.size()) return;
        if (r == 8) { estat.portaretalls = ed::copiaClips (estat.projecte, estat.triats); estat.info = "Copiat"; estat.avisa(); return; }
        if (r == 20) { if (onObreMidi) onObreMidi (pista, clip); return; }
        if (r == 21) { if (onObreVeu) onObreVeu (pista, clip); return; }
        if (r == 22 || r == 23) { if (onEnviaSampler) onEnviaSampler (pista, clip, r == 23); return; }
        comencaCanvi();
        restaura();
        auto& c = estat.projecte.pistes[(size_t) pista].clips[(size_t) clip];
        switch (r)
        {
            case 1: estat.triats = ed::duplicaTriats (estat.projecte, estat.triats); break;
            case 2: ed::treuTriats (estat.projecte, estat.triats); estat.capTriat(); break;
            case 3: ed::posaBandera (estat.projecte, c, t); break;
            case 4: ed::posaBandera (estat.projecte, c, -1.0); break;
            case 5: c.fadeIn = c.fadeOut = 0.0; c.guanyDb = 0.0; break;
            case 6:
            {
                const bool nou = ! c.mut;
                for (auto& ref : estat.triats) if (auto* k = estat.clip (ref)) k->mut = nou;
                break;
            }
            case 7: ed::talla (estat.projecte, pista, clip, tEina (t)); estat.capTriat(); break;
            case 9: for (auto& ref : estat.triats) if (auto* k = estat.clip (ref)) k->rgb = 0; break;
            default: break;
        }
        acabaCanvi();
    });
}

void VistaPistes::mouseDown (const juce::MouseEvent& e)
{
    if (estat.triantFantasma >= 0)
    {
        // 0.12: el clip clicat es veu (o es deixa de veure) en fantasma al Piano Roll.
        const auto l = llocA (e.getPosition());
        const int ei = estat.triantFantasma;
        estat.triantFantasma = -1;
        setMouseCursor (cursor (estat.eina));
        if (l.pista >= 0 && l.clip >= 0 && ei < (int) estat.editors.size())
        {
            auto& fs = estat.editors[(size_t) ei].fantasmes;
            const auto clau = estat.projecte.pistes[(size_t) l.pista].id + "#" + std::to_string (l.clip);
            if (auto it = std::find (fs.begin(), fs.end(), clau); it != fs.end()) { fs.erase (it); estat.info = "Fantasma tret"; }
            else { fs.push_back (clau); estat.info = "Clip en fantasma al Piano Roll"; }
        }
        estat.avisa();
        return;
    }
    grabKeyboardFocus();
    inici = e.getPosition();
    rang0 = estat.rang;   // 0.29: per si el clic és per arrossegar el tros triat al samplejador
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
                estat.triaClip (lloc0.pista, lloc0.clip);
                estat.avisa();
            }
            menuClip (lloc0.pista, lloc0.clip, t0);
        }
        else
        {
            // Botó dret en un lloc buit: torna a la fletxa.
            estat.eina = Eina::Fletxa;
            estat.info = "Fletxa";
            estat.avisa();
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
            estat.capTriat();
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
            estat.capTriat();
            acabaCanvi();
            return;

        case Eina::Cola:
            if (lloc0.clip < 0) return;
            estat.triaClip (lloc0.pista, lloc0.clip);
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
    // Lloc buit: es treu la selecció i, si s'arrossega, es fa un requadre (el capçal només es mou a la regla).
    if (lloc0.pista < 0 || (lloc0.clip < 0 && lloc0.maneta == Maneta::Cap))
    {
        if (! e.mods.isCommandDown() && ! e.mods.isShiftDown())
            estat.capTriat();
        else
            estat.rang.actiu = false;
        if (lloc0.pista >= 0) estat.pistaTriada = lloc0.pista;
        triatsOriginals = estat.triats;
        mode = Mode::Requadre;
        puntRequadre = inici;
        estat.avisa();
        return;
    }

    auto& t = estat.projecte.pistes[(size_t) lloc0.pista];
    estat.rang.actiu = false;

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
                if (! estat.clipTriat (lloc0.pista, lloc0.clip)) estat.triaClip (lloc0.pista, lloc0.clip);
                break;
            case Maneta::Cap: break;
        }
        return;
    }

    const ed::RefClip aquest { lloc0.pista, lloc0.clip };
    if (e.mods.isCommandDown())
    {
        auto& v = estat.triats;
        auto it = std::find (v.begin(), v.end(), aquest);
        if (it != v.end()) v.erase (it); else v.push_back (aquest);
        estat.pistaTriada = lloc0.pista;
    }
    else if (e.mods.isShiftDown() && ! estat.triats.empty() && estat.triats.back().pista == lloc0.pista)
    {
        // Maj: tots els de la pista entre l'últim triat i aquest.
        const double a = t.clips[(size_t) estat.triats.back().clip].iniciVisible();
        const double b = t.clips[(size_t) lloc0.clip].iniciVisible();
        for (int k = 0; k < (int) t.clips.size(); ++k)
        {
            const double x = t.clips[(size_t) k].iniciVisible();
            if (x >= juce::jmin (a, b) - 1e-9 && x <= juce::jmax (a, b) + 1e-9 && ! estat.clipTriat (lloc0.pista, k))
                estat.triats.push_back ({ lloc0.pista, k });
        }
    }
    else if (! estat.clipTriat (lloc0.pista, lloc0.clip))
    {
        // 0.12: un clip que no estava triat només es tria; si s'arrossega, és un requadre (per triar un tros a Temps o Lliure).
        // Per moure'l, primer s'ha de triar (llavors surt la mà).
        estat.triaClip (lloc0.pista, lloc0.clip);
        triatsOriginals = {};
        mode = Mode::Requadre;
        puntRequadre = inici;
        estat.avisa();
        return;
    }
    else
        estat.pistaTriada = lloc0.pista;

    if (! estat.clipTriat (lloc0.pista, lloc0.clip))
    {
        estat.avisa();
        return;
    }

    const auto& c = t.clips[(size_t) lloc0.clip];
    punt0 = ed::puntSnap (estat.projecte, c);
    bandera0 = ed::banderaVisible (estat.projecte, c) ? c.pos + ed::presa (estat.projecte, c.presa)->primer : -1.0;
    comencaCanvi();
    triatsOriginals = estat.triats;
    mode = Mode::Decidint;
    estat.avisa();
}

void VistaPistes::actualitzaRequadre (juce::Point<int> fins, bool afegeix)
{
    puntRequadre = fins;
    const auto r = juce::Rectangle<int> (inici, fins);
    const double ta = estat.t ((float) r.getX()), tb = estat.t ((float) r.getRight());
    const int pa = juce::jmax (0, estat.pistaA (r.getY())), pb = juce::jmin (estat.numPistes() - 1, estat.pistaA (r.getBottom()));

    estat.triats = afegeix ? triatsOriginals : std::vector<ed::RefClip>();
    estat.rang.actiu = false;
    if (pb < pa || tb <= ta) return;

    if (estat.modeRang)
    {
        // Només el que queda dins (enganxat a la graella, si n'hi ha).
        auto& g = estat.rang;
        g.actiu = true;
        g.t0 = juce::jmax (0.0, tEina (ta));
        g.t1 = tEina (tb);
        if (g.t1 <= g.t0) g.t1 = g.t0 + ed::segonsPerTemps (estat.projecte);
        g.pistaDe = pa;
        g.pistaFins = pb;
        estat.info = "Tros: "_ca + posicio (estat.projecte, g.t0) + " a " + posicio (estat.projecte, g.t1)
                   + " · " + juce::String (pb - pa + 1) + (pb == pa ? " pista" : " pistes");
        return;
    }

    for (int i = pa; i <= pb; ++i)
    {
        if (estat.altDe (i) == 0 || estat.filaCarpeta (i)) continue;
        const auto& clips = estat.projecte.pistes[(size_t) i].clips;
        for (int k = 0; k < (int) clips.size(); ++k)
            if (clips[(size_t) k].fiVisible() > ta && clips[(size_t) k].iniciVisible() < tb && ! estat.clipTriat (i, k))
                estat.triats.push_back ({ i, k });
    }
    estat.info = juce::String (estat.triats.size()) + (estat.triats.size() == 1 ? " clip triat" : " clips triats");
}

void VistaPistes::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    auto& p = estat.projecte;
    const double t = estat.t ((float) e.x);

    // 0.29: un clip d'àudio arrossegat fora de les pistes es pot deixar al samplejador
    const auto clipFora = [&]
    {
        if (! (mode == Mode::Decidint || mode == Mode::MouTemps || mode == Mode::MouPista || (mode == Mode::Requadre && lloc0.clip >= 0))) return false;
        if (! arrossegaFora || ! arrossegaFora (e.getScreenPosition())) return false;
        // movent, el projecte bo és la còpia d'abans de moure; fent un requadre, no s'ha mogut res
        const auto& pr = mode == Mode::Requadre ? estat.projecte : copiaInici;
        return lloc0.pista >= 0 && lloc0.pista < (int) pr.pistes.size() && lloc0.clip >= 0
               && lloc0.clip < (int) pr.pistes[(size_t) lloc0.pista].clips.size() && ! mibase::esMidi (pr.pistes[(size_t) lloc0.pista]);
    };
    if (clipFora())
        if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            if (mode == Mode::Requadre)
            {
                // començat a sobre d'un clip: el tros que ja estava triat (si el toca) o el clip sencer
                const bool toca = rang0.actiu && lloc0.pista >= rang0.pistaDe && lloc0.pista <= rang0.pistaFins && t0 >= rang0.t0 && t0 <= rang0.t1;
                estat.rang = rang0;
                if (! toca) estat.rang.actiu = false;
                estat.triats.clear();
                estat.triats.push_back ({ lloc0.pista, lloc0.clip });
            }
            else
            {
                restaura();
                estat.triats = triatsOriginals;
            }
            canviFet = false;
            triatsOriginals.clear();
            mode = Mode::Res;
            const int pi = lloc0.pista, ci = lloc0.clip;
            const auto& t2 = estat.projecte.pistes[(size_t) pi];
            const auto* s = ed::presa (estat.projecte, t2.clips[(size_t) ci].presa);
            const auto nom = s != nullptr ? dibuix::utf8 (s->nom) : juce::String ("Clip");
            juce::Image im (juce::Image::ARGB, 160, 28, true);
            {
                juce::Graphics g (im);
                g.setColour (EstatEstudi::colorPista (t2).withAlpha (0.85f));
                g.fillRoundedRectangle (0.0f, 0.0f, 160.0f, 28.0f, 6.0f);
                g.setColour (juce::Colours::black);
                g.setFont (dibuix::lletra (13.0f, true));
                g.drawText (nom, 6, 0, 148, 28, juce::Justification::centred, true);
            }
            estat.info = "Deixa «"_ca + nom + "» a sobre del samplejador"_ca;
            estat.avisa();
            repaint();
            c->startDragging ("mibase-clip:" + juce::String (pi) + ":" + juce::String (ci), this, juce::ScaledImage (im), true);
            return;
        }

    switch (mode)
    {
        case Mode::Res:
            return;

        case Mode::Requadre:
            if (e.getDistanceFromDragStart() < 4) return;
            actualitzaRequadre (e.getPosition(), e.mods.isCommandDown() || e.mods.isShiftDown());
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
                estat.capTriat();
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
            estat.triats = triatsOriginals;
            const double dt = t - t0;

            if (p.snap == Snap::Lliure && estat.triats.size() == 1 && bandera0 >= 0.0)
            {
                const auto ref = estat.triats[0];
                auto& c = *estat.clip (ref);
                const double posNova = copiaInici.pistes[(size_t) ref.pista].clips[(size_t) ref.clip].pos + dt;
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
                for (auto& ref : estat.triats)
                    minInici = juce::jmin (minInici, copiaInici.pistes[(size_t) ref.pista].clips[(size_t) ref.clip].iniciVisible());
                if (minInici + delta < 0.0) { delta = -minInici; desti = punt0 + delta; }
                for (auto& ref : estat.triats)
                    estat.clip (ref)->pos = copiaInici.pistes[(size_t) ref.pista].clips[(size_t) ref.clip].pos + delta;
                estat.info = (estat.triats.size() > 1 ? juce::String (estat.triats.size()) + " clips · "_ca : juce::String())
                           + "Compàs "_ca + posicio (p, desti, p.snap == Snap::Lliure);
            }
            estat.avisa();
            return;
        }

        case Mode::MouPista:
        {
            restaura();
            const int fila = juce::jlimit (0, estat.numPistes() - 1, estat.pistaA (e.y));
            const int d = fila - lloc0.pista;
            estat.triats = ed::mouDePista (p, triatsOriginals, d);
            if (estat.triats == triatsOriginals) canviFet = false;
            int nova = lloc0.pista;
            for (auto& ref : estat.triats) if (ref.pista >= 0) nova = ref.pista;
            estat.pistaTriada = d == 0 ? lloc0.pista : nova;
            estat.info = "Pista: "_ca + dibuix::utf8 (p.pistes[(size_t) juce::jlimit (0, estat.numPistes() - 1, estat.pistaTriada)].nom);
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
                case Mode::Res: case Mode::Requadre: case Mode::Decidint: case Mode::MouTemps: case Mode::MouPista:
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
                estat.triaClip (lloc0.pista, lloc0.clip);
            canviFet = false;
            estat.avisa();
            break;

        case Mode::Requadre:
            if (estat.triats.size() == 1) estat.pistaTriada = estat.triats[0].pista;
            if (! estat.rang.actiu && estat.triats.empty()) estat.info.clear();
            estat.avisa();
            break;

        case Mode::Llapis:
        {
            comencaCanvi();
            restaura();
            double a = juce::jmin (t0, tLlapis), b = juce::jmax (t0, tLlapis);
            if (b - a < 1e-6) b = a + ed::segonsPerCompas (estat.projecte);   // sense arrossegar: un compàs
            const int nou = ed::creaFinestra (estat.projecte, lloc0.pista, a, b);
            estat.triaClip (lloc0.pista, nou);
            acabaCanvi();
            if (auto* c = estat.clip ({ lloc0.pista, nou }); c != nullptr && c->midi && onObreMidi)
                onObreMidi (lloc0.pista, nou);   // clip MIDI nou: s'obre al Piano Roll per dibuixar-hi
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
                    estat.triaClip (lloc0.pista, r.clip);
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
    if (const int fila = estat.pistaA (e.y); fila >= 0 && fila < estat.numPistes() && estat.filaCarpeta (fila))
    {
        if (auto* c = ed::carpeta (estat.projecte, estat.projecte.pistes[(size_t) fila].carpeta)) c->plegada = false;
        estat.canviat();
        return;
    }
    const auto l = llocA (e.getPosition());
    if (l.pista >= 0 && l.clip >= 0 && estat.projecte.pistes[(size_t) l.pista].clips[(size_t) l.clip].midi)
    {
        if (onObreMidi) onObreMidi (l.pista, l.clip);
        return;
    }
    if (l.pista >= 0 && l.clip >= 0 && mibase::teVeu (estat.projecte.pistes[(size_t) l.pista])
        && ! estat.projecte.pistes[(size_t) l.pista].clips[(size_t) l.clip].presa.empty())
    {
        if (onObreVeu) onObreVeu (l.pista, l.clip);
        return;
    }
    // 0.31: doble clic amb la fletxa en un lloc buit d'una pista: una finestra (clip) d'un compàs
    if (l.pista >= 0 && l.clip < 0 && l.pista < estat.numPistes() && estat.eina == Eina::Fletxa && ! estat.filaCarpeta (l.pista))
    {
        comencaCanvi();
        restaura();
        const double a = juce::jmax (0.0, tEina (estat.t ((float) e.x)));
        const int nou = ed::creaFinestra (estat.projecte, l.pista, a, a + ed::segonsPerCompasA (estat.projecte, a));
        estat.pistaTriada = l.pista;
        estat.triaClip (l.pista, nou);
        acabaCanvi ("Finestra nova d'un compàs"_ca);
        mode = Mode::Res;
        return;
    }
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

    // 0.17: el màster, sempre a dalt.
    {
        const int y = estat.yMaster();
        auto r = juce::Rectangle<int> (0, y, getWidth(), estat.altPista);
        vintage::panell (g, r.toFloat());   // 0.27: metall, com un aparell (les pistes són llises)
        g.setColour (juce::Colour (0x40f5b445));
        g.fillRect (r);
        if (estat.masterTriat()) { g.setColour (tema::triat.withAlpha (0.55f)); g.fillRect (r); }
        g.setColour (tema::guitarra);
        g.fillRect (r.getX(), r.getY() + 4, 4, r.getHeight() - 8);
        g.setColour (tema::guitarra.withAlpha (0.6f));
        g.fillRect (r.getX(), r.getY(), r.getWidth(), 1);
        auto text = r.withTrimmedLeft (14).withTrimmedRight (22).withTrimmedTop (7);
        g.setColour (tema::guitarra);
        g.setFont (dibuix::lletra (14.0f, true));
        g.drawText (dibuix::majuscules ("Màster"_ca), text.removeFromTop (20), juce::Justification::centredLeft, true);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.0f));
        g.drawText ("L+R · "_ca + textSortidaMaster (p) + " · "_ca + EstatEstudi::textDb (p.masterDb), text.removeFromTop (20), juce::Justification::centredLeft, true);
        dibuixEstudi::vumetres (g, juce::Rectangle<float> ((float) getWidth() - 16.0f, (float) y + 8.0f, 10.0f, (float) estat.altPista - 16.0f),
                                estat.nivellsSortida[0], estat.nivellsSortida[1]);
        g.setColour (tema::guitarra.withAlpha (0.55f));
        g.fillRect (0, r.getBottom() - 2, getWidth(), 2);
    }

    for (int i = 0; i < estat.numPistes(); ++i)
    {
        const auto& t = p.pistes[(size_t) i];
        const int y = estat.yPista (i);
        if (estat.altDe (i) == 0 || y > getHeight() || y + estat.altPista < 0) continue;
        auto r = juce::Rectangle<int> (0, y, getWidth(), estat.altPista);

        if (estat.filaCarpeta (i))
        {
            // Carpeta plegada (0.11): nom de la carpeta i M / S per a totes les seves pistes.
            const auto membres = ed::pistesDeCarpeta (p, t.carpeta);
            bool totesTriades = true, totesMut = true, algunaSolo = false;
            for (int m : membres) { totesTriades &= estat.pistaEsTriada (m); totesMut &= p.pistes[(size_t) m].mut; algunaSolo |= p.pistes[(size_t) m].solo; }
            g.setColour (totesTriades ? tema::triat : juce::Colour (0xff1d1d23));
            g.fillRect (r);
            g.setColour (vintage::ledGroc.withAlpha (0.9f));
            g.strokePath (dibuix::icona (dibuix::Icona::Carpeta, juce::Rectangle<float> (12.0f, (float) y + 8.0f, 18.0f, 18.0f)), juce::PathStrokeType (1.4f));
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (14.0f, true));
            if (auto* c = ed::carpeta (const_cast<Projecte&> (p), t.carpeta))
                g.drawText (dibuix::utf8 (c->nom), juce::Rectangle<int> (36, y + 7, getWidth() - 60, 20), juce::Justification::centredLeft, true);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (11.0f));
            g.drawText (juce::String ((int) membres.size()) + " pistes", rectTecla (i, 1).withX (rectTecla (i, 2).getRight() + 8).withWidth (80),
                        juce::Justification::centredLeft, false);
            dibuixEstudi::teclaPetita (g, rectTecla (i, 0), "M", totesMut, vintage::ledBlau);
            dibuixEstudi::teclaPetita (g, rectTecla (i, 1), "S", algunaSolo, vintage::ledGroc);
            g.setColour (tema::linia.withAlpha (0.6f));
            g.fillRect (0, r.getBottom() - 1, getWidth(), 1);
            continue;
        }

        if (estat.pistaEsTriada (i))
        {
            g.setColour (tema::triat);
            g.fillRect (r);
        }
        g.setColour (sona (p, t) ? EstatEstudi::colorPista (t) : tema::suau);
        g.fillRect (r.getX(), r.getY() + 4, 4, r.getHeight() - 8);

        auto text = r.withTrimmedLeft (14).withTrimmedRight (22).withTrimmedTop (7);
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

        auto zonaCanals = juce::Rectangle<int> (rectTecla (i, 2).getRight() + 8, rectTecla (i, 2).getY(), getWidth() - rectTecla (i, 2).getRight() - 30, 24);
        if (mibase::esMidi (t) && zonaCanals.getWidth() > 50)
        {
            // 0.20: botonet per obrir l'instrument (o els kits, a la bateria).
            const auto rb = rectObreInstrument (i).toFloat();
            vintage::tecla (g, rb, false, false, EstatEstudi::colorPista (t));
            dibuix::pintaIcona (g, t.tipus == "bateria" ? dibuix::Icona::Bateria : dibuix::Icona::Finestra, rb.reduced (4.0f), tema::text);
            zonaCanals.removeFromLeft (rb.toNearestInt().getWidth() + 6);
        }
        if (zonaCanals.getWidth() > 30)
        {
            g.setColour (t.grava ? vintage::ledVermell.withAlpha (0.85f) : tema::suau);
            g.setFont (dibuix::lletra (11.0f));
            g.drawFittedText (textPistaCurt (t), zonaCanals,
                              juce::Justification::centredLeft, 1, 0.8f);
        }

        // Vúmetre de la pista (si té el botó de gravar encès, també el que entra).
        dibuixEstudi::vumetres (g, juce::Rectangle<float> ((float) getWidth() - 16.0f, (float) y + 8.0f, 10.0f, (float) estat.altPista - 16.0f),
                                estat.nivell (i, 0), estat.nivell (i, t.canals == 2 ? 1 : 0));

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
        const int y = desti > arrossegant ? estat.yPista (desti + 1) : estat.yPista (desti);
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

    juce::PopupMenu entrades;
    const auto& noms = estat.nomsEntrades;
    for (int e = 0; e + (t.canals == 2 ? 1 : 0) < noms.size(); ++e)
    {
        const bool activa = noms[e].isNotEmpty() && (t.canals == 1 || noms[e + 1].isNotEmpty());
        const auto text = t.canals == 2 ? juce::String (e + 1) + "-" + juce::String (e + 2) + "  (" + noms[e] + " + " + noms[e + 1] + ")"
                                        : juce::String (e + 1) + "  (" + noms[e] + ")";
        entrades.addItem (40 + e, activa ? text : juce::String (e + 1) + "  (apagada a les Preferències)"_ca, activa, t.entrada == e);
    }
    if (noms.isEmpty())
        entrades.addItem (39, "La targeta no té entrades (o està aturada)", false);

    juce::PopupMenu sons;
    if (t.tipus == "midi")
    {
        const int actual = juce::jmax (0, midi::soIntern (t.instrument.empty() ? "intern:piano" : t.instrument));
        for (int k = 0; k < (int) midi::sonsInterns().size(); ++k)
            sons.addItem (60 + k, juce::String::fromUTF8 (midi::sonsInterns()[(size_t) k].nom), true, actual == k && midi::soIntern (t.instrument.empty() ? "intern:piano" : t.instrument) >= 0);
        sons.addSeparator();
        sons.addSubMenu ("General MIDI (128 sons)"_ca, bancgm::menu (200, mibase::gm::programa (t.instrument)), true, juce::Image(), mibase::gm::programa (t.instrument) >= 0);   // 0.31
        sons.addItem (58, "Samplejador"_ca, true, mibase::esSampler (t));
        sons.addSeparator();
        sons.addItem (59, "Plugins VST3: a la columna esquerra, «Instrument»"_ca, false);
    }

    juce::PopupMenu m;
    m.addItem (1, "Canvia el nom…"_ca);
    m.addSubMenu ("Color", colors);
    if (t.tipus == "midi")
        m.addSubMenu ("Instrument"_ca, sons);
    else if (t.tipus == "bateria") {}
    else
    {
        m.addItem (3, t.canals == 2 ? "Fes-la mono" : "Fes-la estèreo"_ca);
        m.addSubMenu ("Entrada per gravar", entrades);
    }
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
            estat.capTriat();
        }
        else if (r == 3) tt.canals = tt.canals == 2 ? 1 : 2;
        else if (r == 58)
        {
            tt.instrument = mibase::instrumentSampler;
            evolucio = "Pista «"_ca + dibuix::utf8 (tt.nom) + "»: samplejador"_ca;
        }
        else if (r >= 200 && r < 328)
        {
            tt.instrument = mibase::gm::instrument (r - 200);
            tt.nomInstrument.clear(); tt.estatInstrument.clear();
            evolucio = "Pista «"_ca + dibuix::utf8 (tt.nom) + "»: General MIDI "_ca + juce::String::fromUTF8 (mibase::gm::nomPrograma (r - 200));
            juce::MessageManager::callAsync ([this] { bancgm::assegura (this, {}); });
        }
        else if (r >= 60)
        {
            tt.instrument = std::string ("intern:") + midi::sonsInterns()[(size_t) (r - 60)].id;
            evolucio = "Pista «"_ca + dibuix::utf8 (tt.nom) + "»: so "_ca + juce::String::fromUTF8 (midi::sonsInterns()[(size_t) (r - 60)].nom);
        }
        else if (r >= 40) tt.entrada = r - 40;
        else if (r >= 20) { tt.color = r - 20; tt.rgb = 0; }
        acabaCanvi (evolucio);
    });
}

juce::Rectangle<int> Capcaleres::rectObreInstrument (int fila) const
{
    const auto r = rectTecla (fila, 2);
    return { r.getRight() + 8, r.getY(), 26, r.getHeight() };
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
    if (fila == -1 && e.y >= estat.yMaster())
    {
        estat.pistaTriada = EstatEstudi::master;   // 0.17: tria el màster (la columna esquerra en mostra el fader i la sortida)
        estat.triats.clear();
        estat.avisa();
        return;
    }
    if (fila < 0 || fila >= estat.numPistes()) return;

    if (estat.filaCarpeta (fila))
    {
        // Carpeta plegada: M / S per a totes; un clic tria totes les seves pistes; doble clic la desplega.
        const auto membres = ed::pistesDeCarpeta (estat.projecte, estat.projecte.pistes[(size_t) fila].carpeta);
        const auto b = teclaA (fila, e.getPosition());
        if (b == Tecla::Mut || b == Tecla::Solo)
        {
            comencaCanvi();
            restaura();
            bool tots = true;
            for (int m : membres) tots &= b == Tecla::Mut ? estat.projecte.pistes[(size_t) m].mut : estat.projecte.pistes[(size_t) m].solo;
            for (int m : membres) (b == Tecla::Mut ? estat.projecte.pistes[(size_t) m].mut : estat.projecte.pistes[(size_t) m].solo) = ! tots;
            acabaCanvi();
            return;
        }
        if (! e.mods.isCommandDown() && ! e.mods.isShiftDown()) { estat.pistesTriades.clear(); estat.capTriat(); }
        for (int m : membres) estat.pistesTriades.push_back (estat.projecte.pistes[(size_t) m].id);
        estat.pistaTriada = fila;
        estat.avisa();
        return;
    }

    // Ctrl: afegeix o treu la pista de les triades; Maj: totes des de la triada fins a aquesta.
    if (e.mods.isCommandDown() && ! e.mods.isPopupMenu())
    {
        const auto& id = estat.projecte.pistes[(size_t) fila].id;
        auto& v = estat.pistesTriades;
        if (estat.pistaTriada >= 0 && estat.pistaTriada < estat.numPistes())
        {
            const auto& principal = estat.projecte.pistes[(size_t) estat.pistaTriada].id;
            if (std::find (v.begin(), v.end(), principal) == v.end()) v.push_back (principal);
        }
        if (auto it = std::find (v.begin(), v.end(), id); it != v.end() && fila != estat.pistaTriada) v.erase (it);
        else if (it == v.end()) v.push_back (id);
        estat.avisa();
        return;
    }
    if (e.mods.isShiftDown() && ! e.mods.isPopupMenu() && estat.pistaTriada >= 0)
    {
        for (int i = juce::jmin (fila, estat.pistaTriada); i <= juce::jmax (fila, estat.pistaTriada); ++i)
            if (estat.altDe (i) > 0) estat.pistesTriades.push_back (estat.projecte.pistes[(size_t) i].id);
        estat.avisa();
        return;
    }
    if (! e.mods.isPopupMenu()) estat.pistesTriades.clear();

    if (estat.pistaTriada != fila) estat.capTriat();
    estat.pistaTriada = fila;

    if (! e.mods.isPopupMenu() && mibase::esMidi (estat.projecte.pistes[(size_t) fila]) && rectObreInstrument (fila).contains (e.getPosition())
        && getWidth() - rectTecla (fila, 2).getRight() - 30 > 50)
    {
        estat.avisa();
        if (onObreInstrument) onObreInstrument (fila);
        return;
    }

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
    if (arrossegant < 0) return;
    // 0.10: una pista MIDI arrossegada fora dels noms es pot deixar al botó de notes fantasma d'un Piano Roll.
    if (! getLocalBounds().contains (e.getPosition()) && arrossegant < estat.numPistes()
        && mibase::esMidi (estat.projecte.pistes[(size_t) arrossegant]))
        if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            const auto& t = estat.projecte.pistes[(size_t) arrossegant];
            const auto nom = dibuix::utf8 (t.nom);
            juce::Image im (juce::Image::ARGB, 140, 28, true);
            {
                juce::Graphics g (im);
                g.setColour (EstatEstudi::colorPista (t).withAlpha (0.85f));
                g.fillRoundedRectangle (0.0f, 0.0f, 140.0f, 28.0f, 6.0f);
                g.setColour (juce::Colours::black);
                g.setFont (dibuix::lletra (13.0f, true));
                g.drawText (nom, 6, 0, 128, 28, juce::Justification::centred, true);
            }
            arrossegant = desti = -1;
            estat.info = "Deixa «"_ca + nom + "» al botó de notes fantasma d'un Piano Roll"_ca;
            estat.avisa();
            c->startDragging ("mibase-pista:" + juce::String (t.id), this, juce::ScaledImage (im), true);
            return;
        }
    if (std::abs (e.y - inici.y) < 6) return;
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
    if (fila >= 0 && fila < estat.numPistes() && estat.filaCarpeta (fila))
    {
        if (auto* c = ed::carpeta (estat.projecte, estat.projecte.pistes[(size_t) fila].carpeta)) c->plegada = false;
        estat.canviat();
        return;
    }
    if (fila >= 0 && fila < estat.numPistes() && teclaA (fila, e.getPosition()) == Tecla::Cap && onCanviaNom)
        onCanviaNom (fila);
}

//==============================================================================
// ColumnaEsquerra
//==============================================================================
ColumnaEsquerra::ColumnaEsquerra (EstatEstudi& e) : SeccioEstudi (e)
{
    addChildComponent (opcions);
    for (auto* b : { &btTreball, &btMescles, &btEstructura })
    {
        addAndMakeVisible (b);
        b->setMidaText (12.0f);
        b->setLlum (vintage::ledTaronja);
    }
    btTreball.onClick = [this] { estat.vistaMescles = estat.vistaEstructura = false; estat.avisa(); };
    btMescles.onClick = [this] { estat.vistaMescles = true; estat.vistaEstructura = false; estat.avisa(); };
    btEstructura.onClick = [this] { estat.vistaEstructura = true; estat.vistaMescles = false; estat.avisa(); };
    changeListenerCallback (nullptr);
}

void ColumnaEsquerra::changeListenerCallback (juce::ChangeBroadcaster*)
{
    const bool ambOpcions = ! estat.foraTreball() && ! ambEditor();
    const int alt = ambOpcions ? opcions.alcada() : 0;
    opcions.setVisible (ambOpcions && alt > 0);
    if (opcions.isVisible()) opcions.actualitza();
    if (alt != altOpcions) { altOpcions = alt; resized(); }
    btTreball.setToggleState (! estat.foraTreball(), juce::dontSendNotification);
    btMescles.setToggleState (estat.vistaMescles, juce::dontSendNotification);
    btEstructura.setToggleState (estat.vistaEstructura, juce::dontSendNotification);
    if (panellEditor != nullptr && panellEditor->isVisible() != ambEditor())
    {
        panellEditor->setVisible (ambEditor());
        resized();
    }
    repaint();
}

void ColumnaEsquerra::resized()
{
    auto r = getLocalBounds().reduced (8, 10);
    btTreball.setBounds (r.removeFromTop (44));
    r.removeFromTop (6);
    btMescles.setBounds (r.removeFromTop (44));
    r.removeFromTop (6);
    btEstructura.setBounds (r.removeFromTop (44));
    r.removeFromTop (16);
    if (panellEditor != nullptr) panellEditor->setBounds (r.withTrimmedLeft (-2).withTrimmedRight (-4).withTrimmedBottom (-6));
    zonaNom = r.removeFromTop (44);
    opcions.setBounds (r.removeFromTop (altOpcions));
    if (altOpcions > 0) r.removeFromTop (8);
    zonaPan = r.removeFromTop (52);
    zonaTecles = r.removeFromBottom (30);
    zonaLcd = r.removeFromBottom (30);
    zonaFader = r;
}

void ColumnaEsquerra::paint (juce::Graphics& g)
{
    vintage::panell (g, getLocalBounds().toFloat());
    g.setColour (tema::linia);
    g.fillRect (getWidth() - 1, 0, 1, getHeight());

    // A la taula de mescles, cada pista ja té la seva tira: aquí només hi ha les pestanyes.
    // Amb un editor obert, hi ha el seu panell (el d'harmonia) en lloc del fader.
    if (estat.foraTreball() || ambEditor()) return;

    auto* t = estat.pista (estat.pistaTriada);
    auto cap = zonaNom.withTrimmedLeft (2);
    if (estat.masterTriat())
    {
        // 0.17: el màster: nom, fader i nivell de la sortida (la sortida es tria a les opcions de sobre).
        g.setColour (tema::accent);
        g.fillRect (cap.getX(), cap.getY() + 2, 4, 34);
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (13.0f, true));
        g.drawText ("Màster"_ca, cap.withTrimmedLeft (10).removeFromTop (20), juce::Justification::centredLeft, true);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.0f));
        g.drawText ("L+R · on arriben les pistes"_ca, cap.withTrimmedLeft (10).withTrimmedTop (20), juce::Justification::centredLeft, true);
        dibuixEstudi::faderVertical (g, zonaFader.toFloat(), estat.projecte.masterDb, tema::accent, true, estat.nivellsSortida[0], estat.nivellsSortida[1]);
        vintage::lcd (g, zonaLcd.reduced (2, 3).toFloat(), EstatEstudi::textDb (estat.projecte.masterDb), {});
        return;
    }
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

    const bool actiu = sona (estat.projecte, *t);
    const auto color = EstatEstudi::colorPista (*t);
    vintage::pom (g, rectPom().toFloat(), (float) t->pan, color, actiu);
    g.setColour (tema::suau);
    g.setFont (dibuix::lletra (10.5f, true));
    auto text = zonaPan.withTrimmedLeft (rectPom().getRight() - zonaPan.getX() + 8);
    g.drawText ("PAN", text.removeFromTop (zonaPan.getHeight() / 2), juce::Justification::bottomLeft);
    g.setColour (tema::textMig);
    g.setFont (dibuix::mono (12.0f));
    g.drawText (EstatEstudi::textPan (t->pan), text, juce::Justification::topLeft);

    dibuixEstudi::faderVertical (g, zonaFader.toFloat(), t->faderDb, color, actiu,
                                 estat.nivell (estat.pistaTriada, 0), estat.nivell (estat.pistaTriada, t->canals == 2 ? 1 : 0));

    vintage::lcd (g, zonaLcd.reduced (2, 3).toFloat(), EstatEstudi::textDb (t->faderDb), {});

    const int ample = 28, sep = 4;
    auto tecles = zonaTecles.withSizeKeepingCentre (ample * 3 + sep * 2, 26);
    dibuixEstudi::teclaPetita (g, tecles.removeFromLeft (ample), "M", t->mut, vintage::ledBlau);
    tecles.removeFromLeft (sep);
    dibuixEstudi::teclaPetita (g, tecles.removeFromLeft (ample), "S", t->solo, vintage::ledGroc);
    tecles.removeFromLeft (sep);
    dibuixEstudi::teclaPetita (g, tecles.removeFromLeft (ample), {}, t->grava, vintage::ledVermell, true);
}

juce::Rectangle<int> ColumnaEsquerra::rectPom() const
{
    return zonaPan.withWidth (44).withSizeKeepingCentre (44, 44).withX (zonaPan.getX() + 4);
}

void ColumnaEsquerra::mouseDown (const juce::MouseEvent& e)
{
    movent = Arrossega::Res;
    if (ambEditor()) return;
    if (estat.masterTriat() && ! estat.foraTreball())
    {
        if (! zonaFader.contains (e.getPosition())) return;
        movent = Arrossega::Fader;
        valor0 = EstatEstudi::posFader (estat.projecte.masterDb);
        ultimaY = e.position.y;
        comencaCanvi();
        return;
    }
    auto* t = estat.pista (estat.pistaTriada);
    if (t == nullptr || estat.foraTreball()) return;

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

    if (zonaPan.contains (e.getPosition()))
    {
        movent = Arrossega::Pan;
        valor0 = (float) t->pan;
    }
    else if (zonaFader.contains (e.getPosition()))
    {
        movent = Arrossega::Fader;
        valor0 = EstatEstudi::posFader (t->faderDb);
    }
    else return;
    ultimaY = e.position.y;
    comencaCanvi();
}

void ColumnaEsquerra::mouseDrag (const juce::MouseEvent& e)
{
    if (movent == Arrossega::Res) return;
    restaura();
    const float dy = e.position.y - ultimaY;
    ultimaY = e.position.y;
    if (estat.masterTriat())
    {
        const auto carril = dibuixEstudi::carrilVertical (zonaFader.toFloat());
        valor0 = juce::jlimit (0.0f, 1.0f, valor0 - dy / carril.getHeight() * (e.mods.isShiftDown() ? 0.1f : 1.0f));
        estat.projecte.masterDb = EstatEstudi::dbFader (valor0);
        estat.info = "Volum del màster: "_ca + EstatEstudi::textDb (estat.projecte.masterDb);
        estat.avisa();
        return;
    }
    auto* t = estat.pista (estat.pistaTriada);
    if (t == nullptr) return;
    if (movent == Arrossega::Pan)
    {
        valor0 = juce::jlimit (-1.0f, 1.0f, valor0 - dy / (e.mods.isShiftDown() ? 1500.0f : 150.0f));
        t->pan = std::abs (valor0) < 0.005f ? 0.0 : (double) valor0;
        estat.info = "Panorama de «"_ca + dibuix::utf8 (t->nom) + "»: "_ca + EstatEstudi::textPan (t->pan);
    }
    else
    {
        const auto carril = dibuixEstudi::carrilVertical (zonaFader.toFloat());
        valor0 = juce::jlimit (0.0f, 1.0f, valor0 - dy / carril.getHeight() * (e.mods.isShiftDown() ? 0.1f : 1.0f));
        t->faderDb = EstatEstudi::dbFader (valor0);
        estat.info = "Volum de «"_ca + dibuix::utf8 (t->nom) + "»: "_ca + EstatEstudi::textDb (t->faderDb) + "  (Maj: més fi; doble clic: 0 dB)";
    }
    estat.avisa();
}

void ColumnaEsquerra::mouseUp (const juce::MouseEvent&)
{
    if (movent != Arrossega::Res) acabaCanvi();
    movent = Arrossega::Res;
}

void ColumnaEsquerra::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (estat.masterTriat() && ! estat.foraTreball() && ! ambEditor() && zonaFader.contains (e.getPosition()))
    {
        comencaCanvi(); restaura(); estat.projecte.masterDb = 0.0; acabaCanvi();
        return;
    }
    if (estat.pista (estat.pistaTriada) == nullptr || estat.foraTreball() || ambEditor()) return;
    const bool pan = zonaPan.contains (e.getPosition());
    if (! pan && ! zonaFader.contains (e.getPosition())) return;
    comencaCanvi();
    restaura();
    if (pan) estat.pista (estat.pistaTriada)->pan = 0.0;
    else estat.pista (estat.pistaTriada)->faderDb = 0.0;
    acabaCanvi();
}

//==============================================================================
// TaulaMescles
//==============================================================================
TaulaMescles::Geo TaulaMescles::geo (juce::Rectangle<int> r, bool master) const
{
    Geo g;
    const int x = r.getX() + 6, w = r.getWidth() - 12;
    g.nom = { x, 14, w, 24 };
    int y = 44;
    g.etInserts = { x, y, w, 12 };
    y += 14;
    for (int k = 0; k < numInserts; ++k) { g.inserts[(size_t) k] = { x, y, w, 18 }; y += 21; }
    y += 4;
    if (! master)
    {
        g.etEnvia = { x, y, w, 12 };
        y += 14;
        for (int k = 0; k < numEnvia; ++k) { g.envia[(size_t) k] = { x, y, w, 18 }; y += 21; }
        y += 4;
        g.pan = { x, y, w, 46 };
        y += 50;
        const int ample = 26, sep = 3;
        const int x0 = r.getCentreX() - (ample * 3 + sep * 2) / 2;
        for (int k = 0; k < 3; ++k) g.tecles[(size_t) k] = { x0 + k * (ample + sep), y, ample, 24 };
        y += 30;
    }
    else
        y = 44 + 14 + numInserts * 21 + 4 + 14 + numEnvia * 21 + 4 + 50 + 30;   // alineat amb les altres tires
    const int alt = juce::jlimit (90, 280, getHeight() - y - 30 - 26);
    g.fader = { r.getX() + 4, y, r.getWidth() - 8, alt };
    g.lcd = { x + 2, g.fader.getBottom() + 4, w - 4, 26 };
    g.numero = { r.getX(), g.lcd.getBottom() + 4, r.getWidth(), 18 };
    return g;
}

void TaulaMescles::pintaTira (juce::Graphics& g, juce::Rectangle<int> r, int i)
{
    const bool master = i < 0;
    const auto& p = estat.projecte;
    const auto* t = master ? nullptr : &p.pistes[(size_t) i];
    const auto color = master ? tema::accent : EstatEstudi::colorPista (*t);
    const auto ge = geo (r, master);

    vintage::panell (g, r.reduced (2, 0).toFloat(), 6.0f);
    if (! master && i == estat.pistaTriada)
    {
        g.setColour (tema::accent.withAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (2, 0).toFloat().reduced (0.5f), 6.0f, 1.5f);
    }
    g.setColour (color);
    g.fillRect (r.getX() + 8, 8, r.getWidth() - 16, 3);
    g.setColour (tema::text);
    g.setFont (dibuix::lletra (12.0f, true));
    g.drawFittedText (master ? "Màster"_ca : dibuix::utf8 (t->nom), ge.nom, juce::Justification::centred, 1, 0.75f);

    auto etiqueta = [&] (juce::Rectangle<int> z, const juce::String& text)
    {
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (9.5f, true));
        g.drawText (text, z, juce::Justification::centredLeft);
    };
    auto ranura = [&] (juce::Rectangle<int> z)
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (z.toFloat(), 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.fillRect (z.getX() + 3, z.getBottom() - 1, z.getWidth() - 6, 1);
        g.setColour (tema::linia2);
        g.setFont (dibuix::lletra (10.5f));
        g.drawText (juce::String::fromUTF8 ("—"), z, juce::Justification::centred);
    };
    etiqueta (ge.etInserts, "INSERTS");
    {
        // 0.20: els inserts de veritat (llumeta, nom, canviar). Si n'hi ha més de 4, l'últim diu quants en falten.
        const auto& v = master ? p.insertsMaster : t->inserts;
        for (int k = 0; k < numInserts; ++k)
        {
            const auto z = ge.inserts[(size_t) k];
            if (k == numInserts - 1 && (int) v.size() > numInserts)
            {
                ranura (z);
                g.setColour (tema::suau);
                g.fillRect (z.reduced (1));
                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.fillRoundedRectangle (z.toFloat(), 3.0f);
                g.setColour (tema::text);
                g.setFont (dibuix::lletra (10.5f));
                g.drawText ("+" + juce::String ((int) v.size() - numInserts + 1) + " (columna esquerra)"_ca, z, juce::Justification::centred);
                continue;
            }
            inserts::pinta (g, z, k < (int) v.size() ? &v[(size_t) k] : nullptr, k == (int) v.size(), true);
        }
    }

    if (! master)
    {
        etiqueta (ge.etEnvia, "ENVIAMENTS");
        for (auto& z : ge.envia) ranura (z);

        const bool actiu = sona (p, *t);
        auto pomR = ge.pan.withWidth (44).withX (ge.pan.getX());
        vintage::pom (g, pomR.toFloat(), (float) t->pan, color, actiu);
        auto text = ge.pan.withTrimmedLeft (48);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (9.5f, true));
        g.drawText ("PAN", text.removeFromTop (ge.pan.getHeight() / 2), juce::Justification::bottomLeft);
        g.setColour (tema::textMig);
        g.setFont (dibuix::mono (11.5f));
        g.drawText (EstatEstudi::textPan (t->pan), text, juce::Justification::topLeft);

        dibuixEstudi::teclaPetita (g, ge.tecles[0], "M", t->mut, vintage::ledBlau);
        dibuixEstudi::teclaPetita (g, ge.tecles[1], "S", t->solo, vintage::ledGroc);
        dibuixEstudi::teclaPetita (g, ge.tecles[2], {}, t->grava, vintage::ledVermell, true);

        dibuixEstudi::faderVertical (g, ge.fader.toFloat(), t->faderDb, color, actiu, estat.nivell (i, 0), estat.nivell (i, t->canals == 2 ? 1 : 0));
        vintage::lcd (g, ge.lcd.toFloat(), EstatEstudi::textDb (t->faderDb), {});
        g.setColour (tema::suau);
        g.setFont (dibuix::mono (12.0f));
        g.drawText (juce::String (i + 1) + "  " + (t->tipus == "bateria" ? juce::String ("BAT") : t->tipus == "midi" ? juce::String ("MIDI") : EstatEstudi::textEntrada (*t)), ge.numero, juce::Justification::centred);
    }
    else
    {
        dibuixEstudi::faderVertical (g, ge.fader.toFloat(), p.masterDb, color, true, estat.nivellsSortida[0], estat.nivellsSortida[1]);
        vintage::lcd (g, ge.lcd.toFloat(), EstatEstudi::textDb (p.masterDb), {});
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.0f));
        g.drawText ("L  R", ge.numero, juce::Justification::centred);
    }
}

void TaulaMescles::paint (juce::Graphics& g)
{
    g.fillAll (tema::fons);
    pintaTira (g, rectMaster(), -1);

    if (estat.numPistes() == 0)
    {
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (14.0f));
        g.drawText ("Encara no hi ha pistes. Crea'n una a la taula de treball.", getLocalBounds().withRight (rectMaster().getX()).reduced (20),
                    juce::Justification::topLeft);
        return;
    }

    juce::Graphics::ScopedSaveState guarda (g);
    g.reduceClipRegion (0, 0, rectMaster().getX() - 6, getHeight());
    for (int i = 0; i < estat.numPistes(); ++i)
    {
        auto r = rectTira (i);
        if (r.getRight() < 0 || r.getX() > getWidth()) continue;
        pintaTira (g, r, i);
    }
}

bool TaulaMescles::troba (juce::Point<int> p, int& quina, Zona& zona, int& tecla) const
{
    tecla = -1;
    zona = Zona::Res;
    juce::Rectangle<int> r;
    if (rectMaster().contains (p)) { quina = -1; r = rectMaster(); }
    else
    {
        quina = tiraA (p.x);
        if (quina < 0 || quina >= estat.numPistes() || p.x >= rectMaster().getX() - 6) return false;
        r = rectTira (quina);
    }
    const auto ge = geo (r, quina < 0);
    if (quina >= 0)
        for (int k = 0; k < 3; ++k)
            if (ge.tecles[(size_t) k].contains (p)) { zona = Zona::Tecla; tecla = k; return true; }
    if (quina >= 0 && ge.pan.contains (p)) zona = Zona::Pan;
    else if (ge.fader.contains (p)) zona = Zona::Fader;
    else
    {
        for (int k = 0; k < numInserts; ++k) if (ge.inserts[(size_t) k].contains (p)) { zona = Zona::Insert; tecla = k; }
        if (quina >= 0) for (auto& z : ge.envia) if (z.contains (p)) zona = Zona::Ranura;
    }
    return true;
}

void TaulaMescles::mouseDown (const juce::MouseEvent& e)
{
    int i = -1, k = -1;
    Zona z;
    arrossega = Zona::Res;
    if (! troba (e.getPosition(), i, z, k)) return;
    tira = i;
    if (i >= 0)
    {
        if (estat.pistaTriada != i) estat.capTriat();
        estat.pistaTriada = i;
    }

    if (z == Zona::Tecla)
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
    if (z == Zona::Insert)
    {
        const auto ge = geo (i < 0 ? rectMaster() : rectTira (i), i < 0);
        auto r = ge.inserts[(size_t) k];
        const auto* v = inserts::llista (estat, i);
        if (v != nullptr && k == numInserts - 1 && (int) v->size() > numInserts)
        {
            estat.info = "Aquesta pista té més efectes: els veuràs tots a la columna esquerra (pestanya Taula de treball)."_ca;
            estat.avisa();
            return;
        }
        inserts::clic (estat, plugins, i, k, r, e.getPosition(), e.mods.isPopupMenu(), this);
        estat.info = inserts::ajuda();
        estat.avisa();
        return;
    }
    if (z == Zona::Ranura)
    {
        estat.info = "Els enviaments a busos arriben més endavant."_ca;
        estat.avisa();
        return;
    }
    if (z == Zona::Pan || z == Zona::Fader)
    {
        arrossega = z;
        if (z == Zona::Pan) valor0 = (float) estat.projecte.pistes[(size_t) i].pan;
        else valor0 = EstatEstudi::posFader (i < 0 ? estat.projecte.masterDb : estat.projecte.pistes[(size_t) i].faderDb);
        ultimaY = e.position.y;
        comencaCanvi();
    }
    estat.avisa();
}

void TaulaMescles::mouseDrag (const juce::MouseEvent& e)
{
    if (arrossega == Zona::Res) return;
    restaura();
    const bool fi = e.mods.isShiftDown();
    const float dy = e.position.y - ultimaY;
    ultimaY = e.position.y;
    if (arrossega == Zona::Pan && tira >= 0)
    {
        auto& t = estat.projecte.pistes[(size_t) tira];
        valor0 = juce::jlimit (-1.0f, 1.0f, valor0 - dy / (fi ? 1500.0f : 150.0f));
        t.pan = std::abs (valor0) < 0.005f ? 0.0 : (double) valor0;
        estat.info = "Panorama de «"_ca + dibuix::utf8 (t.nom) + "»: "_ca + EstatEstudi::textPan (t.pan);
    }
    else if (arrossega == Zona::Fader)
    {
        const auto r = tira < 0 ? rectMaster() : rectTira (tira);
        const auto carril = dibuixEstudi::carrilVertical (geo (r, tira < 0).fader.toFloat());
        valor0 = juce::jlimit (0.0f, 1.0f, valor0 - dy / carril.getHeight() * (fi ? 0.1f : 1.0f));
        const double db = EstatEstudi::dbFader (valor0);
        if (tira < 0)
        {
            estat.projecte.masterDb = db;
            estat.info = "Volum de la sortida: "_ca + EstatEstudi::textDb (db);
        }
        else
        {
            auto& t = estat.projecte.pistes[(size_t) tira];
            t.faderDb = db;
            estat.info = "Volum de «"_ca + dibuix::utf8 (t.nom) + "»: "_ca + EstatEstudi::textDb (db) + "  (Maj: més fi; doble clic: 0 dB)";
        }
    }
    estat.avisa();
}

void TaulaMescles::mouseUp (const juce::MouseEvent&)
{
    if (arrossega != Zona::Res) acabaCanvi();
    arrossega = Zona::Res;
}

void TaulaMescles::mouseDoubleClick (const juce::MouseEvent& e)
{
    int i = -1, k = -1;
    Zona z;
    if (! troba (e.getPosition(), i, z, k) || (z != Zona::Pan && z != Zona::Fader)) return;
    comencaCanvi();
    restaura();
    if (i < 0) estat.projecte.masterDb = 0.0;
    else if (z == Zona::Pan) estat.projecte.pistes[(size_t) i].pan = 0.0;
    else estat.projecte.pistes[(size_t) i].faderDb = 0.0;
    acabaCanvi();
}

void TaulaMescles::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float d = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
    const int maxim = juce::jmax (0, estat.numPistes() * ampleTira - (rectMaster().getX() - 6));
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
    addAndMakeVisible (btEstructura);
    btEstructura.setTooltip ("Quina estructura sona en fer play: la línia de temps tal com és (lineal) o una de la taula d'estructura"_ca);
    btEstructura.onClick = [this]
    {
        const auto& p = estat.projecte;
        juce::PopupMenu m;
        m.addItem (1, "Lineal (la línia de temps tal com és)"_ca, true, p.estructuraActiva.empty());
        for (int i = 0; i < (int) p.estructures.size(); ++i)
            m.addItem (10 + i, juce::String (i + 1) + " · " + dibuix::utf8 (p.estructures[(size_t) i].nom)
                                   + " (" + juce::String ((int) p.estructures[(size_t) i].peces.size()) + " peces)",
                       true, p.estructures[(size_t) i].id == p.estructuraActiva);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btEstructura), [this] (int r)
        {
            if (r <= 0) return;
            comencaCanvi();
            restaura();
            auto& pp = estat.projecte;
            pp.estructuraActiva = r == 1 ? std::string() : pp.estructures[(size_t) (r - 10)].id;
            acabaCanvi();
        });
    };
}

void PanellPista::changeListenerCallback (juce::ChangeBroadcaster*)
{
    const bool hiHaClips = ! estat.triats.empty() || estat.rang.actiu;
    btDuplica.setVisible (hiHaClips);
    btTreu.setVisible (hiHaClips);
    {
        const auto* e = ed::estructura (estat.projecte, estat.projecte.estructuraActiva);
        btEstructura.setButtonText (e == nullptr ? "Estructura: lineal"_ca : "Estructura: "_ca + dibuix::utf8 (e->nom));
    }
    repaint();
}

void PanellPista::resized()
{
    auto botons = getLocalBounds().reduced (18, 16).withTrimmedTop (230).withHeight (36);
    btDuplica.setBounds (botons.removeFromLeft (botons.getWidth() / 2 - 5));
    botons.removeFromLeft (10);
    btTreu.setBounds (botons);
    btEstructura.setBounds (getLocalBounds().reduced (18, 16).removeFromBottom (34));
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
        if (c->midi)
        {
            int toMin = 127, toMax = 0;
            for (auto& n : c->notes) { toMin = juce::jmin (toMin, n.to); toMax = juce::jmax (toMax, n.to); }
            linia ("Clip", "MIDI");
            linia ("Notes", juce::String ((int) c->notes.size()) + (c->notes.empty() ? juce::String()
                       : "  (" + juce::String::fromUTF8 (midi::nomNota (toMin).c_str()) + " a " + juce::String::fromUTF8 (midi::nomNota (toMax).c_str()) + ")"));
            linia ("Comença"_ca, posicio (p, c->iniciVisible(), true));
            linia ("Llargada", juce::String (c->longitud() / ed::segonsPerCompas (p), 2).replaceCharacter ('.', ',') + " compassos");
            linia ("Estat", c->mut ? "silenciat" : "sona");
            linia ("Edita", "doble clic: Piano Roll (P)");
        }
        else
        {
        linia ("Presa", s ? dibuix::utf8 (s->nom) : juce::String ("finestra buida"));
        linia ("Fitxer", s ? dibuix::utf8 (s->fitxer).fromLastOccurrenceOf ("/", false, false) : juce::String ("-"));
        linia ("Comença"_ca, posicio (p, c->iniciVisible(), true));
        linia ("Llargada", juce::String (c->longitud(), 2).replaceCharacter ('.', ',') + " s");
        linia ("Fades", ms (c->fadeIn) + " / " + ms (c->fadeOut));
        linia ("Volum", EstatEstudi::textDb (c->guanyDb) + (c->mut ? " · silenciat"_ca : ""));
        linia ("Bandera", s && ed::banderaVisible (p, *c) ? posicio (p, c->pos + s->primer, true)
                                                          : (s && s->primer >= 0 ? juce::String ("fora del clip") : juce::String ("no en té"_ca)));
        }
    }
    else
    {
        g.setColour (estat.triats.size() > 1 || estat.rang.actiu ? tema::textMig : tema::suau);
        g.setFont (dibuix::lletra (12.5f));
        g.drawFittedText (estat.rang.actiu ? "Tros triat: "_ca + posicio (p, estat.rang.t0) + " a " + posicio (p, estat.rang.t1) + ", "
                                             + juce::String (estat.rang.pistaFins - estat.rang.pistaDe + 1) + " pistes.\nCtrl+C copia només aquest tros."_ca
                          : estat.triats.size() > 1 ? juce::String (estat.triats.size()) + " clips triats"
                                                       : "Clica un clip (amb la fletxa) per veure'n les dades."_ca,
                          info.removeFromTop (60), juce::Justification::topLeft, 3);
    }

    r.removeFromTop (290);
    dibuix::pintaEtiquetaSeccio (g, "Eines", r.removeFromTop (20));
    const char* linies[] = {
        "Fletxa (1): tria i mou. Arrossega en un lloc buit per fer un requadre. Cantonades: retallar i fades; punt del mig: volum.",
        "Llapis (2): clica i arrossega per fer una finestra; sense arrossegar, d'un compàs.",
        "Goma (3) · Tisores (4) · Cola (5): clica un clip i arrossega fins al del costat · Silenci (6).",
        "M: silencia el triat. Ctrl+C / Ctrl+X / Ctrl+V: copia, retalla i enganxa al capçal. Insert: enganxar inserint o trepitjant.",
        "Z: tot el projecte a la pantalla · Maj+Z: el que hi ha triat. Q / W / E: graella Compàs / Temps / Lliure.",
        "Botó dret en un lloc buit: fletxa. El capçal es mou clicant la regla.",
        "Roda: amunt i avall · Maj: de costat · Ctrl: zoom · Alt: alçada.",
    };
    g.setFont (dibuix::lletra (12.0f));
    for (auto* l : linies)
    {
        if (r.getHeight() < 46) break;
        g.setColour (tema::suau);
        g.drawFittedText (juce::String::fromUTF8 (l), r.removeFromTop (46), juce::Justification::topLeft, 3, 0.9f);
    }
}
