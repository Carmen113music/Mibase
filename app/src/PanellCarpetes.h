#pragma once

#include "VistesEstudi.h"

/** Panell de carpetes de pistes (0.11), a l'esquerra dels noms de pista (es mostra amb el botó de la carpeta).
    Com les carpetes d'un explorador: cada carpeta es plega o es desplega (i a la taula de treball es veu com una sola
    fila), s'amaga amb l'ull, i les pistes s'hi posen arrossegant-ne el nom. Clicar el nom d'una carpeta en tria totes
    les pistes (Ctrl o Maj: afegeix a les que ja hi havia). */
class PanellCarpetes : public SeccioEstudi
{
public:
    explicit PanellCarpetes (EstatEstudi& e) : SeccioEstudi (e)
    {
        btNova.setTooltip ("Carpeta nova (també: tria pistes amb Ctrl o Maj i prem Ctrl+G)"_ca);
        btNova.onClick = [this] { if (onCarpetaNova) onCarpetaNova ({}); };
        addAndMakeVisible (btNova);
    }

    /** Demana el nom i crea una carpeta amb aquestes pistes. */
    std::function<void (std::vector<int>)> onCarpetaNova;
    /** Demana el nom nou d'una carpeta. */
    std::function<void (std::string)> onCanviaNom;

    void resized() override { btNova.setBounds (8, 6, getWidth() - 16, 28); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (tema::lateral);
        const auto& p = estat.projecte;
        const auto files = filesPanell();
        g.saveState();
        g.reduceClipRegion (zonaLlista());
        for (int k = 0; k < (int) files.size(); ++k)
        {
            const auto& f = files[(size_t) k];
            auto r = rectFila (k);
            if (r.getBottom() < zonaLlista().getY() || r.getY() > getHeight()) continue;
            const bool sobre = k == filaDesti && arrossegant >= 0;
            if (f.pista < 0)
            {
                const auto* c = carpetaDe (f.carpeta);
                if (c == nullptr) continue;
                const auto membres = mibase::edicio::pistesDeCarpeta (p, c->id);
                bool triada = ! membres.empty();
                for (int i : membres) if (! estat.pistaEsTriada (i)) triada = false;
                g.setColour (sobre ? tema::accent.withAlpha (0.25f) : triada ? tema::triat : juce::Colours::white.withAlpha (0.03f));
                g.fillRoundedRectangle (r.toFloat().reduced (2.0f, 1.0f), 4.0f);
                // Fletxa (plegar), icona, nom, quantes, ull.
                juce::Path fl;
                const float cx = (float) r.getX() + 12.0f, cy = (float) r.getCentreY();
                if (c->plegada) fl.addTriangle (cx - 3, cy - 5, cx - 3, cy + 5, cx + 4, cy);
                else fl.addTriangle (cx - 5, cy - 3, cx + 5, cy - 3, cx, cy + 4);
                g.setColour (tema::suau);
                g.fillPath (fl);
                g.setColour (vintage::ledGroc.withAlpha (c->amagada ? 0.35f : 0.9f));
                g.strokePath (dibuix::icona (dibuix::Icona::Carpeta, juce::Rectangle<float> ((float) r.getX() + 22.0f, cy - 8.0f, 16.0f, 16.0f)),
                              juce::PathStrokeType (1.4f));
                g.setColour (c->amagada ? tema::suau : tema::text);
                g.setFont (dibuix::lletra (13.0f, true));
                g.drawText (dibuix::utf8 (c->nom) + "  (" + juce::String ((int) membres.size()) + ")",
                            r.withTrimmedLeft (44).withTrimmedRight (28), juce::Justification::centredLeft, true);
                pintaUll (g, rectUll (r), ! c->amagada);
            }
            else
            {
                const auto& t = p.pistes[(size_t) f.pista];
                auto rr = r.withTrimmedLeft (f.dins ? 26 : 8);
                g.setColour (sobre ? tema::accent.withAlpha (0.25f) : estat.pistaEsTriada (f.pista) ? tema::triat : juce::Colours::transparentBlack);
                g.fillRoundedRectangle (rr.toFloat().reduced (2.0f, 1.0f), 4.0f);
                g.setColour (EstatEstudi::colorPista (t));
                g.fillRect (rr.getX() + 4, rr.getY() + 6, 3, rr.getHeight() - 12);
                g.setColour (t.mut ? tema::suau : tema::text);
                g.setFont (dibuix::lletra (12.5f));
                g.drawText (dibuix::utf8 (t.nom), rr.withTrimmedLeft (14), juce::Justification::centredLeft, true);
            }
        }
        if (files.empty())
        {
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (12.0f));
            g.drawFittedText ("Encara no hi ha pistes."_ca, zonaLlista().reduced (10), juce::Justification::topLeft, 3);
        }
        // On caurà la pista arrossegada (fora de qualsevol carpeta).
        if (arrossegant >= 0 && filaDesti < 0 && zonaLlista().contains (ratoli))
        {
            g.setColour (tema::accent.withAlpha (0.5f));
            g.drawRect (zonaLlista().reduced (2), 1);
        }
        g.restoreState();
        g.setColour (tema::linia);
        g.fillRect (getWidth() - 1, 0, 1, getHeight());
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        arrossegant = -1;
        filaPremuda = filaA (e.getPosition());
        const auto files = filesPanell();
        if (filaPremuda < 0 || filaPremuda >= (int) files.size()) return;
        const auto f = files[(size_t) filaPremuda];
        const auto r = rectFila (filaPremuda);
        const bool afegeix = e.mods.isCommandDown() || e.mods.isShiftDown();

        if (f.pista < 0)
        {
            auto* c = carpetaDe (f.carpeta);
            if (c == nullptr) return;
            if (e.mods.isPopupMenu()) { menuCarpeta (c->id); return; }
            if (e.x < r.getX() + 22 || rectUll (r).contains (e.getPosition()))
            {
                comencaCanvi();
                restaura();
                if (auto* cc = carpetaDe (f.carpeta))
                {
                    if (e.x < r.getX() + 22) cc->plegada = ! cc->plegada;
                    else cc->amagada = ! cc->amagada;
                }
                acabaCanvi();
                return;
            }
            // Tria totes les pistes de la carpeta.
            const auto membres = mibase::edicio::pistesDeCarpeta (estat.projecte, c->id);
            if (! afegeix) { estat.pistesTriades.clear(); estat.capTriat(); }
            for (int i : membres) afegeixTriada (i);
            if (! membres.empty() && (! afegeix || estat.pistaTriada < 0)) estat.pistaTriada = membres.front();
            estat.avisa();
            return;
        }

        // Una pista: es tria (Ctrl o Maj: s'afegeix o es treu) i es pot arrossegar a una carpeta.
        if (e.mods.isCommandDown() || e.mods.isShiftDown())
        {
            const auto& id = estat.projecte.pistes[(size_t) f.pista].id;
            auto& v = estat.pistesTriades;
            if (auto it = std::find (v.begin(), v.end(), id); it != v.end()) v.erase (it);
            else afegeixTriada (f.pista);
        }
        else
        {
            estat.pistesTriades.clear();
            if (estat.pistaTriada != f.pista) estat.capTriat();
            estat.pistaTriada = f.pista;
        }
        pistaPremuda = f.pista;
        estat.avisa();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (pistaPremuda < 0 || e.getDistanceFromDragStart() < 5) return;
        arrossegant = pistaPremuda;
        ratoli = e.getPosition();
        const int k = filaA (ratoli);
        const auto files = filesPanell();
        filaDesti = (k >= 0 && k < (int) files.size() && files[(size_t) k].pista != arrossegant) ? k : -1;
        const auto desti = carpetaDesti();
        const auto* c = carpetaDe (desti);
        estat.info = "Deixa «"_ca + dibuix::utf8 (estat.projecte.pistes[(size_t) arrossegant].nom) + "» "
                   + (c != nullptr ? "a la carpeta «"_ca + dibuix::utf8 (c->nom) + "»" : "fora de les carpetes"_ca);
        estat.avisa();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (arrossegant >= 0 && arrossegant < estat.numPistes())
        {
            const auto desti = carpetaDesti();
            if (estat.projecte.pistes[(size_t) arrossegant].carpeta != desti)
            {
                const auto id = estat.projecte.pistes[(size_t) arrossegant].id;
                comencaCanvi();
                restaura();
                const int on = mibase::edicio::posaACarpeta (estat.projecte, arrossegant, desti);
                if (estat.pistaTriada == arrossegant) estat.pistaTriada = on;
                acabaCanvi();
                (void) id;
            }
        }
        arrossegant = pistaPremuda = filaDesti = -1;
        estat.info.clear();
        estat.avisa();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int k = filaA (e.getPosition());
        const auto files = filesPanell();
        if (k >= 0 && k < (int) files.size() && files[(size_t) k].pista < 0 && e.x >= rectFila (k).getX() + 22
            && ! rectUll (rectFila (k)).contains (e.getPosition()) && onCanviaNom)
            onCanviaNom (files[(size_t) k].carpeta);
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        const int total = (int) filesPanell().size() * altFila;
        desplY = juce::jlimit (0, juce::jmax (0, total - zonaLlista().getHeight()), desplY - juce::roundToInt (w.deltaY * 120.0f));
        repaint();
    }

private:
    struct Fila { std::string carpeta; int pista = -1; bool dins = false; };
    static constexpr int altFila = 28;

    std::vector<Fila> filesPanell() const
    {
        std::vector<Fila> r;
        const auto& p = estat.projecte;
        for (auto& c : p.carpetes)
        {
            r.push_back ({ c.id, -1, false });
            if (! c.plegada)
                for (int i : mibase::edicio::pistesDeCarpeta (p, c.id)) r.push_back ({ c.id, i, true });
        }
        for (int i = 0; i < (int) p.pistes.size(); ++i)
            if (p.pistes[(size_t) i].carpeta.empty()) r.push_back ({ {}, i, false });
        return r;
    }

    juce::Rectangle<int> zonaLlista() const { return getLocalBounds().withTrimmedTop (40); }
    juce::Rectangle<int> rectFila (int k) const { return { 4, zonaLlista().getY() + k * altFila - desplY, getWidth() - 8, altFila }; }
    static juce::Rectangle<int> rectUll (juce::Rectangle<int> r) { return r.removeFromRight (26).withSizeKeepingCentre (20, 16); }
    int filaA (juce::Point<int> p) const
    {
        if (! zonaLlista().contains (p)) return -1;
        return (p.y - zonaLlista().getY() + desplY) / altFila;
    }

    const mibase::Carpeta* carpetaDe (const std::string& id) const
    {
        for (auto& c : estat.projecte.carpetes) if (c.id == id && ! id.empty()) return &c;
        return nullptr;
    }
    mibase::Carpeta* carpetaDe (const std::string& id) { return mibase::edicio::carpeta (estat.projecte, id); }

    /** La carpeta on cau la pista que s'arrossega (buit: fora). */
    std::string carpetaDesti() const
    {
        const auto files = filesPanell();
        if (filaDesti < 0 || filaDesti >= (int) files.size()) return {};
        return files[(size_t) filaDesti].carpeta;
    }

    void afegeixTriada (int i)
    {
        if (i < 0 || i >= estat.numPistes()) return;
        const auto& id = estat.projecte.pistes[(size_t) i].id;
        if (std::find (estat.pistesTriades.begin(), estat.pistesTriades.end(), id) == estat.pistesTriades.end())
            estat.pistesTriades.push_back (id);
    }

    static void pintaUll (juce::Graphics& g, juce::Rectangle<int> r, bool obert)
    {
        auto f = r.toFloat();
        juce::Path ull;
        ull.startNewSubPath (f.getX(), f.getCentreY());
        ull.quadraticTo (f.getCentreX(), f.getY() - 2.0f, f.getRight(), f.getCentreY());
        ull.quadraticTo (f.getCentreX(), f.getBottom() + 2.0f, f.getX(), f.getCentreY());
        g.setColour (obert ? tema::text.withAlpha (0.8f) : tema::suau.withAlpha (0.6f));
        g.strokePath (ull, juce::PathStrokeType (1.2f));
        if (obert) g.fillEllipse (f.getCentreX() - 2.5f, f.getCentreY() - 2.5f, 5.0f, 5.0f);
        else g.drawLine (f.getX() + 2.0f, f.getBottom(), f.getRight() - 2.0f, f.getY(), 1.2f);
    }

    void menuCarpeta (const std::string& id)
    {
        juce::PopupMenu m;
        m.addItem (1, "Canvia el nom…"_ca);
        m.addItem (2, "Esborra la carpeta (les pistes es queden)"_ca);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this, id] (int r)
        {
            if (r == 1 && onCanviaNom) onCanviaNom (id);
            if (r == 2)
            {
                comencaCanvi();
                restaura();
                mibase::edicio::esborraCarpeta (estat.projecte, id);
                acabaCanvi();
            }
        });
    }

    Boto btNova { "Carpeta nova", dibuix::Icona::Mes, Boto::Estil::Discontinu };
    int desplY = 0, filaPremuda = -1, pistaPremuda = -1, arrossegant = -1, filaDesti = -1;
    juce::Point<int> ratoli;
};
