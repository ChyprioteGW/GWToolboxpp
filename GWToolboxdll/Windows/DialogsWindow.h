#pragma once

#include <Defines.h>

#include <ToolboxWindow.h>

namespace GW::Constants {
    enum class QuestID : uint32_t;
}

class DialogsWindow : public ToolboxWindow {
    DialogsWindow() = default;
    ~DialogsWindow() override = default;

public:
    static DialogsWindow& Instance()
    {
        static DialogsWindow instance;
        return instance;
    }

    [[nodiscard]] const char* Name() const override { return "Dialogs"; }
    [[nodiscard]] const char* Icon() const override { return ICON_FA_COMMENT_DOTS; }

    void Draw(IDirect3DDevice9* pDevice) override;

    void LoadSettings(ToolboxIni* ini) override;
    void SaveSettings(ToolboxIni* ini) override;
    void DrawSettingsInternal() override;

private:
    inline DWORD QuestAcceptDialog(GW::Constants::QuestID quest) { return ((int)quest << 8) | 0x800001; }
    inline DWORD QuestRewardDialog(GW::Constants::QuestID quest) { return ((int)quest << 8) | 0x800007; }

    GW::Constants::QuestID IndexToQuestID(int index);
    DWORD IndexToDialogID(int index);

    int fav_count = 0;
    std::vector<int> fav_index;

    bool show_common = true;
    bool show_uwteles = true;
    bool show_favorites = true;
    bool show_custom = true;

    char customdialogbuf[64] = "";
};
