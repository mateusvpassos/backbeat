#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/BackbeatLookAndFeel.h"
#include "ui/MainComponent.h"
#include "util/Settings.h"

namespace bb
{

class MainWindow final : public juce::DocumentWindow
{
public:
    explicit MainWindow (const juce::String& name)
        : juce::DocumentWindow (name, theme::bg, juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new MainComponent(), true);
        setResizable (true, true);
        setResizeLimits (1060, 740, 2400, 1600);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);

        // Teclas do PC que tocam peças precisam do foco na tela principal.
        if (auto* content = getContentComponent())
            content->grabKeyboardFocus();
    }

    void activeWindowStatusChanged() override
    {
        if (isActiveWindow())
            if (auto* content = getContentComponent())
                if (! content->hasKeyboardFocus (true))
                    content->grabKeyboardFocus();
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
};

class BackbeatApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return false; } // o ASIO só abre em um processo

    void initialise (const juce::String&) override
    {
        // Log em <dados>/backbeat.log (inicialização, erros de dispositivo/plugin). Nunca na thread de áudio.
        logger.reset (new juce::FileLogger (Settings::defaultDataDirectory().getChildFile ("backbeat.log"),
                                            "Backbeat " + getApplicationVersion() + " iniciado", 256 * 1024));
        juce::Logger::setCurrentLogger (logger.get());

        lookAndFeel = std::make_unique<BackbeatLookAndFeel>(); // visual da família (Geist, cores, botões)
        juce::LookAndFeel::setDefaultLookAndFeel (lookAndFeel.get());
        mainWindow = std::make_unique<MainWindow> (getApplicationName());
        juce::Logger::writeToLog ("janela principal pronta");

        if (getCommandLineParameterArray().contains ("--mapeamento"))
            if (auto* main = dynamic_cast<MainComponent*> (mainWindow->getContentComponent()))
                main->openMappingWindow();
    }

    void shutdown() override
    {
        mainWindow.reset();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
        lookAndFeel.reset();
        juce::Logger::writeToLog ("encerrado");
        juce::Logger::setCurrentLogger (nullptr);
        logger.reset();
    }

    void systemRequestedQuit() override { quit(); }

    void anotherInstanceStarted (const juce::String&) override
    {
        if (mainWindow != nullptr)
            mainWindow->toFront (true);
    }

private:
    std::unique_ptr<juce::FileLogger> logger;
    std::unique_ptr<BackbeatLookAndFeel> lookAndFeel;
    std::unique_ptr<MainWindow> mainWindow;
};

} // namespace bb

START_JUCE_APPLICATION (bb::BackbeatApplication)
