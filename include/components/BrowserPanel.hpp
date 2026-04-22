
#pragma once

#include <ui/UIBuilder.hpp>
#include <ui/core/UITypes.hpp>
#include <ui/core/DragOverlay.hpp>
#include <audio/plugins/VST3HostPlatform.hpp>

#include "components/BrowserItem.hpp"
#include "audio/AudioEngine.hpp"

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <optional>

struct BrowserEntry {
    enum class Kind { Sample, VST3Effect, VST3Generator, BuiltinEffect, BuiltinGenerator };

    Kind kind;
    std::string displayName;
    std::string typeSuffix;
    std::string path;
    std::string identifier;  
};

struct BrowserPanelConfig {
    float width = 200.0f;
    float fontSize = 11.0f;
    float headerFontSize = 12.0f;
    float buttonHeight = 22.0f;
    float sectionSpacing = 6.0f;

    gs::ui::UIColor panelBg = {0.10f, 0.10f, 0.12f, 1.0f};
    gs::ui::UIColor sectionColor = {0.5f,  0.7f,  1.0f,  1.0f};
    gs::ui::UIColor itemColor = {0.70f, 0.70f, 0.70f, 1.0f};
    gs::ui::UIColor itemHoverColor = {0.95f, 0.95f, 0.95f, 1.0f};
    gs::ui::UIColor typeSuffixColor = {0.45f, 0.45f, 0.50f, 1.0f};
    gs::ui::UIColor btnNormal = {0.20f, 0.25f, 0.30f, 1.0f};
    gs::ui::UIColor btnHover = {0.30f, 0.35f, 0.40f, 1.0f};
    gs::ui::UIColor btnPress = {0.15f, 0.18f, 0.22f, 1.0f};
};

class BrowserPanel {
public:
    using ProbeFn = std::function<bool(const std::wstring& path, std::string& outName, bool& outIsInstrument)>;

    using PickFileFn = std::function<std::optional<std::wstring>(
        PlatformWindowHandle owner,
        const wchar_t* filter,
        const wchar_t* defaultExt
    )>;

    static std::shared_ptr<BrowserPanel> create(
        PlatformWindowHandle owner,
        AudioEngine* engine,
        ProbeFn probeFn,
        PickFileFn pickFileFn = nullptr,
        BrowserPanelConfig cfg = {});

    void addSample(const std::string& filename);
    void addVST3(const std::wstring& path);
    void addBuiltinGenerator(const std::string& name, const std::string& id);
    void addBuiltinEffect(const std::string& name, const std::string& id);

    const std::vector<BrowserEntry>& entries() const { return m_entries; }

    std::shared_ptr<gs::ui::UIElement> widget() const { return m_root; }

    void rebuild();

    void handleDrop(gs::ui::DragPayload payload);

private:
    BrowserPanel() = default;

    void onAddSampleClicked();
    void onAddPluginClicked();

    static std::string formatEntry(const BrowserEntry& e);

    static gs::ui::DragPayload makePayload(const BrowserEntry& e);

    PlatformWindowHandle m_owner = nullptr;
    ProbeFn m_probeFn;
    PickFileFn m_pickFileFn;
    BrowserPanelConfig m_cfg;

    std::vector<BrowserEntry> m_entries;

    std::shared_ptr<gs::ui::widgets::UIStackPanel> m_root;
    std::shared_ptr<gs::ui::widgets::UIScrollArea> m_scrollBody;

    AudioEngine* m_engine = nullptr;
};