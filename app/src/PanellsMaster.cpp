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
#include <mibase/Clipper.h>
#include <mibase/Dinamica.h>
#include <mibase/LowEnd.h>
#include <mibase/Multibanda.h>
#include <mibase/Transitoris.h>
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
            { "Short-Term màx."_ca, "El Short-Term més alt des que has reiniciat (LUFS): la part més forta."_ca, [] (const lo::Mesures& m) { return lufs (m.shortMax.load()); } },
            { "Momentary màx."_ca, "El Momentary més alt des que has reiniciat (LUFS)."_ca, [] (const lo::Mesures& m) { return lufs (m.momentaryMax.load()); } },
            { "LRA"_ca, "Loudness Range: quant varia el volum al llarg de la cançó (LU). Poc = molt igual; molt = dinàmic."_ca, [] (const lo::Mesures& m) { return num (m.lra.load()) + " LU"; } },
            { "True Peak màx."_ca, "El pic més alt, també entre mostres (dBTP)."_ca, [] (const lo::Mesures& m) { return lufs (m.truePeakMax.load()); } },
            { "Pic màx."_ca, "La mostra més alta (dBFS)."_ca, [] (const lo::Mesures& m) { return lufs (m.samplePeakMax.load()); } },
            { "RMS"_ca, "Volum mitjà dels últims 3 s (dB)."_ca, [] (const lo::Mesures& m) { return lufs (m.rms.load()); } },
            { "Crest"_ca, "Diferència entre pics i volum mitjà (dB). Baix = aixafat; alt = amb cop."_ca, [] (const lo::Mesures& m) { return num (m.crest.load()) + " dB"; } },
            { "Correlació"_ca, "+1 = mono, 0 = molt ample, negatiu = problemes de fase en mono."_ca, [] (const lo::Mesures& m) { return num (m.correlacio.load(), 2); } },
            { "Amplada"_ca, "Side respecte del Mid: 0 % = mono; 20–50 % és habitual; molt més, molt ample (vigila el mono)."_ca,
              [] (const lo::Mesures& m) { const float d = m.sideMid.load(); return d > -99.0f ? juce::String (juce::roundToInt (100.0f * std::pow (10.0f, d / 20.0f))) + " %" : juce::String ("0 %"); } },
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
                cbObjectiu.addItem (u8 (o.nom) + (o.lufs > -0.5f ? juce::String() : o.configurable ? "  ·  tu tries els LUFS · "_ca + num (o.truePeak) + " dBTP"
                                                   : "  ·  "_ca + num (o.lufs, 0) + " LUFS · " + num (o.truePeak) + " dBTP"), id++);
            cbObjectiu.setTooltip ("Un objectiu orientatiu: depèn del material i del gènere. No és obligatori."_ca);
            cbObjectiu.onChange = [this]
            {
                rk->posaObjectiu (cbObjectiu.getSelectedId() - 1);
                if (rk->lufsObjectiu() < -0.5f) rack::posaSostre (*rk, rk->truePeakObjectiu());   // 0.95: el sostre del limitador, el del perfil
                refresca();
            };
            addAndMakeVisible (btAjusta);   // 0.95
            btAjusta.setTooltip ("Ajusta el limitador perquè el màster arribi als LUFS i al true peak del perfil. Mentre ho fa, reprodueix la cançó o la tornada en bucle."_ca);
            btAjusta.onClick = [this]
            {
                if (assistent.actiu()) { assistent.atura(); repaint(); return; }
                const float l = rk->lufsObjectiu();
                if (l > -0.5f) { avisAssistent = "Tria primer un perfil (no «Cap»)."_ca; repaint(); return; }
                avisAssistent.clear();
                assistent.comenca (*rk, l, rk->truePeakObjectiu());
                hIn.buida(); hOut.buida();
                repaint();
            };
            pomLufs = std::make_unique<Pom> ("LUFS", 0.0, 1.0, 0.5, false, [] (double v) { return num ((float) (-24.0 + 19.0 * v)) + " LUFS"; });
            pomLufs->setTooltip ("Els LUFS d'aquest perfil (el tries tu)."_ca);
            pomLufs->onCanvi = [this] (double v) { rk->posaLufsPersonal ((float) (-24.0 + 19.0 * v)); };
            addChildComponent (*pomLufs);
            addAndMakeVisible (btReinicia);
            btReinicia.setTooltip ("Torna a començar les mesures (Integrated, LRA, màxims). Després, reprodueix la cançó sencera."_ca);
            btReinicia.onClick = [this] { assistent.atura(); rk->reiniciaMesures(); hIn.buida(); hOut.buida(); };
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
            const auto& o = lo::objectius()[(size_t) juce::jlimit (0, (int) lo::objectius().size() - 1, rk->objectiu())];
            pomLufs->setVisible (o.configurable);
            pomLufs->setValor ((rk->lufsPersonal() + 24.0) / 19.0, true);
            resized();
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
            if (! rk->igualat()) tc = "Igualat apagat: encén-lo per comparar només el so."_ca;
            else if (std::abs (c) < 0.1f) tc = "Igualat (encara mesurant…)"_ca;
            else tc = (c < 0 ? "Màster abaixat "_ca : "Original abaixat "_ca) + num (std::abs (c)) + " dB per comparar"_ca;
            g.setColour (rk->igualat() ? tema::textMig : tema::suau);
            g.drawFittedText (tc, zonaCompensacio, juce::Justification::centredLeft, 1, 0.85f);
            // LCD: Integrated i objectiu
            const float integ = out->integrated.load();
            const float objL = rk->lufsObjectiu();
            juce::String petit = "Integrated · "_ca + juce::String (out->segons.load() / 60.0 >= 1.0 ? juce::String ((int) (out->segons.load() / 60.0)) + " min " : juce::String())
                                 + juce::String ((int) std::fmod (out->segons.load(), 60.0)) + " s";
            if (objL < -0.5f && valid (integ))   // 0.96: més clar (abans semblava que la distància fos el -10)
            {
                const float d = integ - objL;
                petit = "Objectiu "_ca + num (objL, objL == std::round (objL) ? 0 : 1) + " LUFS · "
                        + (std::abs (d) < 0.05f ? juce::String ("just al punt") : num (std::abs (d)) + (d > 0 ? " per sobre"_ca : " per sota"_ca));
            }
            vintage::lcd (g, zonaLcd.toFloat(), lufs (integ) + " LUFS", petit);
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
            dibuixaHistorial (g, zonaHistorial.toFloat(), hIn, &hOut, objL);
            // 0.95: l'assistent
            g.setFont (dibuix::lletra (11.0f));
            g.setColour (assistent.estat == rack::AssistentLoudness::Estat::Fet ? vintage::ledVerd
                         : assistent.estat == rack::AssistentLoudness::Estat::Escoltant ? ambre
                         : assistent.estat == rack::AssistentLoudness::Estat::Apagat ? tema::suau : vintage::ledTaronja);
            g.drawFittedText (textAssistent (*out), zonaAssistent, juce::Justification::topLeft, 3, 0.85f);
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (9.5f));
            g.drawText ("Short-Term · gris: mix · ambre: màster"_ca, zonaHistorial.withHeight (12).translated (0, -13), juce::Justification::centredLeft);
            // lectura
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (10.5f, true));
            g.drawText ("LECTURA", zonaLectura.withHeight (14), juce::Justification::centredLeft);
            g.setFont (dibuix::lletra (11.0f));
            g.setColour (tema::textMig);
            g.drawFittedText (lectura (*in, *out), zonaLectura.withTrimmedTop (16), juce::Justification::topLeft, 12, 0.9f);
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
            {
                auto fl = r.removeFromTop (pomLufs != nullptr && pomLufs->isVisible() ? 64 : 26);
                if (pomLufs != nullptr && pomLufs->isVisible()) pomLufs->setBounds (fl.removeFromRight (70));
                cbObjectiu.setBounds (fl.withSizeKeepingCentre (fl.getWidth(), 26));
            }
            r.removeFromTop (6);
            btAjusta.setBounds (r.removeFromTop (30));
            zonaAssistent = r.removeFromTop (44).withTrimmedTop (4);
            r.removeFromTop (4);
            btReinicia.setBounds (r.removeFromBottom (28));
            r.removeFromBottom (6);
            zonaTaula = r.removeFromTop (16 + (int) files().size() * 16);
            r.removeFromTop (18);
            zonaHistorial = r.removeFromTop (juce::jlimit (40, 100, r.getHeight() / 3));
            r.removeFromTop (8);
            zonaLectura = r;
        }

        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }

    private:
        /** No és un semàfor de «bo» o «dolent»: diu què veu i què pot voler dir, segons el context. */
        juce::String lectura (const lo::Mesures& in, const lo::Mesures& out) const
        {
            const float objLufs = rk->lufsObjectiu(), objTp = rk->truePeakObjectiu();
            if (out.segons.load() < 3.0) return "Reprodueix la cançó (millor sencera, des de «Reinicia») per tenir mesures."_ca;
            juce::StringArray a;
            const float tp = out.truePeakMax.load(), integ = out.integrated.load(), crest = out.crest.load();
            if (out.clips.load() > 0) a.add ("Hi ha mostres a 0 dBFS: el limitador ha de ser l'últim i amb sostre per sota de 0."_ca);
            if (valid (tp) && tp > objTp + 0.05f)
                a.add ("True peak de "_ca + num (tp) + " dBTP: per sobre de " + num (objTp) + " pot distorsionar en passar a MP3/AAC. Activa True Peak al limitador o baixa'n el sostre."_ca);
            if (valid (out.samplePeakMax.load()) && out.samplePeakMax.load() > -0.01f) a.add ("El pic arriba a 0 dBFS: no ha de passar mai."_ca);
            if (out.sideMid.load() > -3.0f && valid (out.shortTerm.load())) a.add ("Molt ample (el Side gairebé tan fort com el Mid): comprova el mono."_ca);
            if (objLufs < -0.5f && valid (integ) && out.segons.load() > 20.0)
            {
                const float d = integ - objLufs;
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

        juce::String textAssistent (const lo::Mesures& out) const
        {
            using E = rack::AssistentLoudness::Estat;
            const auto& a = assistent;
            if (avisAssistent.isNotEmpty()) return avisAssistent;
            switch (a.estat)
            {
                case E::Apagat: return "Tria un perfil i prem «Ajusta al perfil» mentre sona la cançó."_ca;
                case E::Escoltant:
                    if (! valid (out.shortTerm.load())) return "Esperant música: reprodueix la cançó (o la tornada en bucle)."_ca;
                    return "Escoltant (volta "_ca + juce::String (a.iteracions + 1) + "): " + juce::String (juce::jmin (a.segonsEscolta, out.segons.load()), 0)
                           + " de " + juce::String (a.segonsEscolta, 0) + " s. Deixa-ho sonar."_ca;
                case E::Fet: return "Fet: "_ca + num (a.mesurat) + " LUFS (objectiu " + num (a.objectiu) + "), sostre " + num (a.sostre) + " dBTP. El limitador redueix fins a "_ca
                                    + num (a.grMax) + " dB. Escolta-ho; si perd cop, prova un perfil més fluix."_ca;
                case E::Massa: return "M'he aturat a "_ca + num (a.mesurat) + " LUFS: per arribar a " + num (a.objectiu)
                                      + " el limitador hauria de reduir més de 8 dB i perdries el cop. Prova un perfil més fluix o puja Densitat."_ca;
                case E::NoArriba: return "El limitador ja és al màxim i no hi arriba: la mescla és molt fluixa. Puja-la abans del màster."_ca;
                case E::SenseLimitador: return "Cal un Limitador al final de la cadena (Afegeix un mòdul > Limitador)."_ca;
            }
            return {};
        }

        void timerCallback() override
        {
            if (assistent.actiu())   // 0.95
            {
                assistent.actualitza (*rk);
                btAjusta.setButtonText (assistent.actiu() ? "Atura l'ajust"_ca : "Ajusta al perfil"_ca);
            }
            else if (btAjusta.getButtonText() != "Ajusta al perfil"_ca) btAjusta.setButtonText ("Ajusta al perfil"_ca);
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
        Boto btAjusta { "Ajusta al perfil"_ca, std::nullopt, Boto::Estil::Primari };   // 0.95
        std::unique_ptr<Pom> pomLufs;
        rack::AssistentLoudness assistent;
        juce::String avisAssistent;
        juce::ComboBox cbObjectiu;
        Historial hIn, hOut;
        juce::Rectangle<int> zonaTitolAB, zonaCompensacio, zonaTitolL, zonaLcd, zonaTaula, zonaHistorial, zonaLectura, zonaAssistent;
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
            vintage::lcd (g, zonaLcd.toFloat(), lufs (m.integrated.load()) + " LUFS", "Integrated · " + juce::String ((int) m.segons.load()) + " s");
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

namespace
{
    /** 0.96: una finestra senzilla per a un mòdul de màster: una fila de poms, una barra de quant actua i un consell. */
    class PanellModulSimple : public juce::Component, private juce::Timer
    {
    public:
        struct Def
        {
            juce::String titol, subtitol;
            std::function<juce::String (int)> ajuda;
            std::function<float (int)> perDefecte;      // en unitats
            std::function<bool (int)> bipolar;
            std::function<juce::String (float& quant)> mesura;   // text i 0..1
            std::function<juce::String()> consell;
            std::function<void (int, float)> posa;      // en unitats (per al doble clic)
        };
        PanellModulSimple (std::shared_ptr<mibase::EfecteIntern> e, Def d) : fx (std::move (e)), def (std::move (d))
        {
            for (int p = 0; p < fx->numParametres(); ++p)
            {
                auto pom = std::make_unique<Pom> (juce::String::fromUTF8 (fx->infoParametre (p).nom.c_str()), 0.0, 1.0, (double) fx->parametre (p), def.bipolar (p),
                                                  [this, p] (double v) { return juce::String::fromUTF8 (fx->textParametre (p, (float) v).c_str()); });
                pom->setTooltip (def.ajuda (p));
                pom->onCanvi = [this, p] (double v) { fx->posaParametre (p, (float) v); };
                pom->onReinicia = [this, p] { def.posa (p, def.perDefecte (p)); };
                addAndMakeVisible (*pom);
                poms.push_back (std::move (pom));
            }
            setSize (juce::jmax (520, 96 * (int) poms.size() + 40), 300);
            startTimerHz (20);
        }
        ~PanellModulSimple() override { stopTimer(); }
        void paint (juce::Graphics& g) override
        {
            g.fillAll (tema::fons);
            vintage::panell (g, getLocalBounds().toFloat().reduced (6.0f), 6.0f);
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (17.0f, true));
            g.drawText (def.titol, zonaTitol, juce::Justification::centredLeft);
            g.setColour (ambre);
            g.setFont (dibuix::lletra (10.0f, true));
            g.drawText (def.subtitol, zonaTitol.translated (0, 18), juce::Justification::centredLeft);
            float quant = 0.0f;
            const auto txt = def.mesura (quant);
            auto b = zonaBarra.toFloat();
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (11.5f));
            g.drawText (txt, b.removeFromRight (190.0f), juce::Justification::centredRight);
            b = b.withSizeKeepingCentre (b.getWidth() - 10.0f, 8.0f);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.fillRoundedRectangle (b, 3.0f);
            g.setColour (quant > 0.6f ? vintage::ledTaronja : ambre);
            g.fillRoundedRectangle (b.withWidth (b.getWidth() * juce::jlimit (0.0f, 1.0f, quant)), 3.0f);
            const auto c = def.consell();
            g.setColour (c.startsWith ("!") ? vintage::ledTaronja : tema::suau);
            g.setFont (dibuix::lletra (11.5f));
            g.drawFittedText (c.startsWith ("!") ? c.substring (1) : c, zonaConsell, juce::Justification::topLeft, 3, 0.9f);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (20, 16);
            zonaTitol = r.removeFromTop (20);
            r.removeFromTop (26);
            auto f = r.removeFromTop (120);
            const int a = f.getWidth() / juce::jmax (1, (int) poms.size());
            for (auto& p : poms) p->setBounds (f.removeFromLeft (a).reduced (4, 0));
            r.removeFromTop (10);
            zonaBarra = r.removeFromTop (24);
            r.removeFromTop (8);
            zonaConsell = r;
        }
        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }
    private:
        void timerCallback() override { for (int p = 0; p < (int) poms.size(); ++p) poms[(size_t) p]->setValor (fx->parametre (p), true); repaint(); }
        std::shared_ptr<mibase::EfecteIntern> fx;
        Def def;
        std::vector<std::unique_ptr<Pom>> poms;
        juce::Rectangle<int> zonaTitol, zonaBarra, zonaConsell;
    };
}

juce::Component* creaPanellClipper (std::shared_ptr<mibase::EfecteIntern> e)
{
    namespace cl = mibase::clipper;
    auto* c = dynamic_cast<cl::Clipper*> (e.get());
    if (c == nullptr) return nullptr;
    PanellModulSimple::Def d;
    d.titol = "CLIPPER";
    d.subtitol = "MIBASE · ABANS DEL LIMITADOR"_ca;
    d.ajuda = [] (int p) { return u8 (cl::info (p).ajuda); };
    d.perDefecte = [] (int p) { return cl::info (p).def; };
    d.bipolar = [] (int p) { return p == cl::Sortida; };
    d.posa = [c] (int p, float v) { c->posa (p, v); };
    d.mesura = [c] (float& q)
    {
        const float r = c->mesures().retallMax.exchange (0.0f) * 0.5f + c->mesures().retall.load() * 0.5f;
        q = juce::jlimit (0.0f, 1.0f, r / 6.0f);
        return "Retalla " + num (r) + " dB · " + num (c->mesures().percent.load()) + " % de mostres";
    };
    d.consell = [c]
    {
        const float r = c->mesures().retall.load();
        if (r > 3.0f) return "!Retalla més de 3 dB: pot sonar aspre o distorsionat. Baixa el Drive o deixa que el limitador faci una part de la feina."_ca;
        if (c->valor (cl::Oversampling) < 0.5f) return "Sense oversampling, els aguts poden quedar més aspres. Encén-lo si et sobra CPU."_ca;
        return "Consell: retallar 1–2 dB només als cops més forts (caixa, bombo) deixa més volum al limitador sense que bombi."_ca;
    };
    return new PanellModulSimple (std::move (e), std::move (d));
}

juce::Component* creaPanellTransitoris (std::shared_ptr<mibase::EfecteIntern> e)
{
    namespace tr = mibase::transitoris;
    auto* t = dynamic_cast<tr::Transitoris*> (e.get());
    if (t == nullptr) return nullptr;
    PanellModulSimple::Def d;
    d.titol = "TRANSIENT SHAPER";
    d.subtitol = "MIBASE · COP I CUA"_ca;
    d.ajuda = [] (int p) { return u8 (tr::info (p).ajuda); };
    d.perDefecte = [] (int p) { return tr::info (p).def; };
    d.bipolar = [] (int p) { return p == tr::Atac || p == tr::Sustain || p == tr::Sortida; };
    d.posa = [t] (int p, float v) { t->posa (p, v); };
    d.mesura = [t] (float& q)
    {
        const float g = t->mesures().guanyMax.exchange (0.0f);
        q = juce::jlimit (0.0f, 1.0f, std::abs (g) / 10.0f);
        return "Canvi de volum ara: " + signe (g) + " dB";
    };
    d.consell = [t]
    {
        if (std::abs (t->valor (tr::Atac)) > 60.0f) return "!Amb molt d'Atac, al màster tot el que té cop salta (també les púes i les consonants). Comprova-ho amb el volum igualat."_ca;
        return "Consell: un Atac de +15 a +30 % abans del limitador ajuda que la bateria no perdi el cop. Amb Focus «Mitjos» escolta sobretot la caixa."_ca;
    };
    return new PanellModulSimple (std::move (e), std::move (d));
}

namespace
{
    /** 0.99: el Multibanda: a dalt els globals; a sota, una columna per banda amb els seus poms, Solo, Mute i la reducció. */
    class PanellMultibanda : public juce::Component, private juce::Timer
    {
    public:
        explicit PanellMultibanda (std::shared_ptr<mibase::EfecteIntern> e) : fx (std::move (e)), mb (dynamic_cast<mibase::multibanda::Multibanda*> (fx.get()))
        {
            using namespace mibase::multibanda;
            auto fesPom = [this] (int p, bool bi)
            {
                auto pom = std::make_unique<Pom> (u8 (info (p).nom), 0.0, 1.0, (double) mb->parametre (p), bi,
                                                  [this, p] (double v) { return u8 (mb->textParametre (p, (float) v).c_str()); });
                pom->setTooltip (u8 (info (p).ajuda));
                pom->onCanvi = [this, p] (double v) { mb->posaParametre (p, (float) v); };
                pom->onReinicia = [this, p] { mb->posa (p, info (p).def); };
                addAndMakeVisible (*pom);
                poms.push_back ({ p, std::move (pom) });
            };
            for (int p = 0; p < numGlobals; ++p) fesPom (p, p == Sortida);
            for (int b = 0; b < maxBandes; ++b)
            {
                for (int k : { Llindar, Ratio, Atac, Alliberament, Guany }) fesPom (index (b, k), k == Guany);
                for (int k : { Solo, Mute })
                {
                    auto bt = std::make_unique<Boto> (k == Solo ? "S" : "M", std::nullopt, Boto::Estil::Tecla);
                    bt->setTooltip (u8 (info (index (b, k)).ajuda));
                    const int p = index (b, k);
                    bt->onClick = [this, p] { mb->posa (p, mb->valor (p) >= 0.5f ? 0.0f : 1.0f); };
                    addAndMakeVisible (*bt);
                    botons.push_back ({ p, std::move (bt) });
                }
            }
            setSize (1180, 660);
            startTimerHz (20);
        }
        ~PanellMultibanda() override { stopTimer(); }
        void paint (juce::Graphics& g) override
        {
            using namespace mibase::multibanda;
            g.fillAll (tema::fons);
            vintage::panell (g, getLocalBounds().toFloat().reduced (6.0f), 6.0f);
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (17.0f, true));
            g.drawText ("MULTIBANDA", zonaTitol, juce::Justification::centredLeft);
            g.setColour (ambre);
            g.setFont (dibuix::lletra (10.0f, true));
            g.drawText ("MIBASE · COMPRESSOR PER BANDES"_ca, zonaTitol.translated (0, 18), juce::Justification::centredLeft);
            const int nb = mb->numBandes();
            for (int b = 0; b < maxBandes; ++b)
            {
                auto c = columnes[(size_t) b];
                const bool usada = b < nb;
                g.setColour (juce::Colours::black.withAlpha (usada ? 0.25f : 0.5f));
                g.fillRoundedRectangle (c.toFloat(), 6.0f);
                g.setColour (usada ? tema::text : tema::suau.withAlpha (0.5f));
                g.setFont (dibuix::lletra (12.0f, true));
                auto cap = c.removeFromTop (24);
                g.drawText (usada ? juce::String (nomBanda (b, nb)).toUpperCase() : "(sense fer servir)"_ca, cap, juce::Justification::centred);
                // reducció (barra vertical) a la dreta del títol
                if (usada)
                {
                    const float gr = mb->mesures().reduccio[(size_t) b].load();
                    auto r = zonesGr[(size_t) b].toFloat();
                    g.setColour (juce::Colours::black.withAlpha (0.5f));
                    g.fillRoundedRectangle (r, 3.0f);
                    g.setColour (gr > 6.0f ? vintage::ledTaronja : ambre);
                    g.fillRoundedRectangle (r.withWidth (r.getWidth() * juce::jlimit (0.0f, 1.0f, gr / 12.0f)), 3.0f);
                    g.setColour (tema::text);
                    g.setFont (dibuix::lletra (10.5f));
                    g.drawText ("-" + num (gr) + " dB", r.translated (0.0f, 14.0f), juce::Justification::centred);
                }
            }
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (11.0f));
            g.drawFittedText ("Consell: al màster, 1–3 dB de reducció a la banda que molesta és prou. Escolta la banda amb S (solo) per trobar el problema; si una sola banda redueix molt, potser és un problema de la mescla."_ca,
                              zonaConsell, juce::Justification::topLeft, 2, 0.9f);
        }
        void resized() override
        {
            using namespace mibase::multibanda;
            auto r = getLocalBounds().reduced (20, 16);
            zonaTitol = r.removeFromTop (20);
            r.removeFromTop (24);
            zonaConsell = r.removeFromBottom (34);
            auto dalt = r.removeFromTop (116);
            const int a = dalt.getWidth() / numGlobals;
            for (auto& [p, pom] : poms) if (p < numGlobals) pom->setBounds (dalt.removeFromLeft (a).reduced (4, 0));
            r.removeFromTop (10);
            const int amp = (r.getWidth() - 3 * 10) / maxBandes;
            for (int b = 0; b < maxBandes; ++b)
            {
                auto c = r.removeFromLeft (amp);
                r.removeFromLeft (10);
                columnes[(size_t) b] = c;
                auto in = c.reduced (6, 0);
                in.removeFromTop (26);
                zonesGr[(size_t) b] = in.removeFromTop (8);
                in.removeFromTop (22);
                auto fila = in.removeFromBottom (30).reduced (0, 2);
                for (auto& [p, bt] : botons)
                    if ((p - numGlobals) / paramsPerBanda == b) bt->setBounds ((((p - numGlobals) % paramsPerBanda) == Solo ? fila.removeFromLeft (fila.getWidth() / 2) : fila).reduced (2, 0));
                // els cinc poms: 3 a dalt, 2 a sota
                std::vector<Pom*> ps;
                for (auto& [p, pom] : poms) if (p >= numGlobals && (p - numGlobals) / paramsPerBanda == b) ps.push_back (pom.get());
                // tots de la mateixa mida: 3 a dalt i 2 a sota, centrats
                const int alt = in.getHeight() / 2, w = in.getWidth() / 3;
                auto f1 = in.removeFromTop (alt), f2 = in.withSizeKeepingCentre (2 * w, in.getHeight());
                for (int k = 0; k < (int) ps.size(); ++k)
                    ps[(size_t) k]->setBounds ((k < 3 ? f1.removeFromLeft (w) : f2.removeFromLeft (w)).reduced (2, 0));
            }
        }
        bool keyPressed (const juce::KeyPress& k) override { return cridaTeclaGlobal (k); }
    private:
        void timerCallback() override
        {
            using namespace mibase::multibanda;
            const int nb = mb->numBandes();
            for (auto& [p, pom] : poms)
            {
                pom->setValor (mb->parametre (p), true);
                if (p >= numGlobals) pom->setEnabled ((p - numGlobals) / paramsPerBanda < nb);
                if (p == X2) pom->setEnabled (nb >= 3);
                if (p == X3) pom->setEnabled (nb >= 4);
            }
            for (auto& [p, bt] : botons) { bt->setToggleState (mb->valor (p) >= 0.5f, juce::dontSendNotification); bt->setEnabled ((p - numGlobals) / paramsPerBanda < nb); }
            repaint();
        }
        std::shared_ptr<mibase::EfecteIntern> fx;
        mibase::multibanda::Multibanda* mb;
        std::vector<std::pair<int, std::unique_ptr<Pom>>> poms;
        std::vector<std::pair<int, std::unique_ptr<Boto>>> botons;
        std::array<juce::Rectangle<int>, mibase::multibanda::maxBandes> columnes, zonesGr;
        juce::Rectangle<int> zonaTitol, zonaConsell;
    };
}

juce::Component* creaPanellMultibanda (std::shared_ptr<mibase::EfecteIntern> e)
{
    if (dynamic_cast<mibase::multibanda::Multibanda*> (e.get()) == nullptr) return nullptr;
    return new PanellMultibanda (std::move (e));
}

juce::Component* creaPanellLowEnd (std::shared_ptr<mibase::EfecteIntern> e)
{
    namespace le = mibase::lowend;
    auto* g = dynamic_cast<le::LowEnd*> (e.get());
    if (g == nullptr) return nullptr;
    PanellModulSimple::Def d;
    d.titol = "LOW END FOCUS";
    d.subtitol = "MIBASE · ELS GREUS A PART"_ca;
    d.ajuda = [] (int p) { return u8 (le::info (p).ajuda); };
    d.perDefecte = [] (int p) { return le::info (p).def; };
    d.bipolar = [] (int p) { return p == le::Sub || p == le::Sortida; };
    d.posa = [g] (int p, float v) { g->posa (p, v); };
    d.mesura = [g] (float& q)
    {
        const float a = g->mesures().greusIn.load(), b = g->mesures().greusOut.load();
        q = juce::jlimit (0.0f, 1.0f, std::abs (b - a) / 6.0f);
        if (a < -100.0f) return juce::String ("Greus: sense senyal");
        return "Greus: "_ca + num (a) + " → " + num (b) + " dB (" + signe (b - a) + ")";
    };
    d.consell = [g]
    {
        if (g->valor (le::Sub) > 3.0f) return "!Molt de sub: als altaveus petits no se sent i gasta volum que el limitador haurà de treure."_ca;
        if (g->valor (le::Mono) < 10.0f) return "Consell: posa Mono a 100–150 Hz perquè el baix i el bombo quedin al mig i sonin bé en mono."_ca;
        return "Només hi ha el mix estèreo: Punch i Tight actuen sobre tots els greus (no poden separar el baix del bombo). Si el problema és d'un instrument, arregla'l a la seva pista."_ca;
    };
    return new PanellModulSimple (std::move (e), std::move (d));
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
    if (auto* c = dynamic_cast<mibase::clipper::Clipper*> (m.fx.get()))   // 0.96
    {
        const float r = c->mesures().retall.load();
        quant = juce::jlimit (0.0f, 1.0f, r / 6.0f);
        return r > 0.05f ? "-"_ca + num (r) + " dB retall" : juce::String ("0 dB retall");
    }
    if (auto* mb = dynamic_cast<mibase::multibanda::Multibanda*> (m.fx.get()))   // 0.99: la banda que més redueix
    {
        float gr = 0.0f;
        for (int b = 0; b < mb->numBandes(); ++b) gr = juce::jmax (gr, mb->mesures().reduccio[(size_t) b].load());
        quant = juce::jlimit (0.0f, 1.0f, gr / 12.0f);
        return gr > 0.05f ? "-"_ca + num (gr) + " dB GR" : juce::String ("0 dB GR");
    }
    if (auto* t = dynamic_cast<mibase::transitoris::Transitoris*> (m.fx.get()))
    {
        const float g = t->mesures().guany.load();
        quant = juce::jlimit (0.0f, 1.0f, std::abs (g) / 10.0f);
        return signe (g) + " dB";
    }
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
    if (auto* mb = dynamic_cast<mibase::multibanda::Multibanda*> (m.fx.get()))   // 0.99
    {
        for (int b = 0; b < mb->numBandes(); ++b)
            if (master && mb->mesures().reduccio[(size_t) b].load() > 6.0f)
                return "La banda «"_ca + u8 (mibase::multibanda::nomBanda (b, mb->numBandes())) + "» redueix més de 6 dB: potser és un problema de la mescla."_ca;
        return {};
    }
    if (auto* c = dynamic_cast<mibase::clipper::Clipper*> (m.fx.get()))   // 0.96
    {
        const float r = c->mesures().retall.load();
        if (master && r > 3.0f) return "El clipper retalla més de 3 dB: pot sonar aspre. Baixa el Drive."_ca;
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
