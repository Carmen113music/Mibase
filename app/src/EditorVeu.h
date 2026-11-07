#pragma once

#include "Controls.h"
#include "ServeiVeu.h"
#include "VistesEstudi.h"

/** 0.22: Vocal Editor (a baix, com el Piano Roll). Mostra les notes cantades d'una presa com a «blobs» sobre un
    teclat vertical, amb la corba d'afinació; les eines canvien l'afinació, el vibrato, la deriva, els formants i el
    volum de cada nota. El so el refà el ServeiVeu (fora del fil d'àudio). 0.23: consola a baix (tonalitat, poms que
    actuen sobre les notes triades i botons); les notes es mouen dins la tonalitat triada. */
class EditorVeu : public SeccioEstudi
{
public:
    enum Eina { Fletxa = 0, Vibrato, Deriva, Formant, Volum, numEines };
    static juce::String nomEina (int e);

    EditorVeu (EstatEstudi& e, ServeiVeu& s);

    std::function<void()> onTanca, onCommutaAB;
    std::function<void (bool refer)> onDesfes;
    std::function<void()> onSolo;   // 0.24
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
    Boto btSolo { "S", std::nullopt, Boto::Estil::Tecla };            // 0.24
    Segmentat selMode { { "Visual", "Assistit", "Bloquejat" } };     // 0.24 (0.25: el de la pista)
    Boto btAjusta { "Encaixa", std::nullopt, Boto::Estil::Secundari };
    Boto btTanca { {}, dibuix::Icona::Amaga, Boto::Estil::Subtil };

    /** 0.23: un pom de la consola. Arrossega amunt/avall (Maj = fi); doble clic = el valor de sempre. */
    class Pom : public juce::Component
    {
    public:
        Pom (const juce::String& nom, double mn, double mx, double perDefecte, bool bipolar, std::function<juce::String (double)> text);
        std::function<void()> onComenca, onAcaba;
        std::function<void (double)> onCanvi;
        void setValor (double v, bool igualATots) { valor = v; igual = igualATots; repaint(); }
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
    private:
        juce::String nom;
        double mn, mx, defecte, valor, valor0 = 0.0;
        bool bipolar, igual = true, arrossega = false;
        std::function<juce::String (double)> text;
        void posa (double v);
    };

    // Consola de baix: tonalitat, poms i botons.
    Boto btTo { "C", std::nullopt, Boto::Estil::Secundari };
    Boto btEscalaTon { "Escala", std::nullopt, Boto::Estil::Secundari };
    Pom pCorreccio, pVibrato, pRecta, pFormant, pVolum, pTransicio;
    Boto btTotes { "Tria-les totes", std::nullopt, Boto::Estil::Secundari };
    Boto btCap { "Cap", std::nullopt, Boto::Estil::Secundari };
    Boto btCorregeix { "Corregeix", std::nullopt, Boto::Estil::Primari };
    Boto btOriginal { "A l'original"_ca, std::nullopt, Boto::Estil::Secundari };
    Boto btDesfes { "Desfés"_ca, std::nullopt, Boto::Estil::Secundari };
    Boto btRefes { "Refés"_ca, std::nullopt, Boto::Estil::Secundari };
    Boto btAB { "Corregit", std::nullopt, Boto::Estil::Tecla };
    juce::Rectangle<int> zTon, zPoms, zBotons;
    std::vector<double> objectiusInici;   // la nota cap on va cada nota, mentre es gira el pom de correcció

    static constexpr int altBarra = 34, altRegla = 20, ampleTeclat = 56, altConsola = 112;
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
    enum class Gest { Res, Notes, Requadre, Capcal, Bucle };
    Gest gest = Gest::Res;
    juce::Point<int> inici, ratoli;
    std::vector<mibase::NotaVeu> notesInici;
    int notaSota = -1;

    juce::Rectangle<int> zonaBarra() const { return getLocalBounds().removeFromTop (altBarra); }
    juce::Rectangle<int> zonaRegla() const { return getLocalBounds().withTrimmedTop (altBarra).removeFromTop (altRegla).withTrimmedLeft (ampleTeclat); }
    juce::Rectangle<int> zonaConsola() const { return getLocalBounds().removeFromBottom (altConsola); }
    juce::Rectangle<int> zonaTeclat() const { return getLocalBounds().withTrimmedTop (altBarra + altRegla).withTrimmedBottom (altConsola).removeFromLeft (ampleTeclat); }
    juce::Rectangle<int> zonaNotes() const { return getLocalBounds().withTrimmedTop (altBarra + altRegla).withTrimmedBottom (altConsola).withTrimmedLeft (ampleTeclat); }

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
    void pintaConsola (juce::Graphics& g);
    /** Les notes on actua la consola: les triades o, si no n'hi ha cap, totes. */
    std::vector<int> notesConsola();
    mibase::Tonalitat tonCanco() const;
    void comencaPom();
    void canviaPom (const std::function<void (mibase::NotaVeu&, size_t)>& f);
    void menuTo();
    void menuEscala();
    void aplicaTonalitat (int modeTon, int to, int escala);
    // 0.24: segons el mode d'escala (Visual: semitons; Assistit i Bloquejat: dins la tonalitat).
    int objectiu (double midi);
    int grau (double midi, int dir);
    bool lliure (const juce::ModifierKeys& m) const;
    mibase::ModeEscala mode() const;   // 0.25: el de la pista
};
