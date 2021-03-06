// aboutdlg.h : interface of the CAboutDlg class
//
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Config.h"
#include "DarkModeUI.h"

class PreviewWindow;

#include "resource.h"

class CAboutDlg : public CDialogImpl<CAboutDlg>, DarkModeUI<CAboutDlg>
{
public:
	enum { IDD = IDD_ABOUTBOX };

	CAboutDlg(PreviewWindow& previewWindow, Config& config);

	BEGIN_MSG_MAP(CAboutDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		NOTIFY_HANDLER(IDC_SYSLINK_DB, NM_CLICK, OnLinkClickDB)
		NOTIFY_HANDLER(IDC_SYSLINK_HOMEPAGE, NM_CLICK, OnLinkClickHomePage)
		NOTIFY_HANDLER(IDC_SYSLINK_ORI_HOMEPAGE, NM_CLICK, OnLinkClickOriHomePage)
		NOTIFY_HANDLER(IDC_SYSLINK_VERSIONCHECK, NM_CLICK, OnLinkClickLatestVersion )
		
		COMMAND_ID_HANDLER(IDC_BUTTON_OCR, OnOCR)
		CHAIN_MSG_MAP(DarkModeUI<CAboutDlg>)
	END_MSG_MAP()

// Handler prototypes (uncomment arguments if needed):
//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	LRESULT OnLinkClickDB(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnLinkClickHomePage(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnLinkClickOriHomePage(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnLinkClickLatestVersion(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);

	LRESULT OnOCR(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	Config&		m_config;
	PreviewWindow& m_previewWindow;

	enum TestBounds {
		kDirect, kURACurrentTurnBounds, kAoharuCurrentTurnBounds, kEventCategoryBounds, kEventNameBounds, kEventNameIconBounds, kEventBottomOptionBounds, kCurrentMenuBounds, kRaceDetailBounds, kIkuseiUmaMusumeSubNameBounds, kIkuseiUmaMusumeNameBounds, kMaxCount
	};
	static constexpr LPCWSTR kTestBoundsName[kMaxCount] = {
		L"Direct", L"URACurrentTurnBounds", L"AoharuCurrentTurnBounds", L"EventCategoryBounds", L"EventNameBounds", L"EventNameIconBounds", L"EventBottomOptionBounds", L"CurrentMenuBounds", L"RaceDetailBounds", L"IkuseiUmaMusumeSubNameBounds", L"IkuseiUmaMusumeNameBounds"
	};


	CComboBox	m_cmbTestBounds;
	CEdit	m_editResult;
	CEdit	m_editResult2;
	CTrackBarCtrl	m_sliderThreshold;
};
