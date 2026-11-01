#include "pch.h"
#include "WebPage.h"
#include "desk.h"
#include "helper.h"

LRESULT CWebPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
    m_initializing = TRUE;
    m_monitor.reset(Gdiplus::Bitmap::FromResource(g_hinst, MAKEINTRESOURCEW(IDB_BITMAP1)));
    Button_SetCheck(GetDlgItem(IDC_WEB_ENABLED), BST_CHECKED);
    HWND list = GetDlgItem(IDC_WEB_CONTENTS);
    ListView_SetExtendedListViewStyle(list, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
    RECT rect;
    ::GetClientRect(list, &rect);
    LVCOLUMNW column = {};
    column.mask = LVCF_WIDTH;
    column.cx = rect.right;
    ListView_InsertColumn(list, 0, &column);
    WCHAR title[128] = {};
    LoadStringW(_AtlBaseModule.GetResourceInstance(), IDS_WEB_HOME, title, ARRAYSIZE(title));
    LVITEMW item = {};
    item.mask = LVIF_TEXT | LVIF_STATE;
    item.pszText = title;
    item.stateMask = LVIS_STATEIMAGEMASK;
    item.state = INDEXTOSTATEIMAGEMASK(1);
    ListView_InsertItem(list, &item);
    ListView_SetItemState(list, 0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    ::EnableWindow(GetDlgItem(IDC_WEB_DELETE), FALSE);
    m_initializing = FALSE;
    return TRUE;
}

int CWebPage::OnSetActive()
{
    ::InvalidateRect(GetDlgItem(IDC_WEB_PREVIEW), nullptr, TRUE);
    return 0;
}

LRESULT CWebPage::OnSysColorChange(UINT, WPARAM, LPARAM, BOOL& handled)
{
    ::InvalidateRect(GetDlgItem(IDC_WEB_PREVIEW), nullptr, TRUE);
    handled = FALSE;
    return 0;
}

LRESULT CWebPage::OnDrawItem(UINT, WPARAM, LPARAM parameter, BOOL& handled)
{
    const auto draw = reinterpret_cast<const DRAWITEMSTRUCT*>(parameter);
    if (draw->CtlID != IDC_WEB_PREVIEW)
    {
        handled = FALSE;
        return 0;
    }
    FillRect(draw->hDC, &draw->rcItem, GetSysColorBrush(COLOR_3DFACE));
    if (!m_monitor || m_monitor->GetLastStatus() != Gdiplus::Ok)
        return TRUE;
    const int width = draw->rcItem.right - draw->rcItem.left;
    const int height = draw->rcItem.bottom - draw->rcItem.top;
    const int bitmapWidth = static_cast<int>(m_monitor->GetWidth());
    const int bitmapHeight = static_cast<int>(m_monitor->GetHeight());
    double scale = (std::min)(double(width) / bitmapWidth, double(height) / bitmapHeight);
    int monitorWidth = static_cast<int>(bitmapWidth * scale);
    int monitorHeight = static_cast<int>(bitmapHeight * scale);
    int left = draw->rcItem.left + (width - monitorWidth) / 2;
    int top = draw->rcItem.top + (height - monitorHeight) / 2;
    Gdiplus::Graphics graphics(draw->hDC);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
    Gdiplus::ImageAttributes attributes;
    attributes.SetColorKey(Gdiplus::Color(255, 255, 255, 0), Gdiplus::Color(255, 255, 255, 0));
    graphics.DrawImage(m_monitor.get(), Gdiplus::Rect(left, top, monitorWidth, monitorHeight),
        0, 0, bitmapWidth, bitmapHeight, Gdiplus::UnitPixel, &attributes);
    // Keep the monitor a static sample, colored with the pending desktop scheme.
    RECT screen = { left + MulDiv(16, monitorWidth, bitmapWidth), top + MulDiv(17, monitorHeight, bitmapHeight),
        left + MulDiv(168, monitorWidth, bitmapWidth), top + MulDiv(129, monitorHeight, bitmapHeight) };
    COLORREF previous = SetDCBrushColor(draw->hDC, GetDeskopColor());
    FillRect(draw->hDC, &screen, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
    SetDCBrushColor(draw->hDC, previous);
    return TRUE;
}

LRESULT CWebPage::OnDestroy(UINT, WPARAM, LPARAM, BOOL& handled)
{
    m_monitor.reset();
    handled = FALSE;
    return 0;
}

LRESULT CWebPage::OnItemChanging(int, LPNMHDR notification, BOOL&)
{
    const auto change = reinterpret_cast<const NMLISTVIEW*>(notification);
    // Permit selection/focus, but never let a preview checkbox imply an applied change.
    return !m_initializing && (change->uChanged & LVIF_STATE) &&
        ((change->uOldState ^ change->uNewState) & LVIS_STATEIMAGEMASK) ? TRUE : FALSE;
}
