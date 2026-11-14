#pragma once
// 0.44: la «Guitarra virtual»: la finestra de l'instrument d'una pista que sona amb un .sf2 de guitarra.
// A l'esquerra, la imatge de la guitarra; a la dreta, quina guitarra i quin so, i els controls.
#include "EstatEstudi.h"
#include "Controls.h"
#include "Pom.h"
#include "Guitarres.h"
#include "Plugins.h"
#include <mibase/Guitarra.h>
#include <mibase/Midi.h>

class PanellGuitarra : public juce::Component, private juce::ChangeListener, private juce::Timer
{
public:
    PanellGuitarra (EstatEstudi& e, std::string idPista) : estat (e), id (std::move (idPista))
    {
        setSize (700, 560 + altMastil + 10);
        startTimerHz (25);   // 0.45: el màstil segueix les notes que sonen
        for (auto* b : { &btGuitarra, &btSo }) { addAndMakeVisible (b); b->setMidaText (13.5f); }
        btGuitarra.onClick = [this] { menuGuitarra(); };
        btSo.onClick = [this] { menuSo(); };
        struct Def { Pom* p; double mibase::ParamsGuitarra::* camp; const char* ajuda; };
        const Def defs[] = {
            { &volum, &mibase::ParamsGuitarra::volumDb, "Volum de la guitarra (doble clic: 0 dB)" },
            { &to, &mibase::ParamsGuitarra::to, "To: com el pom de la guitarra. Cap a l'esquerra, menys aguts" },
            { &atac, &mibase::ParamsGuitarra::atac, "Atac: cap a la dreta es toca més fort; cap a l'esquerra, més suau" },
            { &sustain, &mibase::ParamsGuitarra::sustain, "Sustain: quant queda sonant la nota quan es deixa anar" },
            { &afina, &mibase::ParamsGuitarra::afina, "Afinació fina, en centèsimes de semitò" } };
        for (auto& d : defs)
        {
            addAndMakeVisible (d.p);
            d.p->setTooltip (juce::String::fromUTF8 (d.ajuda));
            d.p->onComenca = [this] { estat.desfes.abans (estat.projecte); };
            d.p->onCanvi = [this, camp = d.camp] (double v) { if (auto* t = pista()) { t->guitarra.*camp = v; estat.avisa(); } };
            d.p->onAcaba = [this] { estat.canviat ({}); };
        }
        addAndMakeVisible (octava);
        octava.setTooltip (juce::String::fromUTF8 ("Octava: puja o baixa totes les notes"));
        octava.onComenca = [this] { estat.desfes.abans (estat.projecte); };
        octava.onCanvi = [this] (double v) { if (auto* t = pista()) { t->guitarra.octava = juce::roundToInt (v); estat.avisa(); } };
        octava.onAcaba = [this] { estat.canviat ({}); };
        estat.addChangeListener (this);
        actualitza();
    }
    ~PanellGuitarra() override { stopTimer(); estat.removeChangeListener (this); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14);
        r.removeFromBottom (altMastil + 10);
        r.removeFromLeft (zonaImatge().getWidth() + 14);
        r.removeFromTop (54);                                   // el títol
        btGuitarra.setBounds (r.removeFromTop (34));
        r.removeFromTop (8);
        btSo.setBounds (r.removeFromTop (34));
        r.removeFromTop (26);
        Pom* fila1[] = { &volum, &to, &atac }, * fila2[] = { &sustain, &octava, &afina };
        for (auto* fila : { fila1, fila2 })
        {
            auto f = r.removeFromTop (118);
            const int w = f.getWidth() / 3;
            for (int k = 0; k < 3; ++k) fila[k]->setBounds (f.removeFromLeft (w).reduced (10, 2));
            r.removeFromTop (14);
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (tema::fons);
        const auto z = zonaImatge();
        vintage::panell (g, z.toFloat(), 8.0f);
        const auto* t = pista();
        mibase::gm::RefSf2 ref;
        const bool es = t != nullptr && mibase::gm::llegeixRef (t->instrument, ref);
        const auto fitxer = juce::String::fromUTF8 (ref.fitxer.c_str());
        if (const auto img = es ? guitarres::imatge (fitxer) : juce::Image(); img.isValid())
            g.drawImage (img, z.reduced (10).toFloat(), juce::RectanglePlacement::centred);
        else
            pintaSilueta (g, z.reduced (26).toFloat(), es && (fitxer.containsIgnoreCase ("acoustic") || fitxer.containsIgnoreCase ("acust")));
        pintaMastil (g);
        auto r = getLocalBounds().reduced (14);
        r.removeFromLeft (z.getWidth() + 14);
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (24.0f, true));
        g.drawText (es ? guitarres::part (t->instrument, false) : juce::String::fromUTF8 ("Guitarra virtual"), r.removeFromTop (34), juce::Justification::centredLeft, true);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.5f));
        g.drawText (juce::String::fromUTF8 ("GUITARRA VIRTUAL"), r.removeFromTop (16), juce::Justification::centredLeft);
        if (! es)
        {
            g.setFont (dibuix::lletra (13.0f));
            g.drawFittedText (juce::String::fromUTF8 ("Aquesta pista no té cap guitarra triada. Tria'n una amb el botó «Guitarra»."), getLocalBounds().removeFromBottom (60).reduced (20, 8),
                              juce::Justification::centred, 2);
        }
        else if (guitarres::imatge (fitxer).isNull())
        {
            g.setFont (dibuix::lletra (11.0f));
            g.drawFittedText (juce::String::fromUTF8 ("Sense imatge: posa'n una a Documents\\Mibase\\Guitarres amb un nom que surti al del fitxer (per exemple Jaguar.png)"),
                              juce::Rectangle<int> (z).removeFromBottom (54).reduced (10, 4), juce::Justification::centred, 3);
        }
    }

private:
    static constexpr int altMastil = 150, numTrasts = 15;
    juce::Rectangle<int> zonaImatge() const { return { 14, 14, 228, getHeight() - 28 - altMastil - 10 }; }
    juce::Rectangle<int> zonaMastil() const { return { 14, getHeight() - 14 - altMastil, getWidth() - 28, altMastil }; }

    /** Les notes de la pista que sonen on és el capçal (amb l'octava de la guitarra aplicada). */
    std::vector<int> notesAra()
    {
        std::vector<int> v;
        const auto* t = pista();
        if (t == nullptr) return v;
        const auto& p = estat.projecte;
        for (auto& c : t->clips)
        {
            if (! c.midi || c.mut || estat.capcal < c.iniciVisible() || estat.capcal >= c.fiVisible()) continue;
            const double tick = mibase::midi::ticks (p, estat.capcal) - mibase::midi::ticks (p, c.pos);
            for (auto& n : c.notes)
                if ((double) n.ini <= tick && tick < (double) (n.ini + n.dur)) v.push_back (n.to + 12 * t->guitarra.octava);
        }
        std::sort (v.begin(), v.end());
        v.erase (std::unique (v.begin(), v.end()), v.end());
        return v;
    }
    void timerCallback() override
    {
        auto ara = notesAra();
        if (ara != notesMastil) { notesMastil = std::move (ara); repaint (zonaMastil()); }
    }

    void pintaMastil (juce::Graphics& g)
    {
        // Un màstil vist com una tablatura: la corda aguda a dalt, la celleta a l'esquerra.
        const auto z = zonaMastil();
        vintage::panell (g, z.toFloat(), 8.0f);
        const auto m = z.reduced (34, 18).withTrimmedLeft (10).toFloat();
        g.setColour (juce::Colour (0xff3a2a1e));
        g.fillRoundedRectangle (m, 3.0f);
        // Posició de cada trast (com en una guitarra: cada cop més junts)
        auto xTrast = [&] (int n) { return m.getX() + m.getWidth() * (float) ((1.0 - std::pow (2.0, -n / 12.0)) / (1.0 - std::pow (2.0, -numTrasts / 12.0))); };
        auto yCorda = [&] (int corda) { return m.getY() + m.getHeight() * ((5 - corda) + 0.5f) / 6.0f; };
        for (int marca : { 3, 5, 7, 9, 12, 15 })
        {
            const float x = (xTrast (marca - 1) + xTrast (marca)) * 0.5f;
            g.setColour (juce::Colours::white.withAlpha (0.16f));
            if (marca == 12) { g.fillEllipse (x - 4.0f, yCorda (4) - 4.0f + m.getHeight() / 12.0f, 8.0f, 8.0f); g.fillEllipse (x - 4.0f, yCorda (1) - 4.0f - m.getHeight() / 12.0f, 8.0f, 8.0f); }
            else g.fillEllipse (x - 4.0f, m.getCentreY() - 4.0f, 8.0f, 8.0f);
            g.setColour (tema::suau);
            g.setFont (dibuix::mono (9.5f));
            g.drawText (juce::String (marca), juce::Rectangle<float> (x - 12.0f, m.getBottom() + 2.0f, 24.0f, 12.0f), juce::Justification::centred);
        }
        for (int n = 0; n <= numTrasts; ++n)
        {
            g.setColour (n == 0 ? juce::Colour (0xffe8e2d0) : juce::Colour (0xff9a9a9a).withAlpha (0.8f));
            g.fillRect (juce::Rectangle<float> (xTrast (n) - (n == 0 ? 2.0f : 0.8f), m.getY(), n == 0 ? 4.0f : 1.6f, m.getHeight()));
        }
        static const char* const noms[6] = { "E", "A", "D", "G", "B", "e" };
        for (int c = 0; c < 6; ++c)
        {
            g.setColour (juce::Colour (0xffc9c2ad).withAlpha (0.9f));
            g.fillRect (juce::Rectangle<float> (m.getX(), yCorda (c) - (c < 3 ? 1.1f : 0.6f), m.getWidth(), c < 3 ? 2.2f : 1.2f));
            g.setColour (tema::suau);
            g.setFont (dibuix::mono (10.5f));
            g.drawText (noms[c], juce::Rectangle<float> ((float) z.getX() + 4.0f, yCorda (c) - 7.0f, 16.0f, 14.0f), juce::Justification::centred);
        }
        // Les notes que sonen
        for (auto& l : mibase::guitarra::alMastil (notesMastil))
        {
            if (l.trast > numTrasts) continue;
            const float x = l.trast == 0 ? m.getX() - 14.0f : (xTrast (l.trast - 1) + xTrast (l.trast)) * 0.5f;
            const auto c = juce::Rectangle<float> (18.0f, 18.0f).withCentre ({ x, yCorda (l.corda) });
            g.setColour (tema::accent);
            g.fillEllipse (c);
            g.setColour (juce::Colours::black);
            g.setFont (dibuix::lletra (9.5f, true));
            auto nom = juce::String::fromUTF8 (mibase::midi::nomNota (l.to).c_str());
            while (nom.isNotEmpty() && (juce::CharacterFunctions::isDigit (nom.getLastCharacter()) || nom.getLastCharacter() == '-')) nom = nom.dropLastCharacters (1);
            g.drawText (nom, c, juce::Justification::centred, false);
        }
    }
    std::vector<int> notesMastil;
    mibase::Pista* pista()
    {
        for (auto& t : estat.projecte.pistes) if (t.id == id) return &t;
        return nullptr;
    }
    void changeListenerCallback (juce::ChangeBroadcaster*) override { actualitza(); }

    void actualitza()
    {
        const auto* t = pista();
        const mibase::ParamsGuitarra p = t != nullptr ? t->guitarra : mibase::ParamsGuitarra();
        volum.setValor (p.volumDb, true);
        to.setValor (p.to, true);
        atac.setValor (p.atac, true);
        sustain.setValor (p.sustain, true);
        octava.setValor (p.octava, true);
        afina.setValor (p.afina, true);
        const bool es = t != nullptr && t->instrument.rfind ("sf2:", 0) == 0;
        btGuitarra.setButtonText (juce::String::fromUTF8 ("Guitarra: ") + (es ? guitarres::part (t->instrument, false) : juce::String::fromUTF8 ("cap")));
        btSo.setButtonText (juce::String::fromUTF8 ("So: ") + (es ? guitarres::part (t->instrument, true) : juce::String::fromUTF8 ("—")));
        btSo.setEnabled (es);
        repaint();
    }

    void posaInstrument (const std::string& ins)
    {
        auto* t = pista();
        if (t == nullptr || ins.empty() || t->instrument == ins) return;
        estat.desfes.abans (estat.projecte);
        t->instrument = ins;
        t->nomInstrument.clear();
        t->estatInstrument.clear();
        estat.canviat (juce::String::fromUTF8 ("Guitarra: ") + juce::String::fromUTF8 (mibase::midi::nomInstrument (ins).c_str()));
    }

    void menuGuitarra()
    {
        // Canviar de guitarra conserva el tipus de so si la nova també el té (el mateix banc i programa); si no, el primer.
        auto fitxers = guitarres::carpeta().findChildFiles (juce::File::findFiles, false, "*.sf2");
        fitxers.sort();
        mibase::gm::RefSf2 ara;
        const auto* t = pista();
        const bool es = t != nullptr && mibase::gm::llegeixRef (t->instrument, ara);
        juce::PopupMenu m;
        for (int i = 0; i < fitxers.size(); ++i)
            m.addItem (1 + i, guitarres::nomGuitarra (fitxers[i].getFileName()), true, es && ara.fitxer == fitxers[i].getFileName().toStdString());
        if (fitxers.isEmpty()) m.addItem (-1, juce::String::fromUTF8 ("No hi ha cap .sf2 a Documents\\Mibase\\SoundFonts\\Guitarres"), false);
        m.addSeparator();
        m.addItem (9000, juce::String::fromUTF8 ("Obre la carpeta de les guitarres"));
        juce::Component::SafePointer<PanellGuitarra> jo (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btGuitarra), [jo, fitxers, ara, es] (int r)
        {
            if (jo == nullptr || r <= 0) return;
            if (r == 9000) { guitarres::carpeta().createDirectory(); guitarres::carpeta().revealToUser(); return; }
            if (r - 1 >= fitxers.size()) return;
            const auto f = fitxers[r - 1];
            const auto sons = guitarres::sons (f);
            if (sons.empty()) return;
            auto tria = sons.front();
            if (es) for (auto& s : sons) if (s.banc == ara.banc && s.programa == ara.programa) { tria = s; break; }
            jo->posaInstrument (guitarres::instrumentDe (f.getFileName(), tria));
        });
    }

    void menuSo()
    {
        mibase::gm::RefSf2 ara;
        const auto* t = pista();
        if (t == nullptr || ! mibase::gm::llegeixRef (t->instrument, ara)) return;
        const auto f = guitarres::carpeta().getChildFile (juce::String::fromUTF8 (ara.fitxer.c_str()));
        const auto sons = guitarres::sons (f);
        juce::PopupMenu m;
        for (int i = 0; i < (int) sons.size(); ++i)
            m.addItem (1 + i, juce::String::fromUTF8 (sons[(size_t) i].nom.c_str()), true, sons[(size_t) i].banc == ara.banc && sons[(size_t) i].programa == ara.programa);
        if (sons.empty()) m.addItem (-1, juce::String::fromUTF8 ("No trobo el fitxer d'aquesta guitarra"), false);
        juce::Component::SafePointer<PanellGuitarra> jo (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btSo), [jo, f, sons] (int r)
        {
            if (jo == nullptr || r <= 0 || r - 1 >= (int) sons.size()) return;
            jo->posaInstrument (guitarres::instrumentDe (f.getFileName(), sons[(size_t) (r - 1)]));
        });
    }

    /** Una silueta senzilla (no és cap model concret) per quan no hi ha imatge. */
    static void pintaSilueta (juce::Graphics& g, juce::Rectangle<float> z, bool acustica)
    {
        const float cx = z.getCentreX(), w = juce::jmin (z.getWidth(), z.getHeight() * 0.36f);
        const auto cos = juce::Rectangle<float> (cx - w * 0.5f, z.getBottom() - w * 1.25f, w, w * 1.25f);
        g.setColour (tema::linia2);
        g.fillRoundedRectangle (cos, w * (acustica ? 0.46f : 0.3f));
        const auto coll = juce::Rectangle<float> (cx - w * 0.07f, z.getY() + w * 0.3f, w * 0.14f, cos.getY() - z.getY() - w * 0.1f);
        g.fillRect (coll);
        g.fillRoundedRectangle (juce::Rectangle<float> (cx - w * 0.11f, z.getY(), w * 0.22f, w * 0.34f), 4.0f);
        g.setColour (tema::fons);
        if (acustica) g.fillEllipse (juce::Rectangle<float> (w * 0.3f, w * 0.3f).withCentre ({ cx, cos.getY() + cos.getHeight() * 0.42f }));
        else g.fillRoundedRectangle (juce::Rectangle<float> (w * 0.36f, w * 0.08f).withCentre ({ cx, cos.getY() + cos.getHeight() * 0.5f }), 2.0f);
    }

    EstatEstudi& estat;
    std::string id;
    Boto btGuitarra { "Guitarra", std::nullopt, Boto::Estil::Secundari };
    Boto btSo { "So", std::nullopt, Boto::Estil::Secundari };
    Pom volum { "Volum", -24.0, 6.0, 0.0, true, [] (double v) { return (v > 0.05 ? "+" : "") + juce::String (v, 1) + " dB"; } };
    Pom to { "To", 0.0, 1.0, 1.0, false, [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + " %"; } };
    Pom atac { "Atac", -1.0, 1.0, 0.0, true, [] (double v) { return std::abs (v) < 0.02 ? juce::String ("normal") : (v > 0 ? juce::String::fromUTF8 ("fort ") : juce::String::fromUTF8 ("suau ")) + juce::String (juce::roundToInt (std::abs (v) * 100.0)); } };
    Pom sustain { "Sustain", 0.0, 1.0, 0.5, true, [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + " %"; } };
    Pom octava { "Octava", -2.0, 2.0, 0.0, true, [] (double v) { const int o = juce::roundToInt (v); return (o > 0 ? "+" : "") + juce::String (o); } };
    Pom afina { juce::String::fromUTF8 ("Afinació"), -50.0, 50.0, 0.0, true, [] (double v) { return (v > 0.5 ? "+" : "") + juce::String (juce::roundToInt (v)) + " c"; } };
};

class FinestraGuitarra : public juce::DocumentWindow
{
public:
    bool keyPressed (const juce::KeyPress& k) override { return k == juce::KeyPress::escapeKey && cridaEscGlobal(); }
    FinestraGuitarra (EstatEstudi& e, const std::string& id, const juce::String& titol, std::function<void()> enTancar)
        : juce::DocumentWindow (titol, tema::fons, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
          tancar (std::move (enTancar))
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new PanellGuitarra (e, id), true);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }
    void closeButtonPressed() override { juce::MessageManager::callAsync ([t = tancar] { t(); }); }
private:
    std::function<void()> tancar;
};
