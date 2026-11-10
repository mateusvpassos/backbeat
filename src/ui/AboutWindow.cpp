#include "ui/AboutWindow.h"

#include "ui/Theme.h"

#include "BackbeatAssets.h"

namespace bb
{

namespace
{
    struct License
    {
        const wchar_t* name;
        const wchar_t* text;
    };

    const License licenses[] = {
        { L"JUCE 8", L"© Raw Material Software Limited. Usado sob a GNU AGPLv3." },
        { L"ASIO", L"Tecnologia e SDK da Steinberg Media Technologies GmbH (incluído no JUCE sob a GPLv3). "
                   L"ASIO é marca registrada da Steinberg." },
        { L"VST 3 SDK", L"© Steinberg Media Technologies GmbH, licença MIT. VST é marca registrada da Steinberg." },
        { L"Geist", L"Fonte © The Geist Project Authors, SIL Open Font License 1.1." },
        { L"SSD5", L"O SSD5 e suas amostras são da Steven Slate Drums e não acompanham o Backbeat." },
    };

    class AboutComponent final : public juce::Component
    {
    public:
        AboutComponent (const juce::StringArray& shortcutLines, const juce::File& folder)
            : shortcuts (shortcutLines), dataFolder (folder)
        {
            logo = juce::Drawable::createFromImageData (BackbeatAssets::icon_svg, BackbeatAssets::icon_svgSize);

            closeButton.getProperties().set ("variant", "primary");
            folderButton.getProperties().set ("variant", "ghost");
            folderButton.setTooltip (dataFolder.getFullPathName());
            closeButton.onClick = [this]
            {
                if (auto* w = findParentComponentOfClass<juce::DialogWindow>())
                    w->exitModalState (0);
            };
            folderButton.onClick = [this] { dataFolder.startAsProcess(); };
            addAndMakeVisible (closeButton);
            addAndMakeVisible (folderButton);

            constexpr int width = 560, margin = 28;
            int h = 24 + 56 + 22 + 24 + 22 * juce::jmax (1, (shortcuts.size() + 1) / 2) + 22 + 24;
            for (const auto& l : licenses)
                h += (int) std::ceil (layoutFor (l, (float) (width - 2 * margin)).getHeight()) + 8;
            setSize (width, h + 26 + 32 + 22);
        }

        static juce::TextLayout layoutFor (const License& l, float width)
        {
            juce::AttributedString s;
            s.append (juce::String (l.name) + "  ", theme::font (13.0f, theme::Weight::semibold), theme::text);
            s.append (l.text, theme::font (13.0f), theme::muted);
            s.setWordWrap (juce::AttributedString::byWord);
            juce::TextLayout layout;
            layout.createLayout (s, width);
            return layout;
        }

        void resized() override
        {
            auto bottom = getLocalBounds().reduced (28, 22).removeFromBottom (32);
            closeButton.setBounds (bottom.removeFromRight (110));
            bottom.removeFromRight (8);
            folderButton.setBounds (bottom.removeFromRight (190));
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (theme::bg);
            auto area = getLocalBounds().reduced (28, 24);

            // ---- marca ----
            auto head = area.removeFromTop (56);
            if (logo != nullptr)
                logo->drawWithin (g, head.removeFromLeft (56).toFloat(), juce::RectanglePlacement::centred, 1.0f);
            head.removeFromLeft (16);
            g.setColour (theme::text);
            g.setFont (theme::font (22.0f, theme::Weight::semibold));
            g.drawText ("Backbeat", head.removeFromTop (30), juce::Justification::bottomLeft);
            g.setColour (theme::muted);
            g.setFont (theme::font (13.0f));
            g.drawText (juce::String (L"versão ") + JUCE_APPLICATION_VERSION_STRING
                            + L" · bateria eletrônica + SSD5 via ASIO",
                        head, juce::Justification::topLeft);

            area.removeFromTop (22);

            // ---- atalhos ----
            const auto section = [&] (const juce::String& title)
            {
                g.setColour (theme::muted);
                g.setFont (theme::font (11.5f, theme::Weight::semibold).withExtraKerningFactor (0.06f));
                g.drawText (theme::upper (title), area.removeFromTop (18), juce::Justification::centredLeft);
                area.removeFromTop (6);
            };

            section (L"Atalhos do perfil");
            {
                const int rows = (shortcuts.size() + 1) / 2;
                auto block = area.removeFromTop (22 * juce::jmax (1, rows));
                const int colW = block.getWidth() / 2;
                for (int i = 0; i < shortcuts.size(); ++i)
                {
                    const auto parts = juce::StringArray::fromTokens (shortcuts[i], "|", {});
                    juce::Rectangle<int> r (block.getX() + (i / juce::jmax (1, rows)) * colW,
                                            block.getY() + (i % juce::jmax (1, rows)) * 22, colW - 12, 22);
                    auto keyBox = r.removeFromLeft (64).reduced (0, 2).toFloat();
                    g.setColour (theme::surface);
                    g.fillRoundedRectangle (keyBox, 5.0f);
                    g.setColour (theme::border2);
                    g.drawRoundedRectangle (keyBox.reduced (0.5f), 5.0f, 1.0f);
                    g.setColour (theme::accentText);
                    g.setFont (theme::mono (12.0f));
                    g.drawText (parts[0], keyBox, juce::Justification::centred);
                    r.removeFromLeft (10);
                    g.setColour (theme::text);
                    g.setFont (theme::font (13.0f));
                    g.drawText (parts[1], r, juce::Justification::centredLeft);
                }
                if (shortcuts.isEmpty())
                {
                    g.setColour (theme::dim);
                    g.setFont (theme::font (13.0f));
                    g.drawText (L"Nenhum atalho de comando no perfil (Mapeamento... › Teclado).", block,
                                juce::Justification::centredLeft);
                }
            }

            area.removeFromTop (22);
            section (L"Licenças");
            for (const auto& l : licenses)
            {
                const auto layout = layoutFor (l, (float) area.getWidth());
                const int h = (int) std::ceil (layout.getHeight());
                layout.draw (g, area.removeFromTop (h).toFloat());
                area.removeFromTop (8);
            }
        }

    private:
        juce::StringArray shortcuts;
        juce::File dataFolder;
        std::unique_ptr<juce::Drawable> logo;
        juce::TextButton closeButton { "Fechar" }, folderButton { L"Pasta de configurações" };
    };
}

void showAboutWindow (const juce::StringArray& shortcuts, const juce::File& dataFolder)
{
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (new AboutComponent (shortcuts, dataFolder));
    options.dialogTitle = L"Sobre o Backbeat";
    options.dialogBackgroundColour = theme::bg;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

} // namespace bb
