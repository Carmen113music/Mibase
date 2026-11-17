#include "EditorAutomatitzacio.h"
#include "Text.h"
#include "Vintage.h"
#include <map>
#include <mibase/Edicio.h>

namespace au = mibase::automatitzacio;
namespace ed = mibase::edicio;

namespace
{
    juce::String u8 (const std::string& s) { return juce::String::fromUTF8 (s.c_str(), (int) s.size()); }
    const juce::Colour colAuto { 0xffffc857 };   // groc: el color de l'automatització
    std::map<std::string, std::pair<mibase::Parametritzable*, int>>& cauIndexs() { static std::map<std::string, std::pair<mibase::Parametritzable*, int>> m; return m; }
}

EditorAutomatitzacio::EditorAutomatitzacio (EstatEstudi& e, GestorPlugins& g) : estat (e), plugins (g)
{
    setWantsKeyboardFocus (true);
    for (juce::Component* c : { (juce::Component*) &btLinia, (juce::Component*) &segEina, (juce::Component*) &segForma, (juce::Component*) &btGraella,
                                (juce::Component*) &btQuant, (juce::Component*) &btCopia, (juce::Component*) &btEnganxa, (juce::Component*) &btDuplica,
                                (juce::Component*) &btMenys, (juce::Component*) &btMes, (juce::Component*) &btEstudi, (juce::Component*) &btTanca })
        addAndMakeVisible (c);
    btLinia.setTooltip ("Afegeix una línia: volum, panorama o qualsevol paràmetre dels efectes i de l'instrument de la pista."_ca);
    btLinia.onClick = [this] { menuLinia (&btLinia); };
    segEina.setTooltip ("Fletxa (1): tria i mou punts; doble clic posa o treu un punt; Alt + arrossegar un tram el corba. Llapis (2): dibuixa a mà."_ca);
    segEina.onCanvi = [this] (int i) { estat.automat.eina = i; };
    segForma.setTooltip ("Forma dels trams nous (i dels punts triats): recte, corba suau o esglaó."_ca);
    segForma.onCanvi = [this] (int f) { estat.automat.forma = f; posaForma (f); };
    btGraella.setLlum (colAuto);
    btGraella.setTooltip ("Els punts s'enganxen a la graella del projecte (Alt mentre arrossegues: lliure)."_ca);
    btGraella.onClick = [this] { estat.automat.graella = ! estat.automat.graella; btGraella.setToggleState (estat.automat.graella, juce::dontSendNotification); };
    btQuant.setTooltip ("Quantitza els punts triats a la graella (Q)."_ca);
    btQuant.onClick = [this] { quantitzaTriats(); };
    btCopia.setTooltip ("Copia el tros entre el primer i el darrer punt triats (Ctrl + C)."_ca);
    btCopia.onClick = [this] { copia(); };
    btEnganxa.setTooltip ("Enganxa el tros copiat al capçal (Ctrl + V)."_ca);
    btEnganxa.onClick = [this] { enganxaAlCapcal(); };
    btDuplica.setTooltip ("Duplica el tros triat just després (Ctrl + D)."_ca);
    btDuplica.onClick = [this] { duplicaTriats(); };
    btMenys.setTooltip ("Allunya (Ctrl + roda)"_ca);
    btMes.setTooltip ("Apropa (Ctrl + roda)"_ca);
    btMenys.onClick = [this] { zoom (1.0 / 1.5, (float) zonaCorba (0).getCentreX()); };
    btMes.onClick = [this] { zoom (1.5, (float) zonaCorba (0).getCentreX()); };
    btEstudi.setTooltip ("Posa el mateix zoom i el mateix tros de cançó que la taula de treball."_ca);
    btEstudi.onClick = [this] { igualQueEstudi(); };
    btTanca.setTooltip ("Tanca l'editor d'automatització (les corbes continuen sonant)."_ca);
    btTanca.onClick = [this] { if (onTanca) onTanca(); };
}

// ---------------------------------------------------------------------------
mibase::Pista* EditorAutomatitzacio::pista()
{
    const int i = ed::pistaPerId (estat.projecte, estat.automat.pista);
    return i >= 0 ? estat.pista (i) : nullptr;
}

int EditorAutomatitzacio::numLinies() { auto* t = pista(); return t != nullptr ? (int) t->autos.size() : 0; }

mibase::Automatitzacio* EditorAutomatitzacio::linia (int i)
{
    auto* t = pista();
    return t != nullptr && i >= 0 && i < (int) t->autos.size() ? &t->autos[(size_t) i] : nullptr;
}

mibase::Automatitzacio* EditorAutomatitzacio::liniaTriada()
{
    auto* t = pista();
    if (t == nullptr) return nullptr;
    for (auto& a : t->autos) if (a.id == estat.automat.linia) return &a;
    return nullptr;
}

void EditorAutomatitzacio::obre (int p, const std::string& objectiu, const std::string& nom)
{
    auto* t = estat.pista (p);
    if (t == nullptr) return;
    if (estat.automat.pista != t->id) { estat.automat.linia.clear(); estat.automat.punts.clear(); }
    estat.automat.pista = t->id;
    estat.automat.obert = true;
    if (estat.automat.iniciVista < 0.0 || estat.automat.pxPerSeg <= 0.0) igualQueEstudi();
    if (! objectiu.empty())
    {
        if (auto* a = au::busca (*t, objectiu)) estat.automat.linia = a->id;
        else
        {
            abans();
            estat.automat.linia = au::afegeix (estat.projecte, *estat.pista (p), objectiu, nom).id;
            fet ("Automatització nova: "_ca + u8 (nom));
        }
    }
    else if (liniaTriada() == nullptr && ! t->autos.empty()) estat.automat.linia = t->autos.front().id;
    segEina.setTriat (estat.automat.eina);
    segForma.setTriat (estat.automat.forma);
    btGraella.setToggleState (estat.automat.graella, juce::dontSendNotification);
    repaint();
}

void EditorAutomatitzacio::igualQueEstudi()
{
    estat.automat.iniciVista = estat.iniciVista;
    estat.automat.pxPerSeg = estat.pxPerSegon;
    repaint();
}

void EditorAutomatitzacio::zoom (double f, float x)
{
    const double s = xASeg (x);
    estat.automat.pxPerSeg = juce::jlimit (2.0, 4000.0, estat.automat.pxPerSeg * f);
    estat.automat.iniciVista = juce::jmax (-2.0, s - (x - zonaCorba (0).getX()) / estat.automat.pxPerSeg);
    repaint();
}

// ---------------------------------------------------------------------------
juce::Rectangle<int> EditorAutomatitzacio::zonaLinia (int i) const
{
    const int n = juce::jmax (1, const_cast<EditorAutomatitzacio*> (this)->numLinies());
    const int h = juce::jmax (48, zonaLinies.getHeight() / n);
    return { zonaLinies.getX(), zonaLinies.getY() + i * h, zonaLinies.getWidth(), h };
}
juce::Rectangle<int> EditorAutomatitzacio::zonaCap (int i) const { return zonaLinia (i).removeFromLeft (ampleCap); }
juce::Rectangle<int> EditorAutomatitzacio::zonaCorba (int i) const { return zonaLinia (i).withTrimmedLeft (ampleCap).reduced (0, 4); }

int EditorAutomatitzacio::liniaA (juce::Point<int> p) const
{
    if (! zonaLinies.contains (p)) return -1;
    for (int i = 0, n = const_cast<EditorAutomatitzacio*> (this)->numLinies(); i < n; ++i)
        if (zonaLinia (i).contains (p)) return i;
    return -1;
}

double EditorAutomatitzacio::xASeg (float x) const { return estat.automat.iniciVista + (x - (float) (zonaLinies.getX() + ampleCap)) / estat.automat.pxPerSeg; }
float EditorAutomatitzacio::segAX (double s) const { return (float) (zonaLinies.getX() + ampleCap) + (float) ((s - estat.automat.iniciVista) * estat.automat.pxPerSeg); }
double EditorAutomatitzacio::xACompas (float x) const { return ed::compasDeSegons (estat.projecte, xASeg (x)); }
float EditorAutomatitzacio::compasAX (double c) const { return segAX (ed::segonsDeCompas (estat.projecte, c)); }
double EditorAutomatitzacio::yAValor (const juce::Rectangle<int>& z, float y) const { return juce::jlimit (0.0, 1.0, (double) (z.getBottom() - y) / juce::jmax (1, z.getHeight())); }
float EditorAutomatitzacio::valorAY (const juce::Rectangle<int>& z, double v) const { return (float) z.getBottom() - (float) (v * z.getHeight()); }

double EditorAutomatitzacio::enganxa (double c) const
{
    if (! estat.automat.graella || juce::ModifierKeys::currentModifiers.isAltDown()) return c;
    const auto& p = estat.projecte;
    const double ticksCompas = mibase::ticksPerNegra * 4.0 * juce::jmax (1, p.compasNum) / juce::jmax (1, p.compasDen);
    const double g = (p.graella > 0 ? (double) p.graella : (double) mibase::ticksPerNegra) / ticksCompas;
    return std::round (c / g) * g;
}

int EditorAutomatitzacio::puntA (int i, juce::Point<float> p) const
{
    auto* a = const_cast<EditorAutomatitzacio*> (this)->linia (i);
    if (a == nullptr) return -1;
    const auto z = zonaCorba (i);
    int millor = -1;
    float dMin = 7.0f;
    for (size_t k = 0; k < a->punts.size(); ++k)
    {
        const float d = p.getDistanceFrom ({ compasAX (a->punts[k].compas), valorAY (z, a->punts[k].valor) });
        if (d < dMin) { dMin = d; millor = (int) k; }
    }
    return millor;
}

mibase::Parametritzable* EditorAutomatitzacio::objecte (const mibase::Automatitzacio& a, int& index)
{
    index = -1;
    std::string ii, pid;
    const int tipus = au::llegeixObjectiu (a.objectiu, ii, pid);
    auto* t = pista();
    if (t == nullptr) return nullptr;
    mibase::Parametritzable* obj = nullptr;
    if (tipus == 2) { for (auto& x : t->inserts) if (x.id == ii) obj = plugins.parametres (x); }
    else if (tipus == 3) obj = plugins.parametresInstrument (t->id);
    if (obj == nullptr) return nullptr;
    auto& c = cauIndexs()[t->id + "|" + a.objectiu];
    if (c.first != obj) c = { obj, obj->indexParametre (pid) };
    index = c.second;
    return index >= 0 ? obj : nullptr;
}

juce::String EditorAutomatitzacio::textValor (const mibase::Automatitzacio& a, double v)
{
    std::string ii, pid;
    const int tipus = au::llegeixObjectiu (a.objectiu, ii, pid);
    if (tipus == 0) return u8 (au::textVolum (v));
    if (tipus == 1) return u8 (au::textPan (v));
    int ix = -1;
    if (auto* o = objecte (a, ix)) return u8 (o->textParametre (ix, (float) v));
    return juce::String (juce::roundToInt (v * 100.0)) + " %";
}

void EditorAutomatitzacio::abans() { estat.desfes.abans (estat.projecte); }
void EditorAutomatitzacio::fet (const juce::String& evolucio) { estat.canviat (evolucio); repaint(); }

void EditorAutomatitzacio::triaLinia (int i)
{
    auto* a = linia (i);
    if (a == nullptr || a->id == estat.automat.linia) return;
    estat.automat.linia = a->id;
    estat.automat.punts.clear();
    repaint();
}

// ---------------------------------------------------------------------------
void EditorAutomatitzacio::menuLinia (juce::Component* on, const std::string& idInsert)
{
    auto* t = pista();
    if (t == nullptr) return;
    auto opcions = std::make_shared<std::vector<std::pair<std::string, std::string>>>();   // objectiu, nom
    auto item = [&] (juce::PopupMenu& m, const std::string& obj, const std::string& nom, const juce::String& text, bool pot)
    {
        opcions->push_back ({ obj, nom });
        m.addItem ((int) opcions->size(), text, pot, au::busca (*t, obj) != nullptr);
    };
    auto params = [&] (juce::PopupMenu& m, mibase::Parametritzable* o, const std::function<std::string (const std::string&)>& objectiu, const std::string& prefix)
    {
        int n = 0;
        for (int i = 0, q = o->numParametres(); i < q && n < 600; ++i)
        {
            const auto inf = o->infoParametre (i);
            if (! inf.automatitzable || inf.nom.empty()) continue;
            if (n > 0 && n % 36 == 0) m.addColumnBreak();
            item (m, objectiu (inf.id), prefix + inf.nom, u8 (inf.nom), true);
            ++n;
        }
        if (n == 0) m.addItem (-1, "(no té paràmetres automatitzables)"_ca, false);
    };
    juce::PopupMenu m;
    if (idInsert.empty())
    {
        item (m, au::volum, "Volum", "Volum", true);
        item (m, au::pan, "Panorama", "Panorama", true);
        if (mibase::esMidi (*t))
        {
            if (GestorPlugins::esVst3 (t->instrument))
            {
                if (auto* o = plugins.parametresInstrument (t->id))
                {
                    juce::PopupMenu sub;
                    params (sub, o, [] (const std::string& id) { return au::objectiuInstrument (id); }, "Instrument · ");
                    m.addSubMenu ("Instrument · "_ca + u8 (t->nomInstrument), sub);
                }
                else m.addItem (-1, "Instrument: encara no s'ha obert (fes sonar la pista un moment)"_ca, false);
            }
            else m.addItem (-1, "Instrument intern: de moment, sense paràmetres automatitzables"_ca, false);
        }
        m.addSeparator();
    }
    for (auto& x : t->inserts)
    {
        if (mibase::esInsertVeu (x) || (! idInsert.empty() && x.id != idInsert)) continue;
        auto* o = plugins.parametres (x);
        if (o == nullptr) { m.addItem (-1, u8 (x.nom) + " (apagat o no s'ha pogut obrir)"_ca, false); continue; }
        juce::PopupMenu sub;
        const auto id = x.id;
        params (sub, o, [id] (const std::string& p) { return au::objectiuInsert (id, p); }, x.nom + " · ");
        if (idInsert.empty()) m.addSubMenu (u8 (x.nom), sub);
        else m = sub;
    }
    if (t->inserts.empty() && idInsert.empty()) m.addItem (-1, "(la pista no té efectes)"_ca, false);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (on), [this, opcions] (int r)
    {
        if (r <= 0 || r > (int) opcions->size()) return;
        const auto& [obj, nom] = (*opcions)[(size_t) r - 1];
        obre (ed::pistaPerId (estat.projecte, estat.automat.pista), obj, nom);
    });
}

void EditorAutomatitzacio::menuPunt (int li, int k)
{
    auto* a = linia (li);
    if (a == nullptr || k < 0 || k >= (int) a->punts.size()) return;
    juce::PopupMenu m;
    m.addSectionHeader (textValor (*a, a->punts[(size_t) k].valor));
    m.addItem (1, "Escriu el valor…"_ca);
    m.addItem (2, "Tram recte"_ca, true, a->punts[(size_t) k].forma == 0);
    m.addItem (3, "Tram suau (corba en S)"_ca, true, a->punts[(size_t) k].forma == 1);
    m.addItem (4, "Esglaó (es manté fins al punt següent)"_ca, true, a->punts[(size_t) k].forma == 2);
    m.addItem (6, "Treu la corba del tram"_ca, std::abs (a->punts[(size_t) k].tensio) > 1e-4);
    m.addSeparator();
    m.addItem (5, "Esborra el punt"_ca);
    const auto id = a->id;
    m.showMenuAsync (juce::PopupMenu::Options(), [this, li, k, id] (int r)
    {
        auto* b = linia (li);
        if (b == nullptr || b->id != id || k >= (int) b->punts.size()) return;
        if (r == 1) { escriuValor (li, k); return; }
        if (r == 0) return;
        abans();
        if (r >= 2 && r <= 4) b->punts[(size_t) k].forma = r - 2;
        if (r == 6) b->punts[(size_t) k].tensio = 0.0;
        if (r == 5) { au::esborra (*b, { k }); estat.automat.punts.clear(); }
        fet ("Automatització: punt"_ca);
    });
}

void EditorAutomatitzacio::escriuValor (int li, int k)
{
    auto* a = linia (li);
    if (a == nullptr || k < 0 || k >= (int) a->punts.size()) return;
    std::string ii, pid;
    const int tipus = au::llegeixObjectiu (a->objectiu, ii, pid);
    const juce::String explic = tipus == 0 ? "Volum en dB (per exemple -6,5). Ara: "_ca : tipus == 1 ? "Panorama de -100 (esquerra) a 100 (dreta). Ara: "_ca
                                                                                         : "Valor del 0 al 100 % del recorregut del paràmetre. Ara: "_ca;
    const double v = a->punts[(size_t) k].valor;
    const juce::String actual = tipus == 0 ? juce::String (au::dbDeVolum (v), 1) : tipus == 1 ? juce::String (juce::roundToInt (au::panDe (v) * 100.0))
                                                                              : juce::String (v * 100.0, 1);
    auto llegeix = [tipus] (const juce::String& s, double& out)
    {
        const auto t = s.trim().replaceCharacter (',', '.').retainCharacters ("-+.0123456789");
        if (t.isEmpty()) return false;
        const double x = t.getDoubleValue();
        if (tipus == 0) out = au::volumDeDb (x);
        else if (tipus == 1) out = (juce::jlimit (-100.0, 100.0, x) / 100.0 + 1.0) * 0.5;
        else out = juce::jlimit (0.0, 100.0, x) / 100.0;
        return true;
    };
    const auto id = a->id;
    dialeg = std::make_unique<DialegText> ("Valor del punt"_ca, explic + textValor (*a, v), "D'acord"_ca,
        [llegeix] (const juce::String& s) { double x; return llegeix (s, x) ? juce::String() : "Escriu un número"_ca; },
        [this, li, k, id, llegeix] (const juce::String& s)
        {
            auto* b = linia (li);
            double x;
            if (b == nullptr || b->id != id || k >= (int) b->punts.size() || ! llegeix (s, x)) return;
            abans();
            b->punts[(size_t) k].valor = x;
            fet ("Automatització: valor"_ca);
        }, actual);
    if (auto* arrel = getTopLevelComponent()) DialegText::mostra (*arrel, dialeg);
}

void EditorAutomatitzacio::posaForma (int f)
{
    auto* a = liniaTriada();
    if (a == nullptr || estat.automat.punts.empty()) return;
    abans();
    for (int k : estat.automat.punts) if (k >= 0 && k < (int) a->punts.size()) { a->punts[(size_t) k].forma = f; a->punts[(size_t) k].tensio = 0.0; }
    fet ("Automatització: forma"_ca);
}

void EditorAutomatitzacio::quantitzaTriats()
{
    auto* a = liniaTriada();
    if (a == nullptr || estat.automat.punts.empty()) return;
    const auto& p = estat.projecte;
    const double ticksCompas = mibase::ticksPerNegra * 4.0 * juce::jmax (1, p.compasNum) / juce::jmax (1, p.compasDen);
    abans();
    au::quantitza (*a, estat.automat.punts, (p.graella > 0 ? (double) p.graella : (double) mibase::ticksPerNegra) / ticksCompas);
    estat.automat.punts.clear();
    fet ("Automatització: quantitza"_ca);
}

void EditorAutomatitzacio::copia()
{
    auto* a = liniaTriada();
    if (a == nullptr || estat.automat.punts.size() < 2) return;
    double de = 1e9, fins = -1e9;
    for (int k : estat.automat.punts) if (k >= 0 && k < (int) a->punts.size()) { de = std::min (de, a->punts[(size_t) k].compas); fins = std::max (fins, a->punts[(size_t) k].compas); }
    if (fins > de) portaRetalls() = au::copia (*a, de, fins);
    repaint();
}

void EditorAutomatitzacio::enganxaAlCapcal()
{
    auto* a = liniaTriada();
    if (a == nullptr || portaRetalls().empty()) return;
    abans();
    au::enganxa (*a, portaRetalls(), enganxa (ed::compasDeSegons (estat.projecte, estat.capcal)));
    estat.automat.punts.clear();
    fet ("Automatització: enganxa"_ca);
}

void EditorAutomatitzacio::duplicaTriats()
{
    auto* a = liniaTriada();
    if (a == nullptr || estat.automat.punts.size() < 2) return;
    double de = 1e9, fins = -1e9;
    for (int k : estat.automat.punts) if (k >= 0 && k < (int) a->punts.size()) { de = std::min (de, a->punts[(size_t) k].compas); fins = std::max (fins, a->punts[(size_t) k].compas); }
    if (fins <= de) return;
    abans();
    au::duplica (*a, de, fins);
    estat.automat.punts = au::dins (*a, fins, fins + (fins - de), 0.0, 1.0);
    fet ("Automatització: duplica"_ca);
}

void EditorAutomatitzacio::esborraTriats()
{
    auto* a = liniaTriada();
    if (a == nullptr || estat.automat.punts.empty()) return;
    abans();
    au::esborra (*a, estat.automat.punts);
    estat.automat.punts.clear();
    fet ("Automatització: esborra punts"_ca);
}

// ---------------------------------------------------------------------------
void EditorAutomatitzacio::tic()
{
    if (estat.transport != 0)
    {
        const auto z = zonaCorba (0);
        const float x = segAX (estat.capcal);
        if (x > (float) z.getRight() - 20.0f || x < (float) z.getX())
            estat.automat.iniciVista = estat.capcal - 20.0 / juce::jmax (1.0, estat.automat.pxPerSeg);
    }
    if (std::abs (estat.capcal - capcalPintat) > 1e-4) { capcalPintat = estat.capcal; repaint(); }
}

void EditorAutomatitzacio::paint (juce::Graphics& g)
{
    g.fillAll (tema::fons);
    vintage::panell (g, barra.toFloat());
    auto* t = pista();
    g.setColour (colAuto);
    g.setFont (dibuix::lletra (12.0f, true));
    g.drawText ("AUTOMATITZACIÓ"_ca, barra.withWidth (130).withTrimmedLeft (10), juce::Justification::centredLeft);
    g.setColour (tema::text);
    g.setFont (dibuix::lletra (12.0f));
    g.drawFittedText (t != nullptr ? u8 (t->nom) : "(cap pista)"_ca, juce::Rectangle<int> (barra.getX() + 132, barra.getY(), 110, barra.getHeight()), juce::Justification::centredLeft, 1, 0.8f);

    // Regla: compassos
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRect (regla);
    const double cIni = std::floor (xACompas ((float) (regla.getX() + ampleCap))), cFi = xACompas ((float) regla.getRight()) + 1.0;
    const double pxCompas = compasAX (cIni + 1.0) - compasAX (cIni);
    const int pas = pxCompas < 18 ? 8 : pxCompas < 36 ? 4 : pxCompas < 70 ? 2 : 1;
    g.setFont (dibuix::lletra (10.0f));
    for (double c = cIni; c <= cFi; c += 1.0)
    {
        const float x = compasAX (c);
        if (x < (float) (regla.getX() + ampleCap)) continue;
        if (((long long) c) % pas == 0)
        {
            g.setColour (tema::suau);
            g.drawText (juce::String (ed::numCompas ((int) c)), juce::Rectangle<float> (x + 3.0f, (float) regla.getY(), 40.0f, (float) regla.getHeight()), juce::Justification::centredLeft);
            g.setColour (tema::linia2);
            g.drawVerticalLine ((int) x, (float) regla.getY() + 4.0f, (float) regla.getBottom());
        }
    }
    g.setColour (tema::suau);
    g.setFont (dibuix::lletra (10.0f));
    g.drawText ("Regla: clica per moure el capçal"_ca, regla.withWidth (ampleCap).reduced (8, 0), juce::Justification::centredLeft);

    const int n = numLinies();
    if (n == 0)
    {
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (13.0f));
        g.drawFittedText ("Aquesta pista encara no té automatització. Prem «+ Línia» per triar què vols moure al llarg de la cançó: el volum, el panorama o qualsevol paràmetre dels seus efectes (Reverb, Dinàmica, VST3…) i del seu instrument VST3."_ca,
                          zonaLinies.reduced (40, 20), juce::Justification::centred, 4);
        return;
    }
    const double cCapcal = ed::compasDeSegons (estat.projecte, estat.capcal);
    for (int i = 0; i < n; ++i)
    {
        auto* a = linia (i);
        const bool triada = a->id == estat.automat.linia;
        auto cap = zonaCap (i);
        auto z = zonaCorba (i);
        // Capçalera
        g.setColour (triada ? tema::triat : tema::lateral);
        g.fillRect (cap);
        g.setColour (tema::linia);
        g.drawHorizontalLine (zonaLinia (i).getBottom() - 1, (float) zonaLinia (i).getX(), (float) zonaLinia (i).getRight());
        auto c = cap.reduced (8, 4);
        auto fila1 = c.removeFromTop (18);
        auto led = fila1.removeFromLeft (16).toFloat().reduced (2.0f);
        g.setColour (a->activa ? colAuto : tema::linia2);
        g.fillEllipse (led);
        auto creu = fila1.removeFromRight (16).toFloat();
        dibuix::pintaIcona (g, dibuix::Icona::Tanca, creu.reduced (3.0f), tema::suau);
        g.setColour (a->activa ? tema::text : tema::suau);
        g.setFont (dibuix::lletra (11.5f, triada));
        g.drawFittedText (u8 (a->nom), fila1.withTrimmedLeft (4), juce::Justification::centredLeft, 1, 0.75f);
        std::string ii, pid;
        const int tipus = au::llegeixObjectiu (a->objectiu, ii, pid);
        int ix = -1;
        const bool falta = (tipus == 2 || tipus == 3) && objecte (*a, ix) == nullptr;
        g.setFont (dibuix::mono (12.0f));
        if (falta)
        {
            g.setColour (tema::errorText);
            g.setFont (dibuix::lletra (10.5f));
            g.drawFittedText ("El plugin no hi és o està apagat"_ca, c.removeFromTop (16), juce::Justification::centredLeft, 1, 0.8f);
        }
        else if (! a->punts.empty())
        {
            g.setColour (estat.transport != 0 ? colAuto : tema::textMig);
            g.drawText (textValor (*a, au::valorA (a->punts, cCapcal)), c.removeFromTop (16), juce::Justification::centredLeft);
        }
        else
        {
            g.setColour (tema::suau);
            g.setFont (dibuix::lletra (10.5f));
            g.drawText ("Sense punts: doble clic per posar-ne"_ca, c.removeFromTop (16), juce::Justification::centredLeft);
        }
        // Zona de la corba
        g.setColour (juce::Colours::black.withAlpha (triada ? 0.45f : 0.3f));
        g.fillRect (z);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        for (double cc = cIni; cc <= cFi; cc += 1.0)
        {
            const float x = compasAX (cc);
            if (x >= (float) z.getX()) g.drawVerticalLine ((int) x, (float) z.getY(), (float) z.getBottom());
        }
        g.drawHorizontalLine (z.getCentreY(), (float) z.getX(), (float) z.getRight());
        if (! a->punts.empty())
        {
            const auto col = a->activa ? colAuto : tema::suau;
            juce::Path p;
            bool primer = true;
            for (int x = z.getX(); x <= z.getRight(); x += 2)
            {
                const double v = au::valorA (a->punts, xACompas ((float) x));
                const float y = valorAY (z, v);
                if (primer) { p.startNewSubPath ((float) x, y); primer = false; } else p.lineTo ((float) x, y);
            }
            juce::Path ple (p);
            ple.lineTo ((float) z.getRight(), (float) z.getBottom());
            ple.lineTo ((float) z.getX(), (float) z.getBottom());
            ple.closeSubPath();
            g.setColour (col.withAlpha (0.10f));
            g.fillPath (ple);
            g.setColour (col);
            g.strokePath (p, juce::PathStrokeType (triada ? 2.0f : 1.4f));
            for (size_t k = 0; k < a->punts.size(); ++k)
            {
                const float x = compasAX (a->punts[k].compas);
                if (x < (float) z.getX() - 6 || x > (float) z.getRight() + 6) continue;
                const float y = valorAY (z, a->punts[k].valor);
                const bool tri = triada && std::find (estat.automat.punts.begin(), estat.automat.punts.end(), (int) k) != estat.automat.punts.end();
                g.setColour (tri ? tema::accent : col);
                g.fillEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f);
                g.setColour (tema::fons);
                g.drawEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f, 1.0f);
            }
            // El valor al capçal
            const float xc = segAX (estat.capcal);
            if (xc >= (float) z.getX() && xc <= (float) z.getRight())
            {
                const float y = valorAY (z, au::valorA (a->punts, cCapcal));
                g.setColour (tema::text);
                g.drawEllipse (xc - 5.0f, y - 5.0f, 10.0f, 10.0f, 1.5f);
            }
        }
    }
    // Dibuix a mà o requadre en curs
    if (gest == Gest::Llapis && traç.size() > 1 && liniaGest >= 0)
    {
        const auto z = zonaCorba (liniaGest);
        juce::Path p;
        for (size_t k = 0; k < traç.size(); ++k)
        {
            const float x = compasAX (traç[k].compas), y = valorAY (z, traç[k].valor);
            if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (tema::accent);
        g.strokePath (p, juce::PathStrokeType (2.0f));
    }
    if (gest == Gest::Requadre)
    {
        const auto r = juce::Rectangle<float> (inici, ara);
        g.setColour (tema::accent.withAlpha (0.12f));
        g.fillRect (r);
        g.setColour (tema::accent);
        g.drawRect (r, 1.0f);
    }
    // Capçal
    const float xc = segAX (estat.capcal);
    if (xc >= (float) (zonaLinies.getX() + ampleCap))
    {
        g.setColour (tema::text.withAlpha (0.85f));
        g.drawVerticalLine ((int) xc, (float) regla.getY(), (float) zonaLinies.getBottom());
    }
}

void EditorAutomatitzacio::resized()
{
    auto r = getLocalBounds();
    barra = r.removeFromTop (34);
    regla = r.removeFromTop (20);
    zonaLinies = r;
    auto b = barra.reduced (6, 5);
    b.removeFromLeft (246);
    btTanca.setBounds (b.removeFromRight (26));
    b.removeFromRight (8);
    btEstudi.setBounds (b.removeFromRight (100));
    btMes.setBounds (b.removeFromRight (26));
    btMenys.setBounds (b.removeFromRight (26));
    b.removeFromRight (8);
    btLinia.setBounds (b.removeFromLeft (84));
    b.removeFromLeft (10);
    segEina.setBounds (b.removeFromLeft (130));
    b.removeFromLeft (8);
    segForma.setBounds (b.removeFromLeft (200));
    b.removeFromLeft (8);
    btGraella.setBounds (b.removeFromLeft (76));
    btQuant.setBounds (b.removeFromLeft (28));
    b.removeFromLeft (8);
    const bool cap = b.getWidth() >= 200;
    for (auto* x : { &btCopia, &btEnganxa, &btDuplica }) x->setVisible (cap);
    if (cap)
    {
        btCopia.setBounds (b.removeFromLeft (60));
        btEnganxa.setBounds (b.removeFromLeft (70));
        btDuplica.setBounds (b.removeFromLeft (66));
    }
}

// ---------------------------------------------------------------------------
void EditorAutomatitzacio::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    gest = Gest::Res;
    desfetFet = false;
    const auto pos = e.getPosition();
    inici = ara = e.position;
    if (regla.contains (pos) && pos.x >= regla.getX() + ampleCap)
    {
        gest = Gest::Regla;
        estat.mouCapcal (juce::jmax (0.0, xASeg (e.position.x)), true);
        return;
    }
    const int i = liniaA (pos);
    if (i < 0) return;
    auto* a = linia (i);
    if (zonaCap (i).contains (pos))
    {
        auto c = zonaCap (i).reduced (8, 4);
        auto fila1 = c.removeFromTop (18);
        const auto led = fila1.removeFromLeft (16), creu = fila1.removeFromRight (16);
        if (led.contains (pos))
        {
            abans();
            a->activa = ! a->activa;
            fet (a->activa ? "Automatització encesa"_ca : "Automatització apagada"_ca);
        }
        else if (creu.contains (pos))
        {
            const auto nom = u8 (a->nom);
            abans();
            auto* t = pista();
            t->autos.erase (t->autos.begin() + i);
            estat.automat.punts.clear();
            fet ("Automatització esborrada: "_ca + nom);
        }
        else triaLinia (i);
        return;
    }
    if (! zonaCorba (i).expanded (0, 4).contains (pos)) return;
    triaLinia (i);
    liniaGest = i;
    const int k = puntA (i, e.position);
    if (e.mods.isPopupMenu()) { if (k >= 0) menuPunt (i, k); return; }
    auto& sel = estat.automat.punts;
    if (estat.automat.eina == 1 && k < 0)
    {
        gest = Gest::Llapis;
        traç = { { enganxa (xACompas (e.position.x)), yAValor (zonaCorba (i), e.position.y), estat.automat.forma, 0.0 } };
        return;
    }
    if (k >= 0)
    {
        const bool ja = std::find (sel.begin(), sel.end(), k) != sel.end();
        if (e.mods.isCommandDown()) { if (ja) sel.erase (std::find (sel.begin(), sel.end(), k)); else sel.push_back (k); }
        else if (! ja) sel = { k };
        gest = Gest::Mou;
        puntGest = k;
        abansGest = a->punts;
        c0 = a->punts[(size_t) k].compas;
        v0 = a->punts[(size_t) k].valor;
        repaint();
        return;
    }
    if (e.mods.isAltDown() && a->punts.size() >= 2)
    {
        const double c = xACompas (e.position.x);
        for (size_t s = 0; s + 1 < a->punts.size(); ++s)
            if (a->punts[s].compas <= c && a->punts[s + 1].compas > c && a->punts[s].forma == 0)
            {
                gest = Gest::Tensio;
                puntGest = (int) s;
                tensio0 = a->punts[s].tensio;
                return;
            }
    }
    gest = Gest::Requadre;
    if (! e.mods.isShiftDown()) sel.clear();
    repaint();
}

void EditorAutomatitzacio::mouseDrag (const juce::MouseEvent& e)
{
    ara = e.position;
    if (gest == Gest::Regla) { estat.mouCapcal (juce::jmax (0.0, xASeg (e.position.x)), true); return; }
    auto* a = linia (liniaGest);
    if (a == nullptr) return;
    const auto z = zonaCorba (liniaGest);
    if (gest == Gest::Mou)
    {
        if (e.getDistanceFromDragStart() < 3 && ! desfetFet) return;
        if (! desfetFet)
        {
            // el desfer ha de recordar com era abans de moure
            auto ara2 = a->punts;
            a->punts = abansGest;
            abans();
            a->punts = ara2;
            desfetFet = true;
        }
        a->punts = abansGest;
        const double dc = e.mods.isShiftDown() ? 0.0 : enganxa (c0 + (xACompas (e.position.x) - xACompas (inici.x))) - c0;
        const double dv = (inici.y - e.position.y) / (double) juce::jmax (1, z.getHeight());
        au::mou (*a, estat.automat.punts, dc, dv);
        repaint();
    }
    else if (gest == Gest::Tensio)
    {
        if (! desfetFet) { abans(); desfetFet = true; }
        if (puntGest >= 0 && puntGest < (int) a->punts.size())
            a->punts[(size_t) puntGest].tensio = juce::jlimit (-1.0, 1.0, tensio0 + (inici.y - e.position.y) / 80.0);
        repaint();
    }
    else if (gest == Gest::Llapis)
    {
        const double c = xACompas (e.position.x);
        if (traç.empty() || std::abs (compasAX (c) - compasAX (traç.back().compas)) >= 2.0f)
            traç.push_back ({ c, yAValor (z, e.position.y), estat.automat.forma, 0.0 });
        repaint();
    }
    else if (gest == Gest::Requadre) repaint();
}

void EditorAutomatitzacio::mouseUp (const juce::MouseEvent& e)
{
    auto* a = linia (liniaGest);
    const auto g0 = gest;
    gest = Gest::Res;
    if (a == nullptr) { repaint(); return; }
    const auto z = zonaCorba (liniaGest);
    if ((g0 == Gest::Mou || g0 == Gest::Tensio) && desfetFet) fet ("Automatització: mou"_ca);
    else if (g0 == Gest::Requadre)
    {
        const auto r = juce::Rectangle<float> (inici, e.position);
        if (r.getWidth() > 2 || r.getHeight() > 2)
        {
            auto nous = au::dins (*a, xACompas (r.getX()), xACompas (r.getRight()), yAValor (z, r.getBottom()), yAValor (z, r.getY()));
            for (int k : nous)
                if (std::find (estat.automat.punts.begin(), estat.automat.punts.end(), k) == estat.automat.punts.end()) estat.automat.punts.push_back (k);
        }
    }
    else if (g0 == Gest::Llapis)
    {
        abans();
        if (traç.size() <= 1)
        {
            auto pt = traç.empty() ? mibase::PuntAuto {} : traç.front();
            estat.automat.punts = { au::posaPunt (*a, pt) };
        }
        else
        {
            std::stable_sort (traç.begin(), traç.end(), [] (const mibase::PuntAuto& x, const mibase::PuntAuto& y) { return x.compas < y.compas; });
            auto s = au::simplifica (traç, 0.006);
            for (auto& q : s) q.forma = estat.automat.forma;
            au::substitueix (*a, s.front().compas, s.back().compas, s);
            estat.automat.punts.clear();
        }
        traç.clear();
        fet ("Automatització: dibuix"_ca);
    }
    repaint();
}

void EditorAutomatitzacio::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int i = liniaA (e.getPosition());
    if (i < 0 || ! zonaCorba (i).expanded (0, 4).contains (e.getPosition())) return;
    auto* a = linia (i);
    const int k = puntA (i, e.position);
    abans();
    if (k >= 0) { au::esborra (*a, { k }); estat.automat.punts.clear(); fet ("Automatització: treu un punt"_ca); return; }
    estat.automat.punts = { au::posaPunt (*a, { enganxa (xACompas (e.position.x)), yAValor (zonaCorba (i), e.position.y), estat.automat.forma, 0.0 }) };
    fet ("Automatització: punt nou"_ca);
}

void EditorAutomatitzacio::mouseMove (const juce::MouseEvent& e)
{
    const int i = liniaA (e.getPosition());
    if (i >= 0 && zonaCorba (i).contains (e.getPosition()))
    {
        const int k = puntA (i, e.position);
        auto* a = linia (i);
        setMouseCursor (k >= 0 ? juce::MouseCursor::DraggingHandCursor : estat.automat.eina == 1 ? juce::MouseCursor::CrosshairCursor : juce::MouseCursor::NormalCursor);
        setTooltip (k >= 0 && a != nullptr ? textValor (*a, a->punts[(size_t) k].valor) + " · compàs "_ca
                                              + u8 (ed::textPosicio (estat.projecte, ed::segonsDeCompas (estat.projecte, a->punts[(size_t) k].compas)))
                                            : juce::String());
    }
    else setMouseCursor (juce::MouseCursor::NormalCursor);
}

void EditorAutomatitzacio::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float d = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
    if (e.mods.isCommandDown()) { zoom (d > 0 ? 1.25 : 0.8, e.position.x); return; }
    estat.automat.iniciVista = juce::jmax (-2.0, estat.automat.iniciVista - d * 200.0 / estat.automat.pxPerSeg);
    repaint();
}

bool EditorAutomatitzacio::keyPressed (const juce::KeyPress& k)
{
    const auto m = k.getModifiers();
    const int c = k.getKeyCode();
    auto lletra = [c] (char x) { return c == x || c == x + 32; };
    if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) { esborraTriats(); return true; }
    if (m.isCommandDown() && lletra ('C')) { copia(); return true; }
    if (m.isCommandDown() && lletra ('V')) { enganxaAlCapcal(); return true; }
    if (m.isCommandDown() && lletra ('D')) { duplicaTriats(); return true; }
    if (m.isCommandDown() && lletra ('A'))
    {
        if (auto* a = liniaTriada()) { estat.automat.punts.clear(); for (int i = 0; i < (int) a->punts.size(); ++i) estat.automat.punts.push_back (i); repaint(); }
        return true;
    }
    if (! m.isAnyModifierKeyDown())
    {
        if (lletra ('Q')) { quantitzaTriats(); return true; }
        if (c == '1' || c == '2') { estat.automat.eina = c - '1'; segEina.setTriat (estat.automat.eina); return true; }
        if (lletra ('A')) { if (onTanca) onTanca(); return true; }
    }
    return onTeclaNoUsada ? onTeclaNoUsada (k) : false;
}
