#include "FinestraDinamica.h"

#include "Controls.h"
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

    class PanellDinamica : public juce::Component, private juce::Timer
    {
    public:
        explicit PanellDinamica (std::shared_ptr<Dinamica> d) : din (std::move (d)), corba (*din)
        {
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
            setSize (760, 430);
            refresca();
            startTimerHz (30);
        }
        ~PanellDinamica() override { stopTimer(); }

        void refresca()
        {
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
            g.drawText (ms.sidechain.load() ? "Escolta: SIDECHAIN (una altra pista)"_ca : "Escolta: el so d'aquesta pista (sense sidechain)"_ca, zonaFont,
                        juce::Justification::centredLeft);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            auto dalt = r.removeFromTop (56).reduced (14, 12);
            dalt.removeFromLeft (120);
            segMode.setBounds (dalt.removeFromLeft (juce::jmin (dalt.getWidth() - 220, segMode.ampladaIdeal() + 40)));
            btEscolta.setBounds (dalt.removeFromRight (110));
            dalt.removeFromRight (8);
            segDet.setBounds (dalt.removeFromRight (100));
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

        std::shared_ptr<Dinamica> din;
        Segmentat segMode { { "Compressor", "Gate", "Expander", "De-esser" } };
        Segmentat segDet { { "Peak", "RMS" } };
        Boto btEscolta { "Escolta SC", std::nullopt, Boto::Estil::Tecla };
        Corba corba;
        std::vector<std::pair<int, std::unique_ptr<Pom>>> poms;
        juce::Rectangle<int> zonaGR, zonaFont;
        float grVist = 0.0f;
        juce::TooltipWindow tips { this, 500 };
    };

    class FinestraDinamica : public juce::DocumentWindow
    {
    public:
        FinestraDinamica (std::shared_ptr<Dinamica> d, const juce::String& titol, std::function<void()> enTancar)
            : juce::DocumentWindow (titol, tema::fons, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
              tancar (std::move (enTancar))
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new PanellDinamica (std::move (d)), true);
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

std::unique_ptr<juce::DocumentWindow> creaFinestraDinamica (std::shared_ptr<Dinamica> d, const juce::String& titol, std::function<void()> enTancar)
{
    return std::make_unique<FinestraDinamica> (std::move (d), titol, std::move (enTancar));
}
