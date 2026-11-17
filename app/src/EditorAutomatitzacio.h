#pragma once

#include "Controls.h"
#include "DialegText.h"
#include "EstatEstudi.h"
#include "Plugins.h"
#include <map>
#include <mibase/Automatitzacio.h>

/** 0.56: l'editor d'automatització, a baix (on s'obre el Piano Roll). Mostra les línies (lanes) d'una pista, una sota
    l'altra, amb el seu zoom propi: punts, llapis, trams lineals, suaus o en esglaó, copiar, enganxar, duplicar,
    quantitzar i el valor que té cada paràmetre al capçal. Edita el projecte (amb desfer); el so el fa el motor. */
class EditorAutomatitzacio : public juce::Component, public juce::SettableTooltipClient
{
public:
    EditorAutomatitzacio (EstatEstudi& e, GestorPlugins& g);
    std::function<void()> onTanca;
    std::function<bool (const juce::KeyPress&)> onTeclaNoUsada;

    /** Obre l'editor sobre una pista; si «objectiu» no és buit, hi afegeix (o hi tria) aquesta línia. */
    void obre (int pista, const std::string& objectiu = {}, const std::string& nom = {});
    /** Menú per afegir una línia (també l'obre el botó «+ Línia»). Si «idInsert» no és buit, només els seus paràmetres. */
    void menuLinia (juce::Component* on, const std::string& idInsert = {});
    /** El transport (unes 30 vegades per segon): capçal i valors. */
    void tic();
    /** 0.57: gravació de l'automatització (cal cridar-la sempre, encara que l'editor estigui tancat). */
    void ticGravacio();
    bool escrivint (const std::string& idLinia) const { auto it = grav.find (idLinia); return it != grav.end() && it->second.escrivint; }

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    bool keyPressed (const juce::KeyPress& k) override;

private:
    mibase::Pista* pista();
    mibase::Automatitzacio* linia (int i);
    mibase::Automatitzacio* liniaTriada();
    int numLinies();
    juce::Rectangle<int> zonaLinia (int i) const;     // tota la fila
    juce::Rectangle<int> zonaCorba (int i) const;     // on es dibuixa la corba
    juce::Rectangle<int> zonaCap (int i) const;       // la capçalera de la fila
    struct ZonesCap { juce::Rectangle<int> nom, creu, llegeix, grava, valor; };
    ZonesCap zonesCap (int i) const;
    void menuGrava (int i, juce::Component* on, juce::Rectangle<int> on2);
    void menuCap (int i);
    double valorControl (mibase::Pista& t, const mibase::Automatitzacio& a);
    int liniaA (juce::Point<int> p) const;
    double xASeg (float x) const;
    float segAX (double s) const;
    double xACompas (float x) const;
    float compasAX (double c) const;
    double yAValor (const juce::Rectangle<int>& z, float y) const;
    float valorAY (const juce::Rectangle<int>& z, double v) const;
    double enganxa (double compas) const;             // a la graella (si està encesa)
    int puntA (int linia, juce::Point<float> p) const;
    juce::String textValor (const mibase::Automatitzacio& a, double v);
    mibase::Parametritzable* objecte (const mibase::Automatitzacio& a, int& index);
    void abans();
    void fet (const juce::String& evolucio = {});
    void triaLinia (int i);
    void menuPunt (int linia, int punt);
    void escriuValor (int linia, int punt);
    void quantitzaTriats();
    void copia();
    void enganxaAlCapcal();
    void duplicaTriats();
    void esborraTriats();
    void posaForma (int f);
    void igualQueEstudi();
    void zoom (double factor, float x);

    EstatEstudi& estat;
    GestorPlugins& plugins;
    juce::Rectangle<int> barra, regla, zonaLinies;
    static constexpr int ampleCap = 190;
    Boto btLinia { "+ Línia"_ca, std::nullopt, Boto::Estil::Secundari };
    Segmentat segEina { { "Fletxa", "Llapis" } };
    Segmentat segForma { { "Lineal", "Suau", "Esglaó"_ca } };
    Boto btGraella { "Graella", std::nullopt, Boto::Estil::Tecla };
    Boto btQuant { {}, dibuix::Icona::Quantitza, Boto::Estil::Subtil };
    Boto btCopia { "Copia", std::nullopt, Boto::Estil::Subtil }, btEnganxa { "Enganxa", std::nullopt, Boto::Estil::Subtil },
         btDuplica { "Duplica", std::nullopt, Boto::Estil::Subtil };
    Boto btMenys { "-", std::nullopt, Boto::Estil::Subtil }, btMes { "+", std::nullopt, Boto::Estil::Subtil },
         btEstudi { "Com l'estudi"_ca, std::nullopt, Boto::Estil::Subtil };
    Boto btTanca { {}, dibuix::Icona::Tanca, Boto::Estil::Subtil };

    // Gest en curs
    enum class Gest { Res, Mou, Requadre, Llapis, Tensio, Regla };
    Gest gest = Gest::Res;
    int liniaGest = -1, puntGest = -1;
    juce::Point<float> inici, ara;
    double c0 = 0.0, v0 = 0.0, tensio0 = 0.0;
    std::vector<mibase::PuntAuto> abansGest;          // la línia tal com era (per moure)
    std::vector<mibase::PuntAuto> traç;
    bool desfetFet = false;
    double capcalPintat = -1.0;
    std::unique_ptr<DialegText> dialeg;
    struct EstatGrav { bool escrivint = false; double vPrev = -1.0, ultimCanvi = 0.0; std::vector<mibase::PuntAuto> mostres; };
    std::map<std::string, EstatGrav> grav;
    void desaGravat (mibase::Pista& t, mibase::Automatitzacio& a, EstatGrav& s);
    static std::vector<mibase::PuntAuto>& portaRetalls() { static std::vector<mibase::PuntAuto> v; return v; }
};
