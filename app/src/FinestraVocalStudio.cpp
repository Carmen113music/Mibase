#include "FinestraVocalStudio.h"

#include "Controls.h"
#include "DialegText.h"
#include "Dibuix.h"
#include "EscGlobal.h"
#include "FinestraDelay.h"
#include "FinestraDinamica.h"
#include "FinestraDrive.h"
#include "FinestraEq.h"
#include "FinestraFlotant.h"
#include "FinestraLimitador.h"
#include "FinestraReverb.h"
#include "FinestraSaturacio.h"
#include "NavegadorPresets.h"
#include "Pom.h"
#include "Tema.h"
#include "Text.h"
#include "Vintage.h"
#include <map>
#include <mibase/Delay.h>
#include <mibase/Doubler.h>
#include <mibase/Drive.h>
#include <mibase/Equalitzador.h>
#include <mibase/Limitador.h>
#include <mibase/Reverb.h>
#include <mibase/Saturacio.h>

using namespace mibase::rack;

namespace
{
    juce::String u8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str(), (int) s.size()); }
    juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }
    const juce::Colour turquesa { 0xff59d0d6 };   // el color de la veu (com a Riffs)

    // Presets propis i favorits (Documents\Mibase\Vocal Studio\presets.json)
    juce::File fitxerBiblioteca()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Mibase").getChildFile ("Vocal Studio").getChildFile ("presets.json");
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

    juce::String nomDe (const Modul& m) { return u8 (nomModul (m.clau, m.fx.get())); }
    juce::String nomParam (const Modul& m, int p) { return u8 (m.fx->infoParametre (p).nom); }

    /** Els paràmetres que val la pena oferir per enllaçar a un macro (a l'EQ, només els de les bandes que es fan servir). */
    std::vector<int> paramsOferts (const Modul& m)
    {
        std::vector<int> v;
        auto* q = dynamic_cast<mibase::eq::Equalitzador*> (m.fx.get());
        for (int p = 0, n = m.fx->numParametres(); p < n; ++p)
        {
            const auto inf = m.fx->infoParametre (p);
            if (! inf.automatitzable || inf.id.empty()) continue;
            if (q != nullptr && p >= mibase::eq::numGlobals)
            {
                const int banda = (p - mibase::eq::numGlobals) / mibase::eq::paramsPerBanda;
                if (! q->banda (banda).usada) continue;
            }
            v.push_back (p);
        }
        return v;
    }

    // ------------------------------------------------------------------
    /** Una fletxa petita (amunt / avall) per canviar l'ordre dels mòduls. */
    class BotoFletxa : public juce::Button
    {
    public:
        explicit BotoFletxa (bool a) : juce::Button ({}), amunt (a) {}
        void paintButton (juce::Graphics& g, bool sobre, bool premut) override
        {
            auto r = getLocalBounds().toFloat().reduced (5.0f, 7.0f);
            juce::Path p;
            if (amunt) p.addTriangle (r.getCentreX(), r.getY(), r.getRight(), r.getBottom(), r.getX(), r.getBottom());
            else p.addTriangle (r.getX(), r.getY(), r.getRight(), r.getY(), r.getCentreX(), r.getBottom());
            g.setColour (! isEnabled() ? tema::suau.withAlpha (0.25f) : premut ? turquesa : sobre ? tema::text : tema::suau);
            g.fillPath (p);
        }
    private:
        bool amunt;
    };

    /** Per als mòduls que no tenen finestra pròpia (el Doubler): un pom per paràmetre. */
    class PanellParametres : public juce::Component, private juce::Timer
    {
    public:
        explicit PanellParametres (std::shared_ptr<mibase::EfecteIntern> e) : fx (std::move (e))
        {
            for (int p = 0; p < fx->numParametres(); ++p)
            {
                auto pom = std::make_unique<Pom> (u8 (fx->infoParametre (p).nom), 0.0, 1.0, (double) fx->parametre (p), false,
                                                  [this, p] (double v) { return u8 (fx->textParametre (p, (float) v)); });
                pom->onCanvi = [this, p] (double v) { fx->posaParametre (p, (float) v); };
                if (auto* d = dynamic_cast<mibase::doubler::Doubler*> (fx.get()))
                {
                    pom->setTooltip (u8 (mibase::doubler::info (p).ajuda));
                    pom->onReinicia = [this, d, p] { d->posa (p, mibase::doubler::info (p).def); };
                }
                addAndMakeVisible (*pom);
                poms.push_back (std::move (pom));
            }
            setSize (juce::jmax (360, 92 * (int) poms.size() + 28), 170);
            startTimerHz (15);
        }
        void paint (juce::Graphics& g) override { g.fillAll (tema::fons); vintage::panell (g, getLocalBounds().toFloat().reduced (6.0f), 6.0f); }
        void resized() override
        {
            auto r = getLocalBounds().reduced (14, 18);
            const int a = r.getWidth() / juce::jmax (1, (int) poms.size());
            for (auto& p : poms) p->setBounds (r.removeFromLeft (a).reduced (4, 0));
        }
        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }
    private:
        void timerCallback() override { for (int p = 0; p < (int) poms.size(); ++p) poms[(size_t) p]->setValor (fx->parametre (p), true); }
        std::shared_ptr<mibase::EfecteIntern> fx;
        std::vector<std::unique_ptr<Pom>> poms;
    };

    class FinestraSimple : public juce::DocumentWindow
    {
    public:
        FinestraSimple (const juce::String& titol, juce::Component* contingut, std::function<void()> enTancar)
            : juce::DocumentWindow (titol, tema::fons, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton), tancar (std::move (enTancar))
        {
            setUsingNativeTitleBar (true);
            setContentOwned (contingut, true);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::MessageManager::callAsync ([t = tancar] { t(); }); }
        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }
    private:
        std::function<void()> tancar;
    };

    // ------------------------------------------------------------------
    /** Els enllaços d'un macro: per a cadascun, on va quan el macro és a 0 i on va quan és a 10. */
    class EditorEnllacos : public juce::Component
    {
    public:
        EditorEnllacos (std::shared_ptr<Rack> r, int m, std::function<void()> canviat) : rk (std::move (r)), macro (m), avisa (std::move (canviat))
        {
            refes();
        }
        void refes()
        {
            files.clear();
            const auto& en = rk->enllacos();
            for (int k = 0; k < (int) en.size(); ++k)
            {
                if (en[(size_t) k].macro != macro) continue;
                auto f = std::make_unique<Fila>();
                const auto e = en[(size_t) k];
                auto* mod = rk->modul (e.modul);
                if (mod == nullptr) continue;
                f->k = k;
                f->nom = nomDe (*mod) + " · "_ca + nomParam (*mod, e.param);
                auto text = [this, e] (double v) { auto* md = rk->modul (e.modul); return md != nullptr ? u8 (md->fx->textParametre (e.param, (float) v)) : juce::String(); };
                f->mn = std::make_unique<Pom> ("A 0", 0.0, 1.0, (double) e.mn, false, text);
                f->mx = std::make_unique<Pom> ("A 10", 0.0, 1.0, (double) e.mx, false, text);
                f->mn->setTooltip ("On va el paràmetre amb el macro a 0"_ca);
                f->mx->setTooltip ("On va el paràmetre amb el macro a 10"_ca);
                auto posa = [this, k] (bool esMin, double v)
                {
                    if (k >= (int) rk->enllacos().size()) return;
                    auto e2 = rk->enllacos()[(size_t) k];
                    (esMin ? e2.mn : e2.mx) = (float) v;
                    rk->posaEnllac (k, e2);
                    rk->aplicaMacro (macro);
                    if (avisa) avisa();
                };
                f->mn->onCanvi = [posa] (double v) { posa (true, v); };
                f->mx->onCanvi = [posa] (double v) { posa (false, v); };
                f->treu.onClick = [this, k]
                {
                    juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<EditorEnllacos> (this), k]
                    {
                        if (sp == nullptr) return;
                        sp->rk->treuEnllac (k);
                        if (sp->avisa) sp->avisa();
                        sp->refes();
                    });
                };
                f->treu.setTooltip ("Treu aquest enllaç"_ca);
                addAndMakeVisible (*f->mn);
                addAndMakeVisible (*f->mx);
                addAndMakeVisible (f->treu);
                files.push_back (std::move (f));
            }
            setSize (470, juce::jmax (60, 40 + 74 * (int) files.size()));
            resized();
            repaint();
        }
        void paint (juce::Graphics& g) override
        {
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (13.0f, true));
            g.drawText ("M" + juce::String (macro + 1) + " · "_ca + u8 (rk->nomMacro (macro)), getLocalBounds().removeFromTop (30).reduced (12, 0), juce::Justification::centredLeft);
            if (files.empty())
            {
                g.setColour (tema::suau);
                g.setFont (dibuix::lletra (12.0f));
                g.drawText ("Encara no mou res. Enllaça-hi un paràmetre des del menú del macro."_ca, getLocalBounds().withTrimmedTop (30).reduced (12, 0), juce::Justification::centredLeft);
            }
            g.setFont (dibuix::lletra (12.0f));
            for (auto& f : files)
            {
                g.setColour (tema::text);
                g.drawFittedText (f->nom, f->zonaNom, juce::Justification::centredLeft, 2);
            }
        }
        void resized() override
        {
            auto r = getLocalBounds().withTrimmedTop (34).reduced (12, 0);
            for (auto& f : files)
            {
                auto fila = r.removeFromTop (70);
                r.removeFromTop (4);
                f->treu.setBounds (fila.removeFromRight (28).withSizeKeepingCentre (26, 26));
                f->mx->setBounds (fila.removeFromRight (78));
                f->mn->setBounds (fila.removeFromRight (78));
                f->zonaNom = fila;
            }
        }
    private:
        struct Fila
        {
            int k = 0;
            juce::String nom;
            juce::Rectangle<int> zonaNom;
            std::unique_ptr<Pom> mn, mx;
            Boto treu { {}, dibuix::Icona::Tanca, Boto::Estil::Subtil };
        };
        std::shared_ptr<Rack> rk;
        int macro;
        std::function<void()> avisa;
        std::vector<std::unique_ptr<Fila>> files;
    };

    // ------------------------------------------------------------------
    class PanellVocalStudio;

    /** Un mòdul del rack, com una unitat d'un rack d'estudi: encès, nom, Mix, ordre, obrir i treure. */
    class FilaModul : public juce::Component
    {
    public:
        FilaModul (PanellVocalStudio& p, std::shared_ptr<Rack> r, int i);
        void paint (juce::Graphics& g) override;
        void resized() override;
        void refresca();
        void mouseDoubleClick (const juce::MouseEvent&) override;
        int index;
    private:
        PanellVocalStudio& pare;
        std::shared_ptr<Rack> rk;
        Boto btOn { {}, dibuix::Icona::Encen, Boto::Estil::Tecla };
        Boto btObre { "Obre", dibuix::Icona::Finestra, Boto::Estil::Secundari };
        Boto btTreu { {}, dibuix::Icona::Tanca, Boto::Estil::Subtil };
        BotoFletxa btAmunt { true }, btAvall { false };
        std::unique_ptr<Pom> pomMix;
        juce::Rectangle<int> zonaNom;
    };

    class PanellVocalStudio : public juce::Component, private juce::Timer, public OrigenPresets
    {
    public:
        PanellVocalStudio (std::shared_ptr<Rack> r, juce::String t) : rk (std::move (r)), titol (std::move (t))
        {
            for (auto* b : { &btAnt, &btSeg, &btPreset, &btDesa }) addAndMakeVisible (b);
            addAndMakeVisible (btFav);
            btAnt.setTooltip ("Preset anterior"_ca);
            btSeg.setTooltip ("Preset següent"_ca);
            btPreset.setTooltip ("Clica per veure tots els presets (bàsics, íntims, pop, urbans, rock, efectes)"_ca);
            btFav.setTooltip ("Preferit"_ca);
            btDesa.setTooltip ("Desa tot el rack (mòduls, ajustos i macros) com un preset teu"_ca);
            btAnt.onClick = [this] { pas (-1); };
            btSeg.onClick = [this] { pas (1); };
            btPreset.onClick = [this] { obreNavegador(); };
            btFav.onClick = [this] { commutaFavoritActual(); };
            btDesa.onClick = [this] { desaPreset(); };
            // entrada i sortida
            for (int p : { (int) Entrada, (int) Sortida })
            {
                auto pom = std::make_unique<Pom> (p == Entrada ? "Entrada"_ca : "Sortida"_ca, 0.0, 1.0, 0.5, true,
                                                  [this, p] (double v) { return u8 (rk->textParametre (p, (float) v)); });
                pom->setTooltip (p == Entrada ? "Volum que entra al rack (dB)"_ca : "Volum que surt del rack (dB)"_ca);
                pom->onCanvi = [this, p] (double v) { rk->posaParametre (p, (float) v); };
                pom->onReinicia = [this, p] { rk->posaParametre (p, 0.5f); };
                addAndMakeVisible (*pom);
                (p == Entrada ? pomIn : pomOut) = std::move (pom);
            }
            // macros
            for (int m = 0; m < numMacros; ++m)
            {
                auto& mc = macros[(size_t) m];
                mc.pom = std::make_unique<Pom> (juce::String(), 0.0, 1.0, 0.0, false, [] (double v) { return juce::String (v * 10.0, 1).replaceCharacter ('.', ','); });
                mc.pom->onCanvi = [this, m] (double v) { rk->posaMacro (m, (float) v); };
                addAndMakeVisible (*mc.pom);
                mc.nom.onClick = [this, m] { menuMacro (m); };
                mc.nom.onClicDret = [this, m] { menuMacro (m); };
                mc.nom.setTooltip ("Clica per posar-li nom, enllaçar-hi paràmetres o ajustar-ne el rang"_ca);
                addAndMakeVisible (mc.nom);
            }
            // el rack
            addAndMakeVisible (vista);
            vista.setViewedComponent (&contingut, false);
            vista.setScrollBarsShown (true, false);
            addAndMakeVisible (btAfegeix);
            btAfegeix.setTooltip ("Afegeix un mòdul al final del rack"_ca);
            btAfegeix.onClick = [this] { menuAfegeix(); };
            setSize (900, 700);
            if (nomsPreset()[rk.get()].isEmpty() && ! presetsFabrica().empty())
            {
                Rack inicial;
                inicial.posaEstat (estatInicial());
                if (inicial.estat() == rk->estat()) nomsPreset()[rk.get()] = u8 (presetsFabrica().front().nom);
            }
            refesFiles();
            refresca();
            startTimerHz (25);
        }
        ~PanellVocalStudio() override { stopTimer(); finestresModuls.clear(); }

        // ---- per a les files ----
        void obreModul (int i)
        {
            auto* m = rk->modul (i);
            if (m == nullptr) return;
            if (auto f = finestresModuls.find (m); f != finestresModuls.end()) { f->second->toFront (true); return; }
            const auto t = titol + " · "_ca + nomDe (*m);
            auto tanca = [this, m] { finestresModuls.erase (m); };
            std::unique_ptr<juce::DocumentWindow> w;
            auto fx = m->fx;
            if (auto q = std::dynamic_pointer_cast<mibase::eq::Equalitzador> (fx)) w = creaFinestraEq (q, t, tanca);
            else if (auto d = std::dynamic_pointer_cast<mibase::dinamica::Dinamica> (fx)) w = creaFinestraDinamica (d, t, tanca);
            else if (auto s = std::dynamic_pointer_cast<mibase::saturacio::Saturacio> (fx)) w = creaFinestraSaturacio (s, t, tanca);
            else if (auto dv = std::dynamic_pointer_cast<mibase::drive::Drive> (fx)) w = creaFinestraDrive (dv, t, tanca);
            else if (auto dl = std::dynamic_pointer_cast<mibase::delay::Delay> (fx)) w = creaFinestraDelay (dl, t, tanca);
            else if (auto rv = std::dynamic_pointer_cast<mibase::reverb::Reverb> (fx)) w = creaFinestraReverb (rv, t, tanca);
            else if (auto lm = std::dynamic_pointer_cast<mibase::limitador::Limitador> (fx)) w = creaFinestraLimitador (lm, t, tanca);
            else w = std::make_unique<FinestraSimple> (t, new PanellParametres (fx), tanca);
            if (w == nullptr) return;
            quedaSobreMibase (*w);
            finestresModuls[m] = std::move (w);
        }
        void treuModul (int i)
        {
            if (auto* m = rk->modul (i)) finestresModuls.erase (m);
            rk->treu (i);
            haCanviat();
        }
        void mouModul (int i, int d) { rk->mou (i, i + d); haCanviat(); }
        void haCanviat() { nomsPreset()[rk.get()] = {}; }   // ja no és el preset tal qual

        void refresca()
        {
            const auto nom = nomsPreset()[rk.get()];
            btPreset.setButtonText (nom.isEmpty() ? "(sense preset)"_ca : nom);
            btFav.setToggleState (esFavorit (nom), juce::dontSendNotification);
            pomIn->setValor (rk->parametre (Entrada), true);
            pomOut->setValor (rk->parametre (Sortida), true);
            for (int m = 0; m < numMacros; ++m)
            {
                auto& mc = macros[(size_t) m];
                int n = 0;
                for (const auto& e : rk->enllacos()) if (e.macro == m) ++n;
                mc.pom->setEnabled (n > 0);
                mc.pom->setValor (rk->macro (m), true);
                mc.pom->setTooltip (n > 0 ? "M"_ca + juce::String (m + 1) + " · "_ca + u8 (rk->nomMacro (m)) + " · mou "_ca + juce::String (n) + (n == 1 ? " paràmetre"_ca : " paràmetres"_ca)
                                          : "Aquest macro encara no mou res: clica el nom per enllaçar-hi paràmetres"_ca);
                mc.nom.setButtonText (u8 (rk->nomMacro (m)));
            }
            for (auto& f : files) f->refresca();
            btAfegeix.setEnabled (rk->numModuls() < maxModuls);
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (tema::fons);
            auto cap = getLocalBounds().removeFromTop (60).toFloat();
            vintage::panell (g, cap);
            auto t = cap.reduced (14.0f, 9.0f).removeFromLeft (200.0f);
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (19.0f, true));
            g.drawText ("VOCAL STUDIO", t.removeFromTop (24.0f), juce::Justification::centredLeft);
            g.setColour (turquesa);
            g.setFont (dibuix::lletra (10.0f, true));
            g.drawText ("MIBASE · RACK DE VEU"_ca, t, juce::Justification::centredLeft);
            // vúmetres d'entrada i sortida
            auto& ms = rk->mesures();
            auto nivell = [] (float v) { return v > 1e-5f ? juce::jlimit (0.0f, 1.0f, (20.0f * std::log10 (v) + 48.0f) / 48.0f) : 0.0f; };
            for (auto [z, v] : { std::pair<juce::Rectangle<int>, float> { zonaVuIn, vuIn }, { zonaVuOut, vuOut } })
            {
                auto b = z.toFloat();
                g.setColour (juce::Colours::black.withAlpha (0.55f));
                g.fillRoundedRectangle (b, 2.0f);
                const float a = nivell (v);
                g.setColour (v > 0.98f ? vintage::ledVermell : turquesa);
                g.fillRoundedRectangle (b.withTop (b.getBottom() - b.getHeight() * a), 2.0f);
            }
            juce::ignoreUnused (ms);
            // títols de les zones
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (10.5f, true));
            g.drawText ("MACROS", zonaMacros.withHeight (14).translated (0, -16), juce::Justification::centredLeft);
            g.drawText ("RACK · "_ca + juce::String (rk->numModuls()) + " / " + juce::String (maxModuls) + " MÒDULS"_ca, zonaRack.withHeight (14).translated (0, -16), juce::Justification::centredLeft);
            // macros: fons
            vintage::panell (g, zonaMacros.toFloat().expanded (6.0f, 4.0f), 6.0f);
            // a baix: latència
            const int lat = rk->latencia();
            juce::String txt = lat > 0 ? "Retard del rack: "_ca + juce::String (lat * 1000.0 / 48000.0, 1).replaceCharacter ('.', ',') + " ms (Mibase el compensa). "_ca : juce::String();
            txt << "Doble clic a un mòdul o «Obre» per veure'l sencer."_ca;
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (11.0f));
            g.drawFittedText (txt, zonaText, juce::Justification::centredLeft, 1, 0.85f);
            if (rk->numModuls() == 0)
            {
                g.setFont (dibuix::lletra (13.0f));
                g.drawText ("El rack és buit: afegeix-hi un mòdul o tria un preset."_ca, zonaRack, juce::Justification::centred);
            }
        }

        void resized() override
        {
            auto r = getLocalBounds();
            auto dalt = r.removeFromTop (60).reduced (14, 6);
            // dreta de la capçalera: entrada, vúmetres, sortida
            pomOut->setBounds (dalt.removeFromRight (70));
            zonaVuOut = dalt.removeFromRight (10).reduced (0, 6);
            dalt.removeFromRight (8);
            zonaVuIn = dalt.removeFromRight (10).reduced (0, 6);
            pomIn->setBounds (dalt.removeFromRight (70));
            {
                auto pr = r.removeFromTop (38).reduced (14, 4);
                btAnt.setBounds (pr.removeFromLeft (28));
                btPreset.setBounds (pr.removeFromLeft (juce::jmin (320, pr.getWidth() - 160)));
                btSeg.setBounds (pr.removeFromLeft (28));
                pr.removeFromLeft (6);
                btFav.setBounds (pr.removeFromLeft (26));
                pr.removeFromLeft (6);
                btDesa.setBounds (pr.removeFromLeft (80));
            }
            if (navegador != nullptr) navegador->setBounds (getLocalBounds());
            r.reduce (14, 0);
            r.removeFromTop (22);
            zonaMacros = r.removeFromTop (118);
            {
                auto z = zonaMacros;
                const int a = z.getWidth() / numMacros;
                for (int m = 0; m < numMacros; ++m)
                {
                    auto c = z.removeFromLeft (a).reduced (4, 0);
                    macros[(size_t) m].nom.setBounds (c.removeFromBottom (26));
                    macros[(size_t) m].pom->setBounds (c);
                }
            }
            r.removeFromTop (26);
            zonaText = r.removeFromBottom (24);
            btAfegeix.setBounds (r.removeFromBottom (34).removeFromLeft (220));
            r.removeFromBottom (8);
            zonaRack = r;
            vista.setBounds (r);
            posaFiles();
        }

        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }

    private:
        void timerCallback() override
        {
            rk->recull();
            if (rk->versio() != versioVista) { refesFiles(); refresca(); }
            const auto ara = rk->estat();
            if (ara != vist) { vist = ara; refresca(); }
            // vúmetres (pic que cau a poc a poc)
            auto& ms = rk->mesures();
            vuIn = juce::jmax (ms.entrada.exchange (0.0f), vuIn * 0.82f);
            vuOut = juce::jmax (ms.sortida.exchange (0.0f), vuOut * 0.82f);
            repaint (zonaVuIn.expanded (2));
            repaint (zonaVuOut.expanded (2));
            // les finestres dels mòduls que ja no hi són
            for (auto it = finestresModuls.begin(); it != finestresModuls.end();)
            {
                bool hiEs = false;
                for (int i = 0; i < rk->numModuls(); ++i) if (rk->modul (i) == it->first) hiEs = true;
                it = hiEs ? std::next (it) : finestresModuls.erase (it);
            }
        }

        void refesFiles()
        {
            versioVista = rk->versio();
            files.clear();
            for (int i = 0; i < rk->numModuls(); ++i)
            {
                files.push_back (std::make_unique<FilaModul> (*this, rk, i));
                contingut.addAndMakeVisible (*files.back());
            }
            posaFiles();
        }
        void posaFiles()
        {
            constexpr int alt = 64;
            const int w = juce::jmax (100, vista.getWidth() - (rk->numModuls() * alt > vista.getHeight() ? vista.getScrollBarThickness() : 0));
            contingut.setSize (w, juce::jmax (1, (int) files.size() * alt));
            for (int i = 0; i < (int) files.size(); ++i) files[(size_t) i]->setBounds (0, i * alt, w, alt - 4);
        }

        void menuAfegeix()
        {
            juce::PopupMenu m;
            int id = 1;
            for (const auto& t : tipus())
                m.addItem (id++, u8 (t.nom) + "  ·  "_ca + u8 (t.descripcio));
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (btAfegeix), [sp = juce::Component::SafePointer<PanellVocalStudio> (this)] (int r)
            {
                if (sp == nullptr || r <= 0 || r > (int) tipus().size()) return;
                const int on = sp->rk->afegeix (tipus()[(size_t) r - 1].clau);
                if (on < 0) return;
                sp->haCanviat();
                sp->refesFiles();
                sp->refresca();
                sp->vista.setViewPosition (0, sp->contingut.getHeight());
            });
        }

        void menuMacro (int m)
        {
            juce::PopupMenu menu;
            menu.addSectionHeader ("M"_ca + juce::String (m + 1) + " · "_ca + u8 (rk->nomMacro (m)));
            menu.addItem (1, "Canvia el nom…"_ca);
            menu.addItem (2, "Rang dels enllaços…"_ca);
            // enllaçar: mòdul → paràmetre
            juce::PopupMenu enllaca;
            std::vector<std::pair<int, int>> opcions;   // (mòdul, paràmetre)
            for (int i = 0; i < rk->numModuls(); ++i)
            {
                const auto& mod = *rk->modul (i);
                juce::PopupMenu sub;
                for (int p : paramsOferts (mod))
                {
                    bool ja = false;
                    for (const auto& e : rk->enllacos()) if (e.macro == m && e.modul == i && e.param == p) ja = true;
                    opcions.push_back ({ i, p });
                    sub.addItem (1000 + (int) opcions.size() - 1, nomParam (mod, p), ! ja, ja);
                }
                enllaca.addSubMenu (juce::String (i + 1) + ". " + nomDe (mod), sub);
            }
            menu.addSubMenu ("Enllaça-hi un paràmetre"_ca, enllaca, rk->numModuls() > 0);
            bool te = false;
            for (const auto& e : rk->enllacos()) if (e.macro == m) te = true;
            menu.addItem (3, "Treu tots els enllaços"_ca, te);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (macros[(size_t) m].nom),
                [sp = juce::Component::SafePointer<PanellVocalStudio> (this), m, opcions] (int r)
            {
                if (sp == nullptr || r <= 0) return;
                if (r == 1) sp->canviaNomMacro (m);
                else if (r == 2) sp->obreEnllacos (m);
                else if (r == 3)
                {
                    for (int k = (int) sp->rk->enllacos().size() - 1; k >= 0; --k)
                        if (sp->rk->enllacos()[(size_t) k].macro == m) sp->rk->treuEnllac (k);
                    sp->haCanviat();
                    sp->refresca();
                }
                else if (r >= 1000 && r - 1000 < (int) opcions.size())
                {
                    const auto [i, p] = opcions[(size_t) (r - 1000)];
                    auto* mod = sp->rk->modul (i);
                    if (mod == nullptr) return;
                    // el rang comença on és el paràmetre ara; amb el macro a 10, mig recorregut més enllà
                    const float ara = mod->fx->parametre (p);
                    const float fi = ara < 0.5f ? juce::jmin (1.0f, ara + 0.5f) : juce::jmax (0.0f, ara - 0.5f);
                    // perquè no salti, el macro es queda on és i el rang es calcula al seu voltant
                    // (mn + (fi - mn) · v = ara)
                    const float v = sp->rk->macro (m);
                    const float mn = v < 0.98f ? juce::jlimit (0.0f, 1.0f, (ara - fi * v) / (1.0f - v)) : ara;
                    sp->rk->enllaca ({ m, i, p, mn, fi });
                    sp->haCanviat();
                    sp->refresca();
                    sp->obreEnllacos (m);
                }
            });
        }

        void canviaNomMacro (int m)
        {
            dialeg = std::make_unique<DialegText> ("Nom del macro"_ca, "Un nom curt (per exemple: Aire, Cos, Espai)."_ca, "D'acord"_ca,
                [] (const juce::String& t) { return t.trim().isEmpty() ? "Escriu un nom."_ca : juce::String(); },
                [this, m] (const juce::String& t) { rk->posaNomMacro (m, t.trim().substring (0, 18).toStdString()); haCanviat(); refresca(); },
                u8 (rk->nomMacro (m)));
            DialegText::mostra (*this, dialeg);
        }

        void obreEnllacos (int m)
        {
            auto ed = std::make_unique<EditorEnllacos> (rk, m, [sp = juce::Component::SafePointer<PanellVocalStudio> (this)] { if (sp != nullptr) { sp->haCanviat(); sp->refresca(); } });
            juce::CallOutBox::launchAsynchronously (std::move (ed), macros[(size_t) m].nom.getScreenBounds(), nullptr);
        }

        // ---- presets (OrigenPresets), com al Limitador ----
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
            const auto nom = nomsPreset()[rk.get()].toStdString();
            if (nom.empty()) { btFav.setToggleState (false, juce::dontSendNotification); return; }
            biblioteca().commutaFavorit (biblioteca().busca (nom) != nullptr ? BibliotecaPresets::clauUsuari (nom) : BibliotecaPresets::clauFabrica (nom));
            desaBiblioteca();
            refresca();
        }
        void aplicaPreset (const Preset& p)
        {
            finestresModuls.clear();   // els mòduls d'abans se'n van
            rk->posaEstat (p.estat);
            nomsPreset()[rk.get()] = u8 (p.nom);
            refesFiles();
            refresca();
        }
        void pas (int d)
        {
            const auto v = totsPresets();
            if (v.empty()) return;
            const auto nom = nomsPreset()[rk.get()].toStdString();
            int i = -1;
            for (int k = 0; k < (int) v.size(); ++k) if (v[(size_t) k]->nom == nom) i = k;
            i = i < 0 ? (d > 0 ? 0 : (int) v.size() - 1) : (i + d + (int) v.size()) % (int) v.size();
            aplicaPreset (*v[(size_t) i]);
        }
        void desaPreset()
        {
            const auto actual = nomsPreset()[rk.get()];
            const bool meu = biblioteca().busca (actual.toStdString()) != nullptr;
            const auto proposta = meu ? actual : u8 (biblioteca().nomLliure ((actual.isEmpty() ? juce::String ("La meva veu") : actual + " (meu)"_ca).toStdString()));
            dialeg = std::make_unique<DialegText> ("Desa el preset"_ca, "Nom del preset (si ja en tens un amb aquest nom, se substitueix)."_ca, "Desa"_ca,
                [] (const juce::String& t) { return t.trim().isEmpty() ? "Escriu un nom."_ca : juce::String(); },
                [this] (const juce::String& t)
                {
                    const auto nom = t.trim().toStdString();
                    const auto* ja = biblioteca().busca (nom);
                    biblioteca().desaPreset ({ nom, ja != nullptr ? ja->categoria : std::string ("Meus"), "", rk->estat() });
                    desaBiblioteca();
                    nomsPreset()[rk.get()] = u8 (nom);
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
                // l'etiqueta: quants mòduls té (sense crear-los: es compten al text)
                int n = 0;
                for (size_t a = p.estat.find ("\"clau\""); a != std::string::npos; a = p.estat.find ("\"clau\"", a + 1)) ++n;
                const bool usuari = i < nu;
                v.push_back ({ u8 (p.nom), u8 (p.categoria), u8 (p.descripcio), juce::String (n) + (n == 1 ? " mòdul"_ca : " mòduls"_ca), usuari,
                               biblioteca().esFavorit (usuari ? BibliotecaPresets::clauUsuari (p.nom) : BibliotecaPresets::clauFabrica (p.nom)), i });
            }
            return v;
        }
        juce::String presetActual() override { return nomsPreset()[rk.get()]; }
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
                if (nomsPreset()[rk.get()] == vell) nomsPreset()[rk.get()] = nou;
            }
            refresca();
        }
        void mou (const juce::String& nom, const juce::String& carpeta) override { biblioteca().mou (nom.toStdString(), carpeta.toStdString()); desaBiblioteca(); }
        void esborra (const juce::String& nom) override { biblioteca().esborra (nom.toStdString()); desaBiblioteca(); refresca(); }
        void tancaNavegador() override
        {
            juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PanellVocalStudio> (this)] { if (sp != nullptr) { sp->navegador.reset(); sp->grabKeyboardFocus(); } });
        }

        std::shared_ptr<Rack> rk;
        juce::String titol;
        std::unique_ptr<NavegadorPresetsComu> navegador;
        std::unique_ptr<DialegText> dialeg;
        Boto btAnt { {}, dibuix::Icona::Rebobina, Boto::Estil::Subtil }, btSeg { {}, dibuix::Icona::Endavant, Boto::Estil::Subtil };
        Boto btPreset { "(sense preset)"_ca, std::nullopt, Boto::Estil::Secundari };
        BotoEstrella btFav;
        Boto btDesa { "Desa", dibuix::Icona::Disc, Boto::Estil::Secundari };
        std::unique_ptr<Pom> pomIn, pomOut;
        struct Macro { std::unique_ptr<Pom> pom; Boto nom { {}, std::nullopt, Boto::Estil::Subtil }; };
        std::array<Macro, numMacros> macros;
        juce::Viewport vista;
        juce::Component contingut;
        std::vector<std::unique_ptr<FilaModul>> files;
        Boto btAfegeix { "Afegeix un mòdul"_ca, dibuix::Icona::Mes, Boto::Estil::Discontinu };
        std::map<const Modul*, std::unique_ptr<juce::DocumentWindow>> finestresModuls;
        juce::Rectangle<int> zonaMacros, zonaRack, zonaText, zonaVuIn, zonaVuOut;
        float vuIn = 0.0f, vuOut = 0.0f;
        unsigned versioVista = 0;
        std::string vist;
        juce::TooltipWindow tips { this, 500 };
    };

    // ---- FilaModul ----
    FilaModul::FilaModul (PanellVocalStudio& p, std::shared_ptr<Rack> r, int i) : index (i), pare (p), rk (std::move (r))
    {
        for (auto* b : { &btOn, &btObre, &btTreu }) addAndMakeVisible (b);
        addAndMakeVisible (btAmunt);
        addAndMakeVisible (btAvall);
        btOn.setLlum (turquesa);
        btOn.setTooltip ("Encès / apagat (apagat no sona ni gasta)"_ca);
        btObre.setTooltip ("Obre la finestra del mòdul"_ca);
        btTreu.setTooltip ("Treu el mòdul del rack"_ca);
        btAmunt.setTooltip ("Puja'l (sona abans)"_ca);
        btAvall.setTooltip ("Baixa'l (sona després)"_ca);
        btOn.onClick = [this] { if (auto* m = rk->modul (index)) { rk->posaOn (index, ! m->on.load()); pare.haCanviat(); refresca(); } };
        btObre.onClick = [this] { pare.obreModul (index); };
        btTreu.onClick = [this] { const int i2 = index; juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PanellVocalStudio> (&pare), i2] { if (sp != nullptr) sp->treuModul (i2); }); };
        btAmunt.onClick = [this] { const int i2 = index; juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PanellVocalStudio> (&pare), i2] { if (sp != nullptr) sp->mouModul (i2, -1); }); };
        btAvall.onClick = [this] { const int i2 = index; juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PanellVocalStudio> (&pare), i2] { if (sp != nullptr) sp->mouModul (i2, 1); }); };
        pomMix = std::make_unique<Pom> ("Mix", 0.0, 1.0, 1.0, false, [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + " %"; });
        pomMix->setTooltip ("Quant se sent el mòdul: 100 % = tot; menys = en paral·lel amb el que li arriba"_ca);
        pomMix->onCanvi = [this] (double v) { rk->posaMix (index, (float) v); pare.haCanviat(); };
        pomMix->onReinicia = [this] { rk->posaMix (index, 1.0f); refresca(); };
        addAndMakeVisible (*pomMix);
        refresca();
    }

    void FilaModul::refresca()
    {
        auto* m = rk->modul (index);
        if (m == nullptr) return;
        btOn.setToggleState (m->on.load(), juce::dontSendNotification);
        const bool ambLatencia = m->fx->latencia() > 0;
        pomMix->setEnabled (! ambLatencia);
        pomMix->setValor (ambLatencia ? 1.0 : (double) m->mix.load(), true);
        btAmunt.setEnabled (index > 0);
        btAvall.setEnabled (index < rk->numModuls() - 1);
        repaint();
    }

    void FilaModul::paint (juce::Graphics& g)
    {
        auto* m = rk->modul (index);
        if (m == nullptr) return;
        const bool on = m->on.load();
        auto r = getLocalBounds().toFloat();
        vintage::panell (g, r, 5.0f);
        // cargols del rack
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        for (float y : { r.getY() + 9.0f, r.getBottom() - 13.0f })
        {
            g.fillEllipse (r.getX() + 6.0f, y, 5.0f, 5.0f);
            g.fillEllipse (r.getRight() - 11.0f, y, 5.0f, 5.0f);
        }
        // franja de color
        g.setColour ((on ? turquesa : tema::suau).withAlpha (on ? 0.9f : 0.35f));
        g.fillRect (r.getX() + 16.0f, r.getY() + 8.0f, 3.0f, r.getHeight() - 16.0f);
        auto z = zonaNom.toFloat();
        g.setColour (on ? tema::text : tema::suau);
        g.setFont (dibuix::lletra (14.5f, true));
        g.drawText (juce::String (index + 1) + "  " + dibuix::majuscules (nomDe (*m)), z.removeFromTop (z.getHeight() * 0.5f), juce::Justification::bottomLeft);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.0f));
        juce::String desc;
        if (auto* t = busca (m->clau)) desc = u8 (t->descripcio);
        if (m->fx->latencia() > 0) desc = "Sempre al 100 % (retarda 5 ms, Mibase ho compensa)."_ca;
        g.drawFittedText (desc, z.toNearestInt(), juce::Justification::topLeft, 1, 0.85f);
    }

    void FilaModul::resized()
    {
        auto r = getLocalBounds().reduced (24, 6);
        btOn.setBounds (r.removeFromLeft (36).withSizeKeepingCentre (34, 30));
        r.removeFromLeft (10);
        btTreu.setBounds (r.removeFromRight (30).withSizeKeepingCentre (28, 28));
        r.removeFromRight (4);
        auto fl = r.removeFromRight (22);
        btAmunt.setBounds (fl.removeFromTop (fl.getHeight() / 2));
        btAvall.setBounds (fl);
        r.removeFromRight (6);
        btObre.setBounds (r.removeFromRight (84).withSizeKeepingCentre (84, 30));
        r.removeFromRight (10);
        pomMix->setBounds (r.removeFromRight (62).withTrimmedTop (-4).withTrimmedBottom (-4));
        r.removeFromRight (10);
        zonaNom = r;
    }

    void FilaModul::mouseDoubleClick (const juce::MouseEvent&) { pare.obreModul (index); }

    class FinestraVocalStudio : public juce::DocumentWindow
    {
    public:
        FinestraVocalStudio (std::shared_ptr<Rack> r, const juce::String& titol, std::function<void()> enTancar)
            : juce::DocumentWindow (titol, tema::fons, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
              tancar (std::move (enTancar))
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new PanellVocalStudio (std::move (r), titol), true);
            setResizable (true, true);
            setResizeLimits (760, 560, 1600, 1200);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::MessageManager::callAsync ([t = tancar] { t(); }); }
        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }
    private:
        std::function<void()> tancar;
    };
}

std::unique_ptr<juce::DocumentWindow> creaFinestraVocalStudio (std::shared_ptr<Rack> r, const juce::String& titol, std::function<void()> enTancar)
{
    return std::make_unique<FinestraVocalStudio> (std::move (r), titol, std::move (enTancar));
}
