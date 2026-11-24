#pragma once

/** 0.89: les peces comunes de les finestres dels sintes de Mibase (abans eren dins de FinestraSubBass.cpp):
    textures fetes amb codi, poms (5 estils), selectors, interruptors, faders, gràfic d'envolupants, pantalla de fòsfor,
    rodes i teclat; i PanellSinte, la base de cada finestra (capçalera amb presets, LCD, CPU, desfer, MIDI learn,
    importar/exportar, recents i atzar). Cada sinte fa la seva disposició i el seu aspecte. */
#include "Controls.h"
#include "DialegText.h"
#include "Dibuix.h"
#include "EscGlobal.h"
#include "EstatEstudi.h"
#include "NavegadorPresets.h"
#include "Plugins.h"
#include "Tema.h"
#include "Text.h"
#include <array>
#include <map>
#include <mibase/Drive.h>
#include <mibase/SinteBase.h>

namespace sinteui
{
    inline juce::String u8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str(), (int) s.size()); }
    inline juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

    // ---- colors propis ----
    namespace sb
    {
        inline const juce::Colour serigrafia { 0xffe9e5da };
        inline const juce::Colour serigrafiaSuau { 0xff9f9b92 };
        inline const juce::Colour cian { 0xff35e0d2 };
        inline const juce::Colour lcd { 0xffffb238 };
        inline const juce::Colour lcdFons { 0xff1a1206 };
        inline const juce::Colour fosfor { 0xff4dffc8 };
        inline juce::Colour seccio (int s)
        {
            static const juce::uint32 c[] = { 0xff35e0d2, 0xff8a7bff, 0xffffad3b, 0xff8fe07a, 0xffff6fb5, 0xffff5a36, 0xffb48cff, 0xff6fd3ff };
            return juce::Colour (c[juce::jlimit (0, 7, s)]);
        }
    }

    // ---- textures (fetes un cop i guardades) ----
    enum class Textura { Metall, Fusta, Carboni, Laca, Nit, Panell };
    inline juce::Image textura (Textura t, int w, int h)
    {
        static std::map<std::tuple<int, int, int>, juce::Image> cau;
        const auto clau = std::make_tuple ((int) t, w, h);
        if (auto it = cau.find (clau); it != cau.end()) return it->second;
        juce::Image img (juce::Image::RGB, juce::jmax (1, w), juce::jmax (1, h), false);
        juce::Random r (t == Textura::Fusta ? 1977 : t == Textura::Metall ? 303 : 808);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        if (t == Textura::Metall)
        {
            // alumini anoditzat fosc i raspallat: ratlles horitzontals molt fines i gra
            std::vector<float> fila ((size_t) h);
            for (auto& f : fila) f = (r.nextFloat() - 0.5f) * 7.0f;
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    const float base = 31.0f - 9.0f * (float) y / (float) h;
                    const float v = base + fila[(size_t) y] * (0.6f + 0.4f * std::sin (x * 0.004f + y * 0.7f)) + (r.nextFloat() - 0.5f) * 5.0f;
                    const auto c = (juce::uint8) juce::jlimit (0, 255, (int) v);
                    bd.setPixelColour (x, y, juce::Colour (c, c, (juce::uint8) juce::jlimit (0, 255, (int) (v + 4.0f))));
                }
        }
        else if (t == Textura::Fusta)
        {
            // noguera: veta vertical que ondula, porus i vernís
            const float fase = r.nextFloat() * 10.0f;
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    const float ona = std::sin ((float) x * 0.55f + 2.6f * std::sin ((float) y * 0.011f + (float) x * 0.07f + fase) + 1.2f * std::sin ((float) y * 0.043f));
                    const float veta = 0.5f + 0.5f * ona;
                    const float porus = r.nextFloat() < 0.03f ? -18.0f : 0.0f;
                    const float llum = 1.0f - 0.35f * std::abs ((float) x / (float) juce::jmax (1, w - 1) - 0.35f);
                    const float R = (60.0f + 38.0f * veta + porus) * llum, G = (33.0f + 20.0f * veta + porus * 0.6f) * llum, B = (18.0f + 10.0f * veta) * llum;
                    bd.setPixelColour (x, y, juce::Colour ((juce::uint8) juce::jlimit (0, 255, (int) R), (juce::uint8) juce::jlimit (0, 255, (int) G),
                                                           (juce::uint8) juce::jlimit (0, 255, (int) B)));
                }
        }
        else if (t == Textura::Laca)
        {
            // laca negra brillant: fons molt fosc, un reflex vertical suau i gra finíssim
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    const float u = (float) x / (float) juce::jmax (1, w - 1);
                    const float reflex = 26.0f * std::exp (-std::pow ((u - 0.3f) / 0.12f, 2.0f)) + 8.0f * std::exp (-std::pow ((u - 0.75f) / 0.08f, 2.0f));
                    const float v = 9.0f + reflex * (0.85f + 0.15f * std::sin ((float) y * 0.004f)) + (r.nextFloat() - 0.5f) * 2.0f;
                    const auto c = (juce::uint8) juce::jlimit (0, 255, (int) v);
                    bd.setPixelColour (x, y, juce::Colour (c, c, (juce::uint8) juce::jlimit (0, 255, (int) (v * 1.15f))));
                }
        }
        else if (t == Textura::Panell)
        {
            // 1.4: panell negre mat d'aparell antic (pintura texturada, amb un gra molt fi i un pèl de calidesa)
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    const float base = 20.0f - 5.0f * (float) y / (float) juce::jmax (1, h);
                    const float v = base + (r.nextFloat() - 0.5f) * 6.0f + ((x * 7 + y * 13) % 97 == 0 ? 6.0f : 0.0f);
                    bd.setPixelColour (x, y, juce::Colour ((juce::uint8) juce::jlimit (0, 255, (int) (v + 2.0f)), (juce::uint8) juce::jlimit (0, 255, (int) (v + 1.0f)),
                                                           (juce::uint8) juce::jlimit (0, 255, (int) v)));
                }
        }
        else if (t == Textura::Nit)
        {
            // blau nit anoditzat, raspallat en diagonal molt suau
            std::vector<float> fila ((size_t) (w + h));
            for (auto& f : fila) f = (r.nextFloat() - 0.5f) * 5.0f;
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    const float base = 1.0f - 0.25f * (float) y / (float) h;
                    const float n = fila[(size_t) (x + y)] * 0.7f + (r.nextFloat() - 0.5f) * 3.0f;
                    bd.setPixelColour (x, y, juce::Colour ((juce::uint8) juce::jlimit (0, 255, (int) (17.0f * base + n)),
                                                           (juce::uint8) juce::jlimit (0, 255, (int) (20.0f * base + n)),
                                                           (juce::uint8) juce::jlimit (0, 255, (int) (34.0f * base + n * 1.2f))));
                }
        }
        else
        {
            // fibra de carboni (sarja 2x2)
            constexpr int c = 6;
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                {
                    const int cx = x / c, cy = y / c;
                    const bool horitz = ((cx + cy) / 2) % 2 == 0;
                    const float u = horitz ? (float) (x % c) / c : (float) (y % c) / c;
                    const float v = 17.0f + 13.0f * std::sin (u * 3.14159f) + (r.nextFloat() - 0.5f) * 3.0f;
                    const auto g = (juce::uint8) juce::jlimit (0, 255, (int) v);
                    bd.setPixelColour (x, y, juce::Colour (g, g, (juce::uint8) (g + 2)));
                }
        }
        cau[clau] = img;
        return img;
    }

    inline void cargol (juce::Graphics& g, juce::Point<float> c, float r = 4.0f)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff9a9aa2), c.x - r, c.y - r, juce::Colour (0xff3a3a40), c.x + r, c.y + r, false));
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawLine (c.x - r * 0.6f, c.y - r * 0.3f, c.x + r * 0.6f, c.y + r * 0.3f, 1.2f);
    }

    inline void textEspaiat (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Justification j, float espai)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (g.getCurrentFont(), t, 0.0f, 0.0f);
        // espaiat entre lletres
        for (int i = 0; i < ga.getNumGlyphs(); ++i) ga.getGlyph (i).moveBy (espai * (float) i, 0.0f);
        auto b = ga.getBoundingBox (0, -1, true);
        float x = r.getX();
        if (j.testFlags (juce::Justification::horizontallyCentred)) x = r.getCentreX() - b.getWidth() * 0.5f;
        else if (j.testFlags (juce::Justification::right)) x = r.getRight() - b.getWidth();
        ga.moveRangeOfGlyphs (0, -1, x - b.getX(), r.getCentreY() - b.getCentreY());
        ga.draw (g);
    }

    // ---- icones de les ones ----
    inline juce::Path iconaOna (int tipus, juce::Rectangle<float> r)
    {
        juce::Path p;
        const float x0 = r.getX(), x1 = r.getRight(), y0 = r.getY(), y1 = r.getBottom(), ym = r.getCentreY(), w = r.getWidth();
        switch (tipus)
        {
            case 0: p.startNewSubPath (x0, ym); for (int i = 1; i <= 24; ++i) p.lineTo (x0 + w * i / 24.0f, ym - (y1 - y0) * 0.5f * std::sin (6.2832f * i / 24.0f)); break;
            case 1: p.startNewSubPath (x0, ym); p.lineTo (x0 + w * 0.25f, y0); p.lineTo (x0 + w * 0.75f, y1); p.lineTo (x1, ym); break;
            case 2: p.startNewSubPath (x0, y1); p.lineTo (x0 + w * 0.5f, y0); p.lineTo (x0 + w * 0.5f, y1); p.lineTo (x1, y0); p.lineTo (x1, y1); break;
            case 3: p.startNewSubPath (x0, y1); p.lineTo (x0, y0); p.lineTo (x0 + w * 0.5f, y0); p.lineTo (x0 + w * 0.5f, y1); p.lineTo (x1, y1); p.lineTo (x1, y0); break;
            case 4: p.startNewSubPath (x0, y1); p.lineTo (x0, y0); p.lineTo (x0 + w * 0.22f, y0); p.lineTo (x0 + w * 0.22f, y1); p.lineTo (x0 + w * 0.6f, y1);
                    p.lineTo (x0 + w * 0.6f, y0); p.lineTo (x0 + w * 0.82f, y0); p.lineTo (x0 + w * 0.82f, y1); p.lineTo (x1, y1); break;
            case 5:   // supersaw: tres serres desplaçades
                for (int k = 0; k < 3; ++k)
                {
                    const float dx = w * 0.12f * (float) (k - 1), dy = (y1 - y0) * 0.18f * (float) (k - 1);
                    p.startNewSubPath (x0 + dx * 0.0f, y1 - dy * 0.0f + dy);
                    p.lineTo (x0 + w * 0.5f + dx, y0 + dy + (y1 - y0) * 0.2f);
                    p.lineTo (x0 + w * 0.5f + dx, y1 + dy - (y1 - y0) * 0.2f);
                    p.lineTo (x1 + dx * 0.0f, y0 + dy + (y1 - y0) * 0.2f);
                }
                break;
            case 6:   // wavetable: tres ones apilades en perspectiva
                for (int k = 0; k < 3; ++k)
                {
                    const float ox = w * 0.1f * k, oy = -(y1 - y0) * 0.22f * k, a = (y1 - y0) * 0.28f;
                    p.startNewSubPath (x0 + ox, ym + oy + (y1 - y0) * 0.2f);
                    for (int i = 1; i <= 12; ++i)
                        p.lineTo (x0 + ox + w * 0.75f * i / 12.0f, ym + oy + (y1 - y0) * 0.2f - a * std::sin (6.2832f * i / 12.0f * (1.0f + 0.5f * k)));
                }
                break;
            case 7:   // soroll
            {
                const float v[] = { 0.5f, 0.1f, 0.8f, 0.3f, 0.95f, 0.2f, 0.7f, 0.05f, 0.6f, 0.35f, 0.9f, 0.5f };
                p.startNewSubPath (x0, y0 + (y1 - y0) * v[0]);
                for (int i = 1; i < 12; ++i) p.lineTo (x0 + w * i / 11.0f, y0 + (y1 - y0) * v[i]);
                break;
            }
            default:
            {
                const float v[] = { 0.3f, 0.9f, 0.1f, 0.65f, 0.4f };
                p.startNewSubPath (x0, y0 + (y1 - y0) * v[0]);
                for (int i = 0; i < 5; ++i)
                {
                    p.lineTo (x0 + w * (i + 1) / 5.0f, y0 + (y1 - y0) * v[i]);
                    if (i < 4) p.lineTo (x0 + w * (i + 1) / 5.0f, y0 + (y1 - y0) * v[i + 1]);
                }
            }
        }
        return p;
    }

    /** Quina icona va amb cada nom d'ona. */
    inline int iconaDeNom (const juce::String& n)
    {
        static const char* noms[] = { "Sinus", "Triangle", "Serra", "Quadrada", "Pols", "Supersaw", "Wavetable", "Soroll" };
        for (int i = 0; i < 8; ++i) if (n == noms[i]) return i;
        return 8;   // atzar (mostra i reté)
    }

    //==========================================================================
    /** El que els controls demanen al panell. */
    struct Enllac
    {
        virtual ~Enllac() = default;
        virtual mibase::sinte::Base& sinte() = 0;
        virtual juce::Colour colorSeccio (int s) = 0;
        virtual juce::Colour accent() = 0;
        virtual float previsualitza (const std::string& estat, float* sortida, int n) = 0;
        virtual juce::String textPrevisualitzacio() = 0;
        virtual void comenca (int p) = 0;
        virtual void canvi (int p, float v01) = 0;
        virtual void acaba (int p) = 0;
        virtual void menuControl (int p, juce::Component& c) = 0;
        virtual void info (const juce::String& t) = 0;
    };

    class ControlSB : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
    {
    public:
        ControlSB (Enllac& e, int param, juce::Colour c) : enllac (e), p (param), color (c)
        {
            setTooltip (u8 (enllac.sinte().info (p).ajuda));
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }
        int param() const noexcept { return p; }
        float v01() const { return enllac.sinte().parametre (p); }
        float real() const { return enllac.sinte().valor (p); }
        juce::String text() const { return u8 (enllac.sinte().textValor (p, real())); }
        bool aprenent() const { return enllac.sinte().aprenentCC() == p; }
        int cc() const { return enllac.sinte().ccDeParam (p); }
        void posa01 (float v)
        {
            v = juce::jlimit (0.0f, 1.0f, v);
            enllac.canvi (p, v);
            enllac.info (u8 (enllac.sinte().info (p).nom) + ": " + text());
            repaint();
        }
        void reinicia()
        {
            enllac.comenca (p);
            posa01 (mibase::sinte::normal (enllac.sinte().info (p), enllac.sinte().info (p).def));
            enllac.acaba (p);
        }
        bool menu (const juce::MouseEvent& e)
        {
            if (auto* pare = getParentComponent()) pare->grabKeyboardFocus();   // Ctrl+Z i Espai, a la finestra del sinte
            if (! e.mods.isPopupMenu()) return false;
            enllac.menuControl (p, *this);
            return true;
        }
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
        {
            if (! roda) { roda = true; enllac.comenca (p); }
            const auto& inf = enllac.sinte().info (p);
            float v = v01();
            if (inf.escala == mibase::sinte::Enter) v = mibase::sinte::normal (enllac.sinte().info (p), real() + (w.deltaY > 0 ? 1.0f : -1.0f));
            else v += w.deltaY * 0.06f;
            posa01 (v);
            startTimer (450);
        }
        void mouseEnter (const juce::MouseEvent&) override { sobre = true; repaint(); }
        void mouseExit (const juce::MouseEvent&) override { sobre = false; repaint(); }
    protected:
        Enllac& enllac;
        const int p;
        juce::Colour color;
        bool sobre = false, arrossega = false, roda = false;
        void timerCallback() override { stopTimer(); if (roda) { roda = false; enllac.acaba (p); } }
        void dibuixaCC (juce::Graphics& g, juce::Rectangle<float> r)
        {
            const bool apr = aprenent();
            const int c = cc();
            if (! apr && c < 0) return;
            g.setFont (dibuix::mono (8.5f));
            const auto t = apr ? juce::String ("MIDI?") : "CC" + juce::String (c);
            const auto rr = juce::Rectangle<float> (r.getRight() - 30.0f, r.getY(), 30.0f, 11.0f);
            const float pols = apr ? 0.5f + 0.5f * std::sin ((float) juce::Time::getMillisecondCounter() * 0.012f) : 1.0f;
            g.setColour ((apr ? sb::lcd : enllac.accent()).withAlpha (0.25f * pols));
            g.fillRoundedRectangle (rr, 3.0f);
            g.setColour ((apr ? sb::lcd : enllac.accent()).withAlpha (0.9f * pols));
            g.drawText (t, rr, juce::Justification::centred);
        }
    };

    //==========================================================================
    /** Pom. Tres estils: Gran (alumini amb anell de llums), Faldilla (negre amb faldilla, estil anys 70), Petit (tapa de color). */
    class PomSB : public ControlSB
    {
    public:
        enum class Estil { Gran, Faldilla, Petit, Crom, Bakelita };
        PomSB (Enllac& e, int param, juce::Colour c, Estil s, juce::String etiqueta = {})
            : ControlSB (e, param, c), estil (s), nom (etiqueta.isEmpty() ? u8 (e.sinte().info (param).nom) : etiqueta)
        {
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            const auto& inf = enllac.sinte().info (p);
            const bool bipolar = inf.mn < 0.0f && inf.mx > 0.0f;
            // etiqueta
            g.setColour (sb::serigrafia);
            g.setFont (dibuix::lletra (estil == Estil::Gran || estil == Estil::Crom ? 11.5f : estil == Estil::Petit ? 9.0f : 9.5f, true));
            textEspaiat (g, dibuix::majuscules (nom), r.removeFromTop (14.0f), juce::Justification::centred, estil == Estil::Petit ? 0.3f : 0.8f);
            auto valR = r.removeFromBottom (14.0f);
            const float mida = juce::jmin (r.getWidth(), r.getHeight());
            auto c = r.withSizeKeepingCentre (mida, mida);
            const auto centre = c.getCentre();
            const float pi = juce::MathConstants<float>::pi;
            const float v = v01();
            const float a0 = -0.75f * pi, a1 = 0.75f * pi;
            const float av = a0 + v * (a1 - a0);
            const float azero = bipolar ? a0 + mibase::sinte::normal (enllac.sinte().info (p), 0.0f) * (a1 - a0) : a0;
            const bool actiu = arrossega || roda;

            if (estil == Estil::Gran)
            {
                // anell de 31 llums
                const float rl = c.getWidth() * 0.5f - 3.0f;
                for (int i = 0; i < 31; ++i)
                {
                    const float a = a0 + (a1 - a0) * i / 30.0f;
                    const bool encesa = bipolar ? (a >= juce::jmin (azero, av) - 0.01f && a <= juce::jmax (azero, av) + 0.01f) : a <= av + 0.01f;
                    const auto pt = centre.getPointOnCircumference (rl, a);
                    if (encesa)
                    {
                        g.setColour (color.withAlpha (0.25f));
                        g.fillEllipse (pt.x - 4.5f, pt.y - 4.5f, 9.0f, 9.0f);
                        g.setColour (color.brighter (0.3f));
                    }
                    else g.setColour (juce::Colour (0xff121316));
                    g.fillEllipse (pt.x - 2.1f, pt.y - 2.1f, 4.2f, 4.2f);
                }
                auto cos = c.reduced (c.getWidth() * 0.16f);
                // ombra i vora moletejada
                g.setColour (juce::Colours::black.withAlpha (0.55f));
                g.fillEllipse (cos.translated (0.0f, 4.0f).expanded (2.0f));
                g.setColour (juce::Colour (0xff2b2c31));
                g.fillEllipse (cos);
                g.setColour (juce::Colour (0xff6d6f78));
                for (int i = 0; i < 60; ++i)
                {
                    const float a = av + i * pi * 2.0f / 60.0f;
                    g.drawLine ({ centre.getPointOnCircumference (cos.getWidth() * 0.5f - 0.5f, a), centre.getPointOnCircumference (cos.getWidth() * 0.5f - 5.0f, a) }, 1.0f);
                }
                // cara d'alumini
                auto cara = cos.reduced (6.0f);
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd9dbe2), cara.getX(), cara.getY(), juce::Colour (0xff5f626b), cara.getRight(), cara.getBottom(), false));
                g.fillEllipse (cara);
                juce::ColourGradient brill (juce::Colours::white.withAlpha (0.0f), centre.x, centre.y, juce::Colours::white.withAlpha (0.0f), cara.getRight(), centre.y, true);
                brill.addColour (0.55, juce::Colours::white.withAlpha (0.18f));
                g.setGradientFill (brill);
                g.fillEllipse (cara);
                g.setColour (juce::Colours::black.withAlpha (0.35f));
                g.drawEllipse (cara, 1.0f);
                // ranura i punt de color
                g.setColour (juce::Colour (0xff202227));
                g.drawLine ({ centre.getPointOnCircumference (cara.getWidth() * 0.12f, av), centre.getPointOnCircumference (cara.getWidth() * 0.47f, av) }, 3.0f);
                const auto pt = centre.getPointOnCircumference (cara.getWidth() * 0.36f, av);
                g.setColour (color.withAlpha (0.35f));
                g.fillEllipse (pt.x - 5.0f, pt.y - 5.0f, 10.0f, 10.0f);
                g.setColour (color);
                g.fillEllipse (pt.x - 2.6f, pt.y - 2.6f, 5.2f, 5.2f);
            }
            else if (estil == Estil::Faldilla)
            {
                // escala serigrafiada 0..10
                const float re = c.getWidth() * 0.5f - 1.0f;
                g.setColour (sb::serigrafiaSuau);
                for (int i = 0; i <= 10; ++i)
                {
                    const float a = a0 + (a1 - a0) * i / 10.0f;
                    g.drawLine ({ centre.getPointOnCircumference (re - 4.0f, a), centre.getPointOnCircumference (re, a) }, i % 5 == 0 ? 1.4f : 0.9f);
                }
                auto falda = c.reduced (c.getWidth() * 0.1f);
                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.fillEllipse (falda.translated (0.0f, 3.0f));
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3b41), falda.getX(), falda.getY(), juce::Colour (0xff0d0d10), falda.getX(), falda.getBottom(), false));
                g.fillEllipse (falda);
                // ratlles de la faldilla (giren amb el pom)
                g.setColour (juce::Colours::white.withAlpha (0.55f));
                for (int i = 0; i < 11; ++i)
                {
                    const float a = av + (a1 - a0) * (i - 5) / 10.0f;
                    if (std::abs (a - av) < 0.05f) continue;
                    g.drawLine ({ centre.getPointOnCircumference (falda.getWidth() * 0.5f - 1.0f, a), centre.getPointOnCircumference (falda.getWidth() * 0.5f - 4.0f, a) }, 0.9f);
                }
                auto tapa = falda.reduced (falda.getWidth() * 0.2f);
                g.setColour (juce::Colours::black.withAlpha (0.5f));
                g.fillEllipse (tapa.translated (0.0f, 2.0f));
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2d33), tapa.getX(), tapa.getY(), juce::Colour (0xff121216), tapa.getRight(), tapa.getBottom(), false));
                g.fillEllipse (tapa);
                auto metall = tapa.reduced (tapa.getWidth() * 0.18f);
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xffc9cbd2), metall.getX(), metall.getY(), juce::Colour (0xff6a6c74), metall.getRight(), metall.getBottom(), false));
                g.fillEllipse (metall);
                // indicador blanc a la faldilla i línia a la tapa
                g.setColour (juce::Colours::white);
                g.drawLine ({ centre.getPointOnCircumference (tapa.getWidth() * 0.5f, av), centre.getPointOnCircumference (falda.getWidth() * 0.5f - 1.0f, av) }, 2.2f);
                g.setColour (juce::Colour (0xff1a1a1e));
                g.drawLine ({ centre.getPointOnCircumference (metall.getWidth() * 0.1f, av), centre.getPointOnCircumference (metall.getWidth() * 0.5f, av) }, 1.6f);
                if (actiu || sobre)
                {
                    g.setColour (color.withAlpha (actiu ? 0.8f : 0.4f));
                    juce::Path arc;
                    arc.addCentredArc (centre.x, centre.y, re + 1.5f, re + 1.5f, 0.0f, juce::jmin (azero, av), juce::jmax (azero, av), true);
                    g.strokePath (arc, juce::PathStrokeType (2.0f));
                }
            }
            else if (estil == Estil::Crom)
            {
                // 0.89: crom amb un arc de llum continu (LEAD)
                const float ra = c.getWidth() * 0.5f - 3.0f;
                juce::Path fons;
                fons.addCentredArc (centre.x, centre.y, ra, ra, 0.0f, a0, a1, true);
                g.setColour (juce::Colour (0xff07070c));
                g.strokePath (fons, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                if (std::abs (av - azero) > 0.01f)
                {
                    juce::Path arc;
                    arc.addCentredArc (centre.x, centre.y, ra, ra, 0.0f, juce::jmin (azero, av), juce::jmax (azero, av), true);
                    g.setColour (color.withAlpha (0.25f));
                    g.strokePath (arc, juce::PathStrokeType (9.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    g.setColour (color.brighter (actiu ? 0.5f : 0.2f));
                    g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }
                auto cos = c.reduced (c.getWidth() * 0.17f);
                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.fillEllipse (cos.translated (0.0f, 4.0f).expanded (1.5f));
                juce::ColourGradient crom (juce::Colour (0xfff4f5f8), cos.getX(), cos.getY(), juce::Colour (0xff3a3c44), cos.getRight(), cos.getBottom(), false);
                crom.addColour (0.45, juce::Colour (0xff9a9ca6));
                crom.addColour (0.55, juce::Colour (0xffd8dae0));
                g.setGradientFill (crom);
                g.fillEllipse (cos);
                for (int i = 0; i < 48; ++i)
                {
                    const float a = av + i * pi * 2.0f / 48.0f;
                    g.setColour ((i % 2 ? juce::Colours::white : juce::Colours::black).withAlpha (0.12f));
                    g.drawLine ({ centre.getPointOnCircumference (cos.getWidth() * 0.5f - 0.5f, a), centre.getPointOnCircumference (cos.getWidth() * 0.5f - 4.0f, a) }, 1.0f);
                }
                auto cara = cos.reduced (cos.getWidth() * 0.16f);
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xffc8cad2), cara.getCentreX(), cara.getY(), juce::Colour (0xff6e707a), cara.getCentreX(), cara.getBottom(), false));
                g.fillEllipse (cara);
                g.setColour (juce::Colours::white.withAlpha (0.35f));
                g.drawEllipse (cara.reduced (0.5f), 0.8f);
                g.setColour (juce::Colour (0xff15161b));
                g.drawLine ({ centre.getPointOnCircumference (cara.getWidth() * 0.15f, av), centre.getPointOnCircumference (cos.getWidth() * 0.5f - 2.0f, av) }, 2.4f);
            }
            else if (estil == Estil::Bakelita)
            {
                // 0.89: pom negre pla amb anell de metall i línia blanca (estil polifònic dels 80)
                const float re = c.getWidth() * 0.5f - 1.0f;
                for (int i = 0; i <= 10; ++i)
                {
                    const float a = a0 + (a1 - a0) * i / 10.0f;
                    const auto pt = centre.getPointOnCircumference (re - 1.5f, a);
                    const bool encesa = bipolar ? (a >= juce::jmin (azero, av) - 0.01f && a <= juce::jmax (azero, av) + 0.01f) : a <= av + 0.01f;
                    g.setColour (encesa ? color : sb::serigrafiaSuau.withAlpha (0.5f));
                    g.fillEllipse (pt.x - 1.4f, pt.y - 1.4f, 2.8f, 2.8f);
                }
                auto cos = c.reduced (c.getWidth() * 0.14f);
                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.fillEllipse (cos.translated (0.0f, 3.0f));
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff35363d), cos.getX(), cos.getY(), juce::Colour (0xff101014), cos.getX(), cos.getBottom(), false));
                g.fillEllipse (cos);
                auto anell = cos.reduced (cos.getWidth() * 0.16f);
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xffb8bac2), anell.getX(), anell.getY(), juce::Colour (0xff50525a), anell.getRight(), anell.getBottom(), false));
                g.drawEllipse (anell, 2.0f);
                g.setColour (juce::Colour (0xff1c1d22));
                g.fillEllipse (anell.reduced (1.5f));
                g.setColour (juce::Colours::white.withAlpha (0.92f));
                g.drawLine ({ centre.getPointOnCircumference (anell.getWidth() * 0.12f, av), centre.getPointOnCircumference (cos.getWidth() * 0.5f - 2.0f, av) }, 2.0f);
                if (actiu || sobre) { g.setColour (color.withAlpha (actiu ? 0.6f : 0.3f)); g.drawEllipse (cos.expanded (1.5f), 1.2f); }
            }
            else
            {
                const float ra = c.getWidth() * 0.5f - 2.0f;
                juce::Path fons;
                fons.addCentredArc (centre.x, centre.y, ra, ra, 0.0f, a0, a1, true);
                g.setColour (juce::Colour (0xff0b0b0d));
                g.strokePath (fons, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                if (std::abs (av - azero) > 0.01f)
                {
                    juce::Path arc;
                    arc.addCentredArc (centre.x, centre.y, ra, ra, 0.0f, juce::jmin (azero, av), juce::jmax (azero, av), true);
                    g.setColour (color.withAlpha (actiu ? 1.0f : 0.85f));
                    g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }
                auto cos = c.reduced (c.getWidth() * 0.2f);
                g.setColour (juce::Colours::black.withAlpha (0.5f));
                g.fillEllipse (cos.translated (0.0f, 2.0f));
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4b53), cos.getX(), cos.getY(), juce::Colour (0xff17171b), cos.getX(), cos.getBottom(), false));
                g.fillEllipse (cos);
                g.setColour (juce::Colours::white.withAlpha (0.12f));
                g.drawEllipse (cos.reduced (0.5f), 1.0f);
                const auto pt = centre.getPointOnCircumference (cos.getWidth() * 0.33f, av);
                g.setColour (color);
                g.fillEllipse (pt.x - 2.3f, pt.y - 2.3f, 4.6f, 4.6f);
            }
            // valor
            g.setFont (dibuix::mono (estil == Estil::Gran || estil == Estil::Crom ? 11.0f : 9.5f));
            g.setColour (actiu ? color.brighter (0.4f) : sobre ? sb::serigrafia : sb::serigrafiaSuau);
            g.drawText (text(), valR, juce::Justification::centred);
            dibuixaCC (g, getLocalBounds().toFloat().withTrimmedTop (12.0f));
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (menu (e)) return;
            if (e.mods.isCommandDown() || e.mods.isAltDown()) { reinicia(); bloc = true; return; }
            bloc = false;
            inici = v01();
            continu = inici;
            arrossega = false;
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (bloc || e.mods.isPopupMenu()) return;
            if (! arrossega && e.getDistanceFromDragStart() < 2) return;
            if (! arrossega) { arrossega = true; enllac.comenca (p); }
            const float sens = (e.mods.isShiftDown() ? 0.1f : 1.0f) / 200.0f;
            continu = inici - (float) e.getDistanceFromDragStartY() * sens;
            float v = juce::jlimit (0.0f, 1.0f, continu);
            if (enllac.sinte().info (p).escala == mibase::sinte::Enter) v = mibase::sinte::normal (enllac.sinte().info (p), mibase::sinte::deNormal (enllac.sinte().info (p), v));
            posa01 (v);
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            if (arrossega) { arrossega = false; enllac.acaba (p); repaint(); }
        }
        void mouseDoubleClick (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) reinicia(); }

    private:
        Estil estil;
        juce::String nom;
        float inici = 0.0f, continu = 0.0f;
        bool bloc = false;
    };

    //==========================================================================
    /** Selector de tecles amb llum (ones, models de filtre, modes…). Amb icones d'ona si «ones». */
    class SelectorSB : public ControlSB
    {
    public:
        SelectorSB (Enllac& e, int param, juce::Colour c, bool icones = false, juce::String etiqueta = {})
            : ControlSB (e, param, c), ones (icones), titol (etiqueta)
        {
            for (auto& o : mibase::sinte::opcions (e.sinte().info (param))) textos.add (u8 (o));
        }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            if (titol.isNotEmpty())
            {
                g.setColour (sb::serigrafia);
                g.setFont (dibuix::lletra (9.5f, true));
                textEspaiat (g, dibuix::majuscules (titol), r.removeFromTop (14.0f), juce::Justification::centredLeft, 0.8f);
            }
            const int n = textos.size();
            const int tria = (int) std::lround (real() - enllac.sinte().info (p).mn);
            const float w = r.getWidth() / (float) n;
            if (desplegable())
            {
                // 0.93: si no hi caben totes les opcions, un desplegable (clic = llista; roda = la següent)
                auto b = r.reduced (1.0f, 0.0f);
                g.setColour (juce::Colours::black.withAlpha (0.55f));
                g.fillRoundedRectangle (b.translated (0.0f, 1.5f), 3.5f);
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c2d34), b.getX(), b.getY(), juce::Colour (0xff17181c), b.getX(), b.getBottom(), false));
                g.fillRoundedRectangle (b, 3.5f);
                const bool cap = tria == 0 && (textos[0] == "Cap" || textos[0] == "Sempre");
                g.setColour (color.withAlpha (cap ? 0.25f : 0.7f));
                g.drawRoundedRectangle (b.reduced (0.5f), 3.5f, 1.0f);
                auto fl = b.removeFromRight (16.0f).withSizeKeepingCentre (8.0f, 5.0f);
                juce::Path t;
                t.addTriangle (fl.getX(), fl.getY(), fl.getRight(), fl.getY(), fl.getCentreX(), fl.getBottom());
                g.setColour (sb::serigrafiaSuau);
                g.fillPath (t);
                g.setColour (cap ? sb::serigrafiaSuau : color.brighter (0.35f));
                g.setFont (dibuix::lletra (juce::jmin (11.0f, b.getHeight() * 0.48f), true));
                g.drawFittedText (textos[juce::jlimit (0, n - 1, tria)], b.reduced (6.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
                dibuixaCC (g, getLocalBounds().toFloat());
                return;
            }
            for (int i = 0; i < n; ++i)
            {
                auto b = juce::Rectangle<float> (r.getX() + i * w, r.getY(), w, r.getHeight()).reduced (1.5f, 0.0f);
                const bool on = i == tria;
                g.setColour (juce::Colours::black.withAlpha (0.55f));
                g.fillRoundedRectangle (b.translated (0.0f, 1.5f), 3.5f);
                g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xff34363e) : juce::Colour (0xff2a2b31), b.getX(), b.getY(),
                                                         on ? juce::Colour (0xff1d1e23) : juce::Colour (0xff15161a), b.getX(), b.getBottom(), false));
                g.fillRoundedRectangle (b, 3.5f);
                if (on)
                {
                    g.setColour (color.withAlpha (0.16f));
                    g.fillRoundedRectangle (b, 3.5f);
                    g.setColour (color.withAlpha (0.8f));
                    g.drawRoundedRectangle (b.reduced (0.5f), 3.5f, 1.0f);
                }
                else
                {
                    g.setColour (juce::Colours::white.withAlpha (0.07f));
                    g.drawRoundedRectangle (b.reduced (0.5f), 3.5f, 1.0f);
                }
                const auto col = on ? color.brighter (0.35f) : sb::serigrafiaSuau;
                if (ones)
                {
                    auto ic = b.reduced (b.getWidth() * 0.22f, b.getHeight() * 0.28f);
                    g.setColour (col);
                    g.strokePath (iconaOna (iconaDeNom (textos[i]), ic), juce::PathStrokeType (on ? 1.8f : 1.3f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
                }
                else
                {
                    g.setColour (col);
                    g.setFont (dibuix::lletra (juce::jmin (11.0f, b.getHeight() * 0.48f), true));
                    g.drawFittedText (textos[i], b.reduced (2.0f, 0.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
                }
            }
            dibuixaCC (g, getLocalBounds().toFloat());
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (menu (e)) return;
            auto r = getLocalBounds().toFloat();
            if (titol.isNotEmpty()) r.removeFromTop (14.0f);
            if (desplegable())
            {
                const int tria = (int) std::lround (real() - enllac.sinte().info (p).mn);
                juce::PopupMenu m;
                for (int k = 0; k < textos.size(); ++k) m.addItem (k + 1, textos[k], true, k == tria);
                juce::Component::SafePointer<SelectorSB> jo (this);
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [jo] (int res)
                {
                    if (jo == nullptr || res <= 0) return;
                    auto& en = jo->enllac;
                    en.comenca (jo->p);
                    jo->posa01 (mibase::sinte::normal (en.sinte().info (jo->p), en.sinte().info (jo->p).mn + (float) (res - 1)));
                    en.acaba (jo->p);
                });
                return;
            }
            const int i = juce::jlimit (0, textos.size() - 1, (int) ((e.position.x - r.getX()) / (r.getWidth() / textos.size())));
            enllac.comenca (p);
            posa01 (mibase::sinte::normal (enllac.sinte().info (p), enllac.sinte().info (p).mn + (float) i));
            enllac.acaba (p);
        }
        juce::String getTooltip() override
        {
            if (desplegable()) return u8 (enllac.sinte().info (p).nom) + " · "_ca + u8 (enllac.sinte().info (p).ajuda);
            const auto m = getMouseXYRelative().toFloat();
            const int i = juce::jlimit (0, juce::jmax (0, textos.size() - 1), (int) (m.x / (getWidth() / (float) juce::jmax (1, textos.size()))));
            return textos[i] + " · "_ca + u8 (enllac.sinte().info (p).ajuda);
        }
    private:
        bool ones;
        juce::String titol;
        juce::StringArray textos;
        bool desplegable() const { return ! ones && textos.size() > 2 && (float) getWidth() / (float) textos.size() < 30.0f; }
    };

    //==========================================================================
    /** Interruptor de palanca amb llum. */
    class InterruptorSB : public ControlSB
    {
    public:
        InterruptorSB (Enllac& e, int param, juce::Colour c, juce::String etiqueta = {})
            : ControlSB (e, param, c), nom (etiqueta.isEmpty() ? u8 (e.sinte().info (param).nom) : etiqueta) {}
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            const bool on = real() > 0.5f;
            g.setColour (sb::serigrafia);
            g.setFont (dibuix::lletra (9.5f, true));
            textEspaiat (g, dibuix::majuscules (nom), r.removeFromBottom (13.0f), juce::Justification::centred, 0.6f);
            auto led = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ r.getCentreX(), r.getY() + 5.0f });
            if (on) { g.setColour (color.withAlpha (0.3f)); g.fillEllipse (led.expanded (4.0f)); }
            g.setColour (on ? color.brighter (0.3f) : juce::Colour (0xff1c1c20));
            g.fillEllipse (led);
            r.removeFromTop (12.0f);
            auto placa = r.withSizeKeepingCentre (juce::jmin (26.0f, r.getWidth()), juce::jmin (34.0f, r.getHeight()));
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff8d8f97), placa.getX(), placa.getY(), juce::Colour (0xff3b3c42), placa.getRight(), placa.getBottom(), false));
            g.fillRoundedRectangle (placa, 4.0f);
            auto forat = placa.withSizeKeepingCentre (10.0f, 10.0f);
            g.setColour (juce::Colour (0xff0c0c0e));
            g.fillEllipse (forat);
            // palanca
            const float dir = on ? -1.0f : 1.0f;
            const auto base = forat.getCentre();
            const juce::Point<float> punta (base.x, base.y + dir * placa.getHeight() * 0.42f);
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.drawLine ({ base.translated (1.5f, 1.5f), punta.translated (1.5f, 1.5f) }, 5.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeeeeF2), punta.x - 3.0f, punta.y, juce::Colour (0xff7a7c84), punta.x + 3.0f, punta.y, false));
            g.drawLine ({ base, punta }, 4.5f);
            g.fillEllipse (punta.x - 4.0f, punta.y - 4.0f, 8.0f, 8.0f);
            dibuixaCC (g, getLocalBounds().toFloat());
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (menu (e)) return;
            enllac.comenca (p);
            posa01 (real() > 0.5f ? 0.0f : 1.0f);
            enllac.acaba (p);
        }
    private:
        juce::String nom;
    };

    //==========================================================================
    /** Fader vertical (envolupants), com els dels sintes de baix d'abans. */
    class FaderSB : public ControlSB
    {
    public:
        FaderSB (Enllac& e, int param, juce::Colour c) : ControlSB (e, param, c) { setMouseCursor (juce::MouseCursor::UpDownResizeCursor); }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (sb::serigrafia);
            g.setFont (dibuix::lletra (11.0f, true));
            g.drawText (u8 (enllac.sinte().info (p).nom), r.removeFromBottom (14.0f), juce::Justification::centred);
            auto valR = r.removeFromTop (13.0f);
            g.setFont (dibuix::mono (8.5f));
            g.setColour (arrossega || roda ? color.brighter (0.4f) : sobre ? sb::serigrafia : sb::serigrafiaSuau);
            g.drawFittedText (text(), valR.toNearestInt(), juce::Justification::centred, 1, 0.6f);
            auto pista = r.reduced (0.0f, 8.0f);
            // escala
            g.setColour (sb::serigrafiaSuau.withAlpha (0.6f));
            for (int i = 0; i <= 10; ++i)
            {
                const float y = pista.getBottom() - pista.getHeight() * i / 10.0f;
                const float w = i % 5 == 0 ? 7.0f : 4.0f;
                g.drawLine (pista.getCentreX() - 9.0f - w, y, pista.getCentreX() - 9.0f, y, 0.8f);
                g.drawLine (pista.getCentreX() + 9.0f, y, pista.getCentreX() + 9.0f + w, y, 0.8f);
            }
            auto ranura = pista.withSizeKeepingCentre (4.0f, pista.getHeight());
            g.setColour (juce::Colour (0xff070708));
            g.fillRoundedRectangle (ranura, 2.0f);
            const float y = pista.getBottom() - pista.getHeight() * v01();
            // part plena (llum de la secció)
            g.setColour (color.withAlpha (0.35f));
            g.fillRoundedRectangle (ranura.withTop (y), 2.0f);
            auto cap = juce::Rectangle<float> (22.0f, 14.0f).withCentre ({ pista.getCentreX(), y });
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 2.5f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4b4c54), cap.getX(), cap.getY(), juce::Colour (0xff1a1a1e), cap.getX(), cap.getBottom(), false));
            g.fillRoundedRectangle (cap, 2.5f);
            g.setColour (juce::Colours::white.withAlpha (0.2f));
            g.drawLine (cap.getX() + 3.0f, cap.getY() + 3.5f, cap.getRight() - 3.0f, cap.getY() + 3.5f, 0.8f);
            g.drawLine (cap.getX() + 3.0f, cap.getBottom() - 3.5f, cap.getRight() - 3.0f, cap.getBottom() - 3.5f, 0.8f);
            g.setColour (color);
            g.fillRect (cap.getX() + 2.0f, cap.getCentreY() - 1.0f, cap.getWidth() - 4.0f, 2.0f);
            dibuixaCC (g, getLocalBounds().toFloat());
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (menu (e)) return;
            if (e.mods.isCommandDown() || e.mods.isAltDown()) { reinicia(); bloc = true; return; }
            bloc = false;
            inici = v01();
            arrossega = false;
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (bloc || e.mods.isPopupMenu()) return;
            if (! arrossega && e.getDistanceFromDragStart() < 2) return;
            if (! arrossega) { arrossega = true; enllac.comenca (p); }
            const float alt = juce::jmax (20.0f, (float) getHeight() - 43.0f);
            posa01 (inici - (float) e.getDistanceFromDragStartY() / alt * (e.mods.isShiftDown() ? 0.1f : 1.0f));
        }
        void mouseUp (const juce::MouseEvent&) override { if (arrossega) { arrossega = false; enllac.acaba (p); repaint(); } }
        void mouseDoubleClick (const juce::MouseEvent& e) override { if (! e.mods.isPopupMenu()) reinicia(); }
    private:
        float inici = 0.0f;
        bool bloc = false;
    };

    //==========================================================================
    /** Les dues envolupants dibuixades (volum i filtre). */
    class GraficEnv : public juce::Component
    {
    public:
        /** idx: A, D, S, R del volum i A, D, S, R del filtre. */
        GraficEnv (Enllac& e, std::array<int, 8> i, juce::Colour volum, juce::Colour filtre) : enllac (e), idx (i), colVolum (volum), colFiltre (filtre) {}
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (juce::Colour (0xff08100e));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            for (int i = 1; i < 8; ++i) g.drawVerticalLine ((int) (r.getX() + r.getWidth() * i / 8.0f), r.getY(), r.getBottom());
            auto in = r.reduced (6.0f, 6.0f);
            auto& s = enllac.sinte();
            auto corba = [&] (int a, int d, int st, int rel, juce::Colour c)
            {
                auto amp = [] (float ms) { return std::log1p (ms / 8.0f); };
                const float wa = amp (s.valor (a)), wd = amp (s.valor (d)), wr = amp (s.valor (rel)), ws = 1.2f;
                const float tot = wa + wd + ws + wr;
                const float k = in.getWidth() / tot;
                const float sus = s.valor (st);
                juce::Path p;
                const float y0 = in.getBottom(), y1 = in.getY();
                auto yv = [&] (float v) { return y0 - (y0 - y1) * v; };
                float x = in.getX();
                p.startNewSubPath (x, y0);
                p.quadraticTo (x + wa * k * 0.4f, yv (0.7f), x + wa * k, y1);
                x += wa * k;
                p.quadraticTo (x + wd * k * 0.15f, yv (sus), x + wd * k, yv (sus));
                x += wd * k;
                p.lineTo (x + ws * k, yv (sus));
                x += ws * k;
                p.quadraticTo (x + wr * k * 0.12f, y0, x + wr * k, y0);
                g.setColour (c.withAlpha (0.12f));
                juce::Path pl (p);
                pl.lineTo (in.getRight(), y0);
                pl.closeSubPath();
                g.fillPath (pl);
                g.setColour (c.withAlpha (0.3f));
                g.strokePath (p, juce::PathStrokeType (4.0f));
                g.setColour (c);
                g.strokePath (p, juce::PathStrokeType (1.5f));
            };
            corba (idx[4], idx[5], idx[6], idx[7], colFiltre);
            corba (idx[0], idx[1], idx[2], idx[3], colVolum);
            g.setFont (dibuix::lletra (9.0f, true));
            g.setColour (colVolum);
            g.drawText ("VOLUM", r.reduced (8.0f, 4.0f), juce::Justification::topRight);
            g.setColour (colFiltre);
            g.drawText ("FILTRE", r.reduced (8.0f, 4.0f).withTrimmedTop (12.0f), juce::Justification::topRight);
        }
    private:
        Enllac& enllac;
        std::array<int, 8> idx;
        juce::Colour colVolum, colFiltre;
    };

    //==========================================================================
    /** Pantalla de fòsfor: l'ona que surt (si sona) o la previsualització del so. */
    class Pantalla : public juce::Component
    {
    public:
        explicit Pantalla (Enllac& e) : enllac (e) {}
        void actualitza()
        {
            auto& s = enllac.sinte();
            viu = s.fentSo() && s.nivell() > 1e-3f;
            if (viu)
            {
                const double fs = s.frequenciaMostreig();
                const double hz = juce::jlimit (15.0, 2000.0, (double) s.hzDarrera());
                const int periode = juce::jlimit (8, mibase::sinte::Base::midaOna / 3, (int) std::lround (fs / hz));
                std::vector<float> buf ((size_t) mibase::sinte::Base::midaOna);
                s.copiaOna (buf.data(), mibase::sinte::Base::midaOna);
                // l'últim creuament pujant amb dos cicles al darrere
                int ini = mibase::sinte::Base::midaOna - 2 * periode - 1;
                for (int i = ini; i > periode; --i) if (buf[(size_t) i - 1] < 0.0f && buf[(size_t) i] >= 0.0f) { ini = i; break; }
                punts.resize (240);
                pic = 1e-4f;
                for (int i = 0; i < 240; ++i)
                {
                    const double x = ini + (double) i / 239.0 * 2.0 * periode;
                    const int a = juce::jlimit (0, mibase::sinte::Base::midaOna - 2, (int) x);
                    const float t = (float) (x - a);
                    punts[(size_t) i] = buf[(size_t) a] * (1.0f - t) + buf[(size_t) a + 1] * t;
                    pic = juce::jmax (pic, std::abs (punts[(size_t) i]));
                }
                nota = (int) std::lround (12.0 * std::log2 (hz / 440.0) + 69.0);
                hzText = juce::String (hz, hz < 100.0 ? 1 : 0) + " Hz";
            }
            else
            {
                const auto e = s.estatSo();
                if (e != estatPrevi)
                {
                    estatPrevi = e;
                    previ.assign (240, 0.0f);
                    picPrevi = juce::jmax (1e-4f, enllac.previsualitza (e, previ.data(), 240));
                }
                punts = previ;
                pic = picPrevi;
            }
            repaint();
        }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (juce::Colour (0xff050807));
            g.fillRoundedRectangle (r, 6.0f);
            auto in = r.reduced (4.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0c1d19), in.getCentreX(), in.getCentreY(), juce::Colour (0xff040908), in.getX(), in.getY(), true));
            g.fillRoundedRectangle (in, 5.0f);
            g.setColour (sb::fosfor.withAlpha (0.07f));
            for (int i = 1; i < 8; ++i) g.drawVerticalLine ((int) (in.getX() + in.getWidth() * i / 8.0f), in.getY(), in.getBottom());
            for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (in.getY() + in.getHeight() * i / 4.0f), in.getX(), in.getRight());
            // ratlles de CRT
            g.setColour (juce::Colours::black.withAlpha (0.18f));
            for (float y = in.getY(); y < in.getBottom(); y += 2.0f) g.drawHorizontalLine ((int) y, in.getX(), in.getRight());
            if (! punts.empty())
            {
                const float esc = 0.42f * in.getHeight() / juce::jmax (0.05f, pic);
                juce::Path p;
                for (size_t i = 0; i < punts.size(); ++i)
                {
                    const float x = in.getX() + in.getWidth() * (float) i / (float) (punts.size() - 1);
                    const float y = in.getCentreY() - punts[i] * esc;
                    if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                }
                const auto c = viu ? sb::fosfor : sb::fosfor.withSaturation (0.4f).withAlpha (0.8f);
                g.setColour (c.withAlpha (0.12f));
                g.strokePath (p, juce::PathStrokeType (7.0f));
                g.setColour (c.withAlpha (0.3f));
                g.strokePath (p, juce::PathStrokeType (3.0f));
                g.setColour (c);
                g.strokePath (p, juce::PathStrokeType (1.3f));
            }
            g.setFont (dibuix::mono (9.5f));
            g.setColour (sb::fosfor.withAlpha (0.75f));
            g.drawText (viu ? juce::MidiMessage::getMidiNoteName (nota, true, true, 4) + " · "_ca + hzText : enllac.textPrevisualitzacio(),
                        in.reduced (6.0f, 3.0f), juce::Justification::topLeft);
            // vidre
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), in.getX(), in.getY(), juce::Colours::white.withAlpha (0.0f), in.getX(), in.getCentreY(), false));
            g.fillRoundedRectangle (in, 5.0f);
        }
    private:
        Enllac& enllac;
        std::vector<float> punts, previ;
        float pic = 1.0f, picPrevi = 1.0f;
        bool viu = false;
        int nota = 36;
        juce::String hzText;
        std::string estatPrevi;
    };

    //==========================================================================
    /** Roda de pitch bend (torna al mig) o de modulació. */
    class RodaSB : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        RodaSB (bool bendSiNo, Enllac& e, std::function<void (float)> envia) : bend (bendSiNo), enllac (e), enviar (std::move (envia))
        {
            setTooltip (bend ? "Pitch bend: arrossega amunt o avall; en deixar-la torna al mig"_ca : "Roda de modulació (CC 1): obre el filtre o afegeix LFO, segons la secció Modulació"_ca);
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (sb::serigrafia);
            g.setFont (dibuix::lletra (9.0f, true));
            g.drawText (bend ? "PITCH" : "MOD", r.removeFromBottom (13.0f), juce::Justification::centred);
            const float v = arrossega ? valor : (bend ? enllac.sinte().bend() : enllac.sinte().roda() * 2.0f - 1.0f);
            auto forat = r.reduced (2.0f, 0.0f);
            g.setColour (juce::Colour (0xff050506));
            g.fillRoundedRectangle (forat, 5.0f);
            auto roda = forat.reduced (4.0f, 5.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff101012), roda.getX(), roda.getY(), juce::Colour (0xff101012), roda.getX(), roda.getBottom(), false));
            juce::ColourGradient cil (juce::Colour (0xff0c0c0e), roda.getX(), roda.getY(), juce::Colour (0xff0c0c0e), roda.getX(), roda.getBottom(), false);
            cil.addColour (0.5, juce::Colour (0xff3d3e45));
            g.setGradientFill (cil);
            g.fillRoundedRectangle (roda, 3.0f);
            // estries que es mouen
            const float desp = -v * roda.getHeight() * 0.35f;
            for (int i = -12; i <= 12; ++i)
            {
                const float y = roda.getCentreY() + i * 6.0f + std::fmod (desp, 6.0f);
                if (y < roda.getY() + 2.0f || y > roda.getBottom() - 2.0f) continue;
                const float prof = 1.0f - std::abs (y - roda.getCentreY()) / (roda.getHeight() * 0.5f);
                g.setColour (juce::Colours::black.withAlpha (0.5f * prof));
                g.drawHorizontalLine ((int) y, roda.getX() + 1.0f, roda.getRight() - 1.0f);
                g.setColour (juce::Colours::white.withAlpha (0.10f * prof));
                g.drawHorizontalLine ((int) y + 1, roda.getX() + 1.0f, roda.getRight() - 1.0f);
            }
            const float ym = roda.getCentreY() + desp;
            if (ym > roda.getY() && ym < roda.getBottom())
            {
                g.setColour (enllac.accent().withAlpha (0.9f));
                g.fillRect (roda.getX() + 1.0f, ym - 1.5f, roda.getWidth() - 2.0f, 3.0f);
            }
        }
        void mouseDown (const juce::MouseEvent&) override { arrossega = true; inici = bend ? 0.0f : enllac.sinte().roda() * 2.0f - 1.0f; valor = inici; }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            valor = juce::jlimit (-1.0f, 1.0f, inici - (float) e.getDistanceFromDragStartY() / (getHeight() * 0.5f));
            enviar (valor);
            repaint();
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            arrossega = false;
            if (bend) { valor = 0.0f; enviar (0.0f); }
            repaint();
        }
    private:
        bool bend;
        Enllac& enllac;
        std::function<void (float)> enviar;
        bool arrossega = false;
        float valor = 0.0f, inici = 0.0f;
    };

    //==========================================================================
    /** Teclat de pantalla: més avall a la tecla, més fort. Les tecles que sonen s'encenen (també les del teclat MIDI). */
    class TeclatSB : public juce::Component
    {
    public:
        TeclatSB (Enllac& e, std::function<void (int, int)> on, std::function<void (int)> off) : enllac (e), noteOn (std::move (on)), noteOff (std::move (off)) {}
        int base = 24;   // C1
        int octaves = 4;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (juce::Colour (0xff060607));
            g.fillRoundedRectangle (r, 4.0f);
            auto in = r.reduced (3.0f, 3.0f);
            const int blanques = octaves * 7 + 1;
            const float w = in.getWidth() / blanques;
            static const int blancaTo[] = { 0, 2, 4, 5, 7, 9, 11 };
            for (int i = 0; i < blanques; ++i)
            {
                const int nota = base + (i / 7) * 12 + blancaTo[i % 7];
                auto k = juce::Rectangle<float> (in.getX() + i * w, in.getY(), w - 1.0f, in.getHeight());
                const bool on = enllac.sinte().notaSona (nota) || nota == premuda;
                g.setGradientFill (juce::ColourGradient (on ? enllac.accent().interpolatedWith (juce::Colours::white, 0.55f) : juce::Colour (0xfff3f1ea), k.getX(), k.getY(),
                                                         on ? enllac.accent().interpolatedWith (juce::Colours::white, 0.2f) : juce::Colour (0xffc9c6bd), k.getX(), k.getBottom(), false));
                g.fillRoundedRectangle (k, 2.5f);
                g.setColour (juce::Colours::black.withAlpha (0.35f));
                g.drawRoundedRectangle (k, 2.5f, 0.8f);
                if (i % 7 == 0)
                {
                    g.setColour (juce::Colour (0xff6b675e));
                    g.setFont (dibuix::lletra (9.0f, true));
                    g.drawText (juce::MidiMessage::getMidiNoteName (nota, true, true, 4), k.withTrimmedBottom (3.0f), juce::Justification::centredBottom);
                }
            }
            static const int negraDespres[] = { 0, 1, 3, 4, 5 };   // índex de blanca darrere la qual hi ha una negra
            static const int negraTo[] = { 1, 3, 6, 8, 10 };
            for (int o = 0; o < octaves; ++o)
                for (int k2 = 0; k2 < 5; ++k2)
                {
                    const int nota = base + o * 12 + negraTo[k2];
                    const float x = in.getX() + (o * 7 + negraDespres[k2] + 1) * w - w * 0.32f;
                    auto k = juce::Rectangle<float> (x, in.getY(), w * 0.64f, in.getHeight() * 0.6f);
                    const bool on = enllac.sinte().notaSona (nota) || nota == premuda;
                    g.setColour (juce::Colours::black.withAlpha (0.5f));
                    g.fillRoundedRectangle (k.translated (1.5f, 1.5f), 2.0f);
                    g.setGradientFill (juce::ColourGradient (on ? enllac.accent().darker (0.2f) : juce::Colour (0xff2c2c31), k.getX(), k.getY(),
                                                             on ? enllac.accent().darker (0.8f) : juce::Colour (0xff09090a), k.getX(), k.getBottom(), false));
                    g.fillRoundedRectangle (k, 2.0f);
                    g.setColour (juce::Colours::white.withAlpha (0.12f));
                    g.fillRoundedRectangle (k.reduced (2.5f, 0.0f).withHeight (k.getHeight() * 0.85f).withTrimmedTop (2.0f).withWidth (2.0f), 1.0f);
                }
        }
        int notaA (juce::Point<float> p, int& vel) const
        {
            auto in = getLocalBounds().toFloat().reduced (3.0f, 3.0f);
            const int blanques = octaves * 7 + 1;
            const float w = in.getWidth() / blanques;
            vel = juce::jlimit (30, 127, (int) (40.0f + 87.0f * (p.y - in.getY()) / in.getHeight()));
            static const int negraDespres[] = { 0, 1, 3, 4, 5 };
            static const int negraTo[] = { 1, 3, 6, 8, 10 };
            if (p.y < in.getY() + in.getHeight() * 0.6f)
                for (int o = 0; o < octaves; ++o)
                    for (int k = 0; k < 5; ++k)
                    {
                        const float x = in.getX() + (o * 7 + negraDespres[k] + 1) * w - w * 0.32f;
                        if (p.x >= x && p.x < x + w * 0.64f) return base + o * 12 + negraTo[k];
                    }
            static const int blancaTo[] = { 0, 2, 4, 5, 7, 9, 11 };
            const int i = juce::jlimit (0, blanques - 1, (int) ((p.x - in.getX()) / w));
            return base + (i / 7) * 12 + blancaTo[i % 7];
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            int vel = 100;
            premuda = notaA (e.position, vel);
            noteOn (premuda, vel);
            repaint();
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            int vel = 100;
            const int n = notaA (e.position, vel);
            if (n == premuda || premuda < 0) return;
            const int vella = premuda;
            premuda = n;
            noteOn (n, vel);      // primer la nova: en legato, llisca
            noteOff (vella);
            repaint();
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            if (premuda >= 0) noteOff (premuda);
            premuda = -1;
            repaint();
        }
    private:
        Enllac& enllac;
        std::function<void (int, int)> noteOn;
        std::function<void (int)> noteOff;
        int premuda = -1;
    };

    //==========================================================================
    //==========================================================================
    using Preset = mibase::drive::Preset;
    using BibliotecaPresets = mibase::drive::BibliotecaPresets;
    struct Seccio { int id; juce::String titol; juce::Rectangle<int> r; };

    /** El que fa diferent cada sinte (dades i aspecte). */
    struct ConfigSinte
    {
        juce::String marca, subtitol;                    // «SUB & BASS», «SINTETITZADOR DE BAIXOS»
        juce::String carpeta, extensio, tipusFitxer;     // «SubBass», «mbsub», «mibase-subbass»
        const std::vector<Preset>* presets = nullptr;
        const std::vector<std::string>* categories = nullptr;
        std::function<std::string()> inicial;
        std::function<std::string (unsigned)> aleatori;
        /** 0.90 (opcional): sons nous a l'atzar d'un tipus concret (ATMOS: Analog Warm, Cinematic…). */
        std::vector<std::string> tipusAtzar;
        std::function<std::string (unsigned, int)> aleatoriTipus;
        std::function<std::string (const std::string&, unsigned, float)> varia;
        std::function<float (const std::string&, float*, int)> previsualitza;
        juce::String textPrevi;
        std::array<int, 8> akai {};
        juce::String textAkai;
        std::function<juce::Colour (int)> colorSeccio;
        juce::Colour accent, colorMarca, colorFranja;
        Textura fons = Textura::Metall, galtes = Textura::Fusta, capcalera = Textura::Carboni;
        juce::Colour seccioDalt { 0xff24262d }, seccioBaix { 0xff17181c };
        int ampleGaltes = 26, altCap = 74, ample = 1260, alt = 812;
        /** 1.4 (VINTAGE): aspecte d'aparell antic: el nom del preset en una etiqueta de paper, seccions serigrafiades
            (línia i títol, sense caixes ni llums) i cap pantalla de colors. */
        bool vintage = false;
    };

    inline BibliotecaPresets& biblioteca (const juce::String& carpeta)
    {
        static std::map<juce::String, std::unique_ptr<BibliotecaPresets>> m;
        auto& b = m[carpeta];
        if (b == nullptr)
        {
            b = std::make_unique<BibliotecaPresets>();
            b->carrega (dibuix::cami (juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Mibase").getChildFile (carpeta).getChildFile ("presets.json")));
        }
        return *b;
    }

    /** La base de la finestra d'un sinte: el que tots fan igual. */
    class PanellSinte : public juce::Component, public Enllac, public OrigenPresets, private juce::Timer
    {
    public:
        PanellSinte (std::shared_ptr<mibase::sinte::Base> s, EstatEstudi& e, GestorPlugins* gp, std::string idPista, ConfigSinte c,
                     std::function<void (std::uint8_t, std::uint8_t, std::uint8_t)> dir, std::function<void()> tanca)
            : cfg (std::move (c)), sint (std::move (s)), estat (e), plugins (gp), id (std::move (idPista)), directe (std::move (dir)), tancar (std::move (tanca)),
              pantalla (*this),
              rodaBend (true, *this, [this] (float v) { const int b = juce::jlimit (0, 16383, 8192 + (int) std::lround (v * 8191.0f)); envia (0xE0, (std::uint8_t) (b & 127), (std::uint8_t) (b >> 7)); }),
              rodaMod (false, *this, [this] (float v) { envia (0xB0, 1, (std::uint8_t) juce::jlimit (0, 127, (int) std::lround ((v * 0.5f + 0.5f) * 127.0f))); }),
              teclat (*this, [this] (int n, int v) { envia (0x90, (std::uint8_t) n, (std::uint8_t) v); }, [this] (int n) { envia (0x80, (std::uint8_t) n, 0); })
        {
            darrers.assign ((size_t) sint->numParametres(), 0.0f);
            for (int i = 0; i < sint->numParametres(); ++i) darrers[(size_t) i] = sint->valor (i);
            addAndMakeVisible (pantalla);
            for (auto* c2 : std::initializer_list<juce::Component*> { &rodaBend, &rodaMod, &teclat, &btOctMenys, &btOctMes }) addAndMakeVisible (c2);
            btOctMenys.setTooltip ("Teclat de pantalla una octava avall"_ca);
            btOctMes.setTooltip ("Teclat de pantalla una octava amunt"_ca);
            btOctMenys.onClick = [this] { teclat.base = juce::jmax (0, teclat.base - 12); teclat.repaint(); };
            btOctMes.onClick = [this] { teclat.base = juce::jmin (72, teclat.base + 12); teclat.repaint(); };
            for (auto* b : { &btAnt, &btSeg, &btPresets, &btDesa, &btDesaCom, &btImporta, &btExporta, &btReinicia, &btDesfes, &btRefes, &btAtzar, &btMidi })
                addAndMakeVisible (b);
            addAndMakeVisible (btFav);
            btAnt.setTooltip ("Preset anterior"_ca); btSeg.setTooltip ("Preset següent"_ca);
            btPresets.setTooltip ("Tots els presets: categories, cerca, preferits, recents i a l'atzar"_ca);
            btDesa.setTooltip ("Desa els canvis al preset (si és teu); si és de fàbrica, en fa un de nou"_ca);
            btDesaCom.setTooltip ("Desa el so com un preset nou"_ca);
            btImporta.setTooltip ("Importa presets d'un fitxer ."_ca + cfg.extensio);
            btExporta.setTooltip ("Exporta el so actual a un fitxer ."_ca + cfg.extensio);
            btReinicia.setTooltip ("Torna el preset com era (treu els canvis)"_ca);
            btDesfes.setTooltip ("Desfés l'últim canvi del sinte (Ctrl+Z)"_ca);
            btRefes.setTooltip ("Refés (Ctrl+Y)"_ca);
            btAtzar.setTooltip ("Un preset a l'atzar, un so nou o una variació del que tens"_ca);
            btMidi.setTooltip ("Controls MIDI: les assignacions de CC i els potenciòmetres de l'AKAI"_ca);
            btFav.setTooltip ("Preferit"_ca);
            btAnt.onClick = [this] { pas (-1); };
            btSeg.onClick = [this] { pas (1); };
            btPresets.onClick = [this] { obreNavegador(); };
            btFav.onClick = [this] { commutaFavoritActual(); };
            btDesa.onClick = [this] { desa(); };
            btDesaCom.onClick = [this] { desaCom(); };
            btImporta.onClick = [this] { importa(); };
            btExporta.onClick = [this] { exporta(); };
            btReinicia.onClick = [this] { reiniciaPreset(); };
            btDesfes.onClick = [this] { desfesLocal (false); };
            btRefes.onClick = [this] { desfesLocal (true); };
            btAtzar.onClick = [this] { menuAtzar(); };
            btMidi.onClick = [this] { menuMidi(); };
            btFav.setToggleState (esFavorit (sint->nomPreset), juce::dontSendNotification);
            setWantsKeyboardFocus (true);
            startTimerHz (30);
        }
        ~PanellSinte() override { stopTimer(); }

        // ---- Enllac ----
        mibase::sinte::Base& sinte() override { return *sint; }
        juce::Colour colorSeccio (int s) override { return cfg.colorSeccio (s); }
        juce::Colour accent() override { return cfg.accent; }
        float previsualitza (const std::string& e, float* o, int n) override { return cfg.previsualitza (e, o, n); }
        juce::String textPrevisualitzacio() override { return cfg.textPrevi; }
        void comenca (int) override
        {
            if (gest++ > 0) return;
            pila.push_back (sint->estat());
            if (pila.size() > 200) pila.erase (pila.begin());
            refer.clear();
            if (plugins != nullptr) plugins->desaEstats (estat.projecte);
            estat.desfes.abans (estat.projecte);
        }
        void canvi (int p, float v01) override { sint->posaParametre (p, v01); darrers[(size_t) p] = sint->valor (p); }
        void acaba (int) override
        {
            if (gest > 0 && --gest > 0) return;
            desaAlProjecte();
            repaint (zonaLcd);
            canviat();
        }
        void info (const juce::String& t) override { textInfo = t; repaint (zonaInfo); }
        void menuControl (int p, juce::Component& c) override
        {
            juce::PopupMenu m;
            const int cc = sint->ccDeParam (p);
            m.addSectionHeader (u8 (sint->infoParametre (p).nom));
            m.addItem (1, "Aprèn MIDI: clica i mou un control del teclat"_ca);
            m.addItem (2, "Assigna un número de CC…"_ca);
            if (cc >= 0) m.addItem (3, "Treu l'assignació (CC "_ca + juce::String (cc) + ")");
            if (sint->aprenentCC() >= 0) m.addItem (4, "Deixa d'escoltar el MIDI"_ca);
            m.addSeparator();
            m.addItem (5, "Valor de fàbrica"_ca);
            m.addItem (-1, "Per automatitzar-lo: a la pista, «A» → Instrument"_ca, false);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&c), [this, p] (int r)
            {
                if (r == 1) { sint->aprenCC (p); info ("MIDI learn: mou ara un potenciòmetre o fader del teclat MIDI"_ca); }
                else if (r == 2) demanaCC (p);
                else if (r == 3) { sint->treuCC (p); desaAlProjecte(); }
                else if (r == 4) sint->aprenCC (-1);
                else if (r == 5)
                    for (auto* c2 : controls) if (c2->param() == p) { c2->reinicia(); break; }
                repaint();
            });
        }

        void mouseDown (const juce::MouseEvent&) override { grabKeyboardFocus(); }
        void visibilityChanged() override
        {
            if (isShowing()) juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PanellSinte> (this)] { if (sp != nullptr && sp->isShowing()) sp->grabKeyboardFocus(); });
        }
        bool keyPressed (const juce::KeyPress& k) override
        {
            if (k.getModifiers().isCommandDown() && (k.getKeyCode() == 'Z' || k.getKeyCode() == 'z')) { desfesLocal (k.getModifiers().isShiftDown()); return true; }
            if (k.getModifiers().isCommandDown() && (k.getKeyCode() == 'Y' || k.getKeyCode() == 'y')) { desfesLocal (true); return true; }
            return cridaTeclaGlobal (k);
        }

    protected:
        ConfigSinte cfg;
        std::shared_ptr<mibase::sinte::Base> sint;
        EstatEstudi& estat;
        GestorPlugins* plugins;
        std::string id;
        std::function<void (std::uint8_t, std::uint8_t, std::uint8_t)> directe;
        std::function<void()> tancar;
        juce::OwnedArray<ControlSB> controls;
        std::vector<Seccio> seccions;
        Pantalla pantalla;
        RodaSB rodaBend, rodaMod;
        TeclatSB teclat;
        Boto btOctMenys { "-", std::nullopt, Boto::Estil::Subtil }, btOctMes { "+", std::nullopt, Boto::Estil::Subtil };
        juce::Rectangle<int> zonaLogo, zonaLcd, zonaCpu, zonaTeclat, zonaInfo;
        juce::String textInfo { "Arrossega els poms (Maj = fi), doble clic = valor de fàbrica, botó dret = MIDI learn. La roda del ratolí també els mou."_ca };

        // ---- per crear controls ----
        PomSB* pom (int p, PomSB::Estil e, juce::String et = {}) { auto* c = new PomSB (*this, p, cfg.colorSeccio (sint->info (p).seccio), e, et); controls.add (c); addAndMakeVisible (c); return c; }
        SelectorSB* sel (int p, bool icones, juce::String t = {}) { auto* c = new SelectorSB (*this, p, cfg.colorSeccio (sint->info (p).seccio), icones, t); controls.add (c); addAndMakeVisible (c); return c; }
        InterruptorSB* intr (int p, juce::String et = {}) { auto* c = new InterruptorSB (*this, p, cfg.colorSeccio (sint->info (p).seccio), et); controls.add (c); addAndMakeVisible (c); return c; }
        FaderSB* fad (int p, juce::Colour col) { auto* c = new FaderSB (*this, p, col); controls.add (c); addAndMakeVisible (c); return c; }

        /** Crida-la al principi de resized(): posa la capçalera. */
        void posaCapcalera()
        {
            const int W = getWidth();
            const int x0 = cfg.ampleGaltes + 8, x1 = W - cfg.ampleGaltes - 8;
            zonaLogo = { x0 + 4, 8, 250, cfg.altCap - 14 };
            zonaLcd = { x0 + 262, 10, 400, cfg.altCap - 20 };
            btAnt.setBounds (zonaLcd.getX() + 4, zonaLcd.getCentreY() - 13, 26, 26);
            btSeg.setBounds (zonaLcd.getRight() - 52, zonaLcd.getCentreY() - 13, 26, 26);
            btFav.setBounds (zonaLcd.getRight() - 26, zonaLcd.getCentreY() - 13, 26, 26);
            zonaCpu = { x1 - 96, 12, 96, cfg.altCap - 24 };
            auto bx = juce::Rectangle<int> (zonaLcd.getRight() + 12, 10, zonaCpu.getX() - zonaLcd.getRight() - 20, cfg.altCap - 20);
            auto f1 = bx.removeFromTop (bx.getHeight() / 2).reduced (0, 1);
            auto f2 = bx.reduced (0, 1);
            const int w1 = f1.getWidth() / 5;
            for (auto* b : { &btPresets, &btDesa, &btDesaCom, &btImporta, &btExporta }) b->setBounds (f1.removeFromLeft (w1).reduced (2, 0));
            const int w2 = f2.getWidth() / 5;
            for (auto* b : { &btReinicia, &btDesfes, &btRefes, &btAtzar, &btMidi }) b->setBounds (f2.removeFromLeft (w2).reduced (2, 0));
            zonaInfo = { x0 + 4, getHeight() - 24, x1 - x0 - 8, 22 };
            if (navegador != nullptr) navegador->setBounds (getLocalBounds());
        }
        /** Rodes a l'esquerra, una columna «extra» (el que vulgui cada sinte) i el teclat. Torna on va l'extra. */
        juce::Rectangle<int> posaTeclat (juce::Rectangle<int> zona, int ampleExtra)
        {
            zonaTeclat = zona;
            auto r = zona.reduced (10, 8);
            rodaBend.setBounds (r.removeFromLeft (34));
            r.removeFromLeft (6);
            rodaMod.setBounds (r.removeFromLeft (34));
            r.removeFromLeft (12);
            auto extra = r.removeFromLeft (ampleExtra);
            r.removeFromLeft (10);
            auto oc = r.removeFromLeft (26);
            btOctMes.setBounds (oc.removeFromTop (oc.getHeight() / 2).reduced (0, 2));
            btOctMenys.setBounds (oc.reduced (0, 2));
            r.removeFromLeft (6);
            teclat.setBounds (r);
            return extra;
        }
        juce::Rectangle<int> dins (juce::Rectangle<int> r) const { return r.withTrimmedTop (30).reduced (10, 6); }
        /** 0.90: desa l'estat del sinte al projecte (per a canvis que no passen per un control). */
        void desaCanvis() { desaAlProjecte(); }

        /** Detalls propis de cada secció (es crida després de dibuixar-ne el marc). */
        virtual void detallsSeccio (juce::Graphics&, const Seccio&) {}
        /** El text curt de la llista de presets (model de filtre, mode…). */
        virtual juce::String etiquetaDe (const std::string&) { return {}; }
        /** Cada tic de la pantalla (30 per segon). */
        virtual void tic() {}
        /** Després de qualsevol canvi del so. */
        virtual void canviat() {}

        void paint (juce::Graphics& g) override
        {
            const auto b = getLocalBounds();
            g.drawImageAt (textura (cfg.fons, b.getWidth(), b.getHeight()), 0, 0);
            const int ag = cfg.ampleGaltes;
            const auto galta = textura (cfg.galtes, ag, b.getHeight());
            for (int x : { 0, b.getWidth() - ag })
            {
                g.drawImageAt (galta, x, 0);
                g.setColour (juce::Colours::black.withAlpha (0.45f));
                g.drawVerticalLine (x == 0 ? ag : x - 1, 0.0f, (float) b.getHeight());
                g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), (float) x, 0.0f, juce::Colours::white.withAlpha (0.0f), (float) x + ag * 0.6f, 0.0f, false));
                g.fillRect (x, 0, ag, b.getHeight());
            }
            {
                auto cap = juce::Rectangle<int> (ag, 0, b.getWidth() - 2 * ag, cfg.altCap);
                g.setTiledImageFill (textura (cfg.capcalera, 96, 96), 0, 0, 1.0f);
                g.fillRect (cap);
                g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f), 0.0f, 0.0f, juce::Colours::black.withAlpha (0.25f), 0.0f, (float) cfg.altCap, false));
                g.fillRect (cap);
                g.setGradientFill (juce::ColourGradient (cfg.colorFranja.withAlpha (0.7f), (float) cap.getX(), 0.0f, cfg.colorFranja.withRotatedHue (0.12f).withAlpha (0.7f), (float) cap.getRight(), 0.0f, false));
                g.fillRect (cap.getX(), cap.getBottom() - 2, cap.getWidth(), 1);
                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.fillRect (cap.getX(), cap.getBottom() - 1, cap.getWidth(), 1);
                auto lg = zonaLogo.toFloat();
                g.setFont (dibuix::lletra (10.5f, true));
                g.setColour (tema::accent);
                textEspaiat (g, "MIBASE", lg.removeFromTop (16.0f).translated (2.0f, 2.0f), juce::Justification::centredLeft, 3.5f);
                g.setFont (dibuix::lletra (27.0f, true));
                auto lt = lg.removeFromTop (32.0f);
                for (float d : { 6.0f, 3.5f, 1.5f })
                {
                    g.setColour (cfg.colorMarca.withAlpha (0.08f));
                    textEspaiat (g, cfg.marca, lt.translated (0.0f, d * 0.2f).expanded (d, 0.0f), juce::Justification::centredLeft, 2.0f);
                }
                g.setColour (cfg.colorMarca.interpolatedWith (juce::Colours::white, 0.75f));
                textEspaiat (g, cfg.marca, lt, juce::Justification::centredLeft, 2.0f);
                g.setFont (dibuix::lletra (9.0f, true));
                g.setColour (sb::serigrafiaSuau);
                textEspaiat (g, cfg.subtitol, lg.removeFromTop (14.0f), juce::Justification::centredLeft, 1.6f);
                dibuixaLcd (g);
                dibuixaCpu (g);
            }
            for (auto& s : seccions) dibuixaSeccio (g, s);
            {
                auto tf = zonaTeclat.toFloat();
                g.setColour (juce::Colours::black.withAlpha (0.4f));
                g.fillRoundedRectangle (tf.expanded (3.0f), 7.0f);
                g.setTiledImageFill (textura (cfg.capcalera, 96, 96), 0, 0, 1.0f);
                g.fillRoundedRectangle (tf, 6.0f);
                g.setColour (juce::Colours::white.withAlpha (0.06f));
                g.drawRoundedRectangle (tf.reduced (0.5f), 6.0f, 1.0f);
            }
            g.setFont (dibuix::lletra (11.0f));
            g.setColour (sb::serigrafiaSuau);
            g.drawText (textInfo, zonaInfo, juce::Justification::centredLeft);
        }

        void dibuixaSeccio (juce::Graphics& g, const Seccio& s)
        {
            auto r = s.r.toFloat();
            const auto col = cfg.colorSeccio (s.id);
            if (cfg.vintage)
            {
                // 1.4: serigrafia: un marc de línia fina i el títol a dalt, tallant la línia (com als panells dels 70)
                g.setColour (juce::Colours::black.withAlpha (0.22f));
                g.fillRect (r.reduced (1.0f));
                g.setFont (dibuix::lletra (11.0f, true));
                const float wt = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), s.titol) + 2.2f * (float) s.titol.length() + 16.0f;
                const float y = r.getY() + 12.0f;
                g.setColour (sb::serigrafia.withAlpha (0.55f));
                g.drawLine (r.getX() + 1.0f, y, r.getX() + 12.0f, y, 1.0f);
                g.drawLine (r.getX() + 20.0f + wt, y, r.getRight() - 1.0f, y, 1.0f);
                g.drawLine (r.getX() + 1.0f, y, r.getX() + 1.0f, r.getBottom() - 1.0f, 1.0f);
                g.drawLine (r.getRight() - 1.0f, y, r.getRight() - 1.0f, r.getBottom() - 1.0f, 1.0f);
                g.drawLine (r.getX() + 1.0f, r.getBottom() - 1.0f, r.getRight() - 1.0f, r.getBottom() - 1.0f, 1.0f);
                g.setColour (sb::serigrafia);
                textEspaiat (g, s.titol, juce::Rectangle<float> (r.getX() + 16.0f, y - 8.0f, wt, 16.0f), juce::Justification::centredLeft, 2.2f);
                g.setColour (col);
                g.fillRect (r.getX() + 16.0f, y + 7.0f, 18.0f, 2.0f);   // una ratlleta del color de la secció, com un codi de colors
                detallsSeccio (g, s);
                return;
            }
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (r.translated (0.0f, 3.0f).expanded (1.0f), 8.0f);
            g.setGradientFill (juce::ColourGradient (cfg.seccioDalt, r.getX(), r.getY(), cfg.seccioBaix, r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 7.0f);
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
            auto t = r.removeFromTop (26.0f).reduced (6.0f, 4.0f);
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (t, 4.0f);
            auto llum = t.removeFromLeft (4.0f).reduced (0.0f, 2.0f);
            g.setColour (col.withAlpha (0.35f));
            g.fillRoundedRectangle (llum.expanded (3.0f, 2.0f), 3.0f);
            g.setColour (col);
            g.fillRoundedRectangle (llum, 2.0f);
            g.setColour (sb::serigrafia);
            g.setFont (dibuix::lletra (11.5f, true));
            textEspaiat (g, s.titol, t.withTrimmedLeft (8.0f), juce::Justification::centredLeft, 2.2f);
            g.setColour (col.withAlpha (0.18f));
            g.fillRect (t.getX() + 4.0f, t.getBottom() + 3.0f, t.getWidth() - 4.0f, 1.0f);
            cargol (g, { s.r.toFloat().getRight() - 10.0f, s.r.toFloat().getY() + 13.0f }, 3.5f);
            detallsSeccio (g, s);
        }

        void dibuixaVu (juce::Graphics& g, juce::Rectangle<int> zona)
        {
            auto v = zona.toFloat();
            g.setColour (juce::Colour (0xff070708));
            g.fillRoundedRectangle (v, 2.0f);
            const float niv = sint->nivell();
            const float db = niv > 1e-5f ? 20.0f * std::log10 (niv) : -100.0f;
            const float f = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
            const int seg = 16;
            for (int i = 0; i < seg; ++i)
            {
                const float y = v.getBottom() - (i + 1) * v.getHeight() / seg;
                const bool on = (float) i / seg < f;
                const auto c = i >= seg - 2 ? tema::error : i >= seg - 5 ? sb::lcd : cfg.accent;
                g.setColour (on ? c : c.withAlpha (0.08f));
                g.fillRect (v.getX() + 2.0f, y + 1.0f, v.getWidth() - 4.0f, v.getHeight() / seg - 2.0f);
            }
        }
        /** Llumeta del pas de l'arpegiador / LFO. */
        void dibuixaLed (juce::Graphics& g, juce::Rectangle<int> zona, juce::Colour col, float valor, bool actiu)
        {
            auto led = zona.toFloat();
            g.setColour (col.withAlpha (actiu ? 0.35f * valor : 0.0f));
            g.fillEllipse (led.expanded (5.0f));
            g.setColour (actiu ? col.withAlpha (0.25f + 0.75f * valor) : juce::Colour (0xff1c1c20));
            g.fillEllipse (led);
        }

    private:
        std::vector<float> darrers;
        std::vector<std::string> pila, refer;
        int gest = 0, comptaTimer = 0;
        std::unique_ptr<NavegadorPresetsComu> navegador;
        std::unique_ptr<DialegText> dialeg;
        std::unique_ptr<juce::FileChooser> selector;
        juce::TooltipWindow tips { this, 600 };
        Boto btAnt { {}, dibuix::Icona::Rebobina, Boto::Estil::Subtil }, btSeg { {}, dibuix::Icona::Endavant, Boto::Estil::Subtil };
        BotoEstrella btFav;
        Boto btPresets { "Presets", dibuix::Icona::Lupa, Boto::Estil::Secundari }, btDesa { "Desa", dibuix::Icona::Disc, Boto::Estil::Secundari },
             btDesaCom { "Desa com…"_ca, std::nullopt, Boto::Estil::Secundari }, btImporta { "Importa", std::nullopt, Boto::Estil::Secundari },
             btExporta { "Exporta", std::nullopt, Boto::Estil::Secundari }, btReinicia { "Reinicia", std::nullopt, Boto::Estil::Secundari },
             btDesfes { {}, dibuix::Icona::Desfes, Boto::Estil::Secundari }, btRefes { {}, dibuix::Icona::Refes, Boto::Estil::Secundari },
             btAtzar { "Atzar", std::nullopt, Boto::Estil::Secundari }, btMidi { "MIDI", std::nullopt, Boto::Estil::Secundari };

        BibliotecaPresets& bib() { return biblioteca (cfg.carpeta); }
        void desaBib() { bib().desa (dibuix::cami (juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Mibase").getChildFile (cfg.carpeta).getChildFile ("presets.json"))); }
        juce::File fitxerRecents() const { return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Mibase").getChildFile (cfg.carpeta).getChildFile ("recents.txt"); }
        void apuntaRecent (const std::string& nom)
        {
            auto llista = recents();
            llista.removeString (u8 (nom));
            llista.insert (0, u8 (nom));
            while (llista.size() > 20) llista.remove (20);
            fitxerRecents().getParentDirectory().createDirectory();
            fitxerRecents().replaceWithText (llista.joinIntoString ("\n"));
        }

        void envia (std::uint8_t e, std::uint8_t d1, std::uint8_t d2) { if (directe) directe (e, d1, d2); }
        mibase::Pista* pista() { return estat.pista (mibase::edicio::pistaPerId (estat.projecte, id)); }

        void desaAlProjecte()
        {
            if (auto* t = pista()) { t->estatInstrument = sint->estat(); estat.canviat ({}); }
        }
        void canviSencer (const std::string& nouEstat, const std::string& nomPreset, bool apilar = true)
        {
            if (nouEstat == sint->estatSo() && nomPreset == sint->nomPreset) return;
            if (apilar) { pila.push_back (sint->estat()); if (pila.size() > 200) pila.erase (pila.begin()); refer.clear(); }
            if (plugins != nullptr) plugins->desaEstats (estat.projecte);
            estat.desfes.abans (estat.projecte);
            sint->posaEstat (nouEstat);
            sint->nomPreset = nomPreset;
            for (int i = 0; i < sint->numParametres(); ++i) darrers[(size_t) i] = sint->valor (i);
            desaAlProjecte();
            canviat();
            repaint();
        }

        void timerCallback() override
        {
            if (auto* t = pista(); t == nullptr || t->instrument.rfind ("sinte:", 0) != 0) { juce::MessageManager::callAsync ([f = tancar] { f(); }); stopTimer(); return; }
            for (auto* c : controls)
            {
                const int p = c->param();
                const float v = sint->valor (p);
                if (v != darrers[(size_t) p]) { darrers[(size_t) p] = v; c->repaint(); canviat(); }
                if (sint->aprenentCC() == p || (comptaTimer % 15 == 0 && sint->ccDeParam (p) >= 0)) c->repaint();
            }
            if (++comptaTimer % 2 == 0) pantalla.actualitza();
            repaint (zonaCpu);
            rodaBend.repaint();
            rodaMod.repaint();
            teclat.repaint();
            if (comptaTimer % 10 == 0) repaint (zonaLcd);
            tic();
        }

        void dibuixaLcd (juce::Graphics& g)
        {
            if (cfg.vintage) { dibuixaEtiqueta (g); return; }
            auto r = zonaLcd.toFloat();
            g.setColour (juce::Colours::black.withAlpha (0.7f));
            g.fillRoundedRectangle (r.expanded (2.0f), 6.0f);
            g.setColour (sb::lcdFons);
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (sb::lcd.withAlpha (0.05f));
            for (float y = r.getY() + 2.0f; y < r.getBottom(); y += 3.0f) g.drawHorizontalLine ((int) y, r.getX() + 4.0f, r.getRight() - 4.0f);
            auto in = r.reduced (36.0f, 4.0f).withTrimmedRight (30.0f);
            const auto nom = u8 (sint->nomPreset);
            const auto* p = presetPerNom (sint->nomPreset);
            const bool modificat = p != nullptr && p->estat != sint->estatSo();
            g.setFont (dibuix::mono (10.0f));
            g.setColour (sb::lcd.withAlpha (0.6f));
            g.drawText (p != nullptr ? dibuix::majuscules (u8 (p->categoria)) + (bib().busca (p->nom) != nullptr ? "  ·  MEU"_ca : juce::String()) : juce::String ("SO PROPI"),
                        in.removeFromTop (14.0f), juce::Justification::centredLeft);
            g.setFont (dibuix::mono (19.0f));
            g.setColour (sb::lcd);
            g.drawFittedText ((nom.isEmpty() ? "(sense nom)"_ca : nom) + (modificat ? " *" : ""), in.toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f), r.getX(), r.getY(), juce::Colours::white.withAlpha (0.0f), r.getX(), r.getCentreY(), false));
            g.fillRoundedRectangle (r, 5.0f);
        }

        /** 1.4: el nom del preset escrit a màquina en una etiqueta de paper enganxada al panell. */
        void dibuixaEtiqueta (juce::Graphics& g)
        {
            auto r = zonaLcd.toFloat().reduced (30.0f, 2.0f).withTrimmedRight (26.0f);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillRect (r.translated (1.5f, 2.5f));
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffeee3c6), r.getX(), r.getY(), juce::Colour (0xffd9c9a3), r.getRight(), r.getBottom(), false));
            g.fillRect (r);
            juce::Random rnd (77);
            for (int i = 0; i < 90; ++i)
            {
                g.setColour (juce::Colour (0xff8a6d3b).withAlpha (0.05f + 0.06f * rnd.nextFloat()));
                g.fillEllipse (r.getX() + rnd.nextFloat() * r.getWidth(), r.getY() + rnd.nextFloat() * r.getHeight(), 2.0f, 2.0f);
            }
            g.setColour (juce::Colour (0xffb0402e).withAlpha (0.55f));   // la ratlla vermella de les etiquetes antigues
            g.fillRect (r.getX() + 6.0f, r.getY() + 17.0f, r.getWidth() - 12.0f, 1.0f);
            const auto nom = u8 (sint->nomPreset);
            const auto* p = presetPerNom (sint->nomPreset);
            const bool modificat = p != nullptr && p->estat != sint->estatSo();
            auto in = r.reduced (8.0f, 2.0f);
            g.setColour (juce::Colour (0xff3a2f22));
            g.setFont (dibuix::mono (9.5f));
            g.drawText (p != nullptr ? dibuix::majuscules (u8 (p->categoria)) + (bib().busca (p->nom) != nullptr ? "  ·  MEU"_ca : juce::String()) : juce::String ("SO PROPI"),
                        in.removeFromTop (15.0f), juce::Justification::centredLeft);
            g.setColour (juce::Colour (0xff17120c));
            g.setFont (dibuix::mono (18.0f));
            g.drawFittedText ((nom.isEmpty() ? "(sense nom)"_ca : nom) + (modificat ? " *" : ""), in.toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
        }

        void dibuixaCpu (juce::Graphics& g)
        {
            if (cfg.vintage)
            {
                auto t = zonaCpu.toFloat().reduced (6.0f, 8.0f);
                g.setColour (sb::serigrafiaSuau);
                g.setFont (dibuix::lletra (9.0f, true));
                textEspaiat (g, "VEUS " + juce::String (sint->veusActives()), t.removeFromTop (t.getHeight() / 2), juce::Justification::centredLeft, 1.2f);
                textEspaiat (g, "CPU " + juce::String (juce::jlimit (0.0f, 1.0f, sint->carregaCpu()) * 100.0f, 1) + " %", t, juce::Justification::centredLeft, 1.2f);
                return;
            }
            auto r = zonaCpu.toFloat();
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRoundedRectangle (r, 5.0f);
            const float c = juce::jlimit (0.0f, 1.0f, sint->carregaCpu());
            g.setFont (dibuix::mono (9.5f));
            g.setColour (sb::serigrafiaSuau);
            auto t = r.reduced (8.0f, 6.0f);
            g.drawText ("CPU", t.removeFromTop (12.0f), juce::Justification::centredLeft);
            g.setColour (c > 0.5f ? tema::error : c > 0.25f ? sb::lcd : cfg.accent);
            g.drawText (juce::String (c * 100.0f, 1) + " %", t.removeFromTop (12.0f).withTrimmedTop (-12.0f), juce::Justification::centredRight);
            auto barra = t.removeFromTop (6.0f).withTrimmedTop (2.0f);
            g.setColour (juce::Colour (0xff0b0b0d));
            g.fillRoundedRectangle (barra, 2.0f);
            g.setColour (c > 0.5f ? tema::error : cfg.accent);
            g.fillRoundedRectangle (barra.withWidth (juce::jmax (2.0f, barra.getWidth() * juce::jmin (1.0f, c * 4.0f))), 2.0f);
            g.setColour (sb::serigrafiaSuau);
            g.drawText ("VEUS " + juce::String (sint->veusActives()), t.withTrimmedTop (2.0f), juce::Justification::centredLeft);
        }

        // ---- presets ----
        std::vector<const Preset*> totsPresets()
        {
            std::vector<const Preset*> v;
            for (auto& p : bib().usuari) v.push_back (&p);
            for (auto& p : *cfg.presets) v.push_back (&p);
            return v;
        }
        const Preset* presetPerNom (const std::string& nom)
        {
            if (nom.empty()) return nullptr;
            if (auto* p = bib().busca (nom)) return p;
            for (auto& p : *cfg.presets) if (p.nom == nom) return &p;
            return nullptr;
        }
        std::string clauDe (const std::string& nom) { return bib().busca (nom) != nullptr ? BibliotecaPresets::clauUsuari (nom) : BibliotecaPresets::clauFabrica (nom); }
        bool esFavorit (const std::string& nom) { return ! nom.empty() && bib().esFavorit (clauDe (nom)); }
        void commutaFavoritActual()
        {
            if (sint->nomPreset.empty() || presetPerNom (sint->nomPreset) == nullptr) { btFav.setToggleState (false, juce::dontSendNotification); return; }
            bib().commutaFavorit (clauDe (sint->nomPreset));
            desaBib();
            btFav.setToggleState (esFavorit (sint->nomPreset), juce::dontSendNotification);
        }
        void aplicaPreset (const Preset& p)
        {
            canviSencer (p.estat, p.nom);
            apuntaRecent (p.nom);
            btFav.setToggleState (esFavorit (p.nom), juce::dontSendNotification);
            info ("Preset: "_ca + u8 (p.nom) + " · "_ca + u8 (p.descripcio));
        }
        void pas (int d)
        {
            const auto v = totsPresets();
            if (v.empty()) return;
            int i = -1;
            for (int k = 0; k < (int) v.size(); ++k) if (v[(size_t) k]->nom == sint->nomPreset) i = k;
            i = i < 0 ? (d > 0 ? 0 : (int) v.size() - 1) : (i + d + (int) v.size()) % (int) v.size();
            aplicaPreset (*v[(size_t) i]);
        }
        void desa()
        {
            if (const auto* ja = bib().busca (sint->nomPreset); ! sint->nomPreset.empty() && ja != nullptr)
            {
                bib().desaPreset ({ sint->nomPreset, ja->categoria, ja->descripcio, sint->estatSo() });
                desaBib();
                info ("Desat: "_ca + u8 (sint->nomPreset));
                repaint (zonaLcd);
                return;
            }
            desaCom();
        }
        void desaCom()
        {
            const auto actual = u8 (sint->nomPreset);
            const auto proposta = u8 (bib().nomLliure ((actual.isEmpty() ? juce::String ("So"_ca) : actual + " (meu)"_ca).toStdString()));
            dialeg = std::make_unique<DialegText> ("Desa el preset"_ca, "Nom del preset nou (si ja en tens un amb aquest nom, se substitueix)."_ca, "Desa"_ca,
                [] (const juce::String& t) { return t.trim().isEmpty() ? "Escriu un nom."_ca : juce::String(); },
                [this] (const juce::String& t)
                {
                    const auto nom = t.trim().toStdString();
                    const auto* ja = bib().busca (nom);
                    const auto* origen = presetPerNom (sint->nomPreset);
                    bib().desaPreset ({ nom, ja != nullptr ? ja->categoria : std::string ("Meus"),
                                        origen != nullptr ? "A partir de «" + origen->nom + "»" : std::string(), sint->estatSo() });
                    desaBib();
                    sint->nomPreset = nom;
                    desaAlProjecte();
                    apuntaRecent (nom);
                    btFav.setToggleState (esFavorit (nom), juce::dontSendNotification);
                    info ("Desat com a preset teu: "_ca + u8 (nom));
                    repaint();
                }, proposta);
            DialegText::mostra (*this, dialeg);
        }
        void reiniciaPreset()
        {
            if (const auto* p = presetPerNom (sint->nomPreset)) { canviSencer (p->estat, p->nom); info ("Preset com era: "_ca + u8 (p->nom)); }
            else { canviSencer (cfg.inicial(), {}); info ("So inicial"_ca); }
        }
        void desfesLocal (bool refes)
        {
            auto& de = refes ? refer : pila;
            auto& a = refes ? pila : refer;
            if (de.empty()) { info (refes ? "No hi ha res per refer"_ca : "No hi ha res per desfer al sinte"_ca); return; }
            a.push_back (sint->estat());
            const auto e = de.back();
            de.pop_back();
            std::string nom;
            if (const auto k = e.find (";pnom="); k != std::string::npos) nom = e.substr (k + 6);
            canviSencer (e.substr (0, e.find (";cc=")), nom, false);
            sint->posaEstat (e);
            desaAlProjecte();
            info (refes ? "Refet"_ca : "Desfet"_ca);
        }
        void menuAtzar()
        {
            juce::PopupMenu m;
            m.addItem (4, "Un preset de fàbrica a l'atzar"_ca);
            m.addItem (1, "So nou a l'atzar (fet del no-res, però sempre útil)"_ca);
            m.addItem (2, "Varia una mica el so actual"_ca);
            m.addItem (3, "Varia molt el so actual"_ca);
            if (cfg.aleatoriTipus != nullptr && ! cfg.tipusAtzar.empty())
            {
                juce::PopupMenu t;
                for (size_t i = 0; i < cfg.tipusAtzar.size(); ++i) t.addItem (100 + (int) i, u8 (cfg.tipusAtzar[i]));
                m.addSubMenu ("So nou a l'atzar d'un tipus"_ca, t);
            }
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btAtzar), [this] (int r)
            {
                const auto llavor = (unsigned) juce::Random::getSystemRandom().nextInt();
                if (r >= 100 && cfg.aleatoriTipus != nullptr)
                {
                    canviSencer (cfg.aleatoriTipus (llavor, r - 100), {});
                    info ("So nou a l'atzar («"_ca + u8 (cfg.tipusAtzar[(size_t) (r - 100)]) + "»). Si t'agrada: «Desa com…»"_ca);
                    return;
                }
                if (r == 4 && ! cfg.presets->empty()) aplicaPreset ((*cfg.presets)[(size_t) juce::Random::getSystemRandom().nextInt ((int) cfg.presets->size())]);
                else if (r == 1) { canviSencer (cfg.aleatori (llavor), {}); info ("So nou a l'atzar. Si t'agrada: «Desa com…»"_ca); }
                else if (r == 2 || r == 3)
                {
                    const auto nom = sint->nomPreset;
                    canviSencer (cfg.varia (sint->estatSo(), llavor, r == 2 ? 0.35f : 1.0f), nom);
                    info ("Variació del so (Ctrl+Z per tornar)"_ca);
                }
            });
        }
        void demanaCC (int p)
        {
            dialeg = std::make_unique<DialegText> ("Assigna un CC"_ca, "Número de CC (0–127) que ha de moure «"_ca + u8 (sint->infoParametre (p).nom) + "». L'1 (roda) i el 64 (pedal) no es poden fer servir."_ca, "Assigna"_ca,
                [] (const juce::String& t)
                {
                    const int n = t.trim().getIntValue();
                    if (! t.trim().containsOnly ("0123456789") || t.trim().isEmpty() || n < 0 || n > 127) return "Escriu un número de 0 a 127."_ca;
                    if (mibase::sinte::Base::ccReservat (n)) return "Aquest CC està reservat."_ca;
                    return juce::String();
                },
                [this, p] (const juce::String& t) { sint->assignaCC (t.trim().getIntValue(), p); desaAlProjecte(); repaint(); }, juce::String());
            DialegText::mostra (*this, dialeg);
        }
        void menuMidi()
        {
            juce::PopupMenu m;
            m.addSectionHeader ("Controls MIDI (CC)"_ca);
            m.addItem (-1, "Botó dret sobre qualsevol control: «Aprèn MIDI» o «Assigna un CC»"_ca, false);
            m.addSeparator();
            bool cap = true;
            for (int c = 0; c < 128; ++c)
                if (const int p = sint->paramDeCC (c); p >= 0)
                {
                    m.addItem (1000 + c, "CC "_ca + juce::String (c) + "  →  "_ca + u8 (sint->infoParametre (p).nom) + "   (clica per treure-la)"_ca);
                    cap = false;
                }
            if (cap) m.addItem (-1, "(cap assignació)"_ca, false);
            m.addSeparator();
            m.addItem (1, "Posa els 8 potenciòmetres de l'AKAI MPK Mini (CC 70–77)"_ca);
            m.addItem (2, "Treu totes les assignacions"_ca);
            if (sint->aprenentCC() >= 0) m.addItem (3, "Deixa d'escoltar el MIDI"_ca);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btMidi), [this] (int r)
            {
                if (r >= 1000) { const int q = sint->paramDeCC (r - 1000); if (q >= 0) sint->treuCC (q); }
                else if (r == 1) { for (int i = 0; i < 8; ++i) sint->assignaCC (70 + i, cfg.akai[(size_t) i]); info (cfg.textAkai); }
                else if (r == 2) { for (int p = 0; p < sint->numParametres(); ++p) sint->treuCC (p); }
                else if (r == 3) sint->aprenCC (-1);
                else return;
                desaAlProjecte();
                repaint();
            });
        }
        void importa()
        {
            selector = std::make_unique<juce::FileChooser> ("Importa presets"_ca, juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*." + cfg.extensio);
            selector->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (! f.existsAsFile()) return;
                const auto j = juce::JSON::parse (f);
                const auto presets = j.getProperty ("presets", {});
                const auto* llista = presets.getArray();
                if (j.getProperty ("tipus", {}).toString() != cfg.tipusFitxer || llista == nullptr) { info ("Aquest fitxer no és de presets d'aquest sinte"_ca); return; }
                int n = 0;
                std::string primer;
                const auto prefix = cfg.tipusFitxer.toStdString() + "=";
                for (const auto& v : *llista)
                {
                    const auto estatP = v.getProperty ("estat", {}).toString().toStdString();
                    if (estatP.rfind (prefix, 0) != 0) continue;
                    const auto nom = bib().nomLliure (v.getProperty ("nom", "Importat").toString().toStdString());
                    bib().desaPreset ({ nom, v.getProperty ("categoria", "Importats").toString().toStdString(), v.getProperty ("descripcio", "").toString().toStdString(), estatP });
                    if (primer.empty()) primer = nom;
                    ++n;
                }
                desaBib();
                if (const auto* p = presetPerNom (primer)) aplicaPreset (*p);
                info ("Importats: "_ca + juce::String (n) + " presets"_ca);
            });
        }
        void exporta()
        {
            const auto nom = sint->nomPreset.empty() ? juce::String ("So") : u8 (sint->nomPreset);
            selector = std::make_unique<juce::FileChooser> ("Exporta el so"_ca, juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                            .getChildFile (juce::File::createLegalFileName (nom) + "." + cfg.extensio), "*." + cfg.extensio);
            selector->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                [this, nom] (const juce::FileChooser& fc)
                {
                    auto f = fc.getResult();
                    if (f == juce::File()) return;
                    if (! f.hasFileExtension (cfg.extensio)) f = f.withFileExtension (cfg.extensio);
                    auto* p = new juce::DynamicObject();
                    p->setProperty ("nom", nom);
                    const auto* origen = presetPerNom (sint->nomPreset);
                    p->setProperty ("categoria", origen != nullptr ? u8 (origen->categoria) : juce::String ("Meus"));
                    p->setProperty ("descripcio", origen != nullptr ? u8 (origen->descripcio) : juce::String());
                    p->setProperty ("estat", u8 (sint->estatSo()));
                    auto* arrel = new juce::DynamicObject();
                    arrel->setProperty ("tipus", cfg.tipusFitxer);
                    arrel->setProperty ("versio", 1);
                    arrel->setProperty ("presets", juce::Array<juce::var> { juce::var (p) });
                    const bool ok = f.replaceWithText (juce::JSON::toString (juce::var (arrel)));
                    info (ok ? "Exportat a "_ca + f.getFullPathName() : "No s'ha pogut escriure "_ca + f.getFullPathName());
                });
        }
        void obreNavegador()
        {
            navegador = std::make_unique<NavegadorPresetsComu> (*this);
            addAndMakeVisible (*navegador);
            navegador->setBounds (getLocalBounds());
            navegador->grabKeyboardFocus();
        }
        // OrigenPresets
        juce::StringArray categoriesFabrica() override { juce::StringArray a; for (auto& c : *cfg.categories) a.add (u8 (c)); return a; }
        juce::StringArray carpetesUsuari() override { juce::StringArray a; for (auto& c : bib().carpetes()) a.add (u8 (c)); return a; }
        std::vector<EntradaPreset> entrades() override
        {
            std::vector<EntradaPreset> v;
            const auto tots = totsPresets();
            const int nu = (int) bib().usuari.size();
            for (int i = 0; i < (int) tots.size(); ++i)
            {
                const auto& p = *tots[(size_t) i];
                const bool usuari = i < nu;
                if (etiquetes.count (p.estat) == 0) etiquetes[p.estat] = etiquetaDe (p.estat);   // un cop per so
                v.push_back ({ u8 (p.nom), u8 (p.categoria), u8 (p.descripcio), etiquetes[p.estat], usuari,
                               bib().esFavorit (usuari ? BibliotecaPresets::clauUsuari (p.nom) : BibliotecaPresets::clauFabrica (p.nom)), i });
            }
            return v;
        }
        std::map<std::string, juce::String> etiquetes;
        juce::String presetActual() override { return u8 (sint->nomPreset); }
        void aplica (const EntradaPreset& e) override
        {
            const auto tots = totsPresets();
            if (e.index >= 0 && e.index < (int) tots.size()) aplicaPreset (*tots[(size_t) e.index]);
        }
        void commutaFavorit (const EntradaPreset& e) override
        {
            const auto n = e.nom.toStdString();
            bib().commutaFavorit (e.usuari ? BibliotecaPresets::clauUsuari (n) : BibliotecaPresets::clauFabrica (n));
            desaBib();
            btFav.setToggleState (esFavorit (sint->nomPreset), juce::dontSendNotification);
        }
        bool existeixMeu (const juce::String& nom) override { return bib().busca (nom.toStdString()) != nullptr; }
        void reanomena (const juce::String& vell, const juce::String& nou) override
        {
            if (bib().reanomena (vell.toStdString(), nou.toStdString()))
            {
                desaBib();
                if (sint->nomPreset == vell.toStdString()) { sint->nomPreset = nou.toStdString(); desaAlProjecte(); }
            }
            repaint();
        }
        void mou (const juce::String& nom, const juce::String& carpeta) override { bib().mou (nom.toStdString(), carpeta.toStdString()); desaBib(); }
        void esborra (const juce::String& nom) override { bib().esborra (nom.toStdString()); desaBib(); repaint(); }
        void tancaNavegador() override
        {
            juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PanellSinte> (this)] { if (sp != nullptr) { sp->navegador.reset(); sp->grabKeyboardFocus(); } });
        }
        juce::StringArray recents() override { return juce::StringArray::fromLines (fitxerRecents().loadFileAsString()); }
        bool ambRecentsIAtzar() override { return true; }
    };

    /** La finestra (sense redimensionar; es tanca amb la X). */
    class FinestraSinte : public juce::DocumentWindow
    {
    public:
        FinestraSinte (const juce::String& titol, juce::Component* panell, std::function<void()> enTancar)
            : juce::DocumentWindow (titol, juce::Colour (0xff121316), juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
              tancar (std::move (enTancar))
        {
            setUsingNativeTitleBar (true);
            setContentOwned (panell, true);
            setResizable (false, false);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::MessageManager::callAsync ([t = tancar] { t(); }); }
        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }
    private:
        std::function<void()> tancar;
    };
}
