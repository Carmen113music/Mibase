#pragma once

#include "VistesEstudi.h"

#include <mibase/Midi.h>

/** Petit camp numèric que es canvia arrossegant amunt i avall (o amb la roda). Doble clic: valor inicial. */
class CaixaNumero : public juce::Component, public juce::SettableTooltipClient
{
public:
    CaixaNumero (const juce::String& etiqueta, int minim, int maxim, int inicial)
        : text (etiqueta), mn (minim), mx (maxim), valor (inicial), perDefecte (inicial)
    {
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }
    std::function<void (int)> onCanvi;
    int getValor() const noexcept { return valor; }
    void setValor (int v) { valor = juce::jlimit (mn, mx, v); repaint(); }

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override { valor0 = valor; }
    void mouseDrag (const juce::MouseEvent& e) override { canvia (valor0 - e.getDistanceFromDragStartY() / 2); }
    void mouseDoubleClick (const juce::MouseEvent&) override { canvia (perDefecte); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override { canvia (valor + (w.deltaY > 0 ? 1 : -1) * (std::abs (w.deltaY) > 0.2f ? 5 : 1)); }

private:
    void canvia (int v)
    {
        v = juce::jlimit (mn, mx, v);
        if (v == valor) return;
        valor = v;
        repaint();
        if (onCanvi) onCanvi (valor);
    }
    juce::String text;
    int mn, mx, valor, perDefecte, valor0 = 0;
};

/** El Piano Roll: editor del clip MIDI obert (EstatEstudi::clipObert).
    Teclat vertical, regla, graella sincronitzada amb el tempo i el compàs del projecte, notes i carril de velocitats.
    Viu a la zona de baix de l'estudi i es pot desenganxar a una finestra pròpia (per exemple, a la segona pantalla).

    Punts d'integració per a les fases següents (no es fan servir encara):
    - onAjustaTo: escales i tonalitat (mode assistit o bloquejat), per decidir el to d'una nota nova o moguda.
    - onColorFila: ressaltar les notes de l'escala o de l'acord a la graella.
    - onGenera: un generador (acords, arpegis, rasgueigs…) que afegeix notes MIDI normals al clip.
    - Carril de baix: ara la velocitat; més endavant els controladors (CC1, CC11, CC64, pitch bend…). */
class PianoRoll : public SeccioEstudi, private juce::ScrollBar::Listener
{
public:
    explicit PianoRoll (EstatEstudi& e);
    ~PianoRoll() override;

    // ---- Connexions amb l'estudi ----
    /** Un missatge MIDI cap a l'instrument de la pista (per escoltar les notes en dibuixar-les i amb el teclat). */
    std::function<void (int pista, std::uint8_t estat, std::uint8_t d1, std::uint8_t d2)> onDirecte;
    std::function<void (bool refer)> onDesfes;
    /** Les tecles que el Piano Roll no fa servir (Espai, R…), per quan és en una finestra a part. */
    std::function<bool (const juce::KeyPress&)> onTeclaNoUsada;
    std::function<void()> onDesenganxa, onTanca;

    // ---- Integració per a les fases següents ----
    std::function<int (int to)> onAjustaTo;                       // Fase 2: escala (assistit / bloquejat). -1: no es pot.
    std::function<juce::Colour (int to, juce::Colour fons)> onColorFila;   // Fase 2: ressaltat de l'escala i l'acord.

    void setDesenganxat (bool d);
    /** Torna a llegir l'estat (per exemple, quan l'estudi canvia el clip obert). */
    void actualitza() { changeListenerCallback (nullptr); }
    /** Cridat pel rellotge de l'estudi mentre sona: capçal i vista que el segueix. */
    void ticTransport();
    /** Torna a encaixar la vista amb el clip obert (quan s'obre un clip nou). */
    void encaixaVista();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    bool keyPressed (const juce::KeyPress& k) override;
    bool keyStateChanged (bool) override;

    enum class EinaPR { Fletxa = 0, Llapis, Goma };

    static constexpr int alcadaBarra = 46, ampleTeclat = 74, alcadaRegla = 22, gruix = 10;

protected:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

private:
    enum class Zona { Res, Barra, Teclat, Regla, Graella, VoraCarril, Carril };
    enum class Mode { Res, Requadre, Decidint, Mou, DuradaFi, DuradaInici, Dibuixa, Goma, Teclat, Velocitat, LiniaVel, VoraCarril, Regla };
    enum class Part { Cos, Inici, Fi };

    struct Toc { int nota = -1; Part part = Part::Cos; };

    // Geometria.
    juce::Rectangle<int> zonaTeclat() const;
    juce::Rectangle<int> zonaRegla() const;
    juce::Rectangle<int> zonaGraella() const;
    juce::Rectangle<int> zonaCarril() const;
    juce::Rectangle<int> zonaVoraCarril() const;
    Zona zonaA (juce::Point<int> p) const;

    float xTick (double tickAbs) const;
    double tickX (float x) const;
    float yTo (int to) const;       // dalt de la fila
    int toY (float y) const;

    // Clip obert.
    mibase::Clip* clip();
    const mibase::Clip* clip() const;
    mibase::Pista* pista();
    double posTicks() const;        // on cau Clip::pos, en ticks absoluts
    double tickAbs (const mibase::Nota& n) const { return posTicks() + (double) n.ini; }
    long long pas() const;          // graella en ticks (0 = lliure)
    long long duradaNova() const;
    long long enganxaAbs (double tickAbs, bool avall) const;   // ticks absoluts enganxats → ticks del clip
    Toc tocA (juce::Point<float> p) const;
    int notaCarrilA (juce::Point<float> p) const;

    // Accions.
    void triaEina (EinaPR e);
    void menuGraella();
    void menuDurada();
    void menuNota (int nota);
    void quantitza (bool tambeFinal, double forca = 1.0);
    void esborraTriades();
    void duplicaTriades();
    void copia (bool retalla);
    void enganxa();
    void transposaTriades (int semitons);
    void mouTriades (int passos);
    void allargaTriades (int passos);
    void triaTotes();
    void ajustaVista();
    void canviaOctava (int d);
    void aplica (const std::function<void (mibase::Clip&)>& f, const juce::String& info);
    std::vector<int>& triades() { return triadesNotes; }
    void netejaTriades();
    void actualitzaBarres();
    void scrollBarMoved (juce::ScrollBar* b, double inici) override;

    // So de prova.
    void notaOn (int to, int vel);
    void notaOff (int to);
    void apagaProva();
    void sonaBreu (int to, int vel);

    // Dibuix.
    void pintaBarra (juce::Graphics& g);
    void pintaTeclat (juce::Graphics& g);
    void pintaRegla (juce::Graphics& g);
    void pintaGraella (juce::Graphics& g);
    void pintaNotes (juce::Graphics& g);
    void pintaCarril (juce::Graphics& g);
    void pintaBuit (juce::Graphics& g);

    // Vista.
    double iniciVista = 0.0;        // ticks absoluts a la vora esquerra de la graella
    double pxPerTick = 0.06;
    int altNota = 14;
    int desplY = 0;
    int altCarril = 84;
    bool fantasmes = true;
    bool desenganxat = false;
    bool vistaPendent = false;      // s'ha d'encaixar la vista quan el Piano Roll tingui mida
    int graella = 5;                // índex de midi::valorsGraella() (5 = 1/16)
    int durada = 0;                 // 0: igual que la graella; si no, índex de valorsGraella()
    long long ultimaDurada = 0;     // la de l'última nota estirada a mà (es fa servir quan durada = 0 i la graella és lliure)
    EinaPR eina = EinaPR::Fletxa;
    int pistaVista = -1, clipVist = -1;

    // Selecció i gest.
    std::vector<int> triadesNotes;
    std::vector<int> triadesInici;
    Mode mode = Mode::Res;
    juce::Point<float> inici, ara;
    Toc toc0;
    double tick0 = 0.0;
    int to0 = 0, notaNova = -1, alt0 = 0;
    long long ini0 = 0, dur0 = 0;
    std::vector<int> velIni;        // velocitats a l'inici del gest (carril)
    juce::Point<float> ultimVel;
    int toSonant = -1;              // nota de prova que sona (ratolí)
    std::array<bool, 128> premudes {};
    Toc sobre;
    int sobreCarril = -1, sobreTecla = -1;

    mibase::midi::RetallNotes portaretalls;

    // Barra d'eines.
    juce::OwnedArray<Boto> eines;
    Boto btGraella { "1/16", std::nullopt, Boto::Estil::Secundari };
    Boto btDurada { "Nota: graella", std::nullopt, Boto::Estil::Secundari };
    Boto btQuantitza { "Quantitza", dibuix::Icona::Quantitza, Boto::Estil::Secundari };
    CaixaNumero velNova { "Vel", 1, 127, 100 };
    Boto btFantasmes { {}, dibuix::Icona::Fantasma, Boto::Estil::Tecla };
    Boto btOctAmunt { "Oct +", std::nullopt, Boto::Estil::Secundari };
    Boto btOctAvall { "Oct -", std::nullopt, Boto::Estil::Secundari };
    Boto btDesenganxa { {}, dibuix::Icona::Desenganxa, Boto::Estil::Secundari };
    Boto btTanca { {}, dibuix::Icona::Amaga, Boto::Estil::Secundari };
    juce::ScrollBar barraV { true }, barraH { false };
    juce::Rectangle<int> zonaInfo;
    juce::String info;
};

/** Finestra a part per al Piano Roll desenganxat (es pot portar a una altra pantalla i maximitzar). */
class FinestraPianoRoll : public juce::DocumentWindow
{
public:
    FinestraPianoRoll (PianoRoll& pr, std::function<void()> enTancar);
    void closeButtonPressed() override { if (tanca) tanca(); }

private:
    std::function<void()> tanca;
};
