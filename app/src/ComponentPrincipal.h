#pragma once

#include "Capcalera.h"
#include "Historial.h"
#include "PantallaDiagnostic.h"
#include "PantallaEstudi.h"
#include "PantallaInici.h"
#include "Recents.h"
#include "ServeiAudio.h"

/** Contingut de la finestra: la barra de dalt (amb el menú Fitxer i el transport) i la pantalla que toqui. */
class ComponentPrincipal : public juce::Component
{
public:
    ComponentPrincipal (ServeiAudio& s, Historial& h)
        : servei (s), historial (h), recents (s.getAjustos()), capcalera (s), inici (s.getAjustos(), recents, h)
    {
        addAndMakeVisible (capcalera);
        capcalera.onPantalla = [this] (Capcalera::Pantalla p) { mostra (p); };
        capcalera.onFitxer = [this] (juce::Component& on) { menuFitxer (on); };
        capcalera.onTransport = [this] (Capcalera::Transport t)
        {
            if (estudi) estudi->transport ((int) t);
        };
        capcalera.onEstatTransport = [this] { return estudi ? estudi->estatTransport() : 0; };
        capcalera.onPosicio = [this]
        {
            return estudi ? estudi->textPosicio() : std::pair<juce::String, juce::String>();
        };

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
            mibase::disc::afegeixEvolucio (dibuix::cami (f.getParentDirectory()), dibuix::araText().toStdString(),
                                           "Obert: " + p->nom);
        recents.afegeix (f);
        historial.afegeix ("Projecte obert: "_ca + f.getFullPathName());

        tancaEstudi();
        estudi = std::make_unique<PantallaEstudi> (f, *p, servei.getAjustos(), historial, servei);
        estudi->onObre = [this] (const juce::File& altre) { obre (altre); };
        addChildComponent (*estudi);
        capcalera.setProjecte (estudi->getNom());
        mostra (Capcalera::Pantalla::Estudi);
    }

    void tancaEstudi()
    {
        if (estudi == nullptr) return;
        estudi->desaAra();
        estudi = nullptr;
        capcalera.setProjecte ({});
    }

    void mostra (Capcalera::Pantalla p)
    {
        if (p == Capcalera::Pantalla::Estudi && estudi == nullptr)
            p = Capcalera::Pantalla::Inici;

        if (estudi && actual == Capcalera::Pantalla::Estudi && p != Capcalera::Pantalla::Estudi)
            estudi->desaAra();

        if (p == Capcalera::Pantalla::Diagnostic && diagnostic == nullptr)
        {
            diagnostic = std::make_unique<PantallaDiagnostic> (servei, historial);
            addChildComponent (*diagnostic);
        }
        else if (p != Capcalera::Pantalla::Diagnostic && diagnostic != nullptr)
        {
            diagnostic = nullptr;
        }

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
        capcalera.setBounds (r.removeFromTop (60));
        inici.setBounds (r);
        if (estudi) estudi->setBounds (r);
        if (diagnostic) diagnostic->setBounds (r);
    }

    void paint (juce::Graphics& g) override { g.fillAll (tema::fons); }

private:
    void menuFitxer (juce::Component& on)
    {
        const bool hiHaProjecte = estudi != nullptr;
        juce::PopupMenu m;
        m.addItem (1, "Desa   (Ctrl+S)", hiHaProjecte);
        m.addItem (2, "Nova versió a partir d'aquí…"_ca, hiHaProjecte);
        m.addSeparator();
        m.addItem (3, "Projecte nou…"_ca);
        m.addItem (4, "Obre un projecte…"_ca);
        m.addItem (5, "Tanca el projecte", hiHaProjecte);
        m.addSeparator();
        m.addItem (6, "Importa àudio…   (Ctrl+I)"_ca, hiHaProjecte);
        m.addItem (7, "Exporta el projecte (carpeta per a un altre ordinador)…"_ca, hiHaProjecte);
        m.addItem (8, "Exporta la mescla (WAV)…"_ca, hiHaProjecte);
        m.addSeparator();
        m.addItem (9, "Surt");

        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&on), [this] (int r)
        {
            switch (r)
            {
                case 1: if (estudi) estudi->desaAra(); break;
                case 2: if (estudi) { mostra (Capcalera::Pantalla::Estudi); estudi->novaVersio(); } break;
                case 3: mostra (Capcalera::Pantalla::Inici); inici.obreNou(); break;
                case 4: inici.obreAltre(); break;
                case 5: tancaEstudi(); mostra (Capcalera::Pantalla::Inici); break;
                case 6: if (estudi) { mostra (Capcalera::Pantalla::Estudi); estudi->importaDeMenu(); } break;
                case 7: if (estudi) estudi->exportaProjecte(); break;
                case 8: if (estudi) estudi->exportaMescla(); break;
                case 9: juce::JUCEApplication::getInstance()->systemRequestedQuit(); break;
                default: break;
            }
        });
    }

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
