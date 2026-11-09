#include "EditorVeu.h"
#include "Inserts.h"

#include "mibase/Edicio.h"
#include "mibase/Harmonia.h"
#include "mibase/Midi.h"

namespace vv = mibase::veu;
using mibase::NotaVeu;

namespace
{
    /** Color d'una nota segons com d'afinada queda (respecte a la nota més propera). */
    juce::Colour colorAfinacio (double centreFinal)
    {
        const double c = std::abs (centreFinal - std::round (centreFinal)) * 100.0;
        if (c < 8.0) return tema::sinte;          // afinada
        if (c < 25.0) return tema::guitarra;      // una mica
        return tema::accent;                      // desafinada
    }

    juce::String num (double v, int dec)
    {
        auto s = juce::String (v, dec).replaceCharacter ('.', ',');
        return s;
    }
}

juce::String EditorVeu::nomEina (int e)
{
    switch (e)
    {
        case Fletxa:  return "Afinació: arrossega les notes amunt i avall (s'enganxen als semitons; Alt = lliure). Doble clic = afina-la."_ca;
        case Vibrato: return "Vibrato: arrossega amunt per més vibrato, avall per menys. Doble clic = l'original."_ca;
        case Deriva:  return "Deriva: arrossega avall per aplanar la nota (sense tocar el vibrato). Doble clic = l'original."_ca;
        case Formant: return "Formant: arrossega per canviar el timbre sense canviar la nota. Doble clic = l'original."_ca;
        case Volum:   return "Volum: arrossega per pujar o baixar el volum de la nota. Doble clic = l'original."_ca;
        default: return {};
    }
}

juce::String EditorVeu::nomNota (double midi)
{
    const int n = (int) std::lround (midi);
    return juce::String::fromUTF8 (mibase::harmonia::nomTo (((n % 12) + 12) % 12).c_str()) + juce::String (n / 12 - 1);
}

juce::String EditorVeu::textCents (double c)
{
    const int v = (int) std::lround (c);
    return (v > 0 ? "+" : "") + juce::String (v) + " cents";
}

EditorVeu::EditorVeu (EstatEstudi& e, ServeiVeu& s) : SeccioEstudi (e), servei (s),
    pCorreccio ("Correcció"_ca, 0.0, 100.0, 100.0, false, [] (double v) { return juce::String ((int) std::lround (v)) + " %"; }),
    pVibrato ("Vibrato", 0.0, 200.0, 100.0, false, [] (double v) { return juce::String ((int) std::lround (v)) + " %"; }),
    pRecta ("Treu deriva", 0.0, 100.0, 0.0, false, [] (double v) { return juce::String ((int) std::lround (v)) + " %"; }),
    pFormant ("Formant", -12.0, 12.0, 0.0, true, [] (double v) { return (v > 0.04 ? "+" : "") + num (v, 1) + " st"; }),
    pVolum ("Volum", -24.0, 12.0, 0.0, true, [] (double v) { return (v > 0.04 ? "+" : "") + num (v, 1) + " dB"; }),
    pTransicio ("Transició"_ca, 0.0, 300.0, 60.0, false, [] (double v) { return juce::String ((int) std::lround (v)) + " ms"; })
{
    setWantsKeyboardFocus (true);
    for (int i = 0; i < numEines; ++i)
    {
        auto& b = btEines[i];
        addAndMakeVisible (b);
        b.setTooltip (nomEina (i) + " (tecla "_ca + juce::String (i + 1) + ")");
        b.onClick = [this, i] { estat.veu.eina = i; actualitzaBotons(); repaint(); };
    }
    for (auto* b : { &btOna, &btAB, &btAjusta, &btTanca }) addAndMakeVisible (b);
    btOna.setTooltip ("Mostra l'ona de la gravació de fons"_ca);
    btOna.onClick = [this] { estat.veu.ona = ! estat.veu.ona; actualitzaBotons(); repaint(); };
    btAB.setTooltip ("Encès: sona la veu corregida. Apagat: sona l'original (comparació A/B). Tecla B."_ca);
    btAB.onClick = [this] { if (onCommutaAB) onCommutaAB(); };

    btSolo.setLlum (vintage::ledGroc);
    btSolo.setTooltip ("Solo: només sona aquesta pista; en apagar-lo, les pistes tornen com estaven (tecla * del teclat numèric)"_ca);
    btSolo.onClick = [this] { if (onSolo) onSolo(); };
    addAndMakeVisible (btSolo);
    selMode.setLlum (vintage::ledTaronja);
    selMode.setTooltip ("Visual: les notes es mouen per semitons. Assistit: salten per les notes de la tonalitat (Alt = lliure). "
                        "Bloquejat: només notes de la tonalitat. Cada pista té el seu."_ca);
    selMode.onCanvi = [this] (int i)
    {
        estat.desfes.abans (estat.projecte);
        if (auto* t = estat.pista (estat.veu.pista)) t->modeEscala = i;   // 0.25: el de la pista
        else estat.projecte.modeEscala = (mibase::ModeEscala) i;
        estat.info = i == 0 ? "Escala: només es marca"_ca
                   : i == 1 ? "Escala assistida: les notes salten per la tonalitat (Alt = lliure)"_ca
                            : "Escala bloquejada: només notes de la tonalitat"_ca;
        estat.canviat();
    };
    addAndMakeVisible (selMode);

    // Consola.
    for (auto* b : { &btTo, &btEscalaTon, &btTotes, &btCap, &btCorregeix, &btOriginal, &btDesfes, &btRefes }) addAndMakeVisible (b);
    btTo.setTooltip ("La nota de la tonalitat (les notes es mouen dins d'aquesta tonalitat)"_ca);
    btTo.onClick = [this] { menuTo(); };
    btEscalaTon.setTooltip ("L'escala: la de la cançó, cromàtica (tots els semitons) o una de concreta"_ca);
    btEscalaTon.onClick = [this] { menuEscala(); };
    btTotes.onClick = [this]
    {
        if (auto* p = presa()) { estat.veu.triades.clear(); for (int k = 0; k < (int) p->veu.notes.size(); ++k) estat.veu.triades.push_back (k); estat.avisa(); }
    };
    btCap.onClick = [this] { estat.veu.triades.clear(); estat.avisa(); };
    btCorregeix.setTooltip ("Afina del tot les notes triades (o totes) a la nota de la tonalitat més propera"_ca);
    btCorregeix.onClick = [this]
    {
        auto* p = presa();
        if (p == nullptr) return;
        const auto ns = notesConsola();
        comencaCanvi();
        restaura();
        p = presa();
        if (p == nullptr) return;
        for (int k : ns)
        {
            auto& n = p->veu.notes[(size_t) k];
            n.desplac = objectiu (vv::centreFinal (n)) - n.centre;
        }
        acabaCanvi ("Vocal Editor: notes afinades"_ca);
    };
    btOriginal.setTooltip ("Torna les notes triades (o totes) a com es van cantar"_ca);
    btOriginal.onClick = [this]
    {
        const auto ns = notesConsola();
        comencaCanvi();
        restaura();
        if (auto* p = presa()) vv::restaura (p->veu, ns);
        acabaCanvi ("Vocal Editor: notes a l'original"_ca);
    };
    btDesfes.onClick = [this] { if (onDesfes) onDesfes (false); };
    btRefes.onClick = [this] { if (onDesfes) onDesfes (true); };

    auto pct = [] (double v) { return juce::String ((int) std::lround (v)) + " %"; };
    auto pom = [this] (Pom& p, const juce::String& ajuda)
    {
        addAndMakeVisible (p);
        p.setHelpText (ajuda);
        p.onComenca = [this] { comencaPom(); };
        p.onAcaba = [this] { acabaCanvi ("Vocal Editor: notes editades"_ca); };
    };
    pom (pCorreccio, {}); pom (pVibrato, {}); pom (pRecta, {}); pom (pFormant, {}); pom (pVolum, {});
    (void) pct;
    pCorreccio.onCanvi = [this] (double v)
    {
        canviaPom ([this, v] (NotaVeu& n, size_t i) { if (i < objectiusInici.size()) n.desplac = (objectiusInici[i] - n.centre) * v / 100.0; });
    };
    pVibrato.onCanvi = [this] (double v) { canviaPom ([v] (NotaVeu& n, size_t) { n.modulacio = v / 100.0; }); };
    pRecta.onCanvi = [this] (double v) { canviaPom ([v] (NotaVeu& n, size_t) { n.deriva = 1.0 - v / 100.0; }); };
    pFormant.onCanvi = [this] (double v) { canviaPom ([v] (NotaVeu& n, size_t) { n.formant = v; }); };
    pVolum.onCanvi = [this] (double v) { canviaPom ([v] (NotaVeu& n, size_t) { n.guanyDb = v; }); };
    addAndMakeVisible (pTransicio);
    pTransicio.onComenca = [this] { comencaCanvi(); };
    pTransicio.onCanvi = [this] (double v)
    {
        restaura();
        if (auto* p = presa()) p->veu.transicio = v / 1000.0;
        estat.avisa();
    };
    pTransicio.onAcaba = [this] { acabaCanvi ("Vocal Editor: transició entre notes"_ca); };
    btAjusta.setTooltip ("Encaixa la vista a les notes"_ca);
    btAjusta.onClick = [this] { ajustaVista(); };
    btTanca.setTooltip ("Amaga el Vocal Editor"_ca);
    btTanca.onClick = [this] { if (onTanca) onTanca(); };
    actualitzaBotons();
}

void EditorVeu::actualitzaBotons()
{
    for (int i = 0; i < numEines; ++i) btEines[i].setToggleState (estat.veu.eina == i, juce::dontSendNotification);
    btOna.setToggleState (estat.veu.ona, juce::dontSendNotification);
    btSolo.setToggleState (estat.soloEditor.actiu, juce::dontSendNotification);
    selMode.setTriat ((int) mode());
    auto* p = presa();
    const auto ton = p != nullptr ? vv::tonalitatDe (p->veu, tonCanco()) : tonCanco();
    btTo.setButtonText (juce::String::fromUTF8 (mibase::harmonia::nomTo (ton.to).c_str()));
    btTo.setEnabled (p != nullptr && ! vv::cromatic (p->veu));
    juce::String nomEsc = "Cromàtica"_ca;
    if (p == nullptr || ! vv::cromatic (p->veu))
        nomEsc = juce::String::fromUTF8 (mibase::harmonia::escales()[(size_t) juce::jlimit (0, (int) mibase::harmonia::escales().size() - 1, ton.escala)].nom);
    if (p != nullptr && p->veu.modeTon == 0) nomEsc += " (cançó)"_ca;
    btEscalaTon.setButtonText (nomEsc);
    // Els poms mostren les notes on actuen.
    const auto ns = notesConsola();
    for (auto* x : { &pCorreccio, &pVibrato, &pRecta, &pFormant, &pVolum }) x->setEnabled (! ns.empty());
    if (p != nullptr && ! ns.empty())
    {
        auto valor = [&] (Pom& pm, const std::function<double (const NotaVeu&)>& f)
        {
            const double v0 = f (p->veu.notes[(size_t) ns[0]]);
            bool igual = true;
            for (int k : ns) if (std::abs (f (p->veu.notes[(size_t) k]) - v0) > 0.5) igual = false;
            pm.setValor (v0, igual);
        };
        valor (pCorreccio, [&] (const NotaVeu& n)
        {
            const double obj = objectiu (vv::centreFinal (n)) - n.centre;
            return std::abs (obj) < 1e-6 ? 100.0 : juce::jlimit (0.0, 100.0, n.desplac / obj * 100.0);
        });
        valor (pVibrato, [] (const NotaVeu& n) { return n.modulacio * 100.0; });
        valor (pRecta, [] (const NotaVeu& n) { return (1.0 - n.deriva) * 100.0; });
        valor (pFormant, [] (const NotaVeu& n) { return n.formant; });
        valor (pVolum, [] (const NotaVeu& n) { return n.guanyDb; });
    }
    pTransicio.setEnabled (p != nullptr);
    if (p != nullptr) pTransicio.setValor (p->veu.transicio * 1000.0, true);
    const bool triades = ! estat.veu.triades.empty();
    btCorregeix.setButtonText (triades ? "Corregeix"_ca : "Corregeix-les totes"_ca);
    btOriginal.setButtonText (triades ? "A l'original"_ca : "Totes a l'original"_ca);
    bool actiu = false;
    if (auto* t = estat.pista (estat.veu.pista)) actiu = mibase::veuActiva (*t);
    btAB.setToggleState (actiu, juce::dontSendNotification);
    btAB.setButtonText (actiu ? "Corregit" : "Original");
}

void EditorVeu::changeListenerCallback (juce::ChangeBroadcaster*)
{
    actualitzaBotons();
    repaint();
}

void EditorVeu::resized()
{
    auto r = zonaBarra().reduced (8, 5);
    btTanca.setBounds (r.removeFromRight (28));
    r.removeFromRight (6);
    btAjusta.setBounds (r.removeFromRight (70));
    r.removeFromRight (6);
    btOna.setBounds (r.removeFromRight (52));
    r.removeFromRight (6);
    btSolo.setBounds (r.removeFromRight (34));
    r.removeFromLeft (150);   // el títol
    for (int i = 0; i < numEines; ++i)
    {
        btEines[i].setBounds (r.removeFromLeft (76));
        r.removeFromLeft (3);
    }

    // Consola: tonalitat i A/B | poms | botons.
    auto c = zonaConsola().reduced (12, 8);
    zTon = c.removeFromLeft (190);
    {
        auto t = zTon;
        t.removeFromTop (16);
        auto fila = t.removeFromTop (26);
        btTo.setBounds (fila.removeFromLeft (52));
        fila.removeFromLeft (6);
        btEscalaTon.setBounds (fila);
        t.removeFromTop (4);
        selMode.setBounds (t.removeFromTop (22));
        t.removeFromTop (4);
        btAB.setBounds (t.removeFromTop (24));
    }
    c.removeFromLeft (14);
    zBotons = c.removeFromRight (232);
    {
        auto t = zBotons.withTrimmedTop (4);
        const int h = (t.getHeight() - 10) / 3;
        auto fila = [&] (Boto& a, Boto& b)
        {
            auto f = t.removeFromTop (h);
            a.setBounds (f.removeFromLeft ((f.getWidth() - 6) / 2));
            f.removeFromLeft (6);
            b.setBounds (f);
            t.removeFromTop (5);
        };
        fila (btTotes, btCap);
        fila (btCorregeix, btOriginal);
        fila (btDesfes, btRefes);
    }
    c.removeFromRight (14);
    zPoms = c;
    {
        auto t = zPoms.withTrimmedTop (16);
        Pom* poms[] { &pCorreccio, &pVibrato, &pRecta, &pFormant, &pVolum, &pTransicio };
        const int w = juce::jmin (92, t.getWidth() / 6);
        t = t.withSizeKeepingCentre (w * 6, t.getHeight());
        for (auto* pm : poms) pm->setBounds (t.removeFromLeft (w));
    }
}

const mibase::Clip* EditorVeu::clip()
{
    auto* c = estat.clip ({ estat.veu.pista, estat.veu.clip });
    if (c != nullptr && c->presa == estat.veu.presa && ! c->midi) return c;
    if (auto* t = estat.pista (estat.veu.pista))
        for (int k = 0; k < (int) t->clips.size(); ++k)
            if (t->clips[(size_t) k].presa == estat.veu.presa) { estat.veu.clip = k; return &t->clips[(size_t) k]; }
    return nullptr;
}

const vv::Corbes* EditorVeu::corbes()
{
    auto* s = presa();
    auto a = s != nullptr ? servei.analisi (s->id) : nullptr;
    if (a == nullptr || ! s->veu.analitzada) return nullptr;
    if (! corbesValides || a != analisiCorbes || ! (s->veu == veuCorbes))
    {
        cacheCorbes = vv::corbes (*a, s->veu);
        analisiCorbes = a;
        veuCorbes = s->veu;
        corbesValides = true;
        rmsMax = 1e-6f;
        for (auto v : a->rms) rmsMax = std::max (rmsMax, v);
    }
    return &cacheCorbes;
}

void EditorVeu::ajustaVista()
{
    auto* s = presa();
    const auto* c = clip();
    if (s == nullptr || c == nullptr || getWidth() <= ampleTeclat) return;
    const double ini = c->iniciVisible(), fi = c->fiVisible();
    t0 = ini - 0.05 * (fi - ini);
    pxs = juce::jlimit (5.0, 4000.0, (double) zonaNotes().getWidth() / juce::jmax (0.1, (fi - ini) * 1.1));
    double lo = 200.0, hi = -200.0;
    for (auto& n : s->veu.notes)
    {
        if (c->pos + n.fi < ini || c->pos + n.ini > fi) continue;
        lo = std::min ({ lo, n.centre, vv::centreFinal (n) });
        hi = std::max ({ hi, n.centre, vv::centreFinal (n) });
    }
    if (lo > hi) { lo = 55.0; hi = 72.0; }
    const double files = juce::jmax (1.0, zonaNotes().getHeight() / (double) altSemi);
    if (hi - lo + 6.0 > files) altSemi = (float) juce::jlimit (6.0, 40.0, zonaNotes().getHeight() / (hi - lo + 6.0));
    midiDalt = (hi + lo) * 0.5 + zonaNotes().getHeight() / (double) altSemi * 0.5;
    vistaFeta = true;
    repaint();
}

void EditorVeu::tic()
{
    auto* s = presa();
    if (s == nullptr) return;
    if (! vistaFeta && s->veu.analitzada) ajustaVista();
    if (estat.capcal != ultimCapcal || servei.progres (s->id) >= 0.0f || servei.treballant (s->id))
    {
        ultimCapcal = estat.capcal;
        repaint();
    }
}

//==============================================================================
juce::Rectangle<float> EditorVeu::rectNota (const mibase::Clip& c, const NotaVeu& n) const
{
    const float x0 = xDe (c.pos + n.ini), x1 = xDe (c.pos + n.fi);
    const float y = yDe (vv::centreFinal (n));
    const float h = juce::jmax (8.0f, altSemi * 1.1f);
    return { x0, y - h * 0.5f, juce::jmax (2.0f, x1 - x0), h };
}

bool EditorVeu::triada (int k) const
{
    return std::find (estat.veu.triades.begin(), estat.veu.triades.end(), k) != estat.veu.triades.end();
}

void EditorVeu::triaSola (int k) { estat.veu.triades = { k }; }

void EditorVeu::commuta (int k)
{
    auto& t = estat.veu.triades;
    if (auto it = std::find (t.begin(), t.end(), k); it != t.end()) t.erase (it);
    else t.push_back (k);
}

int EditorVeu::notaA (juce::Point<int> p)
{
    auto* s = presa();
    const auto* c = clip();
    if (s == nullptr || c == nullptr) return -1;
    for (int k = (int) s->veu.notes.size() - 1; k >= 0; --k)
        if (rectNota (*c, s->veu.notes[(size_t) k]).expanded (0.0f, 3.0f).contains (p.toFloat())) return k;
    return -1;
}

//==============================================================================
void EditorVeu::paint (juce::Graphics& g)
{
    g.fillAll (tema::fons);
    // Barra.
    {
        auto b = zonaBarra();
        g.setColour (tema::lateral);
        g.fillRect (b);
        g.setColour (tema::linia);
        g.fillRect (b.getX(), b.getBottom() - 1, b.getWidth(), 1);
        g.setColour (tema::veu);
        g.setFont (dibuix::lletra (13.0f, true));
        auto t = b.reduced (12, 0).removeFromLeft (150);
        g.drawText ("Vocal Editor", t, juce::Justification::centredLeft);
    }

    auto* s = presa();
    const auto* c = clip();
    const auto zona = zonaNotes();
    if (s == nullptr || c == nullptr)
    {
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (13.0f));
        g.drawFittedText ("Fes doble clic en un clip d'una pista amb el Vocal Editor (o obre'l des dels inserts de la pista)."_ca,
                          zona.reduced (20), juce::Justification::centred, 3);
        pintaConsola (g);
        return;
    }

    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (zona);
        pintaGraella (g, *c);
        if (estat.veu.ona) pintaOna (g, *c);
        pintaNotes (g, *c);
        pintaCorba (g, *c);
        // Capçal.
        const float xc = xDe (estat.capcal);
        g.setColour (tema::accent);
        g.fillRect (xc - 0.5f, (float) zona.getY(), 1.5f, (float) zona.getHeight());
        // Requadre.
        if (gest == Gest::Requadre)
        {
            const auto r = juce::Rectangle<int> (inici, ratoli).toFloat();
            g.setColour (tema::accent.withAlpha (0.12f));
            g.fillRect (r);
            g.setColour (tema::accent.withAlpha (0.6f));
            g.drawRect (r, 1.0f);
        }
    }
    pintaRegla (g);
    pintaTeclat (g);
    pintaConsola (g);

    // Estat de l'anàlisi.
    const float p = servei.progres (s->id);
    if (p >= 0.0f || ! s->veu.analitzada)
    {
        auto r = zona.withSizeKeepingCentre (300, 54);
        g.setColour (tema::superficie2.withAlpha (0.95f));
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (13.0f));
        g.drawText ("Analitzant la veu… "_ca + (p >= 0.0f ? juce::String ((int) (p * 100.0f)) + " %" : juce::String()),
                    r.removeFromTop (32), juce::Justification::centred);
        auto barra = r.reduced (20, 6).removeFromTop (6).toFloat();
        g.setColour (tema::linia);
        g.fillRoundedRectangle (barra, 3.0f);
        g.setColour (tema::veu);
        g.fillRoundedRectangle (barra.withWidth (barra.getWidth() * juce::jlimit (0.0f, 1.0f, p)), 3.0f);
    }
    else if (s->veu.notes.empty())
    {
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (13.0f));
        g.drawText ("No s'hi ha trobat cap nota cantada."_ca, zona, juce::Justification::centred);
    }
}

void EditorVeu::pintaGraella (juce::Graphics& g, const mibase::Clip& c)
{
    const auto zona = zonaNotes();
    auto* pv = presa();
    const auto ton = pv != nullptr ? vv::tonalitatDe (pv->veu, tonCanco()) : tonCanco();
    const bool totes = pv == nullptr || vv::cromatic (pv->veu);
    const int nDalt = (int) std::ceil (midiDalt), nBaix = (int) std::floor (midiDe ((float) zona.getBottom()));
    for (int n = nBaix; n <= nDalt; ++n)
    {
        const float y0 = yDe (n + 0.5), y1 = yDe (n - 0.5);
        const bool negra = mibase::midi::esNegra (n);
        // Les files de la tonalitat, més clares; la resta, fosques (les notes s'hi mouen per dins).
        juce::Colour col = negra ? tema::fons.darker (0.25f) : tema::fons;
        if (! totes && mibase::harmonia::aEscala (n, ton)) col = juce::Colour (0xff2b3240);
        else if (! totes) col = tema::fons.darker (0.35f);
        g.setColour (col);
        g.fillRect ((float) zona.getX(), y0, (float) zona.getWidth(), y1 - y0);
        g.setColour (juce::Colours::white.withAlpha ((((n % 12) + 12) % 12) == 0 ? 0.10f : 0.03f));
        g.fillRect ((float) zona.getX(), y1 - 0.5f, (float) zona.getWidth(), 1.0f);
    }
    // Compassos i temps.
    const double spc = mibase::edicio::segonsPerCompas (estat.projecte);
    const double spt = spc / juce::jmax (1, estat.projecte.compasNum);
    const double tf = tDe ((float) zona.getRight());
    for (double t = std::floor (t0 / spt) * spt; t <= tf; t += spt)
    {
        const bool compas = std::abs (std::remainder (t, spc)) < spt * 0.25;
        if (! compas && spt * pxs < 12.0) continue;
        g.setColour (juce::Colours::white.withAlpha (compas ? 0.12f : 0.04f));
        g.fillRect (xDe (t), (float) zona.getY(), 1.0f, (float) zona.getHeight());
    }
    // Fora de la finestra del clip: més fosc.
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    const float xa = xDe (c.iniciVisible()), xb = xDe (c.fiVisible());
    if (xa > zona.getX()) g.fillRect ((float) zona.getX(), (float) zona.getY(), xa - zona.getX(), (float) zona.getHeight());
    if (xb < zona.getRight()) g.fillRect (xb, (float) zona.getY(), zona.getRight() - xb, (float) zona.getHeight());
}

void EditorVeu::pintaOna (juce::Graphics& g, const mibase::Clip& c)
{
    auto it = estat.ones.find (estat.veu.presa);
    if (it == estat.ones.end() || it->second == nullptr) return;
    const auto& ona = *it->second;
    const auto zona = zonaNotes();
    const float mig = (float) zona.getY() + zona.getHeight() * 0.5f, alt = zona.getHeight() * 0.45f;
    juce::Path p;
    juce::Array<juce::Point<float>> baix;
    for (int x = zona.getX(); x < zona.getRight(); x += 2)
    {
        const double ta = tDe ((float) x) - c.pos, tb = tDe ((float) x + 2.0f) - c.pos;
        if (ta < c.inici || tb > c.fi) continue;
        const auto b0 = (size_t) juce::jmax (0.0, ta * ona.sr / OnaPresa::bloc), b1 = (size_t) juce::jmax (0.0, tb * ona.sr / OnaPresa::bloc);
        float mn = 0.0f, mx = 0.0f;
        for (size_t b = b0; b <= b1 && b < ona.maxs.size(); ++b) { mn = juce::jmin (mn, ona.mins[b]); mx = juce::jmax (mx, ona.maxs[b]); }
        if (p.isEmpty()) p.startNewSubPath ((float) x, mig - mx * alt);
        else p.lineTo ((float) x, mig - mx * alt);
        baix.add ({ (float) x, mig - mn * alt });
    }
    for (int i = baix.size() - 1; i >= 0; --i) p.lineTo (baix[i]);
    if (! p.isEmpty())
    {
        p.closeSubPath();
        g.setColour (tema::veu.withAlpha (0.10f));
        g.fillPath (p);
    }
}

void EditorVeu::pintaNotes (juce::Graphics& g, const mibase::Clip& c)
{
    auto* s = presa();
    auto a = servei.analisi (s->id);
    const auto zona = zonaNotes();
    for (int k = 0; k < (int) s->veu.notes.size(); ++k)
    {
        const auto& n = s->veu.notes[(size_t) k];
        const auto r = rectNota (c, n);
        if (r.getRight() < zona.getX() || r.getX() > zona.getRight()) continue;
        if (c.pos + n.fi < c.inici + c.pos || c.pos + n.ini > c.fi + c.pos) continue;
        const bool tri = triada (k);
        const double cf = vv::centreFinal (n);
        const auto col = colorAfinacio (cf);
        // On era abans de moure-la.
        if (std::abs (n.desplac) > 0.005)
        {
            auto r0 = r.withY (yDe (n.centre) - r.getHeight() * 0.5f);
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            juce::Path pr; pr.addRoundedRectangle (r0, 4.0f);
            float guions[] { 3.0f, 3.0f };
            juce::PathStrokeType (1.0f).createDashedStroke (pr, pr, guions, 2);
            g.fillPath (pr);
        }
        // Blob: l'alçada segueix el volum de la nota.
        juce::Path blob;
        if (a != nullptr && r.getWidth() > 6.0f)
        {
            const int i0 = juce::jmax (0, (int) std::ceil (n.ini / vv::pas)), i1 = juce::jmin (a->punts(), (int) std::ceil (n.fi / vv::pas));
            const float yc = r.getCentreY(), hMax = r.getHeight() * 0.5f;
            const float guany = (float) std::pow (10.0, n.guanyDb / 20.0);
            std::vector<juce::Point<float>> dalt;
            const int pasI = juce::jmax (1, (int) (1.5 / (vv::pas * pxs)));
            for (int i = i0; i < i1; i += pasI)
            {
                const float v = std::sqrt (juce::jlimit (0.0f, 1.0f, a->rms[(size_t) i] * guany / rmsMax));
                dalt.push_back ({ xDe (c.pos + i * vv::pas), yc - (2.0f + v * (hMax - 2.0f)) });
            }
            if (dalt.size() >= 2)
            {
                blob.startNewSubPath (r.getX(), yc);
                for (auto& pt : dalt) blob.lineTo (pt);
                blob.lineTo (r.getRight(), yc);
                for (auto it = dalt.rbegin(); it != dalt.rend(); ++it) blob.lineTo (it->x, 2.0f * yc - it->y);
                blob.closeSubPath();
                blob = blob.createPathWithRoundedCorners (3.0f);
            }
        }
        if (blob.isEmpty()) blob.addRoundedRectangle (r, 4.0f);
        g.setColour (col.withAlpha (tri ? 0.85f : 0.55f));
        g.fillPath (blob);
        g.setColour (tri ? tema::text : col.brighter (0.3f));
        g.strokePath (blob, juce::PathStrokeType (tri ? 2.0f : 1.0f));
        // Nom i cents, si hi ha lloc.
        if (r.getWidth() > 44.0f)
        {
            g.setColour (tema::text.withAlpha (0.85f));
            g.setFont (dibuix::lletra (10.5f, tri));
            const double cents = (cf - std::round (cf)) * 100.0;
            g.drawText (nomNota (cf) + (std::abs (cents) >= 1.0 ? "  " + textCents (cents).upToFirstOccurrenceOf (" ", false, false) : juce::String()),
                        r.withY (r.getY() - 14.0f).withHeight (13.0f).toNearestInt(), juce::Justification::centredLeft, false);
        }
    }
}

void EditorVeu::pintaCorba (juce::Graphics& g, const mibase::Clip& c)
{
    const auto* cb = corbes();
    if (cb == nullptr) return;
    const auto zona = zonaNotes();
    const int n = (int) cb->editada.size();
    const int i0 = juce::jmax (0, (int) std::floor ((tDe ((float) zona.getX()) - c.pos) / vv::pas));
    const int i1 = juce::jmin (n, (int) std::ceil ((tDe ((float) zona.getRight()) - c.pos) / vv::pas) + 1);
    const int iIni = (int) std::ceil (c.inici / vv::pas), iFi = (int) std::floor (c.fi / vv::pas);
    auto traça = [&] (const std::vector<float>& v, bool nomesDiferent)
    {
        juce::Path p;
        bool dins = false;
        for (int i = juce::jmax (i0, iIni); i < juce::jmin (i1, iFi); ++i)
        {
            const float m = v[(size_t) i];
            const bool ok = ! std::isnan (m) && (! nomesDiferent || std::abs (cb->canvi[(size_t) i]) > 0.02f);
            if (! ok) { dins = false; continue; }
            const juce::Point<float> pt (xDe (c.pos + i * vv::pas), yDe (m));
            if (! dins) p.startNewSubPath (pt); else p.lineTo (pt);
            dins = true;
        }
        return p;
    };
    g.setColour (juce::Colours::white.withAlpha (0.28f));
    g.strokePath (traça (cb->original, true), juce::PathStrokeType (1.0f));
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.strokePath (traça (cb->editada, false), juce::PathStrokeType (1.4f, juce::PathStrokeType::curved));
}

void EditorVeu::pintaRegla (juce::Graphics& g)
{
    auto r = zonaRegla();
    g.setColour (tema::lateral);
    g.fillRect (r);
    g.setColour (tema::linia);
    g.fillRect (r.getX(), r.getBottom() - 1, r.getWidth(), 1);
    const double spc = mibase::edicio::segonsPerCompas (estat.projecte);
    int cada = 1;
    while (spc * cada * pxs < 50.0) cada *= 2;
    if (const auto& bu = estat.projecte.bucle; bu.marcat && bu.a > bu.de)   // 0.24
    {
        const float xa = juce::jmax ((float) r.getX(), xDe (bu.de * spc)), xb = juce::jmin ((float) r.getRight(), xDe (bu.a * spc));
        if (xb > xa)
        {
            g.setColour ((bu.actiu ? tema::guitarra : tema::suau).withAlpha (bu.actiu ? 0.35f : 0.18f));
            g.fillRect (juce::Rectangle<float> (xa, (float) r.getY() + 2.0f, xb - xa, (float) r.getHeight() - 4.0f));
        }
    }
    g.setFont (dibuix::mono (10.5f));
    const double tf = tDe ((float) r.getRight());
    for (long long k = (long long) std::floor (t0 / spc); k * spc <= tf; ++k)
    {
        if (k % cada != 0) continue;
        const float x = xDe (k * spc);
        g.setColour (tema::linia2);
        g.fillRect (x, (float) r.getY() + 8.0f, 1.0f, (float) r.getHeight() - 8.0f);
        g.setColour (tema::textMig);
        const auto text = juce::String (mibase::edicio::textPosicio (estat.projecte, k * spc)).upToFirstOccurrenceOf (".", false, false);
        g.drawText (text, juce::Rectangle<float> (x + 3.0f, (float) r.getY(), 40.0f, (float) r.getHeight()), juce::Justification::centredLeft);
    }
    const float xc = xDe (estat.capcal);
    if (xc >= r.getX())
    {
        g.setColour (tema::accent);
        juce::Path tri;
        tri.addTriangle (xc - 5.0f, (float) r.getY() + 4.0f, xc + 5.0f, (float) r.getY() + 4.0f, xc, (float) r.getBottom() - 2.0f);
        g.fillPath (tri);
    }
}

void EditorVeu::pintaTeclat (juce::Graphics& g)
{
    auto r = zonaTeclat();
    g.setColour (tema::lateral);
    g.fillRect (getLocalBounds().withTrimmedTop (altBarra).removeFromTop (altRegla).removeFromLeft (ampleTeclat));
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion (r);
    auto* pv = presa();
    const auto ton = pv != nullptr ? vv::tonalitatDe (pv->veu, tonCanco()) : tonCanco();
    const bool totes = pv == nullptr || vv::cromatic (pv->veu);
    const int nDalt = (int) std::ceil (midiDalt), nBaix = (int) std::floor (midiDe ((float) r.getBottom()));
    for (int n = nBaix; n <= nDalt; ++n)
    {
        const float y0 = yDe (n + 0.5), y1 = yDe (n - 0.5);
        const bool negra = mibase::midi::esNegra (n);
        g.setColour (negra ? juce::Colour (0xff202026) : juce::Colour (0xffd9d7de));
        g.fillRect ((float) r.getX(), y0, (float) r.getWidth() - (negra ? 18.0f : 0.0f), y1 - y0);
        if (! totes && mibase::harmonia::aEscala (n, ton))
        {
            g.setColour (tema::accent.withAlpha (negra ? 0.55f : 0.35f));
            g.fillRect ((float) r.getX(), y0, (float) r.getWidth() - (negra ? 18.0f : 0.0f), y1 - y0);
        }
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect ((float) r.getX(), y1 - 0.5f, (float) r.getWidth(), 0.5f);
        if ((((n % 12) + 12) % 12) == 0 && altSemi >= 8.0f)
        {
            g.setColour (juce::Colour (0xff333338));
            g.setFont (dibuix::lletra (juce::jmin (11.0f, altSemi)));
            g.drawText ("C" + juce::String (n / 12 - 1), juce::Rectangle<float> ((float) r.getX(), y0, (float) r.getWidth() - 4.0f, y1 - y0),
                        juce::Justification::centredRight);
        }
    }
    g.setColour (tema::linia);
    g.fillRect (r.getRight() - 1, r.getY(), 1, r.getHeight());
}

//==============================================================================
void EditorVeu::mouseMove (const juce::MouseEvent& e)
{
    const int k = zonaNotes().contains (e.getPosition()) ? notaA (e.getPosition()) : -1;
    setMouseCursor (k < 0 ? juce::MouseCursor::NormalCursor
                          : estat.veu.eina == Fletxa ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::PointingHandCursor);
}

void EditorVeu::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    estat.focusEditor = true;
    inici = ratoli = e.getPosition();
    gest = Gest::Res;
    auto* s = presa();
    if (zonaRegla().contains (e.getPosition()))
    {
        gest = Gest::Capcal;   // si s'arrossega, es fa el bucle (0.24)
        estat.mouCapcal (juce::jmax (0.0, tDe ((float) e.x)), true);
        return;
    }
    if (s == nullptr || ! zonaNotes().contains (e.getPosition()) || ! s->veu.analitzada) return;
    const int k = notaA (e.getPosition());
    if (k < 0)
    {
        if (! e.mods.isCommandDown() && ! e.mods.isShiftDown()) estat.veu.triades.clear();
        gest = Gest::Requadre;
        estat.avisa();
        return;
    }
    if (e.mods.isCommandDown()) commuta (k);
    else if (e.mods.isShiftDown()) { if (! triada (k)) estat.veu.triades.push_back (k); }
    else if (! triada (k)) triaSola (k);
    if (! triada (k)) { estat.avisa(); return; }
    gest = Gest::Notes;
    notaSota = k;
    notesInici = s->veu.notes;
    comencaCanvi();
    estat.avisa();
}

void EditorVeu::mouseDrag (const juce::MouseEvent& e)
{
    ratoli = e.getPosition();
    if (gest == Gest::Capcal && std::abs (e.x - inici.x) > 5) { gest = Gest::Bucle; comencaCanvi(); }
    if (gest == Gest::Capcal) { estat.mouCapcal (juce::jmax (0.0, tDe ((float) e.x)), true); return; }
    if (gest == Gest::Bucle)
    {
        restaura();
        estat.bucleEntre (tDe ((float) inici.x), tDe ((float) e.x));
        estat.info = "Bucle"_ca;
        estat.avisa();
        return;
    }
    if (gest == Gest::Requadre)
    {
        auto* s = presa();
        const auto* c = clip();
        if (s == nullptr || c == nullptr) return;
        const auto r = juce::Rectangle<int> (inici, ratoli).toFloat();
        std::vector<int> t;
        for (int k = 0; k < (int) s->veu.notes.size(); ++k)
            if (rectNota (*c, s->veu.notes[(size_t) k]).intersects (r)) t.push_back (k);
        estat.veu.triades = t;
        estat.avisa();
        return;
    }
    if (gest == Gest::Notes && e.getDistanceFromDragStart() > 2) aplicaArrossegament (e);
}

void EditorVeu::aplicaArrossegament (const juce::MouseEvent& e)
{
    restaura();
    auto* s = presa();
    if (s == nullptr) return;
    const double dy = (double) e.getDistanceFromDragStartY();
    juce::String info;
    for (int k : estat.veu.triades)
    {
        if (k < 0 || k >= (int) s->veu.notes.size() || k >= (int) notesInici.size()) continue;
        auto& n = s->veu.notes[(size_t) k];
        const auto& n0 = notesInici[(size_t) k];
        switch (estat.veu.eina)
        {
            case Fletxa:
            {
                double desti = vv::centreFinal (n0) - dy / altSemi;
                if (! lliure (e.mods)) desti = objectiu (desti);   // semitons o dins la tonalitat, segons el mode
                n.desplac = juce::jlimit (-24.0, 24.0, desti - n.centre);
                if (k == notaSota) info = nomNota (desti) + "  " + textCents ((desti - std::round (desti)) * 100.0);
                break;
            }
            case Vibrato:
                n.modulacio = juce::jlimit (0.0, 2.0, n0.modulacio - dy / 100.0);
                if (k == notaSota) info = "Vibrato " + juce::String ((int) std::lround (n.modulacio * 100.0)) + " %";
                break;
            case Deriva:
                n.deriva = juce::jlimit (0.0, 1.0, n0.deriva - dy / 150.0);
                if (k == notaSota) info = "Deriva " + juce::String ((int) std::lround (n.deriva * 100.0)) + " %";
                break;
            case Formant:
                n.formant = juce::jlimit (-12.0, 12.0, std::round ((n0.formant - dy / 15.0) * 10.0) / 10.0);
                if (k == notaSota) info = "Formant " + num (n.formant, 1) + " st";
                break;
            case Volum:
                n.guanyDb = juce::jlimit (-30.0, 12.0, std::round ((n0.guanyDb - dy / 6.0) * 10.0) / 10.0);
                if (k == notaSota) info = "Volum " + num (n.guanyDb, 1) + " dB";
                break;
            default: break;
        }
    }
    estat.info = info;
    estat.avisa();
}

void EditorVeu::mouseUp (const juce::MouseEvent&)
{
    if (gest == Gest::Notes) acabaCanvi ("Vocal Editor: notes editades"_ca);
    if (gest == Gest::Bucle) acabaCanvi ("Bucle des de l'editor"_ca);
    gest = Gest::Res;
    notaSota = -1;
    repaint();
}

void EditorVeu::restableix (int k)
{
    auto* s = presa();
    if (s == nullptr || k < 0 || k >= (int) s->veu.notes.size()) return;
    auto& n = s->veu.notes[(size_t) k];
    switch (estat.veu.eina)
    {
        case Fletxa:
            n.desplac = objectiu (vv::centreFinal (n)) - n.centre;
            break;
        case Vibrato: n.modulacio = 1.0; break;
        case Deriva: n.deriva = 1.0; break;
        case Formant: n.formant = 0.0; break;
        case Volum: n.guanyDb = 0.0; break;
        default: break;
    }
}

void EditorVeu::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int k = notaA (e.getPosition());
    if (k < 0) return;
    if (! triada (k)) triaSola (k);
    comencaCanvi();
    restaura();
    for (int t : estat.veu.triades) restableix (t);
    acabaCanvi (estat.veu.eina == Fletxa ? "Vocal Editor: nota afinada"_ca : "Vocal Editor: valor original"_ca);
}

void EditorVeu::mouTriades (double semitons, bool lliure, const juce::String& evolucio)
{
    auto* s = presa();
    if (s == nullptr || estat.veu.triades.empty()) return;
    comencaCanvi();
    restaura();
    for (int k : estat.veu.triades)
    {
        if (k < 0 || k >= (int) s->veu.notes.size()) continue;
        auto& n = s->veu.notes[(size_t) k];
        const double desti = lliure ? vv::centreFinal (n) + semitons : (double) grau (vv::centreFinal (n), semitons > 0 ? 1 : -1);
        n.desplac = juce::jlimit (-24.0, 24.0, desti - n.centre);
    }
    acabaCanvi (evolucio);
}

void EditorVeu::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float d = std::abs (w.deltaY) > std::abs (w.deltaX) ? w.deltaY : w.deltaX;
    if (e.mods.isCommandDown())
    {
        const double t = tDe ((float) e.x);
        pxs = juce::jlimit (5.0, 4000.0, pxs * std::pow (1.25, d * 4.0));
        t0 = t - (e.x - zonaNotes().getX()) / pxs;
    }
    else if (e.mods.isAltDown())
    {
        const double m = midiDe ((float) e.y);
        altSemi = juce::jlimit (5.0f, 48.0f, altSemi * (float) std::pow (1.2, d * 4.0));
        midiDalt = m + (e.y - zonaNotes().getY()) / (double) altSemi;
    }
    else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
        t0 -= d * 300.0 / pxs;
    else
        midiDalt = juce::jlimit (30.0, 110.0, midiDalt + d * 12.0);
    repaint();
}

bool EditorVeu::keyPressed (const juce::KeyPress& k)
{
    const int codi = k.getKeyCode();
    const auto mods = k.getModifiers();
    auto* s = presa();
    if (s == nullptr) return false;
    if (codi >= '1' && codi < '1' + numEines && ! mods.isAnyModifierKeyDown()) { estat.veu.eina = codi - '1'; actualitzaBotons(); repaint(); return true; }
    if (codi == juce::KeyPress::upKey || codi == juce::KeyPress::downKey)
    {
        const bool fi = lliure (mods);
        const double d = (codi == juce::KeyPress::upKey ? 1.0 : -1.0) * (fi ? 0.1 : 1.0);
        mouTriades (d, fi, "Vocal Editor: notes mogudes"_ca);
        return true;
    }
    if (mods.isCommandDown() && (codi == 'A' || codi == 'a'))
    {
        estat.veu.triades.clear();
        for (int i = 0; i < (int) s->veu.notes.size(); ++i) estat.veu.triades.push_back (i);
        estat.avisa();
        return true;
    }
    if (codi == juce::KeyPress::escapeKey) { estat.veu.triades.clear(); estat.avisa(); return true; }
    if ((codi == 'B' || codi == 'b') && ! mods.isAnyModifierKeyDown()) { if (onCommutaAB) onCommutaAB(); return true; }
    if (codi == juce::KeyPress::deleteKey || codi == juce::KeyPress::backspaceKey)
    {
        if (estat.veu.triades.empty()) return true;
        comencaCanvi();
        restaura();
        vv::restaura (s->veu, estat.veu.triades);
        acabaCanvi ("Vocal Editor: notes a l'original"_ca);
        return true;
    }
    return false;
}


//==============================================================================
// 0.23: consola de baix
//==============================================================================
mibase::Tonalitat EditorVeu::tonCanco() const { return mibase::harmonia::tonalitatEfectiva (estat.projecte); }

std::vector<int> EditorVeu::notesConsola()
{
    std::vector<int> t;
    auto* p = presa();
    if (p == nullptr) return t;
    for (int k : estat.veu.triades) if (k >= 0 && k < (int) p->veu.notes.size()) t.push_back (k);
    if (t.empty()) for (int k = 0; k < (int) p->veu.notes.size(); ++k) t.push_back (k);
    return t;
}

void EditorVeu::comencaPom()
{
    comencaCanvi();
    objectiusInici.clear();
    if (auto* p = presa())
        for (int k : notesConsola())
            objectiusInici.push_back (objectiu (vv::centreFinal (p->veu.notes[(size_t) k])));
}

void EditorVeu::canviaPom (const std::function<void (NotaVeu&, size_t)>& f)
{
    restaura();
    auto* p = presa();
    if (p == nullptr) return;
    const auto ns = notesConsola();
    for (size_t i = 0; i < ns.size(); ++i) f (p->veu.notes[(size_t) ns[i]], i);
    estat.avisa();
}

void EditorVeu::aplicaTonalitat (int modeTon, int to, int escala)
{
    comencaCanvi();
    restaura();
    if (auto* p = presa()) { p->veu.modeTon = modeTon; p->veu.to = to; p->veu.escala = escala; }
    acabaCanvi ("Vocal Editor: tonalitat"_ca);
}

void EditorVeu::menuTo()
{
    auto* p = presa();
    if (p == nullptr) return;
    const auto ton = vv::tonalitatDe (p->veu, tonCanco());
    juce::PopupMenu m;
    for (int i = 0; i < 12; ++i) m.addItem (i + 1, juce::String::fromUTF8 (mibase::harmonia::nomTo (i).c_str()), true, i == ton.to);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btTo), [this, ton] (int r)
    {
        if (r >= 1) aplicaTonalitat (2, r - 1, ton.escala);
    });
}

void EditorVeu::menuEscala()
{
    auto* p = presa();
    if (p == nullptr) return;
    const auto ton = vv::tonalitatDe (p->veu, tonCanco());
    juce::PopupMenu m;
    m.addItem (1, "La de la cançó ("_ca + juce::String::fromUTF8 (mibase::harmonia::nomTonalitat (tonCanco()).c_str()) + ")", true, p->veu.modeTon == 0);
    m.addItem (2, "Cromàtica (tots els semitons)"_ca, true, p->veu.modeTon == 1);
    m.addSeparator();
    const auto& esc = mibase::harmonia::escales();
    for (int i = 0; i < (int) esc.size(); ++i)
        m.addItem (100 + i, juce::String::fromUTF8 (esc[(size_t) i].nom), true, p->veu.modeTon == 2 && p->veu.escala == i);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btEscalaTon), [this, ton] (int r)
    {
        if (r == 1) aplicaTonalitat (0, ton.to, ton.escala);
        else if (r == 2) aplicaTonalitat (1, ton.to, ton.escala);
        else if (r >= 100) aplicaTonalitat (2, ton.to, r - 100);
    });
}

void EditorVeu::pintaConsola (juce::Graphics& g)
{
    auto r = zonaConsola();
    vintage::panell (g, r.toFloat());
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRect (r.getX(), r.getY(), r.getWidth(), 1);
    g.setFont (dibuix::lletra (10.5f, true));
    g.setColour (tema::suau);
    g.drawText ("TONALITAT", zTon.withHeight (16), juce::Justification::centredLeft);
    auto* p = presa();
    juce::String quines;
    if (p != nullptr)
        quines = estat.veu.triades.empty() ? "TOTES LES NOTES (CAP DE TRIADA)"_ca
                                           : juce::String ((int) estat.veu.triades.size()) + (estat.veu.triades.size() == 1 ? " NOTA TRIADA"_ca : " NOTES TRIADES"_ca);
    g.drawText (quines, zPoms.withHeight (16), juce::Justification::centred);
    // Separadors gravats.
    for (int x : { zTon.getRight() + 7, zBotons.getX() - 7 })
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRect (x, r.getY() + 10, 1, r.getHeight() - 20);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRect (x + 1, r.getY() + 10, 1, r.getHeight() - 20);
    }
}

int EditorVeu::objectiu (double midi)
{
    auto* p = presa();
    if (p == nullptr || mode() == mibase::ModeEscala::Visual) return (int) std::lround (midi);
    return vv::notaObjectiu (midi, p->veu, tonCanco());
}

int EditorVeu::grau (double midi, int dir)
{
    auto* p = presa();
    if (p == nullptr || mode() == mibase::ModeEscala::Visual) return (int) std::lround (midi) + dir;
    return vv::mouGraus (midi, dir, p->veu, tonCanco());
}

bool EditorVeu::lliure (const juce::ModifierKeys& m) const
{
    return m.isAltDown() && mode() != mibase::ModeEscala::Bloquejat;
}

mibase::ModeEscala EditorVeu::mode() const
{
    const auto* t = estat.veu.pista >= 0 && estat.veu.pista < (int) estat.projecte.pistes.size() ? &estat.projecte.pistes[(size_t) estat.veu.pista] : nullptr;
    return t != nullptr ? mibase::modeEscalaDe (estat.projecte, *t) : estat.projecte.modeEscala;
}
