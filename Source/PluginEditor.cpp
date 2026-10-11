#include "PluginEditor.h"
#include "Timing.h"

namespace
{
    const juce::Colour bg       (0xff1b1c1f);
    const juce::Colour panel    (0xff232428);
    const juce::Colour line     (0xff34363c);
    const juce::Colour text     (0xffd9d6cf);
    const juce::Colour dimText  (0xff8a8780);
    const juce::Colour amber    (0xffe0a040);
    const juce::Colour recRed   (0xffe0342c);

    juce::String formatTime (double seconds)
    {
        const int s = (int) seconds;
        return juce::String::formatted ("%02d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60);
    }
}

ScreenRecEditor::ScreenRecEditor (ScreenRecProcessor& p)
    : AudioProcessorEditor (&p), recorder (p.recorder)
{
    addChildComponent (flash);
    flash.setInterceptsMouseClicks (false, false);
    setSize (440, 230);
    recorder.warmUp();
    startTimerHz (30);
}

ScreenRecEditor::~ScreenRecEditor()
{
    stopTimer();
}

void ScreenRecEditor::resized()
{
    recButtonArea = { 28, 62, 104, 104 };
    timeArea = { 156, 78, 260, 64 };
    syncArea = { getWidth() - 112, getHeight() - 34, 96, 22 };
    flash.setBounds (timeArea);
}

juce::Rectangle<int> ScreenRecEditor::physicalScreenRectOf (const juce::Component& c) const
{
    const auto logical = c.getScreenBounds();
    auto physical = juce::Desktop::getInstance().getDisplays().logicalToPhysical (logical);
    // Sample only the middle of the panel so small coordinate errors don't matter.
    return physical.reduced (physical.getWidth() / 4, physical.getHeight() / 4);
}

void ScreenRecEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);

    const auto state = recorder.getState();
    const bool recording = state == Recorder::State::recording;
    const bool busy = state == Recorder::State::starting || state == Recorder::State::saving;

    // ---- header
    g.setColour (panel);
    g.fillRect (0, 0, getWidth(), 44);
    g.setColour (line);
    g.drawHorizontalLine (44, 0.0f, (float) getWidth());

    g.setColour (text);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("SCREENREC", 20, 0, 200, 44, juce::Justification::centredLeft);

    g.setColour (amber);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("BY POURIA MOTABEAN", getWidth() - 220, 0, 200, 44, juce::Justification::centredRight);

    // ---- REC / STOP button
    auto b = recButtonArea.toFloat();
    g.setColour (panel.brighter (hoverRec && ! busy ? 0.15f : 0.0f));
    g.fillEllipse (b);
    g.setColour (line.brighter (0.2f));
    g.drawEllipse (b.reduced (1.0f), 2.0f);

    const auto inner = b.reduced (34.0f);
    if (recording)
    {
        g.setColour (text);
        g.fillRoundedRectangle (inner, 4.0f);
    }
    else
    {
        g.setColour (busy ? recRed.withAlpha (0.35f) : recRed);
        g.fillEllipse (inner.expanded (4.0f));
    }

    g.setColour (dimText);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (recording ? "STOP" : (busy ? "WAIT" : "REC"),
                recButtonArea.withY (recButtonArea.getBottom() + 4).withHeight (16),
                juce::Justification::centred);

    // ---- lamp + time
    const bool lampOn = recording && std::fmod (recorder.getElapsedSeconds(), 1.0) < 0.6;
    const juce::Rectangle<float> lamp ((float) timeArea.getX(), (float) timeArea.getY() - 18.0f, 10.0f, 10.0f);
    g.setColour (lampOn ? recRed : juce::Colour (0xff3a2422));
    g.fillEllipse (lamp);
    if (lampOn)
    {
        g.setColour (recRed.withAlpha (0.25f));
        g.fillEllipse (lamp.expanded (4.0f));
    }

    g.setColour (recording ? text : dimText);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText (recording ? (recorder.isSyncTest() ? "SYNC TEST" : "RECORDING") : "READY",
                (int) lamp.getRight() + 8, (int) lamp.getY() - 3, 160, 16, juce::Justification::centredLeft);

    if (! flash.isVisible())
    {
        g.setColour (recording ? text : dimText);
        g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 46.0f, juce::Font::plain));
        g.drawText (formatTime (recorder.getElapsedSeconds()), timeArea, juce::Justification::centredLeft);
    }

    // ---- status + sync
    g.setColour (line);
    g.drawHorizontalLine (getHeight() - 44, 0.0f, (float) getWidth());

    g.setColour (state == Recorder::State::error ? recRed.brighter (0.3f) : dimText);
    g.setFont (juce::FontOptions (12.0f));
    g.drawFittedText (recorder.getStatusText(), 16, getHeight() - 40, getWidth() - 140, 36,
                      juce::Justification::centredLeft, 2);

    const bool syncEnabled = ! recording && ! busy;
    g.setColour (syncEnabled ? (hoverSync ? amber : dimText) : line);
    g.drawRoundedRectangle (syncArea.toFloat().reduced (0.5f), 4.0f, 1.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("SYNC TEST", syncArea, juce::Justification::centred);

    g.setColour (line.brighter (0.4f));
    g.setFont (juce::FontOptions (10.0f));
    g.drawText ("offset " + juce::String (recorder.getSyncOffsetMs(), 0) + " ms",
                syncArea.translated (0, -18), juce::Justification::centred);
}

void ScreenRecEditor::mouseMove (const juce::MouseEvent& e)
{
    const bool r = recButtonArea.toFloat().contains (e.position)
                && e.position.getDistanceFrom (recButtonArea.getCentre().toFloat()) < recButtonArea.getWidth() * 0.5f;
    const bool s = syncArea.contains (e.getPosition());

    if (r != hoverRec || s != hoverSync)
    {
        hoverRec = r;
        hoverSync = s;
        repaint();
    }
}

void ScreenRecEditor::mouseUp (const juce::MouseEvent& e)
{
    const auto state = recorder.getState();

    if (e.position.getDistanceFrom (recButtonArea.getCentre().toFloat()) < recButtonArea.getWidth() * 0.5f)
    {
        if (state == Recorder::State::recording)
            recorder.stop();
        else if (state == Recorder::State::idle || state == Recorder::State::error)
            recorder.start (false);
    }
    else if (syncArea.contains (e.getPosition())
             && (state == Recorder::State::idle || state == Recorder::State::error))
    {
        flash.on = false;
        flash.setVisible (true);   // must be on screen before the probe rect is taken
        recorder.start (true, physicalScreenRectOf (flash));
    }

    repaint();
}

void ScreenRecEditor::timerCallback()
{
    const auto state = recorder.getState();
    const bool testRunning = recorder.isSyncTest()
                          && (state == Recorder::State::starting || state == Recorder::State::recording);

    if (flash.isVisible() != testRunning)
        flash.setVisible (testRunning);

    if (testRunning)
    {
        const bool on = recorder.isFlashOn();
        if (on != flash.on)
        {
            flash.on = on;
            flash.repaint();
        }

        if (getTimerInterval() != 2)
            startTimer (2);   // check the flash schedule every 2 ms

        if (++fastTimerTicks % 16 != 0)
            return;           // repaint the rest of the UI ~30x per second
    }
    else if (getTimerInterval() != 33)
    {
        startTimer (33);
    }

    repaint (getLocalBounds().withTrimmedTop (45));
}
