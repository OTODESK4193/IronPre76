#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

/**
 * IronPre76AudioProcessorEditor
 * V76sの物理的な操作感を再現する3つのロータリーノブを備えたGUIクラス。
 */
class IronPre76AudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    IronPre76AudioProcessorEditor(IronPre76AudioProcessor&);
    ~IronPre76AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    IronPre76AudioProcessor& audioProcessor;

    // --- UI Components ---
    // メインゲインノブ
    juce::Slider gainSlider;
    // ハイパスフィルター切り替えノブ
    juce::Slider hpfSlider;
    // ローパスフィルター切り替えノブ
    juce::Slider lpfSlider;

    // --- Attachments ---
    // APVTSのパラメータと安全に同期するためのアタッチメント
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hpfAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lpfAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IronPre76AudioProcessorEditor)
};