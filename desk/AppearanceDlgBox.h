#pragma once
#include "pch.h"
#include "wndprvw.h"

// Controller for the classic editor controls embedded in the Appearance page.
class CAppearanceDlgBox
{
public:
	bool Initialize(HWND page);
	void Destroy();
	bool OnCommand(WORD id, WORD code);
	void Refresh();
	void OnPreviewClick(POINT pt);
	ICONMETRICSW iconMetrics = { sizeof(ICONMETRICSW) };
	int iconSize = 32;

private:
	struct ITEM
	{
		UINT name;
		int color1, color2, textColor, metric, font;
	};
	static const ITEM items[];
	const ITEM* CurrentItem() const;
	LOGFONTW* Font(const ITEM& item);
	int Metric(int target) const;
	void SetMetric(int target, int value);
	void ColorButton(int id, int color);
	void UpdatePreview();
	static LRESULT CALLBACK PreviewProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
	static int CALLBACK EnumFont(const LOGFONTW*, const TEXTMETRICW*, DWORD, LPARAM);
	HWND page = nullptr;
	HWND preview = nullptr;
	bool updating = false;
	Microsoft::WRL::ComPtr<IWindowPreview> windowPreview;
	HFONT boldFont = nullptr;
	HFONT italicFont = nullptr;
	MYWINDOWINFO windows[3] = {};
};
