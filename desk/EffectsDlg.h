#pragma once
#include "pch.h"
#include "DeskIcons.h"

class CEffectsDlg : public CPropertyPageImpl<CEffectsDlg>
{
public:
    enum { IDD = IDD_EFFECTSDLG };
    BOOL OnApply();

private:
    BEGIN_MSG_MAP(CEffectsDlg)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        COMMAND_HANDLER(1175, BN_CLICKED, OnEffectChanged)
        COMMAND_HANDLER(1182, CBN_SELCHANGE, OnEffectChanged)
        COMMAND_HANDLER(1177, BN_CLICKED, OnEffectChanged)
        COMMAND_HANDLER(1180, BN_CLICKED, OnEffectChanged)
        COMMAND_HANDLER(1179, BN_CLICKED, OnEffectChanged)
        COMMAND_HANDLER(1181, BN_CLICKED, OnEffectChanged)
        NOTIFY_HANDLER(IDC_EFFECT_ICONS, LVN_ITEMCHANGED, OnIconSelection)
        COMMAND_HANDLER(IDC_EFFECT_CHANGEICON, BN_CLICKED, OnChangeIcon)
        COMMAND_HANDLER(IDC_EFFECT_DEFAULTICON, BN_CLICKED, OnDefaultIcon)
        CHAIN_MSG_MAP(CPropertyPageImpl<CEffectsDlg>)
    END_MSG_MAP()

    enum Changes : UINT
    {
        Animation = 1 << 0,
        Fade = 1 << 1,
        Font = 1 << 2,
        LargeIcons = 1 << 3,
        Drag = 1 << 4,
        Keyboard = 1 << 5
    };

    LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnEffectChanged(WORD, WORD, HWND, BOOL&);
    LRESULT OnIconSelection(int, LPNMHDR, BOOL&);
    LRESULT OnChangeIcon(WORD, WORD, HWND, BOOL&);
    LRESULT OnDefaultIcon(WORD, WORD, HWND, BOOL&);
    void UpdateIconButtons();
    void RefreshIcon(int item);
    BOOL ApplySystemSetting(UINT change, UINT action, UINT parameter, PVOID value);

    UINT m_changes = 0;
    BOOL m_initializing = TRUE;
    FOLDERVIEWMODE m_desktopViewMode = FVM_ICON;
    CDesktopIconSettings m_icons;
    HIMAGELIST m_images = nullptr;
};

