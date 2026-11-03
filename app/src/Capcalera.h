#pragma once

#include "Controls.h"
#include "ServeiAudio.h"

/** Barra de dalt, a totes les pantalles: logotip, menú Fitxer, pestanyes (Inici, Estudi, Diagnòstic),
    l'estat de la targeta i, fix a la dreta, el transport (com un radiocasset) amb la posició. */
class Capcalera : public juce::Component, private juce::Timer
{
public:
    enum class Pantalla { Inici, Estudi, Diagnostic };
    enum class Transport { Principi, Rebobina, Endavant, Atura, Play, Grava };

    explicit Capcalera (ServeiAudio& s) : servei (s)
    {
        for (auto* b : { &btFitxer, &btInici, &btEstudi, &btDiag })
            addAndMakeVisible (b);
        btFitxer.onClick = [this] { if (onFitxer) onFitxer (btFitxer); };
        btInici.onClick = [this] { if (onPantalla) onPantalla (Pantalla::Inici); };
        btEstudi.onClick = [this] { if (onPantalla) onPantalla (Pantalla::Estudi); };
        btDiag.onClick = [this] { if (onPantalla) onPantalla (Pantalla::Diagnostic); };
        btDiag.setTooltip ("Targeta d'àudio, teclat MIDI i latència"_ca);

        struct T { Boto* b; Transport t; const char* ajuda; };
        const T tecles[] = {
            { &btPrincipi, Transport::Principi, "Al principi (Inici)" },
            { &btRebobina, Transport::Rebobina, "Enrere un compàs" },
            { &btEndavant, Transport::Endavant, "Endavant un compàs" },
            { &btAtura,    Transport::Atura,    "Atura" },
            { &btPlay,     Transport::Play,     "Reprodueix (arriba a la versió 0.5)" },
            { &btGrava,    Transport::Grava,    "Grava (arriba a la versió 0.5)" },
        };
        for (auto& t : tecles)
        {
            addAndMakeVisible (t.b);
            t.b->setTooltip (juce::String::fromUTF8 (t.ajuda));
            const auto quina = t.t;
            t.b->onClick = [this, quina] { if (onTransport) onTransport (quina); };
        }
        btPlay.setColorIcona (vintage::ledVerd);
        btGrava.setColorIcona (vintage::ledVermell);
        btPlay.setLlum (vintage::ledVerd);
        btGrava.setLlum (vintage::ledVermell);

        setProjecte ({});
        actualitza();
        startTimerHz (15);
    }

    std::function<void (Pantalla)> onPantalla;
    std::function<void (juce::Component&)> onFitxer;
    std::function<void (Transport)> onTransport;
    /** Posició per a la pantalla: {gran, petit}. Buit: cap projecte obert. */
    std::function<std::pair<juce::String, juce::String>()> onPosicio;

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
        for (auto* b : { &btPrincipi, &btRebobina, &btEndavant, &btAtura })
            b->setEnabled (nom.isNotEmpty());
        btPlay.setEnabled (false);    // 0.5
        btGrava.setEnabled (false);   // 0.5
        resized();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        vintage::panell (g, getLocalBounds().toFloat());

        dibuix::pintaLogo (g, { 18.0f, (float) getHeight() / 2.0f - 13.0f, 26.0f, 26.0f });
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (17.0f, true));
        g.drawText ("Mibase", 54, 0, 80, getHeight(), juce::Justification::centredLeft);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.5f));
        g.drawText (MIBASE_VERSIO, 120, 0, 44, getHeight(), juce::Justification::centredLeft);

        // Estat de la targeta (el que s'encongeix si no hi cap).
        if (! zonaEstat.isEmpty())
        {
            auto p = zonaEstat.toFloat();
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (p, p.getHeight() / 2.0f);
            g.setColour (resum.isEmpty() ? tema::error : vintage::ledVerd);
            g.fillEllipse (p.getX() + 12.0f, p.getCentreY() - 4.0f, 8.0f, 8.0f);
            g.setColour (tema::textMig);
            g.setFont (dibuix::lletra (12.5f));
            g.drawFittedText (resum.isEmpty() ? "Àudio aturat: obre el Diagnòstic"_ca : resum + extra,
                              zonaEstat.withTrimmedLeft (28).withTrimmedRight (12), juce::Justification::centredLeft, 1, 0.75f);
        }

        // Marc del transport i pantalla de posició.
        auto marc = zonaTransport.toFloat().expanded (6.0f, 5.0f);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (marc, 8.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawRoundedRectangle (marc.reduced (0.5f), 8.0f, 1.0f);
        vintage::lcd (g, zonaLcd.toFloat(), posicio.first.isEmpty() ? juce::String ("--.-") : posicio.first,
                      posicio.second.isEmpty() ? juce::String ("--:--.---") : posicio.second, "COMPÀS"_ca);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12, 9);
        r.removeFromLeft (156);

        // Dreta, fix: transport i pantalla (mai es mou ni queda tapat).
        zonaLcd = r.removeFromRight (156);
        r.removeFromRight (8);
        const int tecla = 42;
        zonaTransport = r.removeFromRight (tecla * 6 + 5 * 3).withTrimmedTop (1).withTrimmedBottom (1);
        {
            auto t = zonaTransport;
            for (auto* b : { &btPrincipi, &btRebobina, &btEndavant, &btAtura, &btPlay, &btGrava })
            {
                b->setBounds (t.removeFromLeft (tecla));
                t.removeFromLeft (3);
            }
        }
        zonaLcd = zonaLcd.withY (zonaTransport.getY() - 2).withHeight (zonaTransport.getHeight() + 4);
        r.removeFromRight (18);

        // Esquerra: Fitxer i pestanyes.
        btFitxer.setBounds (r.removeFromLeft (btFitxer.ampladaIdeal()));
        r.removeFromLeft (8);
        btInici.setBounds (r.removeFromLeft (btInici.ampladaIdeal()));
        r.removeFromLeft (4);
        if (btEstudi.isVisible())
        {
            btEstudi.setBounds (r.removeFromLeft (juce::jmin (240, btEstudi.ampladaIdeal(), juce::jmax (90, r.getWidth() - 150))));
            r.removeFromLeft (4);
        }
        btDiag.setBounds (r.removeFromLeft (btDiag.ampladaIdeal()));
        r.removeFromLeft (14);

        zonaEstat = r.getWidth() > 150 ? r.withSizeKeepingCentre (juce::jmin (r.getWidth(), 420), 30).withX (r.getX()) : juce::Rectangle<int>();
    }

private:
    void timerCallback() override { actualitza(); }

    void actualitza()
    {
        if (++comptador % 15 == 0 || comptador == 1)
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

        auto p = onPosicio ? onPosicio() : std::pair<juce::String, juce::String>();
        if (p != posicio)
        {
            posicio = p;
            repaint (zonaLcd);
        }
    }

    ServeiAudio& servei;
    Boto btFitxer { "Fitxer", dibuix::Icona::FletxaAvall, Boto::Estil::Subtil };
    Boto btInici { "Inici", dibuix::Icona::Casa, Boto::Estil::Subtil };
    Boto btEstudi { "Estudi", std::nullopt, Boto::Estil::Subtil };
    Boto btDiag { "Diagnòstic"_ca, dibuix::Icona::Ajustos, Boto::Estil::Subtil };

    Boto btPrincipi { {}, dibuix::Icona::Principi, Boto::Estil::Tecla };
    Boto btRebobina { {}, dibuix::Icona::Rebobina, Boto::Estil::Tecla };
    Boto btEndavant { {}, dibuix::Icona::Endavant, Boto::Estil::Tecla };
    Boto btAtura { {}, dibuix::Icona::Atura, Boto::Estil::Tecla };
    Boto btPlay { {}, dibuix::Icona::Play, Boto::Estil::Tecla };
    Boto btGrava { {}, dibuix::Icona::Grava, Boto::Estil::Tecla };

    juce::Rectangle<int> zonaEstat, zonaTransport, zonaLcd;
    juce::String resum, extra;
    std::pair<juce::String, juce::String> posicio;
    int comptador = 0;
};
