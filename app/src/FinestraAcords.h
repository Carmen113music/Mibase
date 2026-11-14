#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <mibase/Harmonia.h>
#include <mibase/PresetsPads.h>
#include "Controls.h"
#include "EditorAcord.h"
#include "EstatEstudi.h"

/** 0.37: la finestra del llapis de la pista d'Acords: a dalt, un mini Chord Pad (12 pads d'un preset o de la tonalitat);
    a sota, el constructor d'acords. Clicar un pad el posa al constructor (i sona); «Posa-ho» el deixa a la pista.
    El preset triat es recorda a la pista d'Acords (com els pads d'una pista MIDI). */
class FinestraAcords : public juce::Component
{
public:
    FinestraAcords (EstatEstudi& e, int pistaAcords, const std::string& acordInicial,
                    std::function<void (const std::string&)> posa, std::function<void (const std::vector<int>&)> escolta)
        : estat (e), pista (pistaAcords), onPosa (std::move (posa)), onEscolta (std::move (escolta)),
          editor (mibase::harmonia::llegeixAcord (acordInicial), {}, [this] (const mibase::harmonia::AcordPad& a) { sona (a); })
    {
        editor.configura ("Posa-ho"_ca, [this] (const mibase::harmonia::AcordPad& a) { if (! a.buit() && onPosa) onPosa (mibase::harmonia::textPad (a)); }, true);
        addAndMakeVisible (editor);
        btPreset.setMidaText (12.0f);
        btPreset.onClick = [this] { menuPresets(); };
        btPreset.setTooltip ("Quins acords surten als pads: els de la tonalitat o els d'una cançó"_ca);
        addAndMakeVisible (btPreset);
        for (int i = 0; i < mibase::pads::numPads; ++i)
        {
            auto* b = pads.add (new Boto ({}, std::nullopt, Boto::Estil::Secundari));
            b->setMidaText (14.0f);
            b->onClick = [this, i] { triaPad (i); };
            addAndMakeVisible (b);
        }
        refrescaPads();
        setSize (536, 404 + 132);
    }

    void paint (juce::Graphics& g) override
    {
        vintage::panell (g, getLocalBounds().toFloat(), 8.0f);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.5f, true));
        g.drawText ("CHORD PAD", 16, 10, 120, 18, juce::Justification::centredLeft);
        g.setColour (tema::linia);
        g.fillRect (8, 128, getWidth() - 16, 1);
    }

    void resized() override
    {
        btPreset.setBounds (110, 8, getWidth() - 110 - 16, 24);
        // 12 pads com les tecles d'una octava: negres a dalt, blanques a baix
        const int x0 = 16, amp = (getWidth() - 32) / 7, alt = 40;
        int blanca = 0;
        for (int i = 0; i < mibase::pads::numPads; ++i)
        {
            if (mibase::pads::esNegre (i))
                pads[i]->setBounds (x0 + blanca * amp - amp / 2 + 3, 36, amp - 6, alt - 4);
            else
                pads[i]->setBounds (x0 + (blanca++) * amp + 2, 36 + alt, amp - 4, alt);
        }
        editor.setBounds (8, 132, getWidth() - 16, getHeight() - 136);
    }

private:
    mibase::Pista* pistaAcords() { return pista >= 0 && pista < estat.numPistes() ? &estat.projecte.pistes[(size_t) pista] : nullptr; }

    std::vector<std::string> padsActuals()
    {
        if (auto* t = pistaAcords(); t != nullptr && (int) t->pads.size() == mibase::pads::numPads) return t->pads;
        return mibase::pads::padsDeTonalitat (mibase::harmonia::tonalitatEfectiva (estat.projecte));
    }

    void refrescaPads()
    {
        const auto v = padsActuals();
        for (int i = 0; i < pads.size(); ++i)
        {
            const auto nom = i < (int) v.size() ? juce::String::fromUTF8 (v[(size_t) i].c_str()) : juce::String();
            pads[i]->setButtonText (nom);
            pads[i]->setEnabled (nom.isNotEmpty());
        }
        auto* t = pistaAcords();
        btPreset.setButtonText (t == nullptr || t->presetPads.empty() || t->pads.empty()
                                    ? "Acords de la tonalitat ("_ca + juce::String::fromUTF8 (mibase::harmonia::nomTonalitat (mibase::harmonia::tonalitatEfectiva (estat.projecte)).c_str()) + ")"
                                    : juce::String::fromUTF8 (t->presetPads.c_str()));
    }

    void triaPad (int i)
    {
        const auto v = padsActuals();
        if (i >= (int) v.size() || v[(size_t) i].empty()) return;
        const auto a = mibase::harmonia::llegeixAcord (v[(size_t) i]);
        editor.posaAcord (a);
        sona (a);
    }

    void sona (const mibase::harmonia::AcordPad& a)
    {
        if (onEscolta && ! a.buit()) onEscolta (mibase::harmonia::notesPad (a, 4, mibase::harmonia::Voicing::Tancat));
    }

    void posaPads (std::vector<std::string> v, const std::string& nom)
    {
        if (auto* t = pistaAcords())
        {
            if (! v.empty()) v.resize (mibase::pads::numPads);
            t->pads = v;   // buit = els de la tonalitat
            t->presetPads = nom;
            estat.canviat();
        }
        refrescaPads();
    }

    void menuPresets()
    {
        namespace hm = mibase::harmonia;
        juce::PopupMenu m, pop, funk, tonalitats;
        m.addItem (1, "Acords de la tonalitat ("_ca + juce::String::fromUTF8 (hm::nomTonalitat (hm::tonalitatEfectiva (estat.projecte)).c_str()) + ")");
        m.addSeparator();
        const auto& v = mibase::pads::presets();
        auto* t = pistaAcords();
        for (int i = 0; i < (int) v.size(); ++i)
        {
            const auto& p = v[(size_t) i];
            juce::String acords;
            for (int k = 0; k < 4; ++k) acords += (k > 0 ? " " : "") + juce::String (p.acords[(size_t) k]);
            const auto text = juce::String::fromUTF8 (p.titol) + " - " + juce::String::fromUTF8 (p.artista) + "      " + acords + " …";
            (juce::String::fromUTF8 (p.grup) == "Funk" ? funk : pop).addItem (100 + i, text, true, t != nullptr && t->presetPads == p.titol);
        }
        for (int to = 0; to < 12; ++to)
        {
            juce::PopupMenu esc;
            for (int e = 0; e < (int) hm::escales().size() && e < 20; ++e)
                esc.addItem (1000 + to * 20 + e, juce::String::fromUTF8 (hm::nomTonalitat ({ to, e }).c_str()));
            tonalitats.addSubMenu (juce::String (hm::nomTo (to)), esc);
        }
        m.addSubMenu ("Tonalitat"_ca, tonalitats);
        m.addSubMenu ("Pop, rock i indie", pop);
        m.addSubMenu ("Funk", funk);
        juce::Component::SafePointer<FinestraAcords> jo (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btPreset), [jo] (int r)
        {
            if (jo == nullptr || r <= 0) return;
            if (r == 1) jo->posaPads ({}, {});
            else if (r >= 1000)
            {
                const mibase::Tonalitat ton { (r - 1000) / 20, (r - 1000) % 20 };
                jo->posaPads (mibase::pads::padsDeTonalitat (ton), mibase::harmonia::nomTonalitat (ton));
            }
            else if (r >= 100) jo->posaPads (mibase::pads::padsDePreset (r - 100), mibase::pads::presets()[(size_t) (r - 100)].titol);
        });
    }

    EstatEstudi& estat;
    int pista;
    std::function<void (const std::string&)> onPosa;
    std::function<void (const std::vector<int>&)> onEscolta;
    Boto btPreset { "", std::nullopt, Boto::Estil::Secundari };
    juce::OwnedArray<Boto> pads;
    EditorAcord editor;
};
