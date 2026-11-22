#pragma once
// 0.85: el Pool (pestanya del panell de la dreta): tots els àudios del projecte. Clic: on són a la taula; el triangle:
// escolta'l; arrossega'l a la taula; botó dret: canviar el nom, obrir la carpeta, buscar-lo si falta, treure'l si sobra.
#include "Controls.h"
#include "EstatEstudi.h"
#include "mibase/Edicio.h"
#include "mibase/MotorEstudi.h"

class PanellPool : public juce::Component, private juce::ListBoxModel, private juce::TextEditor::Listener,
                   private juce::ChangeListener
{
public:
    inline static const juce::String prefixArrossega { "mibase-presa:" };

    explicit PanellPool (EstatEstudi& e) : estat (e)
    {
        cerca.setTextToShowWhenEmpty ("Cerca pel nom"_ca, tema::suau);
        cerca.setFont (dibuix::lletra (13.0f));
        cerca.setIndents (8, 6);
        cerca.setColour (juce::TextEditor::backgroundColourId, tema::fons);
        cerca.setColour (juce::TextEditor::outlineColourId, tema::linia2);
        cerca.setColour (juce::TextEditor::focusedOutlineColourId, tema::accent);
        cerca.addListener (this);
        addAndMakeVisible (cerca);
        for (auto* b : { &btImporta, &btOrdre, &btSobren, &btMes }) addAndMakeVisible (b);
        btImporta.setTooltip ("Afegeix àudios al Pool (es copien a la carpeta Àudios del projecte) sense posar-los a la taula"_ca);
        btOrdre.setTooltip ("Canvia l'ordre de la llista"_ca);
        btSobren.setTooltip ("Treu del projecte els àudios que no es fan servir enlloc (van a la paperera del projecte; es pot desfer)"_ca);
        btImporta.onClick = [this] { if (onImporta) onImporta(); };
        btOrdre.onClick = [this] { ordre = (ordre + 1) % 4; refesLlista(); };
        btSobren.onClick = [this] { if (onTreuSobrants) onTreuSobrants(); };
        btMes.onClick = [this] { menuMes(); };
        llista.setModel (this);
        llista.setRowHeight (42);
        llista.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        llista.setOutlineThickness (0);
        addAndMakeVisible (llista);
        estat.addChangeListener (this);
    }
    ~PanellPool() override { estat.removeChangeListener (this); }

    // Ho posa la pantalla de l'estudi
    std::function<juce::File (const mibase::Presa&)> fitxerDe;
    std::function<const mibase::AudioPresa* (const std::string&)> audioDe;
    std::function<void (const std::string&)> onEscolta, onLocalitza, onCanviaNom, onBusca, onTreu;
    std::function<void()> onImporta, onTreuSobrants, onBuidaPaperera;
    std::string escoltant;   // la presa que sona ara (per pintar-la)

    void refesLlista()
    {
        static const char* noms[] = { "Ordre: Nom", "Ordre: els més nous", "Ordre: durada", "Ordre: sense fer servir" };
        btOrdre.setButtonText (juce::String::fromUTF8 (noms[ordre]));
        const auto& p = estat.projecte;
        const auto filtre = cerca.getText().trim().toLowerCase();
        files.clear();
        int sobren = 0;
        for (const auto& s : p.preses)
        {
            Fila f;
            f.id = s.id;
            f.nom = juce::String::fromUTF8 (s.nom.c_str());
            f.durada = s.durada;
            f.us = mibase::edicio::usPresa (p, s.id);
            f.falta = fitxerDe != nullptr && ! fitxerDe (s).existsAsFile();
            f.ordre = s.id.size() > 1 ? juce::String (s.id.substr (1)).getIntValue() : 0;
            if (! f.us.enUs()) ++sobren;
            if (filtre.isNotEmpty() && ! f.nom.toLowerCase().contains (filtre)) continue;
            files.push_back (f);
        }
        std::sort (files.begin(), files.end(), [this] (const Fila& a, const Fila& b)
        {
            switch (ordre)
            {
                case 1:  return a.ordre > b.ordre;
                case 2:  return a.durada > b.durada;
                case 3:  if (a.us.enUs() != b.us.enUs()) return ! a.us.enUs(); break;
                default: break;
            }
            return a.nom.compareNatural (b.nom) < 0;
        });
        btSobren.setEnabled (sobren > 0);
        btSobren.setButtonText (sobren > 0 ? "Treu els que sobren ("_ca + juce::String (sobren) + ")" : "Cap àudio sobrant"_ca);
        signatura = signaturaAra();
        llista.updateContent();
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 6);
        auto f1 = r.removeFromTop (30);
        btMes.setBounds (f1.removeFromRight (30));
        f1.removeFromRight (4);
        btImporta.setBounds (f1.removeFromRight (80));
        f1.removeFromRight (4);
        cerca.setBounds (f1);
        r.removeFromTop (4);
        auto f2 = r.removeFromTop (26);
        btSobren.setBounds (f2.removeFromRight (juce::jmin (190, f2.getWidth() / 2 + 20)));
        f2.removeFromRight (4);
        btOrdre.setBounds (f2);
        r.removeFromTop (6);
        llista.setBounds (r);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (tema::superficie);
        if (files.empty())
        {
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (12.5f));
            g.drawFittedText (estat.projecte.preses.empty() ? "El projecte encara no té cap àudio. Grava, importa o fes «Importa…»."_ca
                                                            : "Cap àudio amb aquest nom"_ca,
                              llista.getBounds().reduced (12), juce::Justification::centredTop, 4);
        }
    }

    void visibilityChanged() override { if (isVisible()) refesLlista(); }

private:
    struct Fila
    {
        std::string id;
        juce::String nom;
        double durada = 0.0;
        mibase::edicio::UsPresa us;
        bool falta = false;
        int ordre = 0;
    };

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        if (isShowing() && signaturaAra() != signatura) refesLlista();
    }
    /** Canvia quan canvien els àudios o on es fan servir (no a cada moviment del capçal). */
    juce::int64 signaturaAra() const
    {
        juce::int64 h = (juce::int64) estat.projecte.preses.size();
        for (const auto& s : estat.projecte.preses) h = h * 31 + (juce::int64) std::hash<std::string>{} (s.nom + s.fitxer);
        for (const auto& t : estat.projecte.pistes)
        {
            for (const auto& c : t.clips) h = h * 7 + (juce::int64) std::hash<std::string>{} (c.presa);
            h = h * 13 + (juce::int64) t.takes.size();
        }
        return h;
    }

    // ---- ListBoxModel ----
    int getNumRows() override { return (int) files.size(); }
    const Fila* fila (int i) const { return i >= 0 && i < (int) files.size() ? &files[(size_t) i] : nullptr; }

    void paintListBoxItem (int i, juce::Graphics& g, int w, int h, bool triada) override
    {
        const auto* f = fila (i);
        if (f == nullptr) return;
        auto r = juce::Rectangle<int> (0, 0, w, h).reduced (2, 1);
        g.setColour (triada ? tema::superficie2 : (i % 2 == 0 ? tema::fons.withAlpha (0.35f) : juce::Colours::transparentBlack));
        g.fillRoundedRectangle (r.toFloat(), 3.0f);
        // el triangle d'escoltar (o el quadrat d'aturar)
        auto play = r.removeFromLeft (zonaPlay).toFloat().withSizeKeepingCentre (14.0f, 14.0f);
        g.setColour (f->falta ? tema::suau.withAlpha (0.4f) : (escoltant == f->id ? tema::accent : tema::text.withAlpha (0.8f)));
        if (escoltant == f->id) g.fillRect (play.reduced (2.0f));
        else { juce::Path t; t.addTriangle (play.getX() + 2, play.getY(), play.getX() + 2, play.getBottom(), play.getRight(), play.getCentreY()); g.fillPath (t); }
        r.removeFromLeft (4);
        const bool usat = f->us.enUs();
        g.setColour (f->falta ? tema::errorText : (usat ? tema::text : tema::suau));
        g.setFont (dibuix::lletra (12.5f, true));
        auto dalt = r.removeFromTop (h / 2);
        g.drawText (f->nom, dalt.withTrimmedRight (4), juce::Justification::bottomLeft, true);
        juce::String info;
        info << juce::String (f->durada, f->durada < 10.0 ? 2 : 1) << " s";
        if (const auto* a = audioDe ? audioDe (f->id) : nullptr)
            info << "  ·  " << (a->canals == 2 ? "estèreo"_ca : "mono"_ca) << "  ·  " << juce::String (a->fs / 1000.0, 1) << " kHz";
        info << "  ·  ";
        if (f->falta) info << "no es troba el fitxer"_ca;
        else if (! usat) info << "sense fer servir"_ca;
        else
        {
            if (f->us.clips > 0) info << (f->us.clips == 1 ? "1 clip"_ca : juce::String (f->us.clips) + " clips");
            if (f->us.takes > 0) info << (f->us.clips > 0 ? ", " : "") << (f->us.takes == 1 ? "1 presa"_ca : juce::String (f->us.takes) + " preses");
        }
        g.setColour (f->falta ? tema::errorText.withAlpha (0.85f) : tema::suau);
        g.setFont (dibuix::lletra (10.5f));
        g.drawText (info, r.withTrimmedRight (4), juce::Justification::topLeft, true);
    }

    void listBoxItemClicked (int i, const juce::MouseEvent& e) override
    {
        const auto* f = fila (i);
        if (f == nullptr) return;
        if (e.mods.isPopupMenu()) { menu (*f); return; }
        if (e.x < zonaPlay + 4) { if (! f->falta && onEscolta) onEscolta (f->id); repaint(); return; }
        if (onLocalitza) onLocalitza (f->id);
    }
    void listBoxItemDoubleClicked (int i, const juce::MouseEvent&) override
    {
        if (const auto* f = fila (i); f != nullptr && ! f->falta && onEscolta) onEscolta (f->id);
    }
    juce::var getDragSourceDescription (const juce::SparseSet<int>& sel) override
    {
        if (sel.isEmpty()) return {};
        if (const auto* f = fila (sel[0]); f != nullptr && ! f->falta) return prefixArrossega + juce::String (f->id);
        return {};
    }
    juce::String getTooltipForRow (int) override
    {
        return "Clic: on és a la taula · Triangle o doble clic: escolta'l · Arrossega'l a la taula per posar-lo un altre cop · Botó dret: opcions"_ca;
    }

    void menu (const Fila& f)
    {
        juce::PopupMenu m;
        m.addSectionHeader (f.nom);
        m.addItem (1, "Escolta'l"_ca, ! f.falta);
        m.addItem (2, "On és a la taula"_ca, f.us.clips > 0);
        m.addItem (3, "Canvia el nom…"_ca, ! f.falta);
        m.addItem (4, "Obre la carpeta"_ca, ! f.falta);
        if (f.falta) m.addItem (5, "Busca el fitxer…"_ca);
        m.addSeparator();
        m.addItem (6, f.us.enUs() ? "Treu-lo del projecte (es fa servir: primer treu els clips)"_ca : "Treu-lo del projecte (a la paperera)"_ca, ! f.us.enUs());
        juce::Component::SafePointer<PanellPool> jo (this);
        const auto id = f.id;
        m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [jo, id] (int r)
        {
            if (jo == nullptr || r <= 0) return;
            if (r == 1 && jo->onEscolta) jo->onEscolta (id);
            if (r == 2 && jo->onLocalitza) jo->onLocalitza (id);
            if (r == 3 && jo->onCanviaNom) jo->onCanviaNom (id);
            if (r == 4 && jo->fitxerDe)
                if (const auto* s = mibase::edicio::presa (jo->estat.projecte, id)) jo->fitxerDe (*s).revealToUser();
            if (r == 5 && jo->onBusca) jo->onBusca (id);
            if (r == 6 && jo->onTreu) jo->onTreu (id);
        });
    }

    void menuMes()
    {
        juce::PopupMenu m;
        m.addItem (1, "Obre la carpeta Àudios"_ca);
        m.addItem (2, "Buida la paperera del projecte…"_ca);
        juce::Component::SafePointer<PanellPool> jo (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btMes), [jo] (int r)
        {
            if (jo == nullptr) return;
            if (r == 1) jo->estat.carpeta.getChildFile (juce::String::fromUTF8 (mibase::disc::carpetaAudios)).revealToUser();
            if (r == 2 && jo->onBuidaPaperera) jo->onBuidaPaperera();
        });
    }

    void textEditorTextChanged (juce::TextEditor&) override { refesLlista(); }

    EstatEstudi& estat;
    juce::TextEditor cerca;
    juce::ListBox llista;
    std::vector<Fila> files;
    int ordre = 0;
    juce::int64 signatura = 0;
    static constexpr int zonaPlay = 26;
    Boto btImporta { "Importa…"_ca, std::nullopt, Boto::Estil::Secundari };
    Boto btOrdre { "Ordre: Nom", std::nullopt, Boto::Estil::Secundari };
    Boto btSobren { "Treu els que sobren"_ca, std::nullopt, Boto::Estil::Secundari };
    Boto btMes { {}, dibuix::Icona::Carpeta, Boto::Estil::Subtil };
};
