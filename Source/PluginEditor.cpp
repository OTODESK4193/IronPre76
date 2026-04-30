#include "PluginProcessor.h"
#include "PluginEditor.h"

IronPre76AudioProcessorEditor::IronPre76AudioProcessorEditor(IronPre76AudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // スライダーのスタイル設定 (アナログ機材を模したロータリースタイル)
    gainSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    gainSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible(gainSlider);

    // APVTSの "gain_step" パラメータとUIを紐付け
    gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "gain_step", gainSlider);

    // プラグインウィンドウのデフォルトサイズ
    setSize(400, 300);
}

IronPre76AudioProcessorEditor::~IronPre76AudioProcessorEditor()
{
}

void IronPre76AudioProcessorEditor::paint(juce::Graphics& g)
{
    // 背景の塗りつぶし
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    g.setColour(juce::Colours::white);
    g.setFont(16.0f);

    // ヘッダーテキストの描画
    g.drawFittedText("IronPre76 - Telefunken V76s Model", getLocalBounds().removeFromTop(40), juce::Justification::centred, 1);
}

void IronPre76AudioProcessorEditor::resized()
{
    // 中央にゲインノブを配置
    auto area = getLocalBounds();
    area.removeFromTop(40); // ヘッダー領域を除外

    // ノブのサイズを120x120に設定し、中央に配置
    gainSlider.setBounds(area.withSizeKeepingCentre(120, 120));
}