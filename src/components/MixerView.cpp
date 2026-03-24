#include "components/MixerView.hpp"

#include <algorithm>
#include <utility>

std::shared_ptr<MixerView> MixerView::create(AudioEngine* engine, MixerViewConfig cfg) {
    auto p = std::shared_ptr<MixerView>(new MixerView(engine, std::move(cfg)));
    p->rebuild();
    return p;
}

std::shared_ptr<gs::ui::widgets::UIStackPanel> MixerView::widget() const {
    return m_root;
}

MixerViewConfig& MixerView::config() {
    return m_cfg;
}

const MixerViewConfig& MixerView::config() const {
    return m_cfg;
}

void MixerView::forceRebuild() {
    rebuild();
}

MixerView::MixerView(AudioEngine* engine, MixerViewConfig cfg)
    : m_engine(engine)
    , m_cfg(std::move(cfg))
    , m_root(gs::ui::widgets::UIStackPanel::create(gs::ui::core::UIOrientation::Horizontal))
{
    m_root->onBeforeEmit = [this](gs::ui::core::UICanvas&) {
        if (needsRebuild()) rebuild();
        else syncStates();
    };
}
bool MixerView::needsRebuild() const {
    int trackCount = m_engine->getTrackCount();
    if (trackCount != m_lastTrackCount) return true;

    auto ids = m_engine->getTrackIds();
    for (size_t i = 0; i < ids.size() && i < m_lastChainVersions.size(); ++i) {
        auto* t = m_engine->getTrack(ids[i]);
        if (t && t->getInsertChain().version() != m_lastChainVersions[i])
            return true;
    }

    auto* master = m_engine->getMasterBus();
    if (master && master->getInsertChain().version() != m_lastMasterVersion)
        return true;

    return false;
}

void MixerView::syncStates() {
    for (auto& [trackId, entry] : m_trackUi) {
        auto* t = m_engine->getTrack(trackId);
        if (!t) continue;

        if (entry.muteBtn) {
            if (t->isMuted()) entry.muteBtn->addState(StyleStates::Muted);
            else entry.muteBtn->removeState(StyleStates::Muted);
        }

        if (entry.soloBtn) {
            if (t->isSoloed()) entry.soloBtn->addState(StyleStates::Solo);
            else entry.soloBtn->removeState(StyleStates::Solo);
        }
    }

    if (m_masterMuteBtn) {
        auto* master = m_engine->getMasterBus();
        if (master) {
            if (master->isMuted()) m_masterMuteBtn->addState(StyleStates::Muted);
            else m_masterMuteBtn->removeState(StyleStates::Muted);
        }
    }
}

void MixerView::snapshotVersions() {
    m_lastTrackCount = m_engine->getTrackCount();
    m_lastChainVersions.clear();

    auto ids = m_engine->getTrackIds();
    for (auto id : ids) {
        auto* t = m_engine->getTrack(id);
        m_lastChainVersions.push_back(t ? t->getInsertChain().version() : 0);
    }

    auto* master = m_engine->getMasterBus();
    m_lastMasterVersion = master ? master->getInsertChain().version() : 0;
}

std::shared_ptr<StereoMeterElement> MixerView::makeMeter(const LevelMeter* source) {
    StereoMeterConfig meterCfg;
    meterCfg.width = m_cfg.meterWidth;
    meterCfg.height = m_cfg.meterMinHeight;

    auto meter = std::make_shared<StereoMeterElement>(meterCfg);
    meter->setMeterSource(source);
    return meter;
}

void MixerView::rebuild() {
    namespace ui = gs::ui;
    snapshotVersions();
    m_trackUi.clear();
    m_masterMuteBtn = nullptr;

    std::vector<std::shared_ptr<gs::ui::core::UIElement>> floaters;
    for (auto& c : m_root->children) {
        if (c && c->floating)
            floaters.push_back(c);
    }

    m_root->children.clear();
    m_root->spacing = 0.0f;
    m_root->padding = gs::ui::core::UIThickness(0);

    m_root->widthMode = gs::ui::core::UISizeMode::Fill;
    m_root->heightMode = gs::ui::core::UISizeMode::Fill;

    auto trackScroll = gs::ui::widgets::UIScrollArea::create(gs::ui::widgets::UIScrollDirection::Horizontal);

    trackScroll->contentOrientation = gs::ui::core::UIOrientation::Horizontal;
    trackScroll->m_childSpacing = m_cfg.stripSpacing;
    trackScroll->scrollbarWidth = 8.0f;
    trackScroll->scrollbarMinThumb = 30.0f;
    trackScroll->scrollbarTrackColor = m_cfg.scrollTrackColor;
    trackScroll->scrollbarThumbColor = m_cfg.scrollThumbColor;
    trackScroll->scrollbarThumbHover = m_cfg.scrollThumbHover;
    trackScroll->scrollbarThumbDrag = m_cfg.scrollThumbDrag;
    trackScroll->scrollSpeed = 60.0f;

    trackScroll->widthMode = gs::ui::core::UISizeMode::Flex;
    trackScroll->preferredWidth = 0.0f;
    trackScroll->flexGrow = 1.0f;
    trackScroll->heightMode = gs::ui::core::UISizeMode::Fill;
    trackScroll->preferredHeight = 0.0f;

    auto trackIds = m_engine->getTrackIds();
    for (auto id : trackIds) {
        auto* track = m_engine->getTrack(id);
        if (track) trackScroll->add(buildTrackStrip(track));
    }

    trackScroll->add(buildAddTrackStrip());
    m_root->add(trackScroll);

    auto* master = m_engine->getMasterBus();
    if (master) m_root->add(buildBusStrip(master, "Master"));

    for (auto& f : floaters)
        m_root->add(f);
    cleanupStaleEditors();
}

int MixerView::inputSourceToIndex(const InputSource& src) {
    switch (src.kind) {
        case InputSource::Kind::None:
            return 0;
        case InputSource::Kind::Hardware:
            return (src.hardwareChannel == 0) ? 1 : 2;
        default:
            return 0;
    }
}

std::shared_ptr<gs::ui::core::UIElement> MixerView::buildTrackStrip(Track* track) {
    namespace ui = gs::ui;

    TrackId trackId = track->getId();
    AudioEngine* engine = m_engine;

    auto stripScroll = gs::ui::widgets::UIScrollArea::create(gs::ui::widgets::UIScrollDirection::Vertical);
    stripScroll->flexContent = true;
    stripScroll->heightMode = gs::ui::core::UISizeMode::Fill;
    stripScroll->widthMode = gs::ui::core::UISizeMode::Fixed;
    stripScroll->preferredWidth = m_cfg.stripWidth;
    stripScroll->m_childSpacing = m_cfg.sectionSpacing;
    stripScroll->fill = m_cfg.stripBg;
    stripScroll->cornerRadius = m_cfg.stripRadius;
    stripScroll->borderColor = m_cfg.stripBorderColor;
    stripScroll->borderThickness = m_cfg.stripBorderThickness;
    stripScroll->padding = gs::ui::core::UIThickness(m_cfg.stripPadding);
    stripScroll->scrollbarWidth = m_cfg.fxScrollbarWidth;
    stripScroll->scrollbarMinThumb = 20.0f;
    stripScroll->scrollbarTrackColor = m_cfg.scrollTrackColor;
    stripScroll->scrollbarThumbColor = m_cfg.scrollThumbColor;
    stripScroll->scrollbarThumbHover = m_cfg.scrollThumbHover;
    stripScroll->scrollbarThumbDrag = m_cfg.scrollThumbDrag;
    stripScroll->scrollSpeed = 40.0f;

    auto nameLabel = ui::Text(track->getName())
        .fitText()
        .height(m_cfg.titleRowHeight)
        .color(m_cfg.headerColor);

    auto removeBtn = ui::Button("X")
        .size(m_cfg.fxFontSize)
        .width(m_cfg.smallBtnWidth)
        .height(m_cfg.titleRowHeight)
        .colors(m_cfg.removeBtnNorm, m_cfg.removeBtnHov, m_cfg.removeBtnPress)
        .onClick([engine, trackId]() { engine->removeTrack(trackId); });

    stripScroll->add(ui::HStack()
        .height(m_cfg.titleRowHeight)
        .mainAlign(gs::ui::core::UIMainAxisAlignment::SpaceBetween)
        .crossAlign(gs::ui::core::UICrossAxisAlignment::Center)
        .add(nameLabel)
        .add(removeBtn)
        .build());

    int currentInput = inputSourceToIndex(track->getInput());

    auto inputDropdown = ui::Dropdown({"None", "HW Input 1", "HW Input 2"})
        .selected(currentInput)
        .fontSize(m_cfg.fxFontSize)
        .width(m_cfg.stripWidth - m_cfg.stripPadding * 2)
        .height(m_cfg.smallBtnHeight)
        .headerColor(m_cfg.btnNormal)
        .headerHoverColor(m_cfg.btnHover)
        .onSelection([engine, trackId](int idx) {
            auto* t = engine->getTrack(trackId);
            if (!t) return;

            switch (idx) {
                case 0: t->setInput({InputSource::Kind::None, 0, 0}); break;
                case 1: t->setInput({InputSource::Kind::Hardware, 0, 0}); break;
                case 2: t->setInput({InputSource::Kind::Hardware, 1, 0}); break;
            }
        });

    stripScroll->add(inputDropdown.build());

    stripScroll->add(ui::Text("Inserts")
        .fitText()
        .color(m_cfg.labelColor)
        .height(m_cfg.insertsTextHeight)
        .build());

    auto fxItems = track->getInsertChain().getView();
    AudioChain* chain = &track->getInsertChain();

    auto fxScroll = gs::ui::widgets::UIScrollArea::create(gs::ui::widgets::UIScrollDirection::Vertical);

    fxScroll->m_childSpacing = 1.0f;
    fxScroll->scrollbarWidth = m_cfg.fxScrollbarWidth;
    fxScroll->scrollbarMinThumb = 12.0f;
    fxScroll->scrollbarTrackColor = m_cfg.scrollTrackColor;
    fxScroll->scrollbarThumbColor = m_cfg.scrollThumbColor;
    fxScroll->scrollbarThumbHover = m_cfg.scrollThumbHover;
    fxScroll->scrollbarThumbDrag = m_cfg.scrollThumbDrag;
    fxScroll->scrollSpeed = 30.0f;

    fxScroll->widthMode = gs::ui::core::UISizeMode::Fill;
    fxScroll->heightMode = gs::ui::core::UISizeMode::Fixed;
    fxScroll->preferredWidth = m_cfg.stripWidth - m_cfg.stripPadding * 2;
    fxScroll->preferredHeight = m_cfg.fxScrollMinHeight;
    fxScroll->flexGrow = 1.0f;
    fxScroll->fill = {0.12f, 0.12f, 0.12f, 0.5f};
    fxScroll->cornerRadius = 2.0f;

    if (fxItems.empty()) {
        fxScroll->add(ui::Text("(none)")
            .size(m_cfg.fxFontSize)
            .color(m_cfg.dimColor)
            .height(m_cfg.fxRowHeight)
            .build());
    } else {
        for (size_t i = 0; i < fxItems.size(); ++i) {
            fxScroll->add(buildFxRow(
                fxItems[i], chain, static_cast<int>(i), static_cast<int>(fxItems.size())));
        }
    }

    stripScroll->add(fxScroll);

    auto addFxDropdown = ui::Dropdown({"Gain/Pan", "Distortion"})
        .placeholder("+ Add FX")
        .autoFlush()
        .noArrow()
        .fontSize(m_cfg.fxFontSize)
        .width(m_cfg.stripWidth - m_cfg.stripPadding * 2)
        .height(m_cfg.smallBtnHeight)
        .headerColor(m_cfg.addBtnNormal)
        .headerHoverColor(m_cfg.addBtnHover)
        .onSelection([this, engine, trackId](int idx) {
            EffectInfo info{};
            switch (idx) {
                case 0: info = engine->addGainPanEffect(trackId); break;
                case 1: info = engine->addDistortionEffect(trackId); break;
            }
            if (info.name) m_effectInfoMap[info.id] = info;
        });

    stripScroll->add(addFxDropdown.build());

    auto meter = makeMeter(&track->getMeter());
    meter->flexGrow = 1.0f;
    stripScroll->add(meter);

    stripScroll->add(ui::Text("Pan")
        .size(m_cfg.labelFontSize)
        .color(m_cfg.labelColor)
        .height(m_cfg.panTextHeight)
        .build());

    stripScroll->add(ui::Slider()
        .range(-1.0f, 1.0f)
        .value(track->getPan())
        .width(m_cfg.stripWidth - m_cfg.stripPadding * 2)
        .height(m_cfg.panHeight)
        .showLabel(false)
        .onValueChanged([engine, trackId](float v) {
            auto* t = engine->getTrack(trackId);
            if (t) t->setPan(v);
        })
        .build());

    stripScroll->add(ui::Text("Fader")
        .size(m_cfg.labelFontSize)
        .color(m_cfg.labelColor)
        .height(m_cfg.faderTextHeight)
        .build());

    stripScroll->add(ui::Slider()
        .range(-60.0f, 12.0f)
        .value(track->getFaderDb())
        .width(m_cfg.stripWidth - m_cfg.stripPadding * 2)
        .height(m_cfg.faderHeight)
        .label("dB")
        .onValueChanged([engine, trackId](float v) {
            auto* t = engine->getTrack(trackId);
            if (t) t->setFaderDb(v);
        })
        .build());

    auto msRow = ui::HStack().spacing(4).height(m_cfg.buttonHeight);

    auto muteBtn = ui::Button("M")
        .size(m_cfg.buttonFontSize)
        .width(m_cfg.buttonWidth).height(m_cfg.buttonHeight)
        .colors(m_cfg.btnNormal, m_cfg.btnHover, m_cfg.btnPress)
        .style(StyleStates::Muted, gs::ui::style::StyleOverrides::Fill(m_cfg.muteActive))
        .onClick([engine, trackId]() {
            auto* t = engine->getTrack(trackId);
            if (t) t->setMute(!t->isMuted());
        })
        .build();

    if (track->isMuted()) muteBtn->addState(StyleStates::Muted);

    auto soloBtn = ui::Button("S")
        .size(m_cfg.buttonFontSize)
        .width(m_cfg.buttonWidth).height(m_cfg.buttonHeight)
        .colors(m_cfg.btnNormal, m_cfg.btnHover, m_cfg.btnPress)
        .style(StyleStates::Solo, gs::ui::style::StyleOverrides::Fill(m_cfg.soloActive))
        .onClick([engine, trackId]() {
            auto* t = engine->getTrack(trackId);
            if (t) t->setSolo(!t->isSoloed());
        })
        .build();

    if (track->isSoloed()) soloBtn->addState(StyleStates::Solo);

    msRow.add(muteBtn).add(soloBtn);
    stripScroll->add(msRow.build());

    m_trackUi[trackId] = {muteBtn, soloBtn};

    return stripScroll;
}

std::shared_ptr<gs::ui::core::UIElement> MixerView::buildFxRow(
    const ChainItemView& item, AudioChain* chain, int index, int total)
{
    namespace ui = gs::ui;

    uint64_t id = item.id;

    auto nameLabel = ui::Text(item.name)
        .size(m_cfg.fxFontSize)
        .color(item.bypass ? m_cfg.dimColor : m_cfg.labelColor)
        .height(m_cfg.fxRowHeight)
        .width(46);

    auto bypBtn = ui::Button(item.bypass ? "Off" : "On")
        .size(m_cfg.fxFontSize - 1)
        .width(20).height(m_cfg.fxRowHeight - 2)
        .colors(
            item.bypass ? m_cfg.removeBtnNorm : m_cfg.addBtnNormal,
            m_cfg.btnHover,
            m_cfg.btnPress)
        .onClick([chain, id]() {
            auto* node = chain->findNode(id);
            if (node) {
                node->bypass.store(
                    !node->bypass.load(std::memory_order_relaxed),
                    std::memory_order_relaxed);
            }
        });

    auto editBtn = ui::Button("E")
        .size(m_cfg.fxFontSize - 1)
        .width(20).height(m_cfg.fxRowHeight - 2)
        .colors(m_cfg.editBtnNormal, m_cfg.editBtnHover, m_cfg.editBtnPress)
        .onClick([this, id]() { openEditor(id); });

    auto upBtn = ui::Button("^")
        .size(m_cfg.fxFontSize - 1)
        .width(20).height(m_cfg.fxRowHeight - 2)
        .colors(m_cfg.btnNormal, m_cfg.btnHover, m_cfg.btnPress);
    if (index > 0)
        upBtn.onClick([chain, id]() { chain->moveUp(id); });

    auto dnBtn = ui::Button("v")
        .size(m_cfg.fxFontSize - 1)
        .width(20).height(m_cfg.fxRowHeight - 2)
        .colors(m_cfg.btnNormal, m_cfg.btnHover, m_cfg.btnPress);
    if (index < total - 1)
        dnBtn.onClick([chain, id]() { chain->moveDown(id); });

    auto rmBtn = ui::Button("X")
        .size(m_cfg.fxFontSize - 1)
        .width(20).height(m_cfg.fxRowHeight - 2)
        .colors(m_cfg.removeBtnNorm, m_cfg.removeBtnHov, m_cfg.removeBtnPress)
        .onClick([this, chain, id]() {
            closeEditor(id);
            chain->removeNode(id);
        });

    return ui::HStack()
        .spacing(0)
        .height(m_cfg.fxRowHeight)
        .crossAlign(gs::ui::core::UICrossAxisAlignment::Center)
        .add(bypBtn)
        .add(editBtn)
        .add(nameLabel)
        .add(upBtn)
        .add(dnBtn)
        .add(rmBtn)
        .build();
}

void MixerView::openEditor(uint64_t nodeId) {
    if (m_openEditors.count(nodeId)) return;

    auto it = m_effectInfoMap.find(nodeId);
    if (it == m_effectInfoMap.end()) return;

    namespace ui = gs::ui;

    EffectBinding binding(it->second);

    auto bypassFn = [this](uint64_t id) {
        auto trackIds = m_engine->getTrackIds();
        for (auto tid : trackIds) {
            auto* t = m_engine->getTrack(tid);
            if (!t) continue;

            auto* node = t->getInsertChain().findNode(id);
            if (node) {
                node->bypass.store(
                    !node->bypass.load(std::memory_order_relaxed),
                    std::memory_order_relaxed);
                return;
            }
        }

        auto* master = m_engine->getMasterBus();
        if (master) {
            auto* node = master->getInsertChain().findNode(id);
            if (node) {
                node->bypass.store(
                    !node->bypass.load(std::memory_order_relaxed),
                    std::memory_order_relaxed);
            }
        }
    };

    EffectStripConfig stripCfg;
    stripCfg.width = m_cfg.editorWidth - 16.0f;
    stripCfg.showBypass = true;

    auto strip = EffectStrip::create(
        std::move(binding), std::move(bypassFn), std::move(stripCfg));

    std::string title = it->second.name ? it->second.name : "Effect";

    auto closeBtn = ui::Button("X")
        .size(m_cfg.fxFontSize)
        .width(m_cfg.smallBtnWidth).height(m_cfg.smallBtnHeight)
        .colors(m_cfg.closeBtnNormal, m_cfg.closeBtnHover, m_cfg.closeBtnPress)
        .onClick([this, nodeId]() { closeEditor(nodeId); });

    auto handleBar = ui::HStack()
        .spacing(4)
        .height(m_cfg.editorHandleH)
        .pad(4, 2, 4, 2)
        .crossAlign(gs::ui::core::UICrossAxisAlignment::Center)
        .add(ui::Text(title)
            .size(m_cfg.labelFontSize)
            .color(m_cfg.headerColor))
        .add(closeBtn)
        .build();

    handleBar->fill = m_cfg.editorHandleBg;
    handleBar->cornerRadius = m_cfg.stripRadius;

    auto wrapper = ui::VStack()
        .spacing(2)
        .pad(4)
        .add(handleBar)
        .add(strip->widget())
        .build();

    wrapper->floating = true;
    wrapper->draggable = true;
    wrapper->bringToFrontOnClick = true;
    wrapper->dragHandleHeight = m_cfg.editorHandleH + 8.0f;

    float stagger = static_cast<float>(m_openEditors.size()) * m_cfg.editorStagger;
    wrapper->offsetX = m_cfg.editorOffsetX + stagger;
    wrapper->offsetY = m_cfg.editorOffsetY + stagger;

    wrapper->preferredWidth = m_cfg.editorWidth;
    if (m_cfg.editorHeight > 0)
        wrapper->preferredHeight = m_cfg.editorHeight;

    wrapper->fill = m_cfg.editorBg;
    wrapper->borderColor = m_cfg.editorBorder;
    wrapper->borderThickness = 1.0f;
    wrapper->cornerRadius = m_cfg.stripRadius;

    m_openEditors[nodeId] = {wrapper, strip};
    m_root->add(wrapper);
}

void MixerView::closeEditor(uint64_t nodeId) {
    auto it = m_openEditors.find(nodeId);
    if (it == m_openEditors.end()) return;

    auto& editorWidget = it->second.widget;
    auto& children = m_root->children;
    children.erase(
        std::remove(children.begin(), children.end(), editorWidget),
        children.end());

    m_openEditors.erase(it);
}

void MixerView::cleanupStaleEditors() {
    std::vector<uint64_t> stale;
    for (auto& [nodeId, editor] : m_openEditors) {
        if (!findNodeAnywhere(nodeId))
            stale.push_back(nodeId);
    }

    for (auto id : stale)
        closeEditor(id);
}

AudioProcessor* MixerView::findNodeAnywhere(uint64_t nodeId) const {
    auto trackIds = m_engine->getTrackIds();
    for (auto tid : trackIds) {
        auto* t = m_engine->getTrack(tid);
        if (!t) continue;

        auto* node = t->getInsertChain().findNode(nodeId);
        if (node) return node;
    }

    auto* master = m_engine->getMasterBus();
    if (master) {
        auto* node = master->getInsertChain().findNode(nodeId);
        if (node) return node;
    }

    return nullptr;
}

std::shared_ptr<gs::ui::core::UIElement> MixerView::buildBusStrip(Bus* bus, const std::string& label) {
    namespace ui = gs::ui;

    BusId busId = bus->getId();
    AudioEngine* engine = m_engine;

    auto stripScroll = gs::ui::widgets::UIScrollArea::create(gs::ui::widgets::UIScrollDirection::Vertical);
    stripScroll->flexContent = true;
    stripScroll->heightMode = gs::ui::core::UISizeMode::Fill;
    stripScroll->widthMode = gs::ui::core::UISizeMode::Fixed;
    stripScroll->preferredWidth = m_cfg.busWidth;
    stripScroll->m_childSpacing = m_cfg.sectionSpacing;
    stripScroll->fill = m_cfg.masterStripBg;
    stripScroll->cornerRadius = m_cfg.stripRadius;
    stripScroll->borderColor = m_cfg.stripBorderColor;
    stripScroll->borderThickness = m_cfg.stripBorderThickness;
    stripScroll->padding = gs::ui::core::UIThickness(m_cfg.stripPadding);
    stripScroll->scrollbarWidth = m_cfg.fxScrollbarWidth;
    stripScroll->scrollbarMinThumb = 20.0f;
    stripScroll->scrollbarTrackColor = m_cfg.scrollTrackColor;
    stripScroll->scrollbarThumbColor = m_cfg.scrollThumbColor;
    stripScroll->scrollbarThumbHover = m_cfg.scrollThumbHover;
    stripScroll->scrollbarThumbDrag = m_cfg.scrollThumbDrag;
    stripScroll->scrollSpeed = 40.0f;

    stripScroll->add(ui::Text(label)
        .fitText()
        .color(m_cfg.headerColor)
        .height(m_cfg.titleRowHeight)
        .build());

    stripScroll->add(ui::Text("Inserts")
        .fitText()
        .color(m_cfg.labelColor)
        .height(m_cfg.insertsTextHeight)
        .build());

    auto fxItems = bus->getInsertChain().getView();
    AudioChain* chain = &bus->getInsertChain();

    auto fxScroll = gs::ui::widgets::UIScrollArea::create(gs::ui::widgets::UIScrollDirection::Vertical);

    fxScroll->m_childSpacing = 1.0f;
    fxScroll->scrollbarWidth = m_cfg.fxScrollbarWidth;
    fxScroll->scrollbarMinThumb = 12.0f;
    fxScroll->scrollbarTrackColor = m_cfg.scrollTrackColor;
    fxScroll->scrollbarThumbColor = m_cfg.scrollThumbColor;
    fxScroll->scrollbarThumbHover = m_cfg.scrollThumbHover;
    fxScroll->scrollbarThumbDrag = m_cfg.scrollThumbDrag;
    fxScroll->scrollSpeed = 30.0f;
    fxScroll->widthMode = gs::ui::core::UISizeMode::Fill;
    fxScroll->heightMode = gs::ui::core::UISizeMode::Fixed;
    fxScroll->preferredWidth = m_cfg.busWidth - m_cfg.stripPadding * 2;
    fxScroll->preferredHeight = m_cfg.fxScrollMinHeight;
    fxScroll->flexGrow = 1.0f;
    fxScroll->fill = {0.12f, 0.12f, 0.12f, 0.5f};
    fxScroll->cornerRadius = 2.0f;

    if (fxItems.empty()) {
        fxScroll->add(ui::Text("(none)")
            .size(m_cfg.fxFontSize)
            .color(m_cfg.dimColor)
            .height(m_cfg.fxRowHeight)
            .build());
    } else {
        for (size_t i = 0; i < fxItems.size(); ++i) {
            fxScroll->add(buildFxRow(
                fxItems[i], chain, static_cast<int>(i), static_cast<int>(fxItems.size())));
        }
    }

    stripScroll->add(fxScroll);

    auto meter = makeMeter(&bus->getMeter());
    meter->flexGrow = 1.0f;
    stripScroll->add(meter);

    stripScroll->add(ui::Text("Fader")
        .size(m_cfg.labelFontSize)
        .color(m_cfg.labelColor)
        .height(m_cfg.faderTextHeight)
        .build());

    stripScroll->add(ui::Slider()
        .range(-60.0f, 12.0f)
        .value(bus->getFaderDb())
        .width(m_cfg.busWidth - m_cfg.stripPadding * 2)
        .height(m_cfg.faderHeight)
        .label("dB")
        .onValueChanged([engine, busId](float v) {
            auto* b = engine->getBus(busId);
            if (b) b->setFaderDb(v);
        })
        .build());

    auto masterMuteBtn = ui::Button("M")
        .size(m_cfg.buttonFontSize)
        .width(m_cfg.buttonWidth).height(m_cfg.buttonHeight)
        .colors(m_cfg.btnNormal, m_cfg.btnHover, m_cfg.btnPress)
        .style(StyleStates::Muted, gs::ui::style::StyleOverrides::Fill(m_cfg.muteActive))
        .onClick([engine, busId]() {
            auto* b = engine->getBus(busId);
            if (b) b->setMute(!b->isMuted());
        })
        .build();

    if (bus->isMuted()) masterMuteBtn->addState(StyleStates::Muted);
    m_masterMuteBtn = masterMuteBtn;
    stripScroll->add(masterMuteBtn);

    return stripScroll;
}

std::shared_ptr<gs::ui::core::UIElement> MixerView::buildAddTrackStrip() {
    namespace ui = gs::ui;
    AudioEngine* engine = m_engine;

    auto stripScroll = gs::ui::widgets::UIScrollArea::create(gs::ui::widgets::UIScrollDirection::Vertical);
    stripScroll->flexContent = true;
    stripScroll->heightMode = gs::ui::core::UISizeMode::Fill;
    stripScroll->widthMode = gs::ui::core::UISizeMode::Fixed;
    stripScroll->preferredWidth = m_cfg.stripWidth;
    stripScroll->m_childSpacing = m_cfg.sectionSpacing;
    stripScroll->fill = {0.1f, 0.1f, 0.1f, 0.6f};
    stripScroll->cornerRadius = m_cfg.stripRadius;
    stripScroll->borderColor = m_cfg.stripBorderColor;
    stripScroll->borderThickness = m_cfg.stripBorderThickness;
    stripScroll->padding = gs::ui::core::UIThickness(m_cfg.stripPadding);
    stripScroll->scrollbarWidth = m_cfg.fxScrollbarWidth;
    stripScroll->scrollbarMinThumb = 20.0f;
    stripScroll->scrollbarTrackColor = m_cfg.scrollTrackColor;
    stripScroll->scrollbarThumbColor = m_cfg.scrollThumbColor;
    stripScroll->scrollbarThumbHover = m_cfg.scrollThumbHover;
    stripScroll->scrollbarThumbDrag = m_cfg.scrollThumbDrag;
    stripScroll->scrollSpeed = 40.0f;
    auto centered = ui::VStack()
        .spacing(m_cfg.sectionSpacing)
        .mainAlign(gs::ui::core::UIMainAxisAlignment::Center)
        .crossAlign(gs::ui::core::UICrossAxisAlignment::Center)
        .widthMode(gs::ui::core::UISizeMode::Fill)
        .heightMode(gs::ui::core::UISizeMode::Fill)
        .add(ui::Text("+ New Track")
            .fitText()
            .color(m_cfg.addBtnNormal)
            .height(m_cfg.titleRowHeight))
        .add(ui::Button("Empty")
            .size(m_cfg.buttonFontSize)
            .width(m_cfg.stripWidth - m_cfg.stripPadding * 2)
            .height(m_cfg.buttonHeight)
            .colors(m_cfg.addBtnNormal, m_cfg.addBtnHover, m_cfg.addBtnPress)
            .onClick([engine]() {
                engine->createTrack("Track", 2, {InputSource::Kind::None, 0, 0});
            }))
        .add(ui::Button("HW Input 1")
            .size(m_cfg.buttonFontSize)
            .width(m_cfg.stripWidth - m_cfg.stripPadding * 2)
            .height(m_cfg.buttonHeight)
            .colors(m_cfg.addBtnNormal, m_cfg.addBtnHover, m_cfg.addBtnPress)
            .onClick([engine]() {
                engine->createTrack("Audio", 2, {InputSource::Kind::Hardware, 0, 0});
            }))
        .add(ui::Button("HW Input 2")
            .size(m_cfg.buttonFontSize)
            .width(m_cfg.stripWidth - m_cfg.stripPadding * 2)
            .height(m_cfg.buttonHeight)
            .colors(m_cfg.addBtnNormal, m_cfg.addBtnHover, m_cfg.addBtnPress)
            .onClick([engine]() {
                engine->createTrack("Audio", 2, {InputSource::Kind::Hardware, 1, 0});
            }))
        .build();
    stripScroll->add(centered);
    return stripScroll;
}