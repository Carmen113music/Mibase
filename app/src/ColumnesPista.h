#pragma once

#include "VistesEstudi.h"
#include "Inserts.h"

/** 1.19: columnes a l'estil de Pro Tools al costat dels noms de les pistes (petició de l'Alfredo): els inserts, els
    enviaments i la secció E/S (entrada, sortida, volum i panorama) de totes les pistes alhora, sense haver d'obrir la
    taula de mescles. Es mostren o s'amaguen amb el botó del costat de «Pistes». */
class ColumnesPista : public SeccioEstudi
{
public:
    explicit ColumnesPista (EstatEstudi& e) : SeccioEstudi (e) {}

    GestorPlugins* plugins = nullptr;
    int altTitol = 34;   // l'alçada de la regla: a dalt hi van els títols de les columnes

    static constexpr int ampleInserts = 122, ampleEnvia = 118, ampleES = 116, separacio = 4;
    static constexpr int altSlot = 17, gapSlot = 2;

    /** Alguna pista (o el màster) té la primera columna plena (3 inserts)? Llavors surt la segona, a totes alhora. */
    bool ambSegonaColumna() const
    {
        if (estat.projecte.insertsMaster.size() >= 3) return true;
        for (const auto& t : estat.projecte.pistes) if (t.inserts.size() >= 3) return true;
        return false;
    }
    int ampladaIdeal() const
    {
        return ampleInserts * (ambSegonaColumna() ? 2 : 1) + ampleEnvia + ampleES + separacio * 5;
    }
    std::function<void()> onAmpladaCanviada;

    void paint (juce::Graphics& g) override
    {
        g.fillAll (tema::lateral);
        const auto& p = estat.projecte;
        const auto c = columnes();

        // Títols (a l'altura de la regla).
        {
            auto r = getLocalBounds().removeFromTop (altTitol);
            vintage::panell (g, r.toFloat());
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (10.5f, true));
            g.drawText ("INSERTS A-C", c.insA.withY (0).withHeight (altTitol), juce::Justification::centred);
            if (! c.insB.isEmpty()) g.drawText ("INSERTS D-F", c.insB.withY (0).withHeight (altTitol), juce::Justification::centred);
            g.drawText ("ENVIAMENTS A-C", c.envia.withY (0).withHeight (altTitol), juce::Justification::centred);
            g.drawText ("E/S", c.es.withY (0).withHeight (altTitol), juce::Justification::centred);
        }

        g.saveState();
        g.reduceClipRegion (0, altTitol, getWidth(), getHeight() - altTitol);
        // El màster: els seus inserts i el volum.
        {
            const int y = altTitol + estat.yMaster();
            auto r = juce::Rectangle<int> (0, y, getWidth(), estat.altPista);
            g.setColour (juce::Colour (0x40f5b445));
            g.fillRect (r);
            g.setColour (tema::guitarra.withAlpha (0.55f));
            g.fillRect (0, r.getBottom() - 2, getWidth(), 2);
            pintaInserts (g, -1, y);
            const int nf = numFiles();
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (10.0f));
            if (nf >= 2) g.drawText ("Màster"_ca, slot (c.envia, y, 0), juce::Justification::centred);
            pintaVolPan (g, slot (c.es, y, nf - 1), p.masterDb, 0.0, true);
        }
        for (int i = 0; i < estat.numPistes(); ++i)
        {
            const auto& t = p.pistes[(size_t) i];
            const int y = altTitol + estat.yPista (i);
            if (estat.altDe (i) == 0 || y > getHeight() || y + estat.altDe (i) < altTitol) continue;
            auto r = juce::Rectangle<int> (0, y, getWidth(), estat.altDe (i));
            g.setColour (estat.pistaEsTriada (i) ? tema::triat.withAlpha (0.55f) : EstatEstudi::colorPista (t).withAlpha (0.10f));
            g.fillRect (r);
            g.setColour (tema::linia);
            g.fillRect (0, r.getBottom() - 1, getWidth(), 1);
            if (estat.filaCarpeta (i) || mibase::esAcords (t)) continue;
            pintaInserts (g, i, y);
            pintaEnvia (g, t, y);
            pintaES (g, t, y);
        }
        g.restoreState();
        // Separadors verticals entre columnes
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        for (auto x : { c.insA.getRight() + 2, c.insB.isEmpty() ? -10 : c.insB.getRight() + 2, c.envia.getRight() + 2 })
            if (x > 0) g.fillRect (x, 0, 1, getHeight());
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        arrossegaEnvia = -1;
        const auto lloc = troba (e.getPosition());
        if (lloc.fila == -2) return;
        const int i = lloc.fila;   // -1 = màster
        const auto pos = e.getPosition();
        const bool menu = e.mods.isPopupMenu();
        auto& p = estat.projecte;

        if (lloc.col == Col::Inserts)
        {
            inserts::clic (estat, plugins, i, lloc.k, lloc.r, pos, menu, this);
            estat.info = inserts::ajuda();
            estat.avisa();
            return;
        }
        if (lloc.col == Col::VolPan)
        {
            const bool pan = i >= 0 && pos.x >= lloc.r.getCentreX();
            obrePopup (i, pan, lloc.r);
            return;
        }
        if (i < 0) return;
        auto& t = p.pistes[(size_t) i];
        if (lloc.col == Col::Envia)
        {
            if (lloc.k >= (int) t.enviaments.size() || menu) { menuEnviaments (i); return; }
            if (pos.x < lloc.r.getX() + 15)
            {
                comencaCanvi();
                auto& x = estat.projecte.pistes[(size_t) i].enviaments[(size_t) lloc.k];
                x.actiu = ! (x.actiu && ! x.mut);
                x.mut = false;
                canviFet = true;
                acabaCanvi();
                return;
            }
            comencaCanvi();
            arrossegaEnvia = lloc.k;
            filaEnvia = i;
            valor0 = EstatEstudi::posFader (t.enviaments[(size_t) lloc.k].nivellDb);
            ultimaY = e.position.y;
            estat.info = "Enviament: arrossega amunt i avall = nivell (Maj: més fi; doble clic: 0 dB) · llumeta = encén/apaga · botó dret = més opcions"_ca;
            estat.avisa();
            return;
        }
        if (lloc.col == Col::Entrada)
        {
            if (t.tipus != "audio")
            {
                estat.info = mibase::esMidi (t) ? "El teclat MIDI de la pista es tria al botó E/S de la capçalera"_ca
                                                : "Aquesta pista no grava de cap entrada"_ca;
                estat.avisa();
                return;
            }
            juce::Component::SafePointer<ColumnesPista> jo (this);
            EstatEstudi::menuEntrades (estat.nomsEntrades, t, 1000).showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [jo, i] (int r)
            {
                if (jo == nullptr || r <= 0 || i >= jo->estat.numPistes()) return;
                jo->comencaCanvi();
                if (! EstatEstudi::aplicaEntrada (jo->estat.projecte.pistes[(size_t) i], r, 1000)) return;
                jo->canviFet = true;
                jo->acabaCanvi ("Pista «"_ca + dibuix::utf8 (jo->estat.projecte.pistes[(size_t) i].nom) + "»: entrada "_ca
                                + EstatEstudi::textEntrada (jo->estat.projecte.pistes[(size_t) i]));
            });
            return;
        }
        if (lloc.col == Col::Sortida)
        {
            juce::PopupMenu m;
            EstatEstudi::afegeixMenuSortida (m, p, t, estat.nomsSortides);
            juce::Component::SafePointer<ColumnesPista> jo (this);
            m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [jo, i] (int r)
            {
                if (jo == nullptr || r <= 0 || i >= jo->estat.numPistes()) return;
                jo->comencaCanvi();
                const auto text = EstatEstudi::aplicaSortida (jo->estat.projecte, i, r);
                if (text.isEmpty()) return;
                jo->canviFet = true;
                jo->acabaCanvi ("Pista «"_ca + dibuix::utf8 (jo->estat.projecte.pistes[(size_t) i].nom) + "»: "_ca + text);
            });
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (arrossegaEnvia < 0 || filaEnvia < 0 || filaEnvia >= estat.numPistes()) return;
        auto& t = estat.projecte.pistes[(size_t) filaEnvia];
        if (arrossegaEnvia >= (int) t.enviaments.size()) return;
        const float dy = e.position.y - ultimaY;
        ultimaY = e.position.y;
        valor0 = juce::jlimit (0.0f, 1.0f, valor0 - dy / (e.mods.isShiftDown() ? 1500.0f : 150.0f));
        auto& x = t.enviaments[(size_t) arrossegaEnvia];
        x.nivellDb = EstatEstudi::dbFader (valor0);
        canviFet = true;
        const int d = mibase::edicio::pistaPerId (estat.projecte, x.desti);
        estat.info = "Enviament de «"_ca + dibuix::utf8 (t.nom) + "» a «"_ca + (d >= 0 ? dibuix::utf8 (estat.projecte.pistes[(size_t) d].nom) : juce::String ("?"))
                     + "»: "_ca + EstatEstudi::textDb (x.nivellDb);
        estat.avisa();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (arrossegaEnvia >= 0) acabaCanvi();
        arrossegaEnvia = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const auto lloc = troba (e.getPosition());
        if (lloc.col != Col::Envia || lloc.fila < 0) return;
        const auto& t = estat.projecte.pistes[(size_t) lloc.fila];
        if (lloc.k >= (int) t.enviaments.size()) return;
        comencaCanvi();
        estat.projecte.pistes[(size_t) lloc.fila].enviaments[(size_t) lloc.k].nivellDb = 0.0;
        canviFet = true;
        acabaCanvi();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto lloc = troba (e.getPosition());
        setMouseCursor (lloc.col == Col::Cap ? juce::MouseCursor::NormalCursor : juce::MouseCursor::PointingHandCursor);
    }

protected:
    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        const bool b = ambSegonaColumna();
        if (b != segona) { segona = b; if (onAmpladaCanviada) onAmpladaCanviada(); }
        repaint();
    }

private:
    enum class Col { Cap, Inserts, Envia, Entrada, Sortida, VolPan };
    struct Columnes { juce::Rectangle<int> insA, insB, envia, es; };
    struct Lloc { int fila = -2; Col col = Col::Cap; int k = 0; juce::Rectangle<int> r; };

    bool segona = false;
    int arrossegaEnvia = -1, filaEnvia = -1;
    float valor0 = 0.0f, ultimaY = 0.0f;

    Columnes columnes() const
    {
        Columnes c;
        int x = separacio;
        c.insA = { x, 0, ampleInserts, getHeight() }; x += ampleInserts + separacio;
        if (ambSegonaColumna()) { c.insB = { x, 0, ampleInserts, getHeight() }; x += ampleInserts + separacio; }
        c.envia = { x, 0, ampleEnvia, getHeight() }; x += ampleEnvia + separacio;
        c.es = { x, 0, ampleES, getHeight() };
        return c;
    }
    /** Quantes files hi caben (1 a 3) amb l'alçada de les pistes d'ara. */
    int numFiles() const { return juce::jlimit (1, 3, (estat.altPista - 6) / (altSlot + gapSlot)); }
    juce::Rectangle<int> slot (juce::Rectangle<int> col, int y, int k) const
    {
        return { col.getX() + 2, y + 4 + k * (altSlot + gapSlot), col.getWidth() - 4, altSlot };
    }

    Lloc troba (juce::Point<int> pt) const
    {
        Lloc l;
        if (pt.y < altTitol) return l;
        const auto c = columnes();
        const int nf = numFiles();
        auto provaFila = [&] (int fila, int y) -> bool
        {
            for (int k = 0; k < nf; ++k)
            {
                auto mira = [&] (juce::Rectangle<int> col, Col quina, int index)
                {
                    const auto r = slot (col, y, k);
                    if (! col.isEmpty() && r.contains (pt)) { l = { fila, quina, index, r }; return true; }
                    return false;
                };
                if (mira (c.insA, Col::Inserts, k)) return true;
                if (! c.insB.isEmpty() && mira (c.insB, Col::Inserts, 3 + k)) return true;
                if (fila >= 0 && mira (c.envia, Col::Envia, k)) return true;
                // E/S: amb 3 files entrada, sortida i vol/pan; amb menys, el que hi càpiga des de baix
                const int que = k + (3 - nf);   // 0 entrada, 1 sortida, 2 vol/pan
                if (que == 2 && mira (c.es, Col::VolPan, 0)) return true;
                if (fila >= 0 && que == 1 && mira (c.es, Col::Sortida, 0)) return true;
                if (fila >= 0 && que == 0 && mira (c.es, Col::Entrada, 0)) return true;
            }
            return false;
        };
        {
            const int y = altTitol + estat.yMaster();
            if (pt.y >= y && pt.y < y + estat.altPista) { provaFila (-1, y); return l; }
        }
        for (int i = 0; i < estat.numPistes(); ++i)
        {
            const int y = altTitol + estat.yPista (i);
            if (estat.altDe (i) == 0 || pt.y < y || pt.y >= y + estat.altDe (i)) continue;
            const auto& t = estat.projecte.pistes[(size_t) i];
            if (estat.filaCarpeta (i) || mibase::esAcords (t)) return l;
            provaFila (i, y);
            return l;
        }
        return l;
    }

    void pintaInserts (juce::Graphics& g, int pista, int y)
    {
        const auto c = columnes();
        const auto* v = inserts::llista (estat, pista);
        if (v == nullptr) return;
        const int nf = numFiles();
        for (int k = 0; k < nf; ++k)
        {
            inserts::pinta (g, slot (c.insA, y, k), k < (int) v->size() ? &(*v)[(size_t) k] : nullptr, k == (int) v->size(), true);
            if (! c.insB.isEmpty())
            {
                const int kb = 3 + k;
                inserts::pinta (g, slot (c.insB, y, k), kb < (int) v->size() ? &(*v)[(size_t) kb] : nullptr, kb == (int) v->size(), true);
            }
        }
    }

    void pintaEnvia (juce::Graphics& g, const mibase::Pista& t, int y)
    {
        const auto& p = estat.projecte;
        const auto c = columnes();
        for (int k = 0; k < numFiles(); ++k)
        {
            const auto z = slot (c.envia, y, k);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (z.toFloat(), 3.0f);
            if (k >= (int) t.enviaments.size())
            {
                g.setColour (k == (int) t.enviaments.size() ? tema::suau : tema::linia2);
                g.setFont (dibuix::lletra (10.0f));
                g.drawText (k == (int) t.enviaments.size() ? "+ Enviament"_ca : juce::String::fromUTF8 ("—"), z, juce::Justification::centred);
                continue;
            }
            const auto& e = t.enviaments[(size_t) k];
            const int d = mibase::edicio::pistaPerId (p, e.desti);
            const bool encesa = e.actiu && ! e.mut;
            const auto led = e.mode == 0 ? vintage::ledGroc : (e.mode == 2 ? vintage::ledBlau : vintage::ledVerd);
            g.setColour (encesa ? led : tema::linia2);
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ (float) z.getX() + 8.0f, (float) z.getCentreY() }));
            g.setColour (encesa ? tema::text : tema::suau);
            g.setFont (dibuix::lletra (10.0f));
            g.drawText (d >= 0 ? dibuix::utf8 (p.pistes[(size_t) d].nom) : juce::String ("?"), z.withTrimmedLeft (15).withTrimmedRight (30),
                        juce::Justification::centredLeft, true);
            g.setColour (encesa ? tema::textMig : tema::suau);
            g.setFont (dibuix::mono (9.5f));
            g.drawText (e.nivellDb <= -59.9 ? juce::String ("-oo") : juce::String (e.nivellDb, 1), z.withTrimmedRight (3), juce::Justification::centredRight);
        }
    }

    void caixa (juce::Graphics& g, juce::Rectangle<int> z, const juce::String& text, juce::Colour color)
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (z.toFloat(), 3.0f);
        g.setColour (color);
        g.setFont (dibuix::lletra (10.0f));
        g.drawFittedText (text, z.reduced (5, 0), juce::Justification::centredLeft, 1, 0.75f);
    }

    void pintaES (juce::Graphics& g, const mibase::Pista& t, int y)
    {
        const auto c = columnes();
        const int nf = numFiles();
        for (int k = 0; k < nf; ++k)
        {
            const int que = k + (3 - nf);
            const auto z = slot (c.es, y, k);
            if (que == 0)
            {
                const juce::String txt = t.tipus == "audio" ? "IN  "_ca + EstatEstudi::textEntrada (t)
                                       : mibase::esMidi (t) ? "IN  "_ca + (t.entradaMidi.empty() ? juce::String ("teclat") : dibuix::utf8 (t.entradaMidiNom))
                                                            : juce::String ("IN  -");
                caixa (g, z, txt, t.grava ? vintage::ledVermell : tema::text);
            }
            else if (que == 1)
            {
                const bool alMaster = t.sortida.empty() && t.sortidaTargeta < 0;
                caixa (g, z, "OUT  "_ca + (alMaster ? juce::String ("Master") : EstatEstudi::textDesti (estat.projecte, t)), alMaster ? tema::textMig : tema::text);
            }
            else pintaVolPan (g, z, t.faderDb, t.pan, false);
        }
    }

    void pintaVolPan (juce::Graphics& g, juce::Rectangle<int> z, double db, double pan, bool master)
    {
        g.setColour (juce::Colour (0xff101014));
        g.fillRoundedRectangle (z.toFloat(), 3.0f);
        g.setColour (tema::linia2);
        g.drawRoundedRectangle (z.toFloat().reduced (0.5f), 3.0f, 1.0f);
        g.setFont (dibuix::mono (9.5f));
        auto esq = master ? z : z.withWidth (z.getWidth() / 2);
        g.setColour (vintage::ledVerd);
        g.drawText ("vol " + (db <= -59.9 ? juce::String ("-oo") : juce::String (db, 1)), esq.reduced (4, 0), juce::Justification::centredLeft);
        if (master) return;
        auto dre = z.withTrimmedLeft (z.getWidth() / 2);
        g.setColour (tema::linia2);
        g.fillRect (dre.getX(), z.getY() + 3, 1, z.getHeight() - 6);
        g.setColour (vintage::ledVerd);
        g.drawText ("pan " + textPanCurt (pan), dre.reduced (4, 0), juce::Justification::centredLeft);
    }

    static juce::String textPanCurt (double pan)
    {
        const int v = juce::roundToInt (pan * 100.0);
        if (v == 0) return "C";
        return v < 0 ? "<" + juce::String (-v) : juce::String (v) + ">";
    }

    void menuEnviaments (int i)
    {
        juce::Component::SafePointer<ColumnesPista> jo (this);
        EstatEstudi::menuEnviaments (estat.projecte, estat.projecte.pistes[(size_t) i])
            .showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [jo, i] (int r)
        {
            if (jo == nullptr || r <= 0 || i >= jo->estat.numPistes()) return;
            jo->comencaCanvi();
            const auto text = EstatEstudi::aplicaEnviament (jo->estat.projecte, i, r);
            if (text.isEmpty()) return;
            jo->canviFet = true;
            jo->acabaCanvi (text);
        });
    }

    //==========================================================================
    /** El fader (o el panorama) que surt en clicar «vol» (o «pan»). */
    class Popup : public juce::Component
    {
    public:
        Popup (ColumnesPista& c, int fila, bool pan) : col (c), i (fila), esPan (pan)
        {
            setSize (pan ? 220 : 70, pan ? 52 : 230);
        }
        void paint (juce::Graphics& g) override
        {
            g.fillAll (tema::fons);
            if (esPan)
            {
                const auto car = carril();
                g.setColour (juce::Colours::black.withAlpha (0.6f));
                g.fillRoundedRectangle (car, 3.0f);
                g.setColour (tema::linia2);
                g.fillRect (car.getCentreX() - 0.5f, car.getY() - 4.0f, 1.0f, car.getHeight() + 8.0f);
                const float x = car.getX() + (float) (valor() + 1.0) * 0.5f * car.getWidth();
                g.setColour (tema::accent);
                g.fillRoundedRectangle (juce::Rectangle<float> (12.0f, 20.0f).withCentre ({ x, car.getCentreY() }), 3.0f);
                g.setColour (tema::text);
                g.setFont (dibuix::mono (10.0f));
                g.drawText ("pan " + textPanCurt (valor()), getLocalBounds().removeFromBottom (14), juce::Justification::centred);
                return;
            }
            const double db = valor();
            carrilFader = dibuixEstudi::faderVertical (g, getLocalBounds().toFloat().reduced (6.0f, 6.0f).withTrimmedBottom (14.0f), db, tema::accent, true);
            g.setColour (tema::text);
            g.setFont (dibuix::mono (10.0f));
            g.drawText (EstatEstudi::textDb (db), getLocalBounds().removeFromBottom (16), juce::Justification::centred);
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            col.comencaCanvi();
            y0 = e.position;
            v0 = esPan ? (float) valor() : EstatEstudi::posFader (valor());
            if (esPan) posa (juce::jlimit (-1.0, 1.0, (double) ((e.position.x - carril().getX()) / carril().getWidth() * 2.0f - 1.0f)));
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            const bool fi = e.mods.isShiftDown();
            if (esPan)
            {
                posa (juce::jlimit (-1.0, 1.0, (double) ((e.position.x - carril().getX()) / carril().getWidth() * 2.0f - 1.0f)));
                return;
            }
            const float alt = juce::jmax (40.0f, carrilFader.getHeight());
            const float v = juce::jlimit (0.0f, 1.0f, v0 - (e.position.y - y0.y) / alt * (fi ? 0.1f : 1.0f));
            posa (EstatEstudi::dbFader (v));
        }
        void mouseUp (const juce::MouseEvent&) override { col.acabaCanvi(); }
        void mouseDoubleClick (const juce::MouseEvent&) override
        {
            col.comencaCanvi();
            posa (0.0);   // 0 dB o al mig
            col.acabaCanvi();
        }

    private:
        ColumnesPista& col;
        int i;
        bool esPan;
        juce::Point<float> y0;
        float v0 = 0.0f;
        juce::Rectangle<float> carrilFader;

        juce::Rectangle<float> carril() const { return getLocalBounds().toFloat().reduced (14.0f, 0.0f).withY (12.0f).withHeight (14.0f); }
        double valor() const
        {
            const auto& p = col.estat.projecte;
            if (i < 0) return esPan ? 0.0 : p.masterDb;
            if (i >= (int) p.pistes.size()) return 0.0;
            return esPan ? p.pistes[(size_t) i].pan : p.pistes[(size_t) i].faderDb;
        }
        void posa (double v)
        {
            auto& p = col.estat.projecte;
            if (i < 0) p.masterDb = v;
            else if (i < (int) p.pistes.size())
            {
                auto& t = p.pistes[(size_t) i];
                if (esPan) t.pan = std::abs (v) < 0.01 ? 0.0 : v;
                else t.faderDb = v;
                col.estat.info = (esPan ? "Panorama de «"_ca : "Volum de «"_ca) + dibuix::utf8 (t.nom) + "»: "_ca
                               + (esPan ? EstatEstudi::textPan (t.pan) : EstatEstudi::textDb (t.faderDb));
            }
            col.canviFet = true;
            col.estat.avisa();
            repaint();
        }
    };
    friend class Popup;

    void obrePopup (int fila, bool pan, juce::Rectangle<int> r)
    {
        auto pop = std::make_unique<Popup> (*this, fila, pan);
        juce::CallOutBox::launchAsynchronously (std::move (pop), localAreaToGlobal (r), nullptr);
        estat.info = pan ? "Panorama: arrossega · doble clic: al mig"_ca : "Volum: arrossega (Maj: més fi) · doble clic: 0 dB"_ca;
        estat.avisa();
    }
};
