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
    void acabaCanvi (const juce::String& evolucio = {})
    {
        if (canviFet)
        {
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
    struct Lloc { int vora = 0; int part = -1; };   // vora: -1 inici, 1 final (del bucle si part = -1)
    enum class Accio { Res, Capcal, Vora, Seleccio, Seleccionant };
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
    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("mibase-sample:"); }
    void itemDragMove (const SourceDetails& d) override { const int f = estat.pistaA (d.localPosition.y); if (f != filaDeixar) { filaDeixar = f < estat.numPistes() ? f : -1; repaint(); } }
    void itemDragExit (const SourceDetails&) override { filaDeixar = -2; repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        filaDeixar = -2;
        const int fila = estat.pistaA (d.localPosition.y);
        const juce::File f (d.description.toString().fromFirstOccurrenceOf ("mibase-sample:", false, false));
        if (onSample) onSample (f, (fila >= 0 && fila < estat.numPistes()) ? fila : -1, juce::jmax (0.0, tEina (estat.t ((float) d.localPosition.x))));
        repaint();
    }

    std::function<void (const juce::String& tipus, int canals)> onPistaNova;
    std::function<void (const juce::StringArray&, int pista, double on)> onImporta;    // pista -1: nova
    std::function<void (int pista, double on)> onDemanaImportar;
    /** Obre un clip MIDI al Piano Roll (doble clic, o en fer-ne un de nou amb el llapis). */
    std::function<void (int pista, int clip)> onObreMidi;
    /** 0.22: obre el clip d'àudio (pista, clip) al Vocal Editor. */
    std::function<void (int, int)> onObreVeu;
    std::function<void (int pista, int clip, bool nomesBiblioteca)> onEnviaSampler;   // 0.28
    EstatEstudi::Rang rang0;   // 0.29
    std::function<bool (juce::Point<int> pantalla)> arrossegaFora;   // 0.29: si el ratolí és a sobre d'on es pot deixar un clip (el samplejador)
    /** La cola necessita fer un fitxer nou (preses diferents o forat entre mig). */
    std::function<void (int pista, int a, int b)> onConsolida;

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
    void mouseExit (const juce::MouseEvent&) override { sobre = {}; tTisores = -1.0; repaint(); }

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
                      Llapis, Goma, Cola };
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
    int filaDeixar = -2;
    std::vector<mibase::edicio::RefClip> triatsOriginals;
    bool nomesSeleccio = false;   // 0.32: Maj premut en clicar: el ratolí només selecciona (no mou ni retalla)
    juce::Point<int> puntCola, puntRequadre;
};

/** Noms de les pistes, amb M (silenci), S (solo) i el botó de gravar. */
class Capcaleres : public SeccioEstudi
{
public:
    using SeccioEstudi::SeccioEstudi;
    std::function<void (int pista)> onCanviaNom;
    std::function<void (const juce::String& tipus, int canals)> onPistaNova;
    /** 0.20: el botonet de la pista d'instrument (obre el sinte o el VST) o de bateria (obre els kits). */
    std::function<void (int pista)> onObreInstrument;
    juce::Rectangle<int> rectObreInstrument (int fila) const;

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

/** Columna de l'esquerra: pestanyes Taula de treball / Taula de mescles i el fader de la pista triada. */
class ColumnaEsquerra : public SeccioEstudi
{
public:
    explicit ColumnaEsquerra (EstatEstudi& e);
    /** Panell per als editors (el d'harmonia): quan hi ha un editor obert, ocupa el lloc del fader de la pista. */
    void setPanellEditor (juce::Component* c) { panellEditor = c; if (c != nullptr) addChildComponent (c); changeListenerCallback (nullptr); }
    bool ambEditor() const { return panellEditor != nullptr && estat.pianoRoll && ! estat.foraTreball() && estat.focusEditor; }
    OpcionsPista opcions { estat };
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
    static constexpr int numInserts = 4, numEnvia = 2;
    GestorPlugins* plugins = nullptr;   // 0.20: per als inserts

private:
    struct Geo
    {
        juce::Rectangle<int> nom, etInserts, etEnvia, pan, fader, lcd, numero;
        std::array<juce::Rectangle<int>, numInserts> inserts;
        std::array<juce::Rectangle<int>, numEnvia> envia;
        std::array<juce::Rectangle<int>, 3> tecles;
    };
    enum class Zona { Res, Tecla, Pan, Fader, Ranura, Insert };

    Geo geo (juce::Rectangle<int> tira, bool master) const;
    void pintaTira (juce::Graphics& g, juce::Rectangle<int> r, int i);   // i < 0: la sortida
    bool troba (juce::Point<int> p, int& quina, Zona& zona, int& tecla) const;
    juce::Rectangle<int> rectTira (int i) const { return { i * ampleTira - despl, 0, ampleTira, getHeight() }; }
    juce::Rectangle<int> rectMaster() const { return { getWidth() - ampleTira - 4, 0, ampleTira, getHeight() }; }
    int tiraA (int x) const { return (x + despl) / ampleTira; }

    int despl = 0, tira = -1;
    Zona arrossega = Zona::Res;
    float valor0 = 0.0f, ultimaY = 0.0f;
};

/** Panell de la dreta: el clip triat i com funcionen les eines. */
class PanellPista : public SeccioEstudi
{
public:
    explicit PanellPista (EstatEstudi& e);
    std::function<void()> onDuplica, onTreu;

    void paint (juce::Graphics& g) override;
    void resized() override;

protected:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

private:
    Boto btDuplica { "Duplica", std::nullopt, Boto::Estil::Secundari };
    Boto btTreu { "Treu", std::nullopt, Boto::Estil::Secundari };
    Boto btEstructura { "Estructura: lineal", std::nullopt, Boto::Estil::Secundari };   // 0.15: quina estructura sona
};
