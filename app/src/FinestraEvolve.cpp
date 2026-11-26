#include "FinestraEvolve.h"

#include "Controls.h"
#include "DialegText.h"
#include "Dibuix.h"
#include "EscGlobal.h"
#include "NavegadorPresets.h"
#include "Pom.h"
#include "Tema.h"
#include "Text.h"
#include "Vintage.h"
#include <map>

using namespace mibase::evolve;

namespace
{
    juce::String u8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str(), (int) s.size()); }
    juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

    // Presets propis i favorits (Documents\Mibase\Evolve\presets.json)
    juce::File fitxerBiblioteca()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Mibase").getChildFile ("Evolve").getChildFile ("presets.json");
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
    std::map<const void*, juce::String>& catsTriades() { static std::map<const void*, juce::String> m; return m; }

    /** El color de cada mòdul (també a les marques del pom). */
    juce::Colour colorModul (int m)
    {
        static const juce::uint32 c[numModuls] = { 0xffff6a3d, 0xff4fc3d9, 0xffb18cff, 0xfff5c445 };
        return juce::Colour (c[juce::jlimit (0, numModuls - 1, m)]);
    }
    /** El color del pom segons l'Evolution: fred i suau a baix, calent i viu a dalt. */
    juce::Colour colorEvo (float e01)
    {
        const juce::Colour a (0xff59d0d6), b (0xffb18cff), c (0xffff8a2a), d (0xffff4a3a);
        if (e01 < 0.33f) return a.interpolatedWith (b, e01 / 0.33f);
        if (e01 < 0.7f) return b.interpolatedWith (c, (e01 - 0.33f) / 0.37f);
        return c.interpolatedWith (d, (e01 - 0.7f) / 0.3f);
    }
    juce::String zona (float e)
    {
        if (e <= 0.0f) return "SO ORIGINAL"_ca;
        if (e < 30.0f) return "SUBTIL"_ca;
        if (e < 60.0f) return "EVIDENT"_ca;
        if (e < 80.0f) return "INTENS"_ca;
        return "EXTREM"_ca;
    }
    /** On comença a sonar cada mòdul (el primer punt d'entrada de les seves rutes); -1 si no té rutes. */
    float entradaModul (const Evolve& e, int m)
    {
        float mn = -1.0f;
        for (const auto& r : e.rutes())
            if (modulDeParam (r.param) == m) mn = mn < 0.0f ? r.de : std::min (mn, r.de);
        if (mn < 0.0f && e.valor (paramMix (m)) > 0.0f) mn = 0.0f;   // ja sona a la base
        return mn;
    }

    // ---------------------------------------------------------------------------------------------
    /** El pom gran. */
    class DialEvolution : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        explicit DialEvolution (std::shared_ptr<Evolve> e) : ev (std::move (e))
        {
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
            setTooltip ("EVOLUTION: arrossega amunt o avall (Maj = fi), roda del ratolí, o doble clic per tornar al valor del preset."_ca);
        }
        std::function<void (float)> onCanvi;
        std::function<void()> onReinicia;
        float fase = 0.0f;   // animació (la posa el panell)

        void paint (juce::Graphics& g) override
        {
            const auto b = getLocalBounds().toFloat();
            const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 10.0f;
            const auto c = b.getCentre();
            // el que es veu: el valor del pom (o, amb Hold, el que sona retingut); el halo, el que sona de debò
            const bool hold = ev->valor (Hold) > 0.5f;
            const float v = ev->valor (Evolution), so = hold ? ev->mesures().evolution.load() : v;
            const float e01 = so / 100.0f;
            const auto col = colorEvo (e01);
            const float a0 = juce::MathConstants<float>::pi * -0.75f, a1 = juce::MathConstants<float>::pi * 0.75f;
            auto angle = [&] (float pc) { return a0 + (a1 - a0) * juce::jlimit (0.0f, 1.0f, pc / 100.0f); };
            const float nivell = juce::jlimit (0.0f, 1.0f, ev->mesures().sortida.load() * 1.6f);

            // halo: respira amb el so i creix amb l'Evolution
            if (so > 0.0f && ev->mesures().bypass.load() < 0.99f)
            {
                const float resp = 0.75f + 0.25f * std::sin (fase * (1.0f + 3.0f * e01));
                const float al = (0.10f + 0.35f * e01) * (0.5f + 0.5f * nivell) * resp;
                juce::ColourGradient gr (col.withAlpha (al), c.x, c.y, col.withAlpha (0.0f), c.x + R * 1.15f, c.y, true);
                g.setGradientFill (gr);
                g.fillEllipse (juce::Rectangle<float> (R * 2.3f, R * 2.3f).withCentre (c));
            }
            // pista de l'anell
            const float gruix = R * 0.11f, rAnell = R - gruix * 0.5f;
            juce::Path pista;
            pista.addCentredArc (c.x, c.y, rAnell, rAnell, 0.0f, a0, a1, true);
            g.setColour (juce::Colour (0xff101014));
            g.strokePath (pista, juce::PathStrokeType (gruix, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            // marques de zona (10, 30, 60, 80)
            for (float z : { 10.0f, 30.0f, 60.0f, 80.0f })
            {
                const float a = angle (z);
                const auto p1 = c.getPointOnCircumference (R + 2.0f, a), p2 = c.getPointOnCircumference (R + 8.0f, a);
                g.setColour (tema::linia2);
                g.drawLine ({ p1, p2 }, 1.5f);
            }
            // l'arc de valor (amb degradat de fred a calent)
            if (so > 0.05f)
            {
                const int trossos = juce::jmax (2, (int) (so * 0.8f));
                for (int k = 0; k < trossos; ++k)
                {
                    const float pa = so * (float) k / (float) trossos, pb = so * (float) (k + 1) / (float) trossos;
                    juce::Path t;
                    t.addCentredArc (c.x, c.y, rAnell, rAnell, 0.0f, angle (pa), angle (pb) + 0.01f, true);
                    g.setColour (colorEvo (pb / 100.0f));
                    g.strokePath (t, juce::PathStrokeType (gruix * 0.7f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
                }
            }
            // on entra cada mòdul
            const auto o = ev->ordre();
            for (int k = 0; k < numModuls; ++k)
            {
                const int m = o[(size_t) k];
                if (ev->valor (paramOn (m)) < 0.5f) continue;
                const float pc = entradaModul (*ev, m);
                if (pc < 0.0f) continue;
                const auto p = c.getPointOnCircumference (R + 14.0f, angle (pc));
                g.setColour (colorModul (m).withAlpha (so >= pc && so > 0.0f ? 1.0f : 0.45f));
                g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (p));
            }
            // el cos del pom
            const float rc = R * 0.74f;
            const auto cos = juce::Rectangle<float> (rc * 2.0f, rc * 2.0f).withCentre (c);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillEllipse (cos.translated (0.0f, rc * 0.06f).expanded (2.0f));
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3a44), c.x - rc * 0.5f, c.y - rc * 0.7f, juce::Colour (0xff141418), c.x + rc * 0.4f, c.y + rc, true));
            g.fillEllipse (cos);
            // estries
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            for (int k = 0; k < 48; ++k)
            {
                const float a = (float) k / 48.0f * juce::MathConstants<float>::twoPi;
                g.drawLine ({ c.getPointOnCircumference (rc * 0.9f, a), c.getPointOnCircumference (rc * 0.99f, a) }, 1.0f);
            }
            g.setColour (col.withAlpha (0.25f + 0.5f * e01));
            g.drawEllipse (cos.reduced (1.0f), 1.5f);
            // l'indicador (on és el pom: el que demana l'usuari)
            {
                const float a = angle (v);
                g.setColour (tema::text);
                g.drawLine ({ c.getPointOnCircumference (rc * 0.62f, a), c.getPointOnCircumference (rc * 0.9f, a) }, 3.0f);
            }
            // Hold: el valor retingut
            if (hold)
            {
                const auto p = c.getPointOnCircumference (rAnell, angle (so));
                g.setColour (tema::text);
                g.drawEllipse (juce::Rectangle<float> (gruix * 1.2f, gruix * 1.2f).withCentre (p), 2.0f);
            }
            // el text
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (rc * 0.42f, true));
            g.drawText (juce::String (juce::roundToInt (v)) + "%", cos.withSizeKeepingCentre (rc * 1.6f, rc * 0.5f).translated (0.0f, -rc * 0.08f),
                        juce::Justification::centred);
            g.setColour (so > 0.0f ? col : tema::suau);
            g.setFont (dibuix::lletra (juce::jmax (10.0f, rc * 0.12f), true));
            g.drawText (ev->valor (Hold) > 0.5f ? "HOLD · "_ca + juce::String (juce::roundToInt (so)) + "%" : zona (v),
                        cos.withSizeKeepingCentre (rc * 1.6f, rc * 0.2f).translated (0.0f, rc * 0.3f), juce::Justification::centred);
            g.setColour (tema::suau.withAlpha (0.7f));
            g.setFont (dibuix::lletra (juce::jmax (9.0f, rc * 0.085f), true));
            g.drawText ("EVOLUTION", cos.withSizeKeepingCentre (rc * 1.6f, rc * 0.2f).translated (0.0f, -rc * 0.45f), juce::Justification::centred);
        }

        void mouseDown (const juce::MouseEvent&) override { inici = ev->valor (Evolution); }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            const float sens = e.mods.isShiftDown() ? 0.08f : 0.4f;
            posa (inici - (float) e.getDistanceFromDragStartY() * sens);
        }
        void mouseDoubleClick (const juce::MouseEvent&) override { if (onReinicia) onReinicia(); }
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
        {
            posa (ev->valor (Evolution) + (w.deltaY > 0 ? 1.0f : -1.0f) * (e.mods.isShiftDown() ? 0.2f : 2.0f));
        }

    private:
        void posa (float v)
        {
            v = juce::jlimit (0.0f, 100.0f, v);
            ev->posa (Evolution, v);
            if (onCanvi) onCanvi (v);
            repaint();
        }
        std::shared_ptr<Evolve> ev;
        float inici = 0.0f;
    };

    // ---------------------------------------------------------------------------------------------
    class PanellEvolve : public juce::Component, private juce::Timer, public OrigenPresets
    {
    public:
        explicit PanellEvolve (std::shared_ptr<Evolve> e) : ev (std::move (e)), dial (ev)
        {
            for (auto* b : { &btAnt, &btSeg, &btPreset, &btDesa, &btCat, &btAtzar, &btReset, &btHold }) addAndMakeVisible (b);
            addAndMakeVisible (btFav);
            addAndMakeVisible (segAB);
            addAndMakeVisible (dial);
            btAnt.setTooltip ("Preset anterior (de la categoria triada)"_ca);
            btSeg.setTooltip ("Preset següent (de la categoria triada)"_ca);
            btPreset.setTooltip ("Clica per veure tots els presets"_ca);
            btFav.setTooltip ("Preferit"_ca);
            btDesa.setTooltip ("Desa els ajustos (amb la matriu d'Evolution) com un preset teu"_ca);
            btCat.setTooltip ("Categoria: les fletxes passen pels presets d'aquesta categoria"_ca);
            btAtzar.setTooltip ("Una combinació nova a l'atzar, musical i segura (mai satura). L'Evolution es queda on és."_ca);
            btReset.setTooltip ("Torna al preset tal com era (també l'Evolution)"_ca);
            btHold.setTooltip (u8 (info (Hold).ajuda));
            segAB.setTooltip ("Compara dues configuracions: A i B. En passar a B per primer cop, B és una còpia d'A."_ca);
            btHold.setClickingTogglesState (true);
            btHold.setLlum (tema::accent);
            btAnt.onClick = [this] { pas (-1); };
            btSeg.onClick = [this] { pas (1); };
            btPreset.onClick = [this] { obreNavegador(); };
            btFav.onClick = [this] { commutaFavoritActual(); };
            btDesa.onClick = [this] { desaPreset(); };
            btCat.onClick = [this] { triaCategoria(); };
            btAtzar.onClick = [this]
            {
                ev->posaEstat (ev->estatAtzar ((unsigned) juce::Random::getSystemRandom().nextInt()));
                nomsPreset()[ev.get()] = "(atzar)"_ca;
                refresca();
            };
            btReset.onClick = [this] { reinicia(); };
            btHold.onClick = [this] { ev->posa (Hold, btHold.getToggleState() ? 1.0f : 0.0f); refresca(); };
            segAB.onCanvi = [this] (int k) { canviaAB (k); };
            dial.onCanvi = [this] (float) { repaint(); };
            dial.onReinicia = [this]
            {
                if (const auto* p = presetActualPtr()) { Evolve t; t.posaEstat (p->estat); ev->posa (Evolution, t.valor (Evolution)); }
                else ev->posa (Evolution, info (Evolution).def);
                repaint();
            };
            for (int p : { (int) Entrada, (int) DryWet, (int) Sortida })
            {
                auto pom = std::make_unique<Pom> (u8 (info (p).nom), 0.0, 1.0, (double) Evolve::normal (p, info (p).def), false,
                                                  [this, p] (double v) { return u8 (ev->textParametre (p, (float) v)); });
                pom->setTooltip (u8 (info (p).ajuda));
                pom->onCanvi = [this, p] (double v) { ev->posaParametre (p, (float) v); };
                pom->onReinicia = [this, p] { ev->posa (p, info (p).def); refresca(); };
                addAndMakeVisible (*pom);
                poms.push_back ({ p, std::move (pom) });
            }
            setSize (900, 640);
            refresca();
            startTimerHz (30);
        }
        ~PanellEvolve() override { stopTimer(); }

        void refresca()
        {
            const auto nom = nomsPreset()[ev.get()];
            btPreset.setButtonText (nom.isEmpty() ? "(sense preset)"_ca : nom);
            btFav.setToggleState (esFavorit (nom), juce::dontSendNotification);
            const auto cat = catsTriades()[ev.get()];
            btCat.setButtonText (cat.isEmpty() ? "Totes"_ca : cat);
            btHold.setToggleState (ev->valor (Hold) > 0.5f, juce::dontSendNotification);
            for (auto& [p, pom] : poms) pom->setValor (ev->parametre (p), true);
            vist = ev->estat();
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (tema::fons);
            // fons amb una mica de textura (gra) i llum al centre
            {
                const auto r = getLocalBounds().toFloat();
                juce::ColourGradient gr (juce::Colour (0xff22222a), r.getCentreX(), r.getCentreY() * 0.9f, tema::fons, r.getCentreX(), r.getBottom(), true);
                g.setGradientFill (gr);
                g.fillRect (r);
                juce::Random rnd (113);
                g.setColour (juce::Colours::white.withAlpha (0.025f));
                for (int k = 0; k < 900; ++k) g.fillRect (rnd.nextFloat() * r.getWidth(), rnd.nextFloat() * r.getHeight(), 1.0f, 1.0f);
            }
            vintage::panell (g, getLocalBounds().removeFromTop (56).toFloat());
            auto t = getLocalBounds().removeFromTop (56).reduced (14, 9).removeFromLeft (130).toFloat();
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (20.0f, true));
            g.drawText ("EVOLVE", t.removeFromTop (24.0f), juce::Justification::centredLeft);
            g.setColour (tema::accent);
            g.setFont (dibuix::lletra (10.0f, true));
            g.drawText ("MIBASE", t, juce::Justification::centredLeft);

            // mesuradors d'entrada i sortida
            auto barra = [&] (juce::Rectangle<int> r, float pic, const juce::String& nom)
            {
                g.setColour (tema::suau);
                g.setFont (dibuix::lletra (10.0f, true));
                g.drawText (nom, r.removeFromBottom (14), juce::Justification::centred);
                const auto f = r.toFloat();
                g.setColour (juce::Colour (0xff0e0e12));
                g.fillRoundedRectangle (f, 3.0f);
                const float db = pic > 1e-5f ? 20.0f * std::log10 (pic) : -100.0f;
                const float h = juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
                g.setColour (db > -1.0f ? tema::error : tema::sinte);
                g.fillRoundedRectangle (f.withTop (f.getBottom() - f.getHeight() * h), 3.0f);
            };
            barra (mesIn, ev->mesures().entrada.load(), "IN");
            barra (mesOut, ev->mesures().sortida.load(), "OUT");
            if (ev->mesures().protegeix.load() > 0.3f)
            {
                g.setColour (tema::accent);
                g.setFont (dibuix::lletra (10.0f, true));
                g.drawText ("PROTECCIÓ"_ca, mesOut.withY (mesOut.getBottom() + 2).withHeight (14).expanded (30, 0), juce::Justification::centred);
            }

            // la cadena de mòduls
            const auto o = ev->ordre();
            const float so = ev->valor (Hold) > 0.5f ? ev->mesures().evolution.load() : ev->valor (Evolution);
            for (int k = 0; k < numModuls; ++k)
            {
                const int m = o[(size_t) k];
                const auto r = targetes[(size_t) k].toFloat();
                const bool on = ev->valor (paramOn (m)) > 0.5f;
                const float act = ev->mesures().activitat[(size_t) m].load();
                const auto col = colorModul (m);
                g.setColour (juce::Colour (0xff1c1c22));
                g.fillRoundedRectangle (r, 8.0f);
                g.setColour (on ? col.withAlpha (0.25f + 0.6f * act) : tema::linia);
                g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, on && act > 0.01f ? 2.0f : 1.0f);
                // fletxa de la cadena
                if (k + 1 < numModuls)
                {
                    const float y = r.getCentreY(), x0 = r.getRight() + 3.0f, x1 = targetes[(size_t) k + 1].getX() - 3.0f;
                    juce::Path p;
                    p.addArrow ({ x0, y, x1, y }, 1.5f, 7.0f, 6.0f);
                    g.setColour (tema::linia2);
                    g.fillPath (p);
                }
                auto in = r.reduced (10.0f, 8.0f);
                // llum d'encès
                const auto led = leds[(size_t) k].toFloat();
                g.setColour (on ? col : juce::Colour (0xff3a3a42));
                g.fillEllipse (led);
                if (on && act > 0.01f) { g.setColour (col.withAlpha (0.3f * act)); g.fillEllipse (led.expanded (4.0f)); }
                auto fila = in.removeFromTop (18.0f);
                fila.removeFromLeft (18.0f);
                g.setColour (on ? tema::text : tema::suau);
                g.setFont (dibuix::lletra (13.0f, true));
                g.drawText (nomModul (m), fila, juce::Justification::centredLeft);
                // què fa ara
                g.setColour (tema::suau);
                g.setFont (dibuix::lletra (11.0f));
                g.drawText (resum (m, so), in.removeFromTop (16.0f), juce::Justification::centredLeft);
                const float ent = entradaModul (*ev, m);
                g.drawText (! on ? "Apagat"_ca : ent < 0.0f ? "No entra amb Evolution"_ca : "Entra al "_ca + juce::String (juce::roundToInt (ent)) + " %",
                            in.removeFromTop (16.0f), juce::Justification::centredLeft);
                // barra d'activitat
                auto b = in.removeFromBottom (5.0f);
                g.setColour (juce::Colour (0xff0e0e12));
                g.fillRoundedRectangle (b, 2.5f);
                g.setColour (col);
                g.fillRoundedRectangle (b.withWidth (b.getWidth() * juce::jlimit (0.0f, 1.0f, act)), 2.5f);
            }
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (11.0f));
            g.drawText ("Clica la llum d'un mòdul per encendre'l o apagar-lo. El punt de color al voltant del pom diu on entra."_ca,
                        zonaAjuda, juce::Justification::centredLeft);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            auto dalt = r.removeFromTop (56).reduced (14, 12);
            dalt.removeFromLeft (130);
            segAB.setBounds (dalt.removeFromRight (84));
            dalt.removeFromRight (8);
            btHold.setBounds (dalt.removeFromRight (70));
            dalt.removeFromRight (6);
            btReset.setBounds (dalt.removeFromRight (76));
            dalt.removeFromRight (6);
            btAtzar.setBounds (dalt.removeFromRight (110));
            {
                auto pr = r.removeFromTop (36).reduced (14, 4);
                btCat.setBounds (pr.removeFromLeft (130));
                pr.removeFromLeft (8);
                btAnt.setBounds (pr.removeFromLeft (28));
                btPreset.setBounds (pr.removeFromLeft (juce::jmin (320, pr.getWidth() - 170)));
                btSeg.setBounds (pr.removeFromLeft (28));
                pr.removeFromLeft (6);
                btFav.setBounds (pr.removeFromLeft (26));
                pr.removeFromLeft (6);
                btDesa.setBounds (pr.removeFromLeft (80));
            }
            if (navegador != nullptr) navegador->setBounds (getLocalBounds());
            r.reduce (14, 8);
            zonaAjuda = r.removeFromBottom (20);
            // la cadena, a baix
            auto cad = r.removeFromBottom (juce::jlimit (84, 120, r.getHeight() / 5));
            r.removeFromBottom (8);
            const int gap = 22, w = (cad.getWidth() - gap * (numModuls - 1)) / numModuls;
            for (int k = 0; k < numModuls; ++k)
            {
                targetes[(size_t) k] = cad.removeFromLeft (w);
                cad.removeFromLeft (gap);
                leds[(size_t) k] = juce::Rectangle<int> (12, 12).withPosition (targetes[(size_t) k].getX() + 10, targetes[(size_t) k].getY() + 11);
            }
            // columnes dels costats i el pom al mig
            const int col = juce::jlimit (110, 170, r.getWidth() / 6);
            auto esq = r.removeFromLeft (col), dre = r.removeFromRight (col);
            auto pom = [&] (int p) -> Pom* { for (auto& [q, x] : poms) if (q == p) return x.get(); return nullptr; };
            const int hp = juce::jmin (120, esq.getHeight() / 2 - 10);
            pom (Entrada)->setBounds (esq.removeFromTop (hp));
            esq.removeFromTop (10);
            pom (DryWet)->setBounds (esq.removeFromTop (hp));
            pom (Sortida)->setBounds (dre.removeFromTop (hp));
            dre.removeFromTop (8);
            auto m = dre.removeFromTop (juce::jmin (150, dre.getHeight() - 20)).withSizeKeepingCentre (60, juce::jmin (150, dre.getHeight() - 20));
            mesIn = m.removeFromLeft (24);
            mesOut = m.removeFromRight (24);
            dial.setBounds (r);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const auto o = ev->ordre();
            for (int k = 0; k < numModuls; ++k)
                if (leds[(size_t) k].expanded (6).contains (e.getPosition()))
                {
                    const int p = paramOn (o[(size_t) k]);
                    ev->posa (p, ev->valor (p) > 0.5f ? 0.0f : 1.0f);
                    refresca();
                    return;
                }
        }

        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }

    private:
        juce::String resum (int m, float evo) const
        {
            auto v = [&] (int p) { return ev->efectiu (p, evo); };
            switch (m)
            {
                case MScreamer: return u8 (nomModeScreamer ((int) v (ScrMode))) + " · drive "_ca + juce::String (juce::roundToInt (v (ScrDrive))) + " %";
                case MFiltre: return u8 (nomTipusFiltre ((int) v (FilTipus))) + " · "_ca + u8 (textValor (FilFreq, v (FilFreq)))
                                     + (v (FilLfo) > 0.5f ? " · LFO "_ca + u8 (nomDivisio ((int) v (FilLfoDiv))) : juce::String());
                case MPumper: return u8 (nomModePumper ((int) v (PmpMode))) + " · "_ca + u8 (nomDivisio ((int) v (PmpDiv)))
                                     + ((int) v (PmpMode) == 2 ? " · "_ca + u8 (nomPatro ((int) v (PmpPatro))) : juce::String());
                default: return u8 (nomTipusShimmer ((int) v (ShmTipus))) + " " + u8 (textValor (ShmPitch, v (ShmPitch))) + " · "_ca + u8 (textValor (ShmDecay, v (ShmDecay)))
                                + (v (ShmFreeze) > 0.5f ? " · FREEZE"_ca : juce::String());
            }
        }

        void timerCallback() override
        {
            dial.fase += 0.1f;
            dial.repaint();
            repaint (mesIn.getUnion (mesOut).expanded (40, 20));
            for (auto& t : targetes) repaint (t);
            const auto ara = ev->estat();
            if (ara != vist) refresca();   // automatització o desfer
        }

        // ---- A/B ----
        void canviaAB (int k)
        {
            if (k == ab) return;
            slots[(size_t) ab] = ev->estat();
            noms[(size_t) ab] = nomsPreset()[ev.get()];
            if (slots[(size_t) k].empty()) { slots[(size_t) k] = slots[(size_t) ab]; noms[(size_t) k] = noms[(size_t) ab]; }
            ev->posaEstat (slots[(size_t) k]);
            nomsPreset()[ev.get()] = noms[(size_t) k];
            ab = k;
            refresca();
        }
        void reinicia()
        {
            if (const auto* p = presetActualPtr()) ev->posaEstat (p->estat);
            else { Evolve t; ev->posaEstat (t.estat()); }
            refresca();
        }

        // ---- presets (OrigenPresets), com al Drive ----
        std::vector<const Preset*> totsPresets()
        {
            std::vector<const Preset*> v;
            for (auto& p : biblioteca().usuari) v.push_back (&p);
            for (auto& p : presetsFabrica()) v.push_back (&p);
            return v;
        }
        const Preset* presetActualPtr()
        {
            const auto nom = nomsPreset()[ev.get()].toStdString();
            if (nom.empty()) return nullptr;
            for (const auto* p : totsPresets()) if (p->nom == nom) return p;
            return nullptr;
        }
        bool esFavorit (const juce::String& nom)
        {
            const auto n = nom.toStdString();
            return biblioteca().esFavorit (biblioteca().busca (n) != nullptr ? BibliotecaPresets::clauUsuari (n) : BibliotecaPresets::clauFabrica (n));
        }
        void commutaFavoritActual()
        {
            const auto nom = nomsPreset()[ev.get()].toStdString();
            if (nom.empty() || presetActualPtr() == nullptr) { btFav.setToggleState (false, juce::dontSendNotification); return; }
            biblioteca().commutaFavorit (biblioteca().busca (nom) != nullptr ? BibliotecaPresets::clauUsuari (nom) : BibliotecaPresets::clauFabrica (nom));
            desaBiblioteca();
            refresca();
        }
        void aplicaPreset (const Preset& p)
        {
            ev->posaEstat (p.estat);
            nomsPreset()[ev.get()] = u8 (p.nom);
            refresca();
        }
        void pas (int d)
        {
            std::vector<const Preset*> v;
            const auto cat = catsTriades()[ev.get()].toStdString();
            for (const auto* p : totsPresets()) if (cat.empty() || p->categoria == cat) v.push_back (p);
            if (v.empty()) return;
            const auto nom = nomsPreset()[ev.get()].toStdString();
            int i = -1;
            for (int k = 0; k < (int) v.size(); ++k) if (v[(size_t) k]->nom == nom) i = k;
            i = i < 0 ? (d > 0 ? 0 : (int) v.size() - 1) : (i + d + (int) v.size()) % (int) v.size();
            aplicaPreset (*v[(size_t) i]);
        }
        void triaCategoria()
        {
            juce::PopupMenu m;
            m.addItem (1, "Totes"_ca, true, catsTriades()[ev.get()].isEmpty());
            juce::StringArray cats;
            for (auto& c : categories()) cats.add (u8 (c));
            for (auto& c : biblioteca().carpetes()) if (! cats.contains (u8 (c))) cats.add (u8 (c));
            for (int i = 0; i < cats.size(); ++i) m.addItem (i + 2, cats[i], true, catsTriades()[ev.get()] == cats[i]);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btCat),
                [sp = juce::Component::SafePointer<PanellEvolve> (this), cats] (int r)
                {
                    if (sp == nullptr || r <= 0) return;
                    catsTriades()[sp->ev.get()] = r == 1 ? juce::String() : cats[r - 2];
                    if (r > 1)   // en triar una categoria, sona el primer preset
                        for (const auto* p : sp->totsPresets()) if (u8 (p->categoria) == cats[r - 2]) { sp->aplicaPreset (*p); break; }
                    sp->refresca();
                });
        }
        void desaPreset()
        {
            const auto actual = nomsPreset()[ev.get()];
            const bool meu = biblioteca().busca (actual.toStdString()) != nullptr;
            const auto proposta = meu ? actual : u8 (biblioteca().nomLliure ((actual.isEmpty() || actual == "(atzar)"_ca ? juce::String ("Evolve") : actual + " (meu)"_ca).toStdString()));
            dialeg = std::make_unique<DialegText> ("Desa el preset"_ca, "Nom del preset (si ja en tens un amb aquest nom, se substitueix)."_ca, "Desa"_ca,
                [] (const juce::String& t) { return t.trim().isEmpty() ? "Escriu un nom."_ca : juce::String(); },
                [this] (const juce::String& t)
                {
                    const auto nom = t.trim().toStdString();
                    const auto* ja = biblioteca().busca (nom);
                    biblioteca().desaPreset ({ nom, ja != nullptr ? ja->categoria : std::string ("Meus"), "", ev->estat() });
                    desaBiblioteca();
                    nomsPreset()[ev.get()] = u8 (nom);
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
                const bool usuari = i < nu;
                v.push_back ({ u8 (p.nom), u8 (p.categoria), u8 (p.descripcio), {}, usuari,
                               biblioteca().esFavorit (usuari ? BibliotecaPresets::clauUsuari (p.nom) : BibliotecaPresets::clauFabrica (p.nom)), i });
            }
            return v;
        }
        juce::String presetActual() override { return nomsPreset()[ev.get()]; }
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
                if (nomsPreset()[ev.get()] == vell) nomsPreset()[ev.get()] = nou;
            }
            refresca();
        }
        void mou (const juce::String& nom, const juce::String& carpeta) override { biblioteca().mou (nom.toStdString(), carpeta.toStdString()); desaBiblioteca(); }
        void esborra (const juce::String& nom) override { biblioteca().esborra (nom.toStdString()); desaBiblioteca(); refresca(); }
        void tancaNavegador() override
        {
            juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PanellEvolve> (this)] { if (sp != nullptr) { sp->navegador.reset(); sp->grabKeyboardFocus(); } });
        }

        std::shared_ptr<Evolve> ev;
        DialEvolution dial;
        std::unique_ptr<NavegadorPresetsComu> navegador;
        std::unique_ptr<DialegText> dialeg;
        Boto btAnt { {}, dibuix::Icona::Rebobina, Boto::Estil::Subtil }, btSeg { {}, dibuix::Icona::Endavant, Boto::Estil::Subtil };
        Boto btPreset { "(sense preset)"_ca, std::nullopt, Boto::Estil::Secundari };
        BotoEstrella btFav;
        Boto btDesa { "Desa", dibuix::Icona::Disc, Boto::Estil::Secundari };
        Boto btCat { "Totes"_ca, dibuix::Icona::FletxaAvall, Boto::Estil::Secundari };
        Boto btAtzar { "RANDOMIZE", std::nullopt, Boto::Estil::Secundari };
        Boto btReset { "RESET", std::nullopt, Boto::Estil::Secundari };
        Boto btHold { "HOLD", std::nullopt, Boto::Estil::Tecla };
        Segmentat segAB { { "A", "B" } };
        int ab = 0;
        std::array<std::string, 2> slots;
        std::array<juce::String, 2> noms;
        std::vector<std::pair<int, std::unique_ptr<Pom>>> poms;
        std::array<juce::Rectangle<int>, numModuls> targetes, leds;
        juce::Rectangle<int> mesIn, mesOut, zonaAjuda;
        std::string vist;
        juce::TooltipWindow tips { this, 500 };
    };

    class FinestraEvolve : public juce::DocumentWindow
    {
    public:
        FinestraEvolve (std::shared_ptr<Evolve> e, const juce::String& titol, std::function<void()> enTancar)
            : juce::DocumentWindow (titol, tema::fons, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
              tancar (std::move (enTancar))
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new PanellEvolve (std::move (e)), true);
            setResizable (true, true);
            setResizeLimits (760, 560, 1700, 1200);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::MessageManager::callAsync ([t = tancar] { t(); }); }
        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }
    private:
        std::function<void()> tancar;
    };
}

std::unique_ptr<juce::DocumentWindow> creaFinestraEvolve (std::shared_ptr<Evolve> e, const juce::String& titol, std::function<void()> enTancar)
{
    return std::make_unique<FinestraEvolve> (std::move (e), titol, std::move (enTancar));
}
