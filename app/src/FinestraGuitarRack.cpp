#include "FinestraGuitarRack.h"

#include "Controls.h"
#include "DialegText.h"
#include "Dibuix.h"
#include "EscGlobal.h"
#include "NavegadorPresets.h"
#include "Pom.h"
#include "Tema.h"
#include "Text.h"

#include <mibase/GuitarRack.h>

#include <set>

using namespace mibase::guitarrack;

namespace
{
    juce::String u8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str(), (int) s.size()); }
    juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }
    juce::Colour col (std::uint32_t c) { return juce::Colour (c); }
    constexpr float pi = juce::MathConstants<float>::pi;

    // ============================================================================================ preferits i presets
    juce::File carpeta() { return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Mibase").getChildFile ("GuitarRack"); }
    BibliotecaPresets& biblioteca()
    {
        static BibliotecaPresets b;
        static bool carregada = false;
        if (! carregada) { carregada = true; b.carrega (dibuix::cami (carpeta().getChildFile ("presets.json"))); }
        return b;
    }
    void desaBiblioteca() { carpeta().createDirectory(); biblioteca().desa (dibuix::cami (carpeta().getChildFile ("presets.json"))); }
    /** Pedals i amplis preferits de la biblioteca («p:clau», «a:clau»). */
    std::set<std::string>& preferits()
    {
        static std::set<std::string> s;
        static bool carregat = false;
        if (! carregat)
        {
            carregat = true;
            juce::StringArray l;
            l.addLines (carpeta().getChildFile ("preferits.txt").loadFileAsString());
            for (auto& x : l) if (x.trim().isNotEmpty()) s.insert (x.trim().toStdString());
        }
        return s;
    }
    void commutaPreferit (const std::string& k)
    {
        auto& s = preferits();
        if (s.count (k)) s.erase (k); else s.insert (k);
        juce::StringArray l;
        for (auto& x : s) l.add (u8 (x));
        carpeta().createDirectory();
        carpeta().getChildFile ("preferits.txt").replaceWithText (l.joinIntoString ("\n"));
    }

    // ============================================================================================ dibuix
    void pintaCargol (juce::Graphics& g, juce::Point<float> c, float r)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe8e8ea), c.x - r, c.y - r, juce::Colour (0xff6c6c72), c.x + r, c.y + r, false));
        g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawLine (c.x - r * 0.65f, c.y - r * 0.2f, c.x + r * 0.65f, c.y + r * 0.2f, juce::jmax (1.0f, r * 0.28f));
    }

    void pintaLed (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour color, float intensitat)
    {
        intensitat = juce::jlimit (0.0f, 1.0f, intensitat);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (c.x - r - 1.5f, c.y - r - 1.5f, 2 * r + 3, 2 * r + 3);
        if (intensitat > 0.05f)
        {
            juce::ColourGradient halo (color.withAlpha (0.45f * intensitat), c.x, c.y, color.withAlpha (0.0f), c.x + r * 4.0f, c.y, true);
            g.setGradientFill (halo);
            g.fillEllipse (c.x - r * 4.0f, c.y - r * 4.0f, r * 8.0f, r * 8.0f);
        }
        const auto base = color.darker (2.2f).interpolatedWith (color, intensitat);
        g.setGradientFill (juce::ColourGradient (base.brighter (0.6f), c.x - r * 0.4f, c.y - r * 0.5f, base.darker (0.3f), c.x + r, c.y + r, true));
        g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
        g.setColour (juce::Colours::white.withAlpha (0.35f + 0.4f * intensitat));
        g.fillEllipse (c.x - r * 0.55f, c.y - r * 0.65f, r * 0.6f, r * 0.45f);
    }

    /** Un jack (on es connecta un cable). */
    void pintaJack (juce::Graphics& g, juce::Point<float> c, float r)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd9d9de), c.x - r, c.y - r, juce::Colour (0xff4a4a50), c.x + r, c.y + r, false));
        g.fillEllipse (c.x - r, c.y - r, 2 * r, 2 * r);
        g.setColour (juce::Colour (0xff0a0a0b));
        g.fillEllipse (c.x - r * 0.45f, c.y - r * 0.45f, r * 0.9f, r * 0.9f);
    }

    /** Un cable de guitarra amb les clavilles, que penja una mica. */
    void pintaCable (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, juce::Colour color, float gruix = 6.0f, float caiguda = -1.0f)
    {
        const float d = a.getDistanceFrom (b);
        if (caiguda < 0.0f) caiguda = 14.0f + d * 0.22f;
        juce::Path p;
        p.startNewSubPath (a);
        p.cubicTo (a.translated (d * 0.18f, caiguda), b.translated (-d * 0.18f, caiguda), b);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.strokePath (p, juce::PathStrokeType (gruix + 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::translation (2.0f, 4.0f));
        g.setColour (color);
        g.strokePath (p, juce::PathStrokeType (gruix, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.strokePath (p, juce::PathStrokeType (gruix * 0.28f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::translation (0.0f, -gruix * 0.25f));
        // clavilles: cos metàl·lic i funda de goma
        for (auto pt : { a, b })
        {
            const auto r = juce::Rectangle<float> (gruix * 2.6f, gruix * 1.6f).withCentre (pt);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe2e2e6), r.getX(), r.getY(), juce::Colour (0xff5a5a62), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, gruix * 0.4f);
            g.setColour (juce::Colour (0xff111113));
            g.fillRoundedRectangle (r.reduced (gruix * 0.55f, gruix * 0.1f), gruix * 0.3f);
        }
    }

    /** Un pom: 0 negre de pedal amb ratlla blanca, 1 cromat, 2 cap de pollastre crema, 3 tapa daurada, 4 faldilla amb números,
        5 negre llis amb punt. «v01» = 0..1 (de −135° a +135°). */
    void pintaPom (juce::Graphics& g, juce::Rectangle<float> r, float v01, int estil, juce::Colour cos = juce::Colour (0xff151515))
    {
        const float s = juce::jmin (r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre (s, s);
        const auto c = r.getCentre();
        const float a = -0.75f * pi + juce::jlimit (0.0f, 1.0f, v01) * 1.5f * pi;
        auto punt = [&] (float rad, float ang) { return c.getPointOnCircumference (rad, ang); };
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (r.translated (s * 0.03f, s * 0.07f));
        switch (estil)
        {
            case 1:   // cromat amb estries
            {
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff4f4f6), r.getX(), r.getY(), juce::Colour (0xff6a6a72), r.getRight(), r.getBottom(), false));
                g.fillEllipse (r);
                g.setColour (juce::Colours::black.withAlpha (0.18f));
                for (int i = 0; i < 28; ++i) { const float t = (float) i / 28.0f * 2.0f * pi; g.drawLine ({ punt (s * 0.5f - 0.5f, t), punt (s * 0.42f, t) }, 1.0f); }
                const auto in = r.reduced (s * 0.14f);
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffffff), in.getX(), in.getY(), juce::Colour (0xff9a9aa2), in.getRight(), in.getBottom(), true));
                g.fillEllipse (in);
                g.setColour (juce::Colour (0xff202024));
                g.drawLine ({ punt (s * 0.08f, a), punt (s * 0.36f, a) }, juce::jmax (1.5f, s * 0.07f));
                break;
            }
            case 2:   // cap de pollastre
            {
                g.setColour (juce::Colour (0xff121212));
                g.fillEllipse (r.reduced (s * 0.08f));
                juce::Path p;
                p.addRoundedRectangle (-s * 0.13f, -s * 0.5f, s * 0.26f, s * 0.95f, s * 0.11f);
                p.applyTransform (juce::AffineTransform::rotation (a).translated (c));
                g.setGradientFill (juce::ColourGradient (cos.brighter (0.3f), r.getX(), r.getY(), cos.darker (0.4f), r.getRight(), r.getBottom(), false));
                g.fillPath (p);
                g.setColour (juce::Colours::black.withAlpha (0.4f));
                g.strokePath (p, juce::PathStrokeType (1.0f));
                g.setColour (juce::Colours::white.withAlpha (0.9f));
                g.drawLine ({ punt (s * 0.12f, a), punt (s * 0.42f, a) }, juce::jmax (1.2f, s * 0.04f));
                break;
            }
            case 3:   // tapa daurada
            {
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2b2e), r.getX(), r.getY(), juce::Colour (0xff050506), r.getRight(), r.getBottom(), false));
                g.fillEllipse (r);
                const auto t = r.reduced (s * 0.2f);
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff7e3a0), t.getX(), t.getY(), juce::Colour (0xff9a7a2c), t.getRight(), t.getBottom(), false));
                g.fillEllipse (t);
                g.setColour (juce::Colour (0xff1a1a1a));
                g.drawLine ({ punt (s * 0.05f, a), punt (s * 0.47f, a) }, juce::jmax (1.5f, s * 0.07f));
                break;
            }
            case 4:   // faldilla amb números
            {
                g.setColour (juce::Colour (0xff0d0d0e));
                g.fillEllipse (r);
                g.setColour (juce::Colour (0xffd8d8dc));
                g.setFont (dibuix::lletra (juce::jmax (6.0f, s * 0.15f), true));
                for (int i = 1; i <= 10; ++i)
                {
                    const float t = -0.75f * pi + (float) (i - 1) / 9.0f * 1.5f * pi + (a + 0.75f * pi);
                    const auto pnt = punt (s * 0.4f, t);
                    g.drawText (juce::String (i), juce::Rectangle<float> (s * 0.2f, s * 0.16f).withCentre (pnt), juce::Justification::centred);
                }
                const auto t = r.reduced (s * 0.27f);
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a3e), t.getX(), t.getY(), juce::Colour (0xff09090a), t.getRight(), t.getBottom(), false));
                g.fillEllipse (t);
                g.setColour (juce::Colour (0xffeeeeee));
                g.drawLine ({ punt (s * 0.05f, a), punt (s * 0.22f, a) }, juce::jmax (1.2f, s * 0.05f));
                // marca fixa a dalt
                g.setColour (juce::Colour (0xffd8d8dc));
                g.fillEllipse (juce::Rectangle<float> (s * 0.05f, s * 0.05f).withCentre (punt (s * 0.52f, -0.5f * pi + 0.0f) + juce::Point<float> (0, 0)));
                break;
            }
            case 5:   // negre llis amb punt
            {
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3c3c40), r.getX(), r.getY(), juce::Colour (0xff0b0b0c), r.getRight(), r.getBottom(), false));
                g.fillEllipse (r);
                g.setColour (juce::Colours::white.withAlpha (0.12f));
                g.drawEllipse (r.reduced (1.0f), 1.0f);
                g.setColour (juce::Colours::white);
                g.fillEllipse (juce::Rectangle<float> (s * 0.11f, s * 0.11f).withCentre (punt (s * 0.34f, a)));
                break;
            }
            default:  // negre de pedal amb faldilla i ratlla
            {
                g.setColour (cos.darker (0.5f));
                g.fillEllipse (r);
                const auto t = r.reduced (s * 0.12f);
                g.setGradientFill (juce::ColourGradient (cos.brighter (0.35f), t.getX(), t.getY(), cos.darker (0.3f), t.getRight(), t.getBottom(), false));
                g.fillEllipse (t);
                g.setColour (juce::Colours::black.withAlpha (0.35f));
                for (int i = 0; i < 18; ++i) { const float u = (float) i / 18.0f * 2.0f * pi; g.drawLine ({ punt (s * 0.5f - 0.5f, u), punt (s * 0.44f, u) }, 1.0f); }
                const bool clar = cos.getPerceivedBrightness() > 0.6f;
                g.setColour (clar ? juce::Colour (0xff111111) : juce::Colours::white);
                g.drawLine ({ punt (s * 0.06f, a), punt (s * 0.38f, a) }, juce::jmax (1.4f, s * 0.07f));
                break;
            }
        }
    }

    /** Teixit de reixa (pantalles i combos). */
    void pintaReixa (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
    {
        g.setColour (c);
        g.fillRect (r);
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (r.toNearestInt());
        const float pas = 4.0f;
        g.setColour (c.brighter (0.25f).withAlpha (0.35f));
        for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += pas) g.drawLine (x, r.getY(), x + r.getHeight(), r.getBottom(), 1.0f);
        g.setColour (c.darker (0.6f).withAlpha (0.35f));
        for (float x = r.getX(); x < r.getRight() + r.getHeight(); x += pas) g.drawLine (x, r.getY(), x - r.getHeight(), r.getBottom(), 1.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.08f), r.getX(), r.getY(), juce::Colours::black.withAlpha (0.25f), r.getX(), r.getBottom(), false));
        g.fillRect (r);
    }

    /** Tolex (la pell del moble). */
    void pintaTolex (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float radi)
    {
        g.setGradientFill (juce::ColourGradient (c.brighter (0.18f), r.getX(), r.getY(), c.darker (0.35f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, radi);
        juce::Random rnd (17);
        g.setColour ((c.getPerceivedBrightness() > 0.5f ? juce::Colours::black : juce::Colours::white).withAlpha (0.05f));
        for (int i = 0; i < (int) (r.getWidth() * r.getHeight() / 90.0f); ++i)
            g.fillRect (r.getX() + rnd.nextFloat() * r.getWidth(), r.getY() + rnd.nextFloat() * r.getHeight(), 1.5f, 1.5f);
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.drawRoundedRectangle (r.reduced (1.0f), radi, 1.2f);
    }

    // ============================================================================================ geometria dels pedals
    float proporcio (Forma f)
    {
        switch (f)
        {
            case Forma::Compacte: return 0.58f;
            case Forma::Petit: return 0.56f;
            case Forma::Gran: return 0.82f;
            case Forma::Basculant: return 0.5f;
            case Forma::Ample: return 1.35f;
        }
        return 0.6f;
    }

    struct Geometria
    {
        juce::Rectangle<float> cos, nom, peu, tracio;
        juce::Point<float> led;
        float radiLed = 4.0f;
        std::vector<juce::Rectangle<float>> controls;   // un per control (el del pedal d'expressió és el tracio)
    };

    Geometria geometria (const ModelPedal& m, juce::Rectangle<float> r)
    {
        Geometria q;
        q.cos = r;
        const float w = r.getWidth(), h = r.getHeight();
        const int n = (int) m.controls.size();
        q.radiLed = juce::jmax (2.0f, w * 0.035f);
        auto fila = [&] (juce::Rectangle<float> z, std::vector<int> quins)
        {
            if (quins.empty()) return;
            const float cw = z.getWidth() / (float) quins.size();
            for (size_t i = 0; i < quins.size(); ++i) q.controls[(size_t) quins[i]] = juce::Rectangle<float> (z.getX() + cw * (float) i, z.getY(), cw, z.getHeight());
        };
        q.controls.assign ((size_t) n, {});
        std::vector<int> normals, pedal;
        for (int i = 0; i < n; ++i) (m.controls[(size_t) i].tipus == TipusControl::Pedal ? pedal : normals).push_back (i);
        auto files = [&] (juce::Rectangle<float> z, int maxPerFila)
        {
            const int nn = (int) normals.size();
            if (nn <= maxPerFila) { fila (z, normals); return; }
            const int f1 = (nn + 1) / 2;
            fila (z.removeFromTop (z.getHeight() * 0.5f), std::vector<int> (normals.begin(), normals.begin() + f1));
            fila (z, std::vector<int> (normals.begin() + f1, normals.end()));
        };
        switch (m.forma)
        {
            case Forma::Compacte:
            {
                auto z = juce::Rectangle<float> (r.getX() + w * 0.08f, r.getY() + h * 0.05f, w * 0.84f, h * (n > 4 ? 0.36f : 0.24f));
                files (z, 4);
                q.led = { r.getX() + w * 0.5f, r.getY() + h * (n > 4 ? 0.44f : 0.33f) };
                q.peu = juce::Rectangle<float> (r.getX() + w * 0.04f, r.getY() + h * 0.48f, w * 0.92f, h * 0.48f);
                q.nom = juce::Rectangle<float> (r.getX() + w * 0.1f, r.getY() + h * 0.52f, w * 0.8f, h * 0.08f);
                break;
            }
            case Forma::Petit:
            {
                files (juce::Rectangle<float> (r.getX() + w * 0.06f, r.getY() + h * 0.06f, w * 0.88f, h * (n > 4 ? 0.34f : 0.24f)), 3);
                q.nom = juce::Rectangle<float> (r.getX() + w * 0.06f, r.getY() + h * 0.44f, w * 0.88f, h * 0.16f);
                q.led = { r.getX() + w * 0.5f, r.getY() + h * 0.64f };
                q.peu = juce::Rectangle<float> (w * 0.34f, w * 0.34f).withCentre ({ r.getCentreX(), r.getY() + h * 0.82f });
                break;
            }
            case Forma::Gran:
            {
                files (juce::Rectangle<float> (r.getX() + w * 0.05f, r.getY() + h * 0.06f, w * 0.9f, h * (n > 4 ? 0.36f : 0.26f)), 4);
                q.nom = juce::Rectangle<float> (r.getX() + w * 0.06f, r.getY() + h * 0.45f, w * 0.88f, h * 0.17f);
                q.led = { r.getX() + w * 0.5f, r.getY() + h * 0.68f };
                q.peu = juce::Rectangle<float> (w * 0.24f, w * 0.24f).withCentre ({ r.getCentreX(), r.getY() + h * 0.83f });
                break;
            }
            case Forma::Basculant:
            {
                fila (juce::Rectangle<float> (r.getX() + w * 0.1f, r.getY() + h * 0.02f, w * 0.8f, h * 0.13f), normals);
                q.tracio = juce::Rectangle<float> (r.getX() + w * 0.1f, r.getY() + h * (normals.empty() ? 0.06f : 0.17f), w * 0.8f, h * (normals.empty() ? 0.76f : 0.66f));
                for (int i : pedal) q.controls[(size_t) i] = q.tracio;
                q.nom = juce::Rectangle<float> (r.getX() + w * 0.12f, r.getY() + h * 0.86f, w * 0.76f, h * 0.08f);
                q.led = { r.getRight() - w * 0.06f, r.getY() + h * 0.9f };
                q.peu = q.tracio;
                break;
            }
            case Forma::Ample:
            {
                auto z = juce::Rectangle<float> (r.getX() + w * 0.05f, r.getY() + h * 0.07f, w * 0.9f, h * 0.42f);
                files (z, 4);
                q.nom = juce::Rectangle<float> (r.getX() + w * 0.18f, r.getY() + h * 0.54f, w * 0.64f, h * 0.14f);
                q.led = { r.getX() + w * 0.5f, r.getY() + h * 0.74f };
                q.peu = juce::Rectangle<float> (h * 0.2f, h * 0.2f).withCentre ({ r.getCentreX(), r.getY() + h * 0.86f });
                break;
            }
        }
        return q;
    }

    /** L'estil de pom d'un ampli (ModelAmp::estilPom) en el de pintaPom. */
    int estilPomAmpli (int e)
    {
        static const int t[5] = { 3, 4, 2, 1, 5 };
        return t[juce::jlimit (0, 4, e)];
    }

    int estilPomPedal (const ModelPedal& m)
    {
        if (m.marca == "Boss") return 0;
        if (m.marca == "Strymon" || m.marca == "Roland") return 5;
        if (m.colorPom != 0xff161616) return 2;
        if (m.marca == "Electro-Harmonix") return 5;
        return 0;
    }

    /** Dibuixa un pedal sencer. «valors» pot ser nul (els de sempre). */
    void pintaPedal (juce::Graphics& g, const ModelPedal& m, const Geometria& q, const Pedal* p, bool on, float llum, bool ambPoms, bool gran)
    {
        auto r = q.cos;
        const float w = r.getWidth(), h = r.getHeight();
        const auto c = col (m.color);
        const auto tx = col (m.colorText);
        const float radi = m.forma == Forma::Petit ? w * 0.12f : w * 0.07f;
        // ombra i cos
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (r.translated (w * 0.03f, h * 0.025f), radi);
        if (m.forma == Forma::Basculant)
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2c30), r.getX(), r.getY(), juce::Colour (0xff0c0c0d), r.getRight(), r.getBottom(), false));
            g.fillRoundedRectangle (r, radi);
            // la planxa del pedal, inclinada segons la posició
            float pos = 0.5f;
            for (int i = 0; i < (int) m.controls.size(); ++i)
                if (m.controls[(size_t) i].tipus == TipusControl::Pedal)
                    pos = p != nullptr ? Pedal::normal (m.controls[(size_t) i], p->valor (i)) : Pedal::normal (m.controls[(size_t) i], m.controls[(size_t) i].def);
            auto t = q.tracio;
            const float alcada = t.getHeight() * (0.82f + 0.18f * pos);
            auto pl = t.withHeight (alcada).withY (t.getBottom() - alcada);
            g.setGradientFill (juce::ColourGradient (c.brighter (0.45f), pl.getX(), pl.getY(), c.darker (0.5f), pl.getRight(), pl.getBottom(), false));
            g.fillRoundedRectangle (pl, w * 0.06f);
            auto goma = pl.reduced (pl.getWidth() * 0.1f, pl.getHeight() * 0.06f);
            g.setColour (juce::Colour (0xff111112));
            g.fillRoundedRectangle (goma, w * 0.04f);
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            for (float y = goma.getY() + 3.0f; y < goma.getBottom() - 2.0f; y += juce::jmax (3.0f, h * 0.018f)) g.drawLine (goma.getX() + 2, y, goma.getRight() - 2, y, 1.2f);
            g.setColour (juce::Colours::white.withAlpha (0.2f));
            g.drawRoundedRectangle (pl.reduced (0.5f), w * 0.06f, 1.0f);
        }
        else
        {
            g.setGradientFill (juce::ColourGradient (c.brighter (0.28f), r.getX(), r.getY(), c.darker (0.35f), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, radi);
            g.setColour (juce::Colours::white.withAlpha (0.22f));
            g.drawRoundedRectangle (r.reduced (1.0f), radi, juce::jmax (1.0f, w * 0.012f));
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.drawRoundedRectangle (r, radi, 1.0f);
        }
        // planxa del peu (pedals compactes): més fosca, amb ratlles
        if (m.forma == Forma::Compacte)
        {
            auto pl = q.peu;
            g.setGradientFill (juce::ColourGradient (c.darker (0.05f), pl.getX(), pl.getY(), c.darker (0.55f), pl.getX(), pl.getBottom(), false));
            g.fillRoundedRectangle (pl, w * 0.05f);
            g.setColour (juce::Colours::black.withAlpha (0.25f));
            g.drawRoundedRectangle (pl, w * 0.05f, 1.2f);
            auto goma = pl.withTrimmedTop (pl.getHeight() * 0.42f).reduced (w * 0.08f, pl.getHeight() * 0.07f);
            g.setColour (juce::Colour (0xff101011));
            g.fillRoundedRectangle (goma, w * 0.03f);
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            for (float x = goma.getX() + 3.0f; x < goma.getRight() - 2.0f; x += juce::jmax (3.0f, w * 0.04f)) g.drawLine (x, goma.getY() + 2, x, goma.getBottom() - 2, 1.0f);
        }
        else if (m.forma != Forma::Basculant)
        {
            const int n = m.forma == Forma::Ample ? 2 : 1;
            for (int k = 0; k < n; ++k)
            {
                auto f = q.peu.translated (n == 2 ? (k == 0 ? -w * 0.22f : w * 0.22f) : 0.0f, 0.0f);
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f2f4), f.getX(), f.getY(), juce::Colour (0xff55555c), f.getRight(), f.getBottom(), false));
                g.fillEllipse (f);
                g.setColour (juce::Colours::black.withAlpha (0.35f));
                g.drawEllipse (f, 1.0f);
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xffcfcfd4), f.getX(), f.getY(), juce::Colour (0xff8a8a90), f.getRight(), f.getBottom(), true));
                g.fillEllipse (f.reduced (f.getWidth() * 0.22f));
            }
            if (gran || w > 90.0f)
                for (auto pt : { r.getTopLeft().translated (w * 0.08f, w * 0.08f), r.getTopRight().translated (-w * 0.08f, w * 0.08f),
                                 r.getBottomLeft().translated (w * 0.08f, -w * 0.08f), r.getBottomRight().translated (-w * 0.08f, -w * 0.08f) })
                    pintaCargol (g, pt, juce::jmax (1.5f, w * 0.03f));
        }
        if (m.forma == Forma::Ample)
        {
            // finestreta amb el tipus
            auto d = q.nom.reduced (q.nom.getWidth() * 0.02f, q.nom.getHeight() * 0.05f);
            g.setColour (juce::Colour (0xff0b0d10));
            g.fillRoundedRectangle (d, 4.0f);
        }
        // nom
        g.setColour (m.forma == Forma::Ample ? juce::Colour (0xff8fe3ff) : tx);
        g.setFont (dibuix::lletra (juce::jmax (6.0f, q.nom.getHeight() * (m.forma == Forma::Compacte ? 0.75f : 0.58f)), true));
        g.drawFittedText (dibuix::majuscules (u8 (m.nom)), q.nom.toNearestInt(), juce::Justification::centred, 1, 0.6f);
        if (gran && m.forma != Forma::Compacte)
        {
            g.setColour (tx.withAlpha (0.7f));
            g.setFont (dibuix::lletra (juce::jmax (6.0f, q.nom.getHeight() * 0.22f), true));
            g.drawText ("MIBASE", q.nom.translated (0, q.nom.getHeight() * 0.62f).withHeight (q.nom.getHeight() * 0.4f), juce::Justification::centred);
        }
        pintaLed (g, q.led, q.radiLed, juce::Colour (0xffff3b2f), on ? 0.55f + 0.45f * llum : 0.0f);
        if (ambPoms)
            for (int i = 0; i < (int) m.controls.size() && i < (int) q.controls.size(); ++i)
            {
                const auto& k = m.controls[(size_t) i];
                if (k.tipus == TipusControl::Pedal) continue;
                const float v = p != nullptr ? Pedal::normal (k, p->valor (i)) : Pedal::normal (k, k.def);
                auto z = q.controls[(size_t) i];
                const float s = juce::jmin (z.getWidth() * 0.62f, z.getHeight() * 0.62f);
                auto kr = juce::Rectangle<float> (s, s).withCentre ({ z.getCentreX(), z.getY() + z.getHeight() * 0.42f });
                if (k.tipus == TipusControl::Interruptor)
                {
                    auto sw = kr.withSizeKeepingCentre (s * 0.32f, s * 0.7f);
                    g.setColour (juce::Colour (0xff1a1a1c));
                    g.fillRoundedRectangle (sw, s * 0.08f);
                    g.setColour (juce::Colour (0xffd0d0d4));
                    g.fillRoundedRectangle (sw.withHeight (sw.getHeight() * 0.45f).withY (v > 0.5f ? sw.getY() : sw.getCentreY()), s * 0.06f);
                }
                else pintaPom (g, kr, v, estilPomPedal (m), col (m.colorPom));
            }
    }

    /** Dibuixa un pedal dins un rectangle qualsevol (manté la proporció). */
    Geometria geometriaDins (const ModelPedal& m, juce::Rectangle<float> r)
    {
        const float pr = proporcio (m.forma);
        float h = r.getHeight(), w = h * pr;
        if (w > r.getWidth()) { w = r.getWidth(); h = w / pr; }
        return geometria (m, juce::Rectangle<float> (w, h).withCentre (r.getCentre()));
    }
}

namespace
{
    // ============================================================================================ el que els trossos demanen al panell
    struct Accions
    {
        virtual ~Accions() = default;
        virtual GuitarRack& rack() = 0;
        virtual void obrePedal (int k, juce::Rectangle<int> origen) = 0;
        /** Hi ha hagut un canvi fet per l'usuari (marca el preset com a modificat i ho redibuixa tot). */
        virtual void canviat() = 0;
        virtual void obrePantalla() = 0;
        virtual void menuPedal (int k) = 0;
        virtual juce::DragAndDropContainer* dnd() = 0;
        virtual juce::Component& arrel() = 0;
    };

    // ============================================================================================ un control d'un pedal gran
    class ControlPedal : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        ControlPedal (std::shared_ptr<Pedal> ped, int index, std::function<void()> enCanviar) : p (std::move (ped)), i (index), canviat (std::move (enCanviar))
        {
            const auto& k = control();
            setTooltip (u8 (k.nom) + (k.ajuda.empty() ? juce::String() : ": " + u8 (k.ajuda)) + "\n"_ca + "Arrossega amunt o avall (Maj: fi). Doble clic: el valor de sempre."_ca);
            setMouseCursor (k.tipus == TipusControl::Interruptor ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::UpDownResizeCursor);
        }
        const Control& control() const { return p->model().controls[(size_t) i]; }
        float norm() const { return Pedal::normal (control(), p->valor (i)); }

        void paint (juce::Graphics& g) override
        {
            const auto& k = control();
            const auto& m = p->model();
            auto r = getLocalBounds().toFloat();
            const auto tx = col (m.colorText);
            if (k.tipus == TipusControl::Pedal)
            {
                // la planxa ja la dibuixa el pedal; aquí només la indicació de la posició
                g.setColour (juce::Colours::white.withAlpha (arrossega ? 0.95f : 0.6f));
                g.setFont (dibuix::lletra (juce::jmax (9.0f, r.getHeight() * 0.05f), true));
                g.drawText (dibuix::majuscules (u8 (k.nom)) + "  " + u8 (p->text (i, p->valor (i))), r.removeFromBottom (r.getHeight() * 0.12f), juce::Justification::centred);
                return;
            }
            auto etiqueta = r.removeFromBottom (juce::jmax (12.0f, r.getHeight() * 0.24f));
            const float s = juce::jmin (r.getWidth() * 0.78f, r.getHeight() * 0.9f);
            auto kr = juce::Rectangle<float> (s, s).withCentre (r.getCentre());
            if (k.tipus == TipusControl::Interruptor)
            {
                auto sw = kr.withSizeKeepingCentre (s * 0.34f, s * 0.8f);
                g.setColour (juce::Colours::black.withAlpha (0.5f));
                g.fillRoundedRectangle (sw.expanded (2.0f), s * 0.1f);
                g.setColour (juce::Colour (0xff18181a));
                g.fillRoundedRectangle (sw, s * 0.09f);
                const bool dalt = p->valor (i) > 0.5f;
                auto pal = sw.withHeight (sw.getHeight() * 0.48f).withY (dalt ? sw.getY() + 1.0f : sw.getCentreY() - 1.0f).reduced (1.5f, 0.0f);
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f2f4), pal.getX(), pal.getY(), juce::Colour (0xff77777e), pal.getRight(), pal.getBottom(), false));
                g.fillRoundedRectangle (pal, s * 0.07f);
                g.setColour (tx.withAlpha (0.85f));
                g.setFont (dibuix::lletra (juce::jmax (8.0f, s * 0.17f), true));
                g.drawText (u8 (k.opcions[1]), juce::Rectangle<float> (r.getWidth(), s * 0.2f).withCentre ({ r.getCentreX(), sw.getY() - s * 0.12f }), juce::Justification::centred);
                g.drawText (u8 (k.opcions[0]), juce::Rectangle<float> (r.getWidth(), s * 0.2f).withCentre ({ r.getCentreX(), sw.getBottom() + s * 0.12f }), juce::Justification::centred);
            }
            else
            {
                if (k.tipus == TipusControl::Selector)
                {
                    // marques dels passos
                    g.setColour (tx.withAlpha (0.7f));
                    const int n = (int) k.opcions.size();
                    for (int j = 0; j < n; ++j)
                    {
                        const float a = -0.75f * pi + (float) j / (float) juce::jmax (1, n - 1) * 1.5f * pi;
                        g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre (kr.getCentre().getPointOnCircumference (s * 0.6f, a)));
                    }
                }
                pintaPom (g, kr.reduced (s * 0.06f), norm(), estilPomPedal (m), col (m.colorPom));
            }
            g.setColour (tx);
            g.setFont (dibuix::lletra (juce::jmax (8.5f, etiqueta.getHeight() * 0.48f), true));
            const bool mostraValor = arrossega || sobre || k.tipus == TipusControl::Selector;
            g.drawFittedText (mostraValor && k.tipus != TipusControl::Interruptor ? u8 (p->text (i, p->valor (i))) : dibuix::majuscules (u8 (k.nom)),
                              etiqueta.toNearestInt(), juce::Justification::centredTop, 1, 0.7f);
        }

        void mouseEnter (const juce::MouseEvent&) override { sobre = true; repaint(); }
        void mouseExit (const juce::MouseEvent&) override { sobre = false; repaint(); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            v0 = norm();
            mogut = false;
            const auto& k = control();
            if (k.tipus == TipusControl::Interruptor) { posa (p->valor (i) > 0.5f ? 0.0f : 1.0f); return; }
            if (k.tipus == TipusControl::Selector && e.mods.isPopupMenu()) { pas (-1); return; }
            arrossega = true;
            repaint();
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            const auto& k = control();
            if (k.tipus == TipusControl::Interruptor || ! arrossega) return;
            if (std::abs (e.getDistanceFromDragStartY()) > 2) mogut = true;
            const float rang = k.tipus == TipusControl::Pedal ? (float) getHeight() * 0.8f : 180.0f;
            const float v = juce::jlimit (0.0f, 1.0f, v0 - (float) e.getDistanceFromDragStartY() / (rang * (e.mods.isShiftDown() ? 5.0f : 1.0f)));
            posa (Pedal::deNormal (k, v));
        }
        void mouseUp (const juce::MouseEvent& e) override
        {
            const auto& k = control();
            if (k.tipus == TipusControl::Selector && ! mogut && ! e.mods.isPopupMenu()) pas (1);
            arrossega = false;
            repaint();
        }
        void mouseDoubleClick (const juce::MouseEvent&) override { posa (control().def); }
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
        {
            const auto& k = control();
            if (k.tipus == TipusControl::Selector || k.tipus == TipusControl::Interruptor) { pas (w.deltaY > 0 ? 1 : -1); return; }
            posa (Pedal::deNormal (k, juce::jlimit (0.0f, 1.0f, norm() + w.deltaY * 0.08f)));
        }

    private:
        void pas (int d)
        {
            const auto& k = control();
            const int n = (int) k.opcions.size();
            if (n == 0) return;
            posa ((float) ((((int) std::lround (p->valor (i)) + d) % n + n) % n));
        }
        void posa (float v)
        {
            p->posa (i, v);
            repaint();
            if (auto* pare = getParentComponent()) pare->repaint();
            if (canviat) canviat();
        }
        std::shared_ptr<Pedal> p;
        int i;
        std::function<void()> canviat;
        float v0 = 0.0f;
        bool arrossega = false, sobre = false, mogut = false;
    };

    // ============================================================================================ un pedal petit (a la pedalera)
    class PedalPetit : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        PedalPetit (Accions& a, std::shared_ptr<Pedal> ped, int index) : acc (a), p (std::move (ped)), k (index)
        {
            setTooltip (u8 (p->model().nom) + " (" + "inspirat en "_ca + u8 (p->model().inspirat) + ")\n"
                        + "Clic: obre'l · Clic a l'interruptor de peu: encén o apaga · Arrossega'l per canviar l'ordre · Botó dret: més opcions"_ca);
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }
        Geometria geo() const { return geometriaDins (p->model(), getLocalBounds().toFloat().reduced (2.0f)); }
        void paint (juce::Graphics& g) override
        {
            if (arrossegant) g.setOpacity (0.35f);
            pintaPedal (g, p->model(), geo(), p.get(), p->on.load(), p->activitat(), true, false);
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            arrossegant = false;
            if (e.mods.isPopupMenu()) { acc.menuPedal (k); return; }
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu() || arrossegant || e.getDistanceFromDragStart() < 6) return;
            if (auto* d = acc.dnd())
            {
                arrossegant = true;
                repaint();
                auto img = createComponentSnapshot (getLocalBounds(), true, 1.0f);
                d->startDragging ("pb:" + juce::String (k), this, juce::ScaledImage (img), true);
            }
        }
        void mouseUp (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu()) return;
            if (arrossegant) { arrossegant = false; repaint(); return; }
            const auto q = geo();
            const bool peu = p->model().forma == Forma::Basculant ? false : q.peu.contains (e.position);
            if (peu)
            {
                p->on.store (! p->on.load());
                repaint();
                acc.canviat();
                return;
            }
            acc.obrePedal (k, acc.arrel().getLocalArea (this, getLocalBounds()));
        }
        juce::Point<float> entrada (juce::Component& on) const
        {
            const auto q = geo();
            return on.getLocalPoint (this, juce::Point<float> (q.cos.getX() + 1.0f, q.cos.getY() + q.cos.getHeight() * 0.32f));
        }
        juce::Point<float> sortida (juce::Component& on) const
        {
            const auto q = geo();
            return on.getLocalPoint (this, juce::Point<float> (q.cos.getRight() - 1.0f, q.cos.getY() + q.cos.getHeight() * 0.32f));
        }
        Accions& acc;
        std::shared_ptr<Pedal> p;
        int k;
        bool arrossegant = false;
    };

    // ============================================================================================ la pedalera
    class Pedalera : public juce::Component, public juce::DragAndDropTarget
    {
    public:
        explicit Pedalera (Accions& a) : acc (a) {}

        void refesca()
        {
            petits.clear();
            auto pl = acc.rack().placa();
            for (int k = 0; k < (int) pl->pedals.size(); ++k)
            {
                auto c = std::make_unique<PedalPetit> (acc, pl->pedals[(size_t) k], k);
                addAndMakeVisible (*c);
                petits.push_back (std::move (c));
            }
            placaVista = pl.get();
            resized();
            repaint();
        }
        const Placa* placaVista = nullptr;
        std::vector<std::unique_ptr<PedalPetit>> petits;

        bool esLoop (int k) const { auto pl = acc.rack().placa(); return k < (int) pl->loop.size() && pl->loop[(size_t) k]; }

        void resized() override
        {
            auto r = superficie();
            int nPre = 0, nLoop = 0;
            for (int k = 0; k < (int) petits.size(); ++k) (esLoop (k) ? nLoop : nPre)++;
            const float ampleLoop = nLoop == 0 ? r.getWidth() * 0.2f : r.getWidth() * juce::jlimit (0.24f, 0.55f, 0.08f + 0.12f * (float) nLoop);
            divisio = r.getRight() - ampleLoop;
            colloca (juce::Rectangle<float> (r.getX() + 34.0f, r.getY(), divisio - r.getX() - 44.0f, r.getHeight()), false);
            colloca (juce::Rectangle<float> (divisio + 12.0f, r.getY(), r.getRight() - divisio - 46.0f, r.getHeight()), true);
        }

        juce::Rectangle<float> superficie() const { return getLocalBounds().toFloat().reduced (14.0f, 16.0f).withTrimmedTop (10.0f); }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (4.0f);
            // marc d'alumini i superfície de velcro
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillRoundedRectangle (r.translated (0, 5), 10.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8d8f96), r.getX(), r.getY(), juce::Colour (0xff3a3b40), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 10.0f);
            auto s = r.reduced (8.0f);
            g.setColour (juce::Colour (0xff17171a));
            g.fillRoundedRectangle (s, 6.0f);
            g.setColour (juce::Colours::white.withAlpha (0.025f));
            for (float y = s.getY() + 3.0f; y < s.getBottom(); y += 5.0f) g.drawHorizontalLine ((int) y, s.getX() + 4.0f, s.getRight() - 4.0f);
            // separació del loop
            g.setColour (juce::Colour (0xff2c2c32));
            g.fillRect (divisio - 2.0f, s.getY() + 8.0f, 3.0f, s.getHeight() - 16.0f);
            g.setColour (tema::suau.withAlpha (0.8f));
            g.setFont (dibuix::lletra (10.5f, true));
            g.drawText ("PEDALERA  ·  ABANS DE L'AMPLI"_ca, juce::Rectangle<float> (s.getX() + 30.0f, s.getY() + 2.0f, divisio - s.getX() - 40.0f, 14.0f), juce::Justification::centredLeft);
            g.drawText ("LOOP  ·  DESPRÉS DE LA PANTALLA"_ca, juce::Rectangle<float> (divisio + 10.0f, s.getY() + 2.0f, s.getRight() - divisio - 16.0f, 14.0f), juce::Justification::centredLeft);
            int nPre = 0, nLoop = 0;
            for (int k = 0; k < (int) petits.size(); ++k) (esLoop (k) ? nLoop : nPre)++;
            g.setColour (tema::suau.withAlpha (0.55f));
            g.setFont (dibuix::lletra (12.0f));
            if (nPre == 0) g.drawFittedText ("Arrossega aquí pedals de la biblioteca (a baix)."_ca, juce::Rectangle<float> (s.getX() + 40, s.getY() + 20, divisio - s.getX() - 60, s.getHeight() - 30).toNearestInt(), juce::Justification::centred, 2);
            if (nLoop == 0) g.drawFittedText ("Delays i reverbs aquí: sonen després de la pantalla, com al loop d'efectes."_ca, juce::Rectangle<float> (divisio + 10, s.getY() + 20, s.getRight() - divisio - 50, s.getHeight() - 30).toNearestInt(), juce::Justification::centred, 3);
            // jacks d'entrada (guitarra) i de sortida (cap a Mibase)
            pintaJack (g, jackGuitarra(), 7.0f);
            pintaJack (g, jackSortida(), 7.0f);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (9.0f, true));
            g.drawText ("GUITARRA", juce::Rectangle<float> (70, 12).withCentre (jackGuitarra().translated (14, 20)), juce::Justification::centred);
            g.drawText ("SORTIDA", juce::Rectangle<float> (60, 12).withCentre (jackSortida().translated (-10, 20)), juce::Justification::centred);
            // on cauria el pedal que s'arrossega
            if (marca >= 0.0f)
            {
                g.setColour (tema::accent);
                g.fillRoundedRectangle (marca - 2.0f, s.getY() + 18.0f, 4.0f, s.getHeight() - 28.0f, 2.0f);
            }
        }
        juce::Point<float> jackGuitarra() const { auto s = superficie(); return { s.getX() + 8.0f, s.getCentreY() - 6.0f }; }
        juce::Point<float> jackSortida() const { auto s = superficie(); return { s.getRight() - 10.0f, s.getCentreY() - 6.0f }; }

        // ---- arrossegar i deixar anar ----
        bool isInterestedInDragSource (const SourceDetails& d) override
        {
            const auto t = d.description.toString();
            return t.startsWith ("lib:") || t.startsWith ("pb:");
        }
        void itemDragMove (const SourceDetails& d) override
        {
            const auto x = (float) d.localPosition.x;
            const bool loop = x > divisio;
            // la ratlla: entre els dos pedals més propers del grup
            float m = loop ? divisio + 8.0f : 40.0f;
            for (int k = 0; k < (int) petits.size(); ++k)
                if (esLoop (k) == loop && (float) petits[(size_t) k]->getBounds().getCentreX() < x) m = (float) petits[(size_t) k]->getRight() + 3.0f;
            marca = m;
            repaint();
        }
        void itemDragExit (const SourceDetails&) override { marca = -1.0f; repaint(); }
        void itemDropped (const SourceDetails& d) override
        {
            marca = -1.0f;
            const auto t = d.description.toString();
            const float x = (float) d.localPosition.x;
            const bool loop = x > divisio;
            if (t.startsWith ("lib:"))
            {
                if (acc.rack().numPedals() >= maxPedals) { juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Guitar Rack", "Hi caben com a molt "_ca + juce::String (maxPedals) + " pedals."_ca); return; }
                acc.rack().afegeix (t.fromFirstOccurrenceOf ("lib:", false, false).toStdString(), destinacio (x, loop, -1), loop);
            }
            else
            {
                const int de = t.fromFirstOccurrenceOf ("pb:", false, false).getIntValue();
                acc.rack().mou (de, destinacio (x, loop, de), loop);
            }
            acc.canviat();
        }

    private:
        int destinacio (float x, bool loop, int excloure) const
        {
            int idx = 0;
            for (int k = 0; k < (int) petits.size(); ++k)
            {
                if (k == excloure) continue;
                const bool l = esLoop (k);
                if (! loop) { if (! l && (float) petits[(size_t) k]->getBounds().getCentreX() < x) ++idx; }
                else { if (! l) ++idx; else if ((float) petits[(size_t) k]->getBounds().getCentreX() < x) ++idx; }
            }
            return idx;
        }
        void colloca (juce::Rectangle<float> z, bool loop)
        {
            std::vector<PedalPetit*> v;
            for (int k = 0; k < (int) petits.size(); ++k) if (esLoop (k) == loop) v.push_back (petits[(size_t) k].get());
            if (v.empty()) return;
            const float gap = 26.0f;
            float h = juce::jmin (z.getHeight() - 14.0f, 190.0f), total = 0.0f;
            for (auto* c : v) total += h * proporcio (c->p->model().forma) + gap;
            if (total > z.getWidth()) h *= z.getWidth() / total;
            float x = z.getX();
            for (auto* c : v)
            {
                const float w = h * proporcio (c->p->model().forma) + 4.0f;
                c->setBounds (juce::Rectangle<float> (x, z.getY() + 14.0f + (z.getHeight() - 14.0f - h) * 0.5f, w, h + 4.0f).toNearestInt());
                x += w + gap - 4.0f;
            }
        }
        Accions& acc;
        float divisio = 0.0f, marca = -1.0f;
    };
}

namespace
{
    // ============================================================================================ l'amplificador
    class PomAmpli : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        PomAmpli (Accions& a, int parametre, const juce::String& nom, int estil, juce::Colour text, juce::Colour cos)
            : acc (a), par (parametre), est (estil), etiqueta (nom), tx (text), cs (cos)
        {
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
            setTooltip (nom + ": " + u8 (infoFix (par).ajuda) + "\n"_ca + "Arrossega amunt o avall (Maj: fi). Doble clic: el valor de sempre del model."_ca);
        }
        float valor() const { return acc.rack().valor (par); }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (tx);
            g.setFont (dibuix::lletra (juce::jlimit (8.0f, 12.0f, r.getHeight() * 0.13f), true));
            g.drawFittedText (dibuix::majuscules (etiqueta), r.removeFromTop (juce::jmax (12.0f, r.getHeight() * 0.18f)).toNearestInt(), juce::Justification::centred, 1, 0.6f);
            auto vr = r.removeFromBottom (juce::jmax (11.0f, r.getHeight() * 0.18f));
            const float s = juce::jmin (r.getWidth() * 0.86f, r.getHeight());
            pintaPom (g, juce::Rectangle<float> (s, s).withCentre (r.getCentre()), valor() / 10.0f, est, cs);
            if (arrossega || sobre)
            {
                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.fillRoundedRectangle (vr.withSizeKeepingCentre (juce::jmin (vr.getWidth(), 34.0f), vr.getHeight()), 3.0f);
                g.setColour (tema::guitarra);
                g.setFont (dibuix::lletra (10.0f, true));
                g.drawText (juce::String (valor(), 1).replaceCharacter ('.', ','), vr, juce::Justification::centred);
            }
        }
        void mouseEnter (const juce::MouseEvent&) override { sobre = true; repaint(); }
        void mouseExit (const juce::MouseEvent&) override { sobre = false; repaint(); }
        void mouseDown (const juce::MouseEvent&) override { v0 = valor(); arrossega = true; repaint(); }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            posa (juce::jlimit (0.0f, 10.0f, v0 - (float) e.getDistanceFromDragStartY() / (e.mods.isShiftDown() ? 90.0f : 18.0f)));
        }
        void mouseUp (const juce::MouseEvent&) override { arrossega = false; repaint(); }
        void mouseDoubleClick (const juce::MouseEvent&) override
        {
            const auto& a = amplis()[(size_t) acc.rack().ampli()];
            posa (a.def[(size_t) (par - AmpGain)]);
        }
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override { posa (juce::jlimit (0.0f, 10.0f, valor() + w.deltaY * 1.2f)); }
        int parametre() const { return par; }
    private:
        void posa (float v) { acc.rack().posa (par, v); repaint(); acc.canviat(); }
        Accions& acc;
        int par, est;
        juce::String etiqueta;
        juce::Colour tx, cs;
        float v0 = 0.0f;
        bool arrossega = false, sobre = false;
    };

    /** Un interruptor de palanca (encès/apagat) amb el nom a sobre. */
    class Palanca : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        Palanca (const juce::String& n, juce::Colour text) : nom (n), tx (text) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
        std::function<bool()> llegeix;
        std::function<void (bool)> posa;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (tx);
            g.setFont (dibuix::lletra (juce::jlimit (8.0f, 11.0f, r.getHeight() * 0.14f), true));
            g.drawFittedText (dibuix::majuscules (nom), r.removeFromTop (14.0f).toNearestInt(), juce::Justification::centred, 1, 0.6f);
            const bool on = llegeix ? llegeix() : false;
            const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.7f;
            auto b = juce::Rectangle<float> (s * 0.6f, s).withCentre (r.getCentre());
            g.setColour (juce::Colour (0xff0d0d0e));
            g.fillRoundedRectangle (b, 4.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd8d8dc), b.getX(), b.getY(), juce::Colour (0xff5d5d63), b.getRight(), b.getBottom(), false));
            auto pal = b.reduced (b.getWidth() * 0.22f, b.getHeight() * 0.08f).withHeight (b.getHeight() * 0.5f);
            if (! on) pal = pal.withY (b.getCentreY() - b.getHeight() * 0.04f);
            g.fillRoundedRectangle (pal, 3.0f);
            g.setColour (tx.withAlpha (0.7f));
            g.setFont (dibuix::lletra (8.5f, true));
            g.drawText (on ? "ON" : "OFF", r.removeFromBottom (12.0f), juce::Justification::centred);
        }
        void mouseUp (const juce::MouseEvent& e) override { if (getLocalBounds().contains (e.getPosition()) && posa && llegeix) { posa (! llegeix()); repaint(); } }
    private:
        juce::String nom;
        juce::Colour tx;
    };

    class VistaAmpli : public juce::Component
    {
    public:
        explicit VistaAmpli (Accions& a) : acc (a) {}

        void refesca()
        {
            const auto& m = model();
            poms.clear();
            canals.clear();
            palanques.clear();
            for (int c = 0; c < numAmpCtl; ++c)
            {
                if (m.etiqueta[(size_t) c].empty()) continue;
                auto p = std::make_unique<PomAmpli> (acc, AmpGain + c, u8 (m.etiqueta[(size_t) c]), estilPomAmpli (m.estilPom), col (m.textPanell),
                                                     m.estilPom == 2 ? juce::Colour (0xfff0e6cc) : juce::Colour (0xff151515));
                addAndMakeVisible (*p);
                poms.push_back (std::move (p));
            }
            for (int c = 0; c < (int) m.canals.size(); ++c)
            {
                auto b = std::make_unique<Boto> (u8 (m.canals[(size_t) c]), std::nullopt, Boto::Estil::Tecla);
                b->setLlum (juce::Colour (0xff4cff6a));
                b->setMidaText (10.5f);
                b->setTooltip ("Canal "_ca + u8 (m.canals[(size_t) c]));
                b->onClick = [this, c] { acc.rack().posa (Canal, (float) c); acc.canviat(); actualitza(); };
                addAndMakeVisible (*b);
                canals.push_back (std::move (b));
            }
            if (m.interruptor != Interruptor::Cap)
            {
                auto p = std::make_unique<Palanca> (u8 (m.nomInterruptor), col (m.textPanell));
                p->setTooltip (u8 (m.nomInterruptor) + ": "_ca + (m.interruptor == Interruptor::Bright ? "més aguts a l'entrada."_ca
                                                                  : m.interruptor == Interruptor::Boost ? "més guany i més ajustat."_ca
                                                                  : m.interruptor == Interruptor::Tight ? "greus més ajustats (riffs ràpids)."_ca : "mitjos buidats."_ca));
                p->llegeix = [this] { return acc.rack().valor (BrightP) > 0.5f; };
                p->posa = [this] (bool v) { acc.rack().posa (BrightP, v ? 1.0f : 0.0f); acc.canviat(); };
                addAndMakeVisible (*p);
                palanques.push_back (std::move (p));
            }
            {
                auto p = std::make_unique<Palanca> ("Power", col (m.textPanell));
                p->setTooltip ("Encén o apaga l'amplificador (apagat, el so passa directe a la pantalla)."_ca);
                p->llegeix = [this] { return acc.rack().valor (AmpOn) > 0.5f; };
                p->posa = [this] (bool v) { acc.rack().posa (AmpOn, v ? 1.0f : 0.0f); acc.canviat(); repaint(); };
                addAndMakeVisible (*p);
                palanques.push_back (std::move (p));
            }
            resized();
            actualitza();
            repaint();
        }
        void actualitza()
        {
            const int c = (int) acc.rack().valor (Canal);
            for (int i = 0; i < (int) canals.size(); ++i) canals[(size_t) i]->setToggleState (i == c, juce::dontSendNotification);
            for (auto& p : poms) p->repaint();
            for (auto& p : palanques) p->repaint();
        }

        const ModelAmp& model() const { return amplis()[(size_t) acc.rack().ampli()]; }

        void resized() override
        {
            const auto f = faceplate();
            auto r = f.reduced (f.getHeight() * 0.08f, f.getHeight() * 0.06f);
            r.removeFromLeft (f.getHeight() * 0.55f);   // jack d'entrada
            if (! canals.empty())
            {
                auto c = r.removeFromLeft (juce::jmax (64.0f, f.getWidth() * 0.1f));
                const float h = juce::jmin (26.0f, c.getHeight() / (float) canals.size() - 3.0f);
                float y = c.getCentreY() - (h + 3.0f) * (float) canals.size() * 0.5f;
                for (auto& b : canals) { b->setBounds (juce::Rectangle<float> (c.getX(), y, c.getWidth() - 6.0f, h).toNearestInt()); y += h + 3.0f; }
            }
            auto dreta = r.removeFromRight (r.getHeight() * 0.62f * (float) palanques.size() + r.getHeight() * 0.35f);
            for (auto& p : palanques) p->setBounds (dreta.removeFromLeft (r.getHeight() * 0.62f).toNearestInt());
            pilot = dreta.withSizeKeepingCentre (r.getHeight() * 0.3f, r.getHeight() * 0.3f);
            const float w = r.getWidth() / (float) juce::jmax (1, (int) poms.size());
            for (size_t i = 0; i < poms.size(); ++i) poms[i]->setBounds (juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight()).reduced (2.0f, 0.0f).toNearestInt());
        }

        juce::Rectangle<float> cos() const
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            r.removeFromTop (r.getHeight() * 0.1f);
            return r;
        }
        juce::Rectangle<float> faceplate() const
        {
            auto c = cos().reduced (cos().getHeight() * 0.06f);
            return c.removeFromBottom (c.getHeight() * 0.5f);
        }
        juce::Point<float> jackEntrada() const { const auto f = faceplate(); return { f.getX() + f.getHeight() * 0.32f, f.getCentreY() + f.getHeight() * 0.12f }; }
        juce::Point<float> jackAltaveu() const { const auto c = cos(); return { c.getRight() - 6.0f, c.getCentreY() - c.getHeight() * 0.15f }; }

        void paint (juce::Graphics& g) override
        {
            const auto& m = model();
            auto c = cos();
            // nansa
            auto nansa = juce::Rectangle<float> (c.getWidth() * 0.24f, getLocalBounds().toFloat().getHeight() * 0.09f).withCentre ({ c.getCentreX(), c.getY() - 4.0f });
            g.setColour (juce::Colour (0xff141414));
            g.fillRoundedRectangle (nansa, nansa.getHeight() * 0.45f);
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            g.drawRoundedRectangle (nansa.reduced (2.0f), nansa.getHeight() * 0.4f, 1.0f);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillRoundedRectangle (c.translated (0, 6), 10.0f);
            pintaTolex (g, c, col (m.tolex), 10.0f);
            auto in = c.reduced (c.getHeight() * 0.06f);
            auto reixa = in.removeFromTop (in.getHeight() * 0.5f).reduced (0, 2);
            pintaReixa (g, reixa, col (m.reixa));
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.drawRect (reixa, 2.0f);
            // logotip
            const auto clar = col (m.panell).getPerceivedBrightness() > 0.35f ? col (m.panell) : juce::Colour (0xffeeeeee);
            const auto colorLogo = col (m.reixa).getPerceivedBrightness() > 0.6f ? juce::Colour (0xff1a1a1a) : clar;
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.setFont (juce::Font (juce::FontOptions (reixa.getHeight() * 0.42f, juce::Font::bold | juce::Font::italic)));
            g.drawFittedText (u8 (m.nom), reixa.toNearestInt().translated (2, 3), juce::Justification::centred, 1, 0.5f);
            g.setColour (colorLogo);
            g.drawFittedText (u8 (m.nom), reixa.toNearestInt(), juce::Justification::centred, 1, 0.5f);
            g.setFont (dibuix::lletra (juce::jmax (8.0f, reixa.getHeight() * 0.1f), true));
            g.setColour (colorLogo.withAlpha (0.75f));
            g.drawText ("MIBASE  ·  "_ca + dibuix::majuscules (u8 (m.origen)), reixa.withTrimmedTop (reixa.getHeight() * 0.72f), juce::Justification::centred);
            // panell de controls
            auto f = faceplate();
            const auto pc = col (m.panell);
            g.setGradientFill (juce::ColourGradient (pc.brighter (0.25f), f.getX(), f.getY(), pc.darker (0.3f), f.getX(), f.getBottom(), false));
            g.fillRoundedRectangle (f, 4.0f);
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            for (float x = f.getX(); x < f.getRight(); x += 2.0f) g.drawVerticalLine ((int) x, f.getY() + 1, f.getBottom() - 1);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.drawRoundedRectangle (f, 4.0f, 1.5f);
            pintaJack (g, jackEntrada(), juce::jmax (5.0f, f.getHeight() * 0.09f));
            g.setColour (col (m.textPanell));
            g.setFont (dibuix::lletra (9.0f, true));
            g.drawText ("INPUT", juce::Rectangle<float> (50, 12).withCentre (jackEntrada().translated (0, -f.getHeight() * 0.2f)), juce::Justification::centred);
            // llum pilot (joia vermella): s'encén amb l'ampli i brilla amb el so
            const bool on = acc.rack().valor (AmpOn) > 0.5f;
            const float niv = nivellPilot;
            pintaLed (g, pilot.getCentre(), pilot.getWidth() * 0.45f, juce::Colour (0xffff2a1a), on ? 0.6f + 0.4f * niv : 0.0f);
            pintaJack (g, jackAltaveu(), 5.0f);
        }
        float nivellPilot = 0.0f;
        juce::Rectangle<float> pilot;

    private:
        Accions& acc;
        std::vector<std::unique_ptr<PomAmpli>> poms;
        std::vector<std::unique_ptr<Boto>> canals;
        std::vector<std::unique_ptr<Palanca>> palanques;
    };

    // ============================================================================================ la pantalla
    class VistaPantalla : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit VistaPantalla (Accions& a) : acc (a)
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
            setTooltip ("Clic: obre la pantalla per triar l'altaveu i col·locar els micròfons."_ca);
        }
        juce::Rectangle<float> cos() const
        {
            auto r = getLocalBounds().toFloat().reduced (6.0f);
            const float s = juce::jmin (r.getWidth(), r.getHeight() * 1.05f);
            return r.withSizeKeepingCentre (s, juce::jmin (r.getHeight(), s * 0.95f)).withBottomY (r.getBottom());
        }
        std::vector<juce::Rectangle<float>> altaveus (juce::Rectangle<float> reixa) const
        {
            const auto& c = cabs()[(size_t) acc.rack().cab()];
            std::vector<juce::Rectangle<float>> v;
            if (c.altaveus == 1) v.push_back (reixa.withSizeKeepingCentre (reixa.getHeight() * 0.8f, reixa.getHeight() * 0.8f));
            else if (c.altaveus == 2)
            {
                const float d = juce::jmin (reixa.getWidth() * 0.45f, reixa.getHeight() * 0.85f);
                v.push_back (juce::Rectangle<float> (d, d).withCentre ({ reixa.getX() + reixa.getWidth() * 0.27f, reixa.getCentreY() }));
                v.push_back (juce::Rectangle<float> (d, d).withCentre ({ reixa.getX() + reixa.getWidth() * 0.73f, reixa.getCentreY() }));
            }
            else
            {
                const float d = juce::jmin (reixa.getWidth(), reixa.getHeight()) * 0.45f;
                for (int i = 0; i < 4; ++i)
                    v.push_back (juce::Rectangle<float> (d, d).withCentre ({ reixa.getX() + reixa.getWidth() * (i % 2 == 0 ? 0.27f : 0.73f),
                                                                              reixa.getY() + reixa.getHeight() * (i < 2 ? 0.27f : 0.73f) }));
            }
            return v;
        }
        juce::Point<float> jackEntrada() const { const auto c = cos(); return { c.getX() + 6.0f, c.getCentreY() - c.getHeight() * 0.15f }; }
        juce::Point<float> jackSortida() const { const auto c = cos(); return { c.getRight() - 14.0f, c.getBottom() - 14.0f }; }

        void paint (juce::Graphics& g) override
        {
            const auto& c = cabs()[(size_t) acc.rack().cab()];
            auto r = cos();
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillRoundedRectangle (r.translated (0, 6), 10.0f);
            pintaTolex (g, r, col (c.tolex), 10.0f);
            auto reixa = r.reduced (r.getWidth() * 0.07f);
            reixa.removeFromBottom (r.getHeight() * 0.04f);
            // els altaveus es veuen una mica a través de la tela
            pintaReixa (g, reixa, col (c.reixa));
            for (auto& a : altaveus (reixa))
            {
                g.setColour (juce::Colours::black.withAlpha (0.18f));
                g.fillEllipse (a);
                g.setColour (juce::Colours::black.withAlpha (0.12f));
                g.fillEllipse (a.withSizeKeepingCentre (a.getWidth() * 0.3f, a.getHeight() * 0.3f));
            }
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.drawRect (reixa, 2.0f);
            // placa amb el nom
            auto placa = juce::Rectangle<float> (reixa.getWidth() * 0.42f, juce::jmax (14.0f, reixa.getHeight() * 0.08f)).withCentre ({ reixa.getCentreX(), reixa.getY() + reixa.getHeight() * 0.07f });
            g.setColour (juce::Colour (0xff121212).withAlpha (0.85f));
            g.fillRoundedRectangle (placa, 3.0f);
            g.setColour (juce::Colour (0xffe6d7a6));
            g.setFont (dibuix::lletra (placa.getHeight() * 0.62f, true));
            g.drawFittedText (u8 (c.nom), placa.toNearestInt(), juce::Justification::centred, 1, 0.6f);
            // micròfons davant del primer altaveu
            auto a0 = altaveus (reixa)[0];
            auto mic = [&] (float pos, float dist, const juce::String& lletra, juce::Colour color)
            {
                const float rad = a0.getWidth() * 0.5f;
                const auto p = juce::Point<float> (a0.getCentreX() + pos * rad * 0.9f, a0.getCentreY() + (lletra == "B" ? rad * 0.25f : 0.0f));
                const float d = juce::jmax (8.0f, a0.getWidth() * (0.2f - 0.06f * dist));
                g.setColour (juce::Colours::black.withAlpha (0.45f));
                g.fillEllipse (juce::Rectangle<float> (d, d).withCentre (p.translated (2, 3)));
                g.setGradientFill (juce::ColourGradient (color.brighter (0.4f), p.x - d, p.y - d, color.darker (0.4f), p.x + d, p.y + d, false));
                g.fillEllipse (juce::Rectangle<float> (d, d).withCentre (p));
                g.setColour (juce::Colours::black.withAlpha (0.4f));
                for (float k = -0.3f; k <= 0.31f; k += 0.15f) g.drawLine (p.x - d * 0.4f, p.y + d * k, p.x + d * 0.4f, p.y + d * k, 0.8f);
                g.setColour (juce::Colours::white);
                g.setFont (dibuix::lletra (juce::jmax (8.0f, d * 0.55f), true));
                g.drawText (lletra, juce::Rectangle<float> (d, d).withCentre (p.translated (0, d * 0.95f)), juce::Justification::centred);
            };
            if (acc.rack().valor (CabOn) > 0.5f)
            {
                if (acc.rack().valor (MesclaB) > 0.5f) mic (acc.rack().valor (PosB), acc.rack().valor (DistB), "B", juce::Colour (0xff8a8f98));
                mic (acc.rack().valor (PosA), acc.rack().valor (DistA), "A", juce::Colour (0xff6c7078));
            }
            else
            {
                g.setColour (juce::Colours::black.withAlpha (0.55f));
                g.fillRect (reixa);
                g.setColour (tema::text);
                g.setFont (dibuix::lletra (13.0f, true));
                g.drawFittedText ("PANTALLA APAGADA"_ca, reixa.toNearestInt(), juce::Justification::centred, 2);
            }
            pintaJack (g, jackEntrada(), 5.0f);
            pintaJack (g, jackSortida(), 5.0f);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (8.5f, true));
            g.drawText ("MIC OUT", juce::Rectangle<float> (50, 10).withCentre (jackSortida().translated (-34, 0)), juce::Justification::centred);
        }
        void mouseUp (const juce::MouseEvent& e) override { if (getLocalBounds().contains (e.getPosition())) acc.obrePantalla(); }
    private:
        Accions& acc;
    };

    // ============================================================================================ la biblioteca
    class Miniatura : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        Miniatura (Accions& a, bool esAmpli, int index, std::function<void()> enPreferit) : acc (a), amp (esAmpli), i (index), preferit (std::move (enPreferit))
        {
            setMouseCursor (amp ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::DraggingHandCursor);
            if (amp)
            {
                const auto& m = amplis()[(size_t) i];
                setTooltip (u8 (m.nom) + " · inspirat en "_ca + u8 (m.inspirat) + "\n" + u8 (m.descripcio) + "\n"_ca + "Clic: posa aquest ampli · Botó dret: preferit"_ca);
            }
            else
            {
                const auto& m = pedals()[(size_t) i];
                setTooltip (u8 (m.nom) + " · inspirat en "_ca + u8 (m.inspirat) + "\n" + u8 (m.descripcio) + "\n"_ca
                            + "Arrossega'l a la pedalera (o doble clic) · Botó dret: preferit"_ca);
            }
        }
        std::string clauPreferit() const { return amp ? "a:" + amplis()[(size_t) i].clau : "p:" + pedals()[(size_t) i].clau; }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (3.0f);
            const bool triat = amp && acc.rack().ampli() == i;
            if (isMouseOver() || triat)
            {
                g.setColour (triat ? tema::triat : tema::sobre);
                g.fillRoundedRectangle (r, 6.0f);
                if (triat) { g.setColour (tema::accent); g.drawRoundedRectangle (r, 6.0f, 1.5f); }
            }
            auto nom = r.removeFromBottom (16.0f);
            r.reduce (4.0f, 4.0f);
            if (amp)
            {
                const auto& m = amplis()[(size_t) i];
                auto c = r.withSizeKeepingCentre (r.getWidth(), juce::jmin (r.getHeight(), r.getWidth() * 0.62f));
                pintaTolex (g, c, col (m.tolex), 4.0f);
                auto in = c.reduced (4.0f);
                auto rx = in.removeFromTop (in.getHeight() * 0.55f);
                pintaReixa (g, rx, col (m.reixa));
                g.setColour (col (m.panell));
                g.fillRect (in.reduced (0, 2));
                g.setColour (col (m.reixa).getPerceivedBrightness() > 0.6f ? juce::Colour (0xff1a1a1a) : (col (m.panell).getPerceivedBrightness() > 0.35f ? col (m.panell) : juce::Colours::white));
                g.setFont (juce::Font (juce::FontOptions (rx.getHeight() * 0.45f, juce::Font::bold | juce::Font::italic)));
                g.drawFittedText (u8 (m.nom), rx.toNearestInt(), juce::Justification::centred, 1, 0.5f);
                for (int k = 0; k < 5; ++k) pintaPom (g, juce::Rectangle<float> (in.getHeight() * 0.5f, in.getHeight() * 0.5f).withCentre ({ in.getX() + in.getWidth() * (0.2f + 0.15f * (float) k), in.getCentreY() }), 0.5f, estilPomAmpli (m.estilPom));
            }
            else
            {
                const auto& m = pedals()[(size_t) i];
                pintaPedal (g, m, geometriaDins (m, r), nullptr, true, 0.0f, true, false);
            }
            g.setColour (tema::textMig);
            g.setFont (dibuix::lletra (10.5f, true));
            g.drawFittedText (u8 (amp ? amplis()[(size_t) i].nom : pedals()[(size_t) i].nom), nom.toNearestInt(), juce::Justification::centred, 1, 0.7f);
            if (preferits().count (clauPreferit()))
            {
                juce::Path p;
                p.addStar ({ getWidth() - 12.0f, 12.0f }, 5, 3.0f, 7.0f, 0.0f);
                g.setColour (tema::guitarra);
                g.fillPath (p);
            }
        }
        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu()) { commutaPreferit (clauPreferit()); repaint(); if (preferit) preferit(); }
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (amp || e.mods.isPopupMenu() || e.getDistanceFromDragStart() < 6 || arrossegant) return;
            if (auto* d = acc.dnd())
            {
                arrossegant = true;
                auto img = createComponentSnapshot (getLocalBounds(), true, 1.0f);
                d->startDragging ("lib:" + u8 (pedals()[(size_t) i].clau), this, juce::ScaledImage (img), true);
            }
        }
        void mouseUp (const juce::MouseEvent& e) override
        {
            const bool eraArr = arrossegant;
            arrossegant = false;
            if (e.mods.isPopupMenu() || eraArr || ! amp) return;
            if (acc.rack().ampli() != i)
            {
                acc.rack().posaAmpli (i);
                acc.canviat();
            }
        }
        void mouseDoubleClick (const juce::MouseEvent&) override
        {
            if (amp) return;
            const auto& m = pedals()[(size_t) i];
            if (acc.rack().numPedals() >= maxPedals) return;
            acc.rack().afegeix (m.clau, -1, m.categoria == CatDelay || m.categoria == CatReverb);
            acc.canviat();
        }
    private:
        Accions& acc;
        bool amp;
        int i;
        std::function<void()> preferit;
        bool arrossegant = false;
    };

    class Biblioteca : public juce::Component
    {
    public:
        explicit Biblioteca (Accions& a) : acc (a)
        {
            juce::StringArray noms { "Tots", "Preferits", "Amplis" };
            for (int c = 0; c < numCategories; ++c) noms.add (u8 (nomCategoria (c)));
            for (int k = 0; k < noms.size(); ++k)
            {
                auto b = std::make_unique<Boto> (noms[k], std::nullopt, Boto::Estil::Subtil);
                b->onClick = [this, k] { tab = k; refes(); };
                addAndMakeVisible (*b);
                tabs.push_back (std::move (b));
            }
            addAndMakeVisible (cerca);
            cerca.setTextToShowWhenEmpty ("Busca: nom, marca, tipus, caràcter o estil (p. ex. «fuzz», «Boss», «shoegaze»)"_ca, tema::suau);
            cerca.onTextChange = [this] { refes(); };
            addAndMakeVisible (vista);
            vista.setViewedComponent (&contingut, false);
            vista.setScrollBarsShown (false, true);
            refes();
        }
        void refes()
        {
            for (int k = 0; k < (int) tabs.size(); ++k) tabs[(size_t) k]->setToggleState (k == tab, juce::dontSendNotification);
            items.clear();
            std::vector<std::string> paraules;
            {
                juce::StringArray t;
                t.addTokens (cerca.getText().trim(), " ", "");
                for (auto& w : t) if (w.isNotEmpty()) paraules.push_back (senseAccents (w.toStdString()));
            }
            auto coincideix = [&] (const std::string& text) { for (auto& w : paraules) if (text.find (w) == std::string::npos) return false; return true; };
            const bool buscant = ! paraules.empty();
            auto afegeix = [&] (bool amp, int i) { items.push_back (std::make_unique<Miniatura> (acc, amp, i, [this] { if (tab == 1) juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<Biblioteca> (this)] { if (sp) sp->refes(); }); })); };
            // Tots: els pedals (i, si busques, també els amplis) · Preferits: amplis i pedals marcats · Amplis · cada categoria de pedals
            const bool ambAmplis = tab == 2 || tab == 1 || (tab == 0 && buscant);
            const bool ambPedals = tab != 2;
            if (ambAmplis)
                for (int i = 0; i < (int) amplis().size(); ++i)
                {
                    if (tab == 1 && ! preferits().count ("a:" + amplis()[(size_t) i].clau)) continue;
                    if (! coincideix (textCerca (amplis()[(size_t) i]))) continue;
                    afegeix (true, i);
                }
            if (ambPedals)
                for (int i = 0; i < (int) pedals().size(); ++i)
                {
                    const auto& m = pedals()[(size_t) i];
                    if (tab == 1 && ! preferits().count ("p:" + m.clau)) continue;
                    if (tab >= 3 && m.categoria != tab - 3) continue;
                    if (! coincideix (textCerca (m))) continue;
                    afegeix (false, i);
                }
            for (auto& it : items) contingut.addAndMakeVisible (*it);
            resized();
            contingut.repaint();
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (10, 6);
            auto dalt = r.removeFromTop (26);
            cerca.setBounds (dalt.removeFromRight (juce::jmin (420, dalt.getWidth() / 3)).reduced (0, 1));
            dalt.removeFromRight (8);
            const int wt = juce::jmax (40, dalt.getWidth() / juce::jmax (1, (int) tabs.size()));
            for (auto& t : tabs) t->setBounds (dalt.removeFromLeft (juce::jmin (wt, 108)));
            r.removeFromTop (4);
            vista.setBounds (r);
            const int h = juce::jmax (60, r.getHeight() - 12), w = (int) (h * 0.72f);
            contingut.setSize (juce::jmax (r.getWidth(), (int) items.size() * (w + 4)), h);
            for (int i = 0; i < (int) items.size(); ++i) items[(size_t) i]->setBounds (i * (w + 4), 0, w, h);
        }
        void paint (juce::Graphics& g) override
        {
            g.setColour (tema::lateral);
            g.fillRect (getLocalBounds());
            g.setColour (tema::linia);
            g.drawHorizontalLine (0, 0.0f, (float) getWidth());
            if (items.empty())
            {
                g.setColour (tema::suau);
                g.setFont (dibuix::lletra (12.0f));
                g.drawText (tab == 1 ? "Encara no tens preferits: botó dret sobre un pedal o un ampli per marcar-lo."_ca : "No hi ha res amb aquesta cerca."_ca,
                            getLocalBounds().withTrimmedTop (36), juce::Justification::centred);
            }
        }
        void refrescaAmplis() { for (auto& it : items) it->repaint(); }
    private:
        Accions& acc;
        std::vector<std::unique_ptr<Boto>> tabs;
        juce::TextEditor cerca;
        juce::Viewport vista;
        juce::Component contingut;
        std::vector<std::unique_ptr<Miniatura>> items;
        int tab = 0;
    };
}

namespace
{
    // ============================================================================================ el pedal ampliat
    class PedalGran : public juce::Component, private juce::Timer
    {
    public:
        PedalGran (Accions& a, std::shared_ptr<Pedal> ped, juce::Rectangle<float> desDe) : acc (a), p (std::move (ped)), origen (desDe)
        {
            const auto& m = p->model();
            for (int i = 0; i < (int) m.controls.size(); ++i)
            {
                auto c = std::make_unique<ControlPedal> (p, i, [this] { acc.canviat(); });
                addChildComponent (*c);
                controls.push_back (std::move (c));
            }
            for (auto* b : { &btTanca, &btTreu, &btLoop, &btDefecte }) addChildComponent (b);
            btTanca.setTooltip ("Torna a la pedalera (també: clic fora del pedal o Esc)"_ca);
            btTanca.onClick = [this] { tanca(); };
            btTreu.onClick = [this]
            {
                const int k = index();
                if (k >= 0) { acc.rack().treu (k); acc.canviat(); }
                tanca();
            };
            btLoop.setClickingTogglesState (false);
            btLoop.setTooltip ("Encès: el pedal sona després de la pantalla, com al loop d'efectes d'un ampli (ideal per a delays i reverbs)."_ca);
            btLoop.onClick = [this]
            {
                const int k = index();
                if (k >= 0) { acc.rack().posaLoop (k, ! p->loop.load()); acc.canviat(); }
                btLoop.setToggleState (p->loop.load(), juce::dontSendNotification);
            };
            btLoop.setToggleState (p->loop.load(), juce::dontSendNotification);
            btDefecte.setTooltip ("Torna tots els poms del pedal als valors de sempre."_ca);
            btDefecte.onClick = [this] { for (int i = 0; i < p->numControls(); ++i) p->posa (i, p->model().controls[(size_t) i].def); acc.canviat(); repaint(); for (auto& c : controls) c->repaint(); };
            setWantsKeyboardFocus (true);
            startTimerHz (60);
        }
        std::function<void()> enTancar;
        const Pedal* pedal() const { return p.get(); }
        int index() const
        {
            auto pl = acc.rack().placa();
            for (int k = 0; k < (int) pl->pedals.size(); ++k) if (pl->pedals[(size_t) k] == p) return k;
            return -1;
        }
        void tanca() { if (! tancant) { tancant = true; for (auto& c : controls) c->setVisible (false); for (auto* b : { &btTanca, &btTreu, &btLoop, &btDefecte }) b->setVisible (false); startTimerHz (60); } }
        bool keyPressed (const juce::KeyPress& k) override
        {
            if (k == juce::KeyPress::escapeKey) { tanca(); return true; }
            return cridaTeclaGlobal (k);
        }

        juce::Rectangle<float> desti() const
        {
            auto r = getLocalBounds().toFloat().reduced (20.0f);
            r.removeFromRight (r.getWidth() * 0.3f);
            const float pr = proporcio (p->model().forma);
            float h = juce::jmin (r.getHeight() * 0.94f, 620.0f), w = h * pr;
            if (w > r.getWidth() * 0.95f) { w = r.getWidth() * 0.95f; h = w / pr; }
            return juce::Rectangle<float> (w, h).withCentre (r.getCentre());
        }
        juce::Rectangle<float> actual() const
        {
            const float e = 1.0f - std::pow (1.0f - t, 3.0f);
            const auto d = desti();
            return { origen.getX() + (d.getX() - origen.getX()) * e, origen.getY() + (d.getY() - origen.getY()) * e,
                     origen.getWidth() + (d.getWidth() - origen.getWidth()) * e, origen.getHeight() + (d.getHeight() - origen.getHeight()) * e };
        }
        void resized() override
        {
            const auto q = geometria (p->model(), desti());
            for (int i = 0; i < (int) controls.size(); ++i)
            {
                auto z = q.controls[(size_t) i];
                if (p->model().controls[(size_t) i].tipus != TipusControl::Pedal) z = z.reduced (z.getWidth() * 0.04f, 0.0f);
                controls[(size_t) i]->setBounds (z.toNearestInt());
            }
            auto info = getLocalBounds().toFloat().reduced (20.0f);
            info = info.removeFromRight (info.getWidth() * 0.3f).reduced (10.0f, 0.0f);
            zonaInfo = info;
            auto b = info.removeFromBottom (36.0f * 4.0f + 12.0f);
            btTanca.setBounds (b.removeFromBottom (32.0f).toNearestInt());
            b.removeFromBottom (6.0f);
            btTreu.setBounds (b.removeFromBottom (32.0f).toNearestInt());
            b.removeFromBottom (6.0f);
            btDefecte.setBounds (b.removeFromBottom (32.0f).toNearestInt());
            b.removeFromBottom (6.0f);
            btLoop.setBounds (b.removeFromBottom (32.0f).toNearestInt());
        }
        void paint (juce::Graphics& g) override
        {
            const float e = 1.0f - std::pow (1.0f - t, 3.0f);
            g.setColour (juce::Colours::black.withAlpha (0.72f * e));
            g.fillAll();
            const auto& m = p->model();
            const bool ple = t >= 1.0f && ! tancant;
            pintaPedal (g, m, geometria (m, actual()), p.get(), p->on.load(), p->activitat(), ! ple, true);
            if (ple)
            {
                g.setColour (juce::Colour (0xf0141418));
                g.fillRoundedRectangle (zonaInfo.expanded (12.0f), 10.0f);
                g.setColour (tema::linia);
                g.drawRoundedRectangle (zonaInfo.expanded (12.0f), 10.0f, 1.0f);
                auto r = zonaInfo;
                g.setColour (tema::text);
                g.setFont (dibuix::lletra (24.0f, true));
                g.drawText (u8 (m.nom), r.removeFromTop (32.0f), juce::Justification::centredLeft);
                g.setColour (tema::accent);
                g.setFont (dibuix::lletra (12.5f, true));
                g.drawText ("Inspirat en "_ca + u8 (m.inspirat), r.removeFromTop (20.0f), juce::Justification::centredLeft);
                g.setColour (tema::suau);
                g.setFont (dibuix::lletra (11.0f, true));
                g.drawText (dibuix::majuscules (u8 (nomCategoria (m.categoria))), r.removeFromTop (18.0f), juce::Justification::centredLeft);
                r.removeFromTop (8.0f);
                g.setColour (tema::textMig);
                g.setFont (dibuix::lletra (13.0f));
                g.drawFittedText (u8 (m.descripcio), r.removeFromTop (60.0f).toNearestInt(), juce::Justification::topLeft, 4);
                g.setColour (tema::suau);
                g.setFont (dibuix::lletra (12.0f));
                g.drawFittedText ("Caràcter: "_ca + u8 (m.caracter) + "\n" + "Estils: "_ca + u8 (m.estils), r.removeFromTop (44.0f).toNearestInt(), juce::Justification::topLeft, 3);
                r.removeFromTop (10.0f);
                g.setColour (p->on.load() ? juce::Colour (0xffff6a5a) : tema::suau);
                g.setFont (dibuix::lletra (13.0f, true));
                g.drawText (p->on.load() ? "ENCÈS"_ca : "APAGAT (bypass)"_ca, r.removeFromTop (20.0f), juce::Justification::centredLeft);
                g.setColour (tema::suau);
                g.setFont (dibuix::lletra (11.5f));
                g.drawFittedText (m.forma == Forma::Basculant ? "Arrossega la planxa amunt i avall per moure el pedal. Per encendre'l o apagar-lo, fes servir el llum (clic)."_ca
                                                               : "Clica l'interruptor de peu per encendre'l o apagar-lo."_ca,
                                  r.removeFromTop (40.0f).toNearestInt(), juce::Justification::topLeft, 3);
            }
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (t < 1.0f || tancant) return;
            const auto q = geometria (p->model(), desti());
            const bool enLed = juce::Rectangle<float> (q.radiLed * 6.0f, q.radiLed * 6.0f).withCentre (q.led).contains (e.position);
            if ((p->model().forma != Forma::Basculant && q.peu.contains (e.position)) || enLed)
            {
                p->on.store (! p->on.load());
                acc.canviat();
                repaint();
                return;
            }
            if (! q.cos.contains (e.position) && ! zonaInfo.contains (e.position)) tanca();
        }
    private:
        void timerCallback() override
        {
            if (! tancant && t < 1.0f)
            {
                t = juce::jmin (1.0f, t + 1.0f / 11.0f);
                if (t >= 1.0f)
                {
                    for (auto& c : controls) c->setVisible (true);
                    for (auto* b : { &btTanca, &btTreu, &btLoop, &btDefecte }) b->setVisible (true);
                    grabKeyboardFocus();
                    startTimerHz (25);
                }
                repaint();
                return;
            }
            if (tancant)
            {
                t = juce::jmax (0.0f, t - 1.0f / 9.0f);
                repaint();
                if (t <= 0.0f) { stopTimer(); if (enTancar) juce::MessageManager::callAsync ([f = enTancar] { f(); }); }
                return;
            }
            const auto q = geometria (p->model(), desti());
            repaint (juce::Rectangle<float> (q.radiLed * 10.0f, q.radiLed * 10.0f).withCentre (q.led).toNearestInt());
        }
        Accions& acc;
        std::shared_ptr<Pedal> p;
        juce::Rectangle<float> origen, zonaInfo;
        std::vector<std::unique_ptr<ControlPedal>> controls;
        Boto btTanca { "Torna a la pedalera"_ca, std::nullopt, Boto::Estil::Primari };
        Boto btTreu { "Treu de la pedalera"_ca, std::nullopt, Boto::Estil::Secundari };
        Boto btDefecte { "Valors de sempre"_ca, std::nullopt, Boto::Estil::Secundari };
        Boto btLoop { "Al loop (després de la pantalla)"_ca, std::nullopt, Boto::Estil::Tecla };
        float t = 0.0f;
        bool tancant = false;
    };

    // ============================================================================================ la pantalla oberta: altaveu i micròfons
    class ZoomPantalla : public juce::Component
    {
    public:
        explicit ZoomPantalla (Accions& a) : acc (a)
        {
            for (int i = 0; i < (int) cabs().size(); ++i) cbCab.addItem (u8 (cabs()[(size_t) i].nom) + "  (" + u8 (cabs()[(size_t) i].inspirat) + ")", i + 1);
            for (int i = 0; i < (int) altaveus().size(); ++i) cbAlt.addItem (u8 (altaveus()[(size_t) i].nom) + "  (" + u8 (altaveus()[(size_t) i].inspirat) + ")", i + 1);
            cbCab.onChange = [this] { acc.rack().posaCab (cbCab.getSelectedId() - 1, true); acc.canviat(); actualitza(); };
            cbAlt.onChange = [this] { acc.rack().posaAltaveu (cbAlt.getSelectedId() - 1); acc.canviat(); actualitza(); };
            for (auto* c : { &cbCab, &cbAlt }) addAndMakeVisible (c);
            segA.onCanvi = [this] (int m) { acc.rack().posa (MicA, (float) m); acc.canviat(); repaint(); };
            segB.onCanvi = [this] (int m) { acc.rack().posa (MicB, (float) m); acc.canviat(); repaint(); };
            for (auto* s : { &segA, &segB }) addAndMakeVisible (s);
            auto pom = [this] (int p, const juce::String& nom)
            {
                const auto& f = infoFix (p);
                auto x = std::make_unique<Pom> (nom, (double) f.mn, (double) f.mx, (double) f.def, false,
                                                [this, p] (double v) { const auto& ff = infoFix (p); return u8 (acc.rack().textParametre (p, (float) ((v - ff.mn) / (ff.mx - ff.mn)))); });
                x->setTooltip (u8 (f.ajuda));
                x->onCanvi = [this, p] (double v) { acc.rack().posa (p, (float) v); acc.canviat(); repaint(); };
                x->onReinicia = [this, p] { acc.rack().posa (p, infoFix (p).def); actualitza(); acc.canviat(); };
                addAndMakeVisible (*x);
                poms.push_back ({ p, std::move (x) });
            };
            pom (DistA, "Distància A"_ca); pom (AngleA, "Angle A"); pom (DistB, "Distància B"_ca); pom (AngleB, "Angle B"); pom (MesclaB, "Mescla B"); pom (Room, "Sala");
            addAndMakeVisible (btOn);
            btOn.setTooltip ("Encén o apaga la pantalla. Apagada, se sent l'ampli directe (molt aspre): útil si després hi poses una pantalla de fora."_ca);
            btOn.onClick = [this] { acc.rack().posa (CabOn, acc.rack().valor (CabOn) > 0.5f ? 0.0f : 1.0f); acc.canviat(); actualitza(); };
            addAndMakeVisible (btTanca);
            btTanca.onClick = [this] { if (enTancar) enTancar(); };
            setWantsKeyboardFocus (true);
            actualitza();
        }
        std::function<void()> enTancar;
        void actualitza()
        {
            cbCab.setSelectedId (acc.rack().cab() + 1, juce::dontSendNotification);
            cbAlt.setSelectedId (acc.rack().altaveu() + 1, juce::dontSendNotification);
            segA.setTriat ((int) acc.rack().valor (MicA));
            segB.setTriat ((int) acc.rack().valor (MicB));
            btOn.setToggleState (acc.rack().valor (CabOn) > 0.5f, juce::dontSendNotification);
            for (auto& [p, x] : poms) x->setValor (acc.rack().valor (p), true);
            repaint();
        }
        bool keyPressed (const juce::KeyPress& k) override
        {
            if (k == juce::KeyPress::escapeKey) { if (enTancar) enTancar(); return true; }
            return cridaTeclaGlobal (k);
        }
        juce::Rectangle<float> con() const
        {
            auto r = getLocalBounds().toFloat().reduced (24.0f);
            r.removeFromRight (r.getWidth() * 0.42f);
            const float s = juce::jmin (r.getWidth(), r.getHeight()) * 0.92f;
            return juce::Rectangle<float> (s, s).withCentre (r.getCentre());
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (24);
            auto d = r.removeFromRight ((int) (r.getWidth() * 0.42f)).reduced (8, 0);
            btTanca.setBounds (d.removeFromBottom (32));
            d.removeFromBottom (8);
            auto fila = [&] (int h) { auto x = d.removeFromTop (h); d.removeFromTop (6); return x; };
            fila (18);
            cbCab.setBounds (fila (28));
            fila (14);
            cbAlt.setBounds (fila (28));
            fila (14);
            segA.setBounds (fila (28));
            fila (14);
            segB.setBounds (fila (28));
            auto pr = fila (juce::jmin (110, d.getHeight() / 2 - 20));
            auto pr2 = fila (juce::jmin (110, d.getHeight() - 40));
            const int w = pr.getWidth() / 3;
            for (int i = 0; i < (int) poms.size(); ++i) poms[(size_t) i].second->setBounds ((i < 3 ? pr : pr2).getX() + (i % 3) * w, (i < 3 ? pr : pr2).getY(), w, (i < 3 ? pr : pr2).getHeight());
            btOn.setBounds (d.removeFromTop (30).removeFromLeft (180));
        }
        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (0xfb101013));
            auto d = getLocalBounds().reduced (24);
            d = d.removeFromRight ((int) (d.getWidth() * 0.42f)).reduced (8, 0);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (11.0f, true));
            auto et = [&] (int y, const juce::String& t) { g.drawText (dibuix::majuscules (t), d.getX(), y, d.getWidth(), 16, juce::Justification::centredLeft); };
            et (cbCab.getY() - 18, "Pantalla");
            et (cbAlt.getY() - 18, "Altaveu");
            et (segA.getY() - 18, "Micròfon A (el principal)"_ca);
            et (segB.getY() - 18, "Micròfon B (es barreja amb «Mescla B»)"_ca);
            // el con de l'altaveu
            auto c = con();
            const auto& alt = altaveus()[(size_t) acc.rack().altaveu()];
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.fillEllipse (c.translated (6, 10));
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b3b40), c.getX(), c.getY(), juce::Colour (0xff111114), c.getRight(), c.getBottom(), false));
            g.fillEllipse (c);
            auto susp = c.reduced (c.getWidth() * 0.04f);
            g.setColour (juce::Colour (0xff1b1b1e));
            g.fillEllipse (susp);
            auto cone = susp.reduced (susp.getWidth() * 0.05f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2826), cone.getCentreX(), cone.getCentreY(), juce::Colour (0xff4a4640), cone.getRight(), cone.getCentreY(), true));
            g.fillEllipse (cone);
            g.setColour (juce::Colours::black.withAlpha (0.25f));
            for (int k = 1; k < 8; ++k) g.drawEllipse (cone.reduced (cone.getWidth() * 0.05f * (float) k), 1.0f);
            auto cap = cone.withSizeKeepingCentre (cone.getWidth() * 0.26f, cone.getHeight() * 0.26f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5a5650), cap.getX(), cap.getY(), juce::Colour (0xff1a1816), cap.getRight(), cap.getBottom(), false));
            g.fillEllipse (cap);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (12.0f, true));
            g.drawText (u8 (alt.nom) + "  ·  "_ca + u8 (alt.caracter), c.withY (c.getBottom() + 4).withHeight (18), juce::Justification::centred);
            g.setFont (dibuix::lletra (11.0f));
            g.drawText ("Arrossega els micròfons: cap al centre, més brillant; cap a la vora, més fosc. Amunt i avall: més a prop o més lluny."_ca,
                        c.withY (c.getY() - 26).withHeight (18).expanded (80, 0), juce::Justification::centred);
            // els micròfons
            for (int m = 1; m >= 0; --m)
            {
                if (m == 1 && acc.rack().valor (MesclaB) < 0.5f) continue;
                const auto pt = posicioMic (m);
                const float r = c.getWidth() * (0.075f - 0.03f * acc.rack().valor (m == 0 ? DistA : DistB));
                g.setColour (juce::Colours::black.withAlpha (0.5f));
                g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (pt.translated (4, 6)));
                const auto cc = m == 0 ? juce::Colour (0xffb7bcc6) : juce::Colour (0xff8fa7c9);
                g.setGradientFill (juce::ColourGradient (cc.brighter (0.4f), pt.x - r, pt.y - r, cc.darker (0.5f), pt.x + r, pt.y + r, false));
                g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (pt));
                g.setColour (juce::Colours::black.withAlpha (0.35f));
                for (float k = -0.6f; k <= 0.61f; k += 0.2f) g.drawLine (pt.x - r * 0.7f, pt.y + r * k, pt.x + r * 0.7f, pt.y + r * k, 1.0f);
                g.setColour (juce::Colours::white);
                g.setFont (dibuix::lletra (13.0f, true));
                const auto& mc = micros()[(size_t) acc.rack().valor (m == 0 ? MicA : MicB)];
                g.drawText ((m == 0 ? "A · "_ca : "B · "_ca) + u8 (mc.nom), juce::Rectangle<float> (200, 18).withCentre (pt.translated (0, r + 14)), juce::Justification::centred);
            }
        }
        juce::Point<float> posicioMic (int m) const
        {
            const auto c = con();
            const float pos = acc.rack().valor (m == 0 ? PosA : PosB), dist = acc.rack().valor (m == 0 ? DistA : DistB);
            return { c.getCentreX() + pos * c.getWidth() * 0.44f, c.getCentreY() - c.getHeight() * 0.2f + dist * c.getHeight() * 0.55f + (m == 1 ? 4.0f : 0.0f) };
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            arrossega = -1;
            for (int m : { 0, 1 })
            {
                if (m == 1 && acc.rack().valor (MesclaB) < 0.5f) continue;
                if (posicioMic (m).getDistanceFrom (e.position) < con().getWidth() * 0.09f) arrossega = m;
            }
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (arrossega < 0) return;
            const auto c = con();
            const float pos = juce::jlimit (0.0f, 1.0f, (e.position.x - c.getCentreX()) / (c.getWidth() * 0.44f));
            const float dist = juce::jlimit (0.0f, 1.0f, (e.position.y - (c.getCentreY() - c.getHeight() * 0.2f)) / (c.getHeight() * 0.55f));
            acc.rack().posa (arrossega == 0 ? PosA : PosB, pos);
            acc.rack().posa (arrossega == 0 ? DistA : DistB, dist);
            for (auto& [p, x] : poms) x->setValor (acc.rack().valor (p), true);
            acc.canviat();
            repaint();
        }
    private:
        Accions& acc;
        juce::ComboBox cbCab, cbAlt;
        Segmentat segA { { "Dynamic 57", "Dynamic 421", "Ribbon 121", "Condenser 87" } }, segB { { "Dynamic 57", "Dynamic 421", "Ribbon 121", "Condenser 87" } };
        std::vector<std::pair<int, std::unique_ptr<Pom>>> poms;
        Boto btOn { "Pantalla encesa"_ca, std::nullopt, Boto::Estil::Tecla };
        Boto btTanca { "Torna"_ca, std::nullopt, Boto::Estil::Primari };
        int arrossega = -1;
    };
}

namespace
{
    // ============================================================================================ el panell sencer
    class PanellGuitarRack : public juce::Component, public juce::DragAndDropContainer, private juce::Timer, public OrigenPresets, public Accions
    {
    public:
        explicit PanellGuitarRack (std::shared_ptr<GuitarRack> g) : gr (std::move (g)), escenari (*this), ampli (*this), pantalla (*this), pedalera (*this), biblio (*this)
        {
            for (auto* b : { &btAmp, &btCab, &btMics, &btAnt, &btSeg, &btPreset, &btDesa }) addAndMakeVisible (b);
            addAndMakeVisible (btFav);
            btAmp.setTooltip ("Tria l'amplificador (els poms agafen els valors de sempre del model)."_ca);
            btCab.setTooltip ("Tria la pantalla i l'altaveu."_ca);
            btMics.setTooltip ("Obre la pantalla per triar i col·locar els micròfons."_ca);
            btAmp.onClick = [this] { menuAmplis(); };
            btCab.onClick = [this] { menuPantalles(); };
            btMics.onClick = [this] { obrePantalla(); };
            btAnt.setTooltip ("Preset anterior"_ca);
            btSeg.setTooltip ("Preset següent"_ca);
            btPreset.setTooltip ("Tots els presets (cadenes senceres): net, crunch, rock, alt guany, ambient, indie, estils i els teus"_ca);
            btDesa.setTooltip ("Desa la cadena sencera (pedals, ampli, pantalla i micròfons) com un preset teu"_ca);
            btFav.setTooltip ("Preferit"_ca);
            btAnt.onClick = [this] { pas (-1); };
            btSeg.onClick = [this] { pas (1); };
            btPreset.onClick = [this] { obreNavegador(); };
            btDesa.onClick = [this] { desaPreset(); };
            btFav.onClick = [this] { commutaFavoritActual(); };
            auto pom = [this] (int p, const juce::String& nom, bool bip)
            {
                const auto& f = infoFix (p);
                auto x = std::make_unique<Pom> (nom, (double) f.mn, (double) f.mx, (double) f.def, bip,
                                                [this, p] (double v) { const auto& ff = infoFix (p); return u8 (gr->textParametre (p, (float) ((v - ff.mn) / (ff.mx - ff.mn)))); });
                x->setTooltip (u8 (f.ajuda));
                x->onCanvi = [this, p] (double v) { gr->posa (p, (float) v); canviat(); };
                x->onReinicia = [this, p] { gr->posa (p, infoFix (p).def); refrescaValors(); canviat(); };
                addAndMakeVisible (*x);
                poms.push_back ({ p, std::move (x) });
            };
            pom (Entrada, "Entrada", true);
            pom (Gate, "Gate", false);
            pom (Sortida, "Sortida", true);
            addAndMakeVisible (escenari);
            escenari.addAndMakeVisible (ampli);
            escenari.addAndMakeVisible (pantalla);
            escenari.addAndMakeVisible (pedalera);
            addAndMakeVisible (biblio);
            setSize (1360, 880);
            refrescaTot();
            startTimerHz (30);
        }
        ~PanellGuitarRack() override { stopTimer(); }

        // ---- Accions ----
        GuitarRack& rack() override { return *gr; }
        juce::DragAndDropContainer* dnd() override { return this; }
        juce::Component& arrel() override { return *this; }
        void canviat() override
        {
            gr->marcaModificat();
            refrescaPreset();
            escenari.repaint();
        }
        void obrePedal (int k, juce::Rectangle<int> origen) override
        {
            auto pl = gr->placa();
            if (k < 0 || k >= (int) pl->pedals.size()) return;
            gran = std::make_unique<PedalGran> (*this, pl->pedals[(size_t) k], origen.toFloat().translated ((float) -escenari.getX(), (float) -escenari.getY()));
            gran->enTancar = [this, quin = gran.get()] { if (gran.get() == quin) { gran.reset(); refrescaTot(); } };
            addAndMakeVisible (*gran);
            gran->setBounds (escenari.getBounds());
        }
        void obrePantalla() override
        {
            zoom = std::make_unique<ZoomPantalla> (*this);
            zoom->enTancar = [this] { juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PanellGuitarRack> (this)] { if (sp) { sp->zoom.reset(); sp->refrescaTot(); } }); };
            addAndMakeVisible (*zoom);
            zoom->setBounds (escenari.getBounds());
            zoom->grabKeyboardFocus();
        }
        void menuPedal (int k) override
        {
            auto pl = gr->placa();
            if (k < 0 || k >= (int) pl->pedals.size()) return;
            auto p = pl->pedals[(size_t) k];
            juce::PopupMenu m;
            m.addSectionHeader (u8 (p->model().nom));
            m.addItem (1, "Obre'l"_ca);
            m.addItem (2, p->on.load() ? "Apaga'l (bypass)"_ca : "Encén-lo"_ca);
            m.addItem (3, p->loop.load() ? "Passa'l davant de l'ampli"_ca : "Passa'l al loop (després de la pantalla)"_ca);
            m.addItem (4, "Valors de sempre"_ca);
            m.addSeparator();
            m.addItem (5, "Treu-lo de la pedalera"_ca);
            m.showMenuAsync (juce::PopupMenu::Options(), [this, p, k] (int r)
            {
                if (r == 1 && k < (int) pedalera.petits.size()) obrePedal (k, getLocalArea (pedalera.petits[(size_t) k].get(), pedalera.petits[(size_t) k]->getLocalBounds()));
                if (r == 2) { p->on.store (! p->on.load()); canviat(); }
                if (r == 3) { gr->posaLoop (k, ! p->loop.load()); canviat(); }
                if (r == 4) { for (int i = 0; i < p->numControls(); ++i) p->posa (i, p->model().controls[(size_t) i].def); canviat(); }
                if (r == 5) { gr->treu (k); canviat(); }
            });
        }

        // ---- refrescos ----
        void refrescaTot()
        {
            if (gr->placa().get() != pedalera.placaVista) pedalera.refesca();
            if (gr->ampli() != ampliVist) { ampliVist = gr->ampli(); ampli.refesca(); }
            else ampli.actualitza();
            versioVista = gr->versio();
            btAmp.setButtonText ("Ampli: "_ca + u8 (amplis()[(size_t) gr->ampli()].nom));
            btCab.setButtonText ("Pantalla: "_ca + u8 (cabs()[(size_t) gr->cab()].nom) + " · "_ca + u8 (altaveus()[(size_t) gr->altaveu()].nom));
            btMics.setButtonText ("Micros: "_ca + u8 (micros()[(size_t) gr->valor (MicA)].nom) + (gr->valor (MesclaB) > 0.5f ? " + " + u8 (micros()[(size_t) gr->valor (MicB)].nom) : juce::String()));
            refrescaValors();
            refrescaPreset();
            biblio.refrescaAmplis();
            if (gran != nullptr && gran->index() < 0) gran->tanca();
            if (zoom != nullptr) zoom->actualitza();
            for (int c = 0; c < numFixos; ++c) vistos[(size_t) c] = gr->valor (c);
            escenari.repaint();
        }
        void refrescaValors() { for (auto& [p, x] : poms) x->setValor (gr->valor (p), true); }
        void refrescaPreset()
        {
            const auto n = u8 (gr->nomPreset());
            btPreset.setButtonText (n.isEmpty() ? "(sense preset)"_ca : n + (gr->presetModificat() ? " *" : ""));
            btFav.setToggleState (esFavorit (n), juce::dontSendNotification);
        }

        // ---- dibuix ----
        void paint (juce::Graphics& g) override
        {
            g.fillAll (tema::fons);
            auto dalt = getLocalBounds().removeFromTop (alcadaDalt).toFloat();
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff26262d), 0, dalt.getY(), juce::Colour (0xff18181d), 0, dalt.getBottom(), false));
            g.fillRect (dalt);
            g.setColour (tema::linia);
            g.drawHorizontalLine ((int) dalt.getBottom() - 1, 0.0f, (float) getWidth());
            auto t = dalt.reduced (14.0f, 10.0f).removeFromLeft (124.0f);
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (17.0f, true));
            g.drawText ("GUITAR RACK", t.removeFromTop (22.0f), juce::Justification::centredLeft);
            g.setColour (tema::accent);
            g.setFont (dibuix::lletra (10.0f, true));
            g.drawText ("MIBASE", t, juce::Justification::centredLeft);
            // vúmetres d'entrada i de sortida
            auto vu = [&] (juce::Rectangle<float> r, float v)
            {
                g.setColour (juce::Colours::black.withAlpha (0.5f));
                g.fillRoundedRectangle (r, 2.0f);
                const float db = v > 1e-5f ? 20.0f * std::log10 (v) : -100.0f;
                const float f = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
                auto b = r.reduced (1.0f);
                b = b.removeFromBottom (b.getHeight() * f);
                g.setColour (db > -1.0f ? tema::error : (db > -9.0f ? tema::guitarra : juce::Colour (0xff4cd17a)));
                g.fillRoundedRectangle (b, 1.5f);
            };
            vu (zonaVuIn, vuIn);
            vu (zonaVuOut, vuOut);
        }

        void pintaCables (juce::Graphics& g)
        {
            auto pt = [&] (juce::Component& c, juce::Point<float> p) { return escenari.getLocalPoint (&c, p); };
            const auto negre = juce::Colour (0xff1c1c1f), gris = juce::Colour (0xff2b2b31);
            auto pl = gr->placa();
            juce::Point<float> ant = pt (pedalera, pedalera.jackGuitarra());
            const int n = (int) pedalera.petits.size();
            for (int k = 0; k < n; ++k)
            {
                if (pedalera.esLoop (k)) continue;
                const auto& c = *pedalera.petits[(size_t) k];
                pintaCable (g, ant, c.entrada (escenari), negre, 5.0f, 22.0f);
                ant = c.sortida (escenari);
            }
            // un cable que passa per sobre dels pedals (per la vora de dalt de la pedalera)
            const float vora = (float) pedalera.getY() + 14.0f;
            auto perDalt = [&] (juce::Point<float> a, juce::Point<float> b, juce::Colour c, float dx)
            {
                juce::Path p;
                p.startNewSubPath (a);
                p.cubicTo (a.translated (dx, 0.0f), { a.x + dx, vora }, { a.x + dx * 0.3f, vora });
                p.cubicTo ({ b.x + (a.x - b.x) * 0.4f, vora }, { b.x - dx * 0.6f, vora + (b.y - vora) * 0.3f }, b);
                g.setColour (juce::Colours::black.withAlpha (0.35f));
                g.strokePath (p, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::translation (2.0f, 4.0f));
                g.setColour (c);
                g.strokePath (p, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (juce::Colours::white.withAlpha (0.16f));
                g.strokePath (p, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::translation (0.0f, -1.2f));
                pintaCable (g, a, a, c, 5.0f, 0.0f);
                pintaCable (g, b, b, c, 5.0f, 0.0f);
            };
            // cap a l'entrada de l'ampli
            perDalt (ant, pt (ampli, ampli.jackEntrada()), negre, 34.0f);
            // de l'ampli a la pantalla (cable d'altaveu, més gruixut)
            pintaCable (g, pt (ampli, ampli.jackAltaveu()), pt (pantalla, pantalla.jackEntrada()), juce::Colour (0xff111113), 7.0f, 30.0f);
            ant = pt (pantalla, pantalla.jackSortida());
            bool primer = true;
            for (int k = 0; k < n; ++k)
            {
                if (! pedalera.esLoop (k)) continue;
                const auto& c = *pedalera.petits[(size_t) k];
                if (primer) perDalt (ant, c.entrada (escenari), gris, -20.0f);
                else pintaCable (g, ant, c.entrada (escenari), gris, 5.0f, 22.0f);
                primer = false;
                ant = c.sortida (escenari);
            }
            pintaCable (g, ant, pt (pedalera, pedalera.jackSortida()), gris, 5.0f, 22.0f);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            auto dalt = r.removeFromTop (alcadaDalt).reduced (12, 8);
            dalt.removeFromLeft (128);
            // a la dreta: Entrada, Gate, Sortida i els vúmetres
            auto dreta = dalt.removeFromRight (290);
            zonaVuOut = dreta.removeFromRight (8).toFloat().reduced (0, 4);
            dreta.removeFromRight (4);
            zonaVuIn = dreta.removeFromLeft (8).toFloat().reduced (0, 4);
            dreta.removeFromLeft (4);
            const int wp = dreta.getWidth() / 3;
            for (auto& [p, x] : poms) x->setBounds (dreta.removeFromLeft (wp).expanded (0, 6));
            dalt.removeFromRight (10);
            auto fila = dalt.withSizeKeepingCentre (dalt.getWidth(), 32);
            const int wb = juce::jmin (230, fila.getWidth() / 6);
            btAmp.setBounds (fila.removeFromLeft (wb));
            fila.removeFromLeft (6);
            btCab.setBounds (fila.removeFromLeft (wb + 40));
            fila.removeFromLeft (6);
            btMics.setBounds (fila.removeFromLeft (wb));
            fila.removeFromLeft (14);
            btAnt.setBounds (fila.removeFromLeft (28));
            btPreset.setBounds (fila.removeFromLeft (juce::jmax (120, fila.getWidth() - 28 - 6 - 26 - 6 - 80)));
            btSeg.setBounds (fila.removeFromLeft (28));
            fila.removeFromLeft (6);
            btFav.setBounds (fila.removeFromLeft (26));
            fila.removeFromLeft (6);
            btDesa.setBounds (fila.removeFromLeft (80));
            biblio.setBounds (r.removeFromBottom (juce::jlimit (130, 190, r.getHeight() / 5)));
            escenari.setBounds (r);
            auto e = escenari.getLocalBounds().reduced (14, 6);
            auto sup = e.removeFromTop ((int) (e.getHeight() * 0.54f));
            pantalla.setBounds (sup.removeFromRight ((int) (sup.getWidth() * 0.32f)));
            sup.removeFromRight (30);
            ampli.setBounds (sup.withSizeKeepingCentre (sup.getWidth(), juce::jmin (sup.getHeight(), (int) (sup.getWidth() * 0.46f))).withBottomY (sup.getBottom()));
            e.removeFromTop (8);
            pedalera.setBounds (e);
            if (gran != nullptr) gran->setBounds (escenari.getBounds());
            if (zoom != nullptr) zoom->setBounds (escenari.getBounds());
            if (navegador != nullptr) navegador->setBounds (getLocalBounds());
        }

        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }

    private:
        class Escenari : public juce::Component
        {
        public:
            explicit Escenari (PanellGuitarRack& p) : pan (p) {}
            void paint (juce::Graphics& g) override
            {
                // l'estudi: paret fosca i terra de fusta
                auto r = getLocalBounds().toFloat();
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1e1d22), 0, r.getY(), juce::Colour (0xff121215), 0, r.getBottom() * 0.6f, false));
                g.fillRect (r);
                auto terra = r.withTrimmedTop (r.getHeight() * 0.52f);
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2119), 0, terra.getY(), juce::Colour (0xff17110d), 0, terra.getBottom(), false));
                g.fillRect (terra);
                g.setColour (juce::Colours::black.withAlpha (0.18f));
                for (float x = terra.getX(); x < terra.getRight(); x += 90.0f) g.drawVerticalLine ((int) x, terra.getY(), terra.getBottom());
            }
            void paintOverChildren (juce::Graphics& g) override { pan.pintaCables (g); }
        private:
            PanellGuitarRack& pan;
        };

        void timerCallback() override
        {
            gr->recull();
            auto& m = gr->mesures();
            const float ei = m.entrada.exchange (0.0f), eo = m.sortida.exchange (0.0f), ea = m.ampli.exchange (0.0f);
            vuIn = juce::jmax (ei, vuIn * 0.82f);
            vuOut = juce::jmax (eo, vuOut * 0.82f);
            ampli.nivellPilot = juce::jmax (juce::jlimit (0.0f, 1.0f, ea * 1.5f), ampli.nivellPilot * 0.85f);
            repaint (zonaVuIn.toNearestInt().expanded (2));
            repaint (zonaVuOut.toNearestInt().expanded (2));
            if (gr->versio() != versioVista) { refrescaTot(); return; }
            bool canvi = false;
            for (int c = 0; c < numFixos; ++c) if (std::abs (gr->valor (c) - vistos[(size_t) c]) > 1e-6f) { vistos[(size_t) c] = gr->valor (c); canvi = true; }
            if (canvi) { ampli.actualitza(); refrescaValors(); pantalla.repaint(); if (zoom != nullptr) zoom->actualitza(); }
            ampli.repaint (ampli.pilot.toNearestInt().expanded (14));
            for (auto& p : pedalera.petits)
                if (p->p->on.load())
                {
                    const auto q = p->geo();
                    p->repaint (juce::Rectangle<float> (q.radiLed * 9.0f, q.radiLed * 9.0f).withCentre (q.led).toNearestInt());
                }
        }

        // ---- menús d'ampli i pantalla ----
        void menuAmplis()
        {
            juce::PopupMenu m;
            for (const char* o : { "Britànic", "Americà", "Modern" })
            {
                juce::PopupMenu s;
                for (int i = 0; i < (int) amplis().size(); ++i)
                    if (amplis()[(size_t) i].origen == o)
                        s.addItem (i + 1, u8 (amplis()[(size_t) i].nom) + "   ·   "_ca + u8 (amplis()[(size_t) i].inspirat), true, i == gr->ampli());
                m.addSubMenu (u8 (o) == "Modern" ? "Moderns i d'alt guany"_ca : (u8 (o) == "Americà"_ca ? "Americans"_ca : "Britànics"_ca), s);
            }
            m.addSeparator();
            m.addItem (1000, "Posa-hi també la pantalla i l'altaveu que hi van bé"_ca);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btAmp), [this] (int r)
            {
                if (r == 1000)
                {
                    const auto& a = amplis()[(size_t) gr->ampli()];
                    gr->posaCab (juce::jmax (0, indexCab (a.cab)), false);
                    gr->posaAltaveu (juce::jmax (0, indexAltaveu (a.altaveu)));
                    canviat();
                    return;
                }
                if (r <= 0) return;
                gr->posaAmpli (r - 1);
                canviat();
            });
        }
        void menuPantalles()
        {
            juce::PopupMenu m, a;
            for (int i = 0; i < (int) cabs().size(); ++i) m.addItem (i + 1, u8 (cabs()[(size_t) i].nom) + "   ·   "_ca + u8 (cabs()[(size_t) i].inspirat), true, i == gr->cab());
            for (int i = 0; i < (int) altaveus().size(); ++i) a.addItem (100 + i, u8 (altaveus()[(size_t) i].nom) + "   ·   "_ca + u8 (altaveus()[(size_t) i].inspirat), true, i == gr->altaveu());
            m.addSeparator();
            m.addSubMenu ("Altaveu"_ca, a);
            m.addItem (200, "Micròfons i sala…"_ca);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btCab), [this] (int r)
            {
                if (r >= 1 && r < 100) { gr->posaCab (r - 1, true); canviat(); }
                else if (r >= 100 && r < 200) { gr->posaAltaveu (r - 100); canviat(); }
                else if (r == 200) obrePantalla();
            });
        }

        // ---- presets (OrigenPresets) ----
        std::vector<const Preset*> totsPresets()
        {
            std::vector<const Preset*> v;
            for (auto& p : biblioteca().usuari) v.push_back (&p);
            for (auto& p : presetsFabrica()) v.push_back (&p);
            return v;
        }
        bool esFavorit (const juce::String& nom)
        {
            const auto n = nom.toStdString();
            return ! n.empty() && biblioteca().esFavorit (biblioteca().busca (n) != nullptr ? BibliotecaPresets::clauUsuari (n) : BibliotecaPresets::clauFabrica (n));
        }
        void commutaFavoritActual()
        {
            const auto nom = gr->nomPreset();
            if (nom.empty()) { btFav.setToggleState (false, juce::dontSendNotification); return; }
            biblioteca().commutaFavorit (biblioteca().busca (nom) != nullptr ? BibliotecaPresets::clauUsuari (nom) : BibliotecaPresets::clauFabrica (nom));
            desaBiblioteca();
            refrescaPreset();
        }
        void aplicaPreset (const Preset& p)
        {
            if (gran != nullptr) gran.reset();
            gr->posaEstat (p.estat);
            gr->posaNomPreset (p.nom, false);
            refrescaTot();
        }
        void pas (int d)
        {
            const auto v = totsPresets();
            if (v.empty()) return;
            const auto nom = gr->nomPreset();
            int i = -1;
            for (int k = 0; k < (int) v.size(); ++k) if (v[(size_t) k]->nom == nom) i = k;
            i = i < 0 ? (d > 0 ? 0 : (int) v.size() - 1) : (i + d + (int) v.size()) % (int) v.size();
            aplicaPreset (*v[(size_t) i]);
        }
        void desaPreset()
        {
            const auto actual = u8 (gr->nomPreset());
            const bool meu = biblioteca().busca (actual.toStdString()) != nullptr;
            const auto proposta = meu ? actual : u8 (biblioteca().nomLliure ((actual.isEmpty() ? juce::String ("El meu so"_ca) : actual + " (meu)"_ca).toStdString()));
            dialeg = std::make_unique<DialegText> ("Desa el preset"_ca, "Nom del preset (si ja en tens un amb aquest nom, se substitueix). Es desa la cadena sencera."_ca, "Desa"_ca,
                [] (const juce::String& t) { return t.trim().isEmpty() ? "Escriu un nom."_ca : juce::String(); },
                [this] (const juce::String& t)
                {
                    const auto nom = t.trim().toStdString();
                    const auto* ja = biblioteca().busca (nom);
                    gr->posaNomPreset (nom, false);
                    biblioteca().desaPreset ({ nom, ja != nullptr ? ja->categoria : std::string ("Meus"), "", gr->estat() });
                    desaBiblioteca();
                    refrescaPreset();
                }, proposta);
            DialegText::mostra (*this, dialeg);
        }
        void obreNavegador()
        {
            navegador = std::make_unique<NavegadorPresetsComu> (*this);
            addAndMakeVisible (*navegador);
            navegador->setBounds (getLocalBounds());
            navegador->grabKeyboardFocus();
        }
        juce::StringArray categoriesFabrica() override { juce::StringArray a; for (auto& c : categories()) a.add (u8 (c)); return a; }
        juce::StringArray carpetesUsuari() override { juce::StringArray a; for (auto& c : biblioteca().carpetes()) a.add (u8 (c)); return a; }
        std::vector<EntradaPreset> entrades() override
        {
            std::vector<EntradaPreset> v;
            const auto tots = totsPresets();
            const int nu = (int) biblioteca().usuari.size();
            for (int i = 0; i < (int) tots.size(); ++i)
            {
                const auto& p = *tots[(size_t) i];
                const bool usuari = i < nu;
                juce::String amp;
                {
                    const auto e = p.estat.find ("\"amp\":\"");
                    if (e != std::string::npos)
                    {
                        const auto f = p.estat.find ('"', e + 7);
                        const int a = indexAmpli (p.estat.substr (e + 7, f - e - 7));
                        if (a >= 0) amp = u8 (amplis()[(size_t) a].nom);
                    }
                }
                v.push_back ({ u8 (p.nom), u8 (p.categoria), u8 (p.descripcio), amp, usuari,
                               biblioteca().esFavorit (usuari ? BibliotecaPresets::clauUsuari (p.nom) : BibliotecaPresets::clauFabrica (p.nom)), i });
            }
            return v;
        }
        juce::String presetActual() override { return u8 (gr->nomPreset()); }
        void aplica (const EntradaPreset& e) override
        {
            const auto tots = totsPresets();
            if (e.index >= 0 && e.index < (int) tots.size()) aplicaPreset (*tots[(size_t) e.index]);
        }
        void commutaFavorit (const EntradaPreset& e) override
        {
            const auto n = e.nom.toStdString();
            biblioteca().commutaFavorit (e.usuari ? BibliotecaPresets::clauUsuari (n) : BibliotecaPresets::clauFabrica (n));
            desaBiblioteca();
            refrescaPreset();
        }
        bool existeixMeu (const juce::String& nom) override { return biblioteca().busca (nom.toStdString()) != nullptr; }
        void reanomena (const juce::String& vell, const juce::String& nou) override
        {
            if (biblioteca().reanomena (vell.toStdString(), nou.toStdString()))
            {
                desaBiblioteca();
                if (u8 (gr->nomPreset()) == vell) gr->posaNomPreset (nou.toStdString(), gr->presetModificat());
            }
            refrescaPreset();
        }
        void mou (const juce::String& nom, const juce::String& carpeta) override { biblioteca().mou (nom.toStdString(), carpeta.toStdString()); desaBiblioteca(); }
        void esborra (const juce::String& nom) override { biblioteca().esborra (nom.toStdString()); desaBiblioteca(); refrescaPreset(); }
        void tancaNavegador() override
        {
            juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PanellGuitarRack> (this)] { if (sp != nullptr) { sp->navegador.reset(); sp->grabKeyboardFocus(); } });
        }

        std::shared_ptr<GuitarRack> gr;
        static constexpr int alcadaDalt = 66;
        Escenari escenari;
        VistaAmpli ampli;
        VistaPantalla pantalla;
        Pedalera pedalera;
        Biblioteca biblio;
        std::unique_ptr<PedalGran> gran;
        std::unique_ptr<ZoomPantalla> zoom;
        std::unique_ptr<NavegadorPresetsComu> navegador;
        std::unique_ptr<DialegText> dialeg;
        Boto btAmp { "Ampli", std::nullopt, Boto::Estil::Secundari }, btCab { "Pantalla", std::nullopt, Boto::Estil::Secundari }, btMics { "Micros", std::nullopt, Boto::Estil::Secundari };
        Boto btAnt { {}, dibuix::Icona::Rebobina, Boto::Estil::Subtil }, btSeg { {}, dibuix::Icona::Endavant, Boto::Estil::Subtil };
        Boto btPreset { "(sense preset)"_ca, std::nullopt, Boto::Estil::Secundari };
        BotoEstrella btFav;
        Boto btDesa { "Desa", dibuix::Icona::Disc, Boto::Estil::Secundari };
        std::vector<std::pair<int, std::unique_ptr<Pom>>> poms;
        juce::Rectangle<float> zonaVuIn, zonaVuOut;
        float vuIn = 0.0f, vuOut = 0.0f;
        unsigned versioVista = 0;
        int ampliVist = -1;
        std::array<float, numFixos> vistos {};
        juce::TooltipWindow tips { this, 600 };
    };

    class FinestraGuitarRack : public juce::DocumentWindow
    {
    public:
        FinestraGuitarRack (std::shared_ptr<GuitarRack> g, const juce::String& titol, std::function<void()> enTancar)
            : juce::DocumentWindow (titol, tema::fons, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton | juce::DocumentWindow::maximiseButton),
              tancar (std::move (enTancar))
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new PanellGuitarRack (std::move (g)), true);
            setResizable (true, true);
            setResizeLimits (1040, 700, 2600, 1600);
            if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            {
                const auto a = d->userBounds.toNearestInt();
                setSize (juce::jmin (getWidth(), a.getWidth() - 40), juce::jmin (getHeight(), a.getHeight() - 40));
            }
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::MessageManager::callAsync ([t = tancar] { t(); }); }
        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }
    private:
        std::function<void()> tancar;
    };
}

std::unique_ptr<juce::DocumentWindow> creaFinestraGuitarRack (std::shared_ptr<GuitarRack> g, const juce::String& titol, std::function<void()> enTancar)
{
    return std::make_unique<FinestraGuitarRack> (std::move (g), titol, std::move (enTancar));
}
