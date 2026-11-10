#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Dibuix.h"
#include "Tema.h"
#include "Text.h"

/** 0.28: finestra de càrrega (la mateixa a tot arreu): què s'està fent i el percentatge. «gran» = la de l'arrencada,
    amb el logo i el nom del programa. 0.28.1: amb «sobre», tapa tota la finestra principal (enfosquida, i no deixa
    clicar res) i la targeta va al mig; la feina llarga es fa en segon pla perquè la finestra es pugui anar pintant. */
class FinestraProgres : public juce::Component
{
public:
    explicit FinestraProgres (const juce::String& titolFinestra, bool granDarrencada = false, juce::Component* sobre = nullptr, bool escriptori = true)
        : titol (titolFinestra), gran (granDarrencada)
    {
        setOpaque (false);
        setSize (gran ? 460 : 420, gran ? 250 : 128);
        if (! escriptori) return;   // dins d'una altra finestra (la d'arrencada)
        if (sobre != nullptr)
        {
            // a sobre de tot, dins de la finestra principal (així es veu segur a qualsevol sistema)
            auto* arrel = sobre->getTopLevelComponent();
            cortina = true;
            arrel->addAndMakeVisible (this);
            setBounds (arrel->getLocalBounds());
            toFront (false);
            pintaAra();
            return;
        }
        addToDesktop (juce::ComponentPeer::windowIsTemporary | juce::ComponentPeer::windowIgnoresKeyPresses);
        setAlwaysOnTop (true);
        juce::Rectangle<int> zona;
        if (sobre != nullptr && sobre->getTopLevelComponent()->isShowing()) zona = sobre->getTopLevelComponent()->getScreenBounds();
        else if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()) zona = d->userBounds.toNearestInt();
        setBounds (getBounds().withCentre (zona.getCentre()));
        setVisible (true);
        toFront (false);
        pintaAra();
    }

    /** text: què fa ara. fraccio: de 0 a 1 (< 0: no se sap quant falta). */
    void posa (const juce::String& text, double fraccio)
    {
        // no cal tornar a pintar més de 25 vegades per segon
        const auto ara = juce::Time::getMillisecondCounter();
        const bool canvi = text != quin || std::abs (fraccio - fet) >= 0.01;
        quin = text;
        fet = fraccio;
        if (canvi) repaint();   // sempre (si el fil de la finestra està lliure es pinta igualment)
        if (canvi && ara - darrer >= 40) { darrer = ara; pintaAra(); }
    }

    void paint (juce::Graphics& g) override
    {
        if (cortina) g.fillAll (juce::Colours::black.withAlpha (0.45f));
        auto r = (cortina ? juce::Rectangle<int> (0, 0, 420, 128).withCentre (getLocalBounds().getCentre()) : getLocalBounds()).toFloat().reduced (2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 12.0f);
        g.setColour (tema::superficie);
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (tema::linia2);
        g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
        auto c = r.reduced (22.0f, 16.0f);
        if (gran)
        {
            auto dalt = c.removeFromTop (110.0f);
            dibuix::pintaLogo (g, dalt.removeFromLeft (64.0f).withSizeKeepingCentre (64.0f, 64.0f));
            dalt.removeFromLeft (18.0f);
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (34.0f, true));
            g.drawText ("Mibase", dalt.removeFromTop (60.0f), juce::Justification::bottomLeft);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (13.0f));
            g.drawText ("Versió "_ca + MIBASE_VERSIO + juce::String::fromUTF8 ("  ·  ") + titol, dalt.removeFromTop (24.0f), juce::Justification::centredLeft);
            c.removeFromTop (14.0f);
        }
        else
        {
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (15.0f, true));
            g.drawText (titol, c.removeFromTop (24.0f), juce::Justification::centredLeft, true);
            c.removeFromTop (8.0f);
        }
        // què fa
        auto linia = c.removeFromTop (20.0f);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (12.5f));
        if (fet >= 0.0)
        {
            g.setColour (tema::text);
            g.setFont (dibuix::mono (12.5f));
            g.drawText (juce::String ((int) std::lround (juce::jlimit (0.0, 1.0, fet) * 100.0)) + " %", linia.removeFromRight (52.0f), juce::Justification::centredRight);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (12.5f));
        }
        g.drawText (quin, linia, juce::Justification::centredLeft, true);
        c.removeFromTop (8.0f);
        // barra
        auto barra = c.removeFromTop (10.0f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (barra, 5.0f);
        if (fet >= 0.0)
        {
            g.setColour (tema::accent);
            g.fillRoundedRectangle (barra.withWidth (juce::jmax (10.0f, barra.getWidth() * (float) juce::jlimit (0.0, 1.0, fet))), 5.0f);
        }
        else
        {
            const float w = barra.getWidth() * 0.25f;
            const float x = barra.getX() + std::fmod ((float) juce::Time::getMillisecondCounter() * 0.25f, barra.getWidth() + w) - w;
            g.setColour (tema::accent);
            g.fillRoundedRectangle (juce::Rectangle<float> (x, barra.getY(), w, barra.getHeight()).getIntersection (barra), 5.0f);
        }
    }

    void parentSizeChanged() override { if (cortina && getParentComponent() != nullptr) setBounds (getParentComponent()->getLocalBounds()); }

private:
    void pintaAra()
    {
        repaint();
        if (auto* p = getPeer()) p->performAnyPendingRepaintsNow();
    }

    juce::String titol, quin;
    bool gran = false, cortina = false;
    double fet = 0.0;
    juce::uint32 darrer = 0;
};

/** 0.28: el progrés d'una feina llarga (el posa qui obre la finestra; qui fa la feina el crida si n'hi ha). */
inline std::function<void (const juce::String&, double)>& progresGlobal()
{
    static std::function<void (const juce::String&, double)> f;
    return f;
}

/** 0.28: el contingut de la finestra principal mentre arrenca: fons de l'estudi i, al mig, el logo, el nom i el progrés. */
class PantallaArrencada : public juce::Component
{
public:
    PantallaArrencada() { addAndMakeVisible (targeta); setSize (1360, 860); }
    void posa (const juce::String& text, double fraccio) { targeta.posa (text, fraccio); }
    void paint (juce::Graphics& g) override { g.fillAll (tema::fons); }
    void resized() override { targeta.setBounds (targeta.getBounds().withCentre (getLocalBounds().getCentre())); }
private:
    FinestraProgres targeta { "Estudi personal", true, nullptr, false };
};
