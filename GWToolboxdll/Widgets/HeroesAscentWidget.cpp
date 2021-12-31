#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameContainers/Array.h>

#include <GWCA/GameEntities/Title.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/MemoryMgr.h>

#include <GuiUtils.h>
#include <Widgets/HeroesAscentWidget.h>

namespace {
    static bool IsMapReady()
    {
        return GW::Map::GetInstanceType() != GW::Constants::InstanceType::Loading
        && !GW::Map::GetIsObserving()
        && GW::MemoryMgr::GetGWWindowHandle() == GetActiveWindow();
    }
}

void HeroesAscentWidget::Initialize()
{
    ToolboxWidget::Initialize();
    starting_fame = GetHeroesAscentPoints();
}

uint32_t HeroesAscentWidget::GetHeroesAscentPoints()
{
    GW::GameContext* gameContext = GW::GameContext::instance();
    if (!gameContext || !gameContext->world || !gameContext->world->titles.valid())
        return 0;
    if (gameContext->world->titles.size() < 1) return 0;
    return gameContext->world->titles[0].current_points;
}

bool HeroesAscentWidget::IsHeroesAscentMap()
{
    if (!IsMapReady())
        return false;
    
    switch (GW::Map::GetMapID()) {
        case GW::Constants::MapID::The_Underworld_PvP:
        case GW::Constants::MapID::Fetid_River_mission:
        case GW::Constants::MapID::Burial_Mounds_mission:
        case GW::Constants::MapID::Unholy_Temples_mission:
        case GW::Constants::MapID::Forgotten_Shrines_mission:
        case GW::Constants::MapID::Golden_Gates_mission:
        case GW::Constants::MapID::The_Courtyard:
        case GW::Constants::MapID::Antechamber_mission:
        case GW::Constants::MapID::The_Vault_mission:
        case GW::Constants::MapID::The_Hall_of_Heroes_arena_mission:
        case GW::Constants::MapID::Heroes_Ascent_outpost:
            return true;
        default: return false;
    }
}

bool HeroesAscentWidget::IsCompetitiveMap()
{
    if (!IsMapReady()) return false;

    switch (GW::Map::GetMapID()) {
        case GW::Constants::MapID::The_Underworld_PvP:
        case GW::Constants::MapID::Fetid_River_mission:
        case GW::Constants::MapID::Burial_Mounds_mission:
        case GW::Constants::MapID::Unholy_Temples_mission:
        case GW::Constants::MapID::Forgotten_Shrines_mission:
        case GW::Constants::MapID::Golden_Gates_mission:
        case GW::Constants::MapID::The_Courtyard:
        case GW::Constants::MapID::Antechamber_mission:
        case GW::Constants::MapID::The_Vault_mission:
            return true;
        default: return false;
    }
}

void HeroesAscentWidget::Draw(IDirect3DDevice9 *pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible) return;
    if (!IsHeroesAscentMap()) return;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(130.0f, 80.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, true))) {
        ImVec2 cur;

        ImGui::PushFont(GetFont(GuiUtils::FontSize::header1));
        cur = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
        ImGui::TextColored(ImColor(0, 0, 0), "Fame earned");
        ImGui::SetCursorPos(cur);
        ImGui::Text("Fame earned");
        ImGui::PopFont();

        static char fame[32];
        const uint32_t current_fame = GetHeroesAscentPoints();
        snprintf(fame, 32, "%1ld", current_fame - starting_fame);
        ImGui::PushFont(GetFont(GuiUtils::FontSize::widget_small));
        cur = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
        ImGui::TextColored(ImColor(0, 0, 0), fame);
        ImGui::SetCursorPos(cur);
        ImGui::Text(fame);
        ImGui::PopFont();

//         if (IsCompetitiveMap()) {
//             ImGui::PushFont(GetFont(GuiUtils::FontSize::header1));
//             cur = ImGui::GetCursorPos();
//             ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 1));
//             ImGui::TextColored(ImColor(0, 0, 0), "Timer");
//             ImGui::SetCursorPos(cur);
//             ImGui::Text("Timer");
//             ImGui::PopFont();
// 
//             static char fame2[32];
//             uint32_t famous = GetHeroesAscentPoints();
//             snprintf(fame, 32, "%1ld", famous - starting_fame);
//             ImGui::PushFont(GetFont(GuiUtils::FontSize::widget_small));
//             cur = ImGui::GetCursorPos();
//             ImGui::SetCursorPos(ImVec2(cur.x + 3, cur.y + 2));
//             ImGui::TextColored(ImColor(0, 0, 0), fame2);
//             ImGui::SetCursorPos(cur);
//             ImGui::Text(fame2);
//             ImGui::PopFont();
//         }
    }

    ImGui::End();
    ImGui::PopStyleColor();
}

void HeroesAscentWidget::DrawSettingInternal() {
    ImGui::Text("Note: only visible in Heroes Ascent maps.");
}
