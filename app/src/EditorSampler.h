#pragma once

#include <map>
#include <memory>
#include "Controls.h"
#include "Pom.h"
#include "VistesEstudi.h"
#include <mibase/Sampler.h>

/** 0.26: l'editor del samplejador (a baix, al lloc del Piano Roll). A dalt, l'ona del sample amb la regla i els
    marcadors (inici, final, bucle, fades); a baix, la consola amb els paràmetres. Tot és no destructiu: el fitxer no
    es toca mai. El so el fa el mibase::sampler::Samplejador de la pista (Instruments). */
/** 0.26.1: unes opcions petites, en una o dues columnes (una sola triada), per a la consola. */
class OpcionsPetites : public juce::Component, public juce::SettableTooltipClient
{
public:
    OpcionsPetites (juce::StringArray t, int columnes = 1) : textos (std::move (t)), cols (juce::jmax (1, columnes)) {}
    std::function<void (int)> onCanvi;
    void setTriat (int i) { if (i != triat) { triat = i; repaint(); } }
    int getTriat() const { return triat; }
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
private:
    juce::Rectangle<float> cel (int i) const;
    juce::StringArray textos;
    int cols = 1, triat = 0;
};

class EditorSampler : public SeccioEstudi, public juce::FileDragAndDropTarget, public juce::DragAndDropTarget
{
public:
    explicit EditorSampler (EstatEstudi& e);

    std::function<void()> onTanca, onCarrega;
    std::function<void (const juce::File&)> onFitxer;                 // un fitxer deixat anar a sobre
    std::function<void (std::uint8_t estat, std::uint8_t nota, std::uint8_t vel)> onNota;   // tecla de prova (a la pista)
    std::function<void (double segons)> onEscolta;                   // < 0: atura
    std::function<double()> posicioSo;                                // segons del fitxer original que sonen ara (-1: res)
    std::function<std::shared_ptr<const mibase::sampler::Mostra>()> mostra;   // l'àudio original
    std::function<bool (const juce::KeyPress&)> onTeclaNoUsada;
    std::function<void (int)> onAltConsola;                           // l'alçada dels paràmetres ha canviat (per desar-la)

    /** Al ritme del transport (capçal del so). */
    void tic();
    void encaixa();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    bool keyPressed (const juce::KeyPress& k) override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { sobreFitxer = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { sobreFitxer = false; repaint(); }

    static juce::String nomNota (int nota);   // «C4» (60)

    // 0.27: un sample de la biblioteca
    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("mibase-sample:") && pista() != nullptr; }
    void itemDragEnter (const SourceDetails&) override { sobreFitxer = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { sobreFitxer = false; repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        sobreFitxer = false;
        repaint();
        if (onFitxer) onFitxer (juce::File (d.description.toString().fromFirstOccurrenceOf ("mibase-sample:", false, false)));
    }

protected:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

private:
    mibase::Pista* pista();
    const mibase::ParamsSampler* params();
    void canvia (const std::function<void (mibase::ParamsSampler&)>& f, const juce::String& evolucio = {});
    void sincronitza();
    void refesPics();
    double durada() const;

    // ---- vista ----
    double xASeg (float x) const;
    float segAX (double s) const;
    double enganxa (double s) const;   // al pas per zero (si està encès)

    // ---- marcadors ----
    enum class Marca { Cap, Inici, Fi, BucleDe, BucleA, FadeIn, FadeOut, NouBucle, Escolta };
    Marca marcaA (juce::Point<float> p) const;
    juce::Rectangle<float> etiqueta (Marca mk) const;   // la pestanyeta d'un marcador (per agafar-lo)
    Marca arrossegant = Marca::Cap;
    double segPremut = 0.0;
    bool desfesFet = false;
    void menuOna (double s);

    // ---- teclat de pantalla (sota l'ona; la Root marcada) ----
    static constexpr int notaMin = 24, notaMax = 96;   // C1..C7
    juce::Rectangle<float> rectTecla (int nota) const;
    int notaA (juce::Point<float> p) const;
    void pintaTeclat (juce::Graphics& g);
    int notaRatoli = -1;


    // ---- pics de l'ona ----
    std::shared_ptr<const mibase::sampler::Mostra> m;
    std::vector<float> picMin, picMax;   // per cada blocPic mostres (mono)
    static constexpr int blocPic = 64;
    double posSo = -1.0;
    bool sobreFitxer = false;

    // ---- zones ----
    juce::Rectangle<int> zBarra, zRegla, zFonsOna, zOna, zTeclat, zConsola, zAdsr;

    // ---- barra ----
    Boto btCarrega { "Carrega…"_ca, dibuix::Icona::Importa, Boto::Estil::Secundari };
    Boto btPlay { "Escolta", dibuix::Icona::Play, Boto::Estil::Secundari };
    bool escoltant = false;
    int ticsSenseSo = 0;
    void commutaEscolta();
    Boto btZero { "Pas per zero", std::nullopt, Boto::Estil::Tecla };
    Segmentat selBucle { { "Sense", "Endavant", "Ping-pong", "Sustain + release" } };
    Boto btConsola { "Paràmetres"_ca, std::nullopt, Boto::Estil::Tecla };
    Boto btTanca { {}, dibuix::Icona::Amaga, Boto::Estil::Subtil };

    // ---- consola ----
    OpcionsPetites selToc { { "Play", "One-shot", "Gate" } };
    OpcionsPetites selVeus { { "Poli", "Mono", "Legato" } };
    OpcionsPetites selFiltre { { "Cap", "Baixos", "Alts", "Banda", "Notch" }, 2 };
    OpcionsPetites selPendent { { "12 dB", "24 dB" } };
    Boto btRoot { "Auto", std::nullopt, Boto::Estil::Secundari };
    Boto btAlTempo { "Al tempo", std::nullopt, Boto::Estil::Secundari };
    Boto btX2 { "x2", std::nullopt, Boto::Estil::Secundari };
    Boto btD2 { ":2", std::nullopt, Boto::Estil::Secundari };
    Boto btReversa { "Reversa", std::nullopt, Boto::Estil::Tecla };
    Boto btManteTo { "Manté el to"_ca, std::nullopt, Boto::Estil::Tecla };
    Boto btDurFixa { "Durada fixa", std::nullopt, Boto::Estil::Tecla };
    Boto btFlat { "FLAT", std::nullopt, Boto::Estil::Tecla };
    Boto btSmono { "S.MONO", std::nullopt, Boto::Estil::Tecla };

    struct ParamPom
    {
        std::unique_ptr<Pom> pom;
        std::function<double (const mibase::ParamsSampler&)> llegeix;
        std::function<void (mibase::ParamsSampler&, double)> escriu;
        bool alFinal = false;   // només s'aplica en deixar-lo anar (el que obliga a refer el so)
        double pendent = 0.0;
        int fila = 0;
    };
    std::vector<ParamPom> poms;
    ParamPom& afegeixPom (int fila, const juce::String& nom, double mn, double mx, double def, bool bipolar,
                          std::function<juce::String (double)> text,
                          std::function<double (const mibase::ParamsSampler&)> llegeix,
                          std::function<void (mibase::ParamsSampler&, double)> escriu, bool alFinal = false);
    // La consola és dins d'una zona amb desplaçament; el separador de sobre en canvia l'alçada.
    struct Consola : juce::Component
    {
        std::function<void (juce::Graphics&)> pinta;
        void paint (juce::Graphics& g) override { if (pinta) pinta (g); }
    };
    Consola consola;
    juce::Viewport vistaConsola;
    Divisor divConsola { true };
    int altInici = 0;
    void pintaConsola (juce::Graphics& g);
    struct Lloc { juce::Component* c; int ample; juce::String grup; };   // c == nullptr: el dibuix de l'ADSR
    std::vector<Lloc> fila1, fila2;    // ordre de la consola
    Pom* pomActiu = nullptr;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> grups;
    void pintaAdsr (juce::Graphics& g, juce::Rectangle<float> r);
};
