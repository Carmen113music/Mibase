#include "FinestraVintage.h"
#include "SinteUI.h"
#include <cmath>
#include <set>

using namespace mibase::analogic;
using namespace sinteui;

/* 1.4: Mibase VINTAGE. Aparell dels 70: panell negre mat amb serigrafia crema, galtes de noguera, poms amb faldilla,
   faders per a les envolupants, etiqueta de paper i vúmetre d'agulla. Colors apagats, com els codis dels panells antics. */
namespace
{
    juce::Colour colorVintage (int s)
    {
        //                                 osc         mod         mix         filtre      env         lfo         anal        fx          veu         arp
        static const juce::uint32 c[] = { 0xffe8a33d, 0xffd9733b, 0xffd8cfb4, 0xffd9553a, 0xff6fa9cf, 0xff94c26a, 0xffd9b25c, 0xff9aa9d6, 0xffd8cfb4, 0xff94c26a };
        return juce::Colour (c[juce::jlimit (0, 9, s)]);
    }

    ConfigSinte configVintage()
    {
        ConfigSinte c;
        c.marca = "VINTAGE";
        c.subtitol = "SINTETITZADOR ANALÒGIC CLÀSSIC"_ca;
        c.carpeta = "Vintage"; c.extensio = "mbvin"; c.tipusFitxer = "mibase-vintage";
        c.presets = &presetsFabrica();
        c.categories = &categories();
        c.inicial = [] { return estatInicial(); };
        c.aleatori = [] (unsigned l) { return aleatori (l); };
        c.tipusAtzar = tipusAtzar();
        c.aleatoriTipus = [] (unsigned l, int t) { return aleatori (l, t); };
        c.varia = [] (const std::string& e, unsigned l, float q) { return varia (e, l, q); };
        c.previsualitza = [] (const std::string& e, float* o, int n) { return previsualitza (e, o, n, 48); };
        c.textPrevi = "FORMA D'ONA · C3"_ca;
        c.akai = { FTall, FRes, FEnv, FDrive, AA, AR, Deriva, Glide };
        c.textAkai = "AKAI: K1 tall, K2 ressonància, K3 envolupant del filtre, K4 drive, K5 atac, K6 alliberament, K7 deriva, K8 glide"_ca;
        c.colorSeccio = colorVintage;
        c.accent = juce::Colour (0xffe8a33d);
        c.colorMarca = juce::Colour (0xffe8dcc0);
        c.colorFranja = juce::Colour (0xffd9733b);
        c.fons = Textura::Panell; c.galtes = Textura::Fusta; c.capcalera = Textura::Metall;
        c.ampleGaltes = 34;
        c.ample = 1440; c.alt = 930;
        c.vintage = true;
        return c;
    }

    class PanellVintage : public PanellSinte
    {
    public:
        PanellVintage (std::shared_ptr<Vintage> s, EstatEstudi& e, GestorPlugins* gp, std::string idPista,
                       std::function<void (std::uint8_t, std::uint8_t, std::uint8_t)> dir, std::function<void()> tanca)
            : PanellSinte (s, e, gp, std::move (idPista), configVintage(), std::move (dir), std::move (tanca)), vin (*s)
        {
            using E = PomSB::Estil;
            pantalla.setVisible (false);   // res de pantalles: aquí hi ha un vúmetre d'agulla
            for (int o = 0; o < 3; ++o)
            {
                const int b = o * 6;
                oOna[o] = sel (O1Ona + b, true);
                oPeu[o] = pom (O1Peu + b, E::Petit);
                oSemi[o] = pom (O1Semi + b, E::Petit);
                oFi[o] = pom (O1Fi + b, E::Petit);
                oPw[o] = pom (O1Pw + b, E::Petit);
                oNiv[o] = pom (O1Niv + b, E::Faldilla, "Osc "_ca + juce::String (o + 1));
            }
            mSync = intr (Sync, "Sync 2 → 1"_ca);
            mLent = intr (O3Baixa, "Osc 3 lent"_ca);
            mEnv2 = pom (EnvA2, E::Faldilla);
            mFm = pom (Fm, E::Faldilla);
            xSub = pom (SubNiv, E::Faldilla); xNz = pom (NzNiv, E::Faldilla); xRing = pom (Ring, E::Faldilla);
            xSubOct = sel (SubOct, false); xSubOna = sel (SubOna, false); xColor = sel (NzColor, false);
            fTipus = sel (FTipus, false); fPend = sel (FPendent, false); fMode = sel (FMode, false);
            fTall = pom (FTall, E::Bakelita); fRes = pom (FRes, E::Faldilla);
            for (int p : { (int) FEnv, (int) FKey, (int) FVel, (int) FDrive, (int) FHpf }) fFila.push_back (pom (p, E::Petit));
            for (int p : { (int) AA, (int) AD, (int) AS, (int) AR }) envA.push_back (fad (p, colorVintage (SEnv)));
            for (int p : { (int) FA, (int) FD, (int) FS, (int) FR }) envF.push_back (fad (p, colorVintage (SFiltre)));
            eVel = pom (VelVol, E::Petit);
            lOna = sel (LOna, false); lRitme = sel (LRitme, false); lFont = sel (LFont, false);
            lPerVeu = intr (LPerVeu, "Per veu"_ca);
            for (int p : { (int) LVel, (int) LRetard }) lFila1.push_back (pom (p, E::Petit));
            for (int p : { (int) LTo, (int) LPw, (int) LTall, (int) LVol, (int) RodaTo, (int) RodaTall, (int) AtTall }) lFila2.push_back (pom (p, E::Petit));
            aOn = intr (Analogic, "Analògic"_ca);
            for (int p = Deriva; p <= VarEstereo; ++p) aKnobs.push_back (pom (p, E::Faldilla));
            xChorus = sel (Chorus, false); xDlNota = sel (DlNota, false); xDlTipus = sel (DlTipus, false); xRvTipus = sel (RvTipus, false);
            for (int p : { (int) PhMix, (int) PhVel, (int) DlMix, (int) DlFeedback, (int) RvMix, (int) RvMida }) fxKnobs.push_back (pom (p, E::Petit));
            vMode = sel (Mode, false); vFase = sel (FaseLliure, false);
            for (int p : { (int) Veus, (int) Unison, (int) UniDetune, (int) Glide, (int) BendRang }) vKnobs.push_back (pom (p, E::Petit));
            vVol = pom (Volum, E::Faldilla); vAmple = pom (Ample, E::Petit);
            for (int p : { (int) ArpMode, (int) ArpRitme, (int) ArpOct, (int) ArpGate, (int) ArpSwing, (int) ArpPatro }) filaArp.push_back (pom (p, E::Petit));
            addAndMakeVisible (btVista);
            btVista.setTooltip ("Simple: els controls essencials. Avançat: tots els paràmetres."_ca);
            btVista.onClick = [this] { avancat = ! avancat; sint->posa (Vista, avancat ? 1.0f : 0.0f); desaCanvis(); resized(); repaint(); };
            avancat = sint->valor (Vista) > 0.5f;
            teclat.base = 48;
            setSize (cfg.ample, cfg.alt);
        }

        void resized() override
        {
            posaCapcalera();
            seccions.clear();
            btVista.setButtonText (avancat ? "VISTA: AVANÇAT"_ca : "VISTA: SIMPLE");
            for (auto* c : controls) c->setVisible (false);
            const int W = getWidth(), H = getHeight();
            const int x0 = cfg.ampleGaltes + 8, x1 = W - cfg.ampleGaltes - 8;
            const int g = 8, ampleTot = x1 - x0;
            const int yTec = H - 114;
            int x = x0;
            auto posa = [&] (int idS, const juce::String& t, int w, int y, int h) { seccions.push_back ({ idS, t, { x, y, w, h } }); x += w + g; return seccions.back().r; };
            auto mostra = [] (juce::Component* c, juce::Rectangle<int> r)
            {
                if (c == nullptr) return;
                c->setVisible (true);
                c->setBounds (dynamic_cast<SelectorSB*> (c) != nullptr || dynamic_cast<InterruptorSB*> (c) != nullptr ? r.withSizeKeepingCentre (r.getWidth() - 6, std::min (r.getHeight(), 28)) : r);
            };
            auto fila = [&] (juce::Rectangle<int> r, std::vector<juce::Component*> cs)
            {
                if (cs.empty()) return;
                const int w = r.getWidth() / (int) cs.size();
                for (auto* c : cs) mostra (c, r.removeFromLeft (w));
            };
            const int y1 = cfg.altCap + 10;
            zonaVu = {};
            zonaLlum = {};
            if (avancat)
            {
                // ---- fila 1: oscil·ladors, modulació, mesclador ----
                const int h1 = 252;
                {
                    auto r = dins (posa (SOsc, "OSCIL·LADORS"_ca, 640, y1, h1));
                    const int hf = r.getHeight() / 3;
                    for (int o = 0; o < 3; ++o)
                    {
                        auto f = r.removeFromTop (hf);
                        numOsc[o] = f.removeFromLeft (22);
                        mostra (oOna[o], f.removeFromLeft (200).withSizeKeepingCentre (200, 28));
                        f.removeFromLeft (8);
                        fila (f, { oPeu[o], oSemi[o], oFi[o], oPw[o] });
                    }
                }
                {
                    auto r = dins (posa (SMod, "MODULACIÓ"_ca, 236, y1, h1));
                    auto dalt = r.removeFromTop (r.getHeight() * 3 / 5);
                    fila (dalt, { mEnv2, mFm });
                    auto b = r.reduced (0, 4);
                    mostra (mSync, b.removeFromTop (b.getHeight() / 2).reduced (6, 2));
                    mostra (mLent, b.reduced (6, 2));
                }
                {
                    auto r = dins (posa (SMix, "MESCLADOR"_ca, x0 + ampleTot - x, y1, h1));
                    auto dalt = r.removeFromTop (r.getHeight() * 3 / 5);
                    fila (dalt, { oNiv[0], oNiv[1], oNiv[2], xSub, xNz, xRing });
                    r.removeFromTop (8);
                    fila (r.withSizeKeepingCentre (r.getWidth(), 28), { xSubOct, xSubOna, xColor });
                }
                // ---- fila 2: filtre, envolupants, LFO ----
                x = x0;
                const int y2 = y1 + h1 + g, h2 = 238;
                {
                    auto r = dins (posa (SFiltre, "FILTRE"_ca, 520, y2, h2));
                    auto sels = r.removeFromTop (28);
                    mostra (fTipus, sels.removeFromLeft (250));
                    mostra (fPend, sels.removeFromLeft (110));
                    mostra (fMode, sels);
                    r.removeFromTop (6);
                    auto gran = r.removeFromLeft (130);
                    mostra (fTall, gran);
                    mostra (fRes, r.removeFromLeft (92).withTrimmedTop (10).withTrimmedBottom (40));
                    auto rest = r;
                    fila (rest.removeFromTop (rest.getHeight() / 2), { fFila[0], fFila[1], fFila[2] });
                    fila (rest, { fFila[3], fFila[4] });
                }
                {
                    auto r = dins (posa (SEnv, "ENVOLUPANTS"_ca, 440, y2, h2));
                    titolsEnv = r.removeFromTop (14);
                    auto vel = r.removeFromRight (64);
                    mostra (eVel, vel.withSizeKeepingCentre (64, 74));
                    const int w = r.getWidth() / 8;
                    for (auto* f : envA) mostra (f, r.removeFromLeft (w).reduced (3, 0));
                    for (auto* f : envF) mostra (f, r.removeFromLeft (w).reduced (3, 0));
                }
                {
                    auto r = dins (posa (SLfo, "LFO · RODA · PRESSIÓ"_ca, x0 + ampleTot - x, y2, h2));
                    auto f1 = r.removeFromTop (28);
                    zonaLlum = f1.removeFromLeft (26).withSizeKeepingCentre (18, 18);
                    mostra (lOna, f1);
                    r.removeFromTop (4);
                    auto f2 = r.removeFromTop (r.getHeight() / 2);
                    auto s2 = f2.removeFromLeft (f2.getWidth() / 2);
                    mostra (lFont, s2.removeFromTop (s2.getHeight() / 2).withSizeKeepingCentre (s2.getWidth(), 28));
                    auto s3 = s2.withSizeKeepingCentre (s2.getWidth(), 28);
                    const int wr = s3.getWidth() * 3 / 5;
                    mostra (lRitme, s3.removeFromLeft (wr));
                    mostra (lPerVeu, s3);
                    fila (f2, { lFila1[0], lFila1[1] });
                    fila (r, { lFila2[0], lFila2[1], lFila2[2], lFila2[3], lFila2[4], lFila2[5], lFila2[6] });
                }
                // ---- fila 3: caràcter, efectes, veu ----
                x = x0;
                const int y3 = y2 + h2 + g, h3 = yTec - g - y3;
                {
                    auto r = dins (posa (SAnal, "CARÀCTER ANALÒGIC"_ca, 560, y3, h3));
                    auto esq = r.removeFromLeft (96);
                    mostra (aOn, esq.withSizeKeepingCentre (90, 28));
                    const int hf = r.getHeight() / 2;
                    fila (r.removeFromTop (hf), { aKnobs[0], aKnobs[1], aKnobs[2], aKnobs[3] });
                    fila (r, { aKnobs[4], aKnobs[5], aKnobs[6], aKnobs[7] });
                }
                {
                    auto r = dins (posa (SFx, "EFECTES"_ca, 500, y3, h3));
                    auto s1 = r.removeFromTop (28);
                    mostra (xChorus, s1.removeFromLeft (s1.getWidth() / 3));
                    mostra (xDlTipus, s1.removeFromLeft (s1.getWidth() / 2));
                    mostra (xRvTipus, s1);
                    r.removeFromTop (4);
                    auto s2 = r.removeFromTop (28);
                    mostra (xDlNota, s2.removeFromLeft (s2.getWidth() / 2).reduced (40, 0));
                    r.removeFromTop (2);
                    fila (r, { fxKnobs[0], fxKnobs[1], fxKnobs[2], fxKnobs[3], fxKnobs[4], fxKnobs[5] });
                }
                {
                    auto r = dins (posa (SVeu, "VEU I SORTIDA"_ca, x0 + ampleTot - x, y3, h3));
                    zonaVu = r.removeFromRight (96).withSizeKeepingCentre (96, 64);
                    auto s1 = r.removeFromTop (28);
                    mostra (vMode, s1.removeFromLeft (s1.getWidth() * 3 / 5));
                    mostra (vFase, s1);
                    r.removeFromTop (4);
                    const int hf = r.getHeight() / 2;
                    fila (r.removeFromTop (hf), { vKnobs[0], vKnobs[1], vKnobs[2], vKnobs[3] });
                    fila (r, { vKnobs[4], vAmple, vVol });
                }
            }
            else
            {
                // ---- SIMPLE: el que cal per tocar i triar el caràcter ----
                const int h1 = 330;
                {
                    auto r = dins (posa (SOsc, "OSCIL·LADORS"_ca, 560, y1, h1));
                    const int hf = r.getHeight() / 3;
                    for (int o = 0; o < 3; ++o)
                    {
                        auto f = r.removeFromTop (hf);
                        numOsc[o] = f.removeFromLeft (22);
                        mostra (oOna[o], f.removeFromLeft (220).withSizeKeepingCentre (220, 28));
                        f.removeFromLeft (8);
                        fila (f, { oPeu[o], oFi[o], oNiv[o] });
                    }
                }
                {
                    auto r = dins (posa (SFiltre, "FILTRE"_ca, 440, y1, h1));
                    mostra (fTipus, r.removeFromTop (28));
                    r.removeFromTop (6);
                    auto dalt = r.removeFromTop (r.getHeight() * 3 / 5);
                    mostra (fTall, dalt.removeFromLeft (dalt.getWidth() / 2));
                    mostra (fRes, dalt.reduced (10, 16));
                    fila (r, { fFila[0], fFila[3] });
                }
                {
                    auto r = dins (posa (SEnv, "ENVOLUPANT"_ca, x0 + ampleTot - x, y1, h1));
                    titolsEnv = {};
                    const int w = r.getWidth() / 4;
                    for (auto* f : envA) mostra (f, r.removeFromLeft (w).reduced (8, 10));
                }
                x = x0;
                const int y2 = y1 + h1 + g, h2 = yTec - g - y2;
                {
                    auto r = dins (posa (SAnal, "CARÀCTER ANALÒGIC"_ca, 560, y2, h2));
                    auto esq = r.removeFromLeft (96);
                    mostra (aOn, esq.withSizeKeepingCentre (90, 28));
                    fila (r.withSizeKeepingCentre (r.getWidth(), std::min (r.getHeight(), 120)), { aKnobs[0], aKnobs[3], aKnobs[4], aKnobs[6] });   // deriva, calidesa, saturació, edat
                }
                {
                    auto r = dins (posa (SFx, "EFECTES"_ca, 440, y2, h2));
                    mostra (xChorus, r.removeFromTop (28).reduced (40, 0));
                    r.removeFromTop (6);
                    fila (r.withSizeKeepingCentre (r.getWidth(), std::min (r.getHeight(), 110)), { fxKnobs[2], fxKnobs[4] });   // delay i reverb
                }
                {
                    auto r = dins (posa (SVeu, "VEU I SORTIDA"_ca, x0 + ampleTot - x, y2, h2));
                    zonaVu = r.removeFromRight (96).withSizeKeepingCentre (96, 64);
                    mostra (vMode, r.removeFromTop (28));
                    r.removeFromTop (6);
                    fila (r.withSizeKeepingCentre (r.getWidth(), std::min (r.getHeight(), 110)), { vKnobs[3], vKnobs[1], vVol });   // glide, unison, volum
                }
            }
            {
                auto extra = posaTeclat ({ x0, yTec, ampleTot, H - yTec - 26 }, 520);
                btVista.setBounds (extra.removeFromLeft (100).withSizeKeepingCentre (100, 40));
                extra.removeFromLeft (8);
                zonaArp = extra;
                ledArp = juce::Rectangle<int> (extra.getRight() - 12, extra.getY() + 1, 10, 10);
                fila (extra.withTrimmedTop (14), { filaArp.begin(), filaArp.end() });
            }
        }

    private:
        Vintage& vin;
        Boto btVista { "VISTA", std::nullopt, Boto::Estil::Tecla };
        bool avancat = false;
        SelectorSB *oOna[3] {}, *xSubOct = nullptr, *xSubOna = nullptr, *xColor = nullptr, *fTipus = nullptr, *fPend = nullptr, *fMode = nullptr;
        SelectorSB *lOna = nullptr, *lRitme = nullptr, *lFont = nullptr, *xChorus = nullptr, *xDlNota = nullptr, *xDlTipus = nullptr, *xRvTipus = nullptr;
        SelectorSB *vMode = nullptr, *vFase = nullptr;
        PomSB *oPeu[3] {}, *oSemi[3] {}, *oFi[3] {}, *oPw[3] {}, *oNiv[3] {}, *mEnv2 = nullptr, *mFm = nullptr, *xSub = nullptr, *xNz = nullptr, *xRing = nullptr;
        PomSB *fTall = nullptr, *fRes = nullptr, *eVel = nullptr, *vVol = nullptr, *vAmple = nullptr;
        InterruptorSB *mSync = nullptr, *mLent = nullptr, *lPerVeu = nullptr, *aOn = nullptr;
        std::vector<PomSB*> fFila, lFila1, lFila2, aKnobs, fxKnobs, vKnobs, filaArp;
        std::vector<FaderSB*> envA, envF;
        juce::Rectangle<int> numOsc[3], titolsEnv, zonaVu, zonaLlum, zonaArp, ledArp;
        float agulla = 0.0f;

        juce::String etiquetaDe (const std::string& e) override
        {
            Vintage s;
            s.posaEstat (e);
            static const char* filtres[] = { "Ladder", "OTA", "SEM", "MS-20" };
            static const char* modes[] = { "Mono", "Legato", "Poli" };
            int n = 0;
            for (int o = 0; o < 3; ++o) n += s.valor (O1Niv + o * 6) > 0.0f ? 1 : 0;
            juce::String r = juce::String (n) + " osc · " + filtres[juce::jlimit (0, 3, (int) s.valor (FTipus))] + " · " + modes[juce::jlimit (0, 2, (int) s.valor (Mode))];
            if (s.valor (Unison) > 1.5f) r += " · Unison"_ca;
            if (s.valor (ArpMode) > 0.5f) r += " · Arp"_ca;
            return r;
        }
        void tic() override
        {
            // un preset no canvia la vista que has triat
            if ((sint->valor (Vista) > 0.5f) != avancat) sint->posa (Vista, avancat ? 1.0f : 0.0f);
            // l'agulla té inèrcia, com un vúmetre de veritat
            const float db = 20.0f * std::log10 (std::max (1e-5f, sint->nivell()));
            const float obj = juce::jlimit (0.0f, 1.0f, (db + 30.0f) / 33.0f);
            agulla += (obj - agulla) * (obj > agulla ? 0.35f : 0.12f);
            if (! zonaVu.isEmpty()) repaint (zonaVu);
            if (! zonaLlum.isEmpty()) repaint (zonaLlum.expanded (6));
            repaint (ledArp.expanded (6));
        }
        void paint (juce::Graphics& g) override
        {
            PanellSinte::paint (g);
            g.setFont (dibuix::lletra (10.0f, true));
            g.setColour (colorVintage (SArp));
            textEspaiat (g, "ARPEGIADOR / SEQÜÈNCIES"_ca, zonaArp.toFloat().removeFromTop (12.0f), juce::Justification::centredLeft, 1.6f);
            const int pasA = sinte().pasArp();
            bombeta (g, ledArp, colorVintage (SArp), pasA >= 0 && pasA % 2 == 0 ? 1.0f : 0.15f);
        }
        /** Una bombeta de pilot (com les dels aparells antics): no és cap pantalla. */
        static void bombeta (juce::Graphics& g, juce::Rectangle<int> zona, juce::Colour col, float llum)
        {
            auto r = zona.toFloat();
            g.setColour (juce::Colour (0xff0d0c0a));
            g.fillEllipse (r.expanded (2.0f));
            g.setGradientFill (juce::ColourGradient (col.withMultipliedBrightness (0.25f + 0.9f * llum), r.getCentreX(), r.getCentreY(),
                                                     col.withMultipliedBrightness (0.15f + 0.3f * llum), r.getX(), r.getY(), true));
            g.fillEllipse (r);
            if (llum > 0.3f) { g.setColour (col.withAlpha (0.25f * llum)); g.fillEllipse (r.expanded (4.0f)); }
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.fillEllipse (r.getX() + r.getWidth() * 0.25f, r.getY() + r.getHeight() * 0.2f, r.getWidth() * 0.25f, r.getHeight() * 0.2f);
        }
        void detallsSeccio (juce::Graphics& g, const sinteui::Seccio& s) override
        {
            const auto col = colorVintage (s.id);
            if (s.id == SOsc)
            {
                g.setFont (dibuix::lletra (16.0f, true));
                for (int o = 0; o < 3; ++o)
                {
                    if (numOsc[o].isEmpty()) continue;
                    g.setColour (vin.valor (O1Niv + o * 6) > 0.0f || (o == 2 && vin.valor (O3Baixa) > 0.5f) ? col : sb::serigrafiaSuau);
                    g.drawText (juce::String (o + 1), numOsc[o], juce::Justification::centred);
                }
            }
            else if (s.id == SEnv && ! titolsEnv.isEmpty())
            {
                auto tt = titolsEnv.toFloat().withTrimmedRight (64.0f);
                const float w = tt.getWidth() / 2.0f;
                g.setFont (dibuix::lletra (9.5f, true));
                g.setColour (colorVintage (SEnv));
                textEspaiat (g, "VOLUM", tt.removeFromLeft (w), juce::Justification::centred, 1.5f);
                g.setColour (colorVintage (SFiltre));
                textEspaiat (g, "FILTRE", tt, juce::Justification::centred, 1.5f);
            }
            else if (s.id == SLfo && ! zonaLlum.isEmpty())
                bombeta (g, zonaLlum, col, 0.5f + 0.5f * vin.lfoAra());
            else if (s.id == SVeu && ! zonaVu.isEmpty()) vumetre (g, zonaVu.toFloat());
        }
        /** Vúmetre d'agulla: esfera crema, escala de −20 a +3 i agulla negra. */
        void vumetre (juce::Graphics& g, juce::Rectangle<float> r)
        {
            g.setColour (juce::Colour (0xff0b0a08));
            g.fillRoundedRectangle (r.expanded (3.0f), 4.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff0e2b8), r.getCentreX(), r.getY(), juce::Colour (0xffcfb987), r.getCentreX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 3.0f);
            const juce::Point<float> piv (r.getCentreX(), r.getBottom() + r.getHeight() * 0.45f);
            const float rad = r.getHeight() * 1.18f, a0 = -0.7f, a1 = 0.7f;
            g.setColour (juce::Colour (0xff2a2218));
            for (int i = 0; i <= 10; ++i)
            {
                const float a = a0 + (a1 - a0) * (float) i / 10.0f;
                g.drawLine ({ piv.getPointOnCircumference (rad - (i % 5 == 0 ? 9.0f : 5.0f), a), piv.getPointOnCircumference (rad, a) }, i % 5 == 0 ? 1.4f : 0.9f);
            }
            juce::Path vermell;
            vermell.addCentredArc (piv.x, piv.y, rad - 2.0f, rad - 2.0f, 0.0f, a0 + (a1 - a0) * 0.82f, a1, true);
            g.setColour (juce::Colour (0xffb0402e));
            g.strokePath (vermell, juce::PathStrokeType (3.0f));
            g.setFont (dibuix::lletra (8.0f, true));
            g.setColour (juce::Colour (0xff2a2218));
            g.drawText ("VU", r.withTrimmedTop (r.getHeight() * 0.55f), juce::Justification::centred);
            const float a = a0 + (a1 - a0) * agulla;
            g.saveState();
            g.reduceClipRegion (r.toNearestInt());
            g.setColour (juce::Colour (0xff15110c));
            g.drawLine ({ piv.getPointOnCircumference (rad * 0.35f, a), piv.getPointOnCircumference (rad + 2.0f, a) }, 1.4f);
            g.restoreState();
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.25f), r.getX(), r.getY(), juce::Colours::white.withAlpha (0.0f), r.getX(), r.getCentreY(), false));
            g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.5f), 3.0f);
        }
    };
}

std::unique_ptr<juce::DocumentWindow> creaFinestraVintage (std::shared_ptr<Vintage> sinte, EstatEstudi& estat, GestorPlugins* plugins,
                                                           const std::string& idPista, const juce::String& titol,
                                                           std::function<void (std::uint8_t, std::uint8_t, std::uint8_t)> directe,
                                                           std::function<void()> enTancar)
{
    auto* p = new PanellVintage (std::move (sinte), estat, plugins, idPista, std::move (directe), enTancar);
    return std::make_unique<FinestraSinte> (titol, p, std::move (enTancar));
}
