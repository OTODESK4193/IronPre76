#pragma once

#include <JuceHeader.h>
#include "DSP/TransformerModel.h"
#include "DSP/TubeStageModel.h"
#include "DSP/LinearEQ.h"

/**
 * IronPre76AudioProcessor
 * Telefunken V76sのアナログモデリング・コア・プロセッサ
 */
class IronPre76AudioProcessor : public juce::AudioProcessor
{
public:
    IronPre76AudioProcessor();
    ~IronPre76AudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // パラメータ管理用のAPVTS
    juce::AudioProcessorValueTreeState apvts;

private:
    // パラメータレイアウトの生成
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ゲインステップのインデックス取得用ポインタ（ロックフリーで安全にアクセス）
    std::atomic<float>* gainStepParam = nullptr;

    // --- DSP Components ---
    // リニアEQ (基礎帯域の形成)
    LinearEQ linearEQ;

    // オーバーサンプリング (非線形処理のエイリアシング対策)
    juce::dsp::Oversampling<float> oversampler;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IronPre76AudioProcessor)
};