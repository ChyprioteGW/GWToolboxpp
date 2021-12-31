#pragma once

#include <ToolboxWidget.h>

class HeroesAscentWidget : public ToolboxWidget
{
    HeroesAscentWidget(){};
    ~HeroesAscentWidget(){};

private:
    DWORD starting_fame = 0;
    static uint32_t GetHeroesAscentPoints();
    static bool HeroesAscentWidget::IsHeroesAscentMap();
    static bool HeroesAscentWidget::IsCompetitiveMap();

public:
    static HeroesAscentWidget& Instance()
    {
        static HeroesAscentWidget instance;
        return instance;
    }

    const char* Name() const override { return "Heroes Ascent Helper"; }
    const char* Icon() const override { return ICON_FA_BARS; }

    void Initialize() override;
    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawSettingInternal() override;
};
