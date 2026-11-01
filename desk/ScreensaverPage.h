#pragma once
#include "pch.h"

class CScrSaverDlgProc : public WTL::CPropertyPageImpl<CScrSaverDlgProc>
{
public:
	enum { IDD = IDD_SCRSVRDLG };
	int OnApply();
	int OnSetActive();
	BOOL OnKillActive();

private:
	struct Saver
	{
		std::wstring path;
		std::wstring name;
	};
	BEGIN_MSG_MAP(CScrSaverDlgProc)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_SYSCOLORCHANGE, OnSysColorChange)
		COMMAND_HANDLER(1300, CBN_SELCHANGE, OnSelectionChange)
		COMMAND_HANDLER(1303, BN_CLICKED, OnSettings)
		COMMAND_HANDLER(1304, BN_CLICKED, OnPreview)
		COMMAND_HANDLER(1314, BN_CLICKED, OnPower)
		COMMAND_HANDLER(1320, BN_CLICKED, OnChanged)
		COMMAND_HANDLER(1306, EN_CHANGE, OnChanged)
		CHAIN_MSG_MAP(WTL::CPropertyPageImpl<CScrSaverDlgProc>)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnTimer(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnSysColorChange(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnSelectionChange(WORD, WORD, HWND, BOOL&);
	LRESULT OnSettings(WORD, WORD, HWND, BOOL&);
	LRESULT OnPreview(WORD, WORD, HWND, BOOL&);
	LRESULT OnPower(WORD, WORD, HWND, BOOL&);
	LRESULT OnChanged(WORD, WORD, HWND, BOOL&);
	void AddSaver(const std::wstring& path);
	void EnumerateDirectory(LPCWSTR directory);
	void UpdateControls();
	void CreateMonitor();
	void StartPreview();
	void StopPreview();
	bool Launch(LPCWSTR arguments, PROCESS_INFORMATION& process, bool reportError);
	void StartExternal(bool fullScreen);
	void Error(UINT id);
	const std::wstring& SelectedPath() const;

	std::vector<Saver> _savers;
	HWND _previewHost = nullptr;
	RECT _screen = {};
	HBITMAP _monitorBitmap = nullptr;
	HBITMAP _energyBitmap = nullptr;
	PROCESS_INFORMATION _preview = {};
	PROCESS_INFORMATION _external = {};
	bool _externalPreview = false;
	bool _initializing = true;
	bool _active = false;
	bool _dirty = false;
};