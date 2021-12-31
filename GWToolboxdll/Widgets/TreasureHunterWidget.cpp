#include "stdafx.h"

#include <GWCA/Constants/Constants.h>

#include <GWCA/Context/GameContext.h>
#include <GWCA/Context/WorldContext.h>

#include <GWCA/GameContainers/Array.h>

#include <GWCA/GameEntities/Title.h>

#include <GWCA/Managers/MapMgr.h>
#include <GWCA/Managers/PlayerMgr.h>

#include <GuiUtils.h>
#include <Widgets/TreasureHunterWidget.h>

uint32_t TreasureHunterWidget::GetLuckyPoints()
{
    GW::GameContext* gameContext = GW::GameContext::instance();
    if (!gameContext || !gameContext->world || !gameContext->world->titles.valid())
        return 0;                                         // Sanity checks; context not ready.
    if (gameContext->world->titles.size() < 16) return 0; // No alcohol title
    return gameContext->world->titles[15].current_points;
}
uint32_t TreasureHunterWidget::GetUnluckyPoints()
{
    GW::GameContext* gameContext = GW::GameContext::instance();
    if (!gameContext || !gameContext->world || !gameContext->world->titles.valid())
        return 0;                                        // Sanity checks; context not ready.
    if (gameContext->world->titles.size() < 17)
        return 0; // No alcohol title
    return gameContext->world->titles[16].current_points;
}
uint32_t TreasureHunterWidget::GetTreasureHunterPoints()
{
    GW::GameContext* gameContext = GW::GameContext::instance();
    if (!gameContext || !gameContext->world || !gameContext->world->titles.valid())
        return 0;                                        // Sanity checks; context not ready.
    if (gameContext->world->titles.size() < 44)
        return 0; // No alcohol title
    return gameContext->world->titles[45].current_points;
}
void TreasureHunterWidget::Draw(IDirect3DDevice9 *pDevice) {
    UNREFERENCED_PARAMETER(pDevice);
    if (!visible) return;
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) return;

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::SetNextWindowSize(ImVec2(200.0f, 150.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(Name(), nullptr, GetWinFlags(0, true))) {
        ImVec2 cur;

        // treasure
        ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::header1));
        cur = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
        ImGui::TextColored(ImColor(0, 0, 0), "Treasure Hunter");
        ImGui::SetCursorPos(cur);
        ImGui::Text("Treasure Hunter");
        ImGui::PopFont();

        static char treasure[32];
        snprintf(treasure, 32, "%1ld", TreasureHunterWidget::GetTreasureHunterPoints());
        ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::widget_small));
        cur = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
        ImGui::TextColored(ImColor(0, 0, 0), treasure);
        ImGui::SetCursorPos(cur);
        ImGui::Text(treasure);
        ImGui::PopFont();

        // lucky
        ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::header1));
        cur = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cur.x + 1, cur.y + 1));
        ImGui::TextColored(ImColor(0, 0, 0), "Lucky");
        ImGui::SetCursorPos(cur);
        ImGui::Text("Lucky");
        ImGui::PopFont();

        static char lucky[32];
        snprintf(lucky, 32, "%1ld/%2ld", TreasureHunterWidget::GetLuckyPoints(), TreasureHunterWidget::GetUnluckyPoints());
        ImGui::PushFont(GuiUtils::GetFont(GuiUtils::FontSize::widget_small));
        cur = ImGui::GetCursorPos();
        ImGui::SetCursorPos(ImVec2(cur.x + 2, cur.y + 2));
        ImGui::TextColored(ImColor(0, 0, 0), lucky);
        ImGui::SetCursorPos(cur);
        ImGui::Text(lucky);
        ImGui::PopFont();
    }

    ImGui::End();
    ImGui::PopStyleColor();
}

void TreasureHunterWidget::DrawSettingInternal() {
    ImGui::Text("Note: only visible in explorable areas.");
}
