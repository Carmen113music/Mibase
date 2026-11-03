#pragma once

#include "Dibuix.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <optional>

/** Botó de Mibase: text, icona opcional i quatre estils. */
class Boto : public juce::Button
{
public:
    enum class Estil { Primari, Secundari, Subtil, Discontinu };

    Boto (const juce::String& textBoto, std::optional<dibuix::Icona> icona = std::nullopt, Estil estil = Estil::Secundari)
        : juce::Button (textBoto), ic (icona), es (estil)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus (false);   // amb el ratolí, el botó no es queda marcat
    }

    void setEstil (Estil e) { es = e; repaint(); }
    void setMidaText (float m) { midaText = m; repaint(); }

    int ampladaIdeal() const
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (dibuix::lletra (midaText, es == Estil::Primari), getButtonText(), 0, 0);
        return (int) std::ceil (ga.getBoundingBox (0, -1, true).getWidth()) + (ic ? 26 : 0) + 32;
    }

    void paintButton (juce::Graphics& g, bool sobre, bool premut) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool actiu = isEnabled();
        juce::Colour fons, vora, colorText;

        switch (es)
        {
            case Estil::Primari:
                fons = actiu ? tema::accent : tema::linia;
                if (actiu && sobre) fons = fons.brighter (0.12f);
                if (actiu && premut) fons = fons.darker (0.15f);
                vora = fons;
                colorText = actiu ? tema::fons : tema::suau;
                break;
            case Estil::Secundari:
                fons = premut ? tema::linia : (sobre ? juce::Colour (0xff2a2a31) : tema::superficie2);
                vora = tema::linia2;
                colorText = actiu ? tema::text : tema::suau;
                break;
            case Estil::Subtil:
                fons = getToggleState() ? tema::linia : (sobre ? tema::superficie2 : juce::Colours::transparentBlack);
                vora = juce::Colours::transparentBlack;
                colorText = (getToggleState() || sobre) ? tema::text : juce::Colour (0xffc9c8d1);
                break;
            case Estil::Discontinu:
                fons = sobre ? tema::superficie2 : juce::Colours::transparentBlack;
                vora = tema::linia2;
                colorText = sobre ? tema::text : tema::suau;
                break;
        }

        g.setColour (fons);
        g.fillRoundedRectangle (r, 8.0f);

        if (es == Estil::Discontinu)
        {
            juce::Path p;
            p.addRoundedRectangle (r, 8.0f);
            const float guions[] = { 4.0f, 3.0f };
            juce::Path discontinu;
            juce::PathStrokeType (1.0f).createDashedStroke (discontinu, p, guions, 2);
            g.setColour (vora);
            g.fillPath (discontinu);
        }
        else if (! vora.isTransparent())
        {
            g.setColour (vora);
            g.drawRoundedRectangle (r, 8.0f, 1.0f);
        }

        if (hasKeyboardFocus (false))
        {
            g.setColour (tema::accent.withAlpha (0.6f));
            g.drawRoundedRectangle (r.expanded (1.5f), 9.0f, 1.5f);
        }

        const auto font = dibuix::lletra (midaText, es == Estil::Primari);
        juce::GlyphArrangement ga;
        ga.addLineOfText (font, getButtonText(), 0, 0);
        const float ampleText = ga.getBoundingBox (0, -1, true).getWidth();
        const float ampleIcona = ic ? 17.0f : 0.0f;
        const float separacio = (ic && getButtonText().isNotEmpty()) ? 8.0f : 0.0f;
        const float total = ampleText + ampleIcona + separacio;
        float x = r.getCentreX() - total / 2.0f;

        if (ic)
        {
            dibuix::pintaIcona (g, *ic, { x, r.getCentreY() - 8.5f, 17.0f, 17.0f }, colorText, es == Estil::Primari ? 2.4f : 1.9f);
            x += ampleIcona + separacio;
        }

        g.setColour (colorText);
        g.setFont (font);
        g.drawText (getButtonText(), juce::Rectangle<float> (x, r.getY(), ampleText + 4.0f, r.getHeight()), juce::Justification::centredLeft, false);
    }

private:
    std::optional<dibuix::Icona> ic;
    Estil es;
    float midaText = 14.0f;
};

/** Botons units on només n'hi ha un de triat (Tots / Només d'aquesta carpeta, 2/4 3/4 4/4 6/8…). */
class Segmentat : public juce::Component
{
public:
    std::function<void (int)> onCanvi;

    explicit Segmentat (const juce::StringArray& opcions, bool monoespai = false) : textos (opcions), mono (monoespai)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus (true);
        setMouseClickGrabsKeyboardFocus (false);
    }

    void setTriat (int i, bool avisa = false)
    {
        if (i == triat) return;
        triat = i;
        repaint();
        if (avisa && onCanvi) onCanvi (triat);
    }
    int getTriat() const noexcept { return triat; }

    int ampladaIdeal() const
    {
        int total = 8;
        for (auto& t : textos)
        {
            juce::GlyphArrangement ga;
            ga.addLineOfText (font(), t, 0, 0);
            total += (int) ga.getBoundingBox (0, -1, true).getWidth() + 26;
        }
        return total;
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (tema::superficie);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (tema::linia);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

        for (int i = 0; i < textos.size(); ++i)
        {
            auto z = zona (i);
            if (i == triat)
            {
                g.setColour (tema::linia);
                g.fillRoundedRectangle (z, 7.0f);
            }
            else if (i == sobre)
            {
                g.setColour (tema::superficie2);
                g.fillRoundedRectangle (z, 7.0f);
            }
            g.setColour (i == triat ? tema::text : tema::suau);
            g.setFont (font());
            g.drawText (textos[i], z, juce::Justification::centred);
        }

        if (hasKeyboardFocus (false))
        {
            g.setColour (tema::accent.withAlpha (0.6f));
            g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.5f);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override  { const int s = index (e.x); if (s != sobre) { sobre = s; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override    { sobre = -1; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override  { const int i = index (e.x); if (i >= 0) setTriat (i, true); }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::leftKey  && triat > 0)                   { setTriat (triat - 1, true); return true; }
        if (k == juce::KeyPress::rightKey && triat < textos.size() - 1)   { setTriat (triat + 1, true); return true; }
        return false;
    }

private:
    juce::Font font() const { return mono ? dibuix::mono (13.5f) : dibuix::lletra (13.0f); }

    float ampleText (int i) const
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (font(), textos[i], 0, 0);
        return ga.getBoundingBox (0, -1, true).getWidth() + 26.0f;
    }

    /** Cada opció, tan ampla com el seu text (i el que sobra, repartit). */
    juce::Rectangle<float> zona (int i) const
    {
        auto r = getLocalBounds().toFloat().reduced (3.0f);
        float total = 0.0f;
        for (int k = 0; k < textos.size(); ++k) total += ampleText (k);
        const float extra = juce::jmax (0.0f, (r.getWidth() - total) / (float) juce::jmax (1, textos.size()));
        const float escala = total > r.getWidth() ? r.getWidth() / total : 1.0f;
        float x = r.getX();
        for (int k = 0; k < i; ++k) x += ampleText (k) * escala + extra;
        return { x, r.getY(), ampleText (i) * escala + extra - 2.0f, r.getHeight() };
    }

    int index (int x) const
    {
        for (int i = 0; i < textos.size(); ++i)
            if (zona (i).getX() <= (float) x && (float) x < zona (i).getRight() + 2.0f)
                return i;
        return -1;
    }

    juce::StringArray textos;
    bool mono = false;
    int triat = 0, sobre = -1;
};

/** Separador entre dues seccions: arrossegant-lo es canvia l'amplada. Doble clic: amplada inicial. */
class Divisor : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void()> onComenca, onAcaba, onDobleClic;
    std::function<void (int)> onArrossega;   // píxels des d'on s'ha començat

    Divisor() { setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); setRepaintsOnMouseActivity (true); }

    void paint (juce::Graphics& g) override
    {
        const bool actiu = isMouseOverOrDragging();
        const float x = (float) getWidth() / 2.0f;
        g.setColour (actiu ? tema::accent : tema::linia);
        g.fillRect (juce::Rectangle<float> (x - (actiu ? 1.0f : 0.5f), 0.0f, actiu ? 2.0f : 1.0f, (float) getHeight()));
    }

    void mouseDown (const juce::MouseEvent&) override             { if (onComenca) onComenca(); }
    void mouseDrag (const juce::MouseEvent& e) override           { if (onArrossega) onArrossega (e.getDistanceFromDragStartX()); }
    void mouseUp (const juce::MouseEvent&) override               { if (onAcaba) onAcaba(); }
    void mouseDoubleClick (const juce::MouseEvent&) override      { if (onDobleClic) onDobleClic(); }
};
