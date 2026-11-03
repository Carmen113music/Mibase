#pragma once

#include "Controls.h"
#include "FinestraProjecteNou.h"
#include "Historial.h"
#include "NavegadorCarpetes.h"
#include "Recents.h"

/** El que sabem d'un projecte recent (llegit del disc). */
struct InfoProjecte
{
    juce::File fitxer, carpeta;
    juce::String nom, lloc, tempo, quan, error;
    juce::int64 obert = 0;
    int preses = 0, pistes = 0;
    bool existeix = false;
    std::vector<mibase::disc::EntradaEvolucio> evolucio;
};

class TargetaProjecte : public juce::Component
{
public:
    explicit TargetaProjecte (const InfoProjecte& i) : info (i)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setRepaintsOnMouseActivity (true);
        setWantsKeyboardFocus (true);
        setTitle (info.nom);
    }

    std::function<void()> onTria, onObre;
    bool triada = false;
    InfoProjecte info;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override { if (onTria) onTria(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (onObre) onObre(); }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::returnKey) { if (onObre) onObre(); return true; }
        if (k == juce::KeyPress::spaceKey)  { if (onTria) onTria(); return true; }
        return false;
    }
};

class PanellDetalls : public juce::Component
{
public:
    PanellDetalls();
    void setInfo (const InfoProjecte* i);
    std::function<void()> onObre, onTreu;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    std::optional<InfoProjecte> info;
    Boto btObre { "Obre a l'estudi"_ca, dibuix::Icona::Play, Boto::Estil::Primari };
    Boto btMostra { "Mostra la carpeta a l'Explorador"_ca };
    Boto btTreu { "Treu-lo de la llista"_ca };
    juce::Rectangle<int> zonaStats, zonaEvolucio;
};

class PantallaInici : public juce::Component
{
public:
    PantallaInici (juce::PropertiesFile& ajustos, Recents& recents, Historial& historial);
    ~PantallaInici() override;

    /** Obre un projecte (el gestiona la finestra principal). */
    std::function<void (const juce::File&)> onObre;
    /** S'acaba de crear un projecte (s'obre sense anotar «Obert» al diari). */
    std::function<void (const juce::File&)> onCreat;

    /** Torna a llegir els recents i les carpetes. */
    void refresca();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override { if (isVisible()) refresca(); }

    /** Finestra «Projecte nou» i «Obre un altre projecte» (també des del menú Fitxer). */
    void obreNou();
    void obreAltre();

private:
    void ompleTargetes();
    void colocaTargetes();
    void tria (const juce::File& f);
    void tancaNou();
    InfoProjecte llegeixInfo (const Recents::Entrada& e) const;

    juce::PropertiesFile& ajustos;
    Recents& recents;
    Historial& historial;

    NavegadorCarpetes navegador;
    Divisor divisorEsq, divisorDre;
    int ampleEsq = 300, ampleDre = 340, ampleInici = 0;

    Boto btAltre { "Obre un altre projecte…"_ca, dibuix::Icona::Obre };
    Boto btNou { "Projecte nou", dibuix::Icona::Mes, Boto::Estil::Primari };
    juce::TextEditor cerca;
    Segmentat filtre { { "Tots", "Només d'aquesta carpeta"_ca } };

    juce::Component graella;
    juce::Viewport vistaGraella;
    juce::OwnedArray<TargetaProjecte> targetes;
    std::vector<InfoProjecte> infos;
    juce::File triat;

    PanellDetalls detalls;
    juce::Rectangle<int> zonaCentre, zonaCapcalera, zonaTitol;

    std::unique_ptr<FinestraProjecteNou> finestraNou;
    std::unique_ptr<juce::FileChooser> selector;
};
