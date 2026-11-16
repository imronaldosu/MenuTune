/**
 * MenuTune - replace the Geometry Dash main menu music with a song from the
 * player's Saved Songs collection (GD's own custom-menu-song mechanism).
 *
 * Private local use only. Not for submission to the Geode index.
 */
#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/ui/ListView.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/TextInput.hpp>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace {
    constexpr float POPUP_W = 440.f;
    constexpr float POPUP_H = 300.f;
    constexpr float ROW_W   = 400.f;
    constexpr float ROW_H   = 40.f;
    constexpr float LIST_W  = 400.f;
    constexpr float LIST_H  = 160.f;
    constexpr float LIST_X  = 20.f;
    constexpr float LIST_Y  = 45.f;

    std::string toLowerCopy(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return s;
    }

    std::string formatTime(int seconds) {
        if (seconds < 0) seconds = 0;
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%d:%02d", seconds / 60, seconds % 60);
        return buf;
    }

    // Scale a label to a target height without exceeding maxW (min floor).
    void fitLabel(CCLabelBMFont* label, float maxW, float targetH, float minScale) {
        auto const size = label->getContentSize();
        float scale = targetH / (size.height > 0.f ? size.height : 1.f);
        if (size.width * scale > maxW) {
            scale = maxW / (size.width > 0.f ? size.width : 1.f);
        }
        if (scale < minScale) scale = minScale;
        label->setScale(scale);
    }
}

// One row in the song list. Built once per song and reused across filter
// rebuilds; the ListView reparents it into recycled table view cells.
class SongRow : public CCNode {
public:
    int m_songID = 0;
    std::string m_searchBlob;
    CCLabelBMFont* m_title = nullptr;

    static SongRow* create(SongInfoObject* song, CCObject* target, SEL_MenuHandler callback) {
        auto row = new SongRow();
        if (row && row->init(song, target, callback)) {
            row->autorelease();
            return row;
        }
        delete row;
        return nullptr;
    }

    bool init(SongInfoObject* song, CCObject* target, SEL_MenuHandler callback) {
        if (!CCNode::init()) return false;

        m_songID = song->m_songID;
        this->setContentSize({ ROW_W, ROW_H });
        this->setAnchorPoint({ 0.f, 0.f });
        this->setID("song-row"_spr);

        std::string name   = song->m_songName.empty() ? ("Song " + std::to_string(m_songID)) : song->m_songName;
        std::string artist = song->m_artistName;
        std::string idText = "ID " + std::to_string(m_songID);

        // Title (bigFont), left aligned
        m_title = CCLabelBMFont::create(name.c_str(), "bigFont.fnt");
        m_title->setAnchorPoint({ 0.f, 0.f });
        m_title->setColor(ccc3(255, 255, 255));
        fitLabel(m_title, 300.f, 15.f, 0.22f);
        m_title->setPosition(12.f, 17.f);
        this->addChild(m_title);

        // Duration, right aligned
        auto duration = CCLabelBMFont::create(formatTime(song->m_duration).c_str(), "goldFont.fnt");
        duration->setAnchorPoint({ 0.f, 0.f });
        fitLabel(duration, 60.f, 9.f, 0.3f);
        duration->setPosition(
            ROW_W - 12.f - duration->getContentSize().width * duration->getScale(), 20.f
        );
        this->addChild(duration);

        // Artist + song id
        std::string subText = artist.empty() ? idText : artist + "  ·  " + idText;
        auto sub = CCLabelBMFont::create(subText.c_str(), "goldFont.fnt");
        sub->setAnchorPoint({ 0.f, 0.f });
        sub->setColor(ccc3(170, 180, 195));
        fitLabel(sub, ROW_W - 24.f, 9.f, 0.3f);
        sub->setPosition(12.f, 4.f);
        this->addChild(sub);

        // Whole-row click target (invisible)
        auto menu = CCMenu::create();
        menu->ignoreAnchorPointForPosition(true);
        menu->setContentSize({ ROW_W, ROW_H });
        menu->setPosition(0.f, 0.f);
        menu->setZOrder(10);
        this->addChild(menu);

        auto hitArea = CCSprite::create("GJ_square01.png");
        if (hitArea) {
            auto sz = hitArea->getContentSize();
            if (sz.width > 0.f)  hitArea->setScaleX(ROW_W / sz.width);
            if (sz.height > 0.f) hitArea->setScaleY(ROW_H / sz.height);
            hitArea->setOpacity(0);
        }
        auto item = CCMenuItemSpriteExtra::create(hitArea, target, callback);
        item->setTag(m_songID);
        item->setAnchorPoint({ 0.5f, 0.5f });
        item->ignoreAnchorPointForPosition(false);
        item->setContentSize({ ROW_W, ROW_H });
        menu->addChild(item);
        item->setPosition(ROW_W / 2.f, ROW_H / 2.f);

        m_searchBlob = toLowerCopy(name + " " + artist + " " + idText);
        return true;
    }

    void setSelected(bool selected) {
        if (m_title) {
            m_title->setColor(selected ? ccc3(0, 230, 255) : ccc3(255, 255, 255));
        }
    }
};

// ListView that detaches any row left behind in a recycled cell before
// loading a new one, so our persistent rows are never double-parented.
class MTListView : public ListView {
public:
    static MTListView* create(CCArray* items, float itemHeight, float width, float height) {
        auto ret = new MTListView();
        ret->m_itemSeparation = itemHeight;
        if (ret->init(items, BoomListType::Default, width, height)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    void loadCell(TableViewCell* cell, int index) override {
        if (auto children = cell->getChildren()) {
            std::vector<CCNode*> stale;
            for (unsigned int i = 0; i < children->count(); ++i) {
                auto child = static_cast<CCNode*>(children->objectAtIndex(i));
                if (child->getID() == "song-row"_spr) {
                    stale.push_back(child);
                }
            }
            for (auto child : stale) {
                child->removeFromParent();
            }
        }
        ListView::loadCell(cell, index);
    }
};

class MenuTunePopup : public Popup {
protected:
    TextInput* m_searchInput = nullptr;
    CCLabelBMFont* m_status = nullptr;
    CCLabelBMFont* m_emptyLabel = nullptr;
    ListView* m_list = nullptr;
    CCArray* m_allRows = nullptr;
    CCArray* m_shownRows = nullptr;
    std::string m_filter;

    bool init() {
        if (!Popup::init(POPUP_W, POPUP_H)) return false;

        this->setTitle("MenuTune");

        // Now-playing status line
        m_status = CCLabelBMFont::create("", "goldFont.fnt");
        m_status->setAnchorPoint({ 0.5f, 0.5f });
        m_mainLayer->addChildAtPosition(m_status, Anchor::Top, { 0.f, -44.f });

        // Search box
        m_searchInput = TextInput::create(POPUP_W - 60.f, "Search songs...", "bigFont.fnt");
        m_searchInput->setCommonFilter(CommonFilter::Any);
        m_searchInput->setCallback([this](std::string const& text) {
            m_filter = toLowerCopy(text);
            this->rebuildList();
        });
        m_mainLayer->addChildAtPosition(m_searchInput, Anchor::Top, { 0.f, -74.f });

        // Empty-state label (sits in the middle of the list area)
        m_emptyLabel = CCLabelBMFont::create("No songs found", "goldFont.fnt");
        m_emptyLabel->setAnchorPoint({ 0.5f, 0.5f });
        m_emptyLabel->setColor(ccc3(170, 180, 195));
        m_emptyLabel->setVisible(false);
        m_mainLayer->addChildAtPosition(m_emptyLabel, Anchor::Center, { 0.f, -25.f });

        // Build every row once
        m_allRows = CCArray::create();
        m_allRows->retain();
        this->buildRows();

        // "Default" button - restore the vanilla menu music
        auto defaultSprite = ButtonSprite::create("Default", "goldFont.fnt", "GJ_button_02.png", 0.6f);
        auto defaultButton = CCMenuItemSpriteExtra::create(
            defaultSprite, this, menu_selector(MenuTunePopup::onDefault)
        );
        defaultButton->setAnchorPoint({ 0.5f, 0.5f });
        defaultButton->ignoreAnchorPointForPosition(false);
        m_buttonMenu->addChildAtPosition(defaultButton, Anchor::BottomLeft, { 48.f, 20.f });

        this->rebuildList();
        this->updateNowPlaying();
        return true;
    }

    void buildRows() {
        auto songs = MusicDownloadManager::sharedState()->getDownloadedSongs();
        if (!songs) return;
        for (unsigned int i = 0; i < songs->count(); ++i) {
            auto song = typeinfo_cast<SongInfoObject*>(songs->objectAtIndex(i));
            if (!song) continue;
            if (auto row = SongRow::create(song, this, menu_selector(MenuTunePopup::onSelectSong))) {
                m_allRows->addObject(row);
            }
        }
        log::info("MenuTune: loaded {} saved songs", m_allRows->count());
    }

    void rebuildList() {
        if (m_list) {
            m_list->removeFromParent();
            m_list = nullptr;
        }
        if (m_shownRows) {
            m_shownRows->release();
            m_shownRows = nullptr;
        }

        m_shownRows = CCArray::create();
        m_shownRows->retain();

        for (unsigned int i = 0; i < m_allRows->count(); ++i) {
            auto row = static_cast<SongRow*>(m_allRows->objectAtIndex(i));
            if (m_filter.empty() || row->m_searchBlob.find(m_filter) != std::string::npos) {
                m_shownRows->addObject(row);
            }
        }

        if (m_emptyLabel) {
            m_emptyLabel->setVisible(m_shownRows->count() == 0);
        }

        m_list = MTListView::create(m_shownRows, ROW_H, LIST_W, LIST_H);
        if (m_list) {
            m_list->setPrimaryCellColor(ccc3(20, 28, 44));
            m_list->setSecondaryCellColor(ccc3(27, 37, 57));
            m_list->setCellBorderColor(ccc4(0, 230, 255, 50));
            m_list->ignoreAnchorPointForPosition(true);
            m_list->setPosition(LIST_X, LIST_Y);
            m_list->setZOrder(5);
            m_mainLayer->addChild(m_list);
        }

        this->refreshSelection();
    }

    void refreshSelection() {
        int current = GameManager::sharedState()->m_customMenuSongID;
        for (unsigned int i = 0; i < m_allRows->count(); ++i) {
            auto row = static_cast<SongRow*>(m_allRows->objectAtIndex(i));
            row->setSelected(row->m_songID == current);
        }
    }

    void updateNowPlaying() {
        int id = GameManager::sharedState()->m_customMenuSongID;
        std::string text;
        ccColor3B color;
        if (id == 0) {
            text = "Now playing: Default menu music";
            color = ccc3(190, 195, 205);
        }
        else {
            auto song = MusicDownloadManager::sharedState()->getSongInfoObject(id);
            text = song ? ("Now playing: " + song->m_songName)
                        : ("Now playing: song " + std::to_string(id));
            color = ccc3(0, 230, 255);
        }
        m_status->setString(text.c_str());
        m_status->setColor(color);
        fitLabel(m_status, POPUP_W - 40.f, 11.f, 0.3f);
    }

    void onSelectSong(CCObject* sender) {
        int id = static_cast<CCNode*>(sender)->getTag();
        auto gm = GameManager::sharedState();
        if (gm->m_customMenuSongID != id) {
            gm->m_customMenuSongID = id;
            gm->fadeInMenuMusic();
            log::info("MenuTune: menu song set to {}", id);
        }
        this->updateNowPlaying();
        this->refreshSelection();
    }

    void onDefault(CCObject*) {
        auto gm = GameManager::sharedState();
        if (gm->m_customMenuSongID != 0) {
            gm->m_customMenuSongID = 0;
            gm->fadeInMenuMusic();
            log::info("MenuTune: menu music restored to default");
        }
        this->updateNowPlaying();
        this->refreshSelection();
    }

public:
    static MenuTunePopup* create() {
        auto popup = new MenuTunePopup();
        if (popup->init()) {
            popup->autorelease();
            return popup;
        }
        delete popup;
        return nullptr;
    }

    ~MenuTunePopup() {
        if (m_allRows) m_allRows->release();
        if (m_shownRows) m_shownRows->release();
    }
};

class $modify(MenuTuneMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        if (auto menu = this->getChildByID("bottom-menu")) {
            auto cache = CCSpriteFrameCache::sharedSpriteFrameCache();
            if (!cache->spriteFrameByName("GJ_noteIcon_001.png")) {
                cache->addSpriteFramesWithFile("GJ_GameSheet03.plist");
            }
            // Same construction as Geode's own bottom-menu button: a green
            // circle base sized exactly like the vanilla MenuLayer buttons
            // (MediumAlt = "the bottom buttons in MenuLayer"), with the note
            // icon tinted to the gold used by the sibling icons.
            if (auto spr = CircleButtonSprite::createWithSpriteFrameName(
                "GJ_noteIcon_001.png", 1.f, CircleBaseColor::Green, CircleBaseSize::MediumAlt
            )) {
                if (auto top = typeinfo_cast<cocos2d::CCSprite*>(spr->getTopNode())) {
                    top->setColor(ccc3(255, 240, 0));
                }
                auto button = CCMenuItemSpriteExtra::create(
                    spr, this, menu_selector(MenuTuneMenuLayer::onMenuTune)
                );
                button->setID("note-button"_spr);
                menu->addChild(button);
                menu->updateLayout();
            }
            else {
                log::warn("MenuTune: could not create circle note button");
            }
        }
        else {
            log::warn("MenuTune: bottom-menu not found");
        }
        return true;
    }

    void onMenuTune(CCObject*) {
        if (auto popup = MenuTunePopup::create()) {
            popup->show();
        }
    }
};
