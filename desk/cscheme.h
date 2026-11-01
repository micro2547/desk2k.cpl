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
DWORD CaptureAppearanceScheme(SCHEMEDATA& scheme);
DWORD LoadAppearanceScheme(LPCWSTR name, SCHEMEDATA& scheme);
DWORD SaveAppearanceScheme(LPCWSTR name, const SCHEMEDATA& scheme);
DWORD ApplyAppearanceScheme(const SCHEMEDATA& scheme);
