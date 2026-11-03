#pragma once

#include "Controls.h"

#include <mibase/ProjecteDisc.h>

/** Finestra «Projecte nou», per sobre de la pantalla d'inici:
    nom, on es crearà, tempo, compàs, amb què es comença i què es crearà al disc. */
class FinestraProjecteNou : public juce::Component,
                            private juce::TextEditor::Listener
{
public:
    explicit FinestraProjecteNou (const juce::File& carpetaMare);

    std::function<void (const mibase::Projecte&)> onCrea;
    std::function<void()> onTanca;

    /** Mostra un error que ha passat en crear-lo. */
    void setError (const juce::String& error);

    /** Posa el cursor al nom. */
    void enfocaNom() { nom.grabKeyboardFocus(); }

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& k) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    void textEditorTextChanged (juce::TextEditor&) override { valida(); }
    void textEditorReturnKeyPressed (juce::TextEditor&) override { crea(); }
    void textEditorEscapeKeyPressed (juce::TextEditor&) override { if (onTanca) onTanca(); }

    void valida();
    void crea();
    void canviaTempo (int delta);

    juce::File carpeta;
    juce::Rectangle<int> panell, columnaDreta, zonaPlantilles, zonaCami, zonaTempo, peu;
    juce::Rectangle<int> targetaBuit, targetaBuida;

    juce::TextEditor nom;
    juce::String avis;
    bool error = false;

    Boto btTanca { {}, dibuix::Icona::Tanca, Boto::Estil::Subtil };
    Boto btCanvia { "Canvia"_ca, std::nullopt, Boto::Estil::Subtil };
    Boto btMenys { "-", std::nullopt, Boto::Estil::Subtil }, btMes { "+", std::nullopt, Boto::Estil::Subtil };
    Segmentat compas { { "2/4", "3/4", "4/4", "6/8" }, true };
    Boto btCancella { "Cancel·la"_ca }, btCrea { "Crea i entra a l'estudi"_ca, std::nullopt, Boto::Estil::Primari };

    int tempo = 120;
};
