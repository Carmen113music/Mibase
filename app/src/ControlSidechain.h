#pragma once

#include "Controls.h"
#include "Text.h"
#include <string>

/** 0.57: el que ha de saber la finestra d'un efecte per mostrar i canviar el seu sidechain (el posa l'estudi). */
class ServeiSidechain
{
public:
    virtual ~ServeiSidechain() = default;
    /** Alguna font encesa? */
    virtual bool actiu (const std::string& idInsert) = 0;
    /** «Veu», «Veu + Bateria» o buit si no n'hi ha cap. */
    virtual juce::String nomFonts (const std::string& idInsert) = 0;
    /** Encén o apaga totes les fonts; si no n'hi ha cap, obre el menú per triar-ne. */
    virtual void commuta (const std::string& idInsert, juce::Component* on) = 0;
    /** Menú de pistes (marcar / desmarcar) i opcions de nivell i punt de captació. */
    virtual void menu (const std::string& idInsert, juce::Component* on) = 0;
};

/** 0.57: botó «SC» (encén/apaga) i botó amb la pista d'on escolta, per posar dins la finestra d'un efecte. */
class ControlSidechain : public juce::Component, private juce::Timer
{
public:
    ControlSidechain (ServeiSidechain* s, std::string id) : servei (s), idInsert (std::move (id))
    {
        addAndMakeVisible (btSc);
        addAndMakeVisible (btFont);
        btSc.setLlum (tema::accent);
        btSc.setTooltip ("Sidechain: el so d'una altra pista controla aquest efecte (sense barrejar-s'hi). Encén o apaga."_ca);
        btFont.setTooltip ("D'on escolta: tria la pista (o pistes) i, a «Opcions», el nivell i d'on es capta."_ca);
        btSc.onClick = [this] { if (servei != nullptr) servei->commuta (idInsert, &btFont); refresca(); };
        btFont.onClick = [this] { if (servei != nullptr) servei->menu (idInsert, &btFont); };
        setVisible (servei != nullptr && ! idInsert.empty());
        refresca();
        startTimerHz (4);
    }
    ~ControlSidechain() override { stopTimer(); }

    void resized() override
    {
        auto r = getLocalBounds();
        btSc.setBounds (r.removeFromLeft (46));
        r.removeFromLeft (4);
        btFont.setBounds (r);
    }

private:
    void timerCallback() override { refresca(); }
    void refresca()
    {
        if (servei == nullptr) return;
        const bool on = servei->actiu (idInsert);
        btSc.setToggleState (on, juce::dontSendNotification);
        const auto n = servei->nomFonts (idInsert);
        const juce::String t = n.isEmpty() ? "Tria la pista…"_ca : n;
        if (btFont.getButtonText() != t) btFont.setButtonText (t);
    }
    ServeiSidechain* servei;
    std::string idInsert;
    Boto btSc { "SC", std::nullopt, Boto::Estil::Tecla };
    Boto btFont { {}, dibuix::Icona::FletxaAvall, Boto::Estil::Secundari };
};
