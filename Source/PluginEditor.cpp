#include "PluginProcessor.h"
#include "PluginEditor.h"

IronPre76AudioProcessorEditor::IronPre76AudioProcessorEditor(IronPre76AudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // 共通のスライダー設定用ラムダ
    auto setupRotarySlider = [this](juce::Slider& slider)
        {
            slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
            addAndMakeVisible(slider);
        };

    // 各スライダーの初期化
    setupRotarySlider(gainSlider);
    setupRotarySlider(hpfSlider);
    setupRotarySlider(lpfSlider);

    // APVTSとのパラメータ紐付け
    // ゲイン: 12ステップ (+3dB ～ +76dB)
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "gain_step", gainSlider);

    // HPF: Bridged, 30, 60, 120 Hz
    hpfAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "hpf_freq", hpfSlider);

    // LPF: Off, 8k, 10k, 12k, 15k Hz
    lpfAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "lpf_freq", lpfSlider);

    // 3つのノブを並べるためウィンドウ幅を拡張
    setSize(600, 350);
}

IronPre76AudioProcessorEditor::~IronPre76AudioProcessorEditor()
{
}

void IronPre76AudioProcessorEditor::paint(juce::Graphics& g)
{
    // 背景描画
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    g.setColour(juce::Colours::white);
    g.setFont(20.0f);

    auto area = getLocalBounds();
    auto headerArea = area.removeFromTop(50);
    g.drawFittedText("IronPre76 - Telefunken V76s Model", headerArea, juce::Justification::centred, 1);

    // 各ノブのラベル描画
    g.setFont(14.0f);
    auto labelArea = area.removeFromTop(30);
    auto sectionWidth = labelArea.getWidth() / 3;

    g.drawText("HPF (Hz)", labelArea.removeFromLeft(sectionWidth), juce::Justification::centred);
    g.drawText("GAIN (dB)", labelArea.removeFromLeft(sectionWidth), juce::Justification::centred);
    g.drawText("LPF (Hz)", labelArea, juce::Justification::centred);
}

void IronPre76AudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    area.removeFromTop(80); // ヘッダーとラベル分を確保

    auto sectionWidth = area.getWidth() / 3;

    // レイアウト: HPF | GAIN | LPF の順で配置
    hpfSlider.setBounds(area.removeFromLeft(sectionWidth).withSizeKeepingCentre(130, 130));

    // メインゲインノブを強調するため少し大きく配置
    gainSlider.setBounds(area.removeFromLeft(sectionWidth).withSizeKeepingCentre(160, 160));

    lpfSlider.setBounds(area.withSizeKeepingCentre(130, 130));
}