#pragma once
#include "pch.h"
#include <shlobj.h>

// Desktop namespace icon overrides are staged here until the property sheet applies.
class CDesktopIconSettings
{
public:
    static const int Count = 5;
    void Load();
    HICON GetIcon(int item) const;
    UINT GetLabel(int item) const;
    BOOL ChooseIcon(HWND owner, int item);
    BOOL UseDefault(int item);
    BOOL Apply();

private:
    struct IconSetting
    {
        WCHAR location[MAX_PATH] = {};
        WCHAR defaultLocation[MAX_PATH] = {};
        BOOL changed = FALSE;
    };
    IconSetting m_icons[Count];
};

// Returns the real Explorer desktop view, not the foreground folder's view.
HRESULT GetDesktopFolderView(IFolderView2** view);
