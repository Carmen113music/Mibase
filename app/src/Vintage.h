#pragma once

#include "Tema.h"

#include <juce_gui_basics/juce_gui_basics.h>

/** Estètica «aparell antic»: tecles físiques com les d'un radiocasset, pantalla LCD, faders amb ranura i capçal metàl·lic.
    Tot es dibuixa amb degradats i ombres (sense imatges). */
namespace vintage
{
    inline const juce::Colour ledVerd    { 0xff63d66e };
    inline const juce::Colour ledVermell { 0xffff4b3e };
    inline const juce::Colour ledGroc    { 0xfff5c445 };
    inline const juce::Colour ledBlau    { 0xff6fa8ff };
    inline const juce::Colour ledTaronja { 0xffff8a2a };
    inline const juce::Colour lcdFons    { 0xff1b1e14 };
    inline const juce::Colour lcdText    { 0xffffb84a };

    /** Panell de metall fosc (barres, columnes). */
    inline void panell (juce::Graphics& g, juce::Rectangle<float> r, float radi = 0.0f)
    {
        juce::ColourGradient grad (juce::Colour (0xff2b2b33), r.getX(), r.getY(), juce::Colour (0xff1f1f25), r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
        if (radi > 0.0f) g.fillRoundedRectangle (r, radi); else g.fillRect (r);
        // Línies molt fines horitzontals: metall raspallat.
        g.setColour (juce::Colours::white.withAlpha (0.018f));
        for (float y = r.getY() + 2.0f; y < r.getBottom(); y += 3.0f)
            g.fillRect (r.getX(), y, r.getWidth(), 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRect (r.getX(), r.getY(), r.getWidth(), 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRect (r.getX(), r.getBottom() - 1.0f, r.getWidth(), 1.0f);
    }

    /** Una tecla física. Retorna la zona on dibuixar la icona o el text (es mou avall quan està premuda).
        «encesa»: la llumeta de color de dalt brilla (estat actiu). */
    inline juce::Rectangle<float> tecla (juce::Graphics& g, juce::Rectangle<float> r, bool premuda, bool encesa,
                                         juce::Colour llum, bool actiu = true, bool sobre = false)
    {
        const float gruix = premuda ? 1.0f : 3.0f;   // el «cos» de la tecla que es veu a sota
        const float radi = 4.0f;

        // Ombra a terra.
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (r.translated (0.0f, 1.0f), radi + 1.0f);

        // Cos (lateral fosc).
        auto cos = r.withTrimmedTop (premuda ? 2.0f : 0.0f);
        g.setColour (juce::Colour (0xff121216));
        g.fillRoundedRectangle (cos, radi);

        // Cara de la tecla.
        auto cara = cos.withTrimmedBottom (gruix);
        const auto dalt = actiu ? (premuda ? juce::Colour (0xff2e2e36) : (sobre ? juce::Colour (0xff4a4a55) : juce::Colour (0xff42424c)))
                                : juce::Colour (0xff2c2c33);
        const auto baix = actiu ? (premuda ? juce::Colour (0xff22222a) : juce::Colour (0xff2a2a31)) : juce::Colour (0xff222228);
        g.setGradientFill (juce::ColourGradient (dalt, cara.getX(), cara.getY(), baix, cara.getX(), cara.getBottom(), false));
        g.fillRoundedRectangle (cara, radi);

        // Brillantor de la vora de dalt i contorn.
        g.setColour (juce::Colours::white.withAlpha (premuda ? 0.05f : 0.14f));
        g.fillRect (cara.getX() + radi, cara.getY() + 1.0f, cara.getWidth() - radi * 2.0f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawRoundedRectangle (cos.reduced (0.5f), radi, 1.0f);

        // Llumeta.
        if (! llum.isTransparent())
        {
            auto led = juce::Rectangle<float> (cara.getCentreX() - 7.0f, cara.getY() + 4.0f, 14.0f, 2.5f);
            if (encesa)
            {
                g.setColour (llum.withAlpha (0.35f));
                g.fillRoundedRectangle (led.expanded (3.0f, 2.0f), 3.0f);
                g.setColour (llum);
            }
            else
                g.setColour (llum.withMultipliedSaturation (0.35f).withMultipliedBrightness (0.45f));
            g.fillRoundedRectangle (led, 1.2f);
            cara.removeFromTop (6.0f);
        }

        return cara.reduced (3.0f, 2.0f);
    }

    /** Pantalla LCD ambre amb text gran (i un de petit a sota, opcional). */
    inline void lcd (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& gran, const juce::String& petit, const juce::String& etiqueta = {})
    {
        // Marc enfonsat.
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (r, 5.0f);
        auto vidre = r.reduced (2.0f);
        g.setGradientFill (juce::ColourGradient (lcdFons.brighter (0.15f), vidre.getX(), vidre.getY(), lcdFons, vidre.getX(), vidre.getBottom(), false));
        g.fillRoundedRectangle (vidre, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (vidre.withHeight (vidre.getHeight() * 0.45f), 4.0f);

        auto t = vidre.reduced (8.0f, 2.0f);
        const auto mono = juce::Font::getDefaultMonospacedFontName();

        // Fila petita de dalt: etiqueta a l'esquerra, text petit a la dreta.
        if (etiqueta.isNotEmpty() || petit.isNotEmpty())
        {
            auto fila = t.removeFromTop (juce::jmin (12.0f, t.getHeight() * 0.32f));
            if (etiqueta.isNotEmpty())
            {
                g.setColour (lcdText.withAlpha (0.55f));
                g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
                g.drawText (etiqueta, fila, juce::Justification::centredLeft);
            }
            if (petit.isNotEmpty())
            {
                g.setColour (lcdText.withAlpha (0.75f));
                g.setFont (juce::Font (juce::FontOptions (mono, 10.5f, juce::Font::plain)));
                g.drawText (petit, fila, etiqueta.isNotEmpty() ? juce::Justification::centredRight : juce::Justification::centredLeft);
            }
        }

        // Text gran, amb una mica de resplendor.
        g.setFont (juce::Font (juce::FontOptions (mono, juce::jmax (11.0f, t.getHeight() * 0.95f), juce::Font::bold)));
        g.setColour (lcdText.withAlpha (0.18f));
        g.drawText (gran, t.translated (0.0f, 0.6f).expanded (1.0f, 0.0f), juce::Justification::centredLeft);
        g.setColour (lcdText);
        g.drawText (gran, t, juce::Justification::centredLeft);
    }

    /** Ranura on llisca el fader (vertical o horitzontal). */
    inline void ranura (juce::Graphics& g, juce::Rectangle<float> r)
    {
        const float radi = juce::jmin (r.getWidth(), r.getHeight()) / 2.0f;
        g.setColour (juce::Colour (0xff09090b));
        g.fillRoundedRectangle (r, radi);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        if (r.getHeight() > r.getWidth())
            g.fillRect (r.getRight() - 1.0f, r.getY() + radi, 1.0f, r.getHeight() - radi * 2.0f);
        else
            g.fillRect (r.getX() + radi, r.getBottom() - 1.0f, r.getWidth() - radi * 2.0f, 1.0f);
    }

    /** Capçal metàl·lic del fader amb ratlles i una línia del color de la pista. */
    inline void capFader (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour color, bool vertical, bool actiu = true)
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 3.0f);

        const auto a = actiu ? juce::Colour (0xffe6e6ea) : juce::Colour (0xff9a9aa2);
        const auto b = actiu ? juce::Colour (0xff8c8c95) : juce::Colour (0xff5c5c63);
        if (vertical)
        {
            juce::ColourGradient grad (a, r.getX(), r.getY(), b, r.getX(), r.getBottom(), false);
            grad.addColour (0.5, actiu ? juce::Colour (0xffb9b9c1) : juce::Colour (0xff76767e));
            g.setGradientFill (grad);
        }
        else
        {
            juce::ColourGradient grad (a, r.getX(), r.getY(), b, r.getRight(), r.getY(), false);
            grad.addColour (0.5, actiu ? juce::Colour (0xffb9b9c1) : juce::Colour (0xff76767e));
            g.setGradientFill (grad);
        }
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);

        // Ratlles d'agafar.
        g.setColour (juce::Colours::black.withAlpha (0.3f));
        if (vertical)
        {
            for (float dy : { -5.0f, -3.0f, 3.0f, 5.0f })
                g.fillRect (r.getX() + 3.0f, r.getCentreY() + dy, r.getWidth() - 6.0f, 1.0f);
            g.setColour (color);
            g.fillRect (r.getX() + 2.0f, r.getCentreY() - 1.0f, r.getWidth() - 4.0f, 2.0f);
        }
        else
        {
            for (float dx : { -5.0f, -3.0f, 3.0f, 5.0f })
                g.fillRect (r.getCentreX() + dx, r.getY() + 3.0f, 1.0f, r.getHeight() - 6.0f);
            g.setColour (color);
            g.fillRect (r.getCentreX() - 1.0f, r.getY() + 2.0f, 2.0f, r.getHeight() - 4.0f);
        }
    }

    /** Escala de dB al costat d'un fader vertical. «pos» converteix dB a posició 0..1. */
    template <typename Pos>
    inline void escalaVertical (juce::Graphics& g, juce::Rectangle<float> carril, float xText, Pos pos)
    {
        struct Marca { double db; const char* text; };
        const Marca marques[] = { { 6.0, "+6" }, { 0.0, "0" }, { -6.0, "6" }, { -12.0, "12" }, { -24.0, "24" }, { -48.0, "48" }, { -60.0, "" } };
        g.setFont (juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 9.5f, juce::Font::plain)));
        for (auto& m : marques)
        {
            const float y = carril.getBottom() - carril.getHeight() * pos (m.db);
            const bool zero = std::abs (m.db) < 0.01;
            g.setColour (zero ? tema::textMig : tema::linia2);
            g.fillRect (carril.getX() - 7.0f, y - 0.5f, 5.0f, 1.0f);
            g.fillRect (carril.getRight() + 2.0f, y - 0.5f, 5.0f, 1.0f);
            g.setColour (zero ? tema::textMig : tema::suau);
            g.drawText (m.db <= -60.0 ? juce::String::fromUTF8 ("∞") : juce::String (m.text),
                        juce::Rectangle<float> (xText, y - 6.0f, 22.0f, 12.0f), juce::Justification::centredRight);
        }
    }
}
