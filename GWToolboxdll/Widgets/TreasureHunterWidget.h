#pragma once

#include <ToolboxWidget.h>

class TreasureHunterWidget : public ToolboxWidget
{
    TreasureHunterWidget(){};
    ~TreasureHunterWidget(){};

private:
    DWORD chest_start = 0;
    DWORD lucky_start = 0;
    DWORD unlucky_start = 0;
    uint32_t GetTreasureHunterPoints();
    uint32_t GetLuckyPoints();
    uint32_t GetUnluckyPoints();

public:
    static TreasureHunterWidget& Instance()
    {
        static TreasureHunterWidget instance;
        return instance;
    }

    const char* Name() const override { return "Treasure Hunter"; }
    const char* Icon() const override { return ICON_FA_BARS; }

    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawSettingInternal() override;
};
