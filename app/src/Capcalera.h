#pragma once

#include "Controls.h"
#include "ServeiAudio.h"

/** Barra de dalt, a totes les pantalles: logotip, pestanyes (Inici, Estudi, Diagnòstic) i estat de la targeta. */
class Capcalera : public juce::Component, private juce::Timer
{
public:
    enum class Pantalla { Inici, Estudi, Diagnostic };

    explicit Capcalera (ServeiAudio& s) : servei (s)
    {
        for (auto* b : { &btInici, &btEstudi, &btDiag })
        {
            addAndMakeVisible (b);
            b->setClickingTogglesState (false);
        }
        btInici.onClick = [this] { if (onPantalla) onPantalla (Pantalla::Inici); };
        btEstudi.onClick = [this] { if (onPantalla) onPantalla (Pantalla::Estudi); };
        btDiag.onClick = [this] { if (onPantalla) onPantalla (Pantalla::Diagnostic); };
        btDiag.setTooltip ("Targeta d'àudio, teclat MIDI i latència"_ca);

        actualitza();
        startTimer (1000);
    }

    std::function<void (Pantalla)> onPantalla;

    void setPantalla (Pantalla p)
    {
        btInici.setToggleState (p == Pantalla::Inici, juce::dontSendNotification);
        btEstudi.setToggleState (p == Pantalla::Estudi, juce::dontSendNotification);
        btDiag.setToggleState (p == Pantalla::Diagnostic, juce::dontSendNotification);
        repaint();
    }

    /** Nom del projecte obert (buit: cap). */
    void setProjecte (const juce::String& nom)
    {
        btEstudi.setButtonText (nom.isEmpty() ? juce::String ("Estudi") : "Estudi · "_ca + nom);
        btEstudi.setVisible (nom.isNotEmpty());
        resized();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (tema::fons);
        g.setColour (tema::linia);
        g.fillRect (0, getHeight() - 1, getWidth(), 1);

        dibuix::pintaLogo (g, { 20.0f, (float) getHeight() / 2.0f - 13.0f, 26.0f, 26.0f });
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (17.0f, true));
        g.drawText ("Mibase", 56, 0, 80, getHeight(), juce::Justification::centredLeft);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (12.0f));
        g.drawText (MIBASE_VERSIO, 124, 0, 50, getHeight(), juce::Justification::centredLeft);

        // Pastilla de l'estat de la targeta.
        if (! zonaEstat.isEmpty())
        {
            auto p = zonaEstat.toFloat();
            g.setColour (tema::superficie);
            g.fillRoundedRectangle (p, p.getHeight() / 2.0f);
            g.setColour (tema::linia);
            g.drawRoundedRectangle (p.reduced (0.5f), p.getHeight() / 2.0f, 1.0f);
            g.setColour (resum.isEmpty() ? tema::error : tema::sinte);
            g.fillEllipse (p.getX() + 12.0f, p.getCentreY() - 4.0f, 8.0f, 8.0f);
            g.setColour (tema::textMig);
            g.setFont (dibuix::lletra (13.0f));
            g.drawFittedText (resum.isEmpty() ? juce::String ("Àudio aturat: obre el Diagnòstic"_ca) + "" : resum + extra,
                              zonaEstat.withTrimmedLeft (28).withTrimmedRight (12), juce::Justification::centredLeft, 1, 0.8f);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14, 10);
        r.removeFromLeft (170);

        btDiag.setBounds (r.removeFromRight (btDiag.ampladaIdeal()));

        auto nav = r.removeFromLeft (btInici.ampladaIdeal() + (btEstudi.isVisible() ? juce::jmin (260, btEstudi.ampladaIdeal()) + 6 : 0));
        btInici.setBounds (nav.removeFromLeft (btInici.ampladaIdeal()));
        nav.removeFromLeft (6);
        btEstudi.setBounds (nav);

        r.removeFromRight (10);
        r.removeFromLeft (20);
        const int amplePastilla = juce::jmin (r.getWidth(), 460);
        zonaEstat = amplePastilla > 160 ? r.removeFromRight (amplePastilla) : juce::Rectangle<int>();
    }

private:
    void timerCallback() override { actualitza(); }

    void actualitza()
    {
        auto nou = servei.resumDispositiu();
        const double ms = servei.latenciaMsDesada();
        auto nouExtra = ms > 0 ? " · retard "_ca + juce::String (ms, 1).replaceCharacter ('.', ',') + " ms" : juce::String();
        if (nou != resum || nouExtra != extra)
        {
            resum = nou;
            extra = nouExtra;
            repaint (zonaEstat);
        }
    }

    ServeiAudio& servei;
    Boto btInici { "Inici", dibuix::Icona::Casa, Boto::Estil::Subtil };
    Boto btEstudi { "Estudi", std::nullopt, Boto::Estil::Subtil };
    Boto btDiag { "Diagnòstic"_ca, dibuix::Icona::Ajustos, Boto::Estil::Subtil };
    juce::Rectangle<int> zonaEstat;
    juce::String resum, extra;
};
