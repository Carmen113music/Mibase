#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include "Controls.h"
#include "EstatEstudi.h"
#include "Plugins.h"
#include <mibase/Midi.h>

/** Opcions de la pista triada (o del màster), a la columna esquerra sobre el fader (0.16).
    Pista d'instrument (MIDI): instrument (sinte intern o VST3), so i teclat MIDI, i els inserts.
    Pista d'àudio: els inserts i la sortida. Màster (0.17): els inserts (cadena de màstering, 0.19) i la sortida. */
class OpcionsPista : public juce::Component
{
public:
    static constexpr int maxInserts = 8;

    explicit OpcionsPista (EstatEstudi& e) : estat (e)
    {
        for (auto* b : { &btInstrument, &btSo, &btTeclat, &btSortida })
        {
            b->setMidaText (12.0f);
            addChildComponent (b);
        }
        for (int k = 0; k < maxInserts; ++k)
        {
            auto& b = *inserts.add (new Boto ({}, std::nullopt, Boto::Estil::Discontinu));
            b.setMidaText (11.5f);
            b.onClick = [this, k] { menuInsert (k); };
            addChildComponent (b);
        }
        btInstrument.onClick = [this] { menuInstrument(); };
        btSo.onClick = [this] { menuSo(); };
        btTeclat.onClick = [this] { menuTeclat(); };
        btSortida.setTooltip ("On va el so"_ca);
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
    /** Els plugins del projecte obert (0.19). */
    GestorPlugins* plugins = nullptr;

    /** Alçada que necessita per a la pista triada. */
    int alcada() const
    {
        const int slots = numSlots();
        if (estat.masterTriat()) return 16 + slots * 26 + 28 + 8;
        const auto* t = estat.pista (estat.pistaTriada);
        if (t == nullptr) return 0;
        if (t->tipus == "midi") return (GestorPlugins::esVst3 (t->instrument) ? 2 : 3) * 32 + 16 + slots * 26 + 6;
        return 16 + slots * 26 + 28 + 10;
    }

    void actualitza()
    {
        const auto* t = estat.pista (estat.pistaTriada);
        const bool master = estat.masterTriat();
        const bool midiPista = ! master && t != nullptr && t->tipus == "midi";
        const bool audio = ! master && t != nullptr && ! midiPista;
        const bool vst = midiPista && GestorPlugins::esVst3 (t->instrument);
        btInstrument.setVisible (midiPista);
        btTeclat.setVisible (midiPista);
        btSo.setVisible (midiPista && ! vst);
        btSortida.setVisible (audio || master);

        const auto* v = llistaInserts();
        const int slots = numSlots();
        for (int k = 0; k < maxInserts; ++k)
        {
            auto& b = *inserts[k];
            b.setVisible (v != nullptr && k < slots);
            if (v == nullptr || k >= slots) continue;
            if (k < (int) v->size())
            {
                const auto& x = (*v)[(size_t) k];
                b.setButtonText (juce::String::fromUTF8 (x.nom.c_str()) + (x.actiu ? juce::String() : " (apagat)"));
                b.setEstil (Boto::Estil::Secundari);
                b.setTooltip ("Clica: obrir, apagar, canviar o treure"_ca);
            }
            else
            {
                b.setButtonText (k == (int) v->size() ? "+ Efecte"_ca : "Insert " + juce::String (k + 1) + " · buit"_ca);
                b.setEstil (Boto::Estil::Discontinu);
                b.setTooltip ("Clica per posar-hi un efecte VST3"_ca);
            }
            b.setEnabled (k <= (int) v->size());
        }

        if (midiPista)
        {
            btInstrument.setButtonText ("Instrument: "_ca + (vst ? juce::String::fromUTF8 (t->nomInstrument.c_str()) : "sinte intern"_ca));
            btSo.setButtonText ("So: "_ca + juce::String::fromUTF8 (mibase::midi::nomInstrument (t->instrument.empty() || vst ? "intern:piano" : t->instrument).c_str()));
            btTeclat.setButtonText ("Teclat: "_ca + (t->entradaMidi.empty() ? "qualsevol"_ca : juce::String::fromUTF8 (t->entradaMidiNom.c_str())));
        }
        if (master)
        {
            const auto noms = sortides ? sortides() : juce::StringArray();
            const int k = 2 * estat.projecte.sortidaMaster;
            btSortida.setButtonText ("Sortida: "_ca + (k + 1 < noms.size() ? noms[k] + " / " + noms[k + 1]
                                                                          : juce::String (k + 1) + "/" + juce::String (k + 2)));
        }
        else if (audio) btSortida.setButtonText ("Sortida: Màster (L+R)"_ca);
        resized();
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto fila = [&r] (int h) { auto f = r.removeFromTop (h); r.removeFromTop (4); return f; };
        if (btInstrument.isVisible())
        {
            btInstrument.setBounds (fila (28));
            if (btSo.isVisible()) btSo.setBounds (fila (28));
            btTeclat.setBounds (fila (28));
        }
        yTitol = r.getY();
        r.removeFromTop (16);   // títol «Inserts»
        for (auto* b : inserts)
            if (b->isVisible()) b->setBounds (fila (22));
        if (btSortida.isVisible())
            btSortida.setBounds (fila (28));
    }

    void paint (juce::Graphics& g) override
    {
        if (llistaInserts() == nullptr) return;
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (10.5f, true));
        g.drawText (estat.masterTriat() ? "INSERTS · MÀSTERING"_ca : "INSERTS"_ca, 2, yTitol, getWidth(), 14, juce::Justification::centredLeft);
    }

private:
    /** Els inserts del que hi ha triat (pista o màster), o nullptr. */
    std::vector<mibase::Insert>* llistaInserts() const
    {
        if (estat.masterTriat()) return &estat.projecte.insertsMaster;
        auto* t = estat.pista (estat.pistaTriada);
        return t != nullptr ? &t->inserts : nullptr;
    }

    int numSlots() const
    {
        const auto* v = llistaInserts();
        return v == nullptr ? 0 : juce::jlimit (4, maxInserts, (int) v->size() + 1);
    }

    void canvia (std::function<void (mibase::Pista&)> f, const juce::String& evolucio)
    {
        auto* t = estat.pista (estat.pistaTriada);
        if (t == nullptr) return;
        estat.desfes.abans (estat.projecte);
        f (*estat.pista (estat.pistaTriada));
        estat.canviat (evolucio);
    }

    void canviaInserts (std::function<void (std::vector<mibase::Insert>&)> f, const juce::String& evolucio)
    {
        if (llistaInserts() == nullptr) return;
        if (plugins != nullptr) plugins->desaEstats (estat.projecte);   // que el desfer recordi els ajustos d'ara
        estat.desfes.abans (estat.projecte);
        f (*llistaInserts());
        estat.canviat (evolucio);
    }

    static juce::String titol (const mibase::Insert& x) { return juce::String::fromUTF8 (x.nom.c_str()); }

    void menuInsert (int k)
    {
        auto* v = llistaInserts();
        if (v == nullptr || plugins == nullptr) return;
        auto* boto = inserts[k];
        auto tipus = std::make_shared<juce::Array<juce::PluginDescription>>();
        auto efectes = plugins->menu (false, 1000, *tipus);

        auto posa = [this, k, tipus] (int r, bool nou)
        {
            if (r < 1000 || r - 1000 >= tipus->size()) return;
            const auto d = (*tipus)[r - 1000];
            std::string id;
            canviaInserts ([&] (std::vector<mibase::Insert>& l)
            {
                mibase::Insert x;
                x.id = id = "i" + std::to_string (++estat.projecte.comptador);
                x.plugin = d.createIdentifierString().toStdString();
                x.nom = d.name.toStdString();
                if (nou || k >= (int) l.size()) l.push_back (x);
                else l[(size_t) k] = x;
            }, "Efecte: "_ca + d.name);
            juce::MessageManager::callAsync ([this, id, nom = d.name] { if (plugins != nullptr) plugins->obreFinestra (id, nom); });
        };

        if (k >= (int) v->size())
        {
            efectes.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (boto), [posa] (int r) { posa (r, true); });
            return;
        }
        const auto x = (*v)[(size_t) k];
        juce::PopupMenu m;
        m.addItem (1, "Obre la finestra del plugin");
        m.addItem (2, x.actiu ? "Apaga'l (el so hi passa de llarg)"_ca : "Encén-lo"_ca);
        m.addSubMenu ("Canvia'l per", efectes);
        m.addSeparator();
        m.addItem (3, "Mou amunt", k > 0);
        m.addItem (4, "Mou avall", k + 1 < (int) v->size());
        m.addSeparator();
        m.addItem (5, "Treu-lo");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (boto), [this, k, x, posa] (int r)
        {
            if (r == 1) { if (plugins != nullptr) plugins->obreFinestra (x.id, titol (x)); }
            else if (r == 2) canviaInserts ([k] (std::vector<mibase::Insert>& l) { if (k < (int) l.size()) l[(size_t) k].actiu = ! l[(size_t) k].actiu; }, {});
            else if (r == 3) canviaInserts ([k] (std::vector<mibase::Insert>& l) { if (k > 0 && k < (int) l.size()) std::swap (l[(size_t) k], l[(size_t) k - 1]); }, {});
            else if (r == 4) canviaInserts ([k] (std::vector<mibase::Insert>& l) { if (k + 1 < (int) l.size()) std::swap (l[(size_t) k], l[(size_t) k + 1]); }, {});
            else if (r == 5) canviaInserts ([k] (std::vector<mibase::Insert>& l) { if (k < (int) l.size()) l.erase (l.begin() + k); }, "Treu efecte: "_ca + titol (x));
            else posa (r, false);
        });
    }

    void menuInstrument()
    {
        auto* t = estat.pista (estat.pistaTriada);
        if (t == nullptr) return;
        const bool vst = GestorPlugins::esVst3 (t->instrument);
        auto tipus = std::make_shared<juce::Array<juce::PluginDescription>>();
        juce::PopupMenu m;
        if (vst) { m.addItem (2, "Obre la finestra del plugin"); m.addSeparator(); }
        m.addItem (1, "Sinte intern", true, ! vst);
        if (plugins != nullptr) m.addSubMenu ("Instruments VST3", plugins->menu (true, 1000, *tipus));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btInstrument), [this, tipus] (int r)
        {
            auto* p = estat.pista (estat.pistaTriada);
            if (p == nullptr) return;
            const auto id = p->id;
            if (r == 2 && plugins != nullptr) plugins->obreFinestra ("inst:" + id, juce::String::fromUTF8 (p->nomInstrument.c_str()));
            else if (r == 1 && GestorPlugins::esVst3 (p->instrument))
                canvia ([] (mibase::Pista& q) { q.instrument = "intern:piano"; q.nomInstrument.clear(); q.estatInstrument.clear(); }, "Instrument: sinte intern"_ca);
            else if (r >= 1000 && r - 1000 < tipus->size())
            {
                const auto d = (*tipus)[r - 1000];
                canvia ([d] (mibase::Pista& q)
                {
                    q.instrument = "vst3:" + d.createIdentifierString().toStdString();
                    q.nomInstrument = d.name.toStdString();
                    q.estatInstrument.clear();
                }, "Instrument: "_ca + d.name);
                juce::MessageManager::callAsync ([this, id, nom = d.name] { if (plugins != nullptr) plugins->obreFinestra ("inst:" + id, nom); });
            }
        });
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
    juce::OwnedArray<Boto> inserts;
    int yTitol = 0;
};
