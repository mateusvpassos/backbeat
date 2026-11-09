#pragma once

#include "ui/MappingCommon.h"

#include <array>

namespace bb
{

// Aba "Pads": bata num pad, ele aparece na lista, escolha a peça do SSD5.
// Também: perfis, curva de força e pedal do chimbal.
class PadsPage final : public juce::Component,
                       private juce::ListBoxModel,
                       private juce::Timer
{
public:
    explicit PadsPage (MappingContext&);
    ~PadsPage() override;

    void refresh();                        // perfil trocou ou foi editado por fora
    void handleMidi (const MidiEvent& e);  // eventos crus da bateria (thread de mensagens)

    void paint (juce::Graphics&) override;
    void resized() override;

    float flashFor (int inputNote) const { return juce::isPositiveAndBelow (inputNote, 128) ? flash[(size_t) inputNote] : 0.0f; }

private:
    class Row;
    class CurveView;

    int getNumRows() override;
    void paintListBoxItem (int, juce::Graphics&, int, int, bool) override {}
    juce::Component* refreshComponentForRow (int row, bool selected, juce::Component* existing) override;

    void timerCallback() override;
    void refreshProfileCombo();
    void commit();
    int guessOutputFor (int inputNote) const;
    void startCalibration();

    MappingContext& ctx;

    juce::Label profileLabel;
    juce::ComboBox profileCombo;
    juce::TextButton newButton { "Novo" }, duplicateButton { "Duplicar" },
                     renameButton { "Renomear" }, deleteButton { "Excluir" }, kitButton { "Kit do SSD5..." };
    juce::Label hintLabel, catalogLabel;
    juce::ToggleButton learnToggle { "Adicionar pads novos sozinho" };
    juce::Label unmappedLabel;
    juce::ComboBox unmappedCombo;
    juce::ListBox list;
    juce::Label lastHitLabel;

    juce::Label velocityHeader, curveLabel, minLabel;
    juce::Slider curveSlider, minSlider;
    std::unique_ptr<CurveView> curveView;

    juce::Label hihatHeader, ccLabel, rangeLabel, hihatLiveLabel;
    juce::Slider ccSlider, rangeSlider;
    juce::ToggleButton invertToggle { "Inverter" };
    juce::TextButton calibrateButton { "Calibrar pedal" };

    std::array<float, 128> flash {};
    int lastInputVelocity = -1;
    int hihatLive = -1;

    bool calibrating = false;
    int calMin = 127, calMax = 0;
    double calEndMs = 0.0;
};

} // namespace bb
