#include "pch.h"
#include "DeskIcons.h"
#include <exdisp.h>
#include <servprov.h>
#include <shlguid.h>

namespace
{
    struct DesktopIcon
    {
        LPCWSTR clsid;
        LPCWSTR value;
        UINT label;
        SHSTOCKICONID stock;
    };

    const DesktopIcon icons[CDesktopIconSettings::Count] =
    {
        { L"{20D04FE0-3AEA-1069-A2D8-08002B30309D}", L"", IDS_EFFECT_COMPUTER, SIID_DESKTOPPC },
        { L"{450D8FBA-AD25-11D0-98A8-0800361B1103}", L"", IDS_EFFECT_DOCUMENTS, SIID_DOCNOASSOC },
        { L"{F02C1A0D-BE21-4350-88B0-7367FC96EF3C}", L"", IDS_EFFECT_NETWORK, SIID_WORLD },
        { L"{645FF040-5081-101B-9F08-00AA002F954E}", L"full", IDS_EFFECT_RECYCLEFULL, SIID_RECYCLERFULL },
        { L"{645FF040-5081-101B-9F08-00AA002F954E}", L"empty", IDS_EFFECT_RECYCLEEMPTY, SIID_RECYCLER }
    };

    void IconKey(int item, BOOL perUser, WCHAR (&key)[MAX_PATH])
    {
        StringCchPrintfW(key, ARRAYSIZE(key), perUser ?
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\CLSID\\%s\\DefaultIcon" :
            L"Software\\Classes\\CLSID\\%s\\DefaultIcon", icons[item].clsid);
    }

    BOOL ReadIcon(HKEY root, LPCWSTR key, LPCWSTR value, WCHAR (&location)[MAX_PATH])
    {
        DWORD bytes = sizeof(location);
        location[0] = L'\0';
        return RegGetValueW(root, key, value, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND,
            nullptr, location, &bytes) == ERROR_SUCCESS && location[0] != L'\0';
    }

    void ExtractLocation(LPCWSTR location, WCHAR (&path)[MAX_PATH], int& index)
    {
        WCHAR expanded[MAX_PATH] = {};
        DWORD length = ExpandEnvironmentStringsW(location, expanded, ARRAYSIZE(expanded));
        StringCchCopyW(path, ARRAYSIZE(path), length && length <= ARRAYSIZE(expanded) ? expanded : location);
        index = PathParseIconLocationW(path);
    }
}

HRESULT GetDesktopFolderView(IFolderView2** view)
{
    if (!view)
        return E_POINTER;
    *view = nullptr;
    CComPtr<IShellWindows> windows;
    HRESULT hr = windows.CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_LOCAL_SERVER);
    if (FAILED(hr))
        return hr;
    CComVariant location(CSIDL_DESKTOP), empty;
    long desktopWindow = 0;
    CComPtr<IDispatch> desktop;
    hr = windows->FindWindowSW(&location, &empty, SWC_DESKTOP, &desktopWindow, SWFO_NEEDDISPATCH, &desktop);
    if (FAILED(hr) || !desktop)
        return FAILED(hr) ? hr : E_NOINTERFACE;
    CComQIPtr<IServiceProvider> services(desktop);
    if (!services)
        return E_NOINTERFACE;
    CComPtr<IShellBrowser> browser;
    hr = services->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&browser));
    if (FAILED(hr))
        return hr;
    CComPtr<IShellView> shellView;
    hr = browser->QueryActiveShellView(&shellView);
    return SUCCEEDED(hr) ? shellView->QueryInterface(IID_PPV_ARGS(view)) : hr;
}

void CDesktopIconSettings::Load()
{
    for (int i = 0; i < Count; ++i)
    {
        m_icons[i] = IconSetting{};
        WCHAR key[MAX_PATH];
        IconKey(i, FALSE, key);
        if (!ReadIcon(HKEY_LOCAL_MACHINE, key, icons[i].value, m_icons[i].defaultLocation))
        {
            SHSTOCKICONINFO stock = { sizeof(stock) };
            if (SUCCEEDED(SHGetStockIconInfo(icons[i].stock, SHGSI_ICONLOCATION, &stock)))
                StringCchPrintfW(m_icons[i].defaultLocation, ARRAYSIZE(m_icons[i].defaultLocation),
                    L"%s,%d", stock.szPath, stock.iIcon);
        }
        BOOL hasClassOverride = ReadIcon(HKEY_CURRENT_USER, key, icons[i].value, m_icons[i].location);
        WCHAR userLocation[MAX_PATH];
        IconKey(i, TRUE, key);
        if (ReadIcon(HKEY_CURRENT_USER, key, icons[i].value, userLocation))
            StringCchCopyW(m_icons[i].location, ARRAYSIZE(m_icons[i].location), userLocation);
        else if (!hasClassOverride)
            StringCchCopyW(m_icons[i].location, ARRAYSIZE(m_icons[i].location), m_icons[i].defaultLocation);
    }
}

HICON CDesktopIconSettings::GetIcon(int item) const
{
    if (item < 0 || item >= Count)
        return nullptr;
    WCHAR path[MAX_PATH];
    int index;
    ExtractLocation(m_icons[item].location, path, index);
    HICON icon = nullptr;
    ExtractIconExW(path, index, &icon, nullptr, 1);
    if (!icon)
    {
        SHSTOCKICONINFO stock = { sizeof(stock) };
        if (SUCCEEDED(SHGetStockIconInfo(icons[item].stock, SHGSI_ICON | SHGSI_LARGEICON, &stock)))
            icon = stock.hIcon;
    }
    return icon;
}

UINT CDesktopIconSettings::GetLabel(int item) const
{
    return icons[item].label;
}

BOOL CDesktopIconSettings::ChooseIcon(HWND owner, int item)
{
    if (item < 0 || item >= Count)
        return FALSE;
    WCHAR path[MAX_PATH];
    int index;
    ExtractLocation(m_icons[item].location, path, index);
    if (!PickIconDlg(owner, path, ARRAYSIZE(path), &index))
        return FALSE;
    // Reject an unextractable selection rather than persisting a broken desktop icon.
    HICON icon = nullptr;
    ExtractIconExW(path, index, &icon, nullptr, 1);
    if (!icon)
        return FALSE;
    DestroyIcon(icon);
    WCHAR location[MAX_PATH];
    HRESULT hr = StringCchPrintfW(location, ARRAYSIZE(location), L"\"%s\",%d", path, index);
    if (FAILED(hr))
        return FALSE;
    StringCchCopyW(m_icons[item].location, ARRAYSIZE(m_icons[item].location), location);
    m_icons[item].changed = TRUE;
    return TRUE;
}

BOOL CDesktopIconSettings::UseDefault(int item)
{
    if (item < 0 || item >= Count)
        return FALSE;
    StringCchCopyW(m_icons[item].location, ARRAYSIZE(m_icons[item].location), m_icons[item].defaultLocation);
    m_icons[item].changed = TRUE;
    return TRUE;
}


BOOL CDesktopIconSettings::Apply()
{
    BOOL success = TRUE;
    BOOL updated = FALSE;
    for (int i = 0; i < Count; ++i)
    {
        if (!m_icons[i].changed)
            continue;
        WCHAR keyName[MAX_PATH];
        IconKey(i, TRUE, keyName);
        CRegKey key;
        LSTATUS status = key.Create(HKEY_CURRENT_USER, keyName, nullptr, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE);
        if (status == ERROR_SUCCESS)
            status = key.SetStringValue(icons[i].value, m_icons[i].location, REG_EXPAND_SZ);
        if (status == ERROR_SUCCESS)
        {
            m_icons[i].changed = FALSE;
            updated = TRUE;
        }
        else
            success = FALSE;
    }
    if (updated)
    {
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
        PIDLIST_ABSOLUTE desktop = nullptr;
        if (SUCCEEDED(SHGetSpecialFolderLocation(nullptr, CSIDL_DESKTOP, &desktop)))
        {
            SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_IDLIST, desktop, nullptr);
            CoTaskMemFree(desktop);
        }
    }
    return success;
}
