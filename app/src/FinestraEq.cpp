#include "FinestraEq.h"

#include "Controls.h"
#include "Dibuix.h"
#include "EscGlobal.h"
#include "Pom.h"
#include "Tema.h"
#include "Text.h"
#include "Vintage.h"
#include <mibase/Midi.h>
#include <cmath>
#include <set>

using namespace mibase::eq;

namespace
{
    juce::String u8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str(), (int) s.size()); }
    juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

    constexpr double fVistaMin = 20.0, fVistaMax = 20000.0;

    juce::String coma (double v, int dec)
    {
        if (dec <= 0) return juce::String ((int) std::lround (v));
        return juce::String (v, dec).replaceCharacter ('.', ',');
    }
    juce::String textFreq (double f)
    {
        if (f >= 1000.0) return coma (f / 1000.0, f >= 10000.0 ? 1 : 2) + " kHz";
        return coma (f, f >= 100.0 ? 0 : 1) + " Hz";
    }
    juce::String textDb (double d) { return (d > 0.05 ? "+" : "") + coma (std::abs (d) < 0.05 ? 0.0 : d, 1) + " dB"; }
    juce::String textNota (double f)
    {
        if (f < 8.0) return {};
        const double m = 69.0 + 12.0 * std::log2 (f / 440.0);
        const int n = (int) std::lround (m);
        if (n < 0 || n > 135) return {};
        const int cents = (int) std::lround ((m - n) * 100.0);
        return u8 (mibase::midi::nomNota (n)) + (cents == 0 ? juce::String() : (cents > 0 ? " +" : " ") + juce::String (cents) + "c");
    }

    /** Un color diferent per a cada banda (passos de l'angle d'or, perquè els veïns no s'assemblin). */
    juce::Colour colorBanda (int k)
    {
        const float h = std::fmod (0.07f + 0.61803398875f * (float) k, 1.0f);
        return juce::Colour::fromHSV (h, 0.62f, 0.98f, 1.0f);
    }

    juce::Colour colorCanal (int c)
    {
        switch (c)
        {
            case Esquerre: return tema::midi;
            case Dret: return juce::Colour (0xffff8fc4);
            case Mid: return tema::guitarra;
            case Side: return tema::veu;
            default: return tema::text;
        }
    }

    /** On va el node d'una banda (dB), respecte del guany. Els shelf i el tilt tenen el node a mig cami. */
    double factorNode (int tipus)
    {
        switch (tipus)
        {
            case Bell: return 1.0;
            case LowShelf: case HighShelf: case Tilt: return 0.5;
            default: return 0.0;
        }
    }

    juce::String nomCanalUi (int c)
    {
        switch (c)
        {
            case Esquerre: return "Esquerre (L)";
            case Dret: return "Dret (R)";
            case Mid: return "Mid (el centre)";
            case Side: return "Side (els costats)";
            default: return "Estèreo (tots dos)"_ca;
        }
    }

    /** Llegeix un valor escrit («1,2k», «1200», «-3,5», «0.7»). */
    bool llegeixValor (juce::String t, double& v)
    {
        t = t.trim().toLowerCase().replaceCharacter (',', '.').removeCharacters (" ");
        double mult = 1.0;
        if (t.endsWith ("khz")) { mult = 1000.0; t = t.dropLastCharacters (3); }
        else if (t.endsWith ("hz")) t = t.dropLastCharacters (2);
        else if (t.endsWith ("db")) t = t.dropLastCharacters (2);
        else if (t.endsWith ("k")) { mult = 1000.0; t = t.dropLastCharacters (1); }
        if (t.startsWith ("q")) t = t.substring (1);
        if (t.isEmpty() || ! t.containsOnly ("0123456789.-+")) return false;
        v = t.getDoubleValue() * mult;
        return std::isfinite (v);
    }

    class PanellEq;

    // ---------------------------------------------------------------------------------------------------------------
    /** La gràfica: corbes, nodes i tot el que es fa amb el ratolí. */
    class GraficaEq : public juce::Component
    {
    public:
        explicit GraficaEq (PanellEq& p);
        void paint (juce::Graphics&) override;
        void resized() override { calculat = false; }
        void mouseMove (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override { hover = {}; nodeSobre = -1; repaint(); }
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

        float rangDb = 12.0f;
        void invalida() { calculat = false; repaint(); }

        double xDeFreq (double f) const { return area().getX() + area().getWidth() * std::log (f / fVistaMin) / std::log (fVistaMax / fVistaMin); }
        double freqDeX (double x) const { return fVistaMin * std::pow (fVistaMax / fVistaMin, (x - area().getX()) / std::max (1.0f, area().getWidth())); }
        double yDeDb (double d) const { return area().getCentreY() - area().getHeight() * 0.5 * d / rangDb; }
        double dbDeY (double y) const { return (area().getCentreY() - y) / (area().getHeight() * 0.5) * rangDb; }
        juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (1.0f, 6.0f).withTrimmedBottom (14.0f); }

    private:
        PanellEq& p;
        bool calculat = false;
        std::vector<float> xs;
        std::array<std::vector<float>, maxBandes> corbaBanda;    // dB per columna
        std::array<std::vector<float>, numCanals> corbaTotal;
        std::array<bool, numCanals> teCanal {};
        std::array<Banda, maxBandes> bandes;
        void calcula();
        juce::Point<float> posNode (int k) const;
        int nodeA (juce::Point<float> pos) const;

        juce::Point<float> hover;
        int nodeSobre = -1;
        // arrossegar
        enum class Gest { Res, Nodes, Requadre } gest = Gest::Res;
        juce::Point<float> inici;
        juce::Rectangle<float> requadre;
        std::vector<std::pair<int, Banda>> abans;
        bool mogut = false;
        juce::uint32 ultimaRoda = 0;
    };

    // ---------------------------------------------------------------------------------------------------------------
    /** La llista de bandes (una pastilla per banda). */
    class LlistaBandes : public juce::Component
    {
    public:
        explicit LlistaBandes (PanellEq& p) : p (p) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
    private:
        PanellEq& p;
        std::vector<std::pair<int, juce::Rectangle<float>>> llocs;
    };

    // ---------------------------------------------------------------------------------------------------------------
    /** Editor de valors (doble clic a un node): freqüència, guany i Q escrits a mà. */
    class EditorValors : public juce::Component
    {
    public:
        EditorValors (PanellEq& p, int k);
        void resized() override;
        void paint (juce::Graphics& g) override
        {
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (11.5f));
            for (int i = 0; i < 3; ++i)
                if (camps[(size_t) i].isVisible())
                    g.drawText (noms[i], 8, 8 + i * 30, 90, 24, juce::Justification::centredLeft);
        }
    private:
        PanellEq& p;
        int k;
        std::array<juce::TextEditor, 3> camps;
        Boto btAplica { "Aplica", std::nullopt, Boto::Estil::Primari };
        juce::StringArray noms { "Freqüència"_ca, "Guany", "Q" };
        void aplica();
    };

    // ---------------------------------------------------------------------------------------------------------------
    class PanellEq : public juce::Component, private juce::Timer
    {
    public:
        explicit PanellEq (std::shared_ptr<Equalitzador> e);
        ~PanellEq() override { stopTimer(); }
        Equalitzador& eq() { return *q; }

        // selecció
        std::set<int> triades;
        int focus = -1;
        void tria (int k, bool afegeix)
        {
            if (! afegeix) triades.clear();
            if (k >= 0)
            {
                if (afegeix && triades.count (k) && triades.size() > 1) { triades.erase (k); if (focus == k) focus = *triades.begin(); }
                else { triades.insert (k); focus = k; }
            }
            else focus = -1;
            canviTriades();
        }
        void triaRequadre (const std::vector<int>& v, bool afegeix)
        {
            if (! afegeix) triades.clear();
            for (int k : v) triades.insert (k);
            focus = triades.empty() ? -1 : (std::find (v.begin(), v.end(), focus) != v.end() ? focus : *triades.begin());
            canviTriades();
        }
        void canviTriades() { refresca(); }

        // desfés (propi de la finestra; com a la Reverb)
        void abansDeCanviar()
        {
            auto s = q->estat();
            if (desfer.empty() || desfer.back() != s) desfer.push_back (std::move (s));
            if (desfer.size() > 200) desfer.erase (desfer.begin());
            refer.clear();
        }
        void desfes()
        {
            if (desfer.empty()) return;
            refer.push_back (q->estat());
            q->posaEstat (desfer.back());
            desfer.pop_back();
            netejaTriades();
            refresca();
        }
        void refes()
        {
            if (refer.empty()) return;
            desfer.push_back (q->estat());
            q->posaEstat (refer.back());
            refer.pop_back();
            netejaTriades();
            refresca();
        }
        void netejaTriades()
        {
            for (auto it = triades.begin(); it != triades.end();) it = q->banda (*it).usada ? std::next (it) : triades.erase (it);
            if (focus >= 0 && ! q->banda (focus).usada) focus = triades.empty() ? -1 : *triades.begin();
        }

        // accions
        int creaBanda (Banda b)
        {
            const int k = q->bandaLliure();
            if (k < 0) return -1;
            abansDeCanviar();
            b.usada = true;
            q->posaBanda (k, b);
            tria (k, false);
            return k;
        }
        void esborraTriades()
        {
            if (triades.empty()) return;
            abansDeCanviar();
            for (int k : triades) q->esborra (k);
            triades.clear();
            focus = -1;
            refresca();
        }
        void duplicaTriades()
        {
            if (triades.empty()) return;
            abansDeCanviar();
            std::vector<int> nous;
            for (int k : std::vector<int> (triades.begin(), triades.end()))
            {
                const int l = q->bandaLliure();
                if (l < 0) break;
                auto b = q->banda (k);
                b.freq = (float) juce::jlimit ((double) freqMin, (double) freqMax, b.freq * std::pow (2.0, 1.0 / 3.0));   // un terç d'octava amunt
                q->posaBanda (l, b);
                nous.push_back (l);
            }
            triaRequadre (nous, false);
        }
        /** Canvia una cosa de totes les bandes triades (tipus, pendent, canal, encesa). */
        void aTotes (const std::function<void (Banda&)>& f)
        {
            if (triades.empty()) return;
            abansDeCanviar();
            for (int k : triades) { auto b = q->banda (k); f (b); q->posaBanda (k, b); }
            refresca();
        }
        void menuBanda (int k, juce::Component* on);
        void menuBuit (double f, double db, juce::Component* on);
        void editaValors (int k, juce::Rectangle<int> on);
        void refresca();

        bool keyPressed (const juce::KeyPress& k) override;
        void paint (juce::Graphics&) override;
        void resized() override;

        static std::optional<Banda>& portaBanda() { static std::optional<Banda> b; return b; }

    private:
        std::shared_ptr<Equalitzador> q;
        std::vector<std::string> desfer, refer;
        unsigned versioVista = ~0u;

        GraficaEq grafica { *this };
        LlistaBandes llista { *this };
        // barra de dalt
        Boto btDesfes { {}, dibuix::Icona::Desfes, Boto::Estil::Subtil }, btRefes { {}, dibuix::Icona::Refes, Boto::Estil::Subtil };
        Segmentat segRang { { "6 dB", "12 dB", "24 dB", "30 dB" } };
        Boto btBypass { "Bypass", std::nullopt, Boto::Estil::Tecla };
        Pom pomSortida { "Sortida", 0.0, 1.0, 0.5, true, [this] (double v) { return u8 (q->textParametre (PSortida, (float) v)); } };
        // panell de la banda
        Boto btTipus { "Bell", std::nullopt, Boto::Estil::Secundari };
        Boto btPendent { "12 dB/oct", std::nullopt, Boto::Estil::Secundari };
        Segmentat segCanal { { "Estèreo"_ca, "L", "R", "Mid", "Side" } };
        Boto btEncesa { "Encesa", std::nullopt, Boto::Estil::Tecla };
        Boto btDuplica { "Duplica", std::nullopt, Boto::Estil::Subtil };
        Boto btEsborra { "Esborra", std::nullopt, Boto::Estil::Subtil };
        Pom pomFreq { "Freqüència"_ca, 0.0, 1.0, 0.5, false, [this] (double v) { return focus >= 0 ? u8 (q->textParametre (indexParam (focus, PFreq), (float) v)) : juce::String(); } };
        Pom pomGuany { "Guany", 0.0, 1.0, 0.5, true, [this] (double v) { return focus >= 0 ? u8 (q->textParametre (indexParam (focus, PGuany), (float) v)) : juce::String(); } };
        Pom pomQ { "Q", 0.0, 1.0, 0.5, false, [this] (double v) { return focus >= 0 ? u8 (q->textParametre (indexParam (focus, PQ), (float) v)) : juce::String(); } };
        juce::TextEditor editorValor;
        int pomEscrit = -1;
        juce::Rectangle<int> zonaBanda, zonaMetres, zonaTitol;
        float vuIn = 0.0f, vuOut = 0.0f;
        juce::TooltipWindow tips { this, 500 };

        void timerCallback() override;
        void posaPom (int p, double v01);
        void escriuPom (int p, Pom& pom);
        void mostraControls();
    };

    // ===============================================================================================================
    GraficaEq::GraficaEq (PanellEq& pe) : p (pe)
    {
        setWantsKeyboardFocus (false);
        setMouseClickGrabsKeyboardFocus (false);
    }

    void GraficaEq::calcula()
    {
        calculat = true;
        auto& q = p.eq();
        const double fs = q.frequenciaMostreig() > 0.0 ? q.frequenciaMostreig() : 48000.0;
        const auto a = area();
        xs.clear();
        for (float x = a.getX(); x <= a.getRight() + 0.5f; x += 2.0f) xs.push_back (x);
        const size_t n = xs.size();
        for (auto& c : corbaTotal) c.assign (n, 0.0f);
        teCanal.fill (false);
        for (int k = 0; k < maxBandes; ++k)
        {
            bandes[(size_t) k] = q.banda (k);
            const auto& b = bandes[(size_t) k];
            auto& cb = corbaBanda[(size_t) k];
            if (! b.usada) { cb.clear(); continue; }
            cb.assign (n, 0.0f);
            Seccio s[maxSeccions];
            const int ns = dissenya (b, fs, s);
            for (size_t i = 0; i < n; ++i)
            {
                const double f = freqDeX (xs[i]);
                cb[i] = f < 0.5 * fs ? (float) (20.0 * std::log10 (std::max (1.0e-9, std::abs (resposta (s, ns, f, fs))))) : cb[i > 0 ? i - 1 : 0];
            }
            if (! b.activa) continue;
            teCanal[(size_t) b.canal] = true;
            for (int c = 0; c < numCanals; ++c)
                if (b.canal == Estereo || b.canal == c)
                    for (size_t i = 0; i < n; ++i) corbaTotal[(size_t) c][i] += cb[i];
        }
    }

    juce::Point<float> GraficaEq::posNode (int k) const
    {
        const auto& b = bandes[(size_t) k];
        const double d = factorNode (b.tipus) * b.guany;
        const auto a = area();
        return { (float) juce::jlimit ((double) a.getX(), (double) a.getRight(), xDeFreq (b.freq)),
                 (float) juce::jlimit ((double) a.getY() + 4.0, (double) a.getBottom() - 4.0, yDeDb (d)) };
    }

    int GraficaEq::nodeA (juce::Point<float> pos) const
    {
        int millor = -1;
        float dist = 11.0f;
        for (int k = maxBandes; --k >= 0;)
        {
            if (! bandes[(size_t) k].usada) continue;
            const float d = posNode (k).getDistanceFrom (pos);
            if (d < dist) { dist = d; millor = k; }
        }
        return millor;
    }

    void GraficaEq::paint (juce::Graphics& g)
    {
        if (! calculat) calcula();
        const auto a = area();
        auto& q = p.eq();
        g.setColour (juce::Colour (0xff0d0d10));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);

        // graella
        g.setFont (dibuix::lletra (10.0f));
        for (double f : { 30.0, 40.0, 50.0, 60.0, 70.0, 80.0, 90.0, 100.0, 200.0, 300.0, 400.0, 500.0, 600.0, 700.0, 800.0, 900.0, 1000.0,
                          2000.0, 3000.0, 4000.0, 5000.0, 6000.0, 7000.0, 8000.0, 9000.0, 10000.0 })
        {
            const bool gran = f == 100.0 || f == 1000.0 || f == 10000.0;
            g.setColour (juce::Colours::white.withAlpha (gran ? 0.10f : 0.045f));
            g.drawVerticalLine ((int) xDeFreq (f), a.getY(), a.getBottom());
        }
        g.setColour (tema::suau.withAlpha (0.8f));
        for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
            g.drawText (f >= 1000.0 ? juce::String ((int) (f / 1000.0)) + "k" : juce::String ((int) f), (int) xDeFreq (f) - 20, (int) a.getBottom() + 1, 40, 13,
                        juce::Justification::centred);
        const float pas = rangDb <= 6.0f ? 2.0f : (rangDb <= 12.0f ? 3.0f : (rangDb <= 24.0f ? 6.0f : 10.0f));
        for (float d = -rangDb; d <= rangDb + 0.01f; d += pas)
        {
            g.setColour (juce::Colours::white.withAlpha (d == 0.0f ? 0.16f : 0.05f));
            g.drawHorizontalLine ((int) yDeDb (d), a.getX(), a.getRight());
            if (std::abs (d) < rangDb - 0.01f)
            {
                g.setColour (tema::suau.withAlpha (0.7f));
                g.drawText ((d > 0 ? "+" : "") + juce::String ((int) d), (int) a.getX() + 4, (int) yDeDb (d) - 13, 30, 12, juce::Justification::centredLeft);
            }
        }

        const size_t n = xs.size();
        auto cami = [&] (const std::vector<float>& c)
        {
            juce::Path pa;
            for (size_t i = 0; i < n; ++i)
            {
                const float y = (float) juce::jlimit ((double) a.getY() - 2.0, (double) a.getBottom() + 2.0, yDeDb (c[i]));
                if (i == 0) pa.startNewSubPath (xs[i], y); else pa.lineTo (xs[i], y);
            }
            return pa;
        };

        // corbes de cada banda (ombra plena fins a la línia del 0)
        const float y0 = (float) yDeDb (0.0);
        for (int k = 0; k < maxBandes; ++k)
        {
            const auto& b = bandes[(size_t) k];
            if (! b.usada || corbaBanda[(size_t) k].empty()) continue;
            const bool triada = p.triades.count (k) > 0;
            auto pa = cami (corbaBanda[(size_t) k]);
            auto ple = pa;
            ple.lineTo (xs.back(), y0);
            ple.lineTo (xs.front(), y0);
            ple.closeSubPath();
            const auto c = colorBanda (k);
            g.setColour (c.withAlpha (! b.activa ? 0.03f : (triada ? 0.22f : 0.09f)));
            g.fillPath (ple);
            if (triada || k == nodeSobre)
            {
                g.setColour (c.withAlpha (b.activa ? 0.75f : 0.35f));
                g.strokePath (pa, juce::PathStrokeType (1.2f));
            }
        }

        // corbes totals: primer les de cada canal (L, R, Mid, Side) i, a sobre, la d'estèreo
        const bool especials = teCanal[Esquerre] || teCanal[Dret] || teCanal[Mid] || teCanal[Side];
        if (especials)
        {
            float yl = a.getY() + 4.0f;
            g.setFont (dibuix::lletra (10.5f, true));
            for (int c : { (int) Esquerre, (int) Dret, (int) Mid, (int) Side })
            {
                if (! teCanal[(size_t) c]) continue;
                g.setColour (colorCanal (c).withAlpha (q.bypass() ? 0.4f : 0.95f));
                g.strokePath (cami (corbaTotal[(size_t) c]), juce::PathStrokeType (2.6f));
                g.drawText (u8 (curtCanal (c)), (int) a.getRight() - 30, (int) yl, 26, 13, juce::Justification::centredRight);
                yl += 14.0f;
            }
            g.setColour (tema::text.withAlpha (0.9f));
            g.drawText ("ST", (int) a.getRight() - 30, (int) yl, 26, 13, juce::Justification::centredRight);
        }
        g.setColour (q.bypass() ? tema::suau.withAlpha (0.5f) : tema::text.withAlpha (0.95f));
        g.strokePath (cami (corbaTotal[Estereo]), juce::PathStrokeType (especials ? 1.6f : 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // nodes
        for (int k = 0; k < maxBandes; ++k)
        {
            const auto& b = bandes[(size_t) k];
            if (! b.usada) continue;
            const auto pt = posNode (k);
            const bool triada = p.triades.count (k) > 0;
            const float r = triada || k == nodeSobre ? 9.0f : 7.5f;
            const auto c = colorBanda (k);
            if (triada)
            {
                g.setColour (c.withAlpha (0.25f));
                g.fillEllipse (juce::Rectangle<float> (r * 3.0f, r * 3.0f).withCentre (pt));
            }
            g.setColour (b.activa ? c : juce::Colour (0xff3a3a44));
            g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pt));
            g.setColour (k == p.focus ? tema::text : juce::Colours::black.withAlpha (0.6f));
            g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pt), k == p.focus ? 2.0f : 1.0f);
            g.setColour (b.activa ? juce::Colours::black : tema::suau);
            g.setFont (dibuix::lletra (10.0f, true));
            g.drawText (juce::String (k + 1), juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (pt), juce::Justification::centred);
            if (b.canal != Estereo)
            {
                g.setColour (colorCanal (b.canal));
                g.setFont (dibuix::lletra (9.5f, true));
                g.drawText (u8 (curtCanal (b.canal)), juce::Rectangle<float> (20.0f, 11.0f).withCentre (pt.translated (0.0f, r + 7.0f)), juce::Justification::centred);
            }
        }

        // requadre
        if (gest == Gest::Requadre)
        {
            g.setColour (tema::accent.withAlpha (0.12f));
            g.fillRect (requadre);
            g.setColour (tema::accent.withAlpha (0.7f));
            g.drawRect (requadre, 1.0f);
        }

        // lectura: on és el ratolí o el node de sota
        juce::String t;
        const auto sep = " · "_ca;
        if (nodeSobre >= 0)
        {
            const auto& b = bandes[(size_t) nodeSobre];
            t << juce::String (nodeSobre + 1) << sep << u8 (nomTipus (b.tipus)) << sep << textFreq (b.freq) << " (" << textNota (b.freq) << ")";
            if (teGuany (b.tipus)) t << sep << textDb (b.guany);
            if (teQ (b.tipus, b.pendent)) t << sep << "Q " << coma (b.q, 2);
            if (tePendent (b.tipus)) t << sep << juce::String (dbPerOctava (b.pendent)) << " dB/oct";
            if (b.canal != Estereo) t << sep << u8 (nomCanal (b.canal));
            if (! b.activa) t << sep << "apagada";
        }
        else if (a.contains (hover))
        {
            const double f = freqDeX (hover.x);
            t << textFreq (f) << sep << textNota (f) << sep << textDb (dbDeY (hover.y));
        }
        if (t.isNotEmpty())
        {
            g.setFont (dibuix::lletra (11.5f, true));
            const int w = (int) juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), t) + 16;
            juce::Rectangle<float> r (a.getX() + 34.0f, a.getY() + 4.0f, (float) w, 20.0f);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (nodeSobre >= 0 ? colorBanda (nodeSobre) : tema::textMig);
            g.drawText (t, r, juce::Justification::centred);
        }
        if (q.bypass())
        {
            g.setFont (dibuix::lletra (13.0f, true));
            g.setColour (tema::guitarra.withAlpha (0.9f));
            g.drawText ("BYPASS", a.reduced (8.0f).removeFromBottom (20.0f), juce::Justification::centredRight);
        }
        bool algunaBanda = false;
        for (const auto& b : bandes) algunaBanda = algunaBanda || b.usada;
        if (! algunaBanda)
        {
            g.setFont (dibuix::lletra (13.0f));
            g.setColour (tema::suau);
            g.drawText ("Doble clic a la gràfica per crear una banda (a les vores: passa-alts i passa-baixos)"_ca, a, juce::Justification::centred);
        }
    }

    void GraficaEq::mouseMove (const juce::MouseEvent& e)
    {
        hover = e.position;
        nodeSobre = nodeA (e.position);
        setMouseCursor (nodeSobre >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::CrosshairCursor);
        repaint();
    }

    void GraficaEq::mouseDown (const juce::MouseEvent& e)
    {
        p.grabKeyboardFocus();
        hover = e.position;
        const int k = nodeA (e.position);
        mogut = false;
        inici = e.position;
        if (e.mods.isPopupMenu())
        {
            gest = Gest::Res;
            if (k >= 0) { if (! p.triades.count (k)) p.tria (k, false); p.menuBanda (k, this); }
            else p.menuBuit (freqDeX (e.position.x), dbDeY (e.position.y), this);
            return;
        }
        if (k >= 0)
        {
            if (e.mods.isCommandDown() || (e.mods.isShiftDown() && ! p.triades.count (k))) p.tria (k, true);
            else if (! p.triades.count (k)) p.tria (k, false);
            else { p.focus = k; p.canviTriades(); }
            gest = Gest::Nodes;
            abans.clear();
            for (int t : p.triades) abans.push_back ({ t, p.eq().banda (t) });
            return;
        }
        gest = Gest::Requadre;
        requadre = { e.position, e.position };
    }

    void GraficaEq::mouseDrag (const juce::MouseEvent& e)
    {
        hover = e.position;
        if (gest == Gest::Requadre)
        {
            requadre = juce::Rectangle<float> (inici, e.position);
            repaint();
            return;
        }
        if (gest != Gest::Nodes || abans.empty()) return;
        if (! mogut && e.position.getDistanceFrom (inici) < 3.0f) return;
        if (! mogut)
        {
            mogut = true;
            p.abansDeCanviar();   // el desfés guarda com era abans d'arrossegar
        }
        const double fi = e.mods.isShiftDown() ? 0.15 : 1.0;
        double dx = (e.position.x - inici.x) * fi, dy = (e.position.y - inici.y) * fi;
        if (e.mods.isAltDown()) { if (std::abs (dx) > std::abs (dy)) dy = 0.0; else dx = 0.0; }   // Alt: només un eix
        const auto a = area();
        const double dLogF = dx / std::max (1.0f, a.getWidth()) * std::log (fVistaMax / fVistaMin);
        const double dDb = -dy / (a.getHeight() * 0.5) * rangDb;
        auto& q = p.eq();
        for (auto& [t, b0] : abans)
        {
            auto b = b0;
            b.freq = (float) juce::jlimit (fVistaMin, fVistaMax, std::exp (std::log ((double) b0.freq) + dLogF));
            const double fn = factorNode (b.tipus);
            if (fn > 0.0) b.guany = (float) juce::jlimit (-(double) guanyMax, (double) guanyMax, b0.guany + dDb / fn);
            else if (teQ (b.tipus, b.pendent)) b.q = (float) juce::jlimit ((double) qMin, (double) qMax, std::exp (std::log ((double) b0.q) + dDb / 12.0));
            if (fn > 0.0 && ! e.mods.isShiftDown()) b.guany = (float) (std::round (b.guany * 10.0) / 10.0);
            q.posaBanda (t, b);
        }
        nodeSobre = p.focus;
        p.refresca();
    }

    void GraficaEq::mouseUp (const juce::MouseEvent& e)
    {
        if (gest == Gest::Requadre)
        {
            if (requadre.getWidth() < 3.0f && requadre.getHeight() < 3.0f) { if (! e.mods.isCommandDown()) p.tria (-1, false); }
            else
            {
                std::vector<int> dins;
                for (int k = 0; k < maxBandes; ++k) if (bandes[(size_t) k].usada && requadre.contains (posNode (k))) dins.push_back (k);
                p.triaRequadre (dins, e.mods.isCommandDown() || e.mods.isShiftDown());
            }
        }
        gest = Gest::Res;
        abans.clear();
        repaint();
    }

    void GraficaEq::mouseDoubleClick (const juce::MouseEvent& e)
    {
        const int k = nodeA (e.position);
        if (k >= 0) { p.editaValors (k, juce::Rectangle<int> ((int) e.position.x - 2, (int) e.position.y - 2, 4, 4)); return; }
        const auto a = area();
        if (! a.contains (e.position)) return;
        Banda b;
        b.freq = (float) freqDeX (e.position.x);
        if (e.position.x < a.getX() + a.getWidth() * 0.04f) { b.tipus = HighPass; b.pendent = 3; b.freq = (float) std::max (30.0, (double) b.freq); }
        else if (e.position.x > a.getRight() - a.getWidth() * 0.04f) { b.tipus = LowPass; b.pendent = 3; b.freq = (float) std::min (18000.0, (double) b.freq); }
        else { b.tipus = Bell; b.guany = (float) (std::round (juce::jlimit (-(double) guanyMax, (double) guanyMax, dbDeY (e.position.y)) * 10.0) / 10.0); b.q = 1.0f; }
        if (p.creaBanda (b) < 0) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "EQ Pro", "Ja hi ha les 24 bandes. Esborra'n alguna per fer-ne una de nova."_ca);
    }

    void GraficaEq::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
    {
        int k = nodeA (e.position);
        if (k < 0) return;
        auto& q = p.eq();
        const auto ara = juce::Time::getMillisecondCounter();
        if (ara - ultimaRoda > 700) p.abansDeCanviar();
        ultimaRoda = ara;
        std::vector<int> quines;
        if (p.triades.count (k)) quines.assign (p.triades.begin(), p.triades.end()); else quines.push_back (k);
        const double pas = (w.deltaY >= 0 ? 1.0 : -1.0) * (e.mods.isShiftDown() ? 0.03 : 0.12);
        for (int t : quines)
        {
            auto b = q.banda (t);
            if (! teQ (b.tipus, b.pendent)) continue;
            b.q = (float) juce::jlimit ((double) qMin, (double) qMax, std::exp (std::log ((double) b.q) + pas));
            q.posaBanda (t, b);
        }
        nodeSobre = k;
        p.refresca();
    }

    // ===============================================================================================================
    void LlistaBandes::paint (juce::Graphics& g)
    {
        llocs.clear();
        auto& q = p.eq();
        auto r = getLocalBounds().toFloat();
        g.setFont (dibuix::lletra (11.0f, true));
        int quantes = 0;
        for (int k = 0; k < maxBandes; ++k)
        {
            const auto b = q.banda (k);
            if (! b.usada) continue;
            ++quantes;
            juce::String t = juce::String (k + 1) + " " + u8 (nomTipus (b.tipus));
            const float w = (float) juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), t) + 26.0f;
            if (r.getWidth() < w) break;
            auto c = r.removeFromLeft (w).reduced (2.0f, 3.0f);
            r.removeFromLeft (2.0f);
            llocs.push_back ({ k, c });
            const bool triada = p.triades.count (k) > 0;
            g.setColour (triada ? tema::triat : tema::superficie);
            g.fillRoundedRectangle (c, 4.0f);
            g.setColour (triada ? colorBanda (k) : tema::linia);
            g.drawRoundedRectangle (c, 4.0f, triada ? 1.5f : 1.0f);
            g.setColour (b.activa ? colorBanda (k) : juce::Colour (0xff3a3a44));
            g.fillEllipse (c.getX() + 7.0f, c.getCentreY() - 4.0f, 8.0f, 8.0f);
            g.setColour (b.activa ? tema::text : tema::suau);
            g.drawText (t, c.withTrimmedLeft (19.0f), juce::Justification::centredLeft);
        }
        if (quantes == 0)
        {
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (11.0f));
            g.drawText ("Encara no hi ha cap banda.", r, juce::Justification::centredLeft);
        }
    }

    void LlistaBandes::mouseDown (const juce::MouseEvent& e)
    {
        for (auto& [k, c] : llocs)
            if (c.contains (e.position))
            {
                if (e.mods.isPopupMenu()) { if (! p.triades.count (k)) p.tria (k, false); p.menuBanda (k, this); }
                else p.tria (k, e.mods.isCommandDown() || e.mods.isShiftDown());
                return;
            }
    }

    // ===============================================================================================================
    EditorValors::EditorValors (PanellEq& pe, int banda) : p (pe), k (banda)
    {
        const auto b = p.eq().banda (k);
        const juce::String v[] = { textFreq (b.freq), coma (b.guany, 1), coma (b.q, 2) };
        for (int i = 0; i < 3; ++i)
        {
            auto& c = camps[(size_t) i];
            c.setText (v[i], false);
            c.setFont (dibuix::lletra (13.0f));
            c.setColour (juce::TextEditor::backgroundColourId, tema::superficie);
            c.setColour (juce::TextEditor::textColourId, tema::text);
            c.setColour (juce::TextEditor::outlineColourId, tema::linia);
            c.setColour (juce::TextEditor::focusedOutlineColourId, tema::accent);
            c.onReturnKey = [this] { aplica(); };
            c.onEscapeKey = [this] { if (auto* cb = findParentComponentOfClass<juce::CallOutBox>()) cb->dismiss(); };
            addChildComponent (c);
        }
        camps[0].setVisible (true);
        camps[1].setVisible (teGuany (b.tipus));
        camps[2].setVisible (teQ (b.tipus, b.pendent));
        addAndMakeVisible (btAplica);
        btAplica.onClick = [this] { aplica(); };
        setSize (230, 8 + 3 * 30 + 36);
        juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<EditorValors> (this)] { if (sp != nullptr) { sp->camps[0].grabKeyboardFocus(); sp->camps[0].selectAll(); } });
    }

    void EditorValors::resized()
    {
        for (int i = 0; i < 3; ++i) camps[(size_t) i].setBounds (100, 8 + i * 30, getWidth() - 108, 24);
        btAplica.setBounds (getWidth() - 98, getHeight() - 32, 90, 26);
    }

    void EditorValors::aplica()
    {
        auto& q = p.eq();
        auto b = q.banda (k);
        double v;
        bool algun = false;
        auto nou = b;
        if (llegeixValor (camps[0].getText(), v)) { nou.freq = (float) juce::jlimit ((double) freqMin, (double) freqMax, v); algun = true; }
        if (camps[1].isVisible() && llegeixValor (camps[1].getText(), v)) { nou.guany = (float) juce::jlimit (-(double) guanyMax, (double) guanyMax, v); algun = true; }
        if (camps[2].isVisible() && llegeixValor (camps[2].getText(), v)) { nou.q = (float) juce::jlimit ((double) qMin, (double) qMax, v); algun = true; }
        if (algun && ! (nou == b)) { p.abansDeCanviar(); q.posaBanda (k, nou); p.refresca(); }
        if (auto* cb = findParentComponentOfClass<juce::CallOutBox>()) cb->dismiss();
    }

    // ===============================================================================================================
    PanellEq::PanellEq (std::shared_ptr<Equalitzador> e) : q (std::move (e))
    {
        setWantsKeyboardFocus (true);
        addAndMakeVisible (grafica);
        addAndMakeVisible (llista);
        for (auto* b : { &btDesfes, &btRefes, &btBypass, &btTipus, &btPendent, &btEncesa, &btDuplica, &btEsborra }) addAndMakeVisible (b);
        addAndMakeVisible (segRang);
        addAndMakeVisible (pomSortida);
        addAndMakeVisible (segCanal);
        for (auto* pm : { &pomFreq, &pomGuany, &pomQ }) addAndMakeVisible (pm);
        addChildComponent (editorValor);

        btDesfes.setTooltip ("Desfés (Ctrl+Z)"_ca);
        btRefes.setTooltip ("Refés (Ctrl+Y)"_ca);
        btDesfes.onClick = [this] { desfes(); };
        btRefes.onClick = [this] { refes(); };
        segRang.setTooltip ("Escala de la gràfica (fins a quants dB amunt i avall)"_ca);
        segRang.setTriat (1);
        segRang.onCanvi = [this] (int i) { const float r[] = { 6.0f, 12.0f, 24.0f, 30.0f }; grafica.rangDb = r[juce::jlimit (0, 3, i)]; grafica.invalida(); };
        btBypass.setLlum (tema::guitarra);
        btBypass.setTooltip ("Apaga tot l'EQ per comparar (amb una fosa curta, sense clic). També es pot automatitzar."_ca);
        btBypass.onClick = [this] { abansDeCanviar(); q->posaBypass (! q->bypass()); refresca(); };
        pomSortida.setTooltip ("Volum de sortida de l'EQ (-24 a +24 dB). Ctrl + clic: 0 dB"_ca);
        pomSortida.onComenca = [this] { abansDeCanviar(); };
        pomSortida.onCanvi = [this] (double v) { q->posaParametre (PSortida, (float) v); };
        pomSortida.onReinicia = [this] { abansDeCanviar(); q->posaSortida (0.0f); refresca(); };

        btTipus.setTooltip ("Tipus de filtre"_ca);
        btTipus.onClick = [this]
        {
            if (focus < 0) return;
            juce::PopupMenu m;
            const int t = q->banda (focus).tipus;
            for (int i = 0; i < numTipus; ++i) m.addItem (i + 1, u8 (nomTipus (i)), true, i == t);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (btTipus), [this] (int r)
            {
                if (r > 0) aTotes ([r] (Banda& b) { b.tipus = r - 1; if (tePendent (b.tipus) && b.q > 4.0f) b.q = qDefecte; });
            });
        };
        btPendent.setTooltip ("Pendent del passa-alts o del passa-baixos"_ca);
        btPendent.onClick = [this]
        {
            if (focus < 0) return;
            juce::PopupMenu m;
            const int pe = q->banda (focus).pendent;
            for (int i = 0; i < numPendents; ++i) m.addItem (i + 1, juce::String (dbPerOctava (i)) + " dB/oct", true, i == pe);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (btPendent), [this] (int r)
            {
                if (r > 0) aTotes ([r] (Banda& b) { b.pendent = r - 1; });
            });
        };
        segCanal.setTooltip ("A quina part del so treballa la banda: estèreo (tots dos canals), només l'esquerre o el dret, "
                             "el Mid (el que és al centre) o el Side (el que és als costats)"_ca);
        segCanal.onCanvi = [this] (int c) { aTotes ([c] (Banda& b) { b.canal = c; }); };
        btEncesa.setLlum (vintage::ledVerd);
        btEncesa.setTooltip ("Encén o apaga la banda (apagada, no toca el so)"_ca);
        btEncesa.onClick = [this]
        {
            if (focus < 0) return;
            const bool nou = ! q->banda (focus).activa;
            aTotes ([nou] (Banda& b) { b.activa = nou; });
        };
        btDuplica.setTooltip ("Fa una còpia de les bandes triades, un terç d'octava amunt (Ctrl+D)"_ca);
        btDuplica.onClick = [this] { duplicaTriades(); };
        btEsborra.setTooltip ("Esborra les bandes triades (Supr)"_ca);
        btEsborra.onClick = [this] { esborraTriades(); };

        auto prepara = [this] (Pom& pom, int quin)
        {
            pom.onComenca = [this] { abansDeCanviar(); };
            pom.onCanvi = [this, quin] (double v) { posaPom (quin, v); };
            pom.onEscriu = [this, quin, &pom] { escriuPom (quin, pom); };
            pom.onReinicia = [this, quin]
            {
                if (focus < 0) return;
                abansDeCanviar();
                aTotes ([quin] (Banda& b) { if (quin == PGuany) b.guany = 0.0f; else if (quin == PQ) b.q = qDefecte; else b.freq = 1000.0f; });
            };
        };
        prepara (pomFreq, PFreq);
        prepara (pomGuany, PGuany);
        prepara (pomQ, PQ);
        pomFreq.setTooltip ("Freqüència de la banda. Doble clic: escriu-la (1200, 1,2k…) · Ctrl + clic: 1 kHz · Maj: ajust fi"_ca);
        pomGuany.setTooltip ("Quant puja o baixa. Doble clic: escriu-lo · Ctrl + clic: 0 dB · Maj: ajust fi"_ca);
        pomQ.setTooltip ("Amplada: Q petita = ampla; Q gran = estreta. Doble clic: escriu-la · Ctrl + clic: 0,71"_ca);

        editorValor.setFont (dibuix::lletra (13.0f));
        editorValor.setJustification (juce::Justification::centred);
        editorValor.setColour (juce::TextEditor::backgroundColourId, tema::superficie);
        editorValor.setColour (juce::TextEditor::textColourId, tema::text);
        editorValor.setColour (juce::TextEditor::outlineColourId, tema::accent);
        editorValor.onReturnKey = [this]
        {
            double v;
            if (focus >= 0 && pomEscrit >= 0 && llegeixValor (editorValor.getText(), v))
            {
                const int quin = pomEscrit;
                abansDeCanviar();
                aTotes ([quin, v] (Banda& b)
                {
                    if (quin == PFreq) b.freq = (float) juce::jlimit ((double) freqMin, (double) freqMax, v);
                    else if (quin == PGuany) b.guany = (float) juce::jlimit (-(double) guanyMax, (double) guanyMax, v);
                    else b.q = (float) juce::jlimit ((double) qMin, (double) qMax, v);
                });
            }
            editorValor.setVisible (false);
            pomEscrit = -1;
            grabKeyboardFocus();
        };
        editorValor.onEscapeKey = [this] { editorValor.setVisible (false); pomEscrit = -1; grabKeyboardFocus(); };
        editorValor.onFocusLost = [this] { editorValor.setVisible (false); pomEscrit = -1; };

        setSize (1000, 600);
        refresca();
        startTimerHz (30);
    }

    void PanellEq::posaPom (int quin, double v)
    {
        if (focus < 0) return;
        // la banda del focus va al valor; les altres triades es mouen el mateix tros (en valor normalitzat)
        const int i0 = indexParam (focus, quin);
        const double d = v - q->parametre (i0);
        for (int k : triades)
        {
            const int i = indexParam (k, quin);
            q->posaParametre (i, (float) juce::jlimit (0.0, 1.0, k == focus ? v : q->parametre (i) + d));
        }
        grafica.invalida();
        llista.repaint();
    }

    void PanellEq::escriuPom (int quin, Pom& pom)
    {
        if (focus < 0) return;
        pomEscrit = quin;
        const auto b = pom.getBounds();
        editorValor.setBounds (b.getX() + 4, b.getBottom() - 24, b.getWidth() - 8, 22);
        const auto x = q->banda (focus);
        editorValor.setText (quin == PFreq ? textFreq (x.freq) : (quin == PGuany ? coma (x.guany, 1) : coma (x.q, 2)), false);
        editorValor.setVisible (true);
        editorValor.toFront (true);
        editorValor.grabKeyboardFocus();
        editorValor.selectAll();
    }

    void PanellEq::refresca()
    {
        netejaTriades();
        versioVista = q->versio();
        btBypass.setToggleState (q->bypass(), juce::dontSendNotification);
        pomSortida.setValor (q->parametre (PSortida), true);
        btDesfes.setEnabled (! desfer.empty());
        btRefes.setEnabled (! refer.empty());
        mostraControls();
        grafica.invalida();
        llista.repaint();
        repaint (zonaBanda);
    }

    void PanellEq::mostraControls()
    {
        const bool hiHa = focus >= 0;
        for (juce::Component* c : { (juce::Component*) &btTipus, (juce::Component*) &btPendent, (juce::Component*) &segCanal, (juce::Component*) &btEncesa,
                                    (juce::Component*) &btDuplica, (juce::Component*) &btEsborra, (juce::Component*) &pomFreq, (juce::Component*) &pomGuany,
                                    (juce::Component*) &pomQ })
            c->setVisible (hiHa);
        if (! hiHa) return;
        const auto b = q->banda (focus);
        btTipus.setButtonText (u8 (nomTipus (b.tipus)));
        btPendent.setButtonText (juce::String (dbPerOctava (b.pendent)) + " dB/oct");
        btPendent.setVisible (tePendent (b.tipus));
        segCanal.setTriat (b.canal);
        btEncesa.setToggleState (b.activa, juce::dontSendNotification);
        btEncesa.setButtonText (b.activa ? "Encesa" : "Apagada");
        pomFreq.setValor (q->parametre (indexParam (focus, PFreq)), true);
        pomGuany.setValor (q->parametre (indexParam (focus, PGuany)), true);
        pomGuany.setVisible (teGuany (b.tipus));
        pomQ.setValor (q->parametre (indexParam (focus, PQ)), true);
        pomQ.setVisible (teQ (b.tipus, b.pendent));
        resized();
    }

    void PanellEq::timerCallback()
    {
        if (q->versio() != versioVista) refresca();   // automatització o canvis de fora
        const float i = q->picEntrada(), o = q->picSortida();
        vuIn = std::max (i, vuIn * 0.86f);
        vuOut = std::max (o, vuOut * 0.86f);
        repaint (zonaMetres);
    }

    void PanellEq::paint (juce::Graphics& g)
    {
        g.fillAll (tema::fons);
        vintage::panell (g, getLocalBounds().removeFromTop (52).toFloat());
        auto t = zonaTitol.toFloat();
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (17.0f, true));
        g.drawText ("EQ PRO", t.removeFromTop (21.0f), juce::Justification::centredLeft);
        g.setColour (tema::accent);
        g.setFont (dibuix::lletra (10.0f, true));
        g.drawText ("MIBASE", t, juce::Justification::centredLeft);

        // metres d'entrada i de sortida
        auto m = zonaMetres.toFloat();
        auto pos = [] (double db) { return juce::jlimit (0.0, 1.0, (db + 60.0) / 66.0); };
        g.setFont (dibuix::lletra (8.5f, true));
        g.setColour (tema::suau);
        auto etiq = m.removeFromBottom (13.0f);
        g.drawText ("ENT", etiq.removeFromLeft (m.getWidth() * 0.5f), juce::Justification::centred);
        g.drawText ("SORT", etiq, juce::Justification::centred);
        vintage::vumetre (g, m.removeFromLeft (m.getWidth() * 0.5f).reduced (3.0f, 0.0f), vuIn, pos);
        vintage::vumetre (g, m.reduced (3.0f, 0.0f), vuOut, pos);

        // panell de la banda
        auto zb = zonaBanda.toFloat();
        g.setColour (tema::superficie);
        g.fillRoundedRectangle (zb, 6.0f);
        if (focus < 0)
        {
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (12.5f));
            g.drawFittedText ("Tria una banda (clic al punt) per veure'n els controls. Doble clic al buit: banda nova · arrossegar: freqüència i guany · "
                        "roda: Q · botó dret: opcions · Maj: ajust fi · Alt: un sol eix"_ca, zonaBanda.reduced (14, 6), juce::Justification::centredLeft, 3);
        }
        else
        {
            const auto b = q->banda (focus);
            auto cap = zb.reduced (12.0f, 8.0f).removeFromLeft (64.0f);
            g.setColour (colorBanda (focus));
            g.fillEllipse (cap.getX(), cap.getY() + 4.0f, 30.0f, 30.0f);
            g.setColour (juce::Colours::black);
            g.setFont (dibuix::lletra (14.0f, true));
            g.drawText (juce::String (focus + 1), juce::Rectangle<float> (cap.getX(), cap.getY() + 4.0f, 30.0f, 30.0f), juce::Justification::centred);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (10.5f));
            g.drawText (triades.size() > 1 ? juce::String ((int) triades.size()) + " triades" : juce::String ("BANDA"),
                        juce::Rectangle<float> (cap.getX() - 4.0f, cap.getY() + 38.0f, 72.0f, 14.0f), juce::Justification::centredLeft);
            if (! b.activa)
            {
                g.setColour (tema::suau);
                g.drawText ("apagada", juce::Rectangle<float> (cap.getX() - 4.0f, cap.getY() + 52.0f, 72.0f, 14.0f), juce::Justification::centredLeft);
            }
        }
    }

    void PanellEq::resized()
    {
        auto r = getLocalBounds();
        auto dalt = r.removeFromTop (52).reduced (14, 10);
        zonaTitol = dalt.removeFromLeft (110);
        btDesfes.setBounds (dalt.removeFromLeft (30));
        dalt.removeFromLeft (2);
        btRefes.setBounds (dalt.removeFromLeft (30));
        dalt.removeFromLeft (16);
        segRang.setBounds (dalt.removeFromLeft (juce::jmin (230, segRang.ampladaIdeal() + 16)));
        btBypass.setBounds (dalt.removeFromRight (90));

        r.reduce (12, 8);
        zonaBanda = r.removeFromBottom (128);
        r.removeFromBottom (4);
        llista.setBounds (r.removeFromBottom (28));
        r.removeFromBottom (4);
        zonaMetres = r.removeFromRight (44).withTrimmedBottom (14);
        r.removeFromRight (6);
        grafica.setBounds (r);

        // sortida (a baix a la dreta, fora del panell de la banda)
        pomSortida.setBounds (zonaBanda.removeFromRight (96).reduced (0, 8));
        zonaBanda.removeFromRight (8);
        // controls de la banda
        auto b = zonaBanda.reduced (12, 8);
        b.removeFromLeft (70);
        auto fila = b.removeFromTop (30);
        btTipus.setBounds (fila.removeFromLeft (120));
        fila.removeFromLeft (8);
        if (btPendent.isVisible()) { btPendent.setBounds (fila.removeFromLeft (100)); fila.removeFromLeft (8); }
        segCanal.setBounds (fila.removeFromLeft (juce::jmin (260, segCanal.ampladaIdeal() + 16)));
        fila.removeFromLeft (8);
        btEncesa.setBounds (fila.removeFromLeft (96));
        btEsborra.setBounds (fila.removeFromRight (84));
        fila.removeFromRight (4);
        btDuplica.setBounds (fila.removeFromRight (84));
        b.removeFromTop (2);
        b.removeFromTop (4);
        const int ample = 96;
        for (auto* pm : { &pomFreq, &pomGuany, &pomQ })
        {
            if (! pm->isVisible()) continue;
            pm->setBounds (b.removeFromLeft (ample));
            b.removeFromLeft (8);
        }
    }

    bool PanellEq::keyPressed (const juce::KeyPress& k)
    {
        const auto m = k.getModifiers();
        if (m.isCommandDown() && ! m.isShiftDown() && k.getKeyCode() == 'Z') { desfes(); return true; }
        if (m.isCommandDown() && (k.getKeyCode() == 'Y' || (m.isShiftDown() && k.getKeyCode() == 'Z'))) { refes(); return true; }
        if (m.isCommandDown() && k.getKeyCode() == 'A')
        {
            std::vector<int> v;
            for (int i = 0; i < maxBandes; ++i) if (q->banda (i).usada) v.push_back (i);
            triaRequadre (v, false);
            return true;
        }
        if (m.isCommandDown() && k.getKeyCode() == 'D') { duplicaTriades(); return true; }
        if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) { esborraTriades(); return true; }
        if (k == juce::KeyPress::escapeKey && ! triades.empty()) { tria (-1, false); return true; }
        return cridaTeclaGlobal (k);
    }

    void PanellEq::menuBanda (int k, juce::Component* on)
    {
        const auto b = q->banda (k);
        juce::PopupMenu m, tipus, pend, canal;
        for (int i = 0; i < numTipus; ++i) tipus.addItem (100 + i, u8 (nomTipus (i)), true, i == b.tipus);
        for (int i = 0; i < numPendents; ++i) pend.addItem (200 + i, juce::String (dbPerOctava (i)) + " dB/oct", true, i == b.pendent);
        for (int i = 0; i < numCanals; ++i) canal.addItem (300 + i, nomCanalUi (i), true, i == b.canal);
        m.addSectionHeader (juce::String (k + 1) + " · "_ca + u8 (nomTipus (b.tipus)) + " · "_ca + textFreq (b.freq));
        m.addSubMenu ("Tipus", tipus);
        if (tePendent (b.tipus)) m.addSubMenu ("Pendent", pend);
        m.addSubMenu ("Canal", canal);
        m.addItem (1, b.activa ? juce::String ("Apaga-la") : "Encén-la"_ca);
        m.addItem (2, "Edita els valors…"_ca);
        m.addSeparator();
        m.addItem (3, "Duplica");
        m.addItem (4, "Copia els ajustos");
        m.addItem (5, "Enganxa els ajustos", portaBanda().has_value());
        m.addSeparator();
        m.addItem (6, triades.size() > 1 ? "Esborra les triades" : "Esborra");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (on).withMousePosition(),
                         [this, k, sp = juce::Component::SafePointer<PanellEq> (this)] (int r)
        {
            if (sp == nullptr || r == 0) return;
            if (r >= 100 && r < 100 + numTipus) aTotes ([r] (Banda& x) { x.tipus = r - 100; });
            else if (r >= 200 && r < 200 + numPendents) aTotes ([r] (Banda& x) { x.pendent = r - 200; });
            else if (r >= 300 && r < 300 + numCanals) aTotes ([r] (Banda& x) { x.canal = r - 300; });
            else if (r == 1) { const bool nou = ! q->banda (k).activa; aTotes ([nou] (Banda& x) { x.activa = nou; }); }
            else if (r == 2) editaValors (k, {});
            else if (r == 3) duplicaTriades();
            else if (r == 4) portaBanda() = q->banda (k);
            else if (r == 5 && portaBanda())
            {
                const auto c = *portaBanda();
                aTotes ([c] (Banda& x) { const bool act = x.activa; x = c; x.usada = true; x.activa = act; });
            }
            else if (r == 6) esborraTriades();
        });
    }

    void PanellEq::menuBuit (double f, double db, juce::Component* on)
    {
        juce::PopupMenu m, nova;
        for (int i = 0; i < numTipus; ++i) nova.addItem (100 + i, u8 (nomTipus (i)));
        m.addSubMenu ("Banda nova aquí ("_ca + textFreq (f) + ")", nova, q->bandaLliure() >= 0);
        m.addItem (1, "Enganxa una banda aquí"_ca, portaBanda().has_value() && q->bandaLliure() >= 0);
        m.addSeparator();
        m.addItem (2, "Tria-les totes (Ctrl+A)");
        bool hiHa = false;
        for (int k = 0; k < maxBandes; ++k) hiHa = hiHa || q->banda (k).usada;
        m.addItem (3, "Esborra-les totes", hiHa);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (on).withMousePosition(),
                         [this, f, db, sp = juce::Component::SafePointer<PanellEq> (this)] (int r)
        {
            if (sp == nullptr || r == 0) return;
            if (r >= 100)
            {
                Banda b;
                b.tipus = r - 100;
                b.freq = (float) f;
                b.pendent = 3;
                if (teGuany (b.tipus)) b.guany = (float) (std::round (juce::jlimit (-(double) guanyMax, (double) guanyMax, db / std::max (0.5, factorNode (b.tipus))) * 10.0) / 10.0);
                if (b.tipus == Bell) b.q = 1.0f;
                if (b.tipus == Notch) b.q = 6.0f;
                creaBanda (b);
            }
            else if (r == 1 && portaBanda()) { auto b = *portaBanda(); b.freq = (float) f; creaBanda (b); }
            else if (r == 2) keyPressed (juce::KeyPress ('A', juce::ModifierKeys::commandModifier, 0));
            else if (r == 3)
            {
                abansDeCanviar();
                for (int k = 0; k < maxBandes; ++k) q->esborra (k);
                tria (-1, false);
            }
        });
    }

    void PanellEq::editaValors (int k, juce::Rectangle<int> on)
    {
        if (on.isEmpty())
        {
            const auto p = grafica.getLocalBounds().getCentre();
            on = juce::Rectangle<int> (p.x - 2, p.y - 2, 4, 4);
        }
        juce::CallOutBox::launchAsynchronously (std::make_unique<EditorValors> (*this, k), grafica.localAreaToGlobal (on), nullptr);
    }

    // ===============================================================================================================
    class FinestraEq : public juce::DocumentWindow
    {
    public:
        FinestraEq (std::shared_ptr<Equalitzador> e, const juce::String& titol, std::function<void()> enTancar)
            : juce::DocumentWindow (titol, tema::fons, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
              tancar (std::move (enTancar))
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new PanellEq (std::move (e)), true);
            setResizable (true, true);
            setResizeLimits (780, 470, 2400, 1500);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::MessageManager::callAsync ([t = tancar] { t(); }); }
        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }
    private:
        std::function<void()> tancar;
    };
}

std::unique_ptr<juce::DocumentWindow> creaFinestraEq (std::shared_ptr<Equalitzador> eq, const juce::String& titol, std::function<void()> enTancar)
{
    return std::make_unique<FinestraEq> (std::move (eq), titol, std::move (enTancar));
}
