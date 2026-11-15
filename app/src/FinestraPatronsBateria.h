#pragma once
// 0.50: la biblioteca de patrons de bateria (150 ritmes de 4 compassos). Una finestra a sobre de l'estudi, com els altres
// editors de patrons: a l'esquerra la llista (clic = tria, doble clic = escolta); a la dreta, la variació (estrofa, tornada…),
// les capes que es posaran (bombo, caixa, hihat, plats, toms), el dibuix del patró i els botons d'escoltar i de posar-lo.
#include "Controls.h"
#include "Dibuix.h"
#include "Tema.h"
#include "Text.h"
#include "Vintage.h"
#include <mibase/PatronsBateria.h>

class FinestraPatronsBateria : public juce::Component, private juce::ListBoxModel, private juce::Timer
{
public:
    using Variacio = mibase::bateria::Variacio;
    static constexpr int ampleLlista = 318, ampleDreta = 452, ampleNoms = 52, ampleCasella = 6, altFilaGraella = 15;

    /** Les notes a escoltar (a partir del tick 0). */
    std::function<void (const std::vector<mibase::Nota>&)> onEscolta;
    std::function<void()> onAtura;
    /** Posa'l a la pista. «on»: 0 tot el clip, 1 quatre compassos des del capçal, 2 vuit. Torna el text per mostrar. */
    std::function<juce::String (const mibase::bateria::PatroBat&, Variacio, unsigned capes, int on)> onPosa;
    /** Les files de la pista (per saber quina nota toca cada so). */
    std::function<std::vector<mibase::FilaBateria>()> files;
    double msPerTick = 0.52;
    long long ticksCompas = 4 * mibase::ticksPerNegra;

    FinestraPatronsBateria()
    {
        for (auto* c : std::initializer_list<juce::Component*> { &cerca, &selGrup, &llista, &selVariacio, &btTot, &selOn, &btEscolta, &btBucle, &btAtura, &btPosa })
            addAndMakeVisible (c);
        cerca.setFont (dibuix::lletra (13.5f));
        cerca.setTextToShowWhenEmpty ("Cerca pel nom o per la cançó…"_ca, tema::suau);
        cerca.setColour (juce::TextEditor::backgroundColourId, juce::Colours::black.withAlpha (0.4f));
        cerca.setColour (juce::TextEditor::textColourId, tema::text);
        cerca.setColour (juce::TextEditor::outlineColourId, tema::linia2);
        cerca.onTextChange = [this] { filtra(); };
        selGrup.setLlum (vintage::ledTaronja);
        selGrup.setTriat (memoria().grup);
        selGrup.onCanvi = [this] (int g) { memoria().grup = g; filtra(); };

        llista.setModel (this);
        llista.setRowHeight (24);
        llista.setColour (juce::ListBox::backgroundColourId, juce::Colours::black.withAlpha (0.28f));
        llista.setColour (juce::ListBox::outlineColourId, tema::linia);
        llista.setOutlineThickness (1);

        selVariacio.setLlum (vintage::ledGroc);
        selVariacio.setTriat (memoria().variacio);
        selVariacio.onCanvi = [this] (int v) { memoria().variacio = v; canviat(); };
        for (int k = 0; k < mibase::bateria::numCapes; ++k)
        {
            auto* b = capes.add (new Boto (juce::String::fromUTF8 (mibase::bateria::nomCapa (k)), std::nullopt, Boto::Estil::Tecla));
            b->setLlum (colorCapa (1u << k));
            b->setTooltip ("Encès: aquesta capa es posa a la pista (i és la que se sent en escoltar). El que la pista ja tenia de les capes apagades no es toca"_ca);
            b->onClick = [this, k] { memoria().capes ^= (1u << k); canviat(); };
            addAndMakeVisible (b);
        }
        btTot.setTooltip ("Totes les capes"_ca);
        btTot.onClick = [this] { memoria().capes = mibase::bateria::CapaTot; canviat(); };
        selOn.setLlum (vintage::ledTaronja);
        selOn.setTriat (memoria().on);
        selOn.setTooltip ("On es posa: a tot el clip (repetit cada 4 compassos), o 4 o 8 compassos a partir del compàs on hi ha el capçal"_ca);
        selOn.onCanvi = [this] (int o) { memoria().on = o; };

        btEscolta.setTooltip ("Escolta'l una vegada (també amb doble clic a la llista)"_ca);
        btBucle.setTooltip ("Escolta'l en bucle: si canvies de patró, de variació o de capes, se sent a la volta següent"_ca);
        btAtura.setTooltip ("Para l'escolta"_ca);
        btBucle.setLlum (tema::accent);
        btEscolta.onClick = [this] { posaBucle (false); sona(); };
        btBucle.onClick = [this] { posaBucle (! bucle); if (bucle) sona(); else if (onAtura) onAtura(); };
        btAtura.onClick = [this] { posaBucle (false); if (onAtura) onAtura(); };
        btPosa.setTooltip ("Posa al clip les capes enceses d'aquest patró, amb la variació triada"_ca);
        btPosa.onClick = [this]
        {
            if (auto* p = patro(); p != nullptr && onPosa)
            {
                if ((capesActives() & mibase::bateria::CapaTot) == 0) { avis = "Encén alguna capa que aquest patró tingui"_ca; repaint(); return; }
                avis = onPosa (*p, variacio(), capesActives(), selOn.getTriat());
                repaint();
            }
        };
        setSize (10 + ampleLlista + 12 + ampleDreta + 10, 530);
        filtra();
        canviat();
    }

    ~FinestraPatronsBateria() override { stopTimer(); if (onAtura) onAtura(); }

    void resized() override
    {
        auto tot = getLocalBounds().reduced (10);
        auto esq = tot.removeFromLeft (ampleLlista);
        tot.removeFromLeft (12);
        esq.removeFromTop (22);
        cerca.setBounds (esq.removeFromTop (28)); esq.removeFromTop (6);
        selGrup.setBounds (esq.removeFromTop (28)); esq.removeFromTop (6);
        llista.setBounds (esq);

        auto d = tot;
        d.removeFromTop (22 + 62 + 6);          // nom, referència i anotació (pintats)
        d.removeFromTop (18);                   // títol «Variació»
        selVariacio.setBounds (d.removeFromTop (28));
        d.removeFromTop (20 + 4);               // què fa la variació
        d.removeFromTop (18);                   // títol «Capes»
        {
            auto f = d.removeFromTop (30);
            btTot.setBounds (f.removeFromRight (52)); f.removeFromRight (6);
            const int w = (f.getWidth() - 4 * 5) / mibase::bateria::numCapes;
            for (int k = 0; k < capes.size(); ++k) { capes[k]->setBounds (f.removeFromLeft (w)); f.removeFromLeft (5); }
        }
        d.removeFromTop (10);
        zonaGraella = d.removeFromTop (16 + mibase::bateria::numPistesPatro * altFilaGraella + 4);
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
        namespace bt = mibase::bateria;
        g.fillAll (tema::superficie);
        auto titol = [&] (juce::Rectangle<int> r, const juce::String& t)
        {
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (10.5f, true));
            g.drawText (t, r, juce::Justification::centredLeft);
        };
        titol ({ 10, 10, ampleLlista, 16 }, "PATRONS DE BATERIA · "_ca + juce::String ((int) visibles.size()) + " de 150");
        const int x = 10 + ampleLlista + 12, w = ampleDreta;
        const auto* p = patro();
        if (p == nullptr)
        {
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (13.0f));
            g.drawText ("Tria un patró de la llista"_ca, x, 10, w, 22, juce::Justification::centredLeft);
            return;
        }
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (16.0f, true));
        g.drawText (juce::String (p->num).paddedLeft ('0', 3) + "  " + juce::String::fromUTF8 (p->nom), x, 8, w, 22, juce::Justification::centredLeft);
        g.setColour (tema::accent);
        g.setFont (dibuix::lletra (12.5f));
        g.drawText (juce::String::fromUTF8 (p->ref), x, 32, w, 18, juce::Justification::centredLeft);
        g.setColour (tema::textMig);
        g.drawFittedText (juce::String::fromUTF8 (p->ajuda), x, 50, w, 40, juce::Justification::topLeft, 3, 0.9f);

        titol ({ x, selVariacio.getY() - 18, w, 16 }, "VARIACIÓ (PER A QUINA PART DE LA CANÇÓ)"_ca);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.5f));
        g.drawFittedText (juce::String::fromUTF8 (bt::ajudaVariacio (variacio())), x, selVariacio.getBottom() + 3, w, 18, juce::Justification::centredLeft, 1, 0.8f);
        titol ({ x, btTot.getY() - 18, w, 16 }, "CAPES QUE ES POSEN"_ca);
        titol ({ x, selOn.getY() - 18, w, 16 }, "ON ES POSA"_ca);

        // El dibuix del patró: quatre compassos; com més fort el cop, més viu el color. Les capes apagades, grises.
        static const char* const noms[bt::numPistesPatro] = { "Hihat", "Ride", "Crash", "Caixa", "Bombo", "Tom 1", "Tom 2", "Goliat" };
        const auto gr = bt::graella (*p, variacio());
        const unsigned actives = capesActives();
        const int gx = zonaGraella.getX() + ampleNoms, gy = zonaGraella.getY() + 16;
        g.setFont (dibuix::lletra (10.0f));
        for (int b = 0; b < bt::compassosPatro; ++b)
        {
            g.setColour (tema::suau);
            g.drawText (juce::String (b + 1), gx + b * bt::casellesPatro * ampleCasella + 2, zonaGraella.getY(), 30, 14, juce::Justification::centredLeft);
        }
        for (int i = 0; i < bt::numPistesPatro; ++i)
        {
            const int y = gy + i * altFilaGraella;
            const bool activa = (bt::capaDePista (i) & actives) != 0;
            const bool teCops = gr.p[(size_t) i].find_first_not_of ('-') != std::string::npos;
            g.setColour (i % 2 == 0 ? juce::Colours::black.withAlpha (0.30f) : juce::Colours::black.withAlpha (0.20f));
            g.fillRect (gx, y, bt::llargPatro * ampleCasella, altFilaGraella - 1);
            g.setColour (teCops ? (activa ? tema::textMig : tema::suau.withAlpha (0.6f)) : tema::linia2);
            g.setFont (dibuix::lletra (11.0f));
            g.drawText (noms[i], zonaGraella.getX(), y, ampleNoms - 4, altFilaGraella - 1, juce::Justification::centredLeft);
            const auto col = activa ? colorCapa (bt::capaDePista (i)) : tema::linia2;
            for (int k = 0; k < bt::llargPatro; ++k)
            {
                const char ch = gr.p[(size_t) i][(size_t) k];
                if (ch == '-') continue;
                const float vel = (float) bt::velocitatPatro (*p, gr, variacio(), i, k) / 127.0f;
                const float alt = ch == 'g' ? 5.0f : 4.0f + 9.0f * vel;
                juce::Rectangle<float> c ((float) (gx + k * ampleCasella), (float) y + ((float) altFilaGraella - 1.0f - alt) * 0.5f, (float) ampleCasella - 1.0f, alt);
                g.setColour (col.withAlpha (activa ? 0.35f + 0.65f * vel : 0.8f));
                if (ch == 'o') g.drawRect (c, 1.2f);      // hihat obert: buit per dins
                else g.fillRect (c);
            }
        }
        for (int k = 0; k <= bt::llargPatro; k += 4)
        {
            g.setColour (k % bt::casellesPatro == 0 ? tema::linia2 : tema::linia.withAlpha (0.5f));
            g.fillRect (gx + k * ampleCasella - (k % bt::casellesPatro == 0 ? 1 : 0), gy, 1, bt::numPistesPatro * altFilaGraella - 1);
        }
        if (avis.isNotEmpty())
        {
            g.setColour (tema::guitarra);
            g.setFont (dibuix::lletra (12.0f));
            g.drawFittedText (avis, zonaAvis.reduced (0, 4), juce::Justification::centredLeft, 3, 0.9f);
        }
    }

private:
    /** El que es recorda mentre el programa és obert (tornar a obrir la finestra la deixa on era). */
    struct Memoria { int num = 1, grup = 0, variacio = 0, on = 0; unsigned capes = mibase::bateria::CapaTot; };
    static Memoria& memoria() { static Memoria m; return m; }

    static juce::Colour colorCapa (unsigned capa)
    {
        namespace bt = mibase::bateria;
        return capa == bt::CapaBombo ? tema::accent : capa == bt::CapaCaixa ? tema::guitarra : capa == bt::CapaHihat ? tema::veu
             : capa == bt::CapaPlats ? tema::midi : tema::bateria;
    }

    const mibase::bateria::PatroBat* patro() const { return mibase::bateria::patroBat (memoria().num); }
    Variacio variacio() const { return (Variacio) juce::jlimit (0, mibase::bateria::numVariacions - 1, memoria().variacio); }
    /** Les capes enceses que el patró (amb la variació) té. */
    unsigned capesActives() const
    {
        const auto* p = patro();
        return p != nullptr ? (memoria().capes & mibase::bateria::capesDe (mibase::bateria::graella (*p, variacio()))) : 0u;
    }

    void filtra()
    {
        static const char* const grups[] = { "", "Rock", "Indie", "Pop", "toms" };
        const juce::String g = grups[juce::jlimit (0, 4, selGrup.getTriat())];
        const auto text = cerca.getText().trim();
        visibles.clear();
        for (auto& p : mibase::bateria::patronsBat())
        {
            if (g.isNotEmpty() && ! juce::String::fromUTF8 (p.grup).contains (g)) continue;
            if (text.isNotEmpty() && ! (juce::String::fromUTF8 (p.nom) + " " + juce::String::fromUTF8 (p.ref) + " " + juce::String (p.num).paddedLeft ('0', 3)).containsIgnoreCase (text)) continue;
            visibles.push_back (p.num);
        }
        llista.updateContent();
        const auto it = std::find (visibles.begin(), visibles.end(), memoria().num);
        if (it != visibles.end()) { llista.selectRow ((int) (it - visibles.begin()), true); }
        else llista.deselectAllRows();
        llista.repaint();
        repaint();
    }

    /** Ha canviat el patró, la variació o les capes. */
    void canviat()
    {
        const auto* p = patro();
        const unsigned te = p != nullptr ? mibase::bateria::capesDe (mibase::bateria::graella (*p, variacio())) : 0u;
        for (int k = 0; k < capes.size(); ++k)
        {
            capes[k]->setToggleState ((memoria().capes & (1u << k)) != 0, juce::dontSendNotification);
            capes[k]->setEnabled ((te & (1u << k)) != 0);
        }
        avis = juce::String();
        repaint();
    }

    // ---- la llista ----
    int getNumRows() override { return (int) visibles.size(); }
    void paintListBoxItem (int fila, juce::Graphics& g, int w, int h, bool triada) override
    {
        if (fila < 0 || fila >= (int) visibles.size()) return;
        const auto* p = mibase::bateria::patroBat (visibles[(size_t) fila]);
        if (p == nullptr) return;
        if (triada) { g.setColour (tema::triat); g.fillRect (0, 0, w, h); g.setColour (tema::accent); g.fillRect (0, 0, 3, h); }
        g.setColour (triada ? tema::accent : tema::suau);
        g.setFont (dibuix::mono (11.5f));
        g.drawText (juce::String (p->num).paddedLeft ('0', 3), 8, 0, 28, h, juce::Justification::centredLeft);
        g.setColour (triada ? tema::text : tema::textMig);
        g.setFont (dibuix::lletra (13.0f));
        g.drawText (juce::String::fromUTF8 (p->nom), 40, 0, w - 46, h, juce::Justification::centredLeft, true);
    }
    void selectedRowsChanged (int fila) override
    {
        if (fila < 0 || fila >= (int) visibles.size() || visibles[(size_t) fila] == memoria().num) return;
        memoria().num = visibles[(size_t) fila];
        canviat();
    }
    void listBoxItemDoubleClicked (int fila, const juce::MouseEvent&) override
    {
        if (fila < 0 || fila >= (int) visibles.size()) return;
        memoria().num = visibles[(size_t) fila];
        canviat();
        sona();
    }
    juce::String getTooltipForRow (int fila) override
    {
        if (fila < 0 || fila >= (int) visibles.size()) return {};
        const auto* p = mibase::bateria::patroBat (visibles[(size_t) fila]);
        return p != nullptr ? juce::String::fromUTF8 (p->ref) + juce::String::fromUTF8 (" · ") + juce::String::fromUTF8 (p->ajuda) : juce::String();
    }

    // ---- escoltar ----
    void sona()
    {
        const auto* p = patro();
        if (p == nullptr || ! onEscolta) return;
        const unsigned c = capesActives();
        if (c == 0) { avis = "Encén alguna capa que aquest patró tingui"_ca; repaint(); return; }
        onEscolta (mibase::bateria::notesDePatro (*p, variacio(), c, mibase::bateria::compassosPatro, ticksCompas, files ? files() : std::vector<mibase::FilaBateria>(), 0));
        if (bucle) startTimer (juce::jmax (200, juce::roundToInt ((double) (mibase::bateria::compassosPatro * ticksCompas) * msPerTick)));
    }
    void posaBucle (bool b)
    {
        bucle = b;
        btBucle.setToggleState (b, juce::dontSendNotification);
        if (! b) stopTimer();
    }
    void timerCallback() override { if (bucle) sona(); else stopTimer(); }

    juce::TextEditor cerca;
    Segmentat selGrup { { "Tots", "Rock", "Indie", "Pop", "Toms" } };
    juce::ListBox llista;
    Segmentat selVariacio { { "Original", "Estrofa", "Pre-torn.", "Tornada", "Pont", "Sense tall" } };
    juce::OwnedArray<Boto> capes;
    Boto btTot { "Tot", std::nullopt, Boto::Estil::Secundari };
    Segmentat selOn { { "Tot el clip", juce::String::fromUTF8 ("4 compassos al capçal"), juce::String::fromUTF8 ("8 al capçal") } };
    Boto btEscolta { "Escolta", std::nullopt, Boto::Estil::Secundari };
    Boto btBucle { "Bucle", std::nullopt, Boto::Estil::Tecla };
    Boto btAtura { "Para", std::nullopt, Boto::Estil::Secundari };
    Boto btPosa { "Posa'l a la pista", std::nullopt, Boto::Estil::Primari };
    std::vector<int> visibles;
    juce::Rectangle<int> zonaGraella, zonaAvis;
    juce::String avis;
    bool bucle = false;
};
