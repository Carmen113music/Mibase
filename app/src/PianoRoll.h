#pragma once

#include "VistesEstudi.h"

#include <mibase/Harmonia.h>
#include <mibase/Guitarra.h>
#include <mibase/Midi.h>
#include "EscGlobal.h"
#include "Kits.h"

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

/** 0.38: les pestanyes de sota de l'editor: amb quina eina es treballa el clip (Piano Roll, Chord Pad, Drumpad, patrons).
    Les eines escriuen notes; el so el fa l'instrument de la pista. */
class PestanyesEina : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr int quantes = 5;
    std::function<void (int)> onTria;
    PestanyesEina() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void posa (int triada, std::array<bool, quantes> disponibles)
    {
        if (triada == activa && disponibles == actives) return;
        activa = triada; actives = disponibles; repaint();
    }
    static juce::String nom (int i)
    {
        switch (i)
        {
            case 0: return "Piano Roll";
            case 1: return "Chord Pad";
            case 2: return "Drumpad";
            case 3: return juce::String::fromUTF8 ("Patrons de guitarra");
            default: return juce::String::fromUTF8 ("Patrons de piano");
        }
    }
    juce::Rectangle<int> rect (int i) const
    {
        const int w = juce::jlimit (70, 170, (getWidth() - 12) / quantes);
        return { 6 + i * w, 0, w - 2, getHeight() };
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (tema::lateral);
        g.setColour (tema::linia2);
        g.fillRect (0, 0, getWidth(), 1);
        for (int i = 0; i < quantes; ++i)
        {
            auto r = rect (i).toFloat();
            const bool a = i == activa;
            if (a)
            {
                g.setColour (tema::fons);
                g.fillRoundedRectangle (r.withTrimmedBottom (3.0f).withTop (-4.0f), 4.0f);
                g.setColour (tema::accent);
                g.fillRect (r.getX() + 8.0f, r.getBottom() - 6.0f, r.getWidth() - 16.0f, 2.0f);
            }
            else if (i == sobre && actives[(size_t) i])
            {
                g.setColour (tema::linia2.withAlpha (0.6f));
                g.fillRoundedRectangle (r.withTrimmedBottom (3.0f).withTop (-4.0f), 4.0f);
            }
            g.setFont (dibuix::lletra (12.0f, a));
            g.setColour (a ? tema::text : actives[(size_t) i] ? tema::suau : tema::suau.withAlpha (0.4f));
            g.drawFittedText (nom (i), rect (i).reduced (4, 0).withTrimmedBottom (5), juce::Justification::centred, 1, 0.75f);
        }
    }
    void mouseMove (const juce::MouseEvent& e) override { const int k = a (e.getPosition()); if (k != sobre) { sobre = k; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { sobre = -1; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override { const int k = a (e.getPosition()); if (k >= 0 && onTria) onTria (k); }

private:
    int a (juce::Point<int> p) const { for (int i = 0; i < quantes; ++i) if (rect (i).contains (p)) return i; return -1; }
    int activa = 0, sobre = -1;
    std::array<bool, quantes> actives { true, true, false, false, false };
};

/** Un Piano Roll: editor d'un clip MIDI (el de EstatEstudi::editors[index]). N'hi pot haver fins a 3 apilats;
    tots comparteixen la vista horitzontal (EstatEstudi::vistaEditors), cadascun té la seva alçada de notes.
    Teclat vertical, regla, graella sincronitzada amb el tempo i el compàs del projecte, notes i carril de velocitats.
    Viu a la zona de baix de l'estudi i es pot desenganxar a una finestra pròpia (per exemple, a la segona pantalla).

    Punts d'integració per a les fases següents (no es fan servir encara):
    - (0.8) Escales: ressaltat de l'escala i de l'acord que sona, mode assistit i bloquejat (harmonia::).
    - (0.8) Segell: un acord del panell d'harmonia es posa amb un clic (EstatEstudi::segell).
    - insereixAcords(): els generadors (arpegis, rasgueigs…) hi afegiran notes MIDI normals.
    - Carril de baix: ara la velocitat; més endavant els controladors (CC1, CC11, CC64, pitch bend…). */
class PianoRoll : public SeccioEstudi, public juce::DragAndDropTarget, private juce::ScrollBar::Listener
{
public:
    PianoRoll (EstatEstudi& e, int index);
    void setIndex (int i) { index = i; actualitza(); }
    int getIndex() const noexcept { return index; }
    /** 0.31: l'eina triada des de fora (la barra de dalt de l'estudi): 0 fletxa, 1 llapis, 2 goma. */
    void posaEina (int i) { if (i >= 0 && i <= 2) triaEina ((EinaPR) i); }
    /** 0.31: Ctrl+A / Ctrl+S des de l'estudi. */
    void triaTot() { triaTotes(); }
    void desseleccionaTot() { triadesNotes.clear(); info.clear(); repaint(); }
    ~PianoRoll() override;

    // ---- Connexions amb l'estudi ----
    /** Un missatge MIDI cap a l'instrument de la pista (per escoltar les notes en dibuixar-les i amb el teclat). */
    std::function<void (int pista, std::uint8_t estat, std::uint8_t d1, std::uint8_t d2)> onDirecte;
    std::function<void (bool refer)> onDesfes;
    std::function<void()> onSolo;   // 0.24: solo de la pista de l'editor (tecla * del teclat numèric)
    std::function<void (int fila, const juce::File&)> onSampleAFila;   // 0.27: un sample de la biblioteca deixat a una fila del Drumpad
    /** Les tecles que el Piano Roll no fa servir (Espai, R…), per quan és en una finestra a part. */
    std::function<bool (const juce::KeyPress&)> onTeclaNoUsada;
    std::function<void()> onDesenganxa, onTanca;
    /** La vista horitzontal (comuna) ha canviat: els altres editors s'han de tornar a dibuixar. */
    std::function<void()> onVistaCanviada;
    /** Aquest editor passa a ser l'actiu (s'hi ha clicat). */
    std::function<void()> onActiu;

    // ---- Integració per a les fases següents ----
    std::function<juce::Colour (int to, juce::Colour fons)> onColorFila;   // per a capes de color de més endavant

    // ---- Ordres del panell d'harmonia (sobre aquest editor) ----
    const std::vector<int>& getTriades() const noexcept { return triadesNotes; }
    bool teClip() const { return clip() != nullptr; }
    /** Encén o apaga unes notes a l'instrument de la pista (per escoltar acords i progressions). */
    void sonaNotes (const std::vector<int>& notes, bool encen);
    void escoltaAcord (const std::vector<int>& notes);
    /** Afegeix acords seguits (un rere l'altre, «durada» ticks cadascun) a partir del compàs del capçal
        (o del començament del clip, si el capçal hi cau fora). */
    void insereixAcords (const std::vector<std::vector<int>>& acords, long long duradaAcord, const juce::String& nom);
    void fesAcords (int tipus, bool deTonalitat, int inversio, mibase::harmonia::Voicing v);
    void inverteixTriades (bool amunt);
    void enllacaTriades();
    int pistaDeLEditor() const { return pistaEditor(); }

    /** Chord Pad obert (amb un clip): el teclat MIDI de la pista toca els acords dels pads. */
    bool chordPadActiu() const { return modePads() && clip() != nullptr; }
    /** L'acord de cada pad (0 = C … 11 = B), amb l'octava i el voicing triats: el que toca cada tecla. */
    std::array<std::vector<int>, 12> acordsTeclat() const;

    /** Comença a arrossegar un acord (del panell o d'un pad) cap a un Piano Roll: en amollar-lo a la graella, s'hi posa. */
    static void arrossegaAcord (juce::Component& origen, EstatEstudi& estat, const std::vector<int>& notes, const juce::String& nom,
                                long long durada, bool enllaca, std::vector<mibase::Nota> rasgueig = {});
    static constexpr const char* idArrossegaAcord = "mibase-acord";
    /** Arrossegar una pista (des del seu nom) al botó de notes fantasma: «mibase-pista:<id>». */
    static constexpr const char* prefixArrossegaPista = "mibase-pista:";

    // ---- Arrossegar i deixar anar (acords) ----
    bool isInterestedInDragSource (const SourceDetails& d) override;
    void itemDragEnter (const SourceDetails& d) override { itemDragMove (d); }
    void itemDragMove (const SourceDetails& d) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails& d) override;
    bool shouldDrawDragImageWhenOver() override { return false; }

    void setDesenganxat (bool d);
    /** Torna a llegir l'estat (per exemple, quan l'estudi canvia el clip obert). */
    void actualitza() { changeListenerCallback (nullptr); }
    /** Cridat pel rellotge de l'estudi mentre sona: capçal i vista que el segueix. */
    void ticTransport();
    /** Torna a encaixar la vista amb el clip obert (quan s'obre un clip nou). */
    void encaixaVista (bool tambeHoritzontal = true);

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

    static constexpr int alcadaBarra = 46, ampleOctava = 18, alcadaRegla = 22, gruix = 10, alcadaPestanyes = 26;
    /** 0.38: els menús d'instrument i de so de la pista, des de la barra del Chord Pad (el botó on s'han d'obrir). */
    std::function<void (juce::Component&)> onMenuInstrument, onMenuSo;

protected:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

private:
    enum class Zona { Res, Barra, Octava, Teclat, Regla, Graella, VoraCarril, Carril, Pads };
    enum class Mode { Res, Requadre, Decidint, Mou, DuradaFi, DuradaInici, Dibuixa, Goma, Teclat, Velocitat, LiniaVel, VoraCarril, Regla, Pad };
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
    // 0.20: Drumpad. A una pista de bateria les files són les del kit (FilaBateria), no les 128 notes.
public:
    bool bateria() const;
    /** Amplada de la columna del teclat (al Drumpad, més ample: hi ha els noms dels sons). */
    int ampleTeclat() const { return bateria() ? 132 : 92; }
private:
    int numFiles() const;
    int filaDeTo (int to) const;    // -1 si la nota no té fila
    int toDeFila (int f) const;
    int filaY (float y) const;
    void pintaFilesBateria (juce::Graphics& g);
    void pintaNomsBateria (juce::Graphics& g);
    void mouFiles (mibase::Clip& k, std::vector<int>& idx, int dFiles);

    // Clip obert.
    mibase::Clip* clip();
    const mibase::Clip* clip() const;
    mibase::Pista* pista();
    int pistaEditor() const;
    mibase::ModeEscala modeEscala() const;   // 0.25: el de la pista de l'editor
    int altCarrilEf() const;
    double posTicks() const;        // on cau Clip::pos, en ticks absoluts
    double tickAbs (const mibase::Nota& n) const { return posTicks() + (double) n.ini; }
    long long pas() const;          // graella en ticks (0 = lliure)
    long long duradaNova() const;
    long long enganxaAbs (double tickAbs, bool avall) const;   // ticks absoluts enganxats → ticks del clip
    juce::Rectangle<int> rectOctava (bool amunt) const;

    // Escala.
    mibase::Tonalitat tonalitat() const;
    bool escalaActiva() const;                // assistit o bloquejat
    juce::Colour colorFila (int to, juce::Colour fons, const std::vector<int>& acordAra) const;
    /** Desplaçament de to més proper a «desitjat» amb què totes les notes (que ja eren de l'escala) en continuen sent. */
    int dToEscala (const mibase::Clip& k, const std::vector<int>& idx, int desitjat) const;
    std::vector<int> segellA (juce::Point<float> p) const;   // les notes del segell, a l'octava de sota el ratolí
    void posaSegell (juce::Point<float> p, bool mante);
    void vistaCanviada();

    // Chord Pad (a baix, en lloc de les velocitats).
    bool modePads() const;
    void canviaModePads (bool pads);
    std::vector<std::string> padsActuals() const;
    juce::Rectangle<int> zonaPads() const;     // on hi ha els pads (sota la barra dels pads)
    juce::Rectangle<int> rectPad (int i) const;
    int padA (juce::Point<int> p) const;
    std::vector<int> notesDelPad (int i) const;
    long long duradaPads() const;
    void pintaPads (juce::Graphics& g);
    void menuPresetsPads();
    void menuRasgueig();                                             // 0.29
    bool padsGuitarra() const { return voicingPads.getTriat() == 2; }
    std::vector<int> notesAcordPad (const mibase::harmonia::AcordPad& a) const;   // amb l'octava i el voicing triats (o la posició de guitarra)
    std::vector<mibase::Nota> rasgueigDe (const std::vector<int>& cordes) const;  // buit si no hi ha rasgueig triat
    void obreEditorPad (int i);
    void desaPad (int i, const std::string& nom);
    void actualitzaControlsPads();
    Toc tocA (juce::Point<float> p) const;
    int notaCarrilA (juce::Point<float> p) const;

    // Accions.
    void triaEina (EinaPR e);
    void menuGraella();
    void menuDurada();
    void menuNota (int nota);
    void menuFantasmes();
    std::vector<std::string>* fantasmes();
    void actualitzaFantasmes();
    void pintaOnaFantasma (juce::Graphics& g, const mibase::Clip& kc, juce::Colour col, double t0, double t1);
public:
    /** 0.20 (Drumpad): aplica una eina de bateria (humanitzar, swing, redoble…) als cops triats (o a tots, si no n'hi ha cap).
        La funció rep el clip, els triats, on cau el començament del clip (ticks absoluts) i la finestra del clip (ticks del clip). */
    void ordreBateria (const std::function<void (mibase::Clip&, std::vector<int>& triats, long long despl, long long iniT, long long fiT)>& f,
                       const juce::String& nom);
    long long pasGraella() const { return pas(); }
    int numTriades() const { return (int) triadesNotes.size(); }
    /** Posa uns acords als pads de la pista (per ordre, des de la C) i obre el Chord Pad (0.12). */
    void posaPads (const std::vector<std::string>& acords, const std::string& nom);
private:
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
    void zoomTriades();
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
    void pintaOctaves (juce::Graphics& g);
    void pintaRegla (juce::Graphics& g);
    void pintaGraella (juce::Graphics& g);
    void pintaNotes (juce::Graphics& g);
    void pintaCarril (juce::Graphics& g);
    void pintaBuit (juce::Graphics& g);

    int index = 0;

    // Vista: l'horitzontal és la comuna (EstatEstudi::vistaEditors); la vertical, de cada editor.
    double& iniciVista;             // ticks absoluts a la vora esquerra de la graella
    double& pxPerTick;
    int altNota = 14;
    int desplY = 0;
    int altCarril = 84;
    bool sobreFantasmes = false;    // s'arrossega una pista per sobre del botó de notes fantasma
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
    Boto btBloqueja { {}, dibuix::Icona::Cadenat, Boto::Estil::Tecla };
    Boto btSolo { "S", std::nullopt, Boto::Estil::Tecla };   // 0.24
    float reglaX0 = 0.0f;                                    // 0.24: on s'ha clicat la regla
    bool reglaBucle = false;                                 // 0.24: arrossegant la regla es fa el bucle
    juce::Point<float> ratoli { -1.0f, -1.0f };   // per dibuixar el segell

    PestanyesEina pestanyes;                                                // 0.38: l'eina, a sota
    Boto btInstrPads { "Instrument", std::nullopt, Boto::Estil::Secundari };   // 0.38: què sona (Chord Pad)
    Boto btSoPads { "So", std::nullopt, Boto::Estil::Secundari };
    void triaPestanya (int i);
    void actualitzaPestanyes();
    int baix() const { return getHeight() - gruix - alcadaPestanyes; }    // on s'acaba la graella o el carril
    Boto btPresetPads { "Acords de la tonalitat", std::nullopt, Boto::Estil::Secundari };
    Boto btLlapisPads { "Edita", dibuix::Icona::Llapis, Boto::Estil::Tecla };
    CaixaNumero octavaPads { "Octava", 1, 7, 4 };
    Segmentat voicingPads { { "Tancat", "Obert", "Guitarra" } };
    Boto btRasgueig { "Rasgueig", std::nullopt, Boto::Estil::Secundari };   // 0.29: només amb «Guitarra»
    std::string rasgueigPads = "folk";                                      // id del patró; buit = acord sencer, sense ritme
    double variacioRasgueig = 0.5;                                          // 0.32: 0 cap, 0.25 poca, 0.5 normal, 1 molta
    mutable unsigned llavorRasgueig = 1;                                    // 0.32: nova a cada pad arrossegat
    Segmentat selDuradaPads { { "1 compàs"_ca, "1/2", "1/4" } };
    Boto btMinimPads { "Mov. mínim"_ca, std::nullopt, Boto::Estil::Tecla };
    int padPremut = -1, padSobre = -1;
    std::vector<int> notesPadPremut;
    std::vector<int> notesTeclatPad;    // l'acord que toca el teclat de pantalla (Chord Pad)
    int octavaPremuda = 0;
    Boto btDesenganxa { {}, dibuix::Icona::Desenganxa, Boto::Estil::Secundari };
    Boto btTanca { {}, dibuix::Icona::Amaga, Boto::Estil::Secundari };
    juce::ScrollBar barraV { true }, barraH { false };
    juce::Rectangle<int> zonaInfo;
    juce::String info;
};

/** La zona dels editors: els Piano Rolls apilats, tots de la mateixa alçada i amb la mateixa vista horitzontal.
    És el que va a baix de l'estudi o, desenganxat, a la finestra a part. */
class ZonaEditors : public juce::Component, public juce::DragAndDropContainer
{
public:
    ZonaEditors (EstatEstudi& e, std::function<void (PianoRoll&)> configura);
    /** Crea o treu Piano Rolls perquè n'hi hagi tants com EstatEstudi::editors. */
    void sincronitza();
    PianoRoll* editor (int i) { return (i >= 0 && i < llista.size()) ? llista[i] : nullptr; }
    PianoRoll* actiu() { return editor (estat.editorActiu); }
    int quants() const noexcept { return llista.size(); }
    void setDesenganxat (bool d);
    void repintaTots() { for (auto* p : llista) p->repaint(); }
    void ticTransport() { for (auto* p : llista) p->ticTransport(); }
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (tema::linia2); }
    /** Si s'ha amollat un acord fora del Piano Roll, es descarta. */
    void dragOperationEnded (const juce::DragAndDropTarget::SourceDetails&) override
    {
        if (estat.segell.perArrossegar) { estat.segell = {}; estat.avisa(); }
        repintaTots();
    }

private:
    EstatEstudi& estat;
    std::function<void (PianoRoll&)> configura;
    juce::OwnedArray<PianoRoll> llista;
    bool desenganxat = false;
};

/** Finestra a part per als editors desenganxats (es pot portar a una altra pantalla i maximitzar). */
class FinestraPianoRoll : public juce::DocumentWindow
{
public:
    bool keyPressed (const juce::KeyPress& k) override { return k == juce::KeyPress::escapeKey && cridaEscGlobal(); }   // 0.27
    FinestraPianoRoll (juce::Component& contingut, std::function<void()> enTancar);
    void closeButtonPressed() override { if (tanca) tanca(); }

private:
    std::function<void()> tanca;
};
