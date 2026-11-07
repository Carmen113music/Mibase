#pragma once

#include "Controls.h"
#include "ServeiAudio.h"

/** Barra de dalt, a totes les pantalles: logotip i versió, Inici, el projecte obert, el menú Fitxer (amb les
    Preferències: targeta, MIDI i latència) i, fix a la dreta, el transport (com un radiocasset) amb el bucle i la posició. */
class Capcalera : public juce::Component, private juce::Timer
{
public:
    enum class Pantalla { Inici, Estudi, Diagnostic };
    enum class Transport { Principi, Rebobina, Endavant, Atura, Play, Grava, Bucle };

    explicit Capcalera (ServeiAudio& s) : servei (s)
    {
        for (auto* b : { &btFitxer, &btInici, &btEstudi })
            addAndMakeVisible (b);
        btFitxer.onClick = [this] { if (onFitxer) onFitxer (btFitxer); };
        btInici.onClick = [this] { if (onPantalla) onPantalla (Pantalla::Inici); };
        btEstudi.onClick = [this] { if (onPantalla) onPantalla (Pantalla::Estudi); };
        btEstudi.setTooltip ("Torna a l'estudi del projecte obert"_ca);

        struct T { Boto* b; Transport t; const char* ajuda; };
        const T tecles[] = {
            { &btPrincipi, Transport::Principi, "Al principi (Inici)" },
            { &btRebobina, Transport::Rebobina, "Enrere un compàs" },
            { &btEndavant, Transport::Endavant, "Endavant un compàs" },
            { &btAtura,    Transport::Atura,    "Atura i torna on havies començat (si ja està aturat: al principi)" },
            { &btPlay,     Transport::Play,     "Reprodueix (Espai)" },
            { &btGrava,    Transport::Grava,    "Grava a la pista amb el botó vermell encès, amb un compàs de compte (R)" },
            { &btBucle,    Transport::Bucle,    "Bucle: encès o apagat (L). Si no n'hi ha cap, en marca un de 4 compassos al capçal; es mou a la regla." },
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
        btBucle.setLlum (vintage::ledGroc);

        setProjecte ({});
        actualitza();
        startTimerHz (30);
    }

    std::function<void (Pantalla)> onPantalla;
    std::function<void (juce::Component&)> onFitxer;
    std::function<void (Transport)> onTransport;
    /** Posició per a la pantalla: {gran, petit}. Buit: cap projecte obert. */
    std::function<std::pair<juce::String, juce::String>()> onPosicio;
    /** 0 aturat, 1 sonant, 2 comptant, 3 gravant. */
    std::function<int()> onEstatTransport;
    /** El bucle és encès (llum del botó). */
    std::function<bool()> onBucleActiu;

    void setPantalla (Pantalla p)
    {
        pantalla = p;
        posaTransport();
        btInici.setToggleState (p == Pantalla::Inici, juce::dontSendNotification);
        btEstudi.setToggleState (p == Pantalla::Estudi, juce::dontSendNotification);
        repaint();
    }

    /** Nom del projecte obert (buit: cap). */
    void setProjecte (const juce::String& nom)
    {
        btEstudi.setButtonText (nom.isEmpty() ? juce::String ("Estudi") : nom);
        btEstudi.setVisible (nom.isNotEmpty());
        hiHaProjecte = nom.isNotEmpty();
        posaTransport();
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

        // Marc del transport i pantalla de posició (només dins d'un projecte).
        if (! transportVisible) return;
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

        // Dreta, fix: transport i pantalla (mai es mou ni queda tapat). Només dins d'un projecte.
        if (! transportVisible)
            zonaLcd = zonaTransport = {};
        else
        {
        zonaLcd = r.removeFromRight (156);
        r.removeFromRight (8);
        const int tecla = 42;
        zonaTransport = r.removeFromRight (tecla * 7 + 6 * 3).withTrimmedTop (1).withTrimmedBottom (1);
        {
            auto t = zonaTransport;
            for (auto* b : { &btPrincipi, &btRebobina, &btEndavant, &btAtura, &btPlay, &btGrava, &btBucle })
            {
                b->setBounds (t.removeFromLeft (tecla));
                t.removeFromLeft (3);
            }
        }
        zonaLcd = zonaLcd.withY (zonaTransport.getY() - 2).withHeight (zonaTransport.getHeight() + 4);
        r.removeFromRight (18);
        }

        // Esquerra (0.11): Inici, el projecte obert i el menú Fitxer.
        btInici.setBounds (r.removeFromLeft (btInici.ampladaIdeal()));
        r.removeFromLeft (4);
        if (btEstudi.isVisible())
        {
            btEstudi.setBounds (r.removeFromLeft (juce::jmin (260, btEstudi.ampladaIdeal(), juce::jmax (90, r.getWidth() - 110))));
            r.removeFromLeft (4);
        }
        btFitxer.setBounds (r.removeFromLeft (btFitxer.ampladaIdeal()));
    }

private:
    void timerCallback() override { actualitza(); }

    void posaTransport()
    {
        transportVisible = hiHaProjecte && pantalla != Pantalla::Inici;
        for (auto* b : { &btPrincipi, &btRebobina, &btEndavant, &btAtura, &btPlay, &btGrava })
            b->setVisible (transportVisible);
        btBucle.setVisible (transportVisible);
        resized();
        repaint();
    }

    void actualitza()
    {
        ++comptador;
        // Llums del play i del gravar (durant el compte d'entrada, el gravar fa pampallugues).
        const int t = onEstatTransport ? onEstatTransport() : 0;
        btPlay.setToggleState (t == 1, juce::dontSendNotification);
        btGrava.setToggleState (t == 3 || (t == 2 && (comptador / 4) % 2 == 0), juce::dontSendNotification);
        btBucle.setToggleState (onBucleActiu && onBucleActiu(), juce::dontSendNotification);

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

    Boto btPrincipi { {}, dibuix::Icona::Principi, Boto::Estil::Tecla };
    Boto btRebobina { {}, dibuix::Icona::Rebobina, Boto::Estil::Tecla };
    Boto btEndavant { {}, dibuix::Icona::Endavant, Boto::Estil::Tecla };
    Boto btAtura { {}, dibuix::Icona::Atura, Boto::Estil::Tecla };
    Boto btPlay { {}, dibuix::Icona::Play, Boto::Estil::Tecla };
    Boto btGrava { {}, dibuix::Icona::Grava, Boto::Estil::Tecla };
    Boto btBucle { {}, dibuix::Icona::Bucle, Boto::Estil::Tecla };

    juce::Rectangle<int> zonaTransport, zonaLcd;
    std::pair<juce::String, juce::String> posicio;
    int comptador = 0;
    Pantalla pantalla = Pantalla::Inici;
    bool hiHaProjecte = false, transportVisible = false;
};
