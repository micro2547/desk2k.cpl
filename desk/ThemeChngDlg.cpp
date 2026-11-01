#include "pch.h"
#include "ThemeChngDlg.h"
#include "desk.h"

BOOL CThemeChngDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	_wndQuestion = GetDlgItem(1830);
	_wndTimerText = GetDlgItem(1831);
	_deadline = GetTickCount64() + 15000;
	::SendMessage(_wndQuestion, STM_SETICON,
		reinterpret_cast<WPARAM>(LoadIcon(nullptr, IDI_QUESTION)), 0);
	CenterWindow(GetParent());
	UpdateCountdown();
	if (!SetTimer(67, 200)) EndDialog(IDNO);
	return TRUE;
}

void CThemeChngDlg::UpdateCountdown()
{
	ULONGLONG now = GetTickCount64();
	if (now >= _deadline)
	{
		KillTimer(67);
		EndDialog(IDNO);
		return;
	}
	WCHAR format[128] = {}, text[128] = {};
	LoadString(g_hinst, 2640, format, ARRAYSIZE(format));
	StringCchPrintf(text, ARRAYSIZE(text), format,
		static_cast<int>((_deadline - now + 999) / 1000));
	::SetWindowText(_wndTimerText, text);
}

BOOL CThemeChngDlg::OnTimer(UINT, WPARAM timer, LPARAM, BOOL&)
{
	if (timer == 67) UpdateCountdown();
	return 0;
}

LRESULT CThemeChngDlg::OnYes(UINT, int, HWND, BOOL&)
{
	KillTimer(67);
	EndDialog(IDYES);
	return 0;
}

LRESULT CThemeChngDlg::OnNo(UINT, int, HWND, BOOL&)
{
	KillTimer(67);
	EndDialog(IDNO);
	return 0;
}

void CThemeChngDlg::OnClose()
{
	KillTimer(67);
	EndDialog(IDNO);
}