#pragma once

#include "ControlSidechain.h"
#include "Controls.h"
#include "EstatEstudi.h"
#include "Plugins.h"

/** 0.20: el que es pot fer amb els inserts (efectes) d'una pista o del màster, igual a la columna esquerra i a la
    taula de mescles. Cada insert es mostra com una fila: [encendre] [nom: clic = obre la finestra] [canviar].
    Botó dret: moure'l, treure'l. «pista» = índex de la pista, o -1 per al màster. */
namespace inserts
{
    inline std::vector<mibase::Insert>* llista (EstatEstudi& estat, int pista)
    {
        if (pista < 0) return &estat.projecte.insertsMaster;
        auto* t = estat.pista (pista);
        return t != nullptr ? &t->inserts : nullptr;
    }

    inline juce::String titol (const mibase::Insert& x)
    {
        if (x.plugin == mibase::saturacio::idPlugin) return juce::String::fromUTF8 (mibase::saturacio::nomPlugin);   // 0.68: els projectes vells deien «Saturació»
        return juce::String::fromUTF8 (x.nom.c_str());
    }

    /** 0.22: qui obre el Vocal Editor d'una pista (el posa la pantalla de l'estudi). */
    inline std::function<void (int pista)>& obreVeu() { static std::function<void (int)> f; return f; }

    inline void canvia (EstatEstudi& estat, GestorPlugins* plugins, int pista, const std::function<void (std::vector<mibase::Insert>&)>& f,
                        const juce::String& evolucio)
    {
        if (llista (estat, pista) == nullptr) return;
        if (plugins != nullptr) plugins->desaEstats (estat.projecte);   // que el desfer recordi els ajustos d'ara
        estat.desfes.abans (estat.projecte);
        f (*llista (estat, pista));
        estat.canviat (evolucio);
    }

    inline void obre (EstatEstudi& estat, GestorPlugins* plugins, int pista, int k)
    {
        auto* v = llista (estat, pista);
        if (v == nullptr || k < 0 || k >= (int) v->size()) return;
        const auto x = (*v)[(size_t) k];
        if (mibase::esInsertVeu (x)) { if (obreVeu()) obreVeu() (pista); return; }
        if (plugins == nullptr) return;
        if (GestorPlugins::esIntern (x.plugin))
        {
            const auto* t = estat.pista (pista);
            const juce::String qui = x.plugin == mibase::reverb::idPlugin ? "Reverb · "_ca
                                   : x.plugin == mibase::dinamica::idPlugin ? "Dinàmica · "_ca
                                   : x.plugin == mibase::delay::idPlugin ? "Delay · "_ca
                                   : x.plugin == mibase::saturacio::idPlugin ? "Decapitor · "_ca
                                   : x.plugin == mibase::drive::idPlugin ? "Drive · "_ca
                                   : x.plugin == mibase::guitarrack::idPlugin ? "Guitar Rack · "_ca   // 1.5
                                   : x.plugin == mibase::evolve::idPlugin ? "EVOLVE · "_ca   // 1.13
                                   : x.plugin == mibase::tune::idPlugin ? "Tune · "_ca   // 0.80
                                   : x.plugin == mibase::limitador::idPlugin ? "Limitador · "_ca
                                   : x.plugin == mibase::rack::idPlugin ? "Vocal Studio · "_ca
                                   : x.plugin == mibase::rack::idPluginMaster ? "Mastering Studio · "_ca : "EQ Pro · "_ca;   // 0.94   // 0.65: abans l'EQ deia «Dinàmica»
            plugins->obreIntern (x, qui + (t != nullptr ? juce::String::fromUTF8 (t->nom.c_str()) : "Màster"_ca));
            return;
        }
        plugins->obreFinestra (x.id, titol (x));
    }

    /** 0.56: qui obre l'editor d'automatització amb una línia per a un insert (el posa la pantalla de l'estudi). */
    inline std::function<void (int pista, const std::string& idInsert)>& obreAutomatitzacio() { static std::function<void (int, const std::string&)> f; return f; }

    /** 0.56: la finestra de les fonts de sidechain d'un insert. */
    class PanellSidechain : public juce::Component
    {
    public:
        PanellSidechain (EstatEstudi& e, GestorPlugins* g, int p, std::string id) : estat (e), plugins (g), pista (p), idInsert (std::move (id))
        {
            addAndMakeVisible (btAfegeix);
            btAfegeix.onClick = [this] { menuFonts(); };
            refes();
        }

        mibase::Insert* insert()
        {
            if (auto* v = llista (estat, pista))
                for (auto& x : *v) if (x.id == idInsert) return &x;
            return nullptr;
        }

        void refes()
        {
            files.clear();
            auto* x = insert();
            const int n = x != nullptr ? (int) x->sidechain.size() : 0;
            for (int i = 0; i < n; ++i)
            {
                auto f = std::make_unique<Fila>();
                const auto& font = x->sidechain[(size_t) i];
                const int j = mibase::edicio::pistaPerId (estat.projecte, font.pista);
                f->nom = j >= 0 ? juce::String::fromUTF8 (estat.projecte.pistes[(size_t) j].nom.c_str()) : "(pista esborrada)"_ca;
                f->nivell.setSliderStyle (juce::Slider::LinearHorizontal);
                f->nivell.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
                f->nivell.setRange (-60.0, 12.0, 0.1);
                f->nivell.setTextValueSuffix (" dB");
                f->nivell.setValue (font.nivellDb, juce::dontSendNotification);
                f->nivell.setDoubleClickReturnValue (true, 0.0);
                f->nivell.setTooltip ("Nivell de l'enviament de sidechain (no canvia el que se sent de la pista)."_ca);
                f->nivell.onDragStart = [this] { estat.desfes.abans (estat.projecte); };
                f->nivell.onValueChange = [this, i, s = &f->nivell]
                {
                    if (auto* y = insert(); y != nullptr && i < (int) y->sidechain.size()) y->sidechain[(size_t) i].nivellDb = s->getValue();
                    estat.canviat();
                };
                f->punt.addItemList ({ "Abans dels efectes"_ca, "Després dels efectes (abans del fader)"_ca, "Després del fader"_ca }, 1);
                f->punt.setSelectedItemIndex (font.punt, juce::dontSendNotification);
                f->punt.setTooltip ("D'on es capta el so de la pista d'origen."_ca);
                f->punt.onChange = [this, i, c = &f->punt] { canvia ([i, v = c->getSelectedItemIndex()] (mibase::Insert& y) { if (i < (int) y.sidechain.size()) y.sidechain[(size_t) i].punt = v; }); };
                f->actiu.setLlum (tema::accent);
                f->actiu.setToggleState (font.actiu, juce::dontSendNotification);
                f->actiu.setTooltip ("Encén o apaga aquesta font."_ca);
                f->actiu.onClick = [this, i] { canvia ([i] (mibase::Insert& y) { if (i < (int) y.sidechain.size()) y.sidechain[(size_t) i].actiu = ! y.sidechain[(size_t) i].actiu; }); };
                f->treu.setTooltip ("Treu aquesta font."_ca);
                f->treu.onClick = [this, i] { canvia ([i] (mibase::Insert& y) { if (i < (int) y.sidechain.size()) y.sidechain.erase (y.sidechain.begin() + i); }); };
                for (juce::Component* c : { (juce::Component*) &f->nivell, (juce::Component*) &f->punt, (juce::Component*) &f->actiu, (juce::Component*) &f->treu })
                    addAndMakeVisible (c);
                files.push_back (std::move (f));
            }
            setSize (640, 78 + juce::jmax (1, n) * 34 + 34);
            resized();
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (13.5f, true));
            auto* x = insert();
            g.drawText ("Sidechain · "_ca + (x != nullptr ? titol (*x) : juce::String()), getLocalBounds().reduced (12, 8).removeFromTop (20), juce::Justification::centredLeft);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (11.0f));
            g.drawFittedText ("El so d'aquestes pistes controla l'efecte (ducking, compressió…) però no s'hi barreja. Si no n'hi ha cap, l'efecte escolta el seu propi so."_ca,
                              getLocalBounds().reduced (12, 0).withTrimmedTop (30).withHeight (30), juce::Justification::centredLeft, 2, 0.85f);
            if (files.empty())
            {
                g.drawText ("Encara no hi ha cap font."_ca, getLocalBounds().reduced (12, 0).withTrimmedTop (70).withHeight (30), juce::Justification::centredLeft);
                return;
            }
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (12.0f, true));
            for (size_t i = 0; i < files.size(); ++i)
                g.drawText (files[i]->nom, 12, 70 + (int) i * 34, 120, 28, juce::Justification::centredLeft);
        }

        void resized() override
        {
            for (size_t i = 0; i < files.size(); ++i)
            {
                auto r = juce::Rectangle<int> (136, 70 + (int) i * 34, getWidth() - 148, 28);
                files[i]->treu.setBounds (r.removeFromRight (28));
                r.removeFromRight (4);
                files[i]->actiu.setBounds (r.removeFromRight (54));
                r.removeFromRight (6);
                files[i]->punt.setBounds (r.removeFromRight (200));
                r.removeFromRight (6);
                files[i]->nivell.setBounds (r);
            }
            btAfegeix.setBounds (12, getHeight() - 32, 190, 26);
        }

    private:
        struct Fila
        {
            juce::String nom;
            juce::Slider nivell;
            juce::ComboBox punt;
            Boto actiu { "On", std::nullopt, Boto::Estil::Tecla };
            Boto treu { {}, dibuix::Icona::Tanca, Boto::Estil::Subtil };
        };

        void canvia (std::function<void (mibase::Insert&)> f)
        {
            const auto id = idInsert;
            inserts::canvia (estat, plugins, pista, [f, id] (std::vector<mibase::Insert>& l) { for (auto& x : l) if (x.id == id) f (x); },
                             "Sidechain"_ca);
            juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PanellSidechain> (this)] { if (sp != nullptr) sp->refes(); });
        }

        void menuFonts()
        {
            auto* x = insert();
            if (x == nullptr) return;
            const std::string desti = pista >= 0 && estat.pista (pista) != nullptr ? estat.pista (pista)->id : std::string();
            juce::PopupMenu m;
            for (int j = 0; j < estat.numPistes(); ++j)
            {
                const auto& t = estat.projecte.pistes[(size_t) j];
                if (mibase::esAcords (t)) continue;
                bool ja = false;
                for (auto& f : x->sidechain) ja = ja || f.pista == t.id;
                const bool pot = ! ja && mibase::edicio::potSidechain (estat.projecte, t.id, desti);
                m.addItem (j + 1, juce::String::fromUTF8 (t.nom.c_str()) + (t.id == desti ? " (aquesta)"_ca : (! pot && ! ja ? " (faria un cercle)"_ca : juce::String())), pot, ja);
            }
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btAfegeix), [this] (int r)
            {
                if (r <= 0 || r > estat.numPistes()) return;
                const auto id = estat.projecte.pistes[(size_t) r - 1].id;
                canvia ([id] (mibase::Insert& y) { y.sidechain.push_back ({ id, 0.0, 1, true }); });
            });
        }

        EstatEstudi& estat;
        GestorPlugins* plugins;
        int pista;
        std::string idInsert;
        std::vector<std::unique_ptr<Fila>> files;
        Boto btAfegeix { "+ Afegeix una font", std::nullopt, Boto::Estil::Secundari };
    };

    inline void obreSidechain (EstatEstudi& estat, GestorPlugins* plugins, int pista, int k, juce::Component* on)
    {
        auto* v = llista (estat, pista);
        if (v == nullptr || k < 0 || k >= (int) v->size() || on == nullptr) return;
        auto p = std::make_unique<PanellSidechain> (estat, plugins, pista, (*v)[(size_t) k].id);
        juce::CallOutBox::launchAsynchronously (std::move (p), on->getScreenBounds(), nullptr);
    }

    /** 0.57: el sidechain vist des de la finestra de l'efecte (botó SC i pista d'on escolta). */
    class ServeiScEstudi : public ServeiSidechain
    {
    public:
        ServeiScEstudi (EstatEstudi& e, GestorPlugins& g) : estat (e), plugins (g) {}

        bool actiu (const std::string& id) override
        {
            int p, k;
            auto* x = busca (id, p, k);
            if (x == nullptr) return false;
            for (auto& f : x->sidechain) if (f.actiu) return true;
            return false;
        }

        juce::String nomFonts (const std::string& id) override
        {
            int p, k;
            auto* x = busca (id, p, k);
            juce::String t;
            if (x == nullptr) return t;
            for (auto& f : x->sidechain)
            {
                const int j = mibase::edicio::pistaPerId (estat.projecte, f.pista);
                if (j < 0) continue;
                if (t.isNotEmpty()) t << " + ";
                t << juce::String::fromUTF8 (estat.projecte.pistes[(size_t) j].nom.c_str());
            }
            return t;
        }

        void commuta (const std::string& id, juce::Component* on) override
        {
            int p, k;
            auto* x = busca (id, p, k);
            if (x == nullptr) return;
            if (x->sidechain.empty()) { menu (id, on); return; }
            const bool encendre = ! actiu (id);
            inserts::canvia (estat, &plugins, p, [id, encendre] (std::vector<mibase::Insert>& l)
            {
                for (auto& y : l) if (y.id == id) for (auto& f : y.sidechain) f.actiu = encendre;
            }, encendre ? "Sidechain encès"_ca : "Sidechain apagat"_ca);
        }

        void menu (const std::string& id, juce::Component* on) override
        {
            int p, k;
            auto* x = busca (id, p, k);
            if (x == nullptr) return;
            const std::string desti = p >= 0 && estat.pista (p) != nullptr ? estat.pista (p)->id : std::string();
            juce::PopupMenu m;
            m.addSectionHeader ("D'on escolta (pots marcar-ne més d'una)"_ca);
            for (int j = 0; j < estat.numPistes(); ++j)
            {
                const auto& t = estat.projecte.pistes[(size_t) j];
                if (mibase::esAcords (t) || t.id == desti) continue;
                bool ja = false;
                for (auto& f : x->sidechain) ja = ja || f.pista == t.id;
                const bool pot = ja || mibase::edicio::potSidechain (estat.projecte, t.id, desti);
                m.addItem (j + 1, juce::String::fromUTF8 (t.nom.c_str()) + (pot ? juce::String() : " (faria un cercle)"_ca), pot, ja);
            }
            m.addSeparator();
            m.addItem (100000, "Opcions: nivell i d'on es capta…"_ca, ! x->sidechain.empty());
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (on), [this, id, sp = juce::Component::SafePointer<juce::Component> (on)] (int r)
            {
                int p2, k2;
                if (busca (id, p2, k2) == nullptr) return;
                if (r == 100000) { obreSidechain (estat, &plugins, p2, k2, sp.getComponent()); return; }
                if (r <= 0 || r > estat.numPistes()) return;
                const auto font = estat.projecte.pistes[(size_t) r - 1].id;
                inserts::canvia (estat, &plugins, p2, [id, font] (std::vector<mibase::Insert>& l)
                {
                    for (auto& y : l)
                        if (y.id == id)
                        {
                            auto it = std::find_if (y.sidechain.begin(), y.sidechain.end(), [&] (auto& f) { return f.pista == font; });
                            if (it != y.sidechain.end()) y.sidechain.erase (it);
                            else y.sidechain.push_back ({ font, 0.0, 1, true });
                        }
                }, "Sidechain"_ca);
            });
        }

    private:
        mibase::Insert* busca (const std::string& id, int& pista, int& k)
        {
            for (int p = -1; p < estat.numPistes(); ++p)
                if (auto* v = llista (estat, p))
                    for (size_t i = 0; i < v->size(); ++i)
                        if ((*v)[i].id == id) { pista = p; k = (int) i; return &(*v)[i]; }
            return nullptr;
        }
        EstatEstudi& estat;
        GestorPlugins& plugins;
    };

    inline void commuta (EstatEstudi& estat, GestorPlugins* plugins, int pista, int k)
    {
        auto* v = llista (estat, pista);
        if (v == nullptr || k < 0 || k >= (int) v->size()) return;
        const bool ara = ! (*v)[(size_t) k].actiu;
        canvia (estat, plugins, pista, [k] (std::vector<mibase::Insert>& l) { if (k < (int) l.size()) l[(size_t) k].actiu = ! l[(size_t) k].actiu; },
                (ara ? "Efecte encès: "_ca : "Efecte apagat: "_ca) + titol ((*v)[(size_t) k]));
    }

    /** 0.31: treu l'insert k (els de sota pugen). */
    inline void treu (EstatEstudi& estat, GestorPlugins* plugins, int pista, int k)
    {
        auto* v = llista (estat, pista);
        if (v == nullptr || k < 0 || k >= (int) v->size()) return;
        const auto nom = titol ((*v)[(size_t) k]);
        canvia (estat, plugins, pista, [k] (std::vector<mibase::Insert>& l) { if (k < (int) l.size()) l.erase (l.begin() + k); }, "Treu efecte: "_ca + nom);
    }

    /** Tria un plugin d'efecte per posar-lo a l'insert k (si k és el primer buit, se n'afegeix un). */
    inline void tria (EstatEstudi& estat, GestorPlugins* plugins, int pista, int k, juce::Component* on)
    {
        auto* v = llista (estat, pista);
        if (v == nullptr) return;
        auto tipus = std::make_shared<juce::Array<juce::PluginDescription>>();
        juce::PopupMenu efectes;
        if (plugins != nullptr) efectes = plugins->menu (false, 1000, *tipus);
        const bool nou = k >= (int) v->size();
        juce::PopupMenu m;
        m.addSectionHeader (nou ? "Efecte nou"_ca : "Canvia " + titol ((*v)[(size_t) k]) + " per…"_ca);
        if (! nou)
        {
            // 0.31: treure'l sense posar-ne cap altre
            m.addItem (2, "Treu-lo (sense posar-n'hi cap altre)"_ca);
            m.addSeparator();
        }
        // 0.22: el Vocal Editor (intern), només a les pistes d'àudio i un per pista.
        if (auto* t = estat.pista (pista); t != nullptr && ! mibase::esMidi (*t))
        {
            bool jaHiEs = false;
            for (int i = 0; i < (int) v->size(); ++i) if (i != k && mibase::esInsertVeu ((*v)[(size_t) i])) jaHiEs = true;
            m.addItem (1, "Vocal Editor (Mibase): afina i edita la veu"_ca, ! jaHiEs);
            m.addSeparator();
        }
        // 0.55: la Reverb de Mibase (interna), a qualsevol pista, bus o al màster.
        m.addItem (3, "Reverb (Mibase): espais, plates, convolució, shimmer…"_ca);
        m.addItem (4, "Dinàmica (Mibase): compressor, gate, expander, de-esser"_ca);
        m.addItem (5, "EQ Pro (Mibase): equalitzador de fins a 24 bandes"_ca);
        m.addItem (6, "Delay (Mibase): ecos de cinta, analògics, ping-pong…"_ca);
        m.addItem (7, "Decapitor (Mibase): saturació de cinta, consola, vàlvules o suau (per a la mescla)"_ca);
        m.addItem (13, "Guitar Rack (Mibase): amplis, pantalles, micròfons i pedals de guitarra"_ca);   // 1.5
        m.addItem (14, "EVOLVE (Mibase): un sol pom que transforma el so (distorsió, filtre, bombeig, shimmer)"_ca);   // 1.13
        // 1.7: el Drive sol ja no surt al menú (el substitueix el Guitar Rack); els projectes que el tenen continuen sonant
        m.addItem (9, "Limitador (Mibase): per al màster i els busos, amb LUFS"_ca);
        m.addItem (10, "Vocal Studio (Mibase): rack de veu amb mòduls, 8 macros i presets"_ca);   // 0.70
        m.addItem (11, "Tune (Mibase): afinació de la veu a l'escala"_ca);   // 0.80
        m.addItem (12, "Mastering Studio (Mibase): rack de màster amb loudness, A/B igualat, 8 macros i presets"_ca);   // 0.94
        m.addSeparator();
        for (juce::PopupMenu::MenuItemIterator it (efectes); it.next();)
        {
            auto& i = it.getItem();
            if (i.subMenu != nullptr) m.addSubMenu (i.text, *i.subMenu);
            else m.addItem (i.itemID, i.text, i.isEnabled);
        }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (on), [&estat, plugins, pista, k, nou, tipus] (int r)
        {
            if (r == 2) { treu (estat, plugins, pista, k); return; }
            if (r == 1)
            {
                canvia (estat, plugins, pista, [&] (std::vector<mibase::Insert>& l)
                {
                    mibase::Insert x;
                    x.id = "i" + std::to_string (++estat.projecte.comptador);
                    x.plugin = mibase::pluginVeu;
                    x.nom = "Vocal Editor";
                    if (nou || k >= (int) l.size()) l.push_back (x);
                    else l[(size_t) k] = x;
                }, "Efecte: Vocal Editor"_ca);
                juce::MessageManager::callAsync ([pista] { if (obreVeu()) obreVeu() (pista); });
                return;
            }
            if (r >= 3 && r <= 14)
            {
                std::string id;
                canvia (estat, plugins, pista, [&] (std::vector<mibase::Insert>& l)
                {
                    mibase::Insert x;
                    x.id = id = "i" + std::to_string (++estat.projecte.comptador);
                    static const char* const ids[] = { mibase::reverb::idPlugin, mibase::dinamica::idPlugin, mibase::eq::idPlugin, mibase::delay::idPlugin, mibase::saturacio::idPlugin, mibase::drive::idPlugin, mibase::limitador::idPlugin, mibase::rack::idPlugin, mibase::tune::idPlugin, mibase::rack::idPluginMaster, mibase::guitarrack::idPlugin, mibase::evolve::idPlugin };
                    static const char* const noms[] = { mibase::reverb::nomPlugin, mibase::dinamica::nomPlugin, mibase::eq::nomPlugin, mibase::delay::nomPlugin, mibase::saturacio::nomPlugin, mibase::drive::nomPlugin, mibase::limitador::nomPlugin, mibase::rack::nomPlugin, mibase::tune::nomPlugin, mibase::rack::nomPluginMaster, mibase::guitarrack::nomPlugin, mibase::evolve::nomPlugin };
                    x.plugin = ids[r - 3];
                    x.nom = noms[r - 3];
                    if (nou || k >= (int) l.size()) l.push_back (x);
                    else l[(size_t) k] = x;
                }, "Efecte: "_ca + juce::String::fromUTF8 (r == 3 ? mibase::reverb::nomPlugin : r == 4 ? mibase::dinamica::nomPlugin : r == 5 ? mibase::eq::nomPlugin
                                                     : r == 6 ? mibase::delay::nomPlugin : r == 7 ? mibase::saturacio::nomPlugin : r == 8 ? mibase::drive::nomPlugin
                                                     : r == 9 ? mibase::limitador::nomPlugin : r == 10 ? mibase::rack::nomPlugin : r == 11 ? mibase::tune::nomPlugin
                                                     : r == 13 ? mibase::guitarrack::nomPlugin : r == 14 ? mibase::evolve::nomPlugin : mibase::rack::nomPluginMaster));
                juce::MessageManager::callAsync ([&estat, plugins, pista, id]
                {
                    if (auto* l = llista (estat, pista))
                        for (int i = 0; i < (int) l->size(); ++i)
                            if ((*l)[(size_t) i].id == id) obre (estat, plugins, pista, i);
                });
                return;
            }
            if (r < 1000 || r - 1000 >= tipus->size()) return;
            const auto d = (*tipus)[r - 1000];
            std::string id;
            canvia (estat, plugins, pista, [&] (std::vector<mibase::Insert>& l)
            {
                mibase::Insert x;
                x.id = id = "i" + std::to_string (++estat.projecte.comptador);
                x.plugin = d.createIdentifierString().toStdString();
                x.nom = d.name.toStdString();
                if (nou || k >= (int) l.size()) l.push_back (x);
                else l[(size_t) k] = x;
            }, "Efecte: "_ca + d.name);
            juce::MessageManager::callAsync ([plugins, id, nom = d.name] { if (plugins != nullptr) plugins->obreFinestra (id, nom); });
        });
    }

    /** Botó dret: obre, encén/apaga, canvia, mou o treu. */
    inline void menuMes (EstatEstudi& estat, GestorPlugins* plugins, int pista, int k, juce::Component* on)
    {
        auto* v = llista (estat, pista);
        if (v == nullptr || k < 0 || k >= (int) v->size()) return;
        const auto x = (*v)[(size_t) k];
        juce::PopupMenu m;
        m.addItem (1, mibase::esInsertVeu (x) ? "Obre el Vocal Editor"_ca : (GestorPlugins::esIntern (x.plugin) ? "Obre l'efecte"_ca : "Obre la finestra del plugin"_ca));
        m.addItem (2, x.actiu ? "Apaga'l (el so hi passa de llarg)"_ca : "Encén-lo"_ca);
        // 0.56
        if (! mibase::esInsertVeu (x))
        {
            const bool sc = plugins != nullptr && plugins->admetSidechain (x);
            m.addItem (7, sc ? "Sidechain…"_ca + (x.sidechain.empty() ? juce::String() : " (" + juce::String ((int) x.sidechain.size()) + ")")
                             : "Sidechain (aquest plugin no té entrada lateral)"_ca, sc);
            m.addItem (8, "Automatitza un paràmetre…"_ca, pista >= 0);
        }
        m.addItem (6, "Canvia'l per un altre…"_ca);
        // 0.69: «Retard mínim» no l'apaga mai (per sentir un ampli de guitarra mentre es grava)
        m.addItem (9, "Sempre encès amb RM (Retard mínim)"_ca, true, x.encesAmbRM);
        m.addSeparator();
        m.addItem (3, "Mou amunt"_ca, k > 0);
        m.addItem (4, "Mou avall"_ca, k + 1 < (int) v->size());
        m.addSeparator();
        m.addItem (5, "Treu-lo"_ca);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (on), [&estat, plugins, pista, k, on] (int r)
        {
            if (r == 1) obre (estat, plugins, pista, k);
            else if (r == 2) commuta (estat, plugins, pista, k);
            else if (r == 6) tria (estat, plugins, pista, k, on);
            else if (r == 3) canvia (estat, plugins, pista, [k] (std::vector<mibase::Insert>& l) { if (k > 0 && k < (int) l.size()) std::swap (l[(size_t) k], l[(size_t) k - 1]); }, {});
            else if (r == 4) canvia (estat, plugins, pista, [k] (std::vector<mibase::Insert>& l) { if (k + 1 < (int) l.size()) std::swap (l[(size_t) k], l[(size_t) k + 1]); }, {});
            else if (r == 5) treu (estat, plugins, pista, k);
            else if (r == 9)
                canvia (estat, plugins, pista, [k] (std::vector<mibase::Insert>& l) { if (k < (int) l.size()) l[(size_t) k].encesAmbRM = ! l[(size_t) k].encesAmbRM; },
                        "Efecte: sempre encès amb RM (o ja no)"_ca);
            else if (r == 7) obreSidechain (estat, plugins, pista, k, on);
            else if (r == 8)
            {
                auto* v2 = llista (estat, pista);
                if (v2 != nullptr && k < (int) v2->size() && obreAutomatitzacio()) obreAutomatitzacio() (pista, (*v2)[(size_t) k].id);
            }
        });
    }

    // ---- Dibuix i zones d'una fila (comú a la columna esquerra i a la taula de mescles) ----
    struct Zones { juce::Rectangle<int> encen, nom, canvia; };
    inline Zones zones (juce::Rectangle<int> r)
    {
        const int b = juce::jmin (r.getHeight(), 20);
        Zones z;
        z.encen = r.removeFromLeft (b);
        z.canvia = r.removeFromRight (b);
        z.nom = r.reduced (2, 0);
        return z;
    }

    /** x == nullptr: ranura buida («+ Efecte» si és la primera buida). */
    inline void pinta (juce::Graphics& g, juce::Rectangle<int> r, const mibase::Insert* x, bool primeraBuida, bool petit, double retardMs = 0.0)
    {
        const auto rf = r.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (rf, 3.0f);
        if (x == nullptr)
        {
            g.setColour (primeraBuida ? tema::suau : tema::linia2);
            g.setFont (dibuix::lletra (petit ? 10.0f : 11.0f));
            g.drawText (primeraBuida ? "+ Efecte"_ca : juce::String::fromUTF8 ("—"), r, juce::Justification::centred);
            return;
        }
        const auto z = zones (r);
        // Encendre: llumeta verda si sona, apagada si no.
        const auto led = z.encen.toFloat().reduced (z.encen.getHeight() * 0.22f);
        g.setColour (x->actiu ? juce::Colour (0xff7fd67a) : juce::Colour (0xff4a4a52));
        dibuix::pintaIcona (g, dibuix::Icona::Encen, led, x->actiu ? juce::Colour (0xff7fd67a) : tema::suau);
        // Nom.
        g.setColour (x->actiu ? tema::text : tema::suau);
        g.setFont (dibuix::lletra (petit ? 10.5f : 11.5f, x->actiu));
        auto zNom = z.nom;
        bool scActiu = false;
        for (auto& f : x->sidechain) scActiu = scActiu || f.actiu;
        if (scActiu)   // 0.56: té sidechain
        {
            auto sc = zNom.removeFromRight (20).toFloat().reduced (1.0f, 4.0f);
            g.setColour (tema::accent.withAlpha (0.25f));
            g.fillRoundedRectangle (sc, 2.0f);
            g.setColour (tema::accent);
            g.setFont (dibuix::lletra (8.5f, true));
            g.drawText ("SC", sc, juce::Justification::centred);
            g.setColour (x->actiu ? tema::text : tema::suau);
            g.setFont (dibuix::lletra (petit ? 10.5f : 11.5f, x->actiu));
        }
        if (retardMs >= 0.5 && ! petit)   // 0.68: aquest efecte retarda el so (Mibase ho compensa)
        {
            const auto txt = juce::String (juce::roundToInt (retardMs)) + " ms";
            auto rr = zNom.removeFromRight (juce::jmin (zNom.getWidth() / 2, 36)).toFloat().reduced (1.0f, 4.0f);
            g.setColour (vintage::ledGroc.withAlpha (0.18f));
            g.fillRoundedRectangle (rr, 2.0f);
            g.setColour (vintage::ledGroc);
            g.setFont (dibuix::lletra (8.5f, true));
            g.drawText (txt, rr, juce::Justification::centred);
            g.setColour (x->actiu ? tema::text : tema::suau);
            g.setFont (dibuix::lletra (petit ? 10.5f : 11.5f, x->actiu));
        }
        g.drawFittedText (titol (*x), zNom, juce::Justification::centredLeft, 1, 0.8f);
        // Canviar.
        dibuix::pintaIcona (g, dibuix::Icona::Canvia, z.canvia.toFloat().reduced (z.canvia.getHeight() * 0.22f), tema::suau);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRect (z.encen.getRight(), r.getY() + 3, 1, r.getHeight() - 6);
        g.fillRect (z.canvia.getX() - 1, r.getY() + 3, 1, r.getHeight() - 6);
    }

    /** Un clic sobre una fila d'insert. */
    inline void clic (EstatEstudi& estat, GestorPlugins* plugins, int pista, int k, juce::Rectangle<int> r, juce::Point<int> p,
                      bool menu, juce::Component* on)
    {
        auto* v = llista (estat, pista);
        if (v == nullptr) return;
        if (k >= (int) v->size())
        {
            if (k == (int) v->size()) tria (estat, plugins, pista, k, on);
            return;
        }
        if (menu) { menuMes (estat, plugins, pista, k, on); return; }
        const auto z = zones (r);
        if (z.encen.contains (p)) commuta (estat, plugins, pista, k);
        else if (z.canvia.contains (p)) tria (estat, plugins, pista, k, on);
        else obre (estat, plugins, pista, k);
    }

    inline juce::String ajuda() { return "Llumeta: encén o apaga · nom: obre el plugin · fletxes: canvia'l o treu-lo · arrossega'l fora: el treus · botó dret: mou, treu, sidechain i automatització"_ca; }
}

/** Una fila d'insert com a component (columna esquerra). */
class FilaInsert : public juce::Component, public juce::SettableTooltipClient
{
public:
    FilaInsert (EstatEstudi& e, int k) : estat (e), index (k) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    std::function<int()> pista;            // -1 = màster
    GestorPlugins* plugins = nullptr;
    void paint (juce::Graphics& g) override
    {
        auto* v = inserts::llista (estat, pista ? pista() : -1);
        const mibase::Insert* x = (v != nullptr && index < (int) v->size()) ? &(*v)[(size_t) index] : nullptr;
        const double ms = x != nullptr && x->actiu && plugins != nullptr ? plugins->latenciaMs (x->id) : 0.0;
        inserts::pinta (g, getLocalBounds(), x, v != nullptr && index == (int) v->size(), false, ms);
        if (fora && x != nullptr)
        {
            // 0.31: s'està arrossegant fora: si es deixa anar, es treu
            g.setColour (juce::Colour (0xffe0524a).withAlpha (0.35f));
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (11.0f, true));
            g.drawText ("Deixa'l anar per treure'l"_ca, getLocalBounds(), juce::Justification::centred);
        }
        setTooltip (x != nullptr ? inserts::ajuda() : "Clica per posar-hi un efecte (la Reverb de Mibase o un VST3)"_ca);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto* v = inserts::llista (estat, pista ? pista() : -1);
        if (v == nullptr || index >= (int) v->size() || e.mods.isPopupMenu()) return;
        const bool ara = ! getLocalBounds().expanded (4).contains (e.getPosition()) && e.getDistanceFromDragStart() > 12;
        if (ara != fora)
        {
            fora = ara;
            setMouseCursor (fora ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::PointingHandCursor);
            repaint();
        }
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (fora)
        {
            fora = false;
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
            repaint();
            inserts::treu (estat, plugins, pista ? pista() : -1, index);
            return;
        }
        if (! getLocalBounds().contains (e.getPosition())) return;
        inserts::clic (estat, plugins, pista ? pista() : -1, index, getLocalBounds(), e.getPosition(), e.mods.isPopupMenu(), this);
    }
private:
    bool fora = false;
    EstatEstudi& estat;
    int index;
};
