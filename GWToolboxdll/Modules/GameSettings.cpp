#include "stdafx.h"


#include <GWCA/Constants/Skills.h>
#include <GWCA/Utilities/Scanner.h>

#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameContainers/GamePos.h>

#include <GWCA/GameEntities/Friendslist.h>
#include <GWCA/GameEntities/Guild.h>
#include <GWCA/GameEntities/Quest.h>
#include <GWCA/GameEntities/Camera.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/GameEntities/Map.h>
#include <GWCA/GameEntities/Title.h>

#include <GWCA/Context/ItemContext.h>
#include <GWCA/Context/PartyContext.h>
#include <GWCA/Context/GuildContext.h>

#include <GWCA/Constants/AgentIDs.h>

#include <GWCA/Managers/UIMgr.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/GuildMgr.h>
#include <GWCA/Managers/StoCMgr.h>
#include <GWCA/Managers/CtoSMgr.h>

#include <GWCA/Managers/PartyMgr.h>
#include <GWCA/Managers/CameraMgr.h>
#include <GWCA/Managers/MemoryMgr.h>
#include <GWCA/Managers/RenderMgr.h>
#include <GWCA/Managers/FriendListMgr.h>
#include <GWCA/Managers/GameThreadMgr.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/Managers/EffectMgr.h>

#include <GWCA/Utilities/Scanner.h>
#include <GWCA/Utilities/Hooker.h>

#include <Logger.h>
#include <GuiUtils.h>
#include <GWToolbox.h>
#include <Timer.h>
#include <Color.h>

#include <Windows/StringDecoderWindow.h>
#include <Modules/GameSettings.h>
#include <Modules/Resources.h>

#pragma warning(disable : 6011)

namespace {

    void FlashWindow() {
        FLASHWINFO flashInfo = { 0 };
        flashInfo.cbSize = sizeof(FLASHWINFO);
        flashInfo.hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (!flashInfo.hwnd) return;
        flashInfo.dwFlags = FLASHW_TIMER | FLASHW_TRAY | FLASHW_TIMERNOFG;
        flashInfo.uCount = 0;
        flashInfo.dwTimeout = 0;
        FlashWindowEx(&flashInfo);
    }
    void FocusWindow() {
        HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (!hwnd) return;
        SetForegroundWindow(hwnd);
        ShowWindow(hwnd, SW_RESTORE);
    }

    void PrintTime(wchar_t *buffer, size_t n, DWORD time_sec) {
        DWORD secs = time_sec % 60;
        DWORD minutes = (time_sec / 60) % 60;
        DWORD hours = time_sec / 3600;
        DWORD time = 0;
        const wchar_t *time_unit = L"";
        if (hours != 0) {
            time_unit = L"hour";
            time = hours;
        } else if (minutes != 0) {
            time_unit = L"minute";
            time = minutes;
        } else {
            time_unit = L"second";
            time = secs;
        }
        if (time > 1) {
            swprintf(buffer, n, L"%lu %ss", time, time_unit);
        } else {
            swprintf(buffer, n, L"%lu %s", time, time_unit);
        }
    }
    
    struct PartyInfo : GW::PartyInfo {
        size_t GetPartySize() {
            return players.size() + henchmen.size() + heroes.size();
        }
    };
    static wchar_t* GetMessageCore() {
        GW::Array<wchar_t>* buff = &GW::GameContext::instance()->world->message_buff;
        return buff ? buff->begin() : nullptr;
    }
    static void ClearMessageCore() {
        GW::Array<wchar_t>* buff = &GW::GameContext::instance()->world->message_buff;
        if (buff && buff->valid())
            buff->clear();
    }

    PartyInfo* GetPartyInfo(uint32_t party_id = 0) {
        if (!party_id)
            return (PartyInfo*)GW::PartyMgr::GetPartyInfo();
        GW::PartyContext* p = GW::GameContext::instance()->party;
        if (!p || !p->parties.valid() || party_id >= p->parties.size())
            return nullptr;
        return (PartyInfo*)p->parties[party_id];
    }

    const std::wstring GetPlayerName(uint32_t player_number = 0) {
        GW::Player* player = nullptr;
        if (!player_number) {
            player = GW::PlayerMgr::GetPlayerByID(GW::PlayerMgr::GetPlayerNumber());
            if (!player || !player->name) {
                // Map not loaded; try to get from character context
                auto* g = GW::GameContext::instance();
                if (!g || !g->character || !g->character->player_name)
                    return L"";
                return g->character->player_name;
            }
        }
        else {
            player = GW::PlayerMgr::GetPlayerByID(player_number);
        }
        return player && player->name ? GuiUtils::SanitizePlayerName(player->name) : L"";
    }

    void SetWindowTitle(bool enabled) {
        if (!enabled)
            return;
        HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
        if (!hwnd) return;
        std::wstring title = GetPlayerName();
        if (!title.empty())
            SetWindowTextW(hwnd, title.c_str());
    }

    GW::Player* GetPlayerByName(const wchar_t* _name) {
        if (!_name) return NULL;
        std::wstring name = GuiUtils::SanitizePlayerName(_name);
        GW::PlayerArray& players = GW::PlayerMgr::GetPlayerArray();
        for (GW::Player& player : players) {
            if (!player.name) continue;
            if (name == GuiUtils::SanitizePlayerName(player.name))
                return &player;
        }
        return NULL;
    }

    void WhisperCallback(GW::HookStatus *, const wchar_t *from, const wchar_t *msg) {
        UNREFERENCED_PARAMETER(msg);
        GameSettings&  game_setting = GameSettings::Instance();
        if (game_setting.flash_window_on_pm) FlashWindow();
        auto const status = static_cast<GW::FriendStatus>(GW::FriendListMgr::GetMyStatus());
        if (status == GW::FriendStatus::FriendStatus_Away && !game_setting.afk_message.empty()) {
            wchar_t buffer[120];
            // @Cleanup: Do without this cast
            DWORD diff_time = static_cast<DWORD>((clock() - game_setting.afk_message_time) / CLOCKS_PER_SEC);
            wchar_t time_buffer[128];
            PrintTime(time_buffer, 128, diff_time);
            swprintf(buffer, 120, L"Automatic message: \"%s\" (%s ago)", game_setting.afk_message.c_str(), time_buffer);
            // Avoid infinite recursion
            if (::GetPlayerName() != from)
                GW::Chat::SendChat(from, buffer);
        }
    }

    // used by chat colors grid
    float chat_colors_grid_x[] = { 0, 100, 160, 240 };

    void SaveChannelColor(CSimpleIni *ini, const char *section, const char *chanstr, GW::Chat::Channel chan) {
        char key[128];
        GW::Chat::Color sender, message;
        GW::Chat::GetChannelColors(chan, &sender, &message);
        // @Cleanup: We relie on the fact the Color and GW::Chat::Color are the same format
        snprintf(key, 128, "%s_color_sender", chanstr);
        Colors::Save(ini, section, key, (Color)sender);
        snprintf(key, 128, "%s_color_message", chanstr);
        Colors::Save(ini, section, key, (Color)message);
    }

    void LoadChannelColor(CSimpleIni *ini, const char *section, const char *chanstr, GW::Chat::Channel chan) {
        char key[128];
        GW::Chat::Color sender, message;
        GW::Chat::GetDefaultColors(chan, &sender, &message);
        snprintf(key, 128, "%s_color_sender", chanstr);
        sender = (GW::Chat::Color)Colors::Load(ini, section, key, (Color)sender);
        GW::Chat::SetSenderColor(chan, sender);
        snprintf(key, 128, "%s_color_message", chanstr);
        message = (GW::Chat::Color)Colors::Load(ini, section, key, (Color)message);
        GW::Chat::SetMessageColor(chan, message);
    }

    struct PendingSendChatMessage {};

    static clock_t last_send = 0;
    static uint32_t last_dialog_npc_id = 0;

    static clock_t instance_entered_at = 0;

    static bool ctrl_enter_whisper = false;

    static bool IsInfused(GW::Item* item) {
        return item && item->info_string && wcschr(item->info_string, 0xAC9);
    }

    const enum PING_PARTS {
        NAME=1,
        DESC=2
    };

    struct PendingCast {
        uint32_t slot = 0;
        uint32_t target_id = 0;
        uint32_t call_target = 0;
        bool cast_next_frame = false;
        void reset(uint32_t _slot = 0 , uint32_t _target_id = 0, uint32_t _call_target = 0) {
            slot = _slot;
            target_id = _target_id;
            call_target = _call_target;
            cast_next_frame = false;
        }
        const GW::AgentLiving* GetTarget() {
            const GW::AgentLiving* target = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(target_id));
            return target && target->GetIsLivingType() ? target : nullptr;
        }
        const uint32_t GetSkill() {
            const GW::Skillbar* skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
            return skillbar && skillbar->IsValid() ? skillbar->skills[slot].skill_id : 0;
        }
    } pending_cast;

    struct PlayerChatMessage {
        uint32_t channel;
        wchar_t* message;
        uint32_t player_number;
    };

    struct PartyTargetInfo {
        uint32_t target_type = 0;
        uint32_t is_party_member; // 1 or 0
        uint32_t target_identifier;
    } party_target_info;

    struct SkillData {
        GW::Constants::Profession primary;
        GW::Constants::Profession secondary;
        uint32_t attribute_count;
        GW::Constants::Attribute attribute_keys[12];
        uint32_t attribute_values[12];
        GW::Constants::SkillID skill_ids[8];
    };

    // prophecies = factions
    std::map<GW::Constants::SkillID, GW::Constants::SkillID> duplicate_skills = {
        {GW::Constants::SkillID::Desperation_Blow, GW::Constants::SkillID::Drunken_Blow },
        {GW::Constants::SkillID::Galrath_Slash, GW::Constants::SkillID::Silverwing_Slash },
        {GW::Constants::SkillID::Griffons_Sweep, GW::Constants::SkillID::Leviathans_Sweep },
        {GW::Constants::SkillID::Penetrating_Blow, GW::Constants::SkillID::Penetrating_Chop },
        {GW::Constants::SkillID::Pure_Strike, GW::Constants::SkillID::Jaizhenju_Strike },

        {GW::Constants::SkillID::Bestial_Pounce, GW::Constants::SkillID::Savage_Pounce },
        {GW::Constants::SkillID::Dodge, GW::Constants::SkillID::Zojuns_Haste },
        {GW::Constants::SkillID::Penetrating_Attack, GW::Constants::SkillID::Sundering_Attack },
        {GW::Constants::SkillID::Point_Blank_Shot, GW::Constants::SkillID::Zojuns_Shot },
        {GW::Constants::SkillID::Tigers_Fury, GW::Constants::SkillID::Bestial_Fury },

        {GW::Constants::SkillID::Divine_Healing, GW::Constants::SkillID::Heavens_Delight },
        {GW::Constants::SkillID::Heal_Area, GW::Constants::SkillID::Kareis_Healing_Circle },
        {GW::Constants::SkillID::Heal_Other, GW::Constants::SkillID::Jameis_Gaze },
        {GW::Constants::SkillID::Holy_Strike, GW::Constants::SkillID::Stonesoul_Strike },
        {GW::Constants::SkillID::Symbol_of_Wrath, GW::Constants::SkillID::Kirins_Wrath },

        {GW::Constants::SkillID::Desecrate_Enchantments, GW::Constants::SkillID::Defile_Enchantments },
        {GW::Constants::SkillID::Shadow_Strike, GW::Constants::SkillID::Lifebane_Strike },
        {GW::Constants::SkillID::Spinal_Shivers, GW::Constants::SkillID::Shivers_of_Dread },
        {GW::Constants::SkillID::Touch_of_Agony, GW::Constants::SkillID::Wallows_Bite },
        {GW::Constants::SkillID::Vampiric_Touch, GW::Constants::SkillID::Vampiric_Bite },

        {GW::Constants::SkillID::Arcane_Thievery, GW::Constants::SkillID::Arcane_Larceny },
        {GW::Constants::SkillID::Ethereal_Burden, GW::Constants::SkillID::Kitahs_Burden },
        {GW::Constants::SkillID::Inspired_Enchantment, GW::Constants::SkillID::Revealed_Enchantment },
        {GW::Constants::SkillID::Inspired_Hex, GW::Constants::SkillID::Revealed_Hex },
        {GW::Constants::SkillID::Sympathetic_Visage, GW::Constants::SkillID::Ancestors_Visage },

        {GW::Constants::SkillID::Earthquake, GW::Constants::SkillID::Dragons_Stomp }
    };
    // luxon = kurzick
    std::map<GW::Constants::SkillID, GW::Constants::SkillID> factions_skills = {
        {GW::Constants::SkillID::Save_Yourselves_luxon, GW::Constants::SkillID::Save_Yourselves_kurzick },
        {GW::Constants::SkillID::Aura_of_Holy_Might_luxon, GW::Constants::SkillID::Aura_of_Holy_Might_kurzick },
        {GW::Constants::SkillID::Elemental_Lord_luxon, GW::Constants::SkillID::Elemental_Lord_kurzick },
        {GW::Constants::SkillID::Ether_Nightmare_luxon, GW::Constants::SkillID::Ether_Nightmare_kurzick },
        {GW::Constants::SkillID::Selfless_Spirit_luxon, GW::Constants::SkillID::Selfless_Spirit_kurzick },
        {GW::Constants::SkillID::Shadow_Sanctuary_luxon, GW::Constants::SkillID::Shadow_Sanctuary_kurzick },
        {GW::Constants::SkillID::Signet_of_Corruption_luxon, GW::Constants::SkillID::Signet_of_Corruption_kurzick },
        {GW::Constants::SkillID::Spear_of_Fury_luxon, GW::Constants::SkillID::Spear_of_Fury_kurzick },
        {GW::Constants::SkillID::Summon_Spirits_luxon, GW::Constants::SkillID::Summon_Spirits_kurzick },
        {GW::Constants::SkillID::Triple_Shot_luxon, GW::Constants::SkillID::Triple_Shot_kurzick }
    };

    bool IsSkillUnlocked(GW::Constants::SkillID skill_id) {
        GW::GameContext* g = GW::GameContext::instance();
        GW::WorldContext* w = g->world;

        auto& array = w->unlocked_character_skills;

        uint32_t index = static_cast<uint32_t>(skill_id);
        uint32_t real_index = index / 32;
        if (real_index >= array.size())
            return false;
        uint32_t shift = index % 32;
        uint32_t flag = 1 << shift;
        return (array[real_index] & flag) != 0;
    }

    struct LoadSkillBarPacket {
        uint32_t header;
        uint32_t agent_id;
        uint32_t skill_ids_size = 8;
        GW::Constants::SkillID skill_ids[8];
    } skillbar_packet;

    // Before the game loads the skill bar you want, copy the data over for checking once the bar is loaded.
    void OnPreLoadSkillBar(GW::HookStatus*, void* packet) {
        skillbar_packet = *(LoadSkillBarPacket*)packet;
    }

    // Takes SkillData* ptr, rectifies any missing dupe skills. True if bar has been tweaked.
    bool FixLoadSkillData(GW::Constants::SkillID* skill_ids) {
        auto find_skill = [](GW::Constants::SkillID* skill_ids, GW::Constants::SkillID skill_id) {
            for (int i = 0; i < 8; i++) {
                if (skill_ids[i] == skill_id)
                    return i;
            }
            return -1;
        };
        int found_first;
        int found_second;
        bool unlocked_first;
        bool unlocked_second;
        bool tweaked = false;
        GW::TitleArray titles;
        for (auto& skill : duplicate_skills) {
            found_first = find_skill(skill_ids, skill.first);
            found_second = find_skill(skill_ids, skill.second);
            if (found_first == -1 && found_second == -1)
                continue;
            unlocked_first = IsSkillUnlocked(skill.first);
            unlocked_second = IsSkillUnlocked(skill.second);

            if (found_first != -1 && found_second == -1
                && !unlocked_first && unlocked_second) {
                // First skill found in build template, second skill not already in template, user only has second skill
                skill_ids[found_first] = skill.second;
                tweaked = true;
            }
            else if (found_second != -1 && found_first == -1
                && !unlocked_second && unlocked_first) {
                // Second skill found in build template, first skill not already in template, user only has first skill
                skill_ids[found_second] = skill.first;
                tweaked = true;
            }
        }
        for (auto& skill : factions_skills) {
            found_first = find_skill(skill_ids, skill.first);
            found_second = find_skill(skill_ids, skill.second);
            if (found_first == -1 && found_second == -1)
                continue;
            unlocked_first = IsSkillUnlocked(skill.first);
            unlocked_second = IsSkillUnlocked(skill.second);

            if (found_first != -1 && found_second == -1
                && !unlocked_first && unlocked_second) {
                // First skill found in build template, second skill not already in template, user only has second skill
                skill_ids[found_first] = skill.second;
                tweaked = true;
            }
            else if (found_second != -1 && found_first == -1
                && !unlocked_second && unlocked_first) {
                // Second skill found in build template, first skill not already in template, user only has first skill
                skill_ids[found_second] = skill.first;
                tweaked = true;
            }
            else if (unlocked_first && unlocked_second) {
                // Find skill with higher title track
                titles = GW::GameContext::instance()->world->titles;
                uint32_t kurzick_rank = titles.size() <= GW::Constants::TitleID::Kurzick ? 0 : titles[GW::Constants::TitleID::Kurzick].points_needed_current_rank;
                uint32_t luxon_rank = titles.size() <= GW::Constants::TitleID::Luxon ? 0 : titles[GW::Constants::TitleID::Luxon].points_needed_current_rank;
                if (kurzick_rank > luxon_rank) {
                    skill_ids[std::max(found_first, found_second)] = skill.second;
                    tweaked = true;
                }
                else if (kurzick_rank < luxon_rank) {
                    skill_ids[std::max(found_first, found_second)] = skill.first;
                    tweaked = true;
                }
            }
        }
        return tweaked;
    }
    // Checks loaded skillbar for any missing skills once the game has sent the packet
    void OnPostLoadSkillBar(GW::HookStatus*, void* packet) {
        LoadSkillBarPacket* post_pack = (LoadSkillBarPacket*)packet;
        if (post_pack->agent_id != skillbar_packet.agent_id) {
            skillbar_packet.agent_id = 0;
            return;
        }
        if (FixLoadSkillData(skillbar_packet.skill_ids)) {
            GW::CtoS::SendPacket(sizeof(skillbar_packet), &skillbar_packet);
        }
        skillbar_packet.agent_id = 0;
    }

}

static std::wstring ShorthandItemDescription(GW::Item* item) {
    std::wstring original(item->info_string);
    std::wsmatch m;

    // For armor items, include full item name and a few description bits.
    switch ((GW::Constants::ItemType)item->type) {
    case GW::Constants::ItemType::Headpiece:
    case GW::Constants::ItemType::Boots:
    case GW::Constants::ItemType::Chestpiece:
    case GW::Constants::ItemType::Gloves:
    case GW::Constants::ItemType::Leggings: {
        original = item->complete_name_enc;
        std::wstring item_str(item->info_string);
        std::wregex stacking_att(L"\x2.\x10A\xA84\x10A(.{1,2})\x1\x101\x101\x1\x2\xA3E\x10A\xAA8\x10A\xAB1\x1\x1");
        if (std::regex_search(item_str, m, stacking_att)) {
            wchar_t buffer[64];
            wsprintfW(buffer, L"\x2\xAA8\x10A\xA84\x10A%s\x1\x101\x101\x1", m[1].str().c_str());
            original += buffer;
        }
        std::wregex armor_rating(L"\xA3B\x10A\xA86\x10A\xA44\x1\x101(.)\x1\x2");
        if (std::regex_search(item_str, m, armor_rating)) {
            wchar_t buffer[64];
            wsprintfW(buffer, L"\x2\x102\x2\xA86\x10A\xA44\x1\x101%s", m[1].str().c_str());
            original += buffer;
        }
        if (IsInfused(item))
            original += L"\x2\x102\x2\xAC9";
        return original;
    }
    default:
        break;
    }

    // Replace "Requires 9 Divine Favor" > "q9 Divine Favor"
    std::wregex regexp_req(L".\x10A\x0AA8\x10A\xAA9\x10A.\x1\x101.\x1\x1");
    while (std::regex_search(original, m, regexp_req)) {
        for (auto match : m) {
            std::wstring found = match.str();
            wchar_t buffer[128];
            wsprintfW(buffer, L"\x108\x107, q%d \x1\x2%c", found.at(9) - 0x100, found.at(6));
            original = std::regex_replace(original, std::wregex(found), buffer);
        }
    }
    // Replace "Requires 9 Scythe Mastery" > "q9 Scythe Mastery"
    std::wregex regexp_req2(L".\x10A\xAA8\x10A\xAA9\x10A\x8101.\x1\x101.\x1\x1");
    while (std::regex_search(original, m, regexp_req2)) {
        for (auto match : m) {
            std::wstring found = match.str();
            wchar_t buffer[128];
            wsprintfW(buffer, L"\x108\x107, q%d \x1\x2\x8101%c", found.at(10) - 0x100, found.at(7));
            original = std::regex_replace(original, std::wregex(found), buffer);
        }
    }

    // "vs. Earth damage" > "Earth"
    // "vs. Demons" > "Demons"
    std::wregex vs_damage(L"[\xAAC\xAAF]\x10A.\x1");
    while (std::regex_search(original, m, vs_damage)) {
        for (auto match : m) {
            std::wstring found = match.str();
            wchar_t buffer[4];
            wsprintfW(buffer, L"%c", found.at(2));
            original = std::regex_replace(original, std::wregex(found), buffer);
        }
    }

    // Replace "Lengthens ??? duration on foes by 33%" > "??? duration +33%"
    std::wregex regexp_lengthens_duration(L"\xAA4\x10A.\x1");
    while (std::regex_search(original, m, regexp_lengthens_duration)) {
        for (auto match : m) {
            std::wstring found = match.str();
            wchar_t buffer[64];
            wsprintfW(buffer, L"%c\x2\x108\x107 +33%%\x1", found.at(2));
            original = std::regex_replace(original, std::wregex(found), buffer);
        }
    }

    // Replace "Reduces ??? duration on you by 20%" > "??? duration -20%"
    std::wregex regexp_reduces_duration(L"\xAA7\x10A.\x1");
    while (std::regex_search(original, m, regexp_reduces_duration)) {
        for (auto match : m) {
            std::wstring found = match.str();
            wchar_t buffer[64];
            wsprintfW(buffer, L"%c\x2\x108\x107 -20%%\x1", found.at(2));
            original = std::regex_replace(original, std::wregex(found), buffer);
        }
    }

    // Change "Damage 15% (while Health is above 50%)" to "Damage +15^50"
    //std::wregex damage_15_over_50(L".\x010A\xA85\x010A\xA4C\x1\x101.\x1\x2" L".\x010A\xAA8\x010A\xABC\x10A\xA52\x1\x101.\x1\x1");
    // Change " (while Health is above n)" to "^n";
    std::wregex n_over_n(L"\xAA8\x10A\xABC\x10A\xA52\x1\x101.\x1");
    while (std::regex_search(original, m, n_over_n)) {
        for (auto match : m) {
            std::wstring found = match.str();
            wchar_t buffer[64];
            wsprintfW(buffer, L"\x108\x107^%d\x1", found.at(7) - 0x100);
            original = std::regex_replace(original, std::wregex(found), buffer);
        }
    }

    // Change "Enchantments last 20% longer" to "Ench +20%"
    std::wregex enchantments(L"\xAA2\x101.");
    while (std::regex_search(original, m, enchantments)) {
        for (auto match : m) {
            std::wstring found = match.str();
            wchar_t buffer[64];
            wsprintfW(buffer, L"\x108\x107" L"Enchantments +%d%%\x1", found.at(2) - 0x100);
            original = std::regex_replace(original, std::wregex(found), buffer);
        }
    }

    // "(Chance: 18%)" > "(18%)"
    std::wregex chance_regex(L"\xA87\x10A\xA48\x1\x101.");
    while (std::regex_search(original, m, chance_regex)) {
        for (auto match : m) {
            std::wstring found = match.str();
            wchar_t buffer[64];
            wsprintfW(buffer, L"\x108\x107%d%%\x1", found.at(5) - 0x100);
            original = std::regex_replace(original, std::wregex(found), buffer);
        }
    }
    // Change "Halves skill recharge of <attribute> spells" > "HSR <attribute>"
    std::wregex hsr_attribute(L"\xA81\x10A\xA58\x1\x10B.\x1");
    while (std::regex_search(original, m, hsr_attribute)) {
        for (auto match : m) {
            std::wstring found = match.str();
            wchar_t buffer[64];
            wsprintfW(buffer, L"\x108\x107" L"HSR \x1\x2%c", found.at(5));
            original = std::regex_replace(original, std::wregex(found), buffer);
        }
    }
    // Change "Inscription: "Blah Blah"" to just "Blah Blah"
    std::wregex inscription(L"\x8101\x5DC5\x10A..\x1");
    while (std::regex_search(original, m, inscription)) {
        for (auto match : m) {
            std::wstring found = match.str();
            wchar_t buffer[64];
            wsprintfW(buffer, L"%c%c", found.at(3), found.at(4));
            original = std::regex_replace(original, std::wregex(found), buffer);
        }
    }

    // Change "Halves casting time of <attribute> spells" > "HCT <attribute>"
    std::wregex hct_attribute(L"\xA81\x10A\xA47\x1\x10B.\x1");
    while (std::regex_search(original, m, hct_attribute)) {
        for (auto match : m) {
            std::wstring found = match.str();
            wchar_t buffer[64];
            wsprintfW(buffer, L"\x108\x107" L"HCT \x1\x2%c", found.at(5));
            original = std::regex_replace(original, std::wregex(found), buffer);
        }
    }

    // Change "Piercing Dmg: 11-22" > "Piercing: 11-22"
    std::wregex weapon_dmg(L"\xA89\x10A\xA4E\x1\x10B.\x1\x101.\x102.");
    while (std::regex_search(original, m, weapon_dmg)) {
        for (auto match : m) {
            std::wstring found = match.str();
            wchar_t buffer[64];
            wsprintfW(buffer, L"%c\x2\x108\x107: %d-%d\x1", found.at(5),found.at(8) - 0x100, found.at(10) - 0x100);
            original = std::regex_replace(original, std::wregex(found), buffer);
        }
    }

    // Change "Life draining -3, Health regeneration -1" > "Vampiric" (add at end of description)
    std::wregex vampiric(L"\x2\x102\x2.\x10A\xA86\x10A\xA54\x1\x101.\x1" L"\x2\x102\x2.\x10A\xA7E\x10A\xA53\x1\x101.\x1");
    if (std::regex_search(original, vampiric)) {
        original = std::regex_replace(original, vampiric, L"");
        original += L"\x2\x102\x2\x108\x107" L"Vampiric\x1";
    }
    // Change "Energy gain on hit 1, Energy regeneration -1" > "Zealous" (add at end of description)
    std::wregex zealous(L"\x2\x102\x2.\x10A\xA86\x10A\xA50\x1\x101.\x1" L"\x2\x102\x2.\x10A\xA7E\x10A\xA51\x1\x101.\x1");
    if (std::regex_search(original, zealous)) {
        original = std::regex_replace(original, zealous, L"");
        original += L"\x2\x102\x2\x108\x107" L"Zealous\x1";
    }

    // Change "Damage" > "Dmg"
    original = std::regex_replace(original, std::wregex(L"\xA4C"), L"\xA4E");

    // Change Bow "Two-Handed" > ""
    original = std::regex_replace(original, std::wregex(L"\x8102\x1227"), L"\xA3E");

    // Change "Halves casting time of spells" > "HCT"
    original = std::regex_replace(original, std::wregex(L"\xA80\x10A\xA47\x1"), L"\x108\x107" L"HCT\x1");

    // Change "Halves skill recharge of spells" > "HSR"
    std::wregex half_skill_recharge(L"\xA80\x10A\xA58\x1");
    original = std::regex_replace(original, half_skill_recharge, L"\x108\x107" L"HSR\x1");

    // Remove (Stacking) and (Non-stacking) rubbish
    std::wregex stacking_non_stacking(L"\x2.\x10A\xAA8\x10A[\xAB1\xAB2]\x1\x1");
    original = std::regex_replace(original, stacking_non_stacking, L"");

    // Replace (while affected by a(n) to just (n)
    std::wregex while_affected_by(L"\x8101\x4D9C\x10A.\x1");
    while (std::regex_search(original, m, while_affected_by)) {
        for (auto match : m) {
            std::wstring found = match.str();
            wchar_t buffer[2] = { 0 };
            buffer[0] = found.at(3);
            original = std::regex_replace(original, std::wregex(found), buffer);
        }
    }
    // Replace (while xxx) to just (xxx)
    original = std::regex_replace(original, std::wregex(L"\xAB4"), L"\x108\x107" L"Attacking\x1");
    original = std::regex_replace(original, std::wregex(L"\xAB5"), L"\x108\x107" L"Casting\x1");
    original = std::regex_replace(original, std::wregex(L"\xAB6"), L"\x108\x107" L"Condition\x1");
    original = std::regex_replace(original, std::wregex(L"[\xAB7\x4B6]"), L"\x108\x107" L"Enchanted\x1");
    original = std::regex_replace(original, std::wregex(L"[\xAB8\x4B4]"), L"\x108\x107" L"Hexed\x1");
    original = std::regex_replace(original, std::wregex(L"[\xAB9\xABA]"), L"\x108\x107" L"Stance\x1");

    // Combine Attribute + 3, Attribute + 1 to Attribute +3 +1 (e.g. headpiece)
    std::wregex attribute_stacks(L".\x10A\xA84\x10A.\x1\x101.\x1\x2\x102\x2.\x10A\xA84\x10A.\x1\x101.\x1");
    if (std::regex_search(original, m, attribute_stacks)) {
        for (auto match : m) {
            std::wstring found = match.str();
            if (found[4] != found[16]) continue; // Different attributes.
            wchar_t buffer[64];
            wsprintfW(buffer, L"%c\x10A\xA84\x10A%c\x1\x101%c\x2\xA84\x101%c\x1", 
                found[0], found[4],found[7], found[19]);
            original = std::regex_replace(original, std::wregex(found), buffer);
        }
    }
    return original;
}
static std::wstring ParseItemDescription(GW::Item* item) {
    std::wstring original = ShorthandItemDescription(item);
    
    // Remove "Value: 122 gold"
    original = std::regex_replace(original, std::wregex(L"\x2\x102\x2\xA3E\x10A\xA8A\x10A\xA59\x1\x10B.\x101.(\x102.)?\x1\x1"), L"");

    // Remove other "greyed" generic terms e.g. "Two-Handed", "Unidentified"    
    original = std::regex_replace(original, std::wregex(L"\x2\x102\x2\xA3E\x10A.\x1"), L"");

    // Remove "Necromancer Munne sometimes gives these to me in trade" etc
    original = std::regex_replace(original, std::wregex(L"\x2\x102\x2.\x10A\x8102.\x1"), L"");

    // Remove "Inscription: None"
    original = std::regex_replace(original, std::wregex(L"\x2\x102\x2.\x10A\x8101\x5A1F\x1"), L"");

    // Remove "Crafted in tribute to an enduring legend." etc
    original = std::regex_replace(original, std::wregex(L"\x2\x102\x2.\x10A\x8103.\x1"), L"");

    // Remove "20% Additional damage during festival events" > "Dmg +20% (Festival)"
    original = std::regex_replace(original, std::wregex(L".\x10A\x108\x10A\x8103\xB71\x101\x100\x1\x1"), L"\xA85\x10A\xA4E\x1\x101\x114\x2\xAA8\x10A\x108\x107" L"Festival\x1\x1");

    std::wregex dmg_plus_20(L"\x2\x102\x2.\x10A\xA85\x10A[\xA4C\xA4E]\x1\x101\x114\x1");
    if (item->customized && std::regex_search(original, dmg_plus_20)) {
        // Remove "\nDamage +20%" > "\n"
        original = std::regex_replace(original, dmg_plus_20, L"");
        // Append "Customized"
        original += L"\x2\x102\x2\x108\x107" L"Customized\x1";
    }

    return original;
}

void GameSettings::PingItem(GW::Item* item, uint32_t parts) {
    if (!item) return;
    GW::Player* p = GW::PlayerMgr::GetPlayerByID(GW::PlayerMgr::GetPlayerNumber());
    if (!p) return;
    std::wstring out;
    if ((parts & PING_PARTS::NAME) && item->complete_name_enc) {
        if (out.length())
            out += L"\x2\x102\x2";
        out += item->complete_name_enc;
    }
    else if ((parts & PING_PARTS::NAME) && item->name_enc) {
        if (out.length())
            out += L"\x2\x102\x2";
        out += item->name_enc;
    }
    if ((parts & PING_PARTS::DESC) && item->info_string) {
        if (out.length())
            out += L"\x2\x102\x2";
        out += ParseItemDescription(item);
    }
    #ifdef _DEBUG
        printf("Pinged item:\n");
        StringDecoderWindow::PrintEncStr(out.c_str());
    #endif

    PendingChatMessage* m = PendingChatMessage::queueSend(GW::Chat::Channel::CHANNEL_GROUP, out.c_str(), p->name_enc);
    if (m) GameSettings::Instance().pending_messages.push_back(m);
}
void GameSettings::PingItem(uint32_t item_id, uint32_t parts) {
    return PingItem(GW::Items::GetItemById(item_id), parts);
}

bool PendingChatMessage::Cooldown() {
    return last_send && clock() < last_send + (clock_t)(CLOCKS_PER_SEC / 2);
}
const bool PendingChatMessage::Send() {
    if (!IsDecoded() || this->invalid) return false; // Not ready or invalid.
    std::vector<std::wstring> sanitised_lines = SanitiseForSend();
    wchar_t buf[120];
    size_t len = 0;
    for (size_t i = 0; i < sanitised_lines.size(); i++) {
        size_t san_len = sanitised_lines[i].length();
        const wchar_t* str = sanitised_lines[i].c_str();
        if (len + san_len + 3 > 120) {
            GW::Chat::SendChat('#', buf);
            buf[0] = '\0';
            len = 0;
        }
        if (len) {
            buf[len] = ',';
            buf[len + 1] = ' ';
            len += 2;
        }
        for (size_t j = 0; j < san_len; j++) {
            buf[len] = str[j];
            len++;
        }
        buf[len] = '\0';
    }
    if (len) {
        GW::Chat::SendChat('#', buf);
        last_send = clock();
    }
    printed = true;
    return printed;
}
const bool PendingChatMessage::PrintMessage() {
    if (!IsDecoded() || this->invalid) return false; // Not ready or invalid.
    if (this->printed) return true; // Already printed.
    wchar_t buffer[512];
    switch (channel) {
    case GW::Chat::Channel::CHANNEL_EMOTE:
        GW::Chat::Color dummy, messageCol; // Needed for GW::Chat::GetChannelColors
        GW::Chat::GetChannelColors(GW::Chat::Channel::CHANNEL_ALLIES, &dummy, &messageCol); // ...but set the message to be same color as ally chat
        swprintf(buffer, 512, L"<a=2>%ls</a>: <c=#%06X>%ls</c>", output_sender.c_str(), messageCol & 0x00FFFFFF, output_message.c_str());
        GW::Chat::WriteChat(channel, buffer);
        break;
    default:
        swprintf(buffer, 512, L"<a=2>%ls</a>: %ls", output_sender.c_str(), output_message.c_str());
        GW::Chat::WriteChat(channel, buffer);
        break;
    }
    output_message.clear();
    output_sender.clear();
    printed = true;
    return printed;
};

/*
*   GWToolbox chat log end
*/

void GameSettings::Initialize() {
    ToolboxModule::Initialize();
    
    {
        // Patch that allow storage page (and Anniversary page) to work.
        uintptr_t found = GW::Scanner::Find("\xEB\x17\x33\xD2\x8D\x4A\x06\xEB", "xxxxxxxx", -4);
        printf("[SCAN] StoragePatch = %p\n", (void *)found);

        // Xunlai Chest has a behavior where if you
        // 1. Open chest on page 1 to 14
        // 2. Close chest & open it again
        // -> You should still be on the same page
        // But, if you try with the material page (or anniversary page in the case when you bought all other storage page)
        // you will get back the the page 1. I think it was a intended use for material page & forgot to fix it
        // when they added anniversary page so we do it ourself.
        DWORD page_max = 14;
        ctrl_click_patch.SetPatch(found, &page_max, 1);
        ctrl_click_patch.TogglePatch(true);
    }

    {
        uintptr_t found = GW::Scanner::Find("\x5F\x6A\x00\xFF\x75\xE4\x6A\x4C\xFF\x75\xF8", "xxxxxxxxxxx", -0x44);
        printf("[SCAN] TomePatch = %p\n", (void *)found);
        if (found) {
            tome_patch.SetPatch(found, "\x75\x1E\x90\x90\x90\x90\x90", 7);
        }
    }

    {
        uintptr_t found = GW::Scanner::Find("\xF7\x40\x0C\x10\x00\x02\x00\x75", "xxxxxx??", +7);
        printf("[SCAN] GoldConfirmationPatch = %p\n", (void *)found);
        if (found) {
            gold_confirm_patch.SetPatch(found, "\x90\x90", 2);
        }
    }



    // Save last dialog sender, used for faction donate
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DialogSender>(
        &OnDialog_Entry,
        [this](GW::HookStatus* status, GW::Packet::StoC::DialogSender* pak) {
            UNREFERENCED_PARAMETER(status);
            auto *agent = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(pak->agent_id));
            if (!agent) return;
            last_dialog_npc_id = agent->player_number;
        });
    GW::Agents::RegisterDialogCallback(&OnDialog_Entry, &OnFactionDonate);

    GW::CtoS::RegisterPacketCallback(&OnDialog_Entry, GAME_CMSG_SKILLBAR_LOAD, OnPreLoadSkillBar);
    GW::StoC::RegisterPacketCallback(&OnDialog_Entry, GAME_SMSG_SKILLBAR_UPDATE, OnPostLoadSkillBar, 0x8000);
    GW::StoC::RegisterPacketCallback(&OnDialog_Entry, GAME_SMSG_SKILL_UPDATE_SKILL_COUNT_1, OnUpdateSkillCount, -0x3000);
    GW::StoC::RegisterPacketCallback(&OnDialog_Entry, GAME_SMSG_SKILL_UPDATE_SKILL_COUNT_2, OnUpdateSkillCount, -0x3000);

    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyDefeated>(&PartyDefeated_Entry, &OnPartyDefeated);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GenericValue>(&PartyDefeated_Entry, [](GW::HookStatus* status, GW::Packet::StoC::GenericValue* packet) {
        switch (packet->Value_id) {
        case 11:
            OnAgentMarker(status, packet);
        case 21:
            OnAgentEffect(status, packet);
            break;
        case 22:
            //OnAgentAnimation(status, packet);
            break;
        case 28:
            OnAgentLoopingAnimation(status, packet);
            break;
        }
    });

    // Sanity check to prevent GW crash trying to despawn an agent that we may have already despawned.
    /*GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentRemove>(&PartyDefeated_Entry, [](GW::HookStatus* status, GW::Packet::StoC::AgentRemove* packet) {
        if (false && !GW::Agents::GetAgentByID(packet->agent_id))
            status->blocked = true;
        });*/
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::TradeStart>(&TradeStart_Entry, &OnTradeStarted);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyInviteReceived_Create>(&PartyPlayerAdd_Entry, &OnPartyInviteReceived);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyPlayerAdd>(&PartyPlayerAdd_Entry, &OnPartyPlayerJoined);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::GameSrvTransfer>(&GameSrvTransfer_Entry, &OnMapTravel);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::CinematicPlay>(&CinematicPlay_Entry, &OnCinematic);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::SpeechBubble>(&SpeechBubble_Entry, &OnSpeechBubble);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DisplayDialogue>(&DisplayDialogue_Entry, &OnSpeechDialogue);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::VanquishComplete>(&VanquishComplete_Entry, &OnVanquishComplete);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::DungeonReward>(&VanquishComplete_Entry, &OnDungeonReward);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageServer>(&MessageServer_Entry, &OnServerMessage);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageNPC>(&MessageNPC_Entry,&OnNPCChatMessage);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MessageLocal>(&MessageLocal_Entry, &OnLocalChatMessage);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::MapLoaded>(&PlayerJoinInstance_Entry, &OnMapLoaded);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PlayerJoinInstance>(&PlayerJoinInstance_Entry, &OnPlayerJoinInstance);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PlayerLeaveInstance>(&PlayerLeaveInstance_Entry, &OnPlayerLeaveInstance);
    //GW::StoC::RegisterPostPacketCallback<GW::Packet::StoC::AgentAdd>(&OnAfterAgentAdd_Entry, &OnAfterAgentAdd);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentAdd>(&PartyDefeated_Entry, &OnAgentAdd);
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::AgentState>(&PartyDefeated_Entry, &OnUpdateAgentState);
    // Trigger for message on party change
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::PartyPlayerRemove>(
        &PartyPlayerRemove_Entry,
        [&](GW::HookStatus* status, GW::Packet::StoC::PartyPlayerRemove*) -> void {
            UNREFERENCED_PARAMETER(status);
            check_message_on_party_change = true;
        });
    GW::StoC::RegisterPacketCallback<GW::Packet::StoC::ScreenShake>(&OnScreenShake_Entry, &OnScreenShake);

    GW::UI::RegisterUIMessageCallback(&OnCheckboxPreferenceChanged_Entry, &OnCheckboxPreferenceChanged);
    GW::UI::RegisterUIMessageCallback(&OnChangeTarget_Entry, OnChangeTarget);
    GW::UI::RegisterUIMessageCallback(&OnPlayerChatMessage_Entry, OnPlayerChatMessage);
    GW::UI::RegisterUIMessageCallback(&OnWriteChat_Entry, OnWriteChat);
    GW::UI::RegisterUIMessageCallback(&OnAgentStartCast_Entry, OnAgentStartCast);
    GW::UI::RegisterUIMessageCallback(&OnOpenWikiUrl_Entry, OnOpenWiki);

    GW::UI::RegisterKeydownCallback(&OnChangeTarget_Entry, [this](GW::HookStatus*, uint32_t key) {
        if (key != static_cast<uint32_t>(GW::UI::ControlAction_TargetNearestItem))
            return;
        if (!lazy_chest_looting)
            return;
        targeting_nearest_item = true;
        GW::Agents::ChangeTarget((uint32_t)0); // To ensure OnChangeTarget is triggered
        });
    GW::UI::RegisterKeyupCallback(&OnChangeTarget_Entry, [this](GW::HookStatus*, uint32_t key) {
        if (key != static_cast<uint32_t>(GW::UI::ControlAction_TargetNearestItem))
            return;
        targeting_nearest_item = false;
        });
    GW::Chat::RegisterStartWhisperCallback(&StartWhisperCallback_Entry, &OnStartWhisper);
    GW::FriendListMgr::RegisterFriendStatusCallback(&FriendStatusCallback_Entry,&FriendStatusCallback);
    GW::CtoS::RegisterPacketCallback(&WhisperCallback_Entry, GAME_CMSG_PING_WEAPON_SET, &OnPingWeaponSet);
    GW::SkillbarMgr::RegisterUseSkillCallback(&OnCast_Entry, &OnCast);
    GW::Chat::RegisterSendChatCallback(&SendChatCallback_Entry, &OnSendChat);
    GW::Chat::RegisterWhisperCallback(&WhisperCallback_Entry, &WhisperCallback);
    GW::Chat::RegisterChatEventCallback(&OnPartyTargetChange_Entry, OnPartyTargetChange);



    GW::Chat::CreateCommand(L"reinvite", GameSettings::CmdReinvite);
#ifdef APRIL_FOOLS
    AF::ApplyPatchesIfItsTime();
#endif

}
// Same as GW::PartyMgr::GetPlayerIsLeader() but has an extra check to ignore disconnected people.
bool GameSettings::GetPlayerIsLeader() {
    GW::PartyInfo* party = GW::PartyMgr::GetPartyInfo();
    if (!(party && party->players.size())) return false;
    for (auto &player : party->players) {
        if (!player.connected())
            continue;
        return player.login_number == GW::PlayerMgr::GetPlayerNumber();
    }
    return false;
}

// Helper function; avoids doing string checks on offline friends.
GW::Friend* GameSettings::GetOnlineFriend(wchar_t* account, wchar_t* playing) {
    if (!(account || playing)) return NULL;
    GW::FriendList* fl = GW::FriendListMgr::GetFriendList();
    if (!fl) return NULL;
    uint32_t n_friends = fl->number_of_friend, n_found = 0;
    GW::FriendsListArray& friends = fl->friends;
    for (GW::Friend* it : friends) {
        if (n_found == n_friends) break;
        if (!it) continue;
        if (it->type != GW::FriendType::FriendType_Friend) continue;
        n_found++;
        if (it->status != GW::FriendStatus::FriendStatus_Online) continue;
        if (account && !wcsncmp(it->alias, account, 20))
            return it;
        if (playing && !wcsncmp(it->charname, playing, 20))
            return it;
    }
    return NULL;
}
void GameSettings::MessageOnPartyChange() {
    if (!check_message_on_party_change || GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost)
        return; // Don't need to check, or not an outpost.
    GW::PartyInfo* current_party = GW::PartyMgr::GetPartyInfo();
    GW::AgentLiving* me = GW::Agents::GetPlayerAsAgentLiving();
    if (!me || !current_party || !current_party->players.valid())
        return; // Party not ready yet.
    bool is_leading = false;
    std::vector<std::wstring> current_party_names;
    GW::PlayerPartyMemberArray current_party_players = current_party->players; // Copy the player array here to avoid ptr issues.
    for (size_t i = 0; i < current_party_players.size(); i++) {
        if (!current_party_players[i].login_number)
            continue;
        if (i == 0)
            is_leading = current_party_players[i].login_number == me->login_number;
        wchar_t* player_name = GW::Agents::GetPlayerNameByLoginNumber(current_party_players[i].login_number);
        if (!player_name)
            continue;
        current_party_names.push_back(player_name);
    }
    // If previous party list is empty (i.e. map change), then initialise
    if (!previous_party_names.size()) {
        previous_party_names = current_party_names;
        was_leading = is_leading;
    }
    if (current_party_names.size() > previous_party_names.size()) {
        // Someone joined my party
        for (size_t i = 0; i < current_party_names.size() && notify_when_party_member_joins; i++) {
            bool found = false;
            for (size_t j = 0; j < previous_party_names.size() && !found; j++) {
                found = previous_party_names[j] == current_party_names[i];
            }
            if (!found) {
                wchar_t buffer[128];
                swprintf(buffer, 128, L"<a=1>%ls</a> joined the party.", current_party_names[i].c_str());
                GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buffer);
            }
        }
    } else if (!was_leading && is_leading && current_party_names.size() == 1 && previous_party_names.size() > 2) {
        // We left a party of at least 2 other people
    } else if (current_party_names.size() < previous_party_names.size()) {
        // Someone left my party
        for (size_t i = 0; i < previous_party_names.size() && notify_when_party_member_leaves; i++) {
            bool found = false;
            for (size_t j = 0; j < current_party_names.size() && !found; j++) {
                found = previous_party_names[i] == current_party_names[j];
            }
            if (!found) {
                wchar_t buffer[128];
                swprintf(buffer, 128, L"<a=1>%ls</a> left the party.", previous_party_names[i].c_str());
                GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buffer);
            }
        }
    } 
    was_leading = is_leading;
    previous_party_names = current_party_names;
    check_message_on_party_change = false;
}
void GameSettings::LoadSettings(CSimpleIni* ini) {
    ToolboxModule::LoadSettings(ini);

    maintain_fov = ini->GetBoolValue(Name(), VAR_NAME(maintain_fov), false);
    fov = (float)ini->GetDoubleValue(Name(), VAR_NAME(fov), 1.308997f);
    disable_camera_smoothing = ini->GetBoolValue(Name(), VAR_NAME(disable_camera_smoothing), disable_camera_smoothing);
    tick_is_toggle = ini->GetBoolValue(Name(), VAR_NAME(tick_is_toggle), tick_is_toggle);
    show_timestamps = ini->GetBoolValue(Name(), VAR_NAME(show_timestamps), show_timestamps);
    show_timestamp_24h = ini->GetBoolValue(Name(), VAR_NAME(show_timestamp_24h), show_timestamp_24h);
    show_timestamp_seconds = ini->GetBoolValue(Name(), VAR_NAME(show_timestamp_seconds), show_timestamp_seconds);
    timestamps_color = Colors::Load(ini, Name(), VAR_NAME(timestamps_color), Colors::RGB(0xc0, 0xc0, 0xbf));

    shorthand_item_ping = ini->GetBoolValue(Name(), VAR_NAME(shorthand_item_ping), shorthand_item_ping);
    openlinks = ini->GetBoolValue(Name(), VAR_NAME(openlinks), true);
    auto_url = ini->GetBoolValue(Name(), VAR_NAME(auto_url), true);
    move_item_on_ctrl_click = ini->GetBoolValue(Name(), VAR_NAME(move_item_on_ctrl_click), move_item_on_ctrl_click);
    move_item_to_current_storage_pane = ini->GetBoolValue(Name(), VAR_NAME(move_item_to_current_storage_pane), move_item_to_current_storage_pane);
    move_materials_to_current_storage_pane = ini->GetBoolValue(Name(), VAR_NAME(move_materials_to_current_storage_pane), move_materials_to_current_storage_pane);

    flash_window_on_pm = ini->GetBoolValue(Name(), VAR_NAME(flash_window_on_pm), flash_window_on_pm);
    flash_window_on_party_invite = ini->GetBoolValue(Name(), VAR_NAME(flash_window_on_party_invite), flash_window_on_party_invite);
    flash_window_on_zoning = ini->GetBoolValue(Name(), VAR_NAME(flash_window_on_zoning), flash_window_on_zoning);
    flash_window_on_cinematic = ini->GetBoolValue(Name(), VAR_NAME(flash_window_on_cinematic), flash_window_on_cinematic);
    focus_window_on_launch = ini->GetBoolValue(Name(), VAR_NAME(focus_window_on_launch), focus_window_on_launch);
    focus_window_on_zoning = ini->GetBoolValue(Name(), VAR_NAME(focus_window_on_zoning), focus_window_on_zoning);
    flash_window_on_trade = ini->GetBoolValue(Name(), VAR_NAME(flash_window_on_trade), flash_window_on_trade);
    focus_window_on_trade = ini->GetBoolValue(Name(), VAR_NAME(focus_window_on_trade), focus_window_on_trade);
    flash_window_on_name_ping = ini->GetBoolValue(Name(), VAR_NAME(flash_window_on_name_ping), flash_window_on_name_ping);
    set_window_title_as_charname = ini->GetBoolValue(Name(), VAR_NAME(set_window_title_as_charname), set_window_title_as_charname);

    auto_set_away = ini->GetBoolValue(Name(), VAR_NAME(auto_set_away), auto_set_away);
    auto_set_away_delay = ini->GetLongValue(Name(), VAR_NAME(auto_set_away_delay), auto_set_away_delay);
    auto_set_online = ini->GetBoolValue(Name(), VAR_NAME(auto_set_online), auto_set_online);
    auto_return_on_defeat = ini->GetBoolValue(Name(), VAR_NAME(auto_return_on_defeat), auto_return_on_defeat);

    show_unlearned_skill = ini->GetBoolValue(Name(), VAR_NAME(show_unlearned_skill), show_unlearned_skill);
    auto_skip_cinematic = ini->GetBoolValue(Name(), VAR_NAME(auto_skip_cinematic), auto_skip_cinematic);

    hide_player_speech_bubbles = ini->GetBoolValue(Name(), VAR_NAME(hide_player_speech_bubbles), hide_player_speech_bubbles);
    npc_speech_bubbles_as_chat = ini->GetBoolValue(Name(), VAR_NAME(npc_speech_bubbles_as_chat), npc_speech_bubbles_as_chat);
    redirect_npc_messages_to_emote_chat = ini->GetBoolValue(Name(), VAR_NAME(redirect_npc_messages_to_emote_chat), redirect_npc_messages_to_emote_chat);

    faction_warn_percent = ini->GetBoolValue(Name(), VAR_NAME(faction_warn_percent), faction_warn_percent);
    faction_warn_percent_amount = ini->GetLongValue(Name(), VAR_NAME(faction_warn_percent_amount), faction_warn_percent_amount);
    stop_screen_shake = ini->GetBoolValue(Name(), VAR_NAME(stop_screen_shake), stop_screen_shake);

    disable_gold_selling_confirmation = ini->GetBoolValue(Name(), VAR_NAME(disable_gold_selling_confirmation), disable_gold_selling_confirmation);
    notify_when_friends_online = ini->GetBoolValue(Name(), VAR_NAME(notify_when_friends_online), notify_when_friends_online);
    notify_when_friends_offline = ini->GetBoolValue(Name(), VAR_NAME(notify_when_friends_offline), notify_when_friends_offline);
    notify_when_friends_join_outpost = ini->GetBoolValue(Name(), VAR_NAME(notify_when_friends_join_outpost), notify_when_friends_join_outpost);
    notify_when_friends_leave_outpost = ini->GetBoolValue(Name(), VAR_NAME(notify_when_friends_leave_outpost), notify_when_friends_leave_outpost);

    notify_when_party_member_leaves = ini->GetBoolValue(Name(), VAR_NAME(notify_when_party_member_leaves), notify_when_party_member_leaves);
    notify_when_party_member_joins = ini->GetBoolValue(Name(), VAR_NAME(notify_when_party_member_joins), notify_when_party_member_joins);
    notify_when_players_join_outpost = ini->GetBoolValue(Name(), VAR_NAME(notify_when_players_join_outpost), notify_when_players_join_outpost);
    notify_when_players_leave_outpost = ini->GetBoolValue(Name(), VAR_NAME(notify_when_players_leave_outpost), notify_when_players_leave_outpost);

    auto_age_on_vanquish = ini->GetBoolValue(Name(), VAR_NAME(auto_age_on_vanquish), auto_age_on_vanquish);
    hide_dungeon_chest_popup = ini->GetBoolValue(Name(), VAR_NAME(hide_dungeon_chest_popup), hide_dungeon_chest_popup);
    auto_age2_on_age = ini->GetBoolValue(Name(), VAR_NAME(auto_age2_on_age), auto_age2_on_age);
    auto_accept_invites = ini->GetBoolValue(Name(), VAR_NAME(auto_accept_invites), auto_accept_invites);
    auto_accept_join_requests = ini->GetBoolValue(Name(), VAR_NAME(auto_accept_join_requests), auto_accept_join_requests);

    skip_entering_name_for_faction_donate = ini->GetBoolValue(Name(), VAR_NAME(skip_entering_name_for_faction_donate), skip_entering_name_for_faction_donate);
    improve_move_to_cast = ini->GetBoolValue(Name(), VAR_NAME(improve_move_to_cast), improve_move_to_cast);
    drop_ua_on_cast = ini->GetBoolValue(Name(), VAR_NAME(drop_ua_on_cast), drop_ua_on_cast);

    lazy_chest_looting = ini->GetBoolValue(Name(), VAR_NAME(lazy_chest_looting), lazy_chest_looting);

    block_transmogrify_effect = ini->GetBoolValue(Name(), VAR_NAME(block_transmogrify_effect), block_transmogrify_effect);
    block_sugar_rush_effect = ini->GetBoolValue(Name(), VAR_NAME(block_sugar_rush_effect), block_sugar_rush_effect);
    block_snowman_summoner = ini->GetBoolValue(Name(), VAR_NAME(block_snowman_summoner), block_snowman_summoner);
    block_party_poppers = ini->GetBoolValue(Name(), VAR_NAME(block_party_poppers), block_party_poppers);
    block_bottle_rockets = ini->GetBoolValue(Name(), VAR_NAME(block_bottle_rockets), block_bottle_rockets);
    block_ghostinthebox_effect = ini->GetBoolValue(Name(), VAR_NAME(block_ghostinthebox_effect), block_ghostinthebox_effect);
    block_sparkly_drops_effect = ini->GetBoolValue(Name(), VAR_NAME(block_sparkly_drops_effect), block_sparkly_drops_effect);
    limit_signets_of_capture = ini->GetBoolValue(Name(), VAR_NAME(limit_signets_of_capture), limit_signets_of_capture);

    enable_camera_glitch_fix = ini->GetBoolValue(Name(), VAR_NAME(enable_camera_glitch_fix), enable_camera_glitch_fix);

    ::LoadChannelColor(ini, Name(), "local", GW::Chat::Channel::CHANNEL_ALL);
    ::LoadChannelColor(ini, Name(), "guild", GW::Chat::Channel::CHANNEL_GUILD);
    ::LoadChannelColor(ini, Name(), "team", GW::Chat::Channel::CHANNEL_GROUP);
    ::LoadChannelColor(ini, Name(), "trade", GW::Chat::Channel::CHANNEL_TRADE);
    ::LoadChannelColor(ini, Name(), "alliance", GW::Chat::Channel::CHANNEL_ALLIANCE);
    ::LoadChannelColor(ini, Name(), "whispers", GW::Chat::Channel::CHANNEL_WHISPER);
    ::LoadChannelColor(ini, Name(), "emotes", GW::Chat::Channel::CHANNEL_EMOTE);
    ::LoadChannelColor(ini, Name(), "other", GW::Chat::Channel::CHANNEL_GLOBAL);

    GW::PartyMgr::SetTickToggle(tick_is_toggle);
    GW::UI::SetOpenLinks(openlinks);
    GW::Chat::ToggleTimestamps(show_timestamps);
    GW::Chat::SetTimestampsColor(timestamps_color);
    GW::Chat::SetTimestampsFormat(show_timestamp_24h, show_timestamp_seconds);
    SetWindowTitle(set_window_title_as_charname);

    tome_patch.TogglePatch(show_unlearned_skill);
    gold_confirm_patch.TogglePatch(disable_gold_selling_confirmation);

    if (focus_window_on_launch) {
        FocusWindow();
    }
}

void GameSettings::RegisterSettingsContent() {
    ToolboxModule::RegisterSettingsContent();
    ToolboxModule::RegisterSettingsContent("Inventory Settings", ICON_FA_BOXES,
        [this](const std::string* section, bool is_showing) {
            UNREFERENCED_PARAMETER(section);
            if (!is_showing) return;
            DrawInventorySettings();
        }, 0.9f);

    ToolboxModule::RegisterSettingsContent("Chat Settings", ICON_FA_COMMENTS,
        [this](const std::string* section, bool is_showing) {
            UNREFERENCED_PARAMETER(section);
            if (!is_showing) return;
            DrawChatSettings();
        }, 0.9f);

    ToolboxModule::RegisterSettingsContent("Party Settings", ICON_FA_USERS,
        [this](const std::string* section, bool is_showing) {
            UNREFERENCED_PARAMETER(section);
            if (!is_showing) return;
            DrawPartySettings();
        }, 0.9f);
}

void GameSettings::Terminate() {
    ToolboxModule::Terminate();
    ctrl_click_patch.Reset();
    tome_patch.Reset();
    gold_confirm_patch.Reset();
}

void GameSettings::SaveSettings(CSimpleIni* ini) {
    ToolboxModule::SaveSettings(ini);

    ini->SetBoolValue(Name(), VAR_NAME(maintain_fov), maintain_fov);
    ini->SetDoubleValue(Name(), VAR_NAME(fov), fov);
    ini->SetBoolValue(Name(), VAR_NAME(tick_is_toggle), tick_is_toggle);

    ini->SetBoolValue(Name(), VAR_NAME(disable_camera_smoothing), disable_camera_smoothing);
    ini->SetBoolValue(Name(), VAR_NAME(show_timestamps), show_timestamps);
    ini->SetBoolValue(Name(), VAR_NAME(show_timestamp_24h), show_timestamp_24h);
    ini->SetBoolValue(Name(), VAR_NAME(show_timestamp_seconds), show_timestamp_seconds);
    Colors::Save(ini, Name(), VAR_NAME(timestamps_color), timestamps_color);

    ini->SetBoolValue(Name(), VAR_NAME(openlinks), openlinks);
    ini->SetBoolValue(Name(), VAR_NAME(auto_url), auto_url);
    ini->SetBoolValue(Name(), VAR_NAME(auto_return_on_defeat), auto_return_on_defeat);
    ini->SetBoolValue(Name(), VAR_NAME(shorthand_item_ping), shorthand_item_ping);
    
    ini->SetBoolValue(Name(), VAR_NAME(move_item_on_ctrl_click), move_item_on_ctrl_click);
    ini->SetBoolValue(Name(), VAR_NAME(move_item_to_current_storage_pane), move_item_to_current_storage_pane);
    ini->SetBoolValue(Name(), VAR_NAME(move_materials_to_current_storage_pane), move_materials_to_current_storage_pane);
    ini->SetBoolValue(Name(), VAR_NAME(stop_screen_shake), stop_screen_shake);

    ini->SetBoolValue(Name(), VAR_NAME(flash_window_on_pm), flash_window_on_pm);
    ini->SetBoolValue(Name(), VAR_NAME(flash_window_on_party_invite), flash_window_on_party_invite);
    ini->SetBoolValue(Name(), VAR_NAME(flash_window_on_zoning), flash_window_on_zoning);
    ini->SetBoolValue(Name(), VAR_NAME(focus_window_on_launch), focus_window_on_launch);
    ini->SetBoolValue(Name(), VAR_NAME(focus_window_on_zoning), focus_window_on_zoning);
    ini->SetBoolValue(Name(), VAR_NAME(flash_window_on_cinematic), flash_window_on_cinematic);
    ini->SetBoolValue(Name(), VAR_NAME(flash_window_on_trade), flash_window_on_trade);
    ini->SetBoolValue(Name(), VAR_NAME(focus_window_on_trade), focus_window_on_trade);
    ini->SetBoolValue(Name(), VAR_NAME(flash_window_on_name_ping), flash_window_on_name_ping);
    ini->SetBoolValue(Name(), VAR_NAME(set_window_title_as_charname), set_window_title_as_charname);

    ini->SetBoolValue(Name(), VAR_NAME(auto_set_away), auto_set_away);
    ini->SetLongValue(Name(), VAR_NAME(auto_set_away_delay), auto_set_away_delay);
    ini->SetBoolValue(Name(), VAR_NAME(auto_set_online), auto_set_online);

    ini->SetBoolValue(Name(), VAR_NAME(show_unlearned_skill), show_unlearned_skill);
    ini->SetBoolValue(Name(), VAR_NAME(auto_skip_cinematic), auto_skip_cinematic);

    
    ini->SetBoolValue(Name(), VAR_NAME(hide_player_speech_bubbles), hide_player_speech_bubbles);
    ini->SetBoolValue(Name(), VAR_NAME(npc_speech_bubbles_as_chat), npc_speech_bubbles_as_chat);
    ini->SetBoolValue(Name(), VAR_NAME(redirect_npc_messages_to_emote_chat), redirect_npc_messages_to_emote_chat);

    ini->SetBoolValue(Name(), VAR_NAME(faction_warn_percent), faction_warn_percent);
    ini->SetLongValue(Name(), VAR_NAME(faction_warn_percent_amount), faction_warn_percent_amount);
    ini->SetBoolValue(Name(), VAR_NAME(disable_gold_selling_confirmation), disable_gold_selling_confirmation);

    ini->SetBoolValue(Name(), VAR_NAME(notify_when_friends_online), notify_when_friends_online);
    ini->SetBoolValue(Name(), VAR_NAME(notify_when_friends_offline), notify_when_friends_offline);
    ini->SetBoolValue(Name(), VAR_NAME(notify_when_friends_join_outpost), notify_when_friends_join_outpost);
    ini->SetBoolValue(Name(), VAR_NAME(notify_when_friends_leave_outpost), notify_when_friends_leave_outpost);

    ini->SetBoolValue(Name(), VAR_NAME(notify_when_party_member_leaves), notify_when_party_member_leaves);
    ini->SetBoolValue(Name(), VAR_NAME(notify_when_party_member_joins), notify_when_party_member_joins);
    ini->SetBoolValue(Name(), VAR_NAME(notify_when_players_join_outpost), notify_when_players_join_outpost);
    ini->SetBoolValue(Name(), VAR_NAME(notify_when_players_leave_outpost), notify_when_players_leave_outpost);

    ini->SetBoolValue(Name(), VAR_NAME(auto_age_on_vanquish), auto_age_on_vanquish);
    ini->SetBoolValue(Name(), VAR_NAME(hide_dungeon_chest_popup), hide_dungeon_chest_popup);
    ini->SetBoolValue(Name(), VAR_NAME(auto_age2_on_age), auto_age2_on_age);
    ini->SetBoolValue(Name(), VAR_NAME(auto_accept_invites), auto_accept_invites);
    ini->SetBoolValue(Name(), VAR_NAME(auto_accept_join_requests), auto_accept_join_requests);
    ini->SetBoolValue(Name(), VAR_NAME(skip_entering_name_for_faction_donate), skip_entering_name_for_faction_donate);
    ini->SetBoolValue(Name(), VAR_NAME(improve_move_to_cast), improve_move_to_cast);
    ini->SetBoolValue(Name(), VAR_NAME(drop_ua_on_cast), drop_ua_on_cast);

    ini->SetBoolValue(Name(), VAR_NAME(lazy_chest_looting), lazy_chest_looting);

    ini->SetBoolValue(Name(), VAR_NAME(block_transmogrify_effect), block_transmogrify_effect);
    ini->SetBoolValue(Name(), VAR_NAME(block_sugar_rush_effect), block_sugar_rush_effect);
    ini->SetBoolValue(Name(), VAR_NAME(block_snowman_summoner), block_snowman_summoner);
    ini->SetBoolValue(Name(), VAR_NAME(block_party_poppers), block_party_poppers);
    ini->SetBoolValue(Name(), VAR_NAME(block_bottle_rockets), block_bottle_rockets);
    ini->SetBoolValue(Name(), VAR_NAME(block_ghostinthebox_effect), block_ghostinthebox_effect);
    ini->SetBoolValue(Name(), VAR_NAME(block_sparkly_drops_effect), block_sparkly_drops_effect);
    ini->SetBoolValue(Name(), VAR_NAME(limit_signets_of_capture), limit_signets_of_capture);

    ini->SetBoolValue(Name(), VAR_NAME(enable_camera_glitch_fix), enable_camera_glitch_fix);

    ::SaveChannelColor(ini, Name(), "local", GW::Chat::Channel::CHANNEL_ALL);
    ::SaveChannelColor(ini, Name(), "guild", GW::Chat::Channel::CHANNEL_GUILD);
    ::SaveChannelColor(ini, Name(), "team", GW::Chat::Channel::CHANNEL_GROUP);
    ::SaveChannelColor(ini, Name(), "trade", GW::Chat::Channel::CHANNEL_TRADE);
    ::SaveChannelColor(ini, Name(), "alliance", GW::Chat::Channel::CHANNEL_ALLIANCE);
    ::SaveChannelColor(ini, Name(), "whispers", GW::Chat::Channel::CHANNEL_WHISPER);
    ::SaveChannelColor(ini, Name(), "emotes", GW::Chat::Channel::CHANNEL_EMOTE);
    ::SaveChannelColor(ini, Name(), "other", GW::Chat::Channel::CHANNEL_GLOBAL);
}

void GameSettings::DrawInventorySettings() {
    ImGui::Checkbox("Move items from/to storage with Control+Click", &move_item_on_ctrl_click);
    ImGui::Indent();
    ImGui::Checkbox("Move items to current open storage pane on click", &move_item_to_current_storage_pane);
    ImGui::ShowHelp("Materials follow different logic, see below");
    ImGui::Indent();
    char* logic = "Storage logic: Any available stack/slot";
    if (move_item_to_current_storage_pane) {
        logic = "Storage logic: Current storage pane > Any available stack/slot";
    }
    ImGui::TextDisabled(logic);
    ImGui::Unindent();
    ImGui::Checkbox("Move materials to current open storage pane on click", &move_materials_to_current_storage_pane);
    ImGui::Indent();
    logic = "Storage logic: Materials pane > Any available stack/slot";
    if (move_materials_to_current_storage_pane) {
        logic = "Storage logic: Current storage pane > Materials pane > Any available stack/slot";
    }
    else if (move_item_to_current_storage_pane) {
        logic = "Storage logic: Materials pane > Current storage pane > Any available stack/slot";
    }
    ImGui::TextDisabled(logic);
    ImGui::Unindent();
    ImGui::Unindent();
    ImGui::Checkbox("Shorthand item description on weapon ping", &shorthand_item_ping);
    ImGui::ShowHelp("Include a concise description of your equipped weapon when ctrl+clicking a weapon set");

    ImGui::Checkbox("Lazy chest looting", &lazy_chest_looting);
    ImGui::ShowHelp("Toolbox will try to target any nearby reserved items\nwhen using the 'target nearest item' key next to a chest\nto pick stuff up.");
}

void GameSettings::DrawPartySettings() {
    if(ImGui::Checkbox("Tick is a toggle", &tick_is_toggle))
        GW::PartyMgr::SetTickToggle(tick_is_toggle);
    ImGui::ShowHelp("Ticking in party window will work as a toggle instead of opening the menu");
    ImGui::Checkbox("Automatically accept party invitations when ticked", &auto_accept_invites);
    ImGui::ShowHelp("When you're invited to join someone elses party");
    ImGui::Checkbox("Automatically accept party join requests when ticked", &auto_accept_join_requests);
    ImGui::ShowHelp("When a player wants to join your existing party");
}

void GameSettings::DrawChatSettings() {
    ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoLabel;
    if (ImGui::TreeNodeEx("Chat Colors", ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::Text("Channel");
        ImGui::SameLine(chat_colors_grid_x[1]);
        ImGui::Text("Sender");
        ImGui::SameLine(chat_colors_grid_x[2]);
        ImGui::Text("Message");
        ImGui::Spacing();

        DrawChannelColor("Local", GW::Chat::Channel::CHANNEL_ALL);
        DrawChannelColor("Guild", GW::Chat::Channel::CHANNEL_GUILD);
        DrawChannelColor("Team", GW::Chat::Channel::CHANNEL_GROUP);
        DrawChannelColor("Trade", GW::Chat::Channel::CHANNEL_TRADE);
        DrawChannelColor("Alliance", GW::Chat::Channel::CHANNEL_ALLIANCE);
        DrawChannelColor("Whispers", GW::Chat::Channel::CHANNEL_WHISPER);
        DrawChannelColor("Emotes", GW::Chat::Channel::CHANNEL_EMOTE);
        DrawChannelColor("Other", GW::Chat::Channel::CHANNEL_GLOBAL);

        ImGui::TextDisabled("(Left-click on a color to edit it)");
        ImGui::TreePop();
        ImGui::Spacing();
    }
    if (ImGui::Checkbox("Show chat messages timestamp", &show_timestamps))
        GW::Chat::ToggleTimestamps(show_timestamps);
    ImGui::ShowHelp("Show timestamps in message history.");
    if (show_timestamps) {
        ImGui::Indent();
        if (ImGui::Checkbox("Use 24h", &show_timestamp_24h))
            GW::Chat::SetTimestampsFormat(show_timestamp_24h, show_timestamp_seconds);
        ImGui::SameLine();
        if (ImGui::Checkbox("Show seconds", &show_timestamp_seconds))
            GW::Chat::SetTimestampsFormat(show_timestamp_24h, show_timestamp_seconds);
        ImGui::SameLine();
        ImGui::Text("Color:");
        ImGui::SameLine();
        if (Colors::DrawSettingHueWheel("Color:", &timestamps_color, flags))
            GW::Chat::SetTimestampsColor(timestamps_color);
        ImGui::Unindent();
    }
    ImGui::Checkbox("Hide player chat speech bubbles", &hide_player_speech_bubbles);
    ImGui::ShowHelp("Don't show in-game speech bubbles over player characters that send a message in chat");
    ImGui::Checkbox("Show NPC speech bubbles in emote channel", &npc_speech_bubbles_as_chat);
    ImGui::ShowHelp("Speech bubbles from NPCs and Heroes will appear as emote messages in chat");
    ImGui::Checkbox("Redirect NPC dialog to emote channel", &redirect_npc_messages_to_emote_chat);
    ImGui::ShowHelp("Messages from NPCs that would normally show on-screen and in team chat are instead redirected to the emote channel");
    if (ImGui::Checkbox("Open web links from templates", &openlinks)) {
        GW::UI::SetOpenLinks(openlinks);
    }
    ImGui::ShowHelp("Clicking on template that has a URL as name will open that URL in your browser");

    ImGui::Checkbox("Automatically change urls into build templates.", &auto_url);
    ImGui::ShowHelp("When you write a message starting with 'http://' or 'https://', it will be converted in template format");
}

void GameSettings::DrawSettingInternal() {
    constexpr float checkbox_w = 270.f;
    ImGui::Checkbox("Automatic /age on vanquish", &auto_age_on_vanquish);
    ImGui::ShowHelp("As soon as a vanquish is complete, send /age command to game server to receive server-side completion time.");
    ImGui::Checkbox("Automatic /age2 on /age", &auto_age2_on_age);
    ImGui::ShowHelp("GWToolbox++ will show /age2 time after /age is shown in chat");
    ImGui::Text("Flash Guild Wars taskbar icon when:");
    ImGui::ShowHelp("Only triggers when Guild Wars is not the active window");
    ImGui::Indent();
    ImGui::StartSpacedElements(checkbox_w);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Receiving a private message", &flash_window_on_pm);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Receiving a party invite", &flash_window_on_party_invite);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Zoning in a new map", &flash_window_on_zoning);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Cinematic start/end", &flash_window_on_cinematic);
    ImGui::NextSpacedElement(); ImGui::Checkbox("A player starts trade with you###flash_window_on_trade", &flash_window_on_trade);
    ImGui::NextSpacedElement(); ImGui::Checkbox("A party member pings your name", &flash_window_on_name_ping);
    ImGui::Unindent();

    ImGui::Text("Show Guild Wars in foreground when:");
    ImGui::ShowHelp("When enabled, GWToolbox++ can automatically restore\n"
        "the window from a minimized state when important events\n"
        "occur, such as entering instances.");
    ImGui::Indent();
    ImGui::StartSpacedElements(checkbox_w);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Launching GWToolbox++###focus_window_on_launch", &focus_window_on_launch);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("Zoning in a new map###focus_window_on_zoning", &focus_window_on_zoning);
    ImGui::NextSpacedElement();
    ImGui::Checkbox("A player starts trade with you###focus_window_on_trade", &focus_window_on_trade);
    ImGui::Unindent();

    ImGui::Text("Show a message when a friend:");
    ImGui::Indent();
    ImGui::StartSpacedElements(checkbox_w);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Logs in", &notify_when_friends_online);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Joins your outpost###notify_when_friends_join_outpost", &notify_when_friends_join_outpost);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Logs out", &notify_when_friends_offline);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Leaves your outpost###notify_when_friends_leave_outpost", &notify_when_friends_leave_outpost);
    ImGui::Unindent();

    ImGui::Text("Show a message when a player:");
    ImGui::Indent();
    ImGui::StartSpacedElements(checkbox_w);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Joins your party", &notify_when_party_member_joins);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Joins your outpost###notify_when_players_join_outpost", &notify_when_players_join_outpost);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Leaves your party", &notify_when_party_member_leaves);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Leaves your outpost###notify_when_players_leave_outpost", &notify_when_players_leave_outpost);
    ImGui::Unindent();

    ImGui::Checkbox("Automatically set 'Away' after ", &auto_set_away);
    ImGui::SameLine();
    ImGui::PushItemWidth(50.0f * ImGui::GetIO().FontGlobalScale);
    ImGui::InputInt("##awaydelay", &auto_set_away_delay, 0);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Text("minutes of inactivity");
    ImGui::ShowHelp("Only if you were 'Online'");

    ImGui::Checkbox("Automatically set 'Online' after an input to Guild Wars", &auto_set_online);
    ImGui::ShowHelp("Only if you were 'Away'");

    if (ImGui::Checkbox("Only show non learned skills when using a tome", &show_unlearned_skill)) {
        tome_patch.TogglePatch(show_unlearned_skill);
    }

    ImGui::Checkbox("Automatically skip cinematics", &auto_skip_cinematic);
    ImGui::Checkbox("Automatically return to outpost on defeat", &auto_return_on_defeat);
    ImGui::ShowHelp("Automatically return party to outpost on party wipe if player is leading");
    ImGui::Checkbox("Show warning when earned faction reaches ", &faction_warn_percent);
    ImGui::SameLine();
    ImGui::PushItemWidth(40.0f * ImGui::GetIO().FontGlobalScale);
    ImGui::InputInt("##faction_warn_percent_amount", &faction_warn_percent_amount, 0);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    ImGui::Text("%%");
    ImGui::ShowHelp("Displays when in a challenge mission or elite mission outpost");
    ImGui::Checkbox("Skip character name input when donating faction", &skip_entering_name_for_faction_donate);

    if (ImGui::Checkbox("Disable Gold/Green items confirmation", &disable_gold_selling_confirmation)) {
        gold_confirm_patch.TogglePatch(disable_gold_selling_confirmation);
    }
    ImGui::ShowHelp(
        "Disable the confirmation request when\n"
        "selling Gold and Green items introduced\n"
        "in February 5, 2019 update.");
    ImGui::Checkbox("Hide dungeon chest popup", &hide_dungeon_chest_popup);
    ImGui::Checkbox("Stop screen shake from skills or effects", &stop_screen_shake);
    ImGui::ShowHelp("e.g. Aftershock, Earth shaker, Avalanche effect");
    if (ImGui::Checkbox("Set Guild Wars window title as current logged-in character", &set_window_title_as_charname)) {
        SetWindowTitle(set_window_title_as_charname);
    }
    ImGui::Checkbox("Disable camera smoothing", &disable_camera_smoothing);
    ImGui::Checkbox("Enable Camera Glitch Fix", &enable_camera_glitch_fix);
    ImGui::ShowHelp("Guild Wars handles right clicking to look around in a way\n"
        "that can make the camera 'jump' after releasing the right mouse button.\n"
        "Enable this feature to let gwtoolbox intercept and fix it.");
    ImGui::Checkbox("Improve move to cast spell range", &improve_move_to_cast);
    ImGui::ShowHelp("This should make you stop to cast skills earlier by re-triggering the skill cast when in range.");
    ImGui::Checkbox("Auto-cancel Unyielding Aura when re-casting",&drop_ua_on_cast);
    ImGui::Text("Disable animation and sound from consumables:");
    ImGui::Indent();
    ImGui::StartSpacedElements(300.f);
    constexpr char* doesnt_affect_me = "Only applies to other players";
    ImGui::NextSpacedElement(); ImGui::Checkbox("Tonics", &block_transmogrify_effect);
    ImGui::ShowHelp(doesnt_affect_me);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Sweets", &block_sugar_rush_effect);
    ImGui::ShowHelp(doesnt_affect_me);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Bottle rockets", &block_bottle_rockets);
    ImGui::ShowHelp(doesnt_affect_me);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Party poppers", &block_party_poppers);
    ImGui::NextSpacedElement(); ImGui::Checkbox("Snowman Summoners", &block_snowman_summoner);
    ImGui::ShowHelp(doesnt_affect_me);
#if 0
    //@Cleanup: Ghost in the box spawn effect suppressed, but still need to figure out how to suppress the death effect.
    ImGui::SameLine(column_spacing); ImGui::Checkbox("Ghost-in-the-box", &block_ghostinthebox_effect);
    ImGui::ShowHelp("Also applies to ghost-in-the-boxes that you use");
#endif
    ImGui::Unindent();
    ImGui::Checkbox("Block sparkle effect on dropped items", &block_sparkly_drops_effect);
    ImGui::ShowHelp("Applies to drops that appear after this setting has been changed");
    ImGui::Checkbox("Limit signet of capture to 10 in skills window", &limit_signets_of_capture);
    ImGui::ShowHelp("If your character has purchased more than 10 signets of capture, only show 10 of them in the skills window");
}

void GameSettings::FactionEarnedCheckAndWarn() {
    if (!faction_warn_percent)
        return; // Disabled
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) {
        faction_checked = false;
        return; // Loading or explorable area.
    }
    if (faction_checked)
        return; // Already checked.
    faction_checked = true;
    GW::WorldContext * world_context = GW::GameContext::instance()->world;
    if (!world_context || !world_context->max_luxon || !world_context->total_earned_kurzick) {
        faction_checked = false;
        return; // No world context yet.
    }
    float percent;
    // Avoid invalid user input values.
    if (faction_warn_percent_amount < 0)
        faction_warn_percent_amount = 0; 
    if (faction_warn_percent_amount > 100)
        faction_warn_percent_amount = 100;
    // Warn user to dump faction if we're in a luxon/kurzick mission outpost
    switch (GW::Map::GetMapID()) {
        case GW::Constants::MapID::The_Deep:
        case GW::Constants::MapID::The_Jade_Quarry_Luxon_outpost:
        case GW::Constants::MapID::Fort_Aspenwood_Luxon_outpost:
        case GW::Constants::MapID::Zos_Shivros_Channel:
        case GW::Constants::MapID::The_Aurios_Mines:
            // Player is in luxon mission outpost
            percent = 100.0f * (float)world_context->current_luxon / (float)world_context->max_luxon;
            if (percent >= (float)faction_warn_percent_amount) {
                // Faction earned is over 75% capacity
                Log::Warning("Luxon faction earned is %d of %d", world_context->current_luxon, world_context->max_luxon );
            }
            else if (world_context->current_kurzick > 4999 && world_context->current_kurzick > world_context->current_luxon) {
                // Kurzick faction > Luxon faction
                Log::Warning("Kurzick faction earned is greater than Luxon");
            }
            break;
        case GW::Constants::MapID::Urgozs_Warren:
        case GW::Constants::MapID::The_Jade_Quarry_Kurzick_outpost:
        case GW::Constants::MapID::Fort_Aspenwood_Kurzick_outpost:
        case GW::Constants::MapID::Altrumm_Ruins:
        case GW::Constants::MapID::Amatz_Basin:
            // Player is in kurzick mission outpost
            percent = 100.0f * (float)world_context->current_kurzick / (float)world_context->max_kurzick;
            if (percent >= (float)faction_warn_percent_amount) {
                // Faction earned is over 75% capacity
                Log::Warning("Kurzick faction earned is %d of %d", world_context->current_kurzick, world_context->max_kurzick);
            }
            else if (world_context->current_luxon > 4999 && world_context->current_luxon > world_context->current_kurzick) {
                // Luxon faction > Kurzick faction
                Log::Warning("Luxon faction earned is greater than Kurzick");
            }
            break;
        default:
            break;
    }
}
void GameSettings::SetAfkMessage(std::wstring&& message) {
    
    static size_t MAX_AFK_MSG_LEN = 80;
    if (message.size() <= MAX_AFK_MSG_LEN) {
        afk_message = message;
        afk_message_time = clock();
        Log::Info("Afk message set to \"%S\"", afk_message.c_str());
    } else {
        Log::Error("Afk message must be under 80 characters. (Yours is %zu)", message.size());
    }
}

void GameSettings::Update(float) {
    // See OnSendChat
    if (pending_wiki_search_term && pending_wiki_search_term->wstring().length()) {
        GuiUtils::SearchWiki(pending_wiki_search_term->wstring());
        delete pending_wiki_search_term;
        pending_wiki_search_term = 0;
    }

    // Try to print any pending messages.
    for (auto it = pending_messages.begin(); it != pending_messages.end(); ++it) {
        PendingChatMessage *m = *it;
        if (m->IsSend() && PendingChatMessage::Cooldown()) 
            continue;
        if (m->Consume()) {
            it = pending_messages.erase(it);
            delete m;
            if (it == pending_messages.end()) break;
        }
    }
    if (auto_set_away
        && TIMER_DIFF(activity_timer) > auto_set_away_delay * 60000
        && GW::FriendListMgr::GetMyStatus() == (DWORD)GW::Constants::OnlineStatus::ONLINE) {
        GW::FriendListMgr::SetFriendListStatus(GW::Constants::OnlineStatus::AWAY);
        activity_timer = TIMER_INIT(); // refresh the timer to avoid spamming in case the set status call fails
    }
    //UpdateFOV();
    FactionEarnedCheckAndWarn();

    if (disable_camera_smoothing && !GW::CameraMgr::GetCameraUnlock()) {
        GW::Camera* cam = GW::CameraMgr::GetCamera();
        cam->position = cam->camera_pos_to_go;
        cam->look_at_target = cam->look_at_to_go;
        cam->yaw = cam->yaw_to_go;
        cam->pitch = cam->pitch_to_go;
    }

    if (improve_move_to_cast && pending_cast.target_id) {
        const GW::AgentLiving *me = GW::Agents::GetCharacter();
        const GW::Skillbar *skillbar = GW::SkillbarMgr::GetPlayerSkillbar();
        if (!me || !skillbar) // I don't exist e.g. map change
            return pending_cast.reset();

        const GW::AgentLiving* target = pending_cast.GetTarget();
        if (!target)  // Target no longer valid
            return pending_cast.reset();

        const uint32_t& casting = skillbar->casting;
        if (pending_cast.cast_next_frame) { // Do cast now
            if(pending_cast.GetTarget())
                GW::SkillbarMgr::UseSkill(pending_cast.slot, pending_cast.target_id, pending_cast.call_target);
            pending_cast.reset();
            return;
        }

        if (casting && me->GetIsMoving() && !me->skill && !me->GetIsCasting()) { // casting/skill don't update fast enough, so delay the rupt
            const uint32_t cast_skill = pending_cast.GetSkill();
            if (!cast_skill) // Skill ID no longer valid
                return pending_cast.reset();

            const float range = GetSkillRange(cast_skill);
            if (GW::GetDistance(target->pos, me->pos) <= range && range > 0) {
                GW::UI::Keypress(GW::UI::ControlAction::ControlAction_MoveBackward);
                pending_cast.cast_next_frame = true;
                return;
            }
        }
        if (!casting) // abort the action if not auto walking anymore
            return pending_cast.reset();
    }

#ifdef APRIL_FOOLS
    AF::ApplyPatchesIfItsTime();
#endif
    if (notify_when_party_member_joins || notify_when_party_member_leaves) {
        if (!check_message_on_party_change && !GW::Map::GetIsMapLoaded()) {
            // Map changed - clear previous party names, flag to re-populate on map load
            previous_party_names.clear();
            check_message_on_party_change = true;
        }
        if (check_message_on_party_change)
            GameSettings::MessageOnPartyChange();
    }
    // Re-invite logic
    if (pending_reinvite_type != None) {
        if (!pending_reinvite_id || GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost) {
            pending_reinvite_type = None;
        }
    }
    switch (pending_reinvite_type) {
    case Player: {
        GW::PartyInfo* party = GW::PartyMgr::GetPartyInfo();
        bool still_in_party = false;
        for (size_t i = 0; party && party->players.valid() && i < party->players.size(); i++) {
            auto& member = party->players[i];
            if (member.login_number == pending_reinvite_id) {
                still_in_party = true;
                break;
            }
        }
        GW::Player* player = GW::PlayerMgr::GetPlayerByID(pending_reinvite_id);
        if (!player) {
            pending_reinvite_type = None;
            break;
        }
        if (!still_in_party) {
            wchar_t message[64];
            swprintf(message, _countof(message), L"invite %s", player->name);
            GW::Chat::SendChat('/', message);
            pending_reinvite_type = None;
        }
    } break;
    case Hero: {
        GW::PartyInfo* party = GW::PartyMgr::GetPartyInfo();
        bool still_in_party = false;
        uint32_t my_id = GW::PlayerMgr::GetPlayerNumber();
        for (size_t i = 0; party && party->heroes.valid() && i < party->heroes.size(); i++) {
            auto& member = party->heroes[i];
            if (member.hero_id == pending_reinvite_id && member.owner_player_id == my_id) {
                still_in_party = true;
                break;
            }
        }
        if (!still_in_party) {
            GW::CtoS::SendPacket(0x8, GAME_CMSG_HERO_ADD, pending_reinvite_id);
            pending_reinvite_type = None;
        }
    } break;
    case Henchman: {
        GW::PartyInfo* party = GW::PartyMgr::GetPartyInfo();
        bool still_in_party = false;
        bool is_leading = false;
        uint32_t my_id = GW::PlayerMgr::GetPlayerNumber();
        for (size_t i = 0; party && party->players.valid() && i < party->players.size(); i++) {
            auto& member = party->players[i];
            if (!member.connected())
                continue;
            is_leading = member.login_number == my_id;
            break;
        }
        if (!is_leading) {
            pending_reinvite_type = None;
            break;
        }
        for (size_t i = 0; party && party->henchmen.valid() && i < party->henchmen.size(); i++) {
            auto& member = party->henchmen[i];
            if (member.agent_id == pending_reinvite_id) {
                still_in_party = true;
                break;
            }
        }
        if (!still_in_party) {
            GW::CtoS::SendPacket(0x8, GAME_CMSG_PARTY_INVITE_NPC, pending_reinvite_id);
            pending_reinvite_type = None;
        }
    } break;
    }
}

void GameSettings::DrawFOVSetting() {
    ImGui::Checkbox("Maintain FOV", &maintain_fov);
    ImGui::ShowHelp("GWToolbox will save and maintain the FOV setting used with /cam fov <value>");
}

void GameSettings::UpdateFOV() {
    if (maintain_fov && GW::CameraMgr::GetFieldOfView() != fov) {
        GW::CameraMgr::SetFieldOfView(fov);
    }
}

// Guild Wars does something stupid with the camera position, 
// sending window events to move the mouse to the middle of the screen on the game thread.
// Seems to be an issue when the events come in before the game thread has processed the camera movement.
// Causes camera to "jump" after right click is released.
// Rather than mess around trying to calculate camera yaw/pitch etc, just manage the window events better.
// Its hacky, but so is the GW logic in-game
// 
// Return true if event needs to be blocked due to camera glitch
bool GameSettings::WndProcCameraGlitchFix(UINT Message, WPARAM wParam, LPARAM lParam) {
    if (!enable_camera_glitch_fix)
        return false;
    switch (Message) {
    case WM_RBUTTONDOWN:
        original_right_mouse_down_lparam = lParam;
        break;
    case WM_RBUTTONUP:
        if (right_mouse_down_lparam && lParam != original_right_mouse_down_lparam) {
            // Re-send the right mouse up command to put the cursor back in the original position.
            SendMessage(GW::MemoryMgr::GetGWWindowHandle(), Message, wParam, original_right_mouse_down_lparam);
            return true;
        }
        break;
    case WM_MOUSEMOVE:
        if (wParam != MK_RBUTTON) {
            right_mouse_down_lparam = 0;
            break;
        }
        if (!right_mouse_down_lparam) {
            right_mouse_down_lparam = lParam;
            RECT rect;
            ASSERT(GetWindowRect(GW::MemoryMgr::GetGWWindowHandle(), &rect));
            middle_x = (int)((rect.right - rect.left) / 2);
            middle_y = (int)((rect.bottom - rect.top) / 2);
        }
        int mouse_x = GET_X_LPARAM(lParam);
        int mouse_y = GET_Y_LPARAM(lParam);
        // Guild Wars forces the cursor into the middle of the screen, calculates the offset of this movement, and then sends an override WM_MOUSEMOVE.
        // Block mouse movement if the cursor has jumped too far from the mid point of the screen (i.e. camera jump)
        if (abs(mouse_x - middle_x) > 128
            || abs(mouse_y - middle_y) > 128)
            return true;
        break;
    }
    return false;
}

bool GameSettings::WndProc(UINT Message, WPARAM wParam, LPARAM lParam) {
    UNREFERENCED_PARAMETER(lParam);
    if (WndProcCameraGlitchFix(Message, wParam, lParam))
        return true;
    // Open Whisper to targeted player with Ctrl + Enter
    if (Message == WM_KEYDOWN
        && wParam == VK_RETURN
        && !ctrl_enter_whisper
        && ImGui::GetIO().KeyCtrl
        && !GW::Chat::GetIsTyping()) {
        GW::AgentLiving* target = GW::Agents::GetTargetAsAgentLiving();
        if (target && target->IsPlayer()) {
            const wchar_t* player_name = GW::PlayerMgr::GetPlayerName(target->login_number);
            ctrl_enter_whisper = true;
            GW::GameThread::Enqueue([player_name]() {
                GW::UI::SendUIMessage(GW::UI::kOpenWhisper, (wchar_t*)player_name, nullptr);
                ctrl_enter_whisper = false;
                });
            return true;
        }
    }

    // I don't know what would be the best solution here, but the way we capture every messages as a sign of activity can be bad.
    // Added that because when someone was typing "/afk message" he was put online directly, because "enter-up" was captured.
    if (Message == WM_KEYUP)
        return false;

    activity_timer = TIMER_INIT();
    static clock_t set_online_timer = TIMER_INIT();
    if (auto_set_online
        && TIMER_DIFF(set_online_timer) > 5000 // to avoid spamming in case of failure
        && GW::FriendListMgr::GetMyStatus() == (DWORD)GW::Constants::OnlineStatus::AWAY) {
        printf("%X\n", Message);
        GW::FriendListMgr::SetFriendListStatus(GW::Constants::OnlineStatus::ONLINE);
        set_online_timer = TIMER_INIT();
    }

    return false;
}

void GameSettings::FriendStatusCallback(
    GW::HookStatus *,
    GW::Friend* f,
    GW::FriendStatus status,
    const wchar_t *alias,
    const wchar_t *charname) {
    
    if (!f || !charname || *charname == L'\0')
        return;

    GameSettings& game_setting = GameSettings::Instance();
    if (status == f->status)
        return;
    wchar_t buffer[512];
    switch (status) {
    case GW::FriendStatus::FriendStatus_Offline:
        if (game_setting.notify_when_friends_offline) {
            swprintf(buffer, _countof(buffer), L"%s (%s) has just logged out.", charname, alias);
            GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buffer);
        }
        return;
    case GW::FriendStatus::FriendStatus_Away:
    case GW::FriendStatus::FriendStatus_DND:
    case GW::FriendStatus::FriendStatus_Online:
        if (f->status != GW::FriendStatus::FriendStatus_Offline)
            return;
        if (game_setting.notify_when_friends_online) {
            swprintf(buffer, _countof(buffer), L"<a=1>%s</a> (%s) has just logged in.</c>", charname, alias);
            GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buffer);
        }
        return;
    }
}

// Show weapon description/mods when pinged
void GameSettings::OnPingWeaponSet(GW::HookStatus* status, void* packet) {
    if (!Instance().shorthand_item_ping)
        return;
    struct PingItemPacket {
        uint32_t header;
        uint32_t unk0;
        uint32_t item_id_1;
        uint32_t item_id_2;
    };
    PingItemPacket* pack = static_cast<PingItemPacket*>(packet);
    PingItem(pack->item_id_1, PING_PARTS::NAME | PING_PARTS::DESC);
    PingItem(pack->item_id_2, PING_PARTS::NAME | PING_PARTS::DESC);
    status->blocked = true;
}

// Show a message when player joins the outpost
void GameSettings::OnPlayerJoinInstance(GW::HookStatus* status, GW::Packet::StoC::PlayerJoinInstance* pak) {
    UNREFERENCED_PARAMETER(status);
    GameSettings *instance = &Instance();
    if (!instance->notify_when_players_join_outpost && !instance->notify_when_friends_join_outpost)
        return; // Dont notify about player joining
    if (!pak->player_name || GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost)
        return; // Only message in an outpost.
    if (TIMER_DIFF(instance_entered_at) < 2000)
        return; // Only been in this map for less than 2 seconds; avoids spam on map load.
    if (!GW::Agents::GetPlayerId())
        return; // Current player not loaded in yet
    GW::Agent* agent = GW::Agents::GetAgentByID(pak->agent_id);
    if (agent)
        return; // Player already joined
    if (instance->notify_when_friends_join_outpost) {
        GW::Friend* f = GetOnlineFriend(nullptr, pak->player_name);
        if (f) {
            wchar_t buffer[128];
            swprintf(buffer, 128, L"<a=1>%ls</a> (%ls) entered the outpost.", f->charname, f->alias);
            GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buffer);
            return;
        }
    }
    if (instance->notify_when_players_join_outpost) {
        wchar_t buffer[128];
        swprintf(buffer, 128, L"<a=1>%ls</a> entered the outpost.", pak->player_name);
        GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buffer);
    }
}

// Allow clickable name when a player pings "I'm following X" or "I'm targeting X"
void GameSettings::OnLocalChatMessage(GW::HookStatus* status, GW::Packet::StoC::MessageLocal* pak) {
    if (pak->channel != static_cast<uint32_t>(GW::Chat::Channel::CHANNEL_GROUP) || !pak->player_number)
        return; // Not team chat or no sender
    std::wstring message(GetMessageCore());
    if (message[0] != 0x778 && message[0] != 0x781)
        return; // Not "I'm Following X" or "I'm Targeting X" message.
    size_t start_idx = message.find(L"\xba9\x107");
    if (start_idx == std::wstring::npos)
        return; // Not a player name.
    start_idx += 2;
    size_t end_idx = message.find(L"\x1", start_idx);
    if (end_idx == std::wstring::npos)
        return; // Not a player name, this should never happen.
    std::wstring player_pinged = GuiUtils::SanitizePlayerName(message.substr(start_idx, end_idx));
    if (player_pinged.empty())
        return; // No recipient
    GW::Player* sender = GW::PlayerMgr::GetPlayerByID(pak->player_number);
    if (!sender)
        return;// No sender
    auto instance = &Instance();
    if (instance->flash_window_on_name_ping && GetPlayerName() == player_pinged)
        FlashWindow(); // Flash window - we've been followed!
    // Allow clickable player name
    message.insert(start_idx, L"<a=1>");
    message.insert(end_idx + 5, L"</a>");
    PendingChatMessage* m = PendingChatMessage::queuePrint(GW::Chat::Channel::CHANNEL_GROUP, message.c_str(), sender->name_enc);
    if (m) instance->pending_messages.push_back(m);
    ::ClearMessageCore();
    status->blocked = true; // consume original packet.
}

// Open links on player name click, Ctrl + click name to target, Ctrl + Shift + click name to invite
void GameSettings::OnStartWhisper(GW::HookStatus* status, wchar_t* _name) {
    if (!_name) return;
    GameSettings* instance = &Instance();
    switch (_name[0]) {
        case 0x200B: {
            // Zero-Width Space - wiki link
            GuiUtils::SearchWiki(&_name[1]);
            status->blocked = true;
            return;
        }
        case 0x200C: {
            // Zero Width Non-Joiner - location on disk
            std::filesystem::path p(&_name[1]);
            ShellExecuteW(NULL, L"open", p.parent_path().c_str(), NULL, NULL, SW_SHOWNORMAL);
            status->blocked = true;
            return;
        }
    }
    if (instance->openlinks && (!wcsncmp(_name, L"http://", 7) || !wcsncmp(_name, L"https://", 8))) {
        ShellExecuteW(NULL, L"open", _name, NULL, NULL, SW_SHOWNORMAL);
        status->blocked = true;
        return;
    }
    if (!ImGui::GetIO().KeyCtrl)
        return; // - Next logic only applicable when Ctrl is held

    std::wstring name = GuiUtils::SanitizePlayerName(_name);
    if (ImGui::GetIO().KeyShift && GW::PartyMgr::GetPlayerIsLeader()) {
        wchar_t buf[64];
        swprintf(buf, 64, L"invite %s", name.c_str());
        GW::Chat::SendChat('/', buf);
        status->blocked = true;
        return;
    }
    GW::Player* player = GetPlayerByName(name.c_str());
    if (!ctrl_enter_whisper && player && GW::Agents::GetAgentByID(player->agent_id)) {
        GW::Agents::ChangeTarget(player->agent_id);
        status->blocked = true;
    }
    
}

// Auto accept invitations, flash window on received party invite
void GameSettings::OnPartyInviteReceived(GW::HookStatus* status, GW::Packet::StoC::PartyInviteReceived_Create* packet) {
    UNREFERENCED_PARAMETER(status);
    UNREFERENCED_PARAMETER(packet);
    if (status->blocked)
        return;
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost || !GetPlayerIsLeader())
        return;

    PartyInfo* other_party = GetPartyInfo(packet->target_party_id);
    PartyInfo* my_party = GetPartyInfo();
    if (!other_party || !my_party)
        return;
    if (my_party->GetPartySize() <= other_party->GetPartySize()) {
        // Auto accept if I'm joining a bigger party
        GW::CtoS::SendPacket(0x8, GAME_CMSG_PARTY_ACCEPT_INVITE, packet->target_party_id);
    }
    if (my_party->GetPartySize() > other_party->GetPartySize()) {
        // Auto accept join requests if I'm the bigger party
        GW::CtoS::SendPacket(0x8, GAME_CMSG_PARTY_ACCEPT_INVITE, packet->target_party_id);
    }
}

// Flash window on player added
void GameSettings::OnPartyPlayerJoined(GW::HookStatus* status, GW::Packet::StoC::PartyPlayerAdd* packet) {
    UNREFERENCED_PARAMETER(status);
    UNREFERENCED_PARAMETER(packet);
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost)
        return;
    GameSettings *instance = &Instance();
    instance->check_message_on_party_change = true;
}

// Block overhead arrow marker for zaishen scout
void GameSettings::OnAgentMarker(GW::HookStatus*, GW::Packet::StoC::GenericValue* pak) {
    const GW::Agent* a = GW::Agents::GetAgentByID(pak->agent_id);
    if (a && wcscmp(GW::Agents::GetAgentEncName(a),L"\x8102\x6ED9\xD94E\xBF68\x4409") == 0) {
        pak->Value_id = 12;
    }
}

// Block annoying tonic sounds/effects from other players
void GameSettings::OnAgentEffect(GW::HookStatus* status, GW::Packet::StoC::GenericValue* pak) {
    if (pak->agent_id != GW::Agents::GetPlayerId()) {
        switch (pak->value) {
        case 905:
            status->blocked = Instance().block_snowman_summoner;
            break;
        case 1688:
            status->blocked = Instance().block_bottle_rockets;
            break;
        case 1689:
            status->blocked = Instance().block_party_poppers;
            break;
        case 758: // Chocolate bunny
        case 2063: // e.g. Fruitcake, sugary blue drink
        case 1176: // e.g. Delicious cake
            status->blocked = Instance().block_sugar_rush_effect;
            break;
        case 1491:
            status->blocked = Instance().block_transmogrify_effect;
            break;
        default:
            break;
        }
    }

}

// Block Ghost in the box spawn animation & sound
// Block sparkly item animation
void GameSettings::OnAgentAdd(GW::HookStatus*, GW::Packet::StoC::AgentAdd* packet) {
    if (Instance().block_sparkly_drops_effect && packet->type == 4 && packet->agent_type < 0xFFFFFF) {
        GW::Item* item = GW::Items::GetItemById(packet->agent_type);
        if (item) item->interaction |= 0x2000;
    }
    if (Instance().block_ghostinthebox_effect && false
        && (packet->agent_type & 0x20000000) != 0
        && (packet->agent_type ^ 0x20000000) == GW::Constants::ModelID::Boo) {
        // Boo spawning; reset initial state to 0 from 4096 - this stops the Boo from "animating" in and making the sound
        struct InitialEffectPacket : GW::Packet::StoC::PacketBase {
            uint32_t agent_id = 0;
            uint32_t state = 0;
        } packet2;
        packet2.header = GAME_SMSG_AGENT_INITIAL_EFFECTS;
        packet2.agent_id = packet->agent_id;
        GW::StoC::EmulatePacket(&packet2);
    }
}

// Block ghost in the box death animation & sound
void GameSettings::OnUpdateAgentState(GW::HookStatus* status, GW::Packet::StoC::AgentState* packet ) {
    // @Cleanup: Not found an elegent way to do this; prematurely destroying the agent will crash the client when the id it recycled. Disable for now, here for reference.
    if (packet->state == 0x10 && false) {
        GW::AgentLiving* agent = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(packet->agent_id));
        if (agent && agent->GetIsLivingType() && agent->player_number == GW::Constants::ModelID::Boo) {
            // Boo spawning; reset initial state to 0 from 4096 - this stops the Boo from "animating" in and making the sound
            struct InitialEffectPacket : GW::Packet::StoC::PacketBase {
                uint32_t agent_id = 0;
                uint32_t state = 0x1000;
            } packet2;
            packet2.header = GAME_SMSG_AGENT_INITIAL_EFFECTS;
            packet2.agent_id = packet->agent_id;
            GW::StoC::EmulatePacket(&packet2);
            /*agent->animation_code = 0x32fc5eaf;
            agent->animation_id = 0x2;
            agent->animation_speed = 1.5f;
            agent->animation_type = 0x0;
            agent->type_map = 0x8;
            agent->model_state = 0x400;
            agent->effects = 0x10;*/
            status->blocked = true;
        }
    }
}

// Apply Collector's Edition animations on player dancing, 
void GameSettings::OnAgentLoopingAnimation(GW::HookStatus*, GW::Packet::StoC::GenericValue* pak) {
    if (pak->agent_id != GW::Agents::GetPlayerId() || !Instance().collectors_edition_emotes)
        return;
    static GW::Packet::StoC::GenericValue pak2;
    pak2.agent_id = pak->agent_id;
    pak2.Value_id = 23;
    pak2.value = pak->value; // Glowing hands, any profession
    if (pak->value == 0x43394f1d) { // 0x31939cbb = /dance, 0x43394f1d = /dancenew
        switch ((GW::Constants::Profession)GW::Agents::GetPlayerAsAgentLiving()->primary) {
        case GW::Constants::Profession::Assassin:
        case GW::Constants::Profession::Ritualist:
        case GW::Constants::Profession::Dervish:
        case GW::Constants::Profession::Paragon:
            pak2.value = 14; // Collectors edition Nightfall/Factions
            break;
        default:
            break;
        }
    }
    GW::StoC::EmulatePacket(&pak2);
}

// Skip char name entry dialog when donating faction
void GameSettings::OnFactionDonate(GW::HookStatus* status, uint32_t dialog_id) {
    if (dialog_id != 135) return;
    const int LuxonFactionNPC = GW::Constants::ModelID::Urgoz::HoppingVampire - 102;
    const int KurzickFactionNPC = LuxonFactionNPC - 229;
    auto instance = &Instance();
    if (!instance->skip_entering_name_for_faction_donate) return;
    uint32_t* current_faction = nullptr;
    uint32_t allegiance = 0;
    // Dialog 135 is also used for other NPCs e.g. zaishen keys. Use last_dialog_npc_id to compare.
    switch (last_dialog_npc_id) {
    case LuxonFactionNPC:
        current_faction = &GW::GameContext::instance()->world->current_luxon;
        allegiance = 1;
        break;
    case KurzickFactionNPC:
        current_faction = &GW::GameContext::instance()->world->current_kurzick;
        allegiance = 0;
        break;
    default:
        return;
    }
    GW::GuildContext* c = GW::GuildMgr::GetGuildContext();
    if (!c || !c->player_guild_index || c->guilds[c->player_guild_index]->faction != allegiance)
        return; // Alliance isn't the right faction. Return here and the NPC will reply.
    if (*current_faction < 5000)
        return; // Not enough to donate. Return here and the NPC will reply.
    status->blocked = true;
    GW::CtoS::SendPacket(0x10, GAME_CMSG_DEPOSIT_FACTION, 0, allegiance, 5000);
}

// Show a message when player leaves the outpost
void GameSettings::OnPlayerLeaveInstance(GW::HookStatus* status, GW::Packet::StoC::PlayerLeaveInstance* pak) {
    UNREFERENCED_PARAMETER(status);
    auto instance = &Instance();
    if (!instance->notify_when_players_leave_outpost && !instance->notify_when_friends_leave_outpost)
        return; // Dont notify about player leaving
    if (!pak->player_number || GW::Map::GetInstanceType() != GW::Constants::InstanceType::Outpost)
        return; // Only message in an outpost.
    if (pak->player_number >= GW::PlayerMgr::GetPlayerArray().size())
        return; // Not a valid player.
    wchar_t* player_name = GW::PlayerMgr::GetPlayerName(pak->player_number);
    if (!player_name)
        return; // Failed to get name
    if (instance->notify_when_friends_leave_outpost) {
        GW::Friend* f = GetOnlineFriend(nullptr, player_name);
        if (f) {
            wchar_t buffer[128];
            swprintf(buffer, 128, L"<a=1>%ls</a> (%ls) left the outpost.", f->charname, f->alias);
            GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buffer);
            return;
        }
    }
    if (instance->notify_when_players_leave_outpost) {
        wchar_t buffer[128];
        swprintf(buffer, 128, L"<a=1>%ls</a> left the outpost.", player_name);
        GW::Chat::WriteChat(GW::Chat::Channel::CHANNEL_GLOBAL, buffer);
    }
}

// Redirect NPC messages from team chat to emote chat (emulate speech bubble instead)
void GameSettings::OnNPCChatMessage(GW::HookStatus* status, GW::Packet::StoC::MessageNPC* pak) {
    auto instance = &Instance();
    if (!instance->redirect_npc_messages_to_emote_chat || !pak->sender_name)
        return; // Disabled or message pending
    const wchar_t* message = GetMessageCore();
    PendingChatMessage* m = PendingChatMessage::queuePrint(GW::Chat::Channel::CHANNEL_EMOTE, message, pak->sender_name);
    if (m) instance->pending_messages.push_back(m);
    if (pak->agent_id) {
        // Then forward the message on to speech bubble
        GW::Packet::StoC::SpeechBubble packet;
        packet.agent_id = pak->agent_id;
        wcscpy(packet.message, message);
        if (GW::Agents::GetAgentByID(packet.agent_id))
            GW::StoC::EmulatePacket(&packet);
    }
    ::ClearMessageCore();
    status->blocked = true; // consume original packet.
}

// Automatically return to outpost on defeat
void GameSettings::OnPartyDefeated(GW::HookStatus* status, GW::Packet::StoC::PartyDefeated*) {
    UNREFERENCED_PARAMETER(status);
    if (!Instance().auto_return_on_defeat || !GetPlayerIsLeader())
        return;
    GW::CtoS::SendPacket(0x4, GAME_CMSG_PARTY_RETURN_TO_OUTPOST);
}

// Automatically send /age2 on /age.
void GameSettings::OnServerMessage(GW::HookStatus* status, GW::Packet::StoC::MessageServer* pak) {
    UNREFERENCED_PARAMETER(status);
    if (!Instance().auto_age2_on_age || static_cast<GW::Chat::Channel>(pak->channel) != GW::Chat::Channel::CHANNEL_GLOBAL)
        return; // Disabled or message pending
    const wchar_t* msg = GetMessageCore();
    //0x8101 0x641F 0x86C3 0xE149 0x53E8 0x101 0x107 = You have been in this map for n minutes.
    //0x8101 0x641E 0xE7AD 0xEF64 0x1676 0x101 0x107 0x102 0x107 = You have been in this map for n hours and n minutes.
    if (wmemcmp(msg, L"\x8101\x641F\x86C3\xE149\x53E8", 5) == 0 || wmemcmp(msg, L"\x8101\x641E\xE7AD\xEF64\x1676", 5) == 0) {
        GW::Chat::SendChat('/', "age2");
    }
}

// Print NPC speech bubbles to emote chat.
void GameSettings::OnSpeechBubble(GW::HookStatus* status, GW::Packet::StoC::SpeechBubble* pak) {
    UNREFERENCED_PARAMETER(status);
    GameSettings *instance = &Instance();
    if (!instance->npc_speech_bubbles_as_chat || !pak->message || !pak->agent_id)
        return; // Disabled, invalid, or pending another speech bubble
    size_t len = 0;
    for (size_t i = 0; pak->message[i] != 0; i++)
        len = i + 1;
    if (len < 3)
        return; // Shout skill etc
    GW::AgentLiving* agent = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(pak->agent_id));
    if (!agent || agent->login_number) return; // Agent not found or Speech bubble from player e.g. drunk message.
    PendingChatMessage* m = PendingChatMessage::queuePrint(GW::Chat::Channel::CHANNEL_EMOTE, pak->message, GW::Agents::GetAgentEncName(agent));
    if (m) instance->pending_messages.push_back(m);
}

// NPC dialog messages to emote chat
void GameSettings::OnSpeechDialogue(GW::HookStatus* status, GW::Packet::StoC::DisplayDialogue* pak) {
    auto instance = &Instance();
    if (!instance->redirect_npc_messages_to_emote_chat)
        return; // Disabled or message pending
    GW::Chat::WriteChatEnc(GW::Chat::Channel::CHANNEL_EMOTE, pak->message, pak->name);
    status->blocked = true; // consume original packet.
}

// Automatic /age on vanquish
void GameSettings::OnVanquishComplete(GW::HookStatus* status, GW::Packet::StoC::VanquishComplete*) {
    UNREFERENCED_PARAMETER(status);
    if (!Instance().auto_age_on_vanquish)
        return;
    GW::Chat::SendChat('/', "age");
}

void GameSettings::OnDungeonReward(GW::HookStatus* status, GW::Packet::StoC::DungeonReward*) {
    if (Instance().hide_dungeon_chest_popup)
        status->blocked = true;
}

// Flash/focus window on trade
void GameSettings::OnTradeStarted(GW::HookStatus* status, GW::Packet::StoC::TradeStart*) {
    if (status->blocked)
        return;
    auto instance = &Instance();
    if (instance->flash_window_on_trade)
        FlashWindow();
    if (instance->focus_window_on_trade)
        FocusWindow();
}

// Stop screen shake from aftershock etc
void GameSettings::OnScreenShake(GW::HookStatus* status, void* packet) {
    UNREFERENCED_PARAMETER(packet);
    if (Instance().stop_screen_shake)
        status->blocked = true;
}

// Automatically skip cinematics, flash window on cinematic
void GameSettings::OnCinematic(GW::HookStatus* status, GW::Packet::StoC::CinematicPlay* packet) {
    UNREFERENCED_PARAMETER(status);
    GameSettings *instance = &Instance();
    if (packet->play && instance->auto_skip_cinematic) {
        GW::Map::SkipCinematic();
        return;
    }
}

// Flash/focus window on zoning
void GameSettings::OnMapTravel(GW::HookStatus* status, GW::Packet::StoC::GameSrvTransfer* pak) {
    UNREFERENCED_PARAMETER(status);
    UNREFERENCED_PARAMETER(pak);
    GW::CtoS::SendPacket(0x4, GAME_CMSG_PARTY_LEAVE_GROUP);
}

// Disable native timestamps
void GameSettings::OnCheckboxPreferenceChanged(GW::HookStatus* status, uint32_t msgid, void* wParam, void* lParam) {
    UNREFERENCED_PARAMETER(lParam);
    if (!(msgid == GW::UI::UIMessage::kCheckboxPreference && wParam))
        return;
    uint32_t pref = *(uint32_t*)wParam; // { uint32_t pref, uint32_t value } - don't care about value atm.
    if (pref == GW::UI::CheckboxPreference::CheckboxPreference_ShowChatTimestamps && Instance().show_timestamps) {
        status->blocked = true; // Always block because this UI Message will redraw all timestamps later in the call stack
        if (Instance().show_timestamps && GW::UI::GetCheckboxPreference(GW::UI::CheckboxPreference::CheckboxPreference_ShowChatTimestamps) == 1) {
            Log::Error("Disable GWToolbox timestamps to enable this setting");
            GW::UI::SetCheckboxPreference(GW::UI::CheckboxPreference::CheckboxPreference_ShowChatTimestamps, 0);
        }
    }
}

// Record current party target - this isn't always the same as the compass target.
void GameSettings::OnPartyTargetChange(GW::HookStatus* , uint32_t event_id, uint32_t type, void* wParam, void* lParam) {
    UNREFERENCED_PARAMETER(event_id);
    UNREFERENCED_PARAMETER(lParam);
    // type 0x6 == "Highlight control in frame"
    if (!(type == 0x6 && (uint32_t)wParam > 0xffffff && !lParam))
        return;
    // NB: Trade and quest log (maybe some others) get through here, but wParam is always a pointer to something.
    PartyTargetInfo* pti = (PartyTargetInfo*)wParam;
    // Test 1: Check target_type, make sure its something we're interested in
    switch (pti->target_type) {
    case 0x0:
    case 0x1:
    case 0x2:
    case 0x3:
    case 0x5:
    case 0x9:
        break;
    default:
        return;
    }
    // Test 2: Check has_target, make sure its a valid bool value
    if (pti->is_party_member != 0x1 && pti->is_party_member != 0x0)
        return;
    // Copy
    party_target_info = *pti;
}

void GameSettings::CmdReinvite(const wchar_t*, int, LPWSTR*) {
    ReinviteType& target_type = Instance().pending_reinvite_type;
    uint32_t& target_identifier = Instance().pending_reinvite_id;
    target_type = None;
    target_identifier = 0;
    GW::PartyInfo* party = GW::PartyMgr::GetPartyInfo();
    if (!party || !party->players.valid())
        return;
    GW::Player* first_player = nullptr;
    GW::Player* next_player = nullptr;
    GW::AgentLiving* me = GW::Agents::GetPlayerAsAgentLiving();
    if (!me)
        return;
    switch (party_target_info.target_type) {
    case 0x9: // Targetting player, identifer = player_number
        target_type = Player;
        target_identifier = party_target_info.target_identifier;
        break;
    case 0x2:// Targetting henchman, identifier = agent_id
        target_type = Henchman;
        target_identifier = party_target_info.target_identifier;
        break;
    case 0x3: // Targetting hero, identifier = player_number & agent_id
        target_type = Hero;
        // NOTE: Although this seems to be a valid agent_id, its not a valid agent so GetAgentByID() won't work here.
        target_identifier = (uint32_t)party_target_info.target_identifier & 0x0000ffff;
        break;
    case 0x1: // Hero in Party Search window, identifier = hero_id
    case 0x5: // Targetting pending player, identifier = player_number
    default:
        Log::ErrorW(L"Target a party member to re-invite");
        return;
    }

    // Build some references
    bool target_in_party = false;
    for (size_t i = 0; party->players.valid() && i < party->players.size(); i++) {
        auto& member = party->players[i];
        if (!member.connected())
            continue;
        if (!first_player) {
            first_player = GW::PlayerMgr::GetPlayerByID(member.login_number);
        }
        else if (!next_player) {
            next_player = GW::PlayerMgr::GetPlayerByID(member.login_number);
        }
        if (target_type == Player && member.login_number == target_identifier) {
            target_in_party = true;
        }
    }
    if (!first_player) {
        Log::ErrorW(L"Failed to find party leader");
        return;
    }
    bool leading = first_player->agent_id == me->agent_id;

    switch (target_type) {
    case Player:
        if (target_identifier == me->login_number) {
            // If I'm targeting myself, or I'm not leading; leave and re-join.
            if (!leading) {
                next_player = first_player;
            }
            GW::AgentLiving* next_player_agent = next_player ? static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(next_player->agent_id)) : nullptr;
            if (!next_player_agent || !next_player_agent->GetIsLivingType() || !next_player_agent->IsPlayer()) {
                Log::ErrorW(L"No-one is around to re-join");
                target_type = None;
                return;
            }
            target_identifier = next_player_agent->login_number;
            GW::CtoS::SendPacket(0x4, GAME_CMSG_PARTY_LEAVE_GROUP);
            return;
        }
        else if (!leading) {
            Log::ErrorW(L"Only party leader can re-invite players");
            target_type = None;
            return;
        }
        else if (target_in_party) {
            // Kick this player and re-invite
            GW::CtoS::SendPacket(0x8, GAME_CMSG_PARTY_KICK_PLAYER, target_identifier);
        }
        else {
            // We want to re-invite a player that isn't in the party already; do nothing and we'll invite the player on next frame.
        }
        break;
    case Hero:
        // Check that this is my hero
        target_type = None;
        for (size_t i = 0; target_type == None && party->heroes.valid() && i < party->heroes.size(); i++) {
            auto& member = party->heroes[i];
            if (member.agent_id == target_identifier) {
                if (member.owner_player_id != me->login_number) {
                    Log::ErrorW(L"The targetted hero doesn't belong to you");
                    target_type = None;
                    return;
                }
                target_type = Hero;
                target_identifier = member.hero_id;
                break;
            }
        }
        if (target_type == None) {
            Log::ErrorW(L"Failed to find Hero for agent_id %d", target_identifier);
            return;
        }
        // Kick this hero
        GW::CtoS::SendPacket(0x8, GAME_CMSG_HERO_KICK, target_identifier);
        break;
    case Henchman:
        if (!leading) {
            Log::ErrorW(L"Only party leader can re-invite henchmen");
            target_type = None;
            return;
        }
        // Kick this henchman
        GW::CtoS::SendPacket(0x8, GAME_CMSG_PARTY_KICK_NPC, target_identifier);
        break;
    default:
        Log::ErrorW(L"Target a party member to re-invite");
        target_type = None;
        return;
    }
}

// Turn screenshots into clickable links
void GameSettings::OnWriteChat(GW::HookStatus* status, uint32_t msgid, void* wParam, void*) {
    static bool is_redirecting = false;
    if (is_redirecting) {
        is_redirecting = false;
        return;
    }
    if (!(msgid == GW::UI::kWriteToChatLog && wParam))
        return;
    PlayerChatMessage* msg = static_cast<PlayerChatMessage*>(wParam);
    if (msg->channel != GW::Chat::Channel::CHANNEL_GLOBAL)
        return;
    if (wmemcmp(L"\x846\x107", msg->message, 2) != 0)
        return;
    status->blocked = true;
    wchar_t file_path[256];
    size_t file_path_len = 0;
    wchar_t new_message[256];
    size_t new_message_len = 0;
    wcscpy(&new_message[new_message_len], L"\x846\x107<a=1>\x200C");
    new_message_len += 8;
    for (size_t i = 2; msg->message[i] && msg->message[i] != 0x1;i++) {
        new_message[new_message_len++] = msg->message[i];
        if (msg->message[i] == '\\' && msg->message[i - 1] == '\\')
            continue; // Skip double escaped directory separators when getting the actual file name
        file_path[file_path_len++] = msg->message[i]; 
    }
    file_path[file_path_len] = 0;
    wcscpy(&new_message[new_message_len], L"</a>\x1");
    new_message_len += 5;
    new_message[new_message_len] = 0;
    is_redirecting = true;
    GW::Chat::WriteChatEnc(static_cast<GW::Chat::Channel>(msg->channel), new_message);
    // Copy file to clipboard

    int size = sizeof(DROPFILES) + ((file_path_len + 2) * sizeof(wchar_t));
    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, size);
    DROPFILES* df = (DROPFILES*)GlobalLock(hGlobal);
    ZeroMemory(df, size);
    df->pFiles = sizeof(DROPFILES);
    df->fWide = TRUE;
    LPWSTR ptr = (LPWSTR)(df + 1);
    lstrcpyW(ptr, file_path);
    GlobalUnlock(hGlobal);

    // prepare the clipboard
    OpenClipboard(NULL);
    EmptyClipboard();
    SetClipboardData(CF_HDROP, hGlobal);
    CloseClipboard();
}

// Turn /wiki into /wiki <location>
void GameSettings::OnSendChat(GW::HookStatus* , GW::Chat::Channel chan, wchar_t* msg) {
    if (!GameSettings::Instance().auto_url || !msg) return;
    size_t len = wcslen(msg);
    size_t max_len = 120;

    if (chan == GW::Chat::Channel::CHANNEL_WHISPER) {
        // msg == "Whisper Target Name,msg"
        size_t i;
        for (i = 0; i < len; i++)
            if (msg[i] == ',')
                break;

        if (i < len) {
            msg += i + 1;
            len -= i + 1;
            max_len -= i + 1;
        }
    }

    if (wcsncmp(msg, L"http://", 7) && wcsncmp(msg, L"https://", 8)) return;

    if (len + 5 < max_len) {
        for (size_t i = len; i != 0; --i)
            msg[i] = msg[i - 1];
        msg[0] = '[';
        msg[len + 1] = ';';
        msg[len + 2] = 'x';
        msg[len + 3] = 'x';
        msg[len + 4] = ']';
        msg[len + 5] = 0;
    }
}

// Auto-drop UA when recasting
void GameSettings::OnAgentStartCast(GW::HookStatus* , uint32_t msgid, void* wParam, void*) {
    if (!(msgid == 0x10000027 && wParam && Instance().drop_ua_on_cast))
        return;
    struct Casting {
        uint32_t agent_id;
        GW::Constants::SkillID skill_id;
    } *casting = (Casting*)wParam;
    if (casting->agent_id == GW::Agents::GetPlayerId() && casting->skill_id == GW::Constants::SkillID::Unyielding_Aura) {
        // Cancel UA before recast
        const GW::Buff* buff = GW::Effects::GetPlayerBuffBySkillId(casting->skill_id);
        if (buff && buff->skill_id) {
            GW::Effects::DropBuff(buff->buff_id);
        }
    }
};

// Redirect /wiki commands to go to useful pages
void GameSettings::OnOpenWiki(GW::HookStatus* status, uint32_t msgid, void* wParam, void*) {
    if (msgid != GW::UI::kOpenWikiUrl)
        return;
    std::string url = GuiUtils::ToLower((char*)wParam);
    if (strstr(url.c_str(), "/wiki/main_page")) {
        // Redirect /wiki to /wiki <current map name>
        status->blocked = true;
        GW::AreaInfo* map = GW::Map::GetCurrentMapInfo();
        /*char* buf = new char[32];
        char* map_type = 0;
        switch (map->type) {

        }
        snprintf(buf,32,"Game_link:%s_%d")*/
        Instance().pending_wiki_search_term = new GuiUtils::EncString(map->name_id);
    }
    else if (strstr(url.c_str(), "?search=quest")) {
        // Redirect /wiki quest to /wiki <current quest name>
        status->blocked = true;
        GW::WorldContext* c = GW::GameContext::instance()->world;
        uint32_t quest_id = c->active_quest_id;
        for (const GW::Quest& q : c->quest_log) {
            if (q.quest_id == quest_id) {
                Instance().pending_wiki_search_term = new GuiUtils::EncString(q.name);
                break;
            }
        }
    }
    else if (strstr(url.c_str(), "?search=target")) {
        // Redirect /wiki target to /wiki <current target name>
        status->blocked = true;
        const GW::Agent* a = GW::Agents::GetTarget();
        if (a) {
            Instance().pending_wiki_search_term = new GuiUtils::EncString(GW::Agents::GetAgentEncName(a));
        }
    }
}

// Don't target chest as nearest item, Target green items from chest last
void GameSettings::OnChangeTarget(GW::HookStatus* status, uint32_t msgid, void* wParam, void*) {
    if (!(msgid == GW::UI::kChangeTarget && wParam))
        return;
    GW::UI::ChangeTargetUIMsg* msg = (GW::UI::ChangeTargetUIMsg*)wParam;
    // Logic for re-inviting players
    if (msg->manual_target_id) {
        // Target changed
        GW::AgentLiving* agent = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(msg->manual_target_id));
        if (agent && agent->GetIsLivingType() && agent->IsPlayer()) {
            party_target_info.target_type = 0x9;
            party_target_info.target_identifier = agent->player_number;
        }
    }
    else {
        // Target cleared.
        party_target_info.target_type = 0;
        party_target_info.target_identifier = 0;
    }
    // Logic for targetting nearest item.
    if (!Instance().targeting_nearest_item)
        return;
    GW::Agent* chosen_target = static_cast<GW::AgentItem*>(GW::Agents::GetAgentByID(msg->manual_target_id));
    if (!chosen_target)
        return;
    uint32_t override_manual_agent_id = 0;
    GW::Item* target_item = nullptr;
    const GW::AgentArray agents = GW::Agents::GetAgentArray();
    if (!agents.valid())
        return;
    GW::Agent* me = GW::Agents::GetPlayer();
    if (!me)
        return;
    // If the item targeted is a green that belongs to me, and its next to the chest, try to find another item instead.
    if (chosen_target->GetIsItemType() && ((GW::AgentItem*)chosen_target)->owner == me->agent_id) {
        target_item = GW::Items::GetItemById(((GW::AgentItem*)chosen_target)->item_id);
        if (!target_item || (target_item->interaction & 0x10) == 0)
            return; // Failed to get target item, or is not green.
        for (auto* agent : agents) {
            if (!agent) continue;
            if (!agent->GetIsGadgetType()) continue;
            if (GW::GetDistance(agent->pos, chosen_target->pos) <= GW::Constants::Range::Nearby) {
                // Choose the chest as the target instead of this green item, and drop through to the next loop
                chosen_target = agent;
                override_manual_agent_id = agent->agent_id;
                break;
            }
        }
    }
    
    // If we're targeting a gadget (presume its the chest), try to find adjacent items that belong to me instead.
    if (chosen_target->GetIsGadgetType()) {
        float closest_item_dist = GW::Constants::Range::Compass;
        GW::AgentItem* agent_item = nullptr;
        for (auto* agent : agents) {
            if (!agent || !agent->GetIsItemType()) continue;
            agent_item = agent->GetAsAgentItem();
            if (!agent_item || agent_item->owner != me->agent_id) continue;
            target_item = GW::Items::GetItemById(agent_item->item_id);
            // Don't target green items.
            if (!target_item || (target_item->interaction & 0x10) != 0)
                continue;
            if (GW::GetDistance(agent->pos, chosen_target->pos) > GW::Constants::Range::Nearby)
                continue;
            float dist = GW::GetDistance(me->pos, agent->pos);
            if (dist > closest_item_dist) continue;
            override_manual_agent_id = agent->agent_id;
            closest_item_dist = dist;
        }
    }
    if (override_manual_agent_id) {
        status->blocked = true;
        GW::Agents::ChangeTarget(override_manual_agent_id);
    }
}

void GameSettings::OnCast(GW::HookStatus *, uint32_t agent_id, uint32_t slot, uint32_t target_id, uint32_t /* call_target */)
{
    if (!(target_id && agent_id == GW::Agents::GetPlayerId()))
        return;
    const GW::Skillbar* skill_bar = GW::SkillbarMgr::GetPlayerSkillbar();
    const GW::AgentLiving* me = GW::Agents::GetPlayerAsAgentLiving();
    const GW::Agent* target = GW::Agents::GetAgentByID(target_id);
    if (!skill_bar || !me || !target)
        return;
    if (me->max_energy <= 0 || me->player_number <= 0 || target->agent_id == me->agent_id)
        return;
    if (GW::GetDistance(me->pos, target->pos) > GetSkillRange(skill_bar->skills[slot].skill_id))
        pending_cast.reset(slot, target_id, 0);
}

// Set window title to player name on map load
void GameSettings::OnMapLoaded(GW::HookStatus*, GW::Packet::StoC::MapLoaded*) {
    instance_entered_at = TIMER_INIT();
    SetWindowTitle(Instance().set_window_title_as_charname);
}

// Hide player chat message speech bubbles by redirecting from 0x10000081 to 0x1000007E
void GameSettings::OnPlayerChatMessage(GW::HookStatus* status, uint32_t msg_id, void* wParam, void*) {
    if (msg_id == 0x10000081 && Instance().hide_player_speech_bubbles) {
        status->blocked = true;
        PlayerChatMessage* msg = (PlayerChatMessage*)wParam;
        GW::Player* agent = GW::PlayerMgr::GetPlayerByID(msg->player_number);
        if (!agent)
            return;
        GW::Chat::WriteChatEnc((GW::Chat::Channel)msg->channel, msg->message, agent->name_enc);
    }
}

// Hide more than 10 signets of capture
void GameSettings::OnUpdateSkillCount(GW::HookStatus*, void* packet) {
    GW::Packet::StoC::UpdateSkillCountAfterMapLoad* pak = (GW::Packet::StoC::UpdateSkillCountAfterMapLoad*)packet;
    if (Instance().limit_signets_of_capture && static_cast<GW::Constants::SkillID>(pak->skill_id) == GW::Constants::SkillID::Signet_of_Capture) {
        Instance().actual_signets_of_capture_amount = pak->count;
        if (pak->count > 10)
            pak->count = 10;
    }
}

float GameSettings::GetSkillRange(uint32_t skill_id)
{
    const auto constant_data = GW::SkillbarMgr::GetSkillConstantData(skill_id);
    using T = GW::Constants::SkillType;
    using S = GW::Constants::SkillID;
    switch (static_cast<T>(constant_data.type)) {
        case GW::Constants::SkillType::Hex:
        case GW::Constants::SkillType::Spell:
        case GW::Constants::SkillType::Enchantment:
        case GW::Constants::SkillType::Signet:
        case GW::Constants::SkillType::Condition:
        case GW::Constants::SkillType::Well:
        case GW::Constants::SkillType::Skill:
        case GW::Constants::SkillType::ItemSpell:
        case GW::Constants::SkillType::WeaponSpell:
        case GW::Constants::SkillType::EchoRefrain:
            break;
        default:
        return 0.f;
    }
    switch (static_cast<GW::Constants::SkillID>(constant_data.skill_id)) {
        case S::A_Touch_of_Guile:
        case S::Blackout:
        case S::Blood_Ritual:
        case S::Brawling_Headbutt:
        case S::Brawling_Headbutt_Brawling_skill:
        case S::Dwaynas_Touch:
        case S::Ear_Bite:
        case S::Enfeebling_Touch:
        case S::Expunge_Enchantments:
        case S::Grapple:
        case S::Headbutt:
        case S::Healing_Touch:
        case S::Hex_Eater_Signet:
        case S::Holy_Strike:
        case S::Iron_Palm:
        case S::Lift_Enchantment:
        case S::Lightning_Touch:
        case S::Low_Blow:
        case S::Mending_Touch:
        case S::Palm_Strike:
        case S::Plague_Touch:
        case S::Rending_Touch:
        case S::Renew_Life:
        case S::Restore_Life:
        case S::Shock:
        case S::Shove:
        case S::Shroud_of_Silence:
        case S::Signet_of_Midnight:
        case S::Spirit_to_Flesh:
        case S::Star_Burst:
        case S::Stonesoul_Strike:
        case S::Test_of_Faith:
        case S::Throw_Dirt:
        case S::Ursan_Rage:
        case S::Ursan_Strike:
        case S::Ursan_Strike_Blood_Washes_Blood:
        case S::Vampiric_Bite:
        case S::Vampiric_Touch:
        case S::Vile_Touch:
        case S::Volfen_Claw:
        case S::Volfen_Claw_Curse_of_the_Nornbear:
        case S::Wallows_Bite:
            return 144.f;
        case S::Augury_of_Death:
        case S::Awe:
        case S::Caltrops:
        case S::Crippling_Dagger:
        case S::Dancing_Daggers:
        case S::Disrupting_Dagger:
        case S::Healing_Whisper:
        case S::Resurrection_Chant:
        case S::Scorpion_Wire:
        case S::Seeping_Wound:
        case S::Signet_of_Judgment:
        case S::Signet_of_Judgment_PvP:
        case S::Siphon_Speed:
            return GW::Constants::Range::Spellcast / 2.f;
        default:
            return GW::Constants::Range::Spellcast;
    }
}

void GameSettings::DrawChannelColor(const char *name, GW::Chat::Channel chan) {
    ImGui::PushID(static_cast<int>(chan));
    ImGui::Text(name);
    ImGuiColorEditFlags flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoLabel;
    GW::Chat::Color color, sender_col, message_col;
    GW::Chat::GetChannelColors(chan, &sender_col, &message_col);

    ImGui::SameLine(chat_colors_grid_x[1]);
    color = sender_col;
    if (Colors::DrawSettingHueWheel("Sender Color:", &color, flags) && color != sender_col) {
        GW::Chat::SetSenderColor(chan, color);
    }

    ImGui::SameLine(chat_colors_grid_x[2]);
    color = message_col;
    if (Colors::DrawSettingHueWheel("Message Color:", &color, flags) && color != message_col) {
        GW::Chat::SetMessageColor(chan, color);
    }

    ImGui::SameLine(chat_colors_grid_x[3]);
    if (ImGui::Button("Reset")) {
        GW::Chat::Color col1, col2;
        GW::Chat::GetDefaultColors(chan, &col1, &col2);
        GW::Chat::SetSenderColor(chan, col1);
        GW::Chat::SetMessageColor(chan, col2);
    }
    ImGui::PopID();
}
