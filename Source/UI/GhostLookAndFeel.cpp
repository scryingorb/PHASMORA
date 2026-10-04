#include "GhostLookAndFeel.h"
#include <cmath>

GhostLookAndFeel::GhostLookAndFeel()
{
    setColour(juce::ComboBox::textColourId, GhostTheme::pale);
    setColour(juce::PopupMenu::backgroundColourId, GhostTheme::panel);
    setColour(juce::PopupMenu::textColourId, GhostTheme::pale);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, GhostTheme::deepGreen);
    setColour(juce::PopupMenu::highlightedTextColourId, GhostTheme::mint);
    setColour(juce::TextButton::textColourOffId, GhostTheme::pale);
    setColour(juce::ResizableWindow::backgroundColourId, GhostTheme::background);
    setColour(juce::TooltipWindow::backgroundColourId, GhostTheme::panel);
    setColour(juce::TooltipWindow::textColourId, GhostTheme::pale);
    setColour(juce::TooltipWindow::outlineColourId, GhostTheme::edge);
}
juce::Font GhostLookAndFeel::font(float height, bool bold)
{
    // System sans-serif; no external font dependency or bundled font licence.
    return juce::Font(juce::FontOptions(height, bold ? juce::Font::bold : juce::Font::plain));
}
void GhostLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                       float position, float start, float end, juce::Slider& slider)
{
    const auto macro = static_cast<bool>(slider.getProperties()["ghostMacro"]);
    const auto compact = height < 100;
    const auto labelHeight = macro ? 0.0f : 25.0f;
    const auto radius = juce::jmin(static_cast<float>(width), height - labelHeight) * (macro ? 0.405f : 0.365f);
    const auto cx = x + width * 0.5f;
    const auto cy = y + (height - labelHeight) * 0.5f;
    const auto hover = slider.isMouseOverOrDragging();
    const auto lit = 0.20f + 0.80f * position;
    const auto active = GhostTheme::mint.withMultipliedBrightness(hover ? 1.12f : 1.0f);
    const auto angle = start + position * (end - start);
    const auto circle = [=](float r) { return juce::Rectangle<float>(cx-r, cy-r, r*2, r*2); };

    // Soft halo approximated by a handful of translucent concentric strokes.
    // No raster blurs, animation timer, or processing-thread work.
    for (int layer = macro ? 6 : 3; layer > 0; --layer)
    {
        const auto spread = static_cast<float>(layer) * (macro ? 3.0f : 2.0f);
        g.setColour(active.withAlpha((macro ? 0.016f : 0.012f) * lit));
        g.drawEllipse(circle(radius + spread), spread * 1.5f);
    }
    g.setGradientFill(juce::ColourGradient(GhostTheme::body, cx, cy-radius,
                                          GhostTheme::black, cx, cy+radius, false));
    g.fillEllipse(circle(radius - 5));
    g.setColour(GhostTheme::edge.withAlpha(0.65f));
    g.drawEllipse(circle(radius - 5), 1.0f);
    g.setColour(GhostTheme::deepGreen.withAlpha(0.65f));
    g.drawEllipse(circle(radius - (macro ? 16.0f : 10.0f)), 0.8f);

    const auto ticks = macro ? 40 : 20;
    for (int i = 0; i <= ticks; ++i)
    {
        const auto t = static_cast<float>(i) / ticks;
        const auto a = start + t * (end-start);
        const auto r1 = radius + (macro ? 9.0f : 6.0f);
        const auto r2 = r1 + (i % 5 == 0 ? 5.0f : 2.0f);
        g.setColour(t <= position ? GhostTheme::mint.withAlpha(0.25f + 0.3f*lit) : GhostTheme::edge);
        g.drawLine(cx + std::sin(a)*r1, cy - std::cos(a)*r1,
                   cx + std::sin(a)*r2, cy - std::cos(a)*r2, 1.0f);
    }
    juce::Path track;
    track.addCentredArc(cx, cy, radius, radius, 0, start, end, true);
    g.setColour(GhostTheme::deepGreen);
    g.strokePath(track, juce::PathStrokeType(macro ? 3.0f : 2.0f));
    if (position > 0)
    {
        juce::Path arc;
        arc.addCentredArc(cx, cy, radius, radius, 0, start, angle, true);
        g.setColour(active.withAlpha(0.06f * lit));
        g.strokePath(arc, juce::PathStrokeType(macro ? 13.0f : 7.0f));
        g.setColour(active.withAlpha(0.20f * lit));
        g.strokePath(arc, juce::PathStrokeType(macro ? 6.0f : 3.5f));
        g.setColour(active.withAlpha(0.45f + 0.55f*lit));
        g.strokePath(arc, juce::PathStrokeType(macro ? 2.8f : 1.8f,
                                              juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    // A pale pointer remains legible even at zero intensity.
    const auto pointerInner = radius - (macro ? 19.0f : 12.0f);
    const auto pointerOuter = radius - 9.0f;
    g.setColour(GhostTheme::pale.withAlpha(0.9f));
    g.drawLine(cx + std::sin(angle)*pointerInner, cy - std::cos(angle)*pointerInner,
               cx + std::sin(angle)*pointerOuter, cy - std::cos(angle)*pointerOuter, macro ? 2.5f : 1.8f);
    if (macro)
    {
        g.setFont(font(12, true).withExtraKerningFactor(0.24f));
        g.setColour(GhostTheme::mint.withAlpha(0.75f));
        g.drawText("PHASMORA", juce::Rectangle<float>(cx-65, cy-35, 130, 20), juce::Justification::centred);
    }
    g.setColour(GhostTheme::pale);
    g.setFont(font(macro ? 36.0f : compact ? 14.0f : 18.0f));
    const auto valueHalfWidth = radius - (compact ? 2.0f : 10.0f);
    g.drawText(juce::String(juce::roundToInt(slider.getValue())) + "%",
               juce::Rectangle<float>(cx-valueHalfWidth, cy-(macro ? 8.0f : 12.0f), valueHalfWidth*2, macro ? 44.0f : 24.0f),
               juce::Justification::centred);
    g.setFont(font(macro ? 9.0f : 11.0f, !macro).withExtraKerningFactor(macro ? 0.14f : 0.1f));
    g.setColour(macro ? GhostTheme::muted : GhostTheme::text);
    if (macro)
        g.drawText("INTENSITY", juce::Rectangle<float>(cx-65, cy+37, 130, 16), juce::Justification::centred);
    else
        g.drawText(slider.getName(), x, y+height-23, width, 21, juce::Justification::centred);
    if (slider.hasKeyboardFocus(false))
    {
        g.setColour(GhostTheme::pale.withAlpha(0.65f));
        g.drawEllipse(circle(radius + (macro ? 18.0f : 14.0f)), 1.0f);
    }
}
void GhostLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool hover, bool down)
{
    const auto on = button.getToggleState();
    const auto freeze = button.getComponentID() == "freeze";
    const auto accent = freeze ? GhostTheme::freeze : GhostTheme::mint;
    const auto r = button.getLocalBounds().toFloat().reduced(5);
    juce::Path outline;
    const auto cut = 7.0f;
    outline.startNewSubPath(r.getX()+cut, r.getY());
    outline.lineTo(r.getRight(), r.getY()); outline.lineTo(r.getRight(), r.getBottom()-cut);
    outline.lineTo(r.getRight()-cut, r.getBottom()); outline.lineTo(r.getX(), r.getBottom());
    outline.lineTo(r.getX(), r.getY()+cut); outline.closeSubPath();
    if (on)
    {
        g.setColour(accent.withAlpha(freeze ? 0.09f : 0.035f));
        g.strokePath(outline, juce::PathStrokeType(9));
        g.setColour(accent.withAlpha(freeze ? 0.16f : 0.08f));
        g.strokePath(outline, juce::PathStrokeType(4));
    }
    g.setGradientFill(juce::ColourGradient(on ? accent.withAlpha(freeze ? 0.30f : 0.12f) : GhostTheme::panel,
                                          r.getCentreX(), r.getY(), GhostTheme::background,
                                          r.getCentreX(), r.getBottom(), false));
    g.fillPath(outline);
    g.setColour(on ? accent : hover ? GhostTheme::mint.withAlpha(0.65f) : GhostTheme::edge);
    g.strokePath(outline, juce::PathStrokeType(on && freeze ? 1.5f : 1.0f));
    const auto mid = r.getCentreY();
    if (freeze)
    {
        g.setColour(on ? GhostTheme::pale : GhostTheme::muted);
        g.fillRoundedRectangle(r.getX()+16, mid-5, 2, 10, 0.5f);
        g.fillRoundedRectangle(r.getX()+22, mid-5, 2, 10, 0.5f);
    }
    else
    {
        g.setColour(on ? accent : GhostTheme::muted);
        for (int i = 0; i < 2; ++i)
        {
            const auto px = r.getX()+18+static_cast<float>(i)*7;
            g.drawLine(px+4, mid-5, px, mid, 1.2f);
            g.drawLine(px, mid, px+4, mid+5, 1.2f);
        }
        if (on)
        {
            g.setColour(accent.withAlpha(0.1f));
            g.drawLine(r.getX()+35, r.getBottom()-7, r.getRight()-12, r.getBottom()-11, 1);
        }
    }
    g.setColour(on ? GhostTheme::pale : GhostTheme::text);
    g.setFont(font(11, true).withExtraKerningFactor(0.1f));
    g.drawText(button.getButtonText(), r.withTrimmedLeft(26).translated(down ? 1.0f : 0.0f, down ? 1.0f : 0.0f),
               juce::Justification::centred);
    if (button.hasKeyboardFocus(false))
    {
        g.setColour(GhostTheme::pale);
        g.drawRoundedRectangle(r.reduced(3), 2, 1);
    }
}
void GhostLookAndFeel::drawTooltip(juce::Graphics& g, const juce::String& text, int width, int height)
{
    const auto bounds = juce::Rectangle<int>(width, height).toFloat().reduced(0.5f);
    g.setColour(GhostTheme::panel); g.fillRoundedRectangle(bounds, 4);
    g.setColour(GhostTheme::edge); g.drawRoundedRectangle(bounds, 4, 1);
    g.setColour(GhostTheme::pale); g.setFont(font(13));
    g.drawFittedText(text, bounds.toNearestInt().reduced(7, 4), juce::Justification::centred, 3);
}
void GhostLookAndFeel::paintBackdrop(juce::Graphics& g, juce::Rectangle<int> bounds) const
{
    g.fillAll(GhostTheme::background);
    const auto w = static_cast<float>(bounds.getWidth());
    g.setGradientFill(juce::ColourGradient(GhostTheme::deepGreen.withAlpha(0.50f), w/2, 225,
                                          GhostTheme::background, w/2+275, 370, true));
    g.fillRect(bounds);
    // Deterministic, cached vector wisps. No timer, random generator or blur.
    for (int i = 0; i < 8; ++i)
    {
        const auto n = static_cast<float>(i);
        juce::Path wisp;
        wisp.startNewSubPath(110+n*9, 290+n*2);
        wisp.cubicTo(225+n*5, 155-n*4, 542-n*8, 363+n*4, 704-n*10, 145+n*8);
        g.setColour(GhostTheme::mint.withAlpha(0.012f + n*0.001f));
        g.strokePath(wisp, juce::PathStrokeType(1+n*0.65f));
    }
    for (int i = 0; i < 12; ++i)
    {
        const auto x = 66.0f + static_cast<float>(i)*62;
        const auto y = 125.0f + static_cast<float>((i*37)%145);
        g.setGradientFill(juce::ColourGradient(GhostTheme::mint.withAlpha(0.0f), x, y,
                                              GhostTheme::mint.withAlpha(0.07f), x, y+32, false));
        g.fillRect(x, y, 0.7f, 32.0f);
    }
    g.setColour(GhostTheme::black); g.fillRect(0, 0, bounds.getWidth(), 96);
    g.setColour(GhostTheme::edge.withAlpha(0.35f)); g.drawHorizontalLine(96, 32, w-32);
    g.setColour(GhostTheme::panel.withAlpha(0.75f));
    g.fillRoundedRectangle(32, 450, w-64, 72, 8);
    g.setColour(GhostTheme::edge.withAlpha(0.45f));
    g.drawRoundedRectangle(32.5f, 450.5f, w-65, 71, 8, 1);
    g.setFont(font(10, true).withExtraKerningFactor(0.13f));
    g.setColour(GhostTheme::text);
    g.drawText("AUDIO MEMORY", 34, 34, 200, 20, juce::Justification::centredLeft);
    g.setFont(font(9).withExtraKerningFactor(0.08f)); g.setColour(GhostTheme::muted);
    g.drawText("SPECTRAL EFFECT", 34, 55, 200, 16, juce::Justification::centredLeft);
    g.drawText("V " + juce::String(JucePlugin_VersionString), bounds.getWidth()-130, 72, 96, 18, juce::Justification::centredRight);
    g.drawText("DRY / WET", 553, 475, 118, 20, juce::Justification::centredRight);
}

void GhostLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool down,
                                   int, int, int, int, juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
    const auto active = down || box.isMouseOver(true) || box.hasKeyboardFocus(true);
    g.setColour(active ? GhostTheme::body : GhostTheme::panel); g.fillRoundedRectangle(bounds, 4);
    g.setColour(active ? GhostTheme::mint : GhostTheme::edge); g.drawRoundedRectangle(bounds, 4, 1);
    juce::Path arrow; arrow.addTriangle(width-13.0f, height*0.44f, width-7.0f, height*0.44f, width-10.0f, height*0.60f);
    g.setColour(GhostTheme::mint.withAlpha(0.7f)); g.fillPath(arrow);
}
void GhostLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                            const juce::Colour&, bool hover, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(down ? GhostTheme::deepGreen : GhostTheme::panel); g.fillRoundedRectangle(bounds, 4);
    g.setColour(hover || button.hasKeyboardFocus(true) ? GhostTheme::mint : GhostTheme::edge);
    g.drawRoundedRectangle(bounds, 4, 1);
}
