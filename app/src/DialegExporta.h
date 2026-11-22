#pragma once
// 0.83: finestra d'exportar amb opcions: què (tota la mescla, les pistes triades, un fitxer per pista o per carpeta),
// quin tros (la cançó, lineal o una estructura; el bucle; una part), amb o sense efectes, amb o sense màster, i el format.
#include "Controls.h"
#include "mibase/MotorEstudi.h"
#include "mibase/Edicio.h"

class DialegExporta : public juce::Component
{
public:
    enum Que { Tot = 0, Triades = 1, UnPerPista = 2, UnPerCarpeta = 3 };
    struct Resultat
    {
        int que = Tot;
        std::vector<int> pistes;                 // índexs (Triades / UnPerPista)
        std::vector<std::string> carpetes;       // ids (UnPerCarpeta)
        mibase::OpcionsExport opcions;
        int bits = 24;
    };

    DialegExporta (const mibase::Projecte& p, int pistaTriada, std::function<void (const Resultat&)> enAcceptar)
        : projecte (p), accepta (std::move (enAcceptar)),
          btAccepta ("Exporta", std::nullopt, Boto::Estil::Primari)
    {
        auto prepara = [this] (juce::ComboBox& c)
        {
            c.setColour (juce::ComboBox::textColourId, tema::text);
            addAndMakeVisible (c);
        };
        cbQue.addItem ("Tota la mescla (tal com sona)"_ca, 1);
        cbQue.addItem ("Una mescla amb les pistes triades"_ca, 2);
        cbQue.addItem ("Un fitxer per pista (stems)"_ca, 3);
        cbQue.addItem ("Un fitxer per carpeta (grups)"_ca, 4);
        if (p.carpetes.empty()) cbQue.setItemEnabled (4, false);
        cbQue.setSelectedId (1, juce::dontSendNotification);
        prepara (cbQue);

        cbTros.addItem ("Tota la cançó"_ca, 1);
        cbTros.addItem ("El bucle"_ca, 2);
        cbTros.addItem ("Una part"_ca, 3);
        cbTros.setItemEnabled (2, p.bucle.marcat && p.bucle.a > p.bucle.de);
        cbTros.setItemEnabled (3, ! p.parts.empty());
        cbTros.setSelectedId (p.bucle.marcat && p.bucle.actiu && p.bucle.a > p.bucle.de ? 2 : 1, juce::dontSendNotification);
        prepara (cbTros);

        cbEstructura.addItem ("Lineal (tal com és a la taula)"_ca, 1);
        for (int i = 0; i < (int) p.estructures.size(); ++i)
            cbEstructura.addItem ("Estructura: "_ca + juce::String::fromUTF8 (p.estructures[(size_t) i].nom.c_str()), 2 + i);
        int triadaE = 1;
        for (int i = 0; i < (int) p.estructures.size(); ++i)
            if (p.estructures[(size_t) i].id == p.estructuraActiva) triadaE = 2 + i;
        cbEstructura.setSelectedId (triadaE, juce::dontSendNotification);
        prepara (cbEstructura);

        for (int i = 0; i < (int) p.parts.size(); ++i)
            cbPart.addItem (juce::String::fromUTF8 (p.parts[(size_t) i].nom.c_str()), 1 + i);
        if (! p.parts.empty()) cbPart.setSelectedId (1, juce::dontSendNotification);
        prepara (cbPart);

        cbFormat.addItem ("WAV 24 bits (per mesclar o masteritzar)"_ca, 1);
        cbFormat.addItem ("WAV 16 bits (CD, llocs que ho demanen)"_ca, 2);
        cbFormat.setSelectedId (1, juce::dontSendNotification);
        prepara (cbFormat);

        for (auto* t : { &tgEfectes, &tgMaster })
        {
            t->setToggleState (true, juce::dontSendNotification);
            t->setColour (juce::ToggleButton::textColourId, tema::text);
            t->setColour (juce::ToggleButton::tickColourId, tema::accent);
            addAndMakeVisible (*t);
        }

        // Les pistes que es poden triar (sense busos ni la pista d'acords) i les carpetes
        for (int i = 0; i < (int) p.pistes.size(); ++i)
        {
            const auto& t = p.pistes[(size_t) i];
            if (mibase::esBus (t) || mibase::esAcords (t)) continue;
            auto* b = new juce::ToggleButton (juce::String::fromUTF8 (t.nom.c_str()));
            b->setToggleState (pistaTriada < 0 ? false : i == pistaTriada, juce::dontSendNotification);
            b->setColour (juce::ToggleButton::textColourId, tema::text);
            b->setColour (juce::ToggleButton::tickColourId, tema::accent);
            b->onClick = [this] { actualitza(); };
            indexPistes.push_back (i);
            checksPistes.add (b);
            llista.addAndMakeVisible (b);
        }
        for (const auto& c : p.carpetes)
        {
            auto* b = new juce::ToggleButton (juce::String::fromUTF8 (c.nom.c_str()));
            b->setToggleState (true, juce::dontSendNotification);
            b->setColour (juce::ToggleButton::textColourId, tema::text);
            b->setColour (juce::ToggleButton::tickColourId, tema::accent);
            b->onClick = [this] { actualitza(); };
            idsCarpetes.push_back (c.id);
            checksCarpetes.add (b);
            llista.addAndMakeVisible (b);
        }
        vista.setViewedComponent (&llista, false);
        vista.setScrollBarsShown (true, false);
        addAndMakeVisible (vista);
        addAndMakeVisible (btTotes);
        addAndMakeVisible (btCap);
        btTotes.onClick = [this] { marcaTot (true); };
        btCap.onClick = [this] { marcaTot (false); };

        for (auto* c : { &cbQue, &cbTros, &cbEstructura, &cbPart, &cbFormat }) c->onChange = [this] { actualitza(); };
        addAndMakeVisible (btAccepta);
        addAndMakeVisible (btCancella);
        btAccepta.onClick = [this] { intenta(); };
        btCancella.onClick = [this] { tanca(); };
        setWantsKeyboardFocus (true);
        actualitza();
    }

    static void mostra (juce::Component& sobre, std::unique_ptr<DialegExporta>& propietari)
    {
        auto* arrel = sobre.getTopLevelComponent();
        arrel->addAndMakeVisible (*propietari);
        propietari->setBounds (arrel->getLocalBounds());
        propietari->grabKeyboardFocus();
        propietari->propietariPtr = &propietari;
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xa0060608));
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (panell.toFloat().translated (0, 10).expanded (6), 18.0f);
        g.setColour (tema::superficie);
        g.fillRoundedRectangle (panell.toFloat(), 14.0f);
        g.setColour (tema::linia2);
        g.drawRoundedRectangle (panell.toFloat().reduced (0.5f), 14.0f, 1.0f);
        auto r = panell.reduced (24, 18);
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (18.0f, true));
        g.drawText ("Exporta", r.removeFromTop (28), juce::Justification::centredLeft);
        g.setFont (dibuix::lletra (13.0f, true));
        g.setColour (tema::suau);
        for (auto& [text, y] : etiquetes) g.drawText (text, panell.getX() + 24, y, 110, 30, juce::Justification::centredLeft);
        g.setFont (dibuix::lletra (12.5f));
        g.setColour (avis.isNotEmpty() ? tema::errorText : tema::suau);
        g.drawFittedText (avis.isNotEmpty() ? avis : resum, zonaAvis, juce::Justification::centredLeft, 2, 0.85f);
    }

    void resized() override
    {
        const bool ambLlista = llistaVisible();
        panell = getLocalBounds().withSizeKeepingCentre (juce::jmin (620, getWidth() - 40), juce::jmin (getHeight() - 40, ambLlista ? 640 : 430));
        auto r = panell.reduced (24, 18);
        r.removeFromTop (40);
        etiquetes.clear();
        auto fila = [&] (const juce::String& nom, juce::Component& c, bool visible)
        {
            c.setVisible (visible);
            if (! visible) return;
            auto f = r.removeFromTop (30);
            etiquetes.push_back ({ nom, f.getY() });
            f.removeFromLeft (120);
            c.setBounds (f);
            r.removeFromTop (8);
        };
        const int tros = cbTros.getSelectedId();
        fila ("Què"_ca, cbQue, true);
        fila ("Tros", cbTros, true);
        fila ("Estructura", cbEstructura, tros == 1 && ! projecte.estructures.empty());
        fila ("Part", cbPart, tros == 3);
        fila ("Format", cbFormat, true);
        {
            auto f = r.removeFromTop (28);
            etiquetes.push_back ({ "Inclou", f.getY() });
            f.removeFromLeft (120);
            tgEfectes.setBounds (f.removeFromLeft (190));
            tgMaster.setBounds (f.removeFromLeft (220));
            r.removeFromTop (8);
        }
        auto peu = r.removeFromBottom (44);
        btAccepta.setBounds (peu.removeFromRight (btAccepta.ampladaIdeal()));
        peu.removeFromRight (10);
        btCancella.setBounds (peu.removeFromRight (btCancella.ampladaIdeal()));
        zonaAvis = r.removeFromBottom (40);
        r.removeFromBottom (6);

        vista.setVisible (ambLlista);
        btTotes.setVisible (ambLlista);
        btCap.setVisible (ambLlista);
        if (ambLlista)
        {
            auto cap = r.removeFromTop (28);
            etiquetes.push_back ({ esCarpetes() ? juce::String ("Carpetes") : juce::String ("Pistes"), cap.getY() });
            cap.removeFromLeft (120);
            btTotes.setBounds (cap.removeFromLeft (btTotes.ampladaIdeal()));
            cap.removeFromLeft (8);
            btCap.setBounds (cap.removeFromLeft (btCap.ampladaIdeal()));
            r.removeFromTop (6);
            r.removeFromLeft (120);
            vista.setBounds (r);
            int y = 0;
            const int w = juce::jmax (100, r.getWidth() - 14);
            for (auto* b : checksPistes) { b->setVisible (! esCarpetes()); if (! esCarpetes()) { b->setBounds (0, y, w, 24); y += 26; } }
            for (auto* b : checksCarpetes) { b->setVisible (esCarpetes()); if (esCarpetes()) { b->setBounds (0, y, w, 24); y += 26; } }
            llista.setSize (w, juce::jmax (y, 10));
        }
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override { if (! panell.contains (e.getPosition())) tanca(); }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey) { tanca(); return true; }
        if (k == juce::KeyPress::returnKey) { intenta(); return true; }
        return true;
    }

private:
    bool esCarpetes() const { return cbQue.getSelectedId() == 4; }
    bool llistaVisible() const { return cbQue.getSelectedId() >= 2; }

    void marcaTot (bool si)
    {
        for (auto* b : (esCarpetes() ? checksCarpetes : checksPistes)) b->setToggleState (si, juce::dontSendNotification);
        actualitza();
    }

    Resultat resultat() const
    {
        Resultat r;
        r.que = cbQue.getSelectedId() - 1;
        for (int i = 0; i < checksPistes.size(); ++i)
            if (checksPistes[i]->getToggleState()) r.pistes.push_back (indexPistes[(size_t) i]);
        for (int i = 0; i < checksCarpetes.size(); ++i)
            if (checksCarpetes[i]->getToggleState()) r.carpetes.push_back (idsCarpetes[(size_t) i]);
        const int tros = cbTros.getSelectedId();
        r.opcions.tros = tros == 2 ? mibase::OpcionsExport::Bucle : tros == 3 ? mibase::OpcionsExport::Part : mibase::OpcionsExport::Canco;
        const int e = cbEstructura.getSelectedId() - 2;
        if (e >= 0 && e < (int) projecte.estructures.size()) r.opcions.estructura = projecte.estructures[(size_t) e].id;
        const int pt = cbPart.getSelectedId() - 1;
        if (pt >= 0 && pt < (int) projecte.parts.size()) r.opcions.part = projecte.parts[(size_t) pt].id;
        r.opcions.efectes = tgEfectes.getToggleState();
        r.opcions.master = tgMaster.getToggleState();
        r.bits = cbFormat.getSelectedId() == 2 ? 16 : 24;
        return r;
    }

    void actualitza()
    {
        const auto r = resultat();
        avis.clear();
        if ((r.que == Triades || r.que == UnPerPista) && r.pistes.empty()) avis = "Tria alguna pista."_ca;
        if (r.que == UnPerCarpeta && r.carpetes.empty()) avis = "Tria alguna carpeta."_ca;
        const int n = r.que == UnPerPista ? (int) r.pistes.size() : r.que == UnPerCarpeta ? (int) r.carpetes.size() : 1;
        resum = n == 1 ? "Es farà un fitxer WAV estèreo."_ca
                       : "Es faran "_ca + juce::String (n) + " fitxers WAV estèreo, tots a la mateixa carpeta i de la mateixa llargada (per poder-los posar junts en un altre programa)."_ca;
        btAccepta.setEnabled (avis.isEmpty());
        resized();
    }

    void intenta()
    {
        if (! btAccepta.isEnabled()) return;
        const auto r = resultat();
        auto fet = accepta;
        tanca();
        if (fet) fet (r);
    }

    void tanca()
    {
        if (propietariPtr != nullptr)
            juce::MessageManager::callAsync ([p = propietariPtr] { p->reset(); });
        setVisible (false);
    }

    mibase::Projecte projecte;
    std::function<void (const Resultat&)> accepta;
    juce::ComboBox cbQue, cbTros, cbEstructura, cbPart, cbFormat;
    juce::ToggleButton tgEfectes { "Amb els efectes"_ca }, tgMaster { "Amb el màster (mastering)"_ca };
    juce::Component llista;
    juce::Viewport vista;
    juce::OwnedArray<juce::ToggleButton> checksPistes, checksCarpetes;
    std::vector<int> indexPistes;
    std::vector<std::string> idsCarpetes;
    Boto btTotes { "Totes" }, btCap { "Cap" };
    Boto btAccepta, btCancella { "Cancel·la"_ca };
    juce::Rectangle<int> panell, zonaAvis;
    std::vector<std::pair<juce::String, int>> etiquetes;
    juce::String avis, resum;
    std::unique_ptr<DialegExporta>* propietariPtr = nullptr;
};
