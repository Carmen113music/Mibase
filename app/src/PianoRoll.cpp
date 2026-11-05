#include "PianoRoll.h"

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
PianoRoll::PianoRoll (EstatEstudi& e) : SeccioEstudi (e)
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
    btFantasmes.setTooltip ("Notes fantasma: mostra les notes de les altres pistes MIDI com a referència"_ca);
    btFantasmes.onClick = [this] { fantasmes = ! fantasmes; btFantasmes.setToggleState (fantasmes, juce::dontSendNotification); repaint(); };
    btFantasmes.setToggleState (fantasmes, juce::dontSendNotification);
    btOctAmunt.setTooltip ("Puja la vista una octava (Re Pàg)"_ca);
    btOctAmunt.onClick = [this] { canviaOctava (1); };
    btOctAvall.setTooltip ("Baixa la vista una octava (Av Pàg)"_ca);
    btOctAvall.onClick = [this] { canviaOctava (-1); };
    btDesenganxa.setTooltip ("Desenganxa el Piano Roll en una finestra a part (per portar-lo a una altra pantalla)"_ca);
    btDesenganxa.onClick = [this] { if (onDesenganxa) onDesenganxa(); };
    btTanca.setTooltip ("Amaga el Piano Roll (doble clic a un clip MIDI el torna a obrir)"_ca);
    btTanca.onClick = [this] { if (onTanca) onTanca(); };

    for (auto* b : std::initializer_list<juce::Component*> { &btGraella, &btDurada, &btQuantitza, &velNova, &btFantasmes,
                                                             &btOctAmunt, &btOctAvall, &btDesenganxa, &btTanca })
        addAndMakeVisible (b);
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
    btTanca.setVisible (! d);
    resized();
}

//==============================================================================
// Geometria
//==============================================================================
juce::Rectangle<int> PianoRoll::zonaRegla() const   { return { ampleTeclat, alcadaBarra, getWidth() - ampleTeclat - gruix, alcadaRegla }; }
juce::Rectangle<int> PianoRoll::zonaGraella() const
{
    const int y = alcadaBarra + alcadaRegla;
    return { ampleTeclat, y, getWidth() - ampleTeclat - gruix, juce::jmax (20, getHeight() - gruix - altCarril - 6 - y) };
}
juce::Rectangle<int> PianoRoll::zonaTeclat() const  { auto g = zonaGraella(); return { 0, g.getY(), ampleTeclat, g.getHeight() }; }
juce::Rectangle<int> PianoRoll::zonaVoraCarril() const { auto g = zonaGraella(); return { 0, g.getBottom(), getWidth(), 6 }; }
juce::Rectangle<int> PianoRoll::zonaCarril() const
{
    auto v = zonaVoraCarril();
    return { ampleTeclat, v.getBottom(), getWidth() - ampleTeclat - gruix, juce::jmax (10, getHeight() - gruix - v.getBottom()) };
}

PianoRoll::Zona PianoRoll::zonaA (juce::Point<int> p) const
{
    if (p.y < alcadaBarra) return Zona::Barra;
    if (zonaVoraCarril().contains (p)) return Zona::VoraCarril;
    if (zonaRegla().contains (p)) return Zona::Regla;
    if (zonaTeclat().contains (p)) return Zona::Teclat;
    if (zonaGraella().contains (p)) return Zona::Graella;
    if (zonaCarril().contains (p)) return Zona::Carril;
    return Zona::Res;
}

float PianoRoll::xTick (double t) const { return (float) zonaGraella().getX() + (float) ((t - iniciVista) * pxPerTick); }
double PianoRoll::tickX (float x) const { return iniciVista + (double) (x - (float) zonaGraella().getX()) / pxPerTick; }
float PianoRoll::yTo (int to) const     { return (float) (zonaGraella().getY() + (127 - to) * altNota - desplY); }
int PianoRoll::toY (float y) const
{
    return juce::jlimit (0, 127, 127 - (int) std::floor ((y - (float) zonaGraella().getY() + (float) desplY) / (float) altNota));
}

//==============================================================================
// Clip obert
//==============================================================================
mibase::Clip* PianoRoll::clip() { return estat.clipMidiObert(); }
const mibase::Clip* PianoRoll::clip() const { return const_cast<EstatEstudi&> (estat).clipMidiObert(); }
mibase::Pista* PianoRoll::pista() { return clip() != nullptr ? estat.pista (estat.clipObert.pista) : nullptr; }

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
    if (estat.clipObert.pista != pistaVista || estat.clipObert.clip != clipVist)
    {
        pistaVista = estat.clipObert.pista;
        clipVist = estat.clipObert.clip;
        triadesNotes.clear();
        apagaProva();
        if (clip() != nullptr) encaixaVista();
    }
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

void PianoRoll::encaixaVista()
{
    const auto* c = clip();
    const auto g = zonaGraella();
    if (c == nullptr) return;
    if (g.getWidth() < 40) { vistaPendent = true; return; }
    vistaPendent = false;
    const double a = posTicks() + midi::ticks (estat.projecte, c->inici);
    const double b = posTicks() + midi::ticks (estat.projecte, c->fi);
    pxPerTick = juce::jlimit (0.004, 2.0, (double) (g.getWidth() - 40) / juce::jmax (1.0, b - a));
    iniciVista = a - 16.0 / pxPerTick;

    int toMin = 127, toMax = 0;
    for (auto& n : c->notes) { toMin = juce::jmin (toMin, n.to); toMax = juce::jmax (toMax, n.to); }
    const int centre = c->notes.empty() ? 64 : (toMin + toMax) / 2;
    desplY = juce::jlimit (0, juce::jmax (0, 128 * altNota - g.getHeight()), (127 - centre) * altNota - g.getHeight() / 2);
    actualitzaBarres();
}

void PianoRoll::ajustaVista() { encaixaVista(); repaint(); }

void PianoRoll::ticTransport()
{
    if (! isShowing() || clip() == nullptr) return;
    const auto g = zonaGraella();
    const double t = midi::ticks (estat.projecte, estat.capcal);
    if (estat.transport != 0 && mode == Mode::Res && (t > tickX ((float) g.getRight() - 12.0f) || t < iniciVista))
    {
        iniciVista = t - 24.0 / pxPerTick;
        actualitzaBarres();
    }
    repaint();
}

void PianoRoll::canviaOctava (int d)
{
    desplY = juce::jlimit (0, juce::jmax (0, 128 * altNota - zonaGraella().getHeight()), desplY - d * 12 * altNota);
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
    barraV.setRangeLimits (0.0, 128.0 * altNota, juce::dontSendNotification);
    barraV.setCurrentRange (desplY, g.getHeight(), juce::dontSendNotification);
}

void PianoRoll::scrollBarMoved (juce::ScrollBar* b, double v)
{
    if (b == &barraH) iniciVista = v;
    else desplY = juce::jmax (0, (int) v);
    repaint();
}

void PianoRoll::resized()
{
    auto b = getLocalBounds().removeFromTop (alcadaBarra).reduced (10, 7);
    for (auto* t : eines) { t->setBounds (b.removeFromLeft (36)); b.removeFromLeft (3); }
    b.removeFromLeft (12);
    btGraella.setBounds (b.removeFromLeft (112)); b.removeFromLeft (4);
    btDurada.setBounds (b.removeFromLeft (116)); b.removeFromLeft (4);
    btQuantitza.setBounds (b.removeFromLeft (btQuantitza.ampladaIdeal() + 8)); b.removeFromLeft (10);
    velNova.setBounds (b.removeFromLeft (76).reduced (0, 2)); b.removeFromLeft (10);
    btFantasmes.setBounds (b.removeFromLeft (36)); b.removeFromLeft (10);
    btOctAvall.setBounds (b.removeFromLeft (54)); b.removeFromLeft (3);
    btOctAmunt.setBounds (b.removeFromLeft (54)); b.removeFromLeft (10);
    if (btTanca.isVisible()) { btTanca.setBounds (b.removeFromRight (36)); b.removeFromRight (3); }
    btDesenganxa.setBounds (b.removeFromRight (36));
    b.removeFromRight (10);
    zonaInfo = b;

    const auto g = zonaGraella();
    barraV.setBounds (g.getRight(), g.getY(), gruix, g.getHeight());
    barraH.setBounds (ampleTeclat, getHeight() - gruix, getWidth() - ampleTeclat - gruix, gruix);
    if (vistaPendent && clip() != nullptr) encaixaVista();
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
    if (! onDirecte || estat.clipObert.pista < 0) return;
    if (toSonant >= 0) notaOff (toSonant);
    onDirecte (estat.clipObert.pista, 0x90, (std::uint8_t) juce::jlimit (0, 127, to), (std::uint8_t) juce::jlimit (1, 127, vel));
    toSonant = to;
    premudes[(size_t) juce::jlimit (0, 127, to)] = true;
    repaint (zonaTeclat());
}

void PianoRoll::notaOff (int to)
{
    if (! onDirecte || to < 0) return;
    onDirecte (estat.clipObert.pista, 0x80, (std::uint8_t) juce::jlimit (0, 127, to), 0);
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
    if (! onDirecte || estat.clipObert.pista < 0) return;
    const int p = estat.clipObert.pista;
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
       + " · " + juce::String (quantes) + (quantes == 1 ? " nota" : " notes"));
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
    aplica ([this, passos, p] (Clip& k) { midi::canviaDurada (k, triadesNotes, passos * p, false, p); }, passos > 0 ? "Més llarga" : "Més curta");
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
    m.addSectionHeader ("Graella i quantització");
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
    const auto z = zonaA (e.getPosition());
    Toc t = z == Zona::Graella ? tocA (e.position) : Toc {};
    const int c = z == Zona::Carril ? notaCarrilA (e.position) : -1;
    const int tecla = (z == Zona::Teclat || z == Zona::Graella) ? toY (e.position.y) : -1;
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
        case Zona::Teclat: case Zona::Regla: setMouseCursor (juce::MouseCursor::PointingHandCursor); break;
        case Zona::Carril: setMouseCursor (c >= 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::CrosshairCursor); break;
        case Zona::Graella:
            if (t.nota >= 0 && eina != EinaPR::Goma)
                setMouseCursor (t.part == Part::Cos ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::LeftRightResizeCursor);
            else
                setMouseCursor (VistaPistes::cursor (eina == EinaPR::Fletxa ? Eina::Fletxa : eina == EinaPR::Llapis ? Eina::Llapis : Eina::Goma));
            break;
        case Zona::Barra: case Zona::Res: setMouseCursor (juce::MouseCursor::NormalCursor); break;
    }
}

void PianoRoll::mouseExit (const juce::MouseEvent&)
{
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

    if (z == Zona::VoraCarril) { mode = Mode::VoraCarril; alt0 = altCarril; return; }
    if (c == nullptr) return;

    if (z == Zona::Teclat)
    {
        mode = Mode::Teclat;
        const int vel = juce::jlimit (30, 127, 30 + (int) (97.0f * e.position.x / (float) ampleTeclat));
        notaOn (toY (e.position.y), vel);
        return;
    }
    if (z == Zona::Regla)
    {
        mode = Mode::Regla;
        estat.mouCapcal (juce::jmax (0.0, midi::segons (estat.projecte, tickX (e.position.x))), true);
        estat.avisa();
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
        if (onAjustaTo) { to = onAjustaTo (to); if (to < 0) return; }
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

        case Mode::VoraCarril:
            altCarril = juce::jlimit (40, juce::jmax (60, getHeight() / 2), alt0 - e.getDistanceFromDragStartY());
            resized();
            repaint();
            return;

        case Mode::Teclat:
        {
            const int to = toY (e.position.y);
            if (to != toSonant) notaOn (to, juce::jlimit (30, 127, 30 + (int) (97.0f * e.position.x / (float) ampleTeclat)));
            return;
        }

        case Mode::Regla:
            estat.mouCapcal (juce::jmax (0.0, midi::segons (estat.projecte, tickX (e.position.x))), true);
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
            if (onAjustaTo && dTo != 0) { const int a = onAjustaTo (to0 + dTo); if (a >= 0) dTo = a - to0; }
            midi::mou (*clip(), triadesNotes, dTicks, dTo, -(long long) std::llround (posTicks()));
            midi::encabeix (estat.projecte, *clip());
            const int toAra = juce::jlimit (0, 127, to0 + dTo);
            if (toAra != toSonant && toSonant >= 0) notaOn (toAra, velNova.getValor());
            info = txt (midi::nomNota (toAra)) + " · " + posicioTick (estat.projecte, posTicks() + (double) (ini0 + dTicks))
                 + (triadesNotes.size() > 1 ? " · " + juce::String (triadesNotes.size()) + " notes" : juce::String());
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
    switch (mode)
    {
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
    if (onAjustaTo) { to = onAjustaTo (to); if (to < 0) return; }
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
    desplY = juce::jlimit (0, juce::jmax (0, 128 * altNota - g.getHeight()), desplY);
    actualitzaBarres();
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
    }
    if (mods.isCommandDown() && lletra ('Z')) { if (onDesfes) onDesfes (mods.isShiftDown()); info = mods.isShiftDown() ? "Refet" : "Desfet"; repaint(); return true; }
    if (mods.isCommandDown() && lletra ('Y')) { if (onDesfes) onDesfes (true); info = "Refet"; repaint(); return true; }
    if (cap && codi >= '1' && codi <= '3') { triaEina ((EinaPR) (codi - '1')); return true; }
    if (codi == juce::KeyPress::escapeKey) { triadesNotes.clear(); triaEina (EinaPR::Fletxa); info.clear(); return true; }
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
    pintaTeclat (g);
    pintaCarril (g);

    // Vora entre la graella i el carril.
    const auto v = zonaVoraCarril();
    vintage::panell (g, v.toFloat());
    g.setColour (mode == Mode::VoraCarril ? tema::accent : tema::linia2);
    g.fillRect (v.getCentreX() - 18, v.getCentreY() - 1, 36, 2);
    // Racó de baix a l'esquerra.
    g.setColour (tema::lateral);
    g.fillRect (0, getHeight() - gruix, ampleTeclat, gruix);
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

    if (! zonaInfo.isEmpty())
    {
        auto r = zonaInfo;
        juce::String titol = "PIANO ROLL";
        if (const auto* t = const_cast<PianoRoll*> (this)->pista())
            titol = dibuix::utf8 (t->nom) + " · "_ca + dibuix::utf8 (midi::nomInstrument (t->instrument.empty() ? "intern:piano" : t->instrument));
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

    // Files: les de les tecles negres, més fosques; línies entre si i mi-fa més marcades.
    const int toDalt = toY ((float) zg.getY()), toBaix = toY ((float) zg.getBottom() - 1.0f);
    for (int to = toBaix; to <= toDalt; ++to)
    {
        const float y = yTo (to);
        juce::Colour fons = midi::esNegra (to) ? juce::Colour (0xff131317) : juce::Colour (0xff1b1b21);
        if (onColorFila) fons = onColorFila (to, fons);
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
    if (pg > 0 && pg < ticksPerNegra) linies ((double) pg, tema::linia.withAlpha (0.35f), 1.0f);
    linies ((double) ticksPerNegra, tema::linia.withAlpha (0.9f), 1.0f);
    linies (tpc, tema::linia2, 1.5f);

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

    // Notes fantasma: les de les altres pistes MIDI (i dels altres clips d'aquesta), com a referència.
    if (fantasmes)
    {
        for (int i = 0; i < estat.numPistes(); ++i)
        {
            const auto& pt = p.pistes[(size_t) i];
            if (pt.tipus != "midi") continue;
            const auto col = EstatEstudi::colorPista (pt);
            for (int k = 0; k < (int) pt.clips.size(); ++k)
            {
                if (i == estat.clipObert.pista && k == estat.clipObert.clip) continue;
                const auto& kc = pt.clips[(size_t) k];
                if (! kc.midi || kc.mut) continue;
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
    const auto trep = midi::trepitjades (c->notes);
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
        if (r.getWidth() > 28.0f && altNota >= 11)
        {
            g.setColour (juce::Colours::black.withAlpha (0.75f));
            g.setFont (dibuix::lletra (juce::jmin (11.0f, (float) altNota - 3.0f), true));
            g.drawText (txt (midi::nomNota (n.to)), r.reduced (4.0f, 0.0f), juce::Justification::centredLeft, false);
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
    g.fillRect (0, zr.getY(), ampleTeclat, zr.getHeight());
    juce::Graphics::ScopedSaveState guarda (g);
    g.reduceClipRegion (zr);
    const double tpc = (double) midi::ticksPerCompas (p);
    const double t0 = tickX ((float) zr.getX()), t1 = tickX ((float) zr.getRight());
    const int cada = tpc * pxPerTick < 36.0 ? (tpc * pxPerTick < 12.0 ? 8 : 2) : 1;
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

void PianoRoll::pintaCarril (juce::Graphics& g)
{
    const auto zc = zonaCarril();
    const auto* c = clip();
    const auto* t = const_cast<PianoRoll*> (this)->pista();
    g.setColour (tema::lateral);
    g.fillRect (0, zc.getY(), ampleTeclat, zc.getHeight());
    dibuix::pintaEtiquetaSeccio (g, "Velocitat", juce::Rectangle<int> (8, zc.getY() + 4, ampleTeclat - 10, 16));
    g.setFont (dibuix::lletra (10.5f));
    g.setColour (tema::suau);
    g.drawFittedText ("Maj: línia"_ca, juce::Rectangle<int> (8, zc.getY() + 20, ampleTeclat - 10, 28), juce::Justification::topLeft, 2);

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
FinestraPianoRoll::FinestraPianoRoll (PianoRoll& pr, std::function<void()> enTancar)
    : juce::DocumentWindow ("Piano Roll · Mibase"_ca, tema::fons, juce::DocumentWindow::allButtons), tanca (std::move (enTancar))
{
    setUsingNativeTitleBar (true);
    setResizable (true, true);
    setResizeLimits (700, 360, 10000, 10000);
    setContentNonOwned (&pr, false);
    centreWithSize (1280, 720);
    setVisible (true);
}
