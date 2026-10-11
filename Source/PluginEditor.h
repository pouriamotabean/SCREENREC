#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class ScreenRecEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit ScreenRecEditor (ScreenRecProcessor&);
    ~ScreenRecEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Rectangle<int> physicalScreenRectOf (const juce::Component&) const;

    struct FlashPanel : juce::Component
    {
        bool on = false;
        void paint (juce::Graphics& g) override
        {
            g.fillAll (on ? juce::Colours::white : juce::Colour (0xff101012));
        }
    };

    Recorder& recorder;
    FlashPanel flash;

    juce::Rectangle<int> recButtonArea, syncArea, timeArea;
    bool hoverRec = false, hoverSync = false;
    int fastTimerTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScreenRecEditor)
};
