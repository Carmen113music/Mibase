#pragma once

#include "Controls.h"
#include "EstatEstudi.h"
#include "Plugins.h"
#include <mibase/Harmonia.h>
#include <mibase/Midi.h>
#include "EscGlobal.h"

/** 0.20: el panell enganxat al costat de la finestra d'un instrument (VST3 o sinte intern): l'escala amb què toca el
    teclat MIDI (Visual / Assistit / Bloquejat), la tonalitat, i si aquesta pista la fa servir o no. */
class PanellEscala : public juce::Component, private juce::ChangeListener
{
public:
    PanellEscala (EstatEstudi& e, std::string idPista) : estat (e), id (std::move (idPista))
    {
        for (auto* c : std::initializer_list<juce::Component*> { &btAplica, &btTonalitat, &modes })
            addAndMakeVisible (c);
        btAplica.setClickingTogglesState (false);
        btAplica.onClick = [this]
        {
            auto* t = pista();
            if (t == nullptr) return;
            estat.desfes.abans (estat.projecte);
            t->escalaTeclat = ! t->escalaTeclat;
            estat.canviat();
        };
        btTonalitat.onClick = [this] { menuTonalitat(); };
        modes.onCanvi = [this] (int i)
        {
            estat.desfes.abans (estat.projecte);
            if (auto* t = pista()) t->modeEscala = i;   // 0.25: de la pista
            else estat.projecte.modeEscala = (mibase::ModeEscala) i;
            estat.canviat();
        };
        estat.addChangeListener (this);
        actualitza();
    }
    ~PanellEscala() override { estat.removeChangeListener (this); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff222227));
        g.setColour (juce::Colour (0xff3a3a43));
        g.fillRect (0, 0, 1, getHeight());
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (10.5f, true));
        g.drawText ("ESCALA DEL TECLAT"_ca, 14, 12, getWidth() - 20, 14, juce::Justification::centredLeft);
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (13.0f, true));
        g.drawText (textTonalitat, 14, yTonalitat, getWidth() - 24, 18, juce::Justification::centredLeft);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.5f));
        g.drawFittedText (ajuda, juce::Rectangle<int> (14, yAjuda, getWidth() - 26, getHeight() - yAjuda - 8), juce::Justification::topLeft, 12, 1.0f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12, 0).withTrimmedTop (34);
        btAplica.setBounds (r.removeFromTop (30));
        r.removeFromTop (12);
        yTonalitat = r.getY();
        r.removeFromTop (22);
        btTonalitat.setBounds (r.removeFromTop (26));
        r.removeFromTop (10);
        modes.setBounds (r.removeFromTop (28));
        yAjuda = r.getY() + 10;
    }

private:
    mibase::Pista* pista()
    {
        for (auto& t : estat.projecte.pistes) if (t.id == id) return &t;
        return nullptr;
    }
    void changeListenerCallback (juce::ChangeBroadcaster*) override { actualitza(); }
    void actualitza()
    {
        auto* t = pista();
        const bool aplica = t != nullptr && t->escalaTeclat;
        btAplica.setButtonText (aplica ? "Aquesta pista segueix l'escala"_ca : "Aquesta pista toca lliure"_ca);
        btAplica.setToggleState (aplica, juce::dontSendNotification);
        const int m = (int) (t != nullptr ? mibase::modeEscalaDe (estat.projecte, *t) : estat.projecte.modeEscala);
        modes.setTriat (m);
        modes.setEnabled (aplica);
        const auto ton = mibase::harmonia::tonalitatEfectiva (estat.projecte);
        textTonalitat = juce::String::fromUTF8 (mibase::harmonia::nomTonalitat (ton).c_str())
                        + (estat.projecte.tonalitatAuto ? " (automàtica)"_ca : juce::String());
        ajuda = ! aplica ? "El teclat MIDI toca totes les notes tal qual, encara que el projecte tingui l'escala en Assistit o Bloquejat."_ca
              : m == 0 ? "Visual: el teclat toca lliure; al Piano Roll només es ressalten les notes de l'escala."_ca
              : m == 1 ? "Assistit: una tecla fora de l'escala toca la nota de l'escala més propera (en el sentit que va la mà)."_ca
                       : "Bloquejat: les tecles fora de l'escala no sonen."_ca;
        ajuda += "\n\nEl mode i la tonalitat són del projecte: també valen per al Piano Roll."_ca;
        repaint();
    }
    void menuTonalitat()
    {
        namespace hm = mibase::harmonia;
        juce::PopupMenu m;
        m.addItem (1, "Automàtica (la dedueix de les notes)"_ca, true, estat.projecte.tonalitatAuto);
        m.addSeparator();
        const auto ara = hm::tonalitatEfectiva (estat.projecte);
        for (int to = 0; to < 12; ++to)
        {
            juce::PopupMenu sub;
            for (int e = 0; e < (int) hm::escales().size(); ++e)
                sub.addItem (1000 + to * 100 + e, juce::String::fromUTF8 (hm::escales()[(size_t) e].nom), true,
                             ! estat.projecte.tonalitatAuto && ara.to == to && ara.escala == e);
            m.addSubMenu (juce::String::fromUTF8 (hm::nomTo (to).c_str()), sub);
        }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btTonalitat), [this] (int r)
        {
            if (r <= 0) return;
            estat.desfes.abans (estat.projecte);
            if (r == 1) estat.projecte.tonalitatAuto = true;
            else
            {
                estat.projecte.tonalitatAuto = false;
                estat.projecte.tonalitat = { (r - 1000) / 100, (r - 1000) % 100 };
            }
            estat.canviat ("Tonalitat: "_ca + juce::String::fromUTF8 (hm::nomTonalitat (hm::tonalitatEfectiva (estat.projecte)).c_str()));
        });
    }

    EstatEstudi& estat;
    std::string id;
    Boto btAplica { "Segueix l'escala"_ca, std::nullopt, Boto::Estil::Tecla };
    Boto btTonalitat { "Canvia la tonalitat"_ca, std::nullopt, Boto::Estil::Secundari };
    Segmentat modes { { "Visual", "Assistit", "Bloquejat" } };
    juce::String textTonalitat, ajuda;
    int yTonalitat = 0, yAjuda = 0;
};

/** 0.20: el panell del sinte intern: els 8 sons, com tecles. */
class PanellSinte : public juce::Component, private juce::ChangeListener
{
public:
    PanellSinte (EstatEstudi& e, std::string idPista) : estat (e), id (std::move (idPista))
    {
        const auto& sons = mibase::midi::sonsInterns();
        for (int k = 0; k < (int) sons.size(); ++k)
        {
            auto* b = botons.add (new Boto (juce::String::fromUTF8 (sons[(size_t) k].nom), std::nullopt, Boto::Estil::Tecla));
            b->setMidaText (13.0f);
            b->onClick = [this, k]
            {
                auto* t = pista();
                if (t == nullptr) return;
                estat.desfes.abans (estat.projecte);
                t->instrument = std::string ("intern:") + mibase::midi::sonsInterns()[(size_t) k].id;
                estat.canviat();
            };
            addAndMakeVisible (b);
        }
        setSize (380, 330);
        estat.addChangeListener (this);
        actualitza();
    }
    ~PanellSinte() override { estat.removeChangeListener (this); }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (tema::fons);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (10.5f, true));
        g.drawText ("SINTE INTERN · SO"_ca, 16, 12, getWidth() - 20, 14, juce::Justification::centredLeft);
        g.setFont (dibuix::lletra (11.5f));
        g.drawFittedText ("Canviar de so no talla les notes que sonen."_ca, 16, getHeight() - 34, getWidth() - 32, 24, juce::Justification::centredLeft, 2);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (16, 0).withTrimmedTop (36).withTrimmedBottom (40);
        const int files = (botons.size() + 1) / 2, alt = juce::jmin (52, r.getHeight() / juce::jmax (1, files));
        for (int k = 0; k < botons.size(); ++k)
            botons[k]->setBounds (r.getX() + (k % 2) * (r.getWidth() / 2) + (k % 2) * 4, r.getY() + (k / 2) * alt,
                                  r.getWidth() / 2 - 4, alt - 6);
    }
private:
    mibase::Pista* pista()
    {
        for (auto& t : estat.projecte.pistes) if (t.id == id) return &t;
        return nullptr;
    }
    void changeListenerCallback (juce::ChangeBroadcaster*) override { actualitza(); }
    void actualitza()
    {
        auto* t = pista();
        const int ara = t == nullptr ? -1 : juce::jmax (0, mibase::midi::soIntern (t->instrument.empty() ? "intern:piano" : t->instrument));
        for (int k = 0; k < botons.size(); ++k) botons[k]->setToggleState (k == ara, juce::dontSendNotification);
    }
    EstatEstudi& estat;
    std::string id;
    juce::OwnedArray<Boto> botons;
};

/** La finestra del sinte intern d'una pista (amb l'escala al costat). */
class FinestraSinte : public juce::DocumentWindow
{
public:
    bool keyPressed (const juce::KeyPress& k) override { return k == juce::KeyPress::escapeKey && cridaEscGlobal(); }   // 0.27
    FinestraSinte (EstatEstudi& e, const std::string& id, const juce::String& titol, std::function<void()> enTancar)
        : juce::DocumentWindow (titol, tema::fons, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
          tancar (std::move (enTancar))
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new AmbLateral (new PanellSinte (e, id), std::make_unique<PanellEscala> (e, id)), true);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }
    void closeButtonPressed() override { juce::MessageManager::callAsync ([t = tancar] { t(); }); }
private:
    std::function<void()> tancar;
};
