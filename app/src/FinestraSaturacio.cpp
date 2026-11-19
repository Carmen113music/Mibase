#include "FinestraSaturacio.h"

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

using namespace mibase::saturacio;

namespace
{
    juce::String u8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str(), (int) s.size()); }
    juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }
    juce::String coma (double x, int d) { return juce::String (x, d).replaceCharacter ('.', ','); }

    // Presets propis i favorits (Documents\Mibase\Saturacio\presets.json)
    juce::File fitxerBiblioteca()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Mibase").getChildFile ("Saturacio").getChildFile ("presets.json");
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

    /** La corba de l'estil amb el drive d'ara (entrada → sortida) i on és el so. */
    class VistaCorba : public juce::Component
    {
    public:
        explicit VistaCorba (Saturacio& s) : sat (s) {}
        float nivell = 0.0f;   // pic d'entrada a la corba (lineal, ja amb el drive)
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRoundedRectangle (r, 5.0f);
            r.reduce (12.0f, 12.0f);
            auto aX = [&] (float x) { return r.getCentreX() + x * r.getWidth() * 0.5f; };
            auto aY = [&] (float y) { return r.getCentreY() - juce::jlimit (-1.0f, 1.0f, y) * r.getHeight() * 0.5f; };
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
            g.drawVerticalLine ((int) r.getCentreX(), r.getY(), r.getBottom());
            g.drawLine (aX (-1), aY (-1), aX (1), aY (1), 1.0f);
            const int e = (int) sat.valor (EstilP);
            const float d = sat.driveDb();
            juce::Path p;
            for (int i = 0; i <= 200; ++i)
            {
                const float x = -1.0f + 2.0f * (float) i / 200.0f;
                const float y = corba (e, x, d);
                if (i == 0) p.startNewSubPath (aX (x), aY (y)); else p.lineTo (aX (x), aY (y));
            }
            g.setColour (tema::accent);
            g.strokePath (p, juce::PathStrokeType (2.2f));
            // fins on arriba el so ara
            const float lin = std::pow (10.0f, d / 20.0f);
            const float x = juce::jlimit (0.0f, 1.0f, nivell / juce::jmax (1e-6f, lin));
            if (x > 0.002f)
            {
                g.setColour (tema::accent.withAlpha (0.12f));
                g.fillRect (juce::Rectangle<float> (aX (-x), r.getY(), aX (x) - aX (-x), r.getHeight()));
            }
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (10.0f));
            g.drawText ("ENTRADA", r.removeFromBottom (12.0f), juce::Justification::centredRight);
        }
    private:
        Saturacio& sat;
    };

    class PanellSaturacio : public juce::Component, private juce::Timer, public OrigenPresets
    {
    public:
        explicit PanellSaturacio (std::shared_ptr<Saturacio> s) : sat (std::move (s)), vista (*sat)
        {
            for (auto* b : { &btAnt, &btSeg, &btPreset, &btDesa }) addAndMakeVisible (b);
            addAndMakeVisible (btFav);
            btAnt.setTooltip ("Preset anterior"_ca);
            btSeg.setTooltip ("Preset següent"_ca);
            btPreset.setTooltip ("Clica per veure tots els presets (veus, guitarres, baix, bateria, màster…)"_ca);
            btFav.setTooltip ("Preferit"_ca);
            btDesa.setTooltip ("Desa els ajustos com un preset teu"_ca);
            btAnt.onClick = [this] { pas (-1); };
            btSeg.onClick = [this] { pas (1); };
            btPreset.onClick = [this] { obreNavegador(); };
            btFav.onClick = [this] { commutaFavoritActual(); };
            btDesa.onClick = [this] { desaPreset(); };
            addAndMakeVisible (segEstil);
            segEstil.setTooltip (u8 (info (EstilP).ajuda));
            segEstil.onCanvi = [this] (int m) { sat->posa (EstilP, (float) m); refresca(); };
            for (auto [b, p] : { std::pair<Boto*, int> { &btPunish, Punish }, { &btThump, Thump }, { &btSteep, Steep }, { &btAuto, Auto } })
            {
                addAndMakeVisible (b);
                b->setLlum (p == Punish ? vintage::ledVermell : tema::accent);
                b->setTooltip (u8 (info (p).ajuda));
                b->onClick = [this, p] { sat->posa (p, sat->valor (p) > 0.5f ? 0.0f : 1.0f); refresca(); };
            }
            addAndMakeVisible (vista);
            for (int p : { (int) Drive, (int) LowCut, (int) Tone, (int) HighCut, (int) Mix, (int) Sortida })
            {
                auto pom = std::make_unique<Pom> (u8 (info (p).nom), 0.0, 1.0, (double) Saturacio::normal (p, info (p).def), p == Tone,
                                                  [this, p] (double v) { return u8 (sat->textParametre (p, (float) v)); });
                pom->setTooltip (u8 (info (p).ajuda));
                pom->onCanvi = [this, p] (double v) { sat->posaParametre (p, (float) v); vista.repaint(); };
                pom->onReinicia = [this, p] { sat->posa (p, info (p).def); refresca(); };
                addAndMakeVisible (*pom);
                poms.push_back ({ p, std::move (pom) });
            }
            setSize (860, 470);
            refresca();
            startTimerHz (20);
        }
        ~PanellSaturacio() override { stopTimer(); }

        void refresca()
        {
            const auto nom = nomsPreset()[sat.get()];
            btPreset.setButtonText (nom.isEmpty() ? "(sense preset)"_ca : nom);
            btFav.setToggleState (esFavorit (nom), juce::dontSendNotification);
            segEstil.setTriat ((int) sat->valor (EstilP));
            btPunish.setToggleState (sat->valor (Punish) > 0.5f, juce::dontSendNotification);
            btThump.setToggleState (sat->valor (Thump) > 0.5f, juce::dontSendNotification);
            btSteep.setToggleState (sat->valor (Steep) > 0.5f, juce::dontSendNotification);
            btAuto.setToggleState (sat->valor (Auto) > 0.5f, juce::dontSendNotification);
            for (auto& [p, pom] : poms) pom->setValor (sat->parametre (p), true);
            vista.repaint();
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (tema::fons);
            vintage::panell (g, getLocalBounds().removeFromTop (56).toFloat());
            auto t = getLocalBounds().removeFromTop (56).reduced (14, 9).removeFromLeft (120).toFloat();
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (17.0f, true));
            g.drawText ("SATURACIÓ"_ca, t.removeFromTop (22.0f), juce::Justification::centredLeft);
            g.setColour (tema::accent);
            g.setFont (dibuix::lletra (10.0f, true));
            g.drawText ("MIBASE", t, juce::Justification::centredLeft);
            // entrada i sortida
            const auto& ms = sat->mesures();
            auto barra = [&] (juce::Rectangle<int> z, float v, const char* et)
            {
                auto b = z.toFloat();
                g.setColour (juce::Colours::black.withAlpha (0.55f));
                g.fillRoundedRectangle (b, 3.0f);
                const float a = juce::jlimit (0.0f, 1.0f, v > 1e-5f ? (20.0f * std::log10 (v) + 60.0f) / 60.0f : 0.0f);
                g.setColour (v > 0.99f ? vintage::ledVermell : tema::accent);
                g.fillRoundedRectangle (b.withTop (b.getBottom() - b.getHeight() * a), 3.0f);
                g.setColour (tema::suau);
                g.setFont (dibuix::lletra (9.5f));
                g.drawText (et, z.translated (0, z.getHeight() + 2).withHeight (12).expanded (6, 0), juce::Justification::centred);
            };
            barra (zonaIn, ms.entrada.load(), "IN");
            barra (zonaOut, ms.sortida.load(), "OUT");
            // a baix: l'estil i, amb Auto, quant compensa
            juce::String txt = u8 (descripcioEstil ((int) sat->valor (EstilP)));
            if (sat->valor (Auto) > 0.5f) txt << "   Auto: " << (ms.autoDb.load() > 0 ? "+" : "") << coma (ms.autoDb.load(), 1) << " dB";
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
            auto esq = r.removeFromLeft (juce::jmax (230, r.getWidth() * 34 / 100));
            zonaOut = esq.removeFromRight (12).withTrimmedBottom (16);
            esq.removeFromRight (4);
            zonaIn = esq.removeFromRight (12).withTrimmedBottom (16);
            esq.removeFromRight (8);
            vista.setBounds (esq.withSizeKeepingCentre (esq.getWidth(), juce::jmin (esq.getHeight(), esq.getWidth())));
            r.removeFromLeft (14);
            auto botons = r.removeFromBottom (32);
            const int bw = (botons.getWidth() - 3 * 8) / 4;
            for (auto* b : { &btPunish, &btThump, &btSteep, &btAuto }) { b->setBounds (botons.removeFromLeft (bw)); botons.removeFromLeft (8); }
            r.removeFromBottom (8);
            const int col = 3, files = 2;
            const int a = r.getWidth() / col, h = r.getHeight() / files;
            for (size_t i = 0; i < poms.size(); ++i)
                poms[i].second->setBounds (r.getX() + (int) (i % col) * a, r.getY() + (int) (i / col) * h, a, h - 6);
        }

        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }

    private:
        void timerCallback() override
        {
            vista.nivell = sat->mesures().nivellCorba.load();
            vista.repaint();
            repaint (zonaIn.expanded (8, 16));
            repaint (zonaOut.expanded (8, 16));
            repaint (zonaText);
            const auto ara = sat->estat();
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
            const auto nom = nomsPreset()[sat.get()].toStdString();
            if (nom.empty()) { btFav.setToggleState (false, juce::dontSendNotification); return; }
            biblioteca().commutaFavorit (biblioteca().busca (nom) != nullptr ? BibliotecaPresets::clauUsuari (nom) : BibliotecaPresets::clauFabrica (nom));
            desaBiblioteca();
            refresca();
        }
        void aplicaPreset (const Preset& p)
        {
            sat->posaEstat (p.estat);
            nomsPreset()[sat.get()] = u8 (p.nom);
            refresca();
        }
        void pas (int d)
        {
            const auto v = totsPresets();
            if (v.empty()) return;
            const auto nom = nomsPreset()[sat.get()].toStdString();
            int i = -1;
            for (int k = 0; k < (int) v.size(); ++k) if (v[(size_t) k]->nom == nom) i = k;
            i = i < 0 ? (d > 0 ? 0 : (int) v.size() - 1) : (i + d + (int) v.size()) % (int) v.size();
            aplicaPreset (*v[(size_t) i]);
        }
        void desaPreset()
        {
            const auto actual = nomsPreset()[sat.get()];
            const bool meu = biblioteca().busca (actual.toStdString()) != nullptr;
            const auto proposta = meu ? actual : u8 (biblioteca().nomLliure ((actual.isEmpty() ? juce::String ("Saturació"_ca) : actual + " (meu)"_ca).toStdString()));
            dialeg = std::make_unique<DialegText> ("Desa el preset"_ca, "Nom del preset (si ja en tens un amb aquest nom, se substitueix)."_ca, "Desa"_ca,
                [] (const juce::String& t) { return t.trim().isEmpty() ? "Escriu un nom."_ca : juce::String(); },
                [this] (const juce::String& t)
                {
                    const auto nom = t.trim().toStdString();
                    const auto* ja = biblioteca().busca (nom);
                    biblioteca().desaPreset ({ nom, ja != nullptr ? ja->categoria : std::string ("Meus"), "", sat->estat() });
                    desaBiblioteca();
                    nomsPreset()[sat.get()] = u8 (nom);
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
                Saturacio d;
                d.posaEstat (p.estat);
                const bool usuari = i < nu;
                v.push_back ({ u8 (p.nom), u8 (p.categoria), u8 (p.descripcio), u8 (nomLlargEstil ((int) d.valor (EstilP))), usuari,
                               biblioteca().esFavorit (usuari ? BibliotecaPresets::clauUsuari (p.nom) : BibliotecaPresets::clauFabrica (p.nom)), i });
            }
            return v;
        }
        juce::String presetActual() override { return nomsPreset()[sat.get()]; }
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
                if (nomsPreset()[sat.get()] == vell) nomsPreset()[sat.get()] = nou;
            }
            refresca();
        }
        void mou (const juce::String& nom, const juce::String& carpeta) override { biblioteca().mou (nom.toStdString(), carpeta.toStdString()); desaBiblioteca(); }
        void esborra (const juce::String& nom) override { biblioteca().esborra (nom.toStdString()); desaBiblioteca(); refresca(); }
        void tancaNavegador() override
        {
            juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PanellSaturacio> (this)] { if (sp != nullptr) { sp->navegador.reset(); sp->grabKeyboardFocus(); } });
        }

        std::unique_ptr<NavegadorPresetsComu> navegador;
        std::unique_ptr<DialegText> dialeg;
        Boto btAnt { {}, dibuix::Icona::Rebobina, Boto::Estil::Subtil }, btSeg { {}, dibuix::Icona::Endavant, Boto::Estil::Subtil };
        Boto btPreset { "(sense preset)"_ca, std::nullopt, Boto::Estil::Secundari };
        BotoEstrella btFav;
        Boto btDesa { "Desa", dibuix::Icona::Disc, Boto::Estil::Secundari };
        std::shared_ptr<Saturacio> sat;
        Segmentat segEstil { { "A", "E", "N", "T", "P", "Overdrive", "Fuzz" } };
        Boto btPunish { "Punish", std::nullopt, Boto::Estil::Tecla };
        Boto btThump { "Thump", std::nullopt, Boto::Estil::Tecla };
        Boto btSteep { "Steep", std::nullopt, Boto::Estil::Tecla };
        Boto btAuto { "Auto", std::nullopt, Boto::Estil::Tecla };
        VistaCorba vista;
        std::vector<std::pair<int, std::unique_ptr<Pom>>> poms;
        juce::Rectangle<int> zonaIn, zonaOut, zonaText;
        std::string vist;
        juce::TooltipWindow tips { this, 500 };
    };

    class FinestraSaturacio : public juce::DocumentWindow
    {
    public:
        FinestraSaturacio (std::shared_ptr<Saturacio> s, const juce::String& titol, std::function<void()> enTancar)
            : juce::DocumentWindow (titol, tema::fons, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
              tancar (std::move (enTancar))
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new PanellSaturacio (std::move (s)), true);
            setResizable (true, true);
            setResizeLimits (700, 400, 1600, 1000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::MessageManager::callAsync ([t = tancar] { t(); }); }
        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }
    private:
        std::function<void()> tancar;
    };
}

std::unique_ptr<juce::DocumentWindow> creaFinestraSaturacio (std::shared_ptr<Saturacio> s, const juce::String& titol, std::function<void()> enTancar)
{
    return std::make_unique<FinestraSaturacio> (std::move (s), titol, std::move (enTancar));
}
