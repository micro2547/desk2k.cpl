#include "pch.h"
#include "AppearanceDlgBox.h"
#include "cscheme.h"
#include "helper.h"

namespace
{
	void SelectComboText(HWND combo, LPCWSTR text)
	{
		// WM_SETTEXT alone leaves the list's previous selection in place.
		ComboBox_SetCurSel(combo, ComboBox_FindStringExact(combo, -1, text));
		SetWindowTextW(combo, text);
	}

	// The four-column palette and the extra current-color square follow colorpic.c.
	// Keep this a native modal popup: the scheme changes only after it returns IDOK.
	struct AppearanceColorPalette
	{
		HWND owner;
		COLORREF color;
		COLORREF colors[20] =
		{
			RGB(255,255,255), RGB(0,0,0), RGB(192,192,192), RGB(128,128,128),
			RGB(255,0,0), RGB(128,0,0), RGB(255,255,0), RGB(128,128,0),
			RGB(0,255,0), RGB(0,128,0), RGB(0,255,255), RGB(0,128,128),
			RGB(0,0,255), RGB(0,0,128), RGB(255,0,255), RGB(128,0,128)
		};
		int cell = 0;
		int selected = 0;
		bool closing = false;
		bool pressed = false;

		void Finish(HWND dialog, INT_PTR result)
		{
			if (closing) return;
			closing = true;
			if (GetCapture() == dialog) ReleaseCapture();
			EndDialog(dialog, result);
		}

		void Select(HWND dialog, int index)
		{
			int id = index < 0 ? IDC_APPEARANCE_PALETTE_OTHER :
				index == 20 ? IDC_APPEARANCE_PALETTE_CURRENT : IDC_APPEARANCE_PALETTE_GRID;
			HWND control = GetDlgItem(dialog, id);
			if (selected == index && GetFocus() == control) return;
			selected = index;
			SetFocus(control);
			InvalidateRect(GetDlgItem(dialog, IDC_APPEARANCE_PALETTE_GRID), nullptr, FALSE);
			InvalidateRect(GetDlgItem(dialog, IDC_APPEARANCE_PALETTE_CURRENT), nullptr, FALSE);
		}

		int HitTest(HWND dialog, POINT pt) const
		{
			for (int id : {IDC_APPEARANCE_PALETTE_GRID, IDC_APPEARANCE_PALETTE_CURRENT, IDC_APPEARANCE_PALETTE_OTHER})
			{
				HWND control = GetDlgItem(dialog, id);
				if (!IsWindowVisible(control)) continue;
				RECT rect;
				GetWindowRect(control, &rect);
				MapWindowPoints(nullptr, dialog, reinterpret_cast<POINT*>(&rect), 2);
				if (!PtInRect(&rect, pt)) continue;
				if (id == IDC_APPEARANCE_PALETTE_OTHER) return -1;
				if (id == IDC_APPEARANCE_PALETTE_CURRENT) return 20;
				return (pt.y - rect.top) / cell * 4 + (pt.x - rect.left) / cell;
			}
			return -2;
		}

		void Accept(HWND dialog)
		{
			if (selected < 0) Finish(dialog, IDC_APPEARANCE_PALETTE_OTHER);
			else
			{
				if (selected < 20) color = colors[selected];
				Finish(dialog, IDOK);
			}
		}

		void DrawSquare(HDC dc, RECT rect, COLORREF value, bool focused) const
		{
			FillRect(dc, &rect, GetSysColorBrush(COLOR_BTNFACE));
			if (focused)
			{
				FrameRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
				InflateRect(&rect, -1, -1);
				FrameRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
				InflateRect(&rect, -1, -1);
				FrameRect(dc, &rect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
				InflateRect(&rect, -1, -1);
			}
			else
			{
				InflateRect(&rect, -1, -1);
				DrawEdge(dc, &rect, EDGE_SUNKEN, BF_RECT | BF_ADJUST);
			}
			HBRUSH brush = CreateSolidBrush(value);
			FillRect(dc, &rect, brush);
			DeleteObject(brush);
		}

		static LRESULT CALLBACK ColorKeyProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
			UINT_PTR, DWORD_PTR data)
		{
			auto* palette = reinterpret_cast<AppearanceColorPalette*>(data);
			HWND dialog = GetParent(hwnd);
			if (message == WM_GETDLGCODE)
			{
				auto* key = reinterpret_cast<MSG*>(lParam);
				LRESULT code = DLGC_WANTARROWS;
				if (key && (key->wParam == VK_RETURN || key->wParam == VK_SPACE || key->wParam == VK_F4))
					code |= DLGC_WANTMESSAGE;
				return code;
			}
			if (message == WM_KEYDOWN)
			{
				int index = palette->selected;
				switch (wParam)
				{
				case VK_LEFT: index = index < 20 ? (index + 19) % 20 : 19; break;
				case VK_RIGHT: index = index < 20 ? (index + 1) % 20 : 0; break;
				case VK_UP: index = index < 20 ? (index + 16) % 20 : 16; break;
				case VK_DOWN: index = index < 20 ? (index + 4) % 20 : 0; break;
				case VK_HOME: index = 0; break;
				case VK_END: index = 19; break;
				case VK_RETURN:
				case VK_SPACE:
					palette->selected = GetDlgCtrlID(hwnd) == IDC_APPEARANCE_PALETTE_CURRENT ? 20 : index;
					palette->Accept(dialog);
					return 0;
				case VK_F4:
					palette->Finish(dialog, IDCANCEL);
					return 0;
				default:
					return DefSubclassProc(hwnd, message, wParam, lParam);
				}
				palette->Select(dialog, index);
				return 0;
			}
			if (message == WM_SETFOCUS)
			{
				if (GetDlgCtrlID(hwnd) == IDC_APPEARANCE_PALETTE_CURRENT) palette->selected = 20;
				else if (palette->selected < 0 || palette->selected >= 20) palette->selected = 0;
			}
			if (message == WM_SETFOCUS || message == WM_KILLFOCUS)
				InvalidateRect(hwnd, nullptr, FALSE);
			return DefSubclassProc(hwnd, message, wParam, lParam);
		}

		void Initialize(HWND dialog)
		{
			PALETTEENTRY entries[4] = {};
			GetPaletteEntries(static_cast<HPALETTE>(GetStockObject(DEFAULT_PALETTE)), 8, 4, entries);
			for (int i = 0; i < 4; ++i)
				colors[16 + i] = RGB(entries[i].peRed, entries[i].peGreen, entries[i].peBlue);
			selected = 20;
			for (int i = 0; i < 20; ++i)
				if (colors[i] == color) { selected = i; break; }
			HWND grid = GetDlgItem(dialog, IDC_APPEARANCE_PALETTE_GRID);
			HWND current = GetDlgItem(dialog, IDC_APPEARANCE_PALETTE_CURRENT);
			HWND other = GetDlgItem(dialog, IDC_APPEARANCE_PALETTE_OTHER);
			// Use colorpic.c's resource-based sizing, including its localized
			// font-width accommodation so Other never overlaps the current color.
			RECT gridRect = {}, otherRect = {};
			GetClientRect(grid, &gridRect);
			GetWindowRect(other, &otherRect);
			cell = (std::max)(gridRect.bottom / 5, (RECTWIDTH(otherRect) + 2) / 3);
			const int edge = GetSystemMetricsForDpi(SM_CYEDGE, GetDpiForWindow(dialog));
			SetWindowPos(grid, nullptr, 0, 0, cell * 4, cell * 5, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
			gridRect.right = cell * 4;
			gridRect.bottom = cell * 5;
			MapWindowPoints(grid, dialog, reinterpret_cast<POINT*>(&gridRect), 2);
			SetWindowPos(GetDlgItem(dialog, -1), nullptr, gridRect.left + 1, gridRect.bottom + edge,
				cell * 4 - 2, edge, SWP_NOACTIVATE | SWP_NOZORDER);
			const int bottomRow = gridRect.bottom + 3 * edge;
			MapWindowPoints(nullptr, dialog, reinterpret_cast<POINT*>(&otherRect), 2);
			SetWindowPos(other, nullptr, otherRect.left, bottomRow, RECTWIDTH(otherRect), cell,
				SWP_NOACTIVATE | SWP_NOZORDER);
			SetWindowPos(current, nullptr, gridRect.right - cell, bottomRow, cell, cell,
				SWP_NOACTIVATE | SWP_NOZORDER);
			SetWindowSubclass(grid, ColorKeyProc, 0, reinterpret_cast<DWORD_PTR>(this));
			SetWindowSubclass(current, ColorKeyProc, 0, reinterpret_cast<DWORD_PTR>(this));
			RECT bounds = {0, 0, cell * 4, bottomRow + cell + 1};
			AdjustWindowRectEx(&bounds, GetWindowLong(dialog, GWL_STYLE), FALSE, GetWindowLong(dialog, GWL_EXSTYLE));
			int width = RECTWIDTH(bounds), height = RECTHEIGHT(bounds);
			RECT anchor;
			GetWindowRect(owner, &anchor);
			MONITORINFO monitor = {sizeof(monitor)};
			GetMonitorInfo(MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST), &monitor);
			int x = anchor.left, y = anchor.bottom;
			if (x + width > monitor.rcWork.right) x = monitor.rcWork.right - width;
			if (x < monitor.rcWork.left) x = monitor.rcWork.left;
			if (y + height > monitor.rcWork.bottom) y = anchor.top - height;
			if (y < monitor.rcWork.top) y = monitor.rcWork.top;
			SetWindowPos(dialog, HWND_TOP, x, y, width, height, SWP_NOACTIVATE);
			Select(dialog, selected);
			PostMessage(dialog, WM_APP + 1, 0, 0);
		}

		static INT_PTR CALLBACK DialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
		{
			auto* palette = reinterpret_cast<AppearanceColorPalette*>(GetWindowLongPtr(dialog, DWLP_USER));
			if (message == WM_INITDIALOG)
			{
				palette = reinterpret_cast<AppearanceColorPalette*>(lParam);
				SetWindowLongPtr(dialog, DWLP_USER, lParam);
				palette->Initialize(dialog);
				return FALSE;
			}
			if (!palette) return FALSE;
			switch (message)
			{
			case WM_SETCURSOR:
				SetCursor(LoadCursor(nullptr, IDC_ARROW));
				return TRUE;
			case WM_APP + 1:
				if (!palette->closing)
				{
					SetCursor(LoadCursor(nullptr, IDC_ARROW));
					SetCapture(dialog);
				}
				return TRUE;
			case WM_DRAWITEM:
			{
				auto* draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
				if (draw->CtlID == IDC_APPEARANCE_PALETTE_GRID)
					for (int i = 0; i < 20; ++i)
					{
						RECT rect = {i % 4 * palette->cell, i / 4 * palette->cell,
							(i % 4 + 1) * palette->cell, (i / 4 + 1) * palette->cell};
						palette->DrawSquare(draw->hDC, rect, palette->colors[i],
							i == palette->selected && GetFocus() == draw->hwndItem);
					}
				else if (draw->CtlID == IDC_APPEARANCE_PALETTE_CURRENT)
					palette->DrawSquare(draw->hDC, draw->rcItem, palette->color, GetFocus() == draw->hwndItem);
				return TRUE;
			}
			case WM_MOUSEMOVE:
			{
				int hit = palette->HitTest(dialog, {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
				if (hit >= -1) palette->Select(dialog, hit);
				Button_SetState(GetDlgItem(dialog, IDC_APPEARANCE_PALETTE_OTHER), palette->pressed && hit == -1);
				return TRUE;
			}
			case WM_LBUTTONDOWN:
			case WM_RBUTTONDOWN:
			{
				int hit = palette->HitTest(dialog, {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
				if (hit == -2) palette->Finish(dialog, IDCANCEL);
				else
				{
					palette->pressed = true;
					palette->Select(dialog, hit);
					Button_SetState(GetDlgItem(dialog, IDC_APPEARANCE_PALETTE_OTHER), hit == -1);
				}
				return TRUE;
			}
			case WM_LBUTTONUP:
			case WM_RBUTTONUP:
			{
				if (!palette->pressed) return TRUE;
				int hit = palette->HitTest(dialog, {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
				if (hit == -2) palette->Finish(dialog, IDCANCEL);
				else { palette->Select(dialog, hit); palette->Accept(dialog); }
				return TRUE;
			}
			case WM_COMMAND:
				switch (LOWORD(wParam))
				{
				case IDC_APPEARANCE_PALETTE_OTHER: palette->Finish(dialog, IDC_APPEARANCE_PALETTE_OTHER); break;
				case IDC_APPEARANCE_PALETTE_CURRENT: palette->selected = 20; palette->Accept(dialog); break;
				case IDC_APPEARANCE_PALETTE_GRID:
				case IDOK: palette->Accept(dialog); break;
				case IDCANCEL: palette->Finish(dialog, IDCANCEL); break;
				}
				return TRUE;
			case WM_ACTIVATE:
				if (LOWORD(wParam) == WA_INACTIVE) palette->Finish(dialog, IDCANCEL);
				return TRUE;
			case WM_CAPTURECHANGED:
				if (!palette->closing && reinterpret_cast<HWND>(lParam) != dialog &&
					!IsChild(dialog, reinterpret_cast<HWND>(lParam)))
					palette->Finish(dialog, IDCANCEL);
				return TRUE;
			case WM_CLOSE:
				palette->Finish(dialog, IDCANCEL);
				return TRUE;
			}
			return FALSE;
		}
	};

	LRESULT CALLBACK ColorButtonKeyProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR)
	{
		if (message == WM_GETDLGCODE)
		{
			auto* key = reinterpret_cast<MSG*>(lParam);
			if (key && (key->wParam == VK_DOWN || key->wParam == VK_F4))
				return DefSubclassProc(hwnd, message, wParam, lParam) | DLGC_WANTMESSAGE;
		}
		if ((message == WM_KEYDOWN && (wParam == VK_DOWN || wParam == VK_F4)) ||
			(message == WM_SYSKEYDOWN && wParam == VK_DOWN))
		{
			SendMessage(hwnd, BM_CLICK, 0, 0);
			return 0;
		}
		return DefSubclassProc(hwnd, message, wParam, lParam);
	}
}

// Resource order is independent of the sorted item combo box.
const CAppearanceDlgBox::ITEM CAppearanceDlgBox::items[] =
{
	{4301, COLOR_DESKTOP, -1, -1, -1, -1},
	{4302, COLOR_INACTIVECAPTION, COLOR_GRADIENTINACTIVECAPTION, COLOR_INACTIVECAPTIONTEXT, SM_CYSIZE, 0},
	{4303, COLOR_INACTIVEBORDER, -1, -1, SM_CYBORDER, -1},
	{4304, COLOR_ACTIVECAPTION, COLOR_GRADIENTACTIVECAPTION, COLOR_CAPTIONTEXT, SM_CYSIZE, 0},
	{4305, COLOR_ACTIVEBORDER, -1, -1, SM_CYBORDER, -1},
	{4306, COLOR_MENU, -1, COLOR_MENUTEXT, SM_CYMENUSIZE, 2},
	{4307, COLOR_HIGHLIGHT, -1, COLOR_HIGHLIGHTTEXT, -1, -1},
	{4308, COLOR_WINDOW, -1, COLOR_WINDOWTEXT, -1, -1},
	{4309, COLOR_SCROLLBAR, -1, -1, SM_CYVSCROLL, -1},
	{4310, COLOR_BTNFACE, -1, COLOR_BTNTEXT, -1, -1},
	{4311, -1, -1, -1, SM_CYSMSIZE, 1},
	{4312, -1, -1, -1, SM_CYSIZE, -1},
	{4313, -1, -1, COLOR_WINDOWTEXT, -1, 4},
	{4314, COLOR_APPWORKSPACE, -1, -1, -1, -1},
	{4315, -1, -1, -1, SM_CXICONSPACING, -1},
	{4316, -1, -1, -1, SM_CYICONSPACING, -1},
	{4317, COLOR_INFOBK, -1, COLOR_INFOTEXT, -1, 3},
	{4318, -1, -1, -1, SM_CXICON, 5},
	{4319, -1, -1, COLOR_GRAYTEXT, -1, -1}
};

int CALLBACK CAppearanceDlgBox::EnumFont(const LOGFONTW* font, const TEXTMETRICW*, DWORD, LPARAM data)
{
	HWND combo = reinterpret_cast<HWND>(data);
	if (font->lfFaceName[0] != L'@' && ComboBox_FindStringExact(combo, -1, font->lfFaceName) == CB_ERR)
		ComboBox_AddString(combo, font->lfFaceName);
	return 1;
}

bool CAppearanceDlgBox::Initialize(HWND owner)
{
	page = owner;
	preview = GetDlgItem(page, 1110);
	HWND combo = GetDlgItem(page, 1126);
	for (const auto& item : items)
	{
		WCHAR name[80] = {};
		LoadString(g_hinst, item.name, name, ARRAYSIZE(name));
		int index = ComboBox_AddString(combo, name);
		ComboBox_SetItemData(combo, index, &item);
		if (item.name == 4301) ComboBox_SetCurSel(combo, index);
	}
	LOGFONTW filter = {};
	filter.lfCharSet = DEFAULT_CHARSET;
	HDC dc = GetDC(page);
	EnumFontFamiliesExW(dc, &filter, EnumFont, reinterpret_cast<LPARAM>(GetDlgItem(page, 1129)), 0);
	ReleaseDC(page, dc);
	for (int point = 6; point <= 24; ++point)
	{
		WCHAR text[8];
		StringCchPrintf(text, ARRAYSIZE(text), L"%d", point);
		ComboBox_AddString(GetDlgItem(page, 1130), text);
	}
	LOGFONTW buttonFont = {};
	HFONT dialogFont = reinterpret_cast<HFONT>(SendMessage(page, WM_GETFONT, 0, 0));
	if (GetObject(dialogFont, sizeof(buttonFont), &buttonFont))
	{
		buttonFont.lfWeight = FW_BOLD;
		boldFont = CreateFontIndirect(&buttonFont);
		buttonFont.lfWeight = FW_NORMAL;
		buttonFont.lfItalic = TRUE;
		italicFont = CreateFontIndirect(&buttonFont);
		SendDlgItemMessage(page, 1131, WM_SETFONT, reinterpret_cast<WPARAM>(boldFont), TRUE);
		SendDlgItemMessage(page, 1132, WM_SETFONT, reinterpret_cast<WPARAM>(italicFont), TRUE);
	}
	SendDlgItemMessage(page, 1128, EM_LIMITTEXT, 3, 0);
	SendDlgItemMessage(page, 1130, CB_LIMITTEXT, 3, 0);
	SendDlgItemMessage(page, 1129, CB_LIMITTEXT, LF_FACESIZE - 1, 0);
	SendDlgItemMessage(page, 1133, UDM_SETRANGE32, 1, 500);
	SetWindowSubclass(preview, PreviewProc, 0, reinterpret_cast<DWORD_PTR>(this));
	for (int id : {1135, 1141, 1136})
		SetWindowSubclass(GetDlgItem(page, id), ColorButtonKeyProc, 0, 0);
	windowPreview = Microsoft::WRL::Details::Make<CWindowPreview>(GetClientSIZE(preview), windows,
		static_cast<int>(ARRAYSIZE(windows)), PT_APPEARANCE, nullptr, GetDpiForWindow(page));
	Microsoft::WRL::ComPtr<IWindowConfig> config;
	windowPreview.As(&config);
	config->SetClassicPrev(TRUE);
	HBITMAP bitmap = nullptr;
	HRESULT result = windowPreview->GetPreviewImage(&bitmap);
	SetBitmap(preview, bitmap);
	Refresh();
	return SUCCEEDED(result);
}

void CAppearanceDlgBox::Destroy()
{
	if (!page) return;
	RemoveWindowSubclass(preview, PreviewProc, 0);
	SetBitmap(preview, nullptr);
	for (int id : {1135, 1141, 1136})
	{
		RemoveWindowSubclass(GetDlgItem(page, id), ColorButtonKeyProc, 0);
		DeleteObject(Button_SetBitmap(GetDlgItem(page, id), nullptr));
	}
	windowPreview.Reset();
	DeleteObject(boldFont);
	DeleteObject(italicFont);
	boldFont = italicFont = nullptr;
	page = nullptr;
}

const CAppearanceDlgBox::ITEM* CAppearanceDlgBox::CurrentItem() const
{
	HWND combo = GetDlgItem(page, 1126);
	int index = ComboBox_GetCurSel(combo);
	return index == CB_ERR ? nullptr : reinterpret_cast<const ITEM*>(ComboBox_GetItemData(combo, index));
}

LOGFONTW* CAppearanceDlgBox::Font(const ITEM& item)
{
	auto& scheme = *selectedTheme->selectedScheme;
	switch (item.font)
	{
	case 0: return &scheme.ncm.lfCaptionFont;
	case 1: return &scheme.ncm.lfSmCaptionFont;
	case 2: return &scheme.ncm.lfMenuFont;
	case 3: return &scheme.ncm.lfStatusFont;
	case 4: return &scheme.ncm.lfMessageFont;
	case 5: return &scheme.lfIconTitle;
	default: return nullptr;
	}
}

int CAppearanceDlgBox::Metric(int target) const
{
	if (target == SM_CXICONSPACING) return iconMetrics.iHorzSpacing;
	if (target == SM_CYICONSPACING) return iconMetrics.iVertSpacing;
	if (target == SM_CXICON) return iconSize;
	return NcGetSystemMetrics96(target);
}

void CAppearanceDlgBox::SetMetric(int target, int value)
{
	if (target == SM_CXICONSPACING) iconMetrics.iHorzSpacing = value;
	else if (target == SM_CYICONSPACING) iconMetrics.iVertSpacing = value;
	else if (target == SM_CXICON) iconSize = value;
	else
	{
		NcUpdateSystemMetrics(target, value);
		if (target == SM_CYVSCROLL) NcUpdateSystemMetrics(SM_CXVSCROLL, value);
		if (target == SM_CYSIZE) NcUpdateSystemMetrics(SM_CXSIZE, value);
		if (target == SM_CYSMSIZE) NcUpdateSystemMetrics(SM_CXSMSIZE, value);
	}
}

void CAppearanceDlgBox::ColorButton(int id, int target)
{
	HWND button = GetDlgItem(page, id);
	EnableWindow(button, target >= 0);
	int inset = MulDiv(3, GetDpiForWindow(page), 96);
	SIZE size = GetClientSIZE(button);
	int width = max(1, size.cx - inset * 2);
	int height = max(1, size.cy - inset * 2);
	int arrowWidth = MulDiv(8, GetDpiForWindow(page), 96);
	HDC dc = GetDC(button);
	HDC memory = CreateCompatibleDC(dc);
	HBITMAP bitmap = CreateCompatibleBitmap(dc, width, height);
	if (memory && bitmap)
	{
		HGDIOBJ old = SelectObject(memory, bitmap);
		RECT rect = {0, 0, width, height};
		FillRect(memory, &rect, GetSysColorBrush(COLOR_BTNFACE));
		rect.right = max(1, width - arrowWidth - 2);
		if (target >= 0)
		{
			HBRUSH brush = CreateSolidBrush(NcGetSysColor(target));
			FillRect(memory, &rect, brush);
			DeleteObject(brush);
			// FrameRect stays exactly one device pixel, including at higher DPI.
			FrameRect(memory, &rect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
		}
		RECT separator = {width - arrowWidth, 0, width - arrowWidth + 2, height};
		DrawEdge(memory, &separator, EDGE_ETCHED, BF_LEFT);
		int x = width - arrowWidth / 2, y = height / 2;
		COLORREF arrow = GetSysColor(target >= 0 ? COLOR_BTNTEXT : COLOR_GRAYTEXT);
		for (int row = 0; row < 3; ++row)
			for (int column = -2 + row; column <= 2 - row; ++column)
				SetPixelV(memory, x + column, y - 1 + row, arrow);
		SelectObject(memory, old);
	}
	else
	{
		DeleteObject(bitmap);
		bitmap = nullptr;
	}
	DeleteDC(memory);
	ReleaseDC(button, dc);
	DeleteObject(Button_SetBitmap(button, bitmap));
}

void CAppearanceDlgBox::Refresh()
{
	if (!page || !selectedTheme->selectedScheme) return;
	const ITEM* item = CurrentItem();
	if (!item) return;
	updating = true;
	EnableWindow(GetDlgItem(page, 1128), item->metric >= 0);
	EnableWindow(GetDlgItem(page, 1133), item->metric >= 0);
	EnableWindow(GetDlgItem(page, 1127), item->metric >= 0);
	WCHAR text[16] = {};
	if (item->metric >= 0) StringCchPrintf(text, ARRAYSIZE(text), L"%d", Metric(item->metric));
	SetDlgItemText(page, 1128, text);
	ColorButton(1135, item->color1);
	ColorButton(1141, item->color2);
	ColorButton(1136, item->textColor);
	EnableWindow(GetDlgItem(page, 1134), item->color1 >= 0);
	EnableWindow(GetDlgItem(page, 1140), item->color2 >= 0);
	EnableWindow(GetDlgItem(page, 1138), item->textColor >= 0);
	LOGFONTW* font = Font(*item);
	for (int id : {1129, 1130, 1131, 1132, 1137, 1139}) EnableWindow(GetDlgItem(page, id), font != nullptr);
	SelectComboText(GetDlgItem(page, 1129), font ? font->lfFaceName : L"");
	text[0] = 0;
	if (font) StringCchPrintf(text, ARRAYSIZE(text), L"%d", MulDiv(abs(font->lfHeight), 72, 96));
	SelectComboText(GetDlgItem(page, 1130), text);
	Button_SetCheck(GetDlgItem(page, 1131), font && font->lfWeight >= FW_BOLD ? BST_CHECKED : BST_UNCHECKED);
	Button_SetCheck(GetDlgItem(page, 1132), font && font->lfItalic ? BST_CHECKED : BST_UNCHECKED);
	updating = false;
	UpdatePreview();
}

bool CAppearanceDlgBox::OnCommand(WORD id, WORD code)
{
	if (updating || !selectedTheme->selectedScheme) return false;
	const ITEM* item = CurrentItem();
	if (!item) return false;
	if (id == 1126 && code == CBN_SELCHANGE) { Refresh(); return false; }
	if ((id == 1135 || id == 1141 || id == 1136) && code == BN_CLICKED)
	{
		int target = id == 1135 ? item->color1 : id == 1141 ? item->color2 : item->textColor;
		if (target < 0) return false;
		AppearanceColorPalette palette = {GetDlgItem(page, id), NcGetSysColor(target)};
		INT_PTR result = DialogBoxParam(g_hinst, MAKEINTRESOURCE(IDD_APPEARANCE_COLOR_PALETTE),
			page, AppearanceColorPalette::DialogProc, reinterpret_cast<LPARAM>(&palette));
		SetFocus(GetDlgItem(page, id));
		CHOOSECOLOR color = {};
		color.rgbResult = palette.color;
		if (result == IDC_APPEARANCE_PALETTE_OTHER)
		{
			if (!ColorPicker(palette.color, page, &color))
			{
				if (CommDlgExtendedError())
				{
					WCHAR title[80] = {}, message[256] = {};
					LoadString(g_hinst, 4320, title, ARRAYSIZE(title));
					LoadString(g_hinst, 4329, message, ARRAYSIZE(message));
					MessageBox(page, message, title, MB_OK | MB_ICONERROR);
				}
				return false;
			}
		}
		else if (result != IDOK)
		{
			if (result == -1)
			{
				WCHAR title[80] = {}, message[256] = {};
				LoadString(g_hinst, 4320, title, ARRAYSIZE(title));
				LoadString(g_hinst, 4329, message, ARRAYSIZE(message));
				MessageBox(page, message, title, MB_OK | MB_ICONERROR);
			}
			return false;
		}
		if (color.rgbResult == NcGetSysColor(target)) return false;
		auto& scheme = *selectedTheme->selectedScheme;
		scheme.rgb[target] = color.rgbResult;
		if (target == COLOR_BTNFACE)
		{
			// Follow lookdlg.c's linked 3-D face/shadow/highlight colors.
			COLORREF shadow = ColorAdjustLuma(color.rgbResult, -334, TRUE);
			COLORREF highlight = ColorAdjustLuma(color.rgbResult, 500, TRUE);
			for (int derived : {COLOR_3DLIGHT, COLOR_ACTIVEBORDER, COLOR_INACTIVEBORDER, COLOR_MENU})
				scheme.rgb[derived] = color.rgbResult;
			for (int derived : {COLOR_GRAYTEXT, COLOR_APPWORKSPACE, COLOR_BTNSHADOW, COLOR_INACTIVECAPTION})
				scheme.rgb[derived] = shadow;
			scheme.rgb[COLOR_BTNHIGHLIGHT] = highlight;
			scheme.rgb[COLOR_SCROLLBAR] = highlight == scheme.rgb[COLOR_WINDOW] ? RGB(192,192,192) : highlight;
		}
		if (target == COLOR_DESKTOP) selectedTheme->newColor = color.rgbResult;
		ColorButton(id, target);
	}
	else if (id == 1128 && code == EN_CHANGE && item->metric >= 0)
	{
		BOOL valid = FALSE;
		int value = GetDlgItemInt(page, id, &valid, FALSE);
		if (!valid || value < 1 || value > 500 || value == Metric(item->metric)) return false;
		SetMetric(item->metric, value);
	}
	else
	{
		LOGFONTW* font = Font(*item);
		if (!font) return false;
		if (id == 1129 && (code == CBN_SELCHANGE || code == CBN_EDITCHANGE))
		{
			WCHAR name[LF_FACESIZE] = {};
			if (code == CBN_SELCHANGE)
			{
				HWND combo = GetDlgItem(page, id);
				int index = ComboBox_GetCurSel(combo);
				if (index == CB_ERR) return false;
				ComboBox_GetLBText(combo, index, name);
				// CBN_SELCHANGE arrives before the combo updates its edit text.
				// Synchronize it before preview rendering can dispatch notifications.
				updating = true;
				SetWindowTextW(combo, name);
				updating = false;
			}
			else GetDlgItemText(page, id, name, ARRAYSIZE(name));
			if (!name[0]) return false;
			StringCchCopy(font->lfFaceName, ARRAYSIZE(font->lfFaceName), name);
		}
		else if (id == 1130 && (code == CBN_SELCHANGE || code == CBN_EDITCHANGE))
		{
			WCHAR text[16] = {};
			if (code == CBN_SELCHANGE)
			{
				HWND combo = GetDlgItem(page, id);
				int index = ComboBox_GetCurSel(combo);
				if (index == CB_ERR) return false;
				ComboBox_GetLBText(combo, index, text);
				updating = true;
				SetWindowTextW(combo, text);
				updating = false;
			}
			else GetDlgItemText(page, id, text, ARRAYSIZE(text));
			WCHAR* end = nullptr;
			long points = wcstol(text, &end, 10);
			if (*end || points < 1 || points > 144) return false;
			font->lfHeight = -MulDiv(points, 96, 72);
			if (item->metric == SM_CYSIZE || item->metric == SM_CYMENUSIZE || item->metric == SM_CYSMSIZE)
			{
				int minimum = abs(font->lfHeight) + 6;
				if (Metric(item->metric) < minimum) SetMetric(item->metric, minimum);
				updating = true;
				SetDlgItemInt(page, 1128, Metric(item->metric), FALSE);
				updating = false;
			}
		}
		else if (id == 1131 && code == BN_CLICKED)
		{
			font->lfWeight = font->lfWeight >= FW_BOLD ? FW_NORMAL : FW_BOLD;
			Button_SetCheck(GetDlgItem(page, id), font->lfWeight >= FW_BOLD);
		}
		else if (id == 1132 && code == BN_CLICKED)
		{
			font->lfItalic = !font->lfItalic;
			Button_SetCheck(GetDlgItem(page, id), font->lfItalic);
		}
		else return false;
	}
	UpdatePreview();
	return true;
}


void CAppearanceDlgBox::UpdatePreview()
{
	if (!windowPreview) return;
	HBITMAP bitmap = nullptr;
	windowPreview->GetUpdatedPreviewImage(windows, nullptr, &bitmap, UPDATE_WINDOW | UPDATE_SOLIDCLR);
	SetBitmap(preview, bitmap);
}

LRESULT CALLBACK CAppearanceDlgBox::PreviewProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR data)
{
	if (message == WM_LBUTTONDOWN)
	{
		reinterpret_cast<CAppearanceDlgBox*>(data)->OnPreviewClick({GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)});
		return 0;
	}
	return DefSubclassProc(hwnd, message, wParam, lParam);
}

void CAppearanceDlgBox::OnPreviewClick(POINT point)
{
	Microsoft::WRL::ComPtr<IWindowMetrics> metrics;
	windowPreview.As(&metrics);
	UINT name = 4301;
	struct HIT { int window, part; UINT name; };
	const HIT hits[] = {{0,-1,4303},{0,0,4302},{1,-1,4305},{1,0,4304},
		{1,6,4308},{1,9,4309},{1,8,4306},{2,-1,4313},{2,0,4304},{2,7,4310},
		{0,2,4312},{1,2,4312},{1,3,4312},{1,4,4312},{2,2,4312}};
	for (const auto& hit : hits)
	{
		RECT rect = {};
		if (SUCCEEDED(metrics->GetBoundingRect(hit.window, hit.part, &rect)) && PtInRect(&rect, point)) name = hit.name;
	}
	HWND combo = GetDlgItem(page, 1126);
	for (int i = 0; i < ComboBox_GetCount(combo); ++i)
	{
		const ITEM* item = reinterpret_cast<const ITEM*>(ComboBox_GetItemData(combo, i));
		if (item->name == name) { ComboBox_SetCurSel(combo, i); break; }
	}
	Refresh();
}
