#include "components/BrowserPanel.hpp"

#include <algorithm>
#include <filesystem>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  #include <commdlg.h>
#endif

namespace ui = gs::ui;
using gs::ui::core::DragPayload;
using gs::ui::core::UIColor;

std::shared_ptr<BrowserPanel> BrowserPanel::create(
    PlatformWindowHandle owner,
    ProbeFn probeFn,
    PickFileFn pickFileFn,
    BrowserPanelConfig cfg)
{
    auto bp = std::shared_ptr<BrowserPanel>(new BrowserPanel());
    bp->m_owner = owner;
    bp->m_probeFn = std::move(probeFn);
    bp->m_pickFileFn = std::move(pickFileFn);
    bp->m_cfg = cfg;

    bp->m_root = gs::ui::widgets::UIStackPanel::create(gs::ui::core::UIOrientation::Vertical);
    bp->m_root->spacing = 0.0f;
    bp->m_root->fill = cfg.panelBg;
    bp->m_root->preferredWidth = cfg.width;
    bp->m_root->widthMode = gs::ui::core::UISizeMode::Fixed;
    bp->m_root->heightMode = gs::ui::core::UISizeMode::Fill;
    bp->m_root->cornerRadius = 4.0f;
    bp->m_root->borderColor = {0.25f, 0.25f, 0.30f, 1.0f};
    bp->m_root->borderThickness = 1.0f;

    bp->addBuiltinGenerator("SubSynth", "SubSynth");

    bp->rebuild();
    return bp;
}

void BrowserPanel::addSample(const std::string& filename) {
    BrowserEntry e;
    e.kind = BrowserEntry::Kind::Sample;
    e.displayName = filename;
    e.typeSuffix = "";  // for now samples just show filename.ext
    e.path = filename;
    m_entries.push_back(std::move(e));
    rebuild();
}

void BrowserPanel::addVST3(const std::wstring& path) {
    if (!m_probeFn) return;

    std::string name;
    bool isInstrument = false;
    if (!m_probeFn(path, name, isInstrument)) return;

    std::string narrowPath;
    narrowPath.reserve(path.size());
    for (wchar_t wc : path)
        narrowPath.push_back(wc < 0x80 ? static_cast<char>(wc) : '?');

    BrowserEntry e;
    e.kind = isInstrument ? BrowserEntry::Kind::VST3Generator : BrowserEntry::Kind::VST3Effect;
    e.displayName = name;
    e.typeSuffix = "VST3 Plugin";
    e.path = narrowPath;
    m_entries.push_back(std::move(e));
    rebuild();
}

void BrowserPanel::addBuiltinGenerator(const std::string& name, const std::string& id) {
    BrowserEntry e;
    e.kind        = BrowserEntry::Kind::BuiltinGenerator;
    e.displayName = name;
    e.typeSuffix  = "GSAW";
    e.identifier  = id;
    m_entries.push_back(std::move(e));
}

void BrowserPanel::addBuiltinEffect(const std::string& name, const std::string& id) {
    BrowserEntry e;
    e.kind = BrowserEntry::Kind::BuiltinEffect;
    e.displayName = name;
    e.typeSuffix = "GSAW";
    e.identifier = id;
    m_entries.push_back(std::move(e));
}

std::string BrowserPanel::formatEntry(const BrowserEntry& e) {
    if (e.kind == BrowserEntry::Kind::Sample) {
        return e.displayName;
    }
    if (e.typeSuffix.empty()) return e.displayName;
    return e.displayName + "  " + e.typeSuffix;
}

DragPayload BrowserPanel::makePayload(const BrowserEntry& e) {
    DragPayload p;
    p.displayName = e.displayName;
    p.path        = e.path;
    p.identifier  = e.identifier;

    switch (e.kind) {
        case BrowserEntry::Kind::Sample:
            p.kind = DragPayload::Kind::Sample;
            break;
        case BrowserEntry::Kind::VST3Effect:
            p.kind = DragPayload::Kind::VST3Plugin;
            break;
        case BrowserEntry::Kind::VST3Generator:
            p.kind = DragPayload::Kind::VST3Plugin;
            break;
        case BrowserEntry::Kind::BuiltinEffect:
            p.kind = DragPayload::Kind::BuiltinEffect;
            break;
        case BrowserEntry::Kind::BuiltinGenerator:
            p.kind = DragPayload::Kind::BuiltinGenerator;
            break;
    }
    return p;
}

void BrowserPanel::onAddSampleClicked() {
#ifdef _WIN32
    if (m_pickFileFn) {
        auto result = m_pickFileFn(
            m_owner,
            L"Audio Files (*.wav;*.mp3;*.flac;*.ogg;*.aif;*.aiff)\0"
            L"*.wav;*.mp3;*.flac;*.ogg;*.aif;*.aiff\0"
            L"All Files (*.*)\0*.*\0\0",
            L"wav");
        if (result) {
            std::filesystem::path fp(result.value());
            std::string filename = fp.filename().string();
            addSample(filename);
        }
        return;
    }

    OPENFILENAMEW ofn{};
    wchar_t fileName[MAX_PATH] = L"";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(m_owner);
    ofn.lpstrFilter = L"Audio Files (*.wav;*.mp3;*.flac;*.ogg)\0"
                      L"*.wav;*.mp3;*.flac;*.ogg\0"
                      L"All Files (*.*)\0*.*\0\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = L"wav";
    if (!GetOpenFileNameW(&ofn)) return;

    std::filesystem::path fp(fileName);
    addSample(fp.filename().string());
#endif
}

void BrowserPanel::onAddPluginClicked() {
#ifdef _WIN32
    std::optional<std::wstring> result;

    if (m_pickFileFn) {
        result = m_pickFileFn(
            m_owner,
            L"VST3 Plugin (*.vst3)\0*.vst3\0All Files (*.*)\0*.*\0\0",
            L"vst3");
    } else {
        OPENFILENAMEW ofn{};
        wchar_t fileName[MAX_PATH] = L"";
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = static_cast<HWND>(m_owner);
        ofn.lpstrFilter = L"VST3 Plugin (*.vst3)\0*.vst3\0All Files (*.*)\0*.*\0\0";
        ofn.lpstrFile = fileName;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_NOCHANGEDIR;
        ofn.lpstrDefExt = L"vst3";
        if (GetOpenFileNameW(&ofn))
            result = std::wstring(fileName);
    }

    if (result)
        addVST3(result.value());
#endif
}

void BrowserPanel::rebuild() {
    if (!m_root) return;

    m_root->children.clear();

    auto header = ui::HStack()
        .spacing(4)
        .height(m_cfg.buttonHeight + 8.0f)
        .pad(6, 4, 6, 4)
        .crossAlign(gs::ui::core::UICrossAxisAlignment::Center)
        .add(ui::Button("+ Sample")
            .size(m_cfg.fontSize)
            .width(80).height(m_cfg.buttonHeight)
            .colors(m_cfg.btnNormal, m_cfg.btnHover, m_cfg.btnPress)
            .onClick([this]() { onAddSampleClicked(); }))
        .add(ui::Button("+ Plugin")
            .size(m_cfg.fontSize)
            .width(80).height(m_cfg.buttonHeight)
            .colors(m_cfg.btnNormal, m_cfg.btnHover, m_cfg.btnPress)
            .onClick([this]() { onAddPluginClicked(); }))
        .build();

    m_root->add(header);

    std::vector<const BrowserEntry*> samples, effects, generators;

    for (auto& e : m_entries) {
        switch (e.kind) {
            case BrowserEntry::Kind::Sample:
                samples.push_back(&e);
                break;
            case BrowserEntry::Kind::VST3Effect:
            case BrowserEntry::Kind::BuiltinEffect:
                effects.push_back(&e);
                break;
            case BrowserEntry::Kind::VST3Generator:
            case BrowserEntry::Kind::BuiltinGenerator:
                generators.push_back(&e);
                break;
        }
    }

    auto addSection = [&](const std::string& title, const std::vector<const BrowserEntry*>& items) {
        auto sectionLabel = ui::Text(title)
            .size(m_cfg.headerFontSize)
            .color(m_cfg.sectionColor)
            .height(m_cfg.headerFontSize + 8.0f)
            .build();
        sectionLabel->padding.left = 6.0f;
        sectionLabel->padding.top  = m_cfg.sectionSpacing;
        m_root->add(sectionLabel);

        if (items.empty()) {
            auto emptyLabel = ui::Text("  (empty)")
                .size(m_cfg.fontSize - 1.0f)
                .color({0.4f, 0.4f, 0.4f, 1.0f})
                .height(m_cfg.fontSize + 4.0f)
                .build();
            emptyLabel->padding.left = 12.0f;
            m_root->add(emptyLabel);
            return;
        }

        for (auto* entry : items) {
            std::string label = formatEntry(*entry);
            auto payload = makePayload(*entry);

            auto item = std::make_shared<BrowserItem>(
                label, std::move(payload),
                m_cfg.fontSize, m_cfg.itemColor, m_cfg.itemHoverColor);
            item->padding.left = 12.0f;
            m_root->add(item);
        }
    };

    addSection("Samples",    samples);
    addSection("Effects",    effects);
    addSection("Generators", generators);
}