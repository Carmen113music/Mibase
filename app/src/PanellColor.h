#pragma once

#include "Dibuix.h"
#include "Vintage.h"

#include <array>

/** Tecla de la barra d'eines que mostra un quadret amb el color de la pista o dels clips triats. */
class BotoColor : public juce::Button
{
public:
    BotoColor() : juce::Button ("Color") {}
    void setColor (juce::Colour c) { if (c != color) { color = c; repaint(); } }

    void paintButton (juce::Graphics& g, bool sobre, bool premut) override
    {
        auto cara = vintage::tecla (g, getLocalBounds().toFloat(), premut, false, juce::Colours::transparentBlack, isEnabled(), sobre);
        auto q = cara.withSizeKeepingCentre (juce::jmin (cara.getWidth(), 22.0f), juce::jmin (cara.getHeight(), 16.0f));
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillRoundedRectangle (q.expanded (1.5f), 3.0f);
        g.setColour (color);
        g.fillRoundedRectangle (q, 2.5f);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.fillRect (q.getX() + 2.0f, q.getY() + 1.0f, q.getWidth() - 4.0f, 1.0f);
    }

private:
    juce::Colour color { 0xfff5c445 };
};

/** Roda de colors: el to va al voltant, la saturació del mig (blanc) a la vora; a la dreta, la lluminositat. */
class RodaColor : public juce::Component
{
public:
    std::function<void (juce::Colour)> onCanvi;

    void setColor (juce::Colour c)
    {
        to = c.getHue();
        saturacio = c.getSaturation();
        llum = juce::jmax (0.05f, c.getBrightness());
        imatge = {};
        repaint();
    }

    juce::Colour getColor() const { return juce::Colour::fromHSV (to, saturacio, llum, 1.0f); }

    void paint (juce::Graphics& g) override
    {
        const auto d = disc();
        if (imatge.isNull() || imatge.getWidth() != (int) d.getWidth())
            fesImatge ((int) d.getWidth());
        g.drawImageAt (imatge, (int) d.getX(), (int) d.getY());

        // Marca del color triat.
        const auto c = d.getCentre();
        const float radi = d.getWidth() * 0.5f;
        const auto p = c.getPointOnCircumference (radi * saturacio, to * juce::MathConstants<float>::twoPi);
        g.setColour (juce::Colours::black);
        g.drawEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (p), 2.5f);
        g.setColour (juce::Colours::white);
        g.drawEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (p), 1.2f);

        // Barra de lluminositat.
        const auto b = barra();
        g.setGradientFill (juce::ColourGradient (juce::Colour::fromHSV (to, saturacio, 1.0f, 1.0f), b.getX(), b.getY(),
                                                 juce::Colours::black, b.getX(), b.getBottom(), false));
        g.fillRoundedRectangle (b, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (b, 3.0f, 1.0f);
        const float y = b.getBottom() - b.getHeight() * llum;
        g.setColour (juce::Colours::white);
        g.fillRect (b.getX() - 3.0f, y - 1.5f, b.getWidth() + 6.0f, 3.0f);
    }

    void mouseDown (const juce::MouseEvent& e) override { enBarra = barra().expanded (6.0f).contains (e.position); mouseDrag (e); }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (enBarra)
        {
            const auto b = barra();
            llum = juce::jlimit (0.05f, 1.0f, (b.getBottom() - e.position.y) / b.getHeight());
            imatge = {};
        }
        else
        {
            const auto d = disc();
            const auto v = e.position - d.getCentre();
            float a = std::atan2 (v.x, -v.y);
            if (a < 0.0f) a += juce::MathConstants<float>::twoPi;
            to = a / juce::MathConstants<float>::twoPi;
            saturacio = juce::jlimit (0.0f, 1.0f, v.getDistanceFromOrigin() / (d.getWidth() * 0.5f));
        }
        repaint();
        if (onCanvi) onCanvi (getColor());
    }

private:
    juce::Rectangle<float> disc() const
    {
        const float m = (float) juce::jmin (getWidth() - 34, getHeight());
        return { 0.0f, ((float) getHeight() - m) * 0.5f, m, m };
    }
    juce::Rectangle<float> barra() const { return { (float) getWidth() - 18.0f, 4.0f, 14.0f, (float) getHeight() - 8.0f }; }

    void fesImatge (int mida)
    {
        imatge = juce::Image (juce::Image::ARGB, mida, mida, true);
        const float radi = (float) mida * 0.5f;
        juce::Image::BitmapData dades (imatge, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < mida; ++y)
            for (int x = 0; x < mida; ++x)
            {
                const float dx = (float) x + 0.5f - radi, dy = (float) y + 0.5f - radi;
                const float dist = std::sqrt (dx * dx + dy * dy);
                if (dist > radi) continue;
                float a = std::atan2 (dx, -dy);
                if (a < 0.0f) a += juce::MathConstants<float>::twoPi;
                const float vora = juce::jlimit (0.0f, 1.0f, radi - dist);   // vora suau
                dades.setPixelColour (x, y, juce::Colour::fromHSV (a / juce::MathConstants<float>::twoPi, dist / radi, llum, vora));
            }
    }

    float to = 0.12f, saturacio = 0.7f, llum = 0.95f;
    bool enBarra = false;
    juce::Image imatge;
};

/** Finestreta del color: colors preparats i la roda. «onTria» es crida a cada canvi. */
class PanellColor : public juce::Component
{
public:
    PanellColor (juce::Colour actual, const juce::String& que, std::function<void (juce::Colour)> tria)
        : onTria (std::move (tria)), destinacio (que)
    {
        addAndMakeVisible (roda);
        roda.setColor (actual);
        roda.onCanvi = [this] (juce::Colour c) { triat = c; repaint(); if (onTria) onTria (c); };
        triat = actual;
        setSize (330, 300);
    }

    static const std::array<juce::uint32, 16>& preparats()
    {
        static const std::array<juce::uint32, 16> c {
            0xfff5c445, 0xff59d0d6, 0xffb18cff, 0xff7fd67a, 0xff6fa8ff, 0xffff8fc4, 0xffe8a87c, 0xffc9b8a6,
            0xffff8a2a, 0xffff5a4e, 0xffd4e157, 0xff3ec9a7, 0xff8e9aff, 0xffd98cf0, 0xffb0b0b8, 0xfff2f1f5 };
        return c;
    }

    void paint (juce::Graphics& g) override
    {
        vintage::panell (g, getLocalBounds().toFloat(), 8.0f);
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (14.0f, true));
        g.drawText ("Color: " + destinacio, 14, 8, getWidth() - 28, 22, juce::Justification::centredLeft, true);
        for (int i = 0; i < 16; ++i)
        {
            auto r = quadre (i).toFloat();
            const juce::Colour c (preparats()[(size_t) i]);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.fillRoundedRectangle (r.expanded (1.5f), 4.0f);
            g.setColour (c);
            g.fillRoundedRectangle (r, 3.0f);
            if (c == triat)
            {
                g.setColour (juce::Colours::white);
                g.drawRoundedRectangle (r.expanded (2.5f), 4.0f, 1.5f);
            }
        }
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.5f));
        g.drawText ("O tria'l a la roda:", 14, 108, 200, 16, juce::Justification::centredLeft);
    }

    void resized() override { roda.setBounds (14, 128, getWidth() - 28, getHeight() - 140); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < 16; ++i)
            if (quadre (i).expanded (2).contains (e.getPosition()))
            {
                triat = juce::Colour (preparats()[(size_t) i]);
                roda.setColor (triat);
                repaint();
                if (onTria) onTria (triat);
            }
    }

private:
    juce::Rectangle<int> quadre (int i) const
    {
        const int ample = (getWidth() - 28 - 7 * 8) / 8;
        return { 14 + (i % 8) * (ample + 8), 38 + (i / 8) * 34, ample, 26 };
    }

    std::function<void (juce::Colour)> onTria;
    juce::String destinacio;
    juce::Colour triat;
    RodaColor roda;
};
