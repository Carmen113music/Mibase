#pragma once

#include "Text.h"

#include <juce_gui_basics/juce_gui_basics.h>

/** Colors de Mibase (els mateixos que Riffs) i aspecte fosc d'estudi. */
namespace tema
{
    // Fosc però no del tot negre (0.3: més clar que a la 0.1 i 0.2, perquè es llegeixi millor).
    inline const juce::Colour fons        { 0xff16161b };
    inline const juce::Colour lateral     { 0xff1b1b21 };   // columnes laterals
    inline const juce::Colour superficie  { 0xff212128 };
    inline const juce::Colour superficie2 { 0xff2c2c35 };
    inline const juce::Colour sobre       { 0xff272730 };   // quan el ratolí hi passa per sobre
    inline const juce::Colour triat       { 0xff2a2620 };   // fons d'un element triat (amb un punt de taronja)
    inline const juce::Colour linia       { 0xff383842 };
    inline const juce::Colour linia2      { 0xff4c4c58 };
    inline const juce::Colour text        { 0xfff5f4f8 };
    inline const juce::Colour textMig     { 0xffd6d5de };
    inline const juce::Colour suau        { 0xffa6a5b2 };
    inline const juce::Colour accent      { 0xffff8a2a };   // taronja (abans vermell)
    inline const juce::Colour error       { 0xffff5a4e };   // els errors i les saturacions continuen en vermell
    inline const juce::Colour errorText   { 0xffff8a80 };
    inline const juce::Colour guitarra    { 0xfff5c445 };
    inline const juce::Colour veu         { 0xff59d0d6 };
    inline const juce::Colour bateria     { 0xffb18cff };
    inline const juce::Colour sinte       { 0xff7fd67a };
    inline const juce::Colour midi        { 0xff6fa8ff };
    inline const juce::Colour audio       { 0xffc9b8a6 };   // pista d'àudio genèrica

    class Aspecte : public juce::LookAndFeel_V4
    {
    public:
        Aspecte()
            : juce::LookAndFeel_V4 (juce::LookAndFeel_V4::ColourScheme (
                  fons, superficie2, superficie, linia2, text, superficie2, text, accent, text))
        {
            setColour (juce::ResizableWindow::backgroundColourId, fons);
            setColour (juce::TextButton::buttonColourId, superficie2);
            setColour (juce::TextButton::buttonOnColourId, accent);
            setColour (juce::TextButton::textColourOffId, text);
            setColour (juce::TextButton::textColourOnId, text);
            setColour (juce::ComboBox::backgroundColourId, superficie2);
            setColour (juce::ComboBox::outlineColourId, linia2);
            setColour (juce::ComboBox::arrowColourId, suau);
            setColour (juce::PopupMenu::backgroundColourId, superficie);
            setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
            setColour (juce::TextEditor::backgroundColourId, superficie);
            setColour (juce::TextEditor::outlineColourId, linia);
            setColour (juce::TextEditor::textColourId, text);
            setColour (juce::Label::textColourId, text);
            setColour (juce::Slider::thumbColourId, accent);
            setColour (juce::Slider::trackColourId, linia2);
            setColour (juce::Slider::backgroundColourId, superficie2);
            setColour (juce::Slider::textBoxBackgroundColourId, superficie);
            setColour (juce::Slider::textBoxOutlineColourId, linia);
            setColour (juce::ScrollBar::thumbColourId, linia2);
        }
    };
}
