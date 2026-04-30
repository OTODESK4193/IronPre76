#pragma once
#include <JuceHeader.h>

/**
 * @class LinearEQ
 * V76sの基本となるリニアな周波数応答(20Hz - 20kHz)を管理するクラス。
 * 動的メモリ確保を防ぐため、ProcessorChainを利用してインライン処理を行います。
 */
class LinearEQ
{
public:
    // ステレオ処理用にプロセッサをラップするエイリアスを定義
    using IIRFilter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;

    LinearEQ()
    {
        // コンストラクタで係数オブジェクトを事前確保（DSPセーフ: processBlock内でのnewを排除）
        hpfCoeffs = new juce::dsp::IIR::Coefficients<float>();
        lpfCoeffs = new juce::dsp::IIR::Coefficients<float>();

        // Duplicatorのstateポインタに事前に割り当てておく
        // これにより、prepare呼び出し時に内部フィルタへnullがコピーされるのを防ぐ
        filterChain.template get<0>().state = hpfCoeffs;
        filterChain.template get<1>().state = lpfCoeffs;
    }

    ~LinearEQ() {}

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        // 1. 係数の実数値を先に計算・更新する
        updateCoefficients(spec.sampleRate);

        // 2. 有効な係数が割り当てられた状態でチェーンのprepareを呼ぶ
        filterChain.prepare(spec);
    }

    void process(juce::dsp::AudioBlock<float>& block)
    {
        // メモリ確保が発生しない安全なコンテキストラッパー
        juce::dsp::ProcessContextReplacing<float> context(block);
        filterChain.process(context);
    }

    void reset()
    {
        filterChain.reset();
    }

private:
    void updateCoefficients(double sampleRate)
    {
        // ナイキスト周波数の保護 (サンプルレートの半分 x 0.95)
        // 32kHzや44.1kHz環境で20kHzのLPFを指定した際のNaN発生・無音化を防ぐ
        double safeNyquist = (sampleRate * 0.5) * 0.95;
        float lpfFreq = static_cast<float>(juce::jmin(20000.0, safeNyquist));

        // ポインタを差し替えるのではなく、値渡し（インプレース更新）で係数を上書きする
        *hpfCoeffs = *juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 20.0f, 0.707f);
        *lpfCoeffs = *juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, lpfFreq, 0.707f);
    }

    // 係数の実体を保持するスマートポインタ
    juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>> hpfCoeffs;
    juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>> lpfCoeffs;

    // 2つのIIRフィルタ(ステレオ対応)を直列化するプロセッサチェーン
    juce::dsp::ProcessorChain<
        IIRFilter, // 20Hz HPF
        IIRFilter  // 20kHz LPF
    > filterChain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LinearEQ)
};