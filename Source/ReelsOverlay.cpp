#include "ReelsOverlay.h"
#include "WinUtils.h"
#include <cmath>

namespace
{
    const juce::Colour amber  (0xffe0a040);
    const juce::Colour recRed (0xffe0342c);
    constexpr double aspect = 9.0 / 16.0;   // width / height
}

ReelsOverlay::ReelsOverlay (Recorder& r) : recorder (r)
{
    frame.setOpaque (false);
    frame.setInterceptsMouseClicks (false, false);
    frame.setAlwaysOnTop (true);

    for (auto* h : { &moveHandle, &zoomHandle })
    {
        h->setOpaque (false);
        h->setAlwaysOnTop (true);
        h->setWantsKeyboardFocus (false);
        h->setMouseClickGrabsKeyboardFocus (false);
        h->setSize (handleSize, handleSize);
    }

    moveHandle.setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    zoomHandle.setMouseCursor (juce::MouseCursor::UpDownLeftRightResizeCursor);

    startTimerHz (30);
}

ReelsOverlay::~ReelsOverlay()
{
    stopTimer();
    hide();
}

//==============================================================================

juce::Rectangle<int> ReelsOverlay::screenArea() const
{
    if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        return d->totalArea;

    return { 0, 0, 1920, 1080 };
}

double ReelsOverlay::minHeightFraction() const
{
    double physicalH = 1080.0;
    if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        physicalH = d->totalArea.getHeight() * d->scale;

    // Smallest frame: a third of the screen height (but at least 540 px),
    // so the 1080x1920 output never gets too soft.
    return juce::jlimit (0.1, 1.0, juce::jmax (physicalH / 3.0, 540.0) / physicalH);
}

juce::Rectangle<int> ReelsOverlay::frameRectLogical() const
{
    double cx, cy, h;
    recorder.getReelsFrame (cx, cy, h);

    const auto a = screenArea().toDouble();
    const double hh = h * a.getHeight();
    const double ww = hh * aspect;

    return juce::Rectangle<double> (a.getX() + cx * a.getWidth() - ww * 0.5,
                                    a.getY() + cy * a.getHeight() - hh * 0.5, ww, hh)
               .getSmallestIntegerContainer();
}

void ReelsOverlay::setFrame (double cx, double cy, double h)
{
    const auto a = screenArea().toDouble();
    const double maxH = juce::jmin (1.0, a.getWidth() / aspect / a.getHeight());

    h = juce::jlimit (juce::jmin (minHeightFraction(), maxH), maxH, h);
    const double wf = h * a.getHeight() * aspect / a.getWidth();   // width as fraction of screen width

    cx = juce::jlimit (wf * 0.5, 1.0 - wf * 0.5, cx);
    cy = juce::jlimit (h * 0.5, 1.0 - h * 0.5, cy);

    recorder.setReelsFrame (cx, cy, h);
    layout();
}

//==============================================================================

void ReelsOverlay::show()
{
    if (shown)
        return;

    // Make sure the stored frame fits this screen.
    double cx, cy, h;
    recorder.getReelsFrame (cx, cy, h);
    setFrame (cx, cy, h);

    frame.addToDesktop (juce::ComponentPeer::windowIsTemporary | juce::ComponentPeer::windowIgnoresMouseClicks);
    frame.setVisible (true);

    moveHandle.addToDesktop (juce::ComponentPeer::windowIsTemporary);
    zoomHandle.addToDesktop (juce::ComponentPeer::windowIsTemporary);
    moveHandle.setVisible (true);
    zoomHandle.setVisible (true);

    excludedOk = true;

    if (auto* peer = frame.getPeer())
    {
        WinUtils::makeClickThrough (peer->getNativeHandle());
        excludedOk &= WinUtils::setExcludedFromCapture (peer->getNativeHandle(), true);
    }

    for (auto* hd : { &moveHandle, &zoomHandle })
        if (auto* peer = hd->getPeer())
        {
            WinUtils::makeNoActivate (peer->getNativeHandle());
            excludedOk &= WinUtils::setExcludedFromCapture (peer->getNativeHandle(), true);
        }

    shown = true;
    layout();
    moveHandle.toFront (false);
    zoomHandle.toFront (false);
}

void ReelsOverlay::hide()
{
    if (! shown)
        return;

    for (juce::Component* c : { (juce::Component*) &moveHandle, (juce::Component*) &zoomHandle, (juce::Component*) &frame })
    {
        c->setVisible (false);
        if (c->isOnDesktop())
            c->removeFromDesktop();
    }

    shown = false;
}

void ReelsOverlay::layout()
{
    const auto r = frameRectLogical();
    const auto area = screenArea();
    lastFrameRect = r;

    frame.setBounds (r.expanded (border));
    frame.repaint();

    const int hs = handleSize, gap = border + 2;

    juce::Rectangle<int> tl (r.getX() - hs - gap, r.getY() - hs - gap, hs, hs);
    if (! area.contains (tl))
        tl = { r.getX() + gap, r.getY() + gap, hs, hs };

    juce::Rectangle<int> tr (r.getRight() + gap, r.getY() - hs - gap, hs, hs);
    if (! area.contains (tr))
        tr = { r.getRight() - hs - gap, r.getY() + gap, hs, hs };

    moveHandle.setBounds (tl);
    zoomHandle.setBounds (tr);
}

void ReelsOverlay::timerCallback()
{
    const bool syncTestRunning = recorder.isSyncTest() && recorder.isBusy();
    const bool want = recorder.getMode() == Recorder::Mode::reels
                   && ! syncTestRunning
                   && (editorOpen || recorder.isBusy());

    if (want && ! shown)  show();
    if (! want && shown)  hide();

    if (! shown)
        return;

    const bool rec = recorder.getState() == Recorder::State::recording;
    if (rec != lastRecording)
    {
        lastRecording = rec;
        frame.recording = rec;
        frame.repaint();
    }

    if (frameRectLogical() != lastFrameRect)   // screen layout changed
        layout();
}

//==============================================================================

void ReelsOverlay::FrameWindow::paint (juce::Graphics& g)
{
    const auto inner = getLocalBounds().toFloat().reduced ((float) border);
    const auto colour = recording ? recRed : amber;

    // Outline sits just outside the captured area.
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawRect (inner.expanded (4.0f), 1.0f);
    g.setColour (colour.withAlpha (0.95f));
    g.drawRect (inner.expanded (3.0f), 2.0f);

    // Corner ticks (in the border zone, outside the captured area).
    const auto o = getLocalBounds().toFloat();
    const float t = juce::jmin (40.0f, inner.getWidth() * 0.15f), k = (float) border;
    g.setColour (colour);
    g.fillRect (o.getX(), o.getY(), t, k);                      g.fillRect (o.getX(), o.getY(), k, t);
    g.fillRect (o.getRight() - t, o.getY(), t, k);              g.fillRect (o.getRight() - k, o.getY(), k, t);
    g.fillRect (o.getX(), o.getBottom() - k, t, k);             g.fillRect (o.getX(), o.getBottom() - t, k, t);
    g.fillRect (o.getRight() - t, o.getBottom() - k, t, k);     g.fillRect (o.getRight() - k, o.getBottom() - t, k, t);

    // Centre marker: soft translucent white circle with a fine crosshair.
    const auto c = inner.getCentre();
    const float rad = 20.0f;
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.fillEllipse (c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.drawEllipse (c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f, 1.5f);
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.drawLine (c.x - 7.0f, c.y, c.x + 7.0f, c.y, 1.0f);
    g.drawLine (c.x, c.y - 7.0f, c.x, c.y + 7.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.fillEllipse (c.x - 2.0f, c.y - 2.0f, 4.0f, 4.0f);
}

void ReelsOverlay::Handle::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colour (0xff1b1c1f).withAlpha (0.9f));
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (amber.withAlpha (hover ? 1.0f : 0.8f));
    g.drawRoundedRectangle (b.reduced (1.0f), 6.0f, 2.0f);

    const auto c = b.getCentre();
    const float s = b.getWidth() * 0.3f;
    juce::Path p;

    if (kind == Kind::move)
    {
        p.addArrow ({ c.x, c.y, c.x - s, c.y }, 1.6f, 5.0f, 4.0f);
        p.addArrow ({ c.x, c.y, c.x + s, c.y }, 1.6f, 5.0f, 4.0f);
        p.addArrow ({ c.x, c.y, c.x, c.y - s }, 1.6f, 5.0f, 4.0f);
        p.addArrow ({ c.x, c.y, c.x, c.y + s }, 1.6f, 5.0f, 4.0f);
    }
    else
    {
        p.addArrow ({ c.x - s * 0.1f, c.y + s * 0.1f, c.x + s, c.y - s }, 1.6f, 6.0f, 5.0f);
        p.addArrow ({ c.x + s * 0.1f, c.y - s * 0.1f, c.x - s, c.y + s }, 1.6f, 6.0f, 5.0f);
    }

    g.setColour (amber.withAlpha (hover ? 1.0f : 0.85f));
    g.fillPath (p);
}

void ReelsOverlay::Handle::mouseDown (const juce::MouseEvent&)
{
    owner.recorder.getReelsFrame (startCx, startCy, startH);
}

void ReelsOverlay::Handle::mouseDrag (const juce::MouseEvent& e)
{
    const auto a = owner.screenArea().toDouble();
    const auto d = (e.getScreenPosition() - e.getMouseDownScreenPosition()).toDouble();

    if (kind == Kind::move)
    {
        owner.setFrame (startCx + d.x / a.getWidth(), startCy + d.y / a.getHeight(), startH);
        return;
    }

    // Zoom around the centre: the top-right corner follows the mouse.
    const double cx = a.getX() + startCx * a.getWidth();
    const double cy = a.getY() + startCy * a.getHeight();
    const double h0 = startH * a.getHeight();
    const double cornerX = cx + h0 * aspect * 0.5 + d.x;
    const double cornerY = cy - h0 * 0.5 + d.y;
    const double newH = juce::jmax (2.0 * (cy - cornerY), 2.0 * (cornerX - cx) / aspect);

    owner.setFrame (startCx, startCy, newH / a.getHeight());
}

void ReelsOverlay::Handle::mouseUp (const juce::MouseEvent&)
{
    owner.recorder.saveSettings();
}

void ReelsOverlay::Handle::mouseDoubleClick (const juce::MouseEvent&)
{
    if (kind != Kind::move)
        return;

    double cx, cy, h;
    owner.recorder.getReelsFrame (cx, cy, h);
    owner.setFrame (cx, 0.5, 1.0);
    owner.recorder.saveSettings();
}

void ReelsOverlay::Handle::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    double cx, cy, h;
    owner.recorder.getReelsFrame (cx, cy, h);
    owner.setFrame (cx, cy, h * std::exp (-w.deltaY * 1.2));   // wheel up = zoom in
    owner.recorder.saveSettings();
}
