#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Controls.h"
#include "DialegText.h"
#include "Dibuix.h"
#include "Kits.h"
#include "Tema.h"
#include "Text.h"
#include <mibase/Biblioteca.h>
#include <mibase/Sampler.h>

/** 0.27: la biblioteca de samples (pestanya del panell de la dreta). Una carpeta al disc (per defecte
    Documents\Mibase\Samples) amb un índex «biblioteca.json». Clic = escolta; doble clic = al samplejador obert;
    arrossegar = a la línia de temps, al samplejador, a «+ Pista nova» o a una fila del Drumpad. */
namespace samples
{
    inline constexpr const char* prefixArrossega = "mibase-sample:";

    /** Copia un fitxer d'àudio a «desti» (WAV). Els que no són WAV es converteixen a WAV de 24 bits. Torna l'error o buit. */
    inline juce::String copiaComWav (const juce::File& origen, const juce::File& desti)
    {
        desti.getParentDirectory().createDirectory();
        if (origen.hasFileExtension ("wav")) return origen.copyFileTo (desti) ? juce::String() : "No s'ha pogut copiar el fitxer"_ca;
        std::unique_ptr<juce::AudioFormatReader> r (kits::formats().createReaderFor (origen));
        if (r == nullptr) return "No sé llegir aquest fitxer"_ca;
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> sortida = desti.createOutputStream();
        auto w = sortida != nullptr ? wav.createWriterFor (sortida, juce::AudioFormatWriterOptions{}
                                                                    .withSampleRate (r->sampleRate)
                                                                    .withNumChannels ((int) juce::jmin (2u, r->numChannels))
                                                                    .withBitsPerSample (24))
                                    : nullptr;
        if (w == nullptr || ! w->writeFromAudioReader (*r, 0, -1)) return "No s'ha pogut escriure el WAV"_ca;
        return {};
    }

    inline juce::String nomNota (int n)
    {
        static const char* noms[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        n = juce::jlimit (0, 127, n);
        return juce::String (noms[n % 12]) + juce::String (n / 12 - 1);
    }

    inline juce::Colour colorCategoria (const std::string& c)
    {
        if (c == "Bombo") return juce::Colour (0xffb18cff);
        if (c == "Caixa") return juce::Colour (0xffd79bff);
        if (c == "Hi-hat") return juce::Colour (0xff9fb4ff);
        if (c == "Percussió") return juce::Colour (0xff8fd0c4);
        if (c == "Baix") return juce::Colour (0xff6fa8ff);
        if (c == "Veu") return juce::Colour (0xff59d0d6);
        if (c == "Sintetitzador") return juce::Colour (0xff7fd67a);
        if (c == "FX") return juce::Colour (0xffff8fc4);
        if (c == "Ambient") return juce::Colour (0xfff5b445);
        return juce::Colour (0xff8d8c97);
    }
}

class PanellBiblioteca : public juce::Component, public juce::FileDragAndDropTarget, private juce::ListBoxModel,
                         private juce::TextEditor::Listener
{
public:
    explicit PanellBiblioteca (juce::PropertiesFile& aj) : ajustos (aj)
    {
        cerca.setTextToShowWhenEmpty ("Cerca (nom, tipus, loop, etiqueta, nota)"_ca, tema::suau);
        cerca.setFont (dibuix::lletra (13.0f));
        cerca.setIndents (8, 6);
        cerca.setColour (juce::TextEditor::backgroundColourId, tema::fons);
        cerca.setColour (juce::TextEditor::outlineColourId, tema::linia2);
        cerca.setColour (juce::TextEditor::focusedOutlineColourId, tema::accent);
        cerca.addListener (this);
        addAndMakeVisible (cerca);
        for (auto* b : { &btCategoria, &btOrdre, &btAfegeix, &btMes }) addAndMakeVisible (b);
        btCategoria.onClick = [this] { menuCategoria(); };
        btCategoria.setTooltip ("Mostra només un tipus de so"_ca);
        btOrdre.onClick = [this] { menuOrdre(); };
        btOrdre.setTooltip ("Ordre de la llista"_ca);
        btAfegeix.onClick = [this] { demanaFitxers(); };
        btAfegeix.setTooltip ("Afegeix fitxers a la biblioteca (es copien a la carpeta). També els pots arrossegar aquí des de l'explorador"_ca);
        btMes.onClick = [this] { menuMes(); };
        btMes.setTooltip ("Carpeta de la biblioteca"_ca);
        llista.setModel (this);
        llista.setRowHeight (36);
        llista.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        llista.setOutlineThickness (0);
        llista.setMultipleSelectionEnabled (false);
        addAndMakeVisible (llista);
        ordre = (mibase::biblioteca::Ordre) juce::jlimit (0, 3, ajustos.getIntValue ("biblioteca.ordre", 0));
        descendent = ajustos.getBoolValue ("biblioteca.descendent", false);
        actualitzaBotons();
    }

    std::function<void (const juce::File&)> onEscolta;        // preescolta
    std::function<void (const juce::File&)> onObreAlSampler;  // doble clic
    std::function<void (const juce::String&)> onInfo;         // missatge a la barra d'estat

    juce::File carpeta() const
    {
        const auto c = ajustos.getValue ("biblioteca.carpeta");
        juce::File f = c.isNotEmpty() && juce::File::isAbsolutePath (c) ? juce::File (c)
                     : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Mibase").getChildFile ("Samples");
        f.createDirectory();
        return f;
    }
    juce::File fitxer (const mibase::biblioteca::Sample& s) const { return carpeta().getChildFile (juce::String::fromUTF8 (s.fitxer.c_str())); }

    /** Torna a llegir la carpeta (els fitxers nous s'hi afegeixen amb les dades que se'n poden treure). */
    void recarrega()
    {
        const auto dir = carpeta();
        index = mibase::biblioteca::llegeix (dir.getFullPathName().toStdString());
        const auto nous = mibase::biblioteca::sincronitza (dir.getFullPathName().toStdString(), index);
        for (auto& rel : nous) index.samples.push_back (metadades (dir.getChildFile (juce::String::fromUTF8 (rel.c_str())), rel));
        for (auto& s : index.samples)   // 0.27: els que encara no saben si són one-shot o loop
            if (s.forma.empty()) posaForma (s, dir.getChildFile (juce::String::fromUTF8 (s.fitxer.c_str())));
        mibase::biblioteca::desa (dir.getFullPathName().toStdString(), index);
        refesLlista();
    }

    /** Afegeix fitxers (es copien a la carpeta de la biblioteca). */
    void afegeix (const juce::Array<juce::File>& fitxers)
    {
        if (index.samples.empty()) recarrega();
        const auto dir = carpeta();
        int fets = 0;
        juce::StringArray errors;
        for (auto& f : fitxers)
        {
            if (! mibase::biblioteca::esAudio (f.getFullPathName().toStdString())) continue;
            if (f.isAChildOf (dir)) continue;   // ja hi és
            const auto rel = mibase::biblioteca::nomLliure (dir.getFullPathName().toStdString(),
                                                            juce::File::createLegalFileName (f.getFileNameWithoutExtension()).toStdString(), ".wav");
            const auto desti = dir.getChildFile (juce::String::fromUTF8 (rel.c_str()));
            if (auto e = samples::copiaComWav (f, desti); e.isNotEmpty()) { errors.add (f.getFileName() + ": " + e); continue; }
            auto s = metadades (desti, rel);
            s.nom = f.getFileNameWithoutExtension().toStdString();
            index.samples.push_back (s);
            ++fets;
        }
        mibase::biblioteca::desa (dir.getFullPathName().toStdString(), index);
        refesLlista();
        if (onInfo) onInfo (fets == 1 ? "1 sample afegit a la biblioteca"_ca : juce::String (fets) + " samples afegits a la biblioteca"_ca);
        if (! errors.isEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Algun fitxer no s'ha pogut afegir"_ca, errors.joinIntoString ("\n"));
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 6);
        auto f1 = r.removeFromTop (30);
        btMes.setBounds (f1.removeFromRight (30));
        f1.removeFromRight (4);
        btAfegeix.setBounds (f1.removeFromRight (80));
        f1.removeFromRight (4);
        cerca.setBounds (f1);
        r.removeFromTop (4);
        auto f2 = r.removeFromTop (26);
        btCategoria.setBounds (f2.removeFromLeft (f2.getWidth() / 2 - 2));
        f2.removeFromLeft (4);
        btOrdre.setBounds (f2);
        r.removeFromTop (6);
        llista.setBounds (r);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (tema::superficie);
        if (vista.empty())
        {
            g.setColour (sobreFitxers ? tema::accent : tema::suau);
            g.setFont (dibuix::lletra (12.5f));
            g.drawFittedText (index.samples.empty() ? "La biblioteca és buida. Arrossega-hi fitxers des de l'explorador o fes «Afegeix…»."_ca
                                                    : "Cap sample amb aquest filtre"_ca,
                              llista.getBounds().reduced (12), juce::Justification::centredTop, 4);
        }
        if (sobreFitxers) { g.setColour (tema::accent); g.drawRect (getLocalBounds(), 2); }
    }

    void visibilityChanged() override { if (isVisible() && ! llegida) { llegida = true; recarrega(); } }

    // ---- fitxers des de l'explorador ----
    bool isInterestedInFileDrag (const juce::StringArray& fs) override
    {
        for (auto& f : fs) if (mibase::biblioteca::esAudio (f.toStdString())) return true;
        return false;
    }
    void fileDragEnter (const juce::StringArray&, int, int) override { sobreFitxers = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { sobreFitxers = false; repaint(); }
    void filesDropped (const juce::StringArray& fs, int, int) override
    {
        sobreFitxers = false;
        juce::Array<juce::File> v;
        for (auto& f : fs) v.add (juce::File (f));
        afegeix (v);
    }

private:
    // ---- ListBoxModel ----
    int getNumRows() override { return (int) vista.size(); }

    void paintListBoxItem (int fila, juce::Graphics& g, int w, int h, bool triada) override
    {
        if (fila < 0 || fila >= (int) vista.size()) return;
        const auto& s = index.samples[(size_t) vista[(size_t) fila]];
        auto r = juce::Rectangle<int> (0, 0, w, h).reduced (2, 1);
        g.setColour (triada ? tema::superficie2 : (fila % 2 == 0 ? tema::fons.withAlpha (0.35f) : juce::Colours::transparentBlack));
        g.fillRoundedRectangle (r.toFloat(), 3.0f);
        const auto col = samples::colorCategoria (s.categoria);
        g.setColour (col);
        g.fillRoundedRectangle (r.removeFromLeft (4).toFloat().reduced (0.0f, 3.0f), 2.0f);
        r.removeFromLeft (6);
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (12.5f, true));
        auto dalt = r.removeFromTop (h / 2);
        g.drawText (juce::String::fromUTF8 (s.nom.c_str()), dalt.withTrimmedRight (4), juce::Justification::bottomLeft, true);
        juce::String info = juce::String::fromUTF8 (s.categoria.c_str());
        if (! s.forma.empty()) info += juce::String::fromUTF8 ("  ·  ") + juce::String::fromUTF8 (s.forma.c_str());
        info += juce::String::fromUTF8 ("  ·  ") + juce::String (s.durada, s.durada < 10.0 ? 2 : 1) + " s";
        if (! s.tonalitat.empty()) info += juce::String::fromUTF8 ("  ·  ") + juce::String::fromUTF8 (s.tonalitat.c_str());
        if (s.bpm > 0.0) info += juce::String::fromUTF8 ("  ·  ") + juce::String (s.bpm, 0) + " BPM";
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (10.5f));
        g.drawText (info, r.withTrimmedRight (4), juce::Justification::topLeft, true);
    }

    void listBoxItemClicked (int fila, const juce::MouseEvent& e) override
    {
        if (fila < 0 || fila >= (int) vista.size()) return;
        const int i = vista[(size_t) fila];
        if (e.mods.isPopupMenu()) { menuSample (i); return; }
        if (onEscolta) onEscolta (fitxer (index.samples[(size_t) i]));
    }
    void listBoxItemDoubleClicked (int fila, const juce::MouseEvent&) override
    {
        if (fila >= 0 && fila < (int) vista.size() && onObreAlSampler) onObreAlSampler (fitxer (index.samples[(size_t) vista[(size_t) fila]]));
    }
    juce::var getDragSourceDescription (const juce::SparseSet<int>& files) override
    {
        if (files.isEmpty()) return {};
        const int fila = files[0];
        if (fila < 0 || fila >= (int) vista.size()) return {};
        return juce::String (samples::prefixArrossega) + fitxer (index.samples[(size_t) vista[(size_t) fila]]).getFullPathName();
    }
    juce::String getTooltipForRow (int) override
    {
        return "Clic: escolta · Doble clic: al samplejador · Arrossega'l a la línia de temps, al samplejador, a «+ Pista nova» o a una fila del Drumpad · Botó dret: opcions"_ca;
    }

    void textEditorTextChanged (juce::TextEditor&) override { refesLlista(); }

    // ---- dades ----
    mibase::biblioteca::Sample metadades (const juce::File& f, const std::string& rel)
    {
        mibase::biblioteca::Sample s;
        s.fitxer = rel;
        s.nom = f.getFileNameWithoutExtension().toStdString();
        s.categoria = mibase::biblioteca::categoriaPerNom (f.getFileName().toStdString());
        s.bpm = mibase::biblioteca::bpmPerNom (f.getFileName().toStdString());
        s.data = juce::Time::currentTimeMillis();
        if (std::unique_ptr<juce::AudioFormatReader> r (kits::formats().createReaderFor (f)); r != nullptr && r->sampleRate > 0)
        {
            s.durada = (double) r->lengthInSamples / r->sampleRate;
            posaForma (s, f);
            if (s.durada <= 12.0 && s.forma != mibase::biblioteca::loop && s.categoria != "Bombo" && s.categoria != "Caixa" && s.categoria != "Hi-hat")
                if (auto m = kits::llegeix (f, 12.0))
                    if (const int n = mibase::sampler::detectaRoot (*m); n >= 0) s.tonalitat = samples::nomNota (n).toStdString();
        }
        return s;
    }

    /** One-shot o loop: pel nom i, si no ho diu, pel so. Si és un loop sense BPM, l'estima. */
    static void posaForma (mibase::biblioteca::Sample& s, const juce::File& f)
    {
        s.forma = mibase::biblioteca::formaPerNom (f.getFileName().toStdString());
        double bpm = 0.0;
        if (s.forma.empty())
        {
            if (auto m = kits::llegeix (f, 60.0))
            {
                std::vector<float> mono (m->L);
                for (size_t i = 0; i < mono.size() && i < m->R.size(); ++i) mono[i] = 0.5f * (mono[i] + m->R[i]);
                s.forma = mibase::biblioteca::detectaForma (mono.data(), (long long) mono.size(), m->fs, bpm);
            }
            else s.forma = mibase::biblioteca::oneShot;
        }
        else if (s.forma == mibase::biblioteca::loop && s.bpm <= 0.0 && s.durada > 0.0)
        {
            for (int temps : { 2, 4, 8, 16, 32, 64 })   // com detectaForma: per la durada
            {
                const double b = 60.0 * temps / s.durada;
                if (b >= 70.0 && b <= 180.0 && (bpm <= 0.0 || std::abs (b - 120.0) < std::abs (bpm - 120.0))) bpm = b;
            }
            bpm = std::round (bpm * 10.0) / 10.0;
        }
        if (s.forma == mibase::biblioteca::loop && s.bpm <= 0.0) s.bpm = bpm;
    }

    void desa() { mibase::biblioteca::desa (carpeta().getFullPathName().toStdString(), index); }

    void refesLlista()
    {
        vista = mibase::biblioteca::filtra (index, cerca.getText().toStdString(), categoria, ordre, descendent, forma);
        llista.updateContent();
        llista.repaint();
        repaint();
    }

    void actualitzaBotons()
    {
        juce::String t = categoria.empty() ? (forma.empty() ? "Tots els tipus"_ca : juce::String()) : juce::String::fromUTF8 (categoria.c_str());
        if (! forma.empty()) t += (t.isEmpty() ? "" : juce::String::fromUTF8 (" · ")) + juce::String::fromUTF8 (forma.c_str()) + "s";
        btCategoria.setButtonText (t);
        static const char* noms[] = { "Nom", "Tipus", "Durada", "Data" };
        btOrdre.setButtonText ("Ordre: "_ca + juce::String::fromUTF8 (noms[(int) ordre]) + (descendent ? juce::String::fromUTF8 (" ↓") : juce::String::fromUTF8 (" ↑")));
    }

    void menuCategoria()
    {
        juce::PopupMenu m;
        m.addItem (1, "Tots els tipus"_ca, true, categoria.empty());
        int k = 2;
        for (auto& c : mibase::biblioteca::categories()) m.addItem (k++, juce::String::fromUTF8 (c.c_str()), true, c == categoria);
        m.addSeparator();
        m.addItem (50, "One-shots i loops", true, forma.empty());
        m.addItem (51, "Només one-shots"_ca, true, forma == mibase::biblioteca::oneShot);
        m.addItem (52, "Només loops"_ca, true, forma == mibase::biblioteca::loop);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btCategoria), [this] (int r)
        {
            if (r <= 0) return;
            if (r >= 50) forma = r == 51 ? mibase::biblioteca::oneShot : r == 52 ? mibase::biblioteca::loop : "";
            else categoria = r == 1 ? std::string() : mibase::biblioteca::categories()[(size_t) (r - 2)];
            actualitzaBotons();
            refesLlista();
        });
    }

    void menuOrdre()
    {
        juce::PopupMenu m;
        static const char* noms[] = { "Nom", "Tipus", "Durada", "Data" };
        for (int i = 0; i < 4; ++i) m.addItem (i + 1, juce::String::fromUTF8 (noms[i]), true, (int) ordre == i);
        m.addSeparator();
        m.addItem (10, "De més a menys"_ca, true, descendent);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btOrdre), [this] (int r)
        {
            if (r <= 0) return;
            if (r == 10) descendent = ! descendent;
            else ordre = (mibase::biblioteca::Ordre) (r - 1);
            ajustos.setValue ("biblioteca.ordre", (int) ordre);
            ajustos.setValue ("biblioteca.descendent", descendent);
            actualitzaBotons();
            refesLlista();
        });
    }

    void menuMes()
    {
        juce::PopupMenu m;
        m.addItem (1, "Torna a llegir la carpeta"_ca);
        m.addItem (2, "Obre la carpeta"_ca);
        m.addItem (3, "Canvia la carpeta…"_ca);
        m.addSeparator();
        m.addItem (4, "Carpeta: "_ca + carpeta().getFullPathName(), false);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btMes), [this] (int r)
        {
            if (r == 1) { recarrega(); if (onInfo) onInfo ("Biblioteca: "_ca + juce::String ((int) index.samples.size()) + " samples"); }
            else if (r == 2) carpeta().startAsProcess();
            else if (r == 3)
            {
                selector = std::make_unique<juce::FileChooser> ("Carpeta de la biblioteca"_ca, carpeta());
                selector->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
                {
                    const auto d = fc.getResult();
                    if (! d.isDirectory()) return;
                    ajustos.setValue ("biblioteca.carpeta", d.getFullPathName());
                    recarrega();
                    if (onInfo) onInfo ("Biblioteca a "_ca + d.getFullPathName());
                });
            }
        });
    }

    void demanaFitxers()
    {
        selector = std::make_unique<juce::FileChooser> ("Afegeix samples a la biblioteca"_ca,
                                                        juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        selector->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                               [this] (const juce::FileChooser& fc) { afegeix (fc.getResults()); });
    }

    void menuSample (int i)
    {
        juce::PopupMenu m, cat;
        const auto& s = index.samples[(size_t) i];
        int k = 100;
        for (auto& c : mibase::biblioteca::categories()) cat.addItem (k++, juce::String::fromUTF8 (c.c_str()), true, c == s.categoria);
        m.addItem (1, "Reanomena…"_ca);
        m.addSubMenu ("Tipus de so"_ca, cat);
        juce::PopupMenu fm;
        fm.addItem (60, "One-shot", true, s.forma == mibase::biblioteca::oneShot);
        fm.addItem (61, "Loop", true, s.forma == mibase::biblioteca::loop);
        m.addSubMenu ("One-shot o loop", fm);
        m.addItem (2, "Etiquetes…"_ca);
        m.addItem (3, "BPM…"_ca);
        m.addSeparator();
        m.addItem (4, "Mostra a la carpeta"_ca);
        m.addItem (5, "Elimina de la biblioteca"_ca);
        const auto fitxerS = s.fitxer;
        m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [this, fitxerS] (int r)
        {
            auto* x = index.busca (fitxerS);
            if (x == nullptr || r <= 0) return;
            if (r >= 100) { x->categoria = mibase::biblioteca::categories()[(size_t) (r - 100)]; desa(); refesLlista(); }
            else if (r == 60 || r == 61) { x->forma = r == 60 ? mibase::biblioteca::oneShot : mibase::biblioteca::loop; desa(); refesLlista(); }
            else if (r == 1) demanaText ("Nom del sample"_ca, "Només canvia el nom que es mostra (el fitxer no es toca)."_ca, juce::String::fromUTF8 (x->nom.c_str()),
                                         [this, fitxerS] (const juce::String& t) { if (auto* y = index.busca (fitxerS)) y->nom = t.trim().toStdString(); });
            else if (r == 2) demanaText ("Etiquetes"_ca, "Separades per comes (per exemple: veu, crit, futbol)."_ca,
                                         juce::String::fromUTF8 (mibase::biblioteca::textDeEtiquetes (x->etiquetes).c_str()),
                                         [this, fitxerS] (const juce::String& t) { if (auto* y = index.busca (fitxerS)) y->etiquetes = mibase::biblioteca::etiquetesDeText (t.toStdString()); });
            else if (r == 3) demanaText ("BPM del sample"_ca, "0 = no se sap."_ca, x->bpm > 0.0 ? juce::String (x->bpm, 1) : juce::String ("0"),
                                         [this, fitxerS] (const juce::String& t) { if (auto* y = index.busca (fitxerS)) y->bpm = juce::jlimit (0.0, 300.0, t.getDoubleValue()); });
            else if (r == 4) fitxer (*x).revealToUser();
            else if (r == 5)
            {
                const auto f = fitxer (*x);
                juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Elimina el sample"_ca,
                    "S'esborrarà «"_ca + f.getFileName() + "» de la carpeta de la biblioteca (va a la paperera). Els projectes que el fan servir en tenen una còpia."_ca,
                    "Elimina"_ca, "No"_ca, nullptr, juce::ModalCallbackFunction::create ([this, fitxerS, f] (int ok)
                    {
                        if (ok == 0) return;
                        if (! f.moveToTrash()) f.deleteFile();
                        index.samples.erase (std::remove_if (index.samples.begin(), index.samples.end(),
                                                             [&] (const mibase::biblioteca::Sample& y) { return y.fitxer == fitxerS; }), index.samples.end());
                        desa();
                        refesLlista();
                    }));
            }
        });
    }

    void demanaText (const juce::String& titol, const juce::String& explicacio, const juce::String& inicial, std::function<void (const juce::String&)> fes)
    {
        dialeg = std::make_unique<DialegText> (titol, explicacio, "D'acord",
            [] (const juce::String&) { return juce::String(); },
            [this, fes] (const juce::String& t) { fes (t); desa(); refesLlista(); }, inicial);
        DialegText::mostra (*this, dialeg);
    }

    juce::PropertiesFile& ajustos;
    mibase::biblioteca::Index index;
    std::vector<int> vista;
    std::string categoria, forma;   // filtres (buit: tots)
    mibase::biblioteca::Ordre ordre = mibase::biblioteca::Ordre::Nom;
    bool descendent = false, llegida = false, sobreFitxers = false;
    juce::TextEditor cerca;
    Boto btCategoria { "Tots els tipus", std::nullopt, Boto::Estil::Secundari };
    Boto btOrdre { "Ordre: Nom", std::nullopt, Boto::Estil::Secundari };
    Boto btAfegeix { "Afegeix…"_ca, std::nullopt, Boto::Estil::Secundari };
    Boto btMes { {}, dibuix::Icona::Carpeta, Boto::Estil::Subtil };
    juce::ListBox llista { "Biblioteca" };
    std::unique_ptr<juce::FileChooser> selector;
    std::unique_ptr<DialegText> dialeg;
};
