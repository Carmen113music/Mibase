#include "EditorSampler.h"

#include <cmath>

using mibase::ParamsSampler;

namespace
{
    juce::String txtDb (double v) { return juce::String (v, 1) + " dB"; }
    juce::String txtPct (double v) { return juce::String ((int) std::lround (v)) + " %"; }
    juce::String txtTemps (double s) { return s < 1.0 ? juce::String ((int) std::lround (s * 1000.0)) + " ms" : juce::String (s, 2) + " s"; }
    // poms amb escala no lineal (0..1)
    double segDeEnv (double v) { return 10.0 * v * v * v; }
    double envDeSeg (double s) { return std::cbrt (juce::jlimit (0.0, 10.0, s) / 10.0); }
    double hzDeCut (double v) { return 20.0 * std::pow (1000.0, v); }
    double cutDeHz (double hz) { return std::log (juce::jlimit (20.0, 20000.0, hz) / 20.0) / std::log (1000.0); }
    double segDeXf (double v) { return 0.5 * v * v; }
    double xfDeSeg (double s) { return std::sqrt (juce::jlimit (0.0, 0.5, s) / 0.5); }
    juce::String txtSpeed (double sp)
    {
        if (std::abs (sp - 1.0) < 0.005) return "x1";
        return sp > 1.0 ? "x" + juce::String (sp, 2) : ":" + juce::String (1.0 / sp, 2);
    }
}

//==============================================================================
juce::Rectangle<float> OpcionsPetites::cel (int i) const
{
    const int files = (textos.size() + cols - 1) / cols;
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const float w = r.getWidth() / (float) cols, h = r.getHeight() / (float) juce::jmax (1, files);
    return { r.getX() + (float) (i / files) * w, r.getY() + (float) (i % files) * h, w, h };
}

void OpcionsPetites::paint (juce::Graphics& g)
{
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);
    g.setFont (dibuix::lletra (10.5f, true));
    for (int i = 0; i < textos.size(); ++i)
    {
        auto c = cel (i).reduced (1.0f);
        const bool t = i == triat;
        if (t) { g.setColour (tema::superficie2); g.fillRoundedRectangle (c, 2.0f); }
        auto led = c.removeFromLeft (12.0f).withSizeKeepingCentre (5.0f, 5.0f);
        g.setColour (t ? tema::accent : juce::Colour (0xff3a3a43));
        g.fillEllipse (led);
        g.setColour (t ? tema::text : tema::suau);
        g.drawText (textos[i], c, juce::Justification::centredLeft, true);
    }
}

void OpcionsPetites::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < textos.size(); ++i)
        if (cel (i).contains (e.position) && i != triat)
        {
            triat = i;
            repaint();
            if (onCanvi) onCanvi (i);
        }
}

//==============================================================================
juce::String EditorSampler::nomNota (int n)
{
    static const char* noms[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    n = juce::jlimit (0, 127, n);
    return juce::String (noms[n % 12]) + juce::String (n / 12 - 1);
}

EditorSampler::ParamPom& EditorSampler::afegeixPom (int fila, const juce::String& nom, double mn, double mx, double def, bool bipolar,
                                                    std::function<juce::String (double)> text,
                                                    std::function<double (const ParamsSampler&)> llegeix,
                                                    std::function<void (ParamsSampler&, double)> escriu, bool alFinal)
{
    poms.push_back ({ std::make_unique<Pom> (nom, mn, mx, def, bipolar, std::move (text)), std::move (llegeix), std::move (escriu), alFinal, 0.0, fila });
    const size_t i = poms.size() - 1;
    auto* pom = poms[i].pom.get();
    consola.addAndMakeVisible (*pom);
    pom->onComenca = [this, pom] { estat.desfes.abans (estat.projecte); pomActiu = pom; };
    pom->onCanvi = [this, i] (double v)
    {
        auto& pp = poms[i];
        if (pp.alFinal) { pp.pendent = v; return; }
        if (auto* t = pista()) { pp.escriu (t->sampler, v); estat.avisa(); }
    };
    pom->onAcaba = [this, i]
    {
        auto& pp = poms[i];
        if (auto* t = pista(); t != nullptr && pp.alFinal) pp.escriu (t->sampler, pp.pendent);
        pomActiu = nullptr;
        estat.canviat();
    };
    return poms[i];
}

EditorSampler::EditorSampler (EstatEstudi& e) : SeccioEstudi (e)
{
    setWantsKeyboardFocus (true);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &btCarrega, &btPlay, &btZero, &selBucle, &btConsola, &btTanca })
        addAndMakeVisible (c);
    for (juce::Component* c : std::initializer_list<juce::Component*> { &selToc, &selVeus, &selFiltre, &selPendent, &btRoot, &btAlTempo, &btX2, &btD2,
                                                                         &btReversa, &btManteTo, &btDurFixa, &btFlat, &btSmono })
        consola.addAndMakeVisible (c);
    consola.pinta = [this] (juce::Graphics& g) { pintaConsola (g); };
    vistaConsola.setViewedComponent (&consola, false);
    vistaConsola.setScrollBarsShown (true, false);
    vistaConsola.setScrollBarThickness (8);
    addAndMakeVisible (vistaConsola);
    addAndMakeVisible (divConsola);
    divConsola.setTooltip ("Arrossega per fer els paràmetres més alts o més baixos (doble clic: la mida que cal)"_ca);
    divConsola.onComenca = [this] { altInici = vistaConsola.getHeight(); };
    divConsola.onArrossega = [this] (int dy)
    {
        estat.sampler.altConsola = juce::jlimit (40, juce::jmax (40, getHeight() - 140), altInici - dy);
        resized();
        repaint();
    };
    divConsola.onAcaba = [this] { if (onAltConsola) onAltConsola (estat.sampler.altConsola); };
    divConsola.onDobleClic = [this] { estat.sampler.altConsola = 0; if (onAltConsola) onAltConsola (0); resized(); repaint(); };

    btCarrega.setTooltip ("Carrega un fitxer d'àudio (wav, mp3, ogg, flac). També el pots arrossegar a sobre de l'ona. Es copia al projecte."_ca);
    btCarrega.onClick = [this] { if (onCarrega) onCarrega(); };
    btPlay.setTooltip ("Escolta l'original des del capçal (la línia blanca: clica l'ona per posar-la on vulguis). Esc l'atura"_ca);
    btPlay.onClick = [this] { commutaEscolta(); };
    btZero.setTooltip ("En moure els marcadors, s'enganxen al pas per zero més proper (evita clics)"_ca);
    btZero.onClick = [this] { canvia ([] (ParamsSampler& p) { p.zeroCrossing = ! p.zeroCrossing; }); };
    selBucle.setTooltip ("Bucle: Sustain + release = en prémer sona fins al bucle, mentre mantens repeteix el bucle i en deixar anar continua des del final del bucle fins al final"_ca);
    selBucle.onCanvi = [this] (int i) { canvia ([i] (ParamsSampler& p) { p.modeBucle = i; }, "Samplejador: mode de bucle"_ca); };
    btConsola.setTooltip ("Mostra o amaga els paràmetres (amagats, l'ona es fa més alta). Z = tot el sample; Maj + Z = el fragment; Ctrl + roda = zoom"_ca);
    btConsola.onClick = [this] { estat.sampler.consola = ! estat.sampler.consola; resized(); repaint(); };
    btTanca.setTooltip ("Amaga el samplejador"_ca);
    btTanca.onClick = [this] { if (onEscolta) onEscolta (-1.0); escoltant = false; if (onTanca) onTanca(); };

    selToc.setTooltip ("Play: sona mentre la tecla és premuda (amb bucle i release). One-shot: sona sencer. Gate: calla en deixar anar"_ca);
    selToc.onCanvi = [this] (int i) { canvia ([i] (ParamsSampler& p) { p.modeToc = i; }); };
    selVeus.setTooltip ("Poli: diverses notes alhora. Mono: una sola. Legato: una sola i la nota nova continua el so sense tornar a començar"_ca);
    selVeus.onCanvi = [this] (int i) { canvia ([i] (ParamsSampler& p) { p.veus = i; }); };
    selFiltre.setTooltip ("Filtre: passa-baixos, passa-alts, passa-banda o notch"_ca);
    selFiltre.onCanvi = [this] (int i) { canvia ([i] (ParamsSampler& p) { p.filtre = i; }); };
    selPendent.setTooltip ("Pendent del filtre"_ca);
    selPendent.onCanvi = [this] (int i) { canvia ([i] (ParamsSampler& p) { p.pendent = i == 1 ? 24 : 12; }); };

    btRoot.setTooltip ("Detecta la nota del sample (la del fragment)"_ca);
    btRoot.onClick = [this]
    {
        auto* p = params();
        if (p == nullptr || m == nullptr) return;
        const double dur = durada();
        const double fi = p->fi < 0.0 ? dur : p->fi;
        mibase::sampler::Mostra tros;
        tros.fs = m->fs;
        const auto a = (size_t) juce::jlimit (0.0, (double) m->L.size(), p->inici * m->fs), b = (size_t) juce::jlimit ((double) a, (double) m->L.size(), fi * m->fs);
        tros.L.assign (m->L.begin() + (long) a, m->L.begin() + (long) b);
        if (! m->R.empty()) tros.R.assign (m->R.begin() + (long) a, m->R.begin() + (long) b);
        const int r = mibase::sampler::detectaRoot (tros);
        if (r < 0) { estat.info = "No trobo una nota clara en aquest fragment"_ca; estat.avisa(); return; }
        canvia ([r] (ParamsSampler& q) { q.root = r; }, "Samplejador: nota original "_ca + nomNota (r));
        estat.info = "Nota original: "_ca + nomNota (r);
    };
    btAlTempo.setTooltip ("Posa l'Speed perquè el sample vagi al tempo del projecte (cal saber-ne el BPM)"_ca);
    btAlTempo.onClick = [this]
    {
        auto* p = params();
        if (p == nullptr) return;
        if (p->bpm <= 0.0) { estat.info = "Posa primer el BPM del sample"_ca; estat.avisa(); return; }
        const double sp = juce::jlimit (0.25, 4.0, estat.projecte.tempo / p->bpm);
        canvia ([sp] (ParamsSampler& q) { q.speed = sp; }, "Samplejador: al tempo del projecte"_ca);
    };
    btX2.setTooltip ("El doble de ràpid"_ca);
    btX2.onClick = [this] { canvia ([] (ParamsSampler& p) { p.speed = juce::jlimit (0.25, 4.0, p.speed * 2.0); }); };
    btD2.setTooltip ("La meitat de ràpid"_ca);
    btD2.onClick = [this] { canvia ([] (ParamsSampler& p) { p.speed = juce::jlimit (0.25, 4.0, p.speed / 2.0); }); };
    btReversa.setTooltip ("Sona del revés"_ca);
    btReversa.onClick = [this] { canvia ([] (ParamsSampler& p) { p.reversa = ! p.reversa; }); };
    btManteTo.setTooltip ("Encès: l'Speed canvia la durada sense canviar el to (time-stretch). Apagat: com una cinta (més ràpid = més agut)"_ca);
    btManteTo.onClick = [this] { canvia ([] (ParamsSampler& p) { p.mantenTo = ! p.mantenTo; }); };
    btDurFixa.setTooltip ("Encès: cada tecla canvia el to però el sample dura el mateix (no s'accelera). Apagat: com un sampler clàssic (més agut = més ràpid)"_ca);
    btDurFixa.onClick = [this] { canvia ([] (ParamsSampler& p) { p.durFixa = ! p.durFixa; }, "Samplejador: durada fixa"_ca); };
    btFlat.setTooltip ("Normalitza: el fragment puja fins al màxim sense saturar"_ca);
    btFlat.onClick = [this] { canvia ([] (ParamsSampler& p) { p.flat = ! p.flat; }); };
    btSmono.setTooltip ("Suma els dos canals a mono"_ca);
    btSmono.onClick = [this] { canvia ([] (ParamsSampler& p) { p.smono = ! p.smono; }); };

    // ---- poms ---- (afegeixPom torna una referència: la llista no ha de créixer de lloc)
    poms.reserve (32);
    auto& pVeus = afegeixPom (1, "Veus", 1, 32, 16, false, [] (double v) { return juce::String ((int) std::lround (v)); },
                              [] (const ParamsSampler& p) { return (double) p.maxVeus; }, [] (ParamsSampler& p, double v) { p.maxVeus = (int) std::lround (v); });
    auto& pGain = afegeixPom (1, "Gain", -24, 24, 0, true, txtDb, [] (const ParamsSampler& p) { return p.guanyDb; }, [] (ParamsSampler& p, double v) { p.guanyDb = v; });
    auto& pRoot = afegeixPom (1, "Root", 0, 127, 60, true, [] (double v) { return nomNota ((int) std::lround (v)); },
                              [] (const ParamsSampler& p) { return (double) p.root; }, [] (ParamsSampler& p, double v) { p.root = (int) std::lround (v); });
    auto& pBpm = afegeixPom (1, "BPM", 0, 240, 0, false, [] (double v) { return v < 1.0 ? juce::String::fromUTF8 ("—") : juce::String (v, 1); },
                             [] (const ParamsSampler& p) { return p.bpm; }, [] (ParamsSampler& p, double v) { p.bpm = v < 1.0 ? 0.0 : std::round (v * 10.0) / 10.0; });
    auto& pTune = afegeixPom (1, "Tune", -24, 24, 0, true, [] (double v) { const int s = (int) std::lround (v); return (s > 0 ? "+" : "") + juce::String (s) + " st"; },
                              [] (const ParamsSampler& p) { return p.tune; }, [] (ParamsSampler& p, double v) { p.tune = std::round (v); });
    auto& pFine = afegeixPom (1, "Fine", -100, 100, 0, true, [] (double v) { const int s = (int) std::lround (v); return (s > 0 ? "+" : "") + juce::String (s) + " ct"; },
                              [] (const ParamsSampler& p) { return p.fine; }, [] (ParamsSampler& p, double v) { p.fine = std::round (v); });
    auto& pSpeed = afegeixPom (1, "Speed", -2, 2, 0, true, [] (double v) { return txtSpeed (std::pow (2.0, v)); },
                               [] (const ParamsSampler& p) { return std::log2 (p.speed); }, [] (ParamsSampler& p, double v) { p.speed = juce::jlimit (0.25, 4.0, std::pow (2.0, v)); }, true);
    auto& pWidth = afegeixPom (2, "Width", 0, 200, 100, true, txtPct, [] (const ParamsSampler& p) { return p.width * 100.0; }, [] (ParamsSampler& p, double v) { p.width = v / 100.0; });
    auto& pPan = afegeixPom (2, "Pan", -100, 100, 0, true, [] (double v) { const int s = (int) std::lround (v); return s == 0 ? juce::String ("C") : (s < 0 ? "L" : "R") + juce::String (std::abs (s)); },
                             [] (const ParamsSampler& p) { return p.pan * 100.0; }, [] (ParamsSampler& p, double v) { p.pan = v / 100.0; });
    auto& pVol = afegeixPom (2, "Vol", -60, 6, 0, true, txtDb, [] (const ParamsSampler& p) { return p.volDb; }, [] (ParamsSampler& p, double v) { p.volDb = v; });
    auto& pCut = afegeixPom (2, "Cutoff", 0, 1, 1, false, [] (double v) { const double hz = hzDeCut (v); return hz < 1000.0 ? juce::String ((int) hz) + " Hz" : juce::String (hz / 1000.0, 1) + " kHz"; },
                             [] (const ParamsSampler& p) { return cutDeHz (p.cutoff); }, [] (ParamsSampler& p, double v) { p.cutoff = hzDeCut (v); });
    auto& pReso = afegeixPom (2, "Reso", 0, 100, 0, false, txtPct, [] (const ParamsSampler& p) { return p.reso * 100.0; }, [] (ParamsSampler& p, double v) { p.reso = v / 100.0; });
    auto& pDrive = afegeixPom (2, "Drive", 0, 100, 0, false, txtPct, [] (const ParamsSampler& p) { return p.drive * 100.0; }, [] (ParamsSampler& p, double v) { p.drive = v / 100.0; });
    auto& pA = afegeixPom (2, "Attack", 0, 1, envDeSeg (0.002), false, [] (double v) { return txtTemps (segDeEnv (v)); },
                           [] (const ParamsSampler& p) { return envDeSeg (p.atac); }, [] (ParamsSampler& p, double v) { p.atac = segDeEnv (v); });
    auto& pD = afegeixPom (2, "Decay", 0, 1, envDeSeg (0.2), false, [] (double v) { return txtTemps (segDeEnv (v)); },
                           [] (const ParamsSampler& p) { return envDeSeg (p.decai); }, [] (ParamsSampler& p, double v) { p.decai = segDeEnv (v); });
    auto& pS = afegeixPom (2, "Sustain", 0, 100, 100, false, txtPct, [] (const ParamsSampler& p) { return p.sosteniment * 100.0; }, [] (ParamsSampler& p, double v) { p.sosteniment = v / 100.0; });
    auto& pR = afegeixPom (2, "Release", 0, 1, envDeSeg (0.15), false, [] (double v) { return txtTemps (segDeEnv (v)); },
                           [] (const ParamsSampler& p) { return envDeSeg (p.alliberament); }, [] (ParamsSampler& p, double v) { p.alliberament = segDeEnv (v); });
    auto& pVel = afegeixPom (2, "Vel", 0, 100, 70, false, txtPct, [] (const ParamsSampler& p) { return p.vel * 100.0; }, [] (ParamsSampler& p, double v) { p.vel = v / 100.0; });
    auto& pXf = afegeixPom (2, "X-fade", 0, 1, xfDeSeg (0.02), false, [] (double v) { return txtTemps (segDeXf (v)); },
                            [] (const ParamsSampler& p) { return xfDeSeg (p.crossfade); }, [] (ParamsSampler& p, double v) { p.crossfade = segDeXf (v); });
    pVel.pom->setTooltip ("Quant canvia el volum amb la força de la tecla (0 %: sempre igual)"_ca);
    pXf.pom->setTooltip ("Crossfade al punt d'unió del bucle (perquè no faci clic)"_ca);
    pRoot.pom->setTooltip ("La nota que és el sample tal com està gravat: aquesta tecla el toca sense canviar el to"_ca);

    const int w = 54;
    fila1 = { { &selToc, 80, "Mode" }, { &selVeus, 74, "Mode" }, { pVeus.pom.get(), w, "Mode" },
              { pGain.pom.get(), w, "So" }, { pRoot.pom.get(), w, "So" }, { &btRoot, 46, "So" }, { &btDurFixa, 86, "So" }, { pBpm.pom.get(), w, "So" }, { &btAlTempo, 66, "So" },
              { pTune.pom.get(), w, "So" }, { pFine.pom.get(), w, "So" },
              { pSpeed.pom.get(), w, "Speed" }, { &btX2, 30, "Speed" }, { &btD2, 30, "Speed" }, { &btManteTo, 90, "Speed" }, { &btReversa, 68, "Speed" } };
    fila2 = { { pWidth.pom.get(), w, "Sortida" }, { pPan.pom.get(), w, "Sortida" }, { pVol.pom.get(), w, "Sortida" },
              { &selFiltre, 124, "Filtre" }, { &selPendent, 58, "Filtre" }, { pCut.pom.get(), w, "Filtre" }, { pReso.pom.get(), w, "Filtre" }, { pDrive.pom.get(), w, "Filtre" },
              { pA.pom.get(), w, "ADSR" }, { pD.pom.get(), w, "ADSR" }, { pS.pom.get(), w, "ADSR" }, { pR.pom.get(), w, "ADSR" }, { nullptr, 90, "ADSR" },
              { pVel.pom.get(), w, "Altres" }, { pXf.pom.get(), w, "Altres" }, { &btFlat, 50, "Altres" }, { &btSmono, 64, "Altres" } };
}

//==============================================================================
mibase::Pista* EditorSampler::pista()
{
    for (auto& t : estat.projecte.pistes)
        if (t.id == estat.sampler.pista && mibase::esSampler (t)) return &t;
    return nullptr;
}

const ParamsSampler* EditorSampler::params()
{
    auto* t = pista();
    return t != nullptr ? &t->sampler : nullptr;
}

void EditorSampler::canvia (const std::function<void (ParamsSampler&)>& f, const juce::String& evolucio)
{
    auto* t = pista();
    if (t == nullptr) { sincronitza(); return; }
    estat.desfes.abans (estat.projecte);
    f (t->sampler);
    estat.canviat (evolucio);
}

double EditorSampler::durada() const { return m != nullptr && m->fs > 0.0 ? (double) m->L.size() / m->fs : 0.0; }

void EditorSampler::refesPics()
{
    picMin.clear();
    picMax.clear();
    if (m == nullptr) return;
    const size_t n = m->L.size(), blocs = n / blocPic + 1;
    picMin.assign (blocs, 0.0f);
    picMax.assign (blocs, 0.0f);
    for (size_t i = 0; i < n; ++i)
    {
        const float v = m->R.empty() ? m->L[i] : 0.5f * (m->L[i] + (i < m->R.size() ? m->R[i] : 0.0f));
        const size_t b = i / blocPic;
        picMin[b] = std::min (picMin[b], v);
        picMax[b] = std::max (picMax[b], v);
    }
}

void EditorSampler::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (! estat.sampler.obert) return;
    sincronitza();
    repaint();
}

void EditorSampler::sincronitza()
{
    auto nova = mostra ? mostra() : nullptr;
    if (nova != m)
    {
        const bool primera = m == nullptr;
        m = nova;
        refesPics();
        if (primera || estat.sampler.pxPerSeg <= 0.0) encaixa();
    }
    const auto* p = params();
    if (p == nullptr) return;
    selBucle.setTriat (p->modeBucle);
    selToc.setTriat (p->modeToc);
    selVeus.setTriat (p->veus);
    selFiltre.setTriat (p->filtre);
    selPendent.setTriat (p->pendent == 24 ? 1 : 0);
    btZero.setToggleState (p->zeroCrossing, juce::dontSendNotification);
    btReversa.setToggleState (p->reversa, juce::dontSendNotification);
    btManteTo.setToggleState (p->mantenTo, juce::dontSendNotification);
    btDurFixa.setToggleState (p->durFixa, juce::dontSendNotification);
    btFlat.setToggleState (p->flat, juce::dontSendNotification);
    btSmono.setToggleState (p->smono, juce::dontSendNotification);
    for (auto& pp : poms)
        if (pp.pom.get() != pomActiu) pp.pom->setValor (pp.llegeix (*p), true);
}

//==============================================================================
void EditorSampler::encaixa()
{
    const double d = durada();
    if (d <= 0.0 || zOna.getWidth() <= 0) { estat.sampler.pxPerSeg = 0.0; return; }
    estat.sampler.iniciVista = 0.0;
    estat.sampler.pxPerSeg = zOna.getWidth() / d;
    repaint();
}

double EditorSampler::xASeg (float x) const
{
    const double px = estat.sampler.pxPerSeg > 0.0 ? estat.sampler.pxPerSeg : 100.0;
    return estat.sampler.iniciVista + (x - (float) zOna.getX()) / px;
}

float EditorSampler::segAX (double s) const
{
    const double px = estat.sampler.pxPerSeg > 0.0 ? estat.sampler.pxPerSeg : 100.0;
    return (float) (zOna.getX() + (s - estat.sampler.iniciVista) * px);
}

double EditorSampler::enganxa (double s) const
{
    auto* self = const_cast<EditorSampler*> (this);
    const auto* p = self->params();
    if (p == nullptr || ! p->zeroCrossing || m == nullptr) return s;
    return mibase::sampler::zeroCrossing (*m, s);
}

//==============================================================================
void EditorSampler::resized()
{
    auto r = getLocalBounds();
    zBarra = r.removeFromTop (32);
    zRegla = r.removeFromTop (18);
    zOna = r.reduced (0, 2);
    constexpr int margeOna = 16;   // l'inici i el final no queden mai a tocar de la vora (ni del separador del costat)

    auto b = zBarra.reduced (6, 4);
    btTanca.setBounds (b.removeFromRight (26));
    b.removeFromRight (6);
    b.removeFromLeft (juce::jmin (230, b.getWidth() / 6));   // títol
    auto posaD = [&b] (juce::Component& c, int w) { c.setBounds (b.removeFromRight (w)); b.removeFromRight (6); };
    posaD (btConsola, 92);
    posaD (btZero, 98);
    auto posa = [&b] (juce::Component& c, int w) { c.setBounds (b.removeFromLeft (w)); b.removeFromLeft (6); };
    posa (btCarrega, 92);
    posa (btPlay, 88);
    selBucle.setBounds (b.removeFromLeft (juce::jmin (360, b.getWidth())));
    btConsola.setToggleState (estat.sampler.consola, juce::dontSendNotification);

    // Consola: els grups es posen en files; un grup que no hi cap passa a la fila següent.
    grups.clear();
    std::vector<std::vector<std::vector<Lloc*>>> files (1);   // files → grups → llocs
    {
        std::vector<std::vector<Lloc*>> grupsTots;
        for (auto* llista : { &fila1, &fila2 })
            for (auto& l : *llista)
            {
                if (grupsTots.empty() || grupsTots.back().front()->grup != l.grup) grupsTots.push_back ({});
                grupsTots.back().push_back (&l);
            }
        const int ampleDisp = getWidth() - 16 - 10;
        int x = 0;
        for (auto& g : grupsTots)
        {
            int w = 0;
            for (auto* l : g) w += l->ample + 6;
            if (x > 0 && x + w > ampleDisp) { files.push_back ({}); x = 0; }
            files.back().push_back (g);
            x += w + 14;
        }
    }
    const int altFila = 74;
    const int altCal = (int) files.size() * altFila + 10;
    const int altVista = ! estat.sampler.consola ? 0
                         : estat.sampler.altConsola > 0 ? juce::jlimit (40, juce::jmax (40, getHeight() - 140), estat.sampler.altConsola)
                                                        : juce::jmin (altCal, juce::jmax (40, getHeight() - 140));
    zConsola = getLocalBounds().removeFromBottom (altVista);
    vistaConsola.setVisible (estat.sampler.consola);
    divConsola.setVisible (estat.sampler.consola);
    vistaConsola.setBounds (zConsola);
    divConsola.setBounds (zConsola.getX(), zConsola.getY() - 4, zConsola.getWidth(), 8);
    consola.setSize (zConsola.getWidth() - (altCal > altVista ? 8 : 0), altCal);
    zOna = juce::Rectangle<int>::leftTopRightBottom (zOna.getX(), zOna.getY(), zOna.getRight(), juce::jmax (zOna.getY() + 70, zConsola.getY() - (estat.sampler.consola ? 5 : 2)));
    zTeclat = zOna.removeFromBottom (30);
    zOna.removeFromBottom (2);
    zFonsOna = zOna;
    zOna = zOna.reduced (margeOna, 0);
    zTeclat = zTeclat.reduced (margeOna, 0);
    auto c = consola.getLocalBounds().reduced (8, 5);
    for (auto& fila : files)
    {
        auto zona = c.removeFromTop (altFila).withTrimmedTop (14);
        for (auto& g : fila)
        {
            juce::Rectangle<int> rg;
            for (auto* l : g)
            {
                auto cel = zona.removeFromLeft (l->ample);
                zona.removeFromLeft (6);
                rg = rg.isEmpty() ? cel : rg.getUnion (cel);
                if (l->c == nullptr) zAdsr = cel.reduced (2, 6);
                else if (dynamic_cast<Pom*> (l->c) != nullptr || dynamic_cast<OpcionsPetites*> (l->c) != nullptr) l->c->setBounds (cel);
                else l->c->setBounds (cel.withSizeKeepingCentre (cel.getWidth(), 24));
            }
            grups.push_back ({ g.front()->grup, rg });
            zona.removeFromLeft (14);
        }
    }
    if (estat.sampler.pxPerSeg <= 0.0) encaixa();
}

//==============================================================================
void EditorSampler::paint (juce::Graphics& g)
{
    g.fillAll (tema::fons);
    // barra
    g.setColour (tema::superficie);
    g.fillRect (zBarra);
    g.setColour (tema::linia);
    g.drawHorizontalLine (zBarra.getBottom() - 1, 0.0f, (float) getWidth());
    auto* t = pista();
    const auto* p = t != nullptr ? &t->sampler : nullptr;
    {
        auto tr = zBarra.reduced (10, 0).withWidth (juce::jmin (220, zBarra.getWidth() / 6 - 6));
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (12.5f, true));
        juce::String titol = dibuix::majuscules ("Samplejador"_ca);
        if (t != nullptr) titol += juce::String::fromUTF8 ("  ·  ") + dibuix::utf8 (t->nom);
        g.drawText (titol, tr.removeFromTop (zBarra.getHeight() / 2 + 2).withTrimmedTop (2), juce::Justification::bottomLeft, true);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.0f));
        g.drawText (p != nullptr && ! p->nom.empty() ? dibuix::utf8 (p->nom) + "  ·  "_ca + juce::String (durada(), 2) + " s" : "Cap sample"_ca, tr,
                    juce::Justification::topLeft, true);
    }

    // regla
    g.setColour (tema::superficie);
    g.fillRect (zRegla);
    const double dur = durada();
    const double px = estat.sampler.pxPerSeg > 0.0 ? estat.sampler.pxPerSeg : 100.0;
    {
        double pas = 0.001;
        for (double c : { 0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 30.0, 60.0 })
            if (c * px >= 70.0) { pas = c; break; } else pas = c;
        g.setFont (dibuix::mono (10.0f));
        const double s0 = std::floor (xASeg ((float) zOna.getX()) / pas) * pas;
        for (double s = juce::jmax (0.0, s0); s <= xASeg ((float) zOna.getRight()) && s <= dur + pas; s += pas)
        {
            const float x = segAX (s);
            g.setColour (tema::linia2);
            g.drawVerticalLine ((int) x, (float) zRegla.getY() + 9.0f, (float) zRegla.getBottom());
            g.setColour (tema::suau);
            g.drawText (pas < 1.0 ? juce::String (s, pas < 0.01 ? 3 : pas < 0.1 ? 2 : 1) : juce::String ((int) std::lround (s)) + " s",
                        (int) x + 3, zRegla.getY(), 60, 12, juce::Justification::centredLeft);
        }
        if (p != nullptr && p->bpm > 0.0)   // temps del sample
        {
            const double temps = 60.0 / p->bpm;
            if (temps * px > 6.0)
                for (double s = 0.0; s <= dur; s += temps)
                {
                    g.setColour (tema::guitarra.withAlpha (0.35f));
                    g.drawVerticalLine ((int) segAX (s), (float) zRegla.getBottom() - 5.0f, (float) zRegla.getBottom());
                }
        }
    }

    if (m != nullptr)   // el capçal, a la regla
    {
        const float x = segAX (estat.sampler.capcal);
        if (x >= (float) zFonsOna.getX() && x <= (float) zFonsOna.getRight())
        {
            juce::Path tri;
            tri.addTriangle (x - 5.0f, (float) zRegla.getY() + 7.0f, x + 5.0f, (float) zRegla.getY() + 7.0f, x, (float) zRegla.getBottom());
            g.setColour (tema::text);
            g.fillPath (tri);
        }
    }

    // ona
    g.setColour (juce::Colour (0xff111115));
    g.fillRect (zFonsOna);
    pintaTeclat (g);
    if (m == nullptr || p == nullptr)
    {
        g.setColour (sobreFitxer ? tema::accent : tema::suau);
        g.setFont (dibuix::lletra (13.0f));
        g.drawFittedText (t == nullptr ? "Aquesta pista no té el samplejador"_ca
                          : (p != nullptr && ! p->fitxer.empty() ? "No trobo el fitxer «"_ca + dibuix::utf8 (p->fitxer) + "». Carrega'l de nou."_ca
                                                                : "Carrega un sample amb «Carrega…» o arrossega'l aquí des de l'explorador de fitxers"_ca),
                          zOna.reduced (20), juce::Justification::centred, 2);
        g.setColour (tema::linia);
        g.fillRect (zConsola.getX(), zConsola.getY(), zConsola.getWidth(), 1);
        return;
    }
    const double ini = juce::jlimit (0.0, dur, p->inici), fi = p->fi < 0.0 ? dur : juce::jlimit (ini, dur, p->fi);
    const float mig = (float) zOna.getCentreY(), mitjaAlt = zOna.getHeight() * 0.46f;
    const auto col = EstatEstudi::colorPista (*t);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (zOna);
        g.setColour (tema::linia);
        g.drawHorizontalLine ((int) mig, (float) zOna.getX(), (float) zOna.getRight());
        // bucle (fons)
        const bool bucle = p->bucleDe >= 0.0 && p->bucleA > p->bucleDe;
        if (bucle)
        {
            g.setColour (tema::guitarra.withAlpha (p->modeBucle > 0 ? 0.13f : 0.05f));
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (segAX (p->bucleDe), (float) zOna.getY(), segAX (p->bucleA), (float) zOna.getBottom()));
        }
        const int x0 = juce::jmax (zOna.getX(), (int) segAX (0.0)), x1 = juce::jmin (zOna.getRight(), (int) segAX (dur) + 1);
        const auto n = (long long) m->L.size();
        for (int x = x0; x < x1; ++x)
        {
            const double sa = xASeg ((float) x), sb = xASeg ((float) x + 1.0f);
            long long ia = (long long) (sa * m->fs), ib = juce::jmax (ia + 1, (long long) (sb * m->fs));
            ia = juce::jlimit (0LL, n - 1, ia);
            ib = juce::jlimit (ia + 1, n, ib);
            float mn = 0.0f, mx = 0.0f;
            if (ib - ia > blocPic * 2)
            {
                mn = 1.0f; mx = -1.0f;
                for (long long b = ia / blocPic; b <= (ib - 1) / blocPic && b < (long long) picMin.size(); ++b) { mn = std::min (mn, picMin[(size_t) b]); mx = std::max (mx, picMax[(size_t) b]); }
            }
            else
            {
                mn = 1.0f; mx = -1.0f;
                for (long long i = juce::jmax (0LL, ia - 1); i <= ib && i < n; ++i)
                {
                    const float v = m->R.empty() ? m->L[(size_t) i] : 0.5f * (m->L[(size_t) i] + m->R[(size_t) i]);
                    mn = std::min (mn, v); mx = std::max (mx, v);
                }
            }
            const double s = (sa + sb) * 0.5;
            const bool dins = s >= ini && s <= fi;
            g.setColour (dins ? col : col.withAlpha (0.3f));
            g.drawVerticalLine (x, mig - mx * mitjaAlt, juce::jmax (mig - mx * mitjaAlt + 1.0f, mig - mn * mitjaAlt));
        }
        // fora del fragment, més fosc
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom ((float) zOna.getX(), (float) zOna.getY(), segAX (ini), (float) zOna.getBottom()));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (segAX (fi), (float) zOna.getY(), (float) zOna.getRight(), (float) zOna.getBottom()));
        // fades
        g.setColour (tema::text.withAlpha (0.55f));
        const float yT = (float) zOna.getY() + 3.0f, yB = (float) zOna.getBottom() - 1.0f;
        const float xfi = segAX (ini + p->fadeIn), xfo = segAX (fi - p->fadeOut);
        g.drawLine (segAX (ini), yB, xfi, yT, 1.2f);
        g.drawLine (xfo, yT, segAX (fi), yB, 1.2f);
        auto triangle = [&g] (float x, float y, bool cap)
        {
            juce::Path tri;
            tri.addTriangle (x - 6.0f, y, x + 6.0f, y, x, y + 9.0f);
            g.setColour (cap ? tema::text : tema::textMig);
            g.fillPath (tri);
        };
        triangle (xfi, (float) zOna.getY(), arrossegant == Marca::FadeIn);
        triangle (xfo, (float) zOna.getY(), arrossegant == Marca::FadeOut);
        // marcadors
        auto marcador = [&] (double s, juce::Colour c, const juce::String& text, bool dalt, bool dreta)
        {
            const float x = segAX (s);
            g.setColour (c);
            g.fillRect (x - 0.75f, (float) zOna.getY(), 1.5f, (float) zOna.getHeight());
            g.setFont (dibuix::lletra (9.5f, true));
            const int w = (int) juce::GlyphArrangement::getStringWidth (dibuix::lletra (9.5f, true), text) + 8;
            juce::Rectangle<float> et ((dreta ? x : x - (float) w), dalt ? (float) zOna.getY() + 12.0f : (float) zOna.getBottom() - 15.0f, (float) w, 14.0f);
            g.fillRoundedRectangle (et, 2.0f);
            g.setColour (juce::Colours::black);
            g.drawText (text, et, juce::Justification::centred);
        };
        marcador (ini, tema::accent, "INICI", false, true);
        marcador (fi, tema::accent, "FINAL", false, false);
        if (bucle)
        {
            const auto cb = p->modeBucle > 0 ? tema::guitarra : tema::suau;
            marcador (p->bucleDe, cb, "BUCLE", true, true);
            marcador (p->bucleA, cb, "FI BUCLE", true, false);
        }
        // el capçal (d'on s'escolta l'original) i el que sona
        g.setColour (tema::text.withAlpha (0.9f));
        g.fillRect (segAX (estat.sampler.capcal) - 0.75f, (float) zOna.getY(), 1.5f, (float) zOna.getHeight());
        if (posSo >= 0.0)
        {
            g.setColour (tema::guitarra.withAlpha (0.85f));
            g.fillRect (segAX (posSo) - 0.5f, (float) zOna.getY(), 1.0f, (float) zOna.getHeight());
        }
        if (sobreFitxer)
        {
            g.setColour (tema::accent);
            g.drawRect (zOna, 2);
        }
    }

}

void EditorSampler::pintaConsola (juce::Graphics& g)
{
    g.fillAll (tema::superficie);
    g.setColour (tema::linia);
    g.fillRect (0, 0, consola.getWidth(), 1);
    g.setFont (dibuix::lletra (9.5f, true));
    for (auto& [nom, rg] : grups)
    {
        auto e = juce::Rectangle<int> (rg.getX(), rg.getY() - 14, rg.getWidth(), 12);
        g.setColour (tema::linia2);
        g.drawHorizontalLine (e.getCentreY(), (float) e.getX(), (float) e.getRight());
        const int w = (int) juce::GlyphArrangement::getStringWidth (dibuix::lletra (9.5f, true), dibuix::majuscules (nom)) + 10;
        auto et = e.withSizeKeepingCentre (w, 12);
        g.setColour (tema::superficie);
        g.fillRect (et);
        g.setColour (tema::suau);
        g.drawText (dibuix::majuscules (nom), et, juce::Justification::centred);
    }
    if (! zAdsr.isEmpty()) pintaAdsr (g, zAdsr.toFloat());
}

void EditorSampler::pintaAdsr (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto* p = params();
    if (p == nullptr) return;
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r, 3.0f);
    r = r.reduced (5.0f);
    const double a = p->atac, d = p->decai, rel = p->alliberament;
    const double sost = juce::jmax (0.15, (a + d + rel) * 0.35);
    const double total = juce::jmax (0.001, a + d + sost + rel);
    auto x = [&] (double t) { return r.getX() + (float) (t / total) * r.getWidth(); };
    auto y = [&] (double v) { return r.getBottom() - (float) v * r.getHeight(); };
    juce::Path c;
    c.startNewSubPath (x (0.0), y (0.0));
    c.lineTo (x (a), y (1.0));
    c.lineTo (x (a + d), y (p->sosteniment));
    c.lineTo (x (a + d + sost), y (p->sosteniment));
    c.lineTo (x (total), y (0.0));
    g.setColour (tema::guitarra);
    g.strokePath (c, juce::PathStrokeType (1.6f));
    g.setColour (tema::guitarra.withAlpha (0.12f));
    c.lineTo (x (0.0), y (0.0));
    c.closeSubPath();
    g.fillPath (c);
}

//==============================================================================
EditorSampler::Marca EditorSampler::marcaA (juce::Point<float> q) const
{
    auto* self = const_cast<EditorSampler*> (this);
    const auto* p = self->params();
    if (p == nullptr || m == nullptr) return Marca::Cap;
    if (zRegla.toFloat().contains (q)) return Marca::NouBucle;
    // primer, les pestanyetes (INICI, FINAL, BUCLE, FI BUCLE)
    for (auto mk : { Marca::BucleA, Marca::BucleDe, Marca::Fi, Marca::Inici })
        if (etiqueta (mk).expanded (2.0f).contains (q)) return mk;
    if (! zFonsOna.toFloat().contains (q)) return Marca::Cap;
    const double dur = durada();
    const double ini = p->inici, fi = p->fi < 0.0 ? dur : p->fi;
    auto prop = [&] (double s, float tol) { return std::abs (segAX (s) - q.x) <= tol; };
    if (q.y < zOna.getY() + 14.0f)
    {
        if (prop (ini + p->fadeIn, 7.0f)) return Marca::FadeIn;
        if (prop (fi - p->fadeOut, 7.0f)) return Marca::FadeOut;
    }
    if (p->bucleDe >= 0.0 && p->bucleA > p->bucleDe)
    {
        if (prop (p->bucleA, 5.0f)) return Marca::BucleA;
        if (prop (p->bucleDe, 5.0f)) return Marca::BucleDe;
    }
    if (prop (fi, 5.0f)) return Marca::Fi;
    if (prop (ini, 5.0f)) return Marca::Inici;
    return Marca::Escolta;
}

void EditorSampler::mouseMove (const juce::MouseEvent& e)
{
    const auto mk = marcaA (e.position);
    if (notaA (e.position) >= 0) { setMouseCursor (juce::MouseCursor::PointingHandCursor); return; }
    setMouseCursor (mk == Marca::Escolta ? juce::MouseCursor::PointingHandCursor
                    : mk == Marca::Cap ? juce::MouseCursor::NormalCursor
                    : mk == Marca::NouBucle ? juce::MouseCursor::IBeamCursor
                    : juce::MouseCursor::LeftRightResizeCursor);
}

juce::Rectangle<float> EditorSampler::etiqueta (Marca mk) const
{
    auto* self = const_cast<EditorSampler*> (this);
    const auto* p = self->params();
    if (p == nullptr || m == nullptr) return {};
    const double dur = durada();
    const bool bucle = p->bucleDe >= 0.0 && p->bucleA > p->bucleDe;
    double s = 0.0;
    bool dalt = false, dreta = true;
    juce::String text;
    switch (mk)
    {
        case Marca::Inici:   s = p->inici; text = "INICI"; break;
        case Marca::Fi:      s = p->fi < 0.0 ? dur : p->fi; text = "FINAL"; dreta = false; break;
        case Marca::BucleDe: if (! bucle) return {}; s = p->bucleDe; text = "BUCLE"; dalt = true; break;
        case Marca::BucleA:  if (! bucle) return {}; s = p->bucleA; text = "FI BUCLE"; dalt = true; dreta = false; break;
        case Marca::Cap: case Marca::FadeIn: case Marca::FadeOut: case Marca::NouBucle: case Marca::Escolta: return {};
    }
    const float x = segAX (s);
    const float w = (float) juce::GlyphArrangement::getStringWidth (dibuix::lletra (9.5f, true), text) + 8.0f;
    return { dreta ? x : x - w, dalt ? (float) zOna.getY() + 12.0f : (float) zOna.getBottom() - 15.0f, w, 14.0f };
}

//==============================================================================
juce::Rectangle<float> EditorSampler::rectTecla (int nota) const
{
    auto blanca = [] (int n) { const int c = n % 12; return c == 0 || c == 2 || c == 4 || c == 5 || c == 7 || c == 9 || c == 11; };
    int blanques = 0;
    for (int n = notaMin; n <= notaMax; ++n) if (blanca (n)) ++blanques;
    const float ww = (float) zTeclat.getWidth() / (float) juce::jmax (1, blanques);
    int idx = 0;
    for (int n = notaMin; n < nota; ++n) if (blanca (n)) ++idx;
    const auto z = zTeclat.toFloat();
    if (blanca (nota)) return { z.getX() + idx * ww, z.getY(), ww, z.getHeight() };
    const float bw = ww * 0.62f;
    return { z.getX() + idx * ww - bw * 0.5f, z.getY(), bw, z.getHeight() * 0.6f };
}

int EditorSampler::notaA (juce::Point<float> q) const
{
    if (! zTeclat.toFloat().contains (q)) return -1;
    for (int n = notaMin; n <= notaMax; ++n)
        if (const int c = n % 12; (c == 1 || c == 3 || c == 6 || c == 8 || c == 10) && rectTecla (n).contains (q)) return n;
    for (int n = notaMin; n <= notaMax; ++n)
        if (rectTecla (n).contains (q)) return n;
    return -1;
}

void EditorSampler::pintaTeclat (juce::Graphics& g)
{
    const auto* p = params();
    const int root = p != nullptr ? p->root : -1;
    auto sona = [this] (int n)
    {
        return n == notaRatoli;
    };
    auto pinta = [&] (bool negres)
    {
        for (int n = notaMin; n <= notaMax; ++n)
        {
            const int c = n % 12;
            const bool negra = c == 1 || c == 3 || c == 6 || c == 8 || c == 10;
            if (negra != negres) continue;
            auto r = rectTecla (n);
            juce::Colour col = negra ? juce::Colour (0xff26262c) : juce::Colour (0xffd9d7de);
            if (n == root) col = tema::accent;
            if (sona (n)) col = tema::guitarra;
            g.setColour (col);
            g.fillRect (r.reduced (negra ? 0.0f : 0.5f, 0.0f));
            if (! negra)
            {
                g.setColour (juce::Colours::black.withAlpha (0.45f));
                g.drawVerticalLine ((int) r.getRight(), r.getY(), r.getBottom());
            }
            if (n == root || (c == 0 && ! negra))
            {
                g.setColour (n == root ? juce::Colours::black : juce::Colour (0xff6a6873));
                g.setFont (dibuix::lletra (n == root ? 9.5f : 8.5f, n == root));
                const auto txt = nomNota (n);
                const float tw = (float) juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), txt) + 4.0f;
                g.drawText (txt, r.withSizeKeepingCentre (juce::jmax (r.getWidth(), tw), r.getHeight()).removeFromBottom (12.0f), juce::Justification::centred, false);
            }
        }
    };
    pinta (false);
    pinta (true);
}

void EditorSampler::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    arrossegant = Marca::Cap;
    desfesFet = false;
    if (const int n = notaA (e.position); n >= 0)
    {
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu menu;
            menu.addItem (1, "Fes que " + nomNota (n) + " sigui la Root (la nota original del sample)"_ca);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this, n] (int r)
            {
                if (r == 1) canvia ([n] (ParamsSampler& q) { q.root = n; }, "Samplejador: nota original "_ca + nomNota (n));
            });
            return;
        }
        notaRatoli = n;
        const int vel = juce::jlimit (30, 127, 50 + (int) (77.0f * (e.position.y - (float) zTeclat.getY()) / (float) juce::jmax (1, zTeclat.getHeight())));
        if (onNota) onNota (0x90, (std::uint8_t) n, (std::uint8_t) vel);
        repaint (zTeclat);
        return;
    }
    const auto mk = marcaA (e.position);
    if (mk == Marca::Cap) return;
    segPremut = juce::jlimit (0.0, durada(), xASeg (e.position.x));
    if (e.mods.isPopupMenu()) { if (zOna.contains (e.getPosition())) menuOna (segPremut); return; }
    if (mk == Marca::Escolta)
    {
        // el capçal va on cliques; si ja estava escoltant, continua des d'aquí
        estat.sampler.capcal = segPremut;
        if (escoltant && onEscolta) onEscolta (segPremut);
        repaint();
        return;
    }
    arrossegant = mk;
}

void EditorSampler::mouseDrag (const juce::MouseEvent& e)
{
    if (notaRatoli >= 0)
    {
        const int n = notaA (e.position);
        if (n >= 0 && n != notaRatoli)
        {
            if (onNota) { onNota (0x80, (std::uint8_t) notaRatoli, 0); onNota (0x90, (std::uint8_t) n, 100); }
            notaRatoli = n;
            repaint (zTeclat);
        }
        return;
    }
    if (arrossegant == Marca::Cap) return;
    auto* t = pista();
    if (t == nullptr) return;
    const double dur = durada();
    const double s = juce::jlimit (0.0, dur, xASeg (e.position.x));
    if (! desfesFet) { estat.desfes.abans (estat.projecte); desfesFet = true; }
    auto& p = t->sampler;
    const double fi = p.fi < 0.0 ? dur : p.fi;
    switch (arrossegant)
    {
        case Marca::Inici:   p.inici = juce::jlimit (0.0, juce::jmax (0.0, fi - 0.002), enganxa (s)); break;
        case Marca::Fi:      p.fi = juce::jlimit (p.inici + 0.002, dur, enganxa (s)); if (p.fi >= dur - 1e-6) p.fi = -1.0; break;
        case Marca::BucleDe: p.bucleDe = juce::jlimit (p.inici, p.bucleA - 0.002, enganxa (s)); break;
        case Marca::BucleA:  p.bucleA = juce::jlimit (p.bucleDe + 0.002, fi, enganxa (s)); break;
        case Marca::FadeIn:  p.fadeIn = juce::jlimit (0.0, (fi - p.inici) * 0.5, s - p.inici); break;
        case Marca::FadeOut: p.fadeOut = juce::jlimit (0.0, (fi - p.inici) * 0.5, fi - s); break;
        case Marca::NouBucle:
        {
            const double a = juce::jmin (segPremut, s), b = juce::jmax (segPremut, s);
            if (b - a < 0.005) break;
            p.bucleDe = juce::jlimit (p.inici, fi, enganxa (a));
            p.bucleA = juce::jlimit (p.bucleDe + 0.002, fi, enganxa (b));
            if (p.modeBucle == 0) p.modeBucle = 3;
            break;
        }
        case Marca::Escolta: case Marca::Cap: break;
    }
    estat.avisa();
}

void EditorSampler::mouseUp (const juce::MouseEvent& e)
{
    if (arrossegant == Marca::NouBucle && ! desfesFet)   // clic a la regla (sense arrossegar): el capçal va aquí
    {
        estat.sampler.capcal = juce::jlimit (0.0, durada(), xASeg (e.position.x));
        if (escoltant && onEscolta) onEscolta (estat.sampler.capcal);
        arrossegant = Marca::Cap;
        repaint();
        return;
    }
    if (notaRatoli >= 0)
    {
        if (onNota) onNota (0x80, (std::uint8_t) notaRatoli, 0);
        notaRatoli = -1;
        repaint (zTeclat);
        return;
    }
    if (desfesFet)
    {
        static const char* noms[] = { "", "inici", "final", "bucle", "fi del bucle", "fade d'entrada", "fade de sortida", "bucle", "" };
        estat.canviat ("Samplejador: "_ca + juce::String::fromUTF8 (noms[(int) arrossegant]));
    }
    desfesFet = false;
    arrossegant = Marca::Cap;
    repaint();
}

void EditorSampler::menuOna (double s)
{
    juce::PopupMenu menu;
    menu.addItem (1, "Inici aquí"_ca);
    menu.addItem (2, "Final aquí"_ca);
    menu.addItem (3, "Bucle des d'aquí"_ca);
    menu.addItem (4, "Fi del bucle aquí"_ca);
    const auto* p = params();
    menu.addItem (5, "Treu el bucle"_ca, p != nullptr && p->bucleDe >= 0.0);
    menu.addSeparator();
    menu.addItem (6, "Tot el sample (inici i final als extrems)"_ca);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this, s] (int r)
    {
        if (r <= 0) return;
        const double dur = durada(), z = enganxa (s);
        canvia ([r, z, dur] (ParamsSampler& q)
        {
            const double fi = q.fi < 0.0 ? dur : q.fi;
            if (r == 1) q.inici = juce::jmin (z, fi - 0.002);
            else if (r == 2) q.fi = juce::jmax (z, q.inici + 0.002);
            else if (r == 3) { q.bucleDe = juce::jlimit (q.inici, fi, z); if (q.bucleA <= q.bucleDe) q.bucleA = fi; if (q.modeBucle == 0) q.modeBucle = 3; }
            else if (r == 4) { q.bucleA = juce::jlimit (q.inici, fi, z); if (q.bucleDe < 0.0 || q.bucleDe >= q.bucleA) q.bucleDe = q.inici; if (q.modeBucle == 0) q.modeBucle = 3; }
            else if (r == 5) { q.bucleDe = q.bucleA = -1.0; }
            else if (r == 6) { q.inici = 0.0; q.fi = -1.0; }
        }, "Samplejador: marcadors"_ca);
    });
}

void EditorSampler::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const double dur = durada();
    if (dur <= 0.0) return;
    auto& v = estat.sampler;
    if (v.pxPerSeg <= 0.0) encaixa();
    const double visibles = zOna.getWidth() / v.pxPerSeg;
    if (e.mods.isCommandDown() || e.mods.isCtrlDown())
    {
        const float d = std::abs (w.deltaY) >= std::abs (w.deltaX) ? w.deltaY : w.deltaX;
        if (std::abs (d) < 1.0e-6f) return;
        const double sRatoli = xASeg (e.position.x);
        const double minim = zOna.getWidth() / dur * 0.5;
        v.pxPerSeg = juce::jlimit (minim, 200000.0, v.pxPerSeg * (d > 0.0f ? 1.25 : 1.0 / 1.25));
        v.iniciVista = sRatoli - (e.position.x - zOna.getX()) / v.pxPerSeg;
    }
    else
    {
        const float d = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
        v.iniciVista -= d * visibles * 0.4;   // roda (o Maj + roda): de costat
    }
    const double visiblesAra = zOna.getWidth() / v.pxPerSeg;
    v.iniciVista = juce::jlimit (juce::jmin (0.0, dur - visiblesAra), juce::jmax (0.0, dur - visiblesAra * 0.5), v.iniciVista);
    repaint();
}

//==============================================================================

bool EditorSampler::keyPressed (const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
    const int codi = k.getKeyCode();
    // Z = tot el sample; Maj + Z = el fragment (de l'inici al final), com a la resta d'editors
    if (! mods.isCommandDown() && ! mods.isAltDown() && (codi == 'Z' || codi == 'z'))
    {
        if (! mods.isShiftDown()) { encaixa(); return true; }
        if (const auto* p = params(); p != nullptr && durada() > 0.0)
        {
            const double a = p->inici, b = p->fi < 0.0 ? durada() : p->fi;
            estat.sampler.pxPerSeg = zOna.getWidth() / juce::jmax (0.002, (b - a) * 1.04);
            estat.sampler.iniciVista = a - (b - a) * 0.02;
            repaint();
        }
        return true;
    }
    if (k == juce::KeyPress::escapeKey && escoltant) { if (onEscolta) onEscolta (-1.0); escoltant = false; return true; }
    return onTeclaNoUsada ? onTeclaNoUsada (k) : false;
}



//==============================================================================
bool EditorSampler::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;mp3;ogg;flac;aif;aiff")) return pista() != nullptr;
    return false;
}

void EditorSampler::filesDropped (const juce::StringArray& files, int, int)
{
    sobreFitxer = false;
    repaint();
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;mp3;ogg;flac;aif;aiff")) { if (onFitxer) onFitxer (juce::File (f)); return; }
}

void EditorSampler::commutaEscolta()
{
    if (escoltant) { if (onEscolta) onEscolta (-1.0); escoltant = false; }
    else if (m != nullptr) { if (onEscolta) onEscolta (estat.sampler.capcal); escoltant = true; ticsSenseSo = 0; }
    btPlay.setButtonText (escoltant ? "Atura" : "Escolta");
    btPlay.setIcona (escoltant ? dibuix::Icona::Atura : dibuix::Icona::Play);
}

void EditorSampler::tic()
{
    double s = -1.0;
    if (posicioSo) s = posicioSo();
    if (std::abs (s - posSo) > 1e-5) { posSo = s; repaint (zFonsOna); }
    // l'escolta s'ha acabat sola (final del fitxer)
    if (escoltant && s < 0.0 && ++ticsSenseSo > 6) { escoltant = true; commutaEscolta(); }
    if (s >= 0.0) ticsSenseSo = 0;
}
