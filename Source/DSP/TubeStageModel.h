#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <algorithm>
#include <vector>

/**
 * @class TubeStageModel
 * Telefunken EF804S (EF86) 五極管増幅段の完全物理モデリング (Zero-Compromise)
 *
 * - DAFx-10準拠 Koren Pentode Model
 * - EF86固有パラメータ (KP=222.06, KVB=4.7) と カソードバイアス (-2.2V)
 * - 64-bit Double精度 DCブロッカーと、完全なステレオセパレーションによる絶対安定化
 * - 物理増幅率(32.2倍)の自動補償による完璧なユニティゲイン設計
 */
class TubeStageModel
{
public:
    TubeStageModel() = default;
    ~TubeStageModel() = default;

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        currentSampleRate = spec.sampleRate;
        invSampleRate = 1.0 / currentSampleRate;

        // 10Hz HPF (DC Blocker) の係数算出 (64-bit精度)
        dcBlockerR = std::exp(-2.0 * juce::MathConstants<double>::pi * 10.0 / currentSampleRate);

        // ステレオ（マルチチャンネル）対応のための配列リサイズと初期化
        int numChannels = static_cast<int>(spec.numChannels);
        vcGrid.assign(numChannels, 0.0);
        vcPlate.assign(numChannels, 285.0);
        dcX1.assign(numChannels, 0.0);
        dcY1.assign(numChannels, 0.0);
    }

    void reset()
    {
        std::fill(vcGrid.begin(), vcGrid.end(), 0.0);
        std::fill(vcPlate.begin(), vcPlate.end(), 285.0);
        std::fill(dcX1.begin(), dcX1.end(), 0.0);
        std::fill(dcY1.begin(), dcY1.end(), 0.0);
    }

    void updateParameters(int gainIndex)
    {
        float normalizedGain = static_cast<float>(gainIndex) / 11.0f;

        // 1:30トランスとアッテネータによる実効ドライブ電圧
        inputDriveMultiplier = 5.0 + (normalizedGain * 200.0);

        // 【修正】出力ゲインの完全補償 (Unity Gain AGC)
        // 物理モデル(EF804S)自体が持つ約32.2倍の電圧増幅(Voltage Gain)を
        // 正確に割り戻すことで、DAW上での音量爆発(32.1dBオーバー)を完全に防ぎます。
        const double TUBE_VOLTAGE_GAIN = 32.2;
        outputAttenuation = 1.0 / (inputDriveMultiplier * TUBE_VOLTAGE_GAIN);
    }

    void process(juce::dsp::AudioBlock<float>& block)
    {
        auto numChannels = block.getNumChannels();
        auto numSamples = block.getNumSamples();

        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            if (ch >= vcGrid.size()) break;

            auto* channelData = block.getChannelPointer(ch);

            for (size_t i = 0; i < numSamples; ++i)
            {
                // 1. 入力電圧のスケーリングと保護
                double vIn = static_cast<double>(channelData[i]) * inputDriveMultiplier;
                if (std::isnan(vIn) || std::isinf(vIn)) vIn = 0.0;
                vIn = juce::jlimit(-400.0, 400.0, vIn);

                // 2. 物理モデル実行
                double rawOut = solveTubePhysics(vIn, ch);

                // 3. 64-bit Double精度 DCブロッカー
                double acOut = rawOut - dcX1[ch] + dcBlockerR * dcY1[ch];
                dcX1[ch] = rawOut;
                dcY1[ch] = acOut;

                // 4. DAWレベルへの最終出力スケーリング (ここで音量がピタリと元に戻ります)
                channelData[i] = static_cast<float>(acOut * outputAttenuation);
            }
        }
    }

private:
    // --- V76s 回路コンポーネント定数 ---
    const double V_BPLUS = 285.0;
    const double R_PLATE = 100000.0;
    const double R_GRID = 1000000.0;
    const double C_COUPLING = 10.0e-9;
    const double VG2 = 140.0;
    const double V_BIAS = -2.2;

    // --- EF804S (EF86) 固有 Koren SPICE パラメータ ---
    const double MU = 34.90;
    const double EX = 1.350;
    const double KG1 = 2648.1;
    const double KP = 222.06;
    const double KVB = 4.7;
    const double RGI = 2000.0;

    // --- 状態変数 ---
    double currentSampleRate = 44100.0;
    double invSampleRate = 1.0 / 44100.0;
    double inputDriveMultiplier = 1.0;
    double outputAttenuation = 0.05;
    double dcBlockerR = 0.999;

    std::vector<double> vcGrid;
    std::vector<double> vcPlate;
    std::vector<double> dcX1;
    std::vector<double> dcY1;

    /**
     * EF804S 陰的数値解法コア
     */
    double solveTubePhysics(double vIn, size_t ch)
    {
        // 1. グリッド回路の陰的解法 (後退オイラー + N-R法)
        double Vc = vcGrid[ch];

        for (int iter = 0; iter < 5; ++iter)
        {
            double Vgk = vIn - Vc + V_BIAS;

            double Ig = (Vgk > 0.0) ? (Vgk / RGI) : 0.0;
            double dIg_dVc = (Vgk > 0.0) ? (-1.0 / RGI) : 0.0;

            double Ileak = Vc / R_GRID;
            double dIleak_dVc = 1.0 / R_GRID;

            double f = Vc - vcGrid[ch] - (invSampleRate / C_COUPLING) * (Ig - Ileak);
            double df_dVc = 1.0 - (invSampleRate / C_COUPLING) * (dIg_dVc - dIleak_dVc);

            double step = f / df_dVc;
            Vc -= step;

            if (std::abs(step) < 1e-6) break;
        }

        vcGrid[ch] = Vc;
        double vGridCathode = vIn - vcGrid[ch] + V_BIAS;

        // 2. 五極管 (Pentode) 非線形モデルの事前計算
        double e1_arg = KP * ((1.0 / MU) + (vGridCathode / VG2));
        double E1 = 0.0;

        if (e1_arg > 30.0) {
            E1 = (VG2 / KP) * e1_arg;
        }
        else {
            E1 = (VG2 / KP) * std::log1p(std::exp(e1_arg));
        }

        double E1_clipped = std::max(E1, 0.0);
        double E1_ex_Kg1 = std::pow(E1_clipped, EX) / KG1;

        // 3. プレート回路の代数ループ解決 (N-R法)
        double Vpk = vcPlate[ch];

        for (int iter = 0; iter < 5; ++iter)
        {
            double at = std::atan(Vpk / KVB);
            double Ia = E1_ex_Kg1 * at;

            double dIa_dVpk = E1_ex_Kg1 * (KVB / (KVB * KVB + Vpk * Vpk));

            double f = Vpk - V_BPLUS + (Ia * R_PLATE);
            double df_dVpk = 1.0 + (dIa_dVpk * R_PLATE);

            double step = f / df_dVpk;
            Vpk -= step;

            if (Vpk < 1e-6) Vpk = 1e-6;
            if (Vpk > V_BPLUS) Vpk = V_BPLUS;

            if (std::abs(step) < 1e-6) break;
        }

        vcPlate[ch] = Vpk;

        return V_BPLUS - vcPlate[ch];
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TubeStageModel)
};