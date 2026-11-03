#pragma once

#include "Controls.h"

#include <mibase/ProjecteDisc.h>

/** Estudi PROVISIONAL (versió 0.2): mostra el projecte obert.
    L'estudi de debò (pistes, regla, bandera, graella, metrònom, panells ajustables) arriba a la 0.3. */
class PantallaEstudi : public juce::Component
{
public:
    PantallaEstudi (const juce::File& f, const mibase::Projecte& p) : fitxer (f), projecte (p)
    {
        addAndMakeVisible (btInici);
        addAndMakeVisible (btCarpeta);
        btInici.onClick = [this] { if (onInici) onInici(); };
        btCarpeta.onClick = [this] { fitxer.revealToUser(); };
        evolucio = mibase::disc::llegeixEvolucio (dibuix::cami (fitxer.getParentDirectory()), 8);
    }

    std::function<void()> onInici;
    const juce::File& getFitxer() const noexcept { return fitxer; }
    juce::String getNom() const { return dibuix::utf8 (projecte.nom); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (tema::fons);
        auto r = getLocalBounds().reduced (40, 32);

        g.setColour (tema::text);
        g.setFont (dibuix::lletra (28.0f, true));
        g.drawText (getNom(), r.removeFromTop (40), juce::Justification::centredLeft, true);
        g.setColour (tema::suau);
        g.setFont (dibuix::mono (13.0f));
        g.drawText (juce::String ((int) std::round (projecte.tempo)) + " bpm · "_ca + juce::String (projecte.compasNum) + "/"
                    + juce::String (projecte.compasDen) + "   ·   "_ca + fitxer.getFullPathName(),
                    r.removeFromTop (24), juce::Justification::centredLeft, true);

        r.removeFromTop (28);
        auto caixa = r.removeFromTop (150).withWidth (juce::jmin (r.getWidth(), 760));
        juce::Path p;
        p.addRoundedRectangle (caixa.toFloat().reduced (0.5f), 12.0f);
        juce::Path discontinu;
        const float guions[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.0f).createDashedStroke (discontinu, p, guions, 2);
        g.setColour (tema::linia2);
        g.fillPath (discontinu);

        auto t = caixa.reduced (24, 20);
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (16.0f, true));
        g.drawText ("Aquí anirà l'estudi (versió 0.3)"_ca, t.removeFromTop (26), juce::Justification::centredLeft);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (14.0f));
        g.drawFittedText ("Pistes i clips com a Riffs, regla amb compassos, bandera del primer temps, graella "
                          "Compàs/Temps/Lliure, metrònom, i seccions que es poden fer més amples o més estretes "_ca
                          "arrossegant la vora entre elles."_ca,
                          t.removeFromTop (60), juce::Justification::topLeft, 3);

        r.removeFromTop (28);
        dibuix::pintaEtiquetaSeccio (g, "Evolució del projecte"_ca, r.removeFromTop (20));
        r.removeFromTop (6);
        for (auto& e : evolucio)
        {
            auto fila = r.removeFromTop (26);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (12.5f));
            g.drawText (dibuix::utf8 (e.quan), fila.removeFromLeft (130), juce::Justification::centredLeft);
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (13.5f));
            g.drawText (dibuix::utf8 (e.text), fila, juce::Justification::centredLeft, true);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (40, 32);
        auto fila = r.removeFromTop (40);
        btCarpeta.setBounds (fila.removeFromRight (btCarpeta.ampladaIdeal()).reduced (0, 2));
        fila.removeFromRight (10);
        btInici.setBounds (fila.removeFromRight (btInici.ampladaIdeal()).reduced (0, 2));
    }

private:
    juce::File fitxer;
    mibase::Projecte projecte;
    std::vector<mibase::disc::EntradaEvolucio> evolucio;
    Boto btInici { "Torna a l'inici"_ca, dibuix::Icona::Casa };
    Boto btCarpeta { "Mostra la carpeta" };
};
