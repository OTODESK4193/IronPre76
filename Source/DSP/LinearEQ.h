#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>

/**
 * @class LinearEQ
 * V76s Passive EQ (Phase 1-A: Advanced Static Analog Characteristics)
 * 1:30トランスの位相先行、パーマロイコアとPIOコンデンサの損失に起因する
 * "Lazy Slope"をカスタム双一次変換（パラレル・ミックス Biquad）で事前計算・キャッシュ。
 */
class LinearEQ
{
public:
    // HPF, LPFに加え、位相先行用のAll-pass Filter (APF) を追加した3段構成
    using IIRFilter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;

    LinearEQ()
    {
        hpfCoeffs = new juce::dsp::IIR::Coefficients<float>();
        lpfCoeffs = new juce::dsp::IIR::Coefficients<float>();
        apfCoeffs = new juce::dsp::IIR::Coefficients<float>();

        filterChain.template get<0>().state = hpfCoeffs;
        filterChain.template get<1>().state = lpfCoeffs;
        filterChain.template get<2>().state = apfCoeffs; // Phase Lead Transformer Model
    }

    ~LinearEQ() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        currentSampleRate = spec.sampleRate;
        precalculateCoefficients();
        updateParameters(4, 0, 0); // Default: 34dB, Bridged, Off
        filterChain.prepare(spec);
    }

    void process(juce::dsp::AudioBlock<float>& block)
    {
        juce::dsp::ProcessContextReplacing<float> context(block);
        filterChain.process(context);
    }

    void reset()
    {
        filterChain.reset();
    }

    void updateParameters(int gainIndex, int hpfIndex, int lpfIndex)
    {
        // ゲインとEQ設定の組み合わせから最適化された係数をコピー (オーディオスレッドでのゼロアロケーション)
        if (gainIndex >= 0 && gainIndex < 12)
        {
            if (hpfIndex >= 0 && hpfIndex < 4)
            {
                if (auto* cache = hpfCache[gainIndex][hpfIndex].get())
                    *hpfCoeffs = *cache;
            }

            if (lpfIndex >= 0 && lpfIndex < 5)
            {
                if (auto* cache = lpfCache[gainIndex][lpfIndex].get())
                    *lpfCoeffs = *cache;
            }

            // トランス起因のAPFはゲイン設定に依存して微小変化する想定でキャッシュから読み出し
            if (auto* cache = apfCache[gainIndex].get())
                *apfCoeffs = *cache;
        }
    }

private:
    // --- カスタム Lazy High Pass ---
    // カットオフ周波数を正確に保ちつつ、低域に指定したdBの「棚（フロア）」を作る
    juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>>
        makeLazyHighPass(double sampleRate, float frequency, float Q, float lazyFloorDb)
    {
        double w0 = juce::MathConstants<double>::twoPi * frequency / sampleRate;
        double cosw0 = std::cos(w0);
        double sinw0 = std::sin(w0);
        double alpha = sinw0 / (2.0 * Q);

        // フロア成分の線形ゲイン
        double F = std::pow(10.0, lazyFloorDb / 20.0);

        // 標準HPFの分子
        double b0_hp = (1.0 + cosw0) / 2.0;
        double b1_hp = -(1.0 + cosw0);
        double b2_hp = (1.0 + cosw0) / 2.0;

        // 標準LPFの分子
        double b0_lp = (1.0 - cosw0) / 2.0;
        double b1_lp = 1.0 - cosw0;
        double b2_lp = (1.0 - cosw0) / 2.0;

        // 分子をブレンド (HPF + 微小なLPF)
        double b0 = b0_hp + F * b0_lp;
        double b1 = b1_hp + F * b1_lp;
        double b2 = b2_hp + F * b2_lp;

        double a0 = 1.0 + alpha;
        double a1 = -2.0 * cosw0;
        double a2 = 1.0 - alpha;

        return new juce::dsp::IIR::Coefficients<float>(
            static_cast<float>(b0 / a0), static_cast<float>(b1 / a0), static_cast<float>(b2 / a0),
            static_cast<float>(a0 / a0), static_cast<float>(a1 / a0), static_cast<float>(a2 / a0));
    }

    // --- カスタム Lazy Low Pass ---
    // 同様にLPFの分子に、微小なHPFの分子をブレンドし、高域の棚落ちを作る
    juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>>
        makeLazyLowPass(double sampleRate, float frequency, float Q, float lazyFloorDb)
    {
        double w0 = juce::MathConstants<double>::twoPi * frequency / sampleRate;
        double cosw0 = std::cos(w0);
        double sinw0 = std::sin(w0);
        double alpha = sinw0 / (2.0 * Q);

        double F = std::pow(10.0, lazyFloorDb / 20.0);

        double b0_lp = (1.0 - cosw0) / 2.0;
        double b1_lp = 1.0 - cosw0;
        double b2_lp = (1.0 - cosw0) / 2.0;

        double b0_hp = (1.0 + cosw0) / 2.0;
        double b1_hp = -(1.0 + cosw0);
        double b2_hp = (1.0 + cosw0) / 2.0;

        // 分子をブレンド (LPF + 微小なHPF)
        double b0 = b0_lp + F * b0_hp;
        double b1 = b1_lp + F * b1_hp;
        double b2 = b2_hp + F * b2_hp;

        double a0 = 1.0 + alpha;
        double a1 = -2.0 * cosw0;
        double a2 = 1.0 - alpha;

        return new juce::dsp::IIR::Coefficients<float>(
            static_cast<float>(b0 / a0), static_cast<float>(b1 / a0), static_cast<float>(b2 / a0),
            static_cast<float>(a0 / a0), static_cast<float>(a1 / a0), static_cast<float>(a2 / a0));
    }

    void precalculateCoefficients()
    {
        for (int g = 0; g < 12; ++g)
        {
            // ゲイン設定によるNFB（負帰還）量の変化がQ値に与える影響
            // 高ゲイン（NFB小）ほどQ値が上昇し、不足制動（Underdamped）になる
            float nfbQShift = (g * 0.015f);

            // --- HPF: Bridged, 30, 60, 120 ---
            float hpfFreqs[] = { 5.0f, 31.5f, 62.2f, 124.0f }; // Bridgedは可聴域外の5Hzへ
            float hpfQs[] = { 0.707f, 0.58f, 0.52f, 0.48f };

            for (int h = 0; h < 4; ++h)
            {
                float finalQ = hpfQs[h] + nfbQShift;
                if (h == 0) {
                    // Bridged設定: 完全にフラットなDCカット
                    hpfCache[g][h] = juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, hpfFreqs[h], 0.707f);
                }
                else {
                    // Lazy HPF: -3dB点は正確にhpfFreqsになり、0Hzに向かって -24dB の棚を作る
                    hpfCache[g][h] = makeLazyHighPass(currentSampleRate, hpfFreqs[h], finalQ, -24.0f);
                }
            }

            // --- LPF: Off, 8k, 10k, 12k, 15k ---
            float lpfFreqs[] = { 22000.0f, 7850.0f, 9790.0f, 11680.0f, 14550.0f };
            float lpfQs[] = { 0.707f, 0.58f, 0.60f, 0.62f, 0.65f };
            double safeNyquist = (currentSampleRate * 0.5) * 0.95;

            for (int l = 0; l < 5; ++l)
            {
                float freq = static_cast<float>(juce::jmin(static_cast<double>(lpfFreqs[l]), safeNyquist));
                float finalQ = lpfQs[l] + nfbQShift;

                if (l == 0) {
                    // Off設定: 高域落ちを防ぐため、可聴域外で処理
                    lpfCache[g][l] = juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, freq, 0.707f);
                }
                else {
                    // Lazy LPF: -18dBの棚を持つアナログ特性
                    lpfCache[g][l] = makeLazyLowPass(currentSampleRate, freq, finalQ, -18.0f);
                }
            }

            // --- APF: トランスフォーマーの低域位相先行（Phase Lead） ---
            // 20Hz付近で約+18.5度の位相先行を作り出すため、中心周波数を低域に設定
            float apfFreq = 25.0f + (g * 0.5f);
            apfCache[g] = juce::dsp::IIR::Coefficients<float>::makeAllPass(currentSampleRate, apfFreq, 0.5f);
        }
    }

    double currentSampleRate = 44100.0;

    // 係数キャッシュ: [Gain(12)][Freq(4 or 5)]
    std::array<std::array<juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>>, 4>, 12> hpfCache;
    std::array<std::array<juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>>, 5>, 12> lpfCache;
    std::array<juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>>, 12> apfCache;

    juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>> hpfCoeffs;
    juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>> lpfCoeffs;
    juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>> apfCoeffs;

    // 3段構成へ拡張 (HPF -> LPF -> Transformer APF)
    juce::dsp::ProcessorChain<IIRFilter, IIRFilter, IIRFilter> filterChain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LinearEQ)
};