#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <mibase/Harmonia.h>
#include "Controls.h"
#include "Dibuix.h"
#include "Tema.h"
#include "Text.h"
#include "Vintage.h"

/** El constructor d'acords (fonamental, tipus, tensions i nota greu). 0.37: tret del Piano Roll perquè també el faci
    servir la pista d'Acords. */
namespace editoracord
{
namespace harmonia = mibase::harmonia;

/** Editor d'un acord (com el de la captura): fonamental, tipus, tensions i nota greu; o escriure'l. */
class EditorAcord : public juce::Component
{
public:
    EditorAcord (harmonia::AcordPad inicial, std::function<void (const harmonia::AcordPad&)> canvi,
                 std::function<void (const harmonia::AcordPad&)> escolta)
        : acord (inicial), onCanvi (std::move (canvi)), onEscolta (std::move (escolta))
    {
        construeix();
    }

    /** 0.37: «Aplica» fa una altra cosa (p. ex. posar l'acord a la pista d'Acords) i «Buida» s'amaga. Si «dins» és cert,
        l'editor és part d'una altra finestra (no tanca cap CallOutBox ell sol). */
    void configura (const juce::String& textAplica, std::function<void (const harmonia::AcordPad&)> aplica, bool sensebuida)
    {
        btAplica.setButtonText (textAplica);
        onAplica = std::move (aplica);
        btBuida.setVisible (! sensebuida);
    }
    /** Canvia l'acord que es mostra (sense avisar). */
    void posaAcord (const harmonia::AcordPad& a) { acord = a.buit() ? harmonia::AcordPad { 0, 0, 0, -1 } : a; refresca(); }
    const harmonia::AcordPad& getAcord() const { return acord; }

private:
    void construeix()
    {
        if (acord.buit()) acord = { 0, 0, 0, -1 };
        auto columna = [this] (juce::OwnedArray<Boto>& v, int n, std::function<juce::String (int)> etiqueta, std::function<void (int)> clic)
        {
            for (int i = 0; i < n; ++i)
            {
                auto* b = v.add (new Boto (etiqueta (i), std::nullopt, Boto::Estil::Tecla));
                b->setLlum (vintage::ledTaronja);
                b->setMidaText (12.5f);
                b->onClick = [clic, i] { clic (i); };
                addAndMakeVisible (b);
            }
        };
        columna (fonamentals, 12, [] (int i) { return juce::String (harmonia::nomTo (i)); }, [this] (int i) { acord.arrel = i; canviat(); });
        columna (tipus, harmonia::numTipusPad, [] (int i) { return juce::String (harmonia::nomTipusPad (i)); }, [this] (int i) { acord.tipus = i; canviat(); });
        columna (tensions, harmonia::numTensions, [] (int i) { return juce::String (harmonia::nomTensio (i)); },
                 [this] (int i)
                 {
                     acord.tensions ^= (1u << i);
                     if (i == 0 && (acord.tensions & 1u)) acord.tensions &= ~2u;   // 7 i j7 no van junts
                     if (i == 1 && (acord.tensions & 2u)) acord.tensions &= ~1u;
                     canviat();
                 });
        columna (greus, 12, [] (int i) { return juce::String (harmonia::nomTo (i)); },
                 [this] (int i) { acord.baix = (acord.baix == i || i == acord.arrel) ? -1 : i; canviat(); });
        btEscolta.onClick = [this] { if (onEscolta) onEscolta (acord); };
        btBuida.onClick = [this] { if (onCanvi) onCanvi ({}); tanca(); };
        btMenys.onClick = [this] { inverteix (-1); };
        btMes.onClick = [this] { inverteix (1); };
        btAplica.onClick = [this] { if (onAplica) onAplica (acord); tanca(); };
        btEscolta.setTooltip ("Escolta l'acord"_ca);
        btBuida.setTooltip ("Buida el pad"_ca);
        btMenys.setTooltip ("Inversió avall: la nota de dalt baixa una octava"_ca);
        btMes.setTooltip ("Inversió amunt: la nota de baix puja una octava"_ca);
        btAplica.setTooltip ("Desa l'acord al pad i tanca l'editor (Retorn)"_ca);
        for (auto* b : { &btEscolta, &btBuida, &btMenys, &btMes, &btAplica }) { b->setMidaText (12.0f); addAndMakeVisible (b); }
        btMenys.setMidaText (16.0f);
        btMes.setMidaText (16.0f);
        setWantsKeyboardFocus (true);
        refresca();
        setSize (520, 404);
    }

public:
    void paint (juce::Graphics& g) override
    {
        vintage::panell (g, getLocalBounds().toFloat(), 8.0f);
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (22.0f, true));
        g.drawText (juce::String::fromUTF8 (harmonia::nomPad (acord).c_str()), 16, 8, 160, 30, juce::Justification::centredLeft);
        g.setFont (dibuix::lletra (11.5f));
        g.setColour (tema::suau);
        g.drawText (textInversio (acord.inversio), 16, 34, 170, 14, juce::Justification::centredLeft);
        const char* titols[] = { "Fonamental", "Tipus", "Tensions", "Nota greu" };
        g.setFont (dibuix::lletra (11.5f, true));
        g.setColour (tema::suau);
        for (int c = 0; c < 4; ++c)
            g.drawText (dibuix::majuscules (juce::String::fromUTF8 (titols[c])), 16 + c * 124, 54, 120, 16, juce::Justification::centredLeft);
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::returnKey) { if (onAplica) onAplica (acord); tanca(); return true; }
        if (k == juce::KeyPress::escapeKey) { tanca(); return true; }
        if (k.getTextCharacter() == '+') { inverteix (1); return true; }
        if (k.getTextCharacter() == '-') { inverteix (-1); return true; }
        return false;
    }

    void resized() override
    {
        // A dalt a la dreta, botons petits: Escolta, Buida, −, + i Aplica (que tanca l'editor).
        auto f = juce::Rectangle<int> (176, 14, getWidth() - 16 - 176, 26);
        btAplica.setBounds (f.removeFromRight (70)); f.removeFromRight (8);
        btMes.setBounds (f.removeFromRight (28)); f.removeFromRight (3);
        btMenys.setBounds (f.removeFromRight (28)); f.removeFromRight (8);
        btBuida.setBounds (f.removeFromRight (58)); f.removeFromRight (4);
        btEscolta.setBounds (f.removeFromRight (86));
        auto col = [this] (juce::OwnedArray<Boto>& v, int c)
        {
            for (int i = 0; i < v.size(); ++i) v[i]->setBounds (16 + c * 124, 74 + i * 27, 116, 25);
        };
        col (fonamentals, 0); col (tipus, 1); col (tensions, 2); col (greus, 3);
    }

private:
    void canviat()
    {
        refresca();
        if (onCanvi) onCanvi (acord);
        if (onEscolta) onEscolta (acord);
    }

    void inverteix (int d)
    {
        if (acord.buit()) return;
        acord.inversio = juce::jlimit (-harmonia::maxInversio, harmonia::maxInversio, acord.inversio + d);
        canviat();
    }

    void tanca()
    {
        if (auto* c = findParentComponentOfClass<juce::CallOutBox>()) c->dismiss();
    }

    static juce::String textInversio (int i)
    {
        if (i == 0) return "Posició fonamental"_ca;
        if (i > 0) return juce::String (i) + "a inversió (amunt)"_ca;
        return juce::String (-i) + "a inversió (avall)"_ca;
    }

    void refresca()
    {
        for (int i = 0; i < 12; ++i) fonamentals[i]->setToggleState (acord.arrel == i, juce::dontSendNotification);
        for (int i = 0; i < tipus.size(); ++i) tipus[i]->setToggleState (acord.tipus == i, juce::dontSendNotification);
        for (int i = 0; i < tensions.size(); ++i) tensions[i]->setToggleState ((acord.tensions >> i) & 1u, juce::dontSendNotification);
        for (int i = 0; i < 12; ++i) greus[i]->setToggleState (acord.baix == i, juce::dontSendNotification);
        repaint();
    }

    harmonia::AcordPad acord;
    std::function<void (const harmonia::AcordPad&)> onCanvi, onEscolta, onAplica;
    juce::OwnedArray<Boto> fonamentals, tipus, tensions, greus;
    Boto btEscolta { "Escolta", dibuix::Icona::Play, Boto::Estil::Secundari };
    Boto btBuida { "Buida", std::nullopt, Boto::Estil::Secundari };
    Boto btMenys { "-", std::nullopt, Boto::Estil::Secundari };
    Boto btMes { "+", std::nullopt, Boto::Estil::Secundari };
    Boto btAplica { "Aplica", std::nullopt, Boto::Estil::Primari };
};
}

using editoracord::EditorAcord;
