#pragma once

#include "Capcalera.h"
#include "Historial.h"
#include "PantallaDiagnostic.h"
#include "PantallaEstudi.h"
#include "PantallaInici.h"
#include "Recents.h"
#include "ServeiAudio.h"

/** Contingut de la finestra: la barra de dalt i la pantalla que toqui. */
class ComponentPrincipal : public juce::Component
{
public:
    ComponentPrincipal (ServeiAudio& s, Historial& h)
        : servei (s), historial (h), recents (s.getAjustos()), capcalera (s), inici (s.getAjustos(), recents, h)
    {
        addAndMakeVisible (capcalera);
        capcalera.setProjecte ({});
        capcalera.onPantalla = [this] (Capcalera::Pantalla p) { mostra (p); };

        addChildComponent (inici);
        inici.onObre = [this] (const juce::File& f) { obre (f); };
        inici.onCreat = [this] (const juce::File& f) { obre (f, true); };

        mostra (Capcalera::Pantalla::Inici);
    }

    void obre (const juce::File& f, bool acabatDeCrear = false)
    {
        std::string error;
        auto p = mibase::disc::llegeix (dibuix::cami (f), &error);
        if (! p)
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "No s'ha pogut obrir el projecte"_ca,
                                                    f.getFullPathName() + "\n\n" + dibuix::utf8 (error));
            historial.afegeix ("No s'ha pogut obrir "_ca + f.getFullPathName() + ": " + dibuix::utf8 (error));
            return;
        }

        if (! acabatDeCrear)
            mibase::disc::afegeixEvolucio (dibuix::cami (f.getParentDirectory()), dibuix::araText().toStdString(), "Obert");
        recents.afegeix (f);
        historial.afegeix ("Projecte obert: "_ca + f.getFullPathName());

        if (estudi) estudi->desaAra();
        estudi = std::make_unique<PantallaEstudi> (f, *p, servei.getAjustos(), historial);
        addChildComponent (*estudi);
        capcalera.setProjecte (estudi->getNom());
        mostra (Capcalera::Pantalla::Estudi);
    }

    void mostra (Capcalera::Pantalla p)
    {
        if (p == Capcalera::Pantalla::Estudi && estudi == nullptr)
            p = Capcalera::Pantalla::Inici;

        if (p == Capcalera::Pantalla::Diagnostic && diagnostic == nullptr)
        {
            diagnostic = std::make_unique<PantallaDiagnostic> (servei, historial);
            addChildComponent (*diagnostic);
        }
        else if (p != Capcalera::Pantalla::Diagnostic && diagnostic != nullptr)
        {
            diagnostic = nullptr;   // atura els seus rellotges quan no es veu
        }

        if (estudi && actual == Capcalera::Pantalla::Estudi && p != Capcalera::Pantalla::Estudi)
            estudi->desaAra();

        actual = p;
        inici.setVisible (p == Capcalera::Pantalla::Inici);
        if (estudi) estudi->setVisible (p == Capcalera::Pantalla::Estudi);
        if (diagnostic) diagnostic->setVisible (p == Capcalera::Pantalla::Diagnostic);
        capcalera.setPantalla (p);
        resized();
        if (p == Capcalera::Pantalla::Estudi && estudi) estudi->grabKeyboardFocus();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        capcalera.setBounds (r.removeFromTop (56));
        inici.setBounds (r);
        if (estudi) estudi->setBounds (r);
        if (diagnostic) diagnostic->setBounds (r);
    }

    void paint (juce::Graphics& g) override { g.fillAll (tema::fons); }

private:
    ServeiAudio& servei;
    Historial& historial;
    Recents recents;
    Capcalera capcalera;
    PantallaInici inici;
    std::unique_ptr<PantallaEstudi> estudi;
    std::unique_ptr<PantallaDiagnostic> diagnostic;
    juce::TooltipWindow tooltips { this, 600 };
    Capcalera::Pantalla actual = Capcalera::Pantalla::Inici;
};
