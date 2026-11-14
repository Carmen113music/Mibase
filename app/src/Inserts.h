#pragma once

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

    inline juce::String titol (const mibase::Insert& x) { return juce::String::fromUTF8 (x.nom.c_str()); }

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
        plugins->obreFinestra (x.id, titol (x));
    }

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
        m.addItem (1, mibase::esInsertVeu (x) ? "Obre el Vocal Editor"_ca : "Obre la finestra del plugin"_ca);
        m.addItem (2, x.actiu ? "Apaga'l (el so hi passa de llarg)"_ca : "Encén-lo"_ca);
        m.addItem (6, "Canvia'l per un altre…"_ca);
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
    inline void pinta (juce::Graphics& g, juce::Rectangle<int> r, const mibase::Insert* x, bool primeraBuida, bool petit)
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
        g.drawFittedText (titol (*x), z.nom, juce::Justification::centredLeft, 1, 0.8f);
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

    inline juce::String ajuda() { return "Llumeta: encén o apaga · nom: obre el plugin · fletxes: canvia'l o treu-lo · arrossega'l fora: el treus · botó dret: mou o treu"_ca; }
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
        inserts::pinta (g, getLocalBounds(), x, v != nullptr && index == (int) v->size(), false);
        if (fora && x != nullptr)
        {
            // 0.31: s'està arrossegant fora: si es deixa anar, es treu
            g.setColour (juce::Colour (0xffe0524a).withAlpha (0.35f));
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (11.0f, true));
            g.drawText ("Deixa'l anar per treure'l"_ca, getLocalBounds(), juce::Justification::centred);
        }
        setTooltip (x != nullptr ? inserts::ajuda() : "Clica per posar-hi un efecte VST3"_ca);
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
