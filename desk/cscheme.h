#pragma once
#include "pch.h"
#include "desk.h"

extern int cs_dpi;

DWORD NcGetSysColor(int nIndex);
HBRUSH NcGetSysColorBrush(int nIndex);
BOOL NcDrawFrameControl(HDC hdc, RECT* lprc, UINT uType, int type);
int NcGetSystemMetrics(int nIndex);
void NcUpdateSystemMetrics(int nIndex, int value);
int NcGetSystemMetrics96(int nIndex);
enum class AppearanceSchemeStore { Legacy, Classic, BuiltIn };

struct AppearanceSchemeName
{
	AppearanceSchemeStore store = AppearanceSchemeStore::Legacy;
	std::wstring name;
	std::wstring size;
};

DWORD CaptureAppearanceScheme(SCHEMEDATA& scheme);
DWORD EnumerateAppearanceSchemes(std::vector<AppearanceSchemeName>& names);
DWORD LoadAppearanceScheme(const AppearanceSchemeName& name, SCHEMEDATA& scheme);
DWORD SaveAppearanceScheme(const AppearanceSchemeName& name, const SCHEMEDATA& scheme);
DWORD AppearanceSchemeExists(const AppearanceSchemeName& name);
DWORD DeleteAppearanceScheme(const AppearanceSchemeName& name);
DWORD GetCurrentAppearanceScheme(AppearanceSchemeName& name);
DWORD SetCurrentAppearanceScheme(const AppearanceSchemeName& name);
DWORD ApplyAppearanceScheme(const SCHEMEDATA& scheme);
