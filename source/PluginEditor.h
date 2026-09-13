#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class GlitterLookAndFeel;

    void chooseImpulseResponse();
    void choosePresetToLoad();
    void choosePresetToSave();

    PluginProcessor& processorRef;
    std::unique_ptr<GlitterLookAndFeel> lookAndFeel;
    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::TextButton loadImpulseButton { "Open impulse response .wav" };
    juce::TextButton loadPresetButton { "Load preset" };
    juce::TextButton savePresetButton { "Save preset" };
    juce::Label impulseResponseLabel;
    juce::Label libraryLabel;
    juce::Slider intensitySlider;
    juce::Slider mixSlider;
    juce::Label intensityLabel;
    juce::Label mixLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> intensityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
