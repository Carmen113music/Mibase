#include "PianoRoll.h"
#include <mibase/Bateria.h>

#include <mibase/PresetsPads.h>

#include <numeric>

using namespace mibase;
namespace ed = mibase::edicio;

namespace
{
    juce::String txt (const std::string& s) { return juce::String::fromUTF8 (s.c_str()); }

    /** «3.2» (compàs que es veu . temps) d'un tick absolut. */
    juce::String posicioTick (const Projecte& p, double tickAbs)
    {
        return dibuix::utf8 (ed::textPosicio (p, midi::segons (p, tickAbs) + 1e-9));
    }

    juce::String nomGraella (int i) { return txt (midi::valorsGraella()[(size_t) juce::jlimit (0, (int) midi::valorsGraella().size() - 1, i)].nom); }

    /** Color d'una nota segons la velocitat: més fosca com més fluixa. */
    juce::Colour colorNota (juce::Colour base, int vel)
    {
        const float f = 0.35f + 0.65f * (float) vel / 127.0f;
        return base.withMultipliedBrightness (0.55f + 0.5f * f).withMultipliedSaturation (0.75f + 0.25f * f);
    }
}

//==============================================================================
void CaixaNumero::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (isMouseOverOrDragging() ? tema::accent : tema::linia2);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);
    g.setFont (dibuix::lletra (11.0f));
    g.setColour (tema::suau);
    g.drawText (text, r.reduced (8.0f, 0.0f), juce::Justification::centredLeft);
    g.setFont (dibuix::mono (14.0f));
    g.setColour (vintage::lcdText);
    g.drawText (juce::String (valor), r.reduced (8.0f, 0.0f), juce::Justification::centredRight);
}

//==============================================================================
PianoRoll::PianoRoll (EstatEstudi& e, int idx)
    : SeccioEstudi (e), index (idx), iniciVista (e.vistaEditors.iniciVista), pxPerTick (e.vistaEditors.pxPerTick)
{
    setWantsKeyboardFocus (true);
    setOpaque (true);

    struct E { dibuix::Icona icona; const char* ajuda; };
    const E llista[] = {
        { dibuix::Icona::Fletxa2, "Fletxa (1): tria, mou i allarga notes; arrossega per un lloc buit per fer un requadre. Doble clic: nota nova (o l'esborra)." },
        { dibuix::Icona::Llapis,  "Llapis (2): dibuixa notes (arrossega cap a la dreta per fer-les més llargues)." },
        { dibuix::Icona::Goma,    "Goma (3): esborra les notes per on passes." },
    };
    for (int i = 0; i < 3; ++i)
    {
        auto* b = eines.add (new Boto ({}, llista[i].icona, Boto::Estil::Tecla));
        b->setLlum (vintage::ledTaronja);
        b->setTooltip (juce::String::fromUTF8 (llista[i].ajuda));
        b->onClick = [this, i] { triaEina ((EinaPR) i); };
        addAndMakeVisible (b);
    }

    btGraella.setTooltip ("Graella i quantització: on s'enganxen les notes. Alt mentre arrossegues: sense graella."_ca);
    btGraella.onClick = [this] { menuGraella(); };
    btDurada.setTooltip ("Durada de les notes noves"_ca);
    btDurada.onClick = [this] { menuDurada(); };
    btQuantitza.setTooltip ("Quantitza les notes triades (o totes, si no n'hi ha cap) a la graella. Tecla Q; Maj+Q també el final."_ca);
    btQuantitza.onClick = [this] { quantitza (false); };
    velNova.setTooltip ("Velocitat de les notes noves (arrossega amunt i avall; doble clic: 100)"_ca);
    btFantasmes.setLlum (vintage::ledBlau);
    btFantasmes.setTooltip ("Notes fantasma: arrossega aquí el nom d'una pista MIDI per veure'n les notes com a referència. "
                            "Clica per triar-les o treure-les."_ca);
    btFantasmes.onClick = [this]
    {
        // 0.12: el cursor passa a ser el fantasma; es clica el clip de la taula de treball (MIDI o àudio).
        estat.triantFantasma = estat.triantFantasma == index ? -1 : index;
        info = estat.triantFantasma >= 0 ? "Clica el clip de la taula de treball que vols veure en fantasma (un altre cop: el treus)"_ca : juce::String();
        estat.avisa();
    };
    btBloqueja.setLlum (vintage::ledGroc);
    btBloqueja.setTooltip ("Bloqueja aquest editor amb el seu clip: si obres un altre clip MIDI, s'obre en un editor nou a sota"_ca);
    btBloqueja.onClick = [this]
    {
        if (index < 0 || index >= (int) estat.editors.size()) return;
        auto& ed = estat.editors[(size_t) index];
        ed.bloquejat = ! ed.bloquejat;
        info = ed.bloquejat ? "Bloquejat: es queda amb aquest clip"_ca : "Desbloquejat: hi obrirà el clip MIDI on facis doble clic"_ca;
        estat.avisa();
    };
    btSolo.setLlum (vintage::ledGroc);
    btSolo.setTooltip ("Solo: només sona la pista d'aquest editor; en apagar-lo, les pistes tornen com estaven (tecla * del teclat numèric)"_ca);
    btSolo.onClick = [this] { if (onSolo) onSolo(); };
    addAndMakeVisible (btSolo);
    btDesenganxa.setTooltip ("Desenganxa el Piano Roll en una finestra a part (per portar-lo a una altra pantalla)"_ca);
    btDesenganxa.onClick = [this] { if (onDesenganxa) onDesenganxa(); };
    btTanca.setTooltip ("Amaga el Piano Roll (doble clic a un clip MIDI el torna a obrir)"_ca);
    btTanca.onClick = [this] { if (onTanca) onTanca(); };

    for (auto* b : std::initializer_list<juce::Component*> { &btGraella, &btQuantitza, &velNova, &btFantasmes,
                                                             &btBloqueja, &btDesenganxa, &btTanca, &selMode })
        addAndMakeVisible (b);

    // Chord Pad.
    selMode.setLlum (vintage::ledTaronja);
    selMode.setTooltip ("Notes: a baix, les velocitats. Chord Pad: a baix, pads d'acords per tocar i arrossegar al Piano Roll"_ca);
    selMode.onCanvi = [this] (int i) { canviaModePads (i == 1); };
    btPresetPads.setTooltip ("Preset de pads: els acords de la tonalitat o les seqüències de 50 cançons"_ca);
    btPresetPads.onClick = [this] { menuPresetsPads(); };
    btLlapisPads.setLlum (vintage::ledTaronja);
    btLlapisPads.setTooltip ("Llapis: clica un pad per canviar-ne l'acord (també amb el botó dret)"_ca);
    btLlapisPads.onClick = [this] { btLlapisPads.setToggleState (! btLlapisPads.getToggleState(), juce::dontSendNotification); repaint(); };
    octavaPads.setTooltip ("Octava de la fonamental dels pads (4 = la del do central)");
    octavaPads.onCanvi = [this] (int) { estat.avisa(); };
    voicingPads.setLlum (vintage::ledTaronja);
    voicingPads.onCanvi = [this] (int) { estat.avisa(); };
    selDuradaPads.setLlum (vintage::ledTaronja);
    selDuradaPads.setTooltip ("Durada de l'acord quan l'arrossegues al Piano Roll");
    btMinimPads.setLlum (vintage::ledGroc);
    btMinimPads.setToggleState (true, juce::dontSendNotification);
    btMinimPads.setTooltip ("Moviment mínim: l'acord que deixes es col·loca a prop de l'anterior"_ca);
    btMinimPads.onClick = [this] { btMinimPads.setToggleState (! btMinimPads.getToggleState(), juce::dontSendNotification); };
    for (auto* b : std::initializer_list<juce::Component*> { &btPresetPads, &btLlapisPads, &octavaPads, &voicingPads, &selDuradaPads, &btMinimPads })
        addChildComponent (b);
    for (auto* b : { &barraV, &barraH })
    {
        addAndMakeVisible (b);
        b->addListener (this);
        b->setAutoHide (false);
        b->setColour (juce::ScrollBar::thumbColourId, tema::linia2);
    }
    triaEina (EinaPR::Fletxa);
    changeListenerCallback (nullptr);
}

PianoRoll::~PianoRoll()
{
    apagaProva();
}

void PianoRoll::setDesenganxat (bool d)
{
    desenganxat = d;
    btDesenganxa.setIcona (d ? dibuix::Icona::Enganxa : dibuix::Icona::Desenganxa);
    btDesenganxa.setTooltip (d ? "Torna a posar el Piano Roll a baix de l'estudi"_ca
                               : "Desenganxa el Piano Roll en una finestra a part (per portar-lo a una altra pantalla)"_ca);
    resized();
}

//==============================================================================
// Geometria
//==============================================================================
juce::Rectangle<int> PianoRoll::zonaRegla() const   { return { ampleTeclat(), alcadaBarra, getWidth() - ampleTeclat() - gruix, alcadaRegla }; }
juce::Rectangle<int> PianoRoll::zonaGraella() const
{
    const int y = alcadaBarra + alcadaRegla;
    return { ampleTeclat(), y, getWidth() - ampleTeclat() - gruix, juce::jmax (20, getHeight() - gruix - altCarrilEf() - 6 - y) };
}
juce::Rectangle<int> PianoRoll::zonaTeclat() const  { auto g = zonaGraella(); const int x0 = bateria() ? 0 : ampleOctava; return { x0, g.getY(), ampleTeclat() - x0, g.getHeight() }; }
juce::Rectangle<int> PianoRoll::rectOctava (bool amunt) const
{
    auto g = zonaGraella();
    return { 1, g.getY() + (amunt ? 4 : 42), ampleOctava - 2, 34 };
}
juce::Rectangle<int> PianoRoll::zonaVoraCarril() const { auto g = zonaGraella(); return { 0, g.getBottom(), getWidth(), 6 }; }
juce::Rectangle<int> PianoRoll::zonaCarril() const
{
    auto v = zonaVoraCarril();
    return { ampleTeclat(), v.getBottom(), getWidth() - ampleTeclat() - gruix, juce::jmax (10, getHeight() - gruix - v.getBottom()) };
}

PianoRoll::Zona PianoRoll::zonaA (juce::Point<int> p) const
{
    if (p.y < alcadaBarra) return Zona::Barra;
    if (zonaVoraCarril().contains (p)) return Zona::VoraCarril;
    if (zonaRegla().contains (p)) return Zona::Regla;
    if (zonaTeclat().contains (p)) return Zona::Teclat;
    if (! bateria() && p.x < ampleOctava && p.y >= zonaGraella().getY() && p.y < zonaGraella().getBottom()) return Zona::Octava;
    if (zonaGraella().contains (p)) return Zona::Graella;
    if (zonaCarril().contains (p)) return modePads() ? (zonaPads().contains (p) ? Zona::Pads : Zona::Res) : Zona::Carril;
    return Zona::Res;
}

float PianoRoll::xTick (double t) const { return (float) zonaGraella().getX() + (float) ((t - iniciVista) * pxPerTick); }
double PianoRoll::tickX (float x) const { return iniciVista + (double) (x - (float) zonaGraella().getX()) / pxPerTick; }
float PianoRoll::yTo (int to) const
{
    const int f = filaDeTo (to);
    if (f < 0) return -100000.0f;
    return (float) (zonaGraella().getY() + f * altNota - desplY);
}
int PianoRoll::filaY (float y) const
{
    return juce::jlimit (0, numFiles() - 1, (int) std::floor ((y - (float) zonaGraella().getY() + (float) desplY) / (float) altNota));
}
int PianoRoll::toY (float y) const { return toDeFila (filaY (y)); }

bool PianoRoll::bateria() const
{
    const auto* t = const_cast<PianoRoll*> (this)->pista();
    return t != nullptr && t->tipus == "bateria";
}
int PianoRoll::numFiles() const
{
    if (! bateria()) return 128;
    return juce::jmax (1, (int) const_cast<PianoRoll*> (this)->pista()->files.size());
}
int PianoRoll::filaDeTo (int to) const
{
    if (! bateria()) return 127 - juce::jlimit (0, 127, to);
    const auto& f = const_cast<PianoRoll*> (this)->pista()->files;
    for (int i = 0; i < (int) f.size(); ++i) if (f[(size_t) i].nota == to) return i;
    return -1;
}
int PianoRoll::toDeFila (int f) const
{
    if (! bateria()) return 127 - juce::jlimit (0, 127, f);
    const auto& v = const_cast<PianoRoll*> (this)->pista()->files;
    if (v.empty()) return 36;
    return v[(size_t) juce::jlimit (0, (int) v.size() - 1, f)].nota;
}
void PianoRoll::mouFiles (mibase::Clip& k, std::vector<int>& idx, int dFiles)
{
    for (int i : idx)
    {
        auto& n = k.notes[(size_t) i];
        const int f = filaDeTo (n.to);
        if (f >= 0) n.to = toDeFila (juce::jlimit (0, numFiles() - 1, f + dFiles));
    }
    midi::ordena (k.notes, idx);
}

//==============================================================================
// Clip obert
//==============================================================================
mibase::Clip* PianoRoll::clip() { return estat.clipEditor (index); }
const mibase::Clip* PianoRoll::clip() const { return const_cast<EstatEstudi&> (estat).clipEditor (index); }
mibase::Pista* PianoRoll::pista() { return clip() != nullptr ? estat.pista (estat.editors[(size_t) index].clip.pista) : nullptr; }

int PianoRoll::altCarrilEf() const
{
    // El carril de velocitats no es menja la graella quan l'editor és baix (per exemple, amb diversos editors).
    if (modePads())
        return juce::jlimit (110, 250, (getHeight() - alcadaBarra - alcadaRegla - gruix) / 2);
    return juce::jlimit (30, juce::jmax (30, altCarril), (getHeight() - alcadaBarra - alcadaRegla - gruix) / 3);
}

int PianoRoll::pistaEditor() const
{
    return clip() != nullptr ? estat.editors[(size_t) index].clip.pista : -1;
}

double PianoRoll::posTicks() const
{
    const auto* c = clip();
    return c != nullptr ? midi::ticks (estat.projecte, c->pos) : 0.0;
}

long long PianoRoll::pas() const { return midi::valorsGraella()[(size_t) graella].ticks; }

long long PianoRoll::duradaNova() const
{
    if (durada > 0) return midi::valorsGraella()[(size_t) durada].ticks;
    if (pas() > 0) return pas();
    return ultimaDurada > 0 ? ultimaDurada : ticksPerNegra / 4;
}

long long PianoRoll::enganxaAbs (double t, bool avall) const
{
    const long long p = pas();
    double s = t;
    if (p > 0) s = (avall ? std::floor (t / (double) p) : std::round (t / (double) p)) * (double) p;
    return (long long) std::llround (s - posTicks());
}

PianoRoll::Toc PianoRoll::tocA (juce::Point<float> p) const
{
    Toc t;
    const auto* c = clip();
    if (c == nullptr || ! zonaGraella().toFloat().contains (p)) return t;
    const int to = toY (p.y);
    for (int i = (int) c->notes.size(); --i >= 0;)
    {
        const auto& n = c->notes[(size_t) i];
        if (n.to != to) continue;
        const float x0 = xTick (tickAbs (n)), x1 = juce::jmax (x0 + 3.0f, xTick (tickAbs (n) + (double) n.dur));
        if (p.x < x0 - 1.0f || p.x > x1 + 1.0f) continue;
        t.nota = i;
        const float w = x1 - x0;
        const float vora = w > 18.0f ? 6.0f : w * 0.3f;
        if (p.x >= x1 - vora) t.part = Part::Fi;
        else if (w > 18.0f && p.x <= x0 + 4.0f) t.part = Part::Inici;
        return t;
    }
    return t;
}

int PianoRoll::notaCarrilA (juce::Point<float> p) const
{
    const auto* c = clip();
    if (c == nullptr) return -1;
    int millor = -1;
    float dist = 6.0f;
    for (int i = 0; i < (int) c->notes.size(); ++i)
    {
        const float x = xTick (tickAbs (c->notes[(size_t) i]));
        float d = std::abs (p.x - x - 2.0f);
        if (std::find (triadesNotes.begin(), triadesNotes.end(), i) != triadesNotes.end()) d -= 0.5f;   // les triades guanyen
        if (d < dist) { dist = d; millor = i; }
    }
    return millor;
}

//==============================================================================
// Estat
//==============================================================================
void PianoRoll::changeListenerCallback (juce::ChangeBroadcaster*)
{
    clip();   // posa al dia la pista (si s'ha mogut)
    const auto obert = index >= 0 && index < (int) estat.editors.size() ? estat.editors[(size_t) index].clip : ed::RefClip {};
    if (obert.pista != pistaVista || obert.clip != clipVist)
    {
        pistaVista = obert.pista;
        clipVist = obert.clip;
        triadesNotes.clear();
        apagaProva();
        // L'horitzontal és comuna: només s'encaixa si és l'únic editor (o encara no s'havia fet mai).
        resized();   // 0.20: la columna del teclat és més ampla al Drumpad
        if (clip() != nullptr) encaixaVista (estat.editors.size() <= 1 || ! estat.vistaEditors.feta);
    }
    {
        const bool bloquejat = index >= 0 && index < (int) estat.editors.size() && estat.editors[(size_t) index].bloquejat;
        btBloqueja.setToggleState (bloquejat, juce::dontSendNotification);
        btSolo.setToggleState (estat.soloEditor.actiu, juce::dontSendNotification);
    }
    actualitzaControlsPads();
    actualitzaFantasmes();
    netejaTriades();
    for (int i = 0; i < eines.size(); ++i)
        eines[i]->setToggleState ((int) eina == i, juce::dontSendNotification);
    btGraella.setButtonText ("Graella " + nomGraella (graella));
    btDurada.setButtonText ("Nota " + (durada == 0 ? juce::String ("= graella") : nomGraella (durada)));
    actualitzaBarres();
    repaint();
}

void PianoRoll::netejaTriades()
{
    const auto* c = clip();
    const int n = c != nullptr ? (int) c->notes.size() : 0;
    triadesNotes.erase (std::remove_if (triadesNotes.begin(), triadesNotes.end(), [n] (int i) { return i < 0 || i >= n; }), triadesNotes.end());
}

void PianoRoll::encaixaVista (bool tambeHoritzontal)
{
    const auto* c = clip();
    const auto g = zonaGraella();
    if (c == nullptr) return;
    if (g.getWidth() < 40) { vistaPendent = true; return; }
    vistaPendent = false;
    if (tambeHoritzontal)
    {
        const double a = posTicks() + midi::ticks (estat.projecte, c->inici);
        const double b = posTicks() + midi::ticks (estat.projecte, c->fi);
        pxPerTick = juce::jlimit (0.004, 2.0, (double) (g.getWidth() - 40) / juce::jmax (1.0, b - a));
        iniciVista = a - 16.0 / pxPerTick;
        estat.vistaEditors.feta = true;
        vistaCanviada();
    }

    if (bateria())
    {
        // Drumpad: que hi càpiguen totes les files (si no, com a molt 14 px per fila i es desplaça).
        altNota = juce::jlimit (14, 30, g.getHeight() / numFiles());
        desplY = 0;
        actualitzaBarres();
        return;
    }
    int toMin = 127, toMax = 0;
    for (auto& n : c->notes) { toMin = juce::jmin (toMin, n.to); toMax = juce::jmax (toMax, n.to); }
    const int centre = c->notes.empty() ? 64 : (toMin + toMax) / 2;
    desplY = juce::jlimit (0, juce::jmax (0, numFiles() * altNota - g.getHeight()), (127 - centre) * altNota - g.getHeight() / 2);
    actualitzaBarres();
}

void PianoRoll::ajustaVista() { encaixaVista (true); repaint(); }

void PianoRoll::zoomTriades()
{
    // Maj+Z (0.12): les notes triades a la pantalla (si no n'hi ha cap, tot el clip).
    const auto* c = clip();
    if (c == nullptr || triadesNotes.empty()) { ajustaVista(); return; }
    const auto g = zonaGraella();
    double a = 1e18, b = -1e18;
    int toMin = 127, toMax = 0;
    for (int i : triadesNotes)
    {
        if (i < 0 || i >= (int) c->notes.size()) continue;
        const auto& n = c->notes[(size_t) i];
        a = juce::jmin (a, tickAbs (n)); b = juce::jmax (b, tickAbs (n) + (double) n.dur);
        toMin = juce::jmin (toMin, n.to); toMax = juce::jmax (toMax, n.to);
    }
    if (b <= a) return;
    pxPerTick = juce::jlimit (0.004, 2.0, (double) (g.getWidth() - 40) / juce::jmax (1.0, b - a));
    iniciVista = a - 16.0 / pxPerTick;
    vistaCanviada();
    desplY = juce::jlimit (0, juce::jmax (0, numFiles() * altNota - g.getHeight()), (127 - (toMin + toMax) / 2) * altNota - g.getHeight() / 2);
    actualitzaBarres();
    repaint();
}

mibase::Tonalitat PianoRoll::tonalitat() const { return harmonia::tonalitatEfectiva (estat.projecte); }
bool PianoRoll::escalaActiva() const { return estat.projecte.modeEscala != ModeEscala::Visual && ! bateria(); }

juce::Colour PianoRoll::colorFila (int to, juce::Colour fons, const std::vector<int>& acordAra) const
{
    const auto t = tonalitat();
    if (! harmonia::aEscala (to, t)) fons = fons.darker (0.45f);
    else if (((to - t.to) % 12 + 12) % 12 == 0) fons = fons.interpolatedWith (tema::accent, 0.13f);
    (void) acordAra;   // 0.13: ja no es ressalta l'acord que sona (embrutava)
    if (onColorFila) fons = onColorFila (to, fons);
    return fons;
}

int PianoRoll::dToEscala (const Clip& k, const std::vector<int>& idx, int desitjat) const
{
    if (! escalaActiva()) return desitjat;
    const auto t = tonalitat();
    for (int dist = 0; dist <= 12; ++dist)
        for (int sentit : { 1, -1 })
        {
            const int d = desitjat + dist * sentit;
            bool val = true;
            for (int i : idx)
            {
                if (i < 0 || i >= (int) k.notes.size()) continue;
                const int to = k.notes[(size_t) i].to;
                if (to + d < 0 || to + d > 127) { val = false; break; }
                if (harmonia::aEscala (to, t) && ! harmonia::aEscala (to + d, t)) { val = false; break; }
            }
            if (val) return d;
            if (dist == 0) break;
        }
    return 0;
}

std::vector<int> PianoRoll::segellA (juce::Point<float> p) const
{
    auto n = estat.segell.notes;
    if (n.empty()) return n;
    const int salt = 12 * (int) std::round ((double) (toY (p.y) - n.front()) / 12.0);
    for (auto& x : n) x = juce::jlimit (0, 127, x + salt);
    return n;
}

void PianoRoll::posaSegell (juce::Point<float> p, bool mante)
{
    auto* c = clip();
    if (c == nullptr || ! estat.segell.actiu()) return;
    auto notes = segellA (p);
    const long long ini = enganxaAbs (tickX (p.x), true);
    if (estat.segell.enllaca)
    {
        // Amb moviment mínim des de l'acord d'abans d'aquest clip (si n'hi ha).
        long long millor = -(1LL << 60);
        for (auto& n : c->notes) if (n.ini < ini) millor = juce::jmax (millor, n.ini);
        std::vector<int> abans;
        for (auto& n : c->notes) if (n.ini == millor) abans.push_back (n.to);
        if (abans.size() >= 2) notes = harmonia::enllaca (abans, notes);
    }
    const auto nom = estat.segell.nom;
    const long long dur = estat.segell.durada > 0 ? estat.segell.durada : duradaNova();
    aplica ([&] (Clip& k) { triadesNotes = harmonia::afegeixAcord (k, notes, ini, dur, velNova.getValor()); },
            "Acord " + nom + " a " + posicioTick (estat.projecte, posTicks() + (double) ini) + (mante ? "" : "  (Maj+clic: continua posant-ne)"));
    escoltaAcord (notes);
    if (! mante)
    {
        estat.segell = {};
        estat.avisa();
    }
}

void PianoRoll::sonaNotes (const std::vector<int>& notes, bool encen)
{
    if (! onDirecte || pistaEditor() < 0) return;
    for (int n : notes)
        onDirecte (pistaEditor(), encen ? 0x90 : 0x80, (std::uint8_t) juce::jlimit (0, 127, n), encen ? (std::uint8_t) velNova.getValor() : 0);
}

void PianoRoll::escoltaAcord (const std::vector<int>& notes)
{
    if (! onDirecte || pistaEditor() < 0) return;
    sonaNotes (notes, true);
    juce::Component::SafePointer<PianoRoll> jo (this);
    juce::Timer::callAfterDelay (900, [jo, notes] { if (jo != nullptr) jo->sonaNotes (notes, false); });
}

void PianoRoll::insereixAcords (const std::vector<std::vector<int>>& acords, long long duradaAcord, const juce::String& nom)
{
    auto* c = clip();
    if (c == nullptr || acords.empty()) return;
    const auto& p = estat.projecte;
    const double tpc = (double) midi::ticksPerCompas (p);
    const double a = posTicks() + midi::ticks (p, c->inici), b = posTicks() + midi::ticks (p, c->fi);
    double on = midi::ticks (p, estat.capcal);
    if (on < a - 1e-6 || on >= b) on = a;
    on = std::floor (on / tpc + 1e-9) * tpc;
    const long long ini = (long long) std::llround (on - posTicks());
    aplica ([&] (Clip& k)
    {
        triadesNotes.clear();
        for (size_t i = 0; i < acords.size(); ++i)
            for (int n : harmonia::afegeixAcord (k, acords[i], ini + (long long) i * duradaAcord, duradaAcord, velNova.getValor()))
                triadesNotes.push_back (n);
        // afegeixAcord reordena: es tornen a buscar les noves.
        triadesNotes.clear();
        for (int i = 0; i < (int) k.notes.size(); ++i)
            if (k.notes[(size_t) i].ini >= ini && k.notes[(size_t) i].ini < ini + (long long) acords.size() * duradaAcord) triadesNotes.push_back (i);
    }, nom + " a partir del compàs "_ca + juce::String (ed::numCompas ((int) std::llround (on / tpc))));
}

void PianoRoll::fesAcords (int tipus, bool deTonalitat, int inversio, harmonia::Voicing v)
{
    if (clip() == nullptr || triadesNotes.empty()) { info = "Tria primer les notes que han de ser la fonamental"_ca; repaint(); return; }
    const auto t = tonalitat();
    const int quantes = (int) triadesNotes.size();
    aplica ([&] (Clip& k)
    {
        auto originals = triadesNotes;
        std::vector<long long> inicis;
        for (int i : originals) inicis.push_back (k.notes[(size_t) i].ini);
        harmonia::ferAcords (k, originals, t, tipus, deTonalitat, inversio, v);
        triadesNotes.clear();
        for (int i = 0; i < (int) k.notes.size(); ++i)
            if (std::find (inicis.begin(), inicis.end(), k.notes[(size_t) i].ini) != inicis.end()) triadesNotes.push_back (i);
    }, juce::String (quantes) + (quantes == 1 ? " nota convertida en acord" : " notes convertides en acords"));
}

void PianoRoll::inverteixTriades (bool amunt)
{
    if (clip() == nullptr || triadesNotes.size() < 2) { info = "Tria les notes d'un acord (o de diversos)"_ca; repaint(); return; }
    aplica ([&] (Clip& k) { harmonia::inverteix (k, triadesNotes, amunt); }, amunt ? "Inversió amunt"_ca : "Inversió avall"_ca);
}

void PianoRoll::enllacaTriades()
{
    if (clip() == nullptr || triadesNotes.size() < 4) { info = "Tria dos acords o més"_ca; repaint(); return; }
    aplica ([&] (Clip& k) { harmonia::enllacaTriades (k, triadesNotes); }, "Acords enllaçats amb el mínim moviment"_ca);
}

void PianoRoll::vistaCanviada()
{
    actualitzaBarres();
    if (onVistaCanviada) onVistaCanviada();
    else repaint();
}

void PianoRoll::ticTransport()
{
    if (! isShowing() || clip() == nullptr) return;
    const auto g = zonaGraella();
    const double t = midi::ticks (estat.projecte, estat.capcal);
    if (estat.transport != 0 && mode == Mode::Res && (t > tickX ((float) g.getRight() - 12.0f) || t < iniciVista))
    {
        iniciVista = t - 24.0 / pxPerTick;
        vistaCanviada();
    }
    repaint();
}

void PianoRoll::canviaOctava (int d)
{
    desplY = juce::jlimit (0, juce::jmax (0, numFiles() * altNota - zonaGraella().getHeight()), desplY - d * 12 * altNota);
    actualitzaBarres();
    repaint();
}

void PianoRoll::actualitzaBarres()
{
    const auto g = zonaGraella();
    if (g.getWidth() <= 0) return;
    const auto* c = clip();
    const double visibles = g.getWidth() / pxPerTick;
    double final = iniciVista + visibles;
    if (c != nullptr) final = juce::jmax (final, posTicks() + midi::ticks (estat.projecte, c->fi) + 4.0 * ticksPerNegra * 4);
    barraH.setRangeLimits (juce::jmin (0.0, iniciVista), final, juce::dontSendNotification);
    barraH.setCurrentRange (iniciVista, visibles, juce::dontSendNotification);
    barraV.setRangeLimits (0.0, (double) numFiles() * altNota, juce::dontSendNotification);
    barraV.setCurrentRange (desplY, g.getHeight(), juce::dontSendNotification);
}

void PianoRoll::scrollBarMoved (juce::ScrollBar* b, double v)
{
    if (b == &barraH) { iniciVista = v; vistaCanviada(); }
    else desplY = juce::jmax (0, (int) v);
    repaint();
}

void PianoRoll::resized()
{
    auto b = getLocalBounds().removeFromTop (alcadaBarra).reduced (10, 7);
    // Primer els botons de la dreta (bloqueja, desenganxa, amaga): no s'han de tapar mai.
    btTanca.setBounds (b.removeFromRight (36)); b.removeFromRight (3);
    btDesenganxa.setBounds (b.removeFromRight (36)); b.removeFromRight (3);
    btBloqueja.setBounds (b.removeFromRight (36)); b.removeFromRight (3);
    btSolo.setBounds (b.removeFromRight (36));
    b.removeFromRight (10);

    // Si l'editor és estret, els botons de graella, durada i quantitza s'estrenyen.
    const int fixos = eines.size() * 39 + 12 + 8 + 10 + 86 + 46 + selMode.ampladaIdeal() + 16 + 39;
    const int ideal = 112 + btQuantitza.ampladaIdeal() + 8;
    const float fs = juce::jlimit (0.55f, 1.0f, (float) (b.getWidth() - fixos) / (float) ideal);
    for (auto* t : eines) { t->setBounds (b.removeFromLeft (36)); b.removeFromLeft (3); }
    b.removeFromLeft (12);
    btGraella.setBounds (b.removeFromLeft (juce::roundToInt (112 * fs))); b.removeFromLeft (4);
    // 0.12: sense el botó de durada: les notes noves fan el pas de la graella (o l'última allargada, en lliure).
    btQuantitza.setBounds (b.removeFromLeft (juce::roundToInt ((btQuantitza.ampladaIdeal() + 8) * fs))); b.removeFromLeft (10);
    velNova.setBounds (b.removeFromLeft (76).reduced (0, 2)); b.removeFromLeft (10);
    btFantasmes.setBounds (b.removeFromLeft (36)); b.removeFromLeft (10);
    selMode.setBounds (b.removeFromLeft (juce::jmin (b.getWidth(), selMode.ampladaIdeal() + 6))); b.removeFromLeft (10);
    zonaInfo = b;

    const auto g = zonaGraella();
    barraV.setBounds (g.getRight(), g.getY(), gruix, g.getHeight());
    barraH.setBounds (ampleTeclat(), getHeight() - gruix, getWidth() - ampleTeclat() - gruix, gruix);
    if (vistaPendent && clip() != nullptr) encaixaVista (estat.editors.size() <= 1 || ! estat.vistaEditors.feta);
    {
        // Barra dels pads (a dalt de la zona de baix).
        auto c = zonaCarril();
        auto f = juce::Rectangle<int> (c.getX() + 6, c.getY() + 4, c.getWidth() - 12, 28);
        btPresetPads.setBounds (f.removeFromLeft (juce::jmin (330, f.getWidth() / 3))); f.removeFromLeft (6);
        btLlapisPads.setBounds (f.removeFromLeft (84)); f.removeFromLeft (10);
        octavaPads.setBounds (f.removeFromLeft (84)); f.removeFromLeft (6);
        voicingPads.setBounds (f.removeFromLeft (voicingPads.ampladaIdeal())); f.removeFromLeft (6);
        selDuradaPads.setBounds (f.removeFromLeft (selDuradaPads.ampladaIdeal())); f.removeFromLeft (6);
        btMinimPads.setBounds (f.removeFromLeft (110));
    }
    actualitzaBarres();
}

void PianoRoll::triaEina (EinaPR e)
{
    eina = e;
    for (int i = 0; i < eines.size(); ++i)
        eines[i]->setToggleState ((int) eina == i, juce::dontSendNotification);
    setMouseCursor (VistaPistes::cursor (e == EinaPR::Fletxa ? Eina::Fletxa : e == EinaPR::Llapis ? Eina::Llapis : Eina::Goma));
    repaint();
}

//==============================================================================
// So de prova
//==============================================================================
void PianoRoll::notaOn (int to, int vel)
{
    if (! onDirecte || pistaEditor() < 0) return;
    if (toSonant >= 0) notaOff (toSonant);
    if (modePads() && mode == Mode::Teclat)
    {
        // Amb el Chord Pad, el teclat toca l'acord del pad de la nota (en qualsevol octava).
        notesTeclatPad = notesDelPad (((to % 12) + 12) % 12);
        for (int n : notesTeclatPad)
            onDirecte (pistaEditor(), 0x90, (std::uint8_t) n, (std::uint8_t) juce::jlimit (1, 127, vel));
        padPremut = ((to % 12) + 12) % 12;
        repaint (zonaCarril());
    }
    else
        onDirecte (pistaEditor(), 0x90, (std::uint8_t) juce::jlimit (0, 127, to), (std::uint8_t) juce::jlimit (1, 127, vel));
    toSonant = to;
    premudes[(size_t) juce::jlimit (0, 127, to)] = true;
    repaint (zonaTeclat());
}

void PianoRoll::notaOff (int to)
{
    if (! onDirecte || to < 0) return;
    if (! notesTeclatPad.empty() && to == toSonant)
    {
        for (int n : notesTeclatPad) onDirecte (pistaEditor(), 0x80, (std::uint8_t) n, 0);
        notesTeclatPad.clear();
        padPremut = -1;
        repaint (zonaCarril());
    }
    else
        onDirecte (pistaEditor(), 0x80, (std::uint8_t) juce::jlimit (0, 127, to), 0);
    if (to == toSonant) toSonant = -1;
    premudes[(size_t) juce::jlimit (0, 127, to)] = false;
    repaint (zonaTeclat());
}

void PianoRoll::apagaProva()
{
    if (toSonant >= 0) notaOff (toSonant);
    premudes.fill (false);
}

void PianoRoll::sonaBreu (int to, int vel)
{
    if (! onDirecte || pistaEditor() < 0) return;
    const int p = pistaEditor();
    onDirecte (p, 0x90, (std::uint8_t) juce::jlimit (0, 127, to), (std::uint8_t) juce::jlimit (1, 127, vel));
    juce::Component::SafePointer<PianoRoll> jo (this);
    juce::Timer::callAfterDelay (220, [jo, p, to]
    {
        if (jo != nullptr && jo->onDirecte) jo->onDirecte (p, 0x80, (std::uint8_t) juce::jlimit (0, 127, to), 0);
    });
}

//==============================================================================
// Accions
//==============================================================================
void PianoRoll::aplica (const std::function<void (Clip&)>& f, const juce::String& text)
{
    if (clip() == nullptr) return;
    comencaCanvi();
    restaura();
    f (*clip());
    midi::encabeix (estat.projecte, *clip());
    info = text;
    acabaCanvi();
    repaint();
}

void PianoRoll::ordreBateria (const std::function<void (Clip&, std::vector<int>&, long long, long long, long long)>& f, const juce::String& nom)
{
    const auto* c = clip();
    if (c == nullptr) return;
    const long long despl = (long long) std::llround (posTicks());
    const long long iniT = (long long) std::llround (midi::ticks (estat.projecte, c->inici));
    const long long fiT = (long long) std::llround (midi::ticks (estat.projecte, c->fi));
    aplica ([&] (Clip& k) { f (k, triadesNotes, despl, iniT, fiT); }, nom);
    netejaTriades();
}

void PianoRoll::quantitza (bool tambeFinal, double forca)
{
    auto* c = clip();
    if (c == nullptr || c->notes.empty()) return;
    const long long p = pas() > 0 ? pas() : ticksPerNegra / 4;
    const bool totes = triadesNotes.empty();
    const int quantes = totes ? (int) c->notes.size() : (int) triadesNotes.size();
    // Es quantitza en temps absolut (la graella de la cançó), encara que el clip no comenci just en un compàs.
    aplica ([&] (Clip& k)
    {
        std::vector<int> idx = triadesNotes;
        if (totes) { idx.resize (k.notes.size()); std::iota (idx.begin(), idx.end(), 0); }
        const long long despl = (long long) std::llround (posTicks());
        for (int i : idx) k.notes[(size_t) i].ini += despl;
        midi::quantitza (k, idx, p, forca, tambeFinal);
        for (int i : idx) k.notes[(size_t) i].ini -= despl;
        if (! totes) triadesNotes = idx;
    }, "Quantitzat a "_ca + (pas() > 0 ? nomGraella (graella) : juce::String ("1/16")) + (tambeFinal ? " (inici i final)" : "")
       + " · "_ca + juce::String (quantes) + (quantes == 1 ? " nota" : " notes"));
}

void PianoRoll::esborraTriades()
{
    if (triadesNotes.empty()) return;
    const int n = (int) triadesNotes.size();
    auto t = triadesNotes;
    triadesNotes.clear();
    aplica ([t] (Clip& k) { midi::esborra (k, t); }, juce::String (n) + (n == 1 ? " nota esborrada" : " notes esborrades"));
}

void PianoRoll::duplicaTriades()
{
    if (triadesNotes.empty()) return;
    aplica ([this] (Clip& k) { triadesNotes = midi::duplica (k, triadesNotes, pas()); }, "Duplicat");
}

void PianoRoll::copia (bool retalla)
{
    const auto* c = clip();
    if (c == nullptr || triadesNotes.empty()) return;
    portaretalls = midi::copia (*c, triadesNotes);
    info = juce::String (portaretalls.notes.size()) + (retalla ? " notes retallades" : " notes copiades");
    if (retalla) esborraTriades();
    repaint();
}

void PianoRoll::enganxa()
{
    auto* c = clip();
    if (c == nullptr || portaretalls.buit()) return;
    // Al capçal si cau dins la vista; si no, al començament de la vista.
    double on = midi::ticks (estat.projecte, estat.capcal);
    const auto g = zonaGraella();
    if (on < iniciVista || on > tickX ((float) g.getRight())) on = tickX ((float) g.getX() + 20.0f);
    const long long tick = enganxaAbs (on, true);
    aplica ([this, tick] (Clip& k) { triadesNotes = midi::enganxa (k, portaretalls, tick); }, "Enganxat");
}

void PianoRoll::transposaTriades (int semitons)
{
    if (triadesNotes.empty()) return;
    if (bateria())
    {
        // Drumpad: amunt i avall = a la fila de sobre o de sota.
        aplica ([this, semitons] (Clip& k) { mouFiles (k, triadesNotes, semitons > 0 ? -1 : 1); }, semitons > 0 ? "Fila de sobre"_ca : "Fila de sota"_ca);
        return;
    }
    if (escalaActiva() && std::abs (semitons) == 1)
    {
        // Assistit o bloquejat: un grau de l'escala amunt o avall (cada nota per separat).
        const auto t = tonalitat();
        aplica ([&] (Clip& k)
        {
            for (int i : triadesNotes) k.notes[(size_t) i].to = harmonia::grau (k.notes[(size_t) i].to, t, semitons);
            midi::ordena (k.notes, triadesNotes);
        }, semitons > 0 ? "Un grau amunt" : "Un grau avall");
    }
    else
        aplica ([this, semitons] (Clip& k) { midi::transposa (k, triadesNotes, semitons); }, semitons > 0 ? "Amunt" : "Avall");
    if (const auto* c = clip(); c != nullptr && triadesNotes.size() == 1)
        sonaBreu (c->notes[(size_t) triadesNotes[0]].to, c->notes[(size_t) triadesNotes[0]].vel);
}

void PianoRoll::mouTriades (int passos)
{
    if (triadesNotes.empty()) return;
    const long long p = pas() > 0 ? pas() : ticksPerNegra / 4;
    aplica ([this, passos, p] (Clip& k)
    {
        midi::mou (k, triadesNotes, passos * p, 0, -(long long) std::llround (posTicks()));
    }, passos > 0 ? "Endavant" : "Enrere");
}

void PianoRoll::allargaTriades (int passos)
{
    if (triadesNotes.empty()) return;
    const long long p = pas() > 0 ? pas() : ticksPerNegra / 4;
    aplica ([this, passos, p] (Clip& k) { midi::canviaDurada (k, triadesNotes, passos * p, false, p); }, passos > 0 ? "Més llarga"_ca : "Més curta"_ca);
}

void PianoRoll::triaTotes()
{
    const auto* c = clip();
    if (c == nullptr) return;
    triadesNotes.resize (c->notes.size());
    std::iota (triadesNotes.begin(), triadesNotes.end(), 0);
    info = juce::String (triadesNotes.size()) + " notes triades";
    repaint();
}

void PianoRoll::menuGraella()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Graella i quantització"_ca);
    const auto& v = midi::valorsGraella();
    for (int i = 1; i < (int) v.size(); ++i)
    {
        if (i == 7) m.addSeparator();
        m.addItem (i, txt (v[(size_t) i].nom) + (i >= 7 ? "  (tresets)" : ""), true, graella == i);
    }
    m.addSeparator();
    m.addItem (100, "Lliure (sense graella)", true, graella == 0);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btGraella), [this] (int r)
    {
        if (r == 0) return;
        graella = r == 100 ? 0 : r;
        changeListenerCallback (nullptr);
    });
}

void PianoRoll::menuDurada()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Durada de les notes noves");
    m.addItem (100, "Igual que la graella", true, durada == 0);
    m.addSeparator();
    const auto& v = midi::valorsGraella();
    for (int i = 1; i < (int) v.size(); ++i)
    {
        if (i == 7) m.addSeparator();
        m.addItem (i, txt (v[(size_t) i].nom), true, durada == i);
    }
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btDurada), [this] (int r)
    {
        if (r == 0) return;
        durada = r == 100 ? 0 : r;
        changeListenerCallback (nullptr);
    });
}

void PianoRoll::menuNota (int nota)
{
    juce::ignoreUnused (nota);
    const bool hiHa = ! triadesNotes.empty();
    juce::PopupMenu vel;
    for (int v : { 127, 110, 100, 80, 64, 40 }) vel.addItem (200 + v, juce::String (v));
    vel.addSeparator();
    vel.addItem (10, "+10");
    vel.addItem (11, "-10");

    juce::PopupMenu m;
    m.addItem (1, "Copia   (Ctrl+C)", hiHa);
    m.addItem (2, "Duplica   (Ctrl+D)", hiHa);
    m.addItem (3, "Esborra   (Supr)", hiHa);
    m.addSeparator();
    m.addItem (4, "Quantitza l'inici   (Q)"_ca, hiHa);
    m.addItem (5, "Quantitza l'inici i el final   (Maj+Q)"_ca, hiHa);
    m.addItem (6, "Quantitza a mitges (50 %)", hiHa);
    m.addItem (7, "Totes amb la durada de la graella", hiHa && pas() > 0);
    m.addSubMenu ("Velocitat", vel, hiHa);
    m.addSeparator();
    m.addItem (8, "Puja una octava   (Maj+Amunt)", hiHa);
    m.addItem (9, "Baixa una octava   (Maj+Avall)", hiHa);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this] (int r)
    {
        switch (r)
        {
            case 1: copia (false); break;
            case 2: duplicaTriades(); break;
            case 3: esborraTriades(); break;
            case 4: quantitza (false); break;
            case 5: quantitza (true); break;
            case 6: quantitza (false, 0.5); break;
            case 7: aplica ([this] (Clip& k) { midi::fixaDurada (k, triadesNotes, pas()); }, "Durada " + nomGraella (graella)); break;
            case 8: transposaTriades (12); break;
            case 9: transposaTriades (-12); break;
            case 10: aplica ([this] (Clip& k) { midi::sumaVelocitat (k, triadesNotes, 10); }, "Velocitat +10"); break;
            case 11: aplica ([this] (Clip& k) { midi::sumaVelocitat (k, triadesNotes, -10); }, "Velocitat -10"); break;
            default:
                if (r >= 200) aplica ([this, r] (Clip& k) { midi::velocitat (k, triadesNotes, r - 200); }, "Velocitat " + juce::String (r - 200));
                break;
        }
    });
}

//==============================================================================
// Ratolí
//==============================================================================
void PianoRoll::mouseMove (const juce::MouseEvent& e)
{
    ratoli = e.position;
    if (estat.segell.actiu()) repaint (zonaGraella());
    const auto z = zonaA (e.getPosition());
    Toc t = z == Zona::Graella ? tocA (e.position) : Toc {};
    const int c = z == Zona::Carril ? notaCarrilA (e.position) : -1;
    const int tecla = (z == Zona::Teclat || z == Zona::Graella) ? toY (e.position.y) : -1;
    const int ps = z == Zona::Pads ? padA (e.getPosition()) : -1;
    if (ps != padSobre) { padSobre = ps; repaint (zonaCarril()); }
    if (t.nota != sobre.nota || t.part != sobre.part || c != sobreCarril || tecla != sobreTecla)
    {
        sobre = t;
        sobreCarril = c;
        sobreTecla = tecla;
        repaint();
    }
    switch (z)
    {
        case Zona::VoraCarril: setMouseCursor (juce::MouseCursor::UpDownResizeCursor); break;
        case Zona::Teclat: case Zona::Regla: case Zona::Octava: setMouseCursor (juce::MouseCursor::PointingHandCursor); break;
        case Zona::Carril: setMouseCursor (c >= 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::CrosshairCursor); break;
        case Zona::Graella:
            if (t.nota >= 0 && eina != EinaPR::Goma)
                setMouseCursor (t.part == Part::Cos ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::LeftRightResizeCursor);
            else
                setMouseCursor (VistaPistes::cursor (eina == EinaPR::Fletxa ? Eina::Fletxa : eina == EinaPR::Llapis ? Eina::Llapis : Eina::Goma));
            break;
        case Zona::Pads: setMouseCursor (btLlapisPads.getToggleState() ? VistaPistes::cursor (Eina::Llapis) : juce::MouseCursor::PointingHandCursor); break;
        case Zona::Barra: case Zona::Res: setMouseCursor (juce::MouseCursor::NormalCursor); break;
    }
}

void PianoRoll::mouseExit (const juce::MouseEvent&)
{
    ratoli = { -1.0f, -1.0f };
    sobre = {};
    sobreCarril = sobreTecla = -1;
    repaint();
}

void PianoRoll::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    inici = ara = e.position;
    mode = Mode::Res;
    const auto z = zonaA (e.getPosition());
    auto* c = clip();
    if (estat.editorActiu != index)
    {
        estat.editorActiu = index;
        if (onActiu) onActiu();
    }

    if (z == Zona::Octava)
    {
        if (rectOctava (true).contains (e.getPosition())) { octavaPremuda = 1; canviaOctava (1); }
        else if (rectOctava (false).contains (e.getPosition())) { octavaPremuda = -1; canviaOctava (-1); }
        return;
    }

    if (z == Zona::VoraCarril) { mode = Mode::VoraCarril; alt0 = altCarrilEf(); return; }
    if (c == nullptr) return;

    if (z == Zona::Teclat && bateria())
    {
        // Drumpad: clic al nom = tria la fila (el panell de l'esquerra l'edita) i la fa sonar; a la «M» = la silencia.
        const int f = filaY (e.position.y);
        auto* t = pista();
        if (t == nullptr || f < 0 || f >= (int) t->files.size()) return;
        estat.filaBateria = f;
        if (e.position.x > (float) zonaTeclat().getRight() - 22.0f)
        {
            estat.desfes.abans (estat.projecte);
            t->files[(size_t) f].mut = ! t->files[(size_t) f].mut;
            estat.canviat();
            return;
        }
        mode = Mode::Teclat;
        notaOn (t->files[(size_t) f].nota, juce::jlimit (30, 127, 30 + (int) (97.0f * e.position.x / (float) ampleTeclat())));
        estat.avisa();
        return;
    }
    if (z == Zona::Teclat)
    {
        mode = Mode::Teclat;
        const int vel = juce::jlimit (30, 127, 30 + (int) (97.0f * e.position.x / (float) ampleTeclat()));
        notaOn (toY (e.position.y), vel);
        return;
    }
    if (z == Zona::Regla)
    {
        mode = Mode::Regla;
        reglaX0 = e.position.x;
        reglaBucle = false;
        estat.mouCapcal (juce::jmax (0.0, midi::segons (estat.projecte, tickX (e.position.x))), true);
        estat.avisa();
        return;
    }
    if (z == Zona::Pads)
    {
        const int i = padA (e.getPosition());
        if (i < 0) return;
        if (e.mods.isPopupMenu() || btLlapisPads.getToggleState()) { obreEditorPad (i); return; }
        notesPadPremut = notesDelPad (i);
        if (notesPadPremut.empty()) { obreEditorPad (i); return; }   // pad buit: es tria l'acord
        padPremut = i;
        mode = Mode::Pad;
        sonaNotes (notesPadPremut, true);
        info = juce::String::fromUTF8 (padsActuals()[(size_t) i].c_str()) + ": arrossega'l al Piano Roll per posar-lo"_ca;
        repaint();
        return;
    }
    if (z == Zona::Carril)
    {
        comencaCanvi();
        velIni.clear();
        for (auto& n : c->notes) velIni.push_back (n.vel);
        const int n = notaCarrilA (e.position);
        ultimVel = e.position;
        if (e.mods.isShiftDown() || n < 0)
            mode = Mode::LiniaVel;   // Maj: línia recta (crescendo, diminuendo); en un lloc buit: a mà alçada
        else
        {
            mode = Mode::Velocitat;
            toc0.nota = n;
        }
        mouseDrag (e);
        return;
    }
    if (z != Zona::Graella) return;

    toc0 = tocA (e.position);
    tick0 = tickX (e.position.x);
    to0 = toY (e.position.y);

    if (estat.segell.actiu())
    {
        if (e.mods.isPopupMenu()) { estat.segell = {}; info = "Acord descartat"; estat.avisa(); return; }
        posaSegell (e.position, e.mods.isShiftDown());
        return;
    }

    if (e.mods.isPopupMenu())
    {
        if (toc0.nota >= 0)
        {
            if (std::find (triadesNotes.begin(), triadesNotes.end(), toc0.nota) == triadesNotes.end())
                triadesNotes = { toc0.nota };
            repaint();
            menuNota (toc0.nota);
        }
        else
        {
            triaEina (EinaPR::Fletxa);
            info = "Fletxa";
        }
        return;
    }

    if (eina == EinaPR::Goma)
    {
        comencaCanvi();
        mode = Mode::Goma;
        mouseDrag (e);
        return;
    }

    if (toc0.nota >= 0)
    {
        // Sobre una nota (amb la fletxa o amb el llapis): triar, moure o allargar.
        auto& v = triadesNotes;
        const bool jaTriada = std::find (v.begin(), v.end(), toc0.nota) != v.end();
        if (e.mods.isCommandDown())
        {
            if (jaTriada) v.erase (std::find (v.begin(), v.end(), toc0.nota)); else v.push_back (toc0.nota);
            if (jaTriada) { repaint(); return; }
        }
        else if (e.mods.isShiftDown()) { if (! jaTriada) v.push_back (toc0.nota); }
        else if (! jaTriada) v = { toc0.nota };

        const auto& n = c->notes[(size_t) toc0.nota];
        ini0 = n.ini;
        dur0 = n.dur;
        to0 = n.to;
        comencaCanvi();
        triadesInici = triadesNotes;
        mode = toc0.part == Part::Fi ? Mode::DuradaFi : toc0.part == Part::Inici ? Mode::DuradaInici : Mode::Decidint;
        notaOn (n.to, n.vel);
        info = txt (midi::nomNota (n.to)) + " · " + posicioTick (estat.projecte, tickAbs (n)) + " · vel " + juce::String (n.vel);
        repaint();
        return;
    }

    if (eina == EinaPR::Llapis)
    {
        // Nota nova on es clica (enganxada a la graella), amb la durada triada.
        int to = to0;
        if (estat.projecte.modeEscala == ModeEscala::Bloquejat && ! bateria() && ! harmonia::aEscala (to, tonalitat()))
        {
            info = "Bloquejat: "_ca + txt (midi::nomNota (to)) + " no és de l'escala ("_ca + txt (harmonia::nomTonalitat (tonalitat())) + ")";
            repaint();
            return;
        }
        if (estat.projecte.modeEscala == ModeEscala::Assistit && ! bateria()) to = harmonia::ajusta (to, tonalitat());
        if (bateria()) estat.filaBateria = filaY (e.position.y);
        comencaCanvi();
        restaura();
        ini0 = enganxaAbs (tick0, true);
        dur0 = duradaNova();
        to0 = to;
        notaNova = midi::afegeix (*clip(), { ini0, dur0, to, velNova.getValor(), 0 });
        midi::encabeix (estat.projecte, *clip());
        triadesNotes = { notaNova };
        mode = Mode::Dibuixa;
        notaOn (to, velNova.getValor());
        info = txt (midi::nomNota (to)) + " · " + posicioTick (estat.projecte, posTicks() + (double) ini0);
        estat.avisa();
        return;
    }

    // Fletxa en un lloc buit: es treu la selecció i, arrossegant, es fa un requadre.
    if (! e.mods.isCommandDown() && ! e.mods.isShiftDown()) triadesNotes.clear();
    triadesInici = triadesNotes;
    mode = Mode::Requadre;
    repaint();
}

void PianoRoll::mouseDrag (const juce::MouseEvent& e)
{
    ara = e.position;
    auto* c = clip();
    const bool senseGraella = e.mods.isAltDown();
    const long long p = senseGraella ? 0 : pas();
    auto enganxa = [p] (double t) { return p > 0 ? std::round (t / (double) p) * (double) p : t; };

    switch (mode)
    {
        case Mode::Res: return;

        case Mode::Pad:
            if (e.getDistanceFromDragStart() > 6 && padPremut >= 0)
            {
                sonaNotes (notesPadPremut, false);
                const int i = padPremut;
                padPremut = -1;
                mode = Mode::Res;
                arrossegaAcord (*this, estat, notesPadPremut, juce::String::fromUTF8 (padsActuals()[(size_t) i].c_str()), duradaPads(),
                                btMinimPads.getToggleState());
                repaint();
            }
            return;

        case Mode::VoraCarril:
            altCarril = juce::jlimit (40, juce::jmax (60, getHeight() / 2), alt0 - e.getDistanceFromDragStartY());
            resized();
            repaint();
            return;

        case Mode::Teclat:
        {
            const int to = toY (e.position.y);
            if (to != toSonant) notaOn (to, juce::jlimit (30, 127, 30 + (int) (97.0f * e.position.x / (float) ampleTeclat())));
            return;
        }

        case Mode::Regla:
            // 0.24: arrossegant la regla es fa el bucle (enganxat a la graella del projecte).
            if (! reglaBucle && std::abs (e.position.x - reglaX0) > 5.0f) { reglaBucle = true; comencaCanvi(); }
            if (reglaBucle)
            {
                restaura();
                estat.bucleEntre (midi::segons (estat.projecte, tickX (reglaX0)), midi::segons (estat.projecte, tickX (e.position.x)));
                info = "Bucle"_ca;
            }
            else estat.mouCapcal (juce::jmax (0.0, midi::segons (estat.projecte, tickX (e.position.x))), true);
            estat.avisa();
            return;

        case Mode::Requadre:
        {
            if (c == nullptr || e.getDistanceFromDragStart() < 3) return;
            const auto r = juce::Rectangle<float> (inici, ara);
            auto dins = midi::notesDins (*c, (long long) std::floor (tickX (r.getX()) - posTicks()), (long long) std::ceil (tickX (r.getRight()) - posTicks()),
                                         toY (r.getBottom()), toY (r.getY()));
            triadesNotes = triadesInici;
            for (int i : dins) if (std::find (triadesNotes.begin(), triadesNotes.end(), i) == triadesNotes.end()) triadesNotes.push_back (i);
            info = juce::String (triadesNotes.size()) + (triadesNotes.size() == 1 ? " nota triada" : " notes triades");
            repaint();
            return;
        }

        case Mode::Decidint:
            if (e.getDistanceFromDragStart() < 4) return;
            mode = Mode::Mou;
            [[fallthrough]];
        case Mode::Mou:
        {
            if (c == nullptr) return;
            restaura();
            triadesNotes = triadesInici;
            const double dT = tickX (e.position.x) - tick0;
            const double iniAbs = enganxa (posTicks() + (double) ini0 + dT);
            const long long dTicks = (long long) std::llround (iniAbs - posTicks()) - ini0;
            int dTo = toY (e.position.y) - toY (inici.y);
            if (bateria())
            {
                // Drumpad: es mou per files (cada cop va a la fila de sota o de sobre, encara que les notes no siguin seguides).
                const int dF = filaY (e.position.y) - filaY (inici.y);
                midi::mou (*clip(), triadesNotes, dTicks, 0, -(long long) std::llround (posTicks()));
                mouFiles (*clip(), triadesNotes, dF);
                midi::encabeix (estat.projecte, *clip());
                const int toAra = toDeFila (juce::jlimit (0, numFiles() - 1, filaDeTo (to0) + dF));
                if (toAra != toSonant && toSonant >= 0) notaOn (toAra, velNova.getValor());
                info = posicioTick (estat.projecte, posTicks() + (double) (ini0 + dTicks))
                     + (triadesNotes.size() > 1 ? " · "_ca + juce::String (triadesNotes.size()) + " cops"_ca : juce::String());
                estat.avisa();
                return;
            }
            if (escalaActiva() && dTo != 0 && triadesNotes.size() > 1)
            {
                // 0.14: un grup en assistit o bloquejat es mou per graus de l'escala: cada nota va on toca dins la
                // tonalitat (algunes pugen o baixen un semitò més o menys que d'altres).
                const auto t = tonalitat();
                const int base = harmonia::aEscala (to0, t) ? to0 : harmonia::ajusta (to0, t);
                const int desti = harmonia::ajusta (juce::jlimit (0, 127, to0 + dTo), t);
                const int graus = harmonia::grausEntre (base, desti, t);
                midi::mou (*clip(), triadesNotes, dTicks, 0, -(long long) std::llround (posTicks()));
                for (int i : triadesNotes)
                {
                    auto& n = clip()->notes[(size_t) i];
                    n.to = harmonia::aEscala (n.to, t) ? harmonia::mouGraus (n.to, t, graus) : juce::jlimit (0, 127, n.to + desti - base);
                }
                midi::ordena (clip()->notes, triadesNotes);
                dTo = desti - to0;
            }
            else
            {
                if (escalaActiva() && dTo != 0) dTo = dToEscala (*clip(), triadesNotes, dTo);   // una nota: només a notes de l'escala
                midi::mou (*clip(), triadesNotes, dTicks, dTo, -(long long) std::llround (posTicks()));
            }
            midi::encabeix (estat.projecte, *clip());
            const int toAra = juce::jlimit (0, 127, to0 + dTo);
            if (toAra != toSonant && toSonant >= 0) notaOn (toAra, velNova.getValor());
            info = txt (midi::nomNota (toAra)) + " · " + posicioTick (estat.projecte, posTicks() + (double) (ini0 + dTicks))
                 + (triadesNotes.size() > 1 ? " · "_ca + juce::String (triadesNotes.size()) + " notes" : juce::String());
            estat.avisa();
            return;
        }

        case Mode::DuradaFi:
        case Mode::DuradaInici:
        {
            if (c == nullptr) return;
            restaura();
            triadesNotes = triadesInici;
            const double dT = tickX (e.position.x) - tick0;
            if (mode == Mode::DuradaFi)
            {
                const double fiAbs = enganxa (posTicks() + (double) (ini0 + dur0) + dT);
                long long nova = (long long) std::llround (fiAbs - posTicks()) - ini0;
                if (nova < 1) nova = p > 0 ? p : 10;
                midi::canviaDurada (*clip(), triadesNotes, nova - dur0, false, 1);
                ultimaDurada = nova;
                info = "Durada: "_ca + juce::String ((double) nova / ticksPerNegra, 2).replaceCharacter ('.', ',') + " temps";
            }
            else
            {
                const double iniAbs = enganxa (posTicks() + (double) ini0 + dT);
                const long long d = (long long) std::llround (iniAbs - posTicks()) - ini0;
                midi::canviaDurada (*clip(), triadesNotes, d, true, 1);
                info = "Inici: "_ca + posicioTick (estat.projecte, posTicks() + (double) (ini0 + d));
            }
            midi::encabeix (estat.projecte, *clip());
            estat.avisa();
            return;
        }

        case Mode::Dibuixa:
        {
            if (c == nullptr) return;
            restaura();
            if (bateria())
            {
                // Drumpad: arrossegant amb el llapis es pinten cops a cada casella de la graella, a la mateixa fila.
                const long long pd = duradaNova();
                const double iniAbs = posTicks() + (double) ini0;
                const double fiAbs = enganxa (tickX (e.position.x));
                const long long n = (long long) std::floor (std::abs (fiAbs - iniAbs) / (double) pd + 1e-9);
                const long long sentit = fiAbs >= iniAbs ? 1 : -1;
                triadesNotes.clear();
                for (long long k = 0; k <= n; ++k)
                {
                    const long long ini = ini0 + sentit * k * pd;
                    if (ini < 0) break;
                    const bool hiEs = std::any_of (clip()->notes.begin(), clip()->notes.end(), [&] (const Nota& x) { return x.ini == ini && x.to == to0; });
                    if (! hiEs) triadesNotes.push_back (midi::afegeix (*clip(), { ini, pd, to0, velNova.getValor(), 0 }));
                }
                midi::encabeix (estat.projecte, *clip());
                info = juce::String ((int) n + 1) + " cops"_ca;
                estat.avisa();
                return;
            }
            const double fiAbs = tickX (e.position.x);
            const double iniAbs = posTicks() + (double) ini0;
            long long dur = duradaNova();
            if (fiAbs > iniAbs + 2.0 / pxPerTick)
            {
                const double d = fiAbs - iniAbs;
                dur = p > 0 ? (long long) std::ceil (d / (double) p - 1e-9) * p : (long long) std::llround (d);
                dur = juce::jmax (p > 0 ? p : 10LL, dur);
                ultimaDurada = dur;
            }
            notaNova = midi::afegeix (*clip(), { ini0, dur, to0, velNova.getValor(), 0 });
            midi::encabeix (estat.projecte, *clip());
            triadesNotes = { notaNova };
            info = txt (midi::nomNota (to0)) + " · durada "_ca + juce::String ((double) dur / ticksPerNegra, 2).replaceCharacter ('.', ',') + " temps";
            estat.avisa();
            return;
        }

        case Mode::Goma:
        {
            if (c == nullptr) return;
            const auto t = tocA (e.position);
            if (t.nota >= 0)
            {
                midi::esborra (*c, { t.nota });
                canviFet = true;
                triadesNotes.clear();
                info = "Esborrada";
                estat.avisa();
            }
            return;
        }

        case Mode::Velocitat:
        case Mode::LiniaVel:
        {
            if (c == nullptr) return;
            const auto zc = zonaCarril().toFloat().reduced (0.0f, 5.0f);
            auto velY = [&zc] (float y) { return juce::jlimit (1, 127, 1 + juce::roundToInt (126.0f * (zc.getBottom() - y) / zc.getHeight())); };
            if (mode == Mode::Velocitat)
            {
                restaura();
                auto& k = *clip();
                if ((int) velIni.size() != (int) k.notes.size()) return;
                const int n = toc0.nota;
                const bool grup = triadesNotes.size() > 1 && std::find (triadesNotes.begin(), triadesNotes.end(), n) != triadesNotes.end();
                const int v = velY (e.position.y);
                if (grup)
                {
                    const int d = v - velIni[(size_t) n];
                    for (int i : triadesNotes) k.notes[(size_t) i].vel = juce::jlimit (1, 127, velIni[(size_t) i] + d);
                    info = "Velocitat "_ca + (d >= 0 ? "+" : "") + juce::String (d) + " · " + juce::String (triadesNotes.size()) + " notes";
                }
                else
                {
                    k.notes[(size_t) n].vel = v;
                    info = "Velocitat " + juce::String (v);
                }
            }
            else
            {
                // Línia: Maj = recta des d'on s'ha començat (crescendo o diminuendo); si no, a mà alçada.
                const bool recta = e.mods.isShiftDown();
                if (recta) restaura();
                auto& k = *clip();
                const auto a = recta ? inici : ultimVel, b = e.position;
                const float x0 = juce::jmin (a.x, b.x), x1 = juce::jmax (a.x, b.x);
                const bool nomesTriades = ! triadesNotes.empty();
                int tocades = 0;
                for (int i = 0; i < (int) k.notes.size(); ++i)
                {
                    if (nomesTriades && std::find (triadesNotes.begin(), triadesNotes.end(), i) == triadesNotes.end()) continue;
                    const float x = xTick (tickAbs (k.notes[(size_t) i]));
                    if (x < x0 - 2.0f || x > x1 + 2.0f) continue;
                    const float f = x1 - x0 < 1.0f ? 1.0f : (x - a.x) / (b.x - a.x);
                    k.notes[(size_t) i].vel = velY (a.y + (b.y - a.y) * juce::jlimit (0.0f, 1.0f, f));
                    ++tocades;
                }
                if (tocades > 0) canviFet = true;
                ultimVel = e.position;
                info = recta ? "Línia de velocitats (amolla per acabar)"_ca : "Velocitat " + juce::String (velY (e.position.y));
            }
            estat.avisa();
            return;
        }
    }
}

void PianoRoll::mouseUp (const juce::MouseEvent& e)
{
    octavaPremuda = 0;
    if (mode == Mode::Regla && reglaBucle)
    {
        reglaBucle = false;
        mode = Mode::Res;
        acabaCanvi ("Bucle des de l'editor"_ca);
        return;
    }
    switch (mode)
    {
        case Mode::Pad:
            sonaNotes (notesPadPremut, false);
            padPremut = -1;
            break;
        case Mode::Teclat:
            apagaProva();
            break;
        case Mode::Decidint:
            if (! e.mods.isCommandDown() && ! e.mods.isShiftDown() && toc0.nota >= 0) triadesNotes = { toc0.nota };
            canviFet = false;
            apagaProva();
            break;
        case Mode::Mou: case Mode::DuradaFi: case Mode::DuradaInici: case Mode::Dibuixa:
            apagaProva();
            acabaCanvi();
            break;
        case Mode::Goma: case Mode::Velocitat: case Mode::LiniaVel:
            acabaCanvi();
            break;
        case Mode::Requadre:
            if (triadesNotes.empty()) info.clear();
            break;
        case Mode::Res: case Mode::VoraCarril: case Mode::Regla:
            break;
    }
    mode = Mode::Res;
    repaint();
}

void PianoRoll::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (zonaA (e.getPosition()) != Zona::Graella || clip() == nullptr || eina != EinaPR::Fletxa) return;
    const auto t = tocA (e.position);
    if (t.nota >= 0)
    {
        triadesNotes.clear();
        aplica ([t] (Clip& k) { midi::esborra (k, { t.nota }); }, "Nota esborrada");
        return;
    }
    int to = toY (e.position.y);
    if (estat.projecte.modeEscala == ModeEscala::Bloquejat && ! harmonia::aEscala (to, tonalitat()))
    {
        info = "Bloquejat: "_ca + txt (midi::nomNota (to)) + " no és de l'escala"_ca;
        repaint();
        return;
    }
    if (estat.projecte.modeEscala == ModeEscala::Assistit) to = harmonia::ajusta (to, tonalitat());
    const long long ini = enganxaAbs (tickX (e.position.x), true);
    aplica ([this, ini, to] (Clip& k) { triadesNotes = { midi::afegeix (k, { ini, duradaNova(), to, velNova.getValor(), 0 }) }; },
            "Nota nova: " + txt (midi::nomNota (to)));
    sonaBreu (to, velNova.getValor());
}

void PianoRoll::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float d = std::abs (w.deltaY) >= std::abs (w.deltaX) ? w.deltaY : w.deltaX;
    if (std::abs (d) < 1.0e-6f) return;
    const auto g = zonaGraella();
    if (e.mods.isCommandDown())
    {
        // Zoom del temps a partir del ratolí (el punt de sota el ratolí no es mou).
        const double t = tickX (e.position.x);
        pxPerTick = juce::jlimit (0.004, 3.0, pxPerTick * (d > 0.0f ? 1.25 : 1.0 / 1.25));
        iniciVista = t - (double) (e.position.x - (float) g.getX()) / pxPerTick;
    }
    else if (e.mods.isAltDown())
    {
        const int to = toY (e.position.y);
        const float rel = e.position.y - yTo (to);
        altNota = juce::jlimit (6, 40, altNota + (d > 0.0f ? 2 : -2));
        desplY = (127 - to) * altNota + (int) rel - (int) (e.position.y - (float) g.getY());
    }
    else if (e.mods.isShiftDown() || std::abs (w.deltaX) > std::abs (w.deltaY))
        iniciVista -= d * 300.0 / pxPerTick;
    else
        desplY -= juce::roundToInt (d * 220.0f);
    desplY = juce::jlimit (0, juce::jmax (0, numFiles() * altNota - g.getHeight()), desplY);
    vistaCanviada();
    repaint();
}

//==============================================================================
// Teclat de l'ordinador
//==============================================================================
bool PianoRoll::keyPressed (const juce::KeyPress& k)
{
    const auto mods = k.getModifiers();
    const int codi = k.getKeyCode();
    const bool cap = ! mods.isAnyModifierKeyDown();
    auto lletra = [codi] (char c) { return codi == c || codi == c + 32; };

    if (clip() != nullptr)
    {
        if (codi == juce::KeyPress::deleteKey || codi == juce::KeyPress::backspaceKey) { esborraTriades(); return true; }
        if (mods.isCommandDown() && lletra ('A')) { triaTotes(); return true; }
        if (mods.isCommandDown() && lletra ('C')) { copia (false); return true; }
        if (mods.isCommandDown() && lletra ('X')) { copia (true); return true; }
        if (mods.isCommandDown() && lletra ('V')) { enganxa(); return true; }
        if (mods.isCommandDown() && lletra ('D')) { duplicaTriades(); return true; }
        if (! mods.isCommandDown() && ! mods.isAltDown() && lletra ('Q')) { quantitza (mods.isShiftDown()); return true; }
        if (codi == juce::KeyPress::upKey && ! mods.isCommandDown()) { transposaTriades (mods.isShiftDown() ? 12 : 1); return true; }
        if (codi == juce::KeyPress::downKey && ! mods.isCommandDown()) { transposaTriades (mods.isShiftDown() ? -12 : -1); return true; }
        if (codi == juce::KeyPress::rightKey) { if (mods.isShiftDown()) allargaTriades (1); else mouTriades (1); return true; }
        if (codi == juce::KeyPress::leftKey) { if (mods.isShiftDown()) allargaTriades (-1); else mouTriades (-1); return true; }
        if (cap && lletra ('Z')) { ajustaVista(); return true; }
        if (mods.isShiftDown() && ! mods.isCommandDown() && lletra ('Z')) { zoomTriades(); return true; }
    }
    if (mods.isCommandDown() && lletra ('Z')) { if (onDesfes) onDesfes (mods.isShiftDown()); info = mods.isShiftDown() ? "Refet" : "Desfet"; repaint(); return true; }
    if (mods.isCommandDown() && lletra ('Y')) { if (onDesfes) onDesfes (true); info = "Refet"; repaint(); return true; }
    if (cap && codi >= '1' && codi <= '3') { triaEina ((EinaPR) (codi - '1')); return true; }
    if (codi == juce::KeyPress::escapeKey)
    {
        if (estat.segell.actiu()) { estat.segell = {}; estat.avisa(); info = "Acord descartat"; repaint(); return true; }
        triadesNotes.clear(); triaEina (EinaPR::Fletxa); info.clear(); return true;
    }
    if (codi == juce::KeyPress::pageUpKey) { canviaOctava (1); return true; }
    if (codi == juce::KeyPress::pageDownKey) { canviaOctava (-1); return true; }

    // La resta (Espai, R, Ctrl+S…) la fa l'estudi. Si som en una finestra a part, no hi arriba sola.
    if (desenganxat && onTeclaNoUsada) return onTeclaNoUsada (k);
    return false;
}

bool PianoRoll::keyStateChanged (bool) { return false; }

//==============================================================================
// Dibuix
//==============================================================================
void PianoRoll::paint (juce::Graphics& g)
{
    g.fillAll (tema::fons);
    pintaBarra (g);
    if (clip() == nullptr)
    {
        pintaBuit (g);
        return;
    }
    pintaGraella (g);
    pintaNotes (g);
    pintaRegla (g);
    if (bateria()) pintaNomsBateria (g);
    else
    {
        pintaTeclat (g);
        pintaOctaves (g);
    }
    if (modePads()) pintaPads (g); else pintaCarril (g);

    // Vora entre la graella i el carril.
    const auto v = zonaVoraCarril();
    vintage::panell (g, v.toFloat());
    g.setColour (mode == Mode::VoraCarril ? tema::accent : tema::linia2);
    g.fillRect (v.getCentreX() - 18, v.getCentreY() - 1, 36, 2);
    // Racó de baix a l'esquerra.
    g.setColour (tema::lateral);
    g.fillRect (0, getHeight() - gruix, ampleTeclat(), gruix);
}

void PianoRoll::pintaBuit (juce::Graphics& g)
{
    auto r = getLocalBounds().withTrimmedTop (alcadaBarra);
    g.setColour (tema::lateral);
    g.fillRect (r);
    g.setColour (tema::suau);
    g.setFont (dibuix::lletra (14.0f));
    g.drawFittedText ("Obre un clip MIDI: doble clic a un clip d'una pista d'instrument, o fes-ne un amb el llapis (2) en una pista d'instrument."_ca,
                      r.reduced (40), juce::Justification::centred, 3);
}

void PianoRoll::pintaBarra (juce::Graphics& g)
{
    auto b = getLocalBounds().removeFromTop (alcadaBarra);
    vintage::panell (g, b.toFloat());
    g.setColour (tema::linia);
    g.fillRect (0, b.getBottom() - 1, getWidth(), 1);
    if (estat.editors.size() > 1 && estat.editorActiu == index)
    {
        // L'editor actiu (el que fa servir el panell d'harmonia): una franja taronja a l'esquerra.
        g.setColour (tema::accent);
        g.fillRect (0, 0, 4, alcadaBarra);
    }

    if (! zonaInfo.isEmpty())
    {
        auto r = zonaInfo;
        juce::String titol = "PIANO ROLL";
        if (const auto* t = const_cast<PianoRoll*> (this)->pista())
            titol = dibuix::utf8 (t->nom) + " · "_ca + dibuix::utf8 (midi::nomInstrument (t->instrument.empty() ? "intern:piano" : t->instrument));
        if (index >= 0 && index < (int) estat.editors.size() && estat.editors[(size_t) index].bloquejat) titol += " · bloquejat"_ca;
        g.setFont (dibuix::lletra (12.5f, true));
        g.setColour (tema::text);
        const int ampleTitol = juce::jmin (r.getWidth() / 2, 280);
        g.drawFittedText (titol, r.removeFromRight (ampleTitol), juce::Justification::centredRight, 1, 0.8f);
        g.setFont (dibuix::lletra (12.5f));
        g.setColour (info.isNotEmpty() ? tema::accent : tema::suau);
        g.drawFittedText (info.isNotEmpty() ? info : "Ctrl+roda: zoom · Alt+roda: alçada · Maj+roda: de costat · Q: quantitza · Fletxes: mou"_ca,
                          r.reduced (6, 0), juce::Justification::centredLeft, 1, 0.8f);
    }
}

void PianoRoll::pintaGraella (juce::Graphics& g)
{
    const auto zg = zonaGraella();
    const auto* c = clip();
    const auto* t = const_cast<PianoRoll*> (this)->pista();
    const auto& p = estat.projecte;
    juce::Graphics::ScopedSaveState guarda (g);
    g.reduceClipRegion (zg);

    // Files: les de les tecles negres, més fosques; fora de l'escala, encara més; la fonamental amb un toc de taronja
    // i les notes de l'acord que sona al capçal, de verd. Línies entre si i mi-fa més marcades.
    std::vector<int> acordAra;
    if (p.ressaltaAcord)
        for (int n : harmonia::notesSonant (p, estat.capcal)) acordAra.push_back (n % 12);
    const int toDalt = toY ((float) zg.getY()), toBaix = toY ((float) zg.getBottom() - 1.0f);
    if (bateria()) pintaFilesBateria (g);
    else for (int to = toBaix; to <= toDalt; ++to)
    {
        const float y = yTo (to);
        juce::Colour fons = midi::esNegra (to) ? juce::Colour (0xff17171c) : juce::Colour (0xff1e1e25);
        fons = colorFila (to, fons, acordAra);
        if (to == sobreTecla) fons = fons.brighter (0.08f);
        g.setColour (fons);
        g.fillRect (juce::Rectangle<float> ((float) zg.getX(), y, (float) zg.getWidth(), (float) altNota));
        const int n = to % 12;
        g.setColour (n == 0 ? tema::linia2 : (n == 5 ? tema::linia : tema::linia.withAlpha (0.45f)));
        g.fillRect (juce::Rectangle<float> ((float) zg.getX(), y + (float) altNota - 1.0f, (float) zg.getWidth(), 1.0f));
    }

    // Fora de la finestra del clip: més fosc.
    const double a = posTicks() + midi::ticks (p, c->inici), b = posTicks() + midi::ticks (p, c->fi);
    g.setColour (juce::Colours::black.withAlpha (0.38f));
    g.fillRect (juce::Rectangle<float> ((float) zg.getX(), (float) zg.getY(), juce::jmax (0.0f, xTick (a) - (float) zg.getX()), (float) zg.getHeight()));
    g.fillRect (juce::Rectangle<float> (xTick (b), (float) zg.getY(), juce::jmax (0.0f, (float) zg.getRight() - xTick (b)), (float) zg.getHeight()));

    // Línies verticals: compassos, temps i graella.
    const double tpc = (double) midi::ticksPerCompas (p);
    const long long pg = pas();
    const double t0 = tickX ((float) zg.getX()), t1 = tickX ((float) zg.getRight());
    auto linies = [&] (double interval, juce::Colour color, float gruixLinia)
    {
        if (interval * pxPerTick < 5.0) return;
        for (double k = std::floor (t0 / interval) * interval; k <= t1; k += interval)
        {
            g.setColour (color);
            g.fillRect (juce::Rectangle<float> (xTick (k) - gruixLinia / 2.0f, (float) zg.getY(), gruixLinia, (float) zg.getHeight()));
        }
    };
    if (pg > 0 && pg < ticksPerNegra) linies ((double) pg, tema::linia2.withAlpha (0.45f), 1.0f);
    linies ((double) ticksPerNegra, tema::linia2.withAlpha (0.85f), 1.0f);
    linies (tpc, tema::linia2.brighter (0.35f), 1.5f);

    // Bucle.
    if (p.bucle.marcat)
    {
        const float xa = xTick (p.bucle.de * tpc), xb = xTick (p.bucle.a * tpc);
        g.setColour (tema::guitarra.withAlpha (p.bucle.actiu ? 0.06f : 0.03f));
        g.fillRect (juce::Rectangle<float> (xa, (float) zg.getY(), xb - xa, (float) zg.getHeight()));
    }

    // Vores de la finestra del clip.
    const auto colorPista = t != nullptr ? EstatEstudi::colorPista (*t) : tema::midi;
    g.setColour (colorPista.withAlpha (0.7f));
    g.fillRect (juce::Rectangle<float> (xTick (a) - 1.0f, (float) zg.getY(), 2.0f, (float) zg.getHeight()));
    g.fillRect (juce::Rectangle<float> (xTick (b) - 1.0f, (float) zg.getY(), 2.0f, (float) zg.getHeight()));

    // Notes fantasma: les de les pistes que s'hi han arrossegat (0.10; per defecte, cap), com a referència.
    if (auto* fs = const_cast<PianoRoll*> (this)->fantasmes(); fs != nullptr && ! fs->empty())
    {
        for (int i = 0; i < estat.numPistes(); ++i)
        {
            const auto& pt = p.pistes[(size_t) i];
            const bool totaLaPista = mibase::esMidi (pt) && std::find (fs->begin(), fs->end(), pt.id) != fs->end();
            const auto col = EstatEstudi::colorPista (pt);
            for (int k = 0; k < (int) pt.clips.size(); ++k)
            {
                if (i == pistaEditor() && k == estat.editors[(size_t) index].clip.clip) continue;
                const auto& kc = pt.clips[(size_t) k];
                if (! totaLaPista && std::find (fs->begin(), fs->end(), pt.id + "#" + std::to_string (k)) == fs->end()) continue;
                if (! kc.midi) { pintaOnaFantasma (g, kc, col, t0, t1); continue; }
                if (kc.mut) continue;
                const double pt0 = midi::ticks (p, kc.pos);
                const double w0 = pt0 + midi::ticks (p, kc.inici), w1 = pt0 + midi::ticks (p, kc.fi);
                if (w1 < t0 || w0 > t1) continue;
                for (const auto& n : kc.notes)
                {
                    const double na = pt0 + (double) n.ini;
                    if (na < w0 - 1e-6 || na >= w1) continue;
                    const double nb = juce::jmin (w1, na + (double) n.dur);
                    if (nb < t0 || na > t1) continue;
                    auto r = juce::Rectangle<float> (xTick (na), yTo (n.to) + 1.0f, juce::jmax (2.0f, xTick (nb) - xTick (na)), (float) altNota - 2.0f);
                    g.setColour (col.withAlpha (0.10f));
                    g.fillRoundedRectangle (r, 2.0f);
                    g.setColour (col.withAlpha (0.45f));
                    const float guions[] = { 3.0f, 2.0f };
                    juce::Path pth; pth.addRoundedRectangle (r, 2.0f);
                    juce::Path disc; juce::PathStrokeType (1.0f).createDashedStroke (disc, pth, guions, 2);
                    g.fillPath (disc);
                }
            }
        }
    }

    // Capçal.
    const float xc = xTick (midi::ticks (p, estat.capcal));
    g.setColour (estat.transport >= 2 ? vintage::ledVermell : (estat.transport == 1 ? vintage::ledVerd : tema::text));
    g.fillRect (juce::Rectangle<float> (xc - 0.75f, (float) zg.getY(), 1.5f, (float) zg.getHeight()));
}

void PianoRoll::pintaNotes (juce::Graphics& g)
{
    const auto zg = zonaGraella();
    const auto* c = clip();
    const auto* t = const_cast<PianoRoll*> (this)->pista();
    const auto& p = estat.projecte;
    juce::Graphics::ScopedSaveState guarda (g);
    g.reduceClipRegion (zg);

    const auto base = c->rgb != 0 ? juce::Colour (c->rgb) : (t != nullptr ? EstatEstudi::colorPista (*t) : tema::midi);
    auto trep = midi::trepitjades (c->notes);
    if (bateria()) std::fill (trep.begin(), trep.end(), false);   // 0.20: a la bateria els cops sonen sencers: no es trepitgen
    const double a = posTicks() + midi::ticks (p, c->inici), b = posTicks() + midi::ticks (p, c->fi);
    const double t0 = tickX ((float) zg.getX()), t1 = tickX ((float) zg.getRight());
    std::vector<bool> triada (c->notes.size(), false);
    for (int i : triadesNotes) if (i >= 0 && i < (int) triada.size()) triada[(size_t) i] = true;

    for (int i = 0; i < (int) c->notes.size(); ++i)
    {
        const auto& n = c->notes[(size_t) i];
        const double na = tickAbs (n), nb = na + (double) n.dur;
        if (nb < t0 || na > t1) continue;
        const float y = yTo (n.to);
        if (y > (float) zg.getBottom() || y + (float) altNota < (float) zg.getY()) continue;
        auto r = juce::Rectangle<float> (xTick (na), y + 1.0f, juce::jmax (3.0f, xTick (nb) - xTick (na)), (float) altNota - 2.0f);
        const bool fora = na < a - 1e-6 || na >= b;   // fora de la finestra: no sona
        const bool tri = triada[(size_t) i];
        auto col = colorNota (base, n.vel);
        if (fora) col = col.withSaturation (0.1f).withAlpha (0.45f);

        g.setColour (col);
        g.fillRoundedRectangle (r, 2.5f);
        // Barra de velocitat dins la nota.
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillRect (juce::Rectangle<float> (r.getX() + 1.5f, r.getBottom() - 3.0f, (r.getWidth() - 3.0f) * (float) n.vel / 127.0f, 1.5f));
        if (trep[(size_t) i])
        {
            // Trepitja una altra nota del mateix to: ratllada i vora vermella.
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (r.toNearestInt());
            g.setColour (juce::Colours::black.withAlpha (0.3f));
            for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += 5.0f)
                g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), 1.0f);
        }
        g.setColour (tri ? tema::text : (trep[(size_t) i] ? tema::error : col.darker (0.6f)));
        g.drawRoundedRectangle (r.reduced (0.5f), 2.5f, tri ? 2.0f : 1.0f);
        if (sobre.nota == i && mode == Mode::Res)
        {
            g.setColour (juce::Colours::white.withAlpha (0.18f));
            g.fillRoundedRectangle (r, 2.5f);
            if (sobre.part != Part::Cos)
            {
                g.setColour (tema::text);
                const float x = sobre.part == Part::Fi ? r.getRight() - 3.0f : r.getX() + 1.0f;
                g.fillRect (juce::Rectangle<float> (x, r.getY() + 1.0f, 2.0f, r.getHeight() - 2.0f));
            }
        }
        if (r.getWidth() > 28.0f && altNota >= 11 && ! bateria())
        {
            g.setColour (juce::Colours::black.withAlpha (0.75f));
            g.setFont (dibuix::lletra (juce::jmin (11.0f, (float) altNota - 3.0f), true));
            g.drawText (txt (midi::nomNota (n.to)), r.reduced (4.0f, 0.0f), juce::Justification::centredLeft, false);
        }
    }

    // Segell: l'acord que es posarà amb un clic.
    if (estat.segell.actiu() && zg.toFloat().contains (ratoli))
    {
        const auto notes = segellA (ratoli);
        const double ini = posTicks() + (double) enganxaAbs (tickX (ratoli.x), true);
        const double dur = (double) (estat.segell.durada > 0 ? estat.segell.durada : duradaNova());
        for (int n : notes)
        {
            auto r = juce::Rectangle<float> (xTick (ini), yTo (n) + 1.0f, juce::jmax (4.0f, xTick (ini + dur) - xTick (ini)), (float) altNota - 2.0f);
            g.setColour (vintage::ledVerd.withAlpha (0.35f));
            g.fillRoundedRectangle (r, 2.5f);
            g.setColour (vintage::ledVerd);
            g.drawRoundedRectangle (r.reduced (0.5f), 2.5f, 1.5f);
        }
        if (! notes.empty())
        {
            g.setColour (vintage::ledVerd);
            g.setFont (dibuix::lletra (12.0f, true));
            g.drawText (estat.segell.nom, juce::Rectangle<float> (xTick (ini), yTo (notes.back()) - 18.0f, 120.0f, 16.0f), juce::Justification::centredLeft, false);
        }
    }

    // Requadre.
    if (mode == Mode::Requadre && inici.getDistanceFrom (ara) > 3.0f)
    {
        const auto r = juce::Rectangle<float> (inici, ara);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRect (r);
        g.setColour (tema::text.withAlpha (0.8f));
        g.drawRect (r, 1.0f);
    }
}

void PianoRoll::pintaRegla (juce::Graphics& g)
{
    const auto zr = zonaRegla();
    const auto& p = estat.projecte;
    g.setColour (tema::lateral);
    g.fillRect (zr);
    g.setColour (tema::lateral);
    g.fillRect (0, zr.getY(), ampleTeclat(), zr.getHeight());
    juce::Graphics::ScopedSaveState guarda (g);
    g.reduceClipRegion (zr);
    const double tpc = (double) midi::ticksPerCompas (p);
    const double t0 = tickX ((float) zr.getX()), t1 = tickX ((float) zr.getRight());
    const int cada = tpc * pxPerTick < 36.0 ? (tpc * pxPerTick < 12.0 ? 8 : 2) : 1;
    if (p.bucle.marcat && p.bucle.a > p.bucle.de)   // 0.24: el bucle també es veu (i es fa) aquí
    {
        const float xa = xTick (p.bucle.de * tpc), xb = xTick (p.bucle.a * tpc);
        g.setColour ((p.bucle.actiu ? tema::guitarra : tema::suau).withAlpha (p.bucle.actiu ? 0.35f : 0.18f));
        g.fillRect (juce::Rectangle<float> (xa, (float) zr.getY() + 2.0f, xb - xa, (float) zr.getHeight() - 4.0f));
    }
    g.setFont (dibuix::mono (11.5f));
    for (long long k = (long long) std::floor (t0 / tpc); k * tpc <= t1; ++k)
    {
        const float x = xTick ((double) k * tpc);
        if (k % cada == 0)
        {
            g.setColour (tema::linia2);
            g.fillRect (juce::Rectangle<float> (x, (float) zr.getY() + 4.0f, 1.0f, (float) zr.getHeight() - 4.0f));
            g.setColour (tema::textMig);
            g.drawText (juce::String (ed::numCompas ((int) k)), juce::Rectangle<float> (x + 4.0f, (float) zr.getY(), 50.0f, (float) zr.getHeight()),
                        juce::Justification::centredLeft, false);
        }
        if (ticksPerNegra * pxPerTick > 14.0)
            for (int b = 1; b < p.compasNum; ++b)
            {
                g.setColour (tema::linia);
                g.fillRect (juce::Rectangle<float> (xTick ((double) k * tpc + b * ticksPerNegra), (float) zr.getBottom() - 6.0f, 1.0f, 6.0f));
            }
    }
    const float xc = xTick (midi::ticks (p, estat.capcal));
    if (p.ressaltaAcord)
    {
        const auto r = harmonia::reconeix (harmonia::notesSonant (p, estat.capcal));
        if (r.trobat)
        {
            g.setColour (vintage::ledVerd);
            g.setFont (dibuix::lletra (12.0f, true));
            g.drawText (txt (r.nom), juce::Rectangle<float> (xc + 8.0f, (float) zr.getY(), 90.0f, (float) zr.getHeight()), juce::Justification::centredLeft, false);
        }
    }
    juce::Path tri;
    tri.addTriangle (xc - 6.0f, (float) zr.getY() + 6.0f, xc + 6.0f, (float) zr.getY() + 6.0f, xc, (float) zr.getBottom() - 2.0f);
    g.setColour (estat.transport >= 2 ? vintage::ledVermell : (estat.transport == 1 ? vintage::ledVerd : tema::text));
    g.fillPath (tri);
    g.setColour (tema::linia);
    g.fillRect (zr.getX(), zr.getBottom() - 1, zr.getWidth(), 1);
}

void PianoRoll::pintaTeclat (juce::Graphics& g)
{
    const auto zt = zonaTeclat();
    const auto* c = clip();
    juce::Graphics::ScopedSaveState guarda (g);
    g.reduceClipRegion (zt);
    g.setColour (juce::Colour (0xff0e0e11));
    g.fillRect (zt);

    // Notes que sonen ara (amb el transport en marxa).
    std::array<bool, 128> sonant {};
    if (estat.transport != 0 && c != nullptr)
    {
        const double ara2 = midi::ticks (estat.projecte, estat.capcal);
        for (auto& n : c->notes)
            if (tickAbs (n) <= ara2 && tickAbs (n) + (double) n.dur > ara2) sonant[(size_t) n.to] = true;
    }

    const int toDalt = toY ((float) zt.getY()), toBaix = toY ((float) zt.getBottom() - 1.0f);
    const float xNegra = (float) zt.getWidth() * 0.62f;
    for (int to = toBaix; to <= toDalt; ++to)
    {
        const float y = yTo (to);
        auto r = juce::Rectangle<float> ((float) zt.getX(), y, (float) zt.getWidth() - 1.0f, (float) altNota);
        const bool negra = midi::esNegra (to);
        const bool premuda = premudes[(size_t) to] || sonant[(size_t) to];
        if (negra)
        {
            g.setColour (juce::Colour (0xffe8e6ee));
            g.fillRect (r);
            auto rn = r.withWidth (xNegra).reduced (0.0f, 0.5f);
            g.setColour (premuda ? tema::accent : (to == sobreTecla ? juce::Colour (0xff3a3a44) : juce::Colour (0xff1c1c22)));
            g.fillRoundedRectangle (rn, 2.0f);
        }
        else
        {
            g.setColour (premuda ? tema::accent.withAlpha (0.85f) : (to == sobreTecla ? juce::Colour (0xffd7dbe6) : juce::Colour (0xffeeedf3)));
            g.fillRect (r);
            const int n = to % 12;
            if (n == 0 || n == 5)
            {
                g.setColour (juce::Colour (0xff9a99a6));
                g.fillRect (juce::Rectangle<float> (r.getX(), r.getBottom() - 1.0f, r.getWidth(), 1.0f));
            }
        }
        // Marca de l'escala a la vora dreta de la tecla (taronja: la fonamental).
        {
            const auto t = tonalitat();
            if (harmonia::aEscala (to, t))
            {
                const bool arrel = ((to - t.to) % 12 + 12) % 12 == 0;
                g.setColour (arrel ? tema::accent : juce::Colour (0xff8d8c97));
                g.fillRect (juce::Rectangle<float> (r.getRight() - 4.0f, r.getY() + 1.0f, 3.0f, r.getHeight() - 2.0f));
            }
        }
        if (to % 12 == 0 || (altNota >= 15 && ! negra) || to == sobreTecla)
        {
            g.setColour (to % 12 == 0 ? juce::Colour (0xff222228) : juce::Colour (0xff6b6a76));
            g.setFont (dibuix::lletra (juce::jmin (11.0f, (float) altNota - 1.0f), to % 12 == 0));
            g.drawText (txt (midi::nomNota (to)), r.reduced (4.0f, 0.0f), juce::Justification::centredRight, false);
        }
    }
    g.setColour (tema::linia);
    g.fillRect (zt.getRight() - 1, zt.getY(), 1, zt.getHeight());
}

void PianoRoll::pintaOctaves (juce::Graphics& g)
{
    const auto zt = zonaTeclat();
    juce::Graphics::ScopedSaveState s2 (g);
    g.reduceClipRegion (0, zt.getY(), ampleOctava, zt.getHeight());
    g.setColour (tema::lateral);
    g.fillRect (0, zt.getY(), ampleOctava, zt.getHeight());
    for (int k = 0; k < 2; ++k)
    {
        const bool amunt = k == 0;
        auto r = rectOctava (amunt).toFloat();
        vintage::tecla (g, r, octavaPremuda == (amunt ? 1 : -1), false, tema::accent);
        g.setColour (tema::text);
        g.setFont (dibuix::lletra (15.0f, true));
        g.drawText (amunt ? "+" : "-", r, juce::Justification::centred, false);
    }
    g.setColour (tema::suau);
    g.setFont (dibuix::lletra (9.5f, true));
    g.drawFittedText ("OCT", rectOctava (false).translated (0, 38).withHeight (16), juce::Justification::centred, 1);
}

void PianoRoll::pintaCarril (juce::Graphics& g)
{
    const auto zc = zonaCarril();
    const auto* c = clip();
    const auto* t = const_cast<PianoRoll*> (this)->pista();
    g.setColour (tema::lateral);
    g.fillRect (0, zc.getY(), ampleTeclat(), zc.getHeight());
    dibuix::pintaEtiquetaSeccio (g, "Velocitat", juce::Rectangle<int> (8, zc.getY() + 4, ampleTeclat() - 10, 16));
    g.setFont (dibuix::lletra (10.5f));
    g.setColour (tema::suau);
    g.drawFittedText ("Maj: línia"_ca, juce::Rectangle<int> (8, zc.getY() + 20, ampleTeclat() - 10, 28), juce::Justification::topLeft, 2);

    g.setColour (juce::Colour (0xff121216));
    g.fillRect (zc);
    juce::Graphics::ScopedSaveState guarda (g);
    g.reduceClipRegion (zc);
    const auto zi = zc.toFloat().reduced (0.0f, 5.0f);
    for (int v : { 32, 64, 96, 127 })
    {
        const float y = zi.getBottom() - zi.getHeight() * (float) (v - 1) / 126.0f;
        g.setColour (tema::linia.withAlpha (v == 64 ? 0.8f : 0.4f));
        g.fillRect (juce::Rectangle<float> (zi.getX(), y, zi.getWidth(), 1.0f));
    }
    if (c == nullptr) return;
    const auto base = c->rgb != 0 ? juce::Colour (c->rgb) : (t != nullptr ? EstatEstudi::colorPista (*t) : tema::midi);
    std::vector<bool> triada (c->notes.size(), false);
    for (int i : triadesNotes) if (i >= 0 && i < (int) triada.size()) triada[(size_t) i] = true;
    const double t0 = tickX ((float) zc.getX()) - 100.0, t1 = tickX ((float) zc.getRight());
    // Primer les altres, després les triades i, a sobre de tot, la que es toca (quan n'hi ha diverses al mateix lloc).
    std::vector<int> ordre;
    for (int passada = 0; passada < 3; ++passada)
        for (int i = 0; i < (int) c->notes.size(); ++i)
        {
            const bool sob = i == sobreCarril || (mode == Mode::Velocitat && i == toc0.nota);
            const int grup = sob ? 2 : (triada[(size_t) i] ? 1 : 0);
            if (grup == passada) ordre.push_back (i);
        }
    for (int i : ordre)
    {
        const auto& n = c->notes[(size_t) i];
        const double na = tickAbs (n);
        if (na < t0 || na > t1) continue;
        const float x = xTick (na);
        const float alt = zi.getHeight() * (float) (n.vel - 1) / 126.0f;
        const float ampleNota = xTick (na + (double) n.dur) - x;
        const float w = juce::jlimit (3.0f, 7.0f, ampleNota - 1.0f);
        const bool tri = triada[(size_t) i];
        const bool sob = i == sobreCarril || (mode == Mode::Velocitat && i == toc0.nota);
        g.setColour (tri ? tema::accent : colorNota (base, n.vel));
        g.fillRect (juce::Rectangle<float> (x, zi.getBottom() - alt, w, alt));
        g.fillEllipse (x + w / 2.0f - 3.5f, zi.getBottom() - alt - 3.5f, 7.0f, 7.0f);
        if (sob)
        {
            g.setColour (tema::text);
            g.drawEllipse (x + w / 2.0f - 4.5f, zi.getBottom() - alt - 4.5f, 9.0f, 9.0f, 1.5f);
            g.setFont (dibuix::mono (11.0f));
            g.drawText (juce::String (n.vel), juce::Rectangle<float> (x + 8.0f, zi.getBottom() - alt - 14.0f, 40.0f, 14.0f), juce::Justification::centredLeft, false);
        }
    }
    if (mode == Mode::LiniaVel && ara != inici && juce::ModifierKeys::currentModifiers.isShiftDown())
    {
        g.setColour (tema::text.withAlpha (0.8f));
        g.drawLine (juce::Line<float> (inici, ara), 1.5f);
    }
}


//==============================================================================
// Notes fantasma (0.10)
//==============================================================================
std::vector<std::string>* PianoRoll::fantasmes()
{
    if (index < 0 || index >= (int) estat.editors.size()) return nullptr;
    return &estat.editors[(size_t) index].fantasmes;
}

void PianoRoll::pintaOnaFantasma (juce::Graphics& g, const mibase::Clip& kc, juce::Colour col, double t0, double t1)
{
    // Un clip d'àudio en fantasma (0.12): la seva ona, tènue, sobre tota l'alçada de la graella.
    auto it = estat.ones.find (kc.presa);
    if (it == estat.ones.end() || it->second == nullptr || it->second->maxs.empty()) return;
    const auto& ona = *it->second;
    const auto& p = estat.projecte;
    const auto zg = zonaGraella();
    const double a = juce::jmax (t0, midi::ticks (p, kc.iniciVisible())), b = juce::jmin (t1, midi::ticks (p, kc.fiVisible()));
    if (b <= a) return;
    const float centre = (float) zg.getCentreY(), mig = (float) zg.getHeight() * 0.45f;
    g.setColour (col.withAlpha (0.22f));
    for (int x = (int) xTick (a); x < (int) xTick (b); ++x)
    {
        const double tf = midi::segons (p, tickX ((float) x)) - kc.pos;
        const auto bl = (size_t) juce::jlimit (0.0, (double) ona.maxs.size() - 1, tf * ona.sr / OnaPresa::bloc);
        const float hi = ona.maxs[bl], lo = ona.mins[bl];
        g.fillRect ((float) x, centre - hi * mig, 1.0f, juce::jmax (1.0f, (hi - lo) * mig));
    }
}

void PianoRoll::actualitzaFantasmes()
{
    auto* fs = fantasmes();
    if (fs != nullptr)   // les pistes esborrades ja no hi són
        fs->erase (std::remove_if (fs->begin(), fs->end(), [this] (const std::string& id)
        {
            const auto pista = id.substr (0, id.find ('#'));
            for (auto& t : estat.projecte.pistes) if (t.id == pista) return false;
            return true;
        }), fs->end());
    btFantasmes.setToggleState (sobreFantasmes || estat.triantFantasma == index || (fs != nullptr && ! fs->empty()), juce::dontSendNotification);
}

void PianoRoll::menuFantasmes()
{
    auto* fs = fantasmes();
    if (fs == nullptr) return;
    juce::PopupMenu m;
    m.addSectionHeader ("Notes fantasma de:"_ca);
    int n = 0;
    for (int i = 0; i < estat.numPistes(); ++i)
    {
        const auto& t = estat.projecte.pistes[(size_t) i];
        if (! mibase::esMidi (t)) continue;
        const bool hi = std::find (fs->begin(), fs->end(), t.id) == fs->end() ? false : true;
        m.addItem (100 + i, dibuix::utf8 (t.nom) + (i == pistaEditor() ? " (altres clips)"_ca : juce::String()), true, hi);
        ++n;
    }
    if (n == 0) m.addItem (-1, "No hi ha pistes MIDI"_ca, false);
    m.addSeparator();
    m.addItem (1, "Treu-les totes"_ca, ! fs->empty());
    m.addItem (-1, "Truc: arrossega el nom d'una pista fins a aquest botó"_ca, false);
    juce::Component::SafePointer<PianoRoll> jo (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btFantasmes), [jo] (int r)
    {
        if (jo == nullptr || r <= 0) return;
        auto* f = jo->fantasmes();
        if (f == nullptr) return;
        if (r == 1) f->clear();
        else if (auto* t = jo->estat.pista (r - 100))
        {
            auto it = std::find (f->begin(), f->end(), t->id);
            if (it == f->end()) f->push_back (t->id); else f->erase (it);
        }
        jo->actualitzaFantasmes();
        jo->repaint();
    });
}

//==============================================================================
// Arrossegar acords
//==============================================================================
void PianoRoll::arrossegaAcord (juce::Component& origen, EstatEstudi& estat, const std::vector<int>& notes, const juce::String& nom,
                                long long durada, bool enllaca)
{
    auto* contenidor = juce::DragAndDropContainer::findParentDragContainerFor (&origen);
    if (contenidor == nullptr || notes.empty()) return;
    estat.segell.notes = notes;
    estat.segell.durada = durada;
    estat.segell.enllaca = enllaca;
    estat.segell.nom = nom;
    estat.segell.perArrossegar = true;
    estat.info = "Deixa " + nom + " al Piano Roll, on vulguis"_ca;
    estat.avisa();

    // La imatge que segueix el ratolí: una etiqueta amb el nom de l'acord.
    juce::Image im (juce::Image::ARGB, 90, 30, true);
    {
        juce::Graphics g (im);
        g.setColour (vintage::ledVerd.withAlpha (0.85f));
        g.fillRoundedRectangle (0.0f, 0.0f, 90.0f, 30.0f, 6.0f);
        g.setColour (juce::Colours::black);
        g.setFont (dibuix::lletra (15.0f, true));
        g.drawText (nom, 0, 0, 90, 30, juce::Justification::centred, true);
    }
    contenidor->startDragging (idArrossegaAcord, &origen, juce::ScaledImage (im), true);
}

bool PianoRoll::isInterestedInDragSource (const SourceDetails& d)
{
    if (d.description.toString().startsWith (prefixArrossegaPista)) return fantasmes() != nullptr;
    return d.description.toString() == idArrossegaAcord && clip() != nullptr && estat.segell.actiu();
}

void PianoRoll::itemDragMove (const SourceDetails& d)
{
    if (d.description.toString().startsWith (prefixArrossegaPista))
    {
        const bool damunt = btFantasmes.getBounds().expanded (8).contains (d.localPosition);
        if (damunt != sobreFantasmes) { sobreFantasmes = damunt; actualitzaFantasmes(); repaint(); }
        return;
    }
    ratoli = d.localPosition.toFloat();
    repaint (zonaGraella().expanded (0, 4));
}

void PianoRoll::itemDragExit (const SourceDetails&)
{
    ratoli = { -1.0f, -1.0f };
    sobreFantasmes = false;
    actualitzaFantasmes();
    repaint();
}

void PianoRoll::itemDropped (const SourceDetails& d)
{
    if (d.description.toString().startsWith (prefixArrossegaPista))
    {
        sobreFantasmes = false;
        const auto id = d.description.toString().fromFirstOccurrenceOf (prefixArrossegaPista, false, false).toStdString();
        auto* fs = fantasmes();
        if (fs == nullptr) return;
        if (! btFantasmes.getBounds().expanded (8).contains (d.localPosition))
        {
            info = "Deixa la pista damunt del botó de notes fantasma"_ca;
            repaint();
            return;
        }
        juce::String nom;
        for (auto& t : estat.projecte.pistes) if (t.id == id) nom = dibuix::utf8 (t.nom);
        if (std::find (fs->begin(), fs->end(), id) == fs->end()) fs->push_back (id);
        info = "Notes fantasma de «"_ca + nom + "»"_ca;
        actualitzaFantasmes();
        repaint();
        return;
    }
    const auto p = d.localPosition.toFloat();
    if (zonaGraella().toFloat().contains (p))
    {
        if (estat.editorActiu != index) { estat.editorActiu = index; if (onActiu) onActiu(); }
        posaSegell (p, false);
    }
    ratoli = { -1.0f, -1.0f };
    repaint();
}

//==============================================================================
// Chord Pad
//==============================================================================
namespace
{
    /** Editor d'un acord (com el de la captura): fonamental, tipus, tensions i nota greu; o escriure'l. */
    class EditorAcord : public juce::Component
    {
    public:
        EditorAcord (harmonia::AcordPad inicial, std::function<void (const harmonia::AcordPad&)> canvi,
                     std::function<void (const harmonia::AcordPad&)> escolta)
            : acord (inicial), onCanvi (std::move (canvi)), onEscolta (std::move (escolta))
        {
            if (acord.buit()) acord = { 0, 0, 0, -1 };
            auto columna = [this] (juce::OwnedArray<Boto>& v, int n, std::function<juce::String (int)> etiqueta, std::function<void (int)> clic)
            {
                for (int i = 0; i < n; ++i)
                {
                    auto* b = v.add (new Boto (etiqueta (i), std::nullopt, Boto::Estil::Tecla));
                    b->setLlum (vintage::ledTaronja);
                    b->setMidaText (12.5f);
                    b->onClick = [clic, i] { clic (i); };
                    addAndMakeVisible (b);
                }
            };
            columna (fonamentals, 12, [] (int i) { return juce::String (harmonia::nomTo (i)); }, [this] (int i) { acord.arrel = i; canviat(); });
            columna (tipus, harmonia::numTipusPad, [] (int i) { return juce::String (harmonia::nomTipusPad (i)); }, [this] (int i) { acord.tipus = i; canviat(); });
            columna (tensions, harmonia::numTensions, [] (int i) { return juce::String (harmonia::nomTensio (i)); },
                     [this] (int i)
                     {
                         acord.tensions ^= (1u << i);
                         if (i == 0 && (acord.tensions & 1u)) acord.tensions &= ~2u;   // 7 i j7 no van junts
                         if (i == 1 && (acord.tensions & 2u)) acord.tensions &= ~1u;
                         canviat();
                     });
            columna (greus, 12, [] (int i) { return juce::String (harmonia::nomTo (i)); },
                     [this] (int i) { acord.baix = (acord.baix == i || i == acord.arrel) ? -1 : i; canviat(); });
            btEscolta.onClick = [this] { if (onEscolta) onEscolta (acord); };
            btBuida.onClick = [this] { if (onCanvi) onCanvi ({}); tanca(); };
            btMenys.onClick = [this] { inverteix (-1); };
            btMes.onClick = [this] { inverteix (1); };
            btAplica.onClick = [this] { tanca(); };
            btEscolta.setTooltip ("Escolta l'acord"_ca);
            btBuida.setTooltip ("Buida el pad"_ca);
            btMenys.setTooltip ("Inversió avall: la nota de dalt baixa una octava"_ca);
            btMes.setTooltip ("Inversió amunt: la nota de baix puja una octava"_ca);
            btAplica.setTooltip ("Desa l'acord al pad i tanca l'editor (Retorn)"_ca);
            for (auto* b : { &btEscolta, &btBuida, &btMenys, &btMes, &btAplica }) { b->setMidaText (12.0f); addAndMakeVisible (b); }
            btMenys.setMidaText (16.0f);
            btMes.setMidaText (16.0f);
            setWantsKeyboardFocus (true);
            refresca();
            setSize (520, 404);
        }

        void paint (juce::Graphics& g) override
        {
            vintage::panell (g, getLocalBounds().toFloat(), 8.0f);
            g.setColour (tema::text);
            g.setFont (dibuix::lletra (22.0f, true));
            g.drawText (juce::String::fromUTF8 (harmonia::nomPad (acord).c_str()), 16, 8, 160, 30, juce::Justification::centredLeft);
            g.setFont (dibuix::lletra (11.5f));
            g.setColour (tema::suau);
            g.drawText (textInversio (acord.inversio), 16, 34, 170, 14, juce::Justification::centredLeft);
            const char* titols[] = { "Fonamental", "Tipus", "Tensions", "Nota greu" };
            g.setFont (dibuix::lletra (11.5f, true));
            g.setColour (tema::suau);
            for (int c = 0; c < 4; ++c)
                g.drawText (dibuix::majuscules (juce::String::fromUTF8 (titols[c])), 16 + c * 124, 54, 120, 16, juce::Justification::centredLeft);
        }

        bool keyPressed (const juce::KeyPress& k) override
        {
            if (k == juce::KeyPress::returnKey) { tanca(); return true; }
            if (k == juce::KeyPress::escapeKey) { tanca(); return true; }
            if (k.getTextCharacter() == '+') { inverteix (1); return true; }
            if (k.getTextCharacter() == '-') { inverteix (-1); return true; }
            return false;
        }

        void resized() override
        {
            // A dalt a la dreta, botons petits: Escolta, Buida, −, + i Aplica (que tanca l'editor).
            auto f = juce::Rectangle<int> (176, 14, getWidth() - 16 - 176, 26);
            btAplica.setBounds (f.removeFromRight (70)); f.removeFromRight (8);
            btMes.setBounds (f.removeFromRight (28)); f.removeFromRight (3);
            btMenys.setBounds (f.removeFromRight (28)); f.removeFromRight (8);
            btBuida.setBounds (f.removeFromRight (58)); f.removeFromRight (4);
            btEscolta.setBounds (f.removeFromRight (86));
            auto col = [this] (juce::OwnedArray<Boto>& v, int c)
            {
                for (int i = 0; i < v.size(); ++i) v[i]->setBounds (16 + c * 124, 74 + i * 27, 116, 25);
            };
            col (fonamentals, 0); col (tipus, 1); col (tensions, 2); col (greus, 3);
        }

    private:
        void canviat()
        {
            refresca();
            if (onCanvi) onCanvi (acord);
            if (onEscolta) onEscolta (acord);
        }

        void inverteix (int d)
        {
            if (acord.buit()) return;
            acord.inversio = juce::jlimit (-harmonia::maxInversio, harmonia::maxInversio, acord.inversio + d);
            canviat();
        }

        void tanca()
        {
            if (auto* c = findParentComponentOfClass<juce::CallOutBox>()) c->dismiss();
        }

        static juce::String textInversio (int i)
        {
            if (i == 0) return "Posició fonamental"_ca;
            if (i > 0) return juce::String (i) + "a inversió (amunt)"_ca;
            return juce::String (-i) + "a inversió (avall)"_ca;
        }

        void refresca()
        {
            for (int i = 0; i < 12; ++i) fonamentals[i]->setToggleState (acord.arrel == i, juce::dontSendNotification);
            for (int i = 0; i < tipus.size(); ++i) tipus[i]->setToggleState (acord.tipus == i, juce::dontSendNotification);
            for (int i = 0; i < tensions.size(); ++i) tensions[i]->setToggleState ((acord.tensions >> i) & 1u, juce::dontSendNotification);
            for (int i = 0; i < 12; ++i) greus[i]->setToggleState (acord.baix == i, juce::dontSendNotification);
            repaint();
        }

        harmonia::AcordPad acord;
        std::function<void (const harmonia::AcordPad&)> onCanvi, onEscolta;
        juce::OwnedArray<Boto> fonamentals, tipus, tensions, greus;
        Boto btEscolta { "Escolta", dibuix::Icona::Play, Boto::Estil::Secundari };
        Boto btBuida { "Buida", std::nullopt, Boto::Estil::Secundari };
        Boto btMenys { "-", std::nullopt, Boto::Estil::Secundari };
        Boto btMes { "+", std::nullopt, Boto::Estil::Secundari };
        Boto btAplica { "Aplica", std::nullopt, Boto::Estil::Primari };
    };
}

bool PianoRoll::modePads() const
{
    return index >= 0 && index < (int) estat.editors.size() && estat.editors[(size_t) index].pads && ! bateria();
}

void PianoRoll::canviaModePads (bool pads)
{
    if (index < 0 || index >= (int) estat.editors.size()) return;
    estat.editors[(size_t) index].pads = pads;
    actualitzaControlsPads();
    resized();
    repaint();
    estat.avisa();   // el teclat MIDI passa a tocar acords (o deixa de fer-ho)
}

void PianoRoll::actualitzaControlsPads()
{
    const bool p = modePads();
    selMode.setTriat (p ? 1 : 0);
    selMode.setVisible (! bateria());
    for (auto* b : std::initializer_list<juce::Component*> { &btPresetPads, &btLlapisPads, &octavaPads, &voicingPads, &selDuradaPads, &btMinimPads })
        b->setVisible (p && clip() != nullptr);
    if (const auto* t = const_cast<PianoRoll*> (this)->pista())
        btPresetPads.setButtonText (t->presetPads.empty() || t->pads.empty() ? "Acords de la tonalitat"_ca : juce::String::fromUTF8 (t->presetPads.c_str()));
}

std::vector<std::string> PianoRoll::padsActuals() const
{
    if (const auto* t = const_cast<PianoRoll*> (this)->pista(); t != nullptr && (int) t->pads.size() == pads::numPads)
        return t->pads;
    return pads::padsDeTonalitat (tonalitat());
}

juce::Rectangle<int> PianoRoll::zonaPads() const
{
    return zonaCarril().withTrimmedTop (36).reduced (6, 4);
}

juce::Rectangle<int> PianoRoll::rectPad (int i) const
{
    // Com una octava de piano: les 7 blanques (C D E F G A B) a baix i les 5 negres (C# D# F# G# A#) a dalt, entre les blanques.
    const auto z = zonaPads();
    const int sep = 6;
    const int w = (z.getWidth() - sep * 6) / 7, h = (z.getHeight() - sep) / 2;
    static constexpr int posBlanca[12] = { 0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6 };   // per a les negres: la blanca de la dreta
    if (! pads::esNegre (i))
        return { z.getX() + posBlanca[i] * (w + sep), z.getY() + h + sep, w, h };
    const int wn = juce::roundToInt ((float) w * 0.84f);
    const int centre = z.getX() + posBlanca[i] * (w + sep) - sep / 2;
    return { centre - wn / 2, z.getY(), wn, h };
}

int PianoRoll::padA (juce::Point<int> p) const
{
    for (int i = 0; i < pads::numPads; ++i) if (rectPad (i).contains (p)) return i;
    return -1;
}

std::vector<int> PianoRoll::notesDelPad (int i) const
{
    const auto v = padsActuals();
    if (i < 0 || i >= (int) v.size()) return {};
    return harmonia::notesPad (harmonia::llegeixAcord (v[(size_t) i]), octavaPads.getValor(), (harmonia::Voicing) voicingPads.getTriat());
}

std::array<std::vector<int>, 12> PianoRoll::acordsTeclat() const
{
    std::array<std::vector<int>, 12> r;
    for (int i = 0; i < 12; ++i) r[(size_t) i] = notesDelPad (i);
    return r;
}

long long PianoRoll::duradaPads() const
{
    switch (selDuradaPads.getTriat())
    {
        case 1: return ticksPerNegra * 2;
        case 2: return ticksPerNegra;
        default: return midi::ticksPerCompas (estat.projecte);
    }
}

void PianoRoll::desaPad (int i, const std::string& nom)
{
    auto* t = pista();
    if (t == nullptr || i < 0 || i >= pads::numPads) return;
    estat.desfes.abans (estat.projecte);
    if ((int) t->pads.size() != pads::numPads) { t->pads = padsActuals(); if (t->presetPads.empty()) t->presetPads = "Acords de la tonalitat"; }
    t->pads[(size_t) i] = nom;
    estat.canviat();
}

void PianoRoll::obreEditorPad (int i)
{
    auto* t = pista();
    if (t == nullptr) return;
    const auto v = padsActuals();
    juce::Component::SafePointer<PianoRoll> jo (this);
    auto ed = std::make_unique<EditorAcord> (harmonia::llegeixAcord (v[(size_t) i]),
        [jo, i] (const harmonia::AcordPad& a) { if (jo != nullptr) jo->desaPad (i, harmonia::textPad (a)); },
        [jo] (const harmonia::AcordPad& a)
        {
            if (jo != nullptr) jo->escoltaAcord (harmonia::notesPad (a, jo->octavaPads.getValor(), (harmonia::Voicing) jo->voicingPads.getTriat()));
        });
    juce::CallOutBox::launchAsynchronously (std::move (ed), localAreaToGlobal (rectPad (i)), nullptr);
}

void PianoRoll::posaPads (const std::vector<std::string>& acords, const std::string& nom)
{
    auto* t = pista();
    if (t == nullptr) return;
    estat.desfes.abans (estat.projecte);
    t->pads = acords;
    t->pads.resize (pads::numPads);
    t->presetPads = nom;
    if (! modePads()) canviaModePads (true);
    info = "Pads: "_ca + txt (nom);
    estat.canviat ("Pads de «"_ca + dibuix::utf8 (t->nom) + "»: "_ca + txt (nom));
}

void PianoRoll::menuPresetsPads()
{
    auto* t = pista();
    if (t == nullptr) return;
    juce::PopupMenu m, pop, funk;
    m.addItem (1, "Acords de la tonalitat ("_ca + juce::String::fromUTF8 (harmonia::nomTonalitat (tonalitat()).c_str()) + ")");
    m.addSeparator();
    const auto& v = pads::presets();
    for (int i = 0; i < (int) v.size(); ++i)
    {
        const auto& p = v[(size_t) i];
        juce::String acords;
        for (int k = 0; k < 4; ++k) acords += (k > 0 ? " " : "") + juce::String (p.acords[(size_t) k]);
        const auto text = juce::String::fromUTF8 (p.titol) + " - " + juce::String::fromUTF8 (p.artista) + "      " + acords + " …";
        (juce::String::fromUTF8 (p.grup) == "Funk" ? funk : pop).addItem (100 + i, text, true, t->presetPads == p.titol);
    }
    // 0.12: una tonalitat qualsevol (nota i escala), amb el mateix sistema que «Acords de la tonalitat».
    juce::PopupMenu tonalitats;
    for (int to = 0; to < 12; ++to)
    {
        juce::PopupMenu esc;
        for (int e = 0; e < (int) harmonia::escales().size() && e < 20; ++e)
            esc.addItem (1000 + to * 20 + e, txt (harmonia::nomTonalitat ({ to, e })));
        tonalitats.addSubMenu (juce::String (harmonia::nomTo (to)), esc);
    }
    m.addSubMenu ("Tonalitat", tonalitats);
    m.addSubMenu ("Pop, rock i indie", pop);
    m.addSubMenu ("Funk", funk);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&btPresetPads), [this] (int r)
    {
        auto* tt = pista();
        if (r == 0 || tt == nullptr) return;
        estat.desfes.abans (estat.projecte);
        if (r >= 1000)
        {
            const Tonalitat t { (r - 1000) / 20, (r - 1000) % 20 };
            tt->pads = pads::padsDeTonalitat (t);
            tt->presetPads = "Tonalitat: " + harmonia::nomTonalitat (t);
            info = "Pads: "_ca + txt (tt->presetPads);
            estat.canviat ("Pads de «"_ca + dibuix::utf8 (tt->nom) + "»: "_ca + txt (tt->presetPads));
            return;
        }
        if (r == 1)
        {
            tt->pads.clear();
            tt->presetPads.clear();
            info = "Pads: acords de la tonalitat"_ca;
        }
        else
        {
            const auto& p = pads::presets()[(size_t) (r - 100)];
            tt->pads = pads::padsDePreset (r - 100);
            tt->presetPads = p.titol;
            info = "Pads: "_ca + juce::String::fromUTF8 (p.titol) + " (a les tecles blanques, la seqüència; a les negres, acords de color)"_ca;
        }
        estat.canviat ("Pads de «"_ca + dibuix::utf8 (tt->nom) + "»: "_ca + (r == 1 ? "acords de la tonalitat"_ca : juce::String::fromUTF8 (pads::presets()[(size_t) (r - 100)].titol)));
    });
}

void PianoRoll::pintaPads (juce::Graphics& g)
{
    const auto zc = zonaCarril();
    g.setColour (tema::lateral);
    g.fillRect (0, zc.getY(), ampleTeclat(), zc.getHeight());
    dibuix::pintaEtiquetaSeccio (g, "Chord Pad", juce::Rectangle<int> (8, zc.getY() + 8, ampleTeclat() - 10, 16));
    g.setFont (dibuix::lletra (10.5f));
    g.setColour (tema::suau);
    g.drawFittedText ("Clic: sona. Arrossega'l al Piano Roll. El teclat toca els acords."_ca, juce::Rectangle<int> (8, zc.getY() + 26, ampleTeclat() - 10, 80),
                      juce::Justification::topLeft, 6);
    vintage::panell (g, zc.toFloat());
    if (clip() == nullptr) return;

    const auto v = padsActuals();
    const auto t = tonalitat();
    const auto* pt = const_cast<PianoRoll*> (this)->pista();
    const auto color = pt != nullptr ? EstatEstudi::colorPista (*pt) : tema::midi;
    for (int i = 0; i < pads::numPads; ++i)
    {
        auto r = rectPad (i).toFloat();
        const bool premut = i == padPremut;
        const auto a = harmonia::llegeixAcord (v[(size_t) i]);
        auto cara = vintage::tecla (g, r, premut, premut, color, ! a.buit(), i == padSobre);
        if (a.buit())
        {
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (20.0f));
            g.drawText ("+", cara, juce::Justification::centred, false);
            continue;
        }
        // Les notes de fora de l'escala, un punt taronja a baix (l'acord surt de la tonalitat).
        bool fora = false;
        for (int n : harmonia::notesPad (a)) if (! harmonia::aEscala (n, t)) fora = true;
        g.setColour (premut ? juce::Colours::black : tema::text);
        g.setFont (dibuix::lletra (juce::jlimit (12.0f, 22.0f, cara.getHeight() * 0.32f), true));
        g.drawFittedText (juce::String::fromUTF8 (harmonia::nomPad (a).c_str()), cara.toNearestInt().reduced (4, 2), juce::Justification::centred, 1, 0.7f);
        // La tecla del teclat que el toca (a dalt a l'esquerra) i la inversió (a dalt a la dreta).
        g.setFont (dibuix::lletra (10.0f));
        g.setColour ((premut ? juce::Colours::black : tema::suau).withAlpha (0.8f));
        g.drawText (juce::String (harmonia::nomTo (i)), cara.toNearestInt().reduced (5, 3), juce::Justification::topLeft, false);
        if (a.inversio != 0)
            g.drawText ((a.inversio > 0 ? "inv +" : "inv -") + juce::String (std::abs (a.inversio)), cara.toNearestInt().reduced (5, 3),
                        juce::Justification::topRight, false);
        if (fora)
        {
            g.setColour (tema::accent);
            g.fillEllipse (cara.getCentreX() - 2.5f, cara.getBottom() - 9.0f, 5.0f, 5.0f);
        }
    }
}

//==============================================================================
ZonaEditors::ZonaEditors (EstatEstudi& e, std::function<void (PianoRoll&)> f) : estat (e), configura (std::move (f))
{
    sincronitza();
}

void ZonaEditors::sincronitza()
{
    const int cal = (int) estat.editors.size();
    while (llista.size() > cal) llista.removeLast();
    while (llista.size() < cal)
    {
        auto* pr = llista.add (new PianoRoll (estat, llista.size()));
        if (configura) configura (*pr);
        pr->setDesenganxat (desenganxat);
        addAndMakeVisible (pr);
    }
    for (int i = 0; i < llista.size(); ++i) llista[i]->setIndex (i);
    resized();
    repintaTots();
}

void ZonaEditors::setDesenganxat (bool d)
{
    desenganxat = d;
    for (auto* p : llista) p->setDesenganxat (d);
}

void ZonaEditors::resized()
{
    // Tots de la mateixa alçada (així les referències són iguals), separats per una ratlla.
    auto r = getLocalBounds();
    const int n = llista.size();
    if (n == 0) return;
    const int sep = 3;
    const int alt = (r.getHeight() - sep * (n - 1)) / n;
    for (int i = 0; i < n; ++i)
    {
        llista[i]->setBounds (r.removeFromTop (i == n - 1 ? r.getHeight() : alt));
        r.removeFromTop (sep);
    }
}

//==============================================================================
// 0.20: Drumpad (pista de bateria)
//==============================================================================
void PianoRoll::pintaFilesBateria (juce::Graphics& g)
{
    const auto zg = zonaGraella();
    const auto* t = const_cast<PianoRoll*> (this)->pista();
    if (t == nullptr) return;
    for (int f = 0; f < (int) t->files.size(); ++f)
    {
        const float y = (float) (zg.getY() + f * altNota - desplY);
        if (y > (float) zg.getBottom() || y + (float) altNota < (float) zg.getY()) continue;
        const auto& fila = t->files[(size_t) f];
        juce::Colour fons = (f % 2 == 0) ? juce::Colour (0xff1e1e25) : juce::Colour (0xff1a1a20);
        if (f == estat.filaBateria) fons = fons.brighter (0.12f);
        if (fila.mut) fons = fons.darker (0.5f);
        if (fila.nota == sobreTecla) fons = fons.brighter (0.06f);
        g.setColour (fons);
        g.fillRect (juce::Rectangle<float> ((float) zg.getX(), y, (float) zg.getWidth(), (float) altNota));
        g.setColour (tema::linia.withAlpha (0.6f));
        g.fillRect (juce::Rectangle<float> ((float) zg.getX(), y + (float) altNota - 1.0f, (float) zg.getWidth(), 1.0f));
    }
}

void PianoRoll::pintaNomsBateria (juce::Graphics& g)
{
    const auto zt = zonaTeclat();
    const auto* t = const_cast<PianoRoll*> (this)->pista();
    const auto* c = clip();
    juce::Graphics::ScopedSaveState guarda (g);
    g.reduceClipRegion (zt);
    g.setColour (juce::Colour (0xff141418));
    g.fillRect (zt);
    if (t == nullptr) return;
    std::array<bool, 128> sonant {};
    if (estat.transport != 0 && c != nullptr)
    {
        const double ara2 = midi::ticks (estat.projecte, estat.capcal);
        for (auto& n : c->notes)
            if (tickAbs (n) <= ara2 && tickAbs (n) + 120.0 > ara2) sonant[(size_t) n.to] = true;
    }
    const auto color = EstatEstudi::colorPista (*t);
    for (int f = 0; f < (int) t->files.size(); ++f)
    {
        const auto& fila = t->files[(size_t) f];
        auto r = juce::Rectangle<float> ((float) zt.getX(), (float) (zt.getY() + f * altNota - desplY), (float) zt.getWidth() - 1.0f, (float) altNota);
        if (r.getY() > (float) zt.getBottom() || r.getBottom() < (float) zt.getY()) continue;
        const bool encesa = premudes[(size_t) fila.nota] || sonant[(size_t) fila.nota];
        vintage::tecla (g, r.reduced (1.0f, 0.5f), encesa, f == estat.filaBateria, color);
        // «M»: silenci de la fila.
        auto m = r.removeFromRight (22.0f).reduced (3.0f, juce::jmax (1.0f, (r.getHeight() - 14.0f) / 2.0f));
        g.setColour (fila.mut ? juce::Colour (0xff6fa8ff) : juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (m, 2.0f);
        g.setColour (fila.mut ? juce::Colours::black : tema::suau);
        g.setFont (dibuix::lletra (9.5f, true));
        g.drawText ("M", m, juce::Justification::centred, false);
        g.setColour (fila.mut ? tema::suau : tema::text);
        g.setFont (dibuix::lletra (juce::jmin (12.0f, (float) altNota - 2.0f), f == estat.filaBateria));
        g.drawFittedText (juce::String::fromUTF8 ((fila.nom.empty() ? mibase::bateria::nomTipus (fila.tipus) : fila.nom).c_str()),
                          r.reduced (6.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.75f);
    }
    g.setColour (tema::linia);
    g.fillRect (zt.getRight() - 1, zt.getY(), 1, zt.getHeight());
}

FinestraPianoRoll::FinestraPianoRoll (juce::Component& pr, std::function<void()> enTancar)
    : juce::DocumentWindow ("Piano Roll · Mibase"_ca, tema::fons, juce::DocumentWindow::allButtons), tanca (std::move (enTancar))
{
    setUsingNativeTitleBar (true);
    setResizable (true, true);
    setResizeLimits (700, 360, 10000, 10000);
    setContentNonOwned (&pr, false);
    centreWithSize (1280, 720);
    setVisible (true);
}
