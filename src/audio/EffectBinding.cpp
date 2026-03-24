#include "audio/EffectBinding.hpp"

#include <cstdio>
#include <cstring>
#include <utility>

EffectBinding::EffectBinding(EffectInfo info)
    : m_info(std::move(info)) {
}

EffectBinding& EffectBinding::bind(const char* paramId, std::shared_ptr<gs::ui::widgets::UISliderElement> slider) {
    if (!slider) {
        return *this;
    }

    ParamHandle* handle = findParam(paramId);
    if (!handle) {
        return *this;
    }

    slider->minValue = handle->spec->minValue;
    slider->maxValue = handle->spec->maxValue;
    slider->value = handle->get();
    if (slider->label.empty()) {
        slider->label = handle->spec->label;
    }

    ParamHandle h = *handle;
    slider->valueChanged.connect([h](float v) { h.set(v); });
    m_sliderBindings.push_back({paramId, slider});
    return *this;
}

EffectBinding& EffectBinding::bindValue(const char* paramId, std::shared_ptr<gs::ui::widgets::UITextElement> text, const char* fmt) {
    if (!text) {
        return *this;
    }

    ParamHandle* handle = findParam(paramId);
    if (!handle) {
        return *this;
    }

    updateValueText(text.get(), *handle, fmt);

    std::string fmtStr = fmt ? fmt : "";
    std::weak_ptr<gs::ui::widgets::UITextElement> weakText = text;
    const char* label = handle->spec->label;

    for (auto& sb : m_sliderBindings) {
        if (std::strcmp(sb.paramId, paramId) == 0) {
            if (auto slider = sb.slider.lock()) {
                slider->showLabel = false;
                slider->valueChanged.connect([weakText, fmtStr, label](float v) {
                    if (auto t = weakText.lock()) {
                        char buf[128];
                        if (!fmtStr.empty()) {
                            std::snprintf(buf, sizeof(buf), fmtStr.c_str(), v);
                        } else {
                            std::snprintf(buf, sizeof(buf), "%s: %.1f", label, v);
                        }
                        t->text = buf;
                    }
                });
            }
            break;
        }
    }

    m_valueBindings.push_back({*handle, text, fmtStr, label});
    return *this;
}

EffectBinding& EffectBinding::bindTitle(std::shared_ptr<gs::ui::widgets::UITextElement> text) {
    if (text && m_info.name) {
        text->text = m_info.name;
    }
    return *this;
}

EffectBinding& EffectBinding::bindBypass(std::shared_ptr<gs::ui::widgets::UIButtonElement> button, std::function<void(uint64_t)> toggleFn) {
    if (!button) {
        return *this;
    }

    uint64_t id = m_info.id;
    button->clicked.connect([toggleFn, id]() { toggleFn(id); });
    return *this;
}

void EffectBinding::updateValueTexts() {
    for (auto& vb : m_valueBindings) {
        if (auto text = vb.text.lock()) {
            updateValueText(text.get(), vb.handle, vb.fmt.empty() ? nullptr : vb.fmt.c_str());
        }
    }
}

const EffectInfo& EffectBinding::info() const {
    return m_info;
}

int EffectBinding::index() const {
    return static_cast<int>(m_info.id);
}

bool EffectBinding::valid() const {
    return m_info.name != nullptr;
}

ParamHandle* EffectBinding::findParam(const char* id) {
    for (auto& p : m_info.params) {
        if (p.spec && std::strcmp(p.spec->id, id) == 0) {
            return &p;
        }
    }
    return nullptr;
}

void EffectBinding::updateValueText(gs::ui::widgets::UITextElement* text, const ParamHandle& handle, const char* fmt) {
    char buf[128];
    float val = handle.get();
    if (fmt && fmt[0] != '\0') {
        std::snprintf(buf, sizeof(buf), fmt, val);
    } else {
        std::snprintf(buf, sizeof(buf), "%s: %.1f", handle.spec->label, val);
    }
    text->text = buf;
}