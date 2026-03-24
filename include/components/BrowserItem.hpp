#pragma once

#include <ui/core/UIElement.hpp>
#include <ui/core/UITypes.hpp>
#include <ui/core/DragOverlay.hpp>

#include <string>

class BrowserItem : public gs::ui::core::UIElement {
public:
    BrowserItem(const std::string& label,
                gs::ui::core::DragPayload payload,
                float fontSize = 11.0f,
                gs::ui::core::UIColor textColor = {0.75f, 0.75f, 0.75f, 1.0f},
                gs::ui::core::UIColor hoverColor = {0.9f, 0.9f, 0.9f, 1.0f});

    gs::ui::core::UISize measure(const gs::ui::core::UISize& available) override;
    void arrange(const gs::ui::core::UIRect& finalRect) override;
    void emit(gs::ui::core::UICanvas& canvas) override;

    bool onMouseMove(float x, float y) override;
    bool onMouseDown(float x, float y) override;
    bool onMouseUp(float x, float y) override;

    bool hitTest(float x, float y) const override;
    bool isInteractive() const override;

    const std::string& label() const;
    const gs::ui::core::DragPayload& dragPayload() const;

private:
    std::string                m_label;
    gs::ui::core::DragPayload  m_payload;
    float                      m_fontSize;
    gs::ui::core::UIColor      m_textColor;
    gs::ui::core::UIColor      m_hoverColor;

    bool  m_hovered   = false;
    bool  m_mouseDown = false;
    bool  m_dragging  = false;
    float m_downX     = 0.0f;
    float m_downY     = 0.0f;
};