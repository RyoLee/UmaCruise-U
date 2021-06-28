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

	m_config.i18n.Cover(m_hWnd, m_config.gFont);
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

	CComboBox cmbLanguage = GetDlgItem(IDC_COMBO_LANGUAGE);
	for (int i = 0; i < I18N::kMAXLanguage; i++) {
		LPCWSTR text = m_config.i18n.C_CODE_639_3166[i];
		cmbLanguage.AddString(text);
	}
	if (!m_config.screenShotFolder.empty())
	{
		SetDlgItemText(IDC_EDIT_SS_FOLDER, m_config.screenShotFolder.c_str());
	}
	m_autoStart = m_config.autoStart;
	m_stopUpdatePreviewOnTraining = m_config.stopUpdatePreviewOnTraining;
	m_autoCheckDB = m_config.autoCheckDB;
	m_autoCheckUpgrade = m_config.autoCheckUpgrade;
	m_notifyFavoriteRaceHold = m_config.notifyFavoriteRaceHold;
	m_theme = static_cast<int>(m_config.theme);
	m_windowTopMost = m_config.windowTopMost;
	m_screenshotFolder = m_config.screenShotFolder.wstring().c_str();
	m_language = static_cast<int>(m_config.language);
	DoDataExchange(DDX_LOAD);

	DarkModeInit();

	return 0;
}

LRESULT ConfigDlg::OnOK(WORD, WORD wID, HWND, BOOL&)
{
	DoDataExchange(DDX_SAVE);
	if (m_screenshotFolder.GetLength()) {
		if (!fs::is_directory((LPCWSTR)m_screenshotFolder)) {
			MessageBox(m_config.i18n.GetCSText(STR_NO_SS_DIR), L"Error", MB_ICONERROR);
			return 0;
		}
	}
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
	m_config.windowTopMost = m_windowTopMost;
	m_config.language = static_cast<I18N::CODE_639_3166>(m_language);
	m_config.screenShotFolder = (LPCWSTR)m_screenshotFolder;
	m_config.SaveConfig();

	EndDialog(IDOK);
	return 0;
}

LRESULT ConfigDlg::OnCancel(WORD, WORD, HWND, BOOL&)
{
	EndDialog(IDCANCEL);
	return 0;
}
// スクリーンショットの保存先フォルダを選択する
void ConfigDlg::OnScreenShotFolderSelect(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	DWORD dwOptions = FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST;
	CShellFileOpenDialog dlg(nullptr, dwOptions);
	auto ret = dlg.DoModal();
	if (ret == IDOK) {
		dlg.GetFilePath(m_screenshotFolder);
		DoDataExchange(DDX_LOAD, IDC_EDIT_SS_FOLDER);
	}
}