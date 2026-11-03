#pragma once

#include "Controls.h"
#include "DialegText.h"

#include <juce_data_structures/juce_data_structures.h>
#include <map>
#include <set>

/** Navegador de carpetes de l'esquerra de la pantalla d'inici:
    llocs habituals, l'arbre de carpetes i les unitats. La carpeta triada és on es creen els projectes nous.
    Les carpetes que ja són projectes no surten a l'arbre (no es crea un projecte dins d'un altre). */
class NavegadorCarpetes : public juce::Component
{
public:
    explicit NavegadorCarpetes (juce::PropertiesFile& ajustos);
    ~NavegadorCarpetes() override;

    juce::File getCarpeta() const { return triada; }
    std::function<void()> onCanvi;

    /** Torna a llegir les carpetes del disc. */
    void refresca();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    struct Fila
    {
        enum class Tipus { Titol, Lloc, Carpeta, Unitat, Buit };
        Tipus tipus = Tipus::Buit;
        juce::File f;
        juce::String nom, extra;
        int nivell = 0;
        bool obert = false, teFills = false;
        int projectes = 0;
        int alcada = 34;
    };

    class Llista : public juce::Component
    {
    public:
        explicit Llista (NavegadorCarpetes& n) : nav (n) { setRepaintsOnMouseActivity (true); }
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDoubleClick (const juce::MouseEvent& e) override;
        void mouseMove (const juce::MouseEvent& e) override;
        void mouseExit (const juce::MouseEvent&) override { sobre = -1; repaint(); }
        int filaA (int y) const;
        int sobre = -1;
    private:
        NavegadorCarpetes& nav;
    };

    void construeix();
    void afegeixCarpeta (const juce::File& f, int nivell);
    juce::Array<juce::File> subcarpetes (const juce::File& f) const;
    int projectesDe (const juce::File& f);
    void tria (const juce::File& f);
    void obreOTanca (const juce::File& f);
    void triaArrel (const juce::File& f);
    void carpetaNova();
    void desa();

    juce::PropertiesFile& ajustos;
    juce::File arrel, triada;
    std::set<juce::String> oberts;
    std::map<juce::String, int> memoriaProjectes;
    std::vector<Fila> files;

    Llista llista { *this };
    juce::Viewport vista;
    Boto btNova { "Carpeta nova aquí"_ca, dibuix::Icona::Mes, Boto::Estil::Discontinu };
    std::unique_ptr<DialegText> dialeg;
    juce::Rectangle<int> zonaCami;
};
