#include "FinestraDrive.h"

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

using namespace mibase::drive;

namespace
{
    juce::String u8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str(), (int) s.size()); }
    juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

    // Presets propis i favorits (Documents\Mibase\Drive\presets.json)
    juce::File fitxerBiblioteca()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Mibase").getChildFile ("Drive").getChildFile ("presets.json");
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

    /** El color de cada pedal. */
    juce::Colour colorMode (int m)
    {
        static const juce::uint32 c[numModes] = { 0xffc98a2b, 0xff3e8c52, 0xffc04a26, 0xff7650a3 };
        return juce::Colour (c[juce::jlimit (0, numModes - 1, m)]);
    }

    class PanellDrive : public juce::Component, private juce::Timer, public OrigenPresets
    {
    public:
        explicit PanellDrive (std::shared_ptr<Drive> d) : drv (std::move (d))
        {
            for (auto* b : { &btAnt, &btSeg, &btPreset, &btDesa }) addAndMakeVisible (b);
            addAndMakeVisible (btFav);
            btAnt.setTooltip ("Preset anterior"_ca);
            btSeg.setTooltip ("Preset següent"_ca);
            btPreset.setTooltip ("Clica per veure tots els presets (crunch, overdrive, distorsió, fuzz, baix…)"_ca);
            btFav.setTooltip ("Preferit"_ca);
            btDesa.setTooltip ("Desa els ajustos com un preset teu"_ca);
            btAnt.onClick = [this] { pas (-1); };
            btSeg.onClick = [this] { pas (1); };
            btPreset.onClick = [this] { obreNavegador(); };
            btFav.onClick = [this] { commutaFavoritActual(); };
            btDesa.onClick = [this] { desaPreset(); };
            addAndMakeVisible (segMode);
            segMode.setTooltip (u8 (info (ModeP).ajuda));
            segMode.onCanvi = [this] (int m) { drv->posa (ModeP, (float) m); refresca(); };
            addAndMakeVisible (btTight);
            btTight.setLlum (tema::accent);
            btTight.setTooltip (u8 (info (Tight).ajuda));
            btTight.onClick = [this] { drv->posa (Tight, drv->valor (Tight) > 0.5f ? 0.0f : 1.0f); refresca(); };
            for (int p : { (int) Gain, (int) Tone, (int) Level, (int) Bass, (int) Mix, (int) Gate })
            {
                auto pom = std::make_unique<Pom> (u8 (info (p).nom), 0.0, 1.0, (double) Drive::normal (p, info (p).def), p == Bass,
                                                  [this, p] (double v) { return u8 (drv->textParametre (p, (float) v)); });
                pom->setTooltip (u8 (info (p).ajuda));
                pom->onCanvi = [this, p] (double v) { drv->posaParametre (p, (float) v); };
                pom->onReinicia = [this, p] { drv->posa (p, info (p).def); refresca(); };
                addAndMakeVisible (*pom);
                poms.push_back ({ p, std::move (pom) });
            }
            setSize (780, 520);
            refresca();
            startTimerHz (20);
        }
        ~PanellDrive() override { stopTimer(); }

        void refresca()
        {
            const auto nom = nomsPreset()[drv.get()];
            btPreset.setButtonText (nom.isEmpty() ? "(sense preset)"_ca : nom);
            btFav.setToggleState (esFavorit (nom), juce::dontSendNotification);
            segMode.setTriat ((int) drv->valor (ModeP));
            btTight.setToggleState (drv->valor (Tight) > 0.5f, juce::dontSendNotification);
            for (auto& [p, pom] : poms) pom->setValor (drv->parametre (p), true);
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (tema::fons);
            vintage::panell (g, getLocalBounds().removeFromTop (56).toFloat());
            auto t = getLocalBounds().removeFromTop (56).reduced (14, 9).removeFromLeft (100).toFloat();
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (17.0f, true));
            g.drawText ("DRIVE", t.removeFromTop (22.0f), juce::Justification::centredLeft);
            g.setColour (tema::accent);
            g.setFont (dibuix::lletra (10.0f, true));
            g.drawText ("MIBASE", t, juce::Justification::centredLeft);

            // el pedal: caixa de color, cargols, el nom i el llum
            const int m = (int) drv->valor (ModeP);
            const auto col = colorMode (m);
            auto p = pedal.toFloat();
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillRoundedRectangle (p.translated (0.0f, 4.0f), 14.0f);
            g.setGradientFill (juce::ColourGradient (col.brighter (0.25f), p.getX(), p.getY(), col.darker (0.45f), p.getX(), p.getBottom(), false));
            g.fillRoundedRectangle (p, 14.0f);
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.drawRoundedRectangle (p.reduced (1.0f), 14.0f, 1.5f);
            g.setColour (juce::Colours::black.withAlpha (0.3f));
            for (auto c : { p.getTopLeft().translated (12, 12), p.getTopRight().translated (-12, 12), p.getBottomLeft().translated (12, -12), p.getBottomRight().translated (-12, -12) })
                g.fillEllipse (c.x - 4.0f, c.y - 4.0f, 8.0f, 8.0f);
            // zona fosca dels poms
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (zonaPoms.toFloat().expanded (6.0f), 10.0f);
            // llum: encès quan passa so
            const float niv = drv->mesures().sortida.load();
            const float llum = juce::jlimit (0.0f, 1.0f, niv > 1e-4f ? (20.0f * std::log10 (niv) + 60.0f) / 40.0f : 0.0f);
            const auto led = juce::Rectangle<float> (14.0f, 14.0f).withCentre (zonaLed.toFloat().getCentre());
            g.setColour (juce::Colour (0xffff3b2f).withAlpha (0.25f + 0.75f * llum));
            g.fillEllipse (led);
            if (llum > 0.05f) { g.setColour (juce::Colour (0xffff3b2f).withAlpha (0.25f * llum)); g.fillEllipse (led.expanded (6.0f)); }
            // nom del pedal
            g.setColour (juce::Colours::white.withAlpha (0.92f));
            g.setFont (dibuix::lletra (30.0f, true));
            g.drawText (dibuix::majuscules (u8 (nomMode (m))), zonaNom, juce::Justification::centred);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.setFont (dibuix::lletra (11.0f, true));
            g.drawText ("MIBASE DRIVE", zonaNom.translated (0, zonaNom.getHeight() - 4).withHeight (16), juce::Justification::centred);
            // text de baix
            juce::String txt = u8 (descripcioMode (m));
            if (drv->valor (Gate) > -79.5f && drv->mesures().gate.load() < 0.5f) txt = "Gate tancat: no passa el brunzit."_ca;
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
            segMode.setBounds (dalt);
            r.reduce (14, 10);
            zonaText = r.removeFromBottom (30);
            // a la dreta: Bass, Mix, Gate i Tight
            auto dreta = r.removeFromRight (juce::jmax (150, r.getWidth() / 4));
            btTight.setBounds (dreta.removeFromBottom (32).reduced (10, 0));
            dreta.removeFromBottom (6);
            const int h3 = dreta.getHeight() / 3;
            int k = 0;
            for (auto& [p, pom] : poms)
                if (p == Bass || p == Mix || p == Gate) pom->setBounds (dreta.getX(), dreta.getY() + (k++) * h3, dreta.getWidth(), h3 - 4);
            r.removeFromRight (12);
            // el pedal, al mig
            const int ap = juce::jmin (r.getWidth(), (int) (r.getHeight() * 0.85f));
            pedal = r.withSizeKeepingCentre (ap, r.getHeight());
            auto in = pedal.reduced (24, 22);
            zonaPoms = in.removeFromTop (juce::jmax (110, in.getHeight() * 42 / 100));
            const int a = zonaPoms.getWidth() / 3;
            int i = 0;
            for (auto& [p, pom] : poms)
                if (p == Gain || p == Tone || p == Level) pom->setBounds (zonaPoms.getX() + (i++) * a, zonaPoms.getY(), a, zonaPoms.getHeight());
            in.removeFromTop (16);
            zonaLed = in.removeFromTop (20);
            in.removeFromTop (6);
            zonaNom = in.removeFromTop (44);
        }

        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }

    private:
        void timerCallback() override
        {
            repaint (zonaLed.expanded (12));
            repaint (zonaText);
            const auto ara = drv->estat();
            if (ara != vist) { vist = ara; refresca(); }
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
            const auto nom = nomsPreset()[drv.get()].toStdString();
            if (nom.empty()) { btFav.setToggleState (false, juce::dontSendNotification); return; }
            biblioteca().commutaFavorit (biblioteca().busca (nom) != nullptr ? BibliotecaPresets::clauUsuari (nom) : BibliotecaPresets::clauFabrica (nom));
            desaBiblioteca();
            refresca();
        }
        void aplicaPreset (const Preset& p)
        {
            drv->posaEstat (p.estat);
            nomsPreset()[drv.get()] = u8 (p.nom);
            refresca();
        }
        void pas (int d)
        {
            const auto v = totsPresets();
            if (v.empty()) return;
            const auto nom = nomsPreset()[drv.get()].toStdString();
            int i = -1;
            for (int k = 0; k < (int) v.size(); ++k) if (v[(size_t) k]->nom == nom) i = k;
            i = i < 0 ? (d > 0 ? 0 : (int) v.size() - 1) : (i + d + (int) v.size()) % (int) v.size();
            aplicaPreset (*v[(size_t) i]);
        }
        void desaPreset()
        {
            const auto actual = nomsPreset()[drv.get()];
            const bool meu = biblioteca().busca (actual.toStdString()) != nullptr;
            const auto proposta = meu ? actual : u8 (biblioteca().nomLliure ((actual.isEmpty() ? juce::String ("Drive"_ca) : actual + " (meu)"_ca).toStdString()));
            dialeg = std::make_unique<DialegText> ("Desa el preset"_ca, "Nom del preset (si ja en tens un amb aquest nom, se substitueix)."_ca, "Desa"_ca,
                [] (const juce::String& t) { return t.trim().isEmpty() ? "Escriu un nom."_ca : juce::String(); },
                [this] (const juce::String& t)
                {
                    const auto nom = t.trim().toStdString();
                    const auto* ja = biblioteca().busca (nom);
                    biblioteca().desaPreset ({ nom, ja != nullptr ? ja->categoria : std::string ("Meus"), "", drv->estat() });
                    desaBiblioteca();
                    nomsPreset()[drv.get()] = u8 (nom);
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
                Drive d;
                d.posaEstat (p.estat);
                const bool usuari = i < nu;
                v.push_back ({ u8 (p.nom), u8 (p.categoria), u8 (p.descripcio), u8 (nomMode ((int) d.valor (ModeP))), usuari,
                               biblioteca().esFavorit (usuari ? BibliotecaPresets::clauUsuari (p.nom) : BibliotecaPresets::clauFabrica (p.nom)), i });
            }
            return v;
        }
        juce::String presetActual() override { return nomsPreset()[drv.get()]; }
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
                if (nomsPreset()[drv.get()] == vell) nomsPreset()[drv.get()] = nou;
            }
            refresca();
        }
        void mou (const juce::String& nom, const juce::String& carpeta) override { biblioteca().mou (nom.toStdString(), carpeta.toStdString()); desaBiblioteca(); }
        void esborra (const juce::String& nom) override { biblioteca().esborra (nom.toStdString()); desaBiblioteca(); refresca(); }
        void tancaNavegador() override
        {
            juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PanellDrive> (this)] { if (sp != nullptr) { sp->navegador.reset(); sp->grabKeyboardFocus(); } });
        }

        std::unique_ptr<NavegadorPresetsComu> navegador;
        std::unique_ptr<DialegText> dialeg;
        Boto btAnt { {}, dibuix::Icona::Rebobina, Boto::Estil::Subtil }, btSeg { {}, dibuix::Icona::Endavant, Boto::Estil::Subtil };
        Boto btPreset { "(sense preset)"_ca, std::nullopt, Boto::Estil::Secundari };
        BotoEstrella btFav;
        Boto btDesa { "Desa", dibuix::Icona::Disc, Boto::Estil::Secundari };
        std::shared_ptr<Drive> drv;
        Segmentat segMode { { "Crunch", "Overdrive", "Distorsió"_ca, "Fuzz" } };
        Boto btTight { "Tight", std::nullopt, Boto::Estil::Tecla };
        std::vector<std::pair<int, std::unique_ptr<Pom>>> poms;
        juce::Rectangle<int> pedal, zonaPoms, zonaLed, zonaNom, zonaText;
        std::string vist;
        juce::TooltipWindow tips { this, 500 };
    };

    class FinestraDrive : public juce::DocumentWindow
    {
    public:
        FinestraDrive (std::shared_ptr<Drive> d, const juce::String& titol, std::function<void()> enTancar)
            : juce::DocumentWindow (titol, tema::fons, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
              tancar (std::move (enTancar))
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new PanellDrive (std::move (d)), true);
            setResizable (true, true);
            setResizeLimits (640, 440, 1400, 1000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::MessageManager::callAsync ([t = tancar] { t(); }); }
        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }
    private:
        std::function<void()> tancar;
    };
}

std::unique_ptr<juce::DocumentWindow> creaFinestraDrive (std::shared_ptr<Drive> d, const juce::String& titol, std::function<void()> enTancar)
{
    return std::make_unique<FinestraDrive> (std::move (d), titol, std::move (enTancar));
}
