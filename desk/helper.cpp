#include "pch.h"
#include "helper.h"
#include "desk.h"
#include "uxtheme.h"

VOID _TerminateProcess(PROCESS_INFORMATION& hp)
{
	if (hp.hProcess != nullptr)
	{
		TerminateProcess(hp.hProcess, 0);
		CloseHandle(hp.hThread);
		CloseHandle(hp.hProcess);
		hp.hProcess = nullptr;
		hp.hThread = nullptr;
	}
}

COLORREF GetDeskopColor()
{
	if (selectedTheme && selectedTheme->newColor != 0xB0000000)
		return selectedTheme->newColor;
	return GetSysColor(COLOR_DESKTOP);
}


void EnumDir(LPCWSTR directory, LPCWSTR* extensions, int cExtensions, std::vector<LPWSTR>& vec, BOOL fEnumChildDirs)
{
	WCHAR path[MAX_PATH];
	StringCchPrintf(path, ARRAYSIZE(path), L"%s\\*", directory);

	WIN32_FIND_DATAW data = { 0 };
	// FindExInfoBasic is faster?? according to msdn
	HANDLE hFind = FindFirstFileEx(path, FindExInfoBasic, &data, FindExSearchNameMatch, NULL, 0);
	if (hFind == INVALID_HANDLE_VALUE) return;

	do
	{
		if (lstrcmp(data.cFileName, L"."))
		{
			if (lstrcmp(data.cFileName, L".."))
			{
				WCHAR fullPath[MAX_PATH];
				StringCchPrintf(fullPath, ARRAYSIZE(fullPath), L"%s\\%s", directory, data.cFileName);

				if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && fEnumChildDirs)
				{
					EnumDir(fullPath, extensions, cExtensions, vec, fEnumChildDirs);
				}
				else
				{
					for (int i = 0; i < cExtensions; ++i)
					{
						if (lstrcmp(PathFindExtension(data.cFileName), extensions[i]) == 0)
						{
							vec.push_back(_wcsdup(fullPath));
						}
					}
				}
			}
		}
	} while (FindNextFileW(hFind, &data));
	FindClose(hFind);
}

void FreeBitmap(Gdiplus::Bitmap** bmp)
{
	if (*bmp)
	{
		delete *bmp;
		*bmp = nullptr;
	}
}

HRESULT DrawBitmapIfNotNull(Gdiplus::Bitmap* bmp, Gdiplus::Graphics* graph, Gdiplus::Rect rect)
{
	if (bmp != nullptr)
	{
		return graph->DrawImage(bmp, rect) == Gdiplus::Ok ? S_OK : E_FAIL;
	}
	return E_FAIL;
}

HTHEME OpenNcThemeData(LPVOID file, LPCWSTR pszClassList)
{
	return file ? OpenThemeDataFromFile(file, NULL, pszClassList, 0) : OpenThemeData(NULL, pszClassList);
}

NTSTATUS _OpenThemeSection(ACCESS_MASK mask, HANDLE* hSection)
{
	DWORD sessionId;
	ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);

	WCHAR szNtSection[MAX_PATH];
	StringCchPrintf(szNtSection, ARRAYSIZE(szNtSection), L"\\Sessions\\%lu\\Windows\\ThemeSection", sessionId);

	UNICODE_STRING szNtString;
	RtlInitUnicodeString(&szNtString, szNtSection);

	OBJECT_ATTRIBUTES objAttributes;
	InitializeObjectAttributes(&objAttributes, &szNtString, OBJ_CASE_INSENSITIVE, NULL, NULL);

	return NtOpenSection(hSection, mask, &objAttributes);
}

// ugh
BOOL IsClassicThemeEnabled()
{
	if (GetThemeAppProperties() == 0)return TRUE;
	HANDLE hSection;
	_OpenThemeSection(READ_CONTROL, &hSection);

	DWORD neededSize = 0;
	GetKernelObjectSecurity(hSection, DACL_SECURITY_INFORMATION, nullptr, 0, &neededSize);
	PSECURITY_DESCRIPTOR pSD = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, neededSize);
	GetKernelObjectSecurity(hSection, DACL_SECURITY_INFORMATION, pSD, neededSize, &neededSize);

	LPWSTR sddlString = nullptr;
	ConvertSecurityDescriptorToStringSecurityDescriptor(pSD, SDDL_REVISION, DACL_SECURITY_INFORMATION, &sddlString, nullptr);

	BOOL bRet = FALSE;
	if (StrStrI(sddlString, SDDL_CREATE_CHILD) == NULL)
	{
		bRet = TRUE;
	}

	LocalFree(pSD);
	LocalFree(sddlString);
	CloseHandle(hSection);
	return bRet;
}

wchar_t* strCut(wchar_t* s, const wchar_t* pattern)
{
	if (wchar_t* p = wcsstr(s, pattern))
	{
		wchar_t* q = p + lstrlen(pattern);
		while (*p++ = *q++);
	}
	return s;
}


__declspec(noinline) void ScaleNonClientMetrics(NONCLIENTMETRICS& ncm, int dpi)
{
	ncm.iScrollHeight = MulDiv(ncm.iScrollHeight, dpi, 96);
	ncm.iScrollWidth = MulDiv(ncm.iScrollWidth, dpi, 96);
	ncm.iCaptionHeight = MulDiv(ncm.iCaptionHeight, dpi, 96);
	ncm.iCaptionWidth = MulDiv(ncm.iCaptionWidth, dpi, 96);

	ScaleLogFont(ncm.lfCaptionFont, dpi);
	ncm.iSmCaptionHeight = MulDiv(ncm.iSmCaptionHeight, dpi, 96);
	ncm.iSmCaptionWidth = MulDiv(ncm.iSmCaptionWidth, dpi, 96);

	ScaleLogFont(ncm.lfSmCaptionFont, dpi);
	ncm.iMenuHeight = MulDiv(ncm.iMenuHeight, dpi, 96);
	ncm.iMenuWidth = MulDiv(ncm.iMenuWidth, dpi, 96);

	ScaleLogFont(ncm.lfMenuFont, dpi);
	ScaleLogFont(ncm.lfStatusFont, dpi);
	ScaleLogFont(ncm.lfMessageFont, dpi);
}

void UnScaleNonClientMetrics(NONCLIENTMETRICSW_2k& ncm, int dpi)
{
	ncm.iScrollHeight = MulDiv(ncm.iScrollHeight, 96, dpi);
	ncm.iScrollWidth = MulDiv(ncm.iScrollWidth, 96, dpi);
	ncm.iCaptionHeight = MulDiv(ncm.iCaptionHeight, 96, dpi);
	ncm.iCaptionWidth = MulDiv(ncm.iCaptionWidth, 96, dpi);
	ncm.lfCaptionFont.lfHeight = MulDiv(ncm.lfCaptionFont.lfHeight, 96, dpi);

	ncm.iSmCaptionHeight = MulDiv(ncm.iSmCaptionHeight, 96, dpi);
	ncm.iSmCaptionWidth = MulDiv(ncm.iSmCaptionWidth, 96, dpi);
	ncm.lfSmCaptionFont.lfHeight = MulDiv(ncm.lfSmCaptionFont.lfHeight, 96, dpi);

	ncm.iMenuHeight = MulDiv(ncm.iMenuHeight, 96, dpi);
	ncm.iMenuWidth = MulDiv(ncm.iMenuWidth, 96, dpi);

	ncm.lfMenuFont.lfHeight = MulDiv(ncm.lfMenuFont.lfHeight, 96, dpi);
	ncm.lfStatusFont.lfHeight = MulDiv(ncm.lfStatusFont.lfHeight, 96, dpi);
	ncm.lfMessageFont.lfHeight = MulDiv(ncm.lfMessageFont.lfHeight, 96, dpi);
}

void ScaleNonClientMetrics(NONCLIENTMETRICSW_2k& ncm, int dpi)
{
	ncm.iScrollHeight = MulDiv(ncm.iScrollHeight, dpi, 96);
	ncm.iScrollWidth = MulDiv(ncm.iScrollWidth, dpi, 96);
	ncm.iCaptionHeight = MulDiv(ncm.iCaptionHeight, dpi, 96);
	ncm.iCaptionWidth = MulDiv(ncm.iCaptionWidth, dpi, 96);

	ncm.iSmCaptionHeight = MulDiv(ncm.iSmCaptionHeight, dpi, 96);
	ncm.iSmCaptionWidth = MulDiv(ncm.iSmCaptionWidth, dpi, 96);

	ncm.iMenuHeight = MulDiv(ncm.iMenuHeight, dpi, 96);
	ncm.iMenuWidth = MulDiv(ncm.iMenuWidth, dpi, 96);
}

void ScaleLogFont(LOGFONT& lf, int dpi)
{
	lf.lfHeight = MulDiv(lf.lfHeight, dpi, 96);
}


BOOL ColorPicker(COLORREF clr, HWND hWnd, CHOOSECOLOR* clrOut)
{
	static COLORREF acrCustClr[16];

	CHOOSECOLOR cc = { 0 };
	cc.lStructSize = sizeof(cc);
	cc.hwndOwner = hWnd;
	cc.lpCustColors = acrCustClr;
	cc.rgbResult = clr;
	cc.Flags = CC_RGBINIT | CC_FULLOPEN;

	BOOL out = ChooseColor(&cc);
	*clrOut = cc;
	return out;
}

void SetBitmap(HWND hWnd, HBITMAP hBmp)
{
	HBITMAP hOld = Static_SetBitmap(hWnd, hBmp);
	DeleteBitmap(hOld);
}
