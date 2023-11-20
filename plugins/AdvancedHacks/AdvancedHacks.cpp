#include "AdvancedHacks.h"

#include <filesystem>

#include <GWCA/Constants/Constants.h>
#include <GWCA/GameContainers/Array.h>
#include <GWCA/GameEntities/Skill.h>
#include <GWCA/Managers/ChatMgr.h>
#include <GWCA/Managers/EffectMgr.h>
#include <GWCA/Managers/MapMgr.h>

#include <Defines.h>
#include <Timer.h>
#include <Utils/GuiUtils.h>
#include <GWCA/Utilities/Scanner.h>
#include <GWCA/GWCA.h>
#include <GWCA/Managers/AgentMgr.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/Managers/CtoSMgr.h>
#include <GWCA/Utilities/Hooker.h>
#include <Logger.h>
#include <Timer.h>

DLLAPI ToolboxPlugin* ToolboxPluginInstance()
{
    static AdvancedHacks instance;
    return &instance;
}

void AdvancedHacks::Initialize(ImGuiContext* ctx, ImGuiAllocFns fns, HMODULE toolbox_dll)
{
    ToolboxPlugin::Initialize(ctx, fns, toolbox_dll);
    GW::Scanner::Initialize();
    GW::Initialize();

    GW::Chat::CreateCommand(L"rawdialog", [this](const wchar_t*, int, LPWSTR* argv) {
        uint32_t id = 0;
        if (ParseUInt(argv[1], &id, 0)) {
            GW::CtoS::SendPacket(0x8, 0x0039, id);
        }
    });
    GW::Chat::CreateCommand(L"targetsame", [this](const wchar_t*, int, LPWSTR*) {
        auto same_enemy_type = [](uint32_t target1, uint32_t target2) -> bool {
            if (target1 == target2) {
                return true;
            }
            return false;
        };
        static std::vector<uint32_t> old_targets;
        static uint16_t modelid;
        // target NPC with same model id test s
        const auto agents = GW::Agents::GetAgentArray();
        if (!agents) return;
        const GW::Agent* const me = GW::Agents::GetPlayer();
        if (me == nullptr) return;

        constexpr float max_distance = GW::Constants::SqrRange::Spellcast;
        float min_distance = max_distance;
        size_t closest = static_cast<size_t>(-1);

        // Get current target living
        const GW::AgentLiving* const current_target = static_cast<GW::AgentLiving*>(GW::Agents::GetTarget());

        // If no target or target isn't enemy then return
        if (!current_target || current_target->allegiance != GW::Constants::Allegiance::Enemy) return;

        // If old_targets has entries and current target isn't same modelid then clear old_targets
        if (old_targets.size() > 0 && (!old_targets[0] || !same_enemy_type(modelid, current_target->player_number))) old_targets.clear();

        // If old_targets is empty add current_target
        if (old_targets.size() == 0) old_targets.push_back(current_target->agent_id);

        // For agents in a agentarray
        for (size_t i = 0, size = agents->size(); i < size; ++i) {
            // Get living agent

            const GW::AgentLiving* const agent = static_cast<GW::AgentLiving*>(agents->at(i));

            // If Agent is not null, or me, or not living type, or is dead, or isn't enemy, or isn't the right modelid, or agent is current target, conntinue
            if (agent == nullptr || agent == me || !agent->GetIsLivingType() || agent->GetIsDead() || agent->allegiance != GW::Constants::Allegiance::Enemy || agent == current_target) continue;
            // Check if model ids match
            if (!same_enemy_type(agent->player_number, current_target->player_number)) continue;

            // sets is_old_target as false as the default
            bool is_old_target = false;

            // for each target in old targets
            for (auto ot : old_targets)
                // if agent is already in old_targets, is_old_target = true
                if (agent->agent_id == ot) {
                    is_old_target = true;
                }
            // if is old target then continue
            if (is_old_target) continue;

            // get distance between me and agent
            const float this_distance = GW::GetSquareDistance(me->pos, agents->at(i)->pos);

            if (agent->player_number == 2334 && agent->GetIsCasting()) {
                GW::Agents::ChangeTarget(agent);
                return;
            }
            // continue if its larger than min distance
            if (this_distance > min_distance) continue;
            // set closest target and new minimum distance
            closest = i;
            min_distance = this_distance;
        }

        // If no target switch target and add it to old_targets
        if (closest != static_cast<size_t>(-1)) {
            GW::Agents::ChangeTarget(agents->at(closest));
            old_targets.push_back(agents->at(closest)->agent_id);
            modelid = agents->at(closest)->GetAsAgentLiving()->player_number;
        }
        else {
            GW::AgentLiving* const old_target = static_cast<GW::AgentLiving*>(GW::Agents::GetAgentByID(old_targets[0]));
            if (old_target && old_target->GetIsLivingType() && !old_target->GetIsDead()) {
                GW::Agents::ChangeTarget(old_target);
                old_targets.clear();
                old_targets.push_back(old_target->agent_id);
            }
            else {
                old_targets.clear();
            }
        }
    });
    GW::Chat::WriteChat(GW::Chat::CHANNEL_GWCA1, L"Initialized", L"AdvancedHacks");
}

bool AdvancedHacks::ParseUInt(const wchar_t* str, unsigned int* val, int base)
{
    wchar_t* end;
    if (!str) return false;
    *val = wcstoul(str, &end, base);
    if (str == end || errno == ERANGE)
        return false;
    else
        return true;
}

void AdvancedHacks::Terminate()
{
    ToolboxPlugin::Terminate();
    GW::Terminate();
}

