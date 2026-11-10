#pragma once

#include "VistesEstudi.h"
#include "DialegText.h"
#include "TempoPart.h"

/** Taula d'estructura (0.15): per provar ordres diferents de la cançó sense tocar la taula de treball.
    A dalt, les estructures (1, 2, 3, +); al mig, les peces de l'estructura (parts de la cançó, d'amplada proporcional
    a la durada) amb, a sota de cada una, una fila per pista amb els seus clips en petit (clicant-la, aquella pista no
    sona en aquella peça); a la dreta, les parts de la cançó per arrossegar-les a l'estructura. Una peça s'arrossega per
    canviar-la d'ordre i, deixant-la fora (o amb Supr), es treu. */
class TaulaEstructura : public SeccioEstudi
{
public:
    explicit TaulaEstructura (EstatEstudi& e) : SeccioEstudi (e) { setWantsKeyboardFocus (true); }

    int ampleDreta = 260;   // la columna de les parts (on hi ha el panell d'info a la taula de treball)
    /** Peça que sona ara (la dona el motor; -1: cap). */
    std::function<int()> onTram;

    // 0.16: zoom i desplaçament com a les altres pantalles.
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const float d = (std::abs (w.deltaY) > std::abs (w.deltaX) ? w.deltaY : w.deltaX);
        if (e.mods.isCommandDown())
        {
            const double cAbans = compasX (e.x);
            ppc = juce::jlimit (4.0, 600.0, pxPerCompas() * std::pow (1.25, d * 4.0));
            ajustat = false;
            despl = juce::jmax (0.0, cAbans * ppc - (e.x - zonaPeces().getX()));
        }
        else
        {
            despl = juce::jmax (0.0, despl - d * 300.0);
            if (ajustat) ppc = pxPerCompas();
            ajustat = false;
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (tema::fons);
        const auto& p = estat.projecte;
        pintaPestanyes (g);

        // Columna de la dreta: les parts de la cançó.
        auto dreta = zonaDreta();
        g.setColour (tema::lateral);
        g.fillRect (dreta);
        g.setColour (tema::linia);
        g.fillRect (dreta.getX(), dreta.getY(), 1, dreta.getHeight());
        dibuix::pintaEtiquetaSeccio (g, "Parts de la cançó"_ca, dreta.reduced (14, 12).removeFromTop (20));
        for (int i = 0; i < (int) p.parts.size(); ++i)
            pintaPart (g, rectPartDreta (i), i, 1.0f);
        if (p.parts.empty())
        {
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (12.0f));
            g.drawFittedText ("Encara no hi ha parts. A la taula de treball, fes una selecció a la meitat de dalt de la regla i, "
                              "amb el botó dret, «Fes-ne una part de la cançó»."_ca,
                              dreta.reduced (14, 44).removeFromTop (120), juce::Justification::topLeft, 7);
        }

        // Les peces de l'estructura.
        const auto* e = editada();
        auto zona = zonaPeces();
        g.setColour (tema::linia);
        g.fillRect (zonaNoms().getRight(), zona.getY(), 1, zona.getHeight());
        pintaNoms (g);
        pintaRegla (g);
        if (e == nullptr || e->peces.empty())
        {
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (13.0f));
            g.drawFittedText (e == nullptr ? "Crea una estructura amb el botó +."_ca
                                           : "Arrossega aquí les parts de la cançó de la dreta, en l'ordre que vulguis."_ca,
                              zona.reduced (20), juce::Justification::centred, 3);
        }
        // 0.21: amb zoom, les peces no poden passar per sobre de la columna de les parts.
        g.saveState();
        g.reduceClipRegion (zonaEsquerra().withTrimmedLeft (zonaNoms().getRight() + 1));
        if (e != nullptr)
            for (int k = 0; k < (int) e->peces.size(); ++k)
                pintaPeca (g, k);

        // El capçal, si l'estructura sona.
        if (e != nullptr && onTram)
        {
            const int k = onTram();
            if (k >= 0 && k < (int) e->peces.size())
            {
                const int ip = indexPart (e->peces[(size_t) k].part);
                if (ip >= 0)
                {
                    const auto r = rectPeca (k);
                    const auto& part = p.parts[(size_t) ip];
                    const double s0 = mibase::edicio::segonsDeCompas (p, part.de), s1 = mibase::edicio::segonsDeCompas (p, part.a);
                    const double f = juce::jlimit (0.0, 1.0, (estat.capcal - s0) / juce::jmax (1e-6, s1 - s0));
                    const float x = (float) r.getX() + (float) (f * (r.getWidth() + 3));
                    g.setColour (tema::text);
                    g.fillRect (x - 0.5f, (float) zona.getY(), 1.5f, (float) zona.getHeight());
                }
            }
        }

        // On anirà la peça que s'arrossega.
        if (arrossegant.actiu && zona.contains (ratoli))
        {
            const int x = xInsercio (indexInsercio (ratoli.x));
            g.setColour (tema::accent);
            g.fillRect (x - 1, zona.getY() + 4, 3, zona.getHeight() - 8);
        }
        g.restoreState();
        if (arrossegant.actiu)
        {
            const int i = indexPart (arrossegant.part);
            if (i >= 0) pintaPart (g, juce::Rectangle<int> (110, 46).withCentre (ratoli), i, 0.85f);
        }
    }

    void resized() override {}

    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        arrossegant = {};
        inici = e.getPosition();
        auto& p = estat.projecte;

        // Pestanyes de les estructures.
        for (int i = 0; i <= (int) p.estructures.size(); ++i)
            if (rectPestanya (i).contains (e.getPosition()))
            {
                if (i == (int) p.estructures.size()) { novaEstructura(); return; }
                if (e.mods.isPopupMenu()) { menuEstructura (p.estructures[(size_t) i].id); return; }
                comencaCanvi();
                restaura();
                p.estructuraActiva = p.estructures[(size_t) i].id;
                acabaCanvi();
                return;
            }

        // Una part de la dreta: s'arrossega a l'estructura (0.28: botó dret = tempo i compàs de la part).
        for (int i = 0; i < (int) p.parts.size(); ++i)
            if (rectPartDreta (i).contains (e.getPosition()))
            {
                if (e.mods.isPopupMenu()) { menuPart (p.parts[(size_t) i].id); return; }
                arrossegant = { true, p.parts[(size_t) i].id, -1 };
                return;
            }

        auto* es = editada();
        if (es == nullptr) return;
        for (int k = 0; k < (int) es->peces.size(); ++k)
        {
            const auto r = rectPeca (k).getIntersection (zonaEsquerra());
            if (! r.contains (e.getPosition())) continue;
            pecaTriada = k;
            if (e.mods.isPopupMenu() && e.y < r.getY() + altCap) { menuPart (es->peces[(size_t) k].part); return; }
            if (e.y < r.getY() + altCap)
            {
                // Capçal de la peça: s'arrossega per canviar-la d'ordre.
                arrossegant = { true, es->peces[(size_t) k].part, k };
                repaint();
                return;
            }
            // Fila d'una pista: aquella pista sona o no en aquesta peça.
            const int fila = filaPista (e.y);
            if (fila >= 0 && fila < estat.numPistes())
            {
                comencaCanvi();
                restaura();
                auto* ee = editada();
                auto& sil = ee->peces[(size_t) k].silencis;
                const auto& id = p.pistes[(size_t) fila].id;
                if (auto it = std::find (sil.begin(), sil.end(), id); it != sil.end()) sil.erase (it);
                else sil.push_back (id);
                acabaCanvi();
            }
            repaint();
            return;
        }
        pecaTriada = -1;
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! arrossegant.actiu) return;
        ratoli = e.getPosition();
        repaint();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! arrossegant.actiu) return;
        const auto a = arrossegant;
        arrossegant = {};
        const bool mogut = e.getDistanceFromDragStart() > 4;
        if (! mogut) { repaint(); return; }
        auto& p = estat.projecte;
        comencaCanvi();
        restaura();
        auto* es = editada();
        if (es == nullptr)
        {
            // Encara no hi ha cap estructura: se'n crea una.
            p.estructuraActiva = mibase::edicio::creaEstructura (p, "1");
            es = editada();
        }
        const bool dins = zonaPeces().contains (e.getPosition());
        if (a.desDe >= 0)
        {
            auto peca = es->peces[(size_t) a.desDe];
            es->peces.erase (es->peces.begin() + a.desDe);
            if (dins)
            {
                int on = indexInsercio (e.x);
                if (on > a.desDe) --on;
                es->peces.insert (es->peces.begin() + juce::jlimit (0, (int) es->peces.size(), on), peca);
            }
            pecaTriada = -1;
        }
        else if (dins)
        {
            const int on = indexInsercio (e.x);
            es->peces.insert (es->peces.begin() + juce::jlimit (0, (int) es->peces.size(), on), mibase::PecaEstructura { a.part, {} });
        }
        acabaCanvi();
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const auto& p = estat.projecte;
        for (int i = 0; i < (int) p.estructures.size(); ++i)
            if (rectPestanya (i).contains (e.getPosition())) { canviaNom (p.estructures[(size_t) i].id); return; }
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k.getKeyCode() == 'Z' || k.getTextCharacter() == 'z' || k.getTextCharacter() == 'Z')
        {
            const auto* es = editada();
            if (k.getModifiers().isShiftDown() && es != nullptr && pecaTriada >= 0 && pecaTriada < (int) es->peces.size())
            {
                // Maj+Z: la peça triada a la pantalla.
                ajustat = true;
                double x = 0.0;
                for (int i = 0; i < pecaTriada; ++i) x += compassosPart (es->peces[(size_t) i].part);
                ppc = juce::jlimit (4.0, 600.0, (double) zonaPeces().getWidth() * 0.9 / compassosPart (es->peces[(size_t) pecaTriada].part));
                ajustat = false;
                despl = juce::jmax (0.0, x * ppc - zonaPeces().getWidth() * 0.05);
            }
            else { ajustat = true; despl = 0.0; }   // Z: tota l'estructura
            repaint();
            return true;
        }
        auto* es = editada();
        if ((k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) && es != nullptr
            && pecaTriada >= 0 && pecaTriada < (int) es->peces.size())
        {
            comencaCanvi();
            restaura();
            editada()->peces.erase (editada()->peces.begin() + pecaTriada);
            pecaTriada = -1;
            acabaCanvi();
            return true;
        }
        return false;
    }

private:
    struct Arrossega { bool actiu = false; std::string part; int desDe = -1; };
    static constexpr int altPestanyes = 44, altCap = 48, ampleNomsPistes = 130, altRegla = 24;
    double ppc = 40.0, despl = 0.0;
    bool ajustat = true;   // tota l'estructura cap a la pantalla (fins que es fa zoom)

    mibase::Estructura* editada() { return mibase::edicio::estructura (estat.projecte, estat.projecte.estructuraActiva); }
    const mibase::Estructura* editada() const
    {
        for (auto& e : estat.projecte.estructures) if (e.id == estat.projecte.estructuraActiva) return &e;
        return nullptr;
    }
    int indexPart (const std::string& id) const
    {
        for (int i = 0; i < (int) estat.projecte.parts.size(); ++i) if (estat.projecte.parts[(size_t) i].id == id) return i;
        return -1;
    }

    juce::Rectangle<int> zonaDreta() const { return getLocalBounds().removeFromRight (ampleDreta); }
    juce::Rectangle<int> zonaEsquerra() const { return getLocalBounds().withTrimmedRight (ampleDreta); }
    juce::Rectangle<int> zonaNoms() const { return zonaEsquerra().withTrimmedTop (altPestanyes).removeFromLeft (ampleNomsPistes); }
    juce::Rectangle<int> zonaPeces() const { return zonaEsquerra().withTrimmedTop (altPestanyes).withTrimmedLeft (ampleNomsPistes).reduced (10, 10); }
    juce::Rectangle<int> rectPestanya (int i) const { return { 12 + i * 46, 8, 40, 28 }; }
    juce::Rectangle<int> rectPartDreta (int i) const
    {
        auto d = zonaDreta().reduced (14, 0);
        const int w = (d.getWidth() - 8) / 2;
        return { d.getX() + (i % 2) * (w + 8), 44 + (i / 2) * 58, w, 50 };
    }

    double compassosPart (const std::string& id) const
    {
        const int i = indexPart (id);
        return i < 0 ? 1.0 : juce::jmax (0.25, estat.projecte.parts[(size_t) i].a - estat.projecte.parts[(size_t) i].de);
    }
    double totalCompassos() const
    {
        double t = 0.0;
        if (const auto* e = editada()) for (auto& pc : e->peces) t += compassosPart (pc.part);
        return t;
    }
    double pxPerCompas() const
    {
        if (! ajustat) return ppc;
        const double t = totalCompassos();
        return t <= 0.0 ? 40.0 : juce::jlimit (4.0, 600.0, (double) zonaPeces().getWidth() / t);
    }
    double compasX (int x) const { return ((double) (x - zonaPeces().getX()) + despl) / pxPerCompas(); }
    juce::Rectangle<int> rectPeca (int k) const
    {
        const auto* e = editada();
        const auto z = zonaPeces();
        double x = 0.0;
        for (int i = 0; i < k; ++i) x += compassosPart (e->peces[(size_t) i].part);
        const double px = pxPerCompas();
        const int xa = z.getX() + (int) std::round (x * px - despl);
        const int w = (int) std::round (compassosPart (e->peces[(size_t) k].part) * px);
        return { xa, z.getY() + altRegla, juce::jmax (8, w - 3), z.getHeight() - altRegla };
    }
    int altFila() const
    {
        const int n = juce::jmax (1, estat.numPistes());
        return juce::jlimit (8, 28, (zonaPeces().getHeight() - altRegla - altCap - 6) / n);
    }
    int yFila (int i) const { return zonaPeces().getY() + altRegla + altCap + 6 + i * altFila(); }
    int filaPista (int y) const { const int d = y - yFila (0); return d < 0 ? -1 : d / altFila(); }

    int indexInsercio (int x) const
    {
        const auto* e = editada();
        if (e == nullptr) return 0;
        for (int k = 0; k < (int) e->peces.size(); ++k)
            if (x < rectPeca (k).getCentreX()) return k;
        return (int) e->peces.size();
    }
    int xInsercio (int k) const
    {
        const auto* e = editada();
        if (e == nullptr || e->peces.empty()) return zonaPeces().getX();
        if (k >= (int) e->peces.size()) return rectPeca ((int) e->peces.size() - 1).getRight() + 1;
        return rectPeca (k).getX() - 2;
    }

    juce::Colour colorPart (int i) const
    {
        static const juce::Colour c[] = { juce::Colour (0xffe8743b), juce::Colour (0xff59d0d6), juce::Colour (0xffb18cff), juce::Colour (0xff7fd67a),
                                          juce::Colour (0xffff8fc4), juce::Colour (0xfff5b445), juce::Colour (0xff6fa8ff), juce::Colour (0xffd9534f) };
        const auto& x = estat.projecte.parts[(size_t) i];
        return x.rgb != 0 ? juce::Colour (0xff000000u | x.rgb) : c[(size_t) (i % 8)];
    }

    void pintaPart (juce::Graphics& g, juce::Rectangle<int> r, int i, float alfa) const
    {
        const auto& x = estat.projecte.parts[(size_t) i];
        const auto col = colorPart (i);
        g.setColour (col.withAlpha (0.8f * alfa));
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.setFont (dibuix::lletra (13.0f, true));
        g.drawText (dibuix::utf8 (x.nom), r.getX() + 8, r.getY() + 6, r.getWidth() - 14, 18, juce::Justification::centredLeft, true);
        g.setFont (dibuix::lletra (11.0f));
        g.drawText (textCompassos (x.a - x.de), r.getX() + 8, r.getY() + 26, r.getWidth() - 14, 16, juce::Justification::centredLeft, true);
    }

    static juce::String textCompassos (double c)
    {
        const bool sencer = std::abs (c - std::round (c)) < 1e-6;
        return (sencer ? juce::String ((int) std::round (c)) : juce::String (c, 2)) + (std::abs (c - 1.0) < 1e-6 ? " compàs"_ca : " compassos"_ca);
    }

    void pintaPestanyes (juce::Graphics& g) const
    {
        const auto& p = estat.projecte;
        g.setFont (dibuix::lletra (13.0f, true));
        for (int i = 0; i <= (int) p.estructures.size(); ++i)
        {
            const auto r = rectPestanya (i).toFloat();
            const bool activa = i < (int) p.estructures.size() && p.estructures[(size_t) i].id == p.estructuraActiva;
            g.setColour (activa ? tema::accent : tema::superficie2);
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (activa ? juce::Colours::black : tema::text);
            g.drawText (i < (int) p.estructures.size() ? juce::String (i + 1) : juce::String ("+"), r, juce::Justification::centred);
        }
        const auto* e = editada();
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (12.0f));
        const int x = rectPestanya ((int) p.estructures.size()).getRight() + 14;
        g.drawText (e != nullptr ? "Estructura «"_ca + dibuix::utf8 (e->nom) + "» · sona en fer play (doble clic: nom; botó dret: més)"_ca
                                 : "Sona la línia de temps tal com és (lineal)"_ca,
                    x, 8, zonaEsquerra().getWidth() - x - 8, 28, juce::Justification::centredLeft, true);
    }

    /** Regla de l'estructura (compassos des de 1) i graella darrere les peces. */
    void pintaRegla (juce::Graphics& g) const
    {
        const auto z = zonaPeces();
        const auto& p = estat.projecte;
        const double px = pxPerCompas();
        g.saveState();
        g.reduceClipRegion (z);
        g.setColour (tema::lateral);
        g.fillRect (z.withHeight (altRegla));
        int cada = 1;
        while (px * cada < 30.0) cada *= 2;
        const int c0 = juce::jmax (0, (int) std::floor (despl / px)), c1 = (int) std::ceil ((despl + z.getWidth()) / px);
        for (int c = c0; c <= c1; ++c)
        {
            const float x = (float) (z.getX() + c * px - despl);
            g.setColour (juce::Colours::white.withAlpha (c % cada == 0 ? 0.15f : 0.07f));
            g.fillRect (x, (float) z.getY() + (c % cada == 0 ? 0.0f : altRegla * 0.6f), 1.0f, (float) z.getHeight());
            if (c % cada == 0)
            {
                g.setColour (tema::textMig);
                g.setFont (dibuix::mono (11.0f));
                g.drawText (juce::String (c + 1), juce::Rectangle<float> (x + 4.0f, (float) z.getY(), 40.0f, (float) altRegla), juce::Justification::centredLeft);
            }
            // Temps, si n'hi ha lloc.
            if (px / juce::jmax (1, p.compasNum) >= 10.0)
                for (int t = 1; t < p.compasNum; ++t)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.05f));
                    g.fillRect (x + (float) (t * px / p.compasNum), (float) z.getY() + altRegla, 1.0f, (float) z.getHeight() - altRegla);
                }
        }
        g.restoreState();
    }

    void pintaNoms (juce::Graphics& g) const
    {
        const auto& p = estat.projecte;
        const int h = altFila();
        g.setFont (dibuix::lletra (juce::jmin (12.0f, (float) h - 2.0f)));
        for (int i = 0; i < estat.numPistes(); ++i)
        {
            const auto& t = p.pistes[(size_t) i];
            g.setColour (EstatEstudi::colorPista (t));
            g.fillRect (8, yFila (i) + 2, 3, h - 4);
            g.setColour (tema::textMig);
            g.drawText (dibuix::utf8 (t.nom), 16, yFila (i), ampleNomsPistes - 20, h, juce::Justification::centredLeft, true);
        }
    }

    void pintaPeca (juce::Graphics& g, int k) const
    {
        const auto& p = estat.projecte;
        const auto& pc = editada()->peces[(size_t) k];
        const int ip = indexPart (pc.part);
        if (ip < 0) return;
        const auto& part = p.parts[(size_t) ip];
        const auto r = rectPeca (k);
        const bool esMou = arrossegant.actiu && arrossegant.desDe == k;
        // Capçal amb el nom (el més important).
        auto cap = r.withHeight (altCap).toFloat();
        g.setColour (colorPart (ip).withAlpha (esMou ? 0.35f : 0.9f));
        g.fillRoundedRectangle (cap, 6.0f);
        if (k == pecaTriada) { g.setColour (tema::text); g.drawRoundedRectangle (cap.reduced (1.0f), 6.0f, 2.0f); }
        g.setColour (juce::Colours::black.withAlpha (0.85f));
        g.setFont (dibuix::lletra (14.0f, true));
        g.drawText (dibuix::utf8 (part.nom), cap.reduced (8.0f, 4.0f).toNearestInt(), juce::Justification::centredLeft, true);
        // Les pistes: els clips en petit, proporcionals, dins el tros de la part.
        const double t0 = mibase::edicio::segonsDeCompas (p, part.de), t1 = mibase::edicio::segonsDeCompas (p, part.a);
        const double pxs = (double) r.getWidth() / juce::jmax (1e-6, t1 - t0);
        const int h = altFila();
        for (int i = 0; i < estat.numPistes(); ++i)
        {
            const auto& t = p.pistes[(size_t) i];
            const bool calla = std::find (pc.silencis.begin(), pc.silencis.end(), t.id) != pc.silencis.end();
            auto fila = juce::Rectangle<int> (r.getX(), yFila (i) + 1, r.getWidth(), h - 2);
            g.setColour (juce::Colours::white.withAlpha (0.03f));
            g.fillRect (fila);
            const auto col = calla ? tema::suau.withAlpha (0.25f) : EstatEstudi::colorPista (t).withAlpha (t.mut ? 0.3f : 0.85f);
            for (auto& c : t.clips)
            {
                const double a = juce::jmax (t0, c.iniciVisible()), b = juce::jmin (t1, c.fiVisible());
                if (b <= a || c.mut) continue;
                g.setColour (col);
                g.fillRect (juce::Rectangle<float> ((float) (r.getX() + (a - t0) * pxs), (float) fila.getY(),
                                                    juce::jmax (2.0f, (float) ((b - a) * pxs)), (float) fila.getHeight()));
            }
            if (calla)
            {
                g.setColour (tema::suau);
                g.drawLine ((float) fila.getX() + 2.0f, (float) fila.getCentreY(), (float) fila.getRight() - 2.0f, (float) fila.getCentreY(), 1.0f);
            }
        }
    }

    void novaEstructura()
    {
        comencaCanvi();
        restaura();
        auto& p = estat.projecte;
        p.estructuraActiva = mibase::edicio::creaEstructura (p, std::to_string (p.estructures.size() + 1));
        acabaCanvi ("Estructura nova"_ca);
    }

    /** 0.28: botó dret sobre una part: el seu tempo i compàs. */
    void menuPart (const std::string& id)
    {
        const auto* x = mibase::edicio::part (estat.projecte, id);
        if (x == nullptr) return;
        juce::PopupMenu m;
        tempoPart::afegeix (m, estat.projecte, *x);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this, id] (int r) { tempoPart::tracta (r, *this, estat, id); });
    }

    void menuEstructura (const std::string& id)
    {
        juce::PopupMenu m;
        m.addItem (1, "Canvia el nom…"_ca);
        m.addItem (2, "Duplica-la");
        m.addItem (3, "Esborra-la");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this, id] (int r)
        {
            if (r == 1) { juce::Component::SafePointer<TaulaEstructura> jo (this); juce::MessageManager::callAsync ([jo, id] { if (jo) jo->canviaNom (id); }); return; }
            if (r == 0) return;
            comencaCanvi();
            restaura();
            auto& p = estat.projecte;
            if (r == 2)
                if (auto* e = mibase::edicio::estructura (p, id))
                {
                    auto copia = *e;
                    const auto nou = mibase::edicio::creaEstructura (p, copia.nom + " (2)");
                    auto* n = mibase::edicio::estructura (p, nou);
                    n->peces = copia.peces;
                    p.estructuraActiva = nou;
                }
            if (r == 3)
            {
                p.estructures.erase (std::remove_if (p.estructures.begin(), p.estructures.end(),
                                                     [&id] (const mibase::Estructura& e) { return e.id == id; }), p.estructures.end());
                if (p.estructuraActiva == id) p.estructuraActiva.clear();
            }
            acabaCanvi();
        });
    }

    void canviaNom (const std::string& id)
    {
        auto* e = mibase::edicio::estructura (estat.projecte, id);
        auto* pare = getParentComponent();
        if (e == nullptr || pare == nullptr) return;
        dialeg = std::make_unique<DialegText> ("Nom de l'estructura"_ca, juce::String(), "Canvia"_ca,
            [] (const juce::String& s) { return s.trim().isEmpty() ? "Escriu un nom"_ca : juce::String(); },
            [this, id] (const juce::String& s)
            {
                comencaCanvi();
                restaura();
                if (auto* x = mibase::edicio::estructura (estat.projecte, id)) x->nom = s.trim().toStdString();
                acabaCanvi();
            }, dibuix::utf8 (e->nom));
        DialegText::mostra (*pare, dialeg);
    }

    Arrossega arrossegant;
    juce::Point<int> inici, ratoli;
    int pecaTriada = -1;
    std::unique_ptr<DialegText> dialeg;
};
