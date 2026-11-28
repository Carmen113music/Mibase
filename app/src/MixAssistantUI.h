#pragma once
// 1.23: el MIX ASSISTANT (fase 1). La finestra pròpia (F7, es pot deixar a una segona pantalla; l'Esc no la tanca) i la
// pestanya «Mix» del panell de la dreta. Mostra el diagnòstic de cada pista; encara no canvia res de la mescla.
#include "Controls.h"
#include "EstatEstudi.h"
#include "mibase/MixAssistant.h"
#include <set>

/** El que sap el Mix Assistant ara (no es desa al projecte: es torna a analitzar; sí que es desa l'instrument triat). */
struct ModelMix : public juce::ChangeBroadcaster
{
    struct Fila
    {
        std::string id;
        juce::String nom;
        juce::Colour color;
        mibase::mix::Identificacio ident;
        mibase::mix::DiagnosiPista diag;
        std::shared_ptr<mibase::mix::AnalisiPista> analisi;
        bool analitzada = false;
        juce::String motiu;   // per què no s'ha analitzat («silenciada», «buida»…)
    };
    std::vector<Fila> files;
    bool analitzant = false, fet = false;
    juce::String progres, quan;
    double fraccio = 0.0;

    const Fila* fila (const std::string& id) const
    {
        for (const auto& f : files) if (f.id == id) return &f;
        return nullptr;
    }
    int compta (mibase::mix::Prioritat p) const
    {
        int n = 0;
        for (const auto& f : files) for (const auto& i : f.diag.incidencies) n += i.prioritat == p;
        return n;
    }
    int perPreguntar() const
    {
        int n = 0;
        for (const auto& f : files) n += f.analitzada && f.ident.calPreguntar();
        return n;
    }
    int analitzades() const
    {
        int n = 0;
        for (const auto& f : files) n += f.analitzada;
        return n;
    }
    /** Totes les incidències, de la més greu a la menys. */
    std::vector<std::pair<const Fila*, const mibase::mix::Incidencia*>> totes() const
    {
        std::vector<std::pair<const Fila*, const mibase::mix::Incidencia*>> v;
        for (const auto& f : files) for (const auto& i : f.diag.incidencies) v.push_back ({ &f, &i });
        std::stable_sort (v.begin(), v.end(), [] (auto& a, auto& b) { return a.second->gravetat > b.second->gravetat; });
        return v;
    }
    void avisa() { sendSynchronousChangeMessage(); }
};

namespace mixui
{
    namespace mx = mibase::mix;

    inline juce::String u (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }
    inline juce::Colour colorPrioritat (mx::Prioritat p)
    {
        return p == mx::Prioritat::Alta ? tema::error : p == mx::Prioritat::Mitjana ? tema::accent : tema::suau;
    }
    inline juce::String nomConfianca (mx::Confianca c)
    {
        return c == mx::Confianca::Alta ? "alta"_ca : c == mx::Confianca::Mitjana ? "mitjana"_ca : "baixa (escolta-ho abans)"_ca;
    }

    /** Text amb salts de línia: alçada i dibuix. */
    inline juce::TextLayout disposa (const juce::String& t, const juce::Font& f, juce::Colour c, float w)
    {
        juce::AttributedString a;
        a.setWordWrap (juce::AttributedString::byWord);
        a.append (t, f, c);
        juce::TextLayout l;
        l.createLayout (a, juce::jmax (10.0f, w));
        return l;
    }
    /** Escriu el text a partir de y i torna la y de sota (g pot ser nul: només mesura). */
    inline int escriu (juce::Graphics* g, const juce::String& t, const juce::Font& f, juce::Colour c, int x, int y, int w, int separacio = 6)
    {
        if (t.isEmpty()) return y;
        auto l = disposa (t, f, c, (float) w);
        if (g != nullptr) l.draw (*g, juce::Rectangle<float> ((float) x, (float) y, (float) w, l.getHeight()));
        return y + (int) std::ceil (l.getHeight()) + separacio;
    }
    /** Un botó dibuixat (els de dins de les llistes llargues). */
    inline void pintaBoto (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& t, bool primari, bool sobre)
    {
        auto rf = r.toFloat().reduced (0.5f);
        g.setColour (primari ? (sobre ? tema::accent.brighter (0.15f) : tema::accent) : (sobre ? tema::sobre.brighter (0.1f) : tema::superficie2));
        g.fillRoundedRectangle (rf, 6.0f);
        if (! primari) { g.setColour (tema::linia2); g.drawRoundedRectangle (rf, 6.0f, 1.0f); }
        g.setColour (primari ? juce::Colours::black : tema::text);
        g.setFont (dibuix::lletra (12.5f, primari));
        g.drawText (t, r, juce::Justification::centred);
    }
    inline int ampleBoto (const juce::String& t)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (dibuix::lletra (12.5f, true), t, 0, 0);
        return (int) std::ceil (ga.getBoundingBox (0, -1, true).getWidth()) + 24;
    }
    /** Els tres comptadors (importants, a revisar, detalls). */
    inline void pintaComptadors (juce::Graphics& g, juce::Rectangle<int> r, int alta, int mitjana, int baixa, bool petit = false)
    {
        const juce::String t[] = { juce::String (alta) + (alta == 1 ? " important"_ca : " importants"_ca),
                                   juce::String (mitjana) + " a revisar"_ca, juce::String (baixa) + (baixa == 1 ? " detall"_ca : " detalls"_ca) };
        const juce::Colour c[] = { tema::error, tema::accent, tema::suau };
        const int n[] = { alta, mitjana, baixa };
        int x = r.getX();
        for (int k = 0; k < 3; ++k)
        {
            const int w = ampleBoto (t[k]) + (petit ? 0 : 8);
            if (x + w > r.getRight()) break;
            auto rr = juce::Rectangle<int> (x, r.getY(), w, r.getHeight()).toFloat().reduced (0.5f);
            g.setColour (n[k] > 0 ? c[k].withAlpha (0.16f) : tema::superficie);
            g.fillRoundedRectangle (rr, rr.getHeight() / 2.0f);
            g.setColour (n[k] > 0 ? c[k] : tema::linia2);
            g.drawRoundedRectangle (rr, rr.getHeight() / 2.0f, 1.0f);
            g.fillEllipse (rr.getX() + 9.0f, rr.getCentreY() - 3.5f, 7.0f, 7.0f);
            g.setColour (n[k] > 0 ? tema::text : tema::suau);
            g.setFont (dibuix::lletra (petit ? 11.5f : 12.5f, n[k] > 0));
            g.drawText (t[k], rr.toNearestInt().withTrimmedLeft (20), juce::Justification::centredLeft);
            x += w + 6;
        }
    }
}

/** La part de la dreta de la finestra: el diagnòstic d'una pista (o de totes, per ordre d'importància). */
class LlistaMix : public juce::Component
{
public:
    explicit LlistaMix (ModelMix& m) : model (m) { setRepaintsOnMouseActivity (true); }

    std::function<void (const std::string&)> onTriaInstrument;                          // obre el menú d'instruments
    std::function<void (const std::string&, mibase::mix::Instrument)> onConfirma;      // «Sí, és això»
    std::function<void (const std::string&)> onVesAPista, onMostraPista;

    void mostra (const std::string& idPista) { pista = idPista; refes(); }
    const std::string& pistaMostrada() const noexcept { return pista; }

    void refes()
    {
        const int alt = mesura (getWidth());
        setSize (getWidth(), juce::jmax (alt, getParentHeight()));
        repaint();
    }
    void amplada (int w) { setSize (w, getHeight()); refes(); }

    void paint (juce::Graphics& g) override { dibuixa (&g, getWidth()); }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto ara = zonaA (e.getPosition());
        if (ara != sobre) { sobre = ara; repaint(); }
        setMouseCursor (ara >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseExit (const juce::MouseEvent&) override { if (sobre >= 0) { sobre = -1; repaint(); } }
    void mouseUp (const juce::MouseEvent& e) override
    {
        const int z = zonaA (e.getPosition());
        if (z < 0) return;
        auto zona = zones[(size_t) z];   // còpia: les accions poden refer la llista
        switch (zona.accio)
        {
            case Zona::Detalls:
                if (oberts.count (zona.clau) > 0) oberts.erase (zona.clau); else oberts.insert (zona.clau);
                refes();
                break;
            case Zona::Instrument: if (onTriaInstrument) onTriaInstrument (zona.clau); break;
            case Zona::Confirma: if (onConfirma) onConfirma (zona.clau, zona.inst); break;
            case Zona::VesAPista: if (onVesAPista) onVesAPista (zona.clau); break;
            case Zona::MostraPista: if (onMostraPista) onMostraPista (zona.clau); break;
        }
    }

private:
    struct Zona
    {
        enum Accio { Detalls, Instrument, Confirma, VesAPista, MostraPista } accio;
        juce::Rectangle<int> r;
        std::string clau;
        mibase::mix::Instrument inst = mibase::mix::Instrument::Desconegut;
    };
    ModelMix& model;
    std::string pista;                // buit: totes
    std::set<std::string> oberts;     // incidències (i «mesures:<pista>») amb els detalls oberts
    std::vector<Zona> zones;
    int sobre = -1;

    int zonaA (juce::Point<int> p) const
    {
        for (size_t i = 0; i < zones.size(); ++i) if (zones[i].r.contains (p)) return (int) i;
        return -1;
    }
    int mesura (int w) { return dibuixa (nullptr, w); }

    juce::Rectangle<int> boto (juce::Graphics* g, int x, int y, const juce::String& t, bool primari, Zona z)
    {
        juce::Rectangle<int> r (x, y, mixui::ampleBoto (t), 28);
        z.r = r;
        if (g != nullptr) mixui::pintaBoto (*g, r, t, primari, sobre == (int) zones.size());
        zones.push_back (z);
        return r;
    }

    /** Dibuixa (o només mesura, amb g nul) i torna l'alçada. */
    int dibuixa (juce::Graphics* g, int w)
    {
        namespace mx = mibase::mix;
        using mixui::u;
        zones.clear();
        const int m = 16, W = juce::jmax (200, w - 2 * m);
        int y = 14;
        if (! model.fet)
        {
            y = mixui::escriu (g, model.analitzant ? "Analitzant… Cada pista es renderitza per separat, sense reproduir-la i sense tocar el so original."_ca
                                                   : "Encara no s'ha analitzat. Prem «Analitza la mescla»: Mibase escolta cada pista per separat (sense reproduir-la) i et diu què hi veu, per què pot ser un problema i què proposa.\n\nEn aquesta primera fase el Mix Assistant només diagnostica: no canvia res de la mescla."_ca,
                               dibuix::lletra (14.0f), tema::textMig, m, y, W);
            return y + 20;
        }
        if (pista.empty())
        {
            y = mixui::escriu (g, "Totes les pistes"_ca, dibuix::lletra (18.0f, true), tema::text, m, y, W, 4);
            y = mixui::escriu (g, "De la més important a la menys. Clica el nom d'una pista per veure-la sola."_ca, dibuix::lletra (12.5f), tema::suau, m, y, W, 12);
            if (const int n = model.perPreguntar(); n > 0)
            {
                juce::String noms;
                for (const auto& f : model.files) if (f.analitzada && f.ident.calPreguntar()) noms << (noms.isEmpty() ? "" : ", ") << f.nom;
                y = caixaPregunta (g, m, y, W, (n == 1 ? "Hi ha una pista que no sé quin instrument és: "_ca : juce::String (n) + " pistes no sé quin instrument són: "_ca)
                                   + noms + ". Si m'ho dius, el diagnòstic serà més precís."_ca, {});
            }
            const auto v = model.totes();
            if (v.empty()) y = mixui::escriu (g, "No hi veig res important a cap pista."_ca, dibuix::lletra (14.0f), tema::textMig, m, y, W);
            for (const auto& [f, i] : v) y = targeta (g, *f, *i, m, y, W, true);
            return y + 16;
        }
        const auto* f = model.fila (pista);
        if (f == nullptr) return mixui::escriu (nullptr, " ", dibuix::lletra (14.0f), tema::text, m, y, W);
        // capçalera de la pista
        if (g != nullptr) { g->setColour (f->color); g->fillRoundedRectangle ((float) m, (float) y + 4.0f, 6.0f, 22.0f, 3.0f); }
        y = mixui::escriu (g, f->nom, dibuix::lletra (20.0f, true), tema::text, m + 14, y, W - 14, 4);
        if (! f->analitzada)
        {
            y = mixui::escriu (g, f->motiu, dibuix::lletra (13.5f), tema::textMig, m, y + 6, W);
            return y + 16;
        }
        const auto& id = f->ident;
        {
            juce::String t = "Instrument: "_ca + u (mx::nomInstrument (id.instrument)) + "  ·  "_ca + u (id.perque);
            y = mixui::escriu (g, t, dibuix::lletra (13.0f), tema::textMig, m, y, W - 200, 8);
            Zona z { Zona::Instrument, {}, f->id };
            boto (g, m, y, "Canvia l'instrument"_ca, false, z);
            Zona v { Zona::VesAPista, {}, f->id };
            boto (g, m + mixui::ampleBoto ("Canvia l'instrument"_ca) + 8, y, "Vés a la pista"_ca, false, v);
            y += 40;
        }
        if (id.calPreguntar())
        {
            const bool sembla = id.instrument != mx::Instrument::Desconegut;
            y = caixaPregunta (g, m, y, W, sembla ? "No n'estic segur: em sembla que és «"_ca + u (mx::nomInstrument (id.instrument)) + "». És correcte?"_ca
                                                  : "No sé quin instrument és. Digues-m'ho i el diagnòstic serà més precís."_ca,
                               sembla ? f->id : std::string(), id.instrument);
        }
        y = mixui::escriu (g, u (f->diag.resum), dibuix::lletra (14.0f, true), tema::text, m, y + 2, W, 12);
        for (const auto& i : f->diag.incidencies) y = targeta (g, *f, i, m, y, W, false);
        // les xifres
        {
            const std::string clau = "mesures:" + f->id;
            const bool obert = oberts.count (clau) > 0;
            Zona z { Zona::Detalls, {}, clau };
            boto (g, m, y + 4, obert ? "Amaga les mesures"_ca : "Mostra les mesures"_ca, false, z);
            y += 40;
            if (obert) y = mixui::escriu (g, u (f->diag.mesures), dibuix::mono (12.0f), tema::textMig, m, y, W, 10);
        }
        return y + 16;
    }

    int caixaPregunta (juce::Graphics* g, int x, int y, int w, const juce::String& text, const std::string& idPista,
                       mibase::mix::Instrument inst = mibase::mix::Instrument::Desconegut)
    {
        const int pad = 12;
        const int h0 = mixui::escriu (nullptr, text, dibuix::lletra (13.5f), tema::text, 0, 0, w - 2 * pad, 0);
        const bool ambBotons = ! pista.empty();
        const int h = pad + h0 + (ambBotons ? 10 + 28 : 0) + pad;
        if (g != nullptr)
        {
            auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
            g->setColour (tema::accent.withAlpha (0.10f));
            g->fillRoundedRectangle (r, 8.0f);
            g->setColour (tema::accent.withAlpha (0.6f));
            g->drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        }
        mixui::escriu (g, text, dibuix::lletra (13.5f), tema::text, x + pad, y + pad, w - 2 * pad, 0);
        if (ambBotons)
        {
            int bx = x + pad;
            const int by = y + pad + h0 + 10;
            if (! idPista.empty())
            {
                Zona c { Zona::Confirma, {}, idPista, inst };
                bx = boto (g, bx, by, "Sí, és això"_ca, true, c).getRight() + 8;
            }
            Zona t { Zona::Instrument, {}, pista };
            boto (g, bx, by, "Tria l'instrument"_ca, false, t);
        }
        return y + h + 14;
    }

    int targeta (juce::Graphics* g, const ModelMix::Fila& f, const mibase::mix::Incidencia& i, int x, int y, int w, bool ambPista)
    {
        using mixui::u;
        const int pad = 14, iw = w - 2 * pad - 6;
        const bool obert = oberts.count (i.id) > 0;
        const auto col = mixui::colorPrioritat (i.prioritat);
        const int y0 = y;
        // primer es mesura, després es pinta el fons i el text
        auto contingut = [&] (juce::Graphics* gg) -> int
        {
            int yy = y0 + pad;
            const int tx = x + pad + 6;
            const juce::String et = u (mibase::mix::nomPrioritat (i.prioritat));
            const int we = mixui::ampleBoto (et);
            if (gg != nullptr)
            {
                auto re = juce::Rectangle<float> ((float) (x + w - pad - we), (float) yy, (float) we, 20.0f);
                gg->setColour (col.withAlpha (0.18f));
                gg->fillRoundedRectangle (re, 10.0f);
                gg->setColour (col);
                gg->setFont (dibuix::lletra (11.5f, true));
                gg->drawText (et, re.toNearestInt(), juce::Justification::centred);
            }
            yy = mixui::escriu (gg, u (i.titol), dibuix::lletra (15.0f, true), tema::text, tx, yy, iw - we - 10, 3);
            juce::String on = u (i.on);
            if (ambPista)
            {
                const juce::String np = f.nom;
                const int wp = mixui::ampleBoto (np) - 18;
                if (gg != nullptr)
                {
                    gg->setColour (f.color);
                    gg->fillEllipse ((float) tx, (float) yy + 4.0f, 8.0f, 8.0f);
                    gg->setColour (sobre == (int) zones.size() ? tema::accent : tema::textMig);
                    gg->setFont (dibuix::lletra (12.5f, true));
                    gg->drawText (np, tx + 13, yy, wp + 4, 16, juce::Justification::centredLeft);
                }
                zones.push_back ({ Zona::MostraPista, { tx, yy - 2, wp + 20, 20 }, f.id });
                yy = mixui::escriu (gg, on, dibuix::lletra (12.0f), tema::suau, tx + wp + 26, yy, iw - wp - 26, 6);
            }
            else yy = mixui::escriu (gg, on, dibuix::lletra (12.0f), tema::suau, tx, yy, iw, 6);
            yy = mixui::escriu (gg, u (i.queVeig), dibuix::lletra (13.5f), tema::textMig, tx, yy, iw, 4);
            yy = mixui::escriu (gg, u (i.perque), dibuix::lletra (13.0f), tema::suau, tx, yy, iw, 8);
            yy = mixui::escriu (gg, "Què proposo: "_ca + u (i.solucio), dibuix::lletra (13.5f), tema::text, tx, yy, iw, 8);
            if (gg != nullptr)
            {
                gg->setColour (tema::suau);
                gg->setFont (dibuix::lletra (11.5f));
                gg->drawText ("Confiança: "_ca + mixui::nomConfianca (i.confianca), tx, yy + 6, iw - 140, 16, juce::Justification::centredLeft);
            }
            if (! i.detalls.empty())
            {
                const juce::String tb = obert ? "Amaga els detalls"_ca : "Detalls"_ca;
                Zona z { Zona::Detalls, {}, i.id };
                boto (gg, x + w - pad - mixui::ampleBoto (tb), yy, tb, false, z);
            }
            yy += 34;
            if (obert && ! i.detalls.empty())
            {
                const int hd = mixui::escriu (nullptr, u (i.detalls), dibuix::mono (12.0f), tema::text, 0, 0, iw - 20, 0);
                if (gg != nullptr)
                {
                    gg->setColour (tema::fons);
                    gg->fillRoundedRectangle ((float) tx, (float) yy, (float) iw, (float) hd + 16.0f, 6.0f);
                }
                mixui::escriu (gg, u (i.detalls), dibuix::mono (12.0f), tema::text, tx + 10, yy + 8, iw - 20, 0);
                yy += hd + 24;
            }
            return yy + pad - 6;
        };
        const size_t nz = zones.size();
        const int fi = contingut (nullptr);
        zones.resize (nz);   // es tornen a posar en pintar
        if (g != nullptr)
        {
            auto r = juce::Rectangle<float> ((float) x, (float) y0, (float) w, (float) (fi - y0));
            g->setColour (tema::superficie);
            g->fillRoundedRectangle (r, 8.0f);
            g->setColour (tema::linia);
            g->drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
            g->setColour (col);
            g->fillRoundedRectangle (r.withWidth (5.0f).reduced (0.0f, 6.0f).translated (4.0f, 0.0f), 2.5f);
        }
        contingut (g);
        return fi + 10;
    }
};

/** La finestra del Mix Assistant (el contingut). */
class VistaMixAssistant : public juce::Component, private juce::ChangeListener, private juce::ListBoxModel
{
public:
    explicit VistaMixAssistant (ModelMix& m) : model (m), detall (m)
    {
        setOpaque (true);
        addAndMakeVisible (btAnalitza);
        btAnalitza.setTooltip ("Escolta totes les pistes (renderitzades, sense reproduir-les) i en fa el diagnòstic. No canvia res de la mescla."_ca);
        btAnalitza.onClick = [this] { if (onAnalitza) onAnalitza(); };
        llista.setModel (this);
        llista.setRowHeight (48);
        llista.setColour (juce::ListBox::backgroundColourId, tema::lateral);
        llista.setOutlineThickness (0);
        addAndMakeVisible (llista);
        vista.setViewedComponent (&detall, false);
        vista.setScrollBarsShown (true, false);
        vista.setScrollBarThickness (10);
        addAndMakeVisible (vista);
        detall.onMostraPista = [this] (const std::string& id) { triaPista (id); };
        model.addChangeListener (this);
        actualitza();
    }
    ~VistaMixAssistant() override { model.removeChangeListener (this); }

    std::function<void()> onAnalitza;
    LlistaMix& llistaDetall() noexcept { return detall; }

    /** Mostra una pista (buit: totes). */
    void triaPista (const std::string& id)
    {
        int fila = 0;
        for (size_t i = 0; i < model.files.size(); ++i) if (model.files[i].id == id) fila = (int) i + 1;
        llista.selectRow (fila, false, true);
        detall.mostra (fila == 0 ? std::string() : id);
        vista.setViewPosition (0, 0);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (tema::fons);
        auto r = getLocalBounds();
        auto cap = r.removeFromTop (alcadaCap);
        g.setColour (tema::superficie);
        g.fillRect (cap);
        g.setColour (tema::linia);
        g.fillRect (cap.removeFromBottom (1));
        auto t = cap.reduced (18, 0).withTrimmedRight (btAnalitza.getWidth() + 18);
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (20.0f, true));
        g.drawText ("Mix Assistant", t.getX(), 12, t.getWidth(), 26, juce::Justification::centredLeft);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (12.5f));
        juce::String sub;
        if (model.analitzant) sub = model.progres;
        else if (model.fet) sub = juce::String (model.analitzades()) + " pistes analitzades · "_ca + model.quan + " · Si canvies la mescla, torna a analitzar."_ca;
        else sub = "Analitza, diagnostica i explica. Fase 1: encara no canvia res de la mescla."_ca;
        g.drawText (sub, t.getX(), 38, t.getWidth(), 18, juce::Justification::centredLeft);
        if (model.analitzant)
        {
            auto b = juce::Rectangle<float> ((float) t.getX(), 62.0f, (float) juce::jmin (420, t.getWidth()), 6.0f);
            g.setColour (tema::superficie2);
            g.fillRoundedRectangle (b, 3.0f);
            g.setColour (tema::accent);
            g.fillRoundedRectangle (b.withWidth (b.getWidth() * (float) juce::jlimit (0.0, 1.0, model.fraccio)), 3.0f);
        }
        else if (model.fet)
            mixui::pintaComptadors (g, { t.getX(), 60, t.getWidth(), 24 }, model.compta (mibase::mix::Prioritat::Alta),
                                    model.compta (mibase::mix::Prioritat::Mitjana), model.compta (mibase::mix::Prioritat::Baixa));
        g.setColour (tema::linia);
        g.fillRect (llista.getRight(), alcadaCap, 1, getHeight() - alcadaCap);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto cap = r.removeFromTop (alcadaCap);
        btAnalitza.setBounds (cap.removeFromRight (btAnalitza.ampladaIdeal() + 18).withTrimmedRight (18).withSizeKeepingCentre (btAnalitza.ampladaIdeal(), 34));
        llista.setBounds (r.removeFromLeft (juce::jlimit (200, 290, getWidth() / 4)));
        r.removeFromLeft (1);
        vista.setBounds (r);
        detall.setSize (r.getWidth() - vista.getScrollBarThickness(), detall.getHeight());
        detall.refes();
    }

    void actualitza()
    {
        btAnalitza.setEnabled (! model.analitzant);
        btAnalitza.setButtonText (model.analitzant ? "Analitzant…"_ca : model.fet ? "Torna a analitzar"_ca : "Analitza la mescla"_ca);
        llista.updateContent();
        if (llista.getSelectedRow() < 0) llista.selectRow (0, false, true);
        detall.refes();
        resized();
        repaint();
    }

private:
    static constexpr int alcadaCap = 92;
    ModelMix& model;
    Boto btAnalitza { "Analitza la mescla"_ca, std::nullopt, Boto::Estil::Primari };
    juce::ListBox llista;
    juce::Viewport vista;
    LlistaMix detall;

    void changeListenerCallback (juce::ChangeBroadcaster*) override { actualitza(); }

    int getNumRows() override { return 1 + (int) model.files.size(); }
    void paintListBoxItem (int fila, juce::Graphics& g, int w, int h, bool triada) override
    {
        namespace mx = mibase::mix;
        if (triada) { g.setColour (tema::triat); g.fillRect (0, 0, w, h); g.setColour (tema::accent); g.fillRect (0, 0, 3, h); }
        g.setColour (tema::linia.withAlpha (0.5f));
        g.fillRect (12, h - 1, w - 24, 1);
        if (fila == 0)
        {
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (14.0f, true));
            g.drawText ("Totes les pistes"_ca, 16, 6, w - 30, 20, juce::Justification::centredLeft);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (11.5f));
            g.drawText (model.fet ? "Per ordre d'importància"_ca : "Encara sense analitzar"_ca, 16, 26, w - 30, 16, juce::Justification::centredLeft);
            return;
        }
        if (fila - 1 >= (int) model.files.size()) return;
        const auto& f = model.files[(size_t) fila - 1];
        g.setColour (f.color);
        g.fillRoundedRectangle (14.0f, 10.0f, 5.0f, (float) h - 20.0f, 2.5f);
        int ra = 0, rm = 0, rb = 0;
        for (const auto& i : f.diag.incidencies) (i.prioritat == mx::Prioritat::Alta ? ra : i.prioritat == mx::Prioritat::Mitjana ? rm : rb)++;
        const int dots = f.analitzada ? 46 : 0;
        g.setColour (f.analitzada ? tema::text : tema::suau);
        g.setFont (dibuix::lletra (13.5f, true));
        g.drawText (f.nom, 28, 6, w - 36 - dots, 20, juce::Justification::centredLeft, true);
        g.setFont (dibuix::lletra (11.5f));
        juce::String sota = f.analitzada ? mixui::u (mx::nomInstrument (f.ident.instrument)) : f.motiu;
        if (f.analitzada && f.ident.calPreguntar()) { g.setColour (tema::accent); sota += "  ?"; }
        else g.setColour (tema::suau);
        g.drawText (sota, 28, 26, w - 36 - dots, 16, juce::Justification::centredLeft, true);
        if (! f.analitzada) return;
        float x = (float) w - 14.0f;
        for (auto [n, c] : { std::pair<int, juce::Colour> { rb, tema::suau }, { rm, tema::accent }, { ra, tema::error } })
        {
            if (n == 0) continue;
            g.setColour (c);
            g.setFont (dibuix::lletra (11.5f, true));
            g.drawText (juce::String (n), (int) x - 12, 16, 12, 16, juce::Justification::centredRight);
            g.fillEllipse (x - 21.0f, 21.0f, 7.0f, 7.0f);
            x -= 26.0f;
        }
        if (f.diag.incidencies.empty())
        {
            g.setColour (tema::sinte);
            g.setFont (dibuix::lletra (11.5f, true));
            g.drawText ("Bé"_ca, w - 50, 16, 36, 16, juce::Justification::centredRight);
        }
    }
    void selectedRowsChanged (int fila) override
    {
        if (fila < 0) return;
        detall.mostra (fila == 0 || fila - 1 >= (int) model.files.size() ? std::string() : model.files[(size_t) fila - 1].id);
        vista.setViewPosition (0, 0);
    }
};

/** La pestanya «Mix» del panell de la dreta: el resum i les incidències més importants. */
class PanellMix : public juce::Component, private juce::ChangeListener, private juce::ListBoxModel
{
public:
    explicit PanellMix (ModelMix& m) : model (m)
    {
        for (auto* b : { &btObre, &btAnalitza }) addAndMakeVisible (b);
        btObre.setTooltip ("Obre la finestra del Mix Assistant (F7)"_ca);
        btAnalitza.setTooltip ("Escolta totes les pistes i en fa el diagnòstic (no canvia res de la mescla)"_ca);
        btObre.onClick = [this] { if (onObre) onObre ({}); };
        btAnalitza.onClick = [this] { if (onAnalitza) onAnalitza(); };
        llista.setModel (this);
        llista.setRowHeight (46);
        llista.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        llista.setOutlineThickness (0);
        addAndMakeVisible (llista);
        model.addChangeListener (this);
    }
    ~PanellMix() override { model.removeChangeListener (this); }

    std::function<void (const std::string&)> onObre;   // la pista on ha d'anar (buit: totes)
    std::function<void()> onAnalitza;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().reduced (12, 0);
        r.removeFromTop (zonaBotons);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (12.0f));
        juce::String t;
        if (model.analitzant) t = model.progres;
        else if (! model.fet) t = "Encara no s'ha analitzat la mescla."_ca;
        else t = juce::String (model.analitzades()) + " pistes · "_ca + model.quan;
        g.drawText (t, r.removeFromTop (20), juce::Justification::centredLeft, true);
        if (model.fet && ! model.analitzant)
        {
            auto c = r.removeFromTop (28).withTrimmedTop (4);
            mixui::pintaComptadors (g, c, model.compta (mibase::mix::Prioritat::Alta), model.compta (mibase::mix::Prioritat::Mitjana),
                                    model.compta (mibase::mix::Prioritat::Baixa), true);
            if (const int n = model.perPreguntar(); n > 0)
            {
                g.setColour (tema::accent);
                g.setFont (dibuix::lletra (12.0f));
                g.drawText (juce::String (n) + (n == 1 ? " pista per identificar"_ca : " pistes per identificar"_ca), r.removeFromTop (22), juce::Justification::centredLeft);
            }
        }
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (12, 0);
        auto b = r.removeFromTop (zonaBotons).withTrimmedTop (8).withHeight (32);
        btObre.setBounds (b.removeFromLeft (b.getWidth() * 3 / 5));
        b.removeFromLeft (6);
        btAnalitza.setBounds (b);
        int alt = 20 + (model.fet && ! model.analitzant ? 28 + (model.perPreguntar() > 0 ? 22 : 0) : 0) + 8;
        llista.setBounds (getLocalBounds().withTrimmedTop (zonaBotons + alt));
    }

private:
    static constexpr int zonaBotons = 48;
    ModelMix& model;
    Boto btObre { "Obre el Mix Assistant"_ca, std::nullopt, Boto::Estil::Primari };
    Boto btAnalitza { "Analitza"_ca };
    juce::ListBox llista;
    std::vector<std::pair<std::string, const mibase::mix::Incidencia*>> files;

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        files.clear();
        for (const auto& [f, i] : model.totes())
        {
            if (files.size() >= 40) break;
            files.push_back ({ f->id, i });
        }
        btAnalitza.setEnabled (! model.analitzant);
        llista.updateContent();
        resized();
        repaint();
    }
    int getNumRows() override { return (int) files.size(); }
    void paintListBoxItem (int fila, juce::Graphics& g, int w, int h, bool) override
    {
        if (fila < 0 || fila >= (int) files.size()) return;
        const auto* i = files[(size_t) fila].second;
        const auto* f = model.fila (files[(size_t) fila].first);
        g.setColour (mixui::colorPrioritat (i->prioritat));
        g.fillRoundedRectangle (12.0f, 9.0f, 4.0f, (float) h - 18.0f, 2.0f);
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (12.5f, true));
        g.drawText (mixui::u (i->titol), 24, 5, w - 32, 18, juce::Justification::centredLeft, true);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.5f));
        g.drawText (f != nullptr ? f->nom : juce::String(), 24, 23, w - 32, 16, juce::Justification::centredLeft, true);
        g.setColour (tema::linia.withAlpha (0.5f));
        g.fillRect (12, h - 1, w - 24, 1);
    }
    void listBoxItemClicked (int fila, const juce::MouseEvent&) override
    {
        if (fila >= 0 && fila < (int) files.size() && onObre) onObre (files[(size_t) fila].first);
    }
};
