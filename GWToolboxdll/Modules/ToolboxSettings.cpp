#include "stdafx.h"

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/GamePos.h>
#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/AgentMgr.h>

#include <Defines.h>
#include <Utils/GuiUtils.h>
#include <GWToolbox.h>

#include <Modules/Updater.h>
#include <Modules/Resources.h>
#include <Modules/ChatFilter.h>
#include <Modules/ChatSettings.h>
#include <Modules/ItemFilter.h>
#include <Modules/ChatCommands.h>
#include <Modules/GameSettings.h>
#include <Modules/DiscordModule.h>
#include <Modules/TwitchModule.h>
#include <Modules/PartyWindowModule.h>
#include <Modules/ZrawDeepModule.h>
#include <Modules/AprilFools.h>
#include <Modules/InventoryManager.h>
#include <Modules/TeamspeakModule.h>
#include <Modules/ObserverModule.h>
#include <Modules/Obfuscator.h>
#include <Modules/ChatLog.h>
#include <Modules/HintsModule.h>
#include <Modules/PluginModule.h>
#if 0
#include <Modules/GWFileRequester.h>
#endif
#include <Modules/HallOfMonumentsModule.h>
#include <Modules/ToastNotifications.h>
#include <Modules/LoginModule.h>
#include <Modules/MouseFix.h>

#include <Windows/PconsWindow.h>
#include <Windows/HotkeysWindow.h>
#include <Windows/BuildsWindow.h>
#include <Windows/HeroBuildsWindow.h>
#include <Windows/TravelWindow.h>
#include <Windows/InfoWindow.h>
#include <Windows/MaterialsWindow.h>
#include <Windows/SettingsWindow.h>
#include <Windows/NotePadWindow.h>
#include <Windows/PartyStatisticsWindow.h>
#include <Windows/TradeWindow.h>
#include <Windows/ObjectiveTimerWindow.h>
#include <Windows/FactionLeaderboardWindow.h>
#include <Windows/DailyQuestsWindow.h>
#include <Windows/FriendListWindow.h>
#include <Windows/ObserverPlayerWindow.h>
#include <Windows/ObserverTargetWindow.h>
#include <Windows/ObserverPartyWindow.h>
#include <Windows/ObserverExportWindow.h>
#include <Windows/CompletionWindow.h>
#ifdef _DEBUG
#include <Windows/PacketLoggerWindow.h>
#include <Windows/DoorMonitorWindow.h>
#include <Windows/StringDecoderWindow.h>
#include <Windows/SkillListingWindow.h>
#endif
#include <Windows/RerollWindow.h>

#include <Widgets/TimerWidget.h>
#include <Widgets/HealthWidget.h>
#include <Widgets/DistanceWidget.h>
#include <Widgets/Minimap/Minimap.h>
#include <Widgets/PartyDamage.h>
#include <Widgets/BondsWidget.h>
#include <Widgets/ClockWidget.h>
#include <Widgets/VanquishWidget.h>
#include <Widgets/AlcoholWidget.h>
#include <Widgets/SkillbarWidget.h>
#include <Widgets/SkillMonitorWidget.h>
#include <Widgets/WorldMapWidget.h>
#include <Widgets/EffectsMonitorWidget.h>
#include <Widgets/LatencyWidget.h>
#include <Widgets/TreasureHunterWidget.h>
#include <Widgets/HeroesAscentWidget.h>
#include "ToolboxSettings.h"

namespace {

    ToolboxIni* inifile = nullptr;

    class ModuleToggle {
    public:
        ToolboxModule* toolbox_module;
        const char* name;
        bool enabled;
        ModuleToggle(ToolboxModule& m, bool _enabled = true) : name(m.Name()),toolbox_module(&m),enabled(_enabled) {};
    };
    class WidgetToggle {
    public:
        ToolboxWidget* toolbox_module;
        const char* name;
        bool enabled;
        WidgetToggle(ToolboxWidget& m, bool _enabled = true) : name(m.Name()),toolbox_module(&m),enabled(_enabled) {};
    };
    class WindowToggle {
    public:
        ToolboxWindow* toolbox_module;
        const char* name;
        bool enabled;
        WindowToggle(ToolboxWindow& m, bool _enabled = true) : name(m.Name()),toolbox_module(&m),enabled(_enabled) {};
    };

    const char* modules_ini_section = "Toolbox Modules";

    std::vector<ModuleToggle> optional_modules = {
        ChatFilter::Instance(),
        ItemFilter::Instance(),
        PartyWindowModule::Instance(),
        ToastNotifications::Instance(),
        DiscordModule::Instance(),
        TwitchModule::Instance(),
        TeamspeakModule::Instance(),
        ObserverModule::Instance(),
        ChatLog::Instance(),
        HintsModule::Instance(),
        MouseFix::Instance(),
        Obfuscator::Instance()
    };

    std::vector<WidgetToggle> optional_widgets = {
        TimerWidget::Instance(),
        HealthWidget::Instance(),
        SkillbarWidget::Instance(),
        DistanceWidget::Instance(),
        Minimap::Instance(),
        PartyDamage::Instance(),
        BondsWidget::Instance(),
        ClockWidget::Instance(),
        VanquishWidget::Instance(),
        AlcoholWidget::Instance(),
        WorldMapWidget::Instance(),
        EffectsMonitorWidget::Instance(),
        LatencyWidget::Instance(),
        SkillMonitorWidget::Instance()
    };

    std::vector<WindowToggle> optional_windows = {
        PconsWindow::Instance(),
        HotkeysWindow::Instance(),
        BuildsWindow::Instance(),
        HeroBuildsWindow::Instance(),
        TravelWindow::Instance(),
        InfoWindow::Instance(),
        MaterialsWindow::Instance(),
        TradeWindow::Instance(),
        NotePadWindow::Instance(),
        ObjectiveTimerWindow::Instance(),
        FactionLeaderboardWindow::Instance(),
        DailyQuests::Instance(),
        FriendListWindow::Instance(),
        ObserverPlayerWindow::Instance(),
        ObserverTargetWindow::Instance(),
        ObserverPartyWindow::Instance(),
        ObserverExportWindow::Instance(),
        
        CompletionWindow::Instance(),
        RerollWindow::Instance(),
        PartyStatisticsWindow::Instance()
    };

}

bool ToolboxSettings::move_all = false;

void ToolboxSettings::LoadModules(ToolboxIni* ini) {
    inifile = ini;

    GWToolbox::ToggleModule(Updater::Instance());
    GWToolbox::ToggleModule(ChatCommands::Instance());
    GWToolbox::ToggleModule(GameSettings::Instance());
    GWToolbox::ToggleModule(ChatSettings::Instance());
    GWToolbox::ToggleModule(InventoryManager::Instance());
    GWToolbox::ToggleModule(ZrawDeepModule::Instance());
    GWToolbox::ToggleModule(HallOfMonumentsModule::Instance());
    GWToolbox::ToggleModule(LoginModule::Instance());
    GWToolbox::ToggleModule(AprilFools::Instance());
    GWToolbox::ToggleModule(SettingsWindow::Instance());

#ifdef _DEBUG
#if 0
    GWToolbox::ToggleModule(PartySearchWindow::Instance());
    GWToolbox::ToggleModule(GWFileRequester::Instance());
#endif
    GWToolbox::ToggleModule(PacketLoggerWindow::Instance());
    GWToolbox::ToggleModule(StringDecoderWindow::Instance());
    GWToolbox::ToggleModule(DoorMonitorWindow::Instance());
    GWToolbox::ToggleModule(SkillListingWindow::Instance());
#endif
<<<<<<< HEAD
#ifdef USE_PLUGINS
    if (use_plugins) GWToolbox::ToggleModule(PluginModule::Instance());
=======
    std::sort(
        optional_modules.begin() + static_cast<int>(SettingsWindow::Instance().sep_windows),
        optional_modules.end(),
        [](const ToolboxModule* lhs, const ToolboxModule* rhs) {
            return std::string(lhs->SettingsName()).compare(rhs->SettingsName()) < 0;
        });

    SettingsWindow::Instance().sep_widgets = optional_modules.size();
    if (use_timer) optional_modules.push_back(&TimerWidget::Instance());
    if (use_health) optional_modules.push_back(&HealthWidget::Instance());
    if (use_skillbar) optional_modules.push_back(&SkillbarWidget::Instance());
    if (use_distance) optional_modules.push_back(&DistanceWidget::Instance());
    if (use_minimap) optional_modules.push_back(&Minimap::Instance());
    if (use_damage) optional_modules.push_back(&PartyDamage::Instance());
    if (use_bonds) optional_modules.push_back(&BondsWidget::Instance());
    if (use_clock) optional_modules.push_back(&ClockWidget::Instance());
    if (use_vanquish) optional_modules.push_back(&VanquishWidget::Instance());
    if (use_alcohol) optional_modules.push_back(&AlcoholWidget::Instance());
    if (use_treasure_hunter) optional_modules.push_back(&TreasureHunterWidget::Instance());
    if (use_heroes_ascent) optional_modules.push_back(&HeroesAscentWidget::Instance());
    if (use_world_map) optional_modules.push_back(&WorldMapWidget::Instance());
    if (use_effect_monitor) optional_modules.push_back(&EffectsMonitorWidget::Instance());
#if _DEBUG
    
>>>>>>> f664afeb (add treasure hunter and ha widgets)
#endif
    for (const auto& m : optional_modules) {
        GWToolbox::ToggleModule(*m.toolbox_module, m.enabled);
    }

    for (const auto& m : optional_windows) {
        GWToolbox::ToggleModule(*m.toolbox_module, m.enabled);
    }
    for (const auto& m : optional_widgets) {
        GWToolbox::ToggleModule(*m.toolbox_module, m.enabled);
    }

    
}

void ToolboxSettings::DrawSettingInternal() {
    DrawFreezeSetting();
    ImGui::Separator();

    Updater::Instance().DrawSettingInternal();
    ImGui::Separator();

    ImGui::Checkbox("Save Location Data", &save_location_data);
    ImGui::ShowHelp("Toolbox will save your location every second in a file in Settings Folder.");
    const auto cols = static_cast<size_t>(floor(ImGui::GetWindowWidth() / (170.0f * ImGui::GetIO().FontGlobalScale)));

    ImGui::Separator();
    ImGui::PushID("global_enable");
    ImGui::Text("Enable the following features:");
    ImGui::TextDisabled("Unticking will completely disable a feature from initializing and running. Requires Toolbox restart.");
<<<<<<< HEAD
=======
    static std::vector<std::pair<const char*, bool*>> features{
        {"Alcohol",&use_alcohol},
        {"Bonds",&use_bonds},
        {"Builds",&use_builds},
        {"Clock",&use_clock},
        {"Completion",&use_completion_window},
        {"Daily Quests",&use_daily_quests},
        {"Damage",&use_damage},
        {"Dialogs",&use_dialogs},
        {"Discord",&use_discord},
        {"Distance",&use_distance},
        {"Effect Monitor",&use_effect_monitor},
        {"Health",&use_health},
        {"Hotkeys",&use_hotkeys},
        {"Friend List",&use_friendlist},
        {"Hero Builds",&use_herobuilds},
        {"Info",&use_info},
        {"Materials",&use_materials},
        {"Minimap",&use_minimap},
        {"Notepad",&use_notepad},
        {"Objective Timer",&use_objectivetimer},
        {"Obfuscator",&use_obfuscator},
        {"Party Window",&use_partywindowmodule},
        {"Pcons",&use_pcons},
        {"Reroll",&use_reroll_window},
        {"Timer",&use_timer},
        {"Treasure Hunter",&use_treasure_hunter},
        {"Heroes Ascent",&use_heroes_ascent},
        {"Trade",&use_trade},
        {"Travel",&use_travel},
        {"Teamspeak",&use_teamspeak},
        {"Twitch",&use_twitch},
        {"Observer",&use_observer},
        {"Observer Player Window",&use_observer_player_window},
        {"Observer Target Window",&use_observer_target_window},
        {"Observer Party Window",&use_observer_party_window},
        {"Observer Export Window",&use_observer_export_window},
        {"Party Statistics",&use_party_statistics},
        {"Vanquish counter",&use_vanquish},
        {"World Map",&use_world_map},
    };
>>>>>>> f664afeb (add treasure hunter and ha widgets)
    ImGui::Columns(static_cast<int>(cols), "global_enable_cols", false);

    const auto optional_modules_total = optional_modules.size() + optional_widgets.size() + optional_windows.size();

    const auto items_per_col = static_cast<size_t>(ceil(optional_modules_total / static_cast<float>(cols)));
    size_t col_count = 0;
    for (auto& m : optional_modules) {
        if (ImGui::Checkbox(m.name, &m.enabled)) {
            GWToolbox::Instance().SaveSettings();
            GWToolbox::ToggleModule(*m.toolbox_module, m.enabled);
        }

        col_count++;
        if (col_count == items_per_col) {
            ImGui::NextColumn();
            col_count = 0;
        }
    }
    for (auto& m : optional_widgets) {
        if (ImGui::Checkbox(m.name, &m.enabled)) {
            GWToolbox::ToggleModule(*m.toolbox_module, m.enabled);
        }
        col_count++;
        if (col_count == items_per_col) {
            ImGui::NextColumn();
            col_count = 0;
        }
    }
    for (auto& m : optional_windows) {
        if (ImGui::Checkbox(m.name, &m.enabled)) {
            GWToolbox::ToggleModule(*m.toolbox_module, m.enabled);
        }
        col_count++;
        if (col_count == items_per_col) {
            ImGui::NextColumn();
            col_count = 0;
        }
     }
    ImGui::Columns(1);
    ImGui::PopID();
}

void ToolboxSettings::DrawFreezeSetting() {
    ImGui::Checkbox("Unlock Move All", &move_all);
    ImGui::ShowHelp("Will allow movement and resize of all widgets and windows");
}

void ToolboxSettings::LoadSettings(ToolboxIni* ini) {
    ToolboxModule::LoadSettings(ini);
    inifile = ini; // Keep this to load module info

    move_all = false;
<<<<<<< HEAD
    
    for (auto& m : optional_modules) {
        m.enabled = ini->GetBoolValue(modules_ini_section, m.name, m.enabled);
    }
    for (auto& m : optional_windows) {
        m.enabled = ini->GetBoolValue(modules_ini_section, m.name, m.enabled);
    }
    for (auto& m : optional_widgets) {
        m.enabled = ini->GetBoolValue(modules_ini_section, m.name, m.enabled);
    }
=======
    use_pcons = ini->GetBoolValue(Name(), VAR_NAME(use_pcons), use_pcons);
    use_hotkeys = ini->GetBoolValue(Name(), VAR_NAME(use_hotkeys), use_hotkeys);
    use_builds = ini->GetBoolValue(Name(), VAR_NAME(use_builds), use_builds);
    use_herobuilds = ini->GetBoolValue(Name(), VAR_NAME(use_herobuilds), use_herobuilds);
    use_travel = ini->GetBoolValue(Name(), VAR_NAME(use_travel), use_travel);
    use_dialogs = ini->GetBoolValue(Name(), VAR_NAME(use_dialogs), use_dialogs);
    use_info = ini->GetBoolValue(Name(), VAR_NAME(use_info), use_info);
    use_materials = ini->GetBoolValue(Name(), VAR_NAME(use_materials), use_materials);
    use_timer = ini->GetBoolValue(Name(), VAR_NAME(use_timer), use_timer);
    use_health = ini->GetBoolValue(Name(), VAR_NAME(use_health), use_health);
    use_distance = ini->GetBoolValue(Name(), VAR_NAME(use_distance), use_distance);
    use_minimap = ini->GetBoolValue(Name(), VAR_NAME(use_minimap), use_minimap);
    use_damage = ini->GetBoolValue(Name(), VAR_NAME(use_damage), use_damage);
    use_bonds = ini->GetBoolValue(Name(), VAR_NAME(use_bonds), use_bonds);
    use_clock = ini->GetBoolValue(Name(), VAR_NAME(use_clock), use_clock);
    use_notepad = ini->GetBoolValue(Name(), VAR_NAME(use_notepad), use_notepad);
    use_vanquish = ini->GetBoolValue(Name(), VAR_NAME(use_vanquish), use_vanquish);
    use_alcohol = ini->GetBoolValue(Name(), VAR_NAME(use_alcohol), use_alcohol);
    use_treasure_hunter = ini->GetBoolValue(Name(), VAR_NAME(use_treasure_hunter), use_treasure_hunter);
    use_heroes_ascent = ini->GetBoolValue(Name(), VAR_NAME(use_heroes_ascent), use_heroes_ascent);
    use_trade = ini->GetBoolValue(Name(), VAR_NAME(use_trade), use_trade);
    use_objectivetimer = ini->GetBoolValue(Name(), VAR_NAME(use_objectivetimer), use_objectivetimer);
    save_location_data = ini->GetBoolValue(Name(), VAR_NAME(save_location_data), save_location_data);
    use_gamesettings = ini->GetBoolValue(Name(), VAR_NAME(use_gamesettings), use_gamesettings);
    use_updater = ini->GetBoolValue(Name(), VAR_NAME(use_updater), use_updater);
    use_chatfilter = ini->GetBoolValue(Name(), VAR_NAME(use_chatfilter), use_chatfilter);
    use_chatcommand = ini->GetBoolValue(Name(), VAR_NAME(use_chatcommand), use_chatcommand);
    use_discord = ini->GetBoolValue(Name(), VAR_NAME(use_discord), use_discord);
    use_factionleaderboard = ini->GetBoolValue(Name(), VAR_NAME(use_factionleaderboard), use_factionleaderboard);
    use_teamspeak = ini->GetBoolValue(Name(), VAR_NAME(use_teamspeak), use_teamspeak);
    use_twitch = ini->GetBoolValue(Name(), VAR_NAME(use_twitch), use_twitch);
    use_observer = ini->GetBoolValue(Name(), VAR_NAME(use_observer), use_observer);
    use_observer_player_window = ini->GetBoolValue(Name(), VAR_NAME(use_observer_player_window), use_observer_player_window);
    use_observer_target_window = ini->GetBoolValue(Name(), VAR_NAME(use_observer_target_window), use_observer_target_window);
    use_observer_party_window = ini->GetBoolValue(Name(), VAR_NAME(use_observer_party_window), use_observer_party_window);
    use_observer_export_window = ini->GetBoolValue(Name(), VAR_NAME(use_observer_export_window), use_observer_export_window);
    use_partywindowmodule = ini->GetBoolValue(Name(), VAR_NAME(use_partywindowmodule), use_partywindowmodule);
    use_friendlist = ini->GetBoolValue(Name(), VAR_NAME(use_friendlist), use_friendlist);
    use_serverinfo = ini->GetBoolValue(Name(), VAR_NAME(use_serverinfo), use_serverinfo);
    use_daily_quests = ini->GetBoolValue(Name(), VAR_NAME(use_daily_quests), use_daily_quests);
    use_obfuscator = ini->GetBoolValue(Name(), VAR_NAME(use_obfuscator), use_obfuscator);
    use_completion_window = ini->GetBoolValue(Name(), VAR_NAME(use_completion_window), use_completion_window);
    use_world_map = ini->GetBoolValue(Name(), VAR_NAME(use_world_map), use_world_map);
    use_effect_monitor = ini->GetBoolValue(Name(), VAR_NAME(use_effect_monitor), use_effect_monitor);
    use_reroll_window = ini->GetBoolValue(Name(), VAR_NAME(use_reroll_window), use_reroll_window);
    use_party_statistics = ini->GetBoolValue(Name(), VAR_NAME(use_party_statistics), use_party_statistics);
>>>>>>> f664afeb (add treasure hunter and ha widgets)
}

void ToolboxSettings::SaveSettings(ToolboxIni* ini) {
    ToolboxModule::SaveSettings(ini);
    if (location_file.is_open()) location_file.close();
<<<<<<< HEAD

    for (auto& m : optional_modules) {
        ini->SetBoolValue(modules_ini_section, m.name, m.enabled);
    }
    for (auto& m : optional_windows) {
        ini->SetBoolValue(modules_ini_section, m.name, m.enabled);
    }
    for (auto& m : optional_widgets) {
        ini->SetBoolValue(modules_ini_section, m.name, m.enabled);
    }
=======
    ini->SetBoolValue(Name(), VAR_NAME(use_pcons), use_pcons);
    ini->SetBoolValue(Name(), VAR_NAME(use_hotkeys), use_hotkeys);
    ini->SetBoolValue(Name(), VAR_NAME(use_builds), use_builds);
    ini->SetBoolValue(Name(), VAR_NAME(use_herobuilds), use_herobuilds);
    ini->SetBoolValue(Name(), VAR_NAME(use_travel), use_travel);
    ini->SetBoolValue(Name(), VAR_NAME(use_dialogs), use_dialogs);
    ini->SetBoolValue(Name(), VAR_NAME(use_info), use_info);
    ini->SetBoolValue(Name(), VAR_NAME(use_materials), use_materials);
    ini->SetBoolValue(Name(), VAR_NAME(use_timer), use_timer);
    ini->SetBoolValue(Name(), VAR_NAME(use_health), use_health);
    ini->SetBoolValue(Name(), VAR_NAME(use_distance), use_distance);
    ini->SetBoolValue(Name(), VAR_NAME(use_minimap), use_minimap);
    ini->SetBoolValue(Name(), VAR_NAME(use_damage), use_damage);
    ini->SetBoolValue(Name(), VAR_NAME(use_bonds), use_bonds);
    ini->SetBoolValue(Name(), VAR_NAME(use_clock), use_clock);
    ini->SetBoolValue(Name(), VAR_NAME(use_notepad), use_notepad);
    ini->SetBoolValue(Name(), VAR_NAME(use_vanquish), use_vanquish);
    ini->SetBoolValue(Name(), VAR_NAME(use_alcohol), use_alcohol);
    ini->SetBoolValue(Name(), VAR_NAME(use_treasure_hunter), use_treasure_hunter);
    ini->SetBoolValue(Name(), VAR_NAME(use_heroes_ascent), use_heroes_ascent);
    ini->SetBoolValue(Name(), VAR_NAME(use_trade), use_trade);
    ini->SetBoolValue(Name(), VAR_NAME(use_objectivetimer), use_objectivetimer);
    ini->SetBoolValue(Name(), VAR_NAME(use_factionleaderboard), use_factionleaderboard);
    ini->SetBoolValue(Name(), VAR_NAME(use_discord), use_discord);
    ini->SetBoolValue(Name(), VAR_NAME(use_teamspeak), use_teamspeak);
    ini->SetBoolValue(Name(), VAR_NAME(use_twitch), use_twitch);
    ini->SetBoolValue(Name(), VAR_NAME(use_observer), use_observer);
    ini->SetBoolValue(Name(), VAR_NAME(use_observer_player_window), use_observer_player_window);
    ini->SetBoolValue(Name(), VAR_NAME(use_observer_target_window), use_observer_target_window);
    ini->SetBoolValue(Name(), VAR_NAME(use_observer_party_window), use_observer_party_window);
    ini->SetBoolValue(Name(), VAR_NAME(use_observer_export_window), use_observer_export_window);
    ini->SetBoolValue(Name(), VAR_NAME(use_partywindowmodule), use_partywindowmodule);
    ini->SetBoolValue(Name(), VAR_NAME(use_friendlist), use_friendlist);
    ini->SetBoolValue(Name(), VAR_NAME(use_serverinfo), use_serverinfo);
    ini->SetBoolValue(Name(), VAR_NAME(save_location_data), save_location_data);
    ini->SetBoolValue(Name(), VAR_NAME(use_gamesettings), use_gamesettings);
    ini->SetBoolValue(Name(), VAR_NAME(use_updater), use_updater);
    ini->SetBoolValue(Name(), VAR_NAME(use_chatfilter), use_chatfilter);
    ini->SetBoolValue(Name(), VAR_NAME(use_chatcommand), use_chatcommand);
    ini->SetBoolValue(Name(), VAR_NAME(use_daily_quests), use_daily_quests);
    ini->SetBoolValue(Name(), VAR_NAME(use_obfuscator), use_obfuscator);
    ini->SetBoolValue(Name(), VAR_NAME(use_completion_window), use_completion_window);
    ini->SetBoolValue(Name(), VAR_NAME(use_world_map), use_world_map);
    ini->SetBoolValue(Name(), VAR_NAME(use_effect_monitor), use_effect_monitor);
    ini->SetBoolValue(Name(), VAR_NAME(use_reroll_window), use_reroll_window);
    ini->SetBoolValue(Name(), VAR_NAME(use_party_statistics), use_party_statistics);
>>>>>>> f664afeb (add treasure hunter and ha widgets)
}

void ToolboxSettings::Draw(IDirect3DDevice9*) {
    ImGui::GetStyle().WindowBorderSize = (move_all ? 1.0f : 0.0f);
}

void ToolboxSettings::Update(float delta) {
    UNREFERENCED_PARAMETER(delta);

    // save location data
    if (save_location_data && TIMER_DIFF(location_timer) > 1000) {
        location_timer = TIMER_INIT();
        if (GW::Map::GetInstanceType() == GW::Constants::InstanceType::Explorable
            && GW::Agents::GetPlayer() != nullptr
            && GW::Map::GetInstanceTime() > 3000) {
            GW::Constants::MapID current = GW::Map::GetMapID();
            if (location_current_map != current) {
                location_current_map = current;

                std::wstring map_string;
                switch (current) {
                case GW::Constants::MapID::Domain_of_Anguish:
                    map_string = L"DoA";
                    break;
                case GW::Constants::MapID::Urgozs_Warren:
                    map_string = L"Urgoz";
                    break;
                case GW::Constants::MapID::The_Deep:
                    map_string = L"Deep";
                    break;
                case GW::Constants::MapID::The_Underworld:
                    map_string = L"UW";
                    break;
                case GW::Constants::MapID::The_Fissure_of_Woe:
                    map_string = L"FoW";
                    break;
                default:
                    map_string = std::wstring(L"Map-") + std::to_wstring(static_cast<long>(current));
                }

                std::wstring prof_string = L"";
                GW::AgentLiving* me = GW::Agents::GetCharacter();
                if (me) {
                    prof_string += L" - ";
                    prof_string += GW::Constants::GetWProfessionAcronym(
                        static_cast<GW::Constants::Profession>(me->primary));
                    prof_string += L"-";
                    prof_string += GW::Constants::GetWProfessionAcronym(
                        static_cast<GW::Constants::Profession>(me->secondary));
                }

                SYSTEMTIME localtime;
                GetLocalTime(&localtime);
                std::wstring filename = std::to_wstring(localtime.wYear)
                    + L"-" + std::to_wstring(localtime.wMonth)
                    + L"-" + std::to_wstring(localtime.wDay)
                    + L" - " + std::to_wstring(localtime.wHour)
                    + L"-" + std::to_wstring(localtime.wMinute)
                    + L"-" + std::to_wstring(localtime.wSecond)
                    + L" - " + map_string + prof_string + L".log";

                if (location_file && location_file.is_open()) {
                    location_file.close();
                }
                const std::wstring path = Resources::GetPath(L"location logs", filename);
                location_file.open(path);
            }

            GW::Agent* me = GW::Agents::GetCharacter();
            if (location_file.is_open() && me != nullptr) {
                location_file << "Time=" << GW::Map::GetInstanceTime();
                location_file << " X=" << me->pos.x;
                location_file << " Y=" << me->pos.y;
                location_file << "\n";
            }
        } else {
            location_current_map = GW::Constants::MapID::None;
            location_file.close();
        }
    }
}

