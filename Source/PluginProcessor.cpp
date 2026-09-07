#include "PluginProcessor.h"
#include "PluginEditor.h"

CKStemSplitterAudioProcessor::CKStemSplitterAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Vocals", juce::AudioChannelSet::stereo(), true)
        .withOutput("Bass", juce::AudioChannelSet::stereo(), true)
        .withOutput("Drums", juce::AudioChannelSet::stereo(), true)
        .withOutput("Other", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout CKStemSplitterAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"outputGain", 1},
        "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f),
        0.0f,
        "dB"));

    return { params.begin(), params.end() };
}

void CKStemSplitterAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    stemEngine.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
}

void CKStemSplitterAudioProcessor::releaseResources()
{
    stemEngine.reset();
}

bool CKStemSplitterAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    for (int i = 0; i < 4; ++i)
    {
        if (layouts.getChannelSet(false, i) != juce::AudioChannelSet::stereo() &&
            layouts.getChannelSet(false, i) != juce::AudioChannelSet::disabled())
            return false;
    }
    return true;
}

void CKStemSplitterAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    juce::int64 hostSamplePosition = -1;
    if (auto* playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            if (auto samplePos = pos->getTimeInSamples())
                hostSamplePosition = *samplePos;
        }
    }

    stemEngine.process(buffer, hostSamplePosition);

    const auto* gainParam = apvts.getRawParameterValue("outputGain");
    const float gainDb = gainParam != nullptr ? gainParam->load() : 0.0f;
    buffer.applyGain(juce::Decibels::decibelsToGain(gainDb));
}

juce::AudioProcessorEditor* CKStemSplitterAudioProcessor::createEditor()
{
    return new CKStemSplitterAudioProcessorEditor(*this);
}

void CKStemSplitterAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    const auto source = stemEngine.getSourceFile();
    if (source.existsAsFile())
        state.setProperty("sourceFile", source.getFullPathName(), nullptr);

    const auto model = stemEngine.getModelFile();
    if (model.existsAsFile())
        state.setProperty("modelFile", model.getFullPathName(), nullptr);  

    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void CKStemSplitterAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(apvts.state.getType()))
        {
            auto restored = juce::ValueTree::fromXml(*xml);
            const auto sourcePath = restored.getProperty("sourceFile").toString();
            restored.removeProperty("sourceFile", nullptr);

            const auto modelPath = restored.getProperty("modelFile").toString();
            restored.removeProperty("modelFile", nullptr);

            apvts.replaceState(restored);

            if (sourcePath.isNotEmpty())
            {
                const juce::File source(sourcePath);
                if (source.existsAsFile())
                    stemEngine.setSourceFile(source);
            }

            if (modelPath.isNotEmpty())
            {
                const juce::File model(modelPath);
                if (model.existsAsFile())
                    stemEngine.setModelFile(model);
            }  
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CKStemSplitterAudioProcessor();
}
