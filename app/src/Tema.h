#pragma once

#include "Text.h"

#include <juce_gui_basics/juce_gui_basics.h>

/** Colors de Mibase (els mateixos que Riffs) i aspecte fosc d'estudi. */
namespace tema
{
    inline const juce::Colour fons        { 0xff0e0e10 };
    inline const juce::Colour superficie  { 0xff18181c };
    inline const juce::Colour superficie2 { 0xff222227 };
    inline const juce::Colour linia       { 0xff2a2a31 };
    inline const juce::Colour linia2      { 0xff3a3a43 };
    inline const juce::Colour text        { 0xfff2f1f5 };
    inline const juce::Colour suau        { 0xff8d8c97 };
    inline const juce::Colour accent      { 0xffff4b3e };
    inline const juce::Colour guitarra    { 0xfff5b445 };
    inline const juce::Colour veu         { 0xff59d0d6 };
    inline const juce::Colour bateria     { 0xffb18cff };
    inline const juce::Colour sinte       { 0xff7fd67a };
    inline const juce::Colour midi        { 0xff6fa8ff };

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
