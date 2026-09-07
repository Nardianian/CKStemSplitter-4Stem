#include "PluginEditor.h"

CKStemSplitterAudioProcessorEditor::CKStemSplitterAudioProcessorEditor(CKStemSplitterAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), progressBar(progressValue)
{
    setSize(620, 480);

    titleLabel.setText("CK STEM SPLITTER", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(30.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("FAST VOCAL / INSTRUMENTAL SEPARATION", juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(13.0f));
    subtitleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(subtitleLabel);

    loadModelButton.onClick = [this] { chooseModelFile(); };
    addAndMakeVisible(loadModelButton);

    modelFileLabel.setText("No ONNX model selected", juce::dontSendNotification);
    modelFileLabel.setJustificationType(juce::Justification::centredLeft);
    modelFileLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey); 
    addAndMakeVisible(modelFileLabel); 

    loadButton.onClick = [this] { chooseAudioFile(); };
    addAndMakeVisible(loadButton);

    fileLabel.setText("No audio file selected", juce::dontSendNotification);
    fileLabel.setJustificationType(juce::Justification::centredLeft);
    fileLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(fileLabel);

    outputGainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 24);
    outputGainSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(outputGainSlider);

    outputGainLabel.setText("OUTPUT", juce::dontSendNotification);
    outputGainLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(outputGainLabel);

    analyzeButton.onClick = [this]
    {
        processor.getStemEngine().startSeparation();
    };
    addAndMakeVisible(analyzeButton);

    progressBar.setPercentageDisplay(true);
    addAndMakeVisible(progressBar);

    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(statusLabel);

    gainAttachment = std::make_unique<SliderAttachment>(processor.getAPVTS(), "outputGain", outputGainSlider);

    if (juce::JUCEApplicationBase::isStandaloneApp())
    {
        addAndMakeVisible(exportButton);
        exportButton.setEnabled(false);
        exportButton.onClick = [this] { exportStems(); };
    }

    startTimerHz(12);
}

void CKStemSplitterAudioProcessorEditor::chooseAudioFile()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Choose a song to split",
        juce::File{},
        "*.wav;*.mp3;*.flac;*.ogg;*.aif;*.aiff");

    const auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync(flags, [this](const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (file.existsAsFile())
        {
            processor.getStemEngine().setSourceFile(file);
            fileLabel.setText(file.getFileName(), juce::dontSendNotification);
        }
    });
}

void CKStemSplitterAudioProcessorEditor::chooseModelFile()
{
    modelChooser = std::make_unique<juce::FileChooser>(
        "Choose the Demucs ONNX model",
        juce::File{},
        "*.onnx");

    const auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    modelChooser->launchAsync(flags, [this](const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();
            if (file.existsAsFile())
            {
                processor.getStemEngine().setModelFile(file);
                modelFileLabel.setText(file.getFileName(), juce::dontSendNotification);
            }
        });
}   

void CKStemSplitterAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(16, 16, 20));

    auto panel = getLocalBounds().reduced(20).toFloat();
    g.setColour(juce::Colour::fromRGB(30, 30, 38));
    g.fillRoundedRectangle(panel, 14.0f);

    g.setColour(juce::Colour::fromRGB(210, 35, 45));
    g.fillRoundedRectangle(45.0f, 102.0f, 530.0f, 4.0f, 2.0f);
}

void CKStemSplitterAudioProcessorEditor::resized()
{
    titleLabel.setBounds(50, 35, 520, 42);
    subtitleLabel.setBounds(50, 75, 520, 22);

    loadModelButton.setBounds(55, 125, 145, 38);
    modelFileLabel.setBounds(215, 125, 350, 38);

    loadButton.setBounds(55, 175, 145, 38);
    fileLabel.setBounds(215, 175, 350, 38);

    outputGainLabel.setBounds(425, 222, 120, 22);
    outputGainSlider.setBounds(430, 245, 110, 110);

    if (juce::JUCEApplicationBase::isStandaloneApp())
    {
        analyzeButton.setBounds(55, 300, 145, 48);
        exportButton.setBounds(210, 300, 145, 48);
    }
    else
    {
        analyzeButton.setBounds(55, 300, 300, 48);
    }
    progressBar.setBounds(55, 370, 490, 22);
    statusLabel.setBounds(55, 410, 510, 28);
}

void CKStemSplitterAudioProcessorEditor::timerCallback()
{
    auto& engine = processor.getStemEngine();
    progressValue = engine.getProgress();
    statusLabel.setText(engine.getStatus(), juce::dontSendNotification);
    analyzeButton.setEnabled(!engine.isBusy() && engine.hasSourceFile());
    loadButton.setEnabled(!engine.isBusy());
    if (exportButton.isVisible())
        exportButton.setEnabled(processor.getStemEngine().areStemsReady());
}

void CKStemSplitterAudioProcessorEditor::exportStems()
{
    auto currentSource = processor.getStemEngine().getSourceFile();
    auto cacheDir = processor.getStemEngine().getCacheDirectoryForSource(currentSource);

    exportChooser = std::make_unique<juce::FileChooser>(
        "Select destination folder...",
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
        "");

    auto flags = juce::FileBrowserComponent::canSelectDirectories |
        juce::FileBrowserComponent::openMode;

    exportChooser->launchAsync(flags, [this, cacheDir](const juce::FileChooser& fc)
        {
            auto destDir = fc.getResult();

            if (destDir.isDirectory())
            {
                juce::StringArray stems = { "vocals.wav", "bass.wav", "drums.wav", "other.wav" };
                bool success = true;

                for (const auto& stem : stems)
                {
                    auto sourceFile = cacheDir.getChildFile(stem);
                    auto destFile = destDir.getChildFile(stem);

                    if (sourceFile.existsAsFile())
                    {

                        destFile.deleteFile();
                        if (!sourceFile.copyFileTo(destFile))
                        {
                            success = false;
                        }
                    }
                }

                if (success)
                {
                    juce::NativeMessageBox::showMessageBoxAsync(
                        juce::AlertWindow::InfoIcon,
                        "Export Successful",
                        "Stems saved in:\n" + destDir.getFullPathName());
                }
                else
                {
                    juce::NativeMessageBox::showMessageBoxAsync(
                        juce::AlertWindow::WarningIcon,
                        "Export Error",
                        "Something went wrong during export. Check folder permissions.");
                }
            }
        });
}
