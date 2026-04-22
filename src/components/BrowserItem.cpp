#include "components/BrowserItem.hpp"

#include <cmath>

BrowserItem::BrowserItem(const std::string& label, gs::ui::DragPayload payload, float fontSize, gs::ui::UIColor textColor, gs::ui::UIColor hoverColor)
    : m_label(label)
    , m_payload(std::move(payload))
    , m_fontSize(fontSize)
    , m_textColor(textColor)
    , m_hoverColor(hoverColor)
{
    heightMode = gs::ui::UISizeMode::Fixed;
    preferredHeight = fontSize + 6.0f;
}

gs::ui::UISize BrowserItem::measure(const gs::ui::UISize& available) {
    float w = available.width;
    float h = preferredHeight;
    desired = {w, h};
    return desired;
}

void BrowserItem::arrange(const gs::ui::UIRect& finalRect) {
    arranged = finalRect;
}

void BrowserItem::emit(gs::ui::UICanvas& canvas) {
    if (!visible) return;

    emitBackground(canvas);
    bool hovered = hasState(gs::ui::style::StyleStates::Hovered);
    if (hovered) {
        gs::ui::UINode bg{};
        bg.type = gs::ui::UINodeType::FilledRect;
        bg.x = arranged.x;
        bg.y = arranged.y;
        bg.rectWidth = arranged.width;
        bg.rectHeight = arranged.height;
        bg.color = {0.3f, 0.3f, 0.4f, 0.3f};
        bg.cornerRadius = 2.0f;
        canvas.nodes.push_back(bg);
    }

    auto color = hovered ? m_hoverColor : m_textColor;

    float ascent = canvas.fontAscender * m_fontSize;
    float descent = -canvas.fontDescender * m_fontSize;
    float textH  = ascent + descent;
    float baselineY = arranged.y + (arranged.height - textH) * 0.5f + ascent;

    canvas.addText(m_label, arranged.x + 4.0f, baselineY, m_fontSize, color);
}

bool BrowserItem::onMouseMove(float x, float y) {
    if (!hasState(gs::ui::style::StyleStates::Pressed)) {
        if (arranged.contains(x, y)) addState(gs::ui::style::StyleStates::Hovered);
        else removeState(gs::ui::style::StyleStates::Hovered);
    }

    if (m_mouseDown && !m_dragging) {
        float dx = x - m_downX;
        float dy = y - m_downY;
        if (std::sqrt(dx * dx + dy * dy) > 4.0f) {
            m_dragging = true;
            gs::ui::DragOverlay::get().beginDrag(m_payload, m_downX, m_downY);
        }
    }

    if (m_dragging) {
        gs::ui::DragOverlay::get().updatePosition(x, y);
        requestRender();
        return true;
    }

    return false;
}

bool BrowserItem::onMouseDown(float x, float y) {
    if (!hitTest(x, y)) return false;

    m_mouseDown = true;
    m_dragging = false;
    m_downX = x;
    m_downY = y;

    return true;
}

bool BrowserItem::onMouseUp(float x, float y) {
    bool wasDragging = m_dragging;

    m_mouseDown = false;
    m_dragging = false;

    if (wasDragging) {
        return true;
    }

    return hitTest(x, y);
}

bool BrowserItem::hitTest(float x, float y) const {
    return arranged.contains(x, y);
}

bool BrowserItem::isInteractive() const {
    return true;
}

const std::string& BrowserItem::label() const {
    return m_label;
}

const gs::ui::DragPayload& BrowserItem::dragPayload() const {
    return m_payload;
}
