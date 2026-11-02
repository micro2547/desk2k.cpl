#include "pch.h"
#include "cscheme.h"
#include "helper.h"

int cs_dpi;

DWORD NcGetSysColor(int nIndex)
{
	if (selectedTheme->selectedScheme)
	{
		return selectedTheme->selectedScheme->rgb[nIndex];
	}
	return GetSysColor(nIndex);
}

// callee must free the brush, if selectedScheme isnt NULL
HBRUSH NcGetSysColorBrush(int nIndex)
{
	if (selectedTheme->selectedScheme)
	{
		return CreateSolidBrush(selectedTheme->selectedScheme->rgb[nIndex]);
	}
	return GetSysColorBrush(nIndex);
}

__declspec(noinline) BOOL NcDrawFrameControl(HDC hdc, RECT* lprc, UINT uType, int type)
{
	HBRUSH br = NcGetSysColorBrush(COLOR_BTNFACE);
	FillRect(hdc, lprc, br);
	if (selectedTheme->selectedScheme) DeleteBrush(br);

	DrawEdge(hdc, lprc, EDGE_RAISED, BF_RECT | BF_SOFT);
	{
		int fontHeight = RECTHEIGHT(*lprc) - 4;
		HFONT hFont = CreateFont(fontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
			SYMBOL_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Marlett");

		if (hFont)
		{
			HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
			COLORREF oldTextColor = SetTextColor(hdc, NcGetSysColor(COLOR_BTNTEXT));

			const wchar_t* text = L"\0";
			if (uType == DFC_CAPTION)
			{
				if (type == 1) text = L"r";
				else if (type == 2) text = L"1";
				else if (type == 3) text = L"0";
			}
			else if (uType == DFC_SCROLL)
			{
				if (type == 1) text = L"t";
				else if (type == 2) text = L"u";
			}
			DrawText(hdc, text, 1, lprc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

			SetTextColor(hdc, oldTextColor);
			SelectObject(hdc, hOldFont);
			DeleteObject(hFont);
		}
	}
	return TRUE;
}

int NcGetSystemMetrics96(int nIndex)
{
	if (selectedTheme->selectedScheme)
	{
		int iValue;
		NONCLIENTMETRICSW_2k ncm = selectedTheme->selectedScheme->ncm;
		switch (nIndex)
        {
            case SM_CXHSCROLL:  // fall through
            case SM_CXVSCROLL:  iValue = ncm.iScrollWidth;  break;
            case SM_CYHSCROLL:  // fall through
            case SM_CYVSCROLL:  iValue = ncm.iScrollHeight;  break;

            case SM_CXSIZE:     iValue = ncm.iCaptionWidth;  break;
            case SM_CYSIZE:     iValue = ncm.iCaptionHeight;  break;
            case SM_CYCAPTION:  iValue = ncm.iCaptionHeight + 1;  break;
            case SM_CXSMSIZE:   iValue = ncm.iSmCaptionWidth;  break;
            case SM_CYSMSIZE:   iValue = ncm.iSmCaptionHeight;  break;
            case SM_CXMENUSIZE: iValue = ncm.iMenuWidth;  break;
            case SM_CYMENUSIZE: iValue = ncm.iMenuHeight;  break;

			case SM_CXBORDER:
			case SM_CYBORDER:	iValue = ncm.iBorderWidth; break;

			case SM_CXPADDEDBORDER: iValue = selectedTheme->selectedScheme->iPaddedBorderWidth; break;

            default: return MulDiv(GetSystemMetrics(nIndex), 96, cs_dpi ? cs_dpi : 96);
        }

        return iValue;
	}
	return GetSystemMetrics(nIndex);
}

int NcGetSystemMetrics(int nIndex)
{
	if (selectedTheme->selectedScheme)
		return MulDiv(NcGetSystemMetrics96(nIndex), cs_dpi ? cs_dpi : 96, 96);
	return GetSystemMetrics(nIndex);
}


void NcUpdateSystemMetrics(int nIndex, int value)
{
	if (selectedTheme->selectedScheme)
	{
		NONCLIENTMETRICSW_2k ncm = selectedTheme->selectedScheme->ncm;
		switch (nIndex)
		{
		case SM_CXHSCROLL:  // fall through
		case SM_CXVSCROLL:  ncm.iScrollWidth = value;  break;
		case SM_CYHSCROLL:  // fall through
		case SM_CYVSCROLL:  ncm.iScrollHeight = value;  break;

		case SM_CXSIZE:     ncm.iCaptionWidth = value;  break;
		case SM_CYSIZE:     ncm.iCaptionHeight = value;  break;
		case SM_CYCAPTION:  ncm.iCaptionHeight = value - 1;  break;
		case SM_CXSMSIZE:   ncm.iSmCaptionWidth = value;  break;
		case SM_CYSMSIZE:   ncm.iSmCaptionHeight = value;  break;
		case SM_CXMENUSIZE: ncm.iMenuWidth = value;  break;
		case SM_CYMENUSIZE: ncm.iMenuHeight = value;  break;

		case SM_CXBORDER:
		case SM_CYBORDER:	ncm.iBorderWidth = value; break;
		case SM_CXPADDEDBORDER: selectedTheme->selectedScheme->iPaddedBorderWidth = value; break;
		default: break;
		}

		selectedTheme->selectedScheme->ncm = ncm;
	}
}

namespace
{
	const WCHAR appearanceKey[] = L"Control Panel\\Appearance";
	const WCHAR legacySchemesKey[] = L"Control Panel\\Appearance\\Schemes";
	const WCHAR classicSchemesKey[] = L"Control Panel\\Appearance\\ClassicSchemes";

	std::wstring ClassicSchemePath(const AppearanceSchemeName& name, bool includeSize)
	{
		std::wstring path = std::wstring(classicSchemesKey) + L"\\" + name.name;
		if (includeSize) path += L"\\" + name.size;
		return path;
	}

	bool ValidFont(const LOGFONTW& font)
	{
		for (WCHAR character : font.lfFaceName)
			if (character == L'\0') return true;
		return false;
	}

	DWORD ReadRegistryString(LPCWSTR key, LPCWSTR value, std::wstring& text)
	{
		DWORD bytes = 0;
		LSTATUS status = RegGetValue(HKEY_CURRENT_USER, key, value, RRF_RT_REG_SZ, nullptr, nullptr, &bytes);
		if (status != ERROR_SUCCESS) return status;
		std::vector<WCHAR> buffer(bytes / sizeof(WCHAR) + 1);
		status = RegGetValue(HKEY_CURRENT_USER, key, value, RRF_RT_REG_SZ, nullptr, buffer.data(), &bytes);
		if (status == ERROR_SUCCESS) text = buffer.data();
		return status;
	}

	DWORD WriteRegistryString(LPCWSTR path, LPCWSTR value, const std::wstring& text)
	{
		HKEY key = nullptr;
		LSTATUS status = RegCreateKeyEx(HKEY_CURRENT_USER, path, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
		if (status != ERROR_SUCCESS) return status;
		status = text.empty() ? RegDeleteValue(key, value) :
			RegSetValueEx(key, value, 0, REG_SZ, reinterpret_cast<const BYTE*>(text.c_str()),
				static_cast<DWORD>((text.size() + 1) * sizeof(WCHAR)));
		RegCloseKey(key);
		return status == ERROR_FILE_NOT_FOUND && text.empty() ? ERROR_SUCCESS : status;
	}

	DWORD LoadClassicScheme(const AppearanceSchemeName& name, SCHEMEDATA& scheme)
	{
		HKEY colors = nullptr, sizes = nullptr;
		LSTATUS status = RegOpenKeyEx(HKEY_CURRENT_USER, ClassicSchemePath(name, false).c_str(),
			0, KEY_QUERY_VALUE, &colors);
		if (status != ERROR_SUCCESS) return status;
		status = RegOpenKeyEx(HKEY_CURRENT_USER, ClassicSchemePath(name, true).c_str(),
			0, KEY_QUERY_VALUE, &sizes);
		if (status != ERROR_SUCCESS) { RegCloseKey(colors); return status; }
		// Like WinClassicThemeConfig, missing values retain the current system
		// setting, not the previously selected scheme's setting. Values are 96-DPI.
		status = CaptureAppearanceScheme(scheme);
		if (status == ERROR_SUCCESS)
		{
			WCHAR value[16];
			DWORD data, bytes;
			for (int index = 0; index < MAX_COLORS; ++index)
			{
				StringCchPrintf(value, ARRAYSIZE(value), L"Color%d", index);
				bytes = sizeof(data);
				if (RegGetValue(colors, nullptr, value, RRF_RT_REG_DWORD, nullptr, &data, &bytes) == ERROR_SUCCESS)
					scheme.rgb[index] = data & 0x00ffffff;
			}
			LOGFONTW* fonts[] = {&scheme.ncm.lfCaptionFont, &scheme.ncm.lfSmCaptionFont,
				&scheme.ncm.lfMenuFont, &scheme.ncm.lfStatusFont, &scheme.ncm.lfMessageFont, &scheme.lfIconTitle};
			for (int index = 0; index < ARRAYSIZE(fonts); ++index)
			{
				StringCchPrintf(value, ARRAYSIZE(value), L"Font%d", index);
				LOGFONTW font = {};
				bytes = sizeof(font);
				if (RegGetValue(sizes, nullptr, value, RRF_RT_REG_BINARY, nullptr, &font, &bytes) == ERROR_SUCCESS &&
					bytes == sizeof(font) && ValidFont(font))
					*fonts[index] = font;
			}
			int* metrics[] = {&scheme.ncm.iBorderWidth, &scheme.ncm.iScrollWidth, &scheme.ncm.iScrollHeight,
				&scheme.ncm.iCaptionWidth, &scheme.ncm.iCaptionHeight, &scheme.ncm.iSmCaptionWidth,
				&scheme.ncm.iSmCaptionHeight, &scheme.ncm.iMenuWidth, &scheme.ncm.iMenuHeight,
				&scheme.iPaddedBorderWidth};
			for (int index = 0; index < ARRAYSIZE(metrics); ++index)
			{
				StringCchPrintf(value, ARRAYSIZE(value), L"Size%d", index);
				bytes = sizeof(data);
				if (RegGetValue(sizes, nullptr, value, RRF_RT_REG_DWORD, nullptr, &data, &bytes) == ERROR_SUCCESS &&
					static_cast<int>(data) >= 0)
					*metrics[index] = static_cast<int>(data);
			}
		}
		RegCloseKey(sizes);
		RegCloseKey(colors);
		return status;
	}

	DWORD SaveClassicScheme(const AppearanceSchemeName& name, const SCHEMEDATA& scheme)
	{
		if (name.name.empty() || name.size.empty() || name.name.find(L'\\') != std::wstring::npos ||
			name.size.find(L'\\') != std::wstring::npos) return ERROR_INVALID_NAME;
		HKEY colors = nullptr, sizes = nullptr;
		LSTATUS status = RegCreateKeyEx(HKEY_CURRENT_USER, ClassicSchemePath(name, false).c_str(),
			0, nullptr, 0, KEY_SET_VALUE, nullptr, &colors, nullptr);
		if (status != ERROR_SUCCESS) return status;
		status = RegCreateKeyEx(HKEY_CURRENT_USER, ClassicSchemePath(name, true).c_str(),
			0, nullptr, 0, KEY_SET_VALUE, nullptr, &sizes, nullptr);
		if (status == ERROR_SUCCESS)
		{
			WCHAR value[16];
			for (int index = 0; index < MAX_COLORS && status == ERROR_SUCCESS; ++index)
			{
				StringCchPrintf(value, ARRAYSIZE(value), L"Color%d", index);
				status = RegSetValueEx(colors, value, 0, REG_DWORD,
					reinterpret_cast<const BYTE*>(&scheme.rgb[index]), sizeof(COLORREF));
			}
			const LOGFONTW* fonts[] = {&scheme.ncm.lfCaptionFont, &scheme.ncm.lfSmCaptionFont,
				&scheme.ncm.lfMenuFont, &scheme.ncm.lfStatusFont, &scheme.ncm.lfMessageFont, &scheme.lfIconTitle};
			for (int index = 0; index < ARRAYSIZE(fonts) && status == ERROR_SUCCESS; ++index)
			{
				StringCchPrintf(value, ARRAYSIZE(value), L"Font%d", index);
				LOGFONTW font = *fonts[index];
				if (!ValidFont(font)) { status = ERROR_INVALID_DATA; break; }
				size_t length = wcslen(font.lfFaceName) + 1;
				ZeroMemory(font.lfFaceName + length, (LF_FACESIZE - length) * sizeof(WCHAR));
				status = RegSetValueEx(sizes, value, 0, REG_BINARY,
					reinterpret_cast<const BYTE*>(&font), sizeof(font));
			}
			const int* metrics[] = {&scheme.ncm.iBorderWidth, &scheme.ncm.iScrollWidth, &scheme.ncm.iScrollHeight,
				&scheme.ncm.iCaptionWidth, &scheme.ncm.iCaptionHeight, &scheme.ncm.iSmCaptionWidth,
				&scheme.ncm.iSmCaptionHeight, &scheme.ncm.iMenuWidth, &scheme.ncm.iMenuHeight,
				&scheme.iPaddedBorderWidth};
			for (int index = 0; index < ARRAYSIZE(metrics) && status == ERROR_SUCCESS; ++index)
			{
				StringCchPrintf(value, ARRAYSIZE(value), L"Size%d", index);
				DWORD data = static_cast<DWORD>(*metrics[index]);
				status = RegSetValueEx(sizes, value, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&data), sizeof(data));
			}
			RegCloseKey(sizes);
		}
		RegCloseKey(colors);
		return status;
	}
	// The Windows 2000 layout uses a WORD version followed by alignment padding.
	// Do not persist SCHEMEDATA's preview/runtime fields.
	struct STORED_SCHEME
	{
		WORD version;
		WORD reserved;
		NONCLIENTMETRICSW_2k metrics;
		LOGFONTW iconFont;
		COLORREF colors[MAX_COLORS];
	};
	static_assert(sizeof(NONCLIENTMETRICSW_2k) == 500, "Legacy Unicode metrics layout");
	static_assert(sizeof(STORED_SCHEME) - MAX_COLORS * sizeof(COLORREF) == 596, "Legacy scheme colors offset");
	static_assert(sizeof(STORED_SCHEME) == 712, "Windows 2000 scheme layout");

	bool ValidSchemeFonts(const STORED_SCHEME& stored)
	{
		for (const LOGFONTW* font : {&stored.metrics.lfCaptionFont, &stored.metrics.lfSmCaptionFont,
			&stored.metrics.lfMenuFont, &stored.metrics.lfStatusFont, &stored.metrics.lfMessageFont, &stored.iconFont})
		{
			if (!ValidFont(*font)) return false;
		}
		return true;
	}
	DWORD SpiError()
	{
		DWORD error = GetLastError();
		return error ? error : ERROR_GEN_FAILURE;
	}
	void ScaleFonts(NONCLIENTMETRICSW& metrics, UINT dpi)
	{
		for (LOGFONTW* font : {&metrics.lfCaptionFont, &metrics.lfSmCaptionFont,
			&metrics.lfMenuFont, &metrics.lfStatusFont, &metrics.lfMessageFont})
			ScaleLogFont(*font, dpi);
	}
}

DWORD CaptureAppearanceScheme(SCHEMEDATA& scheme)
{
	NONCLIENTMETRICSW metrics = {sizeof(metrics)};
	LOGFONTW iconFont = {};
	if (!SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, 96) ||
		!SystemParametersInfoForDpi(SPI_GETICONTITLELOGFONT, sizeof(iconFont), &iconFont, 0, 96))
		return SpiError();
	scheme = {};
	scheme.version = 2;
	memcpy(&scheme.ncm, &metrics, sizeof(scheme.ncm));
	scheme.ncm.cbSize = sizeof(scheme.ncm);
	scheme.lfIconTitle = iconFont;
	scheme.iPaddedBorderWidth = metrics.iPaddedBorderWidth;
	for (int i = 0; i < MAX_COLORS; ++i) scheme.rgb[i] = GetSysColor(i);
	return ERROR_SUCCESS;
}

DWORD LoadAppearanceScheme(const AppearanceSchemeName& name, SCHEMEDATA& scheme)
{
	if (name.store == AppearanceSchemeStore::Classic) return LoadClassicScheme(name, scheme);
	if (name.store != AppearanceSchemeStore::Legacy) return ERROR_INVALID_PARAMETER;
	STORED_SCHEME stored = {};
	DWORD size = sizeof(stored), type = 0;
	LSTATUS status = RegGetValue(HKEY_CURRENT_USER, legacySchemesKey,
		name.name.c_str(), RRF_RT_REG_BINARY, &type, &stored, &size);
	if (status == ERROR_MORE_DATA) return ERROR_INVALID_DATA;
	if (status != ERROR_SUCCESS) return status;
	// NT 4 schemes have 25 colors; Windows 2000 and our saved schemes have 29.
	// Require a complete known layout, not an arbitrary truncated color array.
	const DWORD nt4Size = FIELD_OFFSET(STORED_SCHEME, colors) + 25 * sizeof(COLORREF);
	if (stored.version != 2 ||
		(size != nt4Size && size != sizeof(stored)) || !ValidSchemeFonts(stored))
		return ERROR_INVALID_DATA;
	// The original GetMyNonClientMetrics did not initialize cbSize before
	// saving. Installed high-contrast schemes contain 70 here, not 500.
	// It is not a disk-format discriminator; restore it for runtime use.
	scheme.version = 2;
	scheme.ncm = stored.metrics;
	scheme.ncm.cbSize = sizeof(scheme.ncm);
	scheme.lfIconTitle = stored.iconFont;
	memcpy(scheme.rgb, stored.colors, sizeof(scheme.rgb));
	if (size == nt4Size)
	{
		// Match Windows 2000 Scheme_SetScheme's NT 4 color extensions.
		scheme.rgb[COLOR_HOTLIGHT] = scheme.rgb[COLOR_ACTIVECAPTION];
		scheme.rgb[COLOR_GRADIENTACTIVECAPTION] = scheme.rgb[COLOR_ACTIVECAPTION];
		scheme.rgb[COLOR_GRADIENTINACTIVECAPTION] = scheme.rgb[COLOR_INACTIVECAPTION];
	}
	scheme.iPaddedBorderWidth = 0;
	scheme.dpiScaled = FALSE;
	return ERROR_SUCCESS;
}

DWORD SaveAppearanceScheme(const AppearanceSchemeName& name, const SCHEMEDATA& scheme)
{
	if (name.store == AppearanceSchemeStore::Classic) return SaveClassicScheme(name, scheme);
	if (name.store != AppearanceSchemeStore::Legacy || name.name.empty()) return ERROR_INVALID_PARAMETER;
	STORED_SCHEME stored = {};
	stored.version = 2;
	stored.metrics = scheme.ncm;
	stored.metrics.cbSize = sizeof(stored.metrics);
	stored.iconFont = scheme.lfIconTitle;
	memcpy(stored.colors, scheme.rgb, sizeof(stored.colors));
	HKEY key = nullptr;
	LSTATUS status = RegCreateKeyEx(HKEY_CURRENT_USER, legacySchemesKey,
		0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
	if (status == ERROR_SUCCESS)
	{
		status = RegSetValueEx(key, name.name.c_str(), 0, REG_BINARY,
			reinterpret_cast<const BYTE*>(&stored), sizeof(stored));
		RegCloseKey(key);
	}
	return status;
}

DWORD EnumerateAppearanceSchemes(std::vector<AppearanceSchemeName>& names)
{
	names.clear();
	HKEY key = nullptr;
	LSTATUS status = RegOpenKeyEx(HKEY_CURRENT_USER, legacySchemesKey, 0, KEY_QUERY_VALUE, &key);
	if (status == ERROR_SUCCESS)
	{
		for (DWORD index = 0;; ++index)
		{
			WCHAR value[16384];
			DWORD length = ARRAYSIZE(value), type = 0;
			status = RegEnumValue(key, index, value, &length, nullptr, &type, nullptr, nullptr);
			if (status != ERROR_SUCCESS) break;
			if (type == REG_BINARY) names.push_back({AppearanceSchemeStore::Legacy, value, L""});
		}
		RegCloseKey(key);
		if (status != ERROR_NO_MORE_ITEMS) return status;
	}
	else if (status != ERROR_FILE_NOT_FOUND) return status;
	status = RegOpenKeyEx(HKEY_CURRENT_USER, classicSchemesKey, 0, KEY_ENUMERATE_SUB_KEYS, &key);
	if (status == ERROR_FILE_NOT_FOUND) return ERROR_SUCCESS;
	if (status != ERROR_SUCCESS) return status;
	for (DWORD index = 0;; ++index)
	{
		WCHAR scheme[256];
		DWORD length = ARRAYSIZE(scheme);
		status = RegEnumKeyEx(key, index, scheme, &length, nullptr, nullptr, nullptr, nullptr);
		if (status != ERROR_SUCCESS) break;
		HKEY schemeKey = nullptr;
		status = RegOpenKeyEx(key, scheme, 0, KEY_ENUMERATE_SUB_KEYS, &schemeKey);
		if (status != ERROR_SUCCESS) break;
		for (DWORD sizeIndex = 0;; ++sizeIndex)
		{
			WCHAR size[256];
			length = ARRAYSIZE(size);
			status = RegEnumKeyEx(schemeKey, sizeIndex, size, &length, nullptr, nullptr, nullptr, nullptr);
			if (status != ERROR_SUCCESS) break;
			names.push_back({AppearanceSchemeStore::Classic, scheme, size});
		}
		RegCloseKey(schemeKey);
		if (status != ERROR_NO_MORE_ITEMS) break;
	}
	RegCloseKey(key);
	return status == ERROR_NO_MORE_ITEMS ? ERROR_SUCCESS : status;
}

DWORD AppearanceSchemeExists(const AppearanceSchemeName& name)
{
	if (name.store == AppearanceSchemeStore::Legacy)
	{
		DWORD bytes = 0;
		return RegGetValue(HKEY_CURRENT_USER, legacySchemesKey, name.name.c_str(),
			RRF_RT_ANY, nullptr, nullptr, &bytes);
	}
	if (name.store != AppearanceSchemeStore::Classic) return ERROR_INVALID_PARAMETER;
	HKEY key = nullptr;
	LSTATUS status = RegOpenKeyEx(HKEY_CURRENT_USER, ClassicSchemePath(name, false).c_str(),
		0, KEY_QUERY_VALUE, &key);
	if (status == ERROR_SUCCESS) RegCloseKey(key);
	return status;
}

DWORD DeleteAppearanceScheme(const AppearanceSchemeName& name)
{
	HKEY key = nullptr;
	if (name.store == AppearanceSchemeStore::Legacy)
	{
		LSTATUS status = RegOpenKeyEx(HKEY_CURRENT_USER, legacySchemesKey, 0, KEY_SET_VALUE, &key);
		if (status == ERROR_SUCCESS)
		{
			status = RegDeleteValue(key, name.name.c_str());
			RegCloseKey(key);
		}
		return status;
	}
	if (name.store != AppearanceSchemeStore::Classic || name.name.empty() || name.size.empty())
		return ERROR_INVALID_PARAMETER;
	std::wstring path = ClassicSchemePath(name, false);
	LSTATUS status = RegOpenKeyEx(HKEY_CURRENT_USER, path.c_str(), 0, KEY_READ | KEY_WRITE, &key);
	if (status != ERROR_SUCCESS) return status;
	status = RegDeleteTree(key, name.size.c_str());
	DWORD subkeys = 0;
	if (status == ERROR_SUCCESS)
	{
		std::wstring selectedSize;
		if (ReadRegistryString(path.c_str(), L"SelectedSize", selectedSize) == ERROR_SUCCESS &&
			lstrcmpi(selectedSize.c_str(), name.size.c_str()) == 0)
			RegDeleteValue(key, L"SelectedSize");
		status = RegQueryInfoKey(key, nullptr, nullptr, nullptr, &subkeys, nullptr, nullptr,
			nullptr, nullptr, nullptr, nullptr, nullptr);
	}
	RegCloseKey(key);
	if (status == ERROR_SUCCESS && subkeys == 0)
		status = RegDeleteTree(HKEY_CURRENT_USER, path.c_str());
	return status;
}

DWORD GetCurrentAppearanceScheme(AppearanceSchemeName& name)
{
	name = {};
	DWORD status = ReadRegistryString(appearanceKey, L"Current", name.name);
	if (status == ERROR_SUCCESS && !name.name.empty()) return ERROR_SUCCESS;
	if (status != ERROR_SUCCESS && status != ERROR_FILE_NOT_FOUND) return status;
	name.store = AppearanceSchemeStore::Classic;
	status = ReadRegistryString(classicSchemesKey, L"SelectedScheme", name.name);
	if (status == ERROR_FILE_NOT_FOUND) { name = {}; return ERROR_SUCCESS; }
	if (status != ERROR_SUCCESS) return status;
	std::wstring path = ClassicSchemePath(name, false);
	status = ReadRegistryString(path.c_str(), L"SelectedSize", name.size);
	if (status == ERROR_FILE_NOT_FOUND)
	{
		HKEY key = nullptr;
		status = RegOpenKeyEx(HKEY_CURRENT_USER, path.c_str(), 0, KEY_ENUMERATE_SUB_KEYS, &key);
		if (status == ERROR_SUCCESS)
		{
			WCHAR size[256];
			DWORD length = ARRAYSIZE(size);
			status = RegEnumKeyEx(key, 0, size, &length, nullptr, nullptr, nullptr, nullptr);
			if (status == ERROR_SUCCESS) name.size = size;
			RegCloseKey(key);
		}
	}
	if (status == ERROR_FILE_NOT_FOUND || status == ERROR_NO_MORE_ITEMS) { name = {}; return ERROR_SUCCESS; }
	return status;
}

DWORD SetCurrentAppearanceScheme(const AppearanceSchemeName& name)
{
	// Use the native selection values for each store. Clearing the other store's
	// selection prevents identical names from resolving to the wrong source.
	const bool classic = name.store == AppearanceSchemeStore::Classic && !name.name.empty();
	const bool legacy = name.store == AppearanceSchemeStore::Legacy && !name.name.empty();
	DWORD status = WriteRegistryString(appearanceKey, L"Current", legacy ? name.name : std::wstring());
	if (status != ERROR_SUCCESS) return status;
	if (classic)
	{
		status = WriteRegistryString(ClassicSchemePath(name, false).c_str(), L"SelectedSize", name.size);
		if (status != ERROR_SUCCESS) return status;
	}
	return WriteRegistryString(classicSchemesKey, L"SelectedScheme", classic ? name.name : std::wstring());
}

DWORD ApplyAppearanceScheme(const SCHEMEDATA& scheme)
{
	NONCLIENTMETRICSW previousMetrics = {sizeof(previousMetrics)};
	LOGFONTW previousIconFont = {};
	if (!SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(previousMetrics), &previousMetrics, 0) ||
		!SystemParametersInfo(SPI_GETICONTITLELOGFONT, sizeof(previousIconFont), &previousIconFont, 0))
		return SpiError();
	NONCLIENTMETRICSW metrics = {};
	memcpy(&metrics, &scheme.ncm, sizeof(scheme.ncm));
	metrics.cbSize = sizeof(metrics);
	UINT dpi = GetDpiForSystem();
	ScaleNonClientMetrics(metrics, dpi);
	ScaleFonts(metrics, dpi);
	metrics.iPaddedBorderWidth = MulDiv(scheme.iPaddedBorderWidth, dpi, 96);
	LOGFONTW iconFont = scheme.lfIconTitle;
	ScaleLogFont(iconFont, dpi);
	int indices[MAX_COLORS];
	COLORREF previousColors[MAX_COLORS];
	for (int i = 0; i < MAX_COLORS; ++i) { indices[i] = i; previousColors[i] = GetSysColor(i); }
	const UINT flags = SPIF_UPDATEINIFILE | SPIF_SENDCHANGE;
	DWORD error = ERROR_SUCCESS;
	if (!SystemParametersInfo(SPI_SETNONCLIENTMETRICS, sizeof(metrics), &metrics, flags) ||
		!SystemParametersInfo(SPI_SETICONTITLELOGFONT, sizeof(iconFont), &iconFont, flags) ||
		!SetSysColors(MAX_COLORS, indices, scheme.rgb))
		error = SpiError();
	if (error != ERROR_SUCCESS)
	{
		SystemParametersInfo(SPI_SETNONCLIENTMETRICS, sizeof(previousMetrics), &previousMetrics, flags);
		SystemParametersInfo(SPI_SETICONTITLELOGFONT, sizeof(previousIconFont), &previousIconFont, flags);
		SetSysColors(MAX_COLORS, indices, previousColors);
		return error;
	}
	// SetSysColors changes the live session only; persist the documented color
	// strings too, as the original Windows 2000 applet did.
	static const WCHAR* names[MAX_COLORS] = {
		L"Scrollbar", L"Background", L"ActiveTitle", L"InactiveTitle", L"Menu", L"Window",
		L"WindowFrame", L"MenuText", L"WindowText", L"TitleText", L"ActiveBorder", L"InactiveBorder",
		L"AppWorkspace", L"Hilight", L"HilightText", L"ButtonFace", L"ButtonShadow", L"GrayText",
		L"ButtonText", L"InactiveTitleText", L"ButtonHilight", L"ButtonDkShadow", L"ButtonLight",
		L"InfoText", L"InfoWindow", L"ButtonAlternateFace", L"HotTrackingColor",
		L"GradientActiveTitle", L"GradientInactiveTitle"
	};
	HKEY key = nullptr;
	LSTATUS status = RegCreateKeyEx(HKEY_CURRENT_USER, L"Control Panel\\Colors", 0, nullptr,
		0, KEY_SET_VALUE, nullptr, &key, nullptr);
	if (status != ERROR_SUCCESS) return status;
	for (int i = 0; i < MAX_COLORS && status == ERROR_SUCCESS; ++i)
	{
		WCHAR value[16];
		StringCchPrintf(value, ARRAYSIZE(value), L"%u %u %u",
			GetRValue(scheme.rgb[i]), GetGValue(scheme.rgb[i]), GetBValue(scheme.rgb[i]));
		status = RegSetValueEx(key, names[i], 0, REG_SZ, reinterpret_cast<const BYTE*>(value),
			(lstrlen(value) + 1) * sizeof(WCHAR));
	}
	RegCloseKey(key);
	return status;
}
