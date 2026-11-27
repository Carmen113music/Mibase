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
        // 1.15: abans es carregava amb el filtre del Drive i els presets propis d'EVOLVE no tornaven en obrir Mibase
        if (! carregada) { carregada = true; b.prefix = "mibase-evolve="; b.carrega (dibuix::cami (fitxerBiblioteca())); }
        return b;
    }
    void desaBiblioteca() { biblioteca().desa (dibuix::cami (fitxerBiblioteca())); }
    std::map<const void*, juce::String>& nomsPreset() { static std::map<const void*, juce::String> m; return m; }
    std::map<const void*, juce::String>& catsTriades() { static std::map<const void*, juce::String> m; return m; }

    /** El color de cada mòdul (també a les marques del pom). */
    juce::Colour colorModul (int m)
    {
        static const juce::uint32 c[numModuls] = { 0xffff6a3d, 0xff4fc3d9, 0xffb18cff, 0xfff5c445, 0xff6fa8ff, 0xff7fd67a, 0xffff5fa2, 0xff3fd0b0 };
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
    // 1.14: peces del mode ADVANCED

    /** Un paràmetre de valors discrets (tipus, ritme, patró…): clic → menú amb totes les opcions. */
    class SelectorParam : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        SelectorParam (std::shared_ptr<Evolve> e, int param) : ev (std::move (e)), p (param)
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
            setTooltip (u8 (info (p).ajuda));
        }
        std::function<void()> onCanvi;
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (10.5f, true));
            g.drawText (dibuix::majuscules (u8 (info (p).nom)), r.removeFromTop (14.0f), juce::Justification::centred);
            auto c = r.withSizeKeepingCentre (r.getWidth() - 4.0f, juce::jmin (26.0f, r.getHeight())).withY (r.getY() + 4.0f);
            g.setColour (juce::Colour (0xff0e0e12));
            g.fillRoundedRectangle (c, 5.0f);
            g.setColour (sobre ? tema::accent : tema::linia2);
            g.drawRoundedRectangle (c.reduced (0.5f), 5.0f, 1.0f);
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (12.0f, true));
            g.drawFittedText (u8 (textValor (p, ev->valor (p))) + "  " + juce::String::charToString (0x25BE), c.toNearestInt().reduced (4, 0), juce::Justification::centred, 1, 0.7f);
        }
        void mouseEnter (const juce::MouseEvent&) override { sobre = true; repaint(); }
        void mouseExit (const juce::MouseEvent&) override { sobre = false; repaint(); }
        void mouseUp (const juce::MouseEvent& e) override
        {
            if (! e.mouseWasClicked()) return;
            juce::PopupMenu m;
            const auto& in = info (p);
            const int actual = (int) std::lround (ev->valor (p));
            for (int v = (int) in.mn; v <= (int) in.mx; ++v) m.addItem (v - (int) in.mn + 1, u8 (textValor (p, (float) v)), true, v == actual);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [sp = juce::Component::SafePointer<SelectorParam> (this)] (int r)
            {
                if (sp == nullptr || r <= 0) return;
                sp->ev->posa (sp->p, info (sp->p).mn + (float) (r - 1));
                if (sp->onCanvi) sp->onCanvi();
                sp->repaint();
            });
        }
        int param() const { return p; }
    private:
        std::shared_ptr<Evolve> ev;
        int p;
        bool sobre = false;
    };

    /** Un pom d'un paràmetre d'EVOLVE (valor base). */
    std::unique_ptr<Pom> pomDe (std::shared_ptr<Evolve> ev, int p, const juce::String& nom = {})
    {
        const auto& in = info (p);
        auto pom = std::make_unique<Pom> (nom.isEmpty() ? u8 (in.nom) : nom, 0.0, 1.0, (double) Evolve::normal (p, in.def), in.mn < 0.0f && in.mx > 0.0f,
                                          [ev, p] (double v) { return u8 (ev->textParametre (p, (float) v)); });
        pom->setTooltip (u8 (in.ajuda));
        pom->onCanvi = [ev, p] (double v) { ev->posaParametre (p, (float) v); };
        pom->onReinicia = [ev, p] { ev->posa (p, info (p).def); };
        return pom;
    }

    /** 1.15: si l'Evolution o un LFO mouen aquest paràmetre, un anell de color al voltant del pom: del valor base al que sona ara. */
    bool esModulat (const Evolve& ev, int p)
    {
        for (const auto& r : ev.rutes()) if (r.param == p) return true;
        for (const auto& a : ev.assignacions()) if (a.param == p) return true;
        return false;
    }
    void dibuixaModulacio (juce::Graphics& g, const Evolve& ev, int p, juce::Rectangle<int> limits, juce::Colour col)
    {
        if (ev.mesures().bypass.load() > 0.99f || ! esModulat (ev, p)) return;
        auto r = limits.toFloat();
        r.removeFromTop (14.0f);
        r.removeFromBottom (18.0f);
        const auto c = r.withSizeKeepingCentre (juce::jmin (r.getWidth(), r.getHeight()), juce::jmin (r.getWidth(), r.getHeight()));
        const float radi = c.getWidth() * 0.5f + 2.0f;
        if (radi < 6.0f) return;
        const float pi = juce::MathConstants<float>::pi;
        const float b = Evolve::normal (p, ev.valor (p)), e = Evolve::normal (p, ev.mesures().efectiu[(size_t) p].load());
        auto angle = [&] (float t) { return -0.75f * pi + t * 1.5f * pi; };
        juce::Path arc;
        arc.addCentredArc (c.getCentreX(), c.getCentreY(), radi, radi, 0.0f, angle (juce::jmin (b, e)), angle (juce::jmax (b, e)) + 0.001f, true);
        g.setColour (col.withAlpha (0.85f));
        g.strokePath (arc, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col);
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (c.getCentre().getPointOnCircumference (radi, angle (e))));
    }

    /** Els controls del mòdul triat: tots els seus paràmetres, i el nivell d'entrada i de sortida. */
    class EditorModul : public juce::Component
    {
    public:
        EditorModul (std::shared_ptr<Evolve> e, int modul) : ev (std::move (e)), m (modul)
        {
            for (int p = primerParam (m) + 1; p < finalParam (m); ++p) afegeix (p);
            afegeix (NivIn + m);
            afegeix (NivOut + m);
        }
        void refresca()
        {
            for (auto& [p, pom] : poms) pom->setValor (ev->parametre (p), true);
            for (auto& s : sels) s->repaint();
        }
        void paintOverChildren (juce::Graphics& g) override
        {
            for (auto& [p, pom] : poms) dibuixaModulacio (g, *ev, p, pom->getBounds(), colorModul (m));
        }
        void paint (juce::Graphics& g) override
        {
            const auto col = colorModul (m);
            auto r = getLocalBounds().toFloat();
            g.setColour (juce::Colour (0xff1c1c22));
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (col.withAlpha (0.6f));
            g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.5f);
            auto cap = r.reduced (12.0f, 8.0f).removeFromTop (20.0f);
            g.setColour (col);
            g.setFont (dibuix::lletra (15.0f, true));
            g.drawText (nomModul (m), cap, juce::Justification::centredLeft);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (11.5f));
            g.drawText (u8 (descripcioModul (m)), cap.withTrimmedLeft (110.0f), juce::Justification::centredLeft);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (10, 6);
            r.removeFromTop (26);
            const int n = (int) ordre.size();
            const int perFila = juce::jmax (4, juce::jmin (n, r.getWidth() / 84));
            const int files = (n + perFila - 1) / perFila;
            const int w = r.getWidth() / perFila, h = juce::jmin (96, r.getHeight() / juce::jmax (1, files));
            for (int i = 0; i < n; ++i)
                ordre[(size_t) i]->setBounds (r.getX() + (i % perFila) * w, r.getY() + (i / perFila) * h, w - 4, h - 4);
        }
    private:
        void afegeix (int p)
        {
            if (info (p).enter)
            {
                auto s = std::make_unique<SelectorParam> (ev, p);
                addAndMakeVisible (*s);
                ordre.push_back (s.get());
                sels.push_back (std::move (s));
            }
            else
            {
                const bool esNivell = p >= NivIn && p < (int) NivOut + 8;
                auto pom = pomDe (ev, p, esNivell ? (p < NivOut ? "In"_ca : "Out"_ca) : juce::String());
                addAndMakeVisible (*pom);
                ordre.push_back (pom.get());
                poms.push_back ({ p, std::move (pom) });
            }
        }
        std::shared_ptr<Evolve> ev;
        int m;
        std::vector<std::pair<int, std::unique_ptr<Pom>>> poms;
        std::vector<std::unique_ptr<SelectorParam>> sels;
        std::vector<juce::Component*> ordre;
    };

    /** El tram d'una ruta (de 0 a 100 % d'Evolution): s'arrosseguen les vores o tot el tram. Hi surt la corba i on és l'Evolution. */
    class BarraRang : public juce::Component
    {
    public:
        std::function<void (float, float)> onCanvi;
        float de = 0.0f, a = 100.0f, evo = 0.0f;
        int corba = Lineal;
        juce::Colour col = tema::accent;
        void paint (juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat().reduced (2.0f, 3.0f);
            g.setColour (juce::Colour (0xff0e0e12));
            g.fillRoundedRectangle (r, 4.0f);
            for (float z : { 30.0f, 60.0f, 80.0f })
            {
                g.setColour (tema::linia);
                g.drawVerticalLine ((int) x (z, r), r.getY(), r.getBottom());
            }
            // la corba dins del tram (alçada = quant s'ha mogut el paràmetre)
            juce::Path p;
            p.startNewSubPath (r.getX(), r.getBottom());
            p.lineTo (x (de, r), r.getBottom());
            for (int k = 0; k <= 24; ++k)
            {
                const float t = (float) k / 24.0f;
                p.lineTo (x (de + (a - de) * t, r), r.getBottom() - aplicaCorba (corba, t) * r.getHeight());
            }
            p.lineTo (r.getRight(), r.getY());
            g.setColour (col.withAlpha (0.18f));
            juce::Path ple (p); ple.lineTo (r.getRight(), r.getBottom()); ple.closeSubPath();
            g.fillPath (ple);
            g.setColour (col);
            g.strokePath (p, juce::PathStrokeType (1.6f));
            for (float v : { de, a })
            {
                g.setColour (tema::text);
                g.fillRoundedRectangle (juce::Rectangle<float> (5.0f, r.getHeight() + 2.0f).withCentre ({ x (v, r), r.getCentreY() }), 2.0f);
            }
            g.setColour (tema::text.withAlpha (0.8f));
            g.drawVerticalLine ((int) x (evo, r), r.getY() - 2.0f, r.getBottom() + 2.0f);
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            const auto r = getLocalBounds().toFloat().reduced (2.0f, 3.0f);
            const float px = (float) e.x;
            agafa = std::fabs (px - x (de, r)) < 8.0f ? 1 : std::fabs (px - x (a, r)) < 8.0f ? 2 : 3;
            de0 = de; a0 = a;
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            const auto r = getLocalBounds().toFloat().reduced (2.0f, 3.0f);
            const float d = (float) e.getDistanceFromDragStartX() / r.getWidth() * 100.0f;
            if (agafa == 1) de = juce::jlimit (0.0f, a, de0 + d);
            else if (agafa == 2) a = juce::jlimit (de, 100.0f, a0 + d);
            else { const float dd = juce::jlimit (-de0, 100.0f - a0, d); de = de0 + dd; a = a0 + dd; }
            de = std::round (de); a = std::round (a);
            if (onCanvi) onCanvi (de, a);
            repaint();
        }
    private:
        static float x (float v, juce::Rectangle<float> r) { return r.getX() + r.getWidth() * v / 100.0f; }
        int agafa = 0;
        float de0 = 0.0f, a0 = 0.0f;
    };

    /** Un menú amb tots els paràmetres que es poden moure, agrupats per mòdul. Torna (per «fes») l'índex triat. */
    void menuParams (juce::Component* on, std::function<void (int)> fes, bool ambLfo)
    {
        juce::PopupMenu m, glob;
        for (int p : { (int) DryWet, (int) Entrada, (int) Sortida }) glob.addItem (p + 1, u8 (info (p).nom));
        m.addSubMenu ("Global", glob);
        for (int mod = 0; mod < numModuls; ++mod)
        {
            juce::PopupMenu s;
            for (int p = primerParam (mod); p < finalParam (mod); ++p) if (esModulable (p)) s.addItem (p + 1, u8 (info (p).nom));
            s.addItem (NivIn + mod + 1, "In");
            s.addItem (NivOut + mod + 1, "Out");
            m.addSubMenu (nomModul (mod), s);
        }
        if (ambLfo)
            for (int k = 0; k < numLfos; ++k)
            {
                juce::PopupMenu s;
                for (int j = 0; j < paramsPerLfo; ++j) { const int p = paramLfo (k, j); if (esModulable (p)) s.addItem (p + 1, u8 (info (p).nom)); }
                m.addSubMenu ("LFO " + juce::String (k + 1), s);
            }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (on), [fes] (int r) { if (r > 0) fes (r - 1); });
    }
    juce::String nomParam (int p)
    {
        const int m = modulDeParam (p);
        if (m >= 0) return juce::String (nomModul (m)) + " · " + u8 (info (p).nom);
        if (p >= L1Forma) return "LFO " + juce::String ((p - L1Forma) / paramsPerLfo + 1) + " · " + u8 (info (p).nom);
        return u8 (info (p).nom);
    }
    juce::Colour colorParam (int p) { const int m = modulDeParam (p); return m >= 0 ? colorModul (m) : tema::accent; }

    /** La matriu d'evolució: una fila per ruta. */
    class EditorMatriu : public juce::Component
    {
    public:
        EditorMatriu (std::shared_ptr<Evolve> e) : ev (std::move (e))
        {
            addAndMakeVisible (vista);
            vista.setViewedComponent (&cos, false);
            vista.setScrollBarsShown (true, false);
            addAndMakeVisible (btAfegeix);
            btAfegeix.setTooltip ("Una ruta nova: tria quin paràmetre ha de moure l'Evolution"_ca);
            btAfegeix.onClick = [this]
            {
                menuParams (&btAfegeix, [this] (int p)
                {
                    auto v = ev->rutes();
                    v.push_back ({ p, 0.0f, 100.0f, info (p).mx, Lineal });
                    ev->posaRutes (v);
                    torna();
                }, true);
            };
            torna();
        }
        std::function<int()> modulTriat;
        void torna()
        {
            files.clear();
            const auto v = ev->rutes();
            for (int i = 0; i < (int) v.size(); ++i)
            {
                auto f = std::make_unique<Fila> (*this, i, v[(size_t) i]);
                cos.addAndMakeVisible (*f);
                files.push_back (std::move (f));
            }
            signatura = sig();
            resized();
        }
        void refresca()
        {
            if (sig() != signatura) { torna(); return; }
            const auto v = ev->rutes();
            const float evo = ev->valor (Hold) > 0.5f ? ev->mesures().evolution.load() : ev->valor (Evolution);
            for (int i = 0; i < (int) files.size() && i < (int) v.size(); ++i) files[(size_t) i]->actualitza (v[(size_t) i], evo);
        }
        void resized() override
        {
            auto r = getLocalBounds();
            auto dalt = r.removeFromTop (30);
            btAfegeix.setBounds (dalt.removeFromRight (120).reduced (0, 3));
            capcalera = dalt;
            vista.setBounds (r);
            const int h = 40;
            cos.setSize (r.getWidth() - 10, juce::jmax (r.getHeight(), (int) files.size() * h));
            for (int i = 0; i < (int) files.size(); ++i) files[(size_t) i]->setBounds (0, i * h, cos.getWidth(), h - 2);
        }
        void paint (juce::Graphics& g) override
        {
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (11.0f, true));
            auto c = capcalera;
            g.drawText ("PARÀMETRE"_ca, c.removeFromLeft (190).withTrimmedLeft (4), juce::Justification::centredLeft);
            g.drawText ("TRAM (0 → 100 %) I CORBA"_ca, c.removeFromLeft (juce::jmax (120, c.getWidth() - 250)), juce::Justification::centredLeft);
            g.drawText ("VALOR A DALT"_ca, c, juce::Justification::centredLeft);
            if (files.empty())
            {
                g.setFont (dibuix::lletra (12.0f));
                g.drawText ("Encara no hi ha rutes: l'Evolution no mou res. «+ Ruta» per afegir-ne."_ca, vista.getBounds(), juce::Justification::centred);
            }
        }
    private:
        struct Fila : public juce::Component
        {
            Fila (EditorMatriu& em, int index, const Ruta& r) : pare (em), i (index)
            {
                addAndMakeVisible (btParam);
                addAndMakeVisible (barra);
                addAndMakeVisible (btCorba);
                addAndMakeVisible (btTreu);
                btParam.setButtonText (nomParam (r.param));
                btParam.setTooltip ("Canvia el paràmetre"_ca);
                btParam.onClick = [this] { menuParams (&btParam, [this] (int p) { canvia ([p] (Ruta& x) { x.param = p; x.valor = info (p).mx; }); pare.torna(); }, true); };
                barra.col = colorParam (r.param);
                barra.onCanvi = [this] (float de, float a) { canvia ([de, a] (Ruta& x) { x.de = de; x.a = a; }); };
                btCorba.setTooltip ("Corba: lineal, suau (entra a poc a poc), ràpida (entra de cop) o en S"_ca);
                btCorba.onClick = [this] { canvia ([] (Ruta& x) { x.corba = (x.corba + 1) % numCorbes; }); };
                btTreu.setTooltip ("Treu la ruta"_ca);
                btTreu.onClick = [this]
                {
                    auto v = pare.ev->rutes();
                    if (i < (int) v.size()) v.erase (v.begin() + i);
                    pare.ev->posaRutes (v);
                    juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<EditorMatriu> (&pare)] { if (sp != nullptr) sp->torna(); });
                };
                const int p = r.param;
                valor = std::make_unique<Pom> ("", 0.0, 1.0, (double) Evolve::normal (p, r.valor), false, [p] (double v) { return u8 (textValor (p, Evolve::deNormal (p, (float) v))); });
                valor->onCanvi = [this, p] (double v) { canvia ([p, v] (Ruta& x) { x.valor = Evolve::deNormal (p, (float) v); }); };
                valor->setTooltip ("El valor del paràmetre quan l'Evolution arriba al final del tram (a l'inici, el valor base del mòdul)"_ca);
                addAndMakeVisible (*valor);
                actualitza (r, 0.0f);
            }
            void actualitza (const Ruta& r, float evo)
            {
                barra.de = r.de; barra.a = r.a; barra.corba = r.corba; barra.evo = evo;
                barra.repaint();
                btCorba.setButtonText (u8 (nomCorba (r.corba)));
                valor->setValor (Evolve::normal (r.param, r.valor), true);
            }
            void canvia (std::function<void (Ruta&)> f)
            {
                auto v = pare.ev->rutes();
                if (i >= (int) v.size()) return;
                f (v[(size_t) i]);
                pare.ev->posaRutes (v);
            }
            void resized() override
            {
                auto r = getLocalBounds();
                btParam.setBounds (r.removeFromLeft (186).reduced (2, 3));
                btTreu.setBounds (r.removeFromRight (30).reduced (2, 4));
                btCorba.setBounds (r.removeFromRight (74).reduced (2, 4));
                valor->setBounds (r.removeFromRight (140).withTrimmedTop (-14));   // el pom: només el valor (sense nom)
                barra.setBounds (r.reduced (4, 2));
            }
            EditorMatriu& pare;
            int i;
            Boto btParam { "", std::nullopt, Boto::Estil::Subtil };
            BarraRang barra;
            Boto btCorba { "", std::nullopt, Boto::Estil::Secundari };
            Boto btTreu { "", dibuix::Icona::Tanca, Boto::Estil::Subtil };
            std::unique_ptr<Pom> valor;
        };
        std::string sig() const
        {
            std::string s;
            for (const auto& r : ev->rutes()) s += std::to_string (r.param) + ",";
            return s;
        }
        std::shared_ptr<Evolve> ev;
        juce::Viewport vista;
        juce::Component cos;
        std::vector<std::unique_ptr<Fila>> files;
        Boto btAfegeix { "+ Ruta", std::nullopt, Boto::Estil::Secundari };
        juce::Rectangle<int> capcalera;
        std::string signatura;
    };

    /** Un LFO: forma, tempo o Hz, profunditat, fase i offset, el dibuix de l'ona i a què està assignat. */
    class EditorLfo : public juce::Component
    {
    public:
        EditorLfo (std::shared_ptr<Evolve> e, int lfo) : ev (std::move (e)), k (lfo)
        {
            for (int j : { 0, 1, 3 }) { auto s = std::make_unique<SelectorParam> (ev, paramLfo (k, j)); addAndMakeVisible (*s); sels.push_back (std::move (s)); }
            for (int j : { 2, 4, 5, 6 })
            {
                auto pom = pomDe (ev, paramLfo (k, j));
                addAndMakeVisible (*pom);
                poms.push_back ({ paramLfo (k, j), std::move (pom) });
            }
            addAndMakeVisible (btAssigna);
            btAssigna.setTooltip ("Tria un paràmetre perquè aquest LFO el mogui"_ca);
            btAssigna.onClick = [this]
            {
                menuParams (&btAssigna, [this] (int p)
                {
                    auto v = ev->assignacions();
                    v.push_back ({ k, p, 30.0f });
                    ev->posaAssignacions (v);
                    torna();
                }, false);
            };
            torna();
        }
        void torna()
        {
            files.clear();
            const auto v = ev->assignacions();
            for (int i = 0; i < (int) v.size(); ++i)
                if (v[(size_t) i].lfo == k)
                {
                    auto f = std::make_unique<FilaAs> (*this, i, v[(size_t) i]);
                    addAndMakeVisible (*f);
                    files.push_back (std::move (f));
                }
            signatura = sig();
            resized();
        }
        void refresca()
        {
            if (sig() != signatura) { torna(); return; }
            for (auto& [p, pom] : poms) pom->setValor (ev->parametre (p), true);
            const bool sync = ev->valor (paramLfo (k, 1)) > 0.5f;
            for (auto& [p, pom] : poms) if (p == paramLfo (k, 2)) pom->setEnabled (! sync);
            for (auto& s : sels) { s->setEnabled (s->param() != paramLfo (k, 3) || sync); s->repaint(); }
            const auto v = ev->assignacions();
            for (auto& f : files) if (f->i < (int) v.size()) f->valor->setValor ((v[(size_t) f->i].quant + 100.0) / 200.0, true);
            repaint (ona);
        }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (juce::Colour (0xff1c1c22));
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (tema::linia);
            g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
            g.setColour (tema::accent);
            g.setFont (dibuix::lletra (14.0f, true));
            g.drawText ("LFO " + juce::String (k + 1), r.reduced (12.0f, 8.0f).removeFromTop (18.0f), juce::Justification::centredLeft);
            // l'ona, amb el punt d'on és ara
            const auto o = ona.toFloat();
            g.setColour (juce::Colour (0xff0e0e12));
            g.fillRoundedRectangle (o, 5.0f);
            const int forma = (int) ev->valor (paramLfo (k, 0));
            const float dep = ev->valor (paramLfo (k, 4)) * 0.01f, off = ev->valor (paramLfo (k, 6)) * 0.01f;
            juce::Path p;
            for (int i = 0; i <= 120; ++i)
            {
                const float f = (float) i / 120.0f;
                const float v = juce::jlimit (-1.0f, 1.0f, formaLfo (forma, f, (long long) (f * 3.0f)) * dep + off);
                const float xx = o.getX() + f * o.getWidth(), yy = o.getCentreY() - v * o.getHeight() * 0.42f;
                if (i == 0) p.startNewSubPath (xx, yy); else p.lineTo (xx, yy);
            }
            g.setColour (tema::accent.withAlpha (0.9f));
            g.strokePath (p, juce::PathStrokeType (1.6f));
            const float fa = ev->mesures().faseLfo[(size_t) k].load(), va = ev->mesures().lfo[(size_t) k].load();
            g.setColour (tema::text);
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ o.getX() + fa * o.getWidth(), o.getCentreY() - va * o.getHeight() * 0.42f }));
            if (files.empty())
            {
                g.setColour (tema::suau);
                g.setFont (dibuix::lletra (11.5f));
                g.drawText ("No mou res: «+ Assigna»."_ca, zonaFiles, juce::Justification::centredLeft);
            }
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (10, 8);
            auto cap = r.removeFromTop (20);
            btAssigna.setBounds (cap.removeFromRight (110));
            r.removeFromTop (4);
            auto fila = r.removeFromTop (juce::jmin (84, r.getHeight() / 2));
            ona = fila.removeFromLeft (juce::jmin (170, fila.getWidth() / 3)).reduced (0, 6);
            fila.removeFromLeft (6);
            const int n = (int) (sels.size() + poms.size());
            const int w = fila.getWidth() / juce::jmax (1, n);
            int x = fila.getX();
            for (auto& s : sels) { s->setBounds (x, fila.getY(), w - 2, juce::jmin (52, fila.getHeight())); x += w; }
            for (auto& [p, pom] : poms) { pom->setBounds (x, fila.getY(), w - 2, fila.getHeight()); x += w; }
            r.removeFromTop (6);
            zonaFiles = r;
            int y = r.getY();
            for (auto& f : files) { f->setBounds (r.getX(), y, r.getWidth(), 26); y += 28; }
        }
    private:
        struct FilaAs : public juce::Component
        {
            FilaAs (EditorLfo& el, int index, const Assignacio& a) : pare (el), i (index)
            {
                nom = nomParam (a.param);
                col = colorParam (a.param);
                valor = std::make_unique<Pom> ("", 0.0, 1.0, 0.5, true, [] (double v) { return juce::String (juce::roundToInt (v * 200.0 - 100.0)) + " %"; });
                valor->onCanvi = [this] (double v)
                {
                    auto w = pare.ev->assignacions();
                    if (i < (int) w.size()) { w[(size_t) i].quant = (float) (v * 200.0 - 100.0); pare.ev->posaAssignacions (w); }
                };
                valor->setTooltip ("Quant el mou (i cap a on): % del recorregut del paràmetre"_ca);
                valor->setValor ((a.quant + 100.0) / 200.0, true);
                addAndMakeVisible (*valor);
                addAndMakeVisible (btTreu);
                btTreu.onClick = [this]
                {
                    auto w = pare.ev->assignacions();
                    if (i < (int) w.size()) w.erase (w.begin() + i);
                    pare.ev->posaAssignacions (w);
                    juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<EditorLfo> (&pare)] { if (sp != nullptr) sp->torna(); });
                };
            }
            void paint (juce::Graphics& g) override
            {
                g.setColour (col);
                g.fillEllipse (4.0f, getHeight() * 0.5f - 4.0f, 8.0f, 8.0f);
                g.setColour (tema::text);
                g.setFont (dibuix::lletra (12.0f));
                g.drawText (nom, getLocalBounds().withTrimmedLeft (18).withWidth (getWidth() - 170), juce::Justification::centredLeft);
            }
            void resized() override
            {
                auto r = getLocalBounds();
                btTreu.setBounds (r.removeFromRight (26).reduced (2));
                valor->setBounds (r.removeFromRight (130).withTrimmedTop (-14));
            }
            EditorLfo& pare;
            int i;
            juce::String nom;
            juce::Colour col;
            std::unique_ptr<Pom> valor;
            Boto btTreu { "", dibuix::Icona::Tanca, Boto::Estil::Subtil };
        };
        std::string sig() const
        {
            std::string s;
            for (const auto& a : ev->assignacions()) s += std::to_string (a.lfo) + ":" + std::to_string (a.param) + ",";
            return s;
        }
        std::shared_ptr<Evolve> ev;
        int k;
        std::vector<std::unique_ptr<SelectorParam>> sels;
        std::vector<std::pair<int, std::unique_ptr<Pom>>> poms;
        std::vector<std::unique_ptr<FilaAs>> files;
        Boto btAssigna { "+ Assigna", std::nullopt, Boto::Estil::Secundari };
        juce::Rectangle<int> ona, zonaFiles;
        std::string signatura;
    };

    // ---------------------------------------------------------------------------------------------
    class PanellEvolve : public juce::Component, private juce::Timer, public OrigenPresets
    {
    public:
        explicit PanellEvolve (std::shared_ptr<Evolve> e) : ev (std::move (e)), dial (ev)
        {
            for (auto* b : { &btAnt, &btSeg, &btPreset, &btDesa, &btCat, &btAtzar, &btReset, &btHold, &btFitxer }) addAndMakeVisible (b);
            btFitxer.setTooltip ("Exporta presets a un fitxer (.evolve) per passar-los a un altre ordinador, o importa'n"_ca);
            btFitxer.onClick = [this] { menuFitxer(); };
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
            // 1.14: SIMPLE / ADVANCED
            addAndMakeVisible (segVista);
            segVista.setTooltip ("SIMPLE: el pom gran. ADVANCED: cada mòdul, l'ordre de la cadena, la matriu d'Evolution i els LFO."_ca);
            segVista.onCanvi = [this] (int k) { ev->posa (Vista, (float) k); canviaVista (true); };
            addChildComponent (segPest);
            segPest.onCanvi = [this] (int) { mostraPestanya(); };
            setSize (980, 640);
            canviaVista (false);
            refresca();
            startTimerHz (30);
        }
        ~PanellEvolve() override { stopTimer(); ev->posaSolo (-1); }   // el solo no es queda mai encès sense la finestra

        void refresca()
        {
            const auto nom = nomsPreset()[ev.get()];
            btPreset.setButtonText (nom.isEmpty() ? "(sense preset)"_ca : nom);
            btFav.setToggleState (esFavorit (nom), juce::dontSendNotification);
            const auto cat = catsTriades()[ev.get()];
            btCat.setButtonText (cat.isEmpty() ? "Totes"_ca : cat);
            btHold.setToggleState (ev->valor (Hold) > 0.5f, juce::dontSendNotification);
            for (auto& [p, pom] : poms) pom->setValor (ev->parametre (p), true);
            segVista.setTriat (avancat() ? 1 : 0);
            if (avancat() != vistaAra) canviaVista (false);
            if (editor != nullptr) editor->refresca();
            if (matriu != nullptr) matriu->refresca();
            for (auto& l : lfos) if (l != nullptr) l->refresca();
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
            if (avancat()) { pintaCadena (g, o); return; }
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
                g.drawFittedText (resum (m, so), in.removeFromTop (16.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.75f);
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

        void paintOverChildren (juce::Graphics& g) override
        {
            for (auto& [p, pom] : poms) dibuixaModulacio (g, *ev, p, pom->getBounds(), tema::accent);
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
            dalt.removeFromRight (10);
            segVista.setBounds (dalt.removeFromRight (180));
            {
                auto pr = r.removeFromTop (36).reduced (14, 4);
                btCat.setBounds (pr.removeFromLeft (130));
                pr.removeFromLeft (8);
                btAnt.setBounds (pr.removeFromLeft (28));
                btPreset.setBounds (pr.removeFromLeft (juce::jmin (320, pr.getWidth() - 270)));
                btSeg.setBounds (pr.removeFromLeft (28));
                pr.removeFromLeft (6);
                btFav.setBounds (pr.removeFromLeft (26));
                pr.removeFromLeft (6);
                btDesa.setBounds (pr.removeFromLeft (80));
                pr.removeFromLeft (6);
                btFitxer.setBounds (pr.removeFromLeft (90));
            }
            if (navegador != nullptr) navegador->setBounds (getLocalBounds());
            r.reduce (14, 8);
            auto pom = [&] (int p) -> Pom* { for (auto& [q, x] : poms) if (q == p) return x.get(); return nullptr; };
            if (avancat())
            {
                // ADVANCED: la cadena a l'esquerra, el mòdul triat al mig (i a sota, la matriu o els LFO), el pom a la dreta
                auto esq = r.removeFromLeft (220);
                r.removeFromLeft (10);
                auto dre = r.removeFromRight (210);
                r.removeFromRight (10);
                zonaCadena = esq;
                const int hf = juce::jmin (52, (esq.getHeight() - 30) / numModuls);
                esq.removeFromTop (22);
                for (int k = 0; k < numModuls; ++k)
                {
                    files[(size_t) k] = esq.removeFromTop (hf).withTrimmedBottom (4);
                    leds[(size_t) k] = juce::Rectangle<int> (12, 12).withCentre ({ files[(size_t) k].getX() + 26, files[(size_t) k].getY() + 15 });
                    solos[(size_t) k] = juce::Rectangle<int> (22, 18).withPosition (files[(size_t) k].getRight() - 28, files[(size_t) k].getY() + 6);
                }
                dial.setBounds (dre.removeFromTop (juce::jmin (210, dre.getWidth())));
                auto fp = dre.removeFromTop (96);
                const int w3 = fp.getWidth() / 3;
                pom (Entrada)->setBounds (fp.removeFromLeft (w3));
                pom (DryWet)->setBounds (fp.removeFromLeft (w3));
                pom (Sortida)->setBounds (fp);
                dre.removeFromTop (8);
                auto m = dre.removeFromTop (juce::jmin (140, dre.getHeight() - 20)).withSizeKeepingCentre (60, juce::jmin (140, dre.getHeight() - 20));
                mesIn = m.removeFromLeft (24);
                mesOut = m.removeFromRight (24);
                if (editor != nullptr) editor->setBounds (r.removeFromTop (juce::jlimit (150, 250, r.getHeight() * 36 / 100)));
                r.removeFromTop (8);
                segPest.setBounds (r.removeFromTop (28).withWidth (260));
                r.removeFromTop (6);
                if (matriu != nullptr) matriu->setBounds (r);
                const int hl = r.getHeight() / 2;
                if (lfos[0] != nullptr) lfos[0]->setBounds (r.withHeight (hl - 4));
                if (lfos[1] != nullptr) lfos[1]->setBounds (r.withTrimmedTop (hl + 4));
                zonaAjuda = {};
                return;
            }
            zonaAjuda = r.removeFromBottom (20);
            // la cadena, a baix
            auto cad = r.removeFromBottom (juce::jlimit (84, 120, r.getHeight() / 5));
            r.removeFromBottom (8);
            const int gap = 14, w = (cad.getWidth() - gap * (numModuls - 1)) / numModuls;
            for (int k = 0; k < numModuls; ++k)
            {
                targetes[(size_t) k] = cad.removeFromLeft (w);
                cad.removeFromLeft (gap);
                leds[(size_t) k] = juce::Rectangle<int> (12, 12).withPosition (targetes[(size_t) k].getX() + 10, targetes[(size_t) k].getY() + 11);
            }
            // columnes dels costats i el pom al mig
            const int col = juce::jlimit (110, 170, r.getWidth() / 6);
            auto esq = r.removeFromLeft (col), dre = r.removeFromRight (col);
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
            arrossega = desti = -1;
            for (int k = 0; k < numModuls; ++k)
            {
                const int mod = o[(size_t) k];
                if (leds[(size_t) k].expanded (6).contains (e.getPosition()))
                {
                    const int p = paramOn (mod);
                    ev->posa (p, ev->valor (p) > 0.5f ? 0.0f : 1.0f);
                    refresca();
                    return;
                }
                if (! avancat()) continue;
                if (solos[(size_t) k].contains (e.getPosition()))
                {
                    ev->posaSolo (ev->soloActual() == mod ? -1 : mod);
                    repaint();
                    return;
                }
                if (files[(size_t) k].contains (e.getPosition()))
                {
                    if (triat != mod) { triat = mod; triaModul(); }
                    arrossega = k;
                    return;
                }
            }
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (arrossega < 0 || ! avancat()) return;
            desti = -1;
            for (int k = 0; k < numModuls; ++k)
                if (e.y < files[(size_t) k].getCentreY()) { desti = k; break; }
            if (desti < 0) desti = numModuls;
            repaint (zonaCadena);
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            if (arrossega >= 0 && desti >= 0 && desti != arrossega && desti != arrossega + 1)
            {
                auto o = ev->ordre();
                std::vector<int> v (o.begin(), o.end());
                const int m = v[(size_t) arrossega];
                v.erase (v.begin() + arrossega);
                v.insert (v.begin() + (desti > arrossega ? desti - 1 : desti), m);
                for (int k = 0; k < numModuls; ++k) o[(size_t) k] = v[(size_t) k];
                ev->posaOrdre (o);
                refresca();
            }
            arrossega = desti = -1;
            repaint (zonaCadena);
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
                case MReverser: return u8 (nomModeReverser ((int) v (RevMode))) + " · "_ca + u8 (nomDivisio ((int) v (RevDiv)))
                                       + ((int) v (RevPitch) > 0 ? " · "_ca + u8 (textValor (RevPitch, v (RevPitch))) : juce::String());
                case MPitch: return u8 (nomModePitch ((int) v (PitMode))) + " · "_ca + u8 (textValor (PitSemi1, v (PitSemi1)))
                                    + (v (PitNiv2) > 0.5f ? " / "_ca + u8 (textValor (PitSemi2, v (PitSemi2))) : juce::String());
                case MScatter: return u8 (nomDivisio ((int) v (ScaDiv))) + " · "_ca + juce::String (juce::roundToInt (v (ScaDensity))) + " % trencat"_ca;
                case MGranular: return juce::String (juce::roundToInt (v (GraSize))) + " ms · "_ca + juce::String (juce::roundToInt (v (GraDens))) + " /s"
                                       + (v (GraFreeze) > 0.5f ? " · FREEZE"_ca : juce::String());
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
            for (auto& [p, pom] : poms) repaint (pom->getBounds().expanded (4));
            if (avancat())
            {
                repaint (zonaCadena);
                if (editor != nullptr) editor->repaint();
                if (matriu != nullptr && matriu->isVisible()) matriu->refresca();
                for (auto& l : lfos) if (l != nullptr && l->isVisible()) l->refresca();
            }
            const auto ara = ev->estat();
            if (ara != vist) refresca();   // automatització o desfer
        }

        // ---- 1.14: ADVANCED ----
        bool avancat() const { return ev->valor (Vista) > 0.5f; }
        void canviaVista (bool perUsuari)
        {
            vistaAra = avancat();
            for (int i = 0; i < numModuls; ++i) targetes[(size_t) i] = files[(size_t) i] = {};
            if (vistaAra)
            {
                if (matriu == nullptr) { matriu = std::make_unique<EditorMatriu> (ev); addChildComponent (*matriu); }
                for (int k = 0; k < numLfos; ++k) if (lfos[(size_t) k] == nullptr) { lfos[(size_t) k] = std::make_unique<EditorLfo> (ev, k); addChildComponent (*lfos[(size_t) k]); }
                triaModul();
                segPest.setVisible (true);
                mostraPestanya();
                if (perUsuari)
                    if (auto* w = findParentComponentOfClass<juce::DocumentWindow>(); w != nullptr && (w->getWidth() < 1180 || w->getHeight() < 820))
                        w->setSize (juce::jmax (1180, w->getWidth()), juce::jmax (820, w->getHeight()));
            }
            else
            {
                if (editor != nullptr) editor->setVisible (false);
                if (matriu != nullptr) matriu->setVisible (false);
                for (auto& l : lfos) if (l != nullptr) l->setVisible (false);
                segPest.setVisible (false);
                ev->posaSolo (-1);
            }
            resized();
            repaint();
        }
        void triaModul()
        {
            editor = std::make_unique<EditorModul> (ev, triat);
            addAndMakeVisible (*editor);
            editor->refresca();
            resized();
            repaint (zonaCadena);
        }
        void mostraPestanya()
        {
            const bool m = segPest.getTriat() == 0;
            if (matriu != nullptr) matriu->setVisible (avancat() && m);
            for (auto& l : lfos) if (l != nullptr) l->setVisible (avancat() && ! m);
        }
        void pintaCadena (juce::Graphics& g, const std::array<int, numModuls>& o)
        {
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (11.0f, true));
            g.drawText ("CADENA (arrossega per canviar l'ordre)"_ca, zonaCadena.withHeight (18), juce::Justification::centredLeft);
            const int solo1 = ev->soloActual();
            for (int k = 0; k < numModuls; ++k)
            {
                const int m = o[(size_t) k];
                const auto r = files[(size_t) k].toFloat();
                const bool on = ev->valor (paramOn (m)) > 0.5f;
                const float act = ev->mesures().activitat[(size_t) m].load();
                const auto col = colorModul (m);
                g.setColour (m == triat ? juce::Colour (0xff2a2a33) : juce::Colour (0xff1c1c22));
                g.fillRoundedRectangle (r, 6.0f);
                g.setColour (m == triat ? col : tema::linia);
                g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, m == triat ? 1.5f : 1.0f);
                // agafador
                g.setColour (tema::linia2);
                for (int j = 0; j < 3; ++j) g.fillRect (r.getX() + 7.0f, r.getY() + 10.0f + j * 4.0f, 8.0f, 1.5f);
                const auto led = leds[(size_t) k].toFloat();
                g.setColour (on ? col : juce::Colour (0xff3a3a42));
                g.fillEllipse (led);
                g.setColour (on ? tema::text : tema::suau);
                g.setFont (dibuix::lletra (12.5f, true));
                g.drawText (nomModul (m), r.withTrimmedLeft (40.0f).withHeight (30.0f), juce::Justification::centredLeft);
                // solo
                const auto so = solos[(size_t) k].toFloat();
                g.setColour (solo1 == m ? tema::guitarra : juce::Colour (0xff0e0e12));
                g.fillRoundedRectangle (so, 3.0f);
                g.setColour (solo1 == m ? juce::Colours::black : tema::suau);
                g.setFont (dibuix::lletra (11.0f, true));
                g.drawText ("S", so, juce::Justification::centred);
                // activitat
                auto b = r.reduced (40.0f, 0.0f).withY (r.getBottom() - 9.0f).withHeight (4.0f);
                g.setColour (juce::Colour (0xff0e0e12));
                g.fillRoundedRectangle (b, 2.0f);
                g.setColour (col);
                g.fillRoundedRectangle (b.withWidth (b.getWidth() * juce::jlimit (0.0f, 1.0f, act)), 2.0f);
            }
            if (arrossega >= 0 && desti >= 0)
            {
                const int y = desti < numModuls ? files[(size_t) desti].getY() - 2 : files[(size_t) numModuls - 1].getBottom() + 1;
                g.setColour (tema::accent);
                g.fillRect (zonaCadena.getX(), y - 1, zonaCadena.getWidth(), 3);
            }
            if (solo1 >= 0)
            {
                g.setColour (tema::guitarra);
                g.setFont (dibuix::lletra (11.0f, true));
                g.drawText ("SOLO: només sona "_ca + juce::String (nomModul (solo1)), zonaCadena.withTop (files[(size_t) numModuls - 1].getBottom() + 6).withHeight (16), juce::Justification::centredLeft);
            }
        }

        // ---- 1.15: importar / exportar presets (fitxers .evolve) ----
        static juce::File carpetaExport()
        {
            return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Mibase").getChildFile ("Evolve");
        }
        void menuFitxer()
        {
            juce::PopupMenu m;
            m.addItem (1, "Exporta el so d'ara com un preset…"_ca);
            m.addItem (2, "Exporta tots els meus presets…"_ca, ! biblioteca().usuari.empty());
            m.addSeparator();
            m.addItem (3, "Importa presets (.evolve)…"_ca);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btFitxer), [sp = juce::Component::SafePointer<PanellEvolve> (this)] (int r)
            {
                if (sp == nullptr || r <= 0) return;
                if (r == 3) sp->importa(); else sp->exporta (r == 2);
            });
        }
        void exporta (bool tots)
        {
            BibliotecaPresets b;
            juce::String nom = nomsPreset()[ev.get()];
            if (tots) b.usuari = biblioteca().usuari;
            else
            {
                if (nom.isEmpty() || nom == "(atzar)"_ca) nom = "Evolve";
                b.usuari.push_back ({ nom.toStdString(), "Meus", "", ev->estat() });
            }
            carpetaExport().createDirectory();
            const auto proposta = carpetaExport().getChildFile (juce::File::createLegalFileName (tots ? juce::String ("Els meus presets EVOLVE") : nom) + ".evolve");
            tria = std::make_unique<juce::FileChooser> ("Exporta presets d'EVOLVE"_ca, proposta, "*.evolve");
            tria->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                [this, b] (const juce::FileChooser& fc)
                {
                    auto f = fc.getResult();
                    if (f == juce::File()) return;
                    if (! f.hasFileExtension ("evolve")) f = f.withFileExtension ("evolve");
                    const bool ok = b.desa (dibuix::cami (f));
                    juce::AlertWindow::showMessageBoxAsync (ok ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon, "EVOLVE",
                        ok ? "Desat: "_ca + f.getFileName() + " (" + juce::String ((int) b.usuari.size()) + " preset" + (b.usuari.size() == 1 ? "" : "s") + ")"
                           : "No s'ha pogut desar el fitxer."_ca);
                });
        }
        void importa()
        {
            tria = std::make_unique<juce::FileChooser> ("Importa presets d'EVOLVE"_ca, carpetaExport(), "*.evolve");
            tria->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (! f.existsAsFile()) return;
                BibliotecaPresets b;
                b.prefix = "mibase-evolve=";
                int n = 0;
                if (b.carrega (dibuix::cami (f)))
                    for (auto p : b.usuari)
                    {
                        if (p.estat.rfind ("mibase-evolve=", 0) != 0) continue;   // només presets d'EVOLVE
                        if (biblioteca().busca (p.nom) != nullptr) p.nom = biblioteca().nomLliure (p.nom);
                        biblioteca().desaPreset (p);
                        ++n;
                    }
                if (n > 0) desaBiblioteca();
                juce::AlertWindow::showMessageBoxAsync (n > 0 ? juce::MessageBoxIconType::InfoIcon : juce::MessageBoxIconType::WarningIcon, "EVOLVE",
                    n > 0 ? "Importats: "_ca + juce::String (n) + " preset" + (n == 1 ? "" : "s") + ". Són a «Meus» (o a la seva carpeta)."_ca
                          : "Aquest fitxer no té cap preset d'EVOLVE."_ca);
                refresca();
            });
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
        Boto btFitxer { "Fitxer", dibuix::Icona::FletxaAvall, Boto::Estil::Secundari };   // 1.15: importar / exportar
        std::unique_ptr<juce::FileChooser> tria;
        Boto btCat { "Totes"_ca, dibuix::Icona::FletxaAvall, Boto::Estil::Secundari };
        Boto btAtzar { "RANDOMIZE", std::nullopt, Boto::Estil::Secundari };
        Boto btReset { "RESET", std::nullopt, Boto::Estil::Secundari };
        Boto btHold { "HOLD", std::nullopt, Boto::Estil::Tecla };
        Segmentat segAB { { "A", "B" } };
        int ab = 0;
        std::array<std::string, 2> slots;
        std::array<juce::String, 2> noms;
        std::vector<std::pair<int, std::unique_ptr<Pom>>> poms;
        std::array<juce::Rectangle<int>, numModuls> targetes, leds, files, solos;
        Segmentat segVista { { "SIMPLE", "ADVANCED" } }, segPest { { "MATRIU D'EVOLUTION"_ca, "LFO 1 · LFO 2"_ca } };
        int triat = MScreamer, arrossega = -1, desti = -1;
        bool vistaAra = false;
        std::unique_ptr<EditorModul> editor;
        std::unique_ptr<EditorMatriu> matriu;
        std::array<std::unique_ptr<EditorLfo>, numLfos> lfos;
        juce::Rectangle<int> zonaCadena;
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
            setResizeLimits (900, 560, 1700, 1200);
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
