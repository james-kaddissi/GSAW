#include "components/ChannelRackView.hpp"

#include "audio/library/generators/SubtractiveSynth.hpp"
#include "audio/library/generators/BagpipeSynth.hpp"

#include <algorithm>
#include <utility>

std::shared_ptr<ChannelRackView> ChannelRackView::create(
    AudioEngine* engine,
    ChannelRack* rack,
    ChannelRackViewConfig cfg
) {
    auto p = std::shared_ptr<ChannelRackView>(
        new ChannelRackView(engine, rack, std::move(cfg))
    );
    p->rebuild();
    return p;
}

std::shared_ptr<gs::ui::widgets::UIStackPanel> ChannelRackView::widget() const {
    return m_root;
}

ChannelRackViewConfig& ChannelRackView::config() {
    return m_cfg;
}

const ChannelRackViewConfig& ChannelRackView::config() const {
    return m_cfg;
}

void ChannelRackView::forceRebuild() {
    rebuild();
}

ChannelRackView::ChannelRackView(AudioEngine* engine, ChannelRack* rack, ChannelRackViewConfig cfg)
    : m_engine(engine)
    , m_rack(rack)
    , m_cfg(std::move(cfg))
    , m_root(gs::ui::widgets::UIStackPanel::create(gs::ui::UIOrientation::Vertical)) {
    m_root->onBeforeEmit = [this](gs::ui::UICanvas&) {
        if (needsRebuild()) {
            rebuild();
        } else {
            syncStates();
        }
    };
}

bool ChannelRackView::needsRebuild() const {
    if (m_rack->getChannelCount() != m_lastChannelCount) {
        return true;
    }
    if (m_rack->getFocusedChannelId() != m_lastFocusedId) {
        return true;
    }
    return false;
}

void ChannelRackView::syncStates() {
    for (auto& [chId, entry] : m_channelUi) {
        auto* ch = m_rack->getChannel(chId);
        if (!ch) {
            continue;
        }

        if (entry.muteBtn) {
            if (ch->isMuted()) {
                entry.muteBtn->addState(StyleStates::Muted);
            } else {
                entry.muteBtn->removeState(StyleStates::Muted);
            }
        }

        if (entry.armedBtn) {
            if (ch->isArmed()) {
                entry.armedBtn->addState(StyleStates::Armed);
            } else {
                entry.armedBtn->removeState(StyleStates::Armed);
            }
        }

        if (entry.rowPanel) {
            if (m_rack->isFocused(chId)) {
                entry.rowPanel->addState(StyleStates::Focused);
            } else {
                entry.rowPanel->removeState(StyleStates::Focused);
            }
        }
    }
}

void ChannelRackView::rebuild() {
    m_lastChannelCount = m_rack->getChannelCount();
    m_lastFocusedId = m_rack->getFocusedChannelId();
    m_channelUi.clear();

    std::vector<std::shared_ptr<gs::ui::UIElement>> floaters;
    for (auto& c : m_root->children) {
        if (c && c->floating) {
            floaters.push_back(c);
        }
    }

    m_root->children.clear();
    m_root->spacing = 2.0f;
    m_root->padding = gs::ui::UIThickness(m_cfg.sectionPad);
    m_root->fill = m_cfg.bg;
    m_root->widthMode = gs::ui::UISizeMode::Fill;
    m_root->heightMode = gs::ui::UISizeMode::Flex;
    m_root->flexGrow = 1.0f;

    m_root->add(buildTransportBar());

    auto scroll = gs::ui::widgets::UIScrollArea::create(gs::ui::widgets::UIScrollDirection::Vertical);
    scroll->m_childSpacing = m_cfg.rowSpacing;
    scroll->scrollbarWidth = 6.0f;
    scroll->scrollbarMinThumb = 12.0f;
    scroll->scrollbarTrackColor = m_cfg.scrollTrackColor;
    scroll->scrollbarThumbColor = m_cfg.scrollThumbColor;
    scroll->scrollbarThumbHover = m_cfg.scrollThumbHover;
    scroll->scrollbarThumbDrag = m_cfg.scrollThumbDrag;
    scroll->scrollSpeed = 40.0f;
    scroll->widthMode = gs::ui::UISizeMode::Fill;
    scroll->heightMode = gs::ui::UISizeMode::Flex;
    scroll->flexGrow = 1.0f;
    scroll->preferredHeight = 0.0f; 

    auto channelIds = m_rack->getChannelIds();
    for (size_t i = 0; i < channelIds.size(); ++i) {
        auto* ch = m_rack->getChannel(channelIds[i]);
        if (ch) {
            scroll->add(buildChannelRow(ch, i % 2 == 1));
        }
    }

    m_root->add(scroll);
    m_root->add(buildAddChannelRow());

    for (auto& f : floaters) {
        m_root->add(f);
    }
}

std::shared_ptr<gs::ui::UIElement> ChannelRackView::buildTransportBar() {
    namespace ui = gs::ui;
    ChannelRack* rack = m_rack;

    auto bar = ui::HStack()
        .spacing(6)
        .height(m_cfg.transportHeight)
        .crossAlign(gs::ui::UICrossAxisAlignment::Center);

    bar.add(
        ui::Button("Play")
            .size(m_cfg.fontSize)
            .width(50)
            .height(m_cfg.transportHeight - 4)
            .colors(m_cfg.playColor, m_cfg.playHover, m_cfg.btnPress)
            .onClick([rack]() { rack->play(); })
    );

    bar.add(
        ui::Button("Stop")
            .size(m_cfg.fontSize)
            .width(50)
            .height(m_cfg.transportHeight - 4)
            .colors(m_cfg.stopColor, m_cfg.stopHover, m_cfg.btnPress)
            .onClick([rack]() { rack->stop(); })
    );

    bar.add(
        ui::Text("BPM")
            .size(m_cfg.smallFontSize)
            .color(m_cfg.labelColor)
            .height(m_cfg.transportHeight - 4)
    );

    bar.add(
        ui::Slider()
            .range(40.0f, 300.0f)
            .value(static_cast<float>(rack->getBpm()))
            .width(120)
            .height(m_cfg.transportHeight - 8)
            .showLabel(false)
            .onValueChanged([rack](float v) { rack->setBpm(static_cast<double>(v)); })
    );

    auto loopColor = rack->isLooping() ? m_cfg.playColor : m_cfg.btnNormal;
    bar.add(
        ui::Button("Loop")
            .size(m_cfg.fontSize)
            .width(44)
            .height(m_cfg.transportHeight - 4)
            .colors(loopColor, m_cfg.btnHover, m_cfg.btnPress)
            .onClick([rack]() { rack->setLooping(!rack->isLooping()); })
    );

    return bar.build();
}

std::shared_ptr<gs::ui::UIElement> ChannelRackView::buildChannelRow(Channel* ch, bool alt) {
    namespace ui = gs::ui;

    ChannelId chId = ch->getId();
    ChannelRack* rack = m_rack;
    AudioEngine* engine = m_engine;
    bool focused = rack->isFocused(chId);

    auto row = ui::HStack()
        .spacing(4)
        .height(m_cfg.rowHeight)
        .crossAlign(gs::ui::UICrossAxisAlignment::Center);

    auto rowPanel = row.build();
    rowPanel->fill = alt ? m_cfg.rowBgAlt : m_cfg.rowBg;

    rowPanel->styles.set(
        StyleStates::Focused,
        gs::ui::style::StyleOverrides::Border(m_cfg.focusBorder, m_cfg.focusBorderW)
    );

    if (focused) {
        rowPanel->addState(StyleStates::Focused);
    }

    rowPanel->onClick = [rack, chId]() {
        rack->setFocusedChannel(chId);
    };

    auto muteBtn = ui::Button("M")
        .size(m_cfg.smallFontSize)
        .width(m_cfg.buttonWidth)
        .height(m_cfg.rowHeight - 6)
        .colors(m_cfg.btnNormal, m_cfg.btnHover, m_cfg.btnPress)
        .style(StyleStates::Muted, gs::ui::style::StyleOverrides::Fill(m_cfg.muteActive))
        .onClick([rack, chId]() {
            auto* c = rack->getChannel(chId);
            if (c) {
                c->setMute(!c->isMuted());
            }
        })
        .build();

    if (ch->isMuted()) {
        muteBtn->addState(StyleStates::Muted);
    }

    rowPanel->add(muteBtn);

    auto armedBtn = ui::Button("A")
        .size(m_cfg.smallFontSize)
        .width(m_cfg.buttonWidth)
        .height(m_cfg.rowHeight - 6)
        .colors(m_cfg.btnNormal, m_cfg.btnHover, m_cfg.btnPress)
        .style(StyleStates::Armed, gs::ui::style::StyleOverrides::Fill(m_cfg.armedActive))
        .onClick([rack, chId]() {
            auto* c = rack->getChannel(chId);
            if (c) {
                c->setArmed(!c->isArmed());
            }
        })
        .build();

    if (ch->isArmed()) {
        armedBtn->addState(StyleStates::Armed);
    }

    rowPanel->add(armedBtn);

    m_channelUi[chId] = {muteBtn, armedBtn, rowPanel};

    std::string genName = ch->getGenerator() ? ch->getGenerator()->name() : "?";
    std::string label = ch->getName() + " (" + genName + ")";

    rowPanel->add(
        ui::Text(label)
            .size(m_cfg.fontSize)
            .color(m_cfg.headerColor)
            .width(m_cfg.nameWidth)
            .height(m_cfg.rowHeight - 6)
            .build()
    );

    rowPanel->add(
        ui::Button("E")
            .size(m_cfg.smallFontSize)
            .width(m_cfg.buttonWidth)
            .height(m_cfg.rowHeight - 6)
            .colors(m_cfg.editBtnNormal, m_cfg.editBtnHover, m_cfg.editBtnPress)
            .onClick([this, chId]() { openGeneratorEditor(chId); })
            .build()
    );

    rowPanel->add(
        ui::Slider()
            .range(0.0f, 2.0f)
            .value(ch->getVolume())
            .width(m_cfg.sliderWidth)
            .height(m_cfg.rowHeight - 10)
            .showLabel(false)
            .onValueChanged([rack, chId](float v) {
                auto* c = rack->getChannel(chId);
                if (c) {
                    c->setVolume(v);
                }
            })
            .build()
    );

    rowPanel->add(
        ui::Slider()
            .range(-1.0f, 1.0f)
            .value(ch->getPan())
            .width(m_cfg.panSliderWidth)
            .height(m_cfg.rowHeight - 10)
            .showLabel(false)
            .onValueChanged([rack, chId](float v) {
                auto* c = rack->getChannel(chId);
                if (c) {
                    c->setPan(v);
                }
            })
            .build()
    );

    auto trackIds = engine->getTrackIds();
    std::vector<std::string> trackNames;
    int currentRouteIdx = 0;

    for (size_t i = 0; i < trackIds.size(); ++i) {
        auto* t = engine->getTrack(trackIds[i]);
        trackNames.push_back(t ? t->getName() : "Track");
        if (trackIds[i] == ch->getOutputTrackId()) {
            currentRouteIdx = static_cast<int>(i);
        }
    }

    if (trackNames.empty()) {
        trackNames.push_back("(none)");
    }

    rowPanel->add(
        ui::Dropdown(trackNames)
            .selected(currentRouteIdx)
            .fontSize(m_cfg.smallFontSize)
            .width(m_cfg.routeWidth)
            .height(m_cfg.rowHeight - 8)
            .headerColor(m_cfg.btnNormal)
            .headerHoverColor(m_cfg.btnHover)
            .onSelection([rack, engine, chId](int idx) {
                auto ids = engine->getTrackIds();
                if (idx >= 0 && idx < static_cast<int>(ids.size())) {
                    auto* c = rack->getChannel(chId);
                    if (c) {
                        c->setOutputTrackId(ids[idx]);
                    }
                }
            })
            .build()
    );

    auto patternIds = rack->getPatternIds();
    std::vector<std::string> patNames;
    patNames.push_back("(none)");
    int currentPatIdx = 0;

    for (size_t i = 0; i < patternIds.size(); ++i) {
        auto* pat = rack->getPattern(patternIds[i]);
        patNames.push_back(pat ? pat->getName() : "Pattern");
        if (patternIds[i] == rack->getChannelPattern(chId)) {
            currentPatIdx = static_cast<int>(i + 1);
        }
    }

    rowPanel->add(
        ui::Dropdown(patNames)
            .selected(currentPatIdx)
            .fontSize(m_cfg.smallFontSize)
            .width(m_cfg.routeWidth)
            .height(m_cfg.rowHeight - 8)
            .headerColor(m_cfg.btnNormal)
            .headerHoverColor(m_cfg.btnHover)
            .onSelection([rack, chId, patternIds](int idx) {
                if (idx <= 0) {
                    rack->setChannelPattern(chId, kInvalidPatternId);
                } else {
                    int pi = idx - 1;
                    if (pi < static_cast<int>(patternIds.size())) {
                        rack->setChannelPattern(chId, patternIds[pi]);
                    }
                }
            })
            .build()
    );

    rowPanel->add(
        ui::Button("X")
            .size(m_cfg.smallFontSize)
            .width(m_cfg.buttonWidth)
            .height(m_cfg.rowHeight - 6)
            .colors(m_cfg.removeBtnNorm, m_cfg.removeBtnHov, m_cfg.removeBtnPress)
            .onClick([rack, chId]() { rack->removeChannel(chId); })
            .build()
    );

    return rowPanel;
}

std::shared_ptr<gs::ui::UIElement> ChannelRackView::buildAddChannelRow() {
    namespace ui = gs::ui;
    ChannelRack* rack = m_rack;
    AudioEngine* engine = m_engine;

    return ui::Dropdown({"SubtractiveSynth", "BagpipeSynth"})
        .placeholder("+ Add Channel")
        .autoFlush()
        .noArrow()
        .fontSize(m_cfg.fontSize)
        .width(m_cfg.totalWidth)
        .height(m_cfg.rowHeight)
        .headerColor(m_cfg.addBtnNormal)
        .headerHoverColor(m_cfg.addBtnHover)
        .onSelection([rack, engine](int idx) {
            auto trackIds = engine->getTrackIds();
            TrackId outTrack = trackIds.empty() ? kInvalidTrackId : trackIds.front();

            switch (idx) {
                case 0: {
                    auto gen = std::make_unique<SubtractiveSynth>(16);
                    rack->addChannel("SubtractiveSynth", std::move(gen), outTrack);
                    break;
                }
                case 1: {
                    auto gen = std::make_unique<BagpipeSynth>(16);
                    rack->addChannel("BagpipeSynth", std::move(gen), outTrack);
                    break;
                }
            }
        })
        .build();
}

void ChannelRackView::openGeneratorEditor(ChannelId chId) {
    if (m_openEditors.count(chId)) {
        return;
    }

    EffectInfo info = m_rack->getChannelGeneratorInfo(chId);
    if (!info.name) {
        return;
    }

    namespace ui = gs::ui;

    EffectBinding binding(info);

    EffectStripConfig stripCfg;
    stripCfg.width = m_cfg.editorWidth - 16.0f;
    stripCfg.showBypass = false;

    auto strip = EffectStrip::create(
        std::move(binding),
        [](uint64_t) {},
        std::move(stripCfg)
    );

    Channel* ch = m_rack->getChannel(chId);
    std::string title = ch ? ch->getName() : "Generator";

    auto closeBtn = ui::Button("X")
        .size(m_cfg.smallFontSize)
        .width(m_cfg.buttonWidth)
        .height(22)
        .colors(m_cfg.closeBtnNormal, m_cfg.closeBtnHover, m_cfg.closeBtnPress)
        .onClick([this, chId]() { closeGeneratorEditor(chId); });

    auto handleBar = ui::HStack()
        .spacing(4)
        .height(m_cfg.editorHandleH)
        .pad(4, 2, 4, 2)
        .crossAlign(gs::ui::UICrossAxisAlignment::Center)
        .add(
            ui::Text(title)
                .size(m_cfg.fontSize)
                .color(m_cfg.headerColor)
        )
        .add(closeBtn)
        .build();

    handleBar->fill = m_cfg.editorHandleBg;

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

    wrapper->fill = m_cfg.editorBg;
    wrapper->borderColor = m_cfg.editorBorder;
    wrapper->borderThickness = 1.0f;
    wrapper->cornerRadius = m_cfg.stripRadius;

    m_openEditors[chId] = {wrapper, strip};
    m_root->add(wrapper);
}

void ChannelRackView::closeGeneratorEditor(ChannelId chId) {
    auto it = m_openEditors.find(chId);
    if (it == m_openEditors.end()) {
        return;
    }

    auto& children = m_root->children;
    children.erase(
        std::remove(children.begin(), children.end(), it->second.widget),
        children.end()
    );
    m_openEditors.erase(it);
}