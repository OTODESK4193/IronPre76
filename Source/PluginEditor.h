#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class IronPre76AudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    IronPre76AudioProcessorEditor(IronPre76AudioProcessor&);
    ~IronPre76AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    IronPre76AudioProcessor& audioProcessor;

    // V76s ゲイン操作用のスライダー（ロータリーノブ）
    juce::Slider gainSlider;

    // APVTSとの安全な同期用アタッチメント
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IronPre76AudioProcessorEditor)
};