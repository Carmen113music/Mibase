#pragma once

#include "DialegText.h"
#include "EstatEstudi.h"
#include "Historial.h"
#include "Instruments.h"
#include "PanellCarpetes.h"
#include "TaulaEstructura.h"
#include "PanellColor.h"
#include "PanellHarmonia.h"
#include "PianoRoll.h"
#include "Plugins.h"
#include "ServeiAudio.h"
#include "VistesEstudi.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>

/** L'estudi: barra d'eines, columna de l'esquerra (taula de treball / de mescles i fader de la pista triada),
    pistes i clips, i el panell de la dreta. Totes les seccions tenen l'amplada ajustable.
    Fa sonar el projecte i grava a la pista que té el botó de gravar encès (amb el motor de l'estudi). */
class PantallaEstudi : public juce::Component,
                       public juce::DragAndDropContainer,
                       private juce::Timer,
                       private juce::ChangeListener,
                       private juce::ScrollBar::Listener
{
public:
    PantallaEstudi (const juce::File& fitxer, const mibase::Projecte& projecte, juce::PropertiesFile& ajustos,
                    Historial& historial, ServeiAudio& servei);
    ~PantallaEstudi() override;

    const juce::File& getFitxer() const noexcept { return estat.fitxer; }
    juce::String getNom() const { return dibuix::utf8 (estat.projecte.nom); }

    /** Obre un altre fitxer de projecte (per exemple, una versió nova). */
    std::function<void (const juce::File&)> onObre;

    /** Desa ara mateix. */
    void desaAra();

    // Accions del menú Fitxer i del transport.
    void novaVersio();
    void importaDeMenu();
    void exportaProjecte();
    void exportaMescla();
    void transport (int accio);   // 0 principi, 1 enrere un compàs, 2 endavant un compàs, 3 atura, 4 play, 5 grava
    /** 0 aturat, 1 sonant, 2 comptant, 3 gravant. */
    int estatTransport() const noexcept { return estat.transport; }
    /** Botó del bucle del transport (0.11): encén o apaga el bucle (si no n'hi ha, en marca un de 4 compassos al capçal). */
    void commutaBucle();
    void fesCrossfade();
    bool bucleActiu() const noexcept { return estat.projecte.bucle.marcat && estat.projecte.bucle.actiu; }
    std::pair<juce::String, juce::String> textPosicio() const;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& k) override;
    void dragOperationEnded (const juce::DragAndDropTarget::SourceDetails& d) override
    {
        if (estat.segell.perArrossegar) { estat.segell = {}; estat.avisa(); }
        if (d.description.toString().startsWith ("mibase-pista:")) { estat.info.clear(); estat.avisa(); }
        zonaEditors.repintaTots();
    }

private:
    void timerCallback() override { stopTimer(); desaAra(); }

    /** Rellotge del transport i dels vúmetres (unes 30 vegades per segon). */
    struct Rellotge : juce::Timer
    {
        std::function<void()> tic;
        void timerCallback() override { if (tic) tic(); }
    };
    void tic();
    void publicaPla();
    void reprodueix();
    void atura();
    void grava();
    void acabaGravacio();
    void avisDeTargeta (const juce::String& titol, const juce::String& text);
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void scrollBarMoved (juce::ScrollBar* barra, double inici) override;

    void roda (const juce::MouseEvent& e, const juce::MouseWheelDetails& w);
    void actualitzaBarres();
    void colocaSeccions();
    void triaEina (Eina e);
    void menuClaqueta();
    void previsualitzaClic (bool encesa);

    void pistaNova (const juce::String& tipus, int canals);
    void obrePianoRoll (int pista, int clip);
    void configuraEditor (PianoRoll& pr);
    void tancaEditor (int index);
    void mostraPianoRoll (bool visible);
    void desenganxaPianoRoll (bool desenganxa);
    void acabaGravacioMidi();
    void canviaNom (int pista);
    void canviaTempo();
    void demanaImportar (int pista, double on);
    void importa (const juce::StringArray& fitxers, int pista, double on);
    bool importaUn (const juce::File& f, int& pista, double on, double& final, juce::String& error);
    void consolida (int pista, int a, int b);
    void carregaOna (const mibase::Presa& s);
    juce::File fitxerPresa (const mibase::Presa& s) const;
    juce::File audios() const;

    void duplica();
    void treu();
    void copia (bool retalla);
    void enganxa();
    void canviaModeRang (bool rang);
    void canviaInsereix (bool ins);
    void silenciaTriat();
    void demanaNomCarpeta (std::vector<int> pistes, const std::string& idACanviar);
    void zoomA (double a, double b, int pistaDe, int pistaFins);
    void zoomTot();
    void zoomTriat();
    void triaColor();
    void desfes (bool refer);

    juce::PropertiesFile& ajustos;
    Historial& historial;
    ServeiAudio& servei;
    EstatEstudi estat;
    Instruments instruments;   // els de les pistes MIDI (viu abans que el pla que els fa servir)
    GestorPlugins plugins { servei.escaner().llista() };   // 0.19: VST3 (inserts i instruments)
    juce::AudioFormatManager formats;

    // Barra d'eines.
    juce::OwnedArray<Boto> eines;
    Boto btClic { {}, dibuix::Icona::Metronom, Boto::Estil::Tecla };
    Boto btClicMenu { {}, dibuix::Icona::FletxaAvall, Boto::Estil::Secundari };
    Boto btTempo { "120 bpm · 4/4"_ca, std::nullopt, Boto::Estil::Secundari };
    Segmentat snap { { "Compàs"_ca, "Temps", "Lliure" } };
    Boto btRequadre { {}, dibuix::Icona::Requadre, Boto::Estil::Tecla };
    Boto btInsereix { {}, dibuix::Icona::Insereix, Boto::Estil::Tecla };
    BotoColor btColor;
    Boto btDesfes { {}, dibuix::Icona::Desfes, Boto::Estil::Secundari };
    Boto btRefes { {}, dibuix::Icona::Refes, Boto::Estil::Secundari };
    juce::Rectangle<int> zonaBarra, zonaEines, zonaInfo;

    // Seccions.
    ColumnaEsquerra esquerra { estat };
    Capcaleres capcaleres { estat };
    PanellCarpetes carpetes { estat };
    Boto btCarpetes { {}, dibuix::Icona::Carpeta, Boto::Estil::Tecla };
    Regla regla { estat };
    VistaPistes pistes { estat };
    TaulaMescles mescles { estat };
    TaulaEstructura taulaEstructura { estat };
    /** 0.16: qui té el focus (Piano Roll o les pistes) segons on s'ha clicat l'últim cop. */
    struct Focus : juce::MouseListener
    {
        std::function<void()> f;
        void mouseDown (const juce::MouseEvent&) override { if (f) f(); }
    };
    Focus focusEditor, focusPistes;
    PanellPista panell { estat };
    Divisor divEsquerra, divNoms, divPanell, divCarpetes;
    ZonaEditors zonaEditors { estat, [this] (PianoRoll& pr) { configuraEditor (pr); } };
    PanellHarmonia panellHarmonia { estat, [this] { return zonaEditors.actiu(); }, ajustos, [this] { return servei.getTeclatsMidi(); } };
    int ampleEsquerraEditor = 270;   // la columna esquerra amb el panell d'harmonia (més ampla que amb el fader)
    Divisor divPianoRoll { true };
    std::unique_ptr<FinestraPianoRoll> finestraPianoRoll;
    int altPianoRoll = 430, altInici = 0;
    juce::ScrollBar barraV { true }, barraH { false };
    int ampleEsquerra = 132, ampleNoms = 190, ampleCarpetes = 190, amplePanell = 270, ampleInici = 0;
    juce::Rectangle<int> zonaTitolNoms;

    // So i gravació.
    mibase::MapaAudios audioPreses;          // les preses a la memòria, per sonar
    Rellotge rellotge;
    double iniciPlay = 0.0;
    bool esperantMotor = false;         // s'ha enviat una ordre i el fil d'àudio encara no l'ha feta
    int estatEsperat = 0;
    juce::int64 tempsOrdre = 0;
    struct Gravacio
    {
        int pista = -1;
        juce::File fitxer;
        double desDe = 0.0, cursor = 0.0, retard = 0.0;
        bool retardMesurat = false;
        bool midi = false;                                   // grava el teclat MIDI (no la targeta)
        double fins = 0.0;                                   // MIDI: on s'ha aturat
        std::vector<mibase::MotorEstudi::MidiGravat> missatges;
    } gravacio;
    void recullMidiGravat();

    std::unique_ptr<DialegText> dialeg;
    std::unique_ptr<juce::FileChooser> selector;
    juce::int64 ultimaCopia = 0, tempsZoom = 0;
    double anclaZoom = 0.0;
    juce::Point<int> posZoom;
    bool calDesar = false;
};
