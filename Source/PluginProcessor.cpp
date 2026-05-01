#include "PluginProcessor.h"
#include "PluginEditor.h"

IronPre76AudioProcessor::IronPre76AudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "Parameters", createParameterLayout()),
    oversampler(2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true)
{
    gainStepParam = apvts.getRawParameterValue("gain_step");
    hpfFreqParam = apvts.getRawParameterValue("hpf_freq");
    lpfFreqParam = apvts.getRawParameterValue("lpf_freq");
}

IronPre76AudioProcessor::~IronPre76AudioProcessor() {}

juce::AudioProcessorValueTreeState::ParameterLayout IronPre76AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    juce::StringArray gainSteps = { "3", "9", "18", "24", "34", "40", "46", "52", "58", "64", "70", "76" };
    layout.add(std::make_unique<juce::AudioParameterChoice>("gain_step", "Preamp Gain (dB)", gainSteps, 4));

    juce::StringArray hpfSteps = { "Bridged", "30", "60", "120" };
    layout.add(std::make_unique<juce::AudioParameterChoice>("hpf_freq", "HPF Freq (Hz)", hpfSteps, 0));

    juce::StringArray lpfSteps = { "Off", "8k", "10k", "12k", "15k" };
    layout.add(std::make_unique<juce::AudioParameterChoice>("lpf_freq", "LPF Freq (Hz)", lpfSteps, 0));
    return layout;
}

const juce::String IronPre76AudioProcessor::getName() const { return "IronPre76"; }
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

    linearEQ.prepare(spec);
    oversampler.initProcessing(samplesPerBlock);

    // 真空管モデルはオーバーサンプリングされた高いサンプルレートで動作させる
    juce::dsp::ProcessSpec osSpec = spec;
    osSpec.sampleRate = sampleRate * oversampler.getOversamplingFactor();
    tubeStageModel.prepare(osSpec);
}

void IronPre76AudioProcessor::releaseResources()
{
    linearEQ.reset();
    tubeStageModel.reset();
    oversampler.reset();
}

bool IronPre76AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono() ||
        layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void IronPre76AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // パラメータ取得
    int gIdx = static_cast<int>(gainStepParam->load());
    int hIdx = static_cast<int>(hpfFreqParam->load());
    int lIdx = static_cast<int>(lpfFreqParam->load());

    // 1. パラメータの更新
    linearEQ.updateParameters(gIdx, hIdx, lIdx);
    tubeStageModel.updateParameters(gIdx);

    juce::dsp::AudioBlock<float> block(buffer);

    // 2. パッシブEQネットワーク（アナログカーブ）の適用
    linearEQ.process(block);

    // 3. オーバーサンプリングによるエイリアシングノイズの抑制[cite: 2]
    auto upsampledBlock = oversampler.processSamplesUp(block);

    // 4. EF804S 真空管物理モデリングの適用 (Zero-Compromise)
    tubeStageModel.process(upsampledBlock);

    // 5. ダウンサンプリング
    oversampler.processSamplesDown(block);
}

bool IronPre76AudioProcessor::hasEditor() const { return true; }
juce::AudioProcessorEditor* IronPre76AudioProcessor::createEditor() { return new IronPre76AudioProcessorEditor(*this); }

void IronPre76AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void IronPre76AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new IronPre76AudioProcessor(); }