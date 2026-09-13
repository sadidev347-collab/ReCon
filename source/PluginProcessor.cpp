#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr auto intensityId = "intensity";
constexpr auto mixId = "mix";
constexpr auto impulseResponsePath = "impulseResponsePath";
}

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
      apvts (*this, nullptr, "ReConState", createParameterLayout())
{
}

PluginProcessor::~PluginProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const auto percentageAttributes = []
    {
        return juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float value, int)
                                          { return juce::String (juce::roundToInt (value * 100.0f)) + "%"; })
            .withValueFromStringFunction ([] (const juce::String& text)
                                          { return text.getFloatValue() / 100.0f; });
    };

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        intensityId, "Reverb Intensity", juce::NormalisableRange<float> (0.0f, 1.0f), 0.75f,
        percentageAttributes()));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        mixId, "Dry / Wet Mix", juce::NormalisableRange<float> (0.0f, 1.0f), 0.35f,
        percentageAttributes()));

    return layout;
}

const juce::String PluginProcessor::getName() const { return JucePlugin_Name; }

bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    return currentSampleRate > 0.0
               ? static_cast<double> (convolution.getCurrentIRSize()) / currentSampleRate
               : 0.0;
}

int PluginProcessor::getNumPrograms() { return 1; }
int PluginProcessor::getCurrentProgram() { return 0; }
void PluginProcessor::setCurrentProgram (int index) { juce::ignoreUnused (index); }

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return "Default";
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    preparedBlockSize = samplesPerBlock;
    dryBuffer.setSize (2, samplesPerBlock, false, false, true);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels = 2;
    convolution.prepare (spec);
    convolution.reset();

    if (impulseResponseFile.existsAsFile())
        loadImpulseResponse (impulseResponseFile);
}

void PluginProcessor::releaseResources()
{
    convolution.reset();
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
   #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
   #else
    const auto output = layouts.getMainOutputChannelSet();
    if (output != juce::AudioChannelSet::mono() && output != juce::AudioChannelSet::stereo())
        return false;

    #if ! JucePlugin_IsSynth
    if (output != layouts.getMainInputChannelSet())
        return false;
    #endif
    return true;
   #endif
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = juce::jmin (2, buffer.getNumChannels());
    if (numChannels == 0 || numSamples == 0)
        return;

    if (numSamples > preparedBlockSize)
    {
        buffer.clear();
        return;
    }

    for (int channel = 0; channel < numChannels; ++channel)
        dryBuffer.copyFrom (channel, 0, buffer, channel, 0, numSamples);

    juce::dsp::AudioBlock<float> block (buffer);
    convolution.process (juce::dsp::ProcessContextReplacing<float> (block));

    const auto intensity = apvts.getRawParameterValue (intensityId)->load();
    const auto mix = apvts.getRawParameterValue (mixId)->load();
    const auto wetGain = intensity * mix;
    const auto dryGain = 1.0f - mix;

    buffer.applyGain (wetGain);
    for (int channel = 0; channel < numChannels; ++channel)
        buffer.addFrom (channel, 0, dryBuffer, channel, 0, numSamples, dryGain);
}

bool PluginProcessor::loadImpulseResponse (const juce::File& file)
{
    if (! file.existsAsFile() || file.getFileExtension().toLowerCase() != ".wav")
        return false;

    impulseResponseFile = file;
    convolution.loadImpulseResponse (file,
                                     juce::dsp::Convolution::Stereo::yes,
                                     juce::dsp::Convolution::Trim::yes,
                                     0,
                                     juce::dsp::Convolution::Normalise::yes);
    apvts.state.setProperty (impulseResponsePath, file.getFullPathName(), nullptr);
    return true;
}

juce::String PluginProcessor::getImpulseResponseName() const
{
    return impulseResponseFile.existsAsFile() ? impulseResponseFile.getFileName()
                                              : "No impulse response loaded";
}

bool PluginProcessor::savePreset (const juce::File& file)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    return xml != nullptr && xml->writeTo (file);
}

bool PluginProcessor::loadPreset (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr)
        return false;

    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    const auto path = apvts.state.getProperty (impulseResponsePath).toString();
    return path.isEmpty() || loadImpulseResponse (juce::File (path));
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    std::unique_ptr<juce::XmlElement> xml (apvts.copyState().createXml());
    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr)
        return;

    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    const auto path = apvts.state.getProperty (impulseResponsePath).toString();
    if (path.isNotEmpty())
        loadImpulseResponse (juce::File (path));
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

bool PluginProcessor::hasEditor() const { return true; }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
