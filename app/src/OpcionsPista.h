#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include "Controls.h"
#include "EstatEstudi.h"
#include <mibase/Midi.h>

/** Opcions de la pista triada, a la columna esquerra sobre el fader (0.16).
    Pista d'instrument (MIDI): instrument, so i teclat MIDI (com la secció «Pista» del panell del Piano Roll).
    Pista d'àudio: llocs per als inserts (més endavant) i la sortida (enrutament: ara, la principal L+R). */
class OpcionsPista : public juce::Component
{
public:
    explicit OpcionsPista (EstatEstudi& e) : estat (e)
    {
        for (auto* b : { &btInstrument, &btSo, &btTeclat, &btSortida })
        {
            b->setMidaText (12.0f);
            addChildComponent (b);
        }
        for (auto& b : inserts)
        {
            b.setMidaText (11.5f);
            b.setEnabled (false);
            b.setTooltip ("Efectes de la pista (EQ, compressor, reverb…): més endavant"_ca);
            addChildComponent (b);
        }
        btInstrument.onClick = [this]
        {
            juce::PopupMenu m;
            m.addItem (1, "Sinte intern", true, true);
            m.addItem (2, "Plugins VST3  · més endavant"_ca, false);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btInstrument), [] (int) {});
        };
        btSo.onClick = [this] { menuSo(); };
        btTeclat.onClick = [this] { menuTeclat(); };
        btSortida.setTooltip ("On va el so de la pista"_ca);
        btSortida.onClick = [this]
        {
            juce::PopupMenu m;
            if (estat.masterTriat())
            {
                // 0.17: per quin parell de sortides de la targeta surt el màster.
                const auto noms = sortides ? sortides() : juce::StringArray();
                for (int k = 0; k + 1 < noms.size(); k += 2)
                    m.addItem (100 + k / 2, noms[k] + " / " + noms[k + 1], true, estat.projecte.sortidaMaster == k / 2);
                if (noms.size() < 2) m.addItem (1, "Sortides 1/2", true, true);
                m.addSeparator();
                m.addItem (2, "Activa més sortides a Fitxer > Preferències"_ca, false);
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btSortida), [this] (int r)
                {
                    if (r < 100) return;
                    estat.desfes.abans (estat.projecte);
                    estat.projecte.sortidaMaster = r - 100;
                    estat.canviat ("Sortida del màster"_ca);
                });
                return;
            }
            m.addItem (1, "Màster (L+R)"_ca, true, true);
            m.addItem (2, "Sortida directa  · més endavant"_ca, false);
            m.addItem (3, "Subgrup  · més endavant"_ca, false);
            m.addItem (4, "Bus d'efectes  · més endavant"_ca, false);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btSortida), [] (int) {});
        };
    }

    std::function<juce::Array<juce::MidiDeviceInfo>()> teclats;
    /** Noms de les sortides actives de la targeta (0.17). */
    std::function<juce::StringArray()> sortides;

    /** Alçada que necessita per a la pista triada. */
    int alcada() const
    {
        if (estat.masterTriat()) return 28 + 6;
        const auto* t = estat.pista (estat.pistaTriada);
        if (t == nullptr) return 0;
        return t->tipus == "midi" ? 3 * 32 + 6 : 4 * 26 + 32 + 10;
    }

    void actualitza()
    {
        const auto* t = estat.pista (estat.pistaTriada);
        const bool midiPista = t != nullptr && t->tipus == "midi";
        const bool audio = t != nullptr && ! midiPista;
        for (auto* b : { &btInstrument, &btSo, &btTeclat }) b->setVisible (midiPista);
        btSortida.setVisible (audio || estat.masterTriat());
        for (auto& b : inserts) b.setVisible (audio);
        if (estat.masterTriat())
        {
            const auto noms = sortides ? sortides() : juce::StringArray();
            const int k = 2 * estat.projecte.sortidaMaster;
            btSortida.setButtonText ("Sortida: "_ca + (k + 1 < noms.size() ? noms[k] + " / " + noms[k + 1]
                                                                          : juce::String (k + 1) + "/" + juce::String (k + 2)));
        }
        if (midiPista)
        {
            btInstrument.setButtonText ("Instrument: sinte intern"_ca);
            btSo.setButtonText ("So: "_ca + juce::String::fromUTF8 (mibase::midi::nomInstrument (t->instrument.empty() ? "intern:piano" : t->instrument).c_str()));
            btTeclat.setButtonText ("Teclat: "_ca + (t->entradaMidi.empty() ? "qualsevol"_ca : juce::String::fromUTF8 (t->entradaMidiNom.c_str())));
        }
        if (audio) btSortida.setButtonText ("Sortida: Màster (L+R)"_ca);
        resized();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto fila = [&r] (int h) { auto f = r.removeFromTop (h); r.removeFromTop (4); return f; };
        if (btInstrument.isVisible())
        {
            btInstrument.setBounds (fila (28));
            btSo.setBounds (fila (28));
            btTeclat.setBounds (fila (28));
        }
        if (btSortida.isVisible() && estat.masterTriat())
            btSortida.setBounds (fila (28));
        else if (btSortida.isVisible())
        {
            r.removeFromTop (16);   // títol «Inserts»
            for (auto& b : inserts) b.setBounds (fila (22));
            btSortida.setBounds (fila (28));
        }
    }

    void paint (juce::Graphics& g) override
    {
        if (! btSortida.isVisible() || estat.masterTriat()) return;
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (10.5f, true));
        g.drawText ("INSERTS", 2, 0, getWidth(), 14, juce::Justification::centredLeft);
    }

private:
    void canvia (std::function<void (mibase::Pista&)> f, const juce::String& evolucio)
    {
        auto* t = estat.pista (estat.pistaTriada);
        if (t == nullptr) return;
        estat.desfes.abans (estat.projecte);
        f (*estat.pista (estat.pistaTriada));
        estat.canviat (evolucio);
    }

    void menuSo()
    {
        auto* t = estat.pista (estat.pistaTriada);
        if (t == nullptr) return;
        juce::PopupMenu m;
        const int actual = juce::jmax (0, mibase::midi::soIntern (t->instrument.empty() ? "intern:piano" : t->instrument));
        for (int k = 0; k < (int) mibase::midi::sonsInterns().size(); ++k)
            m.addItem (k + 1, juce::String::fromUTF8 (mibase::midi::sonsInterns()[(size_t) k].nom), true, actual == k);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btSo), [this] (int r)
        {
            if (r <= 0) return;
            const std::string id = mibase::midi::sonsInterns()[(size_t) (r - 1)].id;
            canvia ([id] (mibase::Pista& p) { p.instrument = "intern:" + id; }, {});
        });
    }

    void menuTeclat()
    {
        auto* t = estat.pista (estat.pistaTriada);
        if (t == nullptr) return;
        const auto llista = teclats ? teclats() : juce::Array<juce::MidiDeviceInfo>();
        juce::PopupMenu m;
        m.addItem (1, "Qualsevol (el que toca la pista triada)", true, t->entradaMidi.empty());
        m.addSeparator();
        for (int i = 0; i < llista.size(); ++i)
            m.addItem (100 + i, llista[i].name, true, t->entradaMidi == llista[i].identifier.toStdString());
        if (llista.isEmpty()) m.addItem (3, "No hi ha cap teclat MIDI connectat", false);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btTeclat), [this, llista] (int r)
        {
            if (r == 1) canvia ([] (mibase::Pista& p) { p.entradaMidi.clear(); p.entradaMidiNom.clear(); }, {});
            else if (r >= 100 && r - 100 < llista.size())
            {
                const auto d = llista[r - 100];
                canvia ([d] (mibase::Pista& p) { p.entradaMidi = d.identifier.toStdString(); p.entradaMidiNom = d.name.toStdString(); }, {});
            }
        });
    }

    EstatEstudi& estat;
    Boto btInstrument { "Instrument", std::nullopt, Boto::Estil::Secundari };
    Boto btSo { "So", std::nullopt, Boto::Estil::Secundari };
    Boto btTeclat { "Teclat", std::nullopt, Boto::Estil::Secundari };
    Boto btSortida { "Sortida", std::nullopt, Boto::Estil::Secundari };
    std::array<Boto, 4> inserts { Boto { "Insert 1 · buit", std::nullopt, Boto::Estil::Discontinu }, Boto { "Insert 2 · buit", std::nullopt, Boto::Estil::Discontinu },
                                  Boto { "Insert 3 · buit", std::nullopt, Boto::Estil::Discontinu }, Boto { "Insert 4 · buit", std::nullopt, Boto::Estil::Discontinu } };
};
