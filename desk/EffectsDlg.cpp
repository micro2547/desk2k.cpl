#include "pch.h"
#include "EffectsDlg.h"

LRESULT CEffectsDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
    m_changes = 0;
    m_initializing = TRUE;
    BOOL menuAnimation = FALSE, tooltipAnimation = FALSE;
    BOOL menuFade = FALSE, fontSmoothing = FALSE, drag = FALSE, keyboardCues = TRUE;
    BOOL animationSupported = SystemParametersInfoW(SPI_GETMENUANIMATION, 0, &menuAnimation, 0);
    animationSupported &= SystemParametersInfoW(SPI_GETTOOLTIPANIMATION, 0, &tooltipAnimation, 0);
    BOOL fadeSupported = SystemParametersInfoW(SPI_GETMENUFADE, 0, &menuFade, 0);
    BOOL fontSupported = SystemParametersInfoW(SPI_GETFONTSMOOTHING, 0, &fontSmoothing, 0);
    BOOL dragSupported = SystemParametersInfoW(SPI_GETDRAGFULLWINDOWS, 0, &drag, 0);
    BOOL keyboardSupported = SystemParametersInfoW(SPI_GETKEYBOARDCUES, 0, &keyboardCues, 0);
    Button_SetCheck(GetDlgItem(1175), menuAnimation || tooltipAnimation ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(GetDlgItem(1177), fontSmoothing ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(GetDlgItem(1179), drag ? BST_CHECKED : BST_UNCHECKED);
    Button_SetCheck(GetDlgItem(1181), keyboardCues ? BST_UNCHECKED : BST_CHECKED);
    ::EnableWindow(GetDlgItem(1175), animationSupported);
    ::EnableWindow(GetDlgItem(1177), fontSupported);
    ::EnableWindow(GetDlgItem(1179), dragSupported);
    ::EnableWindow(GetDlgItem(1181), keyboardSupported);
    WCHAR text[128];
    for (UINT id : { 2622u, 2623u })
    {
        LoadStringW(_AtlBaseModule.GetResourceInstance(), id, text, ARRAYSIZE(text));
        ComboBox_AddString(GetDlgItem(1182), text);
    }
    ComboBox_SetCurSel(GetDlgItem(1182), menuFade ? 0 : 1);
    ::EnableWindow(GetDlgItem(1182), animationSupported && fadeSupported && (menuAnimation || tooltipAnimation));

    CComPtr<IFolderView2> desktop;
    int iconSize = 32;
    BOOL desktopSupported = SUCCEEDED(GetDesktopFolderView(&desktop)) &&
        SUCCEEDED(desktop->GetViewModeAndIconSize(&m_desktopViewMode, &iconSize));
    Button_SetCheck(GetDlgItem(1180), iconSize > 32 ? BST_CHECKED : BST_UNCHECKED);
    ::EnableWindow(GetDlgItem(1180), desktopSupported);
    // Explorer on supported hosts always renders full-color icons. There is no
    // supported equivalent of the Windows 2000 Shell Icon BPP preference.
    Button_SetCheck(GetDlgItem(IDC_EFFECT_ICONCOLORS), BST_CHECKED);
    ::EnableWindow(GetDlgItem(IDC_EFFECT_ICONCOLORS), FALSE);

    m_icons.Load();
    m_images = ImageList_Create(32, 32, ILC_COLOR32 | ILC_MASK, CDesktopIconSettings::Count, 0);
    HWND list = GetDlgItem(IDC_EFFECT_ICONS);
    ListView_SetImageList(list, m_images, LVSIL_NORMAL);
    // One row, scrolling horizontally just as the Windows 2000 icon selector.
    HDC dc = ::GetDC(m_hWnd);
    int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc)
        ::ReleaseDC(m_hWnd, dc);
    ListView_SetIconSpacing(list, MulDiv(76, dpi, 96), MulDiv(64, dpi, 96));
    for (int i = 0; i < CDesktopIconSettings::Count; ++i)
    {
        HICON icon = m_icons.GetIcon(i);
        int image = ImageList_AddIcon(m_images, icon);
        if (icon)
            DestroyIcon(icon);
        LoadStringW(_AtlBaseModule.GetResourceInstance(), m_icons.GetLabel(i), text, ARRAYSIZE(text));
        LVITEMW item = {};
        item.mask = LVIF_TEXT | LVIF_IMAGE;
        item.iItem = i;
        item.pszText = text;
        item.iImage = image;
        ListView_InsertItem(list, &item);
    }
    ListView_SetItemState(list, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    UpdateIconButtons();
    m_initializing = FALSE;
    return TRUE;
}

LRESULT CEffectsDlg::OnDestroy(UINT, WPARAM, LPARAM, BOOL& handled)
{
    if (m_images)
    {
        ListView_SetImageList(GetDlgItem(IDC_EFFECT_ICONS), nullptr, LVSIL_NORMAL);
        ImageList_Destroy(m_images);
        m_images = nullptr;
    }
    handled = FALSE;
    return 0;
}

LRESULT CEffectsDlg::OnEffectChanged(WORD, WORD id, HWND, BOOL&)
{
    if (m_initializing)
        return 0;
    switch (id)
    {
    case 1175:
        m_changes |= Animation;
        ::EnableWindow(GetDlgItem(1182), Button_GetCheck(GetDlgItem(1175)) == BST_CHECKED);
        break;
    case 1182: m_changes |= Fade; break;
    case 1177: m_changes |= Font; break;
    case 1180: m_changes |= LargeIcons; break;
    case 1179: m_changes |= Drag; break;
    case 1181: m_changes |= Keyboard; break;
    }
    SetModified(TRUE);
    return 0;
}

void CEffectsDlg::UpdateIconButtons()
{
    BOOL selected = ListView_GetNextItem(GetDlgItem(IDC_EFFECT_ICONS), -1, LVNI_SELECTED) >= 0;
    ::EnableWindow(GetDlgItem(IDC_EFFECT_CHANGEICON), selected);
    ::EnableWindow(GetDlgItem(IDC_EFFECT_DEFAULTICON), selected);
}

LRESULT CEffectsDlg::OnIconSelection(int, LPNMHDR, BOOL&)
{
    UpdateIconButtons();
    return 0;
}

void CEffectsDlg::RefreshIcon(int item)
{
    HICON icon = m_icons.GetIcon(item);
    if (icon)
    {
        ImageList_ReplaceIcon(m_images, item, icon);
        DestroyIcon(icon);
        ListView_RedrawItems(GetDlgItem(IDC_EFFECT_ICONS), item, item);
    }
    SetModified(TRUE);
}

LRESULT CEffectsDlg::OnChangeIcon(WORD, WORD, HWND, BOOL&)
{
    int item = ListView_GetNextItem(GetDlgItem(IDC_EFFECT_ICONS), -1, LVNI_SELECTED);
    if (m_icons.ChooseIcon(m_hWnd, item))
        RefreshIcon(item);
    return 0;
}

LRESULT CEffectsDlg::OnDefaultIcon(WORD, WORD, HWND, BOOL&)
{
    int item = ListView_GetNextItem(GetDlgItem(IDC_EFFECT_ICONS), -1, LVNI_SELECTED);
    if (m_icons.UseDefault(item))
        RefreshIcon(item);
    return 0;
}

BOOL CEffectsDlg::ApplySystemSetting(UINT change, UINT action, UINT parameter, PVOID value)
{
    if (!(m_changes & change))
        return TRUE;
    if (!SystemParametersInfoW(action, parameter, value, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE))
        return FALSE;
    m_changes &= ~change;
    return TRUE;
}

BOOL CEffectsDlg::OnApply()
{
    BOOL success = TRUE;
    if (m_changes & Animation)
    {
        BOOL enabled = Button_GetCheck(GetDlgItem(1175)) == BST_CHECKED;
        BOOL menu = SystemParametersInfoW(SPI_SETMENUANIMATION, 0, reinterpret_cast<PVOID>(static_cast<INT_PTR>(enabled)), SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
        BOOL tooltip = SystemParametersInfoW(SPI_SETTOOLTIPANIMATION, 0, reinterpret_cast<PVOID>(static_cast<INT_PTR>(enabled)), SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
        if (menu && tooltip)
            m_changes &= ~Animation;
        else
            success = FALSE;
        // Enabling transitions also uses the effect currently displayed.
        if (enabled)
            m_changes |= Fade;
    }
    if (m_changes & Fade)
    {
        BOOL fade = ComboBox_GetCurSel(GetDlgItem(1182)) == 0;
        BOOL menu = SystemParametersInfoW(SPI_SETMENUFADE, 0, reinterpret_cast<PVOID>(static_cast<INT_PTR>(fade)), SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
        BOOL tooltip = SystemParametersInfoW(SPI_SETTOOLTIPFADE, 0, reinterpret_cast<PVOID>(static_cast<INT_PTR>(fade)), SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
        if (menu && tooltip)
            m_changes &= ~Fade;
        else
            success = FALSE;
    }
    success &= ApplySystemSetting(Font, SPI_SETFONTSMOOTHING, Button_GetCheck(GetDlgItem(1177)) == BST_CHECKED, nullptr);
    success &= ApplySystemSetting(Drag, SPI_SETDRAGFULLWINDOWS, Button_GetCheck(GetDlgItem(1179)) == BST_CHECKED, nullptr);
    success &= ApplySystemSetting(Keyboard, SPI_SETKEYBOARDCUES, 0,
        reinterpret_cast<PVOID>(static_cast<INT_PTR>(Button_GetCheck(GetDlgItem(1181)) != BST_CHECKED)));
    if (m_changes & LargeIcons)
    {
        CComPtr<IFolderView2> desktop;
        if (SUCCEEDED(GetDesktopFolderView(&desktop)) && SUCCEEDED(desktop->SetViewModeAndIconSize(
            m_desktopViewMode, Button_GetCheck(GetDlgItem(1180)) == BST_CHECKED ? 48 : 32)))
            m_changes &= ~LargeIcons;
        else
            success = FALSE;
    }
    success &= m_icons.Apply();
    if (!success)
    {
        WCHAR title[128], message[512];
        LoadStringW(_AtlBaseModule.GetResourceInstance(), IDS_EFFECT_TITLE, title, ARRAYSIZE(title));
        LoadStringW(_AtlBaseModule.GetResourceInstance(), IDS_EFFECT_APPLYERROR, message, ARRAYSIZE(message));
        ::MessageBoxW(m_hWnd, message, title, MB_OK | MB_ICONERROR);
        SetModified(TRUE);
        return PSNRET_INVALID_NOCHANGEPAGE;
    }
    SetModified(FALSE);
    return PSNRET_NOERROR;
}
