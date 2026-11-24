#pragma once

#include "Dibuix.h"
#include "Tema.h"
#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>

/** 1.3: editar un text curt al mateix lloc (nom de pista, tempo…), sense obrir cap finestra.
    Enter o clicar fora desa; Esc cancel·la. Si «valida» torna un text, no es desa: es posa vermell i el text queda per corregir.
    L'editor s'esborra sol en acabar. */
class EditorEnLinia : public juce::TextEditor
{
public:
    static void obre (juce::Component& pare, juce::Rectangle<int> zona, const juce::String& text,
                      std::function<void (const juce::String&)> desa, std::function<juce::String (const juce::String&)> valida = {},
                      float midaLletra = 14.0f)
    {
        auto* e = new EditorEnLinia (std::move (desa), std::move (valida));
        e->setFont (dibuix::lletra (midaLletra, true));
        e->setText (text, false);
        e->setBounds (zona);
        pare.addAndMakeVisible (e);
        e->selectAll();
        e->grabKeyboardFocus();
    }

private:
    EditorEnLinia (std::function<void (const juce::String&)> d, std::function<juce::String (const juce::String&)> v) : desa (std::move (d)), valida (std::move (v))
    {
        setColour (juce::TextEditor::backgroundColourId, tema::fons.darker (0.4f));
        setColour (juce::TextEditor::textColourId, tema::text);
        setColour (juce::TextEditor::outlineColourId, tema::accent);
        setColour (juce::TextEditor::focusedOutlineColourId, tema::accent);
        setColour (juce::TextEditor::highlightColourId, tema::accent.withAlpha (0.4f));
        setJustification (juce::Justification::centredLeft);
        setIndents (4, 2);
        setSelectAllWhenFocused (true);
        onReturnKey = [this] { acaba (true); };
        onEscapeKey = [this] { acaba (false); };
        onFocusLost = [this] { acaba (true); };
    }

    void acaba (bool desar)
    {
        if (acabat) return;
        if (desar && valida)
        {
            const auto err = valida (getText());
            if (err.isNotEmpty())
            {
                setColour (juce::TextEditor::outlineColourId, juce::Colours::red);
                setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::red);
                setTooltip (err);
                repaint();
                if (hasKeyboardFocus (true)) return;   // Enter amb un error: continua editant
                desar = false;                          // ha perdut el focus amb un error: es cancel·la
            }
        }
        acabat = true;
        const auto t = getText();
        auto d = desa;
        juce::Component::SafePointer<EditorEnLinia> jo (this);
        juce::MessageManager::callAsync ([jo, d, t, desar]
        {
            if (desar && d) d (t);
            if (jo != nullptr) { if (auto* p = jo->getParentComponent()) p->removeChildComponent (jo.getComponent()); delete jo.getComponent(); }
        });
    }

    std::function<void (const juce::String&)> desa;
    std::function<juce::String (const juce::String&)> valida;
    bool acabat = false;
};
