#include "FinestraLimitador.h"

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

using namespace mibase::limitador;

namespace
{
    juce::String u8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str(), (int) s.size()); }
    juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }
    juce::String coma (double x, int d) { return juce::String (x, d).replaceCharacter ('.', ','); }

    // Presets propis i favorits (Documents\Mibase\Limitador\presets.json)
    juce::File fitxerBiblioteca()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Mibase").getChildFile ("Limitador").getChildFile ("presets.json");
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

    /** La reducció de guany dels últims 12 s (avall = més limitació). */
    class Grafic : public juce::Component
    {
    public:
        explicit Grafic (Limitador& l) : lm (l) {}
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRoundedRectangle (r, 5.0f);
            r.reduce (10.0f, 8.0f);
            const float maxDb = 12.0f;
            g.setFont (dibuix::lletra (9.5f));
            for (int db : { 3, 6, 9, 12 })
            {
                const float y = r.getY() + r.getHeight() * (float) db / maxDb;
                g.setColour (juce::Colours::white.withAlpha (0.06f));
                g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
                g.setColour (tema::suau);
                g.drawText ("-" + juce::String (db), juce::Rectangle<float> (r.getRight() - 24.0f, y - 12.0f, 24.0f, 11.0f), juce::Justification::centredRight);
            }
            auto& m = lm.mesures();
            const int n = Mesures::numHist, p0 = m.posHist.load();
            juce::Path pa;
            pa.startNewSubPath (r.getX(), r.getY());
            for (int i = 0; i < n; ++i)
            {
                const float v = juce::jlimit (0.0f, maxDb, m.hist[(size_t) ((p0 + i) % n)].load());
                pa.lineTo (r.getX() + r.getWidth() * (float) i / (float) (n - 1), r.getY() + r.getHeight() * v / maxDb);
            }
            pa.lineTo (r.getRight(), r.getY());
            pa.closeSubPath();
            g.setColour (tema::accent.withAlpha (0.35f));
            g.fillPath (pa);
            g.setColour (tema::accent);
            g.strokePath (pa, juce::PathStrokeType (1.4f));
            g.setColour (tema::suau);
            g.drawText ("REDUCCIÓ (dB)"_ca, r.removeFromBottom (12.0f), juce::Justification::centredLeft);
        }
    private:
        Limitador& lm;
    };

    class PanellLimitador : public juce::Component, private juce::Timer, public OrigenPresets
    {
    public:
        explicit PanellLimitador (std::shared_ptr<Limitador> l) : lm (std::move (l)), grafic (*lm)
        {
            for (auto* b : { &btAnt, &btSeg, &btPreset, &btDesa }) addAndMakeVisible (b);
            addAndMakeVisible (btFav);
            btAnt.setTooltip ("Preset anterior"_ca);
            btSeg.setTooltip ("Preset següent"_ca);
            btPreset.setTooltip ("Clica per veure tots els presets (màster, busos, pistes, seguretat…)"_ca);
            btFav.setTooltip ("Preferit"_ca);
            btDesa.setTooltip ("Desa els ajustos com un preset teu"_ca);
            btAnt.onClick = [this] { pas (-1); };
            btSeg.onClick = [this] { pas (1); };
            btPreset.onClick = [this] { obreNavegador(); };
            btFav.onClick = [this] { commutaFavoritActual(); };
            btDesa.onClick = [this] { desaPreset(); };
            addAndMakeVisible (segEstil);
            segEstil.setTooltip (u8 (info (EstilP).ajuda));
            segEstil.onCanvi = [this] (int m) { lm->posa (EstilP, (float) m); refresca(); };
            for (auto [b, p] : { std::pair<Boto*, int> { &btAuto, AutoRel }, { &btTp, TruePeak }, { &btIgualat, Igualat } })
            {
                addAndMakeVisible (b);
                b->setLlum (tema::accent);
                b->setTooltip (u8 (info (p).ajuda));
                b->onClick = [this, p] { lm->posa (p, lm->valor (p) > 0.5f ? 0.0f : 1.0f); refresca(); };
            }
            addAndMakeVisible (grafic);
            for (int p : { (int) Gain, (int) Ceiling, (int) Release, (int) Link })
            {
                auto pom = std::make_unique<Pom> (u8 (info (p).nom), 0.0, 1.0, (double) Limitador::normal (p, info (p).def), false,
                                                  [this, p] (double v) { return u8 (lm->textParametre (p, (float) v)); });
                pom->setTooltip (u8 (info (p).ajuda));
                pom->onCanvi = [this, p] (double v) { lm->posaParametre (p, (float) v); };
                pom->onReinicia = [this, p] { lm->posa (p, info (p).def); refresca(); };
                addAndMakeVisible (*pom);
                poms.push_back ({ p, std::move (pom) });
            }
            setSize (880, 480);
            refresca();
            startTimerHz (25);
        }
        ~PanellLimitador() override { stopTimer(); }

        void refresca()
        {
            const auto nom = nomsPreset()[lm.get()];
            btPreset.setButtonText (nom.isEmpty() ? "(sense preset)"_ca : nom);
            btFav.setToggleState (esFavorit (nom), juce::dontSendNotification);
            segEstil.setTriat ((int) lm->valor (EstilP));
            btAuto.setToggleState (lm->valor (AutoRel) > 0.5f, juce::dontSendNotification);
            btTp.setToggleState (lm->valor (TruePeak) > 0.5f, juce::dontSendNotification);
            btIgualat.setToggleState (lm->valor (Igualat) > 0.5f, juce::dontSendNotification);
            for (auto& [p, pom] : poms) pom->setValor (lm->parametre (p), true);
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (tema::fons);
            vintage::panell (g, getLocalBounds().removeFromTop (56).toFloat());
            auto t = getLocalBounds().removeFromTop (56).reduced (14, 9).removeFromLeft (120).toFloat();
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (17.0f, true));
            g.drawText ("LIMITADOR", t.removeFromTop (22.0f), juce::Justification::centredLeft);
            g.setColour (tema::accent);
            g.setFont (dibuix::lletra (10.0f, true));
            g.drawText ("MIBASE", t, juce::Justification::centredLeft);
            auto& ms = lm->mesures();
            // vúmetres: entrada, reducció (avall) i sortida
            auto barra = [&] (juce::Rectangle<int> z, float a, juce::Colour col, bool avall, const juce::String& et)
            {
                auto b = z.toFloat();
                g.setColour (juce::Colours::black.withAlpha (0.55f));
                g.fillRoundedRectangle (b, 3.0f);
                a = juce::jlimit (0.0f, 1.0f, a);
                g.setColour (col);
                g.fillRoundedRectangle (avall ? b.withHeight (b.getHeight() * a) : b.withTop (b.getBottom() - b.getHeight() * a), 3.0f);
                g.setColour (tema::suau);
                g.setFont (dibuix::lletra (9.5f));
                g.drawText (et, z.translated (0, z.getHeight() + 2).withHeight (12).expanded (8, 0), juce::Justification::centred);
            };
            auto nivell = [] (float v) { return v > 1e-5f ? (20.0f * std::log10 (v) + 48.0f) / 48.0f : 0.0f; };
            barra (zonaIn, nivell (ms.entrada.load()), tema::accent, false, "IN");
            barra (zonaGr, ms.reduccio.load() / 12.0f, vintage::ledVermell, true, "GR");
            barra (zonaOut, nivell (ms.sortida.load()), tema::accent, false, "OUT");
            // LUFS i reducció
            auto zl = zonaLufs;
            const float lu = ms.lufsCurt.load();
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (26.0f, true));
            g.drawText (lu > -70.0f ? coma (lu, 1) : juce::String ("--"), zl.removeFromTop (32), juce::Justification::centred);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (10.0f, true));
            g.drawText ("LUFS (3 s)", zl.removeFromTop (14), juce::Justification::centred);
            zl.removeFromTop (8);
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (18.0f, true));
            g.drawText ("-" + coma (ms.reduccio.load(), 1), zl.removeFromTop (24), juce::Justification::centred);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (10.0f, true));
            g.drawText ("REDUCCIÓ dB"_ca, zl.removeFromTop (14), juce::Justification::centred);
            // a baix: l'estil i la latència
            juce::String txt = u8 (descripcioEstil ((int) lm->valor (EstilP)));
            txt << "  Mira 5 ms endavant (Mibase ho compensa).";
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
            dalt.removeFromLeft (120);
            segEstil.setBounds (dalt);
            r.reduce (14, 10);
            zonaText = r.removeFromBottom (30);
            // dreta: vúmetres i LUFS
            auto dreta = r.removeFromRight (150);
            zonaLufs = dreta.removeFromTop (110);
            dreta.removeFromTop (6);
            auto vu = dreta.withTrimmedBottom (16).withSizeKeepingCentre (3 * 14 + 2 * 10, dreta.getHeight() - 16);
            zonaIn = vu.removeFromLeft (14); vu.removeFromLeft (10);
            zonaGr = vu.removeFromLeft (14); vu.removeFromLeft (10);
            zonaOut = vu.removeFromLeft (14);
            r.removeFromRight (12);
            // esquerra: el gràfic; a sota, els poms i els botons
            auto botons = r.removeFromBottom (32);
            const int bw = (botons.getWidth() - 2 * 8) / 3;
            for (auto* b : { &btAuto, &btTp, &btIgualat }) { b->setBounds (botons.removeFromLeft (bw)); botons.removeFromLeft (8); }
            r.removeFromBottom (8);
            auto zp = r.removeFromBottom (juce::jmax (110, r.getHeight() * 45 / 100));
            r.removeFromBottom (8);
            grafic.setBounds (r);
            const int a = zp.getWidth() / 4;
            for (size_t i = 0; i < poms.size(); ++i) poms[i].second->setBounds (zp.getX() + (int) i * a, zp.getY(), a, zp.getHeight());
        }

        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }

    private:
        void timerCallback() override
        {
            grafic.repaint();
            repaint (zonaLufs);
            for (auto z : { zonaIn, zonaGr, zonaOut }) repaint (z.expanded (10, 16));
            const auto ara = lm->estat();
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
            const auto nom = nomsPreset()[lm.get()].toStdString();
            if (nom.empty()) { btFav.setToggleState (false, juce::dontSendNotification); return; }
            biblioteca().commutaFavorit (biblioteca().busca (nom) != nullptr ? BibliotecaPresets::clauUsuari (nom) : BibliotecaPresets::clauFabrica (nom));
            desaBiblioteca();
            refresca();
        }
        void aplicaPreset (const Preset& p)
        {
            lm->posaEstat (p.estat);
            nomsPreset()[lm.get()] = u8 (p.nom);
            refresca();
        }
        void pas (int d)
        {
            const auto v = totsPresets();
            if (v.empty()) return;
            const auto nom = nomsPreset()[lm.get()].toStdString();
            int i = -1;
            for (int k = 0; k < (int) v.size(); ++k) if (v[(size_t) k]->nom == nom) i = k;
            i = i < 0 ? (d > 0 ? 0 : (int) v.size() - 1) : (i + d + (int) v.size()) % (int) v.size();
            aplicaPreset (*v[(size_t) i]);
        }
        void desaPreset()
        {
            const auto actual = nomsPreset()[lm.get()];
            const bool meu = biblioteca().busca (actual.toStdString()) != nullptr;
            const auto proposta = meu ? actual : u8 (biblioteca().nomLliure ((actual.isEmpty() ? juce::String ("Limitador") : actual + " (meu)"_ca).toStdString()));
            dialeg = std::make_unique<DialegText> ("Desa el preset"_ca, "Nom del preset (si ja en tens un amb aquest nom, se substitueix)."_ca, "Desa"_ca,
                [] (const juce::String& t) { return t.trim().isEmpty() ? "Escriu un nom."_ca : juce::String(); },
                [this] (const juce::String& t)
                {
                    const auto nom = t.trim().toStdString();
                    const auto* ja = biblioteca().busca (nom);
                    biblioteca().desaPreset ({ nom, ja != nullptr ? ja->categoria : std::string ("Meus"), "", lm->estat() });
                    desaBiblioteca();
                    nomsPreset()[lm.get()] = u8 (nom);
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
                Limitador d;
                d.posaEstat (p.estat);
                const bool usuari = i < nu;
                v.push_back ({ u8 (p.nom), u8 (p.categoria), u8 (p.descripcio), u8 (nomEstil ((int) d.valor (EstilP))), usuari,
                               biblioteca().esFavorit (usuari ? BibliotecaPresets::clauUsuari (p.nom) : BibliotecaPresets::clauFabrica (p.nom)), i });
            }
            return v;
        }
        juce::String presetActual() override { return nomsPreset()[lm.get()]; }
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
                if (nomsPreset()[lm.get()] == vell) nomsPreset()[lm.get()] = nou;
            }
            refresca();
        }
        void mou (const juce::String& nom, const juce::String& carpeta) override { biblioteca().mou (nom.toStdString(), carpeta.toStdString()); desaBiblioteca(); }
        void esborra (const juce::String& nom) override { biblioteca().esborra (nom.toStdString()); desaBiblioteca(); refresca(); }
        void tancaNavegador() override
        {
            juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PanellLimitador> (this)] { if (sp != nullptr) { sp->navegador.reset(); sp->grabKeyboardFocus(); } });
        }

        std::unique_ptr<NavegadorPresetsComu> navegador;
        std::unique_ptr<DialegText> dialeg;
        Boto btAnt { {}, dibuix::Icona::Rebobina, Boto::Estil::Subtil }, btSeg { {}, dibuix::Icona::Endavant, Boto::Estil::Subtil };
        Boto btPreset { "(sense preset)"_ca, std::nullopt, Boto::Estil::Secundari };
        BotoEstrella btFav;
        Boto btDesa { "Desa", dibuix::Icona::Disc, Boto::Estil::Secundari };
        std::shared_ptr<Limitador> lm;
        Segmentat segEstil { { "Transparent", "Modern", "Punch", "Agressiu" } };
        Boto btAuto { "Auto release", std::nullopt, Boto::Estil::Tecla };
        Boto btTp { "True Peak", std::nullopt, Boto::Estil::Tecla };
        Boto btIgualat { "Escolta igualat", std::nullopt, Boto::Estil::Tecla };
        Grafic grafic;
        std::vector<std::pair<int, std::unique_ptr<Pom>>> poms;
        juce::Rectangle<int> zonaIn, zonaGr, zonaOut, zonaLufs, zonaText;
        std::string vist;
        juce::TooltipWindow tips { this, 500 };
    };

    class FinestraLimitador : public juce::DocumentWindow
    {
    public:
        FinestraLimitador (std::shared_ptr<Limitador> l, const juce::String& titol, std::function<void()> enTancar)
            : juce::DocumentWindow (titol, tema::fons, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
              tancar (std::move (enTancar))
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new PanellLimitador (std::move (l)), true);
            setResizable (true, true);
            setResizeLimits (720, 420, 1600, 1000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::MessageManager::callAsync ([t = tancar] { t(); }); }
        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }
    private:
        std::function<void()> tancar;
    };
}

std::unique_ptr<juce::DocumentWindow> creaFinestraLimitador (std::shared_ptr<Limitador> l, const juce::String& titol, std::function<void()> enTancar)
{
    return std::make_unique<FinestraLimitador> (std::move (l), titol, std::move (enTancar));
}
