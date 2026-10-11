#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Recorder.h"

// The on-screen REELS frame: a 9:16 outline with a centre marker, a MOVE handle
// (top-left: drag, mouse wheel = zoom, double-click = full height) and a ZOOM
// handle (top-right: drag to resize around the centre). Everything here is
// visible on the monitor but excluded from the recording, and the frame itself
// lets clicks through to Cubase.
class ReelsOverlay : private juce::Timer
{
public:
    explicit ReelsOverlay (Recorder&);
    ~ReelsOverlay() override;

    void setEditorOpen (bool open)  { editorOpen = open; timerCallback(); }

private:
    struct FrameWindow : juce::Component
    {
        bool recording = false;
        void paint (juce::Graphics&) override;
    };

    struct Handle : juce::Component
    {
        enum class Kind { move, zoom };
        Handle (ReelsOverlay& o, Kind k) : owner (o), kind (k) {}

        void paint (juce::Graphics&) override;
        void mouseEnter (const juce::MouseEvent&) override  { hover = true;  repaint(); }
        void mouseExit (const juce::MouseEvent&) override   { hover = false; repaint(); }
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

        ReelsOverlay& owner;
        Kind kind;
        bool hover = false;
        double startCx = 0, startCy = 0, startH = 0;
    };

    void timerCallback() override;
    void show();
    void hide();
    void layout();

    juce::Rectangle<int> screenArea() const;          // primary display, logical px
    double minHeightFraction() const;
    void setFrame (double cx, double cy, double h);   // clamps, publishes, re-lays out
    juce::Rectangle<int> frameRectLogical() const;

    Recorder& recorder;
    FrameWindow frame;
    Handle moveHandle { *this, Handle::Kind::move };
    Handle zoomHandle { *this, Handle::Kind::zoom };

    bool editorOpen = false;
    bool shown = false;
    bool excludedOk = true;
    juce::Rectangle<int> lastFrameRect;
    bool lastRecording = false;

    static constexpr int handleSize = 30;
    static constexpr int border = 4;

    JUCE_DECLARE_NON_COPYABLE (ReelsOverlay)
};
