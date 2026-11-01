﻿/* Windows 2000 Background page. Wallpaper changes are staged; patterns are preview-only. */
#include "pch.h"
#include "BackgroundPage.h"
#include "desk.h"
#include "helper.h"
#include <cwctype>

namespace
{
    const wchar_t kPatternsKey[] = L"Control Panel\\Patterns";
    const wchar_t kDesktopKey[] = L"Control Panel\\Desktop";

    HRESULT SpiFailure()
    {
        DWORD error = GetLastError();
        return HRESULT_FROM_WIN32(error ? error : ERROR_GEN_FAILURE);
    }

    struct WallpaperSetting
    {
        std::wstring value;
        bool exists = false;
    };

    LONG ReadWallpaperSetting(HKEY key, const wchar_t* name, WallpaperSetting& setting)
    {
        DWORD bytes = 0;
        LONG status = RegGetValueW(key, nullptr, name, RRF_RT_REG_SZ, nullptr, nullptr, &bytes);
        if (status == ERROR_FILE_NOT_FOUND) return ERROR_SUCCESS;
        if (status != ERROR_SUCCESS) return status;
        std::vector<wchar_t> value(bytes / sizeof(wchar_t) + 1);
        status = RegGetValueW(key, nullptr, name, RRF_RT_REG_SZ, nullptr, value.data(), &bytes);
        if (status == ERROR_SUCCESS)
        {
            setting.value = value.data();
            setting.exists = true;
        }
        return status;
    }

    LONG WriteWallpaperSetting(HKEY key, const wchar_t* name, const wchar_t* value)
    {
        return RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value),
            static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t)));
    }

    void RestoreWallpaperSetting(HKEY key, const wchar_t* name, const WallpaperSetting& setting)
    {
        // Best-effort rollback; the original write/SPI failure remains the reported error.
        if (setting.exists) WriteWallpaperSetting(key, name, setting.value.c_str());
        else RegDeleteValueW(key, name);
    }

    HRESULT ApplyWallpaper(const wchar_t* wallpaper, DESKTOP_WALLPAPER_POSITION position, bool positionChanged)
    {
        HKEY key = nullptr;
        WallpaperSetting oldTile, oldStyle;
        if (positionChanged)
        {
            LONG status = RegCreateKeyExW(HKEY_CURRENT_USER, kDesktopKey, 0, nullptr, 0,
                KEY_QUERY_VALUE | KEY_SET_VALUE, nullptr, &key, nullptr);
            if (status != ERROR_SUCCESS) return HRESULT_FROM_WIN32(status);
            status = ReadWallpaperSetting(key, L"TileWallpaper", oldTile);
            if (status == ERROR_SUCCESS) status = ReadWallpaperSetting(key, L"WallpaperStyle", oldStyle);
            if (status != ERROR_SUCCESS)
            {
                RegCloseKey(key);
                return HRESULT_FROM_WIN32(status);
            }
            // As in the original backdlg.c, persist placement before asking USER to reload it.
            status = WriteWallpaperSetting(key, L"TileWallpaper", position == DWPOS_TILE ? L"1" : L"0");
            if (status == ERROR_SUCCESS)
                status = WriteWallpaperSetting(key, L"WallpaperStyle", position == DWPOS_STRETCH ? L"2" : L"0");
            if (status != ERROR_SUCCESS)
            {
                RestoreWallpaperSetting(key, L"TileWallpaper", oldTile);
                RestoreWallpaperSetting(key, L"WallpaperStyle", oldStyle);
                RegCloseKey(key);
                return HRESULT_FROM_WIN32(status);
            }
        }
        SetLastError(ERROR_SUCCESS);
        BOOL applied = SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0,
            const_cast<wchar_t*>(wallpaper), SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
        HRESULT result = applied ? S_OK : SpiFailure();
        if (key)
        {
            if (!applied)
            {
                RestoreWallpaperSetting(key, L"TileWallpaper", oldTile);
                RestoreWallpaperSetting(key, L"WallpaperStyle", oldStyle);
            }
            RegCloseKey(key);
        }
        return result;
    }

    std::wstring Text(UINT id)
    {
        const wchar_t* text = nullptr;
        int length = LoadStringW(_AtlBaseModule.GetResourceInstance(), id,
            reinterpret_cast<LPWSTR>(&text), 0);
        return length ? std::wstring(text, length) : std::wstring();
    }

    void Error(HWND owner, UINT id)
    {
        MessageBoxW(owner, Text(id).c_str(), Text(IDS_BACKGROUND_CAPTION).c_str(), MB_OK | MB_ICONERROR);
    }

    bool ParsePattern(const wchar_t* value, BYTE (&rows)[8])
    {
        for (int i = 0; i < 8; ++i)
        {
            while (iswspace(*value)) ++value;
            if (*value < L'0' || *value > L'9') return false;
            wchar_t* end = nullptr;
            unsigned long row = wcstoul(value, &end, 10);
            if (row > 255) return false;
            rows[i] = static_cast<BYTE>(row);
            value = end;
        }
        while (iswspace(*value)) ++value;
        return !*value;
    }

    std::wstring PatternValue(const BYTE (&rows)[8])
    {
        wchar_t value[64];
        StringCchPrintfW(value, ARRAYSIZE(value), L"%u %u %u %u %u %u %u %u",
            rows[0], rows[1], rows[2], rows[3], rows[4], rows[5], rows[6], rows[7]);
        return value;
    }

    std::wstring RegistryString(HKEY key, const wchar_t* name)
    {
        DWORD bytes = 0;
        if (RegGetValueW(key, nullptr, name, RRF_RT_REG_SZ, nullptr, nullptr, &bytes) != ERROR_SUCCESS)
            return {};
        std::vector<wchar_t> value(bytes / sizeof(wchar_t) + 1);
        if (RegGetValueW(key, nullptr, name, RRF_RT_REG_SZ, nullptr, value.data(), &bytes) != ERROR_SUCCESS)
            return {};
        return value.data();
    }

    void ReadPatterns(std::vector<DesktopPattern>& patterns)
    {
        HKEY key;
        bool catalogueExists = RegOpenKeyExW(HKEY_CURRENT_USER, kPatternsKey, 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS;
        if (catalogueExists)
        {
            DWORD maxName = 0, maxData = 0;
            RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, &maxName, &maxData, nullptr, nullptr);
            std::vector<wchar_t> name(maxName + 2), value(maxData / sizeof(wchar_t) + 2);
            for (DWORD i = 0;; ++i)
            {
                DWORD nameSize = static_cast<DWORD>(name.size()), size = static_cast<DWORD>(value.size() * sizeof(wchar_t)), type;
                LONG status = RegEnumValueW(key, i, name.data(), &nameSize, nullptr, &type,
                    reinterpret_cast<BYTE*>(value.data()), &size);
                if (status == ERROR_NO_MORE_ITEMS) break;
                if (status != ERROR_SUCCESS || type != REG_SZ) continue;
                value.back() = 0;
                DesktopPattern pattern;
                if (ParsePattern(value.data(), pattern.rows))
                {
                    pattern.name.assign(name.data(), nameSize);
                    patterns.push_back(std::move(pattern));
                }
            }
            RegCloseKey(key);
        }
        // Modern installations often have no legacy pattern catalogue.
        if (!catalogueExists)
        {
            const BYTE defaults[][8] = {
                {187, 255, 187, 187, 187, 255, 187, 187},
                {170, 85, 170, 85, 170, 85, 170, 85},
                {128, 64, 32, 16, 8, 4, 2, 1},
                {238, 221, 187, 119, 238, 221, 187, 119}
            };
            for (int i = 0; i < ARRAYSIZE(defaults); ++i)
            {
                DesktopPattern pattern;
                pattern.name = Text(IDS_BACKGROUND_BRICKS + i);
                CopyMemory(pattern.rows, defaults[i], sizeof(pattern.rows));
                patterns.push_back(std::move(pattern));
            }
        }
        std::sort(patterns.begin(), patterns.end(), [](const DesktopPattern& a, const DesktopPattern& b) {
            return StrCmpLogicalW(a.name.c_str(), b.name.c_str()) < 0;
        });
    }

    void DrawPattern(HDC dc, RECT rect, const BYTE* rows, COLORREF background, int pixel = 1)
    {
        HBRUSH bg = CreateSolidBrush(background);
        FillRect(dc, &rect, bg);
        DeleteObject(bg);
        if (!rows) return;
        HBRUSH ink = CreateSolidBrush(GetSysColor(COLOR_WINDOWTEXT));
        for (int y = rect.top; y < rect.bottom; y += pixel)
            for (int x = rect.left; x < rect.right; x += pixel)
                if (rows[((y - rect.top) / pixel) % 8] & (0x80 >> (((x - rect.left) / pixel) % 8)))
                {
                    RECT cell = { x, y, (std::min)(LONG(x + pixel), rect.right), (std::min)(LONG(y + pixel), rect.bottom) };
                    FillRect(dc, &cell, ink);
                }
        DeleteObject(ink);
    }

    class PatternEditor
    {
    public:
        std::vector<DesktopPattern>& patterns;
        int selected;
        bool changed = false;
        BYTE rows[8] = {};
        bool dirty = false;
        bool drawing = false;
        bool ink = false;
        int cursor = 0;
        HWND window = nullptr;
        std::wstring editingName;

        PatternEditor(std::vector<DesktopPattern>& values, int selection) : patterns(values), selected(selection) {}

        void Refresh()
        {
            HWND combo = GetDlgItem(window, IDC_BACKGROUND_PATTERN_NAME);
            ComboBox_ResetContent(combo);
            for (const auto& pattern : patterns) ComboBox_AddString(combo, pattern.name.c_str());
            ComboBox_SetCurSel(combo, selected);
            editingName = selected >= 0 ? patterns[selected].name : L"";
            if (selected >= 0) CopyMemory(rows, patterns[selected].rows, sizeof(rows));
            dirty = false;
            Buttons();
            InvalidateRect(window, nullptr, FALSE);
            InvalidateRect(GetDlgItem(window, IDC_BACKGROUND_PATTERN_GRID), nullptr, FALSE);
        }

        int FindName(std::wstring& name)
        {
            wchar_t value[81] = {};
            GetDlgItemTextW(window, IDC_BACKGROUND_PATTERN_NAME, value, ARRAYSIZE(value));
            name = value;
            size_t first = name.find_first_not_of(L" \t"), last = name.find_last_not_of(L" \t");
            name = first == std::wstring::npos ? L"" : name.substr(first, last - first + 1);
            for (int i = 0; i < static_cast<int>(patterns.size()); ++i)
                if (!_wcsicmp(patterns[i].name.c_str(), name.c_str())) return i;
            return -1;
        }

        void Buttons()
        {
            std::wstring name;
            int index = FindName(name);
            EnableWindow(GetDlgItem(window, IDC_BACKGROUND_PATTERN_ADD), !name.empty() && index < 0);
            EnableWindow(GetDlgItem(window, IDC_BACKGROUND_PATTERN_CHANGE), index >= 0 && dirty);
            EnableWindow(GetDlgItem(window, IDC_BACKGROUND_PATTERN_REMOVE), index >= 0);
        }

        void Save(bool add)
        {
            std::wstring name;
            int index = FindName(name);
            if (name.empty() || (add ? index >= 0 : index < 0)) return;
            DesktopPattern pattern;
            pattern.name = name;
            CopyMemory(pattern.rows, rows, sizeof(rows));
            if (add)
            {
                patterns.push_back(std::move(pattern));
                selected = static_cast<int>(patterns.size()) - 1;
            }
            else
            {
                patterns[index] = std::move(pattern);
                selected = index;
            }
            changed = true;
            Refresh();
        }

        bool SavePending()
        {
            if (!dirty) return true;
            int answer = MessageBoxW(window, Text(IDS_BACKGROUND_UNSAVED).c_str(),
                Text(IDS_BACKGROUND_EDITOR_CAPTION).c_str(), MB_YESNOCANCEL | MB_ICONQUESTION);
            if (answer == IDCANCEL) return false;
            if (answer == IDYES)
            {
                std::wstring name;
                int index = FindName(name);
                if (name.empty())
                {
                    Error(window, IDS_BACKGROUND_NAME_REQUIRED);
                    return false;
                }
                Save(index < 0);
            }
            return true;
        }

        static LRESULT CALLBACK GridProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
            UINT_PTR, DWORD_PTR data)
        {
            auto self = reinterpret_cast<PatternEditor*>(data);
            RECT rect;
            GetClientRect(hwnd, &rect);
            switch (message)
            {
            case WM_GETDLGCODE: return DLGC_WANTARROWS | DLGC_WANTCHARS;
            case WM_PAINT:
            {
                PAINTSTRUCT paint;
                HDC dc = BeginPaint(hwnd, &paint);
                for (int y = 0; y < 8; ++y)
                    for (int x = 0; x < 8; ++x)
                    {
                        RECT cell = { x * rect.right / 8, y * rect.bottom / 8,
                            (x + 1) * rect.right / 8, (y + 1) * rect.bottom / 8 };
                        FillRect(dc, &cell, GetSysColorBrush((self->rows[y] & (0x80 >> x)) ? COLOR_WINDOWTEXT : COLOR_DESKTOP));
                        FrameRect(dc, &cell, GetSysColorBrush(COLOR_3DSHADOW));
                        if (GetFocus() == hwnd && self->cursor == y * 8 + x) DrawFocusRect(dc, &cell);
                    }
                EndPaint(hwnd, &paint);
                return 0;
            }
            case WM_LBUTTONDOWN:
                SetFocus(hwnd);
                SetCapture(hwnd);
                self->drawing = true;
                break;
            case WM_MOUSEMOVE:
                if (!self->drawing) return 0;
                break;
            case WM_LBUTTONUP:
                self->drawing = false;
                ReleaseCapture();
                return 0;
            case WM_CAPTURECHANGED: self->drawing = false; return 0;
            case WM_SETFOCUS:
            case WM_KILLFOCUS: InvalidateRect(hwnd, nullptr, FALSE); return 0;
            case WM_KEYDOWN:
                switch (wParam)
                {
                case VK_LEFT: self->cursor = (self->cursor + 63) % 64; break;
                case VK_RIGHT: self->cursor = (self->cursor + 1) % 64; break;
                case VK_UP: self->cursor = (self->cursor + 56) % 64; break;
                case VK_DOWN: self->cursor = (self->cursor + 8) % 64; break;
                case VK_SPACE:
                    self->rows[self->cursor / 8] ^= 0x80 >> (self->cursor % 8);
                    self->dirty = true;
                    self->Buttons();
                    InvalidateRect(GetDlgItem(self->window, IDC_BACKGROUND_PATTERN_SAMPLE), nullptr, FALSE);
                    break;
                default: return DefSubclassProc(hwnd, message, wParam, lParam);
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            default: return DefSubclassProc(hwnd, message, wParam, lParam);
            }
            POINT point = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            if (PtInRect(&rect, point))
            {
                int x = point.x * 8 / rect.right, y = point.y * 8 / rect.bottom;
                BYTE mask = static_cast<BYTE>(0x80 >> x);
                if (message == WM_LBUTTONDOWN) self->ink = !(self->rows[y] & mask);
                if (self->ink) self->rows[y] |= mask;
                else self->rows[y] &= ~mask;
                self->cursor = y * 8 + x;
                self->dirty = true;
                self->Buttons();
                InvalidateRect(hwnd, nullptr, FALSE);
                InvalidateRect(GetDlgItem(self->window, IDC_BACKGROUND_PATTERN_SAMPLE), nullptr, FALSE);
            }
            return 0;
        }

        static INT_PTR CALLBACK Proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
        {
            auto self = reinterpret_cast<PatternEditor*>(GetWindowLongPtrW(hwnd, DWLP_USER));
            if (message == WM_INITDIALOG)
            {
                self = reinterpret_cast<PatternEditor*>(lParam);
                SetWindowLongPtrW(hwnd, DWLP_USER, lParam);
                self->window = hwnd;
                SendDlgItemMessageW(hwnd, IDC_BACKGROUND_PATTERN_NAME, CB_LIMITTEXT, 80, 0);
                SetWindowSubclass(GetDlgItem(hwnd, IDC_BACKGROUND_PATTERN_GRID), GridProc, 1, reinterpret_cast<DWORD_PTR>(self));
                self->Refresh();
                return TRUE;
            }
            if (!self) return FALSE;
            switch (message)
            {
            case WM_DRAWITEM:
            {
                auto draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
                if (draw->CtlID != IDC_BACKGROUND_PATTERN_SAMPLE) return FALSE;
                DrawPattern(draw->hDC, draw->rcItem, self->rows, GetSysColor(COLOR_DESKTOP));
                DrawEdge(draw->hDC, &draw->rcItem, EDGE_SUNKEN, BF_RECT);
                return TRUE;
            }
            case WM_CLOSE:
                if (self->SavePending()) EndDialog(hwnd, IDOK);
                return TRUE;
            case WM_COMMAND:
                switch (LOWORD(wParam))
                {
                case IDOK:
                case IDCANCEL:
                    if (self->SavePending()) EndDialog(hwnd, IDOK);
                    return TRUE;
                case IDC_BACKGROUND_PATTERN_NAME:
                    if (HIWORD(wParam) == CBN_SELCHANGE)
                    {
                        int next = ComboBox_GetCurSel(GetDlgItem(hwnd, IDC_BACKGROUND_PATTERN_NAME));
                        // Preserve the edited name while asking whether to save the old pattern.
                        if (self->dirty)
                        {
                            SetDlgItemTextW(hwnd, IDC_BACKGROUND_PATTERN_NAME, self->editingName.c_str());
                            if (!self->SavePending()) return TRUE;
                        }
                        self->selected = next;
                        self->Refresh();
                    }
                    else if (HIWORD(wParam) == CBN_EDITCHANGE)
                    {
                        self->FindName(self->editingName);
                        self->dirty = true;
                        self->Buttons();
                    }
                    return TRUE;
                case IDC_BACKGROUND_PATTERN_ADD: self->Save(true); return TRUE;
                case IDC_BACKGROUND_PATTERN_CHANGE: self->Save(false); return TRUE;
                case IDC_BACKGROUND_PATTERN_REMOVE:
                {
                    std::wstring name;
                    int index = self->FindName(name);
                    if (index >= 0 && MessageBoxW(hwnd, Text(IDS_BACKGROUND_REMOVE).c_str(),
                        Text(IDS_BACKGROUND_REMOVE_CAPTION).c_str(), MB_YESNO | MB_ICONQUESTION) == IDYES)
                    {
                        self->patterns.erase(self->patterns.begin() + index);
                        self->selected = self->patterns.empty() ? -1 : (std::min)(index, static_cast<int>(self->patterns.size()) - 1);
                        ZeroMemory(self->rows, sizeof(self->rows));
                        self->changed = true;
                        self->Refresh();
                    }
                    return TRUE;
                }
                }
                break;
            }
            return FALSE;
        }
    };

    class PatternDialog
    {
    public:
        std::vector<DesktopPattern> patterns;
        std::wstring value;
        bool catalogueChanged = false;
        HWND window = nullptr;

        void Refresh()
        {
            HWND list = GetDlgItem(window, IDC_BACKGROUND_PATTERN_LIST);
            ListBox_ResetContent(list);
            ListBox_AddString(list, Text(IDS_BACKGROUND_NONE).c_str());
            int selection = 0;
            for (int i = 0; i < static_cast<int>(patterns.size()); ++i)
            {
                ListBox_AddString(list, patterns[i].name.c_str());
                if (PatternValue(patterns[i].rows) == value) selection = i + 1;
            }
            if (!value.empty() && selection == 0)
            {
                DesktopPattern unlisted;
                unlisted.name = Text(IDS_BACKGROUND_UNLISTED);
                if (ParsePattern(value.c_str(), unlisted.rows))
                {
                    patterns.push_back(std::move(unlisted));
                    selection = static_cast<int>(patterns.size());
                    ListBox_AddString(list, patterns.back().name.c_str());
                }
            }
            ListBox_SetCurSel(list, selection);
            InvalidateRect(GetDlgItem(window, IDC_BACKGROUND_PATTERN_SAMPLE), nullptr, FALSE);
        }

        static INT_PTR CALLBACK Proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
        {
            auto self = reinterpret_cast<PatternDialog*>(GetWindowLongPtrW(hwnd, DWLP_USER));
            if (message == WM_INITDIALOG)
            {
                self = reinterpret_cast<PatternDialog*>(lParam);
                SetWindowLongPtrW(hwnd, DWLP_USER, lParam);
                self->window = hwnd;
                self->Refresh();
                return TRUE;
            }
            if (!self) return FALSE;
            if (message == WM_DRAWITEM)
            {
                auto draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
                if (draw->CtlID != IDC_BACKGROUND_PATTERN_SAMPLE) return FALSE;
                BYTE rows[8];
                DrawPattern(draw->hDC, draw->rcItem, ParsePattern(self->value.c_str(), rows) ? rows : nullptr,
                    GetSysColor(COLOR_DESKTOP));
                DrawEdge(draw->hDC, &draw->rcItem, EDGE_SUNKEN, BF_RECT);
                return TRUE;
            }
            if (message == WM_COMMAND)
            {
                switch (LOWORD(wParam))
                {
                case IDOK: EndDialog(hwnd, IDOK); return TRUE;
                case IDCANCEL: EndDialog(hwnd, IDCANCEL); return TRUE;
                case IDC_BACKGROUND_PATTERN_LIST:
                    if (HIWORD(wParam) == LBN_SELCHANGE)
                    {
                        int index = ListBox_GetCurSel(GetDlgItem(hwnd, IDC_BACKGROUND_PATTERN_LIST));
                        self->value = index > 0 ? PatternValue(self->patterns[index - 1].rows) : L"";
                        InvalidateRect(GetDlgItem(hwnd, IDC_BACKGROUND_PATTERN_SAMPLE), nullptr, FALSE);
                    }
                    return TRUE;
                case IDC_BACKGROUND_PATTERN_EDIT:
                {
                    int index = ListBox_GetCurSel(GetDlgItem(hwnd, IDC_BACKGROUND_PATTERN_LIST)) - 1;
                    PatternEditor editor(self->patterns, index);
                    INT_PTR result = DialogBoxParamW(_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCEW(IDD_BACKGROUND_PATTERN_EDITOR),
                        hwnd, PatternEditor::Proc, reinterpret_cast<LPARAM>(&editor));
                    if (result == -1) Error(hwnd, IDS_BACKGROUND_ERROR);
                    if (result == IDOK)
                    {
                        self->catalogueChanged = self->catalogueChanged || editor.changed;
                        self->value = editor.selected >= 0 ? PatternValue(self->patterns[editor.selected].rows) : L"";
                        self->Refresh();
                    }
                    return TRUE;
                }
                }
            }
            return FALSE;
        }
    };

    const DESKTOP_WALLPAPER_POSITION kPositions[] = { DWPOS_CENTER, DWPOS_TILE, DWPOS_STRETCH };
}

BOOL CBackgroundDlgProc::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
    m_wallpaperList = GetDlgItem(1202);
    RECT rect;
    ::GetClientRect(m_wallpaperList, &rect);
    LVCOLUMNW column = {};
    column.mask = LVCF_WIDTH;
    column.cx = rect.right - GetSystemMetrics(SM_CXVSCROLL) - 4;
    ListView_InsertColumn(m_wallpaperList, 0, &column);
    m_images = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 8, 8);
    ListView_SetImageList(m_wallpaperList, m_images, LVSIL_SMALL);
    AddWallpaper(L"");

    std::vector<LPWSTR> files;
    const wchar_t* extensions[] = { L".jpg", L".jpeg", L".bmp", L".dib", L".png", L".gif" };
    wchar_t windows[MAX_PATH], wallpaperDir[MAX_PATH];
    GetWindowsDirectoryW(windows, ARRAYSIZE(windows));
    StringCchPrintfW(wallpaperDir, ARRAYSIZE(wallpaperDir), L"%s\\Web\\Wallpaper", windows);
    EnumDir(wallpaperDir, extensions, ARRAYSIZE(extensions), files, TRUE);
    EnumDir(windows, extensions, ARRAYSIZE(extensions), files, FALSE);
    std::sort(files.begin(), files.end(), [](const wchar_t* a, const wchar_t* b) {
        return StrCmpLogicalW(PathFindFileNameW(a), PathFindFileNameW(b)) < 0;
    });
    for (auto path : files)
    {
        AddWallpaper(path);
        free(path);
    }
    wchar_t wallpaper[MAX_PATH] = {};
    if (!SystemParametersInfoW(SPI_GETDESKWALLPAPER, ARRAYSIZE(wallpaper), wallpaper, 0))
        Error(m_hWnd, IDS_BACKGROUND_ERROR);
    m_wallpaper = wallpaper;
    for (int i = 0; i < ARRAYSIZE(kPositions); ++i)
    {
        ComboBox_AddString(GetDlgItem(1205), Text(IDS_BACKGROUND_CENTER + i).c_str());
    }
    // Keep unsupported host modes intact until the user explicitly selects a classic mode.
    int index = AddWallpaper(m_wallpaper);
    ListView_SetItemState(m_wallpaperList, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    // Keep the explicit no-wallpaper choice visible, even when the current file is farther down.
    ListView_EnsureVisible(m_wallpaperList, 0, FALSE);
    ::EnableWindow(GetDlgItem(1205), !m_wallpaper.empty());
    ::EnableWindow(GetDlgItem(1208), TRUE);
    ReadPatterns(m_patterns);
    HKEY desktop;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kDesktopKey, 0, KEY_QUERY_VALUE, &desktop) == ERROR_SUCCESS)
    {
        const std::wstring tile = RegistryString(desktop, L"TileWallpaper");
        const std::wstring style = RegistryString(desktop, L"WallpaperStyle");
        if (tile == L"1") m_position = DWPOS_TILE;
        else if (style == L"2") m_position = DWPOS_STRETCH;
        else if (style == L"6") m_position = DWPOS_FIT;
        else if (style == L"10") m_position = DWPOS_FILL;
        else if (style == L"22") m_position = DWPOS_SPAN;
        m_pattern = RegistryString(desktop, L"Pattern");
        RegCloseKey(desktop);
        BYTE rows[8];
        if (!ParsePattern(m_pattern.c_str(), rows)) m_pattern.clear();
        else m_pattern = PatternValue(rows);
    }
    for (int i = 0; i < ARRAYSIZE(kPositions); ++i)
        if (m_position == kPositions[i]) ComboBox_SetCurSel(GetDlgItem(1205), i);
    if (!m_wallpaper.empty()) m_picture.reset(Gdiplus::Bitmap::FromFile(m_wallpaper.c_str()));
    m_initializing = false;
    UpdatePreview();
    return TRUE;
}

int CBackgroundDlgProc::AddWallpaper(const std::wstring& path)
{
    for (int i = 0; i < static_cast<int>(m_wallpapers.size()); ++i)
        if (!_wcsicmp(m_wallpapers[i].c_str(), path.c_str())) return i;
    SHFILEINFOW info = {};
    int image = -1;
    if (path.empty())
    {
        info.hIcon = static_cast<HICON>(LoadImageW(_AtlBaseModule.GetResourceInstance(),
            MAKEINTRESOURCEW(IDI_BACKGROUND_NONE), IMAGE_ICON, 16, 16, 0));
    }
    else
    {
        SHGetFileInfoW(path.c_str(), FILE_ATTRIBUTE_NORMAL, &info, sizeof(info), SHGFI_ICON | SHGFI_SMALLICON);
    }
    if (info.hIcon)
    {
        image = ImageList_AddIcon(m_images, info.hIcon);
        DestroyIcon(info.hIcon);
    }
    std::wstring label = path.empty() ? Text(IDS_BACKGROUND_NONE) : PathFindFileNameW(path.c_str());
    if (!path.empty())
    {
        size_t dot = label.find_last_of(L'.');
        if (dot != std::wstring::npos) label.erase(dot);
    }
    LVITEMW item = {};
    item.mask = LVIF_TEXT | LVIF_IMAGE;
    item.iItem = static_cast<int>(m_wallpapers.size());
    item.iImage = image;
    item.pszText = &label[0];
    int index = ListView_InsertItem(m_wallpaperList, &item);
    if (index >= 0) m_wallpapers.push_back(path);
    return index;
}

BOOL CBackgroundDlgProc::OnDestroy(UINT, WPARAM, LPARAM, BOOL&)
{
    Static_SetBitmap(GetDlgItem(1200), nullptr);
    if (m_preview) DeleteObject(m_preview);
    if (m_images) ImageList_Destroy(m_images);
    m_preview = nullptr;
    m_images = nullptr;
    return 0;
}

void CBackgroundDlgProc::MarkChanged()
{
    UpdatePreview();
    SetModified(TRUE);
}

BOOL CBackgroundDlgProc::OnPositionChange(UINT, UINT, HWND, BOOL&)
{
    int selection = ComboBox_GetCurSel(GetDlgItem(1205));
    if (selection >= 0 && selection < ARRAYSIZE(kPositions))
    {
        m_position = kPositions[selection];
        m_positionChanged = true;
        MarkChanged();
    }
    return 0;
}

BOOL CBackgroundDlgProc::OnBrowse(UINT, UINT, HWND, BOOL&)
{
    Microsoft::WRL::ComPtr<IFileOpenDialog> dialog;
    HRESULT result = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
    std::wstring path;
    if (SUCCEEDED(result))
    {
        std::wstring filter = Text(IDS_BACKGROUND_PICTURE_FILTER);
        COMDLG_FILTERSPEC types[] = { { filter.c_str(), L"*.bmp;*.dib;*.gif;*.jpg;*.jpeg;*.png" } };
        DWORD options;
        result = dialog->GetOptions(&options);
        if (SUCCEEDED(result)) result = dialog->SetOptions(options | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM);
        if (SUCCEEDED(result)) result = dialog->SetFileTypes(ARRAYSIZE(types), types);
        if (SUCCEEDED(result)) result = dialog->SetTitle(Text(IDS_BACKGROUND_BROWSE_TITLE).c_str());
        if (SUCCEEDED(result)) result = dialog->Show(m_hWnd);
        Microsoft::WRL::ComPtr<IShellItem> item;
        if (SUCCEEDED(result)) result = dialog->GetResult(&item);
        PWSTR value = nullptr;
        if (SUCCEEDED(result)) result = item->GetDisplayName(SIGDN_FILESYSPATH, &value);
        if (SUCCEEDED(result)) path = value;
        CoTaskMemFree(value);
    }
    else
    {
        wchar_t file[32768] = {};
        std::wstring filter = Text(IDS_BACKGROUND_PICTURE_FILTER);
        filter.push_back(0);
        filter += L"*.bmp;*.dib;*.gif;*.jpg;*.jpeg;*.png";
        filter.push_back(0);
        filter.push_back(0);
        std::wstring title = Text(IDS_BACKGROUND_BROWSE_TITLE);
        OPENFILENAMEW open = { sizeof(open) };
        open.hwndOwner = m_hWnd;
        open.lpstrFile = file;
        open.nMaxFile = ARRAYSIZE(file);
        open.lpstrFilter = filter.c_str();
        open.lpstrTitle = title.c_str();
        open.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
        if (GetOpenFileNameW(&open)) path = file;
        else if (CommDlgExtendedError()) Error(m_hWnd, IDS_BACKGROUND_ERROR);
        result = S_OK;
    }
    if (FAILED(result) && result != HRESULT_FROM_WIN32(ERROR_CANCELLED)) Error(m_hWnd, IDS_BACKGROUND_ERROR);
    if (!path.empty())
    {
        int index = AddWallpaper(path);
        if (index >= 0)
        {
            ListView_SetItemState(m_wallpaperList, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            ListView_EnsureVisible(m_wallpaperList, index, FALSE);
        }
    }
    return 0;
}

BOOL CBackgroundDlgProc::OnWallpaperSelection(WPARAM, LPNMHDR header, BOOL&)
{
    const auto change = reinterpret_cast<NMLISTVIEW*>(header);
    if (m_initializing || !(change->uChanged & LVIF_STATE) || !(change->uNewState & LVIS_SELECTED) ||
        change->iItem < 0 || change->iItem >= static_cast<int>(m_wallpapers.size())) return 0;
    const std::wstring& path = m_wallpapers[change->iItem];
    std::unique_ptr<Gdiplus::Bitmap> picture;
    if (!path.empty())
    {
        picture.reset(Gdiplus::Bitmap::FromFile(path.c_str()));
        if (!picture || picture->GetLastStatus() != Gdiplus::Ok || !picture->GetWidth() || !picture->GetHeight())
        {
            Error(m_hWnd, IDS_BACKGROUND_BAD_PICTURE);
            m_initializing = true;
            int previous = AddWallpaper(m_wallpaper);
            ListView_SetItemState(m_wallpaperList, change->iItem, 0, LVIS_SELECTED);
            if (previous >= 0) ListView_SetItemState(m_wallpaperList, previous, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            m_initializing = false;
            return 0;
        }
    }
    m_picture = std::move(picture);
    m_wallpaper = path;
    m_wallpaperChanged = true;
    ::EnableWindow(GetDlgItem(1205), !path.empty());
    ::EnableWindow(GetDlgItem(1208), TRUE);
    MarkChanged();
    return 0;
}

BOOL CBackgroundDlgProc::OnPattern(UINT, UINT, HWND, BOOL&)
{
    PatternDialog dialog;
    dialog.patterns = m_patterns;
    dialog.value = m_pattern;
    INT_PTR result = DialogBoxParamW(_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCEW(IDD_BACKGROUND_PATTERNS),
        m_hWnd, PatternDialog::Proc, reinterpret_cast<LPARAM>(&dialog));
    if (result == -1) Error(m_hWnd, IDS_BACKGROUND_ERROR);
    if (result == IDOK)
    {
        const bool changed = m_pattern != dialog.value || dialog.catalogueChanged;
        m_pattern = std::move(dialog.value);
        m_patterns = std::move(dialog.patterns);
        if (changed) MarkChanged();
    }
    return 0;
}

BOOL CBackgroundDlgProc::OnApply()
{
    if (!m_wallpaperChanged && !m_positionChanged)
    {
        SetModified(FALSE);
        return PSNRET_NOERROR;
    }
    HRESULT result = S_OK;
    if (m_wallpaperChanged || m_positionChanged)
    {
        wchar_t current[MAX_PATH] = {};
        const wchar_t* wallpaper = m_wallpaper.c_str();
        if (!m_wallpaperChanged)
        {
            SetLastError(ERROR_SUCCESS);
            if (!SystemParametersInfoW(SPI_GETDESKWALLPAPER, ARRAYSIZE(current), current, 0))
                result = SpiFailure();
            else wallpaper = current;
        }
        // An unchanged Fit/Fill/Span mode is never rewritten as a classic mode.
        if (SUCCEEDED(result)) result = ApplyWallpaper(wallpaper, m_position, m_positionChanged);
    }
    if (FAILED(result))
    {
        Error(m_hWnd, IDS_BACKGROUND_APPLY_ERROR);
        return PSNRET_INVALID_NOCHANGEPAGE;
    }
    if (selectedTheme)
    {
        if (m_wallpaperChanged)
        {
            selectedTheme->wallpaperPath = m_wallpaper;
            selectedTheme->wallpaperType = m_wallpaper.empty() ? WT_NOWALL : WT_PICTURE;
            selectedTheme->customWallpaperSelection = false;
        }
        if (m_positionChanged) selectedTheme->posChanged = -1;
        selectedTheme->updateWallThemesPg = true;
    }
    m_wallpaperChanged = m_positionChanged = false;
    SetModified(FALSE);
    return PSNRET_NOERROR;
}

BOOL CBackgroundDlgProc::OnSetActive()
{
    _TerminateProcess(pi);
    UpdatePreview();
    return 0;
}

BOOL CBackgroundDlgProc::OnSystemColorChange(UINT, WPARAM, LPARAM, BOOL&)
{
    UpdatePreview();
    return 0;
}

void CBackgroundDlgProc::UpdatePreview()
{
    HWND preview = GetDlgItem(1200);
    RECT rect;
    if (!::GetClientRect(preview, &rect) || rect.right <= 0 || rect.bottom <= 0) return;
    HDC screen = ::GetDC(preview);
    if (!screen) return;
    HDC dc = CreateCompatibleDC(screen);
    // A 32-bit screen-compatible bitmap mixes GDI's zero alpha with GDI+'s
    // opaque alpha. SS_BITMAP then blends the GDI-filled desktop away.
    // Render into RGB24 so the completed monitor is always wholly opaque.
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = rect.right;
    info.bmiHeader.biHeight = -rect.bottom;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 24;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!screen || !dc || !bitmap)
    {
        if (bitmap) DeleteObject(bitmap);
        if (dc) DeleteDC(dc);
        if (screen) ::ReleaseDC(preview, screen);
        return;
    }
    HGDIOBJ old = SelectObject(dc, bitmap);
    FillRect(dc, &rect, GetSysColorBrush(COLOR_3DFACE));
    GdiFlush();
    const int width = rect.right, height = rect.bottom;
    // Scale the existing classic monitor artwork with the dialog, including high DPI.
    Gdiplus::Bitmap monitor(g_hinst, MAKEINTRESOURCEW(IDB_BITMAP1));
    int sourceWidth = static_cast<int>(monitor.GetWidth()), sourceHeight = static_cast<int>(monitor.GetHeight());
    if (monitor.GetLastStatus() != Gdiplus::Ok || sourceWidth <= 0 || sourceHeight <= 0)
    {
        SelectObject(dc, old);
        DeleteObject(bitmap);
        DeleteDC(dc);
        ::ReleaseDC(preview, screen);
        return;
    }
    int monitorWidth = width, monitorHeight = height;
    if (monitor.GetLastStatus() == Gdiplus::Ok)
    {
        double scale = (std::min)(double(width) / monitor.GetWidth(), double(height) / monitor.GetHeight());
        monitorWidth = static_cast<int>(monitor.GetWidth() * scale);
        monitorHeight = static_cast<int>(monitor.GetHeight() * scale);
        Gdiplus::Graphics graphics(dc);
        Gdiplus::ImageAttributes attributes;
        attributes.SetColorKey(Gdiplus::Color(255, 255, 255, 0), Gdiplus::Color(255, 255, 255, 0));
        graphics.DrawImage(&monitor, Gdiplus::Rect((width - monitorWidth) / 2, (height - monitorHeight) / 2,
            monitorWidth, monitorHeight), 0, 0, monitor.GetWidth(), monitor.GetHeight(), Gdiplus::UnitPixel, &attributes);
        graphics.Flush(Gdiplus::FlushIntentionSync);
    }
    int left = (width - monitorWidth) / 2, top = (height - monitorHeight) / 2;
    RECT desktop = { left + MulDiv(16, monitorWidth, sourceWidth),
        top + MulDiv(17, monitorHeight, sourceHeight),
        left + MulDiv(168, monitorWidth, sourceWidth),
        top + MulDiv(129, monitorHeight, sourceHeight) };
    BYTE rows[8];
    DrawPattern(dc, desktop, ParsePattern(m_pattern.c_str(), rows) ? rows : nullptr, GetDeskopColor());
    GdiFlush();
    if (m_picture && m_picture->GetLastStatus() == Gdiplus::Ok && m_picture->GetWidth() && m_picture->GetHeight())
    {
        Gdiplus::Graphics graphics(dc);
        Gdiplus::Rect bounds(desktop.left, desktop.top, desktop.right - desktop.left, desktop.bottom - desktop.top);
        graphics.SetClip(bounds);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        if (m_position == DWPOS_STRETCH)
            graphics.DrawImage(m_picture.get(), bounds);
        else
        {
            // Wallpaper pixels scale relative to the real primary desktop, not the miniature monitor.
            int imageWidth = (std::max)(1, MulDiv(m_picture->GetWidth(), bounds.Width, GetSystemMetrics(SM_CXSCREEN)));
            int imageHeight = (std::max)(1, MulDiv(m_picture->GetHeight(), bounds.Height, GetSystemMetrics(SM_CYSCREEN)));
            if (m_position == DWPOS_TILE)
            {
                Gdiplus::TextureBrush tiles(m_picture.get(), Gdiplus::WrapModeTile);
                tiles.ScaleTransform(static_cast<Gdiplus::REAL>(imageWidth) / m_picture->GetWidth(),
                    static_cast<Gdiplus::REAL>(imageHeight) / m_picture->GetHeight());
                tiles.TranslateTransform(static_cast<Gdiplus::REAL>(bounds.X), static_cast<Gdiplus::REAL>(bounds.Y),
                    Gdiplus::MatrixOrderAppend);
                graphics.FillRectangle(&tiles, bounds);
            }
            else if (m_position == DWPOS_CENTER)
                graphics.DrawImage(m_picture.get(), Gdiplus::Rect(bounds.X + (bounds.Width - imageWidth) / 2,
                    bounds.Y + (bounds.Height - imageHeight) / 2, imageWidth, imageHeight));
            else
            {
                double scale = m_position == DWPOS_FIT ? (std::min)(double(bounds.Width) / m_picture->GetWidth(), double(bounds.Height) / m_picture->GetHeight()) :
                    (std::max)(double(bounds.Width) / m_picture->GetWidth(), double(bounds.Height) / m_picture->GetHeight());
                int cx = static_cast<int>(m_picture->GetWidth() * scale), cy = static_cast<int>(m_picture->GetHeight() * scale);
                graphics.DrawImage(m_picture.get(), Gdiplus::Rect(bounds.X + (bounds.Width - cx) / 2, bounds.Y + (bounds.Height - cy) / 2, cx, cy));
            }
        }
        graphics.Flush(Gdiplus::FlushIntentionSync);
    }
    SelectObject(dc, old);
    DeleteDC(dc);
    ::ReleaseDC(preview, screen);
    Static_SetBitmap(preview, bitmap);
    if (m_preview) DeleteObject(m_preview);
    m_preview = bitmap;
}
