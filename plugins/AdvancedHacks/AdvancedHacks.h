#pragma once

#include <ToolboxPlugin.h>

class AdvancedHacks : public ToolboxPlugin {
public:
    const char* Name() const override { return "AdvancedHacks"; }
    const char* Icon() const override { return ICON_FA_LOCK_OPEN; }

    void Initialize(ImGuiContext*, ImGuiAllocFns, HMODULE);
    bool ParseUInt(const wchar_t* str, unsigned int* val, int base);
    void Terminate() override;
};
