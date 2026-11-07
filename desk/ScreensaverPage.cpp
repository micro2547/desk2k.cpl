#include "pch.h"
#include "ScreensaverPage.h"
#include "desk.h"
#include "helper.h"

namespace
{
	constexpr LPCWSTR DesktopKey = L"Control Panel\\Desktop";

	BOOL CALLBACK ClosePreviewWindow(HWND window, LPARAM)
	{
		PostMessage(window, WM_CLOSE, 0, 0);
		return TRUE;
	}

	void CloseProcess(PROCESS_INFORMATION& process)
	{
		if (process.hThread) CloseHandle(process.hThread);
		if (process.hProcess) CloseHandle(process.hProcess);
		process = {};
	}
}

LRESULT CScrSaverDlgProc::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	CreateMonitor();
	_energyBitmap = reinterpret_cast<HBITMAP>(LoadImage(g_hinst, MAKEINTRESOURCE(IDB_BITMAP2),
		IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS));
	Static_SetBitmap(GetDlgItem(1305), _energyBitmap);
	SendDlgItemMessage(1307, UDM_SETBUDDY, reinterpret_cast<WPARAM>(GetDlgItem(1306).m_hWnd));
	SendDlgItemMessage(1307, UDM_SETRANGE32, 1, 9999);
	SendDlgItemMessage(1306, EM_LIMITTEXT, 4);

	WCHAR directory[MAX_PATH] = {};
	GetSystemDirectory(directory, ARRAYSIZE(directory));
	EnumerateDirectory(directory);
	GetWindowsDirectory(directory, ARRAYSIZE(directory));
	EnumerateDirectory(directory);

	WCHAR stored[32768] = {};
	DWORD bytes = sizeof(stored);
	std::wstring configured;
	if (RegGetValue(HKEY_CURRENT_USER, DesktopKey, L"SCRNSAVE.EXE",
		RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, nullptr, stored, &bytes) == ERROR_SUCCESS)
	{
		PathUnquoteSpaces(stored);
		WCHAR resolved[32768] = {};
		if (PathIsRelative(stored))
		{
			// Resolve old filename-only registrations against the Windows system directory.
			GetSystemDirectory(directory, ARRAYSIZE(directory));
			StringCchPrintf(resolved, ARRAYSIZE(resolved), L"%s\\%s", directory, stored);
		}
		else StringCchCopy(resolved, ARRAYSIZE(resolved), stored);
		configured = resolved;
		AddSaver(configured);
	}
	std::sort(_savers.begin(), _savers.end(), [](const Saver& a, const Saver& b)
		{ return lstrcmpi(a.name.c_str(), b.name.c_str()) < 0; });
	WCHAR none[128] = {};
	LoadString(g_hinst, 2022, none, ARRAYSIZE(none));
	int noneItem = ComboBox_AddString(GetDlgItem(1300), none);
	ComboBox_SetItemData(GetDlgItem(1300), noneItem, 0);
	BOOL active = FALSE, secure = FALSE;
	int timeout = 900;
	SystemParametersInfo(SPI_GETSCREENSAVEACTIVE, 0, &active, 0);
	SystemParametersInfo(SPI_GETSCREENSAVESECURE, 0, &secure, 0);
	SystemParametersInfo(SPI_GETSCREENSAVETIMEOUT, 0, &timeout, 0);
	int selected = noneItem;
	for (size_t i = 0; i < _savers.size(); ++i)
	{
		int item = ComboBox_AddString(GetDlgItem(1300), _savers[i].name.c_str());
		ComboBox_SetItemData(GetDlgItem(1300), item, i + 1);
		if (active && lstrcmpi(_savers[i].path.c_str(), configured.c_str()) == 0) selected = item;
	}
	ComboBox_SetCurSel(GetDlgItem(1300), selected);
	SendDlgItemMessage(1307, UDM_SETPOS32, 0, (std::max)(1, (std::min)(9999, (timeout + 59) / 60)));
	CheckDlgButton(1320, secure ? BST_CHECKED : BST_UNCHECKED);
	_initializing = false;
	UpdateControls();
	return TRUE;
}

void CScrSaverDlgProc::EnumerateDirectory(LPCWSTR directory)
{
	WCHAR pattern[MAX_PATH] = {};
	if (FAILED(StringCchPrintf(pattern, ARRAYSIZE(pattern), L"%s\\*.scr", directory))) return;
	WIN32_FIND_DATA data = {};
	HANDLE search = FindFirstFile(pattern, &data);
	if (search == INVALID_HANDLE_VALUE) return;
	do
	{
		if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
		WCHAR path[MAX_PATH] = {};
		if (SUCCEEDED(StringCchPrintf(path, ARRAYSIZE(path), L"%s\\%s", directory, data.cFileName)))
			AddSaver(path);
	} while (FindNextFile(search, &data));
	FindClose(search);
}

void CScrSaverDlgProc::AddSaver(const std::wstring& path)
{
	DWORD attributes = GetFileAttributes(path.c_str());
	if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) ||
		lstrcmpi(PathFindExtension(path.c_str()), L".scr") != 0) return;
	for (const auto& saver : _savers)
		if (lstrcmpi(saver.path.c_str(), path.c_str()) == 0) return;
	WCHAR name[512] = {};
	HMODULE module = LoadLibraryEx(path.c_str(), nullptr,
		LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
	if (module)
	{
		if (!LoadString(module, 1, name, ARRAYSIZE(name)))
			LoadString(module, 100, name, ARRAYSIZE(name));
		FreeLibrary(module);
	}
	if (!name[0])
	{
		StringCchCopy(name, ARRAYSIZE(name), PathFindFileName(path.c_str()));
		PathRemoveExtension(name);
	}
	_savers.push_back({ path, name });
}

const std::wstring& CScrSaverDlgProc::SelectedPath() const
{
	static const std::wstring empty;
	int item = ComboBox_GetCurSel(GetDlgItem(1300));
	if (item == CB_ERR) return empty;
	size_t index = static_cast<size_t>(ComboBox_GetItemData(GetDlgItem(1300), item));
	return index > 0 && index <= _savers.size() ? _savers[index - 1].path : empty;
}

void CScrSaverDlgProc::CreateMonitor()
{
	RECT client = {};
	::GetClientRect(GetDlgItem(1302), &client);
	Gdiplus::Bitmap monitor(g_hinst, MAKEINTRESOURCE(IDB_BITMAP1));
	if (monitor.GetLastStatus() != Gdiplus::Ok) return;
	Gdiplus::Bitmap image(client.right, client.bottom, PixelFormat32bppARGB);
	Gdiplus::Graphics graphics(&image);
	COLORREF face = GetSysColor(COLOR_3DFACE);
	graphics.Clear(Gdiplus::Color(255, GetRValue(face), GetGValue(face), GetBValue(face)));
	double scale = (std::min)(static_cast<double>(client.right) / monitor.GetWidth(),
		static_cast<double>(client.bottom) / monitor.GetHeight());
	int width = static_cast<int>(monitor.GetWidth() * scale);
	int height = static_cast<int>(monitor.GetHeight() * scale);
	int x = (client.right - width) / 2, y = (client.bottom - height) / 2;
	Gdiplus::Color mask(255, 255, 255, 0);
	Gdiplus::ImageAttributes attributes;
	attributes.SetColorKey(mask, mask);
	graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
	graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
	graphics.DrawImage(&monitor, Gdiplus::Rect(x, y, width, height),
		0, 0, monitor.GetWidth(), monitor.GetHeight(), Gdiplus::UnitPixel, &attributes);
	_screen = { x + MulDiv(16, width, monitor.GetWidth()), y + MulDiv(17, height, monitor.GetHeight()),
		x + MulDiv(168, width, monitor.GetWidth()), y + MulDiv(129, height, monitor.GetHeight()) };
	COLORREF color = GetDeskopColor();
	Gdiplus::SolidBrush desktop(Gdiplus::Color(255, GetRValue(color), GetGValue(color), GetBValue(color)));
	graphics.FillRectangle(&desktop, static_cast<int>(_screen.left), static_cast<int>(_screen.top),
		static_cast<int>(_screen.right - _screen.left), static_cast<int>(_screen.bottom - _screen.top));
	HBITMAP next = nullptr;
	if (image.GetHBITMAP(Gdiplus::Color(255, GetRValue(face), GetGValue(face), GetBValue(face)), &next) != Gdiplus::Ok)
		return;
	Static_SetBitmap(GetDlgItem(1302), next);
	if (_monitorBitmap) DeleteObject(_monitorBitmap);
	_monitorBitmap = next;
}

void CScrSaverDlgProc::UpdateControls()
{
	bool selected = !SelectedPath().empty();
	bool external = _external.hProcess != nullptr;
	for (int id : { 1303, 1304 }) ::EnableWindow(GetDlgItem(id), selected && !external);
	for (int id : { 1306, 1307, 1316, 1320, 1322 }) ::EnableWindow(GetDlgItem(id), selected);
}

bool CScrSaverDlgProc::Launch(LPCWSTR arguments, PROCESS_INFORMATION& process, bool reportError)
{
	const std::wstring& path = SelectedPath();
	if (path.empty()) return false;
	std::wstring command = L"\"" + path + L"\" " + arguments;
	STARTUPINFO startup = { sizeof(startup) };
	bool launched = CreateProcess(path.c_str(), &command[0], nullptr, nullptr,
		FALSE, 0, nullptr, nullptr, &startup, &process) != FALSE;
	if (launched)
	{
		CloseHandle(process.hThread);
		process.hThread = nullptr;
	}
	else if (reportError) Error(4250);
	return launched;
}

void CScrSaverDlgProc::StopPreview()
{
	if (_preview.hProcess)
	{
		if (_previewHost) ::EnumChildWindows(_previewHost, ClosePreviewWindow, 0);
		if (WaitForSingleObject(_preview.hProcess, 200) == WAIT_TIMEOUT)
			TerminateProcess(_preview.hProcess, 0);
		CloseProcess(_preview);
	}
	if (_previewHost)
	{
		::DestroyWindow(_previewHost);
		_previewHost = nullptr;
	}
}

void CScrSaverDlgProc::StartPreview()
{
	StopPreview();
	if (!_active || _external.hProcess || SelectedPath().empty() || IsRectEmpty(&_screen)) return;
	_previewHost = CreateWindow(WC_STATIC, nullptr, WS_CHILD | WS_VISIBLE | SS_BLACKRECT | WS_CLIPCHILDREN,
		_screen.left, _screen.top, _screen.right - _screen.left, _screen.bottom - _screen.top,
		GetDlgItem(1302), nullptr, g_hinst, nullptr);
	if (!_previewHost) return;
	WCHAR arguments[80] = {};
	StringCchPrintf(arguments, ARRAYSIZE(arguments), L"/p %llu",
		static_cast<unsigned long long>(reinterpret_cast<UINT_PTR>(_previewHost)));
	if (!Launch(arguments, _preview, false))
	{
		::DestroyWindow(_previewHost);
		_previewHost = nullptr;
	}
}

LRESULT CScrSaverDlgProc::OnSelectionChange(WORD, WORD, HWND, BOOL&)
{
	_dirty = true;
	SetModified(TRUE);
	UpdateControls();
	StartPreview();
	return 0;
}

LRESULT CScrSaverDlgProc::OnChanged(WORD, WORD, HWND, BOOL&)
{
	if (!_initializing)
	{
		_dirty = true;
		SetModified(TRUE);
	}
	return 0;
}

void CScrSaverDlgProc::StartExternal(bool fullScreen)
{
	if (_external.hProcess || SelectedPath().empty()) return;
	StopPreview();
	if (!SetTimer(82, 250))
	{
		Error(4250);
		StartPreview();
		return;
	}
	WCHAR arguments[80] = {};
	if (fullScreen) StringCchCopy(arguments, ARRAYSIZE(arguments), L"/s");
	else StringCchPrintf(arguments, ARRAYSIZE(arguments), L"/c:%llu",
		static_cast<unsigned long long>(reinterpret_cast<UINT_PTR>(m_hWnd)));
	_externalPreview = fullScreen;
	if (!Launch(arguments, _external, true))
	{
		KillTimer(82);
		StartPreview();
	}
	UpdateControls();
}

LRESULT CScrSaverDlgProc::OnSettings(WORD, WORD, HWND, BOOL&)
{
	StartExternal(false);
	return 0;
}

LRESULT CScrSaverDlgProc::OnPreview(WORD, WORD, HWND, BOOL&)
{
	StartExternal(true);
	return 0;
}

LRESULT CScrSaverDlgProc::OnTimer(UINT, WPARAM timer, LPARAM, BOOL&)
{
	if (timer == 82 && (!_external.hProcess || WaitForSingleObject(_external.hProcess, 0) != WAIT_TIMEOUT))
	{
		KillTimer(82);
		CloseProcess(_external);
		UpdateControls();
		StartPreview();
	}
	return 0;
}

int CScrSaverDlgProc::OnSetActive()
{
	_active = true;
	CreateMonitor();
	StartPreview();
	return 0;
}

LRESULT CScrSaverDlgProc::OnSysColorChange(UINT, WPARAM, LPARAM, BOOL& handled)
{
	CreateMonitor();
	handled = FALSE;
	return 0;
}

BOOL CScrSaverDlgProc::OnKillActive()
{
	_active = false;
	StopPreview();
	return FALSE;
}

LRESULT CScrSaverDlgProc::OnDestroy(UINT, WPARAM, LPARAM, BOOL&)
{
	KillTimer(82);
	StopPreview();
	if (_externalPreview && _external.hProcess && WaitForSingleObject(_external.hProcess, 0) == WAIT_TIMEOUT)
		TerminateProcess(_external.hProcess, 0);
	CloseProcess(_external);
	if (_monitorBitmap) DeleteObject(_monitorBitmap);
	if (_energyBitmap) DeleteObject(_energyBitmap);
	return 0;
}

int CScrSaverDlgProc::OnApply()
{
	if (!_dirty) return PSNRET_NOERROR;
	std::wstring path = SelectedPath();
	BOOL valid = FALSE;
	UINT minutes = GetDlgItemInt(1306, &valid, FALSE);
	if (!path.empty() && (!valid || minutes < 1 || minutes > 9999))
	{
		Error(4251);
		::SetFocus(GetDlgItem(1306));
		return PSNRET_INVALID_NOCHANGEPAGE;
	}
	if (!path.empty() && GetFileAttributes(path.c_str()) == INVALID_FILE_ATTRIBUTES)
	{
		Error(4250);
		return PSNRET_INVALID_NOCHANGEPAGE;
	}
	HKEY key = nullptr;
	if (RegCreateKeyEx(HKEY_CURRENT_USER, DesktopKey, 0, nullptr, 0,
		KEY_QUERY_VALUE | KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
	{
		Error(4252);
		return PSNRET_INVALID_NOCHANGEPAGE;
	}
	DWORD oldType = REG_SZ, oldBytes = 0;
	LSTATUS status = RegQueryValueEx(key, L"SCRNSAVE.EXE", nullptr, &oldType, nullptr, &oldBytes);
	bool existed = status == ERROR_SUCCESS;
	std::vector<BYTE> oldPath(oldBytes);
	BOOL oldActive = FALSE, oldSecure = FALSE;
	int oldTimeout = 0;
	bool snapshot = (status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND) &&
		(!existed || RegQueryValueEx(key, L"SCRNSAVE.EXE", nullptr, &oldType, oldPath.data(), &oldBytes) == ERROR_SUCCESS) &&
		SystemParametersInfo(SPI_GETSCREENSAVEACTIVE, 0, &oldActive, 0) &&
		SystemParametersInfo(SPI_GETSCREENSAVESECURE, 0, &oldSecure, 0) &&
		SystemParametersInfo(SPI_GETSCREENSAVETIMEOUT, 0, &oldTimeout, 0);
	if (!snapshot)
	{
		RegCloseKey(key);
		Error(4252);
		return PSNRET_INVALID_NOCHANGEPAGE;
	}
	status = path.empty() ? RegDeleteValue(key, L"SCRNSAVE.EXE") :
		RegSetValueEx(key, L"SCRNSAVE.EXE", 0, REG_SZ, reinterpret_cast<const BYTE*>(path.c_str()),
			static_cast<DWORD>((path.size() + 1) * sizeof(WCHAR)));
	bool success = status == ERROR_SUCCESS || (path.empty() && status == ERROR_FILE_NOT_FOUND);
	const UINT flags = SPIF_UPDATEINIFILE | SPIF_SENDCHANGE;
	if (success && !path.empty())
	{
		success = SystemParametersInfo(SPI_SETSCREENSAVESECURE,
			IsDlgButtonChecked(1320) == BST_CHECKED, nullptr, flags) != FALSE;
		if (success) success = SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, minutes * 60, nullptr, flags) != FALSE;
	}
	if (success) success = SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, !path.empty(), nullptr, flags) != FALSE;
	if (!success)
	{
		LSTATUS restoredPath = existed ? RegSetValueEx(key, L"SCRNSAVE.EXE", 0,
			oldType, oldPath.data(), oldBytes) : RegDeleteValue(key, L"SCRNSAVE.EXE");
		bool restored = restoredPath == ERROR_SUCCESS || (!existed && restoredPath == ERROR_FILE_NOT_FOUND);
		restored &= SystemParametersInfo(SPI_SETSCREENSAVESECURE, oldSecure, nullptr, flags) != FALSE;
		restored &= SystemParametersInfo(SPI_SETSCREENSAVETIMEOUT, oldTimeout, nullptr, flags) != FALSE;
		restored &= SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, oldActive, nullptr, flags) != FALSE;
		RegCloseKey(key);
		Error(restored ? 4252 : 4253);
		return PSNRET_INVALID_NOCHANGEPAGE;
	}
	RegCloseKey(key);
	_dirty = false;
	SetModified(FALSE);
	return PSNRET_NOERROR;
}

void CScrSaverDlgProc::Error(UINT id)
{
	WCHAR text[512] = {}, title[128] = {};
	LoadString(g_hinst, id, text, ARRAYSIZE(text));
	LoadString(g_hinst, 4254, title, ARRAYSIZE(title));
	MessageBox(text, title, MB_OK | MB_ICONERROR);
}

LRESULT CScrSaverDlgProc::OnPower(WORD, WORD, HWND, BOOL&)
{
	WCHAR system[MAX_PATH] = {}, control[MAX_PATH] = {}, applet[MAX_PATH] = {};
	GetSystemDirectory(system, ARRAYSIZE(system));
	StringCchPrintf(control, ARRAYSIZE(control), L"%s\\control.exe", system);
	StringCchPrintf(applet, ARRAYSIZE(applet), L"%s\\xppowerc.cpl", system);
	DWORD attributes = GetFileAttributes(applet);
	bool hasApplet = attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
	WCHAR arguments[MAX_PATH + 2] = {};
	if (hasApplet) StringCchPrintf(arguments, ARRAYSIZE(arguments), L"\"%s\"", applet);
	if (reinterpret_cast<INT_PTR>(ShellExecute(m_hWnd, L"open", control,
		hasApplet ? arguments : L"/name Microsoft.PowerOptions", nullptr, SW_SHOWNORMAL)) <= 32) Error(4255);
	return 0;
}
