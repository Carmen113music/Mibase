#include "FinestraAtmos.h"
#include "SinteUI.h"
#include <set>

using namespace mibase::atmos;
using namespace sinteui;

/* 0.90: Mibase ATMOS. Personalitat pròpia: panell de nit amb seccions verd fosc d'aurora, galtes de fusta, llums
   aigua-marina i lavanda. Pantalles grans (forma d'ona, evolució del timbre i moviment estèreo) i macros de crom. */
namespace
{
    constexpr int SVisual = SArp + 1;

    juce::Colour colorAtmos (int s)
    {
        //                                 osc         textura     filtre      env         lfo         matriu      fx          macros      veu         arp         visual
        static const juce::uint32 c[] = { 0xff6fe3d0, 0xffa0e7a0, 0xff7fb0ff, 0xfff9c97f, 0xffc4b5fd, 0xfff0a6ca, 0xff5cc8ff, 0xff6fe3d0, 0xffd6e4f0, 0xffc4b5fd, 0xff6fe3d0 };
        return juce::Colour (c[juce::jlimit (0, 10, s)]);
    }

    ConfigSinte configAtmos()
    {
        ConfigSinte c;
        c.marca = "ATMOS";
        c.subtitol = "PADS · TEXTURES · ATMOSFERES"_ca;
        c.carpeta = "Atmos"; c.extensio = "mbatmos"; c.tipusFitxer = "mibase-atmos";
        c.presets = &presetsFabrica();
        c.categories = &categories();
        c.inicial = [] { return estatInicial(); };
        c.aleatori = [] (unsigned l) { return aleatori (l); };
        c.tipusAtzar = tipusPad();
        c.aleatoriTipus = [] (unsigned l, int t) { return aleatori (l, t); };
        c.varia = [] (const std::string& e, unsigned l, float q) { return varia (e, l, q); };
        c.previsualitza = [] (const std::string& e, float* o, int n) { return previsualitza (e, o, n, 60); };
        c.textPrevi = "FORMA D'ONA · C4"_ca;
        c.akai = { MAtac, MAlliberament, MFiltre, MMoviment, MAmplada, MBrillantor, MCalidesa, MProfunditat };
        c.textAkai = "AKAI: els 8 macros (K1 atac, K2 alliberament, K3 filtre, K4 moviment, K5 amplada, K6 brillantor, K7 calidesa, K8 profunditat)"_ca;
        c.colorSeccio = colorAtmos;
        c.accent = juce::Colour (0xff6fe3d0);
        c.colorMarca = juce::Colour (0xffc4b5fd);
        c.colorFranja = juce::Colour (0xff6fe3d0);
        c.fons = Textura::Nit; c.galtes = Textura::Fusta; c.capcalera = Textura::Carboni;
        c.seccioDalt = juce::Colour (0xff17292c);
        c.seccioBaix = juce::Colour (0xff0d181b);
        c.ample = 1440; c.alt = 980;
        return c;
    }

    void fonsPantalla (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (juce::Colour (0xff081012));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (juce::Colour (0xff6fe3d0).withAlpha (0.06f));
        for (int k = 1; k < 4; ++k) g.fillRect (r.getX() + 4.0f, r.getY() + r.getHeight() * (float) k / 4.0f, r.getWidth() - 8.0f, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (r, 6.0f, 1.2f);
    }

    /** L'evolució del timbre: l'evolució, la mescla A/B, la forma i els tres LFO al llarg dels darrers segons. */
    class VisorEvolucio : public juce::Component
    {
    public:
        explicit VisorEvolucio (Atmos& s) : sinte (s) { setInterceptsMouseClicks (false, false); }
        void mostra()
        {
            hist[(size_t) cap] = { sinte.evolucio(), sinte.mesclaAra(), sinte.formaAra(), 0.5f + 0.5f * sinte.lfoN (0), 0.5f + 0.5f * sinte.lfoN (1), 0.5f + 0.5f * sinte.lfoN (2) };
            cap = (cap + 1) % n;
            repaint();
        }
        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            fonsPantalla (g, r);
            auto zona = r.reduced (8.0f, 18.0f);
            static const juce::uint32 cols[] = { 0xff6fe3d0, 0xffc4b5fd, 0xfff0a6ca, 0x80f9c97f, 0x80a0e7a0, 0x807fb0ff };
            static const char* noms[] = { "EVOLUCIÓ", "MESCLA A/B", "FORMA" };
            for (int k = 5; k >= 0; --k)
            {
                juce::Path p;
                for (int i = 0; i < n; ++i)
                {
                    const float v = hist[(size_t) ((cap + i) % n)][(size_t) k];
                    const float x = zona.getX() + zona.getWidth() * (float) i / (float) (n - 1);
                    const float y = zona.getBottom() - zona.getHeight() * juce::jlimit (0.0f, 1.0f, v);
                    if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                }
                g.setColour (juce::Colour (cols[k]));
                g.strokePath (p, juce::PathStrokeType (k < 3 ? 1.8f : 1.0f));
            }
            g.setFont (dibuix::lletra (9.0f, true));
            float x = r.getX() + 8.0f;
            for (int k = 0; k < 3; ++k)
            {
                g.setColour (juce::Colour (cols[k]));
                g.drawText (juce::String::fromUTF8 (noms[k]), juce::Rectangle<float> (x, r.getY() + 3.0f, 90.0f, 12.0f), juce::Justification::centredLeft);
                x += 90.0f;
            }
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.drawText ("LFO 1-3", juce::Rectangle<float> (r.getRight() - 64.0f, r.getY() + 3.0f, 56.0f, 12.0f), juce::Justification::centredRight);
            g.drawText ("EVOLUCIÓ DEL TIMBRE"_ca, r.removeFromBottom (15.0f).reduced (8.0f, 0.0f), juce::Justification::centredLeft);
        }
    private:
        Atmos& sinte;
        static constexpr int n = 150;
        std::array<std::array<float, 6>, n> hist {};
        int cap = 0;
    };

    /** El moviment estèreo: vectorscopi (mig amunt, costats als laterals), l'auto-pan i l'amplada. */
    class VisorEstereo : public juce::Component
    {
    public:
        explicit VisorEstereo (Atmos& s) : sinte (s) { setInterceptsMouseClicks (false, false); }
        void mostra() { sinte.copiaVector (l.data(), r.data(), Atmos::midaVector); repaint(); }
        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            fonsPantalla (g, b);
            auto zona = b.reduced (10.0f, 18.0f);
            const float cx = zona.getCentreX(), cy = zona.getCentreY(), rad = std::min (zona.getWidth(), zona.getHeight()) * 0.5f;
            g.setColour (juce::Colour (0xff6fe3d0).withAlpha (0.12f));
            g.drawEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f, 1.0f);
            g.drawLine (cx - rad, cy, cx + rad, cy, 1.0f);
            g.drawLine (cx, cy - rad, cx, cy + rad, 1.0f);
            float pic = 0.05f;
            for (int i = 0; i < Atmos::midaVector; ++i) pic = std::max (pic, std::max (std::abs (l[(size_t) i]), std::abs (r[(size_t) i])));
            const float k = rad * 0.7f / pic;
            g.setColour (juce::Colour (0xffc4b5fd).withAlpha (0.55f));
            for (int i = 0; i < Atmos::midaVector; ++i)
            {
                const float x = cx + (r[(size_t) i] - l[(size_t) i]) * k, y = cy - (l[(size_t) i] + r[(size_t) i]) * k;
                g.fillRect (x, y, 1.6f, 1.6f);
            }
            // auto-pan i amplada
            const float pan = sinte.autoPan();
            g.setColour (juce::Colour (0xff6fe3d0));
            g.fillEllipse (cx + pan * rad - 4.0f, zona.getBottom() + 2.0f, 8.0f, 8.0f);
            const float a = juce::jlimit (0.0f, 1.0f, sinte.amplada());
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.fillRoundedRectangle (b.getRight() - 64.0f, b.getY() + 5.0f, 54.0f, 6.0f, 3.0f);
            g.setColour (juce::Colour (0xff6fe3d0));
            g.fillRoundedRectangle (b.getRight() - 64.0f, b.getY() + 5.0f, 54.0f * a, 6.0f, 3.0f);
            g.setFont (dibuix::lletra (9.0f, true));
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.drawText ("AMPLADA", juce::Rectangle<float> (b.getRight() - 130.0f, b.getY() + 2.0f, 62.0f, 12.0f), juce::Justification::centredRight);
            g.drawText ("MOVIMENT ESTÈREO"_ca, b.removeFromBottom (15.0f).reduced (8.0f, 0.0f), juce::Justification::centredLeft);
        }
    private:
        Atmos& sinte;
        std::array<float, Atmos::midaVector> l {}, r {};
    };

    class PanellAtmos : public PanellSinte
    {
    public:
        PanellAtmos (std::shared_ptr<Atmos> s, EstatEstudi& e, GestorPlugins* gp, std::string idPista,
                     std::function<void (std::uint8_t, std::uint8_t, std::uint8_t)> dir, std::function<void()> tanca)
            : PanellSinte (s, e, gp, std::move (idPista), configAtmos(), std::move (dir), std::move (tanca)),
              atmos (*s), grafic (*this, { AA, AD, AS, AR, FA, FD, FS, FR }, colorAtmos (SEnv), colorAtmos (SFiltre)),
              evol (*s), estereo (*s)
        {
            using E = PomSB::Estil;
            for (int o = 0; o < 4; ++o)
            {
                const int b = o * 8;
                ones[o] = sel (O1Ona + b, false);
                for (int p : { O1Oct + b, O1Semi + b, O1Fi + b, O1Forma + b, O1Taula + b, O1Niv + b, O1Pan + b }) osc[o].push_back (pom (p, E::Petit));
            }
            for (int p : { (int) Unison, (int) UniDetune, (int) UniAmple, (int) Fm21, (int) Fm43, (int) Mescla }) text1.push_back (pom (p, E::Petit));
            text1.push_back (intr (FaseLliure, "Lliure"_ca));
            for (int p : { (int) GrDens, (int) GrMida, (int) GrSpray, (int) GrTo, (int) GrOct, (int) Deriva, (int) DerivaVel }) text2.push_back (pom (p, E::Petit));
            nzTipus = sel (NzTipus, false);
            for (int p : { (int) NzNiv, (int) NzColor, (int) NzMov }) text3.push_back (pom (p, E::Petit));
            fTipus = sel (FTipus, false); fPend = sel (FPendent, false);
            fTall = pom (FTall, E::Crom); fRes = pom (FRes, E::Bakelita);
            for (int p : { (int) FEnv, (int) FKey, (int) FDrive, (int) FVel }) filaFiltre.push_back (pom (p, E::Petit));
            for (int p : { (int) AA, (int) AD, (int) AS, (int) AR }) faders.push_back (fad (p, colorAtmos (SEnv)));
            for (int p : { (int) FA, (int) FD, (int) FS, (int) FR }) faders.push_back (fad (p, colorAtmos (SFiltre)));
            addAndMakeVisible (grafic);
            evTemps = pom (EvTemps, E::Petit); evForma = sel (EvForma, false);
            for (int l = 0; l < 3; ++l)
            {
                const int b = l * 5;
                lOna[l] = sel (L1Ona + b, false);
                for (int p : { L1Vel + b, L1Ritme + b, L1Fase + b, L1Entrada + b }) lfo[l].push_back (pom (p, E::Petit));
            }
            for (int k = 0; k < 8; ++k)
            {
                mFont[k] = sel (M1Font + k * 3, false);
                mDesti[k] = sel (M1Desti + k * 3, false);
                mQuant[k] = pom (M1Quant + k * 3, E::Petit, "Quant."_ca);
            }
            // FX: (bloc, controls)
            fx[0] = { pom (ChMix, E::Bakelita, "Mix"_ca), pom (ChVel, E::Petit, "Vel."_ca), pom (ChProf, E::Petit, "Prof."_ca) };
            fx[1] = { pom (PhMix, E::Bakelita, "Mix"_ca), pom (PhVel, E::Petit, "Vel."_ca), pom (PhProf, E::Petit, "Prof."_ca), pom (PhFeedback, E::Petit, "Feedb."_ca) };
            fx[2] = { pom (DlMix, E::Bakelita, "Mix"_ca), pom (DlNota, E::Petit), pom (DlFeedback, E::Petit), intr (DlPingPong) };
            rvTipus = sel (RvTipus, false);
            fx[3] = { pom (RvMix, E::Bakelita, "Mix"_ca), pom (RvMida, E::Petit), pom (RvShimmer, E::Petit), pom (RvTo, E::Petit), intr (RvFreeze, "Freeze"_ca) };
            fx[4] = { pom (ApQuant, E::Bakelita, "Auto-pan"_ca), pom (ApVel, E::Petit, "Vel."_ca), pom (ApRitme, E::Petit) };
            fx[5] = { pom (Ample, E::Bakelita) };
            for (int p = MAtac; p <= MProfunditat; ++p) macros.push_back (pom (p, E::Crom));
            for (int p : { (int) VelVol, (int) BendRang, (int) Glide }) filaVeu.push_back (pom (p, E::Petit));
            vVeus = sel (Veus, false);
            vVol = pom (Volum, E::Bakelita);
            for (int p : { (int) ArpMode, (int) ArpRitme, (int) ArpOct, (int) ArpGate, (int) ArpSwing, (int) ArpPatro }) filaArp.push_back (pom (p, E::Petit));
            addAndMakeVisible (evol);
            addAndMakeVisible (estereo);
            addAndMakeVisible (btVista);
            btVista.setTooltip ("Simple: els controls essencials. Avançat: tots els paràmetres."_ca);
            btVista.onClick = [this] { avancat = ! avancat; sint->posa (Vista, avancat ? 1.0f : 0.0f); desaCanvis(); resized(); repaint(); };
            avancat = sint->valor (Vista) > 0.5f;
            // els controls del mode SIMPLE
            for (auto* c : macros) simples.insert (c);
            for (int b : { 0, 1, 2, 3, 4 }) simples.insert (fx[b][0]);
            simples.insert (fx[3][2]); simples.insert (fx[3][4]); simples.insert (vVol); simples.insert (filaVeu[2]); simples.insert (vVeus);
            for (auto* c : filaArp) simples.insert (c);
            teclat.base = 48;
            setSize (cfg.ample, cfg.alt);
            pantalla.actualitza();
        }

        void resized() override
        {
            posaCapcalera();
            seccions.clear();
            btVista.setButtonText (avancat ? "VISTA: AVANÇAT"_ca : "VISTA: SIMPLE");
            for (auto* c : controls) c->setVisible (avancat || simples.count (c) > 0);
            for (auto* c : { (juce::Component*) &grafic, (juce::Component*) rvTipus }) c->setVisible (avancat);
            const int W = getWidth(), H = getHeight();
            const int x0 = cfg.ampleGaltes + 8, x1 = W - cfg.ampleGaltes - 8;
            const int g = 8, ampleTot = x1 - x0;
            const int yTec = 866;
            int x = x0;
            auto posa = [&] (int idS, const juce::String& t, int w, int y, int h) { seccions.push_back ({ idS, t, { x, y, w, h } }); x += w + g; return seccions.back().r; };
            auto fila = [] (juce::Rectangle<int> r, const std::vector<ControlSB*>& cs)
            {
                if (cs.empty()) return;
                const int w = r.getWidth() / (int) cs.size();
                for (auto* c : cs) c->setBounds (r.removeFromLeft (w));
            };
            const int y1 = cfg.altCap + 8;
            if (! avancat)
            {
                const auto rVis = posa (SVisual, "SO I MOVIMENT"_ca, ampleTot, y1, 340);
                {
                    auto r = dins (rVis);
                    const int w = (r.getWidth() - 16) / 3;
                    pantalla.setBounds (r.removeFromLeft (w)); r.removeFromLeft (8);
                    evol.setBounds (r.removeFromLeft (w)); r.removeFromLeft (8);
                    estereo.setBounds (r);
                }
                x = x0;
                fila (dins (posa (SMacro, "MACROS", ampleTot, y1 + 348, 260)).reduced (0, 6), { macros.begin(), macros.end() });
                x = x0;
                const int y3 = y1 + 348 + 268, h3 = yTec - y3 - g;
                {
                    auto r = dins (posa (SFx, "EFECTES"_ca, 900, y3, h3));
                    zonesFx.clear();
                    std::vector<ControlSB*> cs { fx[0][0], fx[1][0], fx[2][0], fx[3][0], fx[3][2], fx[3][4], fx[4][0] };
                    fila (r, cs);
                }
                {
                    auto r = dins (posa (SVeu, "SORTIDA"_ca, x0 + ampleTot - x, y3, h3));
                    zonaVu = r.removeFromRight (16).reduced (2, 2);
                    r.removeFromRight (6);
                    vVeus->setBounds (r.removeFromLeft (110).withSizeKeepingCentre (110, 28));
                    r.removeFromLeft (8);
                    fila (r, { filaVeu[2], vVol });
                }
            }
            else
            {
                const int h1 = 262, y2 = y1 + h1 + g, h2 = 230, y3 = y2 + h2 + g, h3 = 140, y4 = y3 + h3 + g, h4 = yTec - y4 - g;
                const auto rOsc = posa (SOsc, "OSCIL·LADORS"_ca, 610, y1, h1);
                const auto rTex = posa (SText, "TEXTURA · GRANULAR · DERIVA"_ca, 380, y1, h1);
                const auto rFil = posa (SFiltre, "FILTRE"_ca, x0 + ampleTot - x, y1, h1);
                x = x0;
                const auto rEnv = posa (SEnv, "ENVOLUPANTS I EVOLUCIÓ"_ca, 440, y2, h2);
                const auto rLfo = posa (SLfo, "LFO"_ca, 400, y2, h2);
                const auto rMat = posa (SMod, "MATRIU DE MODULACIÓ"_ca, x0 + ampleTot - x, y2, h2);
                x = x0;
                const auto rFx = posa (SFx, "RACK D'EFECTES"_ca, ampleTot, y3, h3);
                x = x0;
                const auto rMac = posa (SMacro, "MACROS", 640, y4, h4);
                const auto rVis = posa (SVisual, "SO I MOVIMENT"_ca, 420, y4, h4);
                const auto rVeu = posa (SVeu, "VEU I SORTIDA"_ca, x0 + ampleTot - x, y4, h4);
                {
                    auto r = dins (rOsc);
                    const int hf = r.getHeight() / 4;
                    for (int o = 0; o < 4; ++o)
                    {
                        auto f = r.removeFromTop (hf);
                        insignies[o] = f;
                        f.removeFromLeft (22);
                        ones[o]->setBounds (f.removeFromLeft (118).withSizeKeepingCentre (118, 26));
                        f.removeFromLeft (6);
                        fila (f, { osc[o].begin(), osc[o].end() });
                    }
                }
                {
                    auto r = dins (rTex);
                    const int hf = r.getHeight() / 3;
                    fila (r.removeFromTop (hf), text1);
                    fila (r.removeFromTop (hf), { text2.begin(), text2.end() });
                    nzTipus->setBounds (r.removeFromLeft (120).withSizeKeepingCentre (120, 26));
                    r.removeFromLeft (8);
                    fila (r, { text3.begin(), text3.end() });
                }
                {
                    auto r = dins (rFil);
                    auto f = r.removeFromTop (26);
                    fTipus->setBounds (f.removeFromLeft (f.getWidth() / 2 - 4));
                    f.removeFromLeft (8);
                    fPend->setBounds (f);
                    r.removeFromTop (4);
                    auto h = r.removeFromTop (r.getHeight() - 62);
                    fRes->setBounds (h.removeFromRight (h.getWidth() / 3));
                    fTall->setBounds (h);
                    fila (r, { filaFiltre.begin(), filaFiltre.end() });
                }
                {
                    auto r = dins (rEnv);
                    auto ev = r.removeFromRight (84);
                    evForma->setBounds (ev.removeFromBottom (26));
                    evTemps->setBounds (ev.withTrimmedTop (40));
                    zonaEv = ev.withHeight (40);
                    r.removeFromRight (6);
                    grafic.setBounds (r.removeFromTop (44));
                    r.removeFromTop (4);
                    titolsEnv = r.removeFromTop (14);
                    const int w = r.getWidth() / 8;
                    for (auto* f : faders) f->setBounds (r.removeFromLeft (w));
                }
                {
                    auto r = dins (rLfo);
                    const int hf = r.getHeight() / 3;
                    for (int l = 0; l < 3; ++l)
                    {
                        auto f = r.removeFromTop (hf);
                        zonaLfo[l] = f;
                        f.removeFromLeft (44);
                        lOna[l]->setBounds (f.removeFromLeft (104).withSizeKeepingCentre (104, 24));
                        f.removeFromLeft (4);
                        fila (f, { lfo[l].begin(), lfo[l].end() });
                    }
                }
                {
                    auto r = dins (rMat);
                    const int wc = r.getWidth() / 4, hc = r.getHeight() / 2;
                    for (int k = 0; k < 8; ++k)
                    {
                        auto c = juce::Rectangle<int> (r.getX() + (k % 4) * wc, r.getY() + (k / 4) * hc, wc, hc).reduced (3, 2);
                        celes[k] = c;
                        c.removeFromTop (12);
                        auto q = c.removeFromRight (46);
                        mQuant[k]->setBounds (q);
                        auto s = c.withSizeKeepingCentre (c.getWidth(), 52);
                        mFont[k]->setBounds (s.removeFromTop (25));
                        s.removeFromTop (2);
                        mDesti[k]->setBounds (s);
                    }
                }
                {
                    auto r = dins (rFx);
                    const int pesos[] = { 3, 4, 4, 6, 3, 1 };
                    const int total = 21;
                    const int amp = r.getWidth();
                    zonesFx.clear();
                    for (int b = 0; b < 6; ++b)
                    {
                        auto z = r.removeFromLeft (amp * pesos[b] / total).reduced (3, 0);
                        zonesFx.push_back (z);
                        z.removeFromTop (14);
                        if (b == 3) rvTipus->setBounds (z.removeFromLeft (z.getWidth() / 6).withSizeKeepingCentre (z.getWidth() / 6, 26));
                        fila (z, fx[b]);
                    }
                }
                fila (dins (rMac), { macros.begin(), macros.end() });
                {
                    auto r = dins (rVis);
                    const int w = (r.getWidth() - 12) / 3;
                    pantalla.setBounds (r.removeFromLeft (w)); r.removeFromLeft (6);
                    evol.setBounds (r.removeFromLeft (w)); r.removeFromLeft (6);
                    estereo.setBounds (r);
                }
                {
                    auto r = dins (rVeu);
                    zonaVu = r.removeFromRight (16).reduced (2, 2);
                    r.removeFromRight (6);
                    vVeus->setBounds (r.removeFromTop (24).withSizeKeepingCentre (110, 24));
                    std::vector<ControlSB*> cs { filaVeu.begin(), filaVeu.end() };
                    cs.push_back (vVol);
                    fila (r, cs);
                }
            }
            {
                auto extra = posaTeclat ({ x0, yTec, ampleTot, H - yTec - 26 }, 420);
                btVista.setBounds (extra.removeFromLeft (96).withSizeKeepingCentre (96, 40));
                extra.removeFromLeft (8);
                zonaArp = extra;
                ledArp = juce::Rectangle<int> (extra.getRight() - 12, extra.getY() + 1, 10, 10);
                fila (extra.withTrimmedTop (14), { filaArp.begin(), filaArp.end() });
            }
        }

    private:
        Atmos& atmos;
        GraficEnv grafic;
        VisorEvolucio evol;
        VisorEstereo estereo;
        Boto btVista { "VISTA", std::nullopt, Boto::Estil::Tecla };
        bool avancat = false;
        std::set<ControlSB*> simples;
        SelectorSB *ones[4] {}, *nzTipus = nullptr, *fTipus = nullptr, *fPend = nullptr, *evForma = nullptr, *lOna[3] {}, *mFont[8] {}, *mDesti[8] {}, *rvTipus = nullptr, *vVeus = nullptr;
        std::vector<PomSB*> osc[4], text2, text3, filaFiltre, lfo[3], macros, filaVeu, filaArp;
        std::vector<ControlSB*> text1, fx[6];
        std::vector<FaderSB*> faders;
        PomSB *fTall = nullptr, *fRes = nullptr, *evTemps = nullptr, *mQuant[8] {}, *vVol = nullptr;
        juce::Rectangle<int> insignies[4], zonaLfo[3], celes[8], titolsEnv, zonaEv, zonaVu, zonaArp, ledArp;
        std::vector<juce::Rectangle<int>> zonesFx;

        juce::String etiquetaDe (const std::string& e) override
        {
            Atmos s;
            s.posaEstat (e);
            juce::StringArray t;
            static const char* ones8[] = { "Sinus", "Triangle", "Serra", "Quadrada", "Pols", "Soroll", "Wavetable", "Granular" };
            for (int o = 0; o < 4; ++o)
            {
                if (s.valor (O1Niv + o * 8) <= 0.0f) continue;
                const int ona = (int) s.valor (O1Ona + o * 8);
                const juce::String n = ona >= Wavetable ? u8 (nomTaula ((int) s.valor (O1Taula + o * 8))) : juce::String (ones8[ona]);
                if (! t.contains (n)) t.add (n);
            }
            auto r = t.joinIntoString (" + ");
            if (s.valor (EvTemps) > 0.0f && (int) s.valor (M1Font) == FEvolucio) r += " · Evoluciona"_ca;
            if (s.valor (ArpMode) > 0.5f) r += " · Arp"_ca;
            return r;
        }
        void canviat() override { grafic.repaint(); }
        void tic() override
        {
            // un preset no canvia la vista que has triat
            if ((sint->valor (Vista) > 0.5f) != avancat) sint->posa (Vista, avancat ? 1.0f : 0.0f);
            evol.mostra();
            estereo.mostra();
            repaint (zonaVu);
            if (avancat) for (auto& l : zonaLfo) repaint (l.withWidth (44));
            repaint (ledArp.expanded (6));
        }
        void paint (juce::Graphics& g) override
        {
            PanellSinte::paint (g);
            g.setFont (dibuix::lletra (10.0f, true));
            g.setColour (colorAtmos (SArp));
            textEspaiat (g, "ARPEGIADOR / ACORDS RÍTMICS"_ca, zonaArp.toFloat().removeFromTop (12.0f), juce::Justification::centredLeft, 1.6f);
            const int pasA = sinte().pasArp();
            dibuixaLed (g, ledArp, colorAtmos (SArp), pasA >= 0 && pasA % 2 == 0 ? 1.0f : 0.35f, pasA >= 0);
        }
        void detallsSeccio (juce::Graphics& g, const sinteui::Seccio& s) override
        {
            const auto col = colorAtmos (s.id);
            auto petitTitol = [&] (juce::Rectangle<int> r, const juce::String& t, juce::Colour c)
            {
                g.setFont (dibuix::lletra (9.5f, true));
                g.setColour (c);
                textEspaiat (g, t, r.toFloat().removeFromTop (12.0f), juce::Justification::centredLeft, 1.4f);
            };
            auto insignia = [&] (juce::Rectangle<float> c, const juce::String& t)
            {
                g.setColour (col.withAlpha (0.2f));
                g.fillEllipse (c.expanded (2.0f));
                g.setColour (col);
                g.drawEllipse (c, 1.2f);
                g.setFont (dibuix::lletra (10.0f, true));
                g.drawText (t, c, juce::Justification::centred);
            };
            if (s.id == SOsc && avancat)
            {
                for (int o = 0; o < 4; ++o)
                    insignia (juce::Rectangle<float> (18.0f, 18.0f).withCentre ({ (float) insignies[o].getX() + 9.0f, (float) insignies[o].getCentreY() }), juce::String (o + 1));
                g.setFont (dibuix::lletra (9.0f, true));
                g.setColour (sb::serigrafiaSuau);
                g.drawText ("1+2 = A   ·   3+4 = B", s.r.toFloat().removeFromTop (28.0f).reduced (12.0f, 0.0f), juce::Justification::centredRight);
            }
            else if (s.id == SEnv && avancat)
            {
                auto tt = titolsEnv.toFloat();
                g.setFont (dibuix::lletra (9.5f, true));
                g.setColour (colorAtmos (SEnv));
                textEspaiat (g, "VOLUM", tt.removeFromLeft (tt.getWidth() / 2).reduced (8.0f, 0.0f), juce::Justification::centred, 1.5f);
                g.setColour (colorAtmos (SFiltre));
                textEspaiat (g, "FILTRE", tt.reduced (8.0f, 0.0f), juce::Justification::centred, 1.5f);
                petitTitol (zonaEv, "EVOLUCIÓ"_ca, col);
                auto barra = zonaEv.toFloat().withTrimmedTop (16.0f).withHeight (6.0f).reduced (2.0f, 0.0f);
                g.setColour (juce::Colours::white.withAlpha (0.1f));
                g.fillRoundedRectangle (barra, 3.0f);
                g.setColour (col);
                g.fillRoundedRectangle (barra.withWidth (barra.getWidth() * juce::jlimit (0.0f, 1.0f, atmos.evolucio())), 3.0f);
            }
            else if (s.id == SLfo && avancat)
            {
                for (int k = 0; k < 3; ++k)
                {
                    auto badge = zonaLfo[k].withSize (40, 18).withY (zonaLfo[k].getCentreY() - 16).toFloat();
                    g.setColour (col.withAlpha (0.18f));
                    g.fillRoundedRectangle (badge, 4.0f);
                    g.setColour (col);
                    g.drawRoundedRectangle (badge, 4.0f, 1.0f);
                    g.setFont (dibuix::lletra (9.5f, true));
                    g.drawText ("LFO " + juce::String (k + 1), badge, juce::Justification::centred);
                    dibuixaLed (g, juce::Rectangle<int> (zonaLfo[k].getX() + 15, zonaLfo[k].getCentreY() + 6, 10, 10), col, 0.5f + 0.5f * atmos.lfoN (k), true);
                }
            }
            else if (s.id == SMod && avancat)
            {
                for (int k = 0; k < 8; ++k)
                {
                    auto c = celes[k].toFloat();
                    const bool on = std::abs (sinte().valor (M1Quant + k * 3)) > 0.001f;
                    g.setColour (juce::Colours::black.withAlpha (on ? 0.25f : 0.14f));
                    g.fillRoundedRectangle (c, 5.0f);
                    petitTitol (celes[k].translated (6, 2), "VIA "_ca + juce::String (k + 1), on ? col : sb::serigrafiaSuau);
                }
            }
            else if (s.id == SFx)
            {
                static const char* t[] = { "CHORUS", "PHASER", "DELAY", "REVERB", "AUTO-PAN", "ESTÈREO" };
                static const int p[] = { ChMix, PhMix, DlMix, RvMix, ApQuant, Ample };
                if (avancat)
                    for (size_t i = 0; i < zonesFx.size(); ++i)
                    {
                        auto b = zonesFx[i].toFloat();
                        g.setColour (juce::Colours::black.withAlpha (0.22f));
                        g.fillRoundedRectangle (b, 5.0f);
                        const bool on = i == 5 || sinte().valor (p[i]) > 0.001f;
                        petitTitol (zonesFx[i].translated (6, 2), juce::String::fromUTF8 (t[i]), on ? col : sb::serigrafiaSuau);
                        dibuixaLed (g, juce::Rectangle<int> (zonesFx[i].getRight() - 12, zonesFx[i].getY() + 3, 8, 8), col, 1.0f, on);
                    }
            }
            else if (s.id == SVeu) dibuixaVu (g, zonaVu);
        }
    };
}

std::unique_ptr<juce::DocumentWindow> creaFinestraAtmos (std::shared_ptr<Atmos> sinte, EstatEstudi& estat, GestorPlugins* plugins,
                                                         const std::string& idPista, const juce::String& titol,
                                                         std::function<void (std::uint8_t, std::uint8_t, std::uint8_t)> directe,
                                                         std::function<void()> enTancar)
{
    auto* p = new PanellAtmos (std::move (sinte), estat, plugins, idPista, std::move (directe), enTancar);
    return std::make_unique<FinestraSinte> (titol, p, std::move (enTancar));
}
