#pragma once
#include "pch.h"
#include "MonitorArrangement.h"

class CSettingsDlgProc : public WTL::CPropertyPageImpl<CSettingsDlgProc>
{
public:
	enum { IDD = IDD_SETTINGSDLG };
	int OnApply();
	int OnSetActive();

private:
	struct Display
	{
		std::wstring device;
		std::wstring name;
		DEVMODE original = {};
		DEVMODE registry = {};
		bool registryKnown = false;
		DEVMODE baseline = {};
		DEVMODE pending = {};
		DWORD colorDepth = 0;
		DWORD originalColorDepth = 0;
		std::vector<DEVMODE> modes;
		bool originalAttached = false;
		bool attached = false;
		bool originalPrimary = false;
		bool primary = false;
	};

	BEGIN_MSG_MAP(CSettingsDlgProc)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		MESSAGE_HANDLER(WM_HSCROLL, OnHScroll)
		MESSAGE_HANDLER(WM_DISPLAYCHANGE, OnDisplayChange)
		MESSAGE_HANDLER(WM_TIMER, OnTimer)
		MESSAGE_HANDLER(WM_NOTIFY, OnNotify)
		MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
		MESSAGE_HANDLER(WM_SYSCOLORCHANGE, OnSysColorChange)
		COMMAND_HANDLER(1800, CBN_SELCHANGE, OnDisplaySelect)
		COMMAND_HANDLER(1807, CBN_SELCHANGE, OnColorChange)
		COMMAND_ID_HANDLER(1806, OnPrimary)
		COMMAND_ID_HANDLER(1805, OnExtend)
		COMMAND_ID_HANDLER(1822, OnIdentify)
		COMMAND_ID_HANDLER(1802, OnAdvanced)
		COMMAND_ID_HANDLER(1821, OnTroubleshoot)
		CHAIN_MSG_MAP(WTL::CPropertyPageImpl<CSettingsDlgProc>)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnHScroll(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnDisplayChange(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnTimer(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnNotify(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnDrawItem(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnSysColorChange(UINT, WPARAM, LPARAM, BOOL&);
	LRESULT OnDisplaySelect(WORD, WORD, HWND, BOOL&);
	LRESULT OnColorChange(WORD, WORD, HWND, BOOL&);
	LRESULT OnPrimary(WORD, WORD, HWND, BOOL&);
	LRESULT OnExtend(WORD, WORD, HWND, BOOL&);
	LRESULT OnIdentify(WORD, WORD, HWND, BOOL&);
	LRESULT OnAdvanced(WORD, WORD, HWND, BOOL&);
	LRESULT OnTroubleshoot(WORD, WORD, HWND, BOOL&);
	static bool Enumerate(std::vector<Display>& displays, bool modes);
	void ReadDisplays();
	void CheckExternalChanges();
	bool Dirty() const;
	bool ColorsDirty() const;
	bool SaveColors();
	void Changed();
	void UpdateControls();
	void UpdateLayout();
	void UpdatePresentation();
	void BuildColors();
	void UpdatePreview();
	std::vector<DEVMODE> OrientedModes(const Display& display, DWORD orientation) const;
	bool UpdateMode(DWORD width, DWORD height, DWORD depth, DWORD orientation, bool exactDepth);
	bool TestMode(const Display& display, const DEVMODE& mode);
	void SetPrimary(size_t index);
	void SetAttached(size_t index, bool attached);
	void MoveDisplay(size_t index, POINT position);
	void SnapDisplay(size_t index, POINT position);
	void NormalizeOrigin();
	bool ValidateTopology();
	LONG ApplyConfiguration(bool restore, bool changedOnly);
	void Error(UINT stringId);
	void ErrorText(const WCHAR* text);
	void DriverError(const WCHAR* operation, const Display& display, LONG result);
	void CloseIdentify();

	std::vector<Display> _displays;
	std::vector<MonitorLayoutItem> _layout;
	std::vector<SIZE> _resolutions;
	std::vector<HWND> _identifyWindows;
	HFONT _identifyFont = nullptr;
	CMonitorArrangement _arrangement;
	size_t _selected = 0;
	HBITMAP _colorBitmap = nullptr;
	HBITMAP _monitorBitmap = nullptr;
	HBITMAP _previewBitmap = nullptr;
	SIZE _previewResolution = {};
	bool _singleMonitor = false;
	bool _presentationInitialized = false;
	bool _applying = false;
	bool _externalChanged = false;
	bool _enumerationFailed = false;
};
