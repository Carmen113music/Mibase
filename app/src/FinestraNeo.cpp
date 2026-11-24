#include "FinestraNeo.h"
#include "SinteUI.h"
#include <cmath>

using namespace mibase::neo;
using namespace sinteui;

/* 0.93: Mibase NEO. Personalitat pròpia: laca negra-violeta, galtes de carboni i neó (cian, magenta i llima).
   La part de dalt sempre té les 8 macros; a sota, tres pàgines (síntesi, modulació, efectes). */
namespace
{
    constexpr int SPagina = SArp + 1;

    juce::Colour colorNeo (int s)
    {
        //                                 osc         mix         filtre      env         lfo         matriu      macros      fx          veu         arp         pàgina
        static const juce::uint32 c[] = { 0xff3ee8ff, 0xffc6ff4a, 0xffff4fd8, 0xffffb347, 0xff9b7bff, 0xffff7ab8, 0xff3ee8ff, 0xff5ca8ff, 0xffd8d0ff, 0xff9b7bff, 0xff3ee8ff };
        return juce::Colour (c[juce::jlimit (0, 10, s)]);
    }

    ConfigSinte configNeo()
    {
        ConfigSinte c;
        c.marca = "NEO";
        c.subtitol = "SÍNTESI HÍBRIDA · DISSENY DE SO"_ca;
        c.carpeta = "Neo"; c.extensio = "mbneo"; c.tipusFitxer = "mibase-neo";
        c.presets = &presetsFabrica();
        c.categories = &categories();
        c.inicial = [] { return estatInicial(); };
        c.aleatori = [] (unsigned l) { return aleatori (l); };
        c.tipusAtzar = tipusAtzar();
        c.aleatoriTipus = [] (unsigned l, int t) { return aleatori (l, t); };
        c.varia = [] (const std::string& e, unsigned l, float q) { return varia (e, l, q); };
        c.previsualitza = [] (const std::string& e, float* o, int n) { return previsualitza (e, o, n, 60); };
        c.textPrevi = "FORMA D'ONA · C4"_ca;
        c.akai = { Mac1, Mac2, Mac3, Mac4, Mac5, Mac6, Mac7, Mac8 };
        c.textAkai = "AKAI: les 8 macros (K1 caràcter, K2 moviment, K3 brillantor, K4 agressivitat, K5 amplada, K6 espai, K7 evolució, K8 morph)"_ca;
        c.colorSeccio = colorNeo;
        c.accent = juce::Colour (0xff3ee8ff);
        c.colorMarca = juce::Colour (0xffff4fd8);
        c.colorFranja = juce::Colour (0xffc6ff4a);
        c.fons = Textura::Laca; c.galtes = Textura::Carboni; c.capcalera = Textura::Carboni;
        c.seccioDalt = juce::Colour (0xff1e1833);
        c.seccioBaix = juce::Colour (0xff100c1d);
        c.ample = 1440; c.alt = 960;
        return c;
    }

    void fonsVisor (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour col)
    {
        g.setColour (juce::Colour (0xff07060d));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (col.withAlpha (0.05f));
        for (int k = 1; k < 6; ++k) g.fillRect (r.getX() + r.getWidth() * (float) k / 6.0f, r.getY() + 4.0f, 1.0f, r.getHeight() - 8.0f);
        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawRoundedRectangle (r, 6.0f, 1.2f);
    }

    /** L'oscil·lador: la wavetable en 3D (16 posicions en perspectiva) amb la posició d'ara il·luminada, o la forma del
        motor (analògic, harmònic, FM, soroll). Quan sona, la posició es mou amb la modulació (el morph animat). */
    class VisorOsc : public juce::Component
    {
    public:
        VisorOsc (Neo& s, int osc) : sinte (s), o (osc) { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics& g) override
        {
            const auto col = colorNeo (SOsc);
            auto r = getLocalBounds().toFloat();
            fonsVisor (g, r, col);
            const int b = o * 11;
            const int motor = (int) sinte.valor (O1Motor + b), taula = (int) sinte.valor (O1Taula + b);
            const bool sona = sinte.fentSo() && sinte.veusActives() > 0;
            const float morph = motor == MSoroll ? 0.0f : sinte.valor (Morph);
            const float pos = sona ? sinte.posAra (o) : juce::jlimit (0.0f, 1.0f, sinte.valor (O1Pos + b) + morph);
            const float forma = sinte.valor (O1Forma + b);
            const bool apagat = sinte.valor (O1Niv + b) <= 0.0f;
            auto zona = r.reduced (10.0f, 16.0f);
            constexpr int n = 96;
            auto cami = [&] (juce::Rectangle<float> z, auto&& f)
            {
                juce::Path p;
                for (int i = 0; i <= n; ++i)
                {
                    const float x = z.getX() + z.getWidth() * (float) i / n;
                    const float y = z.getCentreY() - z.getHeight() * 0.45f * juce::jlimit (-1.2f, 1.2f, f ((double) i / n));
                    if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                }
                return p;
            };
            if (motor == MWavetable || motor == MGranular)
            {
                // pila en perspectiva: les posicions del fons cap amunt a la dreta
                constexpr int capes = 16;
                const float dx = zona.getWidth() * 0.22f / capes, dy = zona.getHeight() * 0.42f / capes;
                auto base = zona.withTrimmedRight (zona.getWidth() * 0.22f).withTrimmedTop (zona.getHeight() * 0.42f);
                for (int k = capes - 1; k >= 0; --k)
                {
                    const float pk = (float) k / (capes - 1);
                    auto z = base.translated (dx * k, -dy * k);
                    auto p = cami (z, [&] (double f) { return mostraTaula (taula, f, pk); });
                    const float prop = 1.0f - std::min (1.0f, std::abs (pk - pos) * 4.0f);
                    g.setColour (col.withAlpha ((apagat ? 0.06f : 0.1f) + 0.25f * prop));
                    g.strokePath (p, juce::PathStrokeType (1.0f));
                }
                const float kp = pos * (capes - 1);
                auto z = base.translated (dx * kp, -dy * kp);
                auto p = cami (z, [&] (double f) { return mostraTaula (taula, f, pos); });
                g.setColour (col.withAlpha (apagat ? 0.25f : 0.35f));
                g.strokePath (p, juce::PathStrokeType (5.0f));
                g.setColour (apagat ? col.withAlpha (0.5f) : juce::Colours::white);
                g.strokePath (p, juce::PathStrokeType (1.6f));
                if (motor == MGranular)
                {
                    juce::Random rnd ((juce::int64) (juce::Time::getMillisecondCounter() / 60));
                    g.setColour (colorNeo (SMix).withAlpha (0.8f));
                    const float spray = sinte.valor (GrSpray);
                    for (int k = 0; k < 14; ++k)
                    {
                        const float gx = z.getX() + z.getWidth() * rnd.nextFloat();
                        const float gy = z.getCentreY() + (rnd.nextFloat() - 0.5f) * z.getHeight() * (0.2f + spray);
                        g.fillEllipse (gx - 2.0f, gy - 2.0f, 4.0f, 4.0f);
                    }
                }
            }
            else
            {
                const float pw = juce::jlimit (0.04f, 0.96f, (forma < 0.66f ? 0.5f : 0.5f - 0.44f * (forma - 0.66f) / 0.34f) - (pos - 0.2f) * 0.4f);
                auto ona = [&] (double f) -> float
                {
                    switch (motor)
                    {
                        case MAnalogic:
                        {
                            const float tri = 1.0f - 4.0f * std::abs ((float) f - 0.5f), serra = (float) (2.0 * f - 1.0), pul = f < pw ? 1.0f : -1.0f;
                            if (forma < 0.33f) { const float k = forma / 0.33f; return tri * (1.0f - k) + serra * k; }
                            const float k = forma < 0.66f ? (forma - 0.33f) / 0.33f : 1.0f;
                            return serra * (1.0f - k) + pul * k;
                        }
                        case MHarmonic:
                        {
                            const float e = 2.4f - 2.2f * forma, pO = std::min (1.0f, 2.0f * (1.0f - pos)), pE = std::min (1.0f, 2.0f * pos);
                            float s = 0.0f, t = 0.0f;
                            for (int h = 1; h <= 24; ++h) { float a = std::pow ((float) h, -e); if (h > 1) a *= h % 2 ? pO : pE; s += a * std::sin (6.2832f * h * (float) f); t += a; }
                            return s * 1.3f / std::max (0.2f, t);
                        }
                        case MFm:
                        {
                            static const float rat[] = { 0.5f, 1, 1.5f, 2, 2.5f, 3, 3.5f, 4, 5, 6, 7, 8 };
                            const float rr = rat[juce::jlimit (0, 11, (int) std::lround (forma * 11.0f))];
                            return std::sin (6.2832f * (float) f + pos * 8.0f * std::sin (6.2832f * rr * (float) f));
                        }
                        default:
                        {
                            const auto h = (unsigned) (f * 997.0) * 2654435761u;
                            return ((float) (h >> 8 & 0xffff) / 32768.0f - 1.0f) * (0.5f + 0.5f * (1.0f - forma));
                        }
                    }
                };
                auto p = cami (zona, ona);
                g.setColour (col.withAlpha (0.3f));
                g.strokePath (p, juce::PathStrokeType (5.0f));
                g.setColour (apagat ? col.withAlpha (0.45f) : juce::Colours::white);
                g.strokePath (p, juce::PathStrokeType (1.6f));
            }
            // textos
            static const char* motors[] = { "WAVETABLE", "ANALÒGIC", "HARMÒNIC", "FM", "GRANULAR", "SOROLL" };
            g.setFont (dibuix::lletra (9.0f, true));
            g.setColour (col);
            g.drawText (juce::String ("OSC ") + juce::String (o + 1) + "  ·  " + juce::String::fromUTF8 (motors[juce::jlimit (0, 5, motor)]),
                        r.reduced (8.0f, 3.0f).removeFromTop (12.0f), juce::Justification::centredLeft);
            g.setColour (juce::Colours::white.withAlpha (0.45f));
            if (motor == MWavetable || motor == MGranular)
                g.drawText (juce::String::fromUTF8 (nomTaula (taula)) + "  " + juce::String (juce::roundToInt (pos * 100.0f)) + " %",
                            r.reduced (8.0f, 3.0f).removeFromTop (12.0f), juce::Justification::centredRight);
            if (apagat) g.drawText ("APAGAT", r.reduced (8.0f, 3.0f).removeFromBottom (12.0f), juce::Justification::centredRight);
        }
    private:
        Neo& sinte;
        int o;
    };

    /** El seqüenciador de modulació: 16 barres que es dibuixen arrossegant; el pas que sona s'il·lumina. */
    class EditorPassos : public juce::Component
    {
    public:
        EditorPassos (Neo& s, Enllac& e) : sinte (s), enllac (e) {}
        void paint (juce::Graphics& g) override
        {
            const auto col = colorNeo (SMod);
            auto r = getLocalBounds().toFloat();
            fonsVisor (g, r, col);
            auto z = r.reduced (6.0f, 8.0f);
            const int num = juce::jlimit (2, numPassos, (int) sinte.valor (PasNum));
            const int ara = sinte.pasAra();
            const float w = z.getWidth() / numPassos, cy = z.getCentreY();
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.fillRect (z.getX(), cy, z.getWidth(), 1.0f);
            for (int i = 0; i < numPassos; ++i)
            {
                const float v = sinte.valor (Pas1 + i);
                auto barra = juce::Rectangle<float> (z.getX() + w * i + 1.5f, v >= 0.0f ? cy - v * z.getHeight() * 0.5f : cy, w - 3.0f, std::abs (v) * z.getHeight() * 0.5f);
                const bool actiu = i < num;
                g.setColour (actiu ? (i == ara ? juce::Colours::white : col.withAlpha (0.75f)) : col.withAlpha (0.15f));
                g.fillRoundedRectangle (barra.withHeight (std::max (2.0f, barra.getHeight())), 2.0f);
                if (i % 4 == 0)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.18f));
                    g.fillRect (z.getX() + w * i, z.getBottom() + 2.0f, 1.0f, 4.0f);
                }
            }
        }
        void mouseDown (const juce::MouseEvent& e) override { arrossega (e); }
        void mouseDrag (const juce::MouseEvent& e) override { arrossega (e); }
        void mouseUp (const juce::MouseEvent&) override { if (actual >= 0) enllac.acaba (actual); actual = -1; }
    private:
        Neo& sinte;
        Enllac& enllac;
        int actual = -1;
        void arrossega (const juce::MouseEvent& e)
        {
            auto z = getLocalBounds().toFloat().reduced (6.0f, 8.0f);
            const int i = juce::jlimit (0, numPassos - 1, (int) ((e.position.x - z.getX()) / (z.getWidth() / numPassos)));
            const float v = juce::jlimit (-1.0f, 1.0f, (z.getCentreY() - e.position.y) / (z.getHeight() * 0.5f));
            const int p = Pas1 + i;
            if (p != actual) { if (actual >= 0) enllac.acaba (actual); enllac.comenca (p); actual = p; }
            enllac.canvi (p, (v + 1.0f) * 0.5f);
            repaint();
        }
    };

    /** La pàgina que tenies oberta (no és part del so: no es desa amb els presets). */
    int& paginaRecordada() { static int p = 0; return p; }

    class PanellNeo : public PanellSinte
    {
    public:
        PanellNeo (std::shared_ptr<Neo> s, EstatEstudi& e, GestorPlugins* gp, std::string idPista,
                   std::function<void (std::uint8_t, std::uint8_t, std::uint8_t)> dir, std::function<void()> tanca)
            : PanellSinte (s, e, gp, std::move (idPista), configNeo(), std::move (dir), std::move (tanca)),
              neo (*s), grafic (*this, { AA, AD, AS, AR, FA, FD, FS, FR }, colorNeo (SEnv), colorNeo (SFiltre)),
              visor { { *s, 0 }, { *s, 1 }, { *s, 2 } }, passos (*s, *this)
        {
            using E = PomSB::Estil;
            // ---- sempre: macros ----
            for (int p = Mac1; p <= Mac8; ++p) macros.push_back (pom (p, E::Crom, u8 (nomMacro (p - Mac1))));
            // ---- pàgina SÍNTESI ----
            for (int o = 0; o < 3; ++o)
            {
                const int b = o * 11;
                oMotor[o] = sel (O1Motor + b, false); oTaula[o] = sel (O1Taula + b, false); oWarp[o] = sel (O1Warp + b, false);
                for (int p : { O1Pos + b, O1Forma + b, O1WarpQ + b, O1Niv + b, O1Pan + b }) oFila1[o].push_back (pom (p, E::Petit));
                for (int p : { O1Oct + b, O1Semi + b, O1Fi + b }) oFila2[o].push_back (pom (p, E::Petit));
                addAndMakeVisible (visor[o]);
                pag[0].push_back (oMotor[o]); pag[0].push_back (oTaula[o]); pag[0].push_back (oWarp[o]);
                for (auto* c : oFila1[o]) pag[0].push_back (c);
                for (auto* c : oFila2[o]) pag[0].push_back (c);
            }
            mix1 = { pom (Morph, E::Bakelita), pom (Ring, E::Petit), pom (SubNiv, E::Petit), sel (SubOct, false), pom (NzNiv, E::Petit), pom (NzColor, E::Petit) };
            mix2 = { pom (UniVeus, E::Petit), pom (UniDetune, E::Petit), pom (UniAmple, E::Petit), pom (UniBlend, E::Petit), sel (UniMode, false), intr (FaseLliure, "Lliure"_ca) };
            mix3 = { pom (GrDens, E::Petit), pom (GrMida, E::Petit), pom (GrSpray, E::Petit), pom (Fold, E::Petit), sel (DTipus, false), pom (DQuant, E::Petit, "Drive") };
            for (auto* v : { &mix1, &mix2, &mix3 }) for (auto* c : *v) pag[0].push_back (c);
            fTipus = sel (FTipus, false); fPend = sel (FPendent, false);
            fTall = pom (FTall, E::Crom); fRes = pom (FRes, E::Bakelita);
            filt1 = { pom (FEnv, E::Petit), pom (FKey, E::Petit), pom (FDrive, E::Petit), pom (FVocal, E::Petit) };
            filt2 = { pom (RsMix, E::Petit), sel (RsTipus, false), pom (RsDecay, E::Petit) };
            for (auto* c : std::initializer_list<ControlSB*> { fTipus, fPend, fTall, fRes }) pag[0].push_back (c);
            for (auto* c : filt1) pag[0].push_back (c);
            for (auto* c : filt2) pag[0].push_back (c);
            // ---- pàgina MODULACIÓ ----
            addAndMakeVisible (grafic);
            for (int p : { (int) AA, (int) AD, (int) AS, (int) AR }) faders.push_back (fad (p, colorNeo (SEnv)));
            for (int p : { (int) FA, (int) FD, (int) FS, (int) FR }) faders.push_back (fad (p, colorNeo (SFiltre)));
            for (int p : { (int) EA, (int) ED, (int) ES, (int) ER }) faders.push_back (fad (p, colorNeo (SMod)));
            for (auto* f : faders) pag[1].push_back (f);
            for (int l = 0; l < 4; ++l)
            {
                const int b = l * 5;
                lOna[l] = sel (L1Ona + b, false);
                lfo[l] = { pom (L1Vel + b, E::Petit), sel (L1Ritme + b, false), pom (L1Entrada + b, E::Petit), intr (L1Reinicia + b, "Per nota"_ca) };
                pag[1].push_back (lOna[l]);
                for (auto* c : lfo[l]) pag[1].push_back (c);
            }
            addAndMakeVisible (passos);
            filaPassos = { sel (PasRitme, false), pom (PasLlisca, E::Petit), pom (PasNum, E::Petit) };
            for (auto* c : filaPassos) pag[1].push_back (c);
            for (int k = 0; k < numSlots; ++k)
            {
                const int b = M1Font + k * 4;
                mFont[k] = sel (b, false); mVia[k] = sel (b + 1, false); mDesti[k] = sel (b + 2, false);
                mQuant[k] = pom (b + 3, E::Petit, "Quant."_ca);
                for (auto* c : std::initializer_list<ControlSB*> { mFont[k], mVia[k], mDesti[k], mQuant[k] }) pag[1].push_back (c);
            }
            // ---- pàgina EFECTES ----
            fx[0] = { pom (CrMix, E::Bakelita, "Mix"_ca), pom (CrBits, E::Petit), pom (CrMostreig, E::Petit) };
            fx[1] = { pom (ShMix, E::Bakelita, "Mix"_ca), pom (ShHz, E::Petit) };
            fx[2] = { pom (ChMix, E::Bakelita, "Mix"_ca), pom (ChVel, E::Petit), pom (ChProf, E::Petit) };
            fx[3] = { pom (DlMix, E::Bakelita, "Mix"_ca), sel (DlNota, false), pom (DlFeedback, E::Petit), intr (DlPingPong) };
            fx[4] = { pom (RvMix, E::Bakelita, "Mix"_ca), pom (RvMida, E::Petit), sel (RvTipus, false) };
            fx[5] = { pom (EsTilt, E::Bakelita), pom (EsExcita, E::Petit) };
            fx[6] = { pom (Ample, E::Bakelita), pom (Volum, E::Bakelita) };
            for (auto& v : fx) for (auto* c : v) pag[2].push_back (c);
            for (int k = 0; k < numMacros; ++k)
            {
                const int b = Mc1D1 + k * 4;
                rutes[k] = { sel (b, false), pom (b + 1, E::Petit, "Quant."_ca), sel (b + 2, false), pom (b + 3, E::Petit, "Quant."_ca) };
                for (auto* c : rutes[k]) pag[2].push_back (c);
            }
            vMode = sel (Mode, false);
            filaVeu = { pom (Veus, E::Petit), pom (Glide, E::Petit), pom (BendRang, E::Petit), pom (VelVol, E::Petit) };
            pag[2].push_back (vMode);
            for (auto* c : filaVeu) pag[2].push_back (c);
            // ---- arpegiador (sempre) ----
            for (int p : { (int) ArpMode, (int) ArpRitme, (int) ArpOct, (int) ArpGate, (int) ArpSwing, (int) ArpPatro }) filaArp.push_back (pom (p, E::Petit));
            // ---- pestanyes ----
            static const char* noms[] = { "SÍNTESI", "MODULACIÓ", "EFECTES" };
            for (int k = 0; k < 3; ++k)
            {
                auto* b = pestanyes.add (new Boto (juce::String::fromUTF8 (noms[k]), std::nullopt, Boto::Estil::Tecla));
                addAndMakeVisible (b);
                b->onClick = [this, k] { pagina = k; paginaRecordada() = k; resized(); repaint(); };
            }
            pagina = paginaRecordada();
            teclat.base = 48;
            setSize (cfg.ample, cfg.alt);
            pantalla.actualitza();
        }

        void resized() override
        {
            posaCapcalera();
            seccions.clear();
            for (int k = 0; k < 3; ++k) pestanyes[k]->setToggleState (k == pagina, juce::dontSendNotification);
            for (int k = 0; k < 3; ++k) for (auto* c : pag[k]) c->setVisible (k == pagina);
            for (auto& v : visor) v.setVisible (pagina == 0);
            pantalla.setVisible (pagina == 0);
            grafic.setVisible (pagina == 1);
            passos.setVisible (pagina == 1);
            const int W = getWidth(), H = getHeight();
            const int x0 = cfg.ampleGaltes + 8, x1 = W - cfg.ampleGaltes - 8;
            const int g = 8, ampleTot = x1 - x0;
            const int yTec = H - 114;
            int x = x0;
            auto posa = [&] (int idS, const juce::String& t, int w, int y, int h) { seccions.push_back ({ idS, t, { x, y, w, h } }); x += w + g; return seccions.back().r; };
            auto fila = [] (juce::Rectangle<int> r, const std::vector<ControlSB*>& cs)
            {
                if (cs.empty()) return;
                const int w = r.getWidth() / (int) cs.size();
                for (auto* c : cs)
                {
                    auto z = r.removeFromLeft (w);
                    c->setBounds (dynamic_cast<SelectorSB*> (c) != nullptr ? z.withSizeKeepingCentre (z.getWidth() - 6, 26) : z);
                }
            };
            auto filaP = [&] (juce::Rectangle<int> r, const std::vector<PomSB*>& cs) { fila (r, { cs.begin(), cs.end() }); };
            const int y1 = cfg.altCap + 8, hMac = 146;
            // macros i pestanyes
            {
                auto r = dins (posa (SMacro, "MACROS", ampleTot - 190 - g, y1, hMac));
                filaP (r, macros);
                auto p = dins (posa (SPagina, "PÀGINA"_ca, 190, y1, hMac));
                const int hb = p.getHeight() / 3;
                for (auto* b : pestanyes) b->setBounds (p.removeFromTop (hb).reduced (2, 3));
            }
            x = x0;
            const int y2 = y1 + hMac + g, hPag = yTec - g - y2;
            if (pagina == 0)
            {
                const int hA = 318, wO = (ampleTot - 2 * g) / 3;
                for (int o = 0; o < 3; ++o)
                {
                    auto r = dins (posa (SOsc, "OSCIL·LADOR "_ca + juce::String (o + 1), wO, y2, hA));
                    auto f = r.removeFromTop (26);
                    const int w3 = (f.getWidth() - 8) / 3;
                    oMotor[o]->setBounds (f.removeFromLeft (w3)); f.removeFromLeft (4);
                    oTaula[o]->setBounds (f.removeFromLeft (w3)); f.removeFromLeft (4);
                    oWarp[o]->setBounds (f);
                    r.removeFromTop (4);
                    visor[o].setBounds (r.removeFromTop (118));
                    r.removeFromTop (2);
                    const int hf = r.getHeight() / 2;
                    filaP (r.removeFromTop (hf), oFila1[o]);
                    auto r2 = r;
                    filaP (r2.removeFromLeft (r2.getWidth() * 3 / 5), oFila2[o]);
                    insignies[o] = r2;
                }
                x = x0;
                const int y3 = y2 + hA + g, hB = hPag - hA - g;
                {
                    auto r = dins (posa (SMix, "MESCLA · UNISON · GRANULAR · PLEC I DRIVE"_ca, 560, y3, hB));
                    const int hf = r.getHeight() / 3;
                    fila (r.removeFromTop (hf), mix1);
                    fila (r.removeFromTop (hf), mix2);
                    fila (r, mix3);
                }
                {
                    auto r = dins (posa (SFiltre, "FILTRE I RESSONADORS"_ca, 520, y3, hB));
                    auto esq = r.removeFromLeft (210);
                    auto f = esq.removeFromTop (26);
                    fTipus->setBounds (f.removeFromLeft (f.getWidth() / 2 - 2)); f.removeFromLeft (4);
                    fPend->setBounds (f);
                    esq.removeFromTop (4);
                    fRes->setBounds (esq.removeFromRight (82));
                    fTall->setBounds (esq);
                    r.removeFromLeft (8);
                    zonaRess = r.removeFromBottom (r.getHeight() / 2);
                    fila (r, filt1);
                    fila (zonaRess.withTrimmedTop (12), filt2);
                }
                {
                    auto r = dins (posa (SVeu, "SORTIDA"_ca, x0 + ampleTot - x, y3, hB));
                    zonaVu = r.removeFromRight (14).reduced (2, 2);
                    r.removeFromRight (6);
                    pantalla.setBounds (r);
                }
            }
            else if (pagina == 1)
            {
                const int hA = 262;
                {
                    auto r = dins (posa (SEnv, "ENVOLUPANTS"_ca, 520, y2, hA));
                    grafic.setBounds (r.removeFromTop (58));
                    r.removeFromTop (4);
                    titolsEnv = r.removeFromTop (14);
                    const int w = r.getWidth() / 12;
                    for (auto* f : faders) f->setBounds (r.removeFromLeft (w));
                }
                {
                    auto r = dins (posa (SLfo, "LFO"_ca, 470, y2, hA));
                    const int hf = r.getHeight() / 4;
                    for (int l = 0; l < 4; ++l)
                    {
                        auto f = r.removeFromTop (hf);
                        zonaLfo[l] = f;
                        f.removeFromLeft (48);
                        lOna[l]->setBounds (f.removeFromLeft (100).withSizeKeepingCentre (100, 24));
                        f.removeFromLeft (4);
                        auto t = f.removeFromLeft (64);
                        lfo[l][0]->setBounds (t);
                        lfo[l][1]->setBounds (f.removeFromLeft (96).withSizeKeepingCentre (92, 24));
                        lfo[l][2]->setBounds (f.removeFromLeft (f.getWidth() / 2));
                        lfo[l][3]->setBounds (f);
                    }
                }
                {
                    auto r = dins (posa (SMod, "SEQÜENCIADOR DE PASSOS"_ca, x0 + ampleTot - x, y2, hA));
                    auto baix = r.removeFromBottom (70);
                    passos.setBounds (r);
                    filaPassos[0]->setBounds (baix.removeFromLeft (baix.getWidth() / 3).withSizeKeepingCentre (baix.getWidth() / 3 - 8, 26));
                    fila (baix, { filaPassos[1], filaPassos[2] });
                }
                x = x0;
                const int y3 = y2 + hA + g;
                {
                    auto r = dins (posa (SMod, "MATRIU DE MODULACIÓ  ·  FONT × VIA → DESTÍ"_ca, ampleTot, y3, hPag - hA - g));
                    const int wc = r.getWidth() / 3, hc = r.getHeight() / 4;
                    for (int k = 0; k < numSlots; ++k)
                    {
                        auto c = juce::Rectangle<int> (r.getX() + (k / 4) * wc, r.getY() + (k % 4) * hc, wc, hc).reduced (3, 2);
                        celes[k] = c;
                        c.removeFromLeft (24);
                        barres[k] = c.removeFromBottom (5).reduced (2, 0);
                        mQuant[k]->setBounds (c.removeFromRight (52));
                        c.removeFromRight (4);
                        auto s = c.withSizeKeepingCentre (c.getWidth(), 26);
                        const int w3 = (s.getWidth() - 24) / 3;
                        mFont[k]->setBounds (s.removeFromLeft (w3)); s.removeFromLeft (12);
                        mVia[k]->setBounds (s.removeFromLeft (w3)); s.removeFromLeft (12);
                        mDesti[k]->setBounds (s);
                    }
                }
            }
            else
            {
                const int hA = 236;
                {
                    auto r = dins (posa (SFx, "RACK D'EFECTES"_ca, ampleTot, y2, hA));
                    static const int pesos[] = { 3, 2, 3, 4, 3, 2, 2 };
                    const int total = 19, amp = r.getWidth();
                    zonesFx.clear();
                    for (int b = 0; b < 7; ++b)
                    {
                        auto z = r.removeFromLeft (amp * pesos[b] / total).reduced (3, 0);
                        zonesFx.push_back (z);
                        z.removeFromTop (16);
                        fila (z.reduced (2, 6), fx[b]);
                    }
                }
                x = x0;
                const int y3 = y2 + hA + g, hB = hPag - hA - g;
                {
                    auto r = dins (posa (SMacro, "QUÈ MOU CADA MACRO"_ca, 960, y3, hB));
                    const int wc = r.getWidth() / 2, hc = r.getHeight() / 4;
                    for (int k = 0; k < numMacros; ++k)
                    {
                        auto c = juce::Rectangle<int> (r.getX() + (k / 4) * wc, r.getY() + (k % 4) * hc, wc, hc).reduced (4, 2);
                        celesMac[k] = c;
                        c.removeFromLeft (104);
                        const int w = c.getWidth() / 2;
                        auto a = c.removeFromLeft (w).reduced (2, 0);
                        rutes[k][1]->setBounds (a.removeFromRight (52));
                        rutes[k][0]->setBounds (a.withSizeKeepingCentre (a.getWidth() - 4, 26));
                        auto b = c.reduced (2, 0);
                        rutes[k][3]->setBounds (b.removeFromRight (52));
                        rutes[k][2]->setBounds (b.withSizeKeepingCentre (b.getWidth() - 4, 26));
                    }
                }
                {
                    auto r = dins (posa (SVeu, "VEU"_ca, x0 + ampleTot - x, y3, hB));
                    zonaVu = r.removeFromRight (14).reduced (2, 2);
                    r.removeFromRight (6);
                    vMode->setBounds (r.removeFromTop (28).withSizeKeepingCentre (180, 26));
                    r.removeFromTop (6);
                    const int hf = r.getHeight() / 2;
                    filaP (r.removeFromTop (hf), { filaVeu[0], filaVeu[1] });
                    filaP (r, { filaVeu[2], filaVeu[3] });
                }
            }
            {
                auto extra = posaTeclat ({ x0, yTec, ampleTot, H - yTec - 26 }, 440);
                zonaArp = extra;
                ledArp = juce::Rectangle<int> (extra.getRight() - 12, extra.getY() + 1, 10, 10);
                filaP (extra.withTrimmedTop (14), filaArp);
            }
        }

    private:
        Neo& neo;
        GraficEnv grafic;
        VisorOsc visor[3];
        EditorPassos passos;
        juce::OwnedArray<Boto> pestanyes;
        int pagina = 0;
        std::vector<ControlSB*> pag[3];
        SelectorSB *oMotor[3] {}, *oTaula[3] {}, *oWarp[3] {}, *fTipus = nullptr, *fPend = nullptr, *lOna[4] {}, *mFont[numSlots] {}, *mVia[numSlots] {}, *mDesti[numSlots] {}, *vMode = nullptr;
        std::vector<PomSB*> macros, oFila1[3], oFila2[3], filaVeu, filaArp;
        std::vector<ControlSB*> mix1, mix2, mix3, filt1, filt2, lfo[4], filaPassos, fx[7], rutes[numMacros];
        std::vector<FaderSB*> faders;
        PomSB *fTall = nullptr, *fRes = nullptr, *mQuant[numSlots] {};
        juce::Rectangle<int> insignies[3], zonaRess, zonaLfo[4], titolsEnv, celes[numSlots], barres[numSlots], celesMac[numMacros], zonaVu, zonaArp, ledArp;
        std::vector<juce::Rectangle<int>> zonesFx;

        juce::String etiquetaDe (const std::string& e) override
        {
            Neo s;
            s.posaEstat (e);
            static const char* motors[] = { "Wavetable", "Analògic", "Harmònic", "FM", "Granular", "Soroll" };
            static const char* filtres[] = { "LP", "HP", "BP", "Notch", "Ladder", "Formant", "Comb", "Comb" };
            juce::StringArray t;
            for (int o = 0; o < 3; ++o)
            {
                if (s.valor (O1Niv + o * 11) <= 0.0f && ! (o == 0 && s.valor (M1Font) > 0.5f)) continue;
                const juce::String n = juce::String::fromUTF8 (motors[juce::jlimit (0, 5, (int) s.valor (O1Motor + o * 11))]);
                if (! t.contains (n)) t.add (n);
            }
            auto r = t.joinIntoString (" + ") + " · " + filtres[juce::jlimit (0, 7, (int) s.valor (FTipus))];
            if (s.valor (RsMix) > 0.05f) r += " · Ress."_ca;
            if (s.valor (ArpMode) > 0.5f) r += " · Arp"_ca;
            return r;
        }
        void canviat() override { grafic.repaint(); passos.repaint(); for (auto& v : visor) v.repaint(); }
        void tic() override
        {
            repaint (zonaVu);
            if (pagina == 0) for (auto& v : visor) v.repaint();
            if (pagina == 1)
            {
                passos.repaint();
                for (auto& l : zonaLfo) repaint (l.withWidth (48));
                for (auto& b : barres) repaint (b.expanded (2));
            }
            repaint (ledArp.expanded (6));
        }
        void paint (juce::Graphics& g) override
        {
            PanellSinte::paint (g);
            g.setFont (dibuix::lletra (10.0f, true));
            g.setColour (colorNeo (SArp));
            textEspaiat (g, "ARPEGIADOR / SEQÜÈNCIES"_ca, zonaArp.toFloat().removeFromTop (12.0f), juce::Justification::centredLeft, 1.6f);
            const int pasA = sinte().pasArp();
            dibuixaLed (g, ledArp, colorNeo (SArp), pasA >= 0 && pasA % 2 == 0 ? 1.0f : 0.35f, pasA >= 0);
        }
        void detallsSeccio (juce::Graphics& g, const sinteui::Seccio& s) override
        {
            const auto col = colorNeo (s.id);
            auto petitTitol = [&] (juce::Rectangle<int> r, const juce::String& t, juce::Colour c)
            {
                g.setFont (dibuix::lletra (9.5f, true));
                g.setColour (c);
                textEspaiat (g, t, r.toFloat().removeFromTop (12.0f), juce::Justification::centredLeft, 1.4f);
            };
            if (s.id == SOsc && pagina == 0)
            {
                // a la dreta de l'afinació: el nivell de l'oscil·lador com una barra de neó
                for (int o = 0; o < 3; ++o)
                {
                    if (! s.r.contains (insignies[o])) continue;
                    auto z = insignies[o].reduced (8, 10).toFloat();
                    const float niv = neo.valor (O1Niv + o * 11);
                    g.setFont (dibuix::lletra (9.0f, true));
                    g.setColour (sb::serigrafiaSuau);
                    g.drawText ("NIVELL", z.removeFromTop (12.0f), juce::Justification::centredLeft);
                    auto barra = z.withSizeKeepingCentre (z.getWidth(), 8.0f);
                    g.setColour (juce::Colours::white.withAlpha (0.08f));
                    g.fillRoundedRectangle (barra, 4.0f);
                    g.setColour (col.withAlpha (niv > 0.0f ? 0.9f : 0.2f));
                    g.fillRoundedRectangle (barra.withWidth (std::max (8.0f, barra.getWidth() * niv)), 4.0f);
                }
            }
            else if (s.id == SFiltre && pagina == 0) petitTitol (zonaRess, "RESSONADORS"_ca, col);
            else if (s.id == SEnv && pagina == 1)
            {
                auto tt = titolsEnv.toFloat();
                const float w = tt.getWidth() / 3.0f;
                g.setFont (dibuix::lletra (9.5f, true));
                const char* t[] = { "VOLUM", "FILTRE", "MODULACIÓ" };
                const int c[] = { SEnv, SFiltre, SMod };
                for (int k = 0; k < 3; ++k)
                {
                    g.setColour (colorNeo (c[k]));
                    textEspaiat (g, juce::String::fromUTF8 (t[k]), tt.removeFromLeft (w).reduced (4.0f, 0.0f), juce::Justification::centred, 1.5f);
                }
            }
            else if (s.id == SLfo && pagina == 1)
            {
                for (int k = 0; k < 4; ++k)
                {
                    auto badge = zonaLfo[k].withSize (44, 18).withY (zonaLfo[k].getCentreY() - 16).toFloat();
                    g.setColour (col.withAlpha (0.18f));
                    g.fillRoundedRectangle (badge, 4.0f);
                    g.setColour (col);
                    g.drawRoundedRectangle (badge, 4.0f, 1.0f);
                    g.setFont (dibuix::lletra (9.5f, true));
                    g.drawText ("LFO " + juce::String (k + 1), badge, juce::Justification::centred);
                    dibuixaLed (g, juce::Rectangle<int> (zonaLfo[k].getX() + 17, zonaLfo[k].getCentreY() + 6, 10, 10), col, 0.5f + 0.5f * neo.lfoAra (k), true);
                }
            }
            else if (s.id == SMod && pagina == 1 && s.r.getHeight() > 300)
            {
                for (int k = 0; k < numSlots; ++k)
                {
                    auto c = celes[k].toFloat();
                    const bool on = neo.valor (M1Font + k * 4) > 0.5f && neo.valor (M1Desti + k * 4) > 0.5f && std::abs (neo.valor (M1Quant + k * 4)) > 0.001f;
                    g.setColour (juce::Colours::black.withAlpha (on ? 0.28f : 0.14f));
                    g.fillRoundedRectangle (c, 5.0f);
                    g.setFont (dibuix::lletra (11.0f, true));
                    g.setColour (on ? col : sb::serigrafiaSuau);
                    g.drawText (juce::String (k + 1), c.withWidth (24.0f), juce::Justification::centred);
                    // les fletxes entre font, via i destí
                    if (mFont[k] != nullptr)
                    {
                        g.setColour (sb::serigrafiaSuau);
                        g.setFont (dibuix::lletra (12.0f, true));
                        g.drawText (juce::String::fromUTF8 ("×"), juce::Rectangle<int> (mFont[k]->getRight(), mFont[k]->getY(), 12, 26), juce::Justification::centred);
                        g.drawText (juce::String::fromUTF8 ("→"), juce::Rectangle<int> (mVia[k]->getRight(), mVia[k]->getY(), 12, 26), juce::Justification::centred);
                    }
                    // l'activitat (quant mou ara mateix)
                    auto b = barres[k].toFloat();
                    g.setColour (juce::Colours::white.withAlpha (0.07f));
                    g.fillRoundedRectangle (b, 2.0f);
                    if (on)
                    {
                        const float a = juce::jlimit (-1.0f, 1.0f, neo.activitat (k));
                        const float cx = b.getCentreX();
                        g.setColour (col);
                        g.fillRoundedRectangle (a >= 0.0f ? b.withX (cx).withWidth (b.getWidth() * 0.5f * a) : b.withX (cx + b.getWidth() * 0.5f * a).withWidth (-b.getWidth() * 0.5f * a), 2.0f);
                    }
                }
            }
            else if (s.id == SMacro && pagina == 2 && s.r.getY() > cfg.altCap + 200)
            {
                for (int k = 0; k < numMacros; ++k)
                {
                    auto c = celesMac[k].toFloat();
                    g.setColour (juce::Colours::black.withAlpha (0.22f));
                    g.fillRoundedRectangle (c, 5.0f);
                    auto nom = c.withWidth (100.0f).reduced (8.0f, 0.0f);
                    g.setFont (dibuix::lletra (10.0f, true));
                    g.setColour (col);
                    g.drawText (juce::String (k + 1) + "  " + juce::String::fromUTF8 (nomMacro (k)), nom.removeFromTop (c.getHeight() * 0.6f), juce::Justification::bottomLeft);
                    auto barra = nom.withHeight (5.0f).translated (0.0f, 3.0f);
                    g.setColour (juce::Colours::white.withAlpha (0.08f));
                    g.fillRoundedRectangle (barra, 2.5f);
                    g.setColour (col);
                    g.fillRoundedRectangle (barra.withWidth (barra.getWidth() * neo.valor (Mac1 + k)), 2.5f);
                }
                g.setFont (dibuix::lletra (9.0f, false));
                g.setColour (sb::serigrafiaSuau);
                g.drawText ("Moviment: al mig, el so tal com és; escala els LFO i els passos. Evolució: escala les vies de la matriu que la fan servir."_ca,
                            s.r.toFloat().removeFromBottom (14.0f).reduced (12.0f, 0.0f), juce::Justification::centredLeft);
            }
            else if (s.id == SFx && pagina == 2)
            {
                static const char* t[] = { "CRUSH", "SHIFTER", "CHORUS", "DELAY", "REVERB", "ESPECTRE", "SORTIDA" };
                static const int p[] = { CrMix, ShMix, ChMix, DlMix, RvMix, -1, -1 };
                for (size_t i = 0; i < zonesFx.size(); ++i)
                {
                    auto b = zonesFx[i].toFloat();
                    g.setColour (juce::Colours::black.withAlpha (0.24f));
                    g.fillRoundedRectangle (b, 6.0f);
                    const bool on = p[i] < 0 ? (i != 5 || std::abs (neo.valor (EsTilt)) > 0.01f || neo.valor (EsExcita) > 0.001f) : neo.valor (p[i]) > 0.001f;
                    g.setColour (on ? col.withAlpha (0.55f) : juce::Colours::white.withAlpha (0.06f));
                    g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
                    petitTitol (zonesFx[i].translated (8, 3), juce::String::fromUTF8 (t[i]), on ? col : sb::serigrafiaSuau);
                    dibuixaLed (g, juce::Rectangle<int> (zonesFx[i].getRight() - 14, zonesFx[i].getY() + 4, 8, 8), col, 1.0f, on);
                }
            }
            else if (s.id == SVeu) dibuixaVu (g, zonaVu);
        }
    };
}

std::unique_ptr<juce::DocumentWindow> creaFinestraNeo (std::shared_ptr<Neo> sinte, EstatEstudi& estat, GestorPlugins* plugins,
                                                       const std::string& idPista, const juce::String& titol,
                                                       std::function<void (std::uint8_t, std::uint8_t, std::uint8_t)> directe,
                                                       std::function<void()> enTancar)
{
    auto* p = new PanellNeo (std::move (sinte), estat, plugins, idPista, std::move (directe), enTancar);
    return std::make_unique<FinestraSinte> (titol, p, std::move (enTancar));
}
