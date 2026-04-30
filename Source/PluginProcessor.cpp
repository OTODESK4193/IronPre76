#include "PluginProcessor.h"
#include "PluginEditor.h"

IronPre76AudioProcessor::IronPre76AudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    ),
#endif
    apvts(*this, nullptr, "Parameters", createParameterLayout()),
    oversampler(2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true)
{
    // パラメータポインタの初期化（processBlockでの高速アクセス用）
    gainStepParam = apvts.getRawParameterValue("gain_step");
}

IronPre76AudioProcessor::~IronPre76AudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout IronPre76AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // V76sの12ステップ・ゲイン設定
    juce::StringArray gainSteps = { "3", "9", "18", "24", "34", "40", "46", "52", "58", "64", "70", "76" };

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "gain_step",
        "Preamp Gain (dB)",
        gainSteps,
        4 // デフォルト: 34dB (インデックス4)
    ));

    return layout;
}

const juce::String IronPre76AudioProcessor::getName() const { return JucePlugin_Name; }
bool IronPre76AudioProcessor::acceptsMidi() const { return false; }
bool IronPre76AudioProcessor::producesMidi() const { return false; }
bool IronPre76AudioProcessor::isMidiEffect() const { return false; }
double IronPre76AudioProcessor::getTailLengthSeconds() const { return 0.0; }
int IronPre76AudioProcessor::getNumPrograms() { return 1; }
int IronPre76AudioProcessor::getCurrentProgram() { return 0; }
void IronPre76AudioProcessor::setCurrentProgram(int index) {}
const juce::String IronPre76AudioProcessor::getProgramName(int index) { return {}; }
void IronPre76AudioProcessor::changeProgramName(int index, const juce::String& newName) {}

void IronPre76AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();

    // 各DSPコンポーネントの初期化 (動的メモリ確保はここで行う)
    linearEQ.prepare(spec);
    oversampler.initProcessing(samplesPerBlock);
}

void IronPre76AudioProcessor::releaseResources()
{
    linearEQ.reset();
    oversampler.reset();
}

bool IronPre76AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void IronPre76AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // ロックフリーなパラメータ取得 (0 〜 11 のインデックス値が返る)
    int currentGainIndex = static_cast<int>(gainStepParam->load());

    // --- Audio Processing Logic ---
    juce::dsp::AudioBlock<float> block(buffer);

    // 1. リニア処理段 (オーバーサンプリング前に実行しCPU負荷を最小化)
    linearEQ.process(block);

    // 2. オーバーサンプリング (非線形処理の前に実行)
    auto upsampledBlock = oversampler.processSamplesUp(block);

    // 3. 非線形処理
    // TODO: ここに各DSPステージ(真空管・トランス)のprocessを実装

    // 4. ダウンサンプリング
    oversampler.processSamplesDown(block);
}

bool IronPre76AudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* IronPre76AudioProcessor::createEditor()
{
    return new IronPre76AudioProcessorEditor(*this);
}

void IronPre76AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void IronPre76AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new IronPre76AudioProcessor(); }