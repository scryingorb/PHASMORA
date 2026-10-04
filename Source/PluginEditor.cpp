#include "PluginEditor.h"
#include "GhostAssets.h"

GhostAudioProcessorEditor::GhostAudioProcessorEditor(GhostAudioProcessor& processor)
    : AudioProcessorEditor(processor), state(processor.apvts), visualizer(processor),
      ghost(processor.apvts, "ghost", "PHASMORA", "Overall supernatural effect intensity.", true),
      memory(processor.apvts, "memory", "MEMORY", "How far into the recent past the ghosts are drawn from."),
      decay(processor.apvts, "decay", "DECAY", "How long ghost material regenerates."),
      blur(processor.apvts, "blur", "BLUR", "Softens and smears remembered audio."),
      drift(processor.apvts, "drift", "DRIFT", "Adds slow pitch instability."),
      mix(processor.apvts, "mix", "MIX", "Balances dry and ghost signals."),
      reverseAttachment(processor.apvts, "reverse", reverse),
      freezeAttachment(processor.apvts, "freeze", freeze)
{
    setLookAndFeel(&lookAndFeel);
    presetSelector.setComponentID("factoryPreset");
    presetSelector.setTitle("Factory preset");
    presetSelector.setTooltip("Factory preset. Frozen Choir: feed audio, then engage FREEZE.");
    presetSelector.setTextWhenNothingSelected("Custom");
    presetSelector.setJustificationType(juce::Justification::centred);
    for (size_t i = 0; i < GhostPresetManager::presets().size(); ++i)
        presetSelector.addItem(GhostPresetManager::presets()[i].name, static_cast<int>(i) + 1);
    presetSelector.onChange = [this] { selectPreset(presetSelector.getSelectedId() - 1); };
    addAndMakeVisible(presetSelector);
    previousPreset.setComponentID("previousPreset");
    nextPreset.setComponentID("nextPreset");
    previousPreset.setTooltip("Previous preset"); nextPreset.setTooltip("Next preset");
    previousPreset.setTitle("Previous preset"); nextPreset.setTitle("Next preset");
    previousPreset.onClick = [this] { stepPreset(-1); };
    nextPreset.onClick = [this] { stepPreset(1); };
    for (auto* button : { &previousPreset, &nextPreset })
    { button->setWantsKeyboardFocus(true); addAndMakeVisible(*button); }
    timerCallback();
    startTimerHz(10);
    addAndMakeVisible(visualizer); // Behind every control; never intercepts input.
    for (auto* knob : { &ghost, &memory, &decay, &blur, &drift, &mix }) addAndMakeVisible(*knob);
    reverse.setComponentID("reverse"); freeze.setComponentID("freeze");
    reverse.setTooltip("Plays remembered ghost fragments backward.");
    freeze.setTooltip("Captures the current ghost memory.");
    for (auto* button : { &reverse, &freeze })
    {
        button->setTitle(button->getButtonText());
        button->setDescription(button->getTooltip());
        button->setWantsKeyboardFocus(true);
        addAndMakeVisible(*button);
    }
    // Embedded original asset; decode once, keep a 2x header image for HiDPI.
    const auto original = juce::ImageCache::getFromMemory(GhostAssets::PhasmoraLogo_png, GhostAssets::PhasmoraLogo_pngSize);
    if (original.isValid())
        logo = original.rescaled(480, juce::roundToInt(480.0 * original.getHeight() / original.getWidth()),
                                 juce::Graphics::highResamplingQuality);
    setResizable(false, false);
    setSize(820, 540);
    backdrop = juce::Image(juce::Image::RGB, 1640, 1080, true);
    juce::Graphics cache(backdrop);
    cache.addTransform(juce::AffineTransform::scale(2.0f));
    lookAndFeel.paintBackdrop(cache, getLocalBounds());
}
GhostAudioProcessorEditor::~GhostAudioProcessorEditor() { stopTimer(); setLookAndFeel(nullptr); }
void GhostAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.drawImage(backdrop, getLocalBounds().toFloat());
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    g.drawImage(logo, juce::Rectangle<float>(290, 0, 240, 96), juce::RectanglePlacement::centred);
}
void GhostAudioProcessorEditor::resized()
{
    previousPreset.setBounds(548, 37, 26, 28);
    presetSelector.setBounds(578, 37, 170, 28);
    nextPreset.setBounds(752, 37, 26, 28);
    visualizer.setBounds(172, 97, 476, 236);
    ghost.setBounds(294, 106, 232, 222);
    memory.setBounds(100, 321, 128, 122);
    decay.setBounds(264, 321, 128, 122);
    blur.setBounds(428, 321, 128, 122);
    drift.setBounds(592, 321, 128, 122);
    reverse.setBounds(54, 460, 154, 50);
    freeze.setBounds(225, 460, 164, 50);
    mix.setBounds(688, 442, 88, 86);
}

void GhostAudioProcessorEditor::timerCallback()
{
    const auto id = GhostPresetManager::matchingPreset(state) + 1;
    if (presetSelector.getSelectedId() != id)
        presetSelector.setSelectedId(id, juce::dontSendNotification);
}
void GhostAudioProcessorEditor::selectPreset(int index)
{
    GhostPresetManager::load(state, index);
    timerCallback();
}
void GhostAudioProcessorEditor::stepPreset(int direction)
{
    const auto current = GhostPresetManager::matchingPreset(state);
    const auto count = static_cast<int>(GhostPresetManager::presets().size());
    selectPreset(current < 0 ? (direction > 0 ? 0 : count - 1)
                             : (current + direction + count) % count);
}
