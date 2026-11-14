#pragma once

#include "Kits.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include "Controls.h"
#include "EstatEstudi.h"
#include "Plugins.h"
#include "FinestraInstrument.h"
#include "Inserts.h"
#include <mibase/Midi.h>
#include "BancGM.h"
#include "Guitarres.h"

/** Opcions de la pista triada (o del màster), a la columna esquerra sobre el fader (0.16).
    Pista d'instrument (MIDI): instrument (sinte intern o VST3), so i teclat MIDI, i els inserts.
    Pista d'àudio: els inserts i la sortida. Màster (0.17): els inserts (cadena de màstering, 0.19) i la sortida. */
class OpcionsPista : public juce::Component
{
public:
    static constexpr int maxInserts = 8;

    explicit OpcionsPista (EstatEstudi& e) : estat (e)
    {
        btEnvia.setTooltip ("Enviaments de la pista als retorns d'efectes (nivell, mode, treure, afegir-ne)"_ca);
        btEnvia.onClick = [this] { estat.menuEnviamentsPista (btEnvia, estat.pistaTriada); };
        for (auto* b : { &btInstrument, &btSo, &btTeclat, &btSortida, &btEnvia, &btObre, &btKit })
        {
            b->setMidaText (12.0f);
            addChildComponent (b);
        }
        for (int k = 0; k < maxInserts; ++k)
        {
            auto& b = *inserts.add (new FilaInsert (estat, k));
            b.pista = [this] { return estat.masterTriat() ? -1 : estat.pistaTriada; };
            addChildComponent (b);
        }
        btInstrument.onClick = [this] { menuInstrument(); };
        btObre.setTooltip ("Obre el panell de l'instrument (amb l'escala del teclat al costat)"_ca);
        btObre.onClick = [this] { obreInstrument(); };
        btKit.setTooltip ("Kits de bateria: tria'n un, importa'n de nous o revisa'n els sons"_ca);
        btKit.onClick = [this] { if (onObreKits) onObreKits(); };
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
            estat.menuSortidaPista (btSortida, estat.pistaTriada);   // 0.40: el menú de debò (MASTER, FX, Grups)
        };
    }

    std::function<juce::Array<juce::MidiDeviceInfo>()> teclats;
    /** Noms de les sortides actives de la targeta (0.17). */
    std::function<juce::StringArray()> sortides;
    /** Els plugins del projecte obert (0.19). */
    GestorPlugins* plugins = nullptr;
    void setPlugins (GestorPlugins* p) { plugins = p; for (auto* f : inserts) f->plugins = p; }
    /** 0.20: obre la finestra dels kits de bateria (per a la pista triada). */
    std::function<void()> onObreKits;
    std::function<void()> onObreSampler;   // 0.26

    /** Alçada que necessita per a la pista triada. */
    int alcada() const
    {
        const int slots = numSlots();
        if (estat.masterTriat()) return 16 + slots * 26 + 28 + 8;
        const auto* t = estat.pista (estat.pistaTriada);
        if (t == nullptr) return 0;
        // 0.40: sortida i enviaments a totes les pistes (dues files més a les d'instrument, una més a la resta)
        if (t->tipus == "midi") return (GestorPlugins::esVst3 (t->instrument) ? 2 : 3) * 32 + 16 + slots * 26 + 6 + 2 * 32;
        return 16 + slots * 26 + 28 + 10 + 32;
    }

    void actualitza()
    {
        const auto* t = estat.pista (estat.pistaTriada);
        const bool master = estat.masterTriat();
        const bool midiPista = ! master && t != nullptr && t->tipus == "midi";
        const bool bateria = ! master && t != nullptr && mibase::esKit (*t);
        const bool audio = ! master && t != nullptr && ! midiPista && ! bateria;
        const bool vst = midiPista && GestorPlugins::esVst3 (t->instrument);
        btInstrument.setVisible (midiPista);
        btObre.setVisible (midiPista && ! bateria);
        btKit.setVisible (bateria);
        btTeclat.setVisible (midiPista || bateria);
        if (bateria)
        {
            btKit.setButtonText ("Kit: "_ca + juce::String::fromUTF8 ((t->kit.empty() ? std::string ("Intern") : t->kit).c_str()));
            btTeclat.setButtonText ("Teclat: "_ca + (t->entradaMidi.empty() ? "qualsevol"_ca : juce::String::fromUTF8 (t->entradaMidiNom.c_str())));
        }
        const bool sampler = midiPista && mibase::esSampler (*t);
        btSo.setVisible (midiPista && ! vst && ! bateria);   // 0.39: amb el kit de bateria, el «so» és el kit
        btSortida.setVisible (t != nullptr || master);   // 0.40: també a les pistes d'instrument
        btEnvia.setVisible (! master && t != nullptr);
        if (! master && t != nullptr) btEnvia.setButtonText (EstatEstudi::textEnviaments (estat.projecte, *t));

        const auto* v = llistaInserts();
        const int slots = numSlots();
        for (int k = 0; k < maxInserts; ++k)
        {
            auto& b = *inserts[k];
            b.setVisible (v != nullptr && k < slots);
            b.repaint();
        }
        if (midiPista)
        {
            // 0.20: només el nom (al costat hi ha el botó per obrir-lo); el detall, a la bafarada.
            btInstrument.setButtonText (bateria ? "Kit de bateria"_ca : vst ? juce::String::fromUTF8 (t->nomInstrument.c_str()) : sampler ? "Samplejador"_ca
                                        : mibase::gm::programa (t->instrument) >= 0 ? "General MIDI"_ca
                                        : guitarres::part (t->instrument, false).isNotEmpty() ? guitarres::part (t->instrument, false) : "Sinte intern"_ca);   // 0.43
            btInstrument.setTooltip ("Instrument de la pista: clica per canviar-lo (sinte intern, General MIDI, samplejador, kit de bateria o VST3)"_ca);
            if (sampler)
                btSo.setButtonText ("Sample: "_ca + (t->sampler.nom.empty() ? "cap"_ca : juce::String::fromUTF8 (t->sampler.nom.c_str())));
            else
                btSo.setButtonText ("So: "_ca + (guitarres::part (t->instrument, true).isNotEmpty() ? guitarres::part (t->instrument, true)
                                     : juce::String::fromUTF8 (mibase::midi::nomInstrument (t->instrument.empty() || vst ? "intern:piano" : t->instrument).c_str())));
            btTeclat.setButtonText ("Teclat: "_ca + (t->entradaMidi.empty() ? "qualsevol"_ca : juce::String::fromUTF8 (t->entradaMidiNom.c_str())));
        }
        if (master)
        {
            const auto noms = sortides ? sortides() : juce::StringArray();
            const int k = 2 * estat.projecte.sortidaMaster;
            btSortida.setButtonText ("Sortida: "_ca + (k + 1 < noms.size() ? noms[k] + " / " + noms[k + 1]
                                                                          : juce::String (k + 1) + "/" + juce::String (k + 2)));
        }
        else if (t != nullptr)
        {
            // 0.33: si la pista va a un bus, es diu aquí (es tria a la taula de mescles).
            const int d = t != nullptr ? mibase::edicio::pistaPerId (estat.projecte, t->sortida) : -1;
            btSortida.setButtonText (d >= 0 ? "Sortida: "_ca + juce::String::fromUTF8 (estat.projecte.pistes[(size_t) d].nom.c_str())
                                            : "Sortida: Màster (L+R)"_ca);
        }
        resized();
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto fila = [&r] (int h) { auto f = r.removeFromTop (h); r.removeFromTop (4); return f; };
        if (btInstrument.isVisible())
        {
            auto f = fila (28);
            btObre.setBounds (f.removeFromRight (34));
            f.removeFromRight (4);
            btInstrument.setBounds (f);
            if (btSo.isVisible()) btSo.setBounds (fila (28));
            if (btKit.isVisible()) btKit.setBounds (fila (28));
            btTeclat.setBounds (fila (28));
        }
        else if (btKit.isVisible())
        {
            btKit.setBounds (fila (28));
            btTeclat.setBounds (fila (28));
        }
        yTitol = r.getY();
        r.removeFromTop (16);   // títol «Inserts»
        for (auto* b : inserts)
            if (b->isVisible()) b->setBounds (fila (22));
        if (btSortida.isVisible())
            btSortida.setBounds (fila (28));
        if (btEnvia.isVisible())
            btEnvia.setBounds (fila (28));
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

public:
    /** 0.20: obre el panell de l'instrument de la pista triada: el del VST3 o el del sinte intern, amb l'escala al costat. */
    void obreInstrument()
    {
        auto* t = estat.pista (estat.pistaTriada);
        if (t == nullptr || t->tipus != "midi") return;
        if (mibase::esSampler (*t)) { if (onObreSampler) onObreSampler(); return; }
        const auto id = t->id;
        if (GestorPlugins::esVst3 (t->instrument))
        {
            if (plugins != nullptr)
                plugins->obreFinestra ("inst:" + id, juce::String::fromUTF8 (t->nomInstrument.c_str()),
                                       [this, id] { return std::make_unique<PanellEscala> (estat, id); });
            return;
        }
        if (auto f = finestresSinte.find (id); f != finestresSinte.end()) { f->second->toFront (true); return; }
        finestresSinte[id] = std::make_unique<FinestraSinte> (estat, id, juce::String::fromUTF8 (t->nom.c_str()) + " · sinte intern"_ca,
                                                              [this, id] { finestresSinte.erase (id); });
    }
private:
    void menuInstrument()
    {
        auto* t = estat.pista (estat.pistaTriada);
        if (t == nullptr) return;
        const bool vst = GestorPlugins::esVst3 (t->instrument);
        auto tipus = std::make_shared<juce::Array<juce::PluginDescription>>();
        juce::PopupMenu m;
        if (vst) { m.addItem (2, "Obre la finestra del plugin"); m.addSeparator(); }
        const bool sampler = mibase::esSampler (*t);
        const int gm = mibase::gm::programa (t->instrument);
        const bool kit = mibase::esKit (*t);
        m.addItem (1, "Sinte intern", true, ! vst && ! sampler && gm < 0 && ! kit);
        m.addSubMenu ("General MIDI (128 sons)"_ca, bancgm::menu (500, gm), true, juce::Image(), gm >= 0);   // 0.31
        m.addSubMenu ("Guitarres (SoundFonts)"_ca, guitarres::menu (7000, t->instrument), true, juce::Image(), t->instrument.rfind ("sf2:", 0) == 0);   // 0.43
        m.addItem (3, "Samplejador"_ca, true, sampler);
        m.addItem (4, "Kit de bateria"_ca, true, kit);   // 0.39
        if (plugins != nullptr) m.addSubMenu ("Instruments VST3", plugins->menu (true, 1000, *tipus));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btInstrument), [this, tipus] (int r)
        {
            auto* p = estat.pista (estat.pistaTriada);
            if (p == nullptr) return;
            const auto id = p->id;
            if (r == 2) obreInstrument();
            else if (r >= 500 && r < 628) triaGM (r - 500);
            else if (r >= 7000 && r < 12000) triaGuitarra (r - 7000);
            else if (r == 4) { if (! mibase::esKit (*p)) canvia ([] (mibase::Pista& q) { q.instrument = mibase::instrumentKit; q.nomInstrument.clear(); q.estatInstrument.clear(); q.canals = 2; kits::posaKit (q, q.kit.empty() ? kits::ultimKit() : q.kit); }, "Instrument: kit de bateria"_ca); }
            else if (r == 1 && (GestorPlugins::esVst3 (p->instrument) || mibase::esSampler (*p) || mibase::esKit (*p) || mibase::gm::programa (p->instrument) >= 0))
                canvia ([] (mibase::Pista& q) { q.instrument = "intern:piano"; q.nomInstrument.clear(); q.estatInstrument.clear(); }, "Instrument: sinte intern"_ca);
            else if (r == 3)
            {
                if (! mibase::esSampler (*p))
                    canvia ([] (mibase::Pista& q) { q.instrument = mibase::instrumentSampler; q.nomInstrument.clear(); q.estatInstrument.clear(); }, "Instrument: samplejador"_ca);
                if (onObreSampler) juce::MessageManager::callAsync ([this] { if (onObreSampler) onObreSampler(); });
            }
            else if (r >= 1000 && r - 1000 < tipus->size())
            {
                const auto d = (*tipus)[r - 1000];
                canvia ([d] (mibase::Pista& q)
                {
                    q.instrument = "vst3:" + d.createIdentifierString().toStdString();
                    q.nomInstrument = d.name.toStdString();
                    q.estatInstrument.clear();
                }, "Instrument: "_ca + d.name);
                juce::MessageManager::callAsync ([this] { obreInstrument(); });
            }
        });
    }

    /** 0.31: posa un so General MIDI a la pista triada (si no hi ha el banc, pregunta si es baixa). */
    void triaGuitarra (int index)
    {
        const auto ins = guitarres::tria (index);   // 0.43
        if (ins.empty()) return;
        canvia ([ins] (mibase::Pista& q) { q.instrument = ins; q.nomInstrument.clear(); q.estatInstrument.clear(); },
                "Instrument: "_ca + juce::String::fromUTF8 (mibase::midi::nomInstrument (ins).c_str()));
    }

    void triaGM (int prog)
    {
        bancgm::assegura (this, [this, prog]
        {
            canvia ([prog] (mibase::Pista& q) { q.instrument = mibase::gm::instrument (prog); q.nomInstrument.clear(); q.estatInstrument.clear(); },
                    "Instrument: General MIDI "_ca + juce::String::fromUTF8 (mibase::gm::nomPrograma (prog)));
        });
    }

    void menuSo()
    {
        auto* t = estat.pista (estat.pistaTriada);
        if (t == nullptr) return;
        if (mibase::esSampler (*t)) { if (onObreSampler) onObreSampler(); return; }
        juce::PopupMenu m;
        const int actual = juce::jmax (0, mibase::midi::soIntern (t->instrument.empty() ? "intern:piano" : t->instrument));
        const int gm = mibase::gm::programa (t->instrument);
        for (int k = 0; k < (int) mibase::midi::sonsInterns().size(); ++k)
            m.addItem (k + 1, juce::String::fromUTF8 (mibase::midi::sonsInterns()[(size_t) k].nom), true, actual == k && gm < 0);
        m.addSeparator();
        m.addSubMenu ("General MIDI (128 sons)"_ca, bancgm::menu (500, gm), true, juce::Image(), gm >= 0);   // 0.31
        m.addSubMenu ("Guitarres (SoundFonts)"_ca, guitarres::menu (7000, t->instrument), true, juce::Image(), t->instrument.rfind ("sf2:", 0) == 0);   // 0.43
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btSo), [this] (int r)
        {
            if (r <= 0) return;
            if (r >= 500 && r < 628) { triaGM (r - 500); return; }
            if (r >= 7000 && r < 12000) { triaGuitarra (r - 7000); return; }
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
    Boto btEnvia { "Enviaments", std::nullopt, Boto::Estil::Secundari };   // 0.40
    Boto btObre { {}, dibuix::Icona::Finestra, Boto::Estil::Secundari };
    Boto btKit { "Kit", dibuix::Icona::Bateria, Boto::Estil::Secundari };
    juce::OwnedArray<FilaInsert> inserts;
    std::map<std::string, std::unique_ptr<FinestraSinte>> finestresSinte;
public:
    void tancaFinestresSinte() { finestresSinte.clear(); }   // 0.27: Esc
private:
    int yTitol = 0;
};
