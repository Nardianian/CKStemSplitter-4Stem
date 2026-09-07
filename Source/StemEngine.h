#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <mutex>
#include <thread>

class StemEngine
{
public:
    enum class StemMode
    {
        original = 0,
        vocals = 1,
        bass = 2,
        drums = 3,
        other = 4
    };

    StemEngine();
    ~StemEngine();

    void prepare(double sampleRate, int samplesPerBlock, int channels);
    void reset();

    void process(juce::AudioBuffer<float>& buffer, juce::int64 hostSamplePosition);

    void setSourceFile(const juce::File& file);
    juce::File getSourceFile() const;
    bool hasSourceFile() const;
    bool areStemsReady() const { return stemsReady.load(); }

    void setModelFile(const juce::File& file);
    juce::File getModelFile() const;
    bool hasModelFile() const;

    void startSeparation();
    bool isBusy() const noexcept { return busy.load(); }
    bool hasSeparatedStems() const noexcept { return stemsReady.load(); }
    float getProgress() const noexcept { return progress.load(); }

    juce::String getStatus() const;
    juce::File getCacheDirectoryForSource(const juce::File&) const;

private:
    juce::File getEngineExecutable() const;
    juce::File getModelCacheDirectory() const;
    void separationWorker();
    bool loadCachedStems(const juce::File& vocalsFile, const juce::File& bassFile, const juce::File& drumsFile, const juce::File& otherFile);
    void clearStemSources();

    mutable std::mutex stateMutex;
    juce::File sourceFile;

    juce::TimeSliceThread readAheadThread { "CK Stem Splitter Read Ahead" };
    std::unique_ptr<juce::AudioFormatReaderSource> vocalsReaderSource;
    std::unique_ptr<juce::AudioFormatReaderSource> bassReaderSource;
    std::unique_ptr<juce::AudioFormatReaderSource> drumsReaderSource;
    std::unique_ptr<juce::AudioFormatReaderSource> otherReaderSource;

    juce::AudioTransportSource vocalsTransport;
    juce::AudioTransportSource bassTransport;
    juce::AudioTransportSource drumsTransport;
    juce::AudioTransportSource otherTransport;

    double stemSampleRate = 44100.0;
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    int currentChannels = 2;

    std::atomic<bool> busy { false };
    std::atomic<bool> stemsReady { false };
    std::atomic<float> progress { 0.0f };
    std::atomic<bool> shouldStop { false };
    std::thread workerThread;
    juce::String status { "Choose a song to split" };
    juce::File modelFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemEngine)
};

