#pragma once

#include "Controls.h"
#include "ServeiVeu.h"
#include "VistesEstudi.h"

/** 0.22: Vocal Editor (a baix, com el Piano Roll). Mostra les notes cantades d'una presa com a «blobs» sobre un
    teclat vertical, amb la corba d'afinació; les eines canvien l'afinació, el vibrato, la deriva, els formants i el
    volum de cada nota. El so el refà el ServeiVeu (fora del fil d'àudio). Les dades i l'inspector són al PanellVeu. */
class EditorVeu : public SeccioEstudi
{
public:
    enum Eina { Fletxa = 0, Vibrato, Deriva, Formant, Volum, numEines };
    static juce::String nomEina (int e);

    EditorVeu (EstatEstudi& e, ServeiVeu& s);

    std::function<void()> onTanca, onCommutaAB;
    /** S'obre una altra presa: la vista es tornarà a encaixar. */
    void novaPresa() { vistaFeta = false; corbesValides = false; }
    /** Encaixa la vista a les notes del clip. */
    void ajustaVista();
    /** Al ritme del transport (capçal i progrés de l'anàlisi). */
    void tic();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    bool keyPressed (const juce::KeyPress& k) override;

    // Comú amb el panell de la dreta.
    /** Les corbes de la presa (es recalculen quan canvien les notes o l'anàlisi). */
    const mibase::veu::Corbes* corbes();
    static juce::String nomNota (double midi);   // «E4»
    static juce::String textCents (double c);     // «+12 cents»

protected:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

private:
    ServeiVeu& servei;
    Boto btEines[numEines] { { "Afinació"_ca, std::nullopt, Boto::Estil::Tecla }, { "Vibrato", std::nullopt, Boto::Estil::Tecla },
                             { "Deriva", std::nullopt, Boto::Estil::Tecla }, { "Formant", std::nullopt, Boto::Estil::Tecla },
                             { "Volum", std::nullopt, Boto::Estil::Tecla } };
    Boto btOna { "Ona", std::nullopt, Boto::Estil::Tecla };
    Boto btEscala { "Escala", std::nullopt, Boto::Estil::Tecla };
    Boto btAB { "Corregit", std::nullopt, Boto::Estil::Tecla };
    Boto btAjusta { "Encaixa", std::nullopt, Boto::Estil::Secundari };
    Boto btTanca { {}, dibuix::Icona::Amaga, Boto::Estil::Subtil };

    static constexpr int altBarra = 34, altRegla = 20, ampleTeclat = 56;
    double t0 = 0.0, pxs = 100.0;           // vista horitzontal (temps del projecte)
    double midiDalt = 76.0;                 // nota a la vora de dalt
    float altSemi = 14.0f;                  // píxels per semitò
    bool vistaFeta = false;
    double ultimCapcal = -1.0;

    // Corbes en memòria.
    std::shared_ptr<const mibase::veu::Analisi> analisiCorbes;
    mibase::VeuPresa veuCorbes;
    mibase::veu::Corbes cacheCorbes;
    bool corbesValides = false;
    float rmsMax = 1e-6f;

    // Gest en curs.
    enum class Gest { Res, Notes, Requadre, Capcal };
    Gest gest = Gest::Res;
    juce::Point<int> inici, ratoli;
    std::vector<mibase::NotaVeu> notesInici;
    int notaSota = -1;

    juce::Rectangle<int> zonaBarra() const { return getLocalBounds().removeFromTop (altBarra); }
    juce::Rectangle<int> zonaRegla() const { return getLocalBounds().withTrimmedTop (altBarra).removeFromTop (altRegla).withTrimmedLeft (ampleTeclat); }
    juce::Rectangle<int> zonaTeclat() const { return getLocalBounds().withTrimmedTop (altBarra + altRegla).removeFromLeft (ampleTeclat); }
    juce::Rectangle<int> zonaNotes() const { return getLocalBounds().withTrimmedTop (altBarra + altRegla).withTrimmedLeft (ampleTeclat); }

    float xDe (double t) const { return (float) zonaNotes().getX() + (float) ((t - t0) * pxs); }
    double tDe (float x) const { return t0 + (x - (float) zonaNotes().getX()) / pxs; }
    float yDe (double midi) const { return (float) zonaNotes().getY() + (float) ((midiDalt - midi) * altSemi); }
    double midiDe (float y) const { return midiDalt - (y - (float) zonaNotes().getY()) / altSemi; }

    mibase::Presa* presa() { return estat.presaVeu(); }
    /** El clip que es mira (el que s'ha obert, o un altre de la mateixa presa a la pista). */
    const mibase::Clip* clip();
    juce::Rectangle<float> rectNota (const mibase::Clip& c, const mibase::NotaVeu& n) const;
    int notaA (juce::Point<int> p);
    bool triada (int k) const;
    void triaSola (int k);
    void commuta (int k);
    void aplicaArrossegament (const juce::MouseEvent& e);
    void mouTriades (double semitons, bool lliure, const juce::String& evolucio);
    void restableix (int k);

    void pintaGraella (juce::Graphics& g, const mibase::Clip& c);
    void pintaOna (juce::Graphics& g, const mibase::Clip& c);
    void pintaNotes (juce::Graphics& g, const mibase::Clip& c);
    void pintaCorba (juce::Graphics& g, const mibase::Clip& c);
    void pintaRegla (juce::Graphics& g);
    void pintaTeclat (juce::Graphics& g);
    void actualitzaBotons();
};

/** 0.22: el panell de la dreta amb el Vocal Editor obert: estat, inspector de les notes triades (valors exactes
    que es poden escriure) i correcció automàtica. */
class PanellVeu : public SeccioEstudi
{
public:
    PanellVeu (EstatEstudi& e, ServeiVeu& s, EditorVeu& ed);
    std::function<void()> onCommutaAB;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void tic() { repaint(); }

protected:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

private:
    /** Un valor numèric: arrossega amunt/avall per canviar-lo, doble clic per escriure'l. */
    class Camp : public juce::Component
    {
    public:
        Camp (const juce::String& etiqueta, double minim, double maxim, double pas, int decimals, const juce::String& unitat);
        std::function<void()> onComenca, onAcaba;
        std::function<void (double)> onCanvi;
        void setValor (double v, bool igualATots = true) { valor = v; igual = igualATots; repaint(); }
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void resized() override;
    private:
        juce::String text, unitat;
        double mn, mx, pas, valor = 0.0, valor0 = 0.0;
        int dec;
        bool igual = true, arrossega = false;
        std::unique_ptr<juce::TextEditor> editor;
    };

    ServeiVeu& servei;
    EditorVeu& editor;
    Camp cDesplac { "Desplaçament"_ca, -24.0, 24.0, 0.01, 2, "st" };
    Camp cCentre { "Pitch Center", 0.0, 100.0, 1.0, 0, "%" };
    Camp cModulacio { "Pitch Modulation", 0.0, 200.0, 1.0, 0, "%" };
    Camp cDeriva { "Pitch Drift", 0.0, 100.0, 1.0, 0, "%" };
    Camp cFormant { "Formant", -12.0, 12.0, 0.1, 1, "st" };
    Camp cVolum { "Volum", -30.0, 12.0, 0.1, 1, "dB" };
    Camp aCentre { "Centre", 0.0, 100.0, 1.0, 0, "%" };
    Camp aDeriva { "Deriva treta", 0.0, 100.0, 1.0, 0, "%" };
    Camp aVibrato { "Vibrato", 0.0, 200.0, 1.0, 0, "%" };
    Camp aTransicio { "Transició"_ca, 0.0, 300.0, 5.0, 0, "ms" };
    Boto btDesti { "Cromàtic"_ca, std::nullopt, Boto::Estil::Secundari };
    Boto btCorregeix { "Corregeix", std::nullopt, Boto::Estil::Primari };
    Boto btRestaura { "Torna a l'original"_ca, std::nullopt, Boto::Estil::Secundari };
    Boto btAB { "Escolta l'original"_ca, std::nullopt, Boto::Estil::Tecla };

    juce::Rectangle<int> zonaInfo, zonaInspector, zonaAuto;

    std::vector<int> triades();
    /** Aplica «f» a les notes triades com un gest (amb Desfés). */
    void enCampComenca();
    void enCampCanvi (const std::function<void (mibase::NotaVeu&)>& f);
    void actualitzaCamps();
};
