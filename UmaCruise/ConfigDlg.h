#pragma once

#include "Config.h"
#include "DarkModeUI.h"
#include "resource.h"


class ConfigDlg : 
	public CDialogImpl<ConfigDlg>,
	public CWinDataExchange<ConfigDlg>,
	public DarkModeUI<ConfigDlg>
{
public:
	enum { IDD = IDD_CONFIG };

	ConfigDlg(Config& config);

	BEGIN_DDX_MAP(ConfigDlg)
		DDX_CONTROL_HANDLE(IDC_COMBO_REFRESHINTERVAL, m_cmbRefreshInterval)
		DDX_CHECK(IDC_CHECK_AUTOSTART, m_autoStart)
		DDX_CHECK(IDC_CHECK_STOPUPDATEPREVIEWONTRAINING, m_stopUpdatePreviewOnTraining)
		DDX_CHECK(IDC_CHECK_AUTOUPDATE, m_autoCheckDB)
		DDX_CHECK(IDC_CHECK_AUTOUPGREADE,m_autoCheckUpgrade)
		DDX_CHECK(IDC_CHECK_NOTIFY_FAVORITERACEHOLD, m_notifyFavoriteRaceHold)
		DDX_COMBO_INDEX(IDC_COMBO_THEME, m_theme)
		DDX_CHECK(IDC_CHECK_WINDOW_TOPMOST, m_windowTopMost)
	END_DDX_MAP()

	BEGIN_MSG_MAP_EX(ConfigDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)

		CHAIN_MSG_MAP(DarkModeUI<ConfigDlg>)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	
private:
	Config&		m_config;

	CComboBox	m_cmbRefreshInterval;
	bool	m_autoStart = false;
	bool	m_stopUpdatePreviewOnTraining = false;
	bool	m_autoCheckDB = true;
	bool	m_autoCheckUpgrade = true;
	bool	m_notifyFavoriteRaceHold = true;
	int		m_theme = Config::kAuto;
	bool	m_windowTopMost = false;

};
