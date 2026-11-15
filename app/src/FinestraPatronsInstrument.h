#pragma once
// 0.51: la biblioteca de patrons de guitarra o de piano, com la de bateria: una finestra a sobre de l'estudi.
// A l'esquerra la llista (clic = tria, doble clic = escolta); a la dreta, la variació per a la part de la cançó,
// com es toquen els acords (guitarra) o quines mans (piano), un dibuix del que sonarà amb els acords de prova,
// Escolta / Bucle / Para i «Posa'l a la pista».
#include "Controls.h"
#include "Dibuix.h"
#include "Tema.h"
#include "Text.h"
#include "Vintage.h"
#include <mibase/Guitarra.h>
#include <mibase/PartCanco.h>

class FinestraPatronsInstrument : public juce::Component, private juce::ListBoxModel, private juce::Timer
{
public:
    /** El que sonarà en escoltar: les notes (ticks des de 0), els acords de prova i quants ticks dura. */
    struct Prova { std::vector<mibase::Nota> notes; std::vector<mibase::guitarra::Tram> acords; long long durada = 0; };

    std::function<Prova (const std::string& id, unsigned mans)> prova;
    std::function<void (const std::vector<mibase::Nota>&)> onEscolta;
    std::function<void()> onAtura;
    /** S'ha triat un patró (id sense variació): el Piano Roll el fa seu (i la forma dels de rock). */
    std::function<void (const std::string& id)> onTria;
    /** Posa'l. on: 0 tot el clip, 1 un compàs al capçal, 2 quatre, 3 vuit. Torna el text per mostrar. */
    std::function<juce::String (const std::string& id, unsigned mans, int on)> onPosa;
    std::function<int()> forma;               // guitarra: la forma dels acords (mibase::guitarra::Forma)
    std::function<void (int)> onForma;
    double msPerTick = 0.52;

    static constexpr int ampleLlista = 330, ampleDreta = 470, altDibuix = 150;

    FinestraPatronsInstrument (bool esPiano, const std::string& triat) : piano (esPiano)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &cerca, &btGrup, &llista, &selVariacio, &selForma, &btDreta, &btEsquerra,
                                                                 &selOn, &btEscolta, &btBucle, &btAtura, &btPosa })
            addAndMakeVisible (c);
        auto& m = memoria();
        // el patró que l'editor ja tenia triat (sense la variació)
        if (! triat.empty() && mibase::guitarra::patro (triat) != nullptr) m.id = triat.substr (0, triat.rfind ('~') == std::string::npos ? triat.size() : triat.rfind ('~'));
        if (mibase::guitarra::patro (m.id) == nullptr || mibase::guitarra::patro (m.id)->piano != piano) m.id = totsElsPatrons().front()->id;

        cerca.setFont (dibuix::lletra (13.5f));
        cerca.setTextToShowWhenEmpty ("Cerca pel nom…"_ca, tema::suau);
        cerca.setColour (juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha (0.4f));
        cerca.setColour (juce::TextEditor::textColourId, tema::text);
        cerca.setColour (juce::TextEditor::outlineColourId, tema::linia2);
        cerca.onTextChange = [this] { filtra(); };
        btGrup.onClick = [this] { menuGrups(); };

        llista.setModel (this);
        llista.setRowHeight (24);
        llista.setColour (juce::ListBox::backgroundColourId, juce::Colours::black.withAlpha (0.28f));
        llista.setColour (juce::ListBox::outlineColourId, tema::linia);
        llista.setOutlineThickness (1);

        selVariacio.setLlum (vintage::ledGroc);
        selVariacio.setTriat (m.part);
        selVariacio.onCanvi = [this] (int v) { memoria().part = v; canviat(); };

        selForma.setLlum (vintage::ledTaronja);
        selForma.setVisible (! piano);
        selForma.setTooltip ("Com es toquen els acords: oberts, powerchords (fonamental i quinta) o octaves"_ca);
        selForma.onCanvi = [this] (int f) { if (onForma) onForma (f); canviat(); };
        for (auto* b : { &btDreta, &btEsquerra })
        {
            b->setVisible (piano);
            b->setLlum (b == &btDreta ? tema::midi : tema::accent);
            b->setTooltip ("Encesa: aquesta mà es posa a la pista (i és la que se sent en escoltar)"_ca);
        }
        btDreta.onClick = [this] { memoria().mans ^= 1u; if (memoria().mans == 0) memoria().mans = 2u; canviat(); };
        btEsquerra.onClick = [this] { memoria().mans ^= 2u; if (memoria().mans == 0) memoria().mans = 1u; canviat(); };

        selOn.setLlum (vintage::ledTaronja);
        selOn.setTriat (m.on);
        selOn.setTooltip ("On es posa: a tot el clip, o 1, 4 o 8 compassos a partir del compàs on hi ha el capçal. Els acords són els del clip"_ca);
        selOn.onCanvi = [this] (int o) { memoria().on = o; };

        btEscolta.setTooltip ("Escolta'l una vegada amb els acords de prova (també amb doble clic a la llista)"_ca);
        btBucle.setTooltip ("Escolta'l en bucle: si canvies de patró, de variació o d'acords, se sent a la volta següent"_ca);
        btAtura.setTooltip ("Para l'escolta"_ca);
        btBucle.setLlum (tema::accent);
        btEscolta.onClick = [this] { posaBucle (false); sona(); };
        btBucle.onClick = [this] { posaBucle (! bucle); if (bucle) sona(); else if (onAtura) onAtura(); };
        btAtura.onClick = [this] { posaBucle (false); if (onAtura) onAtura(); };
        btPosa.setTooltip ("Posa al clip aquest patró amb la variació triada. Les notes segueixen els acords del clip"_ca);
        btPosa.onClick = [this]
        {
            if (! onPosa) return;
            avis = onPosa (idVariacio(), mans(), selOn.getTriat());
            repaint();
        };
        setSize (10 + ampleLlista + 12 + ampleDreta + 10, 540);
        filtra();
    }

    ~FinestraPatronsInstrument() override { stopTimer(); if (onAtura) onAtura(); }

    /** Cal cridar-la un cop posats els ganxos (dibuix i forma). */
    void comenca() { canviat(); }

    void resized() override
    {
        auto tot = getLocalBounds().reduced (10);
        auto esq = tot.removeFromLeft (ampleLlista);
        tot.removeFromLeft (12);
        esq.removeFromTop (22);
        cerca.setBounds (esq.removeFromTop (28)); esq.removeFromTop (6);
        btGrup.setBounds (esq.removeFromTop (28)); esq.removeFromTop (6);
        llista.setBounds (esq);

        auto d = tot;
        d.removeFromTop (22 + 40 + 4);          // nom, grup i dibuix (pintats)
        d.removeFromTop (18);                   // títol «Variació»
        selVariacio.setBounds (d.removeFromTop (28));
        d.removeFromTop (20 + 4);               // què fa la variació
        d.removeFromTop (18);                   // títol «Acords» / «Mans»
        {
            auto f = d.removeFromTop (28);
            selForma.setBounds (f);
            btDreta.setBounds (f.removeFromLeft (120)); f.removeFromLeft (6);
            btEsquerra.setBounds (f.removeFromLeft (120));
        }
        d.removeFromTop (10);
        zonaDibuix = d.removeFromTop (altDibuix);
        auto baix = d.removeFromBottom (30);
        btEscolta.setBounds (baix.removeFromLeft (74)); baix.removeFromLeft (5);
        btBucle.setBounds (baix.removeFromLeft (64)); baix.removeFromLeft (5);
        btAtura.setBounds (baix.removeFromLeft (58));
        btPosa.setBounds (baix.removeFromRight (150));
        d.removeFromBottom (8);
        selOn.setBounds (d.removeFromBottom (28));
        d.removeFromBottom (18);                // títol «On es posa»
        zonaAvis = d;
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (tema::superficie);
        auto titol = [&] (juce::Rectangle<int> r, const juce::String& t)
        {
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (10.5f, true));
            g.drawText (t, r, juce::Justification::centredLeft);
        };
        titol ({ 10, 10, ampleLlista, 16 }, (piano ? "PATRONS DE PIANO · "_ca : "PATRONS DE GUITARRA · "_ca)
                                              + juce::String ((int) visibles.size()) + " de " + juce::String ((int) totsElsPatrons().size()));
        const int x = 10 + ampleLlista + 12, w = ampleDreta;
        const auto* p = mibase::guitarra::patro (idVariacio());
        if (p == nullptr) return;
        const auto* base = mibase::guitarra::patro (memoria().id);
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (16.0f, true));
        g.drawFittedText (juce::String::fromUTF8 (base != nullptr ? base->nom.c_str() : p->nom.c_str()), x, 8, w, 22, juce::Justification::centredLeft, 1, 0.8f);
        g.setColour (tema::accent);
        g.setFont (dibuix::lletra (12.5f));
        g.drawText (juce::String::fromUTF8 (p->grup.c_str()) + (p->compas.empty() ? juce::String() : " · " + juce::String (p->compas)), x, 32, w, 16, juce::Justification::centredLeft);
        g.setColour (tema::textMig);
        g.setFont (dibuix::lletra (12.0f));
        g.drawFittedText (juce::String::fromUTF8 (p->dibuix.c_str()), x, 48, w, 18, juce::Justification::centredLeft, 1, 0.7f);

        titol ({ x, selVariacio.getY() - 18, w, 16 }, "VARIACIÓ (PER A QUINA PART DE LA CANÇÓ)"_ca);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.5f));
        g.drawFittedText (juce::String::fromUTF8 (mibase::ajudaPart (part(), piano)), x, selVariacio.getBottom() + 3, w, 18, juce::Justification::centredLeft, 1, 0.8f);
        titol ({ x, selForma.getY() - 18, w, 16 }, piano ? "MANS QUE ES POSEN"_ca : "ACORDS"_ca);
        titol ({ x, selOn.getY() - 18, w, 16 }, "ON ES POSA"_ca);

        pintaDibuix (g);
        if (avis.isNotEmpty())
        {
            g.setColour (tema::guitarra);
            g.setFont (dibuix::lletra (12.0f));
            g.drawFittedText (avis, zonaAvis.reduced (0, 4), juce::Justification::centredLeft, 3, 0.9f);
        }
    }

private:
    /** El que es recorda mentre el programa és obert (una per a la guitarra i una per al piano). */
    struct Memoria { std::string id, grup; int part = 0, on = 0; unsigned mans = 3u; };
    Memoria& memoria() const { static Memoria g, p; return piano ? p : g; }

    mibase::PartCanco part() const { return (mibase::PartCanco) juce::jlimit (0, mibase::numPartsCanco - 1, memoria().part); }
    std::string idVariacio() const
    {
        const std::string suf = mibase::sufixPart (part());
        return suf.empty() ? memoria().id : memoria().id + "~" + suf;
    }
    unsigned mans() const { return piano ? memoria().mans : 3u; }

    std::vector<const mibase::guitarra::Patro*> totsElsPatrons() const
    {
        std::vector<const mibase::guitarra::Patro*> v;
        if (piano) { for (auto& p : mibase::guitarra::patronsPiano()) v.push_back (&p); return v; }
        for (auto& p : mibase::guitarra::propis()) v.push_back (&p);
        for (auto& p : mibase::guitarra::patrons()) v.push_back (&p);
        return v;
    }
    std::vector<std::string> grups() const
    {
        std::vector<std::string> g;
        for (auto* p : totsElsPatrons()) if (std::find (g.begin(), g.end(), p->grup) == g.end()) g.push_back (p->grup);
        return g;
    }
    static juce::String nomGrup (const std::string& g)
    {
        auto s = juce::String::fromUTF8 (g.c_str());
        for (auto* pre : { "Piano: ", "Guitarra: " }) if (s.startsWith (pre)) s = s.substring ((int) std::strlen (pre));
        return s.isEmpty() ? s : s.substring (0, 1).toUpperCase() + s.substring (1);
    }

    void menuGrups()
    {
        juce::PopupMenu m;
        m.addItem (1, "Tots els grups"_ca, true, memoria().grup.empty());
        const auto g = grups();
        for (size_t k = 0; k < g.size(); ++k) m.addItem (10 + (int) k, nomGrup (g[k]), true, memoria().grup == g[k]);
        juce::Component::SafePointer<FinestraPatronsInstrument> jo (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btGrup), [jo, g] (int r)
        {
            if (jo == nullptr || r <= 0) return;
            jo->memoria().grup = r == 1 ? std::string() : g[(size_t) (r - 10)];
            jo->filtra();
        });
    }

    void filtra()
    {
        const auto& m = memoria();
        btGrup.setButtonText (m.grup.empty() ? "Grup: tots"_ca : "Grup: "_ca + nomGrup (m.grup));
        const auto text = cerca.getText().trim();
        visibles.clear();
        for (auto* p : totsElsPatrons())
        {
            if (! m.grup.empty() && p->grup != m.grup) continue;
            if (text.isNotEmpty() && ! juce::String::fromUTF8 (p->nom.c_str()).containsIgnoreCase (text)) continue;
            visibles.push_back (p);
        }
        llista.updateContent();
        const auto it = std::find_if (visibles.begin(), visibles.end(), [&] (auto* p) { return p->id == m.id; });
        if (it != visibles.end()) { llista.selectRow ((int) (it - visibles.begin()), true); llista.scrollToEnsureRowIsOnscreen ((int) (it - visibles.begin())); }
        else llista.deselectAllRows();
        llista.repaint();
        repaint();
    }

    /** Ha canviat el patró, la variació, la forma o les mans: es refà el dibuix. */
    void canviat()
    {
        if (forma) selForma.setTriat (forma());
        btDreta.setToggleState ((memoria().mans & 1u) != 0, juce::dontSendNotification);
        btEsquerra.setToggleState ((memoria().mans & 2u) != 0, juce::dontSendNotification);
        dibuixProva = prova ? prova (idVariacio(), mans()) : Prova();
        avis = juce::String();
        repaint();
    }

    void pintaDibuix (juce::Graphics& g)
    {
        // Un petit piano roll amb el que sonarà: quatre compassos amb els acords de prova (els del clip, o els de la tonalitat).
        const auto z = zonaDibuix;
        g.setColour (juce::Colours::black.withAlpha (0.30f));
        g.fillRect (z);
        const auto& pr = dibuixProva;
        if (pr.durada <= 0) return;
        const auto zn = z.withTrimmedTop (16).reduced (2, 3);
        int lo = 127, hi = 0;
        for (auto& n : pr.notes) { lo = std::min (lo, n.to); hi = std::max (hi, n.to); }
        if (lo > hi) { lo = 48; hi = 72; }
        lo -= 2; hi += 2;
        const float hNota = juce::jlimit (2.0f, 7.0f, (float) zn.getHeight() / (float) (hi - lo + 1));
        const float alt = hNota * (float) (hi - lo + 1), y0 = (float) zn.getY() + ((float) zn.getHeight() - alt) * 0.5f;
        auto xDe = [&] (long long t) { return (float) zn.getX() + (float) zn.getWidth() * (float) t / (float) pr.durada; };
        // els acords (a dalt) i les seves línies
        g.setFont (dibuix::lletra (11.0f, true));
        for (auto& a : pr.acords)
        {
            const float xa = xDe (a.ini);
            g.setColour (tema::linia2);
            g.fillRect (xa, (float) z.getY(), 1.0f, (float) z.getHeight());
            g.setColour (tema::bateria);
            g.drawText (juce::String::fromUTF8 (a.acord.c_str()), juce::Rectangle<float> (xa + 4.0f, (float) z.getY() + 1.0f, xDe (a.fi) - xa - 6.0f, 14.0f), juce::Justification::centredLeft, true);
        }
        const auto col = piano ? tema::midi : tema::guitarra;
        for (auto& n : pr.notes)
        {
            const float vel = (float) n.vel / 127.0f;
            const float x1 = xDe (n.ini), x2 = std::max (x1 + 2.0f, xDe (n.ini + n.dur) - 1.0f);
            const float y = y0 + (float) (hi - n.to) * hNota;
            g.setColour (col.withAlpha (0.30f + 0.70f * vel));
            g.fillRect (x1, y, x2 - x1, std::max (1.5f, hNota - 1.0f));
        }
    }

    // ---- la llista ----
    int getNumRows() override { return (int) visibles.size(); }
    void paintListBoxItem (int fila, juce::Graphics& g, int w, int h, bool triada) override
    {
        if (fila < 0 || fila >= (int) visibles.size()) return;
        const auto* p = visibles[(size_t) fila];
        if (triada) { g.setColour (tema::triat); g.fillRect (0, 0, w, h); g.setColour (tema::accent); g.fillRect (0, 0, 3, h); }
        g.setColour (triada ? tema::text : tema::textMig);
        g.setFont (dibuix::lletra (13.0f));
        g.drawText (juce::String::fromUTF8 (p->nom.c_str()), 10, 0, w - 16, h, juce::Justification::centredLeft, true);
    }
    void selectedRowsChanged (int fila) override
    {
        if (fila < 0 || fila >= (int) visibles.size() || visibles[(size_t) fila]->id == memoria().id) return;
        tria (fila);
    }
    void listBoxItemDoubleClicked (int fila, const juce::MouseEvent&) override
    {
        if (fila < 0 || fila >= (int) visibles.size()) return;
        if (visibles[(size_t) fila]->id != memoria().id) tria (fila);
        sona();
    }
    juce::String getTooltipForRow (int fila) override
    {
        if (fila < 0 || fila >= (int) visibles.size()) return {};
        return nomGrup (visibles[(size_t) fila]->grup) + " · "_ca + juce::String::fromUTF8 (visibles[(size_t) fila]->dibuix.c_str());
    }
    void tria (int fila)
    {
        memoria().id = visibles[(size_t) fila]->id;
        if (onTria) onTria (memoria().id);   // pot canviar la forma (els de rock porten powerchords)
        canviat();
    }

    // ---- escoltar ----
    void sona()
    {
        if (! onEscolta) return;
        if (dibuixProva.notes.empty()) { avis = "Aquest patró no fa cap nota amb aquests acords"_ca; repaint(); return; }
        onEscolta (dibuixProva.notes);
        if (bucle) startTimer (juce::jmax (200, juce::roundToInt ((double) dibuixProva.durada * msPerTick)));
    }
    void posaBucle (bool b)
    {
        bucle = b;
        btBucle.setToggleState (b, juce::dontSendNotification);
        if (! b) stopTimer();
    }
    void timerCallback() override { if (bucle) sona(); else stopTimer(); }

    const bool piano;
    juce::TextEditor cerca;
    Boto btGrup { "Grup: tots", std::nullopt, Boto::Estil::Secundari };
    juce::ListBox llista;
    Segmentat selVariacio { { "Original", "Estrofa", "Pre-torn.", "Tornada", "Pont" } };
    Segmentat selForma { { "Oberts", "Powerchords", "Oct. greus", "Oct. agudes" } };
    Boto btDreta { juce::String::fromUTF8 ("Mà dreta"), std::nullopt, Boto::Estil::Tecla };
    Boto btEsquerra { juce::String::fromUTF8 ("Mà esquerra"), std::nullopt, Boto::Estil::Tecla };
    Segmentat selOn { { "Tot el clip", juce::String::fromUTF8 ("1 compàs"), "4 compassos", "8 compassos" } };
    Boto btEscolta { "Escolta", std::nullopt, Boto::Estil::Secundari };
    Boto btBucle { "Bucle", std::nullopt, Boto::Estil::Tecla };
    Boto btAtura { "Para", std::nullopt, Boto::Estil::Secundari };
    Boto btPosa { "Posa'l a la pista", std::nullopt, Boto::Estil::Primari };
    std::vector<const mibase::guitarra::Patro*> visibles;
    Prova dibuixProva;
    juce::Rectangle<int> zonaDibuix, zonaAvis;
    juce::String avis;
    bool bucle = false;
};
