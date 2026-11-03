#pragma once

#include "Controls.h"

/** Finestreta per demanar un text (per exemple, el nom d'una carpeta nova), amb l'estil de Mibase.
    Es posa per sobre de tota la finestra i es tanca sola. */
class DialegText : public juce::Component, private juce::TextEditor::Listener
{
public:
    /** valida: retorna un missatge d'error (o buit si el text és bo). */
    DialegText (const juce::String& titolDialeg, const juce::String& explicacio, const juce::String& textBoto,
                std::function<juce::String (const juce::String&)> validaText,
                std::function<void (const juce::String&)> enAcceptar)
        : titol (titolDialeg), subtitol (explicacio), valida (std::move (validaText)), accepta (std::move (enAcceptar)),
          btAccepta (textBoto, std::nullopt, Boto::Estil::Primari)
    {
        setWantsKeyboardFocus (true);
        text.setFont (dibuix::lletra (15.0f));
        text.setIndents (12, 12);
        text.setColour (juce::TextEditor::backgroundColourId, tema::fons);
        text.setColour (juce::TextEditor::outlineColourId, tema::linia2);
        text.setColour (juce::TextEditor::focusedOutlineColourId, tema::accent);
        text.addListener (this);
        addAndMakeVisible (text);

        addAndMakeVisible (btAccepta);
        addAndMakeVisible (btCancella);
        btAccepta.onClick = [this] { intenta(); };
        btCancella.onClick = [this] { tanca(); };
        actualitza();
    }

    /** Es posa sobre tota la finestra de «sobre» i hi deixa el cursor. */
    static void mostra (juce::Component& sobre, std::unique_ptr<DialegText>& propietari)
    {
        auto* arrel = sobre.getTopLevelComponent();
        arrel->addAndMakeVisible (*propietari);
        propietari->setBounds (arrel->getLocalBounds());
        propietari->text.grabKeyboardFocus();
        propietari->propietariPtr = &propietari;
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xa0060608));
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (panell.toFloat().translated (0, 10).expanded (6), 18.0f);
        g.setColour (tema::superficie);
        g.fillRoundedRectangle (panell.toFloat(), 14.0f);
        g.setColour (tema::linia2);
        g.drawRoundedRectangle (panell.toFloat().reduced (0.5f), 14.0f, 1.0f);

        auto r = panell.reduced (24, 20);
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (18.0f, true));
        g.drawText (titol, r.removeFromTop (28), juce::Justification::centredLeft);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (13.0f));
        g.drawFittedText (subtitol, r.removeFromTop (38), juce::Justification::topLeft, 2, 0.8f);

        g.setColour (error.isNotEmpty() ? tema::errorText : tema::suau);
        g.setFont (dibuix::lletra (12.5f));
        g.drawText (error, text.getBounds().translated (0, 48).withHeight (20), juce::Justification::centredLeft);
    }

    void resized() override
    {
        panell = getLocalBounds().withSizeKeepingCentre (juce::jmin (480, getWidth() - 40), 236);
        auto r = panell.reduced (24, 20);
        r.removeFromTop (70);
        text.setBounds (r.removeFromTop (44));
        auto peu = r.removeFromBottom (44);
        btAccepta.setBounds (peu.removeFromRight (btAccepta.ampladaIdeal()));
        peu.removeFromRight (10);
        btCancella.setBounds (peu.removeFromRight (btCancella.ampladaIdeal()));
    }

    void mouseDown (const juce::MouseEvent& e) override { if (! panell.contains (e.getPosition())) tanca(); }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey) { tanca(); return true; }
        return true;
    }

private:
    void textEditorTextChanged (juce::TextEditor&) override { actualitza(); }
    void textEditorReturnKeyPressed (juce::TextEditor&) override { intenta(); }
    void textEditorEscapeKeyPressed (juce::TextEditor&) override { tanca(); }

    void actualitza()
    {
        const auto t = text.getText();
        error = t.isEmpty() ? juce::String() : valida (t);
        btAccepta.setEnabled (t.isNotEmpty() && error.isEmpty());
        text.setColour (juce::TextEditor::outlineColourId, error.isNotEmpty() ? tema::error : tema::linia2);
        repaint();
    }

    void intenta()
    {
        if (! btAccepta.isEnabled())
            return;
        const auto t = text.getText();
        auto fer = accepta;
        tanca();   // s'esborrarà just després
        fer (t);
    }

    void tanca()
    {
        auto* p = propietariPtr;
        juce::MessageManager::callAsync ([p] { if (p != nullptr && *p != nullptr) p->reset(); });
        setVisible (false);
    }

    juce::String titol, subtitol, error;
    std::function<juce::String (const juce::String&)> valida;
    std::function<void (const juce::String&)> accepta;
    juce::TextEditor text;
    Boto btAccepta;
    Boto btCancella { "Cancel·la"_ca };
    juce::Rectangle<int> panell;
    std::unique_ptr<DialegText>* propietariPtr = nullptr;
};
