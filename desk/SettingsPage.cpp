#include "pch.h"
#include "SettingsPage.h"
#include "desk.h"
#include "ThemeChngDlg.h"

namespace
{
	constexpr UINT_PTR DisplayWatchTimer = 0x5344;
	constexpr UINT_PTR IdentifyTimer = 0x5345;
	constexpr WCHAR ColorsKey[] = L"Software\\Win2000DisplayProperties\\Settings\\Colors";
	void ResourceText(UINT id, WCHAR* text, int count)
	{
		LoadString(g_hinst, id, text, count);
	}

	bool SameMode(const DEVMODE& a, const DEVMODE& b)
	{
		return a.dmPelsWidth == b.dmPelsWidth && a.dmPelsHeight == b.dmPelsHeight &&
			a.dmBitsPerPel == b.dmBitsPerPel && a.dmDisplayFrequency == b.dmDisplayFrequency &&
			a.dmDisplayOrientation == b.dmDisplayOrientation && a.dmDisplayFlags == b.dmDisplayFlags &&
			a.dmPosition.x == b.dmPosition.x && a.dmPosition.y == b.dmPosition.y;
	}

	bool UsableMode(const DEVMODE& mode)
	{
		return mode.dmPelsWidth && mode.dmPelsHeight && mode.dmBitsPerPel &&
			mode.dmPelsWidth <= 32767 && mode.dmPelsHeight <= 32767;
	}

	RECT ModeRect(const DEVMODE& mode)
	{
		return { mode.dmPosition.x, mode.dmPosition.y,
			mode.dmPosition.x + static_cast<LONG>(mode.dmPelsWidth),
			mode.dmPosition.y + static_cast<LONG>(mode.dmPelsHeight) };
	}

	bool Touching(const RECT& a, const RECT& b)
	{
		return ((a.right == b.left || b.right == a.left) &&
			(std::max)(a.top, b.top) < (std::min)(a.bottom, b.bottom)) ||
			((a.bottom == b.top || b.bottom == a.top) &&
			(std::max)(a.left, b.left) < (std::min)(a.right, b.right));
	}

	DEVMODE ChangeMode(const DEVMODE& source, bool attached)
	{
		DEVMODE mode = source;
		mode.dmSize = sizeof(mode);
		mode.dmDriverExtra = 0;
		mode.dmFields = DM_POSITION | DM_PELSWIDTH | DM_PELSHEIGHT;
		if (source.dmBitsPerPel)
			mode.dmFields |= DM_BITSPERPEL;
		mode.dmFields |= source.dmFields & (DM_DISPLAYFREQUENCY | DM_DISPLAYFLAGS | DM_DISPLAYORIENTATION);
		if (attached)
			mode.dmFields |= DM_DISPLAYORIENTATION;
		else
			mode.dmPelsWidth = mode.dmPelsHeight = 0;
		return mode;
	}

BOOL RenderSettingsMonitorPreview(HBITMAP original, HBITMAP preview, SIZE resolution)
{
	if (!original || !preview || resolution.cx <= 0 || resolution.cy <= 0) return FALSE;

	BITMAP bitmap = {};
	if (!GetObject(original, sizeof(bitmap), &bitmap)) return FALSE;
	HDC source = CreateCompatibleDC(nullptr);
	HDC destination = CreateCompatibleDC(nullptr);
	if (!source || !destination)
	{
		if (source) DeleteDC(source);
		if (destination) DeleteDC(destination);
		return FALSE;
	}
	HGDIOBJ oldSource = SelectObject(source, original);
	HGDIOBJ oldDestination = SelectObject(destination, preview);
	BOOL rendered = BitBlt(destination, 0, 0, bitmap.bmWidth, bitmap.bmHeight,
		source, 0, 0, SRCCOPY);
	if (rendered)
	{
		// CMONITOR::SetScreenSize used an 800 x 600 baseline and left the
		// eight-pixel taskbar untouched. Smaller modes magnify a cropped sample;
		// larger modes shrink it into the top-left corner of the desktop.
		constexpr int screenX = 16, screenY = 17, screenWidth = 152;
		constexpr int desktopHeight = 112 - 8;
		SIZE sourceSize = { screenWidth, desktopHeight };
		SIZE destinationSize = sourceSize;
		if (resolution.cx > 800) destinationSize.cx = (std::max)(1, MulDiv(screenWidth, 800, resolution.cx));
		else if (resolution.cx < 800) sourceSize.cx = (std::max)(1, MulDiv(screenWidth, resolution.cx, 800));
		if (resolution.cy > 600) destinationSize.cy = (std::max)(1, MulDiv(desktopHeight, 600, resolution.cy));
		else if (resolution.cy < 600) sourceSize.cy = (std::max)(1, MulDiv(desktopHeight, resolution.cy, 600));

		RECT desktop = { screenX, screenY, screenX + screenWidth, screenY + desktopHeight };
		SetDCBrushColor(destination, GetPixel(source, screenX + 1, screenY + 1));
		FillRect(destination, &desktop, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
		SetStretchBltMode(destination, COLORONCOLOR);
		rendered = StretchBlt(destination, screenX, screenY, destinationSize.cx, destinationSize.cy,
			source, screenX, screenY, sourceSize.cx, sourceSize.cy, SRCCOPY);

		HGDIOBJ oldBrush = SelectObject(destination, GetSysColorBrush(COLOR_DESKTOP));
		const int fillX = screenX + screenWidth - 2, fillY = screenY + 1;
		ExtFloodFill(destination, fillX, fillY, GetPixel(destination, fillX, fillY), FLOODFILLSURFACE);

		// Match LoadMonitorBitmap's system-colored surround and rounded corners.
		SelectObject(destination, GetSysColorBrush(COLOR_3DFACE));
		ExtFloodFill(destination, 0, bitmap.bmHeight - 1,
			GetPixel(destination, 0, bitmap.bmHeight - 1), FLOODFILLSURFACE);
		SetPixel(destination, 0, 0, GetSysColor(COLOR_3DFACE));
		SetPixel(destination, bitmap.bmWidth - 1, 0, GetSysColor(COLOR_3DFACE));
		SelectObject(destination, oldBrush);
	}
	SelectObject(destination, oldDestination);
	SelectObject(source, oldSource);
	DeleteDC(destination);
	DeleteDC(source);
	return rendered;
}
}

LRESULT CSettingsDlgProc::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	if (!_arrangement.Attach(GetDlgItem(1801)))
		ErrorText(L"Windows could not initialize the monitor arrangement control.");
	_monitorBitmap = reinterpret_cast<HBITMAP>(LoadImage(g_hinst, MAKEINTRESOURCE(IDB_BITMAP1),
		IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
	if (_monitorBitmap)
	{
		BITMAP source = {};
		GetObject(_monitorBitmap, sizeof(source), &source);
		BITMAPINFO info = {};
		info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		info.bmiHeader.biWidth = source.bmWidth;
		info.bmiHeader.biHeight = -source.bmHeight;
		info.bmiHeader.biPlanes = 1;
		info.bmiHeader.biBitCount = 32;
		info.bmiHeader.biCompression = BI_RGB;
		void* pixels = nullptr;
		_previewBitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
	}
	ReadDisplays();
	if (!SetTimer(DisplayWatchTimer, 2000))
		ErrorText(L"Display changes cannot be monitored automatically. They will still be checked before applying settings.");
	return TRUE;
}

LRESULT CSettingsDlgProc::OnDestroy(UINT, WPARAM, LPARAM, BOOL&)
{
	KillTimer(DisplayWatchTimer);
	CloseIdentify();
	_arrangement.Detach();
	if (_colorBitmap) DeleteObject(_colorBitmap);
	_colorBitmap = nullptr;
	if (_previewBitmap) DeleteObject(_previewBitmap);
	if (_monitorBitmap) DeleteObject(_monitorBitmap);
	_previewBitmap = _monitorBitmap = nullptr;
	return 0;
}

int CSettingsDlgProc::OnSetActive()
{
	CheckExternalChanges();
	_previewResolution = {};
	UpdatePreview();
	return 0;
}

bool CSettingsDlgProc::Enumerate(std::vector<Display>& displays, bool modes)
{
	displays.clear();
	bool success = true;
	for (DWORD index = 0;; ++index)
	{
		DISPLAY_DEVICE adapter = { sizeof(adapter) };
		if (!EnumDisplayDevices(nullptr, index, &adapter, 0)) break;
		if (adapter.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) continue;
		bool attached = (adapter.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) != 0;
		bool connected = attached;
		std::wstring monitorName;
		for (DWORD monitorIndex = 0;; ++monitorIndex)
		{
			DISPLAY_DEVICE monitor = { sizeof(monitor) };
			if (!EnumDisplayDevices(adapter.DeviceName, monitorIndex, &monitor, 0)) break;
			if (monitor.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) continue;
			if (monitor.StateFlags & (DISPLAY_DEVICE_ACTIVE | DISPLAY_DEVICE_ATTACHED))
			{
				connected = true;
				if (monitorName.empty()) monitorName = monitor.DeviceString;
			}
		}
		if (!connected) continue;
		Display display;
		display.device = adapter.DeviceName;
		display.originalAttached = display.attached = attached;
		display.originalPrimary = display.primary = attached &&
			(adapter.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) != 0;
		display.original.dmSize = sizeof(DEVMODE);
		bool haveMode = EnumDisplaySettingsEx(adapter.DeviceName,
			attached ? ENUM_CURRENT_SETTINGS : ENUM_REGISTRY_SETTINGS, &display.original, 0) != FALSE;
		display.registry.dmSize = sizeof(DEVMODE);
		display.registryKnown = EnumDisplaySettingsEx(adapter.DeviceName, ENUM_REGISTRY_SETTINGS, &display.registry, 0) != FALSE;
		// A virtual/basic driver can expose a current mode before it has saved
		// registry settings. That optional rollback snapshot is not a capability check.
		if (attached && (!haveMode || !UsableMode(display.original))) success = false;
		if (modes)
		{
			for (DWORD modeIndex = 0;; ++modeIndex)
			{
				DEVMODE mode = {};
				mode.dmSize = sizeof(mode);
				// Use the normal driver mode list, as in the original Settings page.
				// EDS_ROTATEDMODE is optional and must not hide ordinary VM modes.
				if (!EnumDisplaySettingsEx(adapter.DeviceName, modeIndex, &mode, 0)) break;
				mode.dmDriverExtra = 0;
				if (UsableMode(mode)) display.modes.push_back(mode);
			}
			if (haveMode && UsableMode(display.original)) display.modes.push_back(display.original);
		}
		display.pending = display.original;
		if (!attached && !UsableMode(display.pending) && !display.modes.empty())
		{
			display.pending = display.modes.front();
			display.pending.dmPosition = display.original.dmPosition;
		}
		display.baseline = display.pending;
		WCHAR defaultMonitor[128] = {}, format[128] = {}, name[512] = {};
		ResourceText(2627, defaultMonitor, ARRAYSIZE(defaultMonitor));
		ResourceText(4200, format, ARRAYSIZE(format));
		StringCchPrintf(name, ARRAYSIZE(name), format,
			monitorName.empty() ? defaultMonitor : monitorName.c_str(), adapter.DeviceString);
		display.name = name;
		displays.push_back(std::move(display));
	}
	return success && std::any_of(displays.begin(), displays.end(), [](const Display& d) { return d.attached && d.primary; });
}

void CSettingsDlgProc::ReadDisplays()
{
	std::wstring selected = _selected < _displays.size() ? _displays[_selected].device : L"";
	std::vector<Display> previous;
	previous.swap(_displays);
	_enumerationFailed = !Enumerate(_displays, true);
	_externalChanged = false;
	_selected = 0;
	POINT cursor = {};
	MONITORINFOEX monitor = {};
	monitor.cbSize = sizeof(monitor);
	if (selected.empty() && GetCursorPos(&cursor) &&
		GetMonitorInfo(MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY), &monitor))
		selected = monitor.szDevice;
	ComboBox_ResetContent(GetDlgItem(1800));
	for (size_t i = 0; i < _displays.size(); ++i)
	{
		auto& display = _displays[i];
		auto saved = std::find_if(previous.begin(), previous.end(),
			[&](const Display& old) { return old.device == display.device; });
		if (saved != previous.end())
		{
			display.colorDepth = saved->colorDepth;
			display.originalColorDepth = saved->originalColorDepth;
		}
		else
		{
			DWORD depth = 0, bytes = sizeof(depth);
			if (RegGetValue(HKEY_CURRENT_USER, ColorsKey, display.device.c_str(),
				RRF_RT_REG_DWORD, nullptr, &depth, &bytes) != ERROR_SUCCESS || !depth)
				depth = display.pending.dmBitsPerPel ? display.pending.dmBitsPerPel : 32;
			display.colorDepth = display.originalColorDepth = depth;
		}
		WCHAR text[640] = {};
		StringCchPrintf(text, ARRAYSIZE(text), L"%u. %s", static_cast<unsigned>(i + 1), _displays[i].name.c_str());
		int item = ComboBox_AddString(GetDlgItem(1800), text);
		if (item >= 0) ComboBox_SetItemData(GetDlgItem(1800), item, i);
		if (_wcsicmp(selected.c_str(), _displays[i].device.c_str()) == 0) _selected = i;
	}
	UpdateControls();
	SetModified(Dirty() || ColorsDirty());
}

bool CSettingsDlgProc::Dirty() const
{
	return std::any_of(_displays.begin(), _displays.end(), [](const Display& d)
	{
		return d.originalAttached != d.attached || d.originalPrimary != d.primary ||
			!SameMode(d.baseline, d.pending);
	});
}

bool CSettingsDlgProc::ColorsDirty() const
{
	return std::any_of(_displays.begin(), _displays.end(),
		[](const Display& d) { return d.colorDepth != d.originalColorDepth; });
}

bool CSettingsDlgProc::SaveColors()
{
	if (!ColorsDirty()) return true;
	HKEY key = nullptr;
	LSTATUS status = RegCreateKeyEx(HKEY_CURRENT_USER, ColorsKey, 0, nullptr, 0,
		KEY_SET_VALUE, nullptr, &key, nullptr);
	const Display* failed = nullptr;
	if (status == ERROR_SUCCESS)
	{
		for (auto& display : _displays)
		{
			if (display.colorDepth == display.originalColorDepth) continue;
			status = RegSetValueEx(key, display.device.c_str(), 0, REG_DWORD,
				reinterpret_cast<const BYTE*>(&display.colorDepth), sizeof(display.colorDepth));
			if (status != ERROR_SUCCESS)
			{
				failed = &display;
				break;
			}
			display.originalColorDepth = display.colorDepth;
		}
		RegCloseKey(key);
	}
	if (status == ERROR_SUCCESS) return true;
	WCHAR detail[512] = {}, text[1024] = {};
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
		status, 0, detail, ARRAYSIZE(detail), nullptr);
	StringCchPrintf(text, ARRAYSIZE(text),
		L"Windows could not save the cosmetic Colors preference for %s.\n\nRegistry error %ld: %s\nApply again to retry.",
		failed ? failed->device.c_str() : ColorsKey, status, detail);
	ErrorText(text);
	return false;
}

void CSettingsDlgProc::Changed()
{
	UpdateControls();
	SetModified(Dirty() || ColorsDirty());
}

void CSettingsDlgProc::CheckExternalChanges()
{
	if (_applying || !m_hWnd) return;
	std::vector<Display> actual;
	bool valid = Enumerate(actual, false);
	bool same = valid && actual.size() == _displays.size();
	if (same)
	{
		for (const auto& saved : _displays)
		{
			auto found = std::find_if(actual.begin(), actual.end(), [&](const Display& d) { return d.device == saved.device; });
			if (found == actual.end() || found->attached != saved.originalAttached ||
				found->primary != saved.originalPrimary ||
				!SameMode(found->original, saved.original) ||
				found->registryKnown != saved.registryKnown ||
				(found->registryKnown && !SameMode(found->registry, saved.registry)))
			{
				same = false;
				break;
			}
		}
	}
	if (same) return;
	if (!Dirty()) ReadDisplays();
	else
	{
		_externalChanged = true;
		SetDlgItemText(1819, L"Display configuration changed. Apply will ask to reload.");
	}
}

LRESULT CSettingsDlgProc::OnDisplayChange(UINT, WPARAM, LPARAM, BOOL& handled)
{
	CheckExternalChanges();
	handled = FALSE;
	return 0;
}

LRESULT CSettingsDlgProc::OnTimer(UINT, WPARAM timer, LPARAM, BOOL& handled)
{
	if (timer == DisplayWatchTimer) CheckExternalChanges();
	else if (timer == IdentifyTimer) CloseIdentify();
	else handled = FALSE;
	return 0;
}

std::vector<DEVMODE> CSettingsDlgProc::OrientedModes(const Display& display, DWORD orientation) const
{
	std::vector<DEVMODE> result;
	for (auto mode : display.modes)
	{
		if ((mode.dmDisplayOrientation ^ orientation) & 1)
			std::swap(mode.dmPelsWidth, mode.dmPelsHeight);
		mode.dmDisplayOrientation = orientation;
		mode.dmPosition = display.pending.dmPosition;
		if (std::none_of(result.begin(), result.end(), [&](const DEVMODE& other) { return SameMode(mode, other); }))
			result.push_back(mode);
	}
	return result;
}

void CSettingsDlgProc::UpdateLayout()
{
	_layout.resize(_displays.size());
	for (size_t i = 0; i < _displays.size(); ++i)
	{
		_layout[i].number = static_cast<int>(i + 1);
		_layout[i].desktop = ModeRect(_displays[i].pending);
		_layout[i].attached = _displays[i].attached;
		_layout[i].primary = _displays[i].primary;
		_layout[i].orientation = _displays[i].pending.dmDisplayOrientation;
	}
}

void CSettingsDlgProc::UpdatePresentation()
{
	if (!m_hWnd) return;
	const bool single = _displays.size() == 1;
	if (_presentationInitialized && single == _singleMonitor) return;
	_singleMonitor = single;
	_presentationInitialized = true;
	for (int id : { 1800, 1801, 1805, 1806, 1819, 1822 })
		::ShowWindow(GetDlgItem(id), single ? SW_HIDE : SW_SHOWNA);
	for (int id : { 1811, 1812 })
		::ShowWindow(GetDlgItem(id), single ? SW_SHOWNA : SW_HIDE);
	SetDlgItemText(1820, single ? L"Display:" : L"&Display:");
	struct Placement { int id; RECT single; RECT multiple; };
	// Original single-monitor dialog coordinates; retain the multi-monitor template.
	static const Placement positions[] = {
		{1820, {7,122,238,9}, {7,104,238,8}},
		{1817, {7,154,115,42}, {7,130,115,43}},
		{1807, {13,166,103,80}, {13,141,103,80}},
		{1813, {13,182,103,9}, {13,158,103,9}},
		{1818, {129,154,116,42}, {129,130,116,43}},
		{1815, {135,166,15,8}, {135,140,15,8}},
		{1808, {156,164,58,17}, {156,140,58,17}},
		{1816, {219,166,21,8}, {219,140,21,8}},
		{1814, {136,182,100,10}, {136,160,100,10}},
		{1821, {129,202,56,14}, {128,202,56,14}}
	};
	for (const auto& placement : positions)
	{
		RECT rect = single ? placement.single : placement.multiple;
		rect.right += rect.left;
		rect.bottom += rect.top;
		::MapDialogRect(m_hWnd, &rect);
		::SetWindowPos(GetDlgItem(placement.id), nullptr, rect.left, rect.top,
			rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
	}
	InvalidateRect(nullptr, TRUE);
}

void CSettingsDlgProc::UpdateControls()
{
	bool available = !_enumerationFailed && _selected < _displays.size();
	UpdatePresentation();
	for (int id : { 1800, 1822, 1805, 1806, 1808 })
		::EnableWindow(GetDlgItem(id), available);
	::EnableWindow(GetDlgItem(1807), _selected < _displays.size());
	UpdateLayout();
	_arrangement.SetMonitors(_layout.data(), _layout.size(), _selected);
	if (_selected < _displays.size())
	{
		SetDlgItemText(1811, _displays[_selected].name.c_str());
		BuildColors();
		UpdatePreview();
	}
	if (!available)
	{
		SetDlgItemText(1819, L"No usable display configuration could be read.");
		return;
	}
	auto& display = _displays[_selected];
	for (int i = 0; i < ComboBox_GetCount(GetDlgItem(1800)); ++i)
		if (static_cast<size_t>(ComboBox_GetItemData(GetDlgItem(1800), i)) == _selected)
			ComboBox_SetCurSel(GetDlgItem(1800), i);
	CheckDlgButton(1806, display.primary ? BST_CHECKED : BST_UNCHECKED);
	CheckDlgButton(1805, display.attached ? BST_CHECKED : BST_UNCHECKED);
	::EnableWindow(GetDlgItem(1806), display.attached && !display.primary);
	::EnableWindow(GetDlgItem(1805), !display.primary && (!display.modes.empty() || display.attached));
	if (!_externalChanged)
		SetDlgItemText(1819, display.attached ?
			L"Drag the monitor icons to match the physical arrangement of your monitors." :
			L"To use this monitor, select Extend my Windows desktop below.");
	_resolutions.clear();
	for (const auto& mode : OrientedModes(display, display.pending.dmDisplayOrientation))
	{
		SIZE size = { static_cast<LONG>(mode.dmPelsWidth), static_cast<LONG>(mode.dmPelsHeight) };
		if (std::none_of(_resolutions.begin(), _resolutions.end(), [&](const SIZE& other)
			{ return size.cx == other.cx && size.cy == other.cy; })) _resolutions.push_back(size);
	}
	std::sort(_resolutions.begin(), _resolutions.end(), [](const SIZE& a, const SIZE& b)
		{ return a.cx == b.cx ? a.cy < b.cy : a.cx < b.cx; });
	SendDlgItemMessage(1808, TBM_SETRANGEMIN, FALSE, 0);
	SendDlgItemMessage(1808, TBM_SETRANGEMAX, TRUE, _resolutions.empty() ? 0 : _resolutions.size() - 1);
	for (size_t i = 0; i < _resolutions.size(); ++i)
		if (_resolutions[i].cx == static_cast<LONG>(display.pending.dmPelsWidth) &&
			_resolutions[i].cy == static_cast<LONG>(display.pending.dmPelsHeight))
			SendDlgItemMessage(1808, TBM_SETPOS, TRUE, i);
	::EnableWindow(GetDlgItem(1808), _resolutions.size() > 1);
}

void CSettingsDlgProc::BuildColors()
{
	auto& display = _displays[_selected];
	// Colors are a legacy visual preference, independent of driver mode support.
	std::vector<DWORD> depths = { 8, 16, 32 };
	if (std::find(depths.begin(), depths.end(), display.colorDepth) == depths.end())
		depths.push_back(display.colorDepth);
	for (const auto& mode : display.modes)
		if (std::find(depths.begin(), depths.end(), mode.dmBitsPerPel) == depths.end())
			depths.push_back(mode.dmBitsPerPel);
	std::sort(depths.begin(), depths.end());
	ComboBox_ResetContent(GetDlgItem(1807));
	for (DWORD depth : depths)
	{
		UINT stringId = depth == 4 ? 4201 : depth == 8 ? 4202 : depth == 16 ? 4203 :
			depth == 24 ? 4204 : depth == 32 ? 4205 : 4206;
		WCHAR format[128] = {}, text[128] = {};
		ResourceText(stringId, format, ARRAYSIZE(format));
		StringCchPrintf(text, ARRAYSIZE(text), format, depth);
		int item = ComboBox_AddString(GetDlgItem(1807), text);
		if (item < 0) continue;
		ComboBox_SetItemData(GetDlgItem(1807), item, depth);
		if (depth == display.colorDepth) ComboBox_SetCurSel(GetDlgItem(1807), item);
	}
}

void CSettingsDlgProc::UpdatePreview()
{
	if (_selected >= _displays.size()) return;
	auto& display = _displays[_selected];
	WCHAR format[128] = {}, text[128] = {};
	ResourceText(2661, format, ARRAYSIZE(format));
	StringCchPrintf(text, ARRAYSIZE(text), format, display.pending.dmPelsWidth, display.pending.dmPelsHeight);
	SetDlgItemText(1814, text);
	if (_singleMonitor)
	{
		SIZE resolution = { static_cast<LONG>(display.pending.dmPelsWidth), static_cast<LONG>(display.pending.dmPelsHeight) };
		if ((resolution.cx != _previewResolution.cx || resolution.cy != _previewResolution.cy) &&
			RenderSettingsMonitorPreview(_monitorBitmap, _previewBitmap, resolution))
			_previewResolution = resolution;
		::InvalidateRect(GetDlgItem(1812), nullptr, FALSE);
	}
	UINT bitmap = display.colorDepth <= 4 ? IDB_4BIT : display.colorDepth <= 8 ? IDB_8BIT :
		display.colorDepth <= 16 ? IDB_16BIT : display.colorDepth <= 24 ? IDB_24BIT : IDB_32BIT;
	RECT colors = {};
	::GetClientRect(GetDlgItem(1813), &colors);
	HBITMAP next = reinterpret_cast<HBITMAP>(LoadImage(g_hinst, MAKEINTRESOURCE(bitmap),
		IMAGE_BITMAP, colors.right, colors.bottom, LR_CREATEDIBSECTION));
	Static_SetBitmap(GetDlgItem(1813), next);
	if (_colorBitmap) DeleteObject(_colorBitmap);
	_colorBitmap = next;
}

LRESULT CSettingsDlgProc::OnSysColorChange(UINT, WPARAM, LPARAM, BOOL& handled)
{
	_previewResolution = {};
	UpdatePreview();
	handled = FALSE;
	return 0;
}

LRESULT CSettingsDlgProc::OnDrawItem(UINT, WPARAM, LPARAM lParam, BOOL& handled)
{
	auto draw = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
	if (!draw || draw->CtlID != 1812) { handled = FALSE; return 0; }
	FillRect(draw->hDC, &draw->rcItem, GetSysColorBrush(COLOR_3DFACE));
	if (!_previewBitmap) return TRUE;
	Gdiplus::Bitmap bitmap(_previewBitmap, nullptr);
	const int width = draw->rcItem.right - draw->rcItem.left;
	const int height = draw->rcItem.bottom - draw->rcItem.top;
	const double scale = (std::min)(double(width) / bitmap.GetWidth(), double(height) / bitmap.GetHeight());
	const int w = static_cast<int>(bitmap.GetWidth() * scale);
	const int h = static_cast<int>(bitmap.GetHeight() * scale);
	Gdiplus::Graphics graphics(draw->hDC);
	graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
	graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
	graphics.DrawImage(&bitmap, Gdiplus::Rect(draw->rcItem.left + (width - w) / 2,
		draw->rcItem.top + (height - h) / 2, w, h), 0, 0, bitmap.GetWidth(), bitmap.GetHeight(), Gdiplus::UnitPixel);
	return TRUE;
}

bool CSettingsDlgProc::TestMode(const Display& display, const DEVMODE& candidate)
{
	DEVMODE mode = ChangeMode(candidate, true);
	LONG result = ChangeDisplaySettingsEx(display.device.c_str(), &mode, nullptr, CDS_TEST, nullptr);
	if (result != DISP_CHANGE_SUCCESSFUL)
	{
		DriverError(L"test the requested display mode", display, result);
		return false;
	}
	return true;
}

bool CSettingsDlgProc::UpdateMode(DWORD width, DWORD height, DWORD depth, DWORD orientation, bool exactDepth)
{
	auto& display = _displays[_selected];
	// A notification for the current selection is not a mode change. Re-selecting
	// from the driver's list can otherwise replace current timing/flags with an
	// equivalent enumerated mode and incorrectly dirty the page on navigation.
	if (display.pending.dmPelsWidth == width && display.pending.dmPelsHeight == height &&
		display.pending.dmBitsPerPel == depth && display.pending.dmDisplayOrientation == orientation)
		return true;
	auto modes = OrientedModes(display, orientation);
	const DEVMODE* best = nullptr;
	DWORD bestScore = MAXDWORD;
	for (const auto& mode : modes)
	{
		if (mode.dmPelsWidth != width || mode.dmPelsHeight != height || (exactDepth && mode.dmBitsPerPel != depth)) continue;
		DWORD score = static_cast<DWORD>(abs(static_cast<int>(mode.dmDisplayFrequency) - static_cast<int>(display.pending.dmDisplayFrequency)));
		if (mode.dmBitsPerPel != depth) score += 10000 + (64 - (std::min)(mode.dmBitsPerPel, 64UL));
		if (score < bestScore) { best = &mode; bestScore = score; }
	}
	if (!best)
	{
		ErrorText(L"The display driver does not list the requested resolution, color depth and orientation combination.");
		UpdateControls();
		return false;
	}
	if (!TestMode(display, *best)) { UpdateControls(); return false; }
	display.pending = *best;
	if (display.attached) SnapDisplay(_selected, { display.pending.dmPosition.x, display.pending.dmPosition.y });
	NormalizeOrigin();
	Changed();
	return true;
}

LRESULT CSettingsDlgProc::OnDisplaySelect(WORD, WORD, HWND, BOOL&)
{
	int item = ComboBox_GetCurSel(GetDlgItem(1800));
	if (item >= 0)
	{
		size_t index = static_cast<size_t>(ComboBox_GetItemData(GetDlgItem(1800), item));
		if (index < _displays.size()) { _selected = index; UpdateControls(); }
	}
	return 0;
}

LRESULT CSettingsDlgProc::OnColorChange(WORD, WORD, HWND, BOOL&)
{
	if (_selected >= _displays.size()) return 0;
	int item = ComboBox_GetCurSel(GetDlgItem(1807));
	if (item >= 0)
	{
		_displays[_selected].colorDepth = static_cast<DWORD>(ComboBox_GetItemData(GetDlgItem(1807), item));
		Changed();
	}
	return 0;
}

LRESULT CSettingsDlgProc::OnHScroll(UINT, WPARAM, LPARAM lParam, BOOL&)
{
	if (reinterpret_cast<HWND>(lParam) != GetDlgItem(1808) || _selected >= _displays.size() || _enumerationFailed) return 0;
	int position = static_cast<int>(SendDlgItemMessage(1808, TBM_GETPOS));
	if (position >= 0 && position < static_cast<int>(_resolutions.size()))
	{
		const auto mode = _displays[_selected].pending;
		UpdateMode(_resolutions[position].cx, _resolutions[position].cy, mode.dmBitsPerPel, mode.dmDisplayOrientation, false);
	}
	return 0;
}

void CSettingsDlgProc::NormalizeOrigin()
{
	auto primary = std::find_if(_displays.begin(), _displays.end(), [](const Display& d) { return d.primary && d.attached; });
	if (primary == _displays.end()) return;
	POINTL origin = primary->pending.dmPosition;
	for (auto& display : _displays)
	{
		display.pending.dmPosition.x -= origin.x;
		display.pending.dmPosition.y -= origin.y;
	}
}

void CSettingsDlgProc::SetPrimary(size_t index)
{
	if (index >= _displays.size() || !_displays[index].attached || _enumerationFailed) return;
	for (size_t i = 0; i < _displays.size(); ++i) _displays[i].primary = i == index;
	NormalizeOrigin();
	Changed();
}

void CSettingsDlgProc::SetAttached(size_t index, bool attached)
{
	if (index >= _displays.size() || _enumerationFailed) return;
	auto& display = _displays[index];
	if (display.attached == attached) return;
	if (!attached)
	{
		if (display.primary || std::count_if(_displays.begin(), _displays.end(), [](const Display& d) { return d.attached; }) <= 1)
		{
			ErrorText(L"Assign another attached monitor as the primary monitor before detaching this monitor. At least one monitor must remain attached.");
			UpdateControls();
			return;
		}
		display.attached = false;
	}
	else
	{
		if (!UsableMode(display.pending) || display.modes.empty())
		{
			ErrorText(L"The display driver provides no usable mode for this monitor.");
			UpdateControls();
			return;
		}
		if (!TestMode(display, display.pending)) { UpdateControls(); return; }
		display.attached = true;
		SnapDisplay(index, { display.pending.dmPosition.x, display.pending.dmPosition.y });
	}
	Changed();
}

LRESULT CSettingsDlgProc::OnPrimary(WORD, WORD, HWND, BOOL&)
{
	SetPrimary(_selected);
	return 0;
}

LRESULT CSettingsDlgProc::OnExtend(WORD, WORD, HWND, BOOL&)
{
	if (_selected < _displays.size()) SetAttached(_selected, !_displays[_selected].attached);
	return 0;
}

void CSettingsDlgProc::SnapDisplay(size_t index, POINT position)
{
	// Share the full edge-candidate search with mouse and keyboard arrangement.
	// Rebuild from pending modes: a resize or attachment can invalidate the old view.
	UpdateLayout();
	const POINT snapped = MonitorArrangementGeometry::SnapPosition(
		_layout.data(), _layout.size(), index, position, 0);
	_displays[index].pending.dmPosition = { snapped.x, snapped.y };
}

void CSettingsDlgProc::MoveDisplay(size_t index, POINT position)
{
	if (index >= _displays.size() || _enumerationFailed) return;
	// Detached icon placement is also staged, but cannot change the real desktop.
	if (_displays[index].attached) SnapDisplay(index, position);
	else _displays[index].pending.dmPosition = { position.x, position.y };
	NormalizeOrigin();
	Changed();
}

LRESULT CSettingsDlgProc::OnNotify(UINT, WPARAM, LPARAM lParam, BOOL& handled)
{
	auto header = reinterpret_cast<NMHDR*>(lParam);
	if (!header || header->hwndFrom != GetDlgItem(1801) || header->code != MLN_ACTION)
	{
		handled = FALSE;
		return 0;
	}
	auto action = reinterpret_cast<MonitorLayoutNotification*>(lParam);
	if (action->index >= _displays.size() || _enumerationFailed) return 0;
	_selected = action->index;
	switch (action->action)
	{
	case MonitorLayoutAction::Select: UpdateControls(); break;
	case MonitorLayoutAction::Move: MoveDisplay(_selected, action->position); break;
	case MonitorLayoutAction::MakePrimary: SetPrimary(_selected); break;
	case MonitorLayoutAction::ToggleAttached: SetAttached(_selected, !_displays[_selected].attached); break;
	case MonitorLayoutAction::Rotate:
		if (action->orientation <= DMDO_270)
		{
			auto mode = _displays[_selected].pending;
			if ((mode.dmDisplayOrientation ^ action->orientation) & 1) std::swap(mode.dmPelsWidth, mode.dmPelsHeight);
			UpdateMode(mode.dmPelsWidth, mode.dmPelsHeight, mode.dmBitsPerPel, action->orientation, true);
		}
		break;
	}
	return 0;
}

void CSettingsDlgProc::CloseIdentify()
{
	KillTimer(IdentifyTimer);
	for (HWND window : _identifyWindows) if (::IsWindow(window)) ::DestroyWindow(window);
	_identifyWindows.clear();
	if (_identifyFont) DeleteObject(_identifyFont);
	_identifyFont = nullptr;
}

LRESULT CSettingsDlgProc::OnIdentify(WORD, WORD, HWND, BOOL&)
{
	CloseIdentify();
	std::vector<Display> actual;
	if (!Enumerate(actual, false))
	{
		ErrorText(L"Windows could not enumerate the attached monitors for identification.");
		return 0;
	}
	_identifyFont = CreateFont(-80, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		DEFAULT_PITCH | FF_SWISS, L"Arial");
	if (!_identifyFont)
	{
		ErrorText(L"Windows could not create the monitor identification font.");
		return 0;
	}
	for (size_t i = 0; i < _displays.size(); ++i)
	{
		auto found = std::find_if(actual.begin(), actual.end(), [&](const Display& d) { return d.device == _displays[i].device && d.attached; });
		if (found == actual.end()) continue;
		WCHAR text[16] = {};
		StringCchPrintf(text, ARRAYSIZE(text), L"%u", static_cast<unsigned>(i + 1));
		RECT rect = ModeRect(found->original);
		// Use the system STATIC class so no callback into the CPL can outlive this page.
		HWND overlay = CreateWindowEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
			L"STATIC", text, WS_POPUP | WS_BORDER | SS_CENTER | SS_CENTERIMAGE,
			rect.left + (rect.right - rect.left - 160) / 2,
			rect.top + (rect.bottom - rect.top - 120) / 2, 160, 120,
			m_hWnd, nullptr, g_hinst, nullptr);
		if (!overlay)
		{
			CloseIdentify();
			ErrorText(L"Windows could not create the monitor identification overlay.");
			return 0;
		}
		_identifyWindows.push_back(overlay);
		::SendMessage(overlay, WM_SETFONT, reinterpret_cast<WPARAM>(_identifyFont), FALSE);
		::ShowWindow(overlay, SW_SHOWNOACTIVATE);
	}
	if (!_identifyWindows.empty() && !SetTimer(IdentifyTimer, 2500))
	{
		CloseIdentify();
		ErrorText(L"Windows could not start the monitor identification timer.");
	}
	if (_identifyWindows.empty()) CloseIdentify();
	return 0;
}

bool CSettingsDlgProc::ValidateTopology()
{
	size_t first = _displays.size();
	unsigned primaryCount = 0;
	for (size_t i = 0; i < _displays.size(); ++i)
	{
		const auto& display = _displays[i];
		if (!display.attached)
		{
			if (display.primary) { ErrorText(L"A detached monitor cannot be primary."); return false; }
			continue;
		}
		if (first == _displays.size()) first = i;
		if (display.primary)
		{
			++primaryCount;
			if (display.pending.dmPosition.x || display.pending.dmPosition.y)
			{ ErrorText(L"The primary monitor must be at the desktop origin."); return false; }
		}
		RECT rect = ModeRect(display.pending);
		if (!UsableMode(display.pending) || rect.left < -32768 || rect.top < -32768 || rect.right > 32767 || rect.bottom > 32767)
		{ ErrorText(L"The display configuration exceeds the supported desktop coordinate range."); return false; }
		for (size_t j = 0; j < i; ++j)
		{
			RECT overlap = {}, other = ModeRect(_displays[j].pending);
			if (_displays[j].attached && IntersectRect(&overlap, &rect, &other))
			{ ErrorText(L"Attached monitors must not overlap. Move the monitor icons so their edges touch."); return false; }
		}
	}
	if (first == _displays.size() || primaryCount != 1)
	{ ErrorText(L"At least one monitor must be attached, with exactly one primary monitor."); return false; }
	std::vector<bool> connected(_displays.size(), false);
	connected[first] = true;
	bool added;
	do
	{
		added = false;
		for (size_t i = 0; i < _displays.size(); ++i)
		{
			if (!_displays[i].attached || connected[i]) continue;
			for (size_t j = 0; j < _displays.size(); ++j)
				if (connected[j] && Touching(ModeRect(_displays[i].pending), ModeRect(_displays[j].pending)))
				{ connected[i] = true; added = true; break; }
		}
	} while (added);
	for (size_t i = 0; i < _displays.size(); ++i)
		if (_displays[i].attached && !connected[i])
		{ ErrorText(L"Every attached monitor must share an edge with the connected desktop. Move the monitor icons together."); return false; }
	return true;
}

LONG CSettingsDlgProc::ApplyConfiguration(bool restore, bool changedOnly)
{
	LONG firstError = DISP_CHANGE_SUCCESSFUL;
	// Stage the target primary first, then attached devices, then detachments. No intermediate reset occurs.
	for (int pass = 0; pass < 3; ++pass)
	{
		for (const auto& display : _displays)
		{
			bool attached = restore ? display.originalAttached : display.attached;
			bool primary = restore ? display.originalPrimary : display.primary;
			if ((primary ? 0 : attached ? 1 : 2) != pass) continue;
			bool changed = display.originalAttached != display.attached || display.originalPrimary != display.primary ||
				!SameMode(display.baseline, display.pending);
			if (changedOnly && !changed) continue;
			DEVMODE mode = ChangeMode(restore ? display.original : display.pending, attached);
			LONG result = ChangeDisplaySettingsEx(display.device.c_str(), &mode, nullptr,
				CDS_UPDATEREGISTRY | CDS_NORESET | (primary ? CDS_SET_PRIMARY : 0), nullptr);
			if (result != DISP_CHANGE_SUCCESSFUL)
			{
				if (firstError == DISP_CHANGE_SUCCESSFUL) firstError = result;
				if (!restore) { DriverError(L"stage the display configuration", display, result); return result; }
				DriverError(L"restore this monitor's saved configuration", display, result);
			}
		}
	}
	// A failed restore must not reset into a partially restored topology either.
	if (firstError != DISP_CHANGE_SUCCESSFUL) return firstError;
	LONG commit = ChangeDisplaySettingsEx(nullptr, nullptr, nullptr, 0, nullptr);
	if (commit != DISP_CHANGE_SUCCESSFUL)
	{
		WCHAR text[512] = {};
		StringCchPrintf(text, ARRAYSIZE(text), L"Windows could not %s the complete display configuration (display-driver result %ld).",
			restore ? L"restore" : L"commit", commit);
		ErrorText(text);
		if (firstError == DISP_CHANGE_SUCCESSFUL) firstError = commit;
	}
	if (restore && firstError == DISP_CHANGE_SUCCESSFUL)
	{
		// Current and saved registry modes can differ (for example, a previously deferred change).
		// Restore the running desktop first, then restore readable saved preferences without another reset.
		for (const auto& display : _displays)
		{
			if (!display.registryKnown) continue;
			DEVMODE mode = ChangeMode(display.registry,
				display.registry.dmPelsWidth != 0 && display.registry.dmPelsHeight != 0);
			LONG result = ChangeDisplaySettingsEx(display.device.c_str(), &mode, nullptr,
				CDS_UPDATEREGISTRY | CDS_NORESET | (display.originalPrimary ? CDS_SET_PRIMARY : 0), nullptr);
			if (result != DISP_CHANGE_SUCCESSFUL)
			{
				DriverError(L"restore the saved registry mode", display, result);
				if (firstError == DISP_CHANGE_SUCCESSFUL) firstError = result;
			}
		}
	}
	return firstError;
}

int CSettingsDlgProc::OnApply()
{
	CheckExternalChanges();
	if (!Dirty())
	{
		if (!SaveColors()) { Changed(); return PSNRET_INVALID_NOCHANGEPAGE; }
		SetModified(FALSE);
		return PSNRET_NOERROR;
	}
	if (_enumerationFailed) return PSNRET_INVALID_NOCHANGEPAGE;
	if (_externalChanged)
	{
		if (MessageBox(L"The display configuration has changed outside this page. Discard your staged display configuration and reload the actual configuration? Your Colors choices will be kept.",
			L"Display Settings", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES) ReadDisplays();
		return PSNRET_INVALID_NOCHANGEPAGE;
	}
	if (!ValidateTopology()) return PSNRET_INVALID_NOCHANGEPAGE;
	// CDS_TEST never writes registry settings or changes the running desktop.
	for (const auto& display : _displays)
	{
		DEVMODE mode = ChangeMode(display.pending, display.attached);
		LONG result = ChangeDisplaySettingsEx(display.device.c_str(), &mode, nullptr,
			CDS_TEST | (display.primary ? CDS_SET_PRIMARY : 0), nullptr);
		if (result != DISP_CHANGE_SUCCESSFUL)
		{ DriverError(L"test the complete staged configuration", display, result); return PSNRET_INVALID_NOCHANGEPAGE; }
	}
	CloseIdentify();
	_applying = true;
	LONG result = ApplyConfiguration(false, true);
	bool keep = false;
	if (result == DISP_CHANGE_SUCCESSFUL)
	{
		CThemeChngDlg confirmation;
		keep = confirmation.DoModal(m_hWnd) == IDYES;
	}
	if (!keep)
	{
		LONG restored = ApplyConfiguration(true, false);
		_applying = false;
		if (restored != DISP_CHANGE_SUCCESSFUL)
		{
			Error(4209);
			ReadDisplays();
			return PSNRET_INVALID_NOCHANGEPAGE;
		}
		// Keep the user's staged choices available for correction after a rejection/failure.
		CheckExternalChanges();
		Changed();
		return PSNRET_INVALID_NOCHANGEPAGE;
	}
	_applying = false;
	// Accept the actual driver result, retaining detached monitors' staged preferred modes.
	std::vector<Display> actual;
	if (!Enumerate(actual, false))
	{
		ErrorText(L"Windows accepted the change but could not re-enumerate the resulting display configuration.");
		ReadDisplays();
		return PSNRET_INVALID_NOCHANGEPAGE;
	}
	for (auto& display : _displays)
	{
		auto found = std::find_if(actual.begin(), actual.end(), [&](const Display& d) { return d.device == display.device; });
		if (found == actual.end())
		{
			bool colorsSaved = SaveColors();
			ReadDisplays();
			return colorsSaved ? PSNRET_NOERROR : PSNRET_INVALID_NOCHANGEPAGE;
		}
		display.original = found->original;
		display.registry = found->registry;
		display.registryKnown = found->registryKnown;
		display.originalAttached = display.attached = found->attached;
		display.originalPrimary = display.primary = found->primary;
		if (display.attached) display.pending = found->original;
		display.baseline = display.pending;
	}
	bool colorsSaved = SaveColors();
	Changed();
	return colorsSaved ? PSNRET_NOERROR : PSNRET_INVALID_NOCHANGEPAGE;
}

void CSettingsDlgProc::Error(UINT stringId)
{
	WCHAR text[512] = {};
	ResourceText(stringId, text, ARRAYSIZE(text));
	ErrorText(text);
}

void CSettingsDlgProc::ErrorText(const WCHAR* text)
{
	WCHAR title[128] = {};
	ResourceText(2023, title, ARRAYSIZE(title));
	MessageBox(text, title, MB_OK | MB_ICONERROR);
}

void CSettingsDlgProc::DriverError(const WCHAR* operation, const Display& display, LONG result)
{
	const WCHAR* reason = result == DISP_CHANGE_BADMODE ? L"The driver does not support this mode or topology." :
		result == DISP_CHANGE_RESTART ? L"The driver requires a restart; this page cannot confirm the change safely." :
		result == DISP_CHANGE_NOTUPDATED ? L"Windows could not update the display registry settings." :
		result == DISP_CHANGE_BADDUALVIEW ? L"The driver rejects this multi-monitor configuration." :
		result == DISP_CHANGE_BADFLAGS ? L"The driver rejects the requested display-change flags." :
		result == DISP_CHANGE_BADPARAM ? L"The driver rejects the requested display parameters." :
		L"The display driver could not complete the operation.";
	WCHAR text[1024] = {};
	StringCchPrintf(text, ARRAYSIZE(text), L"Windows could not %s for %s (%s).\n\n%s\nDisplay-driver result: %ld",
		operation, display.name.c_str(), display.device.c_str(), reason, result);
	ErrorText(text);
}

LRESULT CSettingsDlgProc::OnAdvanced(WORD, WORD, HWND, BOOL&)
{
	if (reinterpret_cast<INT_PTR>(ShellExecute(m_hWnd, L"open",
		L"ms-settings:display-advanced", nullptr, nullptr, SW_SHOWNORMAL)) <= 32) Error(4210);
	return 0;
}

LRESULT CSettingsDlgProc::OnTroubleshoot(WORD, WORD, HWND, BOOL&)
{
	WCHAR system[MAX_PATH] = {}, manager[MAX_PATH] = {};
	GetSystemDirectory(system, ARRAYSIZE(system));
	StringCchPrintf(manager, ARRAYSIZE(manager), L"%s\\devmgmt.msc", system);
	if (reinterpret_cast<INT_PTR>(ShellExecute(m_hWnd, L"open",
		manager, nullptr, nullptr, SW_SHOWNORMAL)) <= 32) Error(4210);
	return 0;
}
