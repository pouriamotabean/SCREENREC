#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ReelsOverlay.h"

ScreenRecProcessor::ScreenRecProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

ScreenRecProcessor::~ScreenRecProcessor()
{
    overlay.reset();
}

void ScreenRecProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    recorder.prepare (sampleRate, samplesPerBlock);
}

bool ScreenRecProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out.isDisabled())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void ScreenRecProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // Audio passes through untouched; the recorder only taps it
    // (and adds the short beeps during a sync test).
    recorder.processAudio (buffer);
}

juce::AudioProcessorEditor* ScreenRecProcessor::createEditor()
{
    return new ScreenRecEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ScreenRecProcessor();
}
