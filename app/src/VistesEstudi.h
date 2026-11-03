#pragma once

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
    /** Fader vertical amb escala de dB. Retorna el rectangle del carril (per saber on clica el ratolí). */
    juce::Rectangle<float> faderVertical (juce::Graphics& g, juce::Rectangle<float> r, double db, juce::Colour color, bool actiu);
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
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    int compas0 = 0;
    bool marcant = false;
};

/** Les pistes amb els clips: el lloc on s'edita, amb les eines de la caixa d'eines. */
class VistaPistes : public SeccioEstudi, public juce::FileDragAndDropTarget
{
public:
    explicit VistaPistes (EstatEstudi& e) : SeccioEstudi (e) { setWantsKeyboardFocus (true); }

    std::function<void (const juce::String& tipus, int canals)> onPistaNova;
    std::function<void (const juce::StringArray&, int pista, double on)> onImporta;    // pista -1: nova
    std::function<void (int pista, double on)> onDemanaImportar;
    /** La cola necessita fer un fitxer nou (preses diferents o forat entre mig). */
    std::function<void (int pista, int a, int b)> onConsolida;

    void paint (juce::Graphics& g) override;
    void mouseMove (const juce::MouseEvent& e) override;
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
    enum class Mode { Res, Capcal, Decidint, MouTemps, MouPista, RetallInici, RetallFi, FadeIn, FadeOut, Guany, Bandera,
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
    double tEina (double t) const;   // temps enganxat a la graella per a les eines

    Mode mode = Mode::Res;
    Lloc lloc0, sobre;
    juce::Point<int> inici;
    double t0 = 0.0, punt0 = 0.0, bandera0 = 0.0, guany0 = 0.0, tLlapis = 0.0, tTisores = -1.0;
    int filaDeixar = -2;
    std::vector<int> triatsOriginals;
    juce::Point<int> puntCola;
};

/** Noms de les pistes, amb M (silenci), S (solo) i el botó de gravar. */
class Capcaleres : public SeccioEstudi
{
public:
    using SeccioEstudi::SeccioEstudi;
    std::function<void (int pista)> onCanviaNom;
    std::function<void (const juce::String& tipus, int canals)> onPistaNova;

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

/** Un fader per pista (horitzontal). */
class Faders : public SeccioEstudi
{
public:
    using SeccioEstudi::SeccioEstudi;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

private:
    juce::Rectangle<float> rectFader (int pista) const;
    int pistaFader = -1;
    float pos0 = 0.0f;
};

/** Columna de l'esquerra: pestanyes Taula de treball / Taula de mescles i el fader de la pista triada. */
class ColumnaEsquerra : public SeccioEstudi
{
public:
    explicit ColumnaEsquerra (EstatEstudi& e);
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
    juce::Rectangle<int> zonaFader, zonaTecles;
    float pos0 = 0.0f;
    bool movent = false;
};

/** Taula de mescles: una tira per pista, amb fader vertical, M, S i gravar. */
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

    static constexpr int ampleTira = 96;

private:
    juce::Rectangle<int> rectTira (int i) const { return { i * ampleTira - despl, 0, ampleTira, getHeight() }; }
    juce::Rectangle<int> rectTecla (int i, int quina) const;
    juce::Rectangle<float> zonaFader (int i) const;
    int tiraA (int x) const { return (x + despl) / ampleTira; }
    int despl = 0, tiraFader = -1;
    float pos0 = 0.0f;
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
};
