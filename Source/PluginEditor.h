#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class CKStemSplitterAudioProcessorEditor : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit CKStemSplitterAudioProcessorEditor(CKStemSplitterAudioProcessor&);
    ~CKStemSplitterAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void chooseAudioFile();
    void chooseModelFile();

    CKStemSplitterAudioProcessor& processor;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::TextButton loadModelButton{ "LOAD MODEL" };
    juce::Label modelFileLabel;
    std::unique_ptr<juce::FileChooser> modelChooser;   
    juce::TextButton loadButton { "LOAD AUDIO" };
    juce::Label fileLabel;
    juce::Slider outputGainSlider;
    juce::Label outputGainLabel;
    juce::TextButton analyzeButton { "SPLIT STEMS" };
    juce::ProgressBar progressBar;
    double progressValue = 0.0;
    juce::Label statusLabel;

    std::unique_ptr<juce::FileChooser> fileChooser;

    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> gainAttachment;

    juce::TextButton exportButton{ "EXPORT STEMS" };
    std::unique_ptr<juce::FileChooser> exportChooser;

    void exportStems();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CKStemSplitterAudioProcessorEditor)
};
