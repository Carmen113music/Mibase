#include "FinestraReverb.h"

#include "Controls.h"
#include "DialegText.h"
#include "Dibuix.h"
#include "EscGlobal.h"
#include "Pom.h"
#include "Tema.h"
#include "Text.h"
#include "Vintage.h"

#include <array>
#include <cmath>

using namespace mibase::reverb;

namespace
{
    // Colors propis de la reverb (dins la paleta de Mibase): la cua en turquesa (com la veu), la interacció en taronja.
    const juce::Colour colCua = tema::veu;
    const juce::Colour colPantalla { 0xff101014 };

    juce::String u8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str(), (int) s.size()); }
    juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

    // ---- Del valor del paràmetre a la posició del pom (0..1) i al revés ----
    bool quadratic (int p) { return p == PreDelay || p == GateHold; }
    double aNorm (int p, double v)
    {
        const auto& i = info (p);
        if (i.mx <= i.mn) return 0.0;
        if (i.log) return std::log (juce::jlimit ((double) i.mn, (double) i.mx, v) / i.mn) / std::log ((double) i.mx / i.mn);
        const double n = juce::jlimit (0.0, 1.0, (v - i.mn) / (i.mx - i.mn));
        return quadratic (p) ? std::sqrt (n) : n;
    }
    double deNorm (int p, double n)
    {
        const auto& i = info (p);
        n = juce::jlimit (0.0, 1.0, n);
        double v = i.log ? i.mn * std::pow ((double) i.mx / i.mn, n) : i.mn + (quadratic (p) ? n * n : n) * (i.mx - i.mn);
        if (i.enter) v = std::round (v);
        return v;
    }
    bool esBipolar (int p) { return p == EqLowG || p == Eq1G || p == Eq2G || p == EqHighG || p == Sortida; }

    juce::File fitxerBiblioteca()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Mibase").getChildFile ("Reverb").getChildFile ("presets.json");
    }
    BibliotecaPresets& biblioteca()
    {
        static BibliotecaPresets b;
        static bool carregada = false;
        if (! carregada) { carregada = true; b.carrega (dibuix::cami (fitxerBiblioteca())); }
        return b;
    }
    void desaBiblioteca() { biblioteca().desa (dibuix::cami (fitxerBiblioteca())); }

    // Un preset aplicat no canvia ni la vista (Simple/Avançat) ni la qualitat triada.
    Params ambVistaIQualitat (Params p, const Params& ara)
    {
        p[ModeVista] = ara[ModeVista];
        p[Qualitat] = ara[Qualitat];
        return p;
    }
    bool mateixSo (Params a, Params b)
    {
        a[Qualitat] = b[Qualitat];
        return a.igualSo (b);
    }

    void pintaCaixa (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& titol)
    {
        g.setColour (tema::superficie);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (tema::linia);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);
        if (titol.isNotEmpty()) dibuix::pintaEtiquetaSeccio (g, titol, r.reduced (12, 0).removeFromTop (26));
    }
}

class PanellReverb;

//==============================================================================
/** Un pom lligat a un paràmetre de la reverb. Es mou de 0 a 1 i el paràmetre fa l'escala (lineal, log…). */
class PomR : public Pom
{
public:
    PomR (PanellReverb& o, int param, const juce::String& nom = {});
    void setParam (int p);
    int param() const noexcept { return p; }
    void refresca();
private:
    PanellReverb& owner;
    int p;
};

//==============================================================================
/** Botó d'estrella (favorit), dibuixat (sense emojis). */
class BotoEstrella : public juce::Button
{
public:
    BotoEstrella() : juce::Button ("favorit") { setMouseCursor (juce::MouseCursor::PointingHandCursor); setWantsKeyboardFocus (false); }
    void paintButton (juce::Graphics& g, bool sobre, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (3.0f);
        const float s = juce::jmin (r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre (s, s);
        juce::Path p;
        p.addStar (r.getCentre(), 5, s * 0.22f, s * 0.5f, 0.0f);
        if (getToggleState()) { g.setColour (tema::guitarra); g.fillPath (p); }
        g.setColour (getToggleState() ? tema::guitarra : (sobre ? tema::text : tema::suau));
        g.strokePath (p, juce::PathStrokeType (1.4f));
    }
};

//==============================================================================
/** La pantalla de dalt: la forma de la reverb (pre-delay, reflexions, cua i decay per bandes), el nivell real de la cua
    que passa, els nivells d'entrada i sortida i l'amplada estèreo. */
class VistaCua : public juce::Component
{
public:
    explicit VistaCua (Reverb& r) : rv (r) { historial.fill (-90.0f); }

    void tic()
    {
        const auto& m = rv.mesures();
        const float cua = m.cua.load();
        historial[(size_t) pos] = cua > 1e-6f ? 20.0f * std::log10 (cua) : -90.0f;
        pos = (pos + 1) % (int) historial.size();
        auto cau = [] (float& v, float nou) { v = nou > v ? nou : v * 0.86f; };
        cau (picE, m.entrada.load());
        cau (picS, m.sortida.load());
        amplada += (m.amplada.load() - amplada) * 0.2f;
        correlacio += (m.correlacio.load() - correlacio) * 0.2f;
        ducking = m.ducking.load();
        gate = m.gate.load();
        freeze = m.freeze.load();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto p = rv.params();
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (r.translated (0, 1), 8.0f);
        g.setColour (colPantalla);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (tema::linia);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

        auto dreta = r.removeFromRight (118.0f).reduced (8.0f, 10.0f);
        auto zona = r.reduced (14.0f, 10.0f);
        auto cap = zona.removeFromTop (18.0f);
        zona.removeFromBottom (14.0f);

        // Capçalera: algorisme i indicadors.
        const int a = (int) p[Alg];
        g.setFont (dibuix::lletra (12.5f, true));
        g.setColour (colCua);
        g.drawText (dibuix::majuscules (u8 (nomAlgorisme (a))), cap, juce::Justification::centredLeft);
        g.setFont (dibuix::lletra (11.5f));
        g.setColour (tema::suau);
        g.drawFittedText (u8 (descripcioAlgorisme (a)), cap.withTrimmedLeft (130.0f).withTrimmedRight (210.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
        auto ind = cap.removeFromRight (205.0f);
        auto indicador = [&] (const juce::String& t, bool on, juce::Colour c)
        {
            auto z = ind.removeFromRight (64.0f).reduced (2.0f, 1.0f);
            g.setColour (on ? c.withAlpha (0.22f) : juce::Colours::white.withAlpha (0.03f));
            g.fillRoundedRectangle (z, 3.0f);
            g.setColour (on ? c : tema::linia2);
            g.setFont (dibuix::lletra (10.0f, true));
            g.drawText (t, z, juce::Justification::centred);
        };
        indicador ("FREEZE", freeze > 0.01f, colCua);
        indicador (ducking > 0.5f ? "DUCK " + juce::String (-ducking, 0) : juce::String ("DUCK"), p[DuckOn] > 0.5f && ducking > 0.5f, tema::accent);
        indicador ("GATE", p[GateOn] > 0.5f && gate > 0.5f, tema::sinte);

        // Escales: temps i dB.
        const double bpm = rv.bpm();
        const double pre = predelaySegons (p, bpm);
        const double rtMig = decaimentA (p, 1000.0), rtGreu = decaimentA (p, 150.0), rtAgut = decaimentA (p, 6000.0);
        const bool infinit = rtMig > 1e6;
        const double rtVist = infinit ? 8.0 : juce::jmin (25.0, rtMig);
        double T = juce::jlimit (0.6, 14.0, pre + rtVist * 1.1 + 0.1);
        if (a == Reverse) T = juce::jlimit (0.6, 4.0, pre + juce::jlimit (0.15, 2.5, (double) p[Decay]) * 1.4);
        if (a == Gated) T = juce::jlimit (0.6, 3.0, pre + juce::jlimit (0.06, 1.4, p[Decay] * 0.35) * 2.2);
        if (a == Early) T = juce::jlimit (0.3, 1.0, pre + 0.25);
        auto xDe = [&] (double t) { return zona.getX() + (float) (t / T) * zona.getWidth(); };
        auto yDe = [&] (double db) { return zona.getY() + (float) (juce::jlimit (0.0, 66.0, -db) / 66.0) * zona.getHeight(); };

        g.setFont (dibuix::mono (9.5f));
        for (double db : { -12.0, -24.0, -36.0, -48.0, -60.0 })
        {
            g.setColour (juce::Colours::white.withAlpha (0.045f));
            g.fillRect (zona.getX(), yDe (db), zona.getWidth(), 1.0f);
        }
        const double pas = T <= 1.0 ? 0.1 : (T <= 3.0 ? 0.5 : (T <= 8.0 ? 1.0 : 2.0));
        for (double t = pas; t < T; t += pas)
        {
            const float x = xDe (t);
            g.setColour (juce::Colours::white.withAlpha (0.045f));
            g.fillRect (x, zona.getY(), 1.0f, zona.getHeight());
            g.setColour (tema::suau.withAlpha (0.6f));
            g.drawText (pas < 1.0 ? juce::String (t, 1) + " s" : juce::String ((int) std::lround (t)) + " s",
                        juce::Rectangle<float> (x - 20.0f, zona.getBottom() + 1.0f, 40.0f, 12.0f), juce::Justification::centred);
        }

        // Nivell real de la cua (els últims segons, de dreta a esquerra).
        {
            // Els últims T segons: a la dreta, ara (a la mateixa escala de temps que la corba).
            juce::Path h;
            const int n = juce::jlimit (2, (int) historial.size(), (int) std::lround (T * 30.0));
            h.startNewSubPath (zona.getRight(), zona.getBottom());
            for (int i = 0; i < n; ++i)
            {
                const int k = (pos - 1 - i + (int) historial.size()) % (int) historial.size();
                const float x = zona.getRight() - zona.getWidth() * (float) i / (float) (n - 1);
                h.lineTo (x, yDe (historial[(size_t) k] + 6.0));
            }
            h.lineTo (zona.getRight() - zona.getWidth(), zona.getBottom());
            h.closeSubPath();
            g.setColour (colCua.withAlpha (0.10f));
            g.fillPath (h);
        }

        // So original i pre-delay.
        g.setColour (tema::text.withAlpha (0.85f));
        g.fillRect (xDe (0.0), yDe (0.0), 2.0f, zona.getBottom() - yDe (0.0));
        if (pre > 0.002)
        {
            const float x0 = xDe (0.0) + 3.0f, x1 = xDe (pre);
            const float y = zona.getY() + 22.0f;
            g.setColour (tema::accent.withAlpha (0.8f));
            g.drawArrow ({ x0, y, x1, y }, 1.2f, 6.0f, 6.0f);
            g.setFont (dibuix::lletra (10.0f, true));
            const auto txt = "PRE " + (p[PreSync] > 0.5f ? u8 (nomNota ((int) p[PreNota])) : juce::String (juce::roundToInt (pre * 1000.0)) + " ms");
            auto et = juce::Rectangle<float> (x1 + 6.0f, y - 7.0f, juce::GlyphArrangement::getStringWidth (dibuix::lletra (10.0f, true), txt) + 10.0f, 14.0f);
            g.setColour (colPantalla.withAlpha (0.85f));
            g.fillRoundedRectangle (et, 3.0f);
            g.setColour (tema::accent);
            g.drawText (txt, et, juce::Justification::centred);
        }

        // Reflexions primerenques.
        if (a != Spring && a != Reverse && a != Shimmer && a != Ambient && a != Plate)
            for (const auto& [t, gn] : reflexionsVisibles (p))
            {
                if (pre + t > T) continue;
                const double db = 20.0 * std::log10 (juce::jmax (1e-4, (double) std::fabs (gn) * 1.6));
                g.setColour (tema::guitarra.withAlpha (0.75f));
                g.fillRect (xDe (pre + t), yDe (db), 1.5f, zona.getBottom() - yDe (db));
            }

        // Envolupant de la cua (forma de l'algorisme) i decay per bandes.
        if (a != Early)
        {
            const float nivell = a == Reverse ? 0.0f : -3.0f - 9.0f * (1.0f - juce::jmin (1.0f, 2.0f * p[EarlyLate] / 100.0f));
            auto corba = [&] (double rt, bool omple, juce::Colour c, float gruix)
            {
                juce::Path cp;
                bool primer = true;
                const int passos = 160;
                for (int i = 0; i <= passos; ++i)
                {
                    const double t = T * i / passos;
                    double db = -200.0;
                    const double tt = t - pre;
                    if (tt >= 0.0)
                    {
                        if (a == Gated)
                        {
                            const double hold = juce::jlimit (0.06, 1.4, p[Decay] * 0.35);
                            db = tt < 0.01 ? nivell - 20.0 * (1.0 - tt / 0.01) : (tt < hold ? nivell - 2.0 * tt / hold : nivell - 2.0 - (tt - hold) * 600.0);
                        }
                        else if (a == Reverse)
                        {
                            const double L = juce::jlimit (0.15, 2.5, (double) p[Decay]);
                            db = tt < L ? -42.0 + 42.0 * std::pow (tt / L, 0.6) : -200.0;
                        }
                        else
                        {
                            const double atac = a == Ambient || a == Shimmer ? 0.12 : (a == Hall ? 0.05 : 0.015);
                            const double creix = tt < atac ? -14.0 * (1.0 - tt / atac) : 0.0;
                            db = nivell + creix - (rt > 1e6 ? 0.0 : 60.0 * tt / rt);
                        }
                    }
                    const float x = xDe (t), y = yDe (db);
                    if (primer) { cp.startNewSubPath (x, y); primer = false; }
                    else cp.lineTo (x, y);
                }
                if (omple)
                {
                    juce::Path ple (cp);
                    ple.lineTo (zona.getRight(), zona.getBottom());
                    ple.lineTo (xDe (0.0), zona.getBottom());
                    ple.closeSubPath();
                    g.setGradientFill (juce::ColourGradient (c.withAlpha (0.35f), 0, zona.getY(), c.withAlpha (0.04f), 0, zona.getBottom(), false));
                    g.fillPath (ple);
                }
                g.setColour (c);
                g.strokePath (cp, juce::PathStrokeType (gruix));
            };
            if (a != Gated && a != Reverse)
            {
                corba (rtGreu, false, tema::bateria.withAlpha (0.8f), 1.0f);
                corba (rtAgut, false, tema::guitarra.withAlpha (0.8f), 1.0f);
            }
            corba (rtMig, true, colCua, 2.0f);
        }

        // Text del decay.
        g.setFont (dibuix::mono (10.5f));
        g.setColour (tema::textMig);
        juce::String t = infinit ? juce::String ("Cua infinita") : "1 kHz " + u8 (textValor (Decay, (float) rtMig));
        if (! infinit && a != Gated && a != Reverse && a != Early)
            t << "   greus " << u8 (textValor (Decay, (float) juce::jmin (99.0, rtGreu))) << " · aguts " << u8 (textValor (Decay, (float) juce::jmin (99.0, rtAgut)));
        g.drawText (t, zona.removeFromTop (16.0f).withTrimmedLeft (zona.getWidth() * 0.35f), juce::Justification::centredRight);

        // Columna dreta: nivells d'entrada i sortida, i amplada estèreo.
        auto mesures = dreta.removeFromLeft (40.0f);
        auto barra = [&] (juce::Rectangle<float> z, float pic, const juce::String& nom)
        {
            auto lbl = z.removeFromBottom (12.0f);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.fillRoundedRectangle (z, 2.0f);
            const float db = pic > 1e-6f ? 20.0f * std::log10 (pic) : -90.0f;
            const float f = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 63.0f);
            auto ple = z.withTrimmedTop (z.getHeight() * (1.0f - f));
            g.setGradientFill (juce::ColourGradient (tema::error, 0, z.getY(), tema::sinte, 0, z.getBottom(), false));
            g.fillRoundedRectangle (ple, 2.0f);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (9.5f, true));
            g.drawText (nom, lbl, juce::Justification::centred);
        };
        barra (mesures.removeFromLeft (16.0f), picE, "E");
        mesures.removeFromLeft (6.0f);
        barra (mesures.removeFromLeft (16.0f), picS, "S");
        dreta.removeFromLeft (8.0f);
        // Amplada: un ventall que s'obre.
        auto zA = dreta.removeFromTop (dreta.getWidth()).reduced (2.0f);
        const auto c = juce::Point<float> (zA.getCentreX(), zA.getBottom() - 6.0f);
        const float rad = zA.getWidth() * 0.5f;
        const float pi = juce::MathConstants<float>::pi;
        juce::Path fons;
        fons.addPieSegment (c.x - rad, c.y - rad, rad * 2, rad * 2, -pi * 0.5f, pi * 0.5f, 0.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillPath (fons);
        const float ang = juce::jlimit (0.04f, 1.0f, amplada) * pi * 0.5f;
        juce::Path ven;
        ven.addPieSegment (c.x - rad, c.y - rad, rad * 2, rad * 2, -ang, ang, 0.0f);
        g.setColour (colCua.withAlpha (0.55f));
        g.fillPath (ven);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (9.5f, true));
        g.drawText ("ESTÈREO"_ca, dreta.removeFromTop (12.0f), juce::Justification::centred);
        g.setFont (dibuix::mono (10.0f));
        g.setColour (correlacio < 0.0f ? tema::errorText : tema::textMig);
        g.drawText ("corr " + juce::String (correlacio, 2), dreta.removeFromTop (14.0f), juce::Justification::centred);
    }

private:
    Reverb& rv;
    std::array<float, 450> historial {};
    int pos = 0;
    float picE = 0, picS = 0, amplada = 0, correlacio = 1, ducking = 0, gate = 1, freeze = 0;
};

//==============================================================================
/** Base dels dos gràfics (EQ i Decay EQ): eix de freqüències logarítmic i punts que s'arrosseguen. */
class GraficFreq : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit GraficFreq (PanellReverb& o) : owner (o) {}
    std::function<void (int punt)> onTria;   // quan es tria un punt (per als poms del costat)
    int triat = -1;

protected:
    PanellReverb& owner;
    int arrossegant = -1;
    juce::Rectangle<float> zona() const { return getLocalBounds().toFloat().reduced (36.0f, 14.0f).withTrimmedBottom (4.0f); }
    float xDe (double f) const { auto z = zona(); return z.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * z.getWidth(); }
    double fDe (float x) const { auto z = zona(); return 20.0 * std::pow (1000.0, juce::jlimit (0.0, 1.0, (double) ((x - z.getX()) / z.getWidth()))); }

    void pintaFons (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (colPantalla);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (tema::linia);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        const auto z = zona();
        g.setFont (dibuix::mono (9.5f));
        for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
        {
            const float x = xDe (f);
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRect (x, z.getY(), 1.0f, z.getHeight());
            g.setColour (tema::suau.withAlpha (0.6f));
            g.drawText (f >= 1000.0 ? juce::String ((int) (f / 1000.0)) + "k" : juce::String ((int) f), juce::Rectangle<float> (x - 16.0f, z.getBottom() + 1.0f, 32.0f, 12.0f), juce::Justification::centred);
        }
    }

    void pintaPunt (juce::Graphics& g, juce::Point<float> c, juce::Colour col, bool sel, const juce::String& etiqueta)
    {
        const float rad = sel ? 7.5f : 6.0f;
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (c.x - rad, c.y - rad + 1.0f, rad * 2, rad * 2);
        g.setColour (sel ? col : col.withAlpha (0.75f));
        g.fillEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2);
        g.setColour (juce::Colours::white.withAlpha (sel ? 0.9f : 0.4f));
        g.drawEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2, 1.2f);
        g.setColour (tema::fons);
        g.setFont (dibuix::lletra (9.0f, true));
        g.drawText (etiqueta, juce::Rectangle<float> (c.x - rad, c.y - rad, rad * 2, rad * 2), juce::Justification::centred);
    }
};

class GraficEq : public GraficFreq
{
public:
    using GraficFreq::GraficFreq;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent&) override { arrossegant = -1; }
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    // punts: 0 Low Cut, 1 greus, 2 banda 1, 3 banda 2, 4 aguts, 5 High Cut
    static constexpr int numPunts = 6;
    static int paramF (int i) { const int v[] = { LowCut, EqLowF, Eq1F, Eq2F, EqHighF, HighCut }; return v[i]; }
    static int paramG (int i) { const int v[] = { -1, EqLowG, Eq1G, Eq2G, EqHighG, -1 }; return v[i]; }
    static int paramQ (int i) { return i == 2 ? Eq1Q : (i == 3 ? Eq2Q : -1); }
private:
    float yDe (double db) const { auto z = zona(); return z.getCentreY() - (float) (db / 18.0) * z.getHeight() * 0.5f; }
    double dbDe (float y) const { auto z = zona(); return juce::jlimit (-15.0, 15.0, -(double) (y - z.getCentreY()) / (z.getHeight() * 0.5) * 18.0); }
    juce::Point<float> punt (int i) const;
    int puntA (juce::Point<float> p) const
    {
        int millor = -1;
        float dist = 14.0f;
        for (int i = 0; i < numPunts; ++i)
            if (const float d = punt (i).getDistanceFrom (p); d < dist) { dist = d; millor = i; }
        return millor;
    }
};

class GraficDecay : public GraficFreq
{
public:
    using GraficFreq::GraficFreq;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent&) override { arrossegant = -1; }
    void mouseDoubleClick (const juce::MouseEvent& e) override;
private:
    float yDe (double s) const { auto z = zona(); return z.getBottom() - (float) (std::log (juce::jlimit (0.05, 40.0, s) / 0.05) / std::log (800.0)) * z.getHeight(); }
    double sDe (float y) const { auto z = zona(); return 0.05 * std::pow (800.0, juce::jlimit (0.0, 1.0, (double) ((z.getBottom() - y) / z.getHeight()))); }
    juce::Point<float> punt (int i) const;
};

//==============================================================================
class NavegadorPresets;

/** Tot el contingut de la finestra. */
class PanellReverb : public juce::Component, private juce::Timer
{
public:
    PanellReverb (std::shared_ptr<Reverb> r);
    ~PanellReverb() override;

    Reverb& reverb() { return *rv; }
    /** Abans d'un canvi (un gest o un clic): es recorda l'estat per poder-lo desfer. */
    void abansDeCanviar();
    void posa (int p, double v);
    void escriu (PomR& pom);
    void aplicaPreset (const Preset& pr, bool usuari);
    void refresca();
    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& k) override;
    void tancaNavegador();
    juce::String nomPreset;   // l'últim preset carregat
    bool presetUsuari = false;
    Params basePreset;

private:
    void timerCallback() override;
    void mostraAvancat (bool a, bool canviaMida);
    void triaPestanya (int t);
    void desfes();
    void refes();
    void canviaAB (int k);
    void pasPreset (int d);
    void menuAlgorisme();
    void obreNavegador();
    void desaComA();
    void actualitzaTextPreset();
    void reconeixPreset();
    std::vector<std::pair<juce::String, bool>> llistaPlana() const;   // (nom, usuari)

    std::shared_ptr<Reverb> rv;
    juce::TooltipWindow tips { this, 500 };

    // Barra de dalt.
    Boto btAlg { "Hall", dibuix::Icona::FletxaAvall, Boto::Estil::Secundari };
    Boto btAnt { {}, dibuix::Icona::Rebobina, Boto::Estil::Subtil }, btSeg { {}, dibuix::Icona::Endavant, Boto::Estil::Subtil };
    Boto btPreset { "Medium Hall", std::nullopt, Boto::Estil::Subtil };
    BotoEstrella btFav;
    Boto btDesa { "Desa", dibuix::Icona::Disc, Boto::Estil::Secundari };
    Segmentat segAB { { "A", "B" } };
    Boto btCopiaAB { "A=B", std::nullopt, Boto::Estil::Subtil };
    Boto btDesfes { {}, dibuix::Icona::Desfes, Boto::Estil::Subtil }, btRefes { {}, dibuix::Icona::Refes, Boto::Estil::Subtil };
    Segmentat segVista { { "Simple", "Avançat"_ca } };

    VistaCua vista;

    // Poms principals.
    PomR pMix, pDecay, pPre, pSize, pWidth, pDamp, pDif, pEL;
    Boto btPreSync { "Sync", std::nullopt, Boto::Estil::Tecla };

    // Pestanyes avançades.
    Segmentat segTabs { { "EQ", "Decay EQ", "Dinàmica"_ca, "Moviment", "Creatiu", "Estèreo i sortida"_ca } };
    int pestanya = 0;
    GraficEq grafEq { *this };
    PomR pLowCut, pHighCut, pBandaF, pBandaG, pBandaQ;
    GraficDecay grafDecay { *this };
    PomR pDecGreus, pCreuGreu, pDecAguts, pCreuAgut, pDamp2;
    Boto btDuck { "Ducking", std::nullopt, Boto::Estil::Tecla }, btGate { "Gate", std::nullopt, Boto::Estil::Tecla };
    PomR pDThr, pDAtk, pDRel, pDAmt, pGThr, pGAtk, pGHold, pGRel;
    Segmentat segMod { { "Chorus", "Aleatori", "Vintage" } };
    Boto btModSync { "Sync", std::nullopt, Boto::Estil::Tecla };
    PomR pRate, pDepth;
    Boto btShim { "Shimmer", std::nullopt, Boto::Estil::Tecla };
    Segmentat segPitch { { "+12", "+7", "+5", "-12" }, true };
    PomR pShimAmt;
    Boto btFreeze { "FREEZE", std::nullopt, Boto::Estil::Tecla };
    PomR pFreezeRel;
    Boto btRev { "Reverse", std::nullopt, Boto::Estil::Tecla };
    PomR pFb;
    PomR pWidth2, pSep;
    Boto btMono { "Greus en mono", std::nullopt, Boto::Estil::Tecla };
    Segmentat segEntrada { { "Estèreo"_ca, "Mono → Estèreo"_ca } };
    Segmentat segQual { { "Eco", "Normal", "Alta" } };
    PomR pOut;

    std::vector<PomR*> poms;
    std::vector<juce::Component*> comps[6];   // què es veu a cada pestanya
    juce::Rectangle<int> zonaTabs, caixes[6][3];

    // Escriure un valor a mà.
    juce::TextEditor editorValor;
    PomR* pomEscrit = nullptr;

    // Desfer, A/B.
    std::vector<Params> desfer, refer;
    Params ab[2];
    int abActiu = 0;
    std::uint32_t versioVista = 0;

    std::unique_ptr<NavegadorPresets> navegador;
    std::unique_ptr<DialegText> dialeg;
    bool avancat = false;
};

//==============================================================================
PomR::PomR (PanellReverb& o, int param, const juce::String& nomPom)
    : Pom (nomPom.isEmpty() ? u8 (info (param).nom) : nomPom, 0.0, 1.0, aNorm (param, info (param).def), esBipolar (param),
           [this] (double n) { return u8 (textValor (p, (float) deNorm (p, n))); }),
      owner (o), p (param)
{
    onComenca = [this] { owner.abansDeCanviar(); };
    onCanvi = [this] (double n) { owner.posa (p, deNorm (p, n)); };
    onEscriu = [this] { owner.escriu (*this); };
    onReinicia = [this] { owner.abansDeCanviar(); owner.posa (p, info (p).def); refresca(); };
    setParam (param);
}

void PomR::setParam (int nou)
{
    p = nou;
    const auto& i = info (p);
    setTooltip (u8 (i.ajuda) + (i.enter ? juce::String() : "\nDoble clic: escriu el valor · Ctrl + clic: valor inicial · Maj: ajust fi"_ca));
    refresca();
}

void PomR::refresca() { setValor (aNorm (p, owner.reverb().valor (p)), true); }

//==============================================================================
/** El navegador de presets: cercador, categories, favorits, fàbrica / meus, i gestió dels presets personals. */
class NavegadorPresets : public juce::Component, private juce::ListBoxModel, private juce::TextEditor::Listener
{
public:
    explicit NavegadorPresets (PanellReverb& o) : owner (o)
    {
        cerca.setTextToShowWhenEmpty ("Cerca un preset (nom, categoria o descripció)…"_ca, tema::suau);
        cerca.setFont (dibuix::lletra (14.0f));
        cerca.setIndents (10, 7);
        cerca.addListener (this);
        addAndMakeVisible (cerca);
        filtre.onCanvi = [this] (int) { actualitza(); };
        addAndMakeVisible (filtre);
        for (auto* l : { &cats, &llista })
        {
            l->setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff18181c));
            l->setColour (juce::ListBox::outlineColourId, tema::linia);
            l->setOutlineThickness (1);
            addAndMakeVisible (*l);
        }
        cats.setModel (&modelCats);
        cats.setRowHeight (26);
        llista.setModel (this);
        llista.setRowHeight (28);
        for (auto* b : { &btReanomena, &btMou, &btEsborra, &btTanca }) addAndMakeVisible (*b);
        btTanca.onClick = [this] { owner.tancaNavegador(); };
        btReanomena.onClick = [this] { reanomena(); };
        btMou.onClick = [this] { mou(); };
        btEsborra.onClick = [this] { esborra(); };
        modelCats.onTria = [this] (int) { actualitza(); };
        actualitza();
        // Situa la selecció al preset actual.
        for (int i = 0; i < (int) files.size(); ++i)
            if (files[(size_t) i].nom == owner.nomPreset) { llista.selectRow (i); llista.scrollToEnsureRowIsOnscreen (i); }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (tema::fons.withAlpha (0.97f));
        g.setColour (tema::linia);
        g.drawRect (getLocalBounds());
        auto peu = getLocalBounds().reduced (14).removeFromBottom (34);
        const int sel = llista.getSelectedRow();
        if (sel >= 0 && sel < (int) files.size())
        {
            g.setColour (tema::textMig);
            g.setFont (dibuix::lletra (13.0f));
            g.drawFittedText (files[(size_t) sel].desc, peu.withTrimmedRight (420), juce::Justification::centredLeft, 2, 0.8f);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14);
        auto dalt = r.removeFromTop (34);
        btTanca.setBounds (dalt.removeFromRight (90));
        dalt.removeFromRight (10);
        filtre.setBounds (dalt.removeFromRight (filtre.ampladaIdeal()));
        dalt.removeFromRight (10);
        cerca.setBounds (dalt);
        r.removeFromTop (10);
        auto peu = r.removeFromBottom (34);
        btEsborra.setBounds (peu.removeFromRight (110));
        peu.removeFromRight (8);
        btMou.setBounds (peu.removeFromRight (140));
        peu.removeFromRight (8);
        btReanomena.setBounds (peu.removeFromRight (130));
        r.removeFromBottom (10);
        cats.setBounds (r.removeFromLeft (200));
        r.removeFromLeft (10);
        llista.setBounds (r);
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey) { owner.tancaNavegador(); return true; }
        if (k == juce::KeyPress::returnKey) { carregaTriat (true); return true; }
        return false;
    }

private:
    struct Fila { juce::String nom, cat, desc; bool usuari; bool fav; const Preset* pr; };

    struct ModelCats : public juce::ListBoxModel
    {
        juce::StringArray noms;
        int triada = 0;
        std::function<void (int)> onTria;
        int getNumRows() override { return noms.size(); }
        void paintListBoxItem (int fila, juce::Graphics& g, int w, int h, bool) override
        {
            const bool sel = fila == triada;
            const bool capcalera = noms[fila].startsWith ("#");
            if (sel) { g.setColour (tema::triat); g.fillRect (0, 0, w, h); g.setColour (tema::accent); g.fillRect (0, 4, 3, h - 8); }
            g.setColour (capcalera ? tema::suau : (sel ? tema::text : tema::textMig));
            g.setFont (dibuix::lletra (capcalera ? 10.5f : 13.0f, capcalera || sel));
            g.drawText (capcalera ? dibuix::majuscules (noms[fila].substring (1)) : noms[fila], 12, 0, w - 16, h, juce::Justification::centredLeft);
        }
        void listBoxItemClicked (int fila, const juce::MouseEvent&) override
        {
            if (noms[fila].startsWith ("#")) return;
            triada = fila;
            if (onTria) onTria (fila);
        }
    };

    int getNumRows() override { return (int) files.size(); }
    void paintListBoxItem (int fila, juce::Graphics& g, int w, int h, bool sel) override
    {
        if (fila < 0 || fila >= (int) files.size()) return;
        const auto& f = files[(size_t) fila];
        if (sel) { g.setColour (tema::triat); g.fillRect (0, 0, w, h); g.setColour (tema::accent); g.fillRect (0, 4, 3, h - 8); }
        // Estrella.
        juce::Path p;
        p.addStar ({ 18.0f, h * 0.5f }, 5, 3.2f, 7.5f, 0.0f);
        if (f.fav) { g.setColour (tema::guitarra); g.fillPath (p); }
        g.setColour (f.fav ? tema::guitarra : tema::linia2);
        g.strokePath (p, juce::PathStrokeType (1.2f));
        g.setColour (f.nom == owner.nomPreset ? tema::accent : tema::text);
        g.setFont (dibuix::lletra (13.5f, sel));
        g.drawText (f.nom, 34, 0, w - 230, h, juce::Justification::centredLeft);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.5f));
        g.drawText ((f.usuari ? "Meu · "_ca : juce::String()) + f.cat + " · " + u8 (nomAlgorisme ((int) f.pr->p[Alg])), w - 230, 0, 222, h, juce::Justification::centredRight);
    }
    void listBoxItemClicked (int fila, const juce::MouseEvent& e) override
    {
        if (fila < 0 || fila >= (int) files.size()) return;
        if (e.x < 32)
        {
            biblioteca().commutaFavorit (files[(size_t) fila].usuari ? BibliotecaPresets::clauUsuari (files[(size_t) fila].nom.toStdString())
                                                                    : BibliotecaPresets::clauFabrica (files[(size_t) fila].nom.toStdString()));
            desaBiblioteca();
            actualitza();
            return;
        }
        carregaTriat (false);   // un clic: s'escolta al moment
        repaint();
    }
    void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override { carregaTriat (true); }
    void selectedRowsChanged (int) override { repaint(); }
    void textEditorTextChanged (juce::TextEditor&) override { actualitza(); }

    void carregaTriat (bool tanca)
    {
        const int fila = llista.getSelectedRow();
        if (fila < 0 || fila >= (int) files.size()) return;
        owner.aplicaPreset (*files[(size_t) fila].pr, files[(size_t) fila].usuari);
        llista.repaint();
        if (tanca) owner.tancaNavegador();
    }

    void actualitza()
    {
        // Categories: Totes, les de fàbrica i les carpetes de l'usuari.
        const auto anterior = modelCats.noms[modelCats.triada];
        modelCats.noms.clear();
        modelCats.noms.add ("Totes");
        modelCats.noms.add ("#Fàbrica"_ca);
        for (const auto& c : categories()) modelCats.noms.add (u8 (c));
        const auto carpetes = biblioteca().carpetes();
        if (! carpetes.empty())
        {
            modelCats.noms.add ("#Meus presets");
            for (const auto& c : carpetes) modelCats.noms.add ("  " + u8 (c));
        }
        modelCats.triada = juce::jmax (0, modelCats.noms.indexOf (anterior));
        cats.updateContent();
        cats.repaint();

        const auto catTriada = modelCats.noms[modelCats.triada].trim();
        const bool totes = modelCats.triada == 0;
        const auto text = cerca.getText().toStdString();
        const int f = filtre.getTriat();   // 0 tots, 1 favorits, 2 fàbrica, 3 meus
        files.clear();
        auto afegeix = [&] (const Preset& p, bool usuari)
        {
            const auto clau = usuari ? BibliotecaPresets::clauUsuari (p.nom) : BibliotecaPresets::clauFabrica (p.nom);
            const bool fav = biblioteca().esFavorit (clau);
            if (f == 1 && ! fav) return;
            if (f == 2 && usuari) return;
            if (f == 3 && ! usuari) return;
            if (! totes && u8 (p.categoria) != catTriada) return;
            if (! coincideix (p.nom + " " + p.categoria + " " + p.descripcio + " " + nomAlgorisme ((int) p.p[Alg]), text)) return;
            files.push_back ({ u8 (p.nom), u8 (p.categoria), u8 (p.descripcio), usuari, fav, &p });
        };
        for (const auto& p : biblioteca().usuari) afegeix (p, true);
        for (const auto& p : presetsFabrica()) afegeix (p, false);
        llista.updateContent();
        llista.repaint();
        const bool hiHaMeu = ! biblioteca().usuari.empty();
        btReanomena.setEnabled (hiHaMeu);
        btMou.setEnabled (hiHaMeu);
        btEsborra.setEnabled (hiHaMeu);
    }

    const Fila* filaMeva()
    {
        const int fila = llista.getSelectedRow();
        if (fila < 0 || fila >= (int) files.size() || ! files[(size_t) fila].usuari)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Presets"_ca,
                                                    "Tria primer un dels teus presets (els de fàbrica no es poden canviar)."_ca);
            return nullptr;
        }
        return &files[(size_t) fila];
    }

    void reanomena()
    {
        auto* f = filaMeva();
        if (f == nullptr) return;
        const auto vell = f->nom.toStdString();
        dialeg = std::make_unique<DialegText> ("Reanomena el preset"_ca, "Nom nou per a «"_ca + f->nom + "»."_ca, "Reanomena"_ca,
            [vell] (const juce::String& t) -> juce::String
            {
                if (t.trim().isEmpty()) return "Escriu un nom."_ca;
                if (t.trim().toStdString() != vell && biblioteca().busca (t.trim().toStdString()) != nullptr) return "Ja tens un preset amb aquest nom."_ca;
                return {};
            },
            [this, vell] (const juce::String& t)
            {
                const auto nou = t.trim().toStdString();
                if (biblioteca().reanomena (vell, nou))
                {
                    desaBiblioteca();
                    if (owner.nomPreset == u8 (vell)) owner.nomPreset = u8 (nou);
                }
                actualitza();
                owner.refresca();
            }, f->nom);
        DialegText::mostra (*this, dialeg);
    }

    void mou()
    {
        auto* f = filaMeva();
        if (f == nullptr) return;
        const auto nom = f->nom.toStdString();
        juce::PopupMenu m;
        m.addSectionHeader ("Mou «"_ca + f->nom + "» a…"_ca);
        const auto carpetes = biblioteca().carpetes();
        for (int i = 0; i < (int) carpetes.size(); ++i) m.addItem (i + 1, u8 (carpetes[(size_t) i]), u8 (carpetes[(size_t) i]) != f->cat);
        m.addSeparator();
        m.addItem (1000, "Carpeta nova…"_ca);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btMou), [this, nom, carpetes] (int r)
        {
            if (r >= 1 && r <= (int) carpetes.size()) { biblioteca().mou (nom, carpetes[(size_t) r - 1]); desaBiblioteca(); actualitza(); }
            else if (r == 1000)
            {
                dialeg = std::make_unique<DialegText> ("Carpeta nova"_ca, "Nom de la carpeta on aniran els teus presets."_ca, "Mou-lo"_ca,
                    [] (const juce::String& t) { return t.trim().isEmpty() ? "Escriu un nom."_ca : juce::String(); },
                    [this, nom] (const juce::String& t) { biblioteca().mou (nom, t.trim().toStdString()); desaBiblioteca(); actualitza(); });
                DialegText::mostra (*this, dialeg);
            }
        });
    }

    void esborra()
    {
        auto* f = filaMeva();
        if (f == nullptr) return;
        const auto nom = f->nom.toStdString();
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Esborra el preset"_ca,
            "Segur que vols esborrar «"_ca + f->nom + "»? No es pot desfer."_ca, "Esborra"_ca, "Cancel·la"_ca, this,
            juce::ModalCallbackFunction::create ([this, nom] (int r)
            {
                if (r == 0) return;
                biblioteca().esborra (nom);
                desaBiblioteca();
                actualitza();
            }));
    }

    PanellReverb& owner;
    juce::TextEditor cerca;
    Segmentat filtre { { "Tots", "Favorits", "Fàbrica"_ca, "Meus" } };
    ModelCats modelCats;
    juce::ListBox cats, llista;
    std::vector<Fila> files;
    Boto btReanomena { "Reanomena", dibuix::Icona::Llapis }, btMou { "Mou a carpeta", dibuix::Icona::Carpeta },
         btEsborra { "Esborra", dibuix::Icona::Goma }, btTanca { "Tanca", dibuix::Icona::Tanca };
    std::unique_ptr<DialegText> dialeg;
};

//==============================================================================
// Gràfic de l'EQ
//==============================================================================
juce::Point<float> GraficEq::punt (int i) const
{
    const auto& rv = const_cast<PanellReverb&> (owner).reverb();
    const double f = rv.valor (paramF (i));
    const double db = paramG (i) >= 0 ? rv.valor (paramG (i)) : 0.0;
    return { xDe (f), yDe (paramG (i) >= 0 ? db : (i == 0 ? -0.0 : 0.0)) };
}

void GraficEq::paint (juce::Graphics& g)
{
    pintaFons (g);
    const auto z = zona();
    const auto p = owner.reverb().params();
    g.setFont (dibuix::mono (9.5f));
    for (double db : { -12.0, -6.0, 0.0, 6.0, 12.0 })
    {
        g.setColour (juce::Colours::white.withAlpha (std::abs (db) < 0.1 ? 0.12f : 0.05f));
        g.fillRect (z.getX(), yDe (db), z.getWidth(), 1.0f);
        g.setColour (tema::suau.withAlpha (0.6f));
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), juce::Rectangle<float> (4.0f, yDe (db) - 6.0f, 28.0f, 12.0f), juce::Justification::centredRight);
    }
    juce::Path c;
    for (int i = 0; i <= 200; ++i)
    {
        const double f = 20.0 * std::pow (1000.0, i / 200.0);
        const float x = xDe (f), y = yDe (juce::jlimit (-20.0, 20.0, respostaEq (p, f)));
        if (i == 0) c.startNewSubPath (x, y); else c.lineTo (x, y);
    }
    juce::Path ple (c);
    ple.lineTo (z.getRight(), yDe (0.0));
    ple.lineTo (z.getX(), yDe (0.0));
    ple.closeSubPath();
    g.setColour (colCua.withAlpha (0.12f));
    g.fillPath (ple);
    g.setColour (colCua);
    g.strokePath (c, juce::PathStrokeType (2.0f));
    const char* etiq[] = { "L", "1", "2", "3", "4", "H" };
    const juce::Colour cols[] = { tema::suau, tema::bateria, tema::accent, tema::guitarra, tema::sinte, tema::suau };
    for (int i = 0; i < numPunts; ++i)
    {
        auto pt = punt (i);
        if (paramG (i) < 0) pt.y = yDe (0.0);
        pintaPunt (g, pt, cols[i], i == triat, etiq[i]);
    }
    g.setColour (tema::suau);
    g.setFont (dibuix::lletra (10.5f));
    g.drawText ("Arrossega els punts · roda: amplada (Q) · doble clic: 0 dB"_ca, z.withHeight (14.0f).translated (0, 2.0f), juce::Justification::centredRight);
}

void GraficEq::mouseDown (const juce::MouseEvent& e)
{
    arrossegant = puntA (e.position);
    if (arrossegant >= 0)
    {
        triat = arrossegant;
        if (onTria) onTria (triat);
        owner.abansDeCanviar();
    }
    repaint();
}

void GraficEq::mouseDrag (const juce::MouseEvent& e)
{
    if (arrossegant < 0) return;
    owner.posa (paramF (arrossegant), fDe (e.position.x));
    if (paramG (arrossegant) >= 0) owner.posa (paramG (arrossegant), dbDe (e.position.y));
    owner.refresca();
}

void GraficEq::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int i = puntA (e.position);
    if (i < 0 || paramG (i) < 0) return;
    owner.abansDeCanviar();
    owner.posa (paramG (i), 0.0);
    owner.refresca();
}

void GraficEq::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int i = triat >= 0 ? triat : puntA (e.position);
    if (i < 0 || paramQ (i) < 0) return;
    const double q = owner.reverb().valor (paramQ (i)) * std::pow (1.15, w.deltaY > 0 ? 1.0 : -1.0);
    owner.posa (paramQ (i), q);
    owner.refresca();
}

//==============================================================================
// Gràfic del Decay EQ (durada de la cua per freqüència)
//==============================================================================
juce::Point<float> GraficDecay::punt (int i) const
{
    const auto& rv = const_cast<PanellReverb&> (owner).reverb();
    const double d = rv.valor (Decay);
    if (i == 0) return { xDe (rv.valor (CreuGreu)), yDe (d * rv.valor (DecGreus)) };
    if (i == 2) return { xDe (rv.valor (CreuAgut)), yDe (d * rv.valor (DecAguts)) };
    return { xDe (std::sqrt (rv.valor (CreuGreu) * rv.valor (CreuAgut))), yDe (d) };
}

void GraficDecay::paint (juce::Graphics& g)
{
    pintaFons (g);
    const auto z = zona();
    const auto p = owner.reverb().params();
    g.setFont (dibuix::mono (9.5f));
    for (double s : { 0.1, 0.3, 1.0, 3.0, 10.0, 30.0 })
    {
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRect (z.getX(), yDe (s), z.getWidth(), 1.0f);
        g.setColour (tema::suau.withAlpha (0.6f));
        g.drawText (s < 1.0 ? juce::String (s, 1) : juce::String ((int) s) + " s", juce::Rectangle<float> (2.0f, yDe (s) - 6.0f, 30.0f, 12.0f), juce::Justification::centredRight);
    }
    // Zones de greus i aguts.
    g.setColour (tema::bateria.withAlpha (0.06f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (z.getX(), z.getY(), xDe (p[CreuGreu]), z.getBottom()));
    g.setColour (tema::guitarra.withAlpha (0.06f));
    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (xDe (p[CreuAgut]), z.getY(), z.getRight(), z.getBottom()));
    juce::Path c;
    for (int i = 0; i <= 200; ++i)
    {
        const double f = 20.0 * std::pow (1000.0, i / 200.0);
        const float x = xDe (f), y = yDe (juce::jmin (40.0, decaimentA (p, f)));
        if (i == 0) c.startNewSubPath (x, y); else c.lineTo (x, y);
    }
    g.setColour (colCua);
    g.strokePath (c, juce::PathStrokeType (2.0f));
    const juce::Colour cols[] = { tema::bateria, tema::accent, tema::guitarra };
    const char* etiq[] = { "G", "M", "A" };
    for (int i = 0; i < 3; ++i) pintaPunt (g, punt (i), cols[i], i == triat, etiq[i]);
    g.setColour (tema::suau);
    g.setFont (dibuix::lletra (10.5f));
    g.drawText ("Quant dura la cua a cada freqüència · G greus · M mitjans (Decay) · A aguts"_ca, z.withHeight (14.0f).translated (0, 2.0f), juce::Justification::centredRight);
}

void GraficDecay::mouseDown (const juce::MouseEvent& e)
{
    arrossegant = -1;
    float dist = 14.0f;
    for (int i = 0; i < 3; ++i)
        if (const float d = punt (i).getDistanceFrom (e.position); d < dist) { dist = d; arrossegant = i; }
    if (arrossegant >= 0) { triat = arrossegant; owner.abansDeCanviar(); }
    repaint();
}

void GraficDecay::mouseDrag (const juce::MouseEvent& e)
{
    if (arrossegant < 0) return;
    auto& rv = owner.reverb();
    const double s = sDe (e.position.y), f = fDe (e.position.x);
    if (arrossegant == 1) owner.posa (Decay, s);
    else if (arrossegant == 0) { owner.posa (DecGreus, s / rv.valor (Decay)); owner.posa (CreuGreu, f); }
    else { owner.posa (DecAguts, s / rv.valor (Decay)); owner.posa (CreuAgut, f); }
    owner.refresca();
}

void GraficDecay::mouseDoubleClick (const juce::MouseEvent& e)
{
    for (int i = 0; i < 3; ++i)
        if (punt (i).getDistanceFrom (e.position) < 14.0f && i != 1)
        {
            owner.abansDeCanviar();
            owner.posa (i == 0 ? DecGreus : DecAguts, 1.0);
            owner.refresca();
        }
}

//==============================================================================
// El panell
//==============================================================================
PanellReverb::PanellReverb (std::shared_ptr<Reverb> r)
    : rv (std::move (r)), vista (*rv),
      pMix (*this, Mix), pDecay (*this, Decay), pPre (*this, PreDelay), pSize (*this, Size), pWidth (*this, Width),
      pDamp (*this, Damping), pDif (*this, Diffusion), pEL (*this, EarlyLate),
      pLowCut (*this, LowCut), pHighCut (*this, HighCut), pBandaF (*this, Eq1F, "Freq."), pBandaG (*this, Eq1G, "Guany"), pBandaQ (*this, Eq1Q, "Q"),
      pDecGreus (*this, DecGreus), pCreuGreu (*this, CreuGreu), pDecAguts (*this, DecAguts), pCreuAgut (*this, CreuAgut), pDamp2 (*this, Damping),
      pDThr (*this, DuckThr), pDAtk (*this, DuckAtk), pDRel (*this, DuckRel), pDAmt (*this, DuckAmt),
      pGThr (*this, GateThr), pGAtk (*this, GateAtk), pGHold (*this, GateHold), pGRel (*this, GateRel),
      pRate (*this, ModRate), pDepth (*this, ModDepth), pShimAmt (*this, ShimAmt), pFreezeRel (*this, FreezeRel), pFb (*this, Feedback),
      pWidth2 (*this, Width), pSep (*this, Separacio), pOut (*this, Sortida)
{
    setWantsKeyboardFocus (true);
    ab[0] = ab[1] = rv->params();
    reconeixPreset();

    // Barra de dalt.
    for (juce::Component* c : { (juce::Component*) &btAlg, (juce::Component*) &btAnt, (juce::Component*) &btSeg, (juce::Component*) &btPreset,
                                (juce::Component*) &btFav, (juce::Component*) &btDesa, (juce::Component*) &segAB, (juce::Component*) &btCopiaAB,
                                (juce::Component*) &btDesfes, (juce::Component*) &btRefes, (juce::Component*) &segVista })
        addAndMakeVisible (c);
    btAlg.setTooltip ("Tria l'algorisme (el tipus d'espai). Cada un té un caràcter propi."_ca);
    btAlg.onClick = [this] { menuAlgorisme(); };
    btAnt.setTooltip ("Preset anterior"_ca);
    btSeg.setTooltip ("Preset següent"_ca);
    btAnt.onClick = [this] { pasPreset (-1); };
    btSeg.onClick = [this] { pasPreset (1); };
    btPreset.setTooltip ("Obre la biblioteca de presets: cerca, categories, favorits i els teus presets."_ca);
    btPreset.onClick = [this] { obreNavegador(); };
    btFav.setTooltip ("Marca aquest preset com a favorit."_ca);
    btFav.onClick = [this]
    {
        if (nomPreset.isEmpty()) return;
        biblioteca().commutaFavorit (presetUsuari ? BibliotecaPresets::clauUsuari (nomPreset.toStdString()) : BibliotecaPresets::clauFabrica (nomPreset.toStdString()));
        desaBiblioteca();
        actualitzaTextPreset();
    };
    btDesa.setTooltip ("Desa els ajustos d'ara com un preset teu."_ca);
    btDesa.onClick = [this] { desaComA(); };
    segAB.setTooltip ("Compara dos ajustos: A i B. Canvia d'un a l'altre per escoltar la diferència."_ca);
    segAB.onCanvi = [this] (int k) { canviaAB (k); };
    btCopiaAB.setTooltip ("Copia l'ajust que sona a l'altre (A i B iguals)."_ca);
    btCopiaAB.onClick = [this] { ab[0] = ab[1] = rv->params(); };
    btDesfes.setTooltip ("Desfés (Ctrl + Z)"_ca);
    btRefes.setTooltip ("Refés (Ctrl + Y)"_ca);
    btDesfes.onClick = [this] { desfes(); };
    btRefes.onClick = [this] { refes(); };
    segVista.setTooltip ("Simple: només el més important. Avançat: totes les seccions. No canvia cap ajust."_ca);
    segVista.onCanvi = [this] (int k)
    {
        rv->posa (ModeVista, (float) k);   // només és de la finestra: no entra al desfer
        mostraAvancat (k == 1, true);
    };

    addAndMakeVisible (vista);

    // Poms principals.
    for (auto* p : { &pMix, &pDecay, &pPre, &pSize, &pWidth, &pDamp, &pDif, &pEL }) { addAndMakeVisible (p); poms.push_back (p); }
    addAndMakeVisible (btPreSync);
    btPreSync.setTooltip (u8 (info (PreSync).ajuda));
    btPreSync.setLlum (tema::accent);
    btPreSync.onClick = [this] { abansDeCanviar(); posa (PreSync, rv->valor (PreSync) > 0.5f ? 0 : 1); refresca(); };

    // Pestanyes.
    addAndMakeVisible (segTabs);
    segTabs.onCanvi = [this] (int t) { triaPestanya (t); };
    auto boto = [this] (Boto& b, int p, juce::Colour llum)
    {
        b.setLlum (llum);
        b.setTooltip (u8 (info (p).ajuda));
        b.onClick = [this, p] { abansDeCanviar(); posa (p, rv->valor (p) > 0.5f ? 0 : 1); refresca(); };
    };
    auto seg = [this] (Segmentat& s, int p)
    {
        s.setTooltip (u8 (info (p).ajuda));
        s.onCanvi = [this, p] (int i) { abansDeCanviar(); posa (p, i); refresca(); };
    };
    boto (btDuck, DuckOn, tema::accent);
    boto (btGate, GateOn, tema::sinte);
    boto (btModSync, ModSync, tema::accent);
    boto (btShim, ShimOn, colCua);
    boto (btFreeze, FreezeOn, colCua);
    boto (btRev, RevOn, tema::bateria);
    boto (btMono, GreusMono, tema::sinte);
    seg (segMod, ModMode);
    seg (segPitch, ShimPitch);
    seg (segEntrada, EntradaMode);
    seg (segQual, Qualitat);
    btFreeze.setMidaText (15.0f);

    grafEq.onTria = [this] (int i)
    {
        const int f = GraficEq::paramF (i), g = GraficEq::paramG (i), q = GraficEq::paramQ (i);
        pBandaF.setParam (f);
        pBandaG.setVisible (g >= 0 && pestanya == 0);
        if (g >= 0) pBandaG.setParam (g);
        pBandaQ.setVisible (q >= 0 && pestanya == 0);
        if (q >= 0) pBandaQ.setParam (q);
        repaint();
    };
    grafEq.triat = 2;

    comps[0] = { &grafEq, &pLowCut, &pHighCut, &pBandaF, &pBandaG, &pBandaQ };
    comps[1] = { &grafDecay, &pDecGreus, &pCreuGreu, &pDecAguts, &pCreuAgut, &pDamp2 };
    comps[2] = { &btDuck, &pDThr, &pDAtk, &pDRel, &pDAmt, &btGate, &pGThr, &pGAtk, &pGHold, &pGRel };
    comps[3] = { &segMod, &pRate, &btModSync, &pDepth };
    comps[4] = { &btShim, &segPitch, &pShimAmt, &btFreeze, &pFreezeRel, &btRev, &pFb };
    comps[5] = { &pWidth2, &pSep, &btMono, &segEntrada, &segQual, &pOut };
    for (auto& v : comps)
        for (auto* c : v)
        {
            addChildComponent (c);
            if (auto* p = dynamic_cast<PomR*> (c)) poms.push_back (p);
        }

    editorValor.setFont (dibuix::mono (13.0f));
    editorValor.setJustification (juce::Justification::centred);
    editorValor.setColour (juce::TextEditor::backgroundColourId, tema::fons);
    editorValor.setColour (juce::TextEditor::outlineColourId, tema::accent);
    editorValor.onReturnKey = [this]
    {
        if (pomEscrit != nullptr)
        {
            float v = 0.0f;
            if (llegeixValor (pomEscrit->param(), editorValor.getText().toStdString(), v))
            {
                abansDeCanviar();
                posa (pomEscrit->param(), v);
            }
        }
        editorValor.setVisible (false);
        pomEscrit = nullptr;
        refresca();
        grabKeyboardFocus();
    };
    editorValor.onEscapeKey = [this] { editorValor.setVisible (false); pomEscrit = nullptr; grabKeyboardFocus(); };
    editorValor.onFocusLost = [this] { editorValor.setVisible (false); pomEscrit = nullptr; };
    addChildComponent (editorValor);

    avancat = rv->valor (ModeVista) > 0.5f;
    segVista.setTriat (avancat ? 1 : 0);
    mostraAvancat (avancat, false);
    setSize (940, avancat ? 680 : 430);
    refresca();
    startTimerHz (30);
}

PanellReverb::~PanellReverb() { stopTimer(); }

void PanellReverb::abansDeCanviar()
{
    desfer.push_back (rv->params());
    if (desfer.size() > 200) desfer.erase (desfer.begin());
    refer.clear();
}

void PanellReverb::posa (int p, double v)
{
    rv->posa (p, (float) v);
    if (p == Alg) btAlg.setButtonText (u8 (nomAlgorisme ((int) rv->valor (Alg))));
}

void PanellReverb::escriu (PomR& pom)
{
    if (info (pom.param()).enter) return;
    pomEscrit = &pom;
    const auto b = pom.getBounds();
    editorValor.setBounds (b.getX() + 4, b.getBottom() - 24, b.getWidth() - 8, 22);
    editorValor.setText (u8 (textValor (pom.param(), rv->valor (pom.param()))), false);
    editorValor.setVisible (true);
    editorValor.toFront (true);
    editorValor.grabKeyboardFocus();
    editorValor.selectAll();
}

void PanellReverb::desfes()
{
    if (desfer.empty()) return;
    refer.push_back (rv->params());
    auto p = desfer.back();
    desfer.pop_back();
    rv->posaParams (p);
    rv->posa (ModeVista, avancat ? 1.0f : 0.0f);
    refresca();
}

void PanellReverb::refes()
{
    if (refer.empty()) return;
    desfer.push_back (rv->params());
    auto p = refer.back();
    refer.pop_back();
    rv->posaParams (p);
    rv->posa (ModeVista, avancat ? 1.0f : 0.0f);
    refresca();
}

void PanellReverb::canviaAB (int k)
{
    if (k == abActiu) return;
    ab[abActiu] = rv->params();
    abActiu = k;
    abansDeCanviar();
    auto p = ab[k];
    p[ModeVista] = avancat ? 1.0f : 0.0f;
    rv->posaParams (p);
    reconeixPreset();
    refresca();
}

void PanellReverb::aplicaPreset (const Preset& pr, bool usuari)
{
    abansDeCanviar();
    rv->posaParams (ambVistaIQualitat (pr.p, rv->params()));
    nomPreset = u8 (pr.nom);
    presetUsuari = usuari;
    basePreset = rv->params();
    refresca();
}

std::vector<std::pair<juce::String, bool>> PanellReverb::llistaPlana() const
{
    std::vector<std::pair<juce::String, bool>> v;
    for (const auto& p : presetsFabrica()) v.push_back ({ u8 (p.nom), false });
    for (const auto& p : biblioteca().usuari) v.push_back ({ u8 (p.nom), true });
    return v;
}

void PanellReverb::pasPreset (int d)
{
    const auto v = llistaPlana();
    int i = -1;
    for (int k = 0; k < (int) v.size(); ++k)
        if (v[(size_t) k].first == nomPreset && v[(size_t) k].second == presetUsuari) i = k;
    i = (i < 0 ? (d > 0 ? 0 : (int) v.size() - 1) : (i + d + (int) v.size()) % (int) v.size());
    const auto& [nom, usuari] = v[(size_t) i];
    if (usuari) { if (auto* p = biblioteca().busca (nom.toStdString())) aplicaPreset (*p, true); }
    else
        for (const auto& p : presetsFabrica())
            if (u8 (p.nom) == nom) { aplicaPreset (p, false); break; }
}

void PanellReverb::reconeixPreset()
{
    const auto ara = rv->params();
    nomPreset.clear();
    for (const auto& p : biblioteca().usuari)
        if (mateixSo (p.p, ara)) { nomPreset = u8 (p.nom); presetUsuari = true; basePreset = ara; return; }
    for (const auto& p : presetsFabrica())
        if (mateixSo (p.p, ara)) { nomPreset = u8 (p.nom); presetUsuari = false; basePreset = ara; return; }
}

void PanellReverb::actualitzaTextPreset()
{
    const bool modificat = nomPreset.isNotEmpty() && ! mateixSo (basePreset, rv->params());
    btPreset.setButtonText (nomPreset.isEmpty() ? "(sense preset)"_ca : nomPreset + (modificat ? " *" : ""));
    btPreset.setMidaText (15.0f);
    btFav.setToggleState (nomPreset.isNotEmpty() && biblioteca().esFavorit (presetUsuari ? BibliotecaPresets::clauUsuari (nomPreset.toStdString())
                                                                                         : BibliotecaPresets::clauFabrica (nomPreset.toStdString())),
                          juce::dontSendNotification);
    btFav.setEnabled (nomPreset.isNotEmpty());
}

void PanellReverb::menuAlgorisme()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Algorisme"_ca);
    const int ara = (int) rv->valor (Alg);
    for (int a = 0; a < numAlgorismes; ++a)
    {
        juce::PopupMenu::Item it (u8 (nomAlgorisme (a)));
        it.itemID = a + 1;
        it.isTicked = a == ara;
        m.addItem (it);
        if (a == Shimmer || a == Reverse) m.addSeparator();
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btAlg), [this] (int r)
    {
        if (r <= 0) return;
        abansDeCanviar();
        posa (Alg, r - 1);
        refresca();
    });
}

void PanellReverb::obreNavegador()
{
    if (navegador != nullptr) { tancaNavegador(); return; }
    navegador = std::make_unique<NavegadorPresets> (*this);
    addAndMakeVisible (*navegador);
    navegador->setBounds (getLocalBounds().withTrimmedTop (56));
    navegador->grabKeyboardFocus();
}

void PanellReverb::tancaNavegador()
{
    if (navegador == nullptr) return;
    juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<PanellReverb> (this)]
    {
        if (sp != nullptr) { sp->navegador.reset(); sp->refresca(); sp->grabKeyboardFocus(); }
    });
}

void PanellReverb::desaComA()
{
    const auto suggerit = nomPreset.isNotEmpty() ? u8 (biblioteca().nomLliure (nomPreset.toStdString())) : juce::String ("El meu preset");
    dialeg = std::make_unique<DialegText> ("Desa el preset"_ca, "Es desarà a «Meus presets». Després el pots moure de carpeta o reanomenar."_ca, "Desa"_ca,
        [] (const juce::String& t) -> juce::String
        {
            if (t.trim().isEmpty()) return "Escriu un nom."_ca;
            for (const auto& p : presetsFabrica()) if (u8 (p.nom) == t.trim()) return "Aquest nom ja és d'un preset de fàbrica."_ca;
            return {};
        },
        [this] (const juce::String& t)
        {
            Preset p;
            p.nom = t.trim().toStdString();
            const auto* antic = biblioteca().busca (p.nom);
            p.categoria = antic != nullptr ? antic->categoria : std::string ("Meus presets");
            p.descripcio = antic != nullptr ? antic->descripcio : std::string ();
            p.p = rv->params();
            biblioteca().desaPreset (p);
            desaBiblioteca();
            nomPreset = t.trim();
            presetUsuari = true;
            basePreset = rv->params();
            refresca();
        }, suggerit);
    DialegText::mostra (*this, dialeg);
}

void PanellReverb::mostraAvancat (bool a, bool canviaMida)
{
    avancat = a;
    for (auto* p : { &pDamp, &pDif, &pEL }) p->setVisible (a);
    segTabs.setVisible (a);
    triaPestanya (pestanya);
    if (canviaMida)
        if (auto* w = findParentComponentOfClass<juce::DocumentWindow>())
        {
            const int alt = a ? 680 : 430;
            w->setSize (w->getWidth(), alt + w->getTitleBarHeight());
            setSize (getWidth(), alt);
        }
    resized();
    repaint();
}

void PanellReverb::triaPestanya (int t)
{
    pestanya = t;
    segTabs.setTriat (t);
    for (int k = 0; k < 6; ++k)
        for (auto* c : comps[k]) c->setVisible (avancat && k == t);
    if (avancat && t == 0 && grafEq.onTria) grafEq.onTria (grafEq.triat);
    resized();
    repaint();
}

void PanellReverb::refresca()
{
    for (auto* p : poms)
    {
        // Pre-delay i rate: amb Sync, el pom tria la nota.
        if (p == &pPre) p->setParam (rv->valor (PreSync) > 0.5f ? PreNota : PreDelay);
        else if (p == &pRate) p->setParam (rv->valor (ModSync) > 0.5f ? ModNota : ModRate);
        else p->refresca();
    }
    btAlg.setButtonText (u8 (nomAlgorisme ((int) rv->valor (Alg))));
    btPreSync.setToggleState (rv->valor (PreSync) > 0.5f, juce::dontSendNotification);
    for (auto [b, p] : { std::pair<Boto*, int> { &btDuck, DuckOn }, { &btGate, GateOn }, { &btModSync, ModSync }, { &btShim, ShimOn },
                          { &btFreeze, FreezeOn }, { &btRev, RevOn }, { &btMono, GreusMono } })
        b->setToggleState (rv->valor (p) > 0.5f, juce::dontSendNotification);
    segMod.setTriat ((int) rv->valor (ModMode));
    segPitch.setTriat ((int) rv->valor (ShimPitch));
    segEntrada.setTriat ((int) rv->valor (EntradaMode));
    segQual.setTriat ((int) rv->valor (Qualitat));
    // Si l'algorisme ja fa shimmer, l'interruptor no cal.
    const bool algShim = (int) rv->valor (Alg) == Shimmer;
    btShim.setEnabled (! algShim);
    btShim.setToggleState (algShim || rv->valor (ShimOn) > 0.5f, juce::dontSendNotification);
    const bool shimOn = algShim || rv->valor (ShimOn) > 0.5f;
    segPitch.setEnabled (shimOn);
    pShimAmt.setEnabled (shimOn);
    for (auto* p : { &pDThr, &pDAtk, &pDRel, &pDAmt }) p->setEnabled (rv->valor (DuckOn) > 0.5f);
    for (auto* p : { &pGThr, &pGAtk, &pGHold, &pGRel }) p->setEnabled (rv->valor (GateOn) > 0.5f);
    const int a = (int) rv->valor (Alg);
    pEL.setEnabled (a != Early && a != Reverse && a != Spring && a != Ambient && a != Shimmer && a != Plate);
    pDecay.setEnabled (a != Early);
    btDesfes.setEnabled (! desfer.empty());
    btRefes.setEnabled (! refer.empty());
    segAB.setTriat (abActiu);
    if (nomPreset.isEmpty()) reconeixPreset();
    actualitzaTextPreset();
    versioVista = rv->versio();
    grafEq.repaint();
    grafDecay.repaint();
    vista.repaint();
    repaint();
}

void PanellReverb::timerCallback()
{
    vista.tic();
    if (rv->versio() != versioVista) refresca();
}

bool PanellReverb::keyPressed (const juce::KeyPress& k)
{
    const auto m = k.getModifiers();
    const int c = k.getKeyCode();
    const bool z = c == 'Z' || c == 'z', y = c == 'Y' || c == 'y';
    if (m.isCommandDown() && z && ! m.isShiftDown()) { desfes(); return true; }
    if (m.isCommandDown() && (y || (z && m.isShiftDown()))) { refes(); return true; }
    if (k == juce::KeyPress::escapeKey && navegador != nullptr) { tancaNavegador(); return true; }
    return cridaTeclaGlobal (k);
}

void PanellReverb::paint (juce::Graphics& g)
{
    g.fillAll (tema::fons);
    vintage::panell (g, getLocalBounds().removeFromTop (56).toFloat());
    // Títol a l'esquerra.
    auto t = getLocalBounds().removeFromTop (56).reduced (14, 9).removeFromLeft (100).toFloat();
    g.setColour (tema::text);
    g.setFont (dibuix::lletra (17.0f, true));
    g.drawText ("REVERB", t.removeFromTop (22.0f), juce::Justification::centredLeft);
    g.setColour (tema::accent);
    g.setFont (dibuix::lletra (10.0f, true));
    g.drawText ("MIBASE", t, juce::Justification::centredLeft);

    if (avancat)
    {
        for (int k = 0; k < 3; ++k)
            if (! caixes[pestanya][k].isEmpty())
            {
                const char* titols[6][3] = { { "", "", "" }, { "", "", "" }, { "Ducking", "Gate", "" }, { "Modulació", "", "" },
                                             { "Shimmer", "Freeze", "Reverse i feedback" }, { "Estèreo", "Sortida i motor", "" } };
                pintaCaixa (g, caixes[pestanya][k], u8 (titols[pestanya][k]));
            }
        if (pestanya == 2)
        {
            // Reducció del ducking i estat del gate.
            const auto& m = rv->mesures();
            auto barra = caixes[2][0].reduced (12, 0).removeFromBottom (14).translated (0, -8);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillRoundedRectangle (barra.toFloat(), 3.0f);
            const float red = juce::jlimit (0.0f, 1.0f, m.ducking.load() / 30.0f);
            g.setColour (tema::accent);
            g.fillRoundedRectangle (barra.toFloat().withWidth (barra.getWidth() * red), 3.0f);
            auto led = caixes[2][1].reduced (12, 0).removeFromBottom (14).translated (0, -8);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillRoundedRectangle (led.toFloat(), 3.0f);
            g.setColour (tema::sinte.withAlpha (rv->valor (GateOn) > 0.5f ? 0.3f + 0.7f * m.gate.load() : 0.15f));
            g.fillRoundedRectangle (led.toFloat().withWidth (led.getWidth() * (rv->valor (GateOn) > 0.5f ? m.gate.load() : 0.0f)), 3.0f);
        }
        if (pestanya == 4)
        {
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (11.0f));
            g.drawFittedText ("Freeze: la cua queda sonant i no hi entra so nou. En treure'l, s'apaga en el temps d'Alliberament."_ca,
                              caixes[4][1].reduced (12, 0).removeFromBottom (34), juce::Justification::centredLeft, 2, 0.85f);
        }
        if (pestanya == 5)
        {
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (11.0f));
            g.drawFittedText ("Qualitat: Eco gasta menys ordinador; Alta fa la cua més densa i suau. El canvi es fa amb una fosa curta."_ca,
                              caixes[5][1].reduced (12, 0).removeFromBottom (34), juce::Justification::centredLeft, 2, 0.85f);
        }
    }
}

void PanellReverb::resized()
{
    auto r = getLocalBounds();
    // Barra de dalt.
    auto dalt = r.removeFromTop (56).reduced (14, 11);
    dalt.removeFromLeft (100);
    btAlg.setBounds (dalt.removeFromLeft (170));
    btAlg.setMidaText (15.0f);
    dalt.removeFromLeft (12);
    segVista.setBounds (dalt.removeFromRight (segVista.ampladaIdeal()));
    dalt.removeFromRight (10);
    btRefes.setBounds (dalt.removeFromRight (34));
    btDesfes.setBounds (dalt.removeFromRight (34));
    dalt.removeFromRight (8);
    btCopiaAB.setBounds (dalt.removeFromRight (46));
    segAB.setBounds (dalt.removeFromRight (64));
    dalt.removeFromRight (10);
    btDesa.setBounds (dalt.removeFromRight (82));
    dalt.removeFromRight (6);
    btFav.setBounds (dalt.removeFromRight (30));
    btAnt.setBounds (dalt.removeFromLeft (30));
    btSeg.setBounds (dalt.removeFromRight (30));
    btPreset.setBounds (dalt.reduced (2, 0));

    r.reduce (14, 10);
    vista.setBounds (r.removeFromTop (avancat ? 168 : 190));
    r.removeFromTop (8);

    // Poms principals: grans i ben separats.
    auto fila = r.removeFromTop (avancat ? 122 : 150);
    std::vector<PomR*> principals { &pMix, &pDecay, &pPre, &pSize, &pWidth };
    if (avancat) for (auto* p : { &pDamp, &pDif, &pEL }) principals.push_back (p);
    const int amp = fila.getWidth() / (int) principals.size();
    for (auto* p : principals)
    {
        auto z = fila.removeFromLeft (amp).reduced (amp > 120 ? 14 : 6, 0);
        auto sota = z.removeFromBottom (24);
        if (p == &pPre) btPreSync.setBounds (sota.withSizeKeepingCentre (64, 22).translated (0, 2));
        p->setBounds (z);
    }

    if (! avancat) return;
    r.removeFromTop (8);
    segTabs.setBounds (r.removeFromTop (30).withSizeKeepingCentre (juce::jmin (r.getWidth(), segTabs.ampladaIdeal() + 120), 30));
    r.removeFromTop (8);
    zonaTabs = r;
    for (auto& c : caixes) for (auto& z : c) z = {};
    auto posaPoms = [] (juce::Rectangle<int> z, std::initializer_list<juce::Component*> l, int maxAmple = 110)
    {
        const int a = juce::jmin (maxAmple, z.getWidth() / (int) l.size());
        auto zz = z.withSizeKeepingCentre (a * (int) l.size(), z.getHeight());
        for (auto* c : l) c->setBounds (zz.removeFromLeft (a).reduced (4, 0));
    };
    auto q = r;
    switch (pestanya)
    {
        case 0:
        {
            auto dreta = q.removeFromRight (360);
            grafEq.setBounds (q.withTrimmedRight (10));
            auto dalt2 = dreta.removeFromTop (dreta.getHeight() / 2);
            posaPoms (dalt2, { &pLowCut, &pHighCut }, 120);
            posaPoms (dreta, { &pBandaF, &pBandaG, &pBandaQ }, 120);
            break;
        }
        case 1:
        {
            auto dreta = q.removeFromRight (360);
            grafDecay.setBounds (q.withTrimmedRight (10));
            auto dalt2 = dreta.removeFromTop (dreta.getHeight() / 2);
            posaPoms (dalt2, { &pDecGreus, &pCreuGreu, &pDamp2 }, 120);
            posaPoms (dreta, { &pDecAguts, &pCreuAgut }, 120);
            break;
        }
        case 2:
        {
            caixes[2][0] = q.removeFromLeft (q.getWidth() / 2 - 5);
            q.removeFromLeft (10);
            caixes[2][1] = q;
            for (int k = 0; k < 2; ++k)
            {
                auto z = caixes[2][k].reduced (12, 8);
                auto cap = z.removeFromTop (24);
                (k == 0 ? btDuck : btGate).setBounds (cap.removeFromRight (110));
                z.removeFromBottom (22);
                if (k == 0) posaPoms (z, { &pDThr, &pDAtk, &pDRel, &pDAmt });
                else posaPoms (z, { &pGThr, &pGAtk, &pGHold, &pGRel });
            }
            break;
        }
        case 3:
        {
            caixes[3][0] = q.withSizeKeepingCentre (juce::jmin (q.getWidth(), 620), q.getHeight());
            auto z = caixes[3][0].reduced (16, 8);
            z.removeFromTop (24);
            auto esq = z.removeFromLeft (230);
            segMod.setBounds (esq.removeFromTop (esq.getHeight() / 2).withSizeKeepingCentre (220, 30));
            btModSync.setBounds (esq.withSizeKeepingCentre (110, 28));
            posaPoms (z, { &pRate, &pDepth }, 130);
            break;
        }
        case 4:
        {
            const int w3 = (q.getWidth() - 20) / 3;
            caixes[4][0] = q.removeFromLeft (w3);
            q.removeFromLeft (10);
            caixes[4][1] = q.removeFromLeft (w3);
            q.removeFromLeft (10);
            caixes[4][2] = q;
            {
                auto z = caixes[4][0].reduced (12, 8);
                btShim.setBounds (z.removeFromTop (24).removeFromRight (110));
                z.removeFromTop (8);
                segPitch.setBounds (z.removeFromTop (30).withSizeKeepingCentre (juce::jmin (z.getWidth(), 220), 30));
                posaPoms (z.reduced (0, 4), { &pShimAmt }, 120);
            }
            {
                auto z = caixes[4][1].reduced (12, 8);
                z.removeFromTop (26);
                z.removeFromBottom (34);
                auto esq = z.removeFromLeft (z.getWidth() / 2);
                btFreeze.setBounds (esq.withSizeKeepingCentre (juce::jmin (esq.getWidth() - 8, 130), 56));
                posaPoms (z, { &pFreezeRel }, 120);
            }
            {
                auto z = caixes[4][2].reduced (12, 8);
                btRev.setBounds (z.removeFromTop (24).removeFromRight (110));
                posaPoms (z.reduced (0, 4), { &pFb }, 120);
            }
            break;
        }
        case 5:
        {
            caixes[5][0] = q.removeFromLeft (q.getWidth() / 2 - 5);
            q.removeFromLeft (10);
            caixes[5][1] = q;
            {
                auto z = caixes[5][0].reduced (12, 8);
                z.removeFromTop (24);
                auto dreta = z.removeFromRight (200);
                btMono.setBounds (dreta.removeFromTop (dreta.getHeight() / 2).withSizeKeepingCentre (170, 28));
                segEntrada.setBounds (dreta.withSizeKeepingCentre (juce::jmin (200, segEntrada.ampladaIdeal()), 30));
                posaPoms (z, { &pWidth2, &pSep }, 120);
            }
            {
                auto z = caixes[5][1].reduced (12, 8);
                z.removeFromTop (24);
                z.removeFromBottom (34);
                auto esq = z.removeFromLeft (z.getWidth() / 2);
                segQual.setBounds (esq.withSizeKeepingCentre (juce::jmin (esq.getWidth(), 210), 30));
                posaPoms (z, { &pOut }, 120);
            }
            break;
        }
        default: break;
    }
    if (navegador != nullptr) navegador->setBounds (getLocalBounds().withTrimmedTop (56));
}

//==============================================================================
namespace
{
    class FinestraReverb : public juce::DocumentWindow
    {
    public:
        FinestraReverb (std::shared_ptr<Reverb> r, const juce::String& titol, std::function<void()> enTancar)
            : juce::DocumentWindow (titol, tema::fons, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
              tancar (std::move (enTancar))
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new PanellReverb (std::move (r)), true);
            setResizable (true, true);
            setResizeLimits (820, 400, 1800, 1300);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::MessageManager::callAsync ([t = tancar] { t(); }); }
        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }
    private:
        std::function<void()> tancar;
    };
}

std::unique_ptr<juce::DocumentWindow> creaFinestraReverb (std::shared_ptr<Reverb> reverb, const juce::String& titol, std::function<void()> enTancar)
{
    return std::make_unique<FinestraReverb> (std::move (reverb), titol, std::move (enTancar));
}
