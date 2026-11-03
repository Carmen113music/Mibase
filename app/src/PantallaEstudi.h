#pragma once

#include "DialegText.h"
#include "EstatEstudi.h"
#include "Historial.h"
#include "ServeiAudio.h"
#include "VistesEstudi.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>

/** L'estudi: barra d'eines, columna de l'esquerra (taula de treball / de mescles i fader de la pista triada),
    pistes i clips, i el panell de la dreta. Totes les seccions tenen l'amplada ajustable.
    Encara no sona ni grava (arriba a la 0.5). */
class PantallaEstudi : public juce::Component,
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
    void transport (int accio);   // 0 principi, 1 enrere un compàs, 2 endavant un compàs, 3 atura
    std::pair<juce::String, juce::String> textPosicio() const;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& k) override;

private:
    void timerCallback() override { stopTimer(); desaAra(); }
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void scrollBarMoved (juce::ScrollBar* barra, double inici) override;

    void roda (const juce::MouseEvent& e, const juce::MouseWheelDetails& w);
    void actualitzaBarres();
    void colocaSeccions();
    void triaEina (Eina e);
    void menuClaqueta();
    void previsualitzaClic (bool encesa);

    void pistaNova (const juce::String& tipus, int canals);
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
    void desfes (bool refer);

    juce::PropertiesFile& ajustos;
    Historial& historial;
    ServeiAudio& servei;
    EstatEstudi estat;
    juce::AudioFormatManager formats;

    // Barra d'eines.
    juce::OwnedArray<Boto> eines;
    Boto btClic { {}, dibuix::Icona::Metronom, Boto::Estil::Tecla };
    Boto btClicMenu { {}, dibuix::Icona::FletxaAvall, Boto::Estil::Secundari };
    Boto btTempo { "120 bpm · 4/4"_ca, std::nullopt, Boto::Estil::Secundari };
    Segmentat snap { { "Compàs"_ca, "Temps", "Lliure" } };
    Boto btDesfes { {}, dibuix::Icona::Desfes, Boto::Estil::Secundari };
    Boto btRefes { {}, dibuix::Icona::Refes, Boto::Estil::Secundari };
    juce::Rectangle<int> zonaBarra, zonaEines, zonaInfo;

    // Seccions.
    ColumnaEsquerra esquerra { estat };
    Capcaleres capcaleres { estat };
    Faders faders { estat };
    Regla regla { estat };
    VistaPistes pistes { estat };
    TaulaMescles mescles { estat };
    PanellPista panell { estat };
    Divisor divEsquerra, divNoms, divFaders, divPanell;
    juce::ScrollBar barraV { true }, barraH { false };
    int ampleEsquerra = 132, ampleNoms = 170, ampleFaders = 150, amplePanell = 270, ampleInici = 0;
    juce::Rectangle<int> zonaTitolNoms, zonaTitolFaders;

    std::unique_ptr<DialegText> dialeg;
    std::unique_ptr<juce::FileChooser> selector;
    juce::int64 ultimaCopia = 0, tempsZoom = 0;
    double anclaZoom = 0.0;
    juce::Point<int> posZoom;
    bool calDesar = false;
};
