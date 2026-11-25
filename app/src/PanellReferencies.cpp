#include "PanellReferencies.h"

#include "Controls.h"
#include "Dibuix.h"
#include "EscGlobal.h"
#include "PanellsMaster.h"
#include "Pom.h"
#include "Tema.h"
#include "Text.h"
#include "Vintage.h"
#include <cmath>
#include <map>
#include <juce_audio_formats/juce_audio_formats.h>
#include <mibase/Diagnosi.h>

namespace
{
    using namespace mibase;
    namespace dg = mibase::diagnosi;
    const juce::Colour ambre { 0xffffb84a };

    juce::String num (float v, int d = 1) { return d == 0 ? juce::String (juce::roundToInt (v)) : juce::String (v, d).replaceCharacter ('.', ','); }
    juce::String signe (float v, int d = 1) { return (v > 0.0f ? "+" : "") + num (v, d); }
    juce::String u8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }
    juce::String temps (double s) { const int t = (int) std::lround (s); return juce::String (t / 60) + ":" + juce::String (t % 60).paddedLeft ('0', 2); }

    std::map<std::string, std::weak_ptr<const rack::Referencia>>& memoria() { static std::map<std::string, std::weak_ptr<const rack::Referencia>> m; return m; }

    // les referències habituals: Documents\Mibase\Mastering Studio\referencies.json
    juce::File fitxerHabituals()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Mibase").getChildFile ("Mastering Studio").getChildFile ("referencies.json");
    }
    std::vector<rack::Rack::EntradaRef> habituals()
    {
        std::vector<rack::Rack::EntradaRef> v;
        const auto j = juce::JSON::parse (fitxerHabituals());
        if (auto* a = j.getArray())
            for (auto& x : *a) v.push_back ({ x["nom"].toString().toStdString(), x["cami"].toString().toStdString() });
        return v;
    }
    void desaHabituals (const std::vector<rack::Rack::EntradaRef>& v)
    {
        juce::Array<juce::var> a;
        for (auto& x : v)
        {
            auto* o = new juce::DynamicObject();
            o->setProperty ("nom", juce::String::fromUTF8 (x.nom.c_str()));
            o->setProperty ("cami", juce::String::fromUTF8 (x.cami.c_str()));
            a.add (juce::var (o));
        }
        fitxerHabituals().getParentDirectory().createDirectory();
        fitxerHabituals().replaceWithText (juce::JSON::toString (juce::var (a)));
    }

    struct Zona { int a, b; const char* nom; };
    const Zona zones[] = { { 0, 2, "Subgreus (< 50 Hz)" }, { 3, 6, "Greus (60–125 Hz)" }, { 8, 11, "Greus-mitjans (200–400 Hz)" }, { 12, 16, "Mitjos (500 Hz–1,2 kHz)" },
                           { 17, 21, "Presència (1,6–4 kHz)" }, { 22, 24, "Brillantor (5–8 kHz)" }, { 25, 27, "Aire (> 10 kHz)" } };

    // ------------------------------------------------------------------
    class PanellReferencies : public juce::Component, private juce::Timer
    {
    public:
        explicit PanellReferencies (std::shared_ptr<rack::Rack> r) : rk (std::move (r))
        {
            for (auto* b : { &btAfegeix, &btTreu, &btHabituals, &btDesaHabitual, &btInici, &btIguala, &btEscMaster, &btEscRef, &btCompara, &btMatching, &btTreuMatching })
            {
                addAndMakeVisible (*b);
                b->setClickingTogglesState (false);
            }
            btAfegeix.onClick = [this] { afegeix(); };
            btTreu.onClick = [this]
            {
                const int k = rk->refActiva;
                if (k < 0 || k >= (int) rk->referencies.size()) return;
                rk->referencies.erase (rk->referencies.begin() + k);
                rk->refActiva = rk->referencies.empty() ? -1 : juce::jmin (k, (int) rk->referencies.size() - 1);
                rk->posaReferencia (nullptr);
                if (rk->refActiva >= 0) carregaReferenciaActiva (*rk);
                rk->marcaModificat();
                masterAn.reset();
                refresca();
            };
            btHabituals.onClick = [this] { menuHabituals(); };
            btDesaHabitual.onClick = [this]
            {
                const int k = rk->refActiva;
                if (k < 0) return;
                auto v = habituals();
                for (auto& x : v) if (x.cami == rk->referencies[(size_t) k].cami) return;
                v.push_back (rk->referencies[(size_t) k]);
                desaHabituals (v);
                avis = "Desada a les habituals."_ca;
                repaint();
            };
            btInici.onClick = [this] { rk->tornaIniciRef(); };
            btIguala.onClick = [this] { rk->posaIgualaRef (! rk->igualaReferencia()); refresca(); };
            btEscMaster.onClick = [this] { rk->posaEscolta (rack::EscoltaMaster); refresca(); };
            btEscRef.onClick = [this] { rk->posaEscolta (rack::EscoltaReferencia); refresca(); };
            btCompara.onClick = [this] { compara(); };
            btMatching.onClick = [this]
            {
                if (rk->referencia() == nullptr) return;
                if (masterAn == nullptr) compara();
                if (masterAn == nullptr) return;
                dg::aplicaMatching (*rk, *masterAn, rk->referencia()->analisi, (float) quantitat / 100.0f);
                avis = "EQ matching aplicat al "_ca + juce::String (quantitat) + " % (mòdul «EQ matching», el pots retocar). Torna a comparar per veure com queda."_ca;
                refresca();
            };
            btTreuMatching.onClick = [this] { if (rk->referencia() != nullptr && masterAn != nullptr) dg::aplicaMatching (*rk, *masterAn, rk->referencia()->analisi, 0.0f);
                                              else dg::aplicaMatching (*rk, {}, {}, 0.0f);
                                              avis = "EQ matching tret."_ca; refresca(); };
            btAfegeix.setTooltip ("Tria una cançó (WAV, AIFF, FLAC, MP3 o OGG) per comparar-hi el màster."_ca);
            btHabituals.setTooltip ("Les teves referències de sempre (es desen per a tots els projectes)."_ca);
            btIguala.setTooltip ("La referència sona al mateix volum que el màster: compares el so, no qui és més fort."_ca);
            btCompara.setTooltip ("Escolta el màster sencer (sense reproduir-lo) i el compara amb la referència: to, volum, dinàmica i estèreo."_ca);
            btMatching.setTooltip ("Posa un EQ de 9 bandes que acosta el to del màster al de la referència, amb la quantitat del pom (cada banda, com a molt ±6 dB)."_ca);

            sliderInici.setSliderStyle (juce::Slider::LinearHorizontal);
            sliderInici.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            sliderInici.onDragEnd = [this] { rk->posaIniciRef ((float) sliderInici.getValue()); };
            addAndMakeVisible (sliderInici);

            pomQuantitat = std::make_unique<Pom> ("Quantitat", 0.0, 1.0, 0.5, false, [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + " %"; });
            pomQuantitat->setTooltip ("Quant s'acosta a la referència: 50 % sol ser prou; 100 % ho copia (no sempre és bona idea)."_ca);
            pomQuantitat->onCanvi = [this] (double v) { quantitat = juce::roundToInt (v * 100.0); };
            pomQuantitat->onReinicia = [this] { quantitat = 50; };
            addAndMakeVisible (*pomQuantitat);

            if (rk->referencia() == nullptr && rk->refActiva >= 0) carregaReferenciaActiva (*rk);
            refresca();
            setSize (920, 800);
            startTimerHz (8);
        }
        ~PanellReferencies() override { stopTimer(); }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (tema::fons);
            vintage::panell (g, getLocalBounds().toFloat().reduced (6.0f), 6.0f);
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (18.0f, true));
            g.drawText ("REFERÈNCIES"_ca, zonaTitol, juce::Justification::centredLeft);
            g.setColour (ambre);
            g.setFont (dibuix::lletra (10.0f, true));
            g.drawText ("MIBASE · MASTERING STUDIO · COMPARA AMB CANÇONS QUE T'AGRADEN"_ca, zonaTitol.translated (0, 20), juce::Justification::centredLeft);
            // la llista
            {
                auto z = zonaLlista;
                g.setColour (juce::Colours::black.withAlpha (0.35f));
                g.fillRoundedRectangle (z.toFloat(), 6.0f);
                if (rk->referencies.empty())
                {
                    g.setColour (tema::suau);
                    g.setFont (dibuix::lletra (12.0f));
                    g.drawFittedText ("Encara no hi ha cap referència. «Afegeix…» per triar una cançó, o «Habituals» per fer servir les de sempre."_ca, z.reduced (12), juce::Justification::centredLeft, 3);
                }
                for (int k = 0; k < (int) rk->referencies.size(); ++k)
                {
                    auto f = filaLlista (k);
                    const bool act = k == rk->refActiva;
                    if (act) { g.setColour (ambre.withAlpha (0.18f)); g.fillRoundedRectangle (f.toFloat(), 4.0f); }
                    g.setColour (act ? ambre : tema::suau);
                    g.fillEllipse (f.removeFromLeft (22).toFloat().withSizeKeepingCentre (8.0f, 8.0f));
                    g.setColour (act ? tema::text : tema::textMig);
                    g.setFont (dibuix::lletra (12.5f, act));
                    const bool existeix = juce::File (u8 (rk->referencies[(size_t) k].cami)).existsAsFile();
                    g.drawText (u8 (rk->referencies[(size_t) k].nom) + (existeix ? juce::String() : "  (no es troba el fitxer)"_ca), f, juce::Justification::centredLeft);
                }
            }
            // la referència activa
            auto ref = rk->referencia();
            {
                auto z = zonaInfo;
                g.setFont (dibuix::lletra (11.5f));
                if (ref == nullptr) { g.setColour (tema::suau); g.drawText ("Cap referència carregada."_ca, z, juce::Justification::topLeft); }
                else
                {
                    const auto& a = ref->analisi;
                    g.setColour (tema::text);
                    g.drawText (u8 (ref->nom) + "  ·  " + temps (a.segons) + "  ·  " + num (a.loud.integrated) + " LUFS  ·  LRA " + num (a.loud.lra) + " LU  ·  TP " + num (a.loud.truePeakMax)
                                + " dBTP", z.removeFromTop (18), juce::Justification::centredLeft);
                    g.setColour (tema::suau);
                    g.drawText ("Sona des de "_ca + temps (rk->iniciRef()) + "  ·  ara a " + temps (rk->posicioRef())
                                + (rk->igualaReferencia() ? "  ·  volum igualat ("_ca + signe (rk->guanyRefDb()) + " dB)" : "  ·  al seu volum"_ca), z.removeFromTop (18), juce::Justification::centredLeft);
                }
            }
            // comparació
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (10.5f, true));
            g.drawText ("COMPARACIÓ AMB EL MÀSTER (TO RELATIU, AL MATEIX VOLUM)"_ca, zonaGrafic.withHeight (14).translated (0, -18), juce::Justification::centredLeft);
            dibuixaGrafic (g, zonaGrafic.toFloat(), ref.get());
            dibuixaTaula (g, zonaTaula, ref.get());
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (10.5f, true));
            g.drawText ("EQ MATCHING", zonaMatching.withHeight (14), juce::Justification::centredLeft);
            g.setFont (dibuix::lletra (11.0f));
            g.drawFittedText ("Acosta el to del màster al de la referència, sense copiar-lo del tot. Va en un mòdul a part (EQ matching) que pots retocar o treure."_ca,
                              zonaMatching.withTrimmedTop (16).withTrimmedLeft (90).withHeight (34), juce::Justification::topLeft, 2, 0.9f);
            if (avis.isNotEmpty())
            {
                g.setColour (ambre);
                g.setFont (dibuix::lletra (11.5f));
                g.drawFittedText (avis, zonaAvis, juce::Justification::centredLeft, 2, 0.9f);
            }
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (22, 18);
            zonaTitol = r.removeFromTop (22);
            r.removeFromTop (26);
            {
                auto fila = r.removeFromTop (30);
                for (auto* b : { &btAfegeix, &btTreu, &btHabituals, &btDesaHabitual }) { b->setBounds (fila.removeFromLeft (150)); fila.removeFromLeft (8); }
            }
            r.removeFromTop (8);
            zonaLlista = r.removeFromTop (118);
            r.removeFromTop (8);
            zonaInfo = r.removeFromTop (38);
            {
                auto fila = r.removeFromTop (30);
                btEscMaster.setBounds (fila.removeFromLeft (110)); fila.removeFromLeft (6);
                btEscRef.setBounds (fila.removeFromLeft (110)); fila.removeFromLeft (12);
                btIguala.setBounds (fila.removeFromLeft (150)); fila.removeFromLeft (12);
                btInici.setBounds (fila.removeFromRight (130)); fila.removeFromRight (8);
                sliderInici.setBounds (fila);
            }
            r.removeFromTop (12);
            btCompara.setBounds (r.removeFromTop (30).removeFromLeft (240));
            r.removeFromTop (24);
            zonaAvis = r.removeFromBottom (36);
            zonaMatching = r.removeFromBottom (92);
            {
                auto m = zonaMatching.withTrimmedTop (16);
                pomQuantitat->setBounds (m.removeFromLeft (84));
                m.removeFromLeft (8);
                auto bt = m.withTrimmedTop (40).removeFromTop (30);
                btMatching.setBounds (bt.removeFromLeft (200)); bt.removeFromLeft (8);
                btTreuMatching.setBounds (bt.removeFromLeft (160));
            }
            r.removeFromBottom (10);
            zonaTaula = r.removeFromRight (340);
            r.removeFromRight (12);
            zonaGrafic = r;
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            for (int k = 0; k < (int) rk->referencies.size(); ++k)
                if (filaLlista (k).contains (e.getPosition()))
                {
                    if (k == rk->refActiva) return;
                    rk->refActiva = k;
                    rk->posaReferencia (nullptr);
                    carregaReferenciaActiva (*rk);
                    rk->marcaModificat();
                    masterAn.reset();
                    refresca();
                    return;
                }
        }
        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }

    private:
        juce::Rectangle<int> filaLlista (int k) const { return zonaLlista.reduced (8, 6).withHeight (22).translated (0, k * 22); }

        void refresca()
        {
            const bool hiHa = rk->referencia() != nullptr;
            btTreu.setEnabled (rk->refActiva >= 0);
            btDesaHabitual.setEnabled (rk->refActiva >= 0);
            for (auto* b : { &btInici, &btEscRef, &btCompara, &btMatching, &btIguala }) b->setEnabled (hiHa);
            btIguala.setToggleState (rk->igualaReferencia(), juce::dontSendNotification);
            btEscMaster.setToggleState (rk->escolta() == rack::EscoltaMaster, juce::dontSendNotification);
            btEscRef.setToggleState (rk->escolta() == rack::EscoltaReferencia, juce::dontSendNotification);
            if (auto ref = rk->referencia())
            {
                sliderInici.setRange (0.0, juce::jmax (1.0, ref->analisi.segons - 1.0), 0.5);
                sliderInici.setValue (rk->iniciRef(), juce::dontSendNotification);
            }
            sliderInici.setEnabled (hiHa);
            repaint();
        }

        void afegeix()
        {
            chooser = std::make_unique<juce::FileChooser> ("Tria una cançó de referència"_ca, juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [sp = juce::Component::SafePointer<PanellReferencies> (this)] (const juce::FileChooser& fc)
            {
                if (sp == nullptr || fc.getResult() == juce::File()) return;
                sp->afegeixFitxer (fc.getResult());
            });
        }
        void afegeixFitxer (const juce::File& f)
        {
            const auto cami = f.getFullPathName().toStdString();
            for (int k = 0; k < (int) rk->referencies.size(); ++k)
                if (rk->referencies[(size_t) k].cami == cami) { rk->refActiva = k; rk->posaReferencia (nullptr); carregaReferenciaActiva (*rk); refresca(); return; }
            juce::String error;
            auto ref = carregaReferencia (f, error);
            if (ref == nullptr) { avis = error; repaint(); return; }
            rk->referencies.push_back ({ ref->nom, cami });
            rk->refActiva = (int) rk->referencies.size() - 1;
            rk->posaIniciRef (0.0f);
            rk->posaReferencia (ref);
            rk->marcaModificat();
            masterAn.reset();
            avis = "Carregada. Prem «Referència» per escoltar-la, o «Compara amb el màster»."_ca;
            refresca();
        }
        void menuHabituals()
        {
            const auto v = habituals();
            juce::PopupMenu m;
            if (v.empty()) m.addItem (-1, "Encara no n'hi ha cap (tria una referència i «Desa com a habitual»)"_ca, false);
            for (int k = 0; k < (int) v.size(); ++k) m.addItem (k + 1, u8 (v[(size_t) k].nom), juce::File (u8 (v[(size_t) k].cami)).existsAsFile());
            if (! v.empty()) { m.addSeparator(); m.addItem (1000, "Treu de les habituals…"_ca); }
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btHabituals), [sp = juce::Component::SafePointer<PanellReferencies> (this), v] (int r)
            {
                if (sp == nullptr || r <= 0) return;
                if (r == 1000)
                {
                    juce::PopupMenu t;
                    for (int k = 0; k < (int) v.size(); ++k) t.addItem (k + 1, u8 (v[(size_t) k].nom));
                    t.showMenuAsync ({}, [v] (int q) { if (q <= 0) return; auto w = v; w.erase (w.begin() + (q - 1)); desaHabituals (w); });
                    return;
                }
                sp->afegeixFitxer (juce::File (u8 (v[(size_t) r - 1].cami)));
            });
        }

        void compara()
        {
            if (rk->referencia() == nullptr) return;
            btCompara.setButtonText ("Analitzant…"_ca);
            btCompara.setEnabled (false);
            juce::Timer::callAfterDelay (60, [sp = juce::Component::SafePointer<PanellReferencies> (this)]
            {
                if (sp == nullptr) return;
                auto inf = analitzaAra (false, contextDe (*sp->rk));
                if (sp == nullptr) return;
                sp->btCompara.setButtonText ("Compara amb el màster"_ca);
                sp->btCompara.setEnabled (true);
                if (inf == nullptr || ! inf->ambMaster) { sp->avis = "No s'ha pogut escoltar el màster (cal un projecte obert amb alguna cosa que soni)."_ca; sp->repaint(); return; }
                sp->masterAn = std::make_shared<dg::Analisi> (inf->master);
                sp->avis.clear();
                sp->repaint();
            });
        }

        void dibuixaGrafic (juce::Graphics& g, juce::Rectangle<float> r, const rack::Referencia* ref)
        {
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (r, 5.0f);
            const float mn = -24.0f, mx = 12.0f;
            auto x = [&] (int b) { return r.getX() + 8.0f + (r.getWidth() - 16.0f) * (float) b / (float) (dg::numBandes - 1); };
            auto y = [&] (float db) { return r.getBottom() - 8.0f - (juce::jlimit (mn, mx, db) - mn) / (mx - mn) * (r.getHeight() - 16.0f); };
            g.setFont (dibuix::lletra (9.5f));
            for (float db : { -18.0f, -12.0f, -6.0f, 0.0f, 6.0f })
            {
                g.setColour (tema::linia);
                g.drawHorizontalLine ((int) y (db), r.getX() + 4.0f, r.getRight() - 4.0f);
            }
            for (int b : { 2, 5, 8, 11, 14, 17, 20, 23, 26 })
            {
                g.setColour (tema::suau.withAlpha (0.7f));
                const float f = dg::freqBanda (b);
                g.drawText (f >= 1000.0f ? num (f / 1000.0f, f >= 10000.0f ? 0 : 1) + "k" : juce::String ((int) f), juce::Rectangle<float> (x (b) - 20.0f, r.getBottom() - 16.0f, 40.0f, 12.0f), juce::Justification::centred);
            }
            auto traca = [&] (const std::array<float, dg::numBandes>& c, juce::Colour col, float gruix)
            {
                juce::Path p;
                for (int b = 0; b < dg::numBandes; ++b) { if (b == 0) p.startNewSubPath (x (b), y (c[(size_t) b])); else p.lineTo (x (b), y (c[(size_t) b])); }
                g.setColour (col);
                g.strokePath (p, juce::PathStrokeType (gruix, juce::PathStrokeType::curved));
            };
            if (ref != nullptr && ref->analisi.valid) traca (ref->analisi.corba(), tema::textMig, 1.6f);
            if (masterAn != nullptr && masterAn->valid) traca (masterAn->corba(), ambre, 2.0f);
            g.setFont (dibuix::lletra (10.5f));
            g.setColour (tema::textMig);
            g.drawText ("gris: referència"_ca, r.reduced (10.0f, 6.0f), juce::Justification::topLeft);
            g.setColour (ambre);
            g.drawText (masterAn != nullptr ? "ambre: màster"_ca : "ambre: màster (prem «Compara amb el màster»)"_ca, r.reduced (10.0f, 6.0f).translated (0.0f, 14.0f), juce::Justification::topLeft);
        }

        void dibuixaTaula (juce::Graphics& g, juce::Rectangle<int> r, const rack::Referencia* ref)
        {
            g.setColour (juce::Colours::black.withAlpha (0.3f));
            g.fillRoundedRectangle (r.toFloat(), 5.0f);
            r = r.reduced (10, 8);
            if (ref == nullptr || masterAn == nullptr || ! masterAn->valid)
            {
                g.setColour (tema::suau);
                g.setFont (dibuix::lletra (11.5f));
                g.drawFittedText ("Aquí sortirà en què s'assemblen i en què es diferencien el màster i la referència."_ca, r, juce::Justification::topLeft, 4);
                return;
            }
            const auto& a = ref->analisi;
            const auto& m = *masterAn;
            auto fila = [&] (const juce::String& nom, const juce::String& valor, bool destaca)
            {
                auto f = r.removeFromTop (17);
                g.setColour (tema::suau);
                g.drawText (nom, f.removeFromLeft (170), juce::Justification::centredLeft);
                g.setColour (destaca ? ambre : tema::text);
                g.drawText (valor, f, juce::Justification::centredRight);
            };
            g.setFont (dibuix::lletra (10.5f, true));
            g.setColour (tema::suau);
            g.drawText ("TO: EL MÀSTER RESPECTE DE LA REFERÈNCIA"_ca, r.removeFromTop (16), juce::Justification::centredLeft);
            g.setFont (dibuix::lletra (11.5f));
            const auto cm = m.corba(), cr = a.corba();
            for (const auto& z : zones)
            {
                float d = 0;
                for (int b = z.a; b <= z.b; ++b) d += cm[(size_t) b] - cr[(size_t) b];
                d /= (float) (z.b - z.a + 1);
                fila (juce::String::fromUTF8 (z.nom), std::abs (d) < 0.5f ? "igual" : signe (d) + " dB", std::abs (d) >= 2.0f);
            }
            r.removeFromTop (6);
            g.setFont (dibuix::lletra (10.5f, true));
            g.setColour (tema::suau);
            g.drawText ("MÀSTER / REFERÈNCIA"_ca, r.removeFromTop (16), juce::Justification::centredLeft);
            g.setFont (dibuix::lletra (11.5f));
            auto pc = [] (float db) { return juce::String (juce::roundToInt (100.0f * std::pow (10.0f, db / 20.0f))) + " %"; };
            fila ("Loudness (LUFS)", num (m.loud.integrated) + " / " + num (a.loud.integrated), std::abs (m.loud.integrated - a.loud.integrated) > 2.0f);
            fila ("Rang (LRA)", num (m.loud.lra) + " / " + num (a.loud.lra) + " LU", std::abs (m.loud.lra - a.loud.lra) > 3.0f);
            const float pm = m.loud.truePeakMax - m.loud.integrated, pa = a.loud.truePeakMax - a.loud.integrated;
            fila ("Pics sobre el volum", num (pm) + " / " + num (pa) + " dB", std::abs (pm - pa) > 2.0f);
            fila ("Amplada (Side/Mid)", pc (m.sideMidDb) + " / " + pc (a.sideMidDb), std::abs (m.sideMidDb - a.sideMidDb) > 3.0f);
            fila ("Correlació"_ca, num (m.correlacio, 2) + " / " + num (a.correlacio, 2), false);
            auto greusSide = [] (const dg::Analisi& x) { double mm = 0, ss = 0; for (int b = 0; b <= 6; ++b) { mm += x.mid[(size_t) b]; ss += x.side[(size_t) b]; } return mm > 0 ? (float) (10.0 * std::log10 (std::max (ss, 1e-20) / mm)) : -100.0f; };
            fila ("Greus als costats", pc (greusSide (m)) + " / " + pc (greusSide (a)), greusSide (m) > greusSide (a) + 6.0f);
        }

        void timerCallback() override
        {
            if (btEscRef.getToggleState() != (rk->escolta() == rack::EscoltaReferencia) || btEscMaster.getToggleState() != (rk->escolta() == rack::EscoltaMaster)) refresca();
            repaint (zonaInfo);
        }

        std::shared_ptr<rack::Rack> rk;
        std::shared_ptr<dg::Analisi> masterAn;
        std::unique_ptr<juce::FileChooser> chooser;
        std::unique_ptr<Pom> pomQuantitat;
        int quantitat = 50;
        juce::String avis;
        juce::Slider sliderInici;
        Boto btAfegeix { "Afegeix…"_ca, std::nullopt, Boto::Estil::Primari };
        Boto btTreu { "Treu", std::nullopt, Boto::Estil::Secundari };
        Boto btHabituals { "Habituals", std::nullopt, Boto::Estil::Secundari };
        Boto btDesaHabitual { "Desa com a habitual", std::nullopt, Boto::Estil::Secundari };
        Boto btInici { "Torna a l'inici"_ca, std::nullopt, Boto::Estil::Secundari };
        Boto btIguala { "Volum igualat", std::nullopt, Boto::Estil::Tecla };
        Boto btEscMaster { "Màster"_ca, std::nullopt, Boto::Estil::Tecla };
        Boto btEscRef { "Referència"_ca, std::nullopt, Boto::Estil::Tecla };
        Boto btCompara { "Compara amb el màster"_ca, std::nullopt, Boto::Estil::Primari };
        Boto btMatching { "Aplica el matching", std::nullopt, Boto::Estil::Primari };
        Boto btTreuMatching { "Treu el matching", std::nullopt, Boto::Estil::Secundari };
        juce::Rectangle<int> zonaTitol, zonaLlista, zonaInfo, zonaGrafic, zonaTaula, zonaMatching, zonaAvis;
    };

    class FinestraRef : public juce::DocumentWindow
    {
    public:
        FinestraRef (juce::Component* c, std::function<void()> t)
            : juce::DocumentWindow ("Mastering Studio · Referències"_ca, tema::fons, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton), tancar (std::move (t))
        {
            setUsingNativeTitleBar (true);
            setContentOwned (c, true);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::MessageManager::callAsync ([t = tancar] { t(); }); }
        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }
    private:
        std::function<void()> tancar;
    };
}

std::shared_ptr<const rack::Referencia> carregaReferencia (const juce::File& f, juce::String& error)
{
    const auto cami = f.getFullPathName().toStdString();
    if (auto r = memoria()[cami].lock()) return r;
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> lector (fm.createReaderFor (f));
    if (lector == nullptr) { error = "No s'ha pogut llegir «"_ca + f.getFileName() + "» (format no reconegut)."_ca; return nullptr; }
    const double fs = lector->sampleRate > 0 ? lector->sampleRate : 44100.0;
    const auto n = (int) juce::jmin<juce::int64> (lector->lengthInSamples, (juce::int64) (fs * 20.0 * 60.0));
    if (n < (int) fs) { error = "La cançó és massa curta."_ca; return nullptr; }
    juce::MouseCursor::showWaitCursor();
    juce::AudioBuffer<float> buf ((int) juce::jmax (2u, lector->numChannels), n);
    lector->read (&buf, 0, n, 0, true, true);
    auto ref = std::make_shared<rack::Referencia>();
    ref->nom = f.getFileNameWithoutExtension().toStdString();
    ref->cami = cami;
    ref->fs = fs;
    ref->L.assign (buf.getReadPointer (0), buf.getReadPointer (0) + n);
    ref->R.assign (lector->numChannels >= 2 ? buf.getReadPointer (1) : buf.getReadPointer (0), (lector->numChannels >= 2 ? buf.getReadPointer (1) : buf.getReadPointer (0)) + n);
    ref->analisi = dg::analitza (ref->L.data(), ref->R.data(), ref->L.size(), fs);
    juce::MouseCursor::hideWaitCursor();
    memoria()[cami] = ref;
    return ref;
}

bool carregaReferenciaActiva (rack::Rack& r)
{
    if (r.refActiva < 0 || r.refActiva >= (int) r.referencies.size()) return false;
    juce::String error;
    auto ref = carregaReferencia (juce::File (u8 (r.referencies[(size_t) r.refActiva].cami)), error);
    if (ref == nullptr) return false;
    r.posaReferencia (ref);
    return true;
}

std::unique_ptr<juce::DocumentWindow> creaFinestraReferencies (std::shared_ptr<rack::Rack> rk, std::function<void()> enTancar)
{
    return std::make_unique<FinestraRef> (new PanellReferencies (std::move (rk)), std::move (enTancar));
}
