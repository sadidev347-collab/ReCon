#include "PluginEditor.h"
#include <cmath>

class PluginEditor::GlitterLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    GlitterLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (0xff050505));
        setColour (juce::Label::textColourId, juce::Colour (0xffe9e9e9));
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff171717));
        setColour (juce::TextButton::textColourOffId, juce::Colour (0xfff5f5f5));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        const auto diameter = static_cast<float> (juce::jmin (width, height)) - 16.0f;
        const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, diameter, diameter)
                                .withCentre (juce::Point<float> (static_cast<float> (x)
                                                                     + static_cast<float> (width) * 0.5f,
                                                                 static_cast<float> (y)
                                                                     + static_cast<float> (height) * 0.5f));
        const auto centre = bounds.getCentre();
        const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        g.setColour (juce::Colour (0xff111111));
        g.fillEllipse (bounds);

        juce::ColourGradient ring (juce::Colours::white.withAlpha (0.9f), bounds.getX(), bounds.getY(),
                                   juce::Colour (0xff666666), bounds.getRight(), bounds.getBottom(), false);
        g.setGradientFill (ring);
        g.drawEllipse (bounds, 1.5f);

        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius - 4.0f, radius - 4.0f, 0.0f,
                           rotaryStartAngle, angle, true);
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.strokePath (arc, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        const auto pointerLength = radius * 0.58f;
        const auto pointerAngle = angle - juce::MathConstants<float>::halfPi;
        const auto pointerEnd = centre + juce::Point<float> (std::cos (pointerAngle) * pointerLength,
                                                              std::sin (pointerAngle) * pointerLength);
        g.setColour (juce::Colours::white);
        g.drawLine (centre.x, centre.y, pointerEnd.x, pointerEnd.y, 3.0f);
        g.fillEllipse (juce::Rectangle<float> (centre.x - 4.0f, centre.y - 4.0f, 8.0f, 8.0f));

        juce::ignoreUnused (slider);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        auto colour = shouldDrawButtonAsDown ? juce::Colour (0xff3a3a3a)
                                             : (shouldDrawButtonAsHighlighted
                                                    ? juce::Colour (0xff292929)
                                                    : backgroundColour);
        g.setColour (colour);
        g.fillRoundedRectangle (bounds, 12.0f);
        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.drawRoundedRectangle (bounds, 12.0f, 1.0f);
    }
};

PluginEditor::PluginEditor (PluginProcessor& processor)
    : AudioProcessorEditor (&processor),
      processorRef (processor),
      lookAndFeel (std::make_unique<GlitterLookAndFeel>())
{
    setLookAndFeel (lookAndFeel.get());

    for (auto* slider : { &intensitySlider, &mixSlider })
    {
        slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 22);
        slider->setColour (juce::Slider::textBoxTextColourId, juce::Colours::white);
        slider->setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff111111));
        addAndMakeVisible (slider);
    }

    intensityLabel.setText ("REVERB INTENSITY", juce::dontSendNotification);
    mixLabel.setText ("DRY / WET MIX", juce::dontSendNotification);
    intensityLabel.setJustificationType (juce::Justification::centred);
    mixLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (intensityLabel);
    addAndMakeVisible (mixLabel);

    loadImpulseButton.onClick = [this] { chooseImpulseResponse(); };
    addAndMakeVisible (loadImpulseButton);
    impulseResponseLabel.setText (processorRef.getImpulseResponseName(), juce::dontSendNotification);
    impulseResponseLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (impulseResponseLabel);

    libraryLabel.setText ("USER PRESET LIBRARY", juce::dontSendNotification);
    libraryLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (libraryLabel);
    loadPresetButton.onClick = [this] { choosePresetToLoad(); };
    savePresetButton.onClick = [this] { choosePresetToSave(); };
    addAndMakeVisible (loadPresetButton);
    addAndMakeVisible (savePresetButton);

    intensityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "intensity", intensitySlider);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, "mix", mixSlider);

    setSize (700, 520);
}

PluginEditor::~PluginEditor()
{
    setLookAndFeel (nullptr);
}

void PluginEditor::chooseImpulseResponse()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Choose an impulse response", juce::File{}, "*.wav");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& chooser)
                              {
                                  const auto file = chooser.getResult();
                                  if (processorRef.loadImpulseResponse (file))
                                      impulseResponseLabel.setText (processorRef.getImpulseResponseName(),
                                                                    juce::dontSendNotification);
                              });
}

void PluginEditor::choosePresetToLoad()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Open ReCon preset", juce::File{},
                                                        "*.reconpreset");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& chooser)
                              {
                                  processorRef.loadPreset (chooser.getResult());
                                  impulseResponseLabel.setText (processorRef.getImpulseResponseName(),
                                                                juce::dontSendNotification);
                              });
}

void PluginEditor::choosePresetToSave()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Save ReCon preset", juce::File{},
                                                        "*.reconpreset");
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                              | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& chooser)
                              {
                                  auto file = chooser.getResult();
                                  if (file.getFileExtension().isEmpty())
                                      file = file.withFileExtension (".reconpreset");
                                  processorRef.savePreset (file);
                              });
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff050505));

    juce::ColourGradient background (juce::Colour (0xff0d0d0d), 0.0f, 0.0f,
                                     juce::Colour (0xff070707), static_cast<float> (getWidth()),
                                     static_cast<float> (getHeight()), false);
    g.setGradientFill (background);
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (8.0f), 22.0f);

    auto content = getLocalBounds().toFloat().reduced (24.0f);
    g.setColour (juce::Colour (0xff111111));
    g.fillRoundedRectangle (content, 18.0f);
    g.setColour (juce::Colours::white.withAlpha (0.13f));
    g.drawRoundedRectangle (content, 18.0f, 1.0f);

    auto controls = content.reduced (18.0f);
    controls.removeFromTop (54.0f);
    controls = controls.removeFromTop (255.0f);
    g.setColour (juce::Colour (0xff0a0a0a));
    g.fillRoundedRectangle (controls, 16.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawRoundedRectangle (controls, 16.0f, 1.0f);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced (42);
    auto header = area.removeFromTop (42);
    loadImpulseButton.setBounds (header.removeFromLeft (250));
    impulseResponseLabel.setBounds (header.reduced (4));

    area.removeFromTop (10);
    auto knobs = area.removeFromTop (245);
    const auto knobSize = juce::jmin (170, knobs.getWidth() / 3);
    auto left = knobs.removeFromLeft (knobs.getWidth() / 2);
    auto right = knobs;
    intensitySlider.setBounds (left.withSizeKeepingCentre (knobSize, knobSize));
    mixSlider.setBounds (right.withSizeKeepingCentre (knobSize, knobSize));
    intensityLabel.setBounds (intensitySlider.getX() - 20, intensitySlider.getBottom() - 2,
                              intensitySlider.getWidth() + 40, 28);
    mixLabel.setBounds (mixSlider.getX() - 20, mixSlider.getBottom() - 2,
                        mixSlider.getWidth() + 40, 28);

    area.removeFromTop (24);
    auto presetArea = area.removeFromTop (90);
    libraryLabel.setBounds (presetArea.removeFromLeft (230));
    savePresetButton.setBounds (presetArea.removeFromRight (130).reduced (4, 18));
    loadPresetButton.setBounds (presetArea.removeFromRight (130).reduced (4, 18));
}
