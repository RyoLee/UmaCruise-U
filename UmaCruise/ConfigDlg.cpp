#include "stdafx.h"
#include "ConfigDlg.h"

#include "Utility\json.hpp"
#include "Utility\CommonUtility.h"
#include "Utility\Logger.h"
#include "Utility\WinHTTPWrapper.h"

using json = nlohmann::json;
using namespace WinHTTPWrapper;

ConfigDlg::ConfigDlg(Config& config) : m_config(config)
{
}

LRESULT ConfigDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	DoDataExchange(DDX_LOAD);

	enum { kMaxRefershCount = 10 };
	for (int i = 1; i <= kMaxRefershCount; ++i) {
		m_cmbRefreshInterval.AddString(std::to_wstring(i).c_str());
	}
	m_cmbRefreshInterval.SetCurSel(m_config.refreshInterval - 1);

	CComboBox cmbTheme = GetDlgItem(IDC_COMBO_THEME);
	LPCWSTR themeList[] = { L"Auto", L"Dark", L"Light" };
	for (LPCWSTR themeText : themeList) {
		cmbTheme.AddString(themeText);
	}

	m_autoStart = m_config.autoStart;
	m_stopUpdatePreviewOnTraining = m_config.stopUpdatePreviewOnTraining;
	m_autoCheckDB = m_config.autoCheckDB;
	m_autoCheckUpgrade =m_config.autoCheckUpgrade;
	m_notifyFavoriteRaceHold = m_config.notifyFavoriteRaceHold;
	m_theme = static_cast<int>(m_config.theme);
	DoDataExchange(DDX_LOAD);

	DarkModeInit();

	return 0;
}

LRESULT ConfigDlg::OnOK(WORD, WORD wID, HWND, BOOL&)
{
	DoDataExchange(DDX_SAVE);

	const int index = m_cmbRefreshInterval.GetCurSel();
	if (index == -1) {
		ATLASSERT(FALSE);
		ERROR_LOG << L"m_cmbRefreshInterval.GetCurSel == -1";
	} else {
		m_config.refreshInterval = index + 1;
	}
	m_config.autoStart = m_autoStart;
	m_config.stopUpdatePreviewOnTraining = m_stopUpdatePreviewOnTraining;
	m_config.autoCheckDB = m_autoCheckDB;
	m_config.autoCheckUpgrade = m_autoCheckUpgrade;
	m_config.notifyFavoriteRaceHold = m_notifyFavoriteRaceHold;
	m_config.theme = static_cast<Config::Theme>(m_theme);

	m_config.SaveConfig();

	EndDialog(IDOK);
	return 0;
}

LRESULT ConfigDlg::OnCancel(WORD, WORD, HWND, BOOL&)
{
	EndDialog(IDCANCEL);
	return 0;
}
