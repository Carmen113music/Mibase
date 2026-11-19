#include "FinestraDinamica.h"

#include "ControlSidechain.h"
#include "Controls.h"
#include "DialegText.h"
#include "NavegadorPresets.h"
#include <map>
#include "Dibuix.h"
#include "EscGlobal.h"
#include "Pom.h"
#include "Tema.h"
#include "Text.h"
#include "Vintage.h"

using namespace mibase::dinamica;

namespace
{
    juce::String u8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str(), (int) s.size()); }
    juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

    // 0.58: presets propis i favorits (Documents\Mibase\Dinamica\presets.json)
    juce::File fitxerBiblioteca()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Mibase").getChildFile ("Dinamica").getChildFile ("presets.json");
    }
    BibliotecaPresets& biblioteca()
    {
        static BibliotecaPresets b;
        static bool carregada = false;
        if (! carregada) { carregada = true; b.carrega (dibuix::cami (fitxerBiblioteca())); }
        return b;
    }
    void desaBiblioteca() { biblioteca().desa (dibuix::cami (fitxerBiblioteca())); }
    std::map<const void*, juce::String>& nomsPreset() { static std::map<const void*, juce::String> m; return m; }   // per efecte (mentre dura la sessió)

    /** La corba (entrada → sortida) amb el punt on és ara el so. */
    class Corba : public juce::Component
    {
    public:
        explicit Corba (Dinamica& d) : din (d) {}
        float nivell = -90.0f;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRoundedRectangle (r, 5.0f);
            r.reduce (10.0f, 10.0f);
            auto aX = [&] (float db) { return r.getX() + r.getWidth() * (db + 72.0f) / 72.0f; };
            auto aY = [&] (float db) { return r.getBottom() - r.getHeight() * juce::jlimit (0.0f, 1.0f, (db + 72.0f) / 72.0f); };
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            for (float d = -60.0f; d < 0.0f; d += 12.0f) { g.drawVerticalLine ((int) aX (d), r.getY(), r.getBottom()); g.drawHorizontalLine ((int) aY (d), r.getX(), r.getRight()); }
            g.drawLine (aX (-72), aY (-72), aX (0), aY (0), 1.0f);
            juce::Path p;
            for (float d = -72.0f; d <= 0.01f; d += 0.5f)
            {
                const float y = d - din.reduccioPer (d);
                if (d <= -71.9f) p.startNewSubPath (aX (d), aY (y)); else p.lineTo (aX (d), aY (y));
            }
            g.setColour (tema::accent);
            g.strokePath (p, juce::PathStrokeType (2.0f));
            if (nivell > -72.0f)
            {
                const float x = juce::jmin (nivell, 0.0f);
                g.setColour (tema::text);
                g.fillEllipse (aX (x) - 4.0f, aY (x - din.reduccioPer (x)) - 4.0f, 8.0f, 8.0f);
            }
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (10.0f));
            g.drawText ("ENTRADA", r.removeFromBottom (12.0f), juce::Justification::centredRight);
        }
    private:
        Dinamica& din;
    };

    class PanellDinamica : public juce::Component, private juce::Timer, public OrigenPresets
    {
    public:
        PanellDinamica (std::shared_ptr<Dinamica> d, ServeiSidechain* sc, const std::string& id) : din (std::move (d)), corba (*din), ctrlSc (sc, id)
        {
            // 0.58: presets
            for (auto* b : { &btAnt, &btSeg, &btPreset, &btDesa }) addAndMakeVisible (b);
            addAndMakeVisible (btFav);
            btAnt.setTooltip ("Preset anterior"_ca);
            btSeg.setTooltip ("Preset següent"_ca);
            btPreset.setTooltip ("Clica per veure tots els presets (veus, guitarres, baix, bateria, sidechain…)"_ca);
            btFav.setTooltip ("Preferit"_ca);
            btDesa.setTooltip ("Desa els ajustos com un preset teu"_ca);
            btAnt.onClick = [this] { pas (-1); };
            btSeg.onClick = [this] { pas (1); };
            btPreset.onClick = [this] { obreNavegador(); };
            btFav.onClick = [this] { commutaFavoritActual(); };
            btDesa.onClick = [this] { desaPreset(); };
            addChildComponent (ctrlSc);
            ctrlSc.setVisible (sc != nullptr && ! id.empty());
            addAndMakeVisible (segMode);
            segMode.setTooltip (u8 (info (ModeP).ajuda));
            segMode.onCanvi = [this] (int m) { din->posa (ModeP, (float) m); refresca(); };
            addAndMakeVisible (segDet);
            segDet.setTooltip (u8 (info (Detector).ajuda));
            segDet.onCanvi = [this] (int m) { din->posa (Detector, (float) m); };
            addAndMakeVisible (btEscolta);
            btEscolta.setLlum (tema::accent);
            btEscolta.setTooltip (u8 (info (Escolta).ajuda));
            btEscolta.onClick = [this] { din->posa (Escolta, din->valor (Escolta) > 0.5f ? 0.0f : 1.0f); refresca(); };
            addAndMakeVisible (corba);
            for (int p : { (int) Llindar, (int) Ratio, (int) Atac, (int) Alliberament, (int) Genoll, (int) Rang, (int) Guany, (int) FiltreGreus, (int) FiltreAguts,
                           (int) FreqDeEsser, (int) Mix })
            {
                auto pom = std::make_unique<Pom> (u8 (info (p).nom), 0.0, 1.0, (double) Dinamica::normal (p, info (p).def), false,
                                                  [this, p] (double v) { return u8 (din->textParametre (p, (float) v)); });
                pom->setTooltip (u8 (info (p).ajuda));
                pom->onCanvi = [this, p] (double v) { din->posaParametre (p, (float) v); corba.repaint(); };
                pom->onReinicia = [this, p] { din->posa (p, info (p).def); refresca(); };
                addAndMakeVisible (*pom);
                poms.push_back ({ p, std::move (pom) });
            }
            setSize (820, 470);
            refresca();
            startTimerHz (30);
        }
        ~PanellDinamica() override { stopTimer(); }

        void refresca()
        {
            const auto nom = nomsPreset()[din.get()];
            btPreset.setButtonText (nom.isEmpty() ? "(sense preset)"_ca : nom);
            btFav.setToggleState (esFavorit (nom), juce::dontSendNotification);
            const int m = (int) din->valor (ModeP);
            segMode.setTriat (m);
            segDet.setTriat ((int) din->valor (Detector));
            btEscolta.setToggleState (din->valor (Escolta) > 0.5f, juce::dontSendNotification);
            for (auto& [p, pom] : poms)
            {
                pom->setValor (din->parametre (p), true);
                bool on = true;
                if (p == Ratio) on = m != Gate;
                if (p == Genoll) on = m == Compressor || m == DeEsser;
                if (p == Rang) on = m == Gate || m == Expander;
                if (p == FreqDeEsser) on = m == DeEsser;
                if (p == FiltreGreus || p == FiltreAguts) on = m != DeEsser;
                pom->setEnabled (on);
            }
            corba.repaint();
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (tema::fons);
            vintage::panell (g, getLocalBounds().removeFromTop (56).toFloat());
            auto t = getLocalBounds().removeFromTop (56).reduced (14, 9).removeFromLeft (120).toFloat();
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (17.0f, true));
            g.drawText ("DINÀMICA"_ca, t.removeFromTop (22.0f), juce::Justification::centredLeft);
            g.setColour (tema::accent);
            g.setFont (dibuix::lletra (10.0f, true));
            g.drawText ("MIBASE", t, juce::Justification::centredLeft);
            // Reducció de guany
            const auto& ms = din->mesures();
            auto b = zonaGR.toFloat();
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRoundedRectangle (b, 4.0f);
            const float red = juce::jlimit (0.0f, 1.0f, grVist / 30.0f);
            g.setColour (tema::accent);
            g.fillRoundedRectangle (b.withHeight (b.getHeight() * red), 4.0f);
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (11.0f, true));
            g.drawText ("GR", b.removeFromBottom (16.0f), juce::Justification::centred);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (11.0f));
            g.drawText ("-" + juce::String (grVist, 1).replaceCharacter ('.', ',') + " dB", zonaGR.translated (0, zonaGR.getHeight() + 2).withHeight (16),
                        juce::Justification::centred);
            // D'on escolta
            g.setFont (dibuix::lletra (11.5f, ms.sidechain.load()));
            g.setColour (ms.sidechain.load() ? tema::accent : tema::suau);
            g.drawText (ms.sidechain.load() ? "Escolta: SIDECHAIN (una altra pista). Botó SC: encén o apaga; al costat, tria la pista."_ca
                                                : "Escolta: el so d'aquesta pista. Per fer-la escoltar una altra pista: botó SC (a dalt)."_ca, zonaFont,
                        juce::Justification::centredLeft);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            auto dalt = r.removeFromTop (56).reduced (14, 12);
            {
                auto pr = r.removeFromTop (34).reduced (14, 3);
                btAnt.setBounds (pr.removeFromLeft (28));
                btPreset.setBounds (pr.removeFromLeft (juce::jmin (300, pr.getWidth() - 160)));
                btSeg.setBounds (pr.removeFromLeft (28));
                pr.removeFromLeft (6);
                btFav.setBounds (pr.removeFromLeft (26));
                pr.removeFromLeft (6);
                btDesa.setBounds (pr.removeFromLeft (80));
            }
            if (navegador != nullptr) navegador->setBounds (getLocalBounds());
            dalt.removeFromLeft (120);
            segMode.setBounds (dalt.removeFromLeft (juce::jmin (dalt.getWidth() - 420, segMode.ampladaIdeal() + 20)));
            btEscolta.setBounds (dalt.removeFromRight (100));
            dalt.removeFromRight (8);
            segDet.setBounds (dalt.removeFromRight (90));
            dalt.removeFromRight (8);
            ctrlSc.setBounds (dalt.removeFromRight (juce::jmin (200, juce::jmax (120, dalt.getWidth() - 10))));
            r.reduce (14, 10);
            zonaFont = r.removeFromBottom (20);
            auto esq = r.removeFromLeft (230);
            zonaGR = esq.removeFromRight (30).withTrimmedBottom (20);
            corba.setBounds (esq.withTrimmedRight (8).withSizeKeepingCentre (esq.getWidth() - 8, juce::jmin (esq.getHeight(), esq.getWidth() - 8)));
            r.removeFromLeft (12);
            const int col = 4, files = (int) (poms.size() + col - 1) / col;
            const int a = r.getWidth() / col, h = r.getHeight() / juce::jmax (1, files);
            for (size_t i = 0; i < poms.size(); ++i)
                poms[i].second->setBounds (r.getX() + (int) (i % col) * a, r.getY() + (int) (i / col) * h, a, h - 6);
        }

        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }

    private:
        void timerCallback() override
        {
            const auto& ms = din->mesures();
            const float r = ms.reduccio.load();
            grVist = r > grVist ? r : grVist * 0.9f + r * 0.1f;
            const float e = ms.entrada.load();
            corba.nivell = e > 1e-5f ? 20.0f * std::log10 (e) : -90.0f;
            corba.repaint();
            repaint (zonaGR.expanded (0, 20));
            repaint (zonaFont);
        }

        // ---- 0.58: presets (OrigenPresets) ----
        std::vector<const Preset*> totsPresets()
        {
            std::vector<const Preset*> v;
            for (auto& p : biblioteca().usuari) v.push_back (&p);
            for (auto& p : presetsFabrica()) v.push_back (&p);
            return v;
        }
        bool esFavorit (const juce::String& nom)
        {
            const auto n = nom.toStdString();
            return biblioteca().esFavorit (biblioteca().busca (n) != nullptr ? BibliotecaPresets::clauUsuari (n) : BibliotecaPresets::clauFabrica (n));
        }
        void commutaFavoritActual()
        {
            const auto nom = nomsPreset()[din.get()].toStdString();
            if (nom.empty()) { btFav.setToggleState (false, juce::dontSendNotification); return; }
            biblioteca().commutaFavorit (biblioteca().busca (nom) != nullptr ? BibliotecaPresets::clauUsuari (nom) : BibliotecaPresets::clauFabrica (nom));
            desaBiblioteca();
            refresca();
        }
        void aplicaPreset (const Preset& p)
        {
            din->posaEstat (p.estat);
            nomsPreset()[din.get()] = u8 (p.nom);
            refresca();
        }
        void pas (int d)
        {
            const auto v = totsPresets();
            if (v.empty()) return;
            const auto nom = nomsPreset()[din.get()].toStdString();
            int i = -1;
            for (int k = 0; k < (int) v.size(); ++k) if (v[(size_t) k]->nom == nom) i = k;
            i = i < 0 ? (d > 0 ? 0 : (int) v.size() - 1) : (i + d + (int) v.size()) % (int) v.size();
            aplicaPreset (*v[(size_t) i]);
        }
        void desaPreset()
        {
            const auto actual = nomsPreset()[din.get()];
            const bool meu = biblioteca().busca (actual.toStdString()) != nullptr;
            const auto proposta = meu ? actual : u8 (biblioteca().nomLliure ((actual.isEmpty() ? juce::String ("Dinàmica"_ca) : actual + " (meu)"_ca).toStdString()));
            dialeg = std::make_unique<DialegText> ("Desa el preset"_ca, "Nom del preset (si ja en tens un amb aquest nom, se substitueix)."_ca, "Desa"_ca,
                [] (const juce::String& t) { return t.trim().isEmpty() ? "Escriu un nom."_ca : juce::String(); },
                [this] (const juce::String& t)
                {
                    const auto nom = t.trim().toStdString();
                    const auto* ja = biblioteca().busca (nom);
                    biblioteca().desaPreset ({ nom, ja != nullptr ? ja->categoria : std::string ("Meus"), "", din->estat() });
                    desaBiblioteca();
                    nomsPreset()[din.get()] = u8 (nom);
                    refresca();
                }, proposta);
            DialegText::mostra (*this, dialeg);
        }
        void obreNavegador()
        {
            navegador = std::make_unique<NavegadorPresetsComu> (*this);
            addAndMakeVisible (*navegador);
            navegador->setBounds (getLocalBounds());
            navegador->grabKeyboardFocus();
        }
        juce::StringArray categoriesFabrica() override { juce::StringArray a; for (auto& c : categories()) a.add (u8 (c)); return a; }
        juce::StringArray carpetesUsuari() override { juce::StringArray a; for (auto& c : biblioteca().carpetes()) a.add (u8 (c)); return a; }
        std::vector<EntradaPreset> entrades() override
        {
            std::vector<EntradaPreset> v;
            const auto tots = totsPresets();
            const int nu = (int) biblioteca().usuari.size();
            for (int i = 0; i < (int) tots.size(); ++i)
            {
                const auto& p = *tots[(size_t) i];
                Dinamica d;
                d.posaEstat (p.estat);
                const bool usuari = i < nu;
                v.push_back ({ u8 (p.nom), u8 (p.categoria), u8 (p.descripcio), u8 (nomMode ((int) d.valor (ModeP))), usuari,
                               biblioteca().esFavorit (usuari ? BibliotecaPresets::clauUsuari (p.nom) : BibliotecaPresets::clauFabrica (p.nom)), i });
            }
            return v;
        }
        juce::String presetActual() override { return nomsPreset()[din.get()]; }
        void aplica (const EntradaPreset& e) override
        {
            const auto tots = totsPresets();
            if (e.index >= 0 && e.index < (int) tots.size()) aplicaPreset (*tots[(size_t) e.index]);
        }
        void commutaFavorit (const EntradaPreset& e) override
        {
            const auto n = e.nom.toStdString();
            biblioteca().commutaFavorit (e.usuari ? BibliotecaPresets::clauUsuari (n) : BibliotecaPresets::clauFabrica (n));
            desaBiblioteca();
            refresca();
        }
        bool existeixMeu (const juce::String& nom) override { return biblioteca().busca (nom.toStdString()) != nullptr; }
        void reanomena (const juce::String& vell, const juce::String& nou) override
        {
            if (biblioteca().reanomena (vell.toStdString(), nou.toStdString()))
            {
                desaBiblioteca();
                if (nomsPreset()[din.get()] == vell) nomsPreset()[din.get()] = nou;
            }
            refresca();
        }
        void mou (const juce::String& nom, const juce::String& carpeta) override { biblioteca().mou (nom.toStdString(), carpeta.toStdString()); desaBiblioteca(); }
        void esborra (const juce::String& nom) override { biblioteca().esborra (nom.toStdString()); desaBiblioteca(); refresca(); }
        void tancaNavegador() override
        {
            juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PanellDinamica> (this)] { if (sp != nullptr) { sp->navegador.reset(); sp->grabKeyboardFocus(); } });
        }

        std::unique_ptr<NavegadorPresetsComu> navegador;
        std::unique_ptr<DialegText> dialeg;
        Boto btAnt { {}, dibuix::Icona::Rebobina, Boto::Estil::Subtil }, btSeg { {}, dibuix::Icona::Endavant, Boto::Estil::Subtil };
        Boto btPreset { "(sense preset)"_ca, std::nullopt, Boto::Estil::Secundari };
        BotoEstrella btFav;
        Boto btDesa { "Desa", dibuix::Icona::Disc, Boto::Estil::Secundari };
        std::shared_ptr<Dinamica> din;
        Segmentat segMode { { "Compressor", "Gate", "Expander", "De-esser" } };
        Segmentat segDet { { "Peak", "RMS" } };
        Boto btEscolta { "Escolta SC", std::nullopt, Boto::Estil::Tecla };
        Corba corba;
        ControlSidechain ctrlSc;
        std::vector<std::pair<int, std::unique_ptr<Pom>>> poms;
        juce::Rectangle<int> zonaGR, zonaFont;
        float grVist = 0.0f;
        juce::TooltipWindow tips { this, 500 };
    };

    class FinestraDinamica : public juce::DocumentWindow
    {
    public:
        FinestraDinamica (std::shared_ptr<Dinamica> d, const juce::String& titol, std::function<void()> enTancar, ServeiSidechain* sc, const std::string& id)
            : juce::DocumentWindow (titol, tema::fons, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
              tancar (std::move (enTancar))
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new PanellDinamica (std::move (d), sc, id), true);
            setResizable (true, true);
            setResizeLimits (640, 360, 1600, 1000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::MessageManager::callAsync ([t = tancar] { t(); }); }
        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }
    private:
        std::function<void()> tancar;
    };
}

std::unique_ptr<juce::DocumentWindow> creaFinestraDinamica (std::shared_ptr<Dinamica> d, const juce::String& titol, std::function<void()> enTancar,
                                                            ServeiSidechain* sc, const std::string& idInsert)
{
    return std::make_unique<FinestraDinamica> (std::move (d), titol, std::move (enTancar), sc, idInsert);
}
