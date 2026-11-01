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
			bool terminated = false;
			for (WCHAR character : font->lfFaceName)
				if (character == L'\0') { terminated = true; break; }
			if (!terminated) return false;
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

DWORD LoadAppearanceScheme(LPCWSTR name, SCHEMEDATA& scheme)
{
	STORED_SCHEME stored = {};
	DWORD size = sizeof(stored), type = 0;
	LSTATUS status = RegGetValue(HKEY_CURRENT_USER, L"Control Panel\\Appearance\\Schemes",
		name, RRF_RT_REG_BINARY, &type, &stored, &size);
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

DWORD SaveAppearanceScheme(LPCWSTR name, const SCHEMEDATA& scheme)
{
	STORED_SCHEME stored = {};
	stored.version = 2;
	stored.metrics = scheme.ncm;
	stored.metrics.cbSize = sizeof(stored.metrics);
	stored.iconFont = scheme.lfIconTitle;
	memcpy(stored.colors, scheme.rgb, sizeof(stored.colors));
	HKEY key = nullptr;
	LSTATUS status = RegCreateKeyEx(HKEY_CURRENT_USER, L"Control Panel\\Appearance\\Schemes",
		0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
	if (status == ERROR_SUCCESS)
	{
		status = RegSetValueEx(key, name, 0, REG_BINARY,
			reinterpret_cast<const BYTE*>(&stored), sizeof(stored));
		RegCloseKey(key);
	}
	return status;
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
