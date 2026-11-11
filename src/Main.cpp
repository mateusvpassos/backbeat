#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/BackbeatLookAndFeel.h"
#include "ui/MainComponent.h"
#include "ui/WindowChrome.h"
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
        setResizeLimits (1320, 790, 2400, 1600);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
        darkTitleBar (*this, true);

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

// Textos do JUCE (diálogo Áudio...) em português. O "samples" do buffer é fixo no JUCE e fica em inglês.
static juce::String portugueseStrings()
{
    return juce::String::fromUTF8 (R"pt(language: Portuguese (Brazil)
countries: br
"Audio device type:" = "Tipo de driver:"
"Device:" = "Dispositivo:"
"Output:" = "Saída:"
"Input:" = "Entrada:"
"Active input channels:" = "Entradas ativas:"
"Active output channels:" = "Saídas ativas:"
"Sample rate:" = "Taxa de amostragem:"
"Audio buffer size:" = "Tamanho do buffer:"
"Test" = "Testar"
"Plays a test tone" = "Toca um tom de teste"
"Control Panel" = "Painel de controle"
"Opens the device's own control panel" = "Abre o painel de controle do próprio dispositivo"
"Reset Device" = "Reiniciar dispositivo"
"Resets the audio interface - sometimes needed after changing a device's properties in its custom control panel" = "Reinicia a interface de áudio (às vezes é preciso depois de mudar algo no painel de controle dela)"
"Show advanced settings..." = "Mostrar opções avançadas..."
"Error when trying to open audio device!" = "Erro ao abrir o dispositivo de áudio"
"(no audio input channels found)" = "(nenhuma entrada de áudio encontrada)"
"(no audio output channels found)" = "(nenhuma saída de áudio encontrada)"
"none" = "nenhum"
"OK" = "OK"
)pt");
}

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

        juce::LocalisedStrings::setCurrentMappings (new juce::LocalisedStrings (portugueseStrings(), false));
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
