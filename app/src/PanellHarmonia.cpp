#include "PanellHarmonia.h"
#include "BancGM.h"

using namespace mibase;
namespace hm = mibase::harmonia;

namespace
{
    juce::String txt (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }

    struct Rellotge : juce::Timer
    {
        std::function<void()> tic;
        void timerCallback() override { if (tic) tic(); }
    };
}

//==============================================================================
class PanellHarmonia::Contingut : public juce::Component
{
public:
    Contingut (EstatEstudi& es, std::function<PianoRoll*()> ed, juce::PropertiesFile& aj, std::function<juce::Array<juce::MidiDeviceInfo>()> tec)
        : estat (es), editor (std::move (ed)), ajustos (aj), teclats (std::move (tec))
    {
        // ---- Pista ----
        btInstrument.onClick = [this] { menuInstrument(); };
        btInstrument.setTooltip ("Qui fa el so de la pista (els plugins VST3 arribaran a la versió següent)"_ca);
        btSo.onClick = [this] { menuSo(); };
        btSo.setTooltip ("So (preset) del sinte intern"_ca);
        btTeclat.onClick = [this] { menuTeclat(); };
        btTeclat.setTooltip ("Quin teclat MIDI toca aquesta pista. «Qualsevol»: el que toca la pista triada. "
                             "Un teclat concret: toca sempre aquesta pista, encara que no estigui triada."_ca);

        // ---- Tonalitat ----
        for (int i = 0; i < 12; ++i)
        {
            auto* b = arrels.add (new Boto (txt (hm::nomTo (i)), std::nullopt, Boto::Estil::Tecla));
            b->setLlum (vintage::ledTaronja);
            b->setMidaText (12.5f);
            b->setTooltip ("Tonalitat de " + txt (hm::nomTo (i)) + " (la tries a mà: deixa de ser automàtica)");
            b->onClick = [this, i] { canviaTonalitat ({ i, tonalitat().escala }, false); };
            addAndMakeVisible (b);
        }
        btEscala.onClick = [this] { menuEscala(); };
        btEscala.setTooltip ("Tipus d'escala"_ca);
        btAuto.setLlum (vintage::ledVerd);
        btAuto.setTooltip ("Automàtica: Mibase dedueix la tonalitat de totes les notes MIDI de la cançó (major o menor). "_ca
                           "Si tries una tonalitat a mà, deixa de ser automàtica."_ca);
        btAuto.onClick = [this] { canviaTonalitat (estat.projecte.tonalitat, ! estat.projecte.tonalitatAuto); };

        // ---- Mode ----
        mode.setLlum (vintage::ledTaronja);
        mode.onCanvi = [this] (int i)
        {
            estat.desfes.abans (estat.projecte);
            if (auto* t = pistaEditor()) t->modeEscala = i;   // 0.25: cada pista (editor) té el seu
            else estat.projecte.modeEscala = (ModeEscala) i;
            estat.info = i == 0 ? "Escala: només es ressalta"_ca
                       : i == 1 ? "Escala assistida: les notes de fora van a la més propera de l'escala"_ca
                                : "Escala bloquejada: no es poden posar notes de fora de l'escala"_ca;
            estat.canviat();
        };
        btAcord.setLlum (vintage::ledVerd);
        btAcord.setTooltip ("Pinta de verd, al Piano Roll, les notes de l'acord que sona al capçal (i en diu el nom a la regla)"_ca);
        btAcord.onClick = [this]
        {
            estat.projecte.ressaltaAcord = ! estat.projecte.ressaltaAcord;
            estat.canviat();
        };

        // ---- Acords de la tonalitat ----
        for (int i = 0; i < 7; ++i)
        {
            auto* b = graus.add (new BotoArrossegable ("-", std::nullopt, Boto::Estil::Secundari));
            b->setMidaText (12.5f);
            b->onClick = [this, i] { triaGrau (i); };
            b->onArrossega = [this, i, b] { arrossegaGrau (*b, i); };
            addAndMakeVisible (b);
        }
        setenes.onCanvi = [this] (int) { actualitza(); };
        durada.setTriat (0);
        btMinim.setLlum (vintage::ledGroc);
        btMinim.setToggleState (true, juce::dontSendNotification);
        btMinim.setTooltip ("Moviment mínim: cada acord nou es col·loca a prop de l'anterior (com ho faria un pianista)"_ca);
        btMinim.onClick = [this] { btMinim.setToggleState (! btMinim.getToggleState(), juce::dontSendNotification); };

        // ---- Constructor ----
        btFonamental.onClick = [this] { menuFonamental(); };
        btFonamental.setTooltip ("Nota fonamental de l'acord");
        btTipus.onClick = [this] { menuTipus(); };
        btTipus.setTooltip ("Tipus d'acord");
        inversio.setTriat (0);
        voicing.setTriat (0);
        voicing.onCanvi = [] (int) {};
        octava.setTooltip ("Octava de la fonamental (4 = la del do central)");
        btDobla.setLlum (vintage::ledTaronja);
        btDobla.setTooltip ("Dobla tot l'acord una octava amunt"_ca);
        btDobla.onClick = [this] { btDobla.setToggleState (! btDobla.getToggleState(), juce::dontSendNotification); };
        btBaix.setLlum (vintage::ledTaronja);
        btBaix.setTooltip ("Afegeix la fonamental una octava per sota (baix)");
        btBaix.onClick = [this] { btBaix.setToggleState (! btBaix.getToggleState(), juce::dontSendNotification); };
        btEscolta.onClick = [this] { if (auto* e = editor()) e->escoltaAcord (acordConstructor()); };
        btEscolta.setTooltip ("Escolta l'acord");
        btPosa.onClick = [this] { armaSegell (acordConstructor(), nomConstructor()); };
        btPosa.onArrossega = [this]
        {
            if (auto* e = editor()) e->escoltaAcord (acordConstructor());
            PianoRoll::arrossegaAcord (btPosa, estat, acordConstructor(), nomConstructor(), duradaAcord(), btMinim.getToggleState());
        };
        btPosa.setTooltip ("Arrossega'l fins al Piano Roll i deixa'l al temps que vulguis (o clica'l i després clica al Piano Roll)"_ca);
        btFesAcord.onClick = [this] { if (auto* e = editor()) e->fesAcords (tipus, false, inversio.getTriat(), (hm::Voicing) voicing.getTriat()); };
        btFesAcord.setTooltip ("Cada nota triada es converteix en un acord del tipus triat (ella és la fonamental)"_ca);
        btFesEscala.onClick = [this] { if (auto* e = editor()) e->fesAcords (0, true, inversio.getTriat(), (hm::Voicing) voicing.getTriat()); };
        btFesEscala.setTooltip ("Cada nota triada es converteix en l'acord que li toca a la tonalitat (A → Am a La menor)"_ca);
        btInvAmunt.onClick = [this] { if (auto* e = editor()) e->inverteixTriades (true); };
        btInvAmunt.setTooltip ("Inversió amunt: la nota de baix de cada acord triat puja una octava"_ca);
        btInvAvall.onClick = [this] { if (auto* e = editor()) e->inverteixTriades (false); };
        btInvAvall.setTooltip ("Inversió avall: la nota de dalt de cada acord triat baixa una octava"_ca);
        btEnllaca.onClick = [this] { if (auto* e = editor()) e->enllacaTriades(); };
        btEnllaca.setTooltip ("Enllaça els acords triats perquè es moguin el mínim d'un a l'altre"_ca);

        // ---- Progressió ----
        btProgressio.onClick = [this] { menuProgressio(); };
        btProgressio.setTooltip ("Progressió d'acords (amb els acords de la tonalitat)"_ca);
        compassos.setTriat (1);
        compassos.onCanvi = [this] (int) { actualitza(); };
        btEscoltaProg.setLlum (vintage::ledVerd);
        btEscoltaProg.onClick = [this] { escoltaProgressio (! btEscoltaProg.getToggleState()); };
        btEscoltaProg.setTooltip ("Escolta la progressió al tempo de la cançó"_ca);
        btInsereix.onClick = [this] { insereixProgressio(); };
        btInsereix.setTooltip ("Posa la progressió al clip de l'editor actiu, a partir del compàs del capçal (un acord per compàs)"_ca);

        for (auto* c : std::initializer_list<juce::Component*> { &btInstrument, &btSo, &btTeclat, &btEscala, &btAuto, &mode, &btAcord, &setenes, &durada, &btMinim,
                                                                 &btFonamental, &btTipus, &inversio, &voicing, &octava, &btDobla, &btBaix,
                                                                 &btEscolta, &btPosa, &btFesAcord, &btFesEscala, &btInvAmunt, &btInvAvall,
                                                                 &btEnllaca, &btProgressio, &compassos, &btEscoltaProg, &btInsereix, &btAplicaPads })
            addAndMakeVisible (c);

        reproductor.tic = [this] { pasProgressio(); };

        // Seccions (plegables clicant el títol).
        seccions[0] = { "Pista", "pista", { &btInstrument, &btSo, &btTeclat }, false, {} };
        seccions[1] = { "Tonalitat", "tonalitat", { &btEscala, &btAuto }, false, {} };
        for (auto* b : arrels) seccions[1].fills.push_back (b);
        seccions[2] = { "Escala al Piano Roll", "escala", { &mode }, false, {} };   // 0.12: sense «Ressalta l'acord» (sempre encès)
        btAcord.setVisible (false);
        seccions[3] = { "Acords de la tonalitat", "acords", { &setenes, &durada, &btMinim }, false, {} };
        for (auto* b : graus) seccions[3].fills.push_back (b);
        seccions[4] = { "Constructor d'acords", "constructor", { &btFonamental, &btTipus, &inversio, &voicing, &octava, &btDobla, &btBaix,
                                                                 &btEscolta, &btPosa, &btFesAcord, &btFesEscala, &btInvAvall, &btInvAmunt, &btEnllaca }, false, {} };
        seccions[5] = { juce::String::fromUTF8 ("Progressió"), "progressio", { &btProgressio, &compassos, &btEscoltaProg, &btInsereix, &btAplicaPads }, false, {} };
        btAplicaPads.setTooltip ("Posa els acords d'aquesta progressió als pads del Chord Pad (per ordre: C, C#, D…)"_ca);
        btAplicaPads.onClick = [this]
        {
            auto* pr = editor ? editor() : nullptr;
            if (pr == nullptr) return;
            std::vector<std::string> v;
            for (auto& a : hm::acordsProgressio (tonalitat(), progressio, setenes.getTriat() == 1))
                if (std::find (v.begin(), v.end(), a.nom) == v.end()) v.push_back (a.nom);
            pr->posaPads (v, txt (hm::progressions()[(size_t) progressio].nom).toStdString());
        };
        for (auto& sc : seccions) sc.plegada = ajustos.getBoolValue ("panellHarmonia.plegada." + sc.clau, false);
        actualitza();
    }

    std::function<void()> onMida;   // l'alçada ha canviat (s'ha plegat o desplegat una secció)
    std::function<void()> onObreSampler;   // 0.26

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (auto& sc : seccions)
            if (sc.rect.contains (e.getPosition()))
            {
                sc.plegada = ! sc.plegada;
                ajustos.setValue ("panellHarmonia.plegada." + sc.clau, sc.plegada);
                resized();
                if (onMida) onMida();
                repaint();
                return;
            }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        bool sobre = false;
        for (auto& sc : seccions) sobre = sobre || sc.rect.contains (e.getPosition());
        setMouseCursor (sobre ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }

    ~Contingut() override { escoltaProgressio (false); }

    /** Torna a llegir el projecte (tonalitat, mode…). */
    void actualitza()
    {
        const auto t = tonalitat();
        for (int i = 0; i < 12; ++i)
        {
            arrels[i]->setButtonText (txt (hm::nomTo (i, t)));
            arrels[i]->setToggleState (t.to == i, juce::dontSendNotification);
        }
        btEscala.setButtonText (txt (hm::escales()[(size_t) t.escala].nom));
        btAuto.setToggleState (estat.projecte.tonalitatAuto, juce::dontSendNotification);
        btAuto.setButtonText (estat.projecte.tonalitatAuto ? "Automàtica"_ca : "A mà (clica per detectar-la)"_ca);
        mode.setTriat ((int) (pistaEditor() != nullptr ? mibase::modeEscalaDe (estat.projecte, *pistaEditor()) : estat.projecte.modeEscala));
        btAcord.setToggleState (estat.projecte.ressaltaAcord, juce::dontSendNotification);

        const auto acords = hm::acordsDeTonalitat (t, setenes.getTriat() == 1);
        for (int i = 0; i < 7; ++i)
        {
            graus[i]->setButtonText (txt (acords[(size_t) i].nom));
            graus[i]->setTooltip (txt (acords[(size_t) i].roma) + ": clica per escoltar-lo i després clica al Piano Roll per posar-lo"_ca);
        }
        btFonamental.setButtonText (txt (hm::nomTo (fonamental, t)));
        btTipus.setButtonText (txt (hm::tipusAcords()[(size_t) tipus].nom));
        btProgressio.setButtonText (txt (hm::progressions()[(size_t) progressio].nom));

        if (auto* pe = pistaEditor())
        {
            if (mibase::esSampler (*pe))
            {
                btInstrument.setButtonText ("Instrument: samplejador"_ca);
                btSo.setButtonText ("Sample: "_ca + (pe->sampler.nom.empty() ? "cap"_ca : txt (pe->sampler.nom)));
            }
            else
            {
                btInstrument.setButtonText (pe->instrument.rfind ("vst3:", 0) == 0 ? "Instrument: "_ca + txt (pe->nomInstrument)
                                            : mibase::gm::programa (pe->instrument) >= 0 ? "Instrument: General MIDI"_ca : "Instrument: sinte intern"_ca);
                btSo.setButtonText ("So: "_ca + txt (midi::nomInstrument (pe->instrument.empty() ? "intern:piano" : pe->instrument)));
            }
            btTeclat.setButtonText ("Teclat MIDI: "_ca + (pe->entradaMidi.empty() ? "qualsevol"_ca : txt (pe->entradaMidiNom)));
        }
        for (auto* b : { &btInstrument, &btSo, &btTeclat }) b->setEnabled (pistaEditor() != nullptr);
        repaint();
    }

    int alcadaIdeal() const { return alcada; }

    void resized() override
    {
        const int w = getWidth();
        int y = 0;
        auto fila = [&] (int alt) { auto r = juce::Rectangle<int> (0, y, w, alt); y += alt + 4; return r; };
        auto obre = [&] (int k)
        {
            auto& sc = seccions[(size_t) k];
            sc.rect = juce::Rectangle<int> (0, y, w, 22);
            y += 26;
            for (auto* c : sc.fills) c->setVisible (! sc.plegada);
            return ! sc.plegada;
        };
        zPista = zEstat = zNotes = zAcordAra = zAjudaAcords = zDurada = zTriades = zProgressio = {};

        if (obre (0))
        {
            zPista = fila (18);
            btInstrument.setBounds (fila (28));
            btSo.setBounds (fila (28));
            btTeclat.setBounds (fila (28));
            y += 8;
        }

        if (obre (1))
        {
            zEstat = fila (34);
            for (int r = 0; r < 3; ++r)
            {
                auto f = fila (30);
                const int a = (w - 9) / 4;
                for (int c = 0; c < 4; ++c) arrels[r * 4 + c]->setBounds (f.getX() + c * (a + 3), f.getY(), a, 30);
            }
            btEscala.setBounds (fila (30));
            btAuto.setBounds (fila (30));
            zNotes = fila (18);
            y += 8;
        }

        if (obre (2))
        {
            mode.setBounds (fila (32));
            y += 8;
        }

        if (obre (3))
        {
            setenes.setBounds (fila (28));
            for (int r = 0; r < 2; ++r)
            {
                auto f = fila (32);
                const int a = (w - 9) / 4;
                for (int c = 0; c < 4; ++c)
                {
                    const int k = r * 4 + c;
                    if (k < 7) graus[k]->setBounds (f.getX() + c * (a + 3), f.getY(), a, 32);
                }
            }
            zAjudaAcords = fila (30);
            zDurada = fila (14);
            durada.setBounds (fila (28));
            btMinim.setBounds (fila (28));
            y += 8;
        }

        // 0.12: el constructor d'acords ja no hi és (en Roger: massa complicat).
        seccions[4].rect = {};
        for (auto* c : seccions[4].fills) c->setVisible (false);
        if (false)
        {
            {
                auto f = fila (30);
                btFonamental.setBounds (f.removeFromLeft (58));
                f.removeFromLeft (4);
                btTipus.setBounds (f);
            }
            inversio.setBounds (fila (28));
            voicing.setBounds (fila (28));
            {
                auto f = fila (28);
                octava.setBounds (f.removeFromLeft (84));
                f.removeFromLeft (4);
                btDobla.setBounds (f.removeFromLeft ((f.getWidth() - 4) / 2));
                f.removeFromLeft (4);
                btBaix.setBounds (f);
            }
            {
                auto f = fila (30);
                btEscolta.setBounds (f.removeFromLeft ((f.getWidth() - 4) / 2));
                f.removeFromLeft (4);
                btPosa.setBounds (f);
            }
            zTriades = fila (16);
            btFesAcord.setBounds (fila (28));
            btFesEscala.setBounds (fila (28));
            {
                auto f = fila (28);
                const int a = (w - 8) / 3;
                btInvAvall.setBounds (f.removeFromLeft (a)); f.removeFromLeft (4);
                btInvAmunt.setBounds (f.removeFromLeft (a)); f.removeFromLeft (4);
                btEnllaca.setBounds (f);
            }
            y += 8;
        }

        if (obre (5))
        {
            btProgressio.setBounds (fila (30));
            zProgressio = fila (34);
            compassos.setBounds (fila (28));
            {
                auto f = fila (30);
                btEscoltaProg.setBounds (f.removeFromLeft ((f.getWidth() - 4) / 2));
                f.removeFromLeft (4);
                btInsereix.setBounds (f);
            }
            btAplicaPads.setBounds (fila (30));
        }
        alcada = y + 10;
    }

    void paint (juce::Graphics& g) override
    {
        // Títols de les seccions, amb un triangle (obert / plegat).
        for (auto& sc : seccions)
        {
            if (sc.rect.isEmpty()) continue;
            auto r = sc.rect;
            g.setColour (juce::Colours::black.withAlpha (0.22f));
            g.fillRoundedRectangle (r.toFloat(), 4.0f);
            auto tri = r.removeFromLeft (20).toFloat().withSizeKeepingCentre (8.0f, 8.0f);
            juce::Path pth;
            if (sc.plegada) pth.addTriangle (tri.getX() + 1.0f, tri.getY(), tri.getRight(), tri.getCentreY(), tri.getX() + 1.0f, tri.getBottom());
            else pth.addTriangle (tri.getX(), tri.getY() + 1.0f, tri.getRight(), tri.getY() + 1.0f, tri.getCentreX(), tri.getBottom());
            g.setColour (tema::suau);
            g.fillPath (pth);
            dibuix::pintaEtiquetaSeccio (g, sc.nom, r);
        }

        if (! zPista.isEmpty())
        {
            g.setFont (dibuix::lletra (12.5f, true));
            if (auto* t = pistaEditor())
            {
                g.setColour (EstatEstudi::colorPista (*t));
                g.fillRect (zPista.getX(), zPista.getY() + 3, 4, zPista.getHeight() - 6);
                g.setColour (tema::text);
                g.drawText (dibuix::utf8 (t->nom), zPista.withTrimmedLeft (10), juce::Justification::centredLeft, true);
            }
            else
            {
                g.setColour (tema::suau);
                g.drawText ("Obre un clip MIDI"_ca, zPista, juce::Justification::centredLeft, true);
            }
        }

        const auto& p = estat.projecte;
        const auto t = tonalitat();
        const auto d = hm::detecta (p);
        g.setFont (dibuix::lletra (12.0f));
        juce::String estatText;
        juce::Colour col = tema::suau;
        if (! p.tonalitatAuto) estatText = "Triada a mà: "_ca + txt (hm::nomTonalitat (t));
        else if (d.fiable)
        {
            estatText = "Detectada: "_ca + txt (hm::nomTonalitat (t)) + " (" + juce::String (juce::roundToInt (d.confianca * 100.0)) + " %)";
            col = vintage::ledVerd;
        }
        else
        {
            estatText = "Encara no hi ha prou notes per detectar-la: tria-la a mà (ara "_ca + txt (hm::nomTonalitat (t)) + ")";
            col = tema::textMig;
        }
        g.setColour (col);
        if (! zEstat.isEmpty()) g.drawFittedText (estatText, zEstat, juce::Justification::topLeft, 2, 0.85f);

        // Notes de l'escala (la fonamental en taronja).
        if (! zNotes.isEmpty())
        {
            auto r = zNotes;
            const auto notes = hm::notesEscala (t);
            const int a = juce::jmax (18, r.getWidth() / juce::jmax (1, (int) notes.size()));
            g.setFont (dibuix::mono (12.5f));
            for (size_t i = 0; i < notes.size(); ++i)
            {
                g.setColour (i == 0 ? tema::accent : tema::text);
                g.drawText (txt (hm::nomTo (notes[i], t)), r.removeFromLeft (a), juce::Justification::centredLeft, false);
            }
        }

        // Acord que sona al capçal.
        if (! zAcordAra.isEmpty())
        {
            const auto r = hm::reconeix (hm::notesSonant (p, estat.capcal));
            g.setFont (dibuix::lletra (12.5f));
            g.setColour (tema::suau);
            auto z = zAcordAra;
            g.drawText ("Acord al capçal:"_ca, z.removeFromLeft (104), juce::Justification::centredLeft, false);
            g.setColour (r.trobat ? vintage::ledVerd : tema::suau);
            g.setFont (dibuix::lletra (13.0f, true));
            g.drawText (r.trobat ? txt (r.nom) : juce::String ("-"), z, juce::Justification::centredLeft, false);
        }

        g.setFont (dibuix::lletra (11.0f));
        g.setColour (tema::suau);
        if (! zAjudaAcords.isEmpty())
        g.drawFittedText (estat.segell.actiu() ? "Ara clica al Piano Roll per posar " + estat.segell.nom + " (Maj+clic: més; Esc o botó dret: deixa-ho)"_ca
                                               : "Arrossega'l al Piano Roll (o clic i un altre clic al Piano Roll)."_ca,
                          zAjudaAcords, juce::Justification::topLeft, 2, 0.85f);
        g.drawText ("Durada de cada acord", zDurada, juce::Justification::centredLeft, false);
        g.drawText ("Sobre les notes triades al Piano Roll:"_ca, zTriades, juce::Justification::centredLeft, false);

        // Progressió amb els acords d'aquesta tonalitat.
        if (! zProgressio.isEmpty())
        {
            const auto acords = hm::acordsProgressio (t, progressio, setenes.getTriat() == 1);
            juce::String s;
            for (size_t i = 0; i < acords.size() && i < 12; ++i)
                s += (i > 0 ? " - " : "") + txt (acords[i].nom);
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (12.5f, true));
            g.drawFittedText (s, zProgressio, juce::Justification::topLeft, 2, 0.8f);
        }
    }

    /** Refresc del que canvia mentre sona (l'acord del capçal). */
    void refresca()
    {
        if (std::abs (estat.capcal - capcalVist) > 1e-4)
        {
            capcalVist = estat.capcal;
            repaint (zAcordAra);
        }
    }

private:
    hm::Voicing voicingTriat() const { return (hm::Voicing) voicing.getTriat(); }
    Tonalitat tonalitat() const { return hm::tonalitatEfectiva (estat.projecte); }

    long long duradaAcord() const
    {
        switch (durada.getTriat())
        {
            case 0: return midi::ticksPerCompas (estat.projecte);
            case 1: return ticksPerNegra * 2;
            case 2: return ticksPerNegra;
            default: return 0;   // la del Piano Roll («Nota»)
        }
    }

    int arrelMidi (int pc) const
    {
        int n = 12 * (octava.getValor() + 1) + pc;
        if (pc > 7) n -= 12;   // les fonamentals altes, a sota (A, Bb, B per sota del C)
        return n;
    }

    std::vector<int> acordConstructor() const
    {
        return hm::acord (arrelMidi (fonamental), tipus, inversio.getTriat(), voicingTriat(), btDobla.getToggleState() ? 2 : 1, btBaix.getToggleState());
    }

    juce::String nomConstructor() const
    {
        const auto n = acordConstructor();
        const int baix = n.empty() ? fonamental : n.front();
        return txt (hm::nomAcord (fonamental, tipus, btBaix.getToggleState() ? -1 : baix));
    }

    Pista* pistaEditor() const
    {
        auto* e = editor();
        return e != nullptr ? estat.pista (e->pistaDeLEditor()) : nullptr;
    }

    void canviaPista (const std::function<void (Pista&)>& f, const juce::String& evolucio)
    {
        auto* t = pistaEditor();
        if (t == nullptr) return;
        estat.desfes.abans (estat.projecte);
        f (*t);
        estat.canviat (evolucio);
        actualitza();
    }

    /** 0.31: posa un so General MIDI a la pista (si no hi ha el banc, pregunta si es baixa). */
    void triaGM (int prog)
    {
        bancgm::assegura (this, [this, prog]
        {
            canviaPista ([prog] (Pista& q) { q.instrument = mibase::gm::instrument (prog); q.nomInstrument.clear(); q.estatInstrument.clear(); },
                         "Instrument: General MIDI "_ca + txt (mibase::gm::nomPrograma (prog)));
        });
    }

    void menuInstrument()
    {
        auto* t = pistaEditor();
        if (t == nullptr) return;
        const bool sampler = mibase::esSampler (*t), vst = t->instrument.rfind ("vst3:", 0) == 0;
        const int gm = mibase::gm::programa (t->instrument);
        juce::PopupMenu m;
        m.addItem (1, "Sinte intern", true, ! sampler && ! vst && gm < 0);
        m.addSubMenu ("General MIDI (128 sons)"_ca, bancgm::menu (500, gm), true, juce::Image(), gm >= 0);   // 0.31
        m.addItem (3, "Samplejador"_ca, true, sampler);
        m.addSeparator();
        m.addItem (2, "Plugins VST3: a la columna esquerra de la taula de treball"_ca, false);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btInstrument), [this] (int r)
        {
            auto* p = pistaEditor();
            if (p == nullptr) return;
            if (r >= 500 && r < 628) { triaGM (r - 500); return; }
            if (r == 1 && (mibase::esSampler (*p) || mibase::gm::programa (p->instrument) >= 0))
                canviaPista ([] (Pista& q) { q.instrument = "intern:piano"; }, "Instrument: sinte intern"_ca);
            else if (r == 3)
            {
                if (! mibase::esSampler (*p))
                    canviaPista ([] (Pista& q) { q.instrument = mibase::instrumentSampler; q.nomInstrument.clear(); q.estatInstrument.clear(); }, "Instrument: samplejador"_ca);
                if (onObreSampler) juce::MessageManager::callAsync ([f = onObreSampler] { f(); });
            }
        });
    }

    void menuSo()
    {
        auto* t = pistaEditor();
        if (t == nullptr) return;
        if (mibase::esSampler (*t)) { if (onObreSampler) onObreSampler(); return; }
        juce::PopupMenu m;
        const int actual = juce::jmax (0, midi::soIntern (t->instrument.empty() ? "intern:piano" : t->instrument));
        const int gm = mibase::gm::programa (t->instrument);
        for (int k = 0; k < (int) midi::sonsInterns().size(); ++k)
            m.addItem (k + 1, txt (midi::sonsInterns()[(size_t) k].nom), true, actual == k && gm < 0);
        m.addSeparator();
        m.addSubMenu ("General MIDI (128 sons)"_ca, bancgm::menu (500, gm), true, juce::Image(), gm >= 0);   // 0.31
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btSo), [this] (int r)
        {
            if (r <= 0) return;
            if (r >= 500 && r < 628) { triaGM (r - 500); return; }
            const auto& so = midi::sonsInterns()[(size_t) (r - 1)];
            canviaPista ([&so] (Pista& p) { p.instrument = std::string ("intern:") + so.id; },
                         "Pista «"_ca + (pistaEditor() ? dibuix::utf8 (pistaEditor()->nom) : juce::String()) + "»: so "_ca + txt (so.nom));
        });
    }

    void menuTeclat()
    {
        auto* t = pistaEditor();
        if (t == nullptr) return;
        const auto llista = teclats ? teclats() : juce::Array<juce::MidiDeviceInfo>();
        juce::PopupMenu m;
        m.addItem (1, "Qualsevol (el que toca la pista triada)", true, t->entradaMidi.empty());
        m.addSeparator();
        bool hiEs = false;
        for (int i = 0; i < llista.size(); ++i)
        {
            const bool aquest = t->entradaMidi == llista[i].identifier.toStdString();
            hiEs = hiEs || aquest;
            m.addItem (100 + i, llista[i].name, true, aquest);
        }
        if (! t->entradaMidi.empty() && ! hiEs)
            m.addItem (2, txt (t->entradaMidiNom) + "  (desconnectat)", true, true);
        if (llista.isEmpty())
            m.addItem (3, "No hi ha cap teclat MIDI connectat", false);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btTeclat), [this, llista] (int r)
        {
            if (r == 1) canviaPista ([] (Pista& p) { p.entradaMidi.clear(); p.entradaMidiNom.clear(); }, {});
            else if (r >= 100 && r - 100 < llista.size())
            {
                const auto d = llista[r - 100];
                canviaPista ([&d] (Pista& p) { p.entradaMidi = d.identifier.toStdString(); p.entradaMidiNom = d.name.toStdString(); },
                             "Pista «"_ca + (pistaEditor() ? dibuix::utf8 (pistaEditor()->nom) : juce::String()) + "»: la toca el teclat «"_ca + d.name + "»"_ca);
            }
        });
    }

    void arrossegaGrau (juce::Component& origen, int g)
    {
        const auto a = hm::acordsDeTonalitat (tonalitat(), setenes.getTriat() == 1)[(size_t) g];
        fonamental = a.pc;
        tipus = a.tipus;
        actualitza();
        if (auto* e = editor()) e->escoltaAcord (acordConstructor());
        PianoRoll::arrossegaAcord (origen, estat, acordConstructor(), txt (a.nom), duradaAcord(), btMinim.getToggleState());
    }

    void canviaTonalitat (Tonalitat t, bool automatica)
    {
        estat.desfes.abans (estat.projecte);
        auto& p = estat.projecte;
        if (automatica)
        {
            const auto d = hm::detecta (p);
            if (d.fiable) t = d.tonalitat;
        }
        p.tonalitatAuto = automatica;
        p.tonalitat = t;
        estat.info = "Tonalitat: "_ca + txt (hm::nomTonalitat (hm::tonalitatEfectiva (p))) + (automatica ? " (automàtica)" : "")
                   + ". Les notes no es toquen."_ca;
        estat.canviat ("Tonalitat: "_ca + txt (hm::nomTonalitat (hm::tonalitatEfectiva (p))) + (automatica ? " (automàtica)" : ""));
        fonamental = hm::tonalitatEfectiva (p).to;
        actualitza();
    }

    void menuEscala()
    {
        juce::PopupMenu m;
        m.addSectionHeader ("Escala");
        const auto t = tonalitat();
        for (int i = 0; i < (int) hm::escales().size(); ++i)
        {
            if (i == 10) m.addSeparator();
            m.addItem (i + 1, txt (hm::escales()[(size_t) i].nom), true, t.escala == i);
        }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btEscala), [this] (int r)
        {
            if (r > 0) canviaTonalitat ({ tonalitat().to, r - 1 }, false);
        });
    }

    void menuFonamental()
    {
        juce::PopupMenu m;
        const auto t = tonalitat();
        for (int i = 0; i < 12; ++i)
            m.addItem (i + 1, txt (hm::nomTo (i, t)) + (hm::aEscala (i, t) ? juce::String() : "   (fora de l'escala)"), true, fonamental == i);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btFonamental), [this] (int r)
        {
            if (r > 0) { fonamental = r - 1; actualitza(); }
        });
    }

    void menuTipus()
    {
        juce::PopupMenu m;
        for (int i = 0; i < (int) hm::tipusAcords().size(); ++i)
        {
            if (i == 2 || i == 6 || i == 10 || i == 15) m.addSeparator();
            const auto& tp = hm::tipusAcords()[(size_t) i];
            m.addItem (i + 1, txt (tp.nom) + "   (" + txt (hm::nomTo (fonamental) + tp.sufix) + ")", true, tipus == i);
        }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btTipus), [this] (int r)
        {
            if (r > 0) { tipus = r - 1; actualitza(); }
        });
    }

    void menuProgressio()
    {
        juce::PopupMenu m;
        const auto t = tonalitat();
        for (int i = 0; i < (int) hm::progressions().size(); ++i)
        {
            const auto& pr = hm::progressions()[(size_t) i];
            const auto acords = hm::acordsProgressio (t, i, false);
            juce::String s;
            for (size_t k = 0; k < acords.size() && k < 4; ++k) s += (k > 0 ? " " : "") + txt (acords[k].nom);
            if (i == 7) m.addSeparator();
            m.addItem (i + 1, txt (pr.nom) + "      " + s + (acords.size() > 4 ? juce::String::fromUTF8 (" …") : juce::String()), true, progressio == i);
        }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btProgressio), [this] (int r)
        {
            if (r > 0) { progressio = r - 1; actualitza(); }
        });
    }

    void triaGrau (int g)
    {
        const auto t = tonalitat();
        const auto a = hm::acordsDeTonalitat (t, setenes.getTriat() == 1)[(size_t) g];
        // El constructor es posa en aquest acord (així es pot retocar: inversió, voicing…).
        fonamental = a.pc;
        tipus = a.tipus;
        actualitza();
        armaSegell (acordConstructor(), txt (a.nom));
    }

    void armaSegell (const std::vector<int>& notes, const juce::String& nom)
    {
        auto* e = editor();
        if (e == nullptr || ! e->teClip())
        {
            estat.info = "Obre un clip MIDI al Piano Roll per posar-hi acords"_ca;
            estat.avisa();
            return;
        }
        e->escoltaAcord (notes);
        estat.segell.notes = notes;
        estat.segell.durada = duradaAcord();
        estat.segell.enllaca = btMinim.getToggleState();
        estat.segell.nom = nom;
        estat.info = nom + ": clica al Piano Roll per posar-lo (Maj+clic: continua; Esc: deixa-ho)"_ca;
        estat.avisa();
    }

    std::vector<std::vector<int>> acordsProgressio() const
    {
        const int comp[] = { 2, 4, 8, 16 };
        const int octavaProg = octava.getValor();
        return hm::generaProgressio (tonalitat(), progressio, comp[compassos.getTriat()], setenes.getTriat() == 1, voicingTriat(),
                                     btMinim.getToggleState(), octavaProg);
    }

    void escoltaProgressio (bool encen)
    {
        auto* e = editor();
        if (! encen || e == nullptr || ! e->teClip())
        {
            reproductor.stopTimer();
            if (! sonant.empty() && e != nullptr) e->sonaNotes (sonant, false);
            sonant.clear();
            btEscoltaProg.setToggleState (false, juce::dontSendNotification);
            return;
        }
        acordsSonant = acordsProgressio();
        pas = 0;
        btEscoltaProg.setToggleState (true, juce::dontSendNotification);
        const double segonsCompas = 60.0 / juce::jmax (20.0, estat.projecte.tempo) * juce::jmax (1, estat.projecte.compasNum);
        pasProgressio();
        reproductor.startTimer (juce::jmax (100, juce::roundToInt (segonsCompas * 1000.0)));
    }

    void pasProgressio()
    {
        auto* e = editor();
        if (e == nullptr) { escoltaProgressio (false); return; }
        if (! sonant.empty()) e->sonaNotes (sonant, false);
        if (pas >= (int) acordsSonant.size()) { escoltaProgressio (false); return; }
        sonant = acordsSonant[(size_t) pas++];
        e->sonaNotes (sonant, true);
    }

    void insereixProgressio()
    {
        auto* e = editor();
        if (e == nullptr || ! e->teClip())
        {
            estat.info = "Obre un clip MIDI al Piano Roll per posar-hi la progressió"_ca;
            estat.avisa();
            return;
        }
        escoltaProgressio (false);
        const auto acords = acordsProgressio();
        e->insereixAcords (acords, midi::ticksPerCompas (estat.projecte), "Progressió ("_ca + juce::String ((int) acords.size()) + " compassos)");
    }

    EstatEstudi& estat;
    std::function<PianoRoll*()> editor;
    juce::PropertiesFile& ajustos;
    std::function<juce::Array<juce::MidiDeviceInfo>()> teclats;

    struct Seccio
    {
        juce::String nom, clau;
        std::vector<juce::Component*> fills;
        bool plegada = false;
        juce::Rectangle<int> rect;
    };
    std::array<Seccio, 6> seccions;

    Boto btInstrument { "Instrument: sinte intern", std::nullopt, Boto::Estil::Secundari };
    Boto btSo { "So: Piano", std::nullopt, Boto::Estil::Secundari };
    Boto btTeclat { "Teclat MIDI: qualsevol", std::nullopt, Boto::Estil::Secundari };
    juce::OwnedArray<Boto> arrels;
    juce::OwnedArray<BotoArrossegable> graus;
    Boto btEscala { "Major", std::nullopt, Boto::Estil::Secundari };
    Boto btAuto { "Automàtica"_ca, std::nullopt, Boto::Estil::Tecla };
    Segmentat mode { { "Visual", "Assistit", "Bloquejat" } };
    Boto btAcord { "Ressalta l'acord que sona", std::nullopt, Boto::Estil::Tecla };
    Segmentat setenes { { "Tríades"_ca, "Setenes" } };
    Segmentat durada { { "1 compàs"_ca, "1/2", "1/4", "Nota" } };
    Boto btMinim { "Moviment mínim"_ca, std::nullopt, Boto::Estil::Tecla };
    Boto btFonamental { "C", std::nullopt, Boto::Estil::Secundari };
    Boto btTipus { "Major", std::nullopt, Boto::Estil::Secundari };
    Segmentat inversio { { "Fonam.", "1a", "2a", "3a" } };
    Segmentat voicing { { "Tancat", "Obert" } };
    CaixaNumero octava { "Octava", 1, 7, 4 };
    Boto btDobla { "2 oct.", std::nullopt, Boto::Estil::Tecla };
    Boto btBaix { "Baix", std::nullopt, Boto::Estil::Tecla };
    Boto btEscolta { "Escolta", dibuix::Icona::Play, Boto::Estil::Secundari };
    BotoArrossegable btPosa { "Posa'l", dibuix::Icona::Llapis, Boto::Estil::Secundari };
    Boto btFesAcord { "Fes-ne acords d'aquest tipus", std::nullopt, Boto::Estil::Secundari };
    Boto btFesEscala { "Fes-ne acords de la tonalitat", std::nullopt, Boto::Estil::Secundari };
    Boto btInvAvall { "Inv. avall", std::nullopt, Boto::Estil::Secundari };
    Boto btInvAmunt { "Inv. amunt", std::nullopt, Boto::Estil::Secundari };
    Boto btEnllaca { "Enllaça"_ca, std::nullopt, Boto::Estil::Secundari };
    Boto btProgressio { "Pop", std::nullopt, Boto::Estil::Secundari };
    Segmentat compassos { { "2", "4", "8", "16" } };
    Boto btEscoltaProg { "Escolta", dibuix::Icona::Play, Boto::Estil::Tecla };
    Boto btInsereix { "Insereix", dibuix::Icona::Notes, Boto::Estil::Primari };
    Boto btAplicaPads { "Aplica al Chord Pad", std::nullopt, Boto::Estil::Secundari };

    juce::Rectangle<int> zPista, zEstat, zNotes, zAcordAra, zAjudaAcords, zDurada, zTriades, zProgressio;
    int alcada = 900;
    int fonamental = 0, tipus = 0, progressio = 0;
    double capcalVist = -1.0;

    Rellotge reproductor;
    std::vector<std::vector<int>> acordsSonant;
    std::vector<int> sonant;
    int pas = 0;
};

//==============================================================================
PanellHarmonia::PanellHarmonia (EstatEstudi& e, std::function<PianoRoll*()> editorActiu, juce::PropertiesFile& ajustos,
                                std::function<juce::Array<juce::MidiDeviceInfo>()> teclats) : estat (e)
{
    contingut = std::make_unique<Contingut> (e, std::move (editorActiu), ajustos, std::move (teclats));
    contingut->onMida = [this] { resized(); };
    contingut->onObreSampler = [this] { if (onObreSampler) onObreSampler(); };
    vista.setViewedComponent (contingut.get(), false);
    vista.setScrollBarsShown (true, false);
    vista.setScrollBarThickness (8);
    addAndMakeVisible (vista);
    estat.addChangeListener (this);
    startTimerHz (10);
}

PanellHarmonia::~PanellHarmonia()
{
    stopTimer();
    estat.removeChangeListener (this);
}

void PanellHarmonia::timerCallback()
{
    if (isShowing()) contingut->refresca();
}

void PanellHarmonia::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (isShowing()) contingut->actualitza();
}

void PanellHarmonia::resized()
{
    vista.setBounds (getLocalBounds());
    contingut->setSize (getWidth() - 10, 100);
    contingut->resized();
    contingut->setSize (getWidth() - 10, contingut->alcadaIdeal());
}

void PanellHarmonia::paint (juce::Graphics&) {}
