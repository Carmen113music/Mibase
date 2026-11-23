#include "FinestraSubBass.h"
#include "SinteUI.h"

using namespace mibase::subbass;
using namespace sinteui;

/* 0.88: Mibase SUB & BASS. Galtes de noguera, metall raspallat, capçalera de carboni i llums de color per secció.
   0.89: va sobre la base comuna (SinteUI.h) i té l'arpegiador / seqüenciador al costat del teclat. */
namespace
{
    juce::Colour colorSB (int s)
    {
        static const juce::uint32 c[] = { 0xff35e0d2, 0xff8a7bff, 0xffffad3b, 0xff8fe07a, 0xffff6fb5, 0xffff5a36, 0xffb48cff, 0xff6fd3ff, 0xffffd84d };
        return juce::Colour (c[juce::jlimit (0, 8, s)]);
    }

    ConfigSinte configSubBass()
    {
        ConfigSinte c;
        c.marca = "SUB & BASS";
        c.subtitol = "SINTETITZADOR DE BAIXOS";
        c.carpeta = "SubBass"; c.extensio = "mbsub"; c.tipusFitxer = "mibase-subbass";
        c.presets = &presetsFabrica();
        c.categories = &categories();
        c.inicial = [] { return estatInicial(); };
        c.aleatori = [] (unsigned l) { return aleatori (l); };
        c.varia = [] (const std::string& e, unsigned l, float q) { return varia (e, l, q); };
        c.previsualitza = [] (const std::string& e, float* o, int n) { return previsualitza (e, o, n, 36); };
        c.textPrevi = "PREVISUALITZACIÓ · C2"_ca;
        c.akai = { FTall, FRes, FEnv, FD, DQuant, Glide, LQuant, Volum };
        c.textAkai = "AKAI: K1 tall, K2 ressonància, K3 envolupant, K4 caiguda del filtre, K5 drive, K6 glide, K7 LFO, K8 volum"_ca;
        c.colorSeccio = colorSB;
        c.accent = juce::Colour (0xff35e0d2);
        c.colorMarca = juce::Colour (0xff35e0d2);
        c.colorFranja = juce::Colour (0xff35e0d2);
        c.fons = Textura::Metall; c.galtes = Textura::Fusta; c.capcalera = Textura::Carboni;
        return c;
    }

    class PanellSubBass : public PanellSinte
    {
    public:
        PanellSubBass (std::shared_ptr<SubBass> s, EstatEstudi& e, GestorPlugins* gp, std::string idPista,
                       std::function<void (std::uint8_t, std::uint8_t, std::uint8_t)> dir, std::function<void()> tanca)
            : PanellSinte (s, e, gp, std::move (idPista), configSubBass(), std::move (dir), std::move (tanca)),
              grafic (*this, { AA, AD, AS, AR, FA, FD, FS, FR }, colorSB (SEnv), colorSB (SFiltre))
        {
            using E = PomSB::Estil;
            o1Ona = sel (O1Ona, true); o2Ona = sel (O2Ona, true);
            for (int p : { (int) O1Oct, (int) O1Semi, (int) O1Fi, (int) O1Pw, (int) O1Niv }) fila1.push_back (pom (p, E::Petit));
            for (int p : { (int) O2Oct, (int) O2Semi, (int) O2Fi, (int) O2Pw, (int) O2Niv }) fila2.push_back (pom (p, E::Petit));
            fila3.push_back (intr (Sync));
            for (int p : { (int) Fm, (int) Unison, (int) UniDetune, (int) UniAmple, (int) Fase }) fila3.push_back (pom (p, E::Petit));
            fila3.push_back (intr (FaseLliure));
            fila3.push_back (pom (Clic, E::Petit));
            subOna = sel (SubOna, true); subOct = sel (SubOct, false);
            subNiv = pom (SubNiv, E::Faldilla); subFil = intr (SubFiltre);
            soroll = pom (SorollNiv, E::Petit); sorollC = pom (SorollColor, E::Petit);
            fModel = sel (FModel, false); fPend = sel (FPendent, false);
            fTall = pom (FTall, E::Gran); fRes = pom (FRes, E::Faldilla);
            for (int p : { (int) FEnv, (int) FKey, (int) FDrive }) filaFiltre.push_back (pom (p, E::Petit));
            for (int p : { (int) AA, (int) AD, (int) AS, (int) AR }) faders.push_back (fad (p, colorSB (SEnv)));
            for (int p : { (int) FA, (int) FD, (int) FS, (int) FR }) faders.push_back (fad (p, colorSB (SFiltre)));
            addAndMakeVisible (grafic);
            lOna = sel (LOna, true); lDesti = sel (LDesti, false, "Destí"_ca);
            lVel = pom (LVel, E::Faldilla); lQuant = pom (LQuant, E::Faldilla); lRitme = pom (LRitme, E::Petit); lRet = intr (LRetrig);
            dTipus = sel (DTipus, false); dLloc = sel (DLloc, false);
            dQuant = pom (DQuant, E::Faldilla); dTo = pom (DTo, E::Petit); dMix = pom (DMix, E::Petit);
            filaMod.push_back (pom (RodaTall, E::Petit, "Roda → tall"_ca));
            filaMod.push_back (pom (RodaLfo, E::Petit, "Roda → LFO"_ca));
            filaMod.push_back (pom (BendRang, E::Petit, "Bend"_ca));
            filaMod.push_back (pom (VelVol, E::Petit, "Vel → vol"_ca));
            filaMod.push_back (pom (VelFiltre, E::Petit, "Vel → filt"_ca));
            filaMod.push_back (pom (PEnvQuant, E::Petit, "Env. to"_ca));
            filaMod.push_back (pom (PEnvTemps, E::Petit, "Temps to"_ca));
            sVol = pom (Volum, E::Faldilla); sAmple = pom (Ample, E::Petit); sLim = intr (Limit);
            vMode = sel (Mode, false); vGlide = pom (Glide, E::Petit);
            for (int p : { (int) ArpMode, (int) ArpRitme, (int) ArpOct, (int) ArpGate, (int) ArpSwing, (int) ArpPatro }) filaArp.push_back (pom (p, E::Petit));
            setSize (cfg.ample, cfg.alt);
            pantalla.actualitza();
        }

        void resized() override
        {
            posaCapcalera();
            seccions.clear();
            const int W = getWidth(), H = getHeight();
            const int x0 = cfg.ampleGaltes + 8, x1 = W - cfg.ampleGaltes - 8;
            const int g = 8;
            const int y1 = cfg.altCap + 8, h1 = 318, y2 = y1 + h1 + g, h2 = 258, y3 = y2 + h2 + g;
            const int ampleTot = x1 - x0;
            int x = x0;
            auto posa = [&] (int idS, const juce::String& t, int w, int y, int h) { seccions.push_back ({ idS, t, { x, y, w, h } }); x += w + g; return seccions.back().r; };
            const auto rOsc = posa (SOsc, "OSCIL·LADORS"_ca, 468, y1, h1);
            const auto rSub = posa (SSub, "SUB & SOROLL"_ca, 168, y1, h1);
            const auto rFil = posa (SFiltre, "FILTRE"_ca, 300, y1, h1);
            const auto rDrv = posa (SDrive, "DRIVE"_ca, x0 + ampleTot - x, y1, h1);
            x = x0;
            const auto rEnv = posa (SEnv, "ENVOLUPANTS"_ca, 432, y2, h2);
            const auto rLfo = posa (SLfo, "LFO"_ca, 246, y2, h2);
            const auto rMod = posa (SMod, "MODULACIÓ"_ca, 262, y2, h2);
            const auto rOut = posa (SSortida, "SORTIDA"_ca, x0 + ampleTot - x, y2, h2);
            {
                auto r = dins (rOsc);
                auto filaOsc = [&] (SelectorSB* s, std::vector<PomSB*>& poms, juce::Rectangle<int> f)
                {
                    f.removeFromLeft (22);
                    s->setBounds (f.removeFromLeft (150).withSizeKeepingCentre (150, 30));
                    f.removeFromLeft (8);
                    const int w = f.getWidth() / (int) poms.size();
                    for (auto* p : poms) p->setBounds (f.removeFromLeft (w));
                };
                insignia1 = r.removeFromTop (82);
                filaOsc (o1Ona, fila1, insignia1);
                r.removeFromTop (6);
                insignia2 = r.removeFromTop (82);
                filaOsc (o2Ona, fila2, insignia2);
                r.removeFromTop (8);
                separadorOsc = r.getY() - 4;
                const int w = r.getWidth() / (int) fila3.size();
                for (auto* c : fila3) c->setBounds (r.removeFromLeft (w).withTrimmedTop (4));
            }
            {
                auto r = dins (rSub);
                subOna->setBounds (r.removeFromTop (30));
                r.removeFromTop (6);
                subOct->setBounds (r.removeFromTop (24).reduced (16, 0));
                r.removeFromTop (4);
                auto f = r.removeFromTop (92);
                subFil->setBounds (f.removeFromRight (52));
                subNiv->setBounds (f);
                r.removeFromTop (6);
                separadorSub = r.getY() - 2;
                const int w = r.getWidth() / 2;
                soroll->setBounds (r.removeFromLeft (w).withTrimmedTop (6));
                sorollC->setBounds (r.withTrimmedTop (6));
            }
            {
                auto r = dins (rFil);
                fModel->setBounds (r.removeFromTop (28));
                r.removeFromTop (4);
                fPend->setBounds (r.removeFromTop (22).withSizeKeepingCentre (130, 22));
                r.removeFromTop (2);
                auto f = r.removeFromTop (150);
                fRes->setBounds (f.removeFromRight (86).withSizeKeepingCentre (86, 96));
                fTall->setBounds (f);
                const int w = r.getWidth() / 3;
                for (auto* p : filaFiltre) p->setBounds (r.removeFromLeft (w));
            }
            {
                auto r = dins (rDrv);
                dTipus->setBounds (r.removeFromTop (28));
                r.removeFromTop (6);
                dQuant->setBounds (r.removeFromTop (112).withSizeKeepingCentre (110, 112));
                r.removeFromTop (4);
                auto f = r.removeFromTop (78);
                dTo->setBounds (f.removeFromLeft (f.getWidth() / 2));
                dMix->setBounds (f);
                r.removeFromTop (4);
                dLloc->setBounds (r.removeFromTop (26));
            }
            {
                auto r = dins (rEnv);
                grafic.setBounds (r.removeFromTop (58));
                r.removeFromTop (6);
                titolsEnv = r.removeFromTop (14);
                const int w = r.getWidth() / 8;
                for (auto* f : faders) f->setBounds (r.removeFromLeft (w));
            }
            {
                auto r = dins (rLfo);
                auto f = r.removeFromTop (28);
                ledLfo = f.removeFromRight (20).withSizeKeepingCentre (12, 12);
                lOna->setBounds (f);
                r.removeFromTop (6);
                auto f2 = r.removeFromTop (92);
                const int w = f2.getWidth() / 3;
                lVel->setBounds (f2.removeFromLeft (w));
                lRitme->setBounds (f2.removeFromLeft (w).withSizeKeepingCentre (w, 72));
                lQuant->setBounds (f2);
                r.removeFromTop (6);
                auto f3 = r.removeFromTop (44);
                lRet->setBounds (f3.removeFromRight (56));
                lDesti->setBounds (f3.withTrimmedRight (6));
            }
            {
                auto r = dins (rMod);
                auto f1 = r.removeFromTop (r.getHeight() / 2);
                const int w1 = f1.getWidth() / 3;
                for (int i = 0; i < 3; ++i) filaMod[(size_t) i]->setBounds (f1.removeFromLeft (w1).reduced (0, 6));
                const int w2 = r.getWidth() / 4;
                for (int i = 3; i < 7; ++i) filaMod[(size_t) i]->setBounds (r.removeFromLeft (w2).reduced (0, 6));
            }
            {
                auto r = dins (rOut);
                pantalla.setBounds (r.removeFromTop (96));
                r.removeFromTop (8);
                zonaVu = r.removeFromRight (16).reduced (2, 6);
                auto f = r;
                sVol->setBounds (f.removeFromLeft (f.getWidth() / 2));
                auto d = f;
                sAmple->setBounds (d.removeFromTop (d.getHeight() * 55 / 100));
                sLim->setBounds (d);
            }
            {
                auto extra = posaTeclat ({ x0, y3, ampleTot, H - y3 - 26 }, 500);
                auto v = extra.removeFromLeft (150);
                vMode->setBounds (v.removeFromTop (34).withTrimmedTop (4));
                vGlide->setBounds (v.withTrimmedTop (2));
                extra.removeFromLeft (14);
                zonaArp = extra;
                auto a = extra.withTrimmedTop (14);
                ledArp = juce::Rectangle<int> (extra.getRight() - 12, extra.getY() + 1, 10, 10);
                const int w = a.getWidth() / (int) filaArp.size();
                for (auto* p : filaArp) p->setBounds (a.removeFromLeft (w));
            }
        }

    private:
        GraficEnv grafic;
        SelectorSB *o1Ona = nullptr, *o2Ona = nullptr, *subOna = nullptr, *subOct = nullptr, *fModel = nullptr, *fPend = nullptr,
                   *lOna = nullptr, *lDesti = nullptr, *dTipus = nullptr, *dLloc = nullptr, *vMode = nullptr;
        std::vector<ControlSB*> fila3;
        std::vector<PomSB*> fila1, fila2, filaFiltre, filaMod, filaArp;
        std::vector<FaderSB*> faders;
        PomSB *subNiv = nullptr, *soroll = nullptr, *sorollC = nullptr, *fTall = nullptr, *fRes = nullptr, *lVel = nullptr, *lQuant = nullptr, *lRitme = nullptr,
              *dQuant = nullptr, *dTo = nullptr, *dMix = nullptr, *sVol = nullptr, *sAmple = nullptr, *vGlide = nullptr;
        InterruptorSB *subFil = nullptr, *lRet = nullptr, *sLim = nullptr;
        juce::Rectangle<int> zonaVu, ledLfo, ledArp, zonaArp, insignia1, insignia2, titolsEnv;
        int separadorOsc = 0, separadorSub = 0;

        juce::String etiquetaDe (const std::string& e) override
        {
            SubBass s;
            s.posaEstat (e);
            auto t = u8 (opcions (FModel)[(size_t) s.valor (FModel)]) + " · "_ca + u8 (opcions (Mode)[(size_t) s.valor (Mode)]);
            if (s.valor (ArpMode) > 0.5f) t += " · Arp"_ca;
            return t;
        }
        void canviat() override { grafic.repaint(); }
        void tic() override
        {
            repaint (zonaVu);
            repaint (ledLfo.expanded (6));
            repaint (ledArp.expanded (6));
        }
        void paint (juce::Graphics& g) override
        {
            PanellSinte::paint (g);
            g.setFont (dibuix::lletra (10.0f, true));
            g.setColour (colorSB (SArp));
            textEspaiat (g, "ARPEGIADOR / SEQÜENCIADOR"_ca, zonaArp.toFloat().removeFromTop (12.0f), juce::Justification::centredLeft, 1.6f);
            const int pasA = sinte().pasArp();
            dibuixaLed (g, ledArp, colorSB (SArp), pasA >= 0 && pasA % 2 == 0 ? 1.0f : 0.35f, pasA >= 0);
        }
        void detallsSeccio (juce::Graphics& g, const sinteui::Seccio& s) override
        {
            const auto col = colorSB (s.id);
            if (s.id == SOsc)
            {
                for (auto [ins, n] : { std::pair { insignia1, 1 }, std::pair { insignia2, 2 } })
                {
                    auto c = juce::Rectangle<float> (18.0f, 18.0f).withCentre ({ (float) ins.getX() + 9.0f, (float) ins.getY() + 22.0f });
                    g.setColour (col.withAlpha (0.2f));
                    g.fillEllipse (c.expanded (2.0f));
                    g.setColour (col);
                    g.drawEllipse (c, 1.2f);
                    g.setFont (dibuix::lletra (11.0f, true));
                    g.drawText (juce::String (n), c, juce::Justification::centred);
                }
                g.setColour (juce::Colours::black.withAlpha (0.4f));
                g.fillRect ((float) s.r.getX() + 12.0f, (float) separadorOsc, (float) s.r.getWidth() - 24.0f, 1.0f);
                g.setColour (juce::Colours::white.withAlpha (0.05f));
                g.fillRect ((float) s.r.getX() + 12.0f, (float) separadorOsc + 1.0f, (float) s.r.getWidth() - 24.0f, 1.0f);
            }
            else if (s.id == SSub)
            {
                g.setColour (juce::Colours::black.withAlpha (0.4f));
                g.fillRect ((float) s.r.getX() + 12.0f, (float) separadorSub, (float) s.r.getWidth() - 24.0f, 1.0f);
            }
            else if (s.id == SEnv)
            {
                auto tt = titolsEnv.toFloat();
                g.setFont (dibuix::lletra (9.5f, true));
                g.setColour (colorSB (SEnv));
                textEspaiat (g, "VOLUM", tt.removeFromLeft (tt.getWidth() / 2).reduced (8.0f, 0.0f), juce::Justification::centred, 1.5f);
                g.setColour (colorSB (SFiltre));
                textEspaiat (g, "FILTRE", tt.reduced (8.0f, 0.0f), juce::Justification::centred, 1.5f);
                g.setColour (juce::Colours::black.withAlpha (0.4f));
                g.fillRect ((float) titolsEnv.getCentreX() - 0.5f, (float) titolsEnv.getY(), 1.0f, (float) (s.r.getBottom() - titolsEnv.getY() - 10));
            }
            else if (s.id == SLfo)
            {
                const bool actiu = sinte().valor (LQuant) > 0.001f || sinte().valor (RodaLfo) > 0.001f;
                dibuixaLed (g, ledLfo, col, 0.5f + 0.5f * sinte().lfo(), actiu);
            }
            else if (s.id == SSortida) dibuixaVu (g, zonaVu);
        }
    };
}

std::unique_ptr<juce::DocumentWindow> creaFinestraSubBass (std::shared_ptr<SubBass> sinte, EstatEstudi& estat, GestorPlugins* plugins,
                                                           const std::string& idPista, const juce::String& titol,
                                                           std::function<void (std::uint8_t, std::uint8_t, std::uint8_t)> directe,
                                                           std::function<void()> enTancar)
{
    auto* p = new PanellSubBass (std::move (sinte), estat, plugins, idPista, std::move (directe), enTancar);
    return std::make_unique<FinestraSinte> (titol, p, std::move (enTancar));
}
