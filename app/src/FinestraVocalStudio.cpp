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
#include "PanellsMaster.h"
#include "Pom.h"
#include "Tema.h"
#include "Text.h"
#include "Vintage.h"
#include <map>
#include <mibase/Delay.h>
#include <mibase/Doubler.h>
#include <mibase/Drive.h>
#include <mibase/Equalitzador.h>
#include <mibase/Harmonia.h>
#include <mibase/Harmonitzador.h>
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
    // 0.94: els del Mastering Studio, a part (Documents\Mibase\Mastering Studio\presets.json)
    juce::File fitxerBiblioteca (bool master = false)
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Mibase").getChildFile (master ? "Mastering Studio" : "Vocal Studio").getChildFile ("presets.json");
    }
    BibliotecaPresets& biblioteca (bool master = false)
    {
        static BibliotecaPresets b[2];
        static bool carregada[2] {};
        if (! carregada[master ? 1 : 0]) { carregada[master ? 1 : 0] = true; b[master ? 1 : 0].carrega (dibuix::cami (fitxerBiblioteca (master))); }
        return b[master ? 1 : 0];
    }
    void desaBiblioteca (bool master = false) { biblioteca (master).desa (dibuix::cami (fitxerBiblioteca (master))); }

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
                if (auto* o = dynamic_cast<mibase::octava::Octava*> (fx.get()))   // 0.73
                {
                    pom->setTooltip (u8 (mibase::octava::info (p).ajuda));
                    pom->onReinicia = [this, o, p] { o->posa (p, mibase::octava::info (p).def); };
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

    /** 0.73: l'Harmonitzador: el mode, què canta i on van les veus, els controls de totes les veus i els de cada veu. */
    class PanellHarmonitzador : public juce::Component, private juce::Timer
    {
    public:
        explicit PanellHarmonitzador (std::shared_ptr<mibase::harmonitzador::Harmonitzador> e)
            : fx (std::move (e)), selMode (juce::StringArray { "Tonalitat", "Acords", "Teclat MIDI" })
        {
            namespace H = mibase::harmonitzador;
            selMode.setTriat ((int) fx->valor (H::ModeP));
            selMode.onCanvi = [this] (int m) { fx->posa (mibase::harmonitzador::ModeP, (float) m); actualitza(); };
            selMode.setTooltip (u8 (H::info (H::ModeP).ajuda));
            addAndMakeVisible (selMode);
            auto fesPom = [this] (int p)
            {
                namespace H = mibase::harmonitzador;
                const juce::String nom = p >= H::Interval1 && p < H::Nivell1 ? "On va"_ca
                                       : p >= H::Nivell1 && p < H::Pan1 ? "Nivell"_ca
                                       : p >= H::Pan1 ? "Pan"_ca : u8 (H::info (p).nom);
                auto pom = std::make_unique<Pom> (nom, 0.0, 1.0, (double) fx->parametre (p),
                                                  p == mibase::harmonitzador::Formant || (p >= mibase::harmonitzador::Pan1 && p < mibase::harmonitzador::Pan1 + 4),
                                                  [this, p] (double v) { return u8 (fx->textParametre (p, (float) v)); });
                pom->onCanvi = [this, p] (double v) { fx->posaParametre (p, (float) v); };
                pom->onReinicia = [this, p] { fx->posa (p, mibase::harmonitzador::info (p).def); };
                pom->setTooltip (u8 (mibase::harmonitzador::info (p).ajuda));
                addAndMakeVisible (*pom);
                poms[p] = std::move (pom);
            };
            for (int p = H::Sec; p < H::numParams; ++p) fesPom (p);
            setSize (700, 560);
            actualitza();
            startTimerHz (15);
        }
        void paint (juce::Graphics& g) override
        {
            namespace H = mibase::harmonitzador;
            g.fillAll (tema::fons);
            vintage::panell (g, getLocalBounds().toFloat().reduced (6.0f), 6.0f);
            // pantalla: què cantes i on van les veus
            g.setColour (juce::Colour (0xff0d1214));
            g.fillRoundedRectangle (lcd.toFloat(), 5.0f);
            g.setColour (tema::linia);
            g.drawRoundedRectangle (lcd.toFloat().reduced (0.5f), 5.0f, 1.0f);
            g.setColour (turquesa);
            g.setFont (dibuix::lletra (15.0f, true));
            g.drawText (textLcd, lcd.reduced (10, 0), juce::Justification::centredLeft, true);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (12.5f));
            g.drawFittedText (ajuda, ajudaR, juce::Justification::centredLeft, 2);
            g.setFont (dibuix::lletra (12.0f, true));
            g.setColour (tema::text);
            g.drawText ("TOTES LES VEUS"_ca, titolGlobal, juce::Justification::centredLeft, false);
            for (int v = 0; v < H::numVeus; ++v)
            {
                auto r = columnes[(size_t) v].toFloat();
                g.setColour (tema::superficie.withAlpha (0.6f));
                g.fillRoundedRectangle (r, 6.0f);
                const int nota = fx->notaVeu (v);
                g.setColour (nota >= 0 ? turquesa : tema::linia);
                g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.2f);
                g.setColour (tema::text);
                g.setFont (dibuix::lletra (13.0f, true));
                g.drawText ("Veu "_ca + juce::String (v + 1), r.removeFromTop (24.0f).reduced (8.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, false);
                g.setColour (nota >= 0 ? turquesa : tema::suau);
                g.setFont (dibuix::lletra (13.0f, true));
                g.drawText (nota >= 0 ? nomNota (nota) : juce::String ("—"_ca), columnes[(size_t) v].withHeight (24).reduced (8, 0), juce::Justification::centredRight, false);
            }
        }
        void resized() override
        {
            namespace H = mibase::harmonitzador;
            auto r = getLocalBounds().reduced (18, 16);
            auto dalt = r.removeFromTop (34);
            selMode.setBounds (dalt.removeFromLeft (300));
            dalt.removeFromLeft (12);
            lcd = dalt;
            r.removeFromTop (6);
            ajudaR = r.removeFromTop (34);
            r.removeFromTop (6);
            titolGlobal = r.removeFromTop (18);
            auto fila = r.removeFromTop (120);
            const int a = fila.getWidth() / 5;
            for (int p = H::Sec; p <= H::Lliscament; ++p) poms[p]->setBounds (fila.removeFromLeft (a).reduced (6, 0));
            r.removeFromTop (12);
            const int ac = r.getWidth() / H::numVeus;
            for (int v = 0; v < H::numVeus; ++v)
            {
                auto c = r.removeFromLeft (ac).reduced (5, 0);
                columnes[(size_t) v] = c;
                c.removeFromTop (26);
                poms[H::Interval1 + v]->setBounds (c.removeFromTop (c.getHeight() / 2).reduced (18, 2));
                auto baix = c.reduced (2, 2);
                poms[H::Nivell1 + v]->setBounds (baix.removeFromLeft (baix.getWidth() / 2));
                poms[H::Pan1 + v]->setBounds (baix);
            }
        }
        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }
    private:
        static juce::String nomNota (int n) { return u8 (mibase::harmonia::nomTo (((n % 12) + 12) % 12)) + juce::String (n / 12 - 1); }
        void actualitza()
        {
            namespace H = mibase::harmonitzador;
            const int mode = (int) fx->valor (H::ModeP);
            ajuda = mode == H::Tonalitat ? "Cada veu va uns graus amunt o avall dins de la tonalitat de la cançó (la que surt al panell d'Harmonia). Gira «On va» de cada veu; 0 = apagada."_ca
                  : mode == H::Acords ? "Cada veu va a una nota de l'acord que sona a la pista d'Acords (1a per sobre, 2a per sobre…). On no hi ha acord, fa servir la tonalitat."_ca
                  : "Toca notes al teclat MIDI (amb aquesta pista triada): cada nota és una veu, de greu a agut (fins a 4). Baixa «Veu» si vols sentir només les del teclat."_ca;
            for (int v = 0; v < H::numVeus; ++v) poms[H::Interval1 + v]->setEnabled (mode != H::Teclat);
            repaint();
        }
        void timerCallback() override
        {
            namespace H = mibase::harmonitzador;
            for (auto& [p, pom] : poms) pom->setValor (fx->parametre (p), true);
            if (selMode.getTriat() != (int) fx->valor (H::ModeP)) { selMode.setTriat ((int) fx->valor (H::ModeP)); actualitza(); }
            const float d = fx->notaDetectada();
            juce::String t = d >= 0.0f ? "Cantes: "_ca + nomNota ((int) std::lround (d)) : juce::String ("Cantes: —"_ca);
            juce::String veus;
            for (int v = 0; v < H::numVeus; ++v) if (fx->notaVeu (v) >= 0) veus << "  " << nomNota (fx->notaVeu (v));
            if (veus.isNotEmpty()) t << "   Veus:" << veus;
            if (t != textLcd) { textLcd = t; repaint(); }
        }
        std::shared_ptr<mibase::harmonitzador::Harmonitzador> fx;
        Segmentat selMode;
        std::map<int, std::unique_ptr<Pom>> poms;
        std::array<juce::Rectangle<int>, mibase::harmonitzador::numVeus> columnes;
        juce::Rectangle<int> lcd, ajudaR, titolGlobal;
        juce::String textLcd, ajuda;
    };

    /** 0.79: el Tune: d'on surt l'escala, la pantalla (què cantes, on la porta, quant corregeix) i els poms. */
    class PanellTune : public juce::Component, private juce::Timer
    {
    public:
        explicit PanellTune (std::shared_ptr<mibase::tune::Tune> e)
            : fx (std::move (e)), selMode (juce::StringArray { "Cançó"_ca, "Triada", "Cromàtica"_ca })
        {
            namespace T = mibase::tune;
            selMode.setTriat ((int) fx->valor (T::ModeP));
            selMode.onCanvi = [this] (int m) { fx->posa (mibase::tune::ModeP, (float) m); actualitza(); };
            selMode.setTooltip (u8 (T::info (T::ModeP).ajuda));
            addAndMakeVisible (selMode);
            const char* const notes[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            for (int i = 0; i < 12; ++i) cbTonica.addItem (notes[i], i + 1);
            for (int i = 0; i < T::numEscales; ++i) cbEscala.addItem (u8 (T::nomEscala (i)), i + 1);
            cbTonica.onChange = [this] { fx->posa (mibase::tune::Tonica, (float) (cbTonica.getSelectedId() - 1)); };
            cbEscala.onChange = [this] { fx->posa (mibase::tune::Escala, (float) (cbEscala.getSelectedId() - 1)); };
            cbTonica.setTooltip (u8 (T::info (T::Tonica).ajuda));
            cbEscala.setTooltip (u8 (T::info (T::Escala).ajuda));
            addAndMakeVisible (cbTonica);
            addAndMakeVisible (cbEscala);
            for (int p = T::Velocitat; p < T::numParams; ++p)
            {
                auto pom = std::make_unique<Pom> (u8 (T::info (p).nom), 0.0, 1.0, (double) fx->parametre (p), false,
                                                  [this, p] (double v) { return u8 (fx->textParametre (p, (float) v)); });
                pom->onCanvi = [this, p] (double v) { fx->posaParametre (p, (float) v); };
                pom->onReinicia = [this, p] { fx->posa (p, mibase::tune::info (p).def); };
                pom->setTooltip (u8 (T::info (p).ajuda));
                addAndMakeVisible (*pom);
                poms[p] = std::move (pom);
            }
            setSize (640, 430);
            actualitza();
            startTimerHz (30);
        }
        void paint (juce::Graphics& g) override
        {
            g.fillAll (tema::fons);
            vintage::panell (g, getLocalBounds().toFloat().reduced (6.0f), 6.0f);
            auto r = pantalla.toFloat();
            g.setColour (juce::Colour (0xff0d1214));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (tema::linia);
            g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
            auto dalt = r.removeFromTop (r.getHeight() * 0.62f).reduced (14.0f, 4.0f);
            g.setFont (dibuix::lletra (22.0f, true));
            g.setColour (detectada >= 0.0f ? turquesa : tema::suau);
            g.drawText (detectada >= 0.0f ? nomNota ((int) std::lround (detectada)) : juce::String ("—"_ca), dalt.removeFromLeft (110.0f), juce::Justification::centredLeft);
            g.setFont (dibuix::lletra (13.0f));
            g.setColour (tema::suau);
            if (detectada >= 0.0f)
            {
                const int c = (int) std::lround ((detectada - std::round (detectada)) * 100.0f);
                g.drawText ((c > 0 ? "+" : "") + juce::String (c) + " cèntims"_ca, dalt.removeFromLeft (110.0f), juce::Justification::centredLeft);
            }
            else dalt.removeFromLeft (110.0f);
            g.setFont (dibuix::lletra (22.0f, true));
            g.setColour (desti >= 0 ? tema::accent : tema::suau);
            g.drawText (desti >= 0 ? "→  "_ca + nomNota (desti) : juce::String(), dalt, juce::Justification::centredRight);   // 0.80: la fletxa sortia com «â□□»
            // la correcció d'ara: una barra des del mig (±100 cèntims)
            auto barra = r.reduced (14.0f, 8.0f);
            const float mig = barra.getCentreX();
            g.setColour (tema::linia);
            g.fillRoundedRectangle (barra.withHeight (6.0f).withCentre ({ mig, barra.getCentreY() }), 3.0f);
            g.fillRect (mig - 1.0f, barra.getY(), 2.0f, barra.getHeight());
            const float c = juce::jlimit (-100.0f, 100.0f, correccio);
            const float x = mig + c / 100.0f * barra.getWidth() * 0.5f;
            g.setColour (turquesa);
            g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (mig, x), barra.getCentreY() - 3.0f, std::abs (x - mig), 6.0f), 3.0f);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (12.5f));
            g.drawFittedText (ajuda, ajudaR, juce::Justification::centredLeft, 2);
        }
        void resized() override
        {
            namespace T = mibase::tune;
            auto r = getLocalBounds().reduced (18, 16);
            auto dalt = r.removeFromTop (32);
            selMode.setBounds (dalt.removeFromLeft (300));
            dalt.removeFromLeft (12);
            cbTonica.setBounds (dalt.removeFromLeft (70));
            dalt.removeFromLeft (8);
            cbEscala.setBounds (dalt.removeFromLeft (190));
            r.removeFromTop (12);
            pantalla = r.removeFromTop (92);
            r.removeFromTop (6);
            ajudaR = r.removeFromTop (36);
            r.removeFromTop (8);
            const int a = r.getWidth() / 4;
            for (int p = T::Velocitat; p < T::numParams; ++p) poms[p]->setBounds (r.removeFromLeft (a).reduced (8, 0));
        }
        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }
    private:
        static juce::String nomNota (int n) { return u8 (mibase::harmonia::nomTo (((n % 12) + 12) % 12)) + juce::String (n / 12 - 1); }
        void actualitza()
        {
            namespace T = mibase::tune;
            const int mode = (int) fx->valor (T::ModeP);
            cbTonica.setSelectedId ((int) fx->valor (T::Tonica) + 1, juce::dontSendNotification);
            cbEscala.setSelectedId ((int) fx->valor (T::Escala) + 1, juce::dontSendNotification);
            cbTonica.setEnabled (mode == T::Triada);
            cbEscala.setEnabled (mode == T::Triada);
            ajuda = mode == T::Canco ? "Afina a l'escala de la cançó (la tonalitat del projecte, la del panell d'Harmonia)."_ca
                  : mode == T::Triada ? "Afina a l'escala que triïs aquí."_ca
                  : "Afina a la nota més propera, sigui quina sigui (totes les notes)."_ca;
            ajuda << "  Velocitat 0 ms = l'efecte robòtic; 20–50 ms = natural."_ca;
            repaint();
        }
        void timerCallback() override
        {
            namespace T = mibase::tune;
            for (auto& [p, pom] : poms) pom->setValor (fx->parametre (p), true);
            if (selMode.getTriat() != (int) fx->valor (T::ModeP)) { selMode.setTriat ((int) fx->valor (T::ModeP)); actualitza(); }
            const float d = fx->notaDetectada();
            const int t = fx->notaDesti();
            const float c = fx->correccioAra();
            if (std::abs (d - detectada) > 0.01f || t != desti || std::abs (c - correccio) > 0.5f)
            {
                detectada = d; desti = t; correccio = c;
                repaint (pantalla);
            }
        }
        std::shared_ptr<mibase::tune::Tune> fx;
        Segmentat selMode;
        juce::ComboBox cbTonica, cbEscala;
        std::map<int, std::unique_ptr<Pom>> poms;
        juce::Rectangle<int> pantalla, ajudaR;
        juce::String ajuda;
        float detectada = -1.0f, correccio = 0.0f;
        int desti = -1;
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
                if (mod == nullptr && e.modul != -1) continue;
                f->k = k;
                f->nom = mod != nullptr ? nomDe (*mod) + " · "_ca + nomParam (*mod, e.param) : "Rack · "_ca + u8 (rk->infoParametre (e.param).nom);   // 0.76
                auto text = [this, e] (double v)
                {
                    if (e.modul == -1) return u8 (rk->textParametre (e.param, (float) v));
                    auto* md = rk->modul (e.modul);
                    return md != nullptr ? u8 (md->fx->textParametre (e.param, (float) v)) : juce::String();
                };
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
        void mouseDown (const juce::MouseEvent& e) override;
        int index;
    private:
        PanellVocalStudio& pare;
        std::shared_ptr<Rack> rk;
        Boto btOn { {}, dibuix::Icona::Encen, Boto::Estil::Tecla };
        Boto btObre { "Obre", dibuix::Icona::Finestra, Boto::Estil::Secundari };
        Boto btTreu { {}, dibuix::Icona::Tanca, Boto::Estil::Subtil };
        BotoFletxa btAmunt { true }, btAvall { false };
        std::unique_ptr<Pom> pomMix;
        juce::Rectangle<int> zonaNom, zonaAct;   // 0.94: quant actua
    };

    /** 0.76: la capçalera d'un camí: «Cadena principal» o un retorn (nom, Envia, Retorn, Pan, Ducking, + mòdul, treu). */
    class CapCami : public juce::Component
    {
    public:
        CapCami (PanellVocalStudio& p, std::shared_ptr<Rack> r, int c);
        void paint (juce::Graphics& g) override;
        void resized() override;
        void refresca();
        int cami;
    private:
        PanellVocalStudio& pare;
        std::shared_ptr<Rack> rk;
        Boto btNom { {}, std::nullopt, Boto::Estil::Subtil };
        Boto btMes { "Mòdul"_ca, dibuix::Icona::Mes, Boto::Estil::Subtil };
        Boto btTreu { {}, dibuix::Icona::Tanca, Boto::Estil::Subtil };
        std::array<std::unique_ptr<Pom>, 4> poms;
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
            btAfegeix.setTooltip ("Afegeix un mòdul al final de la cadena principal"_ca);
            btAfegeix.onClick = [this] { menuAfegeix (0, &btAfegeix); };
            addAndMakeVisible (btSolo);   // 0.79: solo de la pista sense haver d'anar-hi
            btSolo.setTooltip ("Solo de la pista d'aquest Vocal Studio"_ca);
            btSolo.setClickingTogglesState (false);
            btSolo.onClick = [this] { if (accioPistes) { accioPistes (4); refresca(); } };
            addAndMakeVisible (btPistes);   // 0.78
            btPistes.setTooltip ("Converteix els retorns en busos d'efectes de debò (carpeta «VS · …»), amb els enviaments i el ducking. Al Vocal Studio hi queda la cadena principal."_ca);
            btPistes.onClick = [this] { if (accioPistes) { accioPistes (1); refesFiles(); refresca(); } };
            addAndMakeVisible (btTreuCadena);
            btTreuCadena.setTooltip ("Esborra els busos «VS · …» que reben aquesta pista (i la seva carpeta). Es pot desfer."_ca);
            btTreuCadena.onClick = [this] { if (accioPistes) { accioPistes (2); refresca(); } };
            addAndMakeVisible (btRetorn);   // 0.76
            btRetorn.setTooltip ("Un camí en paral·lel: reverb, delay, compressió paral·lela, dobles o harmonies, amb el seu volum i ducking"_ca);
            btRetorn.onClick = [this] { menuRetorn(); };
            setSize (900, 700);
            if (esM())   // 0.94: Mastering Studio: sense retorns, «A pistes reals» ni solo; amb el panell del màster
            {
                for (auto* b : { &btRetorn, &btPistes, &btTreuCadena, &btSolo }) b->setVisible (false);
                panellMaster.reset (creaPanellMaster (rk));
                addAndMakeVisible (*panellMaster);
                btPreset.setTooltip ("Clica per veure tots els presets del màster (bàsics, objectiu sonor i gèneres)"_ca);
                btDesa.setTooltip ("Desa tot el Mastering Studio (mòduls, ajustos, macros i objectiu) com un preset teu"_ca);
                setSize (1180, 920);
            }
            if (rk->nomPreset().empty() && ! presetsFabrica (rk->tipusRack()).empty())
            {
                Rack inicial (rk->tipusRack());
                inicial.posaEstat (estatInicial (rk->tipusRack()));
                if (inicial.estat() == rk->estat()) rk->posaNomPreset (presetsFabrica (rk->tipusRack()).front().nom);
            }
            refesFiles();
            refresca();
            startTimerHz (25);
        }
        ~PanellVocalStudio() override { stopTimer(); finestresModuls.clear(); }
        bool esM() const { return rk->esMaster(); }   // 0.94: Mastering Studio

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
            else if (auto st = std::dynamic_pointer_cast<mibase::estereo::Estereo> (fx))   // 0.94
                w = std::make_unique<FinestraSimple> (t, creaPanellEstereo (st), tanca);
            else if (auto ld = std::dynamic_pointer_cast<mibase::loudness::Modul> (fx))
                w = std::make_unique<FinestraSimple> (t, creaPanellLoudness (ld), tanca);
            else if (auto* pc = creaPanellClipper (fx))   // 0.96
                w = std::make_unique<FinestraSimple> (t, pc, tanca);
            else if (auto* pt = creaPanellTransitoris (fx))
                w = std::make_unique<FinestraSimple> (t, pt, tanca);
            else if (auto* pm = creaPanellMultibanda (fx))   // 0.99
                w = std::make_unique<FinestraSimple> (t, pm, tanca);
            else if (auto* pl = creaPanellLowEnd (fx))
                w = std::make_unique<FinestraSimple> (t, pl, tanca);
            else if (auto h = std::dynamic_pointer_cast<mibase::harmonitzador::Harmonitzador> (fx))   // 0.73
                w = std::make_unique<FinestraSimple> (t, new PanellHarmonitzador (h), tanca);
            else if (auto tu = std::dynamic_pointer_cast<mibase::tune::Tune> (fx))   // 0.79
                w = std::make_unique<FinestraSimple> (t, new PanellTune (tu), tanca);
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
        void mouModul (int i, int d)
        {
            // 0.76: dins del mateix camí (salta els mòduls dels altres camins)
            const int c = rk->cami (i);
            for (int j = i + d; j >= 0 && j < rk->numModuls(); j += d)
                if (rk->cami (j) == c) { rk->mou (i, j); haCanviat(); return; }
        }
        int veiDelCami (int i, int d) const
        {
            const int c = rk->cami (i);
            for (int j = i + d; j >= 0 && j < rk->numModuls(); j += d) if (rk->cami (j) == c) return j;
            return -1;
        }
        int ordreDinsCami (int i) const
        {
            int n = 0;
            for (int j = 0; j <= i; ++j) if (rk->cami (j) == rk->cami (i)) ++n;
            return n;
        }
        void menuMouACami (int i)
        {
            juce::PopupMenu m;
            m.addItem (100, "Restableix el mòdul"_ca);   // 0.94
            m.addItem (101, "Duplica el mòdul"_ca, rk->numModuls() < maxModuls);
            if (! esM())
            {
                m.addSectionHeader ("Mou el mòdul a…"_ca);
                m.addItem (1, "Cadena principal"_ca, rk->cami (i) != 0, rk->cami (i) == 0);
                for (int k = 0; k < maxRetorns; ++k)
                    if (rk->retorn (k).actiu) m.addItem (2 + k, "Retorn "_ca + juce::String (k + 1) + " · " + u8 (rk->retorn (k).nom), rk->cami (i) != k + 1, rk->cami (i) == k + 1);
            }
            m.showMenuAsync ({}, [sp = juce::Component::SafePointer<PanellVocalStudio> (this), i] (int r)
            {
                if (sp == nullptr || r <= 0) return;
                if (r == 100) { sp->restableixModul (i); return; }
                if (r == 101) { juce::MessageManager::callAsync ([sp, i] { if (sp != nullptr) sp->duplicaModul (i); }); return; }
                sp->rk->posaCami (i, r - 1);
                sp->haCanviat(); sp->refesFiles(); sp->refresca();
            });
        }
        void treuRetorn (int k)
        {
            for (int i = rk->numModuls() - 1; i >= 0; --i)
                if (rk->cami (i) == k + 1) { if (auto* m = rk->modul (i)) finestresModuls.erase (m); rk->treu (i); }
            rk->activaRetorn (k, false);
            haCanviat(); refesFiles(); refresca();
        }
        void canviaNomRetorn (int k)
        {
            dialeg = std::make_unique<DialegText> ("Nom del retorn"_ca, "Un nom curt (per exemple: Reverb, Delay, Dobles)."_ca, "D'acord"_ca,
                [] (const juce::String& t) { return t.trim().isEmpty() ? "Escriu un nom."_ca : juce::String(); },
                [this, k] (const juce::String& t) { rk->activaRetorn (k, true, t.trim().substring (0, 24).toStdString()); haCanviat(); refesFiles(); refresca(); },
                u8 (rk->retorn (k).nom));
            DialegText::mostra (*this, dialeg);
        }
        void menuAfegeix (int cami, juce::Component* on)
        {
            juce::PopupMenu m;
            int id = 1;
            for (const auto& t : tipus (rk->tipusRack()))
                m.addItem (id++, u8 (t.nom) + "  ·  "_ca + u8 (t.descripcio));
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (on), [sp = juce::Component::SafePointer<PanellVocalStudio> (this), cami] (int r)
            {
                if (sp == nullptr || r <= 0 || r > (int) tipus (sp->rk->tipusRack()).size()) return;
                const int nou = sp->rk->afegeix (tipus (sp->rk->tipusRack())[(size_t) r - 1].clau, -1, cami);
                if (nou < 0) return;
                if (cami > 0) sp->al100 (nou);
                sp->haCanviat(); sp->refesFiles(); sp->refresca();
            });
        }
        /** En un retorn, la reverb, el delay, el Doubler i l'harmonitzador sonen només amb el seu so (sense la veu). */
        void al100 (int i)
        {
            auto* m = rk->modul (i);
            if (m == nullptr) return;
            if (auto rv = std::dynamic_pointer_cast<mibase::reverb::Reverb> (m->fx)) rv->posa (mibase::reverb::Mix, 100);
            else if (auto dl = std::dynamic_pointer_cast<mibase::delay::Delay> (m->fx)) dl->posa (mibase::delay::Mix, 100);
            else if (auto d = std::dynamic_pointer_cast<mibase::doubler::Doubler> (m->fx)) { d->posa (mibase::doubler::Veu, 0); d->posa (mibase::doubler::Quantitat, 100); }
            else if (auto h = std::dynamic_pointer_cast<mibase::harmonitzador::Harmonitzador> (m->fx)) h->posa (mibase::harmonitzador::Sec, 0);
            else if (auto o = std::dynamic_pointer_cast<mibase::octava::Octava> (m->fx)) o->posa (mibase::octava::Mescla, 100);
        }
        void menuRetorn()
        {
            int k = -1;
            for (int q = 0; q < maxRetorns && k < 0; ++q) if (! rk->retorn (q).actiu) k = q;
            if (k < 0) return;
            juce::PopupMenu m;
            m.addSectionHeader ("Retorn nou"_ca);
            m.addItem (1, "Reverb (plate)"_ca);
            m.addItem (2, "Delay (negra, amb ducking)"_ca);
            m.addItem (3, "Compressió paral·lela"_ca);
            m.addItem (4, "Dobles (Doubler)"_ca);
            m.addItem (5, "Harmonies (Harmonitzador)"_ca);
            m.addItem (6, "Buit"_ca);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (btRetorn), [sp = juce::Component::SafePointer<PanellVocalStudio> (this), k] (int r)
            {
                if (sp == nullptr || r <= 0) return;
                auto& rk = *sp->rk;
                static const char* const noms[] = { "", "Reverb", "Delay", "Comp. paral·lela", "Dobles", "Harmonies", "Retorn" };
                rk.activaRetorn (k, true, r == 6 ? ("Retorn " + std::to_string (k + 1)) : noms[r]);
                rk.posaValor (Envia1 + k, 0.0f);
                rk.posaValor (Retorn1 + k, r == 1 ? -14.0f : r == 2 ? -16.0f : r == 3 ? -18.0f : -10.0f);
                rk.posaValor (PanRetorn1 + k, 0.0f);
                rk.posaValor (Ducking1 + k, r == 2 ? 8.0f : 0.0f);
                int nou = -1;
                if (r == 1) nou = rk.afegeix ("reverb", -1, k + 1);
                else if (r == 2) nou = rk.afegeix ("delay", -1, k + 1);
                else if (r == 3)
                {
                    nou = rk.afegeix ("compressor", -1, k + 1);
                    if (auto* md = rk.modul (nou))
                        if (auto d = std::dynamic_pointer_cast<mibase::dinamica::Dinamica> (md->fx))
                        {
                            using namespace mibase::dinamica;
                            d->posa (Llindar, -40); d->posa (Ratio, 20); d->posa (Atac, 0.5f); d->posa (Alliberament, 50); d->posa (Guany, 14);
                        }
                }
                else if (r == 4) nou = rk.afegeix ("doubler", -1, k + 1);
                else if (r == 5) nou = rk.afegeix ("harmonitzador", -1, k + 1);
                if (nou >= 0) sp->al100 (nou);
                sp->haCanviat(); sp->refesFiles(); sp->refresca();
            });
        }
        void haCanviat() { rk->marcaModificat(); }   // 0.79: el nom es queda (amb «· modificat»)
        juce::Colour color() const { return esM() ? juce::Colour (0xffffb84a) : turquesa; }   // 0.94: ambre per al màster
        /** 0.94: restableix un mòdul (els valors amb què neix) o el duplica just a sota. */
        void restableixModul (int i)
        {
            auto* m = rk->modul (i);
            if (m == nullptr) return;
            if (auto nou = creaModul (m->clau, nullptr)) { m->fx->posaEstat (nou->estat()); rk->posaMix (i, 1.0f); haCanviat(); refresca(); }
        }
        void duplicaModul (int i)
        {
            auto* m = rk->modul (i);
            if (m == nullptr || rk->numModuls() >= maxModuls) return;
            const auto clau = m->clau;
            const auto estatFx = m->fx->estat();
            const float mix = m->mix.load();
            const bool on = m->on.load();
            const int nou = rk->afegeix (clau, i + 1, rk->cami (i));
            if (auto* d = rk->modul (nou)) { d->fx->posaEstat (estatFx); rk->posaMix (nou, mix); rk->posaOn (nou, on); }
            haCanviat(); refesFiles(); refresca();
        }

        void refresca()
        {
            const auto nom = u8 (rk->nomPreset());
            btPreset.setButtonText (nom.isEmpty() ? "(sense preset)"_ca : nom + (rk->presetModificat() ? juce::String (" · modificat"_ca) : juce::String()));
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
            for (auto& c : caps) c->refresca();
            btAfegeix.setEnabled (rk->numModuls() < maxModuls);
            {
                bool lliure = false;
                for (int k = 0; k < maxRetorns; ++k) lliure = lliure || ! rk->retorn (k).actiu;
                btRetorn.setEnabled (lliure && rk->numModuls() < maxModuls);
                bool hiHa = false;
                for (int k = 0; k < maxRetorns; ++k) hiHa = hiHa || rk->retorn (k).actiu;
                btPistes.setEnabled (accioPistes != nullptr);   // 0.83: sense retorns, explica per què (abans semblava que no feia res)
                juce::ignoreUnused (hiHa);
                btSolo.setEnabled (accioPistes != nullptr);
                btSolo.setToggleState (accioPistes != nullptr && accioPistes (3) > 0, juce::dontSendNotification);
                btTreuCadena.setEnabled (accioPistes != nullptr && accioPistes (0) > 0);
            }
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
            g.drawText (esM() ? "MASTERING STUDIO" : "VOCAL STUDIO", t.removeFromTop (24.0f).withWidth (260.0f), juce::Justification::centredLeft);
            g.setColour (color());
            g.setFont (dibuix::lletra (10.0f, true));
            g.drawText (esM() ? "MIBASE · RACK DE MÀSTER"_ca : "MIBASE · RACK DE VEU"_ca, t.withWidth (260.0f), juce::Justification::centredLeft);
            // vúmetres d'entrada i sortida
            auto& ms = rk->mesures();
            auto nivell = [] (float v) { return v > 1e-5f ? juce::jlimit (0.0f, 1.0f, (20.0f * std::log10 (v) + 48.0f) / 48.0f) : 0.0f; };
            for (auto [z, v] : { std::pair<juce::Rectangle<int>, float> { zonaVuIn, vuIn }, { zonaVuOut, vuOut } })
            {
                auto b = z.toFloat();
                g.setColour (juce::Colours::black.withAlpha (0.55f));
                g.fillRoundedRectangle (b, 2.0f);
                const float a = nivell (v);
                g.setColour (v > 0.98f ? vintage::ledVermell : color());
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
            txt << (esM() ? "Doble clic o «Obre» per veure un mòdul. Botó dret: restableix o duplica. Compara sempre amb «Igualat»."_ca
                          : "Doble clic a un mòdul o «Obre» per veure'l sencer."_ca);
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
                btSolo.setBounds (pr.removeFromRight (34));   // 0.79
            }
            if (navegador != nullptr) navegador->setBounds (getLocalBounds());
            r.reduce (14, 0);
            if (panellMaster != nullptr)   // 0.94
            {
                auto d = r.removeFromRight (310);
                panellMaster->setBounds (d.withTrimmedBottom (8).withTrimmedTop (4));
                r.removeFromRight (12);
            }
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
            {
                auto b = r.removeFromBottom (34);
                btAfegeix.setBounds (b.removeFromLeft (220));
                b.removeFromLeft (10);
                btRetorn.setBounds (b.removeFromLeft (220));
                btTreuCadena.setBounds (b.removeFromRight (170));   // 0.78
                b.removeFromRight (10);
                btPistes.setBounds (b.removeFromRight (190));
            }
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
            // 0.79: la llista només es refà si canvia l'estructura (mòduls, camins, retorns); moure un pom (Mix…) no la refà,
            // perquè si no, el pom que s'arrossega desapareix a mig gest (baixava a batzegades)
            if (rk->versio() != versioVista)
            {
                versioVista = rk->versio();
                const auto sig = signatura();
                if (sig != signaturaVista) { refesFiles(); refresca(); }
            }
            const auto ara = rk->estat();
            if (ara != vist) { vist = ara; refresca(); }
            if (accioPistes != nullptr)   // 0.79: el solo es pot haver canviat des de la pista
            {
                const bool so = accioPistes (3) > 0;
                if (so != btSolo.getToggleState()) btSolo.setToggleState (so, juce::dontSendNotification);
            }
            // vúmetres (pic que cau a poc a poc)
            auto& ms = rk->mesures();
            vuIn = juce::jmax (ms.entrada.exchange (0.0f), vuIn * 0.82f);
            vuOut = juce::jmax (ms.sortida.exchange (0.0f), vuOut * 0.82f);
            repaint (zonaVuIn.expanded (2));
            repaint (zonaVuOut.expanded (2));
            if (++tic % 3 == 0) for (auto& f : files) f->repaint();   // 0.94: quant actua cada mòdul i els avisos
            // les finestres dels mòduls que ja no hi són
            for (auto it = finestresModuls.begin(); it != finestresModuls.end();)
            {
                bool hiEs = false;
                for (int i = 0; i < rk->numModuls(); ++i) if (rk->modul (i) == it->first) hiEs = true;
                it = hiEs ? std::next (it) : finestresModuls.erase (it);
            }
        }

        std::string signatura() const
        {
            std::string r;
            for (int i = 0; i < rk->numModuls(); ++i) if (auto* m = rk->modul (i)) r += m->clau + ":" + std::to_string (rk->cami (i)) + ";";
            for (int k = 0; k < maxRetorns; ++k) r += (rk->retorn (k).actiu ? "1" : "0") + rk->retorn (k).nom + "|";
            return r;
        }
        std::string signaturaVista;
        void refesFiles()
        {
            // 0.76: per camins: la cadena principal i, a sota, cada retorn actiu amb els seus mòduls
            versioVista = rk->versio();
            signaturaVista = signatura();
            files.clear();
            caps.clear();
            ordre.clear();
            for (int c = 0; c <= maxRetorns; ++c)
            {
                if (c > 0 && ! rk->retorn (c - 1).actiu) continue;
                bool hiHaRetorns = false;
                for (int k = 0; k < maxRetorns; ++k) hiHaRetorns = hiHaRetorns || rk->retorn (k).actiu;
                if (c > 0 || hiHaRetorns)
                {
                    caps.push_back (std::make_unique<CapCami> (*this, rk, c));
                    contingut.addAndMakeVisible (*caps.back());
                    ordre.push_back ({ caps.back().get(), c == 0 ? 30 : 74 });
                }
                for (int i = 0; i < rk->numModuls(); ++i)
                {
                    if (rk->cami (i) != c) continue;
                    files.push_back (std::make_unique<FilaModul> (*this, rk, i));
                    contingut.addAndMakeVisible (*files.back());
                    ordre.push_back ({ files.back().get(), 64 });
                }
            }
            posaFiles();
        }
        void posaFiles()
        {
            int total = 0;
            for (auto& [c, a] : ordre) total += a;
            const int w = juce::jmax (100, vista.getWidth() - (total > vista.getHeight() ? vista.getScrollBarThickness() : 0));
            contingut.setSize (w, juce::jmax (1, total));
            int y = 0;
            for (auto& [c, a] : ordre) { c->setBounds (0, y, w, a - 4); y += a; }
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
            {   // 0.76: els volums dels retorns
                juce::PopupMenu sub;
                for (int k = 0; k < maxRetorns; ++k)
                {
                    if (! rk->retorn (k).actiu) continue;
                    for (int p : { (int) Envia1 + k, (int) Retorn1 + k, (int) PanRetorn1 + k, (int) Ducking1 + k })
                    {
                        bool ja = false;
                        for (const auto& e : rk->enllacos()) if (e.macro == m && e.modul == -1 && e.param == p) ja = true;
                        opcions.push_back ({ -1, p });
                        sub.addItem (1000 + (int) opcions.size() - 1, u8 (rk->infoParametre (p).nom), ! ja, ja);
                    }
                }
                if (sub.getNumItems() > 0) enllaca.addSubMenu ("Retorns"_ca, sub);
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
                    if (mod == nullptr && i != -1) return;
                    // el rang comença on és el paràmetre ara; amb el macro a 10, mig recorregut més enllà
                    const float ara = mod != nullptr ? mod->fx->parametre (p) : sp->rk->parametre (p);
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
            for (auto& p : biblioteca (esM()).usuari) v.push_back (&p);
            for (auto& p : presetsFabrica (rk->tipusRack())) v.push_back (&p);
            return v;
        }
        bool esFavorit (const juce::String& nom)
        {
            const auto n = nom.toStdString();
            return biblioteca (esM()).esFavorit (biblioteca (esM()).busca (n) != nullptr ? BibliotecaPresets::clauUsuari (n) : BibliotecaPresets::clauFabrica (n));
        }
        void commutaFavoritActual()
        {
            const auto nom = rk->nomPreset();
            if (nom.empty()) { btFav.setToggleState (false, juce::dontSendNotification); return; }
            biblioteca (esM()).commutaFavorit (biblioteca (esM()).busca (nom) != nullptr ? BibliotecaPresets::clauUsuari (nom) : BibliotecaPresets::clauFabrica (nom));
            desaBiblioteca (esM());
            refresca();
        }
        void aplicaPreset (const Preset& p)
        {
            finestresModuls.clear();   // els mòduls d'abans se'n van
            // 1.12: el perfil de loudness que has triat i els teus LUFS del CD es queden (abans, el preset els canviava)
            const int objectiu = rk->objectiu();
            const float lufs = rk->lufsPersonal();
            rk->posaEstat (p.estat);
            if (rk->esMaster())
            {
                if (objectiu > 0) rk->posaObjectiu (objectiu);   // amb «Cap», el del preset
                rk->posaLufsPersonal (lufs);
                if (rk->lufsObjectiu() < -0.5f) mibase::rack::posaSostre (*rk, rk->truePeakObjectiu());
            }
            rk->posaNomPreset (p.nom);
            refesFiles();
            refresca();
        }
        void pas (int d)
        {
            const auto v = totsPresets();
            if (v.empty()) return;
            const auto nom = rk->nomPreset();
            int i = -1;
            for (int k = 0; k < (int) v.size(); ++k) if (v[(size_t) k]->nom == nom) i = k;
            i = i < 0 ? (d > 0 ? 0 : (int) v.size() - 1) : (i + d + (int) v.size()) % (int) v.size();
            aplicaPreset (*v[(size_t) i]);
        }
        void desaPreset()
        {
            const auto actual = u8 (rk->nomPreset());
            const bool meu = biblioteca (esM()).busca (actual.toStdString()) != nullptr;
            const auto proposta = meu ? actual : u8 (biblioteca (esM()).nomLliure ((actual.isEmpty() ? juce::String (esM() ? "El meu màster"_ca : "La meva veu") : actual + " (meu)"_ca).toStdString()));
            dialeg = std::make_unique<DialegText> ("Desa el preset"_ca, "Nom del preset (si ja en tens un amb aquest nom, se substitueix)."_ca, "Desa"_ca,
                [] (const juce::String& t) { return t.trim().isEmpty() ? "Escriu un nom."_ca : juce::String(); },
                [this] (const juce::String& t)
                {
                    const auto nom = t.trim().toStdString();
                    const auto* ja = biblioteca (esM()).busca (nom);
                    biblioteca (esM()).desaPreset ({ nom, ja != nullptr ? ja->categoria : std::string ("Meus"), "", rk->estat() });
                    desaBiblioteca (esM());
                    rk->posaNomPreset (nom);
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
        juce::StringArray categoriesFabrica() override { juce::StringArray a; for (auto& c : categories (rk->tipusRack())) a.add (u8 (c)); return a; }
        juce::StringArray carpetesUsuari() override { juce::StringArray a; for (auto& c : biblioteca (esM()).carpetes()) a.add (u8 (c)); return a; }
        std::vector<EntradaPreset> entrades() override
        {
            std::vector<EntradaPreset> v;
            const auto tots = totsPresets();
            const int nu = (int) biblioteca (esM()).usuari.size();
            for (int i = 0; i < (int) tots.size(); ++i)
            {
                const auto& p = *tots[(size_t) i];
                // l'etiqueta: quants mòduls té (sense crear-los: es compten al text)
                int n = 0;
                for (size_t a = p.estat.find ("\"clau\""); a != std::string::npos; a = p.estat.find ("\"clau\"", a + 1)) ++n;
                const bool usuari = i < nu;
                v.push_back ({ u8 (p.nom), u8 (p.categoria), u8 (p.descripcio), juce::String (n) + (n == 1 ? " mòdul"_ca : " mòduls"_ca), usuari,
                               biblioteca (esM()).esFavorit (usuari ? BibliotecaPresets::clauUsuari (p.nom) : BibliotecaPresets::clauFabrica (p.nom)), i });
            }
            return v;
        }
        juce::String presetActual() override { return u8 (rk->nomPreset()); }
        void aplica (const EntradaPreset& e) override
        {
            const auto tots = totsPresets();
            if (e.index >= 0 && e.index < (int) tots.size()) aplicaPreset (*tots[(size_t) e.index]);
        }
        void commutaFavorit (const EntradaPreset& e) override
        {
            const auto n = e.nom.toStdString();
            biblioteca (esM()).commutaFavorit (e.usuari ? BibliotecaPresets::clauUsuari (n) : BibliotecaPresets::clauFabrica (n));
            desaBiblioteca (esM());
            refresca();
        }
        bool existeixMeu (const juce::String& nom) override { return biblioteca (esM()).busca (nom.toStdString()) != nullptr; }
        void reanomena (const juce::String& vell, const juce::String& nou) override
        {
            if (biblioteca (esM()).reanomena (vell.toStdString(), nou.toStdString()))
            {
                desaBiblioteca (esM());
                if (u8 (rk->nomPreset()) == vell) rk->posaNomPreset (nou.toStdString(), rk->presetModificat());
            }
            refresca();
        }
        void mou (const juce::String& nom, const juce::String& carpeta) override { biblioteca (esM()).mou (nom.toStdString(), carpeta.toStdString()); desaBiblioteca (esM()); }
        void esborra (const juce::String& nom) override { biblioteca (esM()).esborra (nom.toStdString()); desaBiblioteca (esM()); refresca(); }
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
        Boto btRetorn { "Afegeix un retorn"_ca, dibuix::Icona::Mes, Boto::Estil::Discontinu };   // 0.76
        Boto btPistes { "A pistes reals"_ca, std::nullopt, Boto::Estil::Primari };              // 0.78
        Boto btSolo { "S", std::nullopt, Boto::Estil::Tecla };                                     // 0.79
        Boto btTreuCadena { "Treu la cadena"_ca, std::nullopt, Boto::Estil::Secundari };
    public:
        std::function<int (int)> accioPistes;
    private:
        std::vector<std::unique_ptr<CapCami>> caps;
        std::vector<std::pair<juce::Component*, int>> ordre;
        friend class CapCami;
        std::map<const Modul*, std::unique_ptr<juce::DocumentWindow>> finestresModuls;
        juce::Rectangle<int> zonaMacros, zonaRack, zonaText, zonaVuIn, zonaVuOut;
        float vuIn = 0.0f, vuOut = 0.0f;
        unsigned versioVista = 0;
        unsigned tic = 0;
        std::unique_ptr<juce::Component> panellMaster;   // 0.94
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
        btAmunt.setEnabled (pare.veiDelCami (index, -1) >= 0);   // 0.76: dins del seu camí
        btAvall.setEnabled (pare.veiDelCami (index, 1) >= 0);
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
        g.setColour ((on ? (rk->esMaster() ? juce::Colour (0xffffb84a) : turquesa) : tema::suau).withAlpha (on ? 0.9f : 0.35f));
        g.fillRect (r.getX() + 16.0f, r.getY() + 8.0f, 3.0f, r.getHeight() - 16.0f);
        auto z = zonaNom.toFloat();
        g.setColour (on ? tema::text : tema::suau);
        g.setFont (dibuix::lletra (14.5f, true));
        g.drawText (juce::String (pare.ordreDinsCami (index)) + "  " + dibuix::majuscules (nomDe (*m)), z.removeFromTop (z.getHeight() * 0.5f), juce::Justification::bottomLeft);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.0f));
        juce::String desc;
        if (auto* t = busca (m->clau)) desc = u8 (t->descripcio);
        if (m->fx->latencia() > 0)   // 0.73: el retard de debò de cada mòdul (Limitador 5 ms, Harmonitzador i Octava uns 36 ms)
            desc = "Sempre al 100 % (retarda "_ca + juce::String (1000.0 * m->fx->latencia() / juce::jmax (8000.0, rk->frequencia()), 1).replaceCharacter ('.', ',')
                   + " ms, Mibase ho compensa; apagat, també, perquè no faci clics)."_ca;   // 1.12: amb la freqüència de debò
        // 0.94: avisos (en taronja) si el mòdul és en una zona perillosa
        const auto avis = avisModul (*m, rk->esMaster());
        if (avis.isNotEmpty()) { g.setColour (vintage::ledTaronja); desc = avis; }
        g.drawFittedText (desc, z.toNearestInt(), juce::Justification::topLeft, 1, 0.85f);
        // 0.94: quant actua (reducció de guany o canvi de volum) amb una barreta
        float quant = 0.0f;
        const auto act = activitatModul (*m, quant);
        if (act.isNotEmpty())
        {
            auto a = zonaAct.toFloat();
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (11.0f));
            g.drawText (act, a.removeFromTop (a.getHeight() * 0.55f), juce::Justification::centredRight);
            auto b = a.withSizeKeepingCentre (a.getWidth(), 5.0f);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillRoundedRectangle (b, 2.0f);
            g.setColour ((quant > 0.5f ? vintage::ledTaronja : (rk->esMaster() ? juce::Colour (0xffffb84a) : turquesa)).withAlpha (0.85f));
            g.fillRoundedRectangle (b.withWidth (b.getWidth() * quant), 2.0f);
        }
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
        zonaAct = r.removeFromRight (96).reduced (0, 10);   // 0.94
        r.removeFromRight (8);
        zonaNom = r;
    }

    void FilaModul::mouseDoubleClick (const juce::MouseEvent&) { pare.obreModul (index); }
    void FilaModul::mouseDown (const juce::MouseEvent& e) { if (e.mods.isPopupMenu()) pare.menuMouACami (index); }   // 0.76

    // ---- CapCami (0.76) ----
    CapCami::CapCami (PanellVocalStudio& p, std::shared_ptr<Rack> r, int c) : cami (c), pare (p), rk (std::move (r))
    {
        if (cami == 0) return;
        const int k = cami - 1;
        for (auto* b : { &btNom, &btMes, &btTreu }) addAndMakeVisible (b);
        btNom.setTooltip ("Clica per canviar el nom del retorn"_ca);
        btMes.setTooltip ("Afegeix un mòdul a aquest retorn"_ca);
        btTreu.setTooltip ("Treu el retorn i els seus mòduls"_ca);
        btNom.onClick = [this, k] { pare.canviaNomRetorn (k); };
        btMes.onClick = [this] { pare.menuAfegeix (cami, &btMes); };
        btTreu.onClick = [this, k] { juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PanellVocalStudio> (&pare), k] { if (sp != nullptr) sp->treuRetorn (k); }); };
        const int ps[4] { Envia1 + k, Retorn1 + k, PanRetorn1 + k, Ducking1 + k };
        const char* const noms[4] { "Envia", "Retorn", "Pan", "Ducking" };
        const char* const ajudes[4] { "Quant de la veu (després de la cadena principal) entra al retorn", "Quant se sent el retorn",
                                      "On sona el retorn (esquerra / dreta)", "Quant baixa el retorn mentre la veu sona (perquè la veu quedi clara)" };
        for (int q = 0; q < 4; ++q)
        {
            const int pp = ps[q];
            poms[(size_t) q] = std::make_unique<Pom> (u8 (noms[q]), 0.0, 1.0, (double) rk->parametre (pp), q == 2,
                                                      [this, pp] (double v) { return u8 (rk->textParametre (pp, (float) v)); });
            poms[(size_t) q]->setTooltip (u8 (ajudes[q]));
            poms[(size_t) q]->onCanvi = [this, pp] (double v) { rk->posaParametre (pp, (float) v); pare.haCanviat(); };
            poms[(size_t) q]->onReinicia = [this, pp] { float mn, mx, def; rangParam (pp, mn, mx, def); rk->posaValor (pp, def); refresca(); };
            addAndMakeVisible (*poms[(size_t) q]);
        }
        refresca();
    }
    void CapCami::refresca()
    {
        if (cami == 0) return;
        const int k = cami - 1;
        btNom.setButtonText ("RETORN "_ca + juce::String (cami) + " · " + dibuix::majuscules (u8 (rk->retorn (k).nom)));
        const int ps[4] { Envia1 + k, Retorn1 + k, PanRetorn1 + k, Ducking1 + k };
        for (int q = 0; q < 4; ++q) poms[(size_t) q]->setValor (rk->parametre (ps[q]), true);
        btMes.setEnabled (rk->numModuls() < maxModuls);
    }
    void CapCami::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        if (cami == 0)
        {
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (11.0f, true));
            g.drawText ("CADENA PRINCIPAL"_ca, r.reduced (4.0f, 0.0f), juce::Justification::bottomLeft);
            return;
        }
        g.setColour (turquesa.withAlpha (0.10f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (turquesa.withAlpha (0.55f));
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        g.fillRect (r.getX() + 8.0f, r.getBottom() - 3.0f, r.getWidth() - 16.0f, 1.0f);
    }
    void CapCami::resized()
    {
        if (cami == 0) return;
        auto r = getLocalBounds().reduced (10, 4);
        btTreu.setBounds (r.removeFromRight (28).withSizeKeepingCentre (26, 26));
        r.removeFromRight (4);
        btMes.setBounds (r.removeFromRight (90).withSizeKeepingCentre (90, 28));
        r.removeFromRight (8);
        for (int q = 3; q >= 0; --q) poms[(size_t) q]->setBounds (r.removeFromRight (64));
        r.removeFromRight (8);
        btNom.setBounds (r.withSizeKeepingCentre (r.getWidth(), 30));
    }

    class FinestraVocalStudio : public juce::DocumentWindow
    {
    public:
        FinestraVocalStudio (std::shared_ptr<Rack> r, const juce::String& titol, std::function<void()> enTancar, std::function<int (int)> accioPistes)
            : juce::DocumentWindow (titol, tema::fons, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
              tancar (std::move (enTancar))
        {
            setUsingNativeTitleBar (true);
            auto* p = new PanellVocalStudio (std::move (r), titol);
            p->accioPistes = std::move (accioPistes);
            setContentOwned (p, true);
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

std::unique_ptr<juce::DocumentWindow> creaFinestraVocalStudio (std::shared_ptr<Rack> r, const juce::String& titol, std::function<void()> enTancar,
                                                               std::function<int (int)> accioPistes)
{
    return std::make_unique<FinestraVocalStudio> (std::move (r), titol, std::move (enTancar), std::move (accioPistes));
}

std::unique_ptr<juce::DocumentWindow> creaFinestraTune (std::shared_ptr<mibase::tune::Tune> t, const juce::String& titol, std::function<void()> enTancar)
{
    return std::make_unique<FinestraSimple> (titol, new PanellTune (std::move (t)), std::move (enTancar));
}
