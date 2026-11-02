#include "pch.h"
#include "AppearancePage.h"
#include "cscheme.h"
#include "helper.h"
#include "DeskIcons.h"

namespace
{
	struct SaveName { WCHAR text[100]; };
	INT_PTR CALLBACK SaveSchemeProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
	{
		auto* name = reinterpret_cast<SaveName*>(GetWindowLongPtr(dialog, DWLP_USER));
		if (message == WM_INITDIALOG)
		{
			name = reinterpret_cast<SaveName*>(lParam);
			SetWindowLongPtr(dialog, DWLP_USER, lParam);
			SendDlgItemMessage(dialog, 4330, EM_LIMITTEXT, ARRAYSIZE(name->text) - 1, 0);
			SetDlgItemText(dialog, 4330, name->text);
			SendDlgItemMessage(dialog, 4330, EM_SETSEL, 0, -1);
			EnableWindow(GetDlgItem(dialog, IDOK), name->text[0] != 0);
			return TRUE;
		}
		if (message == WM_COMMAND)
		{
			if (LOWORD(wParam) == 4330 && HIWORD(wParam) == EN_CHANGE)
			{
				WCHAR text[100] = {};
				GetDlgItemText(dialog, 4330, text, ARRAYSIZE(text));
				StrTrim(text, L" \t");
				EnableWindow(GetDlgItem(dialog, IDOK), text[0] != 0);
				return TRUE;
			}
			if (LOWORD(wParam) == IDOK && name)
			{
				GetDlgItemText(dialog, 4330, name->text, ARRAYSIZE(name->text));
				StrTrim(name->text, L" \t");
				if (!name->text[0]) return TRUE;
				EndDialog(dialog, IDOK);
				return TRUE;
			}
			if (LOWORD(wParam) == IDCANCEL) { EndDialog(dialog, IDCANCEL); return TRUE; }
		}
		return FALSE;
	}
	std::wstring ResourceText(UINT id)
	{
		WCHAR text[512] = {};
		LoadString(g_hinst, id, text, ARRAYSIZE(text));
		return text;
	}
	void StandardScheme(SCHEMEDATA& data)
	{
		// Windows 2000 loads its defaults from registry schemes (lookdlg.c).
		// Keep the built-in equivalent of the installed Windows Standard scheme,
		// independently of the MS Sans Serif font used by this applet's controls.
		static const COLORREF colors[MAX_COLORS] = {
			RGB(212,208,200), RGB(58,110,165), RGB(10,36,106), RGB(128,128,128),
			RGB(212,208,200), RGB(255,255,255), RGB(0,0,0), RGB(0,0,0), RGB(0,0,0),
			RGB(255,255,255), RGB(212,208,200), RGB(212,208,200), RGB(128,128,128),
			RGB(10,36,106), RGB(255,255,255), RGB(212,208,200), RGB(128,128,128),
			RGB(128,128,128), RGB(0,0,0), RGB(212,208,200), RGB(255,255,255),
			RGB(64,64,64), RGB(212,208,200), RGB(0,0,0), RGB(255,255,225),
			RGB(181,181,181), RGB(0,0,128), RGB(166,202,240), RGB(192,192,192)
		};
		data.version = 2;
		data.ncm.cbSize = sizeof(data.ncm);
		memcpy(data.rgb, colors, sizeof(colors));
		data.ncm.iBorderWidth = 1;
		data.ncm.iScrollWidth = data.ncm.iScrollHeight = 16;
		data.ncm.iCaptionWidth = 18; data.ncm.iCaptionHeight = 18;
		data.ncm.iSmCaptionWidth = 12; data.ncm.iSmCaptionHeight = 15;
		data.ncm.iMenuWidth = data.ncm.iMenuHeight = 18;
		data.iPaddedBorderWidth = 0;
		data.dpiScaled = FALSE;
		for (LOGFONTW* font : {&data.ncm.lfCaptionFont, &data.ncm.lfSmCaptionFont, &data.ncm.lfMenuFont,
			&data.ncm.lfStatusFont, &data.ncm.lfMessageFont, &data.lfIconTitle})
		{
			*font = {};
			font->lfHeight = -11;
			font->lfWeight = (font == &data.ncm.lfCaptionFont || font == &data.ncm.lfSmCaptionFont) ?
				FW_BOLD : FW_NORMAL;
			font->lfCharSet = ANSI_CHARSET;
			StringCchCopy(font->lfFaceName, ARRAYSIZE(font->lfFaceName), L"Tahoma");
		}
	}
}

LRESULT CAppearanceDlgProc::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	DWORD result = CaptureAppearanceScheme(scheme);
	if (result != ERROR_SUCCESS) { Error(4321, result); return TRUE; }
	if (!SystemParametersInfoForDpi(SPI_GETICONMETRICS, sizeof(editor.iconMetrics), &editor.iconMetrics, 0, 96))
	{
		Error(4321, GetLastError() ? GetLastError() : ERROR_GEN_FAILURE);
		return TRUE;
	}
	Microsoft::WRL::ComPtr<IFolderView2> view;
	FOLDERVIEWMODE mode;
	if (SUCCEEDED(GetDesktopFolderView(&view))) view->GetViewModeAndIconSize(&mode, &editor.iconSize);
	appliedIconMetrics = editor.iconMetrics;
	appliedIconSize = editor.iconSize;
	appliedDesktopColor = selectedTheme->newColor;
	selectedTheme->selectedScheme = &scheme;
	selectedTheme->szMsstylePath = L"(classic)";
	result = GetCurrentAppearanceScheme(schemeName);
	if (result != ERROR_SUCCESS) Error(4322, result);
	lastName = schemeName.name;
	PopulateSchemes();
	initialized = editor.Initialize(m_hWnd);
	if (!initialized) Error(4321, ERROR_INVALID_DATA);
	return TRUE;
}

void CAppearanceDlgProc::PopulateSchemes()
{
	HWND combo = GetDlgItem(1114);
	ComboBox_ResetContent(combo);
	registryNames.clear();
	int current = ComboBox_AddString(combo, ResourceText(2004).c_str());
	ComboBox_SetItemData(combo, current, 0); // Unnamed, currently edited configuration.
	DWORD result = EnumerateAppearanceSchemes(registryNames);
	if (result != ERROR_SUCCESS) Error(4322, result);
	for (size_t index = 0; index < registryNames.size(); ++index)
	{
		const auto& name = registryNames[index];
		WCHAR display[256] = {};
		std::wstring label = name.name;
		if (name.name[0] == L'@' && SUCCEEDED(SHLoadIndirectString(name.name.c_str(), display, ARRAYSIZE(display), nullptr)))
			label = display;
		int item = ComboBox_AddString(combo, label.c_str());
		ComboBox_SetItemData(combo, item, index + 2);
	}
	std::wstring standard = ResourceText(2005);
	if (ComboBox_FindStringExact(combo, -1, standard.c_str()) == CB_ERR)
	{
		int index = ComboBox_AddString(combo, standard.c_str());
		ComboBox_SetItemData(combo, index, 1); // Built-in, not a persisted user scheme.
	}
	SelectSchemeName();
}

void CAppearanceDlgProc::SelectSchemeName()
{
	HWND combo = GetDlgItem(1114);
	int index = CB_ERR;
	const AppearanceSchemeName builtIn{AppearanceSchemeStore::BuiltIn, ResourceText(2005), L""};
	for (int item = 0; item < ComboBox_GetCount(combo); ++item)
	{
		ULONG_PTR data = ComboBox_GetItemData(combo, item);
		if (data == 0)
		{
			if (schemeName.name.empty()) { index = item; break; }
			continue;
		}
		const AppearanceSchemeName& name = data == 1 ? builtIn : registryNames[data - 2];
		if (name.store == schemeName.store && lstrcmpi(name.name.c_str(), schemeName.name.c_str()) == 0 &&
			lstrcmpi(name.size.c_str(), schemeName.size.c_str()) == 0) { index = item; break; }
	}
	if (index == CB_ERR)
	{
		schemeName.name.clear();
		for (int item = 0; item < ComboBox_GetCount(combo); ++item)
			if (ComboBox_GetItemData(combo, item) == 0) { index = item; break; }
	}
	ComboBox_SetCurSel(combo, index);
	::EnableWindow(GetDlgItem(1118), index != CB_ERR && ComboBox_GetItemData(combo, index) > 1);
}

LRESULT CAppearanceDlgProc::OnScheme(WORD, WORD, HWND combo, BOOL&)
{
	if (!initialized) return 0;
	int index = ComboBox_GetCurSel(combo);
	if (index == CB_ERR) return 0;
	ULONG_PTR data = ComboBox_GetItemData(combo, index);
	AppearanceSchemeName name = data == 0 ? AppearanceSchemeName{} : data == 1 ?
		AppearanceSchemeName{AppearanceSchemeStore::BuiltIn, ResourceText(2005), L""} : registryNames[data - 2];
	SCHEMEDATA candidate = scheme;
	DWORD result = ERROR_SUCCESS;
	if (data == 0) result = CaptureAppearanceScheme(candidate);
	else if (data == 1) StandardScheme(candidate);
	else result = LoadAppearanceScheme(name, candidate);
	if (result != ERROR_SUCCESS) { Error(4322, result); SelectSchemeName(); return 0; }
	scheme = candidate;
	selectedTheme->newColor = scheme.rgb[COLOR_DESKTOP];
	schemeName = name;
	lastName = name.name;
	SelectSchemeName();
	editor.Refresh();
	Modified();
	return 0;
}

LRESULT CAppearanceDlgProc::OnSave(WORD, WORD, HWND, BOOL&)
{
	if (!initialized) return 0;
	SaveName name = {};
	StringCchCopy(name.text, ARRAYSIZE(name.text), lastName.c_str());
	INT_PTR dialogResult = DialogBoxParam(g_hinst, MAKEINTRESOURCE(IDD_APPEARANCEBOX), m_hWnd, SaveSchemeProc,
		reinterpret_cast<LPARAM>(&name));
	if (dialogResult == -1) { Error(4323, GetLastError()); return 0; }
	if (dialogResult != IDOK) return 0;
	AppearanceSchemeName target = schemeName;
	target.name = name.text;
	if (target.store == AppearanceSchemeStore::BuiltIn) target.store = AppearanceSchemeStore::Legacy;
	if (target.store == AppearanceSchemeStore::Classic && target.size.empty()) target.size = L"Normal";
	DWORD exists = AppearanceSchemeExists(target);
	if (exists != ERROR_SUCCESS && exists != ERROR_FILE_NOT_FOUND) { Error(4323, exists); return 0; }
	if (exists == ERROR_SUCCESS &&
		MessageBox(ResourceText(4327).c_str(), ResourceText(4320).c_str(), MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
	DWORD result = SaveAppearanceScheme(target, scheme);
	if (result != ERROR_SUCCESS) { Error(4323, result); return 0; }
	schemeName = target;
	lastName = name.text;
	PopulateSchemes();
	Modified();
	return 0;
}

LRESULT CAppearanceDlgProc::OnDelete(WORD, WORD, HWND, BOOL&)
{
	if (schemeName.name.empty() || schemeName.store == AppearanceSchemeStore::BuiltIn) return 0;
	if (MessageBox(ResourceText(4328).c_str(), ResourceText(4320).c_str(), MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
	DWORD result = DeleteAppearanceScheme(schemeName);
	if (result != ERROR_SUCCESS) { Error(4324, result); return 0; }
	schemeName.name.clear();
	PopulateSchemes();
	Modified();
	return 0;
}

LRESULT CAppearanceDlgProc::OnEditor(WORD code, WORD id, HWND, BOOL&)
{
	if (initialized && editor.OnCommand(id, code))
	{
		schemeName.name.clear();
		SelectSchemeName();
		Modified();
	}
	return 0;
}

void CAppearanceDlgProc::Modified()
{
	dirty = true;
	SetModified(TRUE);
}

int CAppearanceDlgProc::OnSetActive()
{
	_TerminateProcess(pi);
	if (initialized)
	{
		if (selectedTheme->newColor != 0xB0000000) scheme.rgb[COLOR_DESKTOP] = selectedTheme->newColor;
		editor.Refresh();
	}
	return 0;
}

int CAppearanceDlgProc::OnApply()
{
	if (!initialized || !dirty) return PSNRET_NOERROR;
	if (selectedTheme->newColor != 0xB0000000) scheme.rgb[COLOR_DESKTOP] = selectedTheme->newColor;
	DWORD result = ApplyAppearanceScheme(scheme);
	if (result != ERROR_SUCCESS) { Error(4325, result); return PSNRET_INVALID_NOCHANGEPAGE; }
	if (editor.iconMetrics.iHorzSpacing != appliedIconMetrics.iHorzSpacing ||
		editor.iconMetrics.iVertSpacing != appliedIconMetrics.iVertSpacing)
	{
		ICONMETRICSW metrics = editor.iconMetrics;
		UINT dpi = GetDpiForWindow(m_hWnd);
		metrics.iHorzSpacing = MulDiv(metrics.iHorzSpacing, dpi, 96);
		metrics.iVertSpacing = MulDiv(metrics.iVertSpacing, dpi, 96);
		metrics.lfFont = scheme.lfIconTitle;
		ScaleLogFont(metrics.lfFont, dpi);
		if (!SystemParametersInfo(SPI_SETICONMETRICS, sizeof(metrics), &metrics, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE))
		{ Error(4325, GetLastError()); return PSNRET_INVALID_NOCHANGEPAGE; }
	}
	if (editor.iconSize != appliedIconSize)
	{
		Microsoft::WRL::ComPtr<IFolderView2> view;
		FOLDERVIEWMODE mode;
		int oldSize;
		HRESULT hr = GetDesktopFolderView(&view);
		if (SUCCEEDED(hr)) hr = view->GetViewModeAndIconSize(&mode, &oldSize);
		if (SUCCEEDED(hr)) hr = view->SetViewModeAndIconSize(mode, editor.iconSize);
		if (FAILED(hr)) { Error(4325, HRESULT_CODE(hr)); return PSNRET_INVALID_NOCHANGEPAGE; }
	}
	DWORD status = SetCurrentAppearanceScheme(schemeName);
	if (status != ERROR_SUCCESS) { Error(4325, status); return PSNRET_INVALID_NOCHANGEPAGE; }
	appliedDesktopColor = selectedTheme->newColor;
	appliedIconMetrics = editor.iconMetrics;
	appliedIconSize = editor.iconSize;
	dirty = false;
	SetModified(FALSE);
	return PSNRET_NOERROR;
}

void CAppearanceDlgProc::OnReset()
{
	if (initialized) selectedTheme->newColor = appliedDesktopColor;
}

LRESULT CAppearanceDlgProc::OnDestroy(UINT, WPARAM, LPARAM, BOOL&)
{
	editor.Destroy();
	if (selectedTheme->selectedScheme == &scheme) selectedTheme->selectedScheme = nullptr;
	return 0;
}

void CAppearanceDlgProc::Error(UINT operation, DWORD error)
{
	WCHAR detail[512] = {}, message[1024] = {};
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, error, 0, detail, ARRAYSIZE(detail), nullptr);
	StringCchPrintf(message, ARRAYSIZE(message), ResourceText(4326).c_str(), ResourceText(operation).c_str(), error, detail);
	MessageBox(message, ResourceText(4320).c_str(), MB_OK | MB_ICONERROR);
}
