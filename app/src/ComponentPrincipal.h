#pragma once

#include "Capcalera.h"
#include "Historial.h"
#include "PantallaDiagnostic.h"
#include "PantallaEstudi.h"
#include "PantallaInici.h"
#include "Recents.h"
#include "ServeiAudio.h"
#include <thread>

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
            if (estudi && t == Capcalera::Transport::Bucle) estudi->commutaBucle();
            else if (estudi) estudi->transport ((int) t);
        };
        capcalera.onEstatTransport = [this] { return estudi ? estudi->estatTransport() : 0; };
        capcalera.onBucleActiu = [this] { return estudi != nullptr && estudi->bucleActiu(); };
        capcalera.onPosicio = [this]
        {
            return estudi ? estudi->textPosicio() : std::pair<juce::String, juce::String>();
        };

        addChildComponent (inici);
        inici.onObre = [this] (const juce::File& f) { obre (f); };
        inici.onCreat = [this] (const juce::File& f) { obre (f, true); };

        mostra (Capcalera::Pantalla::Inici);
    }

    ~ComponentPrincipal() override { if (obrint != nullptr) obrint->atura = true; }

    void obre (const juce::File& f, bool acabatDeCrear = false)
    {
        if (obrint != nullptr) return;   // 0.28.1: ja se n'està obrint un
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
        comencaObrir (f, std::make_shared<mibase::Projecte> (*p));
    }

    // 0.28.1: els àudios es llegeixen en segon pla (la finestra de càrrega es va pintant i Windows no diu que no respon);
    // quan acaba, es fa la pantalla de l'estudi.
    struct FeinaObrir
    {
        std::atomic<bool> atura { false };
        Precarrega pre;
    };
    std::shared_ptr<FeinaObrir> obrint;
    std::unique_ptr<FinestraProgres> cortina;

    void comencaObrir (const juce::File& f, std::shared_ptr<mibase::Projecte> p)
    {
        if (obrint != nullptr) return;   // ja se n'està obrint un
        obrint = std::make_shared<FeinaObrir>();
        cortina = std::make_unique<FinestraProgres> ("Obrint «"_ca + dibuix::utf8 (p->nom) + "»"_ca, false, this);
        cortina->posa ("Llegint els àudios…"_ca, 0.0);
        struct Fitxer { std::string id; juce::File f; juce::int64 mida; };
        std::vector<Fitxer> fitxers;
        juce::int64 total = 0;
        for (auto& s : p->preses)
        {
            const auto fp = dibuix::fitxer (mibase::disc::camiRelatiu (dibuix::cami (f.getParentDirectory()), s.fitxer));
            const auto mida = juce::jmax ((juce::int64) 1, fp.getSize());
            fitxers.push_back ({ s.id, fp, mida });
            total += mida;
        }
        juce::Component::SafePointer<ComponentPrincipal> jo (this);
        auto feina = obrint;
        std::thread ([jo, feina, fitxers, total, f, p]
        {
            juce::AudioFormatManager fm;
            fm.registerBasicFormats();
            juce::int64 abans = 0;
            auto darrer = juce::Time::getMillisecondCounter();
            for (size_t i = 0; i < fitxers.size(); ++i)
            {
                if (feina->atura) return;
                const auto& x = fitxers[i];
                const auto text = "Llegint els àudios: "_ca + juce::String ((int) i + 1) + " de " + juce::String ((int) fitxers.size())
                                  + juce::String::fromUTF8 (" · ") + x.f.getFileName();
                auto progres = [&] (double fet)
                {
                    const auto ara = juce::Time::getMillisecondCounter();
                    if (ara - darrer < 50) return;
                    darrer = ara;
                    const double frac = 0.9 * ((double) abans + (double) x.mida * fet) / (double) juce::jmax ((juce::int64) 1, total);
                    juce::MessageManager::callAsync ([jo, text, frac] { if (jo != nullptr && jo->cortina != nullptr) jo->cortina->posa (text, frac); });
                };
                progres (0.0);
                auto r = llegeixPresa (fm, x.f, progres, &feina->atura);
                if (r.ona != nullptr) feina->pre[x.id] = std::move (r);
                abans += x.mida;
            }
            juce::MessageManager::callAsync ([jo, feina, f, p]
            {
                if (jo == nullptr || feina->atura) return;
                if (jo->cortina != nullptr) jo->cortina->posa ("Obrint els plugins i els instruments…"_ca, 0.92);
                // un moment perquè es pinti, i després es fa l'estudi (amb els plugins)
                juce::Timer::callAfterDelay (60, [jo, feina, f, p] { if (jo != nullptr) jo->acabaObrir (f, *p, feina); });
            });
        }).detach();
    }

    void acabaObrir (const juce::File& f, const mibase::Projecte& p, std::shared_ptr<FeinaObrir> feina)
    {
        estudi = std::make_unique<PantallaEstudi> (f, p, servei.getAjustos(), historial, servei, &feina->pre);
        cortina = nullptr;
        obrint = nullptr;
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
    /** Preferències (0.12): la pantalla de diagnòstic en una finestra a part, amb el botó de tancar. */
    struct FinestraPreferencies : juce::DocumentWindow
    {
        FinestraPreferencies (ServeiAudio& s, Historial& h, std::function<void()> enTancar)
            : juce::DocumentWindow ("Preferències · Mibase"_ca, tema::fons, juce::DocumentWindow::closeButton), tanca (std::move (enTancar))
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new PantallaDiagnostic (s, h), false);
            setResizable (true, true);
            setResizeLimits (800, 560, 10000, 10000);
            centreWithSize (1100, 760);
            setVisible (true);
        }
        void closeButtonPressed() override { if (tanca) tanca(); }
        std::function<void()> tanca;
    };
    std::unique_ptr<FinestraPreferencies> preferencies;

    void obrePreferencies()
    {
        if (preferencies != nullptr) { preferencies->toFront (true); return; }
        preferencies = std::make_unique<FinestraPreferencies> (servei, historial, [this]
        {
            juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<ComponentPrincipal> (this)] { if (sp) sp->preferencies = nullptr; });
        });
    }

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
        m.addItem (10, "Preferències (targeta d'àudio, MIDI i latència)…"_ca);
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
                case 10: obrePreferencies(); break;
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
