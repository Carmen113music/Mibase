#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Controls.h"
#include "DialegText.h"
#include "Dibuix.h"
#include "Kits.h"
#include "FinestraProgres.h"
#include <set>
#include <map>
#include <mutex>
#include <unordered_map>
#include <thread>
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
    inline juce::String copiaComWav (juce::AudioFormatManager& fm, const juce::File& origen, const juce::File& desti)
    {
        desti.getParentDirectory().createDirectory();
        if (origen.hasFileExtension ("wav")) return origen.copyFileTo (desti) ? juce::String() : "No s'ha pogut copiar el fitxer"_ca;
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (origen));
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
    inline juce::String copiaComWav (const juce::File& origen, const juce::File& desti) { return copiaComWav (kits::formats(), origen, desti); }

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
        for (auto* b : { &btOrdre, &btAfegeix, &btMes }) addAndMakeVisible (b);
        // 0.28: filtres amb botons (se'n poden encendre diversos; cap encès = tots)
        for (auto* b : { &btOneShot, &btLoop })
        {
            addAndMakeVisible (b);
            b->setClickingTogglesState (true);
            b->setLlum (tema::guitarra);
            b->onClick = [this] { refesLlista(); };
        }
        btOneShot.setTooltip ("Només els one-shots (un cop o una nota). Amb Loop també encès, tots dos; tots dos apagats, tots"_ca);
        btLoop.setTooltip ("Només els loops (trossos que es repeteixen). Amb One-shot també encès, tots dos; tots dos apagats, tots"_ca);
        for (auto& c : mibase::biblioteca::categories())
        {
            auto b = std::make_unique<Boto> (juce::String::fromUTF8 (c.c_str()), std::nullopt, Boto::Estil::Tecla);
            b->setMidaText (11.0f);
            b->setLlum (samples::colorCategoria (c));
            b->setClickingTogglesState (true);
            b->setTooltip ("Mostra els d'aquest tipus (se'n poden encendre diversos; cap encès = tots)"_ca);
            b->onClick = [this] { refesLlista(); };
            addAndMakeVisible (*b);
            btTipus.push_back (std::move (b));
        }
        btOrdre.onClick = [this] { menuOrdre(); };
        // 0.29: veure-ho per carpetes
        addAndMakeVisible (btCarpetes);
        btCarpetes.setClickingTogglesState (true);
        btCarpetes.setLlum (tema::accent);
        btCarpetes.setMidaText (11.0f);
        btCarpetes.setTooltip ("Mostra els samples agrupats per les carpetes on són (clic a una carpeta per entrar-hi)"_ca);
        btCarpetes.setToggleState (ajustos.getBoolValue ("biblioteca.perCarpetes", false), juce::dontSendNotification);
        carpetaActual = ajustos.getValue ("biblioteca.carpetaActual").toStdString();
        btCarpetes.onClick = [this] { ajustos.setValue ("biblioteca.perCarpetes", btCarpetes.getToggleState()); refesLlista(); };
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

    /** Torna a llegir la carpeta (els fitxers nous s'hi afegeixen amb les dades que se'n poden treure). L'anàlisi es fa
        en segon pla (0.28.1): els samples van sortint a la llista i es pot continuar treballant. */
    void recarrega()
    {
        llegida = true;
        if (feina != nullptr) { calRellegir = true; return; }   // quan acabi el que fa
        llanca (true, {});
    }

    /** 0.29: un fitxer que ja és dins la carpeta de la biblioteca (l'acabem d'escriure): només cal analitzar-lo. */
    void nouFitxer (const juce::File& f)
    {
        if (! llegida) { recarrega(); return; }   // ja el trobarà
        auto rel = f.getRelativePathFrom (carpeta()).replaceCharacter ('\\', '/').toStdString();
        encua ({ { f, {}, rel, {}, false, false } });
    }

    /** Afegeix fitxers (es copien a la carpeta de la biblioteca), en segon pla. Cada fitxer porta el camí que tindrà a
        dins (0.29: una carpeta afegida hi va amb les seves subcarpetes). */
    void afegeix (const std::vector<std::pair<juce::File, juce::String>>& fitxers)
    {
        if (! llegida) recarrega();
        const auto dir = carpeta();
        std::vector<Tasca> ts;
        for (auto& [f, relDesti] : fitxers)
        {
            if (! mibase::biblioteca::esAudio (f.getFullPathName().toStdString()) || f.isAChildOf (dir)) continue;
            // un nom lliure (tenint en compte els que ja s'han reservat en aquesta tanda)
            const auto subc = relDesti.containsChar ('/') ? relDesti.upToLastOccurrenceOf ("/", true, false) : juce::String();
            const auto base = (subc + juce::File::createLegalFileName (f.getFileNameWithoutExtension())).toStdString();
            std::string rel = base + ".wav";
            for (int k = 2; dir.getChildFile (juce::String::fromUTF8 (rel.c_str())).exists() || reservats.count (rel) > 0; ++k)
                rel = base + " " + std::to_string (k) + ".wav";
            reservats.insert (rel);
            ts.push_back ({ f, dir.getChildFile (juce::String::fromUTF8 (rel.c_str())), rel, f.getFileNameWithoutExtension().toStdString(), true, false });
        }
        if (! ts.empty()) encua (std::move (ts));
    }
    void afegeix (const juce::Array<juce::File>& fitxers)
    {
        bool carpetes = false;
        for (auto& f : fitxers) carpetes = carpetes || f.isDirectory();
        if (! carpetes) { afegeix (ambCarpetes (fitxers)); return; }
        // una carpeta pot tenir milers de fitxers: es busquen en segon pla
        if (onInfo) onInfo ("Buscant els àudios de la carpeta…"_ca);
        juce::Component::SafePointer<PanellBiblioteca> jo (this);
        std::thread ([jo, fitxers]
        {
            auto v = std::make_shared<std::vector<std::pair<juce::File, juce::String>>> (ambCarpetes (fitxers));
            juce::MessageManager::callAsync ([jo, v] { if (jo != nullptr) jo->afegeix (*v); });
        }).detach();
    }

    ~PanellBiblioteca() override { if (feina != nullptr) feina->atura = true; }

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
        auto f2 = r.removeFromTop (30);
        btOneShot.setBounds (f2.removeFromLeft (f2.getWidth() / 2 - 2));
        f2.removeFromLeft (4);
        btLoop.setBounds (f2);
        r.removeFromTop (4);
        for (size_t i = 0; i < btTipus.size(); i += 3)
        {
            auto fila = r.removeFromTop (26);
            const int w = (fila.getWidth() - 8) / 3;
            for (size_t k = i; k < std::min (btTipus.size(), i + 3); ++k)
            {
                btTipus[k]->setBounds (fila.removeFromLeft (w));
                fila.removeFromLeft (4);
            }
            r.removeFromTop (3);
        }
        r.removeFromTop (3);
        {
            auto f3 = r.removeFromTop (26);
            btCarpetes.setBounds (f3.removeFromRight (juce::jmin (110, f3.getWidth() / 2)));
            f3.removeFromRight (4);
            btOrdre.setBounds (f3);
        }
        if (barra != nullptr)   // 0.28.1: el progrés de l'anàlisi, a dalt de la llista
        {
            r.removeFromTop (6);
            barra->setBounds (r.removeFromTop (128));
        }
        r.removeFromTop (6);
        llista.setBounds (r);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (tema::superficie);
        if (files.empty())
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
        for (auto& f : fs) if (mibase::biblioteca::esAudio (f.toStdString()) || juce::File (f).isDirectory()) return true;   // 0.28: també carpetes
        return false;
    }
    void fileDragEnter (const juce::StringArray&, int, int) override { sobreFitxers = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { sobreFitxers = false; repaint(); }
    void filesDropped (const juce::StringArray& fs, int, int) override
    {
        sobreFitxers = false;
        juce::Array<juce::File> v;
        for (auto& f : fs) v.add (juce::File (f));
        afegeix (ambCarpetes (v));
    }

private:
    // ---- ListBoxModel ----
    int getNumRows() override { return (int) files.size(); }

    const mibase::biblioteca::Sample* sampleDeFila (int fila) const
    {
        if (fila < 0 || fila >= (int) files.size() || files[(size_t) fila].sample < 0) return nullptr;
        return &index.samples[(size_t) files[(size_t) fila].sample];
    }

    void paintListBoxItem (int fila, juce::Graphics& g, int w, int h, bool triada) override
    {
        if (fila < 0 || fila >= (int) files.size()) return;
        const auto& fl = files[(size_t) fila];
        auto r = juce::Rectangle<int> (0, 0, w, h).reduced (2, 1);
        g.setColour (triada ? tema::superficie2 : (fila % 2 == 0 ? tema::fons.withAlpha (0.35f) : juce::Colours::transparentBlack));
        g.fillRoundedRectangle (r.toFloat(), 3.0f);
        if (fl.sample < 0)   // 0.29: una carpeta (o la de dalt)
        {
            r.removeFromLeft (6);
            dibuix::pintaIcona (g, fl.amunt ? dibuix::Icona::Desfes : dibuix::Icona::Carpeta, r.removeFromLeft (20).toFloat().withSizeKeepingCentre (18.0f, 18.0f),
                                fl.amunt ? tema::suau : tema::accent);
            r.removeFromLeft (8);
            if (fl.amunt)
            {
                g.setColour (tema::suau);
                g.setFont (dibuix::lletra (12.0f));
                g.drawText ("Amunt  ·  "_ca + juce::String::fromUTF8 (carpetaActual.c_str()).replace ("/", juce::String::fromUTF8 (" › ")),
                            r.withTrimmedRight (4), juce::Justification::centredLeft, true);
                return;
            }
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (12.5f, true));
            auto dalt = r.removeFromTop (h / 2);
            g.drawText (juce::String::fromUTF8 (fl.carpeta.c_str()), dalt.withTrimmedRight (4), juce::Justification::bottomLeft, true);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (10.5f));
            g.drawText (fl.n == 1 ? "1 sample"_ca : juce::String (fl.n) + " samples", r.withTrimmedRight (4), juce::Justification::topLeft, true);
            return;
        }
        const auto& s = index.samples[(size_t) fl.sample];
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
        // la carpeta on és (si no és la que ja es mira)
        if (const auto b = s.fitxer.find_last_of ('/'); b != std::string::npos)
        {
            const auto dirS = s.fitxer.substr (0, b);
            if (! btCarpetes.getToggleState() || dirS != carpetaActual)
                info += juce::String::fromUTF8 ("  ·  ") + juce::String::fromUTF8 (dirS.c_str());
        }
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (10.5f));
        g.drawText (info, r.withTrimmedRight (4), juce::Justification::topLeft, true);
    }

    void listBoxItemClicked (int fila, const juce::MouseEvent& e) override
    {
        if (fila < 0 || fila >= (int) files.size()) return;
        const auto fl = files[(size_t) fila];
        if (fl.amunt)
        {
            const auto b = carpetaActual.find_last_of ('/');
            entraA (b == std::string::npos ? std::string() : carpetaActual.substr (0, b));
            return;
        }
        if (fl.sample < 0) { entraA (carpetaActual.empty() ? fl.carpeta : carpetaActual + "/" + fl.carpeta); return; }
        if (e.mods.isPopupMenu()) { menuSample (fl.sample); return; }
        if (onEscolta) onEscolta (fitxer (index.samples[(size_t) fl.sample]));
    }
    void listBoxItemDoubleClicked (int fila, const juce::MouseEvent&) override
    {
        if (auto* s = sampleDeFila (fila); s != nullptr && onObreAlSampler) onObreAlSampler (fitxer (*s));
    }
    juce::var getDragSourceDescription (const juce::SparseSet<int>& sel) override
    {
        if (sel.isEmpty()) return {};
        if (auto* s = sampleDeFila (sel[0])) return juce::String (samples::prefixArrossega) + fitxer (*s).getFullPathName();
        return {};
    }
    juce::String getTooltipForRow (int fila) override
    {
        if (sampleDeFila (fila) == nullptr) return "Clic: entra a la carpeta"_ca;
        return "Clic: escolta · Doble clic: al samplejador · Arrossega'l a la línia de temps, al samplejador, a «+ Pista nova» o a una fila del Drumpad · Botó dret: opcions"_ca;
    }

    void textEditorTextChanged (juce::TextEditor&) override { refesLlista(); }

    // ---- dades ----
    static mibase::biblioteca::Sample metadades (juce::AudioFormatManager& fm, const juce::File& f, const std::string& rel)
    {
        mibase::biblioteca::Sample s;
        s.fitxer = rel;
        s.nom = f.getFileNameWithoutExtension().toStdString();
        s.categoria = mibase::biblioteca::categoriaPerNom (f.getFileName().toStdString());
        s.bpm = mibase::biblioteca::bpmPerNom (f.getFileName().toStdString());
        s.data = juce::Time::currentTimeMillis();
        if (std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f)); r != nullptr && r->sampleRate > 0)
        {
            s.durada = (double) r->lengthInSamples / r->sampleRate;
            posaForma (fm, s, f);
            if (s.durada <= 12.0 && s.forma != mibase::biblioteca::loop && s.categoria != "Bombo" && s.categoria != "Caixa" && s.categoria != "Hi-hat")
                if (auto m = kits::llegeix (fm, f, 12.0))
                    if (const int n = mibase::sampler::detectaRoot (*m); n >= 0) s.tonalitat = samples::nomNota (n).toStdString();
        }
        return s;
    }

    /** One-shot o loop: pel nom i, si no ho diu, pel so. Si és un loop sense BPM, l'estima. */
    static void posaForma (juce::AudioFormatManager& fm, mibase::biblioteca::Sample& s, const juce::File& f)
    {
        s.forma = mibase::biblioteca::formaPerNom (f.getFileName().toStdString());
        double bpm = 0.0;
        if (s.forma.empty())
        {
            if (auto m = kits::llegeix (fm, f, 60.0))
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

    /** 0.29: es desa en un fil a part (amb desenes de milers de samples trigaria); si n'hi ha un de més nou, el vell no s'escriu. */
    void desa()
    {
        auto copia = std::make_shared<mibase::biblioteca::Index> (index);
        const auto dir = carpeta().getFullPathName().toStdString();
        const int num = ++desadesDemanades();
        std::thread ([copia, dir, num]
        {
            const std::lock_guard<std::mutex> l (mutexDesa());
            if (num == desadesDemanades()) mibase::biblioteca::desa (dir, *copia);
        }).detach();
    }
    static std::mutex& mutexDesa() { static std::mutex m; return m; }
    static std::atomic<int>& desadesDemanades() { static std::atomic<int> n { 0 }; return n; }

    //==========================================================================
    // 0.28.1 / 0.29: lectura, anàlisi i còpia en segon pla
    struct Tasca
    {
        juce::File origen, desti;   // desti: on es copia (si copia)
        std::string rel, nom;       // camí dins la biblioteca i nom per mostrar (si se'n vol un altre)
        bool copia = false;         // afegir de fora (es copia/converteix)
        bool nomesForma = false;    // ja hi és: només falta saber si és one-shot o loop
    };
    struct Resultat { Tasca t; mibase::biblioteca::Sample s; juce::String err; };
    struct Feina { std::atomic<bool> atura { false }; };
    std::shared_ptr<Feina> feina;
    std::vector<Tasca> cua;
    bool calRellegir = false;
    std::set<std::string> reservats;
    std::unique_ptr<FinestraProgres> barra;
    juce::StringArray errors;
    int fetsTanda = 0, totalTanda = 0, afegitsTanda = 0;
    juce::uint32 darrerRefes = 0, darrerDesa = 0;

    void encua (std::vector<Tasca> ts)
    {
        if (feina == nullptr) { llanca (false, std::move (ts)); return; }
        totalTanda += (int) ts.size();
        cua.insert (cua.end(), ts.begin(), ts.end());
        actualitzaBarra ({});
    }

    /** rellegeix: llegeix l'índex i la carpeta (en segon pla) i n'analitza els nous; si no, només les tasques. */
    void llanca (bool rellegeix, std::vector<Tasca> ts)
    {
        if (! rellegeix && ts.empty()) return;
        feina = std::make_shared<Feina>();
        fetsTanda = 0; afegitsTanda = 0;
        totalTanda = (int) ts.size();
        errors.clear();
        darrerDesa = juce::Time::getMillisecondCounter();
        if (barra == nullptr)
        {
            barra = std::make_unique<FinestraProgres> ("Biblioteca"_ca, false, nullptr, false);
            addAndMakeVisible (*barra);
            resized();
        }
        if (rellegeix) barra->posa ("Llegint la carpeta de la biblioteca…"_ca, 0.0);
        else actualitzaBarra ({});
        juce::Component::SafePointer<PanellBiblioteca> jo (this);
        auto fe = feina;
        const auto dir = carpeta();
        std::thread ([jo, fe, ts = std::move (ts), rellegeix, dir]() mutable
        {
            namespace bb = mibase::biblioteca;
            if (rellegeix)
            {
                auto idx = std::make_shared<bb::Index> (bb::llegeix (dir.getFullPathName().toStdString()));
                const auto nous = bb::sincronitza (dir.getFullPathName().toStdString(), *idx);
                if (fe->atura) return;
                {
                    const std::lock_guard<std::mutex> l (mutexDesa());
                    bb::desa (dir.getFullPathName().toStdString(), *idx);
                }
                for (auto& rel : nous)
                    ts.push_back ({ dir.getChildFile (juce::String::fromUTF8 (rel.c_str())), {}, rel, {}, false, false });
                for (auto& x : idx->samples)   // 0.27: els que encara no saben si són one-shot o loop
                    if (x.forma.empty()) ts.push_back ({ dir.getChildFile (juce::String::fromUTF8 (x.fitxer.c_str())), {}, x.fitxer, {}, false, true });
                const int n = (int) ts.size();
                juce::MessageManager::callAsync ([jo, fe, idx, n] { if (jo != nullptr && jo->feina == fe) jo->indexLlegit (std::move (*idx), n); });
            }
            juce::AudioFormatManager fm;
            fm.registerBasicFormats();
            auto lot = std::make_shared<std::vector<Resultat>>();
            auto darrer = juce::Time::getMillisecondCounter();
            auto envia = [&]
            {
                if (lot->empty()) return;
                juce::MessageManager::callAsync ([jo, fe, lot] { if (jo != nullptr && jo->feina == fe) jo->resultats (*lot); });
                lot = std::make_shared<std::vector<Resultat>>();
                darrer = juce::Time::getMillisecondCounter();
            };
            for (auto& t : ts)
            {
                if (fe->atura) return;
                Resultat r { t, {}, {} };
                if (t.copia) r.err = samples::copiaComWav (fm, t.origen, t.desti);
                if (r.err.isEmpty())
                {
                    const auto f = t.copia ? t.desti : t.origen;
                    if (t.nomesForma) { r.s.fitxer = t.rel; posaForma (fm, r.s, f); }
                    else
                    {
                        r.s = metadades (fm, f, t.rel);
                        if (! t.nom.empty()) r.s.nom = t.nom;
                    }
                }
                lot->push_back (std::move (r));
                // en lots: amb milers de fitxers, un missatge per fitxer col·lapsaria la finestra
                if (lot->size() >= 500 || juce::Time::getMillisecondCounter() - darrer >= 300) envia();
            }
            envia();
            juce::MessageManager::callAsync ([jo, fe] { if (jo != nullptr && jo->feina == fe) jo->acabat(); });
        }).detach();
    }

    void indexLlegit (mibase::biblioteca::Index&& idx, int n)
    {
        index = std::move (idx);
        totalTanda += n;
        refesLlista();
        actualitzaBarra ({});
    }

    void actualitzaBarra (const juce::String& fitxer)
    {
        if (barra == nullptr) return;
        barra->posa ("Analitzant "_ca + juce::String (juce::jmin (fetsTanda + 1, totalTanda)) + " de " + juce::String (totalTanda)
                     + (fitxer.isNotEmpty() ? juce::String::fromUTF8 (" · ") + fitxer : juce::String()),
                     totalTanda > 0 ? (double) fetsTanda / (double) totalTanda : 0.0);
    }

    void resultats (const std::vector<Resultat>& lot)
    {
        std::unordered_map<std::string, size_t> pos;   // per als que només esperaven la forma
        for (auto& r : lot)
        {
            ++fetsTanda;
            reservats.erase (r.t.rel);
            if (r.err.isNotEmpty()) { if (errors.size() < 30) errors.add (r.t.origen.getFileName() + ": " + r.err); }
            else if (r.t.nomesForma)
            {
                if (pos.empty()) for (size_t i = 0; i < index.samples.size(); ++i) pos[index.samples[i].fitxer] = i;
                if (auto it = pos.find (r.t.rel); it != pos.end())
                {
                    auto& x = index.samples[it->second];
                    x.forma = r.s.forma;
                    if (x.bpm <= 0.0) x.bpm = r.s.bpm;
                }
            }
            else
            {
                index.samples.push_back (r.s);
                ++afegitsTanda;
            }
        }
        const auto ara = juce::Time::getMillisecondCounter();
        if (ara - darrerDesa > 30000) { darrerDesa = ara; desa(); }
        // la llista, com a molt un cop per segon (amb molts samples, refer-la costa)
        if (ara - darrerRefes > 1000 || index.samples.size() < 2000) { darrerRefes = ara; refesLlista(); }
        if (! lot.empty()) actualitzaBarra (lot.back().t.origen.getFileName());
    }

    void acabat()
    {
        feina = nullptr;
        desa();
        refesLlista();
        if (calRellegir) { calRellegir = false; auto ts = std::move (cua); cua.clear(); llanca (true, std::move (ts)); return; }
        if (! cua.empty()) { auto ts = std::move (cua); cua.clear(); llanca (false, std::move (ts)); return; }
        barra = nullptr;
        resized();
        if (onInfo && afegitsTanda > 0)
            onInfo (afegitsTanda == 1 ? "1 sample nou a la biblioteca"_ca : juce::String (afegitsTanda) + " samples nous a la biblioteca"_ca);
        if (! errors.isEmpty())
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Algun fitxer no s'ha pogut afegir"_ca, errors.joinIntoString ("\n"));
        errors.clear();
    }

    /** 0.28: les carpetes es converteixen en els àudios que tenen a dins (també a les subcarpetes). 0.29: cada un amb el camí
        que tindrà a la biblioteca (la carpeta afegida i les seves subcarpetes). */
    static std::vector<std::pair<juce::File, juce::String>> ambCarpetes (const juce::Array<juce::File>& v)
    {
        std::vector<std::pair<juce::File, juce::String>> r;
        for (auto& f : v)
        {
            if (! f.isDirectory()) { r.push_back ({ f, f.getFileName() }); continue; }
            const auto pare = f.getParentDirectory();
            for (juce::RangedDirectoryIterator it (f, true, "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg", juce::File::findFiles); it != juce::RangedDirectoryIterator(); ++it)
                r.push_back ({ it->getFile(), it->getFile().getRelativePathFrom (pare).replaceCharacter ('\\', '/') });
        }
        return r;
    }

    void refesLlista()
    {
        std::vector<std::string> cats, formes;
        for (size_t i = 0; i < btTipus.size(); ++i)
            if (btTipus[i]->getToggleState()) cats.push_back (mibase::biblioteca::categories()[i]);
        if (btOneShot.getToggleState()) formes.push_back (mibase::biblioteca::oneShot);
        if (btLoop.getToggleState()) formes.push_back (mibase::biblioteca::loop);
        const auto vista = mibase::biblioteca::filtra (index, cerca.getText().toStdString(), cats, formes, ordre, descendent);
        files.clear();
        if (! btCarpetes.getToggleState())
            for (int i : vista) files.push_back ({ i });
        else
        {
            // 0.29: per carpetes: la de dalt (si no som a l'arrel), les subcarpetes i els samples d'aquesta carpeta.
            // Cercant, també surten els de les subcarpetes.
            const auto prefix = carpetaActual.empty() ? std::string() : carpetaActual + "/";
            const bool cercant = cerca.getText().trim().isNotEmpty();
            std::map<std::string, int> subs;
            std::vector<Fila> mostres;
            for (int i : vista)
            {
                const auto& rel = index.samples[(size_t) i].fitxer;
                if (rel.compare (0, prefix.size(), prefix) != 0) continue;
                const auto barra2 = rel.find ('/', prefix.size());
                if (barra2 != std::string::npos) ++subs[rel.substr (prefix.size(), barra2 - prefix.size())];
                if (barra2 == std::string::npos || cercant) mostres.push_back ({ i });
            }
            if (! carpetaActual.empty()) files.push_back ({ -1, {}, 0, true });
            std::vector<std::pair<std::string, int>> v (subs.begin(), subs.end());
            std::sort (v.begin(), v.end(), [] (const auto& a, const auto& b)
            {
                return juce::String::fromUTF8 (a.first.c_str()).compareNatural (juce::String::fromUTF8 (b.first.c_str())) < 0;
            });
            for (auto& [nom, n] : v) files.push_back ({ -1, nom, n, false });
            files.insert (files.end(), mostres.begin(), mostres.end());
        }
        llista.updateContent();
        llista.repaint();
        repaint();
    }

    /** 0.29: entra en una carpeta (o, buida, torna a l'arrel). */
    void entraA (const std::string& c)
    {
        carpetaActual = c;
        ajustos.setValue ("biblioteca.carpetaActual", juce::String::fromUTF8 (c.c_str()));
        llista.deselectAllRows();
        refesLlista();
        llista.scrollToEnsureRowIsOnscreen (0);
    }

    void actualitzaBotons()
    {
        static const char* noms[] = { "Nom", "Tipus", "Durada", "Data" };
        btOrdre.setButtonText ("Ordre: "_ca + juce::String::fromUTF8 (noms[(int) ordre]) + (descendent ? juce::String::fromUTF8 (" ↓") : juce::String::fromUTF8 (" ↑")));
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
        selector->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectDirectories
                               | juce::FileBrowserComponent::canSelectMultipleItems,
                               [this] (const juce::FileChooser& fc) { afegeix (ambCarpetes (fc.getResults())); });
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
    struct Fila
    {
        int sample = -1;            // índex a index.samples (-1: una carpeta o «amunt»)
        std::string carpeta;        // el nom de la subcarpeta
        int n = 0;                  // quants samples hi ha (amb el filtre)
        bool amunt = false;         // la fila de tornar a la carpeta de dalt
    };
    std::vector<Fila> files;
    std::string carpetaActual;      // 0.29: la carpeta que es mira (relativa; buida = l'arrel)

    mibase::biblioteca::Ordre ordre = mibase::biblioteca::Ordre::Nom;
    bool descendent = false, llegida = false, sobreFitxers = false;
    juce::TextEditor cerca;
    Boto btOneShot { "ONE-SHOT", std::nullopt, Boto::Estil::Tecla };
    Boto btLoop { "LOOP", std::nullopt, Boto::Estil::Tecla };
    std::vector<std::unique_ptr<Boto>> btTipus;
    Boto btOrdre { "Ordre: Nom", std::nullopt, Boto::Estil::Secundari };
    Boto btCarpetes { "CARPETES", std::nullopt, Boto::Estil::Tecla };
    Boto btAfegeix { "Afegeix…"_ca, std::nullopt, Boto::Estil::Secundari };
    Boto btMes { {}, dibuix::Icona::Carpeta, Boto::Estil::Subtil };
    juce::ListBox llista { "Biblioteca" };
    std::unique_ptr<juce::FileChooser> selector;
    std::unique_ptr<DialegText> dialeg;
};
