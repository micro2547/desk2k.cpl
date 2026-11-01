#pragma once
#include "pch.h"

struct DesktopPattern
{
    std::wstring name;
    BYTE rows[8] = {};
};

class CBackgroundDlgProc : public CPropertyPageImpl<CBackgroundDlgProc>
{
public:
    enum { IDD = IDD_BACKGROUNDDLG };

private:
    BEGIN_MSG_MAP(CBackgroundDlgProc)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_SYSCOLORCHANGE, OnSystemColorChange)
        COMMAND_HANDLER(1205, CBN_SELCHANGE, OnPositionChange)
        COMMAND_HANDLER(1203, BN_CLICKED, OnBrowse)
        COMMAND_HANDLER(1208, BN_CLICKED, OnPattern)
        NOTIFY_HANDLER(1202, LVN_ITEMCHANGED, OnWallpaperSelection)
        CHAIN_MSG_MAP(WTL::CPropertyPageImpl<CBackgroundDlgProc>)
    END_MSG_MAP()

    BOOL OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
    BOOL OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
    BOOL OnSystemColorChange(UINT, WPARAM, LPARAM, BOOL&);
    BOOL OnPositionChange(UINT, UINT, HWND, BOOL&);
    BOOL OnBrowse(UINT, UINT, HWND, BOOL&);
    BOOL OnPattern(UINT, UINT, HWND, BOOL&);
    BOOL OnWallpaperSelection(WPARAM, LPNMHDR, BOOL&);
    BOOL OnApply();
    BOOL OnSetActive();
    int AddWallpaper(const std::wstring& path);
    void UpdatePreview();
    void MarkChanged();

    HWND m_wallpaperList = nullptr;
    bool m_initializing = true;
    bool m_wallpaperChanged = false;
    bool m_positionChanged = false;
    std::vector<std::wstring> m_wallpapers;
    std::wstring m_wallpaper;
    std::wstring m_pattern;
    std::vector<DesktopPattern> m_patterns;
    DESKTOP_WALLPAPER_POSITION m_position = DWPOS_CENTER;
    HIMAGELIST m_images = nullptr;
    HBITMAP m_preview = nullptr;
    std::unique_ptr<Gdiplus::Bitmap> m_picture;
};
