#pragma once

#include "Controls.h"
#include "DialegText.h"
#include "Tema.h"
#include "Text.h"
#include <functional>
#include <vector>

/** 0.58: el navegador de presets (cercador, categories, favorits, presets propis), el mateix aspecte que el de la Reverb,
    per a qualsevol efecte intern. L'efecte hi posa les dades amb «OrigenPresets». */
struct EntradaPreset
{
    juce::String nom, cat, desc, etiqueta;
    bool usuari = false, fav = false;
    int index = 0;   // per a l'origen
};

/** Estrella de preferit (també la fa servir la Reverb). */
class BotoEstrella : public juce::Button
{
public:
    BotoEstrella() : juce::Button ("favorit") { setMouseCursor (juce::MouseCursor::PointingHandCursor); setWantsKeyboardFocus (false); }
    void paintButton (juce::Graphics& g, bool sobre, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (3.0f);
        const float s = juce::jmin (r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre (s, s);
        juce::Path p;
        p.addStar (r.getCentre(), 5, s * 0.22f, s * 0.5f, 0.0f);
        if (getToggleState()) { g.setColour (tema::guitarra); g.fillPath (p); }
        g.setColour (getToggleState() ? tema::guitarra : (sobre ? tema::text : tema::suau));
        g.strokePath (p, juce::PathStrokeType (1.4f));
    }
};

class OrigenPresets
{
public:
    virtual ~OrigenPresets() = default;
    virtual juce::StringArray categoriesFabrica() = 0;
    virtual juce::StringArray carpetesUsuari() = 0;
    virtual std::vector<EntradaPreset> entrades() = 0;           // primer els de l'usuari
    virtual juce::String presetActual() = 0;
    virtual void aplica (const EntradaPreset& e) = 0;
    virtual void commutaFavorit (const EntradaPreset& e) = 0;
    virtual bool existeixMeu (const juce::String& nom) = 0;
    virtual void reanomena (const juce::String& vell, const juce::String& nou) = 0;
    virtual void mou (const juce::String& nom, const juce::String& carpeta) = 0;
    virtual void esborra (const juce::String& nom) = 0;
    virtual void tancaNavegador() = 0;
    /** 0.89: presets triats fa poc (el primer, el més recent), si l'origen en porta. Amb això surten «Recents» i «Atzar». */
    virtual juce::StringArray recents() { return {}; }
    virtual bool ambRecentsIAtzar() { return false; }
};

class NavegadorPresetsComu : public juce::Component, private juce::ListBoxModel, private juce::TextEditor::Listener
{
public:
    explicit NavegadorPresetsComu (OrigenPresets& o) : origen (o)
    {
        cerca.setTextToShowWhenEmpty ("Cerca un preset (nom, categoria o descripció)…"_ca, tema::suau);
        cerca.setFont (dibuix::lletra (14.0f));
        cerca.setIndents (10, 7);
        cerca.addListener (this);
        addAndMakeVisible (cerca);
        filtre.onCanvi = [this] (int) { actualitza(); };
        addAndMakeVisible (filtre);
        for (auto* l : { &cats, &llista })
        {
            l->setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff18181c));
            l->setColour (juce::ListBox::outlineColourId, tema::linia);
            l->setOutlineThickness (1);
            addAndMakeVisible (*l);
        }
        cats.setModel (&modelCats);
        cats.setRowHeight (26);
        llista.setModel (this);
        llista.setRowHeight (28);
        for (auto* b : { &btReanomena, &btMou, &btEsborra, &btTanca }) addAndMakeVisible (*b);
        if (origen.ambRecentsIAtzar())   // 0.89
        {
            addAndMakeVisible (btRecents);
            addAndMakeVisible (btAtzar);
            btRecents.setClickingTogglesState (true);
            btRecents.setTooltip ("Només els presets que has fet servir fa poc (el més recent a dalt)"_ca);
            btAtzar.setTooltip ("Un preset a l'atzar d'entre els que es veuen a la llista (respecta la categoria i la cerca)"_ca);
            btRecents.onClick = [this] { actualitza(); };
            btAtzar.onClick = [this]
            {
                if (files.empty()) return;
                const int i = juce::Random::getSystemRandom().nextInt ((int) files.size());
                llista.selectRow (i);
                llista.scrollToEnsureRowIsOnscreen (i);
                origen.aplica (files[(size_t) i]);
                repaint();
            };
        }
        btTanca.onClick = [this] { origen.tancaNavegador(); };
        btReanomena.onClick = [this] { reanomena(); };
        btMou.onClick = [this] { mou(); };
        btEsborra.onClick = [this] { esborra(); };
        modelCats.onTria = [this] (int) { actualitza(); };
        actualitza();
        for (int i = 0; i < (int) files.size(); ++i)
            if (files[(size_t) i].nom == origen.presetActual()) { llista.selectRow (i); llista.scrollToEnsureRowIsOnscreen (i); }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (tema::fons.withAlpha (0.97f));
        g.setColour (tema::linia);
        g.drawRect (getLocalBounds());
        auto peu = getLocalBounds().reduced (14).removeFromBottom (34);
        const int sel = llista.getSelectedRow();
        if (sel >= 0 && sel < (int) files.size())
        {
            g.setColour (tema::textMig);
            g.setFont (dibuix::lletra (13.0f));
            g.drawFittedText (files[(size_t) sel].desc, peu.withTrimmedRight (420), juce::Justification::centredLeft, 2, 0.8f);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14);
        auto dalt = r.removeFromTop (34);
        btTanca.setBounds (dalt.removeFromRight (90));
        dalt.removeFromRight (10);
        filtre.setBounds (dalt.removeFromRight (filtre.ampladaIdeal()));
        dalt.removeFromRight (10);
        if (btRecents.isVisible())
        {
            btAtzar.setBounds (dalt.removeFromRight (90));
            dalt.removeFromRight (6);
            btRecents.setBounds (dalt.removeFromRight (100));
            dalt.removeFromRight (10);
        }
        cerca.setBounds (dalt);
        r.removeFromTop (10);
        auto peu = r.removeFromBottom (34);
        btEsborra.setBounds (peu.removeFromRight (110));
        peu.removeFromRight (8);
        btMou.setBounds (peu.removeFromRight (140));
        peu.removeFromRight (8);
        btReanomena.setBounds (peu.removeFromRight (130));
        r.removeFromBottom (10);
        cats.setBounds (r.removeFromLeft (190));
        r.removeFromLeft (10);
        llista.setBounds (r);
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey) { origen.tancaNavegador(); return true; }
        if (k == juce::KeyPress::returnKey) { carregaTriat (true); return true; }
        return false;
    }

    void actualitza()
    {
        const auto anterior = modelCats.noms[modelCats.triada];
        modelCats.noms.clear();
        modelCats.noms.add ("Totes");
        modelCats.noms.add ("#Fàbrica"_ca);
        modelCats.noms.addArray (origen.categoriesFabrica());
        const auto carpetes = origen.carpetesUsuari();
        if (! carpetes.isEmpty())
        {
            modelCats.noms.add ("#Meus presets");
            for (auto& c : carpetes) modelCats.noms.add ("  " + c);
        }
        modelCats.triada = juce::jmax (0, modelCats.noms.indexOf (anterior));
        cats.updateContent();
        cats.repaint();
        const auto catTriada = modelCats.noms[modelCats.triada].trim();
        const bool totes = modelCats.triada == 0;
        const auto text = senseAccents (cerca.getText());
        const int f = filtre.getTriat();
        files.clear();
        bool hiHaMeu = false;
        for (auto& e : origen.entrades())
        {
            hiHaMeu = hiHaMeu || e.usuari;
            if (f == 1 && ! e.fav) continue;
            if (f == 2 && e.usuari) continue;
            if (f == 3 && ! e.usuari) continue;
            if (! totes && e.cat != catTriada) continue;
            if (text.isNotEmpty() && ! senseAccents (e.nom + " " + e.cat + " " + e.desc + " " + e.etiqueta).contains (text)) continue;
            files.push_back (e);
        }
        if (btRecents.getToggleState())   // 0.89: només els recents, en ordre
        {
            const auto rec = origen.recents();
            std::vector<EntradaPreset> ord;
            for (auto& r : rec)
                for (auto& e : files) if (e.nom == r) { ord.push_back (e); break; }
            files = ord;
        }
        llista.updateContent();
        llista.repaint();
        for (auto* b : { &btReanomena, &btMou, &btEsborra }) b->setEnabled (hiHaMeu);
    }

private:
    static juce::String senseAccents (const juce::String& s)
    {
        auto t = s.toLowerCase();
        const juce::String de = juce::String::fromUTF8 ("àáèéíïòóúüç·"), a = "aaeeiioouuc.";
        for (int i = 0; i < de.length(); ++i) t = t.replaceCharacter (de[i], a[i]);
        return t.trim();
    }

    struct ModelCats : public juce::ListBoxModel
    {
        juce::StringArray noms;
        int triada = 0;
        std::function<void (int)> onTria;
        int getNumRows() override { return noms.size(); }
        void paintListBoxItem (int fila, juce::Graphics& g, int w, int h, bool) override
        {
            const bool sel = fila == triada;
            const bool capcalera = noms[fila].startsWith ("#");
            if (sel) { g.setColour (tema::triat); g.fillRect (0, 0, w, h); g.setColour (tema::accent); g.fillRect (0, 4, 3, h - 8); }
            g.setColour (capcalera ? tema::suau : (sel ? tema::text : tema::textMig));
            g.setFont (dibuix::lletra (capcalera ? 10.5f : 13.0f, capcalera || sel));
            g.drawText (capcalera ? dibuix::majuscules (noms[fila].substring (1)) : noms[fila], 12, 0, w - 16, h, juce::Justification::centredLeft);
        }
        void listBoxItemClicked (int fila, const juce::MouseEvent&) override
        {
            if (noms[fila].startsWith ("#")) return;
            triada = fila;
            if (onTria) onTria (fila);
        }
    };

    int getNumRows() override { return (int) files.size(); }
    void paintListBoxItem (int fila, juce::Graphics& g, int w, int h, bool sel) override
    {
        if (fila < 0 || fila >= (int) files.size()) return;
        const auto& f = files[(size_t) fila];
        if (sel) { g.setColour (tema::triat); g.fillRect (0, 0, w, h); g.setColour (tema::accent); g.fillRect (0, 4, 3, h - 8); }
        juce::Path p;
        p.addStar ({ 18.0f, h * 0.5f }, 5, 3.2f, 7.5f, 0.0f);
        if (f.fav) { g.setColour (tema::guitarra); g.fillPath (p); }
        g.setColour (f.fav ? tema::guitarra : tema::linia2);
        g.strokePath (p, juce::PathStrokeType (1.2f));
        g.setColour (f.nom == origen.presetActual() ? tema::accent : tema::text);
        g.setFont (dibuix::lletra (13.5f, sel));
        g.drawText (f.nom, 34, 0, w - 230, h, juce::Justification::centredLeft);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.5f));
        g.drawText ((f.usuari ? "Meu · "_ca : juce::String()) + f.cat + (f.etiqueta.isNotEmpty() ? " · "_ca + f.etiqueta : juce::String()), w - 230, 0, 222, h,
                    juce::Justification::centredRight);
    }
    void listBoxItemClicked (int fila, const juce::MouseEvent& e) override
    {
        if (fila < 0 || fila >= (int) files.size()) return;
        if (e.x < 32) { origen.commutaFavorit (files[(size_t) fila]); actualitza(); return; }
        carregaTriat (false);
        repaint();
    }
    void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override { carregaTriat (true); }
    void selectedRowsChanged (int) override { repaint(); }
    void textEditorTextChanged (juce::TextEditor&) override { actualitza(); }

    void carregaTriat (bool tanca)
    {
        const int fila = llista.getSelectedRow();
        if (fila < 0 || fila >= (int) files.size()) return;
        origen.aplica (files[(size_t) fila]);
        llista.repaint();
        if (tanca) origen.tancaNavegador();
    }

    const EntradaPreset* filaMeva()
    {
        const int fila = llista.getSelectedRow();
        if (fila < 0 || fila >= (int) files.size() || ! files[(size_t) fila].usuari)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Presets"_ca,
                                                    "Tria primer un dels teus presets (els de fàbrica no es poden canviar)."_ca);
            return nullptr;
        }
        return &files[(size_t) fila];
    }

    void reanomena()
    {
        auto* f = filaMeva();
        if (f == nullptr) return;
        const auto vell = f->nom;
        dialeg = std::make_unique<DialegText> ("Reanomena el preset"_ca, "Nom nou per a «"_ca + vell + "»."_ca, "Reanomena"_ca,
            [this, vell] (const juce::String& t) -> juce::String
            {
                if (t.trim().isEmpty()) return "Escriu un nom."_ca;
                if (t.trim() != vell && origen.existeixMeu (t.trim())) return "Ja tens un preset amb aquest nom."_ca;
                return {};
            },
            [this, vell] (const juce::String& t) { origen.reanomena (vell, t.trim()); actualitza(); }, vell);
        DialegText::mostra (*this, dialeg);
    }

    void mou()
    {
        auto* f = filaMeva();
        if (f == nullptr) return;
        const auto nom = f->nom;
        juce::PopupMenu m;
        m.addSectionHeader ("Mou «"_ca + nom + "» a…"_ca);
        const auto carpetes = origen.carpetesUsuari();
        for (int i = 0; i < carpetes.size(); ++i) m.addItem (i + 1, carpetes[i], carpetes[i] != f->cat);
        m.addSeparator();
        m.addItem (1000, "Carpeta nova…"_ca);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btMou), [this, nom, carpetes] (int r)
        {
            if (r >= 1 && r <= carpetes.size()) { origen.mou (nom, carpetes[r - 1]); actualitza(); }
            else if (r == 1000)
            {
                dialeg = std::make_unique<DialegText> ("Carpeta nova"_ca, "Nom de la carpeta on aniran els teus presets."_ca, "Mou-lo"_ca,
                    [] (const juce::String& t) { return t.trim().isEmpty() ? "Escriu un nom."_ca : juce::String(); },
                    [this, nom] (const juce::String& t) { origen.mou (nom, t.trim()); actualitza(); });
                DialegText::mostra (*this, dialeg);
            }
        });
    }

    void esborra()
    {
        auto* f = filaMeva();
        if (f == nullptr) return;
        const auto nom = f->nom;
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Esborra el preset"_ca,
            "Segur que vols esborrar «"_ca + nom + "»? No es pot desfer."_ca, "Esborra"_ca, "Cancel·la"_ca, this,
            juce::ModalCallbackFunction::create ([this, nom] (int r) { if (r != 0) { origen.esborra (nom); actualitza(); } }));
    }

    OrigenPresets& origen;
    juce::TextEditor cerca;
    Segmentat filtre { { "Tots", "Favorits", "Fàbrica"_ca, "Meus" } };
    ModelCats modelCats;
    juce::ListBox cats, llista;
    std::vector<EntradaPreset> files;
    Boto btReanomena { "Reanomena", dibuix::Icona::Llapis }, btMou { "Mou a carpeta", dibuix::Icona::Carpeta },
         btEsborra { "Esborra", dibuix::Icona::Goma }, btTanca { "Tanca", dibuix::Icona::Tanca };
    Boto btRecents { "Recents", std::nullopt, Boto::Estil::Tecla }, btAtzar { "Atzar", std::nullopt, Boto::Estil::Secundari };   // 0.89
    std::unique_ptr<DialegText> dialeg;
};
