#pragma once

#include "Controls.h"
#include "Dibuix.h"
#include "Tema.h"

/** 0.23 (Vocal Editor; 0.26 comú amb el samplejador): un pom. Arrossega amunt/avall (Maj = fi); doble clic = el valor de sempre. */
class Pom : public juce::Component, public juce::SettableTooltipClient
{
public:
    Pom (const juce::String& nom, double mn, double mx, double perDefecte, bool bipolar, std::function<juce::String (double)> text);
    std::function<void()> onComenca, onAcaba;
    std::function<void (double)> onCanvi;
    void setValor (double v, bool igualATots) { valor = v; igual = igualATots; repaint(); }
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
private:
    juce::String nom;
    double mn, mx, defecte, valor, valor0 = 0.0;
    bool bipolar, igual = true, arrossega = false;
    std::function<juce::String (double)> text;
    void posa (double v);
};


inline Pom::Pom (const juce::String& n, double minim, double maxim, double perDefecte, bool bi, std::function<juce::String (double)> t)
    : nom (n), mn (minim), mx (maxim), defecte (perDefecte), valor (perDefecte), bipolar (bi), text (std::move (t))
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

inline void Pom::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float alfa = isEnabled() ? 1.0f : 0.4f;
    g.setColour (tema::suau.withAlpha (alfa));
    g.setFont (dibuix::lletra (10.5f, true));
    g.drawText (dibuix::majuscules (nom), r.removeFromTop (14.0f), juce::Justification::centred);
    auto valorR = r.removeFromBottom (18.0f).reduced (6.0f, 1.0f);
    auto c = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()), juce::jmin (r.getWidth(), r.getHeight()));
    const auto centre = c.getCentre();
    const float radi = c.getWidth() * 0.5f;
    const float pi = juce::MathConstants<float>::pi;
    auto angle = [&] (double v) { return -0.75f * pi + (float) ((v - mn) / (mx - mn)) * 1.5f * pi; };
    juce::Path fons;
    fons.addCentredArc (centre.x, centre.y, radi - 2.0f, radi - 2.0f, 0.0f, -0.75f * pi, 0.75f * pi, true);
    g.setColour (juce::Colour (0xff09090b));
    g.strokePath (fons, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float a0 = bipolar ? angle (defecte) : -0.75f * pi, a1 = angle (valor);
    if (std::abs (a1 - a0) > 0.01f && isEnabled())
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radi - 2.0f, radi - 2.0f, 0.0f, juce::jmin (a0, a1), juce::jmax (a0, a1), true);
        g.setColour (igual ? tema::accent : tema::suau);
        g.strokePath (arc, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    auto cos = c.reduced (7.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (cos.translated (0.0f, 2.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff55555f), cos.getX(), cos.getY(), juce::Colour (0xff1f1f25), cos.getX(), cos.getBottom(), false));
    g.fillEllipse (cos);
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.drawEllipse (cos.reduced (0.5f), 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    for (int i = 0; i < 20; ++i)
    {
        const float a = (float) i / 20.0f * juce::MathConstants<float>::twoPi;
        g.drawLine ({ centre.getPointOnCircumference (cos.getWidth() * 0.5f - 0.5f, a), centre.getPointOnCircumference (cos.getWidth() * 0.5f - 3.0f, a) }, 1.0f);
    }
    g.setColour (isEnabled() ? juce::Colour (0xffeeeef2) : tema::suau);
    g.drawLine ({ centre.getPointOnCircumference (cos.getWidth() * 0.15f, a1), centre.getPointOnCircumference (cos.getWidth() * 0.5f - 3.0f, a1) }, 2.2f);
    // Valor (pantalleta).
    g.setColour (juce::Colours::black.withAlpha (0.45f * alfa));
    g.fillRoundedRectangle (valorR, 3.0f);
    g.setColour ((arrossega ? tema::accent : tema::guitarra).withAlpha (alfa));
    g.setFont (dibuix::mono (11.0f));
    g.drawText (isEnabled() ? (igual ? text (valor) : juce::String::fromUTF8 ("—")) : juce::String::fromUTF8 ("—"), valorR, juce::Justification::centred);
}

inline void Pom::posa (double v)
{
    valor = juce::jlimit (mn, mx, v);
    igual = true;
    if (onCanvi) onCanvi (valor);
    repaint();
}

inline void Pom::mouseDown (const juce::MouseEvent&) { valor0 = valor; arrossega = false; }

inline void Pom::mouseDrag (const juce::MouseEvent& e)
{
    if (! isEnabled()) return;
    if (! arrossega && e.getDistanceFromDragStart() < 2) return;
    if (! arrossega) { arrossega = true; if (onComenca) onComenca(); }
    const double rang = (mx - mn) * (e.mods.isShiftDown() ? 0.1 : 1.0);
    posa (valor0 - e.getDistanceFromDragStartY() * rang / 180.0);
}

inline void Pom::mouseUp (const juce::MouseEvent&)
{
    if (arrossega) { arrossega = false; if (onAcaba) onAcaba(); repaint(); }
}

inline void Pom::mouseDoubleClick (const juce::MouseEvent&)
{
    if (! isEnabled()) return;
    if (onComenca) onComenca();
    posa (defecte);
    if (onAcaba) onAcaba();
}

