#pragma once

#include <audio/core/AudioTypes.hpp>
#include <ui/widgets/UISliderElement.hpp>
#include <ui/widgets/UITextElement.hpp>
#include <ui/widgets/UIButtonElement.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

using namespace gs::audio;

class EffectBinding {
public:
    EffectBinding() = default;
    explicit EffectBinding(EffectInfo info);

    EffectBinding& bind(const char* paramId, std::shared_ptr<gs::ui::UISliderElement> slider);
    EffectBinding& bindValue(const char* paramId, std::shared_ptr<gs::ui::UITextElement> text, const char* fmt = nullptr);
    EffectBinding& bindTitle(std::shared_ptr<gs::ui::UITextElement> text);
    EffectBinding& bindBypass(std::shared_ptr<gs::ui::UIButtonElement> button, std::function<void(uint64_t)> toggleFn);

    void updateValueTexts();

    const EffectInfo& info() const;
    int index() const;
    bool valid() const;

private:
    struct ValueBinding {
        ParamHandle handle;
        std::weak_ptr<gs::ui::UITextElement> text;
        std::string fmt;
        const char* label;
    };

    struct SliderBinding {
        const char* paramId;
        std::weak_ptr<gs::ui::UISliderElement> slider;
    };

    ParamHandle* findParam(const char* id);
    static void updateValueText(gs::ui::UITextElement* text, const ParamHandle& handle, const char* fmt);

    EffectInfo m_info{};
    std::vector<ValueBinding> m_valueBindings;
    std::vector<SliderBinding> m_sliderBindings;
};