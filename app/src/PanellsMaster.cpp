#include "PanellsMaster.h"

#include "Controls.h"
#include "Dibuix.h"
#include "EscGlobal.h"
#include "Pom.h"
#include "Tema.h"
#include "Text.h"
#include "Vintage.h"
#include <cmath>
#include <deque>
#include <mibase/Dinamica.h>
#include <mibase/Equalitzador.h>
#include <mibase/Limitador.h>
#include <mibase/Saturacio.h>

namespace
{
    using namespace mibase;
    namespace lo = mibase::loudness;
    const juce::Colour ambre { 0xffffb84a };   // el color del màster

    juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }
    juce::String num (float v, int d = 1) { return juce::String (v, d).replaceCharacter ('.', ','); }
    bool valid (float v) { return v > lo::silenci + 1.0f; }
    juce::String lufs (float v) { return valid (v) ? num (v) : juce::String ("-inf"); }
    juce::String signe (float v, int d = 1) { return (v > 0.0f ? "+" : "") + num (v, d); }

    /** El text de les mesures d'un Mesures (o «—» si no n'hi ha). */
    struct Fila { juce::String nom, ajuda; std::function<juce::String (const lo::Mesures&)> valor; };
    const std::vector<Fila>& files()
    {
        static const std::vector<Fila> f {
            { "Short-Term"_ca, "Volum percebut dels últims 3 segons (LUFS)."_ca, [] (const lo::Mesures& m) { return lufs (m.shortTerm.load()); } },
            { "Momentary"_ca, "Volum percebut dels últims 400 ms (LUFS)."_ca, [] (const lo::Mesures& m) { return lufs (m.momentary.load()); } },
            { "LRA"_ca, "Loudness Range: quant varia el volum al llarg de la cançó (LU). Poc = molt igual; molt = dinàmic."_ca, [] (const lo::Mesures& m) { return num (m.lra.load()) + " LU"; } },
            { "True Peak màx."_ca, "El pic més alt, també entre mostres (dBTP)."_ca, [] (const lo::Mesures& m) { return lufs (m.truePeakMax.load()); } },
            { "Pic màx."_ca, "La mostra més alta (dBFS)."_ca, [] (const lo::Mesures& m) { return lufs (m.samplePeakMax.load()); } },
            { "RMS"_ca, "Volum mitjà dels últims 3 s (dB)."_ca, [] (const lo::Mesures& m) { return lufs (m.rms.load()); } },
            { "Crest"_ca, "Diferència entre pics i volum mitjà (dB). Baix = aixafat; alt = amb cop."_ca, [] (const lo::Mesures& m) { return num (m.crest.load()) + " dB"; } },
            { "Correlació"_ca, "+1 = mono, 0 = molt ample, negatiu = problemes de fase en mono."_ca, [] (const lo::Mesures& m) { return num (m.correlacio.load(), 2); } },
            { "Clips"_ca, "Mostres a 0 dBFS o més."_ca, [] (const lo::Mesures& m) { return juce::String (m.clips.load()); } },
        };
        return f;
    }

    /** Historial del Short-Term (un punt cada 100 ms, 90 s). */
    struct Historial
    {
        std::deque<float> v;
        void afegeix (float x) { v.push_back (x); while (v.size() > 900) v.pop_front(); }
        void buida() { v.clear(); }
    };

    void dibuixaHistorial (juce::Graphics& g, juce::Rectangle<float> r, const Historial& in, const Historial* out, float objectiu)
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (r, 4.0f);
        const float mn = -36.0f, mx = -3.0f;
        auto y = [&] (float lu) { return r.getBottom() - (juce::jlimit (mn, mx, lu) - mn) / (mx - mn) * r.getHeight(); };
        g.setFont (dibuix::lletra (9.5f));
        for (float lu : { -30.0f, -20.0f, -10.0f })
        {
            g.setColour (tema::linia);
            g.drawHorizontalLine ((int) y (lu), r.getX(), r.getRight());
            g.setColour (tema::suau.withAlpha (0.7f));
            g.drawText (juce::String ((int) lu), r.withY (y (lu) - 11.0f).withHeight (10.0f).reduced (3.0f, 0.0f), juce::Justification::topLeft);
        }
        if (objectiu < -0.5f)
        {
            g.setColour (ambre.withAlpha (0.55f));
            const float yo = y (objectiu);
            for (float x = r.getX(); x < r.getRight(); x += 8.0f) g.drawLine (x, yo, juce::jmin (r.getRight(), x + 4.0f), yo, 1.0f);
        }
        auto traca = [&] (const Historial& h, juce::Colour c, float gruix)
        {
            if (h.v.size() < 2) return;
            juce::Path p;
            const float dx = r.getWidth() / 899.0f;
            float x = r.getRight() - dx * (float) (h.v.size() - 1);
            bool comencat = false;
            for (float lu : h.v)
            {
                if (valid (lu)) { if (! comencat) { p.startNewSubPath (x, y (lu)); comencat = true; } else p.lineTo (x, y (lu)); }
                else comencat = false;
                x += dx;
            }
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (gruix));
        };
        traca (in, tema::suau.withAlpha (0.55f), 1.0f);
        if (out != nullptr) traca (*out, ambre, 1.6f);
    }

    // ------------------------------------------------------------------
    /** El panell lateral del Mastering Studio. */
    class PanellMaster : public juce::Component, private juce::Timer
    {
    public:
        explicit PanellMaster (std::shared_ptr<rack::Rack> r) : rk (std::move (r))
        {
            for (auto* b : { &btOriginal, &btProcessat, &btIgualat }) { addAndMakeVisible (*b); b->setClickingTogglesState (false); }
            btOriginal.setTooltip ("Escolta el mix tal com entra (sense el Mastering Studio). Tecla ràpida: clica i torna."_ca);
            btProcessat.setTooltip ("Escolta el màster (amb tota la cadena)."_ca);
            btIgualat.setTooltip ("Iguala el volum entre l'original i el màster per comparar només el so: més fort no vol dir millor. Les mesures sempre són les del màster de debò."_ca);
            btOriginal.onClick = [this] { rk->posaOriginal (true); refresca(); };
            btProcessat.onClick = [this] { rk->posaOriginal (false); refresca(); };
            btIgualat.onClick = [this] { rk->posaIgualat (! rk->igualat()); refresca(); };
            addAndMakeVisible (cbObjectiu);
            int id = 1;
            for (const auto& o : lo::objectius())
                cbObjectiu.addItem (u8 (o.nom) + (o.lufs < -0.5f ? "  ·  "_ca + num (o.lufs, 0) + " LUFS" : juce::String()), id++);
            cbObjectiu.setTooltip ("Un objectiu orientatiu: depèn del material i del gènere. No és obligatori."_ca);
            cbObjectiu.onChange = [this] { rk->posaObjectiu (cbObjectiu.getSelectedId() - 1); refresca(); };
            addAndMakeVisible (btReinicia);
            btReinicia.setTooltip ("Torna a començar les mesures (Integrated, LRA, màxims). Després, reprodueix la cançó sencera."_ca);
            btReinicia.onClick = [this] { rk->reiniciaMesures(); hIn.buida(); hOut.buida(); };
            refresca();
            startTimerHz (10);
        }
        ~PanellMaster() override { stopTimer(); }

        void refresca()
        {
            btOriginal.setToggleState (rk->original(), juce::dontSendNotification);
            btProcessat.setToggleState (! rk->original(), juce::dontSendNotification);
            btIgualat.setToggleState (rk->igualat(), juce::dontSendNotification);
            cbObjectiu.setSelectedId (rk->objectiu() + 1, juce::dontSendNotification);
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            vintage::panell (g, getLocalBounds().toFloat(), 6.0f);
            const auto* in = rk->loudnessEntrada();
            const auto* out = rk->loudnessSortida();
            if (in == nullptr || out == nullptr) return;
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (10.5f, true));
            g.drawText ("A / B", zonaTitolAB, juce::Justification::centredLeft);
            g.drawText ("LOUDNESS DEL MÀSTER"_ca, zonaTitolL, juce::Justification::centredLeft);
            // compensació de l'A/B
            g.setFont (dibuix::lletra (11.0f));
            const float c = rk->compensacioDb();
            juce::String tc;
            if (! rk->igualat()) tc = "Sense igualar: el més fort sembla millor."_ca;
            else if (std::abs (c) < 0.1f) tc = "Igualat (encara mesurant…)"_ca;
            else tc = (c < 0 ? "Màster abaixat "_ca : "Original abaixat "_ca) + num (std::abs (c)) + " dB per comparar"_ca;
            g.setColour (rk->igualat() ? tema::textMig : tema::errorText);
            g.drawFittedText (tc, zonaCompensacio, juce::Justification::centredLeft, 1, 0.85f);
            // LCD: Integrated i objectiu
            const auto& obj = lo::objectius()[(size_t) juce::jlimit (0, (int) lo::objectius().size() - 1, rk->objectiu())];
            const float integ = out->integrated.load();
            juce::String petit = "Integrated · "_ca + juce::String (out->segons.load() / 60.0 >= 1.0 ? juce::String ((int) (out->segons.load() / 60.0)) + " min " : juce::String())
                                 + juce::String ((int) std::fmod (out->segons.load(), 60.0)) + " s";
            if (obj.lufs < -0.5f && valid (integ)) petit = signe (integ - obj.lufs) + " LU respecte de l'objectiu"_ca;
            vintage::lcd (g, zonaLcd.toFloat(), lufs (integ) + " LUFS", petit, "MÀSTER"_ca);
            // taula: mix | màster
            auto t = zonaTaula;
            g.setFont (dibuix::lletra (10.0f, true));
            g.setColour (tema::suau);
            {
                auto cap = t.removeFromTop (16);
                cap.removeFromLeft (cap.getWidth() / 2);
                g.drawText ("MIX", cap.removeFromLeft (cap.getWidth() / 2), juce::Justification::centredRight);
                g.setColour (ambre);
                g.drawText ("MÀSTER"_ca, cap, juce::Justification::centredRight);
            }
            const int alt = juce::jmax (14, t.getHeight() / (int) files().size());
            g.setFont (dibuix::lletra (11.5f));
            for (const auto& f : files())
            {
                auto fl = t.removeFromTop (alt);
                g.setColour (tema::suau);
                g.drawText (f.nom, fl.removeFromLeft (fl.getWidth() / 2), juce::Justification::centredLeft);
                g.setColour (tema::textMig);
                g.drawText (f.valor (*in), fl.removeFromLeft (fl.getWidth() / 2), juce::Justification::centredRight);
                g.setColour (tema::text);
                g.drawText (f.valor (*out), fl, juce::Justification::centredRight);
            }
            // historial
            dibuixaHistorial (g, zonaHistorial.toFloat(), hIn, &hOut, obj.lufs);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (9.5f));
            g.drawText ("Short-Term · gris: mix · ambre: màster"_ca, zonaHistorial.withHeight (12).translated (0, -13), juce::Justification::centredLeft);
            // lectura
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (10.5f, true));
            g.drawText ("LECTURA", zonaLectura.withHeight (14), juce::Justification::centredLeft);
            g.setFont (dibuix::lletra (11.0f));
            g.setColour (tema::textMig);
            g.drawFittedText (lectura (*in, *out, obj), zonaLectura.withTrimmedTop (16), juce::Justification::topLeft, 12, 0.9f);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (12, 10);
            zonaTitolAB = r.removeFromTop (16);
            {
                auto b = r.removeFromTop (30);
                const int a = (b.getWidth() - 8) / 3;
                btOriginal.setBounds (b.removeFromLeft (a));
                b.removeFromLeft (4);
                btProcessat.setBounds (b.removeFromLeft (a));
                b.removeFromLeft (4);
                btIgualat.setBounds (b);
            }
            zonaCompensacio = r.removeFromTop (20);
            r.removeFromTop (6);
            zonaTitolL = r.removeFromTop (16);
            zonaLcd = r.removeFromTop (54);
            r.removeFromTop (6);
            cbObjectiu.setBounds (r.removeFromTop (26));
            r.removeFromTop (6);
            btReinicia.setBounds (r.removeFromBottom (28));
            r.removeFromBottom (6);
            zonaTaula = r.removeFromTop (16 + 9 * 17);
            r.removeFromTop (18);
            zonaHistorial = r.removeFromTop (juce::jlimit (50, 110, r.getHeight() / 3));
            r.removeFromTop (8);
            zonaLectura = r;
        }

        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }

    private:
        /** No és un semàfor de «bo» o «dolent»: diu què veu i què pot voler dir, segons el context. */
        juce::String lectura (const lo::Mesures& in, const lo::Mesures& out, const lo::Objectiu& obj) const
        {
            if (out.segons.load() < 3.0) return "Reprodueix la cançó (millor sencera, des de «Reinicia») per tenir mesures."_ca;
            juce::StringArray a;
            const float tp = out.truePeakMax.load(), integ = out.integrated.load(), crest = out.crest.load();
            if (out.clips.load() > 0) a.add ("Hi ha mostres a 0 dBFS: el limitador ha de ser l'últim i amb sostre per sota de 0."_ca);
            if (valid (tp) && tp > obj.truePeak + 0.05f)
                a.add ("True peak de "_ca + num (tp) + " dBTP: per sobre de " + num (obj.truePeak) + " pot distorsionar en passar a MP3/AAC. Activa True Peak al limitador o baixa'n el sostre."_ca);
            if (obj.lufs < -0.5f && valid (integ) && out.segons.load() > 20.0)
            {
                const float d = integ - obj.lufs;
                if (d > 2.0f) a.add (num (d) + " LU més fort que l'objectiu: la plataforma l'abaixarà i només hauràs perdut dinàmica. Baixa el macro Loudness."_ca);
                else if (d < -3.0f) a.add (num (-d) + " LU més fluix que l'objectiu. Si la cançó és tranquil·la, pot ser el que vols; si no, puja el macro Loudness."_ca);
            }
            if (valid (out.rms.load()) && crest > 0.5f && crest < 6.0f)
                a.add ("Crest de "_ca + num (crest) + " dB: el so pot estar aixafat (els cops de la bateria perden força)."_ca);
            const float lraIn = in.lra.load(), lraOut = out.lra.load();
            if (out.segons.load() > 30.0 && lraIn > 3.0f && lraIn - lraOut > 3.0f)
                a.add ("El màster ha perdut "_ca + num (lraIn - lraOut) + " LU de rang (de " + num (lraIn) + " a " + num (lraOut) + "): les parts fluixes i fortes s'assemblen més. Si la cançó ha de créixer, comprimeix menys."_ca);
            const float corr = out.correlacio.load();
            if (corr < 0.0f) a.add ("Correlació negativa: en mono es perdran coses (sobretot el que és als costats). Revisa l'Estèreo."_ca);
            else if (corr < 0.2f && valid (out.shortTerm.load())) a.add ("Correlació molt baixa: molt ample; comprova-ho amb «Escolta en mono» a l'Estèreo."_ca);
            if (a.isEmpty()) a.add ("Cap avís. Ara, escolta: compara amb el volum igualat i amb una referència."_ca);
            return "- " + a.joinIntoString ("\n- ");
        }

        void timerCallback() override
        {
            if (const auto* in = rk->loudnessEntrada()) hIn.afegeix (in->shortTerm.load());
            if (const auto* out = rk->loudnessSortida()) hOut.afegeix (out->shortTerm.load());
            if (const auto* out = rk->loudnessSortida(); out != nullptr && out->segons.load() < 0.2) { hIn.buida(); hOut.buida(); }
            if (btOriginal.getToggleState() != rk->original() || btIgualat.getToggleState() != rk->igualat() || cbObjectiu.getSelectedId() != rk->objectiu() + 1) refresca();
            repaint();
        }

        std::shared_ptr<rack::Rack> rk;
        Boto btOriginal { "Original", std::nullopt, Boto::Estil::Tecla };
        Boto btProcessat { "Màster"_ca, std::nullopt, Boto::Estil::Tecla };
        Boto btIgualat { "Igualat", std::nullopt, Boto::Estil::Tecla };
        Boto btReinicia { "Reinicia les mesures"_ca, std::nullopt, Boto::Estil::Secundari };
        juce::ComboBox cbObjectiu;
        Historial hIn, hOut;
        juce::Rectangle<int> zonaTitolAB, zonaCompensacio, zonaTitolL, zonaLcd, zonaTaula, zonaHistorial, zonaLectura;
    };

    // ------------------------------------------------------------------
    /** La finestra de l'Estèreo: 11 poms, escolta en mono, correlació d'entrada i sortida i avisos. */
    class PanellEstereo : public juce::Component, private juce::Timer
    {
    public:
        explicit PanellEstereo (std::shared_ptr<estereo::Estereo> e) : fx (std::move (e))
        {
            using namespace mibase::estereo;
            for (int p = 0; p < numParams; ++p)
            {
                if (p == Mono) continue;
                const bool bi = p == Mid || p == Side || p == Balanc || p == Sortida;
                auto pom = std::make_unique<Pom> (u8 (info (p).nom), 0.0, 1.0, (double) Estereo::normal (p, info (p).def), bi,
                                                  [this, p] (double v) { return u8 (fx->textParametre (p, (float) v).c_str()); });
                pom->setTooltip (u8 (info (p).ajuda));
                pom->onCanvi = [this, p] (double v) { fx->posaParametre (p, (float) v); };
                pom->onReinicia = [this, p] { fx->posa (p, info (p).def); };
                addAndMakeVisible (*pom);
                poms.push_back ({ p, std::move (pom) });
            }
            addAndMakeVisible (btMono);
            btMono.setClickingTogglesState (false);
            btMono.setTooltip (u8 (info (Mono).ajuda));
            btMono.onClick = [this] { fx->posa (estereo::Mono, fx->valor (estereo::Mono) >= 0.5f ? 0.0f : 1.0f); };
            setSize (720, 420);
            startTimerHz (15);
        }
        ~PanellEstereo() override { stopTimer(); }
        void paint (juce::Graphics& g) override
        {
            g.fillAll (tema::fons);
            vintage::panell (g, getLocalBounds().toFloat().reduced (6.0f), 6.0f);
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (17.0f, true));
            g.drawText ("ESTÈREO"_ca, zonaTitol, juce::Justification::centredLeft);
            g.setColour (ambre);
            g.setFont (dibuix::lletra (10.0f, true));
            g.drawText ("MIBASE · STEREO IMAGER"_ca, zonaTitol.translated (0, 18), juce::Justification::centredLeft);
            const auto& ms = fx->mesures();
            auto barra = [&] (juce::Rectangle<int> z, const juce::String& nom, float c)
            {
                auto r = z.toFloat();
                g.setColour (tema::suau);
                g.setFont (dibuix::lletra (11.0f));
                g.drawText (nom, r.removeFromLeft (150.0f), juce::Justification::centredLeft);
                auto txt = r.removeFromRight (50.0f);
                g.setColour (juce::Colours::black.withAlpha (0.5f));
                g.fillRoundedRectangle (r.reduced (0.0f, 5.0f), 3.0f);
                const float mig = r.getCentreX(), x = mig + c * r.getWidth() * 0.5f;
                g.setColour (c < 0.0f ? vintage::ledVermell : c < 0.3f ? vintage::ledGroc : vintage::ledVerd);
                g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (mig, x), r.getY() + 7.0f, juce::jmax (mig, x), r.getBottom() - 7.0f));
                g.setColour (tema::linia2);
                g.drawVerticalLine ((int) mig, r.getY() + 4.0f, r.getBottom() - 4.0f);
                g.setColour (tema::text);
                g.drawText (num (c, 2), txt, juce::Justification::centredRight);
            };
            barra (zonaCorrIn, "Correlació entrada"_ca, ms.correlacioIn.load());
            barra (zonaCorrOut, "Correlació sortida"_ca, ms.correlacioOut.load());
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (11.0f));
            const float smIn = ms.sideMidIn.load(), smOut = ms.sideMidOut.load();
            g.drawText ("Side respecte del Mid: entrada "_ca + (smIn > -99.0f ? num (smIn) + " dB" : juce::String ("-inf"))
                        + " · sortida "_ca + (smOut > -99.0f ? num (smOut) + " dB" : juce::String ("-inf")), zonaSide, juce::Justification::centredLeft);
            // avís: l'amplada puja però la correlació baixa
            juce::String avis;
            if (ms.correlacioOut.load() < 0.0f) avis = "Correlació negativa: en mono es perdrà part del so. Tanca l'amplada o puja el Mono als greus."_ca;
            else if (smOut > smIn + 3.0f && ms.correlacioOut.load() < 0.3f) avis = "L'amplada puja però la correlació baixa molt: comprova-ho amb «Escolta en mono»."_ca;
            else if (fx->valor (estereo::AmpGreus) * fx->valor (estereo::Amplada) / 100.0f > 110.0f)
                avis = "Obrir els greus perjudica la compatibilitat mono i el control del baix. Als greus, millor tancar."_ca;
            g.setColour (avis.isEmpty() ? tema::suau : vintage::ledTaronja);
            g.drawFittedText (avis.isEmpty() ? "Consell: obre els aguts, deixa els greus al mig (Mono als greus 80–150 Hz)."_ca : avis, zonaAvis, juce::Justification::centredLeft, 2, 0.9f);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (20, 16);
            zonaTitol = r.removeFromTop (20);
            auto dalt = r.removeFromTop (20);
            btMono.setBounds (dalt.withLeft (dalt.getRight() - 150).withY (zonaTitol.getY()).withHeight (30));
            r.removeFromTop (8);
            for (int fila = 0; fila < 2; ++fila)
            {
                auto f = r.removeFromTop (110);
                const int n = fila == 0 ? 6 : 5;
                const int a = f.getWidth() / 6;
                for (int k = 0; k < n; ++k) poms[(size_t) (fila * 6 + k)].second->setBounds (f.removeFromLeft (a).reduced (4, 0));
            }
            r.removeFromTop (6);
            zonaCorrIn = r.removeFromTop (24);
            zonaCorrOut = r.removeFromTop (24);
            zonaSide = r.removeFromTop (22);
            zonaAvis = r;
        }
        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }
    private:
        void timerCallback() override
        {
            for (auto& [p, pom] : poms) pom->setValor (fx->parametre (p), true);
            btMono.setToggleState (fx->valor (estereo::Mono) >= 0.5f, juce::dontSendNotification);
            repaint();
        }
        std::shared_ptr<estereo::Estereo> fx;
        std::vector<std::pair<int, std::unique_ptr<Pom>>> poms;
        Boto btMono { "Escolta en mono", std::nullopt, Boto::Estil::Tecla };
        juce::Rectangle<int> zonaTitol, zonaCorrIn, zonaCorrOut, zonaSide, zonaAvis;
    };

    // ------------------------------------------------------------------
    /** La finestra del mòdul Loudness Meter (mesura el punt de la cadena on és). */
    class PanellLoudness : public juce::Component, private juce::Timer
    {
    public:
        explicit PanellLoudness (std::shared_ptr<lo::Modul> m) : mod (std::move (m))
        {
            addAndMakeVisible (btReinicia);
            btReinicia.onClick = [this] { mod->mesurador().reinicia(); h.buida(); };
            setSize (420, 500);
            startTimerHz (10);
        }
        ~PanellLoudness() override { stopTimer(); }
        void paint (juce::Graphics& g) override
        {
            g.fillAll (tema::fons);
            vintage::panell (g, getLocalBounds().toFloat().reduced (6.0f), 6.0f);
            const auto& m = mod->mesurador().mesures();
            vintage::lcd (g, zonaLcd.toFloat(), lufs (m.integrated.load()) + " LUFS", "Integrated · " + juce::String ((int) m.segons.load()) + " s", "LOUDNESS");
            auto t = zonaTaula;
            const int alt = t.getHeight() / (int) files().size();
            g.setFont (dibuix::lletra (12.0f));
            for (const auto& f : files())
            {
                auto fl = t.removeFromTop (alt);
                g.setColour (tema::suau);
                g.drawText (f.nom, fl.removeFromLeft (fl.getWidth() / 2), juce::Justification::centredLeft);
                g.setColour (tema::text);
                g.drawText (f.valor (m), fl, juce::Justification::centredRight);
            }
            dibuixaHistorial (g, zonaHistorial.toFloat(), h, nullptr, 0.0f);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (20, 18);
            zonaLcd = r.removeFromTop (56);
            r.removeFromTop (10);
            btReinicia.setBounds (r.removeFromBottom (28));
            r.removeFromBottom (8);
            zonaTaula = r.removeFromTop (9 * 22);
            r.removeFromTop (10);
            zonaHistorial = r;
        }
        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }
    private:
        void timerCallback() override { h.afegeix (mod->mesurador().mesures().shortTerm.load()); repaint(); }
        std::shared_ptr<lo::Modul> mod;
        Historial h;
        Boto btReinicia { "Reinicia les mesures"_ca, std::nullopt, Boto::Estil::Secundari };
        juce::Rectangle<int> zonaLcd, zonaTaula, zonaHistorial;
    };
}

juce::Component* creaPanellMaster (std::shared_ptr<mibase::rack::Rack> r) { return new PanellMaster (std::move (r)); }
juce::Component* creaPanellEstereo (std::shared_ptr<mibase::estereo::Estereo> e) { return new PanellEstereo (std::move (e)); }
juce::Component* creaPanellLoudness (std::shared_ptr<mibase::loudness::Modul> m) { return new PanellLoudness (std::move (m)); }

juce::String activitatModul (const mibase::rack::Modul& m, float& quant)
{
    quant = 0.0f;
    if (! m.on.load()) return {};
    if (auto* d = dynamic_cast<const mibase::dinamica::Dinamica*> (m.fx.get()))
    {
        const float gr = d->mesures().reduccio.load();
        quant = juce::jlimit (0.0f, 1.0f, gr / 12.0f);
        return gr > 0.05f ? "-"_ca + num (gr) + " dB GR" : juce::String ("0 dB GR");
    }
    if (auto* l = dynamic_cast<mibase::limitador::Limitador*> (m.fx.get()))
    {
        const float gr = l->mesures().reduccio.load();
        quant = juce::jlimit (0.0f, 1.0f, gr / 12.0f);
        return gr > 0.05f ? "-"_ca + num (gr) + " dB GR" : juce::String ("0 dB GR");
    }
    if (dynamic_cast<const mibase::loudness::Modul*> (m.fx.get()) != nullptr) return {};
    const float c = m.canviDb.load();
    quant = juce::jlimit (0.0f, 1.0f, std::abs (c) / 6.0f);
    return std::abs (c) < 0.05f ? juce::String ("0 dB") : signe (c) + " dB";
}

juce::String avisModul (const mibase::rack::Modul& m, bool master)
{
    if (! m.on.load()) return {};
    if (auto* l = dynamic_cast<mibase::limitador::Limitador*> (m.fx.get()))
    {
        const float gr = l->mesures().reduccio.load();
        if (gr > 6.0f) return "El limitador treballa massa (-"_ca + num (gr) + " dB): perds cop. Reparteix la feina amb el compressor o baixa el Loudness."_ca;
        return {};
    }
    if (auto* d = dynamic_cast<const mibase::dinamica::Dinamica*> (m.fx.get()))
    {
        using namespace mibase::dinamica;
        if ((int) d->valor (ModeP) != Compressor) return {};
        const float gr = d->mesures().reduccio.load();
        if (master && gr > 5.0f) return "Massa compressió per a un màster (-"_ca + num (gr) + " dB). Puja el llindar o baixa el ratio: 1–3 dB sol ser prou."_ca;
        if (master && d->valor (Atac) < 3.0f) return "Atac molt ràpid: es menja els cops de la bateria. En un màster, 10–50 ms."_ca;
        return {};
    }
    if (auto* q = dynamic_cast<const mibase::eq::Equalitzador*> (m.fx.get()))
    {
        float maxim = 0.0f;
        for (int k = 0; k < mibase::eq::maxBandes; ++k)
        {
            const auto b = q->banda (k);
            if (b.usada && b.activa && mibase::eq::teGuany (b.tipus)) maxim = juce::jmax (maxim, b.guany);
        }
        if (master && maxim > 6.0f) return "Un realç de "_ca + num (maxim) + " dB és molt per a un màster: potser és un problema de la mescla (o prova un tall en una altra zona)."_ca;
        return {};
    }
    if (auto* s = dynamic_cast<const mibase::saturacio::Saturacio*> (m.fx.get()))
    {
        if (master && s->valor (mibase::saturacio::Drive) > 6.0f && m.mix.load() > 0.5f) return "Molta saturació per a un màster: vigila els greus i la duresa."_ca;
        return {};
    }
    if (auto* e = dynamic_cast<const mibase::estereo::Estereo*> (m.fx.get()))
    {
        if (e->mesures().correlacioOut.load() < 0.0f) return "Correlació negativa: problemes en mono."_ca;
        if (e->valor (mibase::estereo::AmpGreus) * e->valor (mibase::estereo::Amplada) / 100.0f > 110.0f) return "Greus oberts: perjudica el mono."_ca;
        return {};
    }
    return {};
}
