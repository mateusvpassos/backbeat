#include "ui/KeysPage.h"

namespace bb
{

// ---------------------------------------------------------------------------
// Botão que captura uma tecla: clique e pressione a tecla (Esc cancela).
// ---------------------------------------------------------------------------

class KeysPage::KeyCaptureButton final : public juce::TextButton
{
public:
    KeyCaptureButton()
    {
        setWantsKeyboardFocus (true);
        onClick = [this] { setListening (true); };
    }

    std::function<void (const juce::KeyPress&)> onKeyCaptured;

    void setKey (const juce::String& description)
    {
        keyText = description;
        updateText();
    }

    void setListening (bool shouldListen)
    {
        listening = shouldListen;
        if (listening)
            grabKeyboardFocus();
        updateText();
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (! listening)
            return juce::TextButton::keyPressed (key);

        setListening (false);
        if (key != juce::KeyPress (juce::KeyPress::escapeKey) && onKeyCaptured)
            onKeyCaptured (key);
        return true;
    }

    void focusLost (FocusChangeType) override
    {
        if (listening)
            setListening (false);
    }

private:
    void updateText()
    {
        setButtonText (listening ? juce::String::fromUTF8 ("pressione a tecla...")
                                 : (keyText.isEmpty() ? juce::String ("(clique e escolha)") : keyText));
        setColour (juce::TextButton::buttonColourId, listening ? theme::accent.darker (0.2f) : theme::panelLight);
    }

    bool listening = false;
    juce::String keyText;
};

// ---------------------------------------------------------------------------
// Linha: [tecla] [ação] [força] [×]
// ---------------------------------------------------------------------------

class KeysPage::Row final : public juce::Component
{
public:
    explicit Row (KeysPage& p) : page (p)
    {
        velocitySlider.setRange (1, 127, 1);
        velocitySlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 20);
        velocitySlider.setTooltip (juce::String::fromUTF8 ("Força com que a tecla toca a peça"));

        for (auto* c : std::initializer_list<juce::Component*> { &keyButton, &actionBox, &velocitySlider, &removeButton })
            addAndMakeVisible (c);

        keyButton.onKeyCaptured = [this] (const juce::KeyPress& k) { page.assignKey (row, k); };

        actionBox.onChange = [this]
        {
            if (! isValidRow())
                return;
            const int id = actionBox.getSelectedId();
            auto& b = page.ctx.profiles.edit().keys.getReference (row);

            if (id >= pieceCombo::commandIdBase)
            {
                b.command = (Command) (id - pieceCombo::commandIdBase);
                b.note = -1;
            }
            else if (pieceCombo::noteForId (id) >= 0)
            {
                b.command = Command::none;
                b.note = pieceCombo::noteForId (id);
            }
            velocitySlider.setVisible (b.command == Command::none);
            page.commit();
        };

        velocitySlider.onDragEnd = [this]
        {
            if (! isValidRow())
                return;
            page.ctx.profiles.edit().keys.getReference (row).velocity = (int) velocitySlider.getValue();
            page.commit();
        };

        removeButton.onClick = [this]
        {
            if (! isValidRow())
                return;
            page.ctx.profiles.edit().keys.remove (row);
            page.commit();
        };
    }

    void update (int newRow)
    {
        row = newRow;
        if (! isValidRow())
            return;

        const auto& b = page.ctx.profiles.current().keys.getReference (row);
        keyButton.setKey (b.key);

        pieceCombo::fill (actionBox, page.ctx.catalog, false, true);
        if (b.command != Command::none)
            actionBox.setSelectedId (pieceCombo::commandIdBase + (int) b.command, juce::dontSendNotification);
        else
            pieceCombo::select (actionBox, b.note);

        velocitySlider.setValue (b.velocity, juce::dontSendNotification);
        velocitySlider.setVisible (b.command == Command::none);
    }

    void startCapture() { keyButton.setListening (true); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (theme::panelLight);
        g.fillRoundedRectangle (getLocalBounds().reduced (2, 2).toFloat(), 5.0f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 4);
        removeButton.setBounds (r.removeFromRight (30));
        r.removeFromRight (8);
        keyButton.setBounds (r.removeFromLeft (170));
        r.removeFromLeft (10);
        velocitySlider.setBounds (r.removeFromRight (180));
        r.removeFromRight (10);
        actionBox.setBounds (r);
    }

private:
    bool isValidRow() const { return juce::isPositiveAndBelow (row, page.ctx.profiles.current().keys.size()); }

    KeysPage& page;
    int row = -1;
    KeyCaptureButton keyButton;
    juce::ComboBox actionBox;
    juce::Slider velocitySlider;
    juce::TextButton removeButton { juce::String::fromUTF8 ("\xc3\x97") };
};

// ---------------------------------------------------------------------------
// Página
// ---------------------------------------------------------------------------

KeysPage::KeysPage (MappingContext& c) : ctx (c)
{
    theme::caption (hintLabel, juce::String (
        L"Teclas que tocam peças funcionam com o Backbeat em foco. Comandos podem funcionar em qualquer janela: "
        L"prefira F1–F12 ou Ctrl/Alt + tecla, porque a tecla deixa de chegar aos outros programas."));
    hintLabel.setMinimumHorizontalScale (1.0f);
    theme::caption (warningLabel, {});
    warningLabel.setColour (juce::Label::textColourId, theme::error);

    list.setModel (this);
    list.setRowHeight (38);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);

    for (auto* comp : std::initializer_list<juce::Component*> { &hintLabel, &globalToggle, &warningLabel, &list, &addButton })
        addAndMakeVisible (comp);

    globalToggle.onClick = [this]
    {
        ctx.profiles.edit().globalCommandKeys = globalToggle.getToggleState();
        commit();
    };

    addButton.onClick = [this]
    {
        ctx.profiles.edit().keys.add ({ {}, 38, Command::none, 100 });
        captureRowOnRefresh = ctx.profiles.current().keys.size() - 1;
        commit();
    };

    refresh();
}

KeysPage::~KeysPage()
{
    list.setModel (nullptr);
}

void KeysPage::commit()
{
    juce::Component::SafePointer<KeysPage> safe (this);
    juce::MessageManager::callAsync ([safe]
    {
        if (safe != nullptr)
            safe->ctx.profiles.commit();
    });
}

void KeysPage::assignKey (int row, const juce::KeyPress& key)
{
    auto& keys = ctx.profiles.edit().keys;
    if (! juce::isPositiveAndBelow (row, keys.size()))
        return;

    const auto description = key.getTextDescription();

    // Uma tecla só pode ter uma função: tira de outra linha se já estava em uso.
    for (int i = keys.size(); --i >= 0;)
        if (i != row && juce::KeyPress::createFromDescription (keys.getReference (i).key) == key)
        {
            keys.remove (i);
            if (i < row)
                --row;
        }

    keys.getReference (row).key = description;
    commit();
}

void KeysPage::setHotkeyFailures (const juce::StringArray& failed)
{
    warningLabel.setText (failed.isEmpty() ? juce::String()
                                           : juce::String::fromUTF8 ("Não deu para registrar como atalho global: ")
                                                 + failed.joinIntoString (", ")
                                                 + juce::String::fromUTF8 (" (outro programa já usa?)"),
                          juce::dontSendNotification);
}

void KeysPage::refresh()
{
    globalToggle.setToggleState (ctx.profiles.current().globalCommandKeys, juce::dontSendNotification);
    list.updateContent();
    list.repaint();

    if (juce::isPositiveAndBelow (captureRowOnRefresh, getNumRows()))
    {
        list.scrollToEnsureRowIsOnscreen (captureRowOnRefresh);
        if (auto* row = dynamic_cast<Row*> (list.getComponentForRowNumber (captureRowOnRefresh)))
            row->startCapture();
    }
    captureRowOnRefresh = -1;
}

int KeysPage::getNumRows()
{
    return ctx.profiles.current().keys.size();
}

juce::Component* KeysPage::refreshComponentForRow (int row, bool, juce::Component* existing)
{
    if (! juce::isPositiveAndBelow (row, getNumRows()))
    {
        delete existing;
        return nullptr;
    }

    auto* r = dynamic_cast<Row*> (existing);
    if (r == nullptr)
    {
        delete existing;
        r = new Row (*this);
    }

    r->update (row);
    return r;
}

void KeysPage::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
}

void KeysPage::resized()
{
    auto area = getLocalBounds().reduced (16, 12);
    hintLabel.setBounds (area.removeFromTop (36));
    globalToggle.setBounds (area.removeFromTop (26));
    warningLabel.setBounds (area.removeFromTop (20));
    area.removeFromTop (6);
    addButton.setBounds (area.removeFromBottom (30).removeFromLeft (170));
    area.removeFromBottom (8);
    list.setBounds (area);
}

} // namespace bb
