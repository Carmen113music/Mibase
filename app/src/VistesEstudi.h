#pragma once

#include "DialegText.h"
#include "OpcionsPista.h"

#include "Controls.h"
#include "EstatEstudi.h"

/** Base de totes les seccions de l'estudi: es tornen a dibuixar quan l'estat canvia. */
class SeccioEstudi : public juce::Component, protected juce::ChangeListener
{
public:
    explicit SeccioEstudi (EstatEstudi& e) : estat (e) { estat.addChangeListener (this); }
    ~SeccioEstudi() override { estat.removeChangeListener (this); }

    /** Roda del ratolí, igual a totes les seccions (un gest, un sentit):
        roda = amunt i avall · Maj + roda = de costat · Ctrl + roda = zoom del temps · Alt + roda = alçada de les pistes. */
    std::function<void (const juce::MouseEvent&, const juce::MouseWheelDetails&)> onRoda;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override { if (onRoda) onRoda (e, w); }

protected:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }

    /** Guarda el projecte tal com és ara, per poder-lo restaurar mentre s'arrossega. */
    void comencaCanvi() { copiaInici = estat.projecte; canviFet = false; }
    /** Torna al projecte de l'inici del gest (per aplicar-hi el canvi de nou, des de zero). */
    void restaura() { estat.projecte = copiaInici; canviFet = true; }
    /** Acaba el gest: si s'ha canviat res, queda al Desfés i es desa. */
    /** 0.41: es crida quan un canvi s'acaba, amb el projecte de com estava abans. */
    std::function<void (const mibase::Projecte& abans)> onAbansDeTancar;
    void acabaCanvi (const juce::String& evolucio = {})
    {
        if (canviFet)
        {
            if (onAbansDeTancar) onAbansDeTancar (copiaInici);   // 0.41
            estat.desfes.abans (copiaInici);
            estat.canviat (evolucio);
        }
        canviFet = false;
        estat.info.clear();
        estat.avisa();
    }

    EstatEstudi& estat;
    mibase::Projecte copiaInici;
    bool canviFet = false;
};

/** Dibuix compartit de l'estudi (faders i tecletes amb aspecte d'aparell antic). */
namespace dibuixEstudi
{
    /** Tecleta petita (M, S, gravar): quan està encesa, la cara agafa el color. */
    void teclaPetita (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text, bool encesa, juce::Colour color, bool rodona = false);
    /** Fader vertical amb escala de dB i, si «nivellL» no és negatiu, vúmetres a la dreta (amb la mateixa escala).
        Retorna el rectangle del carril (per saber on clica el ratolí). */
    juce::Rectangle<float> faderVertical (juce::Graphics& g, juce::Rectangle<float> r, double db, juce::Colour color, bool actiu,
                                          float nivellL = -1.0f, float nivellR = -1.0f);
    /** Dos vúmetres verticals (esquerra i dreta) amb l'escala del fader. */
    void vumetres (juce::Graphics& g, juce::Rectangle<float> r, float l, float d);

    /** Fader horitzontal. */
    void faderHoritzontal (juce::Graphics& g, juce::Rectangle<float> r, double db, juce::Colour color, bool actiu);
    /** Carril d'un fader vertical dins la zona r (el mateix càlcul que faderVertical). */
    juce::Rectangle<float> carrilVertical (juce::Rectangle<float> r);
}

/** Regla de compassos i temps. Clic: capçal. Arrossegar: marca el bucle (compassos sencers).
    Doble clic dins el bucle: l'encén o l'apaga. Botó dret: opcions del bucle. */
class Regla : public SeccioEstudi
{
public:
    using SeccioEstudi::SeccioEstudi;
    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    // 0.13: meitat de baix = capçal; meitat de dalt = bucle (selecció) i parts de la cançó.
    struct Lloc { int vora = 0; int part = -1; int marca = 0; };   // vora: -1 inici, 1 final (del bucle si part = -1); marca (1.18): 1 inici de la cançó, 2 final
    enum class Accio { Res, Capcal, Vora, Seleccio, Seleccionant, Marca };
    Lloc queHiHa (juce::Point<int> p) const;
    double compas (double t) const;
    std::pair<double, double> seleccio (double ta, double tb) const;
    double vora (double t) const;
    juce::String textRang (double de, double a) const;
    void menu (const juce::MouseEvent& e, Lloc q);
public:
    void demanaNom (const std::string& idPart);
private:

    Accio accio = Accio::Res;
    Lloc llocVora;
    int partClicada = -1;
    juce::Point<int> inici;
    std::unique_ptr<DialegText> dialeg;
};

/** Les pistes amb els clips: el lloc on s'edita, amb les eines de la caixa d'eines. */
class VistaPistes : public SeccioEstudi, public juce::FileDragAndDropTarget, public juce::DragAndDropTarget
{
public:
    explicit VistaPistes (EstatEstudi& e) : SeccioEstudi (e) { setWantsKeyboardFocus (true); }

    /** 0.27: un sample de la biblioteca deixat a la taula (fila -1: sota les pistes). */
    std::function<void (const juce::File&, int fila, double temps)> onSample;
    std::function<void (const std::string& presa, int fila, double temps)> onPresa;   // 0.85: del Pool
    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        return d.description.toString().startsWith ("mibase-sample:") || d.description.toString().startsWith ("mibase-presa:")   // 0.85: del Pool
            || (d.description.toString() == "mibase-acord" && estat.segell.actiu());   // 0.36: acords del Chord Pad a la pista d'Acords
    }
    void itemDragMove (const SourceDetails& d) override { const int f = estat.pistaA (d.localPosition.y); if (f != filaDeixar) { filaDeixar = f < estat.numPistes() ? f : -1; repaint(); } }
    void itemDragExit (const SourceDetails&) override { filaDeixar = -2; repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        if (d.description.toString() == "mibase-acord") { deixaAcord (d.localPosition); return; }   // 0.36
        filaDeixar = -2;
        int fila = estat.pistaA (d.localPosition.y);
        if (fila >= 0 && fila < estat.numPistes() && mibase::esBus (estat.projecte.pistes[(size_t) fila])) fila = -1;   // 0.34: a un bus no
        if (d.description.toString().startsWith ("mibase-presa:"))   // 0.85: un àudio del Pool
        {
            if (onPresa) onPresa (d.description.toString().fromFirstOccurrenceOf ("mibase-presa:", false, false).toStdString(),
                                  (fila >= 0 && fila < estat.numPistes()) ? fila : -1, juce::jmax (0.0, tEina (estat.t ((float) d.localPosition.x))));
            repaint();
            return;
        }
        const juce::File f (d.description.toString().fromFirstOccurrenceOf ("mibase-sample:", false, false));
        if (onSample) onSample (f, (fila >= 0 && fila < estat.numPistes()) ? fila : -1, juce::jmax (0.0, tEina (estat.t ((float) d.localPosition.x))));
        repaint();
    }

    /** 0.36: menú d'acords de la pista d'Acords: clip -1 = posar-ne un de nou (al compàs de «t»); si no, canviar-lo. */
    void menuAcords (int pista, int clip, double t);
    void deixaAcord (juce::Point<int> on);
    void obreFinestraAcords (int pista, int clip, double t, juce::Point<int> on, double fi = -1.0);   // 0.37 (0.42: «fi» = tros dibuixat)
    bool dibuixaAcord = false;            // 0.42: s'està dibuixant la finestra d'un acord nou amb el llapis
    int pistaAcord = -1;
    double acordT0 = 0.0, acordT1 = 0.0;
    std::function<void (const std::vector<int>&)> onEscoltaNotes;                    // 0.37: fer sonar un acord (per escoltar-lo)

    std::function<void (const juce::String& tipus, int canals)> onPistaNova;
    std::function<void (const juce::StringArray&, int pista, double on)> onImporta;    // pista -1: nova
    std::function<void (int pista, double on)> onDemanaImportar;
    /** Obre un clip MIDI al Piano Roll (doble clic, o en fer-ne un de nou amb el llapis). */
    std::function<void (int pista, int clip)> onObreMidi;
    std::function<void()> onVistaMoguda;   // 0.54: el requadre ha desplaçat la vista (cal moure les barres)
    /** 0.22: obre el clip d'àudio (pista, clip) al Vocal Editor. */
    std::function<void (int, int)> onObreVeu;
    std::function<void (int pista, int clip, bool nomesBiblioteca)> onEnviaSampler;   // 0.28
    /** 1.7: canviar el nom de l'àudio d'un clip al mateix lloc (id de la presa i zona del nom). */
    std::function<void (const std::string& idPresa, juce::Rectangle<int> zona)> onCanviaNomPresa;
    EstatEstudi::Rang rang0;   // 0.29
    std::function<bool (juce::Point<int> pantalla)> arrossegaFora;   // 0.29: si el ratolí és a sobre d'on es pot deixar un clip (el samplejador)
    /** La cola necessita fer un fitxer nou (preses diferents o forat entre mig). */
    std::function<void (int pista, int a, int b)> onConsolida;
    std::function<void (int pista)> onConsolidaComp;   // 0.81
    std::function<void (int pista)> onPrint;           // 0.82
    /** 0.87: un clip d'àudio deixat a la pista d'Acords: se'n treuen els acords. */
    std::function<void (int pista, int clip)> onAcordsDeClip;

    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void modifierKeysChanged (const juce::ModifierKeys& m) override   // 0.32: el cursor de Maj sense haver de moure el ratolí
    {
        if (! isMouseOver() || estat.eina != Eina::Fletxa || mode != Mode::Res) return;
        if (m.isShiftDown() && ! m.isCommandDown()) setMouseCursor (juce::MouseCursor::CrosshairCursor);
        else setMouseCursor (juce::MouseCursor::NormalCursor);
    }
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent&) override { sobre = {}; sobreElastic = {}; tTisores = -1.0; repaint(); }

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int y) override { filaDeixar = estat.pistaA (y); repaint(); }
    void fileDragMove (const juce::StringArray&, int, int y) override { filaDeixar = estat.pistaA (y); repaint(); }
    void fileDragExit (const juce::StringArray&) override { filaDeixar = -2; repaint(); }
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    /** Menú de pista nova. */
    static void menuPistaNova (juce::Component& on, std::function<void (const juce::String&, int)> tria);
    /** Cursor del ratolí de cada eina. */
    static juce::MouseCursor cursor (Eina e);

    /** Alçada mínima de les pistes (perquè sempre es vegin M, S i gravar). */
    static constexpr int alcadaMinima = 66;

protected:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { setMouseCursor (cursor (estat.eina)); repaint(); }

private:
    void pintaFilaCarpeta (juce::Graphics& g, int fila);
    enum class Mode { Res, Requadre, Decidint, MouTemps, MouPista, RetallInici, RetallFi, FadeIn, FadeOut, Guany, Bandera,
                      Llapis, Goma, Cola, CompTria, CompVora, ElasticVora, ElasticMarcador };
    // ---- 0.86: Elastic Audio (eina 7) ----
    /** zona: -1 / 1 vora esquerra / dreta (estirar), 2 un marcador, 3 el marcador del mig (encara no hi és), 4 dins el clip. */
    struct LlocElastic { int pista = -1, clip = -1, zona = 0, marcador = -1; };
    LlocElastic elasticA (juce::Point<int> p) const;
    void mouseDownElastic (const juce::MouseEvent& e);
    void dragElastic (double t);
    void pintaElastic (juce::Graphics& g, int pista, int indexClip, const mibase::Clip& c, juce::Rectangle<float> r);
    LlocElastic elastic0, sobreElastic;
    double elasticT0 = 0.0;   // on era la vora o el marcador en començar
    bool elasticMogut = false;
    int filaAcordsDeixar = -1;   // 0.87: s'arrossega un clip d'àudio a sobre de la pista d'Acords
    // ---- 0.81: subpistes de preses i comping ----
    void pintaTakes (juce::Graphics& g, int pista);
    bool mouseDownLane (const juce::MouseEvent& e);
    void menuLane (int pista, int take, double t);
    /** La zona del comp (de la presa) on cau «x», si n'és a prop de la vora: -1 esquerra, 1 dreta, 0 dins, 2 fora. */
    int voraZona (int pista, int take, float x, std::pair<double, double>& zona) const;
    int compPista = -1, compTakeIdx = -1;
    double compA = 0.0, compB = 0.0;
    std::pair<double, double> compZona0 { 0.0, 0.0 };
    bool compVoraEsq = false;
    enum class Maneta { Cap, RetallInici, RetallFi, FadeIn, FadeOut, Guany, Bandera };

    struct Lloc { int pista = -1, clip = -1; Maneta maneta = Maneta::Cap; bool pistaNova = false; };

    Lloc llocA (juce::Point<int> p) const;
    juce::Rectangle<float> rectClip (int pista, const mibase::Clip& c) const;
    float yGuany (const mibase::Clip& c, juce::Rectangle<float> r) const;
    void pintaClip (juce::Graphics& g, int pista, int indexClip, const mibase::Clip& c);
    void pintaOna (juce::Graphics& g, const mibase::Clip& c, const mibase::Presa& s, OnaPresa& ona, juce::Rectangle<float> r, juce::Colour color);
    void pintaGraella (juce::Graphics& g);
    void menuClip (int pista, int clip, double t);
    void mouseDownFletxa (const juce::MouseEvent& e);
    void actualitzaRequadre (juce::Point<int> fins, bool afegeix);
    double tEina (double t) const;   // temps enganxat a la graella per a les eines

    Mode mode = Mode::Res;
    Lloc lloc0, sobre;
    juce::Point<int> inici;
    double t0 = 0.0, punt0 = 0.0, bandera0 = 0.0, guany0 = 0.0, tLlapis = 0.0, tTisores = -1.0;
    bool llapisPerCtrl = false;   // 0.72: Ctrl + arrossegar amb la fletxa fa un clip buit (preparat per gravar)
    int filaDeixar = -2;
    std::unique_ptr<DialegText> dialegAcord;   // 0.36
    std::vector<mibase::edicio::RefClip> triatsOriginals;
    bool nomesSeleccio = false;   // 0.32: Maj premut en clicar: el ratolí només selecciona (no mou ni retalla)
    juce::Point<int> puntCola, puntRequadre;
    double ancoraT = 0.0; int ancoraY = 0;     // 0.54: on comença el requadre (temps i alçada de contingut)
    void comencaRequadre();
    void desplacaVoraRequadre (juce::Point<int> p);
};

/** Noms de les pistes, amb M (silenci), S (solo) i el botó de gravar. */
class Capcaleres : public SeccioEstudi
{
public:
    /** 1.3: on és el nom d'una pista (per editar-lo al mateix lloc). */
    juce::Rectangle<int> rectNom (int fila) const { return { 10, estat.yPista (fila) + 4, juce::jmax (80, getWidth() - 40), 24 }; }
    using SeccioEstudi::SeccioEstudi;
    std::function<void (int pista)> onCanviaNom;
    std::function<void (const juce::String& tipus, int canals)> onPistaNova;
    /** 0.20: el botonet de la pista d'instrument (obre el sinte o el VST) o de bateria (obre els kits). */
    std::function<void (int pista)> onObreInstrument;
    std::function<void (int pista)> onAutomatitzacio;   // 0.56
    juce::Rectangle<int> rectEntradaCap (int fila) const;   // 0.34
    juce::Rectangle<int> rectSortidaCap (int fila) const;   // 0.35
    juce::Rectangle<int> zonaES (int fila) const;
    juce::Rectangle<int> rectSegueix (int fila) const;   // 0.41
    juce::Rectangle<int> rectAltraTonalitat (int fila) const;   // 0.65
    std::function<juce::Array<juce::MidiDeviceInfo>()> teclats;   // 0.40: per al menú E/S de les pistes d'instrument
    bool esEstreta (int fila) const;
    juce::Rectangle<int> rectES (int fila) const;   // 0.37
    juce::Rectangle<int> rectObreInstrument (int fila) const;
    juce::Rectangle<int> rectPreses (int fila) const;   // 0.81: plega / desplega les preses
    std::function<void (int pista)> onConsolidaComp;      // 0.81
    std::function<void (int pista)> onPrint;              // 0.82

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    enum class Tecla { Cap, Mut, Solo, Grava };
    Tecla teclaA (int pista, juce::Point<int> p) const;
    juce::Rectangle<int> rectTecla (int pista, int quina) const;
    void menu (int pista);

    int arrossegant = -1, desti = -1;
    juce::Point<int> inici;
};

/** 0.37: amb la pista d'Acords triada, la columna esquerra mostra les pistes MIDI amb un botó cadascuna:
    encès = la pista segueix la pista d'Acords (els seus clips de patró llegiran els acords). */
class PanellSeguidors : public juce::Component
{
public:
    explicit PanellSeguidors (EstatEstudi& e) : estat (e) {}

    void refresca()
    {
        std::vector<std::string> ids;
        for (const auto& t : estat.projecte.pistes) if (t.tipus == "midi") ids.push_back (t.id);
        if (ids != idsBotons)
        {
            idsBotons = ids;
            botons.clear();
            for (const auto& id : ids)
            {
                auto* b = botons.add (new Boto ({}, std::nullopt, Boto::Estil::Tecla));
                b->setLlum (juce::Colour (0xffa98bff));
                b->setMidaText (12.5f);
                b->setClickingTogglesState (false);
                b->onClick = [this, id] { commuta (id); };
                addAndMakeVisible (b);
            }
            resized();
        }
        for (int i = 0; i < botons.size(); ++i)
            if (const int k = mibase::edicio::pistaPerId (estat.projecte, idsBotons[(size_t) i]); k >= 0)
            {
                const auto& t = estat.projecte.pistes[(size_t) k];
                botons[i]->setButtonText (juce::String (k + 1) + "  " + juce::String::fromUTF8 (t.nom.c_str()));
                botons[i]->setToggleState (t.segueixAcords, juce::dontSendNotification);
                botons[i]->setTooltip (t.segueixAcords ? "Segueix la pista d'Acords. Clica per deixar-la anar sola."_ca
                                                       : "No segueix la pista d'Acords. Clica perquè la segueixi."_ca);
            }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xffa98bff));
        g.setFont (dibuix::lletra (11.5f, true));
        g.drawFittedText ("SEGUEIXEN ELS ACORDS"_ca, 4, 0, getWidth() - 8, 30, juce::Justification::centredLeft, 2, 1.0f);
        g.setColour (tema::suau);
        g.setFont (dibuix::lletra (11.0f));
        const int y = 34 + botons.size() * 34 + 6;
        g.drawFittedText (botons.isEmpty() ? "Encara no hi ha cap pista MIDI."_ca
                                           : "Encès: els clips de patró de la pista llegeixen els acords d'aquí. Llapis o doble clic a la pista d'Acords: posar-hi acords."_ca,
                          4, botons.isEmpty() ? 34 : y, getWidth() - 8, 60, juce::Justification::topLeft, 5, 0.85f);
    }

    void resized() override
    {
        for (int i = 0; i < botons.size(); ++i) botons[i]->setBounds (2, 34 + i * 34, getWidth() - 4, 30);
    }

private:
    void commuta (const std::string& id)
    {
        const int k = mibase::edicio::pistaPerId (estat.projecte, id);
        if (k < 0) return;
        estat.desfes.abans (estat.projecte);
        auto& t = estat.projecte.pistes[(size_t) k];
        t.segueixAcords = ! t.segueixAcords;
        estat.canviat ("«"_ca + juce::String::fromUTF8 (t.nom.c_str()) + (t.segueixAcords ? "» segueix la pista d'Acords"_ca : "» ja no segueix la pista d'Acords"_ca));
        refresca();
    }

    EstatEstudi& estat;
    std::vector<std::string> idsBotons;
    juce::OwnedArray<Boto> botons;
};

/** Columna de l'esquerra: pestanyes Taula de treball / Taula de mescles i el fader de la pista triada. */
class ColumnaEsquerra : public SeccioEstudi
{
public:
    explicit ColumnaEsquerra (EstatEstudi& e);
    /** Panell per als editors (el d'harmonia): quan hi ha un editor obert, ocupa el lloc del fader de la pista. */
    void setPanellEditor (juce::Component* c) { panellEditor = c; if (c != nullptr) addChildComponent (c); changeListenerCallback (nullptr); }
    bool ambEditor() const
    {
        if (panellEditor == nullptr || estat.foraTreball() || acordsTriada()) return false;
        return estat.pianoRoll && estat.focusEditor;
    }
    /** 0.37: amb la pista d'Acords triada, la llista de pistes que la segueixen (en lloc del fader i els inserts). */
    bool ambAcords() const { return ! estat.foraTreball() && acordsTriada(); }
    PanellSeguidors seguidors { estat };
    bool acordsTriada() const { auto* t = estat.pista (estat.pistaTriada); return t != nullptr && mibase::esAcords (*t); }
    OpcionsPista opcions { estat };
    std::function<void()> onMescles;   // 1.16: la taula de mescles va en una finestra a part
    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

protected:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

private:
    Boto btTreball { "Taula de treball", dibuix::Icona::Pistes, Boto::Estil::Tecla };
    Boto btMescles { "Taula de mescles", dibuix::Icona::Mescla, Boto::Estil::Tecla };
    Boto btEstructura { "Taula d'estructura", dibuix::Icona::Notes, Boto::Estil::Tecla };
    juce::Component* panellEditor = nullptr;
    juce::Rectangle<int> zonaNom, zonaPan, zonaFader, zonaLcd, zonaTecles;
    int altOpcions = 0;
    juce::Rectangle<int> rectPom() const;
    enum class Arrossega { Res, Fader, Pan };
    Arrossega movent = Arrossega::Res;
    float valor0 = 0.0f, ultimaY = 0.0f;   // en arrossegar: valor actual i on era el ratolí (amb Maj, deu vegades més fi)
};

/** Taula de mescles: una tira per pista (inserts i enviaments, que arribaran més endavant, panorama, M, S, gravar,
    fader i vúmetre) i, fixa a la dreta, la sortida. */
class TaulaMescles : public SeccioEstudi
{
public:
    using SeccioEstudi::SeccioEstudi;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override;

    static constexpr int ampleTira = 100;
    static constexpr int numInserts = 4, numEnvia = 4;
    static constexpr int ampleNou = 74;   // 0.33: la columna dels botons de bus nou
    GestorPlugins* plugins = nullptr;   // 0.20: per als inserts
    std::function<void (int pista)> onCanviaNom;
    /** 1.7: on és el nom d'una tira (per editar-lo al mateix lloc). Buit si no es veu. */
    juce::Rectangle<int> rectNom (int pista) const { const auto r = rectTira (pista); return r.isEmpty() ? r : geo (r, false).nom; }
    /** 0.33: demana el nom i crea un bus (de grup o retorn). Si «pista» >= 0, aquella pista hi queda connectada:
        amb la sortida («comSortida») o amb un enviament nou. */
    std::function<void (bool fx, int pista, bool comSortida)> onBusNou;

private:
    struct Geo
    {
        juce::Rectangle<int> nom, etInserts, etEnvia, entrada, sortida, pan, fader, lcd, numero;
        std::array<juce::Rectangle<int>, numInserts> inserts;
        std::array<juce::Rectangle<int>, numEnvia> envia;
        std::array<juce::Rectangle<int>, 3> tecles;
    };
    enum class Zona { Res, Tecla, Pan, Fader, Envia, Insert, Entrada, Sortida, Nom, BusNou };

    Geo geo (juce::Rectangle<int> tira, bool master) const;
    void pintaTira (juce::Graphics& g, juce::Rectangle<int> r, int i);   // i < 0: la sortida
    bool troba (juce::Point<int> p, int& quina, Zona& zona, int& tecla) const;
    /** 0.33: ordre de les tires: primer les pistes, després els busos de grup i al final els retorns. */
    std::vector<int> ordreTires() const;
    juce::Rectangle<int> rectLloc (int lloc) const { return { lloc * ampleTira - despl, 0, ampleTira, getHeight() }; }
    juce::Rectangle<int> rectTira (int i) const;      // i = índex de la pista
    juce::Rectangle<int> rectNou() const { return rectLloc (estat.numPistes()).withWidth (ampleNou); }
    juce::Rectangle<int> rectMaster() const { return { getWidth() - ampleTira - 4, 0, ampleTira, getHeight() }; }
    void pintaNou (juce::Graphics& g);
    void mostraTira (int i);
    void menuEnviament (int pista, int k, bool nou);
    void menuSortida (int pista);
    void menuEntrada (int pista);   // 0.34
    void menuBus (int pista);
    void canvia (const juce::String& evolucio, const std::function<void (mibase::Projecte&)>& f);

    int despl = 0, tira = -1, enviaK = -1;
    Zona arrossega = Zona::Res;
    float valor0 = 0.0f, ultimaY = 0.0f;
};

/** Panell de la dreta: el clip triat i com funcionen les eines. */
class PanellPista : public SeccioEstudi
{
public:
    explicit PanellPista (EstatEstudi& e);
    std::function<void()> onDuplica, onTreu;
    std::function<void (juce::Component&)> onElastic;   // 0.86: el menú d'Elastic Audio del clip triat
    std::function<void()> onAcords;                       // 0.87: els acords del clip triat a la pista d'Acords

    void paint (juce::Graphics& g) override;
    void resized() override;

protected:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

private:
    Boto btDuplica { "Duplica", std::nullopt, Boto::Estil::Secundari };
    Boto btTreu { "Treu", std::nullopt, Boto::Estil::Secundari };
    Boto btElastic { "Elastic Audio", dibuix::Icona::Elastic, Boto::Estil::Secundari };   // 0.86
    Boto btAcords { "Treu els acords", std::nullopt, Boto::Estil::Secundari };            // 0.87
    // ---- 0.87: la pista triada i les seves notes ----
    juce::TextEditor notes;
    std::string idNotes;     // de quina pista són les notes que hi ha escrites
    void desaNotes();
    int yPista = 0;          // on comença la secció de la pista (es calcula a resized)
    // ---- 0.92: el teclat MIDI de la pista d'instrument (Visual / Assistit / Bloquejat) ----
    Segmentat modesTeclat { { "Visual", "Assistit", "Bloquejat" } };
    bool ambTeclat() const;
    int fiPista = 0;         // on acaba la targeta de la pista
    Boto btEstructura { "Estructura: lineal", std::nullopt, Boto::Estil::Secundari };   // 0.15: quina estructura sona
};
