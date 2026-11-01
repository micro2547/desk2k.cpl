#pragma once
#include "pch.h"

// Deliberately visual-only: Active Desktop is not available on the host shell.
class CWebPage : public CPropertyPageImpl<CWebPage>
{
public:
    enum { IDD = IDD_WEBDLG };
    int OnSetActive();

private:
    BEGIN_MSG_MAP(CWebPage)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
        MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
        MESSAGE_HANDLER(WM_SYSCOLORCHANGE, OnSysColorChange)
        NOTIFY_HANDLER(IDC_WEB_CONTENTS, LVN_ITEMCHANGING, OnItemChanging)
        CHAIN_MSG_MAP(CPropertyPageImpl<CWebPage>)
    END_MSG_MAP()

    LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnDrawItem(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnDestroy(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnSysColorChange(UINT, WPARAM, LPARAM, BOOL&);
    LRESULT OnItemChanging(int, LPNMHDR, BOOL&);

    std::unique_ptr<Gdiplus::Bitmap> m_monitor;
    BOOL m_initializing = TRUE;
};
