#include "Historial.h"
#include "ComponentPrincipal.h"
#include "ServeiAudio.h"
#include "Tema.h"
#include "EscanerPlugins.h"

#include <juce_gui_basics/juce_gui_basics.h>

class FinestraPrincipal : public juce::DocumentWindow
{
public:
    FinestraPrincipal()
        : juce::DocumentWindow ("Mibase " MIBASE_VERSIO "", tema::fons, juce::DocumentWindow::allButtons)
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&aspecte);

        servei = std::make_unique<ServeiAudio> (historial);

        setUsingNativeTitleBar (true);
        setResizable (true, true);
        setResizeLimits (1100, 700, 5000, 3000);
        setContentOwned (new ComponentPrincipal (*servei, historial), false);
        centreWithSize (1360, 860);
        setVisible (true);
    }

    ~FinestraPrincipal() override
    {
        clearContentComponent();
        servei = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    tema::Aspecte aspecte;
    Historial historial;
    std::unique_ptr<ServeiAudio> servei;
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
