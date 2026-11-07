#pragma once

#include "Controls.h"
#include "EstatEstudi.h"
#include "Kits.h"
#include "ServeiAudio.h"

/** 0.20: la finestra dels kits de bateria. A l'esquerra els kits; a la dreta els sons del kit triat (clic: sona; clic al tipus:
    es canvia, per si la classificació s'ha equivocat). A baix: importar una carpeta, baixar els kits de Riffs, obrir la
    carpeta dels kits, posar el kit a la pista de bateria triada i esborrar-lo. */
class FinestraKits : public juce::DocumentWindow
{
public:
    FinestraKits (EstatEstudi& e, ServeiAudio& s, std::function<void()> enTancar)
        : juce::DocumentWindow ("Kits de bateria"_ca, tema::fons, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
          tancar (std::move (enTancar))
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new Contingut (e, s), true);
        setResizable (true, false);
        centreWithSize (760, 520);
        setVisible (true);
    }
    void closeButtonPressed() override { juce::MessageManager::callAsync ([t = tancar] { t(); }); }

private:
    std::function<void()> tancar;

    class Contingut : public juce::Component, private juce::ListBoxModel, private juce::URL::DownloadTaskListener
    {
    public:
        Contingut (EstatEstudi& e, ServeiAudio& s) : estat (e), servei (s)
        {
            modelSons.pare = this;
            llistaSons.setModel (&modelSons);
            llistaKits.setModel (this);
            for (auto* l : { &llistaKits, &llistaSons })
            {
                l->setRowHeight (26);
                l->setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff18181c));
                l->setColour (juce::ListBox::outlineColourId, tema::linia);
                l->setOutlineThickness (1);
                addAndMakeVisible (*l);
            }
            for (auto* b : { &btImporta, &btBaixa, &btCarpeta, &btFesServir, &btEsborra })
            {
                b->setMidaText (12.0f);
                addAndMakeVisible (*b);
            }
            btImporta.onClick = [this] { importa(); };
            btBaixa.onClick = [this] { baixa(); };
            btCarpeta.onClick = [] { kits::carpeta().revealToUser(); };
            btFesServir.onClick = [this] { fesServir(); };
            btEsborra.onClick = [this] { esborra(); };
            setSize (760, 520);
            recarrega();
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (tema::fons);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (10.5f, true));
            g.drawText ("KITS"_ca, 14, 10, 200, 16, juce::Justification::centredLeft);
            g.drawText ("SONS · clic: escolta'l · clic al tipus: canvia'l"_ca, llistaSons.getX(), 10, llistaSons.getWidth(), 16, juce::Justification::centredLeft);
            g.setFont (dibuix::lletra (11.5f));
            g.drawFittedText (missatge, 14, getHeight() - 30, getWidth() - 28, 22, juce::Justification::centredLeft, 1);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (14).withTrimmedTop (18).withTrimmedBottom (26);
            auto botons = r.removeFromBottom (32);
            r.removeFromBottom (10);
            llistaKits.setBounds (r.removeFromLeft (juce::jmax (180, r.getWidth() / 3)));
            r.removeFromLeft (10);
            llistaSons.setBounds (r);
            const int n = 5, w = (botons.getWidth() - (n - 1) * 6) / n;
            for (auto* b : { &btImporta, &btBaixa, &btCarpeta, &btFesServir, &btEsborra }) { b->setBounds (botons.removeFromLeft (w)); botons.removeFromLeft (6); }
        }

    private:
        // ---- Llista de kits ----
        int getNumRows() override { return (int) kitsAra.size(); }
        void paintListBoxItem (int fila, juce::Graphics& g, int w, int h, bool triat) override
        {
            if (fila < 0 || fila >= (int) kitsAra.size()) return;
            const auto& k = kitsAra[(size_t) fila];
            if (triat) { g.setColour (tema::accent.withAlpha (0.25f)); g.fillRect (0, 0, w, h); }
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (13.0f, k.nom == kitPista()));
            g.drawText (juce::String::fromUTF8 (k.nom.c_str()) + (k.nom == kitPista() ? "  (pista)"_ca : juce::String()), 10, 0, w - 60, h, juce::Justification::centredLeft);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (11.0f));
            g.drawText (juce::String ((int) k.sons.size()) + " sons"_ca, w - 70, 0, 62, h, juce::Justification::centredRight);
        }
        void selectedRowsChanged (int) override { llistaSons.updateContent(); llistaSons.repaint(); actualitzaBotons(); }

        // ---- Llista de sons ----
        struct ModelSons : public juce::ListBoxModel
        {
            Contingut* pare = nullptr;
            int getNumRows() override { auto* k = pare != nullptr ? pare->kitTriat() : nullptr; return k != nullptr ? (int) k->sons.size() : 0; }
            void paintListBoxItem (int fila, juce::Graphics& g, int w, int h, bool triat) override
            {
                auto* k = pare->kitTriat();
                if (k == nullptr || fila >= (int) k->sons.size()) return;
                const auto& s = k->sons[(size_t) fila];
                if (triat) { g.setColour (tema::accent.withAlpha (0.18f)); g.fillRect (0, 0, w, h); }
                dibuix::pintaIcona (g, dibuix::Icona::Play, juce::Rectangle<float> (6.0f, 5.0f, (float) h - 10.0f, (float) h - 10.0f), tema::suau);
                g.setColour (tema::text);
                g.setFont (dibuix::lletra (12.5f));
                g.drawFittedText (juce::String::fromUTF8 (s.nom.c_str()), h + 4, 0, w - h - 150, h, juce::Justification::centredLeft, 1, 0.8f);
                auto rt = juce::Rectangle<int> (w - 140, 3, 134, h - 6);
                g.setColour (juce::Colours::black.withAlpha (0.35f));
                g.fillRoundedRectangle (rt.toFloat(), 3.0f);
                g.setColour (s.tipus == "altres" ? tema::suau : juce::Colour (0xfff5b445));
                g.setFont (dibuix::lletra (11.5f, true));
                g.drawText (juce::String::fromUTF8 (mibase::bateria::nomTipus (s.tipus).c_str()), rt, juce::Justification::centred);
            }
            void listBoxItemClicked (int fila, const juce::MouseEvent& e) override
            {
                if (e.x > pare->llistaSons.getVisibleRowWidth() - 146) pare->menuTipus (fila);
                else pare->escolta (fila);
            }
        };

        mibase::bateria::Kit* kitTriat()
        {
            const int i = llistaKits.getSelectedRow();
            return i >= 0 && i < (int) kitsAra.size() ? &kitsAra[(size_t) i] : nullptr;
        }
        std::string kitPista()
        {
            auto* t = estat.pista (estat.pistaTriada);
            return t != nullptr && t->tipus == "bateria" ? t->kit : std::string();
        }

        void recarrega (const juce::String& triar = {})
        {
            const auto anterior = kitTriat() != nullptr ? kitTriat()->nom : kitPista();
            kitsAra = kits::llista();
            llistaKits.updateContent();
            const auto busca = triar.isNotEmpty() ? triar.toStdString() : anterior;
            int fila = 0;
            for (int i = 0; i < (int) kitsAra.size(); ++i) if (kitsAra[(size_t) i].nom == busca) fila = i;
            llistaKits.selectRow (fila);
            llistaSons.updateContent();
            repaint();
            actualitzaBotons();
        }

        void actualitzaBotons()
        {
            auto* t = estat.pista (estat.pistaTriada);
            const bool pistaBat = t != nullptr && t->tipus == "bateria";
            btFesServir.setEnabled (pistaBat && kitTriat() != nullptr);
            btFesServir.setButtonText (pistaBat ? "Posa'l a «"_ca + juce::String::fromUTF8 (t->nom.c_str()) + "»" : "Tria una pista de bateria"_ca);
            btEsborra.setEnabled (kitTriat() != nullptr && kitTriat()->nom != mibase::bateria::kitIntern);
        }

        void escolta (int fila)
        {
            auto* k = kitTriat();
            if (k == nullptr || fila >= (int) k->sons.size()) return;
            const auto& s = k->sons[(size_t) fila];
            const auto so = k->nom == mibase::bateria::kitIntern ? s.fitxer : mibase::bateria::camiSo (*k, s);
            servei.escoltaSo (kits::carrega (so));
        }

        void menuTipus (int fila)
        {
            auto* k = kitTriat();
            if (k == nullptr || fila >= (int) k->sons.size()) return;
            if (k->nom == mibase::bateria::kitIntern) { missatge = "Els sons del kit intern no es poden reclassificar."_ca; repaint(); return; }
            juce::PopupMenu m;
            const auto& tp = mibase::bateria::tipus();
            for (int i = 0; i < (int) tp.size(); ++i) m.addItem (i + 1, juce::String::fromUTF8 (tp[(size_t) i].nom), true, k->sons[(size_t) fila].tipus == tp[(size_t) i].id);
            m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [this, fila] (int r)
            {
                auto* kk = kitTriat();
                if (r <= 0 || kk == nullptr || fila >= (int) kk->sons.size()) return;
                kk->sons[(size_t) fila].tipus = mibase::bateria::tipus()[(size_t) r - 1].id;
                kits::desa (*kk);
                missatge = "Desat: "_ca + juce::String::fromUTF8 (kk->sons[(size_t) fila].nom.c_str()) + " és "_ca
                           + juce::String::fromUTF8 (mibase::bateria::nomTipus (kk->sons[(size_t) fila].tipus).c_str())
                           + ". Torna a posar el kit a la pista perquè ho agafi."_ca;
                llistaSons.repaint();
                repaint();
            });
        }

        void importa()
        {
            selector = std::make_unique<juce::FileChooser> ("Tria la carpeta d'un kit (o la que en conté uns quants)"_ca,
                                                            juce::File::getSpecialLocation (juce::File::userMusicDirectory));
            selector->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
            {
                const auto d = fc.getResult();
                if (! d.isDirectory()) return;
                importaCarpeta (d);
            });
        }

        void importaCarpeta (const juce::File& d)
        {
            missatge = "Important…"_ca;
            repaint();
            const auto r = kits::importa (d);
            if (r.kits.isEmpty()) missatge = "No s'hi ha trobat cap so (WAV, MP3, AIFF, FLAC o OGG)."_ca;
            else
                missatge = juce::String (r.kits.size()) + (r.kits.size() == 1 ? " kit importat ("_ca : " kits importats ("_ca) + juce::String (r.sons) + " sons"
                           + (r.perSo > 0 ? ", "_ca + juce::String (r.perSo) + " classificats pel so" : juce::String())
                           + (r.error > 0 ? ", "_ca + juce::String (r.error) + " no s'han pogut llegir" : juce::String())
                           + "). Revisa els tipus: si n'hi ha cap d'equivocat, clica'l."_ca;
            recarrega (r.kits.isEmpty() ? juce::String() : r.kits[0]);
        }

        void baixa()
        {
            if (descarrega != nullptr) return;
            fitxerBaixat = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("mibase-kits-riffs.zip");
            fitxerBaixat.deleteFile();
            missatge = "Baixant els kits de Riffs de GitHub…"_ca;
            repaint();
            descarrega = kits::urlKitsRiffs().downloadToFile (fitxerBaixat, juce::URL::DownloadTaskOptions().withListener (this));
            if (descarrega == nullptr) { missatge = "No s'ha pogut començar a baixar (hi ha connexió?)."_ca; repaint(); }
        }
        void finished (juce::URL::DownloadTask* t, bool ok) override
        {
            const bool bo = ok && ! t->hadError() && fitxerBaixat.existsAsFile();
            juce::MessageManager::callAsync ([this, bo, viu = std::weak_ptr<bool> (viu)]
            {
                if (viu.expired()) return;
                descarrega.reset();
                if (! bo) { missatge = "No s'han pogut baixar els kits (mira la connexió)."_ca; repaint(); return; }
                const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("mibase-kits-riffs");
                dir.deleteRecursively();
                dir.createDirectory();
                juce::ZipFile zip (fitxerBaixat);
                zip.uncompressTo (dir);
                importaCarpeta (dir);
                dir.deleteRecursively();
            });
        }

        void fesServir()
        {
            auto* k = kitTriat();
            auto* t = estat.pista (estat.pistaTriada);
            if (k == nullptr || t == nullptr || t->tipus != "bateria") return;
            estat.desfes.abans (estat.projecte);
            kits::posaKit (*t, k->nom);
            estat.canviat ("Kit de bateria: "_ca + juce::String::fromUTF8 (k->nom.c_str()));
            missatge = "Ara «"_ca + juce::String::fromUTF8 (t->nom.c_str()) + "» sona amb el kit "_ca + juce::String::fromUTF8 (k->nom.c_str()) + ".";
            llistaKits.repaint();
            repaint();
        }

        void esborra()
        {
            auto* k = kitTriat();
            if (k == nullptr || k->nom == mibase::bateria::kitIntern) return;
            const auto nom = juce::String::fromUTF8 (k->nom.c_str());
            const auto dir = juce::File (juce::String::fromUTF8 (k->carpeta.c_str()));
            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Esborrar el kit?"_ca,
                "S'esborrarà «"_ca + nom + "» de la carpeta dels kits (els fitxers originals que vas importar no es toquen). Les pistes que el facin servir es quedaran sense aquests sons."_ca,
                "Esborra"_ca, "Cancel·la"_ca, this, juce::ModalCallbackFunction::create ([this, dir, nom] (int r)
                {
                    if (r != 1) return;
                    dir.deleteRecursively();
                    missatge = "Kit esborrat: "_ca + nom;
                    recarrega();
                }));
        }

        EstatEstudi& estat;
        ServeiAudio& servei;
        std::vector<mibase::bateria::Kit> kitsAra;
        juce::ListBox llistaKits, llistaSons;
        ModelSons modelSons;
        Boto btImporta { "Importa una carpeta…"_ca, dibuix::Icona::Importa, Boto::Estil::Primari };
        Boto btBaixa { "Baixa els de Riffs"_ca, std::nullopt, Boto::Estil::Secundari };
        Boto btCarpeta { "Obre la carpeta"_ca, dibuix::Icona::Carpeta, Boto::Estil::Secundari };
        Boto btFesServir { "Posa'l a la pista"_ca, std::nullopt, Boto::Estil::Secundari };
        Boto btEsborra { "Esborra el kit"_ca, std::nullopt, Boto::Estil::Secundari };
        std::unique_ptr<juce::FileChooser> selector;
        std::unique_ptr<juce::URL::DownloadTask> descarrega;
        juce::File fitxerBaixat;
        juce::String missatge { "Importa una carpeta amb sons: cada carpeta es converteix en un kit i els sons es classifiquen sols."_ca };
        std::shared_ptr<bool> viu = std::make_shared<bool> (true);
    };
};
