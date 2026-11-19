#include "FinestraDelay.h"

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

using namespace mibase::delay;

namespace
{
    juce::String u8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str(), (int) s.size()); }
    juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }
    juce::String coma (double x, int d) { return juce::String (x, d).replaceCharacter ('.', ','); }

    // Presets propis i favorits (Documents\Mibase\Delay\presets.json)
    juce::File fitxerBiblioteca()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Mibase").getChildFile ("Delay").getChildFile ("presets.json");
    }
    BibliotecaPresets& biblioteca()
    {
        static BibliotecaPresets b;
        static bool carregada = false;
        if (! carregada) { carregada = true; b.carrega (dibuix::cami (fitxerBiblioteca())); }
        return b;
    }
    void desaBiblioteca() { biblioteca().desa (dibuix::cami (fitxerBiblioteca())); }
    std::map<const void*, juce::String>& nomsPreset() { static std::map<const void*, juce::String> m; return m; }

    /** Els ecos que sentiràs: amunt l'esquerra, avall la dreta (es calcula, no escolta el so). */
    class VistaEcos : public juce::Component
    {
    public:
        explicit VistaEcos (Delay& d) : dl (d) {}
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRoundedRectangle (r, 5.0f);
            auto cap = r.removeFromTop (22.0f).reduced (10.0f, 2.0f);
            r.reduce (10.0f, 8.0f);
            const auto ecos = dl.ecos (32, 4.0);
            double fi = 1.0;
            for (const auto& e : ecos) fi = std::max (fi, e.temps * 1.08);
            fi = std::min (fi, 4.0);
            const float mig = r.getCentreY();
            auto aX = [&] (double t) { return r.getX() + (float) (t / fi) * r.getWidth(); };
            // temps de la cançó (negres)
            const double negra = 60.0 / dl.bpm();
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            for (double t = negra; t < fi; t += negra) g.drawVerticalLine ((int) aX (t), r.getY(), r.getBottom());
            g.setColour (tema::linia);
            g.drawHorizontalLine ((int) mig, r.getX(), r.getRight());
            // el so original
            g.setColour (tema::textMig);
            g.fillRect (juce::Rectangle<float> (aX (0.0), r.getY() + 4.0f, 3.0f, r.getHeight() - 8.0f));
            const float alt = r.getHeight() * 0.5f - 4.0f;
            for (const auto& e : ecos)
            {
                const float x = aX (e.temps);
                if (e.l > 0.0f) { g.setColour (tema::accent.withAlpha (0.4f + 0.6f * e.l)); g.fillRect (juce::Rectangle<float> (x - 1.5f, mig - alt * e.l, 3.0f, alt * e.l)); }
                if (e.r > 0.0f) { g.setColour (tema::accent.brighter (0.4f).withAlpha (0.4f + 0.6f * e.r)); g.fillRect (juce::Rectangle<float> (x - 1.5f, mig, 3.0f, alt * e.r)); }
            }
            g.setFont (dibuix::lletra (10.0f));
            g.setColour (tema::suau);
            g.drawText ("E", juce::Rectangle<float> (r.getRight() - 12.0f, r.getY(), 12.0f, 12.0f), juce::Justification::centred);
            g.drawText ("D", juce::Rectangle<float> (r.getRight() - 12.0f, r.getBottom() - 12.0f, 12.0f, 12.0f), juce::Justification::centred);
            // a dalt: el temps
            const bool sync = dl.valor (Sync) > 0.5f;
            juce::String t = sync ? u8 (nomNota ((int) dl.valor (Nota))) + "  ·  " : juce::String();
            t << coma (dl.tempsL() * 1000.0, 0) << " ms";
            if (std::fabs (dl.valor (Desfas)) > 0.5f && (int) dl.valor (EncaminamentP) != Mono) t << "  /  D " << coma (dl.tempsR() * 1000.0, 0) << " ms";
            if (dl.valor (Freeze) > 0.5f) t << "  ·  FREEZE";
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (12.5f, true));
            g.drawText (t, cap, juce::Justification::centredLeft);
        }
    private:
        Delay& dl;
    };

    class PanellDelay : public juce::Component, private juce::Timer, public OrigenPresets
    {
    public:
        explicit PanellDelay (std::shared_ptr<Delay> d) : dl (std::move (d)), vista (*dl)
        {
            for (auto* b : { &btAnt, &btSeg, &btPreset, &btDesa }) addAndMakeVisible (b);
            addAndMakeVisible (btFav);
            btAnt.setTooltip ("Preset anterior"_ca);
            btSeg.setTooltip ("Preset següent"_ca);
            btPreset.setTooltip ("Clica per veure tots els presets (rítmics, ping-pong, cinta, veus, guitarres, ambient…)"_ca);
            btFav.setTooltip ("Preferit"_ca);
            btDesa.setTooltip ("Desa els ajustos com un preset teu"_ca);
            btAnt.onClick = [this] { pas (-1); };
            btSeg.onClick = [this] { pas (1); };
            btPreset.onClick = [this] { obreNavegador(); };
            btFav.onClick = [this] { commutaFavoritActual(); };
            btDesa.onClick = [this] { desaPreset(); };
            addAndMakeVisible (segCar);
            segCar.setTooltip (u8 (info (CaracterP).ajuda));
            segCar.onCanvi = [this] (int m) { dl->posa (CaracterP, (float) m); refresca(); };
            addAndMakeVisible (segEnc);
            segEnc.setTooltip (u8 (info (EncaminamentP).ajuda));
            segEnc.onCanvi = [this] (int m) { dl->posa (EncaminamentP, (float) m); refresca(); };
            for (auto* b : { &btSync, &btFreeze }) { addAndMakeVisible (b); b->setLlum (tema::accent); }
            btSync.setTooltip (u8 (info (Sync).ajuda));
            btFreeze.setTooltip (u8 (info (Freeze).ajuda));
            btSync.onClick = [this] { dl->posa (Sync, dl->valor (Sync) > 0.5f ? 0.0f : 1.0f); refresca(); };
            btFreeze.onClick = [this] { dl->posa (Freeze, dl->valor (Freeze) > 0.5f ? 0.0f : 1.0f); refresca(); };
            addAndMakeVisible (vista);
            for (int p : { (int) Temps, (int) Nota, (int) Desfas, (int) Feedback, (int) Mix,
                           (int) LowCut, (int) HighCut, (int) Drive, (int) Difusio, (int) Amplada,
                           (int) ModRate, (int) ModDepth, (int) DuckAmt, (int) DuckThr, (int) DuckRel, (int) Sortida })
            {
                auto pom = std::make_unique<Pom> (u8 (info (p).nom), 0.0, 1.0, (double) Delay::normal (p, info (p).def), p == Desfas,
                                                  [this, p] (double v) { return u8 (dl->textParametre (p, (float) v)); });
                pom->setTooltip (u8 (info (p).ajuda));
                pom->onCanvi = [this, p] (double v) { dl->posaParametre (p, (float) v); vista.repaint(); };
                pom->onReinicia = [this, p] { dl->posa (p, info (p).def); refresca(); };
                addAndMakeVisible (*pom);
                poms.push_back ({ p, std::move (pom) });
            }
            setSize (900, 480);
            refresca();
            startTimerHz (15);
        }
        ~PanellDelay() override { stopTimer(); }

        void refresca()
        {
            const auto nom = nomsPreset()[dl.get()];
            btPreset.setButtonText (nom.isEmpty() ? "(sense preset)"_ca : nom);
            btFav.setToggleState (esFavorit (nom), juce::dontSendNotification);
            segCar.setTriat ((int) dl->valor (CaracterP));
            segEnc.setTriat ((int) dl->valor (EncaminamentP));
            const bool sync = dl->valor (Sync) > 0.5f;
            btSync.setToggleState (sync, juce::dontSendNotification);
            btFreeze.setToggleState (dl->valor (Freeze) > 0.5f, juce::dontSendNotification);
            const bool duck = dl->valor (DuckAmt) > 0.05f;
            for (auto& [p, pom] : poms)
            {
                pom->setValor (dl->parametre (p), true);
                if (p == Temps) pom->setVisible (! sync);
                if (p == Nota) pom->setVisible (sync);
                if (p == Desfas) pom->setEnabled ((int) dl->valor (EncaminamentP) != Mono);
                if (p == DuckThr || p == DuckRel) pom->setEnabled (duck);
            }
            vista.repaint();
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (tema::fons);
            vintage::panell (g, getLocalBounds().removeFromTop (56).toFloat());
            auto t = getLocalBounds().removeFromTop (56).reduced (14, 9).removeFromLeft (100).toFloat();
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (17.0f, true));
            g.drawText ("DELAY", t.removeFromTop (22.0f), juce::Justification::centredLeft);
            g.setColour (tema::accent);
            g.setFont (dibuix::lletra (10.0f, true));
            g.drawText ("MIBASE", t, juce::Justification::centredLeft);
            // nivell dels ecos i ducking
            const auto& ms = dl->mesures();
            auto b = zonaNivell.toFloat();
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRoundedRectangle (b, 4.0f);
            const float e = ms.eco.load();
            const float alt = juce::jlimit (0.0f, 1.0f, e > 1e-5f ? (20.0f * std::log10 (e) + 60.0f) / 60.0f : 0.0f);
            g.setColour (tema::accent);
            g.fillRoundedRectangle (b.withTop (b.getBottom() - b.getHeight() * alt), 4.0f);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (10.0f));
            g.drawText ("ECO", zonaNivell.translated (0, zonaNivell.getHeight() + 2).withHeight (14), juce::Justification::centred);
            // text de baix: el caràcter i el ducking
            juce::String txt = u8 (descripcioCaracter ((int) dl->valor (CaracterP)));
            if (const float dk = ms.ducking.load(); dk > 0.3f) txt = "Ducking: els ecos baixen "_ca + coma (dk, 1) + " dB mentre toques.";
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (11.5f));
            g.drawFittedText (txt, zonaText, juce::Justification::centredLeft, 2, 0.85f);
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
            dalt.removeFromLeft (100);
            btFreeze.setBounds (dalt.removeFromRight (78));
            dalt.removeFromRight (6);
            btSync.setBounds (dalt.removeFromRight (64));
            dalt.removeFromRight (10);
            const int amp = dalt.getWidth();
            segCar.setBounds (dalt.removeFromLeft (amp * 55 / 100));
            dalt.removeFromLeft (8);
            segEnc.setBounds (dalt);
            r.reduce (14, 10);
            zonaText = r.removeFromBottom (32);
            auto esq = r.removeFromLeft (juce::jmax (220, r.getWidth() * 36 / 100));
            zonaNivell = esq.removeFromRight (16).withTrimmedBottom (18);
            esq.removeFromRight (6);
            vista.setBounds (esq);
            r.removeFromLeft (12);
            // 5 columnes: Temps i Nota comparteixen el primer lloc
            const int col = 5, files = 3;
            const int a = r.getWidth() / col, h = r.getHeight() / files;
            int k = 0;
            for (auto& [p, pom] : poms)
            {
                const int i = p == Nota ? 0 : k;
                pom->setBounds (r.getX() + (i % col) * a, r.getY() + (i / col) * h, a, h - 6);
                if (p != Temps) ++k;
            }
        }

        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }

    private:
        void timerCallback() override
        {
            repaint (zonaNivell.expanded (2, 20));
            repaint (zonaText);
            // si s'automatitza o canvia el tempo, els ecos dibuixats ho segueixen
            const auto ara = dl->estat() + "|" + juce::String (dl->bpm()).toStdString();
            if (ara != vist) { vist = ara; for (auto& [p, pom] : poms) pom->setValor (dl->parametre (p), true); vista.repaint(); }
        }

        // ---- presets (OrigenPresets), com a la Dinàmica ----
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
            const auto nom = nomsPreset()[dl.get()].toStdString();
            if (nom.empty()) { btFav.setToggleState (false, juce::dontSendNotification); return; }
            biblioteca().commutaFavorit (biblioteca().busca (nom) != nullptr ? BibliotecaPresets::clauUsuari (nom) : BibliotecaPresets::clauFabrica (nom));
            desaBiblioteca();
            refresca();
        }
        void aplicaPreset (const Preset& p)
        {
            dl->posaEstat (p.estat);
            nomsPreset()[dl.get()] = u8 (p.nom);
            refresca();
        }
        void pas (int d)
        {
            const auto v = totsPresets();
            if (v.empty()) return;
            const auto nom = nomsPreset()[dl.get()].toStdString();
            int i = -1;
            for (int k = 0; k < (int) v.size(); ++k) if (v[(size_t) k]->nom == nom) i = k;
            i = i < 0 ? (d > 0 ? 0 : (int) v.size() - 1) : (i + d + (int) v.size()) % (int) v.size();
            aplicaPreset (*v[(size_t) i]);
        }
        void desaPreset()
        {
            const auto actual = nomsPreset()[dl.get()];
            const bool meu = biblioteca().busca (actual.toStdString()) != nullptr;
            const auto proposta = meu ? actual : u8 (biblioteca().nomLliure ((actual.isEmpty() ? juce::String ("Delay"_ca) : actual + " (meu)"_ca).toStdString()));
            dialeg = std::make_unique<DialegText> ("Desa el preset"_ca, "Nom del preset (si ja en tens un amb aquest nom, se substitueix)."_ca, "Desa"_ca,
                [] (const juce::String& t) { return t.trim().isEmpty() ? "Escriu un nom."_ca : juce::String(); },
                [this] (const juce::String& t)
                {
                    const auto nom = t.trim().toStdString();
                    const auto* ja = biblioteca().busca (nom);
                    biblioteca().desaPreset ({ nom, ja != nullptr ? ja->categoria : std::string ("Meus"), "", dl->estat() });
                    desaBiblioteca();
                    nomsPreset()[dl.get()] = u8 (nom);
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
                Delay d;
                d.posaEstat (p.estat);
                const bool usuari = i < nu;
                v.push_back ({ u8 (p.nom), u8 (p.categoria), u8 (p.descripcio), u8 (nomCaracter ((int) d.valor (CaracterP))), usuari,
                               biblioteca().esFavorit (usuari ? BibliotecaPresets::clauUsuari (p.nom) : BibliotecaPresets::clauFabrica (p.nom)), i });
            }
            return v;
        }
        juce::String presetActual() override { return nomsPreset()[dl.get()]; }
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
                if (nomsPreset()[dl.get()] == vell) nomsPreset()[dl.get()] = nou;
            }
            refresca();
        }
        void mou (const juce::String& nom, const juce::String& carpeta) override { biblioteca().mou (nom.toStdString(), carpeta.toStdString()); desaBiblioteca(); }
        void esborra (const juce::String& nom) override { biblioteca().esborra (nom.toStdString()); desaBiblioteca(); refresca(); }
        void tancaNavegador() override
        {
            juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PanellDelay> (this)] { if (sp != nullptr) { sp->navegador.reset(); sp->grabKeyboardFocus(); } });
        }

        std::unique_ptr<NavegadorPresetsComu> navegador;
        std::unique_ptr<DialegText> dialeg;
        Boto btAnt { {}, dibuix::Icona::Rebobina, Boto::Estil::Subtil }, btSeg { {}, dibuix::Icona::Endavant, Boto::Estil::Subtil };
        Boto btPreset { "(sense preset)"_ca, std::nullopt, Boto::Estil::Secundari };
        BotoEstrella btFav;
        Boto btDesa { "Desa", dibuix::Icona::Disc, Boto::Estil::Secundari };
        std::shared_ptr<Delay> dl;
        Segmentat segCar { { "Digital", "Cinta", "Analògic"_ca, "Lo-fi" } };
        Segmentat segEnc { { "Estèreo"_ca, "Ping-pong", "Mono" } };
        Boto btSync { "Sync", std::nullopt, Boto::Estil::Tecla };
        Boto btFreeze { "Freeze", std::nullopt, Boto::Estil::Tecla };
        VistaEcos vista;
        std::vector<std::pair<int, std::unique_ptr<Pom>>> poms;
        juce::Rectangle<int> zonaNivell, zonaText;
        std::string vist;
        juce::TooltipWindow tips { this, 500 };
    };

    class FinestraDelay : public juce::DocumentWindow
    {
    public:
        FinestraDelay (std::shared_ptr<Delay> d, const juce::String& titol, std::function<void()> enTancar)
            : juce::DocumentWindow (titol, tema::fons, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
              tancar (std::move (enTancar))
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new PanellDelay (std::move (d)), true);
            setResizable (true, true);
            setResizeLimits (720, 400, 1700, 1000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::MessageManager::callAsync ([t = tancar] { t(); }); }
        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }
    private:
        std::function<void()> tancar;
    };
}

std::unique_ptr<juce::DocumentWindow> creaFinestraDelay (std::shared_ptr<Delay> d, const juce::String& titol, std::function<void()> enTancar)
{
    return std::make_unique<FinestraDelay> (std::move (d), titol, std::move (enTancar));
}
