#pragma once
#include <mibase/Diagnosi.h>
#include "DialegExporta.h"

#include <mibase/SeguimentCapcal.h>
#include "DialegText.h"
#include "EditorVeu.h"
#include "EditorSampler.h"
#include "EditorAutomatitzacio.h"
#include "Inserts.h"
#include "PanellBiblioteca.h"
#include "PanellPool.h"
#include "FinestraProgres.h"
#include "CarregaPreses.h"
#include "EstatEstudi.h"
#include "Historial.h"
#include "Instruments.h"
#include "PanellCarpetes.h"
#include "TaulaEstructura.h"
#include "PanellColor.h"
#include "PanellHarmonia.h"
#include "PanellBateria.h"
#include "FinestraKits.h"
#include "PianoRoll.h"
#include "Plugins.h"
#include "ServeiAudio.h"
#include "ServeiElastic.h"
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
                    Historial& historial, ServeiAudio& servei, Precarrega* precarrega = nullptr);
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
    /** 1.00: l'ANALYZE del Mastering Studio: renderitza el mix (sense màster), el màster i, si cal, cada pista sola. */
    std::shared_ptr<mibase::diagnosi::Informe> analitzaMaster (bool ambPistes, const mibase::diagnosi::Context& ctx);
    void transport (int accio);   // 0 principi, 1 enrere un compàs, 2 endavant un compàs, 3 atura, 4 play, 5 grava
    /** 0 aturat, 1 sonant, 2 comptant, 3 gravant. */
    int estatTransport() const noexcept { return estat.transport; }
    /** Botó del bucle del transport (0.11): encén o apaga el bucle (si no n'hi ha, en marca un de 4 compassos al capçal). */
    void commutaBucle();
    void fesCrossfade();
    bool bucleActiu() const noexcept { return estat.projecte.bucle.marcat && estat.projecte.bucle.actiu; }
    /** 0.68: «Retard mínim» (el botó RM, o sol mentre es grava). */
    void commutaRetardMinim();
    void menuRetardMinim (juce::Component& on);   // 0.69: a partir de quants ms apaga
    bool retardMinimActiu() const noexcept { return retardMinimBoto || estatTransport() >= 2; }
    bool retardMinimBoto = false;
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
    void grava (double cursor);
    void acabaGravacio();
    // 0.72: gravació intuïtiva amb el 0 del teclat numèric
    void gravaContextual();               // el 0 i el botó de gravar del transport
    void punchIn (int pista);
    void punchOut();
    int pistaArmada() const;
    int clipPerGravar (int pista) const;   // el clip buit on anirà la gravació (o -1)
    double iniciPreroll (double cursor) const;
    void revisaArmament();               // canviar de pista desarma l'anterior (i no arma la nova)
    bool esNumpad0 (const juce::KeyPress& k) const;
    juce::int64 darrerNumpad0 = 0;
    std::string idTriadaAbans;
    bool esperaExacte = false;            // s'espera exactament «estatEsperat» (punch in/out)
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
    // 0.22: Vocal Editor.
    void obreVeu (int pista, int clip);
    void obreVeuDePista (int pista);
    void tancaVeu();
    // 0.26: samplejador
    void obreSampler (int pista);
    void tancaSampler();
    void obreAutomatitzacio (int pista);   // 0.56
    void tancaAutomatitzacio();
    void tancaTot();   // 0.27: Esc global
    void demanaSample();
    void carregaSample (const std::string& idPista, const juce::File& f);
    // 0.27: biblioteca
    void deixaSample (const juce::File& f, int fila, double temps);
    void sampleAFila (int fila, const juce::File& f);
    int pistaSamplerNova (const juce::File& f);
    void enviaAlSampler (int pista, int clip, bool nomesBiblioteca);   // 0.28
    juce::File escriuTros (int pista, int clip, const juce::File& dir);   // 0.29
    void clipAlSampler (int pista, int clip);                            // 0.29
    void samplerABiblioteca();                                           // 0.29
    void gravaSampler();                                               // 0.28
    void commutaVeuAB();
    /** Envia al ServeiVeu l'estat de les preses que fan servir les pistes amb el Vocal Editor. */
    void sincronitzaVeu();
    /** Recull el que el ServeiVeu ha acabat (anàlisis i so corregit). */
    void recullVeu();
    /** 0.86: Elastic Audio. Demana el so estirat dels clips que en tenen i refà el mapa de sons per al pla
        (Vocal Editor + Elastic). «ara»: els que falten es fan al moment (abans d'exportar o fer un Print). */
    void sincronitzaElastic (bool ara = false);
    void recullElastic();
    /** El menú d'Elastic Audio del clip triat (algorisme, marcadors als cops, quantitzar, restaurar). */
    void menuElastic (juce::Component& boto);
    /** 0.87: escolta el clip d'àudio i en posa els acords a la pista d'Acords (els dubtosos, marcats). */
    void treuAcords (const mibase::Clip& c, const juce::String& nom);
    // 0.24: solo dels editors.
    std::string pistaEditor() const;
    void commutaSoloEditor();
    void seguixSoloEditor();
    void restauraSolos();
    void configuraEditor (PianoRoll& pr);
    void tancaEditor (int index);
    void mostraPianoRoll (bool visible);
    void desenganxaPianoRoll (bool desenganxa);
    void acabaGravacioMidi();
    void canviaNom (int pista);
    void busNou (bool fx, int pista, bool comSortida);
    juce::File sampleEscoltat;               // 0.40: el sample de la biblioteca que s'està escoltant
    juce::uint32 sampleEscoltatFins = 0;     // fins quan sona (ms del comptador)
    void canviaTempo();
    void demanaImportar (int pista, double on);
    void importa (const juce::StringArray& fitxers, int pista, double on);
    bool importaUn (const juce::File& f, int& pista, double on, double& final, juce::String& error);
    void consolida (int pista, int a, int b);
    void consolidaComp (int pista);   // 0.81
    void printPista (int pista);      // 0.82: amb els efectes aplicats
    void fesExportacio (const DialegExporta::Resultat& r, const juce::File& desti);   // 0.83
    std::unique_ptr<DialegExporta> dialegExporta;                                     // 0.83
    bool renderitzaPista (const mibase::Pista& t, bool ambEfectes, double ini, double fi, int canals, const juce::File& desti,
                          double minFi, long long& frames, std::string& error, double escalfa = 0.0, float* pic = nullptr);
    void carregaOna (const mibase::Presa& s);
    std::function<void (double)> progresOna;   // 0.28: mentre es llegeix un àudio (0..1)
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
    bool eraDavant = true;
    unsigned int noValidsVistos = 0;   // 0.64: trossos silenciats per valors no vàlids (sonda del mesclador)   // 0.62.1: Mibase era la finestra de davant (per refrescar el so en tornar-hi)
    void refrescaSo();
    ServeiAudio& servei;
    EstatEstudi estat;
    ServeiVeu serveiVeu;       // 0.22: abans dels components que el fan servir
    ServeiElastic serveiElastic;   // 0.86
    mibase::MapaAudios mapaSo;     // 0.86: el que es dona al pla: preses del Vocal Editor i sons estirats
    std::map<std::string, std::string> clauDeClip;   // 0.86: «pista#clip» -> la clau del darrer so estirat que s'ha fet
    Instruments instruments;   // els de les pistes MIDI (viu abans que el pla que els fa servir)
    GestorPlugins plugins { servei.escaner().llista() };   // 0.19: VST3 (inserts i instruments)
    juce::AudioFormatManager formats;

    // Barra d'eines.
    juce::OwnedArray<Boto> eines;
    Boto btClic { {}, dibuix::Icona::Metronom, Boto::Estil::Tecla };
    Boto btClicMenu { {}, dibuix::Icona::FletxaAvall, Boto::Estil::Secundari };
    Boto btTempo { "120 bpm · 4/4"_ca, std::nullopt, Boto::Estil::Secundari };
    Segmentat snap { { "Compàs"_ca, "Graella", "Lliure" } };
    Boto btGraella { "1/4", std::nullopt, Boto::Estil::Secundari };   // 0.42: subdivisió de la taula de treball
    void menuGraella();
    Boto btRequadre { {}, dibuix::Icona::Requadre, Boto::Estil::Tecla };
    Boto btInsereix { {}, dibuix::Icona::Insereix, Boto::Estil::Tecla };
    BotoColor btColor;
    Boto btDesfes { {}, dibuix::Icona::Desfes, Boto::Estil::Secundari };
    Boto btRefes { {}, dibuix::Icona::Refes, Boto::Estil::Secundari };
    Boto btFont { {}, dibuix::Icona::Font, Boto::Estil::Tecla };   // 0.97: Font d'inspiració
    std::unique_ptr<juce::DocumentWindow> finestraFont;
    void obreFont();
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
        std::function<void (juce::Component*)> entra;   // 0.27: el ratolí hi entra (sense clicar)
        void mouseDown (const juce::MouseEvent&) override { if (f) f(); }
        void mouseEnter (const juce::MouseEvent& e) override { if (entra) entra (e.eventComponent); }
    };
    bool potAgafarFocus() const;
    Focus focusEditor, focusPistes;
    PanellPista panell { estat };
    PanellBiblioteca biblioteca { ajustos };                          // 0.27
    Segmentat pestDreta { { "Info", "Biblioteca", "Pool" } };   // 0.92: «Clip» → «Info» (clip i pista)
    int dreta = 0;                                                    // 0.85: 0 clip, 1 biblioteca, 2 Pool
    PanellPool pool { estat };                                        // 0.85
    std::string presaEscoltada;                                       // 0.85
    juce::uint32 presaEscoltadaFins = 0;
    void escoltaPresa (const std::string& id);
    void localitzaPresa (const std::string& id);
    void canviaNomPresa (const std::string& id);
    void buscaPresa (const std::string& id);
    void aLaPaperera (const std::vector<std::string>& ids);
    void buidaPaperera();
    void importaAlPool();
    void deixaPresa (const std::string& id, int fila, double temps);
    std::string copiaAlPool (const juce::File& f, const juce::String& nomBase, juce::String& error);
    juce::File paperera() const { return audios().getChildFile ("Paperera"); }
    Divisor divEsquerra, divNoms, divPanell, divCarpetes;
    ZonaEditors zonaEditors { estat, [this] (PianoRoll& pr) { configuraEditor (pr); } };
    PanellHarmonia panellHarmonia { estat, [this] { return zonaEditors.actiu(); }, ajustos, [this] { return servei.getTeclatsMidi(); } };
    PanellBateria panellBateria { estat, [this] { return zonaEditors.actiu(); } };   // 0.20
    EditorVeu editorVeu { estat, serveiVeu };            // 0.22
    EditorSampler editorSampler { estat };               // 0.26
    EditorAutomatitzacio editorAuto { estat, plugins };  // 0.56
    inserts::ServeiScEstudi serveiSc { estat, plugins };  // 0.57
    PanellEditor panellEditor { estat, panellHarmonia, panellBateria, [this] { return zonaEditors.actiu(); } };
    std::unique_ptr<FinestraKits> finestraKits;
    void obreKits();
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
        bool punch = false;                                  // 0.72: començada amb Punch In (sonant)
        bool perSampler = false;                             // 0.28: grava dins el samplejador (un sample nou, no un clip)
        double fins = 0.0;                                   // MIDI: on s'ha aturat
        std::vector<mibase::MotorEstudi::MidiGravat> missatges;
    } gravacio;
    void recullMidiGravat();

    std::unique_ptr<DialegText> dialeg;
    std::unique_ptr<juce::FileChooser> selector;
    juce::int64 ultimaCopia = 0, tempsZoom = 0;
    double anclaZoom = 0.0;
    // 0.54: seguiment suau del capçal mentre sona
    mibase::SeguimentCapcal seguiment;
    double darrerTicMs = 0.0;
    int transportAbans = 0;
    double segonsPerCentrar() const;
    juce::Point<int> posZoom;
    bool calDesar = false;
};
