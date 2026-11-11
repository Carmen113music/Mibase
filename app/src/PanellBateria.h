#pragma once

#include "Kits.h"
#include "PianoRoll.h"
#include "Pom.h"
#include "DialegText.h"
#include <mibase/Bateria.h>

/** 0.20: el panell de la columna esquerra quan l'editor actiu és un Drumpad (pista de bateria).
    Kit · la fila triada (so, volum, panorama, afinació, decaïment, tall, nota) · eines sobre els cops (humanitzar, swing,
    redoble, flam) · ritmes de partida i fills · pads del teclat. */
class PanellBateria : public juce::Component, private juce::ChangeListener
{
public:
    PanellBateria (EstatEstudi& e, std::function<PianoRoll*()> editorActiu) : estat (e), editor (std::move (editorActiu))
    {
        vista.setViewedComponent (&cont, false);
        vista.setScrollBarsShown (true, false);
        vista.setScrollBarThickness (8);
        addAndMakeVisible (vista);
        cont.addAndMakeVisible (btDesaKit);   // 0.28
        btDesaKit.setMidaText (12.0f);
        btDesaKit.setTooltip ("Desa els sons d'aquesta bateria (per exemple, els que hi has arrossegat de la biblioteca) com un kit nou, per fer-lo servir en altres pistes i projectes"_ca);
        btDesaKit.onClick = [this] { desaKit(); };
        for (auto* c : std::initializer_list<juce::Component*> { &btKit, &btSo, &volum, &pan, &afina, &decai, &talla, &nota, &btFilaNova, &btTreuFila,
                                                                  &msTemps, &pctVel, &btPrimer, &btHumanitza, &swingPct, &swingPas, &btSwing,
                                                                  &copsRedoble, &btRedoble, &btFlam, &btRitme, &compassosRitme, &cadaFill, &estilFill,
                                                                  &btLlarg, &btFill, &btPadsOrdre })
            cont.addAndMakeVisible (c);
        for (auto* b : { &btKit, &btSo, &btFilaNova, &btTreuFila, &btHumanitza, &btSwing, &btRedoble, &btFlam, &btRitme, &btFill, &btPrimer, &btLlarg, &btPadsOrdre })
            b->setMidaText (12.0f);
        btKit.onClick = [this] { if (onObreKits) onObreKits(); };
        btSo.onClick = [this] { menuSo(); };
        btFilaNova.onClick = [this] { menuFilaNova(); };
        btTreuFila.onClick = [this] { canviaPista ([this] (mibase::Pista& t)
        {
            const int f = estat.filaBateria;
            if (f >= 0 && f < (int) t.files.size()) t.files.erase (t.files.begin() + f);
            estat.filaBateria = juce::jmax (0, f - 1);
        }, "Treu una fila del Drumpad"_ca); };
        // 0.28: poms com a la resta de l'app (un pas de desfer per gest)
        auto param = [this] (Pom& c, std::function<void (mibase::FilaBateria&, int)> f)
        {
            c.onComenca = [this] { estat.desfes.abans (estat.projecte); };
            c.onCanvi = [this, f] (double v)
            {
                auto* t = pista();
                if (t == nullptr || estat.filaBateria >= (int) t->files.size()) return;
                f (t->files[(size_t) estat.filaBateria], (int) std::lround (v));
                estat.avisa();
            };
            c.onAcaba = [this] { estat.canviat(); };
        };
        param (volum, [] (mibase::FilaBateria& x, int v) { x.volumDb = v; });
        param (pan, [] (mibase::FilaBateria& x, int v) { x.pan = v / 100.0; });
        param (afina, [] (mibase::FilaBateria& x, int v) { x.afina = v; });
        param (decai, [] (mibase::FilaBateria& x, int v) { x.decai = v / 100.0; });
        param (talla, [] (mibase::FilaBateria& x, int v) { x.talla = v; });
        param (nota, [] (mibase::FilaBateria& x, int v) { x.nota = v; });
        volum.setTooltip ("Volum de la fila (dB). Arrossega amunt i avall; doble clic: 0"_ca);
        pan.setTooltip ("Panorama: -100 esquerra, 100 dreta"_ca);
        afina.setTooltip ("Afinació en semitons"_ca);
        decai.setTooltip ("Quina part del so sona (100 % = sencer). Més baix: més sec"_ca);
        talla.setTooltip ("Grup de tall: un cop d'un grup talla els altres del mateix grup (1 = hihats: el tancat talla l'obert). 0 = cap"_ca);
        nota.setTooltip ("La nota MIDI que toca aquesta fila (General MIDI: 36 bombo, 38 caixa, 42 hihat…)"_ca);

        btPrimer.setClickingTogglesState (true);
        btPrimer.setToggleState (true, juce::dontSendNotification);
        btHumanitza.onClick = [this]
        {
            const double ms = msTemps.getValor(), pv = pctVel.getValor();
            const bool fixa = btPrimer.getToggleState();
            const auto llavor = (std::uint32_t) ++llavors;
            ordre ([=, this] (mibase::Clip& k, std::vector<int>& sel, long long d, long long, long long)
            {
                mibase::bateria::humanitza (k, sel, ms, pv, llavor, fixa, estat.projecte.tempo, mibase::midi::ticksPerCompas (estat.projecte), d);
            }, "Humanitzat (±"_ca + juce::String ((int) ms) + " ms, ±"_ca + juce::String ((int) pv) + " %)");
        };
        swingPas.setTriat (1);
        btSwing.onClick = [this]
        {
            const double pc = swingPct.getValor();
            const long long p = swingPas.getTriat() == 0 ? mibase::ticksPerNegra / 2 : mibase::ticksPerNegra / 4;
            ordre ([=] (mibase::Clip& k, std::vector<int>& sel, long long d, long long, long long) { mibase::bateria::swing (k, sel, pc, p, d); },
                   "Swing "_ca + juce::String ((int) pc) + " %");
        };
        copsRedoble.setTriat (2);
        btRedoble.onClick = [this]
        {
            auto* pr = editor ? editor() : nullptr;
            if (pr == nullptr || pr->numTriades() == 0) { avis ("Tria primer els cops que vols convertir en redoble"_ca); return; }
            const int cops = copsRedoble.getTriat() + 2;
            const long long p = pr->pasGraella() > 0 ? pr->pasGraella() : mibase::ticksPerNegra / 4;
            ordre ([=] (mibase::Clip& k, std::vector<int>& sel, long long, long long, long long) { mibase::bateria::redoble (k, sel, cops, p, 0.45); },
                   "Redoble de "_ca + juce::String (cops));
        };
        btFlam.onClick = [this]
        {
            auto* pr = editor ? editor() : nullptr;
            if (pr == nullptr || pr->numTriades() == 0) { avis ("Tria primer els cops on vols el flam"_ca); return; }
            ordre ([] (mibase::Clip& k, std::vector<int>& sel, long long, long long, long long) { mibase::bateria::flam (k, sel, mibase::ticksPerNegra / 24, 0.5); },
                   "Flam"_ca);
        };
        btRitme.onClick = [this] { menuRitmes(); };
        compassosRitme.setTriat (0);
        cadaFill.setTriat (1);
        estilFill.setTriat (2);
        btLlarg.setClickingTogglesState (true);
        btFill.onClick = [this]
        {
            auto* t = pista();
            if (t == nullptr) return;
            const auto files = t->files;
            const int cada = cadaFill.getTriat() == 0 ? 2 : cadaFill.getTriat() == 1 ? 4 : 8;
            const int estil = estilFill.getTriat();
            const bool llarg = btLlarg.getToggleState();
            const auto llavor = (std::uint32_t) ++llavors;
            const auto tpc = mibase::midi::ticksPerCompas (estat.projecte);
            const long long tpt = tpc / juce::jmax (1, estat.projecte.compasNum);
            int quants = 0;
            ordre ([&] (mibase::Clip& k, std::vector<int>& sel, long long d, long long a, long long b)
            {
                sel.clear();
                quants = mibase::bateria::fill (k, files, tpc, tpt, d, a, b, cada, estil, llarg, llavor);
            }, "Fills cada "_ca + juce::String (cada) + " compassos");
            if (quants == 0) avis ("No hi ha cap compàs de final de frase dins el clip (fes-lo més llarg)"_ca);
        };
        btPadsOrdre.setClickingTogglesState (false);
        btPadsOrdre.onClick = [this] { canviaPista ([] (mibase::Pista& t) { t.padsPerOrdre = ! t.padsPerOrdre; }, {}); };

        estat.addChangeListener (this);
        actualitza();
    }
    ~PanellBateria() override { estat.removeChangeListener (this); }

    std::function<void()> onObreKits;

    void resized() override
    {
        vista.setBounds (getLocalBounds());
        const int w = juce::jmax (180, getWidth() - (vista.isVerticalScrollBarShown() ? 10 : 0));
        cont.setSize (w, 10);
        int y = 8;
        auto fila = [&] (int h) { auto r = juce::Rectangle<int> (10, y, w - 20, h); y += h + 6; return r; };
        auto titol = [&] (int& yt) { yt = y; y += 20; };
        titol (yKit);
        {
            auto r = fila (28);
            btDesaKit.setBounds (r.removeFromRight (juce::jmin (110, r.getWidth() / 2)));
            r.removeFromRight (6);
            btKit.setBounds (r);
        }
        y += 6;
        titol (yFila);
        btSo.setBounds (fila (28));
        for (const auto& filaPoms : { std::array<Pom*, 3> { &volum, &pan, &afina }, std::array<Pom*, 3> { &decai, &talla, &nota } })
        {
            auto r = fila (64);
            const int m = r.getWidth() / 3;
            for (auto* pm : filaPoms) pm->setBounds (r.removeFromLeft (m));
        }
        {
            auto r = fila (26); const int m = r.getWidth() / 2 - 3;
            btFilaNova.setBounds (r.removeFromLeft (m)); btTreuFila.setBounds (r.removeFromRight (m));
        }
        y += 6;
        titol (yEines);
        {
            auto r = fila (26); const int m = r.getWidth() / 2 - 3;
            msTemps.setBounds (r.removeFromLeft (m)); pctVel.setBounds (r.removeFromRight (m));
        }
        {
            auto r = fila (28); const int m = r.getWidth() / 2 - 3;
            btPrimer.setBounds (r.removeFromLeft (m)); btHumanitza.setBounds (r.removeFromRight (m));
        }
        {
            auto r = fila (26);
            swingPct.setBounds (r.removeFromLeft (r.getWidth() / 2 - 3)); r.removeFromLeft (6);
            swingPas.setBounds (r);
        }
        btSwing.setBounds (fila (28));
        {
            auto r = fila (28);
            copsRedoble.setBounds (r.removeFromLeft (juce::jmin (r.getWidth() / 2, copsRedoble.ampladaIdeal() + 6))); r.removeFromLeft (6);
            btRedoble.setBounds (r.removeFromLeft (r.getWidth() / 2 - 3)); r.removeFromLeft (6);
            btFlam.setBounds (r);
        }
        y += 6;
        titol (yRitmes);
        compassosRitme.setBounds (fila (26));
        btRitme.setBounds (fila (28));
        {
            auto r = fila (26);
            cadaFill.setBounds (r);
        }
        {
            auto r = fila (26);
            estilFill.setBounds (r);
        }
        {
            auto r = fila (28); const int m = r.getWidth() / 2 - 3;
            btLlarg.setBounds (r.removeFromLeft (m)); btFill.setBounds (r.removeFromRight (m));
        }
        y += 6;
        titol (yPads);
        btPadsOrdre.setBounds (fila (30));
        yAjudaPads = y;
        y += 64;
        cont.setSize (w, y + 10);
    }

    void paint (juce::Graphics& g) override { g.fillAll (tema::lateral); }

private:
    struct Contingut : public juce::Component
    {
        PanellBateria* p = nullptr;
        void paint (juce::Graphics& g) override { p->pintaContingut (g); }
    };

    void pintaContingut (juce::Graphics& g)
    {
        g.fillAll (tema::lateral);
        auto titol = [&] (int y, const juce::String& t)
        {
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (10.5f, true));
            g.drawText (t, 10, y, cont.getWidth() - 20, 16, juce::Justification::centredLeft);
            g.setColour (tema::linia);
            g.fillRect (10, y + 17, cont.getWidth() - 20, 1);
        };
        titol (yKit, "BATERIA · KIT"_ca);
        titol (yFila, "FILA: "_ca + nomFila.toUpperCase());
        titol (yEines, "EINES (ELS COPS TRIATS O TOT EL CLIP)"_ca);
        titol (yRitmes, "RITMES DE PARTIDA I FILLS"_ca);
        titol (yPads, "PADS DEL TECLAT"_ca);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.0f));
        const auto* t = const_cast<PanellBateria*> (this)->pista();
        g.drawFittedText (t != nullptr && t->padsPerOrdre
                              ? "Els pads (notes 36, 37, 38…) toquen les files per ordre: el pad 1 la primera fila, el 2 la segona…"_ca
                              : "Cada tecla o pad toca la fila de la seva nota (General MIDI). Les tecles del teclat també toquen."_ca,
                          juce::Rectangle<int> (10, yAjudaPads, cont.getWidth() - 20, 58), juce::Justification::topLeft, 4, 1.0f);
    }

    mibase::Pista* pista()
    {
        auto* pr = editor ? editor() : nullptr;
        if (pr == nullptr) return nullptr;
        auto* t = estat.pista (pr->pistaDeLEditor());
        return t != nullptr && t->tipus == "bateria" ? t : nullptr;
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override { actualitza(); }

    void actualitza()
    {
        cont.p = this;
        auto* t = pista();
        const bool hi = t != nullptr;
        for (auto* c : cont.getChildren()) c->setEnabled (hi);
        if (! hi) { nomFila = "-"; cont.repaint(); return; }
        estat.filaBateria = juce::jlimit (0, juce::jmax (0, (int) t->files.size() - 1), estat.filaBateria);
        btKit.setButtonText ("Kit: "_ca + juce::String::fromUTF8 ((t->kit.empty() ? std::string ("Intern") : t->kit).c_str()));
        const bool hiFila = estat.filaBateria < (int) t->files.size();
        for (auto* c : std::initializer_list<juce::Component*> { &btSo, &volum, &pan, &afina, &decai, &talla, &nota, &btTreuFila })
            c->setEnabled (hiFila);
        if (hiFila)
        {
            const auto& f = t->files[(size_t) estat.filaBateria];
            nomFila = juce::String::fromUTF8 ((f.nom.empty() ? mibase::bateria::nomTipus (f.tipus) : f.nom).c_str());
            auto so = juce::String::fromUTF8 (f.so.c_str());
            so = so.startsWith ("intern:") ? "intern"_ca : so.fromLastOccurrenceOf ("/", false, false).upToLastOccurrenceOf (".", false, false);
            btSo.setButtonText ("So: "_ca + so);
            volum.setValor (std::round (f.volumDb), true);
            pan.setValor (std::round (f.pan * 100.0), true);
            afina.setValor (std::round (f.afina), true);
            decai.setValor (std::round (f.decai * 100.0), true);
            talla.setValor (f.talla, true);
            nota.setValor (f.nota, true);
        }
        else nomFila = "-";
        btPadsOrdre.setToggleState (t->padsPerOrdre, juce::dontSendNotification);
        btPadsOrdre.setButtonText (t->padsPerOrdre ? "Pads per ordre de files"_ca : "Pads per nota (General MIDI)"_ca);
        cont.repaint();
    }

    void canviaPista (const std::function<void (mibase::Pista&)>& f, const juce::String& evolucio)
    {
        auto* t = pista();
        if (t == nullptr) return;
        estat.desfes.abans (estat.projecte);
        f (*t);
        estat.canviat (evolucio);
    }

    void ordre (const std::function<void (mibase::Clip&, std::vector<int>&, long long, long long, long long)>& f, const juce::String& nom)
    {
        auto* pr = editor ? editor() : nullptr;
        if (pr == nullptr || ! pr->teClip()) { avis ("Obre un clip de bateria (doble clic al clip)"_ca); return; }
        pr->ordreBateria (f, nom);
    }

    void avis (const juce::String& t) { estat.info = t; estat.avisa(); }

    void menuSo()
    {
        auto* t = pista();
        if (t == nullptr || estat.filaBateria >= (int) t->files.size()) return;
        const auto tipusFila = t->files[(size_t) estat.filaBateria].tipus;
        auto llista = std::make_shared<std::vector<std::pair<std::string, std::string>>>();   // so, nom
        juce::PopupMenu m;
        int id = 1;
        auto afegeix = [&] (juce::PopupMenu& menu, const mibase::bateria::Kit& k, const mibase::bateria::SoKit& s)
        {
            const auto so = k.nom == mibase::bateria::kitIntern ? s.fitxer : mibase::bateria::camiSo (k, s);
            llista->push_back ({ so, s.nom });
            menu.addItem (id++, juce::String::fromUTF8 (s.nom.c_str()), true, so == t->files[(size_t) estat.filaBateria].so);
        };
        for (auto& k : kits::llista())
        {
            juce::PopupMenu sub, delTipus;
            std::map<std::string, juce::PopupMenu> perTipus;
            for (auto& s : k.sons)
            {
                if (s.tipus == tipusFila) afegeix (delTipus, k, s);
                else afegeix (perTipus[s.tipus], k, s);
            }
            if (delTipus.getNumItems() > 0) sub.addSectionHeader (juce::String::fromUTF8 (mibase::bateria::nomTipus (tipusFila).c_str()));
            for (juce::PopupMenu::MenuItemIterator it (delTipus); it.next();) sub.addItem (it.getItem());
            if (! perTipus.empty()) sub.addSeparator();
            for (auto& [tp, menu] : perTipus) sub.addSubMenu (juce::String::fromUTF8 (mibase::bateria::nomTipus (tp).c_str()), menu);
            m.addSubMenu (juce::String::fromUTF8 (k.nom.c_str()) + (k.nom == t->kit ? "  (el de la pista)"_ca : juce::String()), sub);
        }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btSo), [this, llista] (int r)
        {
            if (r <= 0 || r > (int) llista->size()) return;
            const auto [so, nom] = (*llista)[(size_t) r - 1];
            canviaPista ([this, so = so] (mibase::Pista& p)
            {
                if (estat.filaBateria < (int) p.files.size()) p.files[(size_t) estat.filaBateria].so = so;
            }, "So de la fila: "_ca + juce::String::fromUTF8 (nom.c_str()));
        });
    }

    void menuFilaNova()
    {
        auto* t = pista();
        if (t == nullptr) return;
        juce::PopupMenu m;
        const auto& tp = mibase::bateria::tipus();
        for (int i = 0; i < (int) tp.size(); ++i) m.addItem (i + 1, juce::String::fromUTF8 (tp[(size_t) i].nom));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btFilaNova), [this] (int r)
        {
            if (r <= 0) return;
            const auto ti = mibase::bateria::tipus()[(size_t) r - 1];
            canviaPista ([this, ti] (mibase::Pista& p)
            {
                mibase::FilaBateria f;
                f.tipus = ti.id;
                f.nom = ti.nom;
                f.talla = ti.talla;
                // Una nota que no faci servir cap altra fila (la General MIDI si està lliure).
                auto lliure = [&p] (int n) { return std::none_of (p.files.begin(), p.files.end(), [n] (const mibase::FilaBateria& x) { return x.nota == n; }); };
                int n = ti.nota;
                while (! lliure (n) && n < 127) ++n;
                f.nota = n;
                // El primer so d'aquest tipus del kit de la pista (o l'intern).
                f.so = std::string ("intern:") + ti.id;
                if (auto k = kits::kit (p.kit); k && k->nom != mibase::bateria::kitIntern)
                    for (auto& s : k->sons) if (s.tipus == ti.id) { f.so = mibase::bateria::camiSo (*k, s); break; }
                p.files.push_back (f);
                estat.filaBateria = (int) p.files.size() - 1;
            }, "Fila nova al Drumpad: "_ca + juce::String::fromUTF8 (ti.nom));
        });
    }

    void menuRitmes()
    {
        juce::PopupMenu m;
        std::map<std::string, juce::PopupMenu> grups;
        const auto& rs = mibase::bateria::ritmes();
        for (int i = 0; i < (int) rs.size(); ++i)
        {
            const auto& r = rs[(size_t) i];
            const bool encaixa = r.num == estat.projecte.compasNum && r.den == estat.projecte.compasDen;
            grups[r.grup].addItem (i + 1, juce::String::fromUTF8 (r.nom) + (encaixa ? juce::String() : "  (" + juce::String (r.num) + "/" + juce::String (r.den) + ")"));
        }
        for (auto& [g, sub] : grups) m.addSubMenu (juce::String::fromUTF8 (g.c_str()), sub);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btRitme), [this] (int r)
        {
            if (r <= 0) return;
            auto* t = pista();
            if (t == nullptr) return;
            const auto& ritme = mibase::bateria::ritmes()[(size_t) r - 1];
            const auto files = t->files;
            const auto tpc = mibase::midi::ticksPerCompas (estat.projecte);
            const int triat = compassosRitme.getTriat();
            ordre ([&] (mibase::Clip& k, std::vector<int>& sel, long long, long long a, long long b)
            {
                // Omple la finestra del clip (o 1, 2 o 4 compassos des del seu començament). Treu el que hi havia en aquell tros.
                const int total = (int) juce::jmax (1LL, (b - a + tpc - 1) / tpc);
                const int n = triat == 0 ? total : juce::jmin (total, triat == 1 ? 1 : triat == 2 ? 2 : 4);
                const long long fi = a + n * tpc;
                k.notes.erase (std::remove_if (k.notes.begin(), k.notes.end(), [&] (const mibase::Nota& x) { return x.ini >= a && x.ini < fi; }), k.notes.end());
                for (auto& x : mibase::bateria::notesDeRitme (ritme, n, tpc, files, a)) k.notes.push_back (x);
                sel.clear();
                std::vector<int> cap;
                mibase::midi::ordena (k.notes, cap);
            }, "Ritme: "_ca + juce::String::fromUTF8 (ritme.nom));
        });
    }

    EstatEstudi& estat;
    std::function<PianoRoll*()> editor;
    juce::Viewport vista;
    Contingut cont;
    juce::uint32 ultimDesfes = 0;
    int llavors = 0;
    juce::String nomFila;
    int yKit = 0, yFila = 0, yEines = 0, yRitmes = 0, yPads = 0, yAjudaPads = 0;

    Boto btKit { "Kit", dibuix::Icona::Bateria, Boto::Estil::Secundari };
    Boto btSo { "So", std::nullopt, Boto::Estil::Secundari };
    Boto btDesaKit { "Desa el kit…"_ca, std::nullopt, Boto::Estil::Secundari };
    std::unique_ptr<DialegText> dialeg;

    /** 0.28: els sons de les files es copien a Kits\<nom>\ i es desa «kit.json». 0.31: també els interns i els ajustos de les files. */
    void desaKit()
    {
        auto* t = pista();
        if (t == nullptr) return;
        dialeg = std::make_unique<DialegText> ("Desa el kit"_ca, "Nom del kit nou (es crea una carpeta a Documents\\Mibase\\Kits amb els sons)."_ca, "Desa'l"_ca,
            [] (const juce::String& n) -> juce::String
            {
                if (n.trim().isEmpty()) return "Escriu un nom."_ca;
                if (kits::carpeta().getChildFile (juce::File::createLegalFileName (n.trim())).exists()) return "Ja hi ha un kit amb aquest nom."_ca;
                return {};
            },
            [this] (const juce::String& n)
            {
                // 0.31: la mateixa funció que la finestra de kits: sons (també els interns, com a WAV) i ajustos de cada fila
                auto* tt = pista();
                if (tt == nullptr) return;
                juce::String error;
                const auto desat = kits::desaDePista (*tt, n, error);
                if (desat.isEmpty()) { estat.info = error; estat.avisa(); return; }
                estat.desfes.abans (estat.projecte);
                kits::posaKit (*tt, desat.toStdString());
                estat.info = "Kit «"_ca + desat + "» desat amb els ajustos de cada fila."_ca + (error.isNotEmpty() ? " " + error : juce::String());
                estat.canviat ("Kit nou: «"_ca + desat + "»"_ca);
            });
        DialegText::mostra (*this, dialeg);
    }
    // 0.28: poms (com al samplejador i al Vocal Editor)
    Pom volum { "Vol", -40.0, 6.0, 0.0, true, [] (double v) { return juce::String ((int) std::lround (v)) + " dB"; } },
        pan { "Pan", -100.0, 100.0, 0.0, true, [] (double v) { const int s = (int) std::lround (v); return s == 0 ? juce::String ("C") : (s < 0 ? "L" : "R") + juce::String (std::abs (s)); } },
        afina { "Afina", -12.0, 12.0, 0.0, true, [] (double v) { const int s = (int) std::lround (v); return (s > 0 ? "+" : "") + juce::String (s) + " st"; } },
        decai { "Llarg", 5.0, 100.0, 100.0, false, [] (double v) { return juce::String ((int) std::lround (v)) + " %"; } },
        talla { "Tall", 0.0, 8.0, 0.0, false, [] (double v) { const int s = (int) std::lround (v); return s == 0 ? juce::String::fromUTF8 ("—") : juce::String (s); } },
        nota { "Nota", 0.0, 127.0, 36.0, false, [] (double v) { static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
                                                                 const int k = (int) std::lround (v); return juce::String (n[k % 12]) + juce::String (k / 12 - 1) + " (" + juce::String (k) + ")"; } };
    Boto btFilaNova { "+ Fila", std::nullopt, Boto::Estil::Secundari };
    Boto btTreuFila { "Treu la fila", std::nullopt, Boto::Estil::Secundari };
    CaixaNumero msTemps { "± ms"_ca, 0, 40, 8 }, pctVel { "± vel %"_ca, 0, 50, 10 };
    Boto btPrimer { "1r temps quiet", std::nullopt, Boto::Estil::Tecla };
    Boto btHumanitza { "Humanitza", std::nullopt, Boto::Estil::Primari };
    CaixaNumero swingPct { "Swing %", 50, 75, 58 };
    Segmentat swingPas { { "1/8", "1/16" } };
    Boto btSwing { "Aplica el swing", std::nullopt, Boto::Estil::Secundari };
    Segmentat copsRedoble { { "2", "3", "4" } };
    Boto btRedoble { "Redoble", std::nullopt, Boto::Estil::Secundari };
    Boto btFlam { "Flam", std::nullopt, Boto::Estil::Secundari };
    Segmentat compassosRitme { { "Tot el clip", "1", "2", "4" } };
    Boto btRitme { "Posa un ritme…"_ca, std::nullopt, Boto::Estil::Primari };
    Segmentat cadaFill { { "Cada 2", "Cada 4", "Cada 8" } };
    Segmentat estilFill { { "Caixa", "Toms", "Mixt" } };
    Boto btLlarg { "Mig compàs"_ca, std::nullopt, Boto::Estil::Tecla };
    Boto btFill { "Posa fills", std::nullopt, Boto::Estil::Secundari };
    Boto btPadsOrdre { "Pads", std::nullopt, Boto::Estil::Tecla };
};

/** La columna esquerra amb un editor obert: el panell d'harmonia o, si l'editor actiu és un Drumpad, el de la bateria. */
class PanellEditor : public juce::Component, private juce::ChangeListener
{
public:
    PanellEditor (EstatEstudi& e, juce::Component& harmonia, PanellBateria& bat, std::function<PianoRoll*()> actiu)
        : estat (e), panellHarmonia (harmonia), panellBateria (bat), editor (std::move (actiu))
    {
        addAndMakeVisible (panellHarmonia);
        addChildComponent (panellBateria);
        estat.addChangeListener (this);
        tria();
    }
    ~PanellEditor() override { estat.removeChangeListener (this); }
    void resized() override
    {
        panellHarmonia.setBounds (getLocalBounds());
        panellBateria.setBounds (getLocalBounds());
    }
private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { tria(); }
    void tria()
    {
        auto* pr = editor ? editor() : nullptr;
        const bool bat = pr != nullptr && pr->bateria();
        panellHarmonia.setVisible (! bat);
        panellBateria.setVisible (bat);
    }
    EstatEstudi& estat;
    juce::Component& panellHarmonia;
    PanellBateria& panellBateria;
    std::function<PianoRoll*()> editor;
};
