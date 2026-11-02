#pragma once
#include "pch.h"
#include "AppearanceDlgBox.h"
#include "cscheme.h"

class CAppearanceDlgProc : public WTL::CPropertyPageImpl<CAppearanceDlgProc>
{
public:
	enum { IDD = IDD_APPEARANCEDLG };
	BEGIN_MSG_MAP(CAppearanceDlgProc)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		COMMAND_HANDLER(1114, CBN_SELCHANGE, OnScheme)
		COMMAND_HANDLER(1117, BN_CLICKED, OnSave)
		COMMAND_HANDLER(1118, BN_CLICKED, OnDelete)
		COMMAND_RANGE_HANDLER(1126, 1141, OnEditor)
		CHAIN_MSG_MAP(WTL::CPropertyPageImpl<CAppearanceDlgProc>)
	END_MSG_MAP()
	int OnSetActive();
	int OnApply();
	void OnReset();

private:
	LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnScheme(WORD, WORD, HWND, BOOL&);
	LRESULT OnSave(WORD, WORD, HWND, BOOL&);
	LRESULT OnDelete(WORD, WORD, HWND, BOOL&);
	LRESULT OnEditor(WORD, WORD, HWND, BOOL&);
	void Error(UINT operation, DWORD error);
	void PopulateSchemes();
	void SelectSchemeName();
	void Modified();
	CAppearanceDlgBox editor;
	SCHEMEDATA scheme = {};
	AppearanceSchemeName schemeName;
	std::wstring lastName;
	std::vector<AppearanceSchemeName> registryNames;
	COLORREF appliedDesktopColor = 0;
	ICONMETRICSW appliedIconMetrics = {sizeof(ICONMETRICSW)};
	int appliedIconSize = 32;
	bool initialized = false;
	bool dirty = false;
};
