#include "ui/PadsPage.h"

#include "util/DrumNames.h"

namespace bb
{

// ---------------------------------------------------------------------------
// Linha da lista: [entrada] → [peça] [▶] [×]
// ---------------------------------------------------------------------------

class PadsPage::Row final : public juce::Component
{
public:
    explicit Row (PadsPage& p) : page (p)
    {
        theme::body (inputLabel);
        arrowLabel.setText (juce::String::fromUTF8 ("\xe2\x86\x92"), juce::dontSendNotification); // →
        arrowLabel.setColour (juce::Label::textColourId, theme::textDim);
        arrowLabel.setJustificationType (juce::Justification::centred);

        testButton.setTooltip (L"Ouvir esta peça");
        removeButton.setTooltip ("Tirar este pad da lista");

        for (auto* c : std::initializer_list<juce::Component*> { &inputLabel, &arrowLabel, &pieceBox, &testButton, &removeButton })
            addAndMakeVisible (c);

        pieceBox.onChange = [this]
        {
            const int id = pieceBox.getSelectedId();
            if (id == 0 || ! isValidRow())
                return;

            page.ctx.profiles.edit().notes.getReference (row).output = pieceCombo::noteForId (id);
            page.commit();
        };

        testButton.onClick = [this]
        {
            if (! isValidRow())
                return;
            const int out = page.ctx.profiles.current().notes.getReference (row).output;
            if (out >= 0)
            {
                page.ctx.engine.playNote (out, 100, true);
                page.ctx.engine.playNote (out, 0, false);
            }
        };

        removeButton.onClick = [this]
        {
            if (! isValidRow())
                return;
            const int input = page.ctx.profiles.current().notes.getReference (row).input;
            page.ctx.profiles.edit().removeRule (input);
            page.commit();
        };
    }

    void update (int newRow)
    {
        row = newRow;
        if (! isValidRow())
            return;

        const auto& rule = page.ctx.profiles.current().notes.getReference (row);
        inputNote = rule.input;
        inputLabel.setText ("Pad " + describeDrumNote (rule.input), juce::dontSendNotification);

        pieceCombo::fill (pieceBox, page.ctx.catalog, true, false);
        pieceCombo::select (pieceBox, rule.output);
        testButton.setEnabled (rule.output >= 0);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const float f = page.flashFor (inputNote);
        g.setColour (theme::panelLight.interpolatedWith (theme::accent.withAlpha (0.55f), f));
        g.fillRoundedRectangle (getLocalBounds().reduced (2, 2).toFloat(), 5.0f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10, 4);
        removeButton.setBounds (r.removeFromRight (30));
        r.removeFromRight (6);
        testButton.setBounds (r.removeFromRight (36));
        r.removeFromRight (10);
        inputLabel.setBounds (r.removeFromLeft (r.getWidth() * 2 / 5));
        arrowLabel.setBounds (r.removeFromLeft (30));
        pieceBox.setBounds (r);
    }

private:
    bool isValidRow() const { return juce::isPositiveAndBelow (row, page.ctx.profiles.current().notes.size()); }

    PadsPage& page;
    int row = -1, inputNote = -1;
    juce::Label inputLabel, arrowLabel;
    juce::ComboBox pieceBox;
    juce::TextButton testButton { juce::String::fromUTF8 ("\xe2\x96\xb6") }, removeButton { juce::String::fromUTF8 ("\xc3\x97") };
};

// ---------------------------------------------------------------------------
// Gráfico da curva de força
// ---------------------------------------------------------------------------

class PadsPage::CurveView final : public juce::Component
{
public:
    explicit CurveView (PadsPage& p) : page (p) {}

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (theme::panelLight);
        g.fillRoundedRectangle (r, 4.0f);

        g.setColour (theme::textDim.withAlpha (0.35f));
        g.drawLine (r.getX(), r.getBottom(), r.getRight(), r.getY(), 1.0f); // linear

        const auto& prof = page.ctx.profiles.current();
        juce::Path curve;
        for (int v = 1; v < 128; ++v)
        {
            const float x = r.getX() + r.getWidth() * (v / 127.0f);
            const float y = r.getBottom() - r.getHeight() * (MappingProfile::curveVelocity (v, prof.velocityCurve, prof.velocityMin) / 127.0f);
            if (v == 1) curve.startNewSubPath (x, y); else curve.lineTo (x, y);
        }
        g.setColour (theme::accent);
        g.strokePath (curve, juce::PathStrokeType (2.0f));

        if (page.lastInputVelocity > 0)
        {
            const int v = page.lastInputVelocity;
            const float x = r.getX() + r.getWidth() * (v / 127.0f);
            const float y = r.getBottom() - r.getHeight() * (MappingProfile::curveVelocity (v, prof.velocityCurve, prof.velocityMin) / 127.0f);
            g.setColour (juce::Colours::white);
            g.fillEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f);
        }
    }

private:
    PadsPage& page;
};

// ---------------------------------------------------------------------------
// Página
// ---------------------------------------------------------------------------

PadsPage::PadsPage (MappingContext& c) : ctx (c)
{
    curveView = std::make_unique<CurveView> (*this);

    theme::caption (profileLabel, "Perfil");
    theme::caption (hintLabel, L"Bata num pad: ele aparece aqui e acende quando chega. Escolha ao lado qual peça do plugin ele toca.");
    theme::caption (catalogLabel, {});
    theme::caption (unmappedLabel, "Notas fora da lista");
    theme::body (lastHitLabel);
    theme::header (velocityHeader, L"FORÇA (VELOCITY)"); // toUpperCase do JUCE não converte o "ç"
    theme::caption (curveLabel, "Curva");
    theme::caption (minLabel, juce::String::fromUTF8 ("Mínimo"));
    theme::header (hihatHeader, "Pedal do chimbal");
    theme::caption (ccLabel, "CC");
    theme::caption (rangeLabel, "Faixa do pedal");
    theme::caption (hihatLiveLabel, juce::String::fromUTF8 ("mexa o pedal para ver o valor"));

    unmappedCombo.addItem ("Tocar como chegam", 1);
    unmappedCombo.addItem ("Ignorar", 2);
    learnToggle.setToggleState (true, juce::dontSendNotification);

    list.setModel (this);
    list.setRowHeight (38);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);

    curveSlider.setRange (-1.0, 1.0, 0.01);
    curveSlider.setDoubleClickReturnValue (true, 0.0);
    curveSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    curveSlider.setTooltip ("Esquerda: toques fracos ficam mais fracos. Direita: toques fracos ficam mais fortes.");
    minSlider.setRange (1, 100, 1);
    minSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 20);
    minSlider.setDoubleClickReturnValue (true, 1.0);

    ccSlider.setSliderStyle (juce::Slider::IncDecButtons);
    ccSlider.setRange (0, 127, 1);
    ccSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 44, 22);
    rangeSlider.setSliderStyle (juce::Slider::TwoValueHorizontal);
    rangeSlider.setRange (0, 127, 1);
    rangeSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);

    for (auto* comp : std::initializer_list<juce::Component*> {
             &profileLabel, &profileCombo, &newButton, &duplicateButton, &renameButton, &deleteButton, &kitButton,
             &hintLabel, &catalogLabel, &learnToggle, &unmappedLabel, &unmappedCombo, &list, &lastHitLabel,
             &velocityHeader, &curveLabel, &curveSlider, &minLabel, &minSlider, curveView.get(),
             &hihatHeader, &ccLabel, &ccSlider, &invertToggle, &rangeLabel, &rangeSlider, &hihatLiveLabel, &calibrateButton })
        addAndMakeVisible (comp);

    profileCombo.onChange = [this]
    {
        const auto name = profileCombo.getText();
        if (name.isNotEmpty() && name != ctx.profiles.current().name)
            ctx.profiles.select (name);
    };

    newButton.onClick = [this]
    {
        askText ("Nome do novo perfil", ctx.profiles.uniqueName ("Perfil"),
                 [this] (const juce::String& name) { if (name.isNotEmpty()) ctx.profiles.createNew (name, false); });
    };

    duplicateButton.onClick = [this]
    {
        ctx.profiles.createNew (ctx.profiles.current().name + juce::String::fromUTF8 (" (cópia)"), true);
    };

    renameButton.onClick = [this]
    {
        askText ("Novo nome do perfil", ctx.profiles.current().name, [this] (const juce::String& name)
        {
            if (name.isNotEmpty() && name != ctx.profiles.current().name && ! ctx.profiles.rename (name))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Renomear",
                                                        juce::String::fromUTF8 ("Já existe um perfil com esse nome."));
        });
    };

    deleteButton.onClick = [this]
    {
        juce::AlertWindow::showOkCancelBox (
            juce::MessageBoxIconType::QuestionIcon, "Excluir perfil",
            "Excluir \"" + ctx.profiles.current().name + juce::String::fromUTF8 ("\"? O arquivo vai para a Lixeira."),
            "Excluir", "Cancelar", nullptr,
            juce::ModalCallbackFunction::create ([this] (int result)
            {
                if (result == 1 && ! ctx.profiles.removeCurrent())
                    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Excluir perfil",
                                                            juce::String::fromUTF8 ("É preciso ter pelo menos um perfil."));
            }));
    };

    kitButton.setTooltip (L"Ler a lista de peças de outro kit do SSD5 (.ssd5kit)");
    kitButton.onClick = [this] { if (ctx.chooseKitFile) ctx.chooseKitFile(); };

    unmappedCombo.onChange = [this]
    {
        ctx.profiles.edit().passUnmappedNotes = unmappedCombo.getSelectedId() == 1;
        commit();
    };

    curveSlider.onValueChange = [this] { ctx.profiles.edit().velocityCurve = curveSlider.getValue(); curveView->repaint(); };
    minSlider.onValueChange   = [this] { ctx.profiles.edit().velocityMin = (int) minSlider.getValue(); curveView->repaint(); };
    curveSlider.onDragEnd = [this] { commit(); };
    minSlider.onDragEnd   = [this] { commit(); };

    ccSlider.onValueChange = [this] { ctx.profiles.edit().hihatCc = (int) ccSlider.getValue(); commit(); };
    invertToggle.onClick   = [this] { ctx.profiles.edit().hihatInvert = invertToggle.getToggleState(); commit(); };
    rangeSlider.onValueChange = [this]
    {
        ctx.profiles.edit().hihatMin = (int) rangeSlider.getMinValue();
        ctx.profiles.edit().hihatMax = (int) rangeSlider.getMaxValue();
    };
    rangeSlider.onDragEnd = [this] { commit(); };
    calibrateButton.onClick = [this] { startCalibration(); };

    refresh();
    startTimerHz (30);
}

PadsPage::~PadsPage()
{
    list.setModel (nullptr);
}

void PadsPage::commit()
{
    // Assíncrono: commit() recria as linhas da lista, e quem chamou pode ser um botão de uma delas.
    // A edição já está no perfil; aqui só salva, recompila o mapeamento e chama refresh() de volta.
    juce::Component::SafePointer<PadsPage> safe (this);
    juce::MessageManager::callAsync ([safe]
    {
        if (safe != nullptr)
            safe->ctx.profiles.commit();
    });
}

void PadsPage::refreshProfileCombo()
{
    profileCombo.clear (juce::dontSendNotification);
    const auto names = ctx.profiles.names();
    for (int i = 0; i < names.size(); ++i)
        profileCombo.addItem (names[i], i + 1);
    profileCombo.setText (ctx.profiles.current().name, juce::dontSendNotification);
}

void PadsPage::refresh()
{
    const auto& p = ctx.profiles.current();

    refreshProfileCombo();
    unmappedCombo.setSelectedId (p.passUnmappedNotes ? 1 : 2, juce::dontSendNotification);
    curveSlider.setValue (p.velocityCurve, juce::dontSendNotification);
    minSlider.setValue (p.velocityMin, juce::dontSendNotification);
    ccSlider.setValue (p.hihatCc, juce::dontSendNotification);
    invertToggle.setToggleState (p.hihatInvert, juce::dontSendNotification);
    rangeSlider.setMinAndMaxValues (juce::jmin (p.hihatMin, p.hihatMax), juce::jmax (p.hihatMin, p.hihatMax), juce::dontSendNotification);
    catalogLabel.setText (juce::String::fromUTF8 ("Peças: ") + ctx.catalog.sourceDescription
                              + " (" + juce::String (ctx.catalog.getPieces().size()) + ")",
                          juce::dontSendNotification);
    kitButton.setVisible (ctx.catalog.isSsd5);

    list.updateContent();
    list.repaint();
    curveView->repaint();
}

int PadsPage::getNumRows()
{
    return ctx.profiles.current().notes.size();
}

juce::Component* PadsPage::refreshComponentForRow (int row, bool, juce::Component* existing)
{
    auto* r = dynamic_cast<Row*> (existing);

    if (! juce::isPositiveAndBelow (row, getNumRows()))
    {
        delete existing;
        return nullptr;
    }

    if (r == nullptr)
    {
        delete existing;
        r = new Row (*this);
    }

    r->update (row);
    return r;
}

int PadsPage::guessOutputFor (int input) const
{
    // SSD5: primeiro o que o perfil padrão (GM → SSD5) faria; senão a mesma nota, se ele tiver essa peça.
    // Os outros plugins já falam General MIDI: a mesma nota.
    if (! ctx.catalog.isSsd5)
        return input;
    const auto defaults = MappingProfile::defaultNexusGm();
    if (const int i = defaults.indexOfInput (input); i >= 0)
    {
        const int out = defaults.notes.getReference (i).output;
        if (ctx.catalog.find (out) != nullptr)
            return out;
    }

    return ctx.catalog.find (input) != nullptr ? input : -1;
}

void PadsPage::handleMidi (const MidiEvent& e)
{
    const auto type = e.data[0] & 0xF0;

    if (type == 0x90 && e.size >= 3 && e.data[2] > 0)
    {
        const int input = e.data[1];
        flash[(size_t) input] = 1.0f;
        lastInputVelocity = e.data[2];

        auto& profiles = ctx.profiles;
        int index = profiles.current().indexOfInput (input);

        if (index < 0 && learnToggle.getToggleState())
        {
            profiles.edit().setRule (input, guessOutputFor (input));
            commit();
            index = profiles.current().indexOfInput (input);
        }

        if (index >= 0)
        {
            list.selectRow (index);
            list.scrollToEnsureRowIsOnscreen (index);
        }

        const int outVel = MappingProfile::curveVelocity (e.data[2], profiles.current().velocityCurve, profiles.current().velocityMin);
        juce::String text = "Pad " + describeDrumNote (input) + juce::String::fromUTF8 ("  ·  força ") + juce::String (e.data[2]);
        text << juce::String::fromUTF8 ("  →  ")
             << (e.mapped >= 0 ? ctx.catalog.nameFor (e.mapped) + juce::String::fromUTF8 ("  ·  força ") + juce::String (outVel)
                               : juce::String ("ignorado"));
        lastHitLabel.setText (text, juce::dontSendNotification);
        curveView->repaint();
    }
    else if (type == 0xB0 && e.size >= 3 && e.data[1] == ctx.profiles.current().hihatCc)
    {
        hihatLive = e.data[2];
        if (calibrating)
        {
            calMin = juce::jmin (calMin, hihatLive);
            calMax = juce::jmax (calMax, hihatLive);
        }

        hihatLiveLabel.setText ("pedal: " + juce::String (hihatLive) + juce::String::fromUTF8 (" → plugin (CC4): ")
                                    + juce::String (e.mapped >= 0 ? e.mapped : 0)
                                    + juce::String::fromUTF8 ("  (0 = fechado ... 127 = aberto)"),
                                juce::dontSendNotification);
    }
}

void PadsPage::startCalibration()
{
    calibrating = true;
    calMin = 127;
    calMax = 0;
    calEndMs = juce::Time::getMillisecondCounterHiRes() + 6000.0;
    calibrateButton.setEnabled (false);
}

void PadsPage::timerCallback()
{
    bool anyFlash = false;
    for (auto& f : flash)
        if (f > 0.0f)
        {
            f = juce::jmax (0.0f, f - 0.05f);
            anyFlash = true;
        }

    if (anyFlash)
        list.repaint();

    if (calibrating)
    {
        const double remaining = calEndMs - juce::Time::getMillisecondCounterHiRes();
        if (remaining > 0.0)
        {
            calibrateButton.setButtonText ("Abra e feche o pedal... " + juce::String ((int) std::ceil (remaining / 1000.0)));
        }
        else
        {
            calibrating = false;
            calibrateButton.setEnabled (true);
            calibrateButton.setButtonText ("Calibrar pedal");

            if (calMax - calMin >= 10)
            {
                ctx.profiles.edit().hihatMin = calMin;
                ctx.profiles.edit().hihatMax = calMax;
                commit();
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Calibrar pedal",
                    juce::String::fromUTF8 ("Não chegou CC do pedal (ou mexeu pouco). Confira o número do CC: "
                                            "mexa o pedal e veja o valor que aparece embaixo da faixa."));
            }
        }
    }
}

void PadsPage::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);
}

void PadsPage::resized()
{
    auto area = getLocalBounds().reduced (16, 12);

    {
        auto row = area.removeFromTop (28);
        profileLabel.setBounds (row.removeFromLeft (50));
        profileCombo.setBounds (row.removeFromLeft (240));
        row.removeFromLeft (8);
        for (auto* b : { &newButton, &duplicateButton, &renameButton, &deleteButton })
        {
            b->setBounds (row.removeFromLeft (84));
            row.removeFromLeft (6);
        }
        kitButton.setBounds (row.removeFromRight (120));
    }
    area.removeFromTop (8);
    hintLabel.setBounds (area.removeFromTop (20));
    {
        auto row = area.removeFromTop (26);
        learnToggle.setBounds (row.removeFromLeft (240));
        unmappedLabel.setBounds (row.removeFromLeft (130));
        unmappedCombo.setBounds (row.removeFromLeft (170));
        row.removeFromLeft (12);
        catalogLabel.setBounds (row);
    }
    area.removeFromTop (6);

    // Rodapé: força | chimbal
    auto bottom = area.removeFromBottom (150);
    lastHitLabel.setBounds (area.removeFromBottom (26));
    area.removeFromBottom (4);
    list.setBounds (area);

    bottom.removeFromTop (8);
    auto left = bottom.removeFromLeft (bottom.getWidth() / 2).reduced (0, 0);
    auto right = bottom;
    right.removeFromLeft (20);

    velocityHeader.setBounds (left.removeFromTop (18));
    {
        auto graph = left.removeFromRight (120);
        curveView->setBounds (graph.withHeight (juce::jmin (graph.getHeight(), 120)));
        left.removeFromRight (12);
        auto r1 = left.removeFromTop (30);
        curveLabel.setBounds (r1.removeFromLeft (60));
        curveSlider.setBounds (r1);
        auto r2 = left.removeFromTop (30);
        minLabel.setBounds (r2.removeFromLeft (60));
        minSlider.setBounds (r2);
    }

    hihatHeader.setBounds (right.removeFromTop (18));
    {
        auto r1 = right.removeFromTop (30);
        ccLabel.setBounds (r1.removeFromLeft (30));
        ccSlider.setBounds (r1.removeFromLeft (110));
        r1.removeFromLeft (12);
        invertToggle.setBounds (r1.removeFromLeft (90));
        calibrateButton.setBounds (r1.removeFromRight (190));
        auto r2 = right.removeFromTop (30);
        rangeLabel.setBounds (r2.removeFromLeft (100));
        rangeSlider.setBounds (r2);
        hihatLiveLabel.setBounds (right.removeFromTop (22));
    }
}

} // namespace bb
