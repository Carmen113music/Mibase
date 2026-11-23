#include "FinestraLead.h"
#include "SinteUI.h"

using namespace mibase::lead;
using namespace sinteui;

/* 0.89: Mibase LEAD. Personalitat pròpia: panell blau nit, galtes de laca negra, llums de posta de sol (magenta → ambre),
   poms de crom amb arc de llum per als controls principals i poms de baquelita amb anell metàl·lic (estil dels polifònics
   dels 80). La forma d'ona viu a la secció MIX; l'arpegiador, al costat del teclat. */
namespace
{
    juce::Colour colorLead (int s)
    {
        static const juce::uint32 c[] = { 0xffff4fa3, 0xffff7a5c, 0xffffb347, 0xff5ce1e6, 0xffa78bfa, 0xff4fd1a5, 0xfff9e264, 0xffff9fd0, 0xff7fb0ff };
        return juce::Colour (c[juce::jlimit (0, 8, s)]);
    }

    ConfigSinte configLead()
    {
        ConfigSinte c;
        c.marca = "LEAD";
        c.subtitol = "SINTETITZADOR DE SOLISTES";
        c.carpeta = "Lead"; c.extensio = "mblead"; c.tipusFitxer = "mibase-lead";
        c.presets = &presetsFabrica();
        c.categories = &categories();
        c.inicial = [] { return estatInicial(); };
        c.aleatori = [] (unsigned l) { return aleatori (l); };
        c.varia = [] (const std::string& e, unsigned l, float q) { return varia (e, l, q); };
        c.previsualitza = [] (const std::string& e, float* o, int n) { return previsualitza (e, o, n, 60); };
        c.textPrevi = "FORMA D'ONA · C4"_ca;
        c.akai = { FTall, FRes, FEnv, Glide, L1Quant, DQuant, DlMix, RvMix };
        c.textAkai = "AKAI: K1 tall, K2 ressonància, K3 envolupant, K4 glide, K5 vibrato (LFO 1), K6 drive, K7 delay, K8 reverb"_ca;
        c.colorSeccio = colorLead;
        c.accent = juce::Colour (0xffff4fa3);
        c.colorMarca = juce::Colour (0xffffb347);
        c.colorFranja = juce::Colour (0xffff4fa3);
        c.fons = Textura::Nit; c.galtes = Textura::Laca; c.capcalera = Textura::Nit;
        c.seccioDalt = juce::Colour (0xff1e2136);
        c.seccioBaix = juce::Colour (0xff121426);
        c.ample = 1320; c.alt = 884;
        return c;
    }

    class PanellLead : public PanellSinte
    {
    public:
        PanellLead (std::shared_ptr<Lead> s, EstatEstudi& e, GestorPlugins* gp, std::string idPista,
                    std::function<void (std::uint8_t, std::uint8_t, std::uint8_t)> dir, std::function<void()> tanca)
            : PanellSinte (s, e, gp, std::move (idPista), configLead(), std::move (dir), std::move (tanca)),
              grafic (*this, { AA, AD, AS, AR, FA, FD, FS, FR }, colorLead (SEnv), colorLead (SFiltre))
        {
            using E = PomSB::Estil;
            for (int o = 0; o < 3; ++o)
            {
                const int b = o * 6;
                ones[o] = sel (O1Ona + b, true);
                for (int p : { O1Oct + b, O1Semi + b, O1Fi + b, O1Forma + b, O1Taula + b }) osc[o].push_back (pom (p, E::Petit));
            }
            filaOsc.push_back (intr (Sync2)); filaOsc.push_back (intr (Sync3));
            for (int p : { (int) Fm12, (int) Fm31, (int) Unison, (int) UniDetune, (int) UniAmple, (int) Deriva }) filaOsc.push_back (pom (p, E::Petit));
            filaOsc.push_back (intr (FaseLliure, "Lliure"_ca));
            for (int p : { (int) Niv1, (int) Niv2, (int) Niv3 }) mix1.push_back (pom (p, E::Petit));
            subNiv = pom (SubNiv, E::Petit); subOct = sel (SubOct, false); subOna = sel (SubOna, false);
            for (int p : { (int) SorollNiv, (int) SorollColor, (int) Ring }) mix3.push_back (pom (p, E::Petit));
            fTipus = sel (FTipus, false); fPend = sel (FPendent, false);
            fTall = pom (FTall, E::Crom); fRes = pom (FRes, E::Bakelita);
            for (int p : { (int) FEnv, (int) FKey, (int) FDrive }) filaFiltre.push_back (pom (p, E::Petit));
            for (int p : { (int) AA, (int) AD, (int) AS, (int) AR }) faders.push_back (fad (p, colorLead (SEnv)));
            for (int p : { (int) FA, (int) FD, (int) FS, (int) FR }) faders.push_back (fad (p, colorLead (SFiltre)));
            addAndMakeVisible (grafic);
            for (int l = 0; l < 2; ++l)
            {
                const int b = l * 7;
                lOna[l] = sel (L1Ona + b, true);
                for (int p : { L1Vel + b, L1Ritme + b, L1Desti + b, L1Quant + b, L1Retard + b }) lfo[l].push_back (pom (p, E::Petit));
                lRet[l] = intr (L1Reinicia + b, "Reinicia"_ca);
            }
            dTipus = sel (DTipus, false);
            dQuant = pom (DQuant, E::Bakelita, "Drive"_ca); dTo = pom (DTo, E::Petit);
            chMix = pom (ChMix, E::Bakelita, "Mix"_ca); chVel = pom (ChVel, E::Petit, "Vel."_ca); chProf = pom (ChProf, E::Petit, "Prof."_ca);
            dlMix = pom (DlMix, E::Bakelita, "Mix"_ca); dlNota = pom (DlNota, E::Petit); dlFb = pom (DlFeedback, E::Petit); dlPp = intr (DlPingPong);
            rvMix = pom (RvMix, E::Bakelita, "Mix"_ca); rvMida = pom (RvMida, E::Petit);
            for (int p : { (int) RodaVib, (int) RodaTall, (int) AtVib, (int) AtTall, (int) VelVol, (int) VelFiltre, (int) BendRang, (int) PEnvQuant, (int) PEnvTemps, (int) VibVel })
                filaMod.push_back (pom (p, E::Petit));
            vMode = sel (Mode, false);
            vGlide = pom (Glide, E::Bakelita); vVol = pom (Volum, E::Crom); vAmple = pom (Ample, E::Petit);
            for (int p : { (int) ArpMode, (int) ArpRitme, (int) ArpOct, (int) ArpGate, (int) ArpSwing, (int) ArpPatro }) filaArp.push_back (pom (p, E::Petit));
            teclat.base = 48;
            setSize (cfg.ample, cfg.alt);
            pantalla.actualitza();
        }

        void resized() override
        {
            posaCapcalera();
            seccions.clear();
            const int W = getWidth(), H = getHeight();
            const int x0 = cfg.ampleGaltes + 8, x1 = W - cfg.ampleGaltes - 8;
            const int g = 8, ampleTot = x1 - x0;
            const int y1 = cfg.altCap + 8, h1 = 282, y2 = y1 + h1 + g, h2 = 250, y3 = y2 + h2 + g, h3 = 104, y4 = y3 + h3 + g;
            int x = x0;
            auto posa = [&] (int idS, const juce::String& t, int w, int y, int h) { seccions.push_back ({ idS, t, { x, y, w, h } }); x += w + g; return seccions.back().r; };
            const auto rOsc = posa (SOsc, "OSCIL·LADORS"_ca, 570, y1, h1);
            const auto rMix = posa (SMix, "MIX", 236, y1, h1);
            const auto rFil = posa (SFiltre, "FILTRE"_ca, x0 + ampleTot - x, y1, h1);
            x = x0;
            const auto rEnv = posa (SEnv, "ENVOLUPANTS"_ca, 356, y2, h2);
            const auto rLfo = posa (SLfo, "LFO"_ca, 430, y2, h2);
            const auto rFx = posa (SFx, "FX", x0 + ampleTot - x, y2, h2);
            x = x0;
            const auto rMod = posa (SMod, "MODULACIÓ"_ca, 640, y3, h3);
            const auto rVeu = posa (SVeu, "VEU I SORTIDA"_ca, x0 + ampleTot - x, y3, h3);

            {
                auto r = dins (rOsc);
                for (int o = 0; o < 3; ++o)
                {
                    auto f = r.removeFromTop (62);
                    insignies[o] = f;
                    f.removeFromLeft (22);
                    ones[o]->setBounds (f.removeFromLeft (196).withSizeKeepingCentre (196, 28));
                    f.removeFromLeft (8);
                    const int w = f.getWidth() / 5;
                    for (auto* p : osc[o]) p->setBounds (f.removeFromLeft (w));
                }
                r.removeFromTop (4);
                separadorOsc = r.getY() - 2;
                const int w = r.getWidth() / (int) filaOsc.size();
                for (auto* c : filaOsc) c->setBounds (r.removeFromLeft (w).withTrimmedTop (2));
            }
            {
                auto r = dins (rMix);
                pantalla.setBounds (r.removeFromTop (66));
                r.removeFromTop (4);
                const int hf = r.getHeight() / 3;
                auto f1 = r.removeFromTop (hf);
                const int w1 = f1.getWidth() / 3;
                for (auto* p : mix1) p->setBounds (f1.removeFromLeft (w1));
                auto f2 = r.removeFromTop (hf);
                subNiv->setBounds (f2.removeFromLeft (f2.getWidth() / 3));
                auto so = f2.reduced (4, 8);
                subOct->setBounds (so.removeFromTop (so.getHeight() / 2).reduced (0, 1));
                subOna->setBounds (so.reduced (0, 1));
                const int w3 = r.getWidth() / 3;
                for (auto* p : mix3) p->setBounds (r.removeFromLeft (w3));
            }
            {
                auto r = dins (rFil);
                auto f = r.removeFromTop (28);
                fTipus->setBounds (f.removeFromLeft (f.getWidth() * 3 / 5));
                f.removeFromLeft (8);
                fPend->setBounds (f);
                r.removeFromTop (6);
                auto h = r.removeFromTop (150);
                fRes->setBounds (h.removeFromRight (h.getWidth() / 3).withSizeKeepingCentre (96, 110));
                fTall->setBounds (h);
                const int w = r.getWidth() / 3;
                for (auto* p : filaFiltre) p->setBounds (r.removeFromLeft (w));
            }
            {
                auto r = dins (rEnv);
                grafic.setBounds (r.removeFromTop (52));
                r.removeFromTop (4);
                titolsEnv = r.removeFromTop (14);
                const int w = r.getWidth() / 8;
                for (auto* f : faders) f->setBounds (r.removeFromLeft (w));
            }
            {
                auto r = dins (rLfo);
                for (int l = 0; l < 2; ++l)
                {
                    auto f = r.removeFromTop (r.getHeight() / (2 - l));
                    zonaLfo[l] = f;
                    auto cap = f.removeFromTop (26);
                    cap.removeFromLeft (52);
                    ledLfo[l] = cap.removeFromRight (16).withSizeKeepingCentre (10, 10);
                    lRet[l]->setBounds (cap.removeFromRight (70).withTrimmedTop (-4));
                    lOna[l]->setBounds (cap.withSizeKeepingCentre (cap.getWidth() - 10, 24));
                    const int w = f.getWidth() / 5;
                    for (auto* p : lfo[l]) p->setBounds (f.removeFromLeft (w));
                }
            }
            {
                auto r = dins (rFx);
                const int wq = r.getWidth() / 2, hq = r.getHeight() / 2;
                auto q = [&] (int i) { return juce::Rectangle<int> (r.getX() + (i % 2) * wq, r.getY() + (i / 2) * hq, wq, hq).reduced (4, 2); };
                for (int i = 0; i < 4; ++i) blocsFx[i] = q (i);
                {
                    auto b = blocsFx[0].withTrimmedTop (14);
                    dTipus->setBounds (b.removeFromTop (22));
                    const int w = b.getWidth() / 2;
                    dQuant->setBounds (b.removeFromLeft (w)); dTo->setBounds (b);
                }
                {
                    auto b = blocsFx[1].withTrimmedTop (14);
                    const int w = b.getWidth() / 3;
                    chMix->setBounds (b.removeFromLeft (w)); chVel->setBounds (b.removeFromLeft (w)); chProf->setBounds (b);
                }
                {
                    auto b = blocsFx[2].withTrimmedTop (14);
                    const int w = b.getWidth() / 4;
                    dlMix->setBounds (b.removeFromLeft (w)); dlNota->setBounds (b.removeFromLeft (w)); dlFb->setBounds (b.removeFromLeft (w)); dlPp->setBounds (b);
                }
                {
                    auto b = blocsFx[3].withTrimmedTop (14);
                    const int w = b.getWidth() / 2;
                    rvMix->setBounds (b.removeFromLeft (w)); rvMida->setBounds (b);
                }
            }
            {
                auto r = dins (rMod);
                const int w = r.getWidth() / (int) filaMod.size();
                for (auto* p : filaMod) p->setBounds (r.removeFromLeft (w));
            }
            {
                auto r = dins (rVeu);
                zonaVu = r.removeFromRight (16).reduced (2, 2);
                r.removeFromRight (6);
                vMode->setBounds (r.removeFromLeft (190).withSizeKeepingCentre (190, 28));
                r.removeFromLeft (8);
                const int w = r.getWidth() / 3;
                vGlide->setBounds (r.removeFromLeft (w)); vVol->setBounds (r.removeFromLeft (w)); vAmple->setBounds (r);
            }
            {
                auto extra = posaTeclat ({ x0, y4, ampleTot, H - y4 - 26 }, 330);
                zonaArp = extra;
                ledArp = juce::Rectangle<int> (extra.getRight() - 12, extra.getY() + 1, 10, 10);
                auto a = extra.withTrimmedTop (14);
                const int w = a.getWidth() / (int) filaArp.size();
                for (auto* p : filaArp) p->setBounds (a.removeFromLeft (w));
            }
        }

    private:
        GraficEnv grafic;
        SelectorSB* ones[3] {}; SelectorSB *subOct = nullptr, *subOna = nullptr, *fTipus = nullptr, *fPend = nullptr, *lOna[2] {}, *dTipus = nullptr, *vMode = nullptr;
        std::vector<PomSB*> osc[3], mix1, mix3, filaFiltre, lfo[2], filaMod, filaArp;
        std::vector<ControlSB*> filaOsc;
        std::vector<FaderSB*> faders;
        PomSB *subNiv = nullptr, *fTall = nullptr, *fRes = nullptr, *dQuant = nullptr, *dTo = nullptr, *chMix = nullptr, *chVel = nullptr, *chProf = nullptr,
              *dlMix = nullptr, *dlNota = nullptr, *dlFb = nullptr, *rvMix = nullptr, *rvMida = nullptr, *vGlide = nullptr, *vVol = nullptr, *vAmple = nullptr;
        InterruptorSB *lRet[2] {}, *dlPp = nullptr;
        juce::Rectangle<int> insignies[3], zonaLfo[2], ledLfo[2], blocsFx[4], titolsEnv, zonaVu, zonaArp, ledArp;
        int separadorOsc = 0;

        juce::String etiquetaDe (const std::string& e) override
        {
            Lead s;
            s.posaEstat (e);
            static const char* ones3[] = { "Sinus", "Triangle", "Serra", "Quadrada", "Pols", "Supersaw", "Wavetable", "Soroll" };
            auto t = juce::String (ones3[(int) s.valor (O1Ona)]) + " · "_ca + u8 (mibase::sinte::opcions (mibase::lead::info (Mode))[(size_t) s.valor (Mode)]);
            if (s.valor (ArpMode) > 0.5f) t += " · Arp"_ca;
            return t;
        }
        void canviat() override { grafic.repaint(); }
        void tic() override
        {
            repaint (zonaVu);
            for (auto& l : ledLfo) repaint (l.expanded (6));
            repaint (ledArp.expanded (6));
        }
        void paint (juce::Graphics& g) override
        {
            PanellSinte::paint (g);
            g.setFont (dibuix::lletra (10.0f, true));
            g.setColour (colorLead (SArp));
            textEspaiat (g, "ARPEGIADOR / SEQÜENCIADOR"_ca, zonaArp.toFloat().removeFromTop (12.0f), juce::Justification::centredLeft, 1.6f);
            const int pasA = sinte().pasArp();
            dibuixaLed (g, ledArp, colorLead (SArp), pasA >= 0 && pasA % 2 == 0 ? 1.0f : 0.35f, pasA >= 0);
        }
        void detallsSeccio (juce::Graphics& g, const sinteui::Seccio& s) override
        {
            const auto col = colorLead (s.id);
            auto separador = [&] (float y, const juce::Rectangle<int>& r)
            {
                g.setColour (juce::Colours::black.withAlpha (0.45f));
                g.fillRect ((float) r.getX() + 12.0f, y, (float) r.getWidth() - 24.0f, 1.0f);
                g.setColour (juce::Colours::white.withAlpha (0.05f));
                g.fillRect ((float) r.getX() + 12.0f, y + 1.0f, (float) r.getWidth() - 24.0f, 1.0f);
            };
            auto petitTitol = [&] (juce::Rectangle<int> r, const juce::String& t, juce::Colour c)
            {
                g.setFont (dibuix::lletra (9.5f, true));
                g.setColour (c);
                textEspaiat (g, t, r.toFloat().removeFromTop (12.0f), juce::Justification::centredLeft, 1.6f);
            };
            if (s.id == SOsc)
            {
                for (int o = 0; o < 3; ++o)
                {
                    auto c = juce::Rectangle<float> (18.0f, 18.0f).withCentre ({ (float) insignies[o].getX() + 9.0f, (float) insignies[o].getCentreY() });
                    g.setColour (col.withAlpha (0.2f));
                    g.fillEllipse (c.expanded (2.0f));
                    g.setColour (col);
                    g.drawEllipse (c, 1.2f);
                    g.setFont (dibuix::lletra (11.0f, true));
                    g.drawText (juce::String (o + 1), c, juce::Justification::centred);
                }
                separador ((float) separadorOsc, s.r);
            }
            else if (s.id == SEnv)
            {
                auto tt = titolsEnv.toFloat();
                g.setFont (dibuix::lletra (9.5f, true));
                g.setColour (colorLead (SEnv));
                textEspaiat (g, "VOLUM", tt.removeFromLeft (tt.getWidth() / 2).reduced (8.0f, 0.0f), juce::Justification::centred, 1.5f);
                g.setColour (colorLead (SFiltre));
                textEspaiat (g, "FILTRE", tt.reduced (8.0f, 0.0f), juce::Justification::centred, 1.5f);
                g.setColour (juce::Colours::black.withAlpha (0.4f));
                g.fillRect ((float) titolsEnv.getCentreX() - 0.5f, (float) titolsEnv.getY(), 1.0f, (float) (s.r.getBottom() - titolsEnv.getY() - 10));
            }
            else if (s.id == SLfo)
            {
                auto* l = dynamic_cast<Lead*> (&sinte());
                for (int k = 0; k < 2; ++k)
                {
                    auto badge = zonaLfo[k].withSize (44, 20).translated (0, 2).toFloat();
                    g.setColour (col.withAlpha (0.18f));
                    g.fillRoundedRectangle (badge, 4.0f);
                    g.setColour (col);
                    g.drawRoundedRectangle (badge, 4.0f, 1.0f);
                    g.setFont (dibuix::lletra (10.0f, true));
                    g.drawText ("LFO " + juce::String (k + 1), badge, juce::Justification::centred);
                    const float v = k == 0 ? sinte().lfo() : (l != nullptr ? l->lfo2() : 0.0f);
                    dibuixaLed (g, ledLfo[k], col, 0.5f + 0.5f * v, sinte().valor (k == 0 ? L1Quant : L2Quant) > 0.001f);
                }
                separador ((float) zonaLfo[1].getY() - 2.0f, s.r);
            }
            else if (s.id == SFx)
            {
                const char* t[] = { "DRIVE", "CHORUS", "DELAY", "REVERB" };
                const int p[] = { DQuant, ChMix, DlMix, RvMix };
                for (int i = 0; i < 4; ++i)
                {
                    auto b = blocsFx[i].toFloat();
                    g.setColour (juce::Colours::black.withAlpha (0.22f));
                    g.fillRoundedRectangle (b, 5.0f);
                    const bool on = sinte().valor (p[i]) > 0.001f;
                    petitTitol (blocsFx[i].translated (8, 3), t[i], on ? col : sb::serigrafiaSuau);
                    dibuixaLed (g, juce::Rectangle<int> (blocsFx[i].getRight() - 14, blocsFx[i].getY() + 4, 8, 8), col, 1.0f, on);
                }
            }
            else if (s.id == SVeu) dibuixaVu (g, zonaVu);
        }
    };
}

std::unique_ptr<juce::DocumentWindow> creaFinestraLead (std::shared_ptr<Lead> sinte, EstatEstudi& estat, GestorPlugins* plugins,
                                                        const std::string& idPista, const juce::String& titol,
                                                        std::function<void (std::uint8_t, std::uint8_t, std::uint8_t)> directe,
                                                        std::function<void()> enTancar)
{
    auto* p = new PanellLead (std::move (sinte), estat, plugins, idPista, std::move (directe), enTancar);
    return std::make_unique<FinestraSinte> (titol, p, std::move (enTancar));
}
