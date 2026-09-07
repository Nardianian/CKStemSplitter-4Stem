#include "StemEngine.h"

StemEngine::StemEngine()
{
    readAheadThread.startThread();
}

StemEngine::~StemEngine()
{
    shouldStop.store(true);
    if (workerThread.joinable())
        workerThread.join();

    stemsReady.store(false);
    {
        std::scoped_lock lock(stateMutex);
        clearStemSources();
    }
    readAheadThread.stopThread(2000);
}

void StemEngine::prepare(double sampleRate, int samplesPerBlock, int channels)
{
    currentSampleRate = sampleRate;
    currentBlockSize = samplesPerBlock;
    currentChannels = channels;

    std::scoped_lock lock(stateMutex);
    vocalsTransport.prepareToPlay(samplesPerBlock, sampleRate);
    bassTransport.prepareToPlay(samplesPerBlock, sampleRate);
    drumsTransport.prepareToPlay(samplesPerBlock, sampleRate);
    otherTransport.prepareToPlay(samplesPerBlock, sampleRate);  
}

void StemEngine::reset()
{
    // Keep cached stems and transport sources ready between playback starts/stops.
}

void StemEngine::clearStemSources()
{
    vocalsTransport.stop(); bassTransport.stop(); drumsTransport.stop(); otherTransport.stop();
    vocalsTransport.setSource(nullptr); bassTransport.setSource(nullptr);
    drumsTransport.setSource(nullptr); otherTransport.setSource(nullptr);

    vocalsReaderSource.reset(); bassReaderSource.reset();
    drumsReaderSource.reset(); otherReaderSource.reset();
}

void StemEngine::setSourceFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return;

    stemsReady.store(false);
    std::scoped_lock lock(stateMutex);
    sourceFile = file;
    progress.store(0.0f);
    clearStemSources();
    status = "Ready to split: " + file.getFileName();
}

juce::File StemEngine::getSourceFile() const
{
    std::scoped_lock lock(stateMutex);
    return sourceFile;
}

bool StemEngine::hasSourceFile() const
{
    std::scoped_lock lock(stateMutex);
    return sourceFile.existsAsFile();
}

void StemEngine::setModelFile(const juce::File& file)
{
    std::scoped_lock lock(stateMutex);
    modelFile = file;
}

juce::File StemEngine::getModelFile() const
{
    std::scoped_lock lock(stateMutex);
    return modelFile;
}

bool StemEngine::hasModelFile() const
{
    std::scoped_lock lock(stateMutex);
    return modelFile.existsAsFile();
}  

juce::File StemEngine::getEngineExecutable() const
{
    auto base = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
        .getChildFile("Commercial Kings")
        .getChildFile("CK Stem Splitter")
        .getChildFile("engine");

#if JUCE_WINDOWS
    return base.getChildFile("ckstem-engine.exe");
#else
    return base.getChildFile("ckstem-engine");
#endif
}

juce::File StemEngine::getModelCacheDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory)
        .getChildFile("Commercial Kings")
        .getChildFile("CK Stem Splitter")
        .getChildFile("engine")
        .getChildFile("models");
}

juce::File StemEngine::getCacheDirectoryForSource(const juce::File& source) const
{
    const auto keyText = source.getFullPathName() + "|" + juce::String(source.getSize()) + "|" + juce::String(source.getLastModificationTime().toMilliseconds());
    const auto key = juce::String::toHexString(static_cast<juce::int64>(keyText.hashCode64()));

    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Commercial Kings")
        .getChildFile("CK Stem Splitter")
        .getChildFile("Cache")
        .getChildFile(key);
}

void StemEngine::startSeparation()
{
    if (busy.exchange(true))
        return;

    if (!hasSourceFile())
    {
        busy.store(false);
        std::scoped_lock lock(stateMutex);
        status = "Choose an audio file first";
        return;
    }

    shouldStop.store(false);
    progress.store(0.02f);
    stemsReady.store(false);

    if (workerThread.joinable())
        workerThread.join();

    workerThread = std::thread([this] { separationWorker(); });
}

void StemEngine::separationWorker()
{
    juce::File localSource;
    {
        std::scoped_lock lock(stateMutex);
        localSource = sourceFile;
        status = "Preparing AI separation...";
    }

    const auto cacheDir = getCacheDirectoryForSource(localSource);
    cacheDir.createDirectory();
    const auto vocals = cacheDir.getChildFile("vocals.wav");
    const auto bass = cacheDir.getChildFile("bass.wav");
    const auto drums = cacheDir.getChildFile("drums.wav");
    const auto other = cacheDir.getChildFile("other.wav");

    if (vocals.existsAsFile() && bass.existsAsFile() && drums.existsAsFile() && other.existsAsFile())
    {
        progress.store(0.9f);
        if (loadCachedStems(vocals, bass, drums, other)) progress.store(1.0f);
        busy.store(false);
        return;
    }

    const auto engine = getEngineExecutable();
    if (!engine.existsAsFile())
    {
        std::scoped_lock lock(stateMutex);
        status = "AI engine is missing - reinstall CK Stem Splitter";
        busy.store(false);
        return;
    }

    {
        std::scoped_lock lock(stateMutex);
        status = "Separating vocals, bass, drums, other ...";
    }
    progress.store(0.1f);

    juce::ChildProcess process;
    juce::StringArray args;
    args.add(engine.getFullPathName());
    args.add("separate");
    args.add(localSource.getFullPathName());
    args.add(cacheDir.getFullPathName());
    args.add("--model");
    args.add(modelFile.getFileNameWithoutExtension());
    args.add("--providers");
    args.add("auto");
    args.add("--cache-dir");
    args.add(modelFile.getParentDirectory().getFullPathName());
    args.add("--shifts");
    args.add("2");
    args.add("--verbose");

    if (!process.start(args))
    {
        std::scoped_lock lock(stateMutex);
        status = "Could not start the AI engine";
        busy.store(false);
        return;
    }

    juce::String pending;
    while (process.isRunning() && !shouldStop.load())
    {
        pending += process.readAllProcessOutput();
        auto lines = juce::StringArray::fromLines(pending);
        if (!pending.endsWithChar('\n') && lines.size() > 0)
        {
            pending = lines[lines.size() - 1];
            lines.remove(lines.size() - 1);
        }
        else
        {
            pending.clear();
        }

        for (const auto& line : lines)
        {
            if (line.containsIgnoreCase("download"))
            {
                std::scoped_lock lock(stateMutex);
                status = "Preparing bundled AI model...";
                progress.store(0.15f);
            }
            else if (line.containsIgnoreCase("separat") || line.containsIgnoreCase("segment"))
            {
                std::scoped_lock lock(stateMutex);
                status = "Separating vocals, bass, drums, other...";
                progress.store(0.55f);
            }
            else if (line.containsIgnoreCase("writ") || line.containsIgnoreCase("save"))
            {
                progress.store(0.88f);
            }
        }
        juce::Thread::sleep(75);
    }

    if (shouldStop.load())
    {
        process.kill();
        busy.store(false);
        return;
    }

    const auto exitCode = process.getExitCode();

    if (exitCode != 0 || !vocals.existsAsFile() || !bass.existsAsFile() || !drums.existsAsFile() || !other.existsAsFile())
    {
        std::scoped_lock lock(stateMutex);
        status = "Stem separation failed (engine code " + juce::String(exitCode) + ")";
        busy.store(false);
        return;
    }

    if (loadCachedStems(vocals, bass, drums, other))
        progress.store(1.0f);
    busy.store(false);
}

bool StemEngine::loadCachedStems(const juce::File& vocalsFile, const juce::File& bassFile, const juce::File& drumsFile, const juce::File& otherFile)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    auto vocalReader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(vocalsFile));
    auto bassReader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(bassFile));
    auto drumsReader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(drumsFile));
    auto otherReader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(otherFile));

    if (vocalReader == nullptr || bassReader == nullptr || drumsReader == nullptr || otherReader == nullptr)
    {
        std::scoped_lock lock(stateMutex);
        status = "Could not read separated WAV files";
        return false;
    }

    const auto vocalRate = vocalReader->sampleRate;
    const auto bassRate = bassReader->sampleRate;
    const auto drumsRate = drumsReader->sampleRate;
    const auto otherRate = otherReader->sampleRate;
    if (std::abs(vocalRate - bassRate) > 1.0 || std::abs(vocalRate - drumsRate) > 1.0 || std::abs(vocalRate - otherRate) > 1.0)
    {
        std::scoped_lock lock(stateMutex);
        status = "Separated stems have mismatched sample rates";
        return false;
    }

    auto newVocals = std::make_unique<juce::AudioFormatReaderSource>(vocalReader.release(), true);
    auto newBass = std::make_unique<juce::AudioFormatReaderSource>(bassReader.release(), true);
    auto newDrums = std::make_unique<juce::AudioFormatReaderSource>(drumsReader.release(), true);
    auto newOther = std::make_unique<juce::AudioFormatReaderSource>(otherReader.release(), true);

    stemsReady.store(false);
    {
        std::scoped_lock lock(stateMutex);
        clearStemSources();
        vocalsReaderSource = std::move(newVocals);
        bassReaderSource = std::move(newBass);
        drumsReaderSource = std::move(newDrums);
        otherReaderSource = std::move(newOther);
        stemSampleRate = vocalRate;

        constexpr int readAheadSamples = 262144;
        vocalsTransport.setSource(vocalsReaderSource.get(), readAheadSamples, &readAheadThread, stemSampleRate, 2);
        bassTransport.setSource(bassReaderSource.get(), readAheadSamples, &readAheadThread, stemSampleRate, 2);
        drumsTransport.setSource(drumsReaderSource.get(), readAheadSamples, &readAheadThread, stemSampleRate, 2);
        otherTransport.setSource(otherReaderSource.get(), readAheadSamples, &readAheadThread, stemSampleRate, 2);
        vocalsTransport.prepareToPlay(currentBlockSize, currentSampleRate);
        bassTransport.prepareToPlay(currentBlockSize, currentSampleRate);
        drumsTransport.prepareToPlay(currentBlockSize, currentSampleRate);
        otherTransport.prepareToPlay(currentBlockSize, currentSampleRate);
        vocalsTransport.start();
        bassTransport.start();
        drumsTransport.start();
        otherTransport.start();
        status = "Stems ready- Route outputs and press Play in DAW";
    }

    stemsReady.store(true);
    return true;
}

void StemEngine::process(juce::AudioBuffer<float>& buffer, juce::int64 hostSamplePosition)
{
    if (!stemsReady.load() || hostSamplePosition < 0 || currentSampleRate <= 0.0)
        return;

    std::unique_lock<std::mutex> lock(stateMutex, std::try_to_lock);
    if (!lock.owns_lock())
        return;

    const double targetSeconds = static_cast<double>(hostSamplePosition) / currentSampleRate;
    const double blockSeconds = static_cast<double>(buffer.getNumSamples()) / currentSampleRate;

    // Lambda Function to process the stem on his stereo channels
    auto processStem = [&](juce::AudioTransportSource& transport, int startChannel)
        {
            // Control DAW output channel enabled
            if (startChannel + 1 < buffer.getNumChannels())
            {
                const double driftSeconds = std::abs(transport.getCurrentPosition() - targetSeconds);
                if (driftSeconds > juce::jmax(0.050, blockSeconds * 4.0))
                    transport.setPosition(targetSeconds);

                juce::AudioBuffer<float> busBuffer(buffer.getArrayOfWritePointers() + startChannel, 2, buffer.getNumSamples());
                juce::AudioSourceChannelInfo info(&busBuffer, 0, busBuffer.getNumSamples());
                transport.getNextAudioBlock(info);
            }
        };

    // Routing to four DAW stereo output
    processStem(vocalsTransport, 0); // Uscita 1: Canali 0, 1 (Vocals)
    processStem(bassTransport, 2);   // Uscita 2: Canali 2, 3 (Bass)
    processStem(drumsTransport, 4);  // Uscita 3: Canali 4, 5 (Drums)
    processStem(otherTransport, 6);  // Uscita 4: Canali 6, 7 (Other)
}

juce::String StemEngine::getStatus() const
{
    std::scoped_lock lock(stateMutex);
    return status;
}
