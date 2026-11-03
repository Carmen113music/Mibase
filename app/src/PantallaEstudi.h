#pragma once

#include "DialegText.h"
#include "EstatEstudi.h"
#include "Historial.h"
#include "VistesEstudi.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_data_structures/juce_data_structures.h>

/** L'estudi (0.3): pistes i clips com a Riffs, amb seccions d'amplada ajustable.
    Encara no sona ni grava (arriba a la 0.4): serveix per crear pistes, importar àudio i provar l'edició. */
class PantallaEstudi : public juce::Component,
                       private juce::Timer,
                       private juce::ChangeListener,
                       private juce::ScrollBar::Listener
{
public:
    PantallaEstudi (const juce::File& fitxer, const mibase::Projecte& projecte, juce::PropertiesFile& ajustos, Historial& historial);
    ~PantallaEstudi() override;

    const juce::File& getFitxer() const noexcept { return estat.fitxer; }
    juce::String getNom() const { return dibuix::utf8 (estat.projecte.nom); }

    /** Desa ara mateix (per exemple, en sortir de l'estudi). */
    void desaAra();

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

    void pistaNova (const juce::String& tipus);
    void canviaNom (int pista);
    void canviaTempo();
    void demanaImportar (int pista, double on);
    void importa (const juce::StringArray& fitxers, int pista, double on);
    bool importaUn (const juce::File& f, int& pista, double on, double& final, juce::String& error);
    void carregaOna (const mibase::Presa& s);
    juce::File fitxerPresa (const mibase::Presa& s) const;

    void duplica();
    void treu();
    void desfes (bool refer);

    juce::PropertiesFile& ajustos;
    Historial& historial;
    EstatEstudi estat;
    juce::AudioFormatManager formats;

    // Barra de dalt.
    Boto btPrincipi { {}, dibuix::Icona::Principi, Boto::Estil::Subtil };
    Boto btPlay { {}, dibuix::Icona::Play, Boto::Estil::Subtil };
    Boto btGrava { {}, dibuix::Icona::Grava, Boto::Estil::Subtil };
    Boto btClic { {}, dibuix::Icona::Metronom, Boto::Estil::Subtil };
    Boto btTempo { "120 bpm · 4/4"_ca, std::nullopt, Boto::Estil::Subtil };
    Segmentat snap { { "Compàs"_ca, "Temps", "Lliure" } };
    Boto btDesfes { {}, dibuix::Icona::Desfes, Boto::Estil::Subtil };
    Boto btRefes { {}, dibuix::Icona::Refes, Boto::Estil::Subtil };
    Boto btImporta { "Importa àudio…"_ca, dibuix::Icona::Importa, Boto::Estil::Secundari };
    juce::Rectangle<int> zonaBarra, zonaPosicio, zonaInfo;

    // Seccions.
    Capcaleres capcaleres { estat };
    Faders faders { estat };
    Regla regla { estat };
    VistaPistes pistes { estat };
    PanellPista panell { estat };
    Divisor divNoms, divFaders, divPanell;
    juce::ScrollBar barraV { true }, barraH { false };
    int ampleNoms = 170, ampleFaders = 150, amplePanell = 300, ampleInici = 0;
    juce::Rectangle<int> zonaTitolNoms, zonaTitolFaders;

    std::unique_ptr<DialegText> dialeg;
    std::unique_ptr<juce::FileChooser> selector;
    juce::int64 ultimaCopia = 0;
    bool calDesar = false;
};
