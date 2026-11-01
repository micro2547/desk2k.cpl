#include "pch.h"
#include "BackgroundPage.h"
#include "ScreensaverPage.h"
#include "AppearancePage.h"
#include "SettingsPage.h"
#include "EffectsDlg.h"
#include "WebPage.h"
#include "desk.h"
#include "helper.h"
#include "uxtheme.h"

HINSTANCE g_hinst;
IDesktopWallpaper* pDesktopWallpaper = NULL;
ULONG_PTR gdiplusToken;

static THEMEINFO themeState;
THEMEINFO* selectedTheme = &themeState;
PROCESS_INFORMATION pi = {};

namespace
{
	class DialogFonts;
	thread_local DialogFonts* activeDialogFonts = nullptr;

	// Keep these handles alive for the whole sheet, including its common dialogs.
	class DialogFonts
	{
	public:
		DialogFonts() : previous(activeDialogFonts)
		{
			activeDialogFonts = this;
			hook = SetWindowsHookExW(WH_CALLWNDPROCRET, DialogReturned, nullptr, GetCurrentThreadId());
		}

		~DialogFonts()
		{
			if (hook) UnhookWindowsHookEx(hook);
			activeDialogFonts = previous;
			for (HFONT font : fonts)
				if (font) DeleteObject(font);
		}

		void Apply(HWND window)
		{
			if (!dpi) dpi = GetDpiForWindow(window);
			SetFont(window);
			EnumChildWindows(window, SetChildFont, reinterpret_cast<LPARAM>(this));
		}

	private:
		void SetFont(HWND window)
		{
			LOGFONTW current = {};
			GetObjectW(reinterpret_cast<HFONT>(SendMessageW(window, WM_GETFONT, 0, 0)),
				sizeof(current), &current);
			const int style = (current.lfWeight >= FW_BOLD ? 1 : 0) | (current.lfItalic ? 2 : 0);
			if (!fonts[style])
			{
				LOGFONTW font = {};
				font.lfHeight = -MulDiv(8, dpi ? dpi : 96, 72);
				font.lfWeight = style & 1 ? FW_BOLD : FW_NORMAL;
				font.lfItalic = (style & 2) != 0;
				font.lfCharSet = DEFAULT_CHARSET;
				StringCchCopyW(font.lfFaceName, ARRAYSIZE(font.lfFaceName), L"MS Sans Serif");
				fonts[style] = CreateFontIndirectW(&font);
			}
			if (fonts[style]) SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(fonts[style]), TRUE);
		}

		static BOOL CALLBACK SetChildFont(HWND window, LPARAM context)
		{
			reinterpret_cast<DialogFonts*>(context)->SetFont(window);
			return TRUE;
		}

		static LRESULT CALLBACK DialogReturned(int code, WPARAM wParam, LPARAM lParam)
		{
			if (code >= 0 && activeDialogFonts)
			{
				const auto message = reinterpret_cast<const CWPRETSTRUCT*>(lParam);
				if (message->message == WM_INITDIALOG)
					activeDialogFonts->Apply(message->hwnd);
			}
			return CallNextHookEx(nullptr, code, wParam, lParam);
		}

		DialogFonts* previous;
		HHOOK hook = nullptr;
		HFONT fonts[4] = {};
		int dpi = 0;
	};
}


class CDisplaySheet : public WTL::CPropertySheetImpl<CDisplaySheet>
{
public:
	explicit CDisplaySheet(LPCWSTR title) : WTL::CPropertySheetImpl<CDisplaySheet>(title) {}

	void OnSheetInitialized()
	{
		if (activeDialogFonts) activeDialogFonts->Apply(m_hWnd);
	}
};


void PropertySheetMoment(HWND owner, LPWSTR lpCmdLine)
{
	// COM belongs to the calling UI thread, never the loader lock.
	const HRESULT apartment = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	INITCOMMONCONTROLSEX controls = { sizeof(controls),
		ICC_WIN95_CLASSES | ICC_UPDOWN_CLASS | ICC_BAR_CLASSES };
	InitCommonControlsEx(&controls);
	Gdiplus::GdiplusStartupInput startup;
	if (Gdiplus::GdiplusStartup(&gdiplusToken, &startup, nullptr) != Gdiplus::Ok)
	{
		if (SUCCEEDED(apartment)) CoUninitialize();
		return;
	}

	themeState = THEMEINFO{};
	themeState.newColor = 0xB0000000;
	themeState.useDesktopColor = true;
	CoCreateInstance(CLSID_DesktopWallpaper, nullptr, CLSCTX_ALL,
		IID_PPV_ARGS(&pDesktopWallpaper));
	InitUxtheme();

	WCHAR title[128] = {};
	LoadStringW(g_hinst, IDS_DISPLAY_TITLE, title, ARRAYSIZE(title));
	{
		DialogFonts dialogFonts;
		CDisplaySheet sheet(title);
		sheet.m_psh.dwFlags |= PSH_USEICONID;
		sheet.m_psh.pszIcon = MAKEINTRESOURCEW(IDI_ICON1);
		CBackgroundDlgProc background;
		CScrSaverDlgProc saver;
		CAppearanceDlgProc appearance;
		CWebPage web;
		CEffectsDlg effects;
		CSettingsDlgProc settings;
		sheet.AddPage(background);
		sheet.AddPage(saver);
		sheet.AddPage(appearance);
		sheet.AddPage(web);
		sheet.AddPage(effects);
		sheet.AddPage(settings);
		if (lpCmdLine && *lpCmdLine)
		{
			WCHAR* end = nullptr;
			const long index = wcstol(lpCmdLine, &end, 10);
			if (end != lpCmdLine && index >= 0 && index < 6)
				sheet.SetActivePage(static_cast<int>(index));
		}
		sheet.DoModal(owner);
	}

	_TerminateProcess(pi);
	if (pDesktopWallpaper) { pDesktopWallpaper->Release(); pDesktopWallpaper = nullptr; }
	Gdiplus::GdiplusShutdown(gdiplusToken);
	if (SUCCEEDED(apartment)) CoUninitialize();
}

extern "C" LONG APIENTRY CPlApplet(
	HWND hwndCPL,			// handle of Control Panel window
	UINT uMsg,				// message
	LONG_PTR lParam1,       // first message parameter
	LONG_PTR lParam2        // second message parameter
)
{
	LPCPLINFO lpCPlInfo;
	LONG retCode = 0;

	switch (uMsg)
	{
	case CPL_INIT:
		return TRUE;

	case CPL_GETCOUNT:
		return 1L;

	case CPL_INQUIRE:
		lpCPlInfo = (LPCPLINFO)lParam2;
		lpCPlInfo->idIcon = IDI_ICON1;
		lpCPlInfo->idName = IDS_THEMESCPL;
		lpCPlInfo->idInfo = IDS_THEMESDESC;
		return (LONG)TRUE;

	case CPL_DBLCLK:
		lParam2 = 0L;
		// fall through
	case CPL_STARTWPARMS:
		PropertySheetMoment(hwndCPL, (LPWSTR)lParam2);
		return (LONG)TRUE;
	}
	return retCode;
}
