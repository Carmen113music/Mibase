#pragma once

#include "Dibuix.h"
#include "Vintage.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <optional>

/** Botó de Mibase: text, icona opcional i quatre estils. */
class Boto : public juce::Button
{
public:
    /** Primari: tecla taronja · Secundari: tecla gris · Tecla: tecla amb llumeta (encesa si està activada)
        · Subtil: pla (pestanyes) · Discontinu: vora de ratlletes. */
    enum class Estil { Primari, Secundari, Tecla, Subtil, Discontinu };

    Boto (const juce::String& textBoto, std::optional<dibuix::Icona> icona = std::nullopt, Estil estil = Estil::Secundari)
        : juce::Button (textBoto), ic (icona), es (estil)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setWantsKeyboardFocus (false);   // amb el ratolí, el botó no es queda marcat
    }

    void setEstil (Estil e) { es = e; repaint(); }
    void setIcona (std::optional<dibuix::Icona> i) { ic = i; repaint(); }
    /** Color de la llumeta (estil Tecla). */
    void setLlum (juce::Colour c) { llum = c; repaint(); }
    /** Color fix de la icona (per exemple, el play verd). */
    void setColorIcona (juce::Colour c) { colorIcona = c; repaint(); }
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
        juce::Colour colorText;
        juce::Rectangle<float> contingut = r;

        switch (es)
        {
            case Estil::Primari:
            {
                // Tecla taronja.
                const float gruix = premut ? 1.0f : 3.0f;
                g.setColour (juce::Colours::black.withAlpha (0.45f));
                g.fillRoundedRectangle (r.translated (0.0f, 1.0f), 5.0f);
                auto cos = r.withTrimmedTop (premut ? 2.0f : 0.0f);
                g.setColour (actiu ? juce::Colour (0xff8a4210) : juce::Colour (0xff2a2a31));
                g.fillRoundedRectangle (cos, 4.0f);
                auto cara = cos.withTrimmedBottom (gruix);
                const auto a = actiu ? (sobre ? juce::Colour (0xffffb067) : juce::Colour (0xffffa04e)) : juce::Colour (0xff3a3a42);
                const auto b = actiu ? juce::Colour (0xffe8741c) : juce::Colour (0xff2e2e35);
                g.setGradientFill (juce::ColourGradient (a, cara.getX(), cara.getY(), b, cara.getX(), cara.getBottom(), false));
                g.fillRoundedRectangle (cara, 4.0f);
                g.setColour (juce::Colours::white.withAlpha (actiu ? 0.35f : 0.08f));
                g.fillRect (cara.getX() + 4.0f, cara.getY() + 1.0f, cara.getWidth() - 8.0f, 1.0f);
                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.drawRoundedRectangle (cos.reduced (0.5f), 4.0f, 1.0f);
                contingut = cara;
                colorText = actiu ? juce::Colour (0xff1a1206) : tema::suau;
                break;
            }
            case Estil::Secundari:
                contingut = vintage::tecla (g, r, premut, false, juce::Colours::transparentBlack, actiu, sobre);
                colorText = actiu ? tema::text : tema::suau;
                break;
            case Estil::Tecla:
                contingut = vintage::tecla (g, r, premut || getToggleState(), getToggleState(), llum, actiu, sobre);
                colorText = actiu ? (getToggleState() ? tema::text : tema::textMig) : tema::suau.withAlpha (0.6f);
                break;
            case Estil::Subtil:
            {
                if (getToggleState())
                {
                    // Com una tecla enfonsada.
                    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff18181d), r.getX(), r.getY(), juce::Colour (0xff26262e), r.getX(), r.getBottom(), false));
                    g.fillRoundedRectangle (r, 6.0f);
                    g.setColour (juce::Colours::black.withAlpha (0.6f));
                    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
                    g.setColour (tema::accent);
                    g.fillRoundedRectangle (r.withTrimmedTop (r.getHeight() - 3.0f).reduced (8.0f, 0.0f), 1.5f);
                }
                else if (sobre)
                {
                    g.setColour (tema::superficie2);
                    g.fillRoundedRectangle (r, 6.0f);
                }
                colorText = (getToggleState() || sobre) ? tema::text : tema::textMig;
                if (! actiu) colorText = tema::suau.withAlpha (0.6f);
                break;
            }
            case Estil::Discontinu:
            {
                g.setColour (sobre ? tema::superficie2 : juce::Colours::transparentBlack);
                g.fillRoundedRectangle (r, 8.0f);
                juce::Path p;
                p.addRoundedRectangle (r, 8.0f);
                const float guions[] = { 4.0f, 3.0f };
                juce::Path discontinu;
                juce::PathStrokeType (1.0f).createDashedStroke (discontinu, p, guions, 2);
                g.setColour (tema::linia2);
                g.fillPath (discontinu);
                colorText = sobre ? tema::text : tema::suau;
                break;
            }
        }

        const auto font = dibuix::lletra (midaText, es == Estil::Primari);
        juce::GlyphArrangement ga;
        ga.addLineOfText (font, getButtonText(), 0, 0);
        const float ampleText = ga.getBoundingBox (0, -1, true).getWidth();
        const float ampleIcona = ic ? juce::jmin (17.0f, contingut.getHeight() - 2.0f) : 0.0f;
        const float separacio = (ic && getButtonText().isNotEmpty()) ? 8.0f : 0.0f;
        const float total = ampleText + ampleIcona + separacio;
        float x = contingut.getCentreX() - total / 2.0f;

        if (ic)
        {
            const auto ci = (colorIcona.isTransparent() || ! actiu) ? colorText : colorIcona;
            dibuix::pintaIcona (g, *ic, { x, contingut.getCentreY() - ampleIcona / 2.0f, ampleIcona, ampleIcona }, ci, es == Estil::Primari ? 2.4f : 1.9f);
            x += ampleIcona + separacio;
        }

        g.setColour (colorText);
        g.setFont (font);
        g.drawText (getButtonText(), juce::Rectangle<float> (x, contingut.getY(), ampleText + 4.0f, contingut.getHeight()), juce::Justification::centredLeft, false);
    }

private:
    std::optional<dibuix::Icona> ic;
    Estil es;
    juce::Colour llum, colorIcona;
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
        // Com un selector de funcions d'un aparell: tecles juntes; la triada, enfonsada i amb la llum encesa.
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (r, 7.0f);

        for (int i = 0; i < textos.size(); ++i)
        {
            auto z = zona (i).withTrimmedRight (-1.0f);
            const bool triada = i == triat;
            auto c = vintage::tecla (g, z, triada, triada, colorLlum, isEnabled(), i == sobre && ! triada);
            g.setColour (triada ? tema::text : tema::textMig);
            g.setFont (font());
            g.drawText (textos[i], c, juce::Justification::centred);
        }

        if (hasKeyboardFocus (false))
        {
            g.setColour (tema::accent.withAlpha (0.6f));
            g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.5f);
        }
    }

    void setLlum (juce::Colour c) { colorLlum = c; repaint(); }

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
        auto r = getLocalBounds().toFloat().reduced (2.0f);
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
    juce::Colour colorLlum = vintage::ledTaronja;
    bool mono = false;
    int triat = 0, sobre = -1;
};

/** Separador entre dues seccions: arrossegant-lo es canvia l'amplada. Doble clic: amplada inicial. */
class Divisor : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void()> onComenca, onAcaba, onDobleClic;
    std::function<void (int)> onArrossega;   // píxels des d'on s'ha començat

    /** «fila»: separa una zona de dalt d'una de baix (s'arrossega amunt i avall; onArrossega dona els píxels en vertical). */
    explicit Divisor (bool esFila = false) : fila (esFila)
    {
        setMouseCursor (fila ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::LeftRightResizeCursor);
        setRepaintsOnMouseActivity (true);
    }

    void paint (juce::Graphics& g) override
    {
        const bool actiu = isMouseOverOrDragging();
        g.setColour (actiu ? tema::accent : tema::linia);
        if (fila)
        {
            const float y = (float) getHeight() / 2.0f;
            g.fillRect (juce::Rectangle<float> (0.0f, y - (actiu ? 1.0f : 0.5f), (float) getWidth(), actiu ? 2.0f : 1.0f));
            return;
        }
        const float x = (float) getWidth() / 2.0f;
        g.fillRect (juce::Rectangle<float> (x - (actiu ? 1.0f : 0.5f), 0.0f, actiu ? 2.0f : 1.0f, (float) getHeight()));
    }

    // Es compta en coordenades de pantalla: el separador es mou mentre l'arrossegues,
    // i comptar des de dins seu faria que la mida anés i tornés.
    void mouseDown (const juce::MouseEvent& e) override           { xInici = fila ? e.getScreenY() : e.getScreenX(); if (onComenca) onComenca(); }
    void mouseDrag (const juce::MouseEvent& e) override           { if (onArrossega) onArrossega ((fila ? e.getScreenY() : e.getScreenX()) - xInici); }
    void mouseUp (const juce::MouseEvent&) override               { if (onAcaba) onAcaba(); }
    void mouseDoubleClick (const juce::MouseEvent&) override      { if (onDobleClic) onDobleClic(); }

private:
    bool fila = false;
    int xInici = 0;
};
