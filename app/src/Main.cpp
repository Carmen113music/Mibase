#include "Historial.h"
#include "ComponentPrincipal.h"
#include "ServeiAudio.h"
#include "Tema.h"
#include "EscanerPlugins.h"
#include "FinestraProgres.h"

#include <juce_gui_basics/juce_gui_basics.h>

class FinestraPrincipal : public juce::DocumentWindow
{
public:
    FinestraPrincipal()
        : juce::DocumentWindow ("Mibase " MIBASE_VERSIO "", tema::fons, juce::DocumentWindow::allButtons)
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&aspecte);
        setUsingNativeTitleBar (true);
        setResizable (true, true);
        setResizeLimits (1100, 700, 5000, 3000);
        // 0.28: primer, la pantalla d'arrencada (logo, nom i què es carrega); el programa es carrega just després
        arrencada = new PantallaArrencada();
        setContentOwned (arrencada, false);
        centreWithSize (1360, 860);
        setVisible (true);
        arrencada->posa ("Començant…"_ca, 0.02);
        juce::Component::SafePointer<FinestraPrincipal> jo (this);
        juce::Timer::callAfterDelay (60, [jo] { if (jo != nullptr) jo->carrega(); });
    }

    ~FinestraPrincipal() override
    {
        clearContentComponent();
        principal = nullptr;
        servei = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    void carrega()
    {
        const auto inici = juce::Time::getMillisecondCounter();
        arrencada->posa ("Ajustos, targeta d'àudio (ASIO) i teclats MIDI…"_ca, 0.15);
        servei = std::make_unique<ServeiAudio> (historial);
        arrencada->posa ("Plugins VST3, projectes recents i pantalla d'inici…"_ca, 0.65);
        principal = std::make_unique<ComponentPrincipal> (*servei, historial);
        arrencada->posa ("A punt"_ca, 1.0);
        const int queda = juce::jmax (250, 1100 - (int) (juce::Time::getMillisecondCounter() - inici));   // que es vegi una mica
        juce::Component::SafePointer<FinestraPrincipal> jo (this);
        juce::Timer::callAfterDelay (queda, [jo]
        {
            if (jo == nullptr) return;
            jo->mostraPrincipal();
        });
    }

    void mostraPrincipal()
    {
        {
            principal->setSize (getContentComponent()->getWidth(), getContentComponent()->getHeight());
            setContentOwned (principal.release(), false);
            arrencada = nullptr;
        }
    }

    tema::Aspecte aspecte;
    Historial historial;
    std::unique_ptr<ServeiAudio> servei;
    std::unique_ptr<ComponentPrincipal> principal;   // mentre la pantalla d'arrencada encara es veu
    PantallaArrencada* arrencada = nullptr;           // el contingut de la finestra al principi (el té la finestra)
};

class MibaseApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Mibase"; }
    const juce::String getApplicationVersion() override { return MIBASE_VERSIO; }
    // El procés que prova un plugin (0.18) és un segon Mibase sense finestra.
    bool moreThanOneInstanceAllowed() override          { return getCommandLineParameters().contains (EscanerPlugins::argument); }

    void initialise (const juce::String&) override
    {
        const auto args = getCommandLineParameterArray();
        if (args.size() >= 3 && args[0] == EscanerPlugins::argument)
        {
            setApplicationReturnValue (EscanerPlugins::escanejaUn (args[1], args[2]));
            quit();
            return;
        }
        finestra = std::make_unique<FinestraPrincipal>();
    }
    void shutdown() override                       { finestra = nullptr; }
    void systemRequestedQuit() override            { quit(); }

private:
    std::unique_ptr<FinestraPrincipal> finestra;
};

START_JUCE_APPLICATION (MibaseApp)
