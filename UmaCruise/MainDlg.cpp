
#include "stdafx.h"
#include "MainDlg.h"

#include <regex>
#include <unordered_set>
#include <tesseract\baseapi.h>
#include <leptonica\allheaders.h>

#include <opencv2\opencv.hpp>

#include <boost\algorithm\string\trim_all.hpp>
#include "Utility\CodeConvert.h"
#include "Utility\CommonUtility.h"
#include "Utility\json.hpp"
#include "Utility\timer.h"
#include "Utility\Logger.h"
#include "Utility\WinHTTPWrapper.h"
#include "win32-darkmode\DarkMode.h"

#include "GDIWindowCapture.h"
#include "DesktopDuplication.h"
#include "WindowsGraphicsCaptureWrapper.h"

#include "ConfigDlg.h"

using namespace WinHTTPWrapper;
using json = nlohmann::json;
using namespace CodeConvert;
using namespace cv;


bool IsDarkMode()
{
	CRegKey regkey;
	LSTATUS ret = regkey.Open(HKEY_CURRENT_USER, LR"(SOFTWARE\Microsoft\Windows\CurrentVersion\Themes\Personalize)", KEY_READ);
	if (ret == ERROR_SUCCESS) {
		DWORD value = 0;
		ret = regkey.QueryDWORDValue(L"AppsUseLightTheme", value);
		if (ret == ERROR_SUCCESS) {
			bool isDarkMode = !value;
			return isDarkMode;
		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////

CMainDlg::CMainDlg() : m_raceListWindow(m_config), m_wndScreenShotButton(this, 1)
{
}

BOOL CMainDlg::PreTranslateMessage(MSG* pMsg)
{
	return CWindow::IsDialogMessage(pMsg);
}

void CMainDlg::ChangeWindowTitle(const std::wstring& title)
{
	CString str = L"UmaCruise-U - ";
	str.Format(L"UmaCruise-U %s - %s", m_config.kAppVersion, title.c_str());
	SetWindowText(str);
}

LRESULT CMainDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&)
{
	// center the dialog on the screen
	CenterWindow();

	// set icons
	HICON hIcon = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON));
	SetIcon(hIcon, TRUE);
	HICON hIconSmall = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON));
	SetIcon(hIconSmall, FALSE);

	// register object for message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	UIAddChildWindowContainer(m_hWnd);

	// フォルダパスの文字コードチェック
	auto exeDir = GetExeDirectory().wstring();
	auto sjisDir = ShiftJISfromUTF16(exeDir);
	auto sjis_utf16exeDir = UTF16fromShiftJIS(sjisDir);
	if (exeDir != sjis_utf16exeDir) {
		//ERROR_LOG << L"exeDir contain unicode";
		MessageBox(m_config.i18n.GetCSText(STR_ERROR_PATH_CSET),L"Error", MB_ICONERROR);
	}

	m_config.LoadConfig();
	m_config.i18n.Load(m_config.language);
	if(m_config.autoCheckUpgrade){
		_CheckUmaCruiseU();
	}
	if(m_config.autoCheckDB){
		_CheckUmaLibrary(m_config.language);
	}
	ChangeGlobalTheme(m_config.theme);
	m_config.i18n.Cover(m_hWnd, m_config.gFont);

	DoDataExchange(DDX_LOAD);

	// 選択肢エディットの背景色を設定
	m_optionBkColor[0] = RGB(203, 247, 148);
	m_optionBkColor[1] = RGB(255, 236, 150);
	m_optionBkColor[2] = RGB(255, 203, 228);
	m_optionBkColor[3] = RGB(100, 149, 237);
	m_optionBkColor[4] = RGB(123, 104, 238);
	for (int i = 0; i < std::size(m_brsOptions); ++i) {
		m_brsOptions[i].CreateSolidBrush(m_optionBkColor[i]);
	}

	// デフォルトフォント取得
	CEdit edit = GetDlgItem(IDC_EDIT_OPTION1);
	HFONT font = edit.GetFont();
	CLogFont lf;
	GetObject(font, sizeof(LOGFONT), (LPVOID)&lf);
	m_effectFont.CreateFontIndirectW(&lf);

	// 選択肢効果エディットを初期化
	for (size_t i = 0; i < kMaxOptionEffect; ++i) {
		const int IDC_EFFECT = IDC_EDIT_EFFECT1 + i;

		// これを設定しないとフォント表示がおかしくなる
		GetDlgItem(IDC_EFFECT).SendMessage(EM_SETLANGOPTIONS, 0, (LPARAM)IMF_UIFONTS);
		//GetDlgItem(IDC_EFFECT).SetFont(m_effectFont);
	}

	// ポップアップリッチエディット作成
	m_popupRichEdit.SetFont(lf.CreateFontIndirectW());
	m_popupRichEdit.Create(m_hWnd);

	// プレビューウィンドウ作成
	m_previewWindow.Create(m_hWnd);
	m_previewWindow.SetNotifyDragdropBounds([this](const CRect& rcBounds) {
		m_rcBounds = rcBounds;
	});

	// UmaMusumeLibraryを読み込み
	if (!_ReloadUmaMusumeLibrary()) {
		MessageBox(L"LoadUmaMusumeLibrary failed");
	}

	// SkillLibraryを読み込み
	if (!m_skillLibrary.LoadSkillLibrary()) {
		ERROR_LOG << L"LoadSkillLibrary failed";
		ATLASSERT(FALSE);
	}

	if (!m_umaTextRecoginzer.LoadSetting()) {
		ERROR_LOG << L"m_umaTextRecoginzer.LoadSetting failed";
		ATLASSERT(FALSE);
	}

	// レース一覧ウィンドウ作成
	m_raceListWindow.Create(m_hWnd);
	m_umaEventLibrary.RegisterNotifyChangeIkuseiUmaMusume([this](const std::wstring& umaName) {
		m_raceListWindow.ChangeIkuseiUmaMusume(umaName);
	});

	try {
		{
			std::ifstream ifs((GetExeDirectory() / L"UmaLibrary" / "Common.json").wstring());
			ATLASSERT(ifs);
			if (!ifs) {
				ERROR_LOG << L"Common.json does not exist...";
				ChangeWindowTitle(m_config.i18n.GetWSText(STR_NO_COMMON));
			} else {
				json jsonCommon;
				ifs >> jsonCommon;

				m_targetWindowName = UTF16fromUTF8(jsonCommon["Common"]["TargetWindow"]["WindowName"].get<std::string>()).c_str();
				m_targetClassName = UTF16fromUTF8(jsonCommon["Common"]["TargetWindow"]["ClassName"].get<std::string>()).c_str();

				m_effectStatusInc = ColorFromText(jsonCommon["Theme"]["Status"].value("+", "#4CAF50"));
				m_effectStatusDec = ColorFromText(jsonCommon["Theme"]["Status"].value("-", "#F44336"));
			}
		}

		std::ifstream fs((GetExeDirectory() / "setting.json").wstring());
		if (fs) {
			json jsonSetting;
			fs >> jsonSetting;

			{
				auto& windowRect = jsonSetting["MainDlg"]["WindowRect"];
				if (windowRect.is_null() == false) {
					CRect rc(windowRect[0], windowRect[1], windowRect[2], windowRect[3]);
					SetWindowPos(NULL, rc.left, rc.top, 0, 0, SWP_NOSIZE | SWP_NOOWNERZORDER);
				}
				if (m_config.windowTopMost) {
					SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
				}

				m_bShowRaceList = jsonSetting["MainDlg"].value<bool>("ShowRaceList", m_bShowRaceList);
				m_bShowExOpts = jsonSetting["MainDlg"].value<bool>("ShowExOpts", m_bShowExOpts);
			}

			{
				auto& windowRect = jsonSetting["PreviewWindow"]["WindowRect"];
				if (windowRect.is_null() == false) {
					CRect rc(windowRect[0], windowRect[1], windowRect[2], windowRect[3]);
					m_previewWindow.SetWindowPos(NULL, rc.left, rc.top, rc.Width(), rc.Height(), SWP_NOOWNERZORDER);
				}
				bool showWindow = jsonSetting["PreviewWindow"].value<bool>("ShowWindow", false);
				if (showWindow) {
					m_previewWindow.ShowWindow(SW_NORMAL);
				}
			}
		}
		_InitRaceListWindow();
		DoDataExchange(DDX_LOAD);

	} catch (std::exception& e)
	{
		ATLTRACE(L"%s\n", (LPCWSTR)(CA2W(e.what())));
		ERROR_LOG << L"LoadConfig failed: " << (LPCWSTR)(CA2W(e.what()));
		ATLASSERT(FALSE);
	}
	ChangeWindowTitle(m_config.i18n.GetWSText(STR_INIT_SUCCESS));

	if (m_config.autoStart) {
		CButton(GetDlgItem(IDC_CHECK_START)).SetCheck(BST_CHECKED);
		OnStart(0, 0, NULL);
	}

	DarkModeInit();
	if(!m_bShowExOpts){
		_ShowHideExOpts(m_bShowExOpts);
	}
	return TRUE;
}

LRESULT CMainDlg::OnDestroy(UINT, WPARAM, LPARAM, BOOL&)
{
	// unregister message filtering and idle updates
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);

	if (m_threadAutoDetect.joinable()) {
		m_cancelAutoDetect = true;
		m_threadAutoDetect.detach();
		::Sleep(5 * 1000);
	}

	return 0;
}


LRESULT CMainDlg::OnAppAbout(WORD, WORD, HWND, BOOL&)
{
	CAboutDlg dlg(m_previewWindow,m_config);
	dlg.DoModal();
	return 0;
}

// レースリストの表示を切り替え
void CMainDlg::OnShowHideRaceList(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	m_bShowRaceList = !m_bShowRaceList;
	_ExtentOrShrinkWindow(m_bShowRaceList);	
}

void CMainDlg::OnShowHideExOpts(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	m_bShowExOpts = !m_bShowExOpts;
	_ShowHideExOpts(m_bShowExOpts);
}
void CMainDlg::_ShowHideExOpts(bool bExtent)
{
	CRect rOptGroup;
	CRect rOpt1;
	CRect rOpt2;
	CRect rRaceListGroup;
	CRect rRaceList;
	CRect rcWindow;
	CRect rlWindow;
	GetWindowRect(&rcWindow);
	GetDlgItem(IDC_STATIC_OPT_GROUP).GetWindowRect(rOptGroup);
	m_raceListWindow.GetWindowRect(rlWindow);
	m_raceListWindow.GetDlgItem(IDC_STATIC_RACELIST_GROUP).GetWindowRect(rRaceListGroup);
	m_raceListWindow.GetDlgItem(IDC_LIST_RACE).GetWindowRect(rRaceList);
	GetDlgItem(IDC_EDIT_OPTION1).GetWindowRect(rOpt1);
	GetDlgItem(IDC_EDIT_OPTION2).GetWindowRect(rOpt2);
	int dH = (rOpt2.CenterPoint().y - rOpt1.CenterPoint().y)*2;
	if (!bExtent) {
		dH = -1 * dH;
	}
	m_raceListWindow.GetDlgItem(IDC_STATIC_RACELIST_GROUP).SetWindowPos(NULL, 0, 0, rRaceListGroup.Width(), rRaceListGroup.Height() + dH, SWP_NOZORDER | SWP_NOMOVE);
	m_raceListWindow.GetDlgItem(IDC_LIST_RACE).SetWindowPos(NULL, 0, 0, rRaceList.Width(), rRaceList.Height() + dH, SWP_NOZORDER | SWP_NOMOVE);
	GetDlgItem(IDC_STATIC_OPT_GROUP).SetWindowPos(NULL, 0, 0, rOptGroup.Width(), rOptGroup.Height()+ dH, SWP_NOZORDER | SWP_NOMOVE);
	m_raceListWindow.SetWindowPos(NULL, 0, 0, rlWindow.Width(), rlWindow.Height()+ dH, SWP_NOZORDER | SWP_NOMOVE);
	SetWindowPos(NULL, 0, 0, rcWindow.Width(), rcWindow.Height()+ dH, SWP_NOZORDER | SWP_NOMOVE);
	GetDlgItem(IDC_EDIT_OPTION4).ShowWindow(bExtent);
	GetDlgItem(IDC_EDIT_EFFECT4).ShowWindow(bExtent);
	GetDlgItem(IDC_EDIT_OPTION5).ShowWindow(bExtent);
	GetDlgItem(IDC_EDIT_EFFECT5).ShowWindow(bExtent);
}

LRESULT CMainDlg::OnCancel(WORD, WORD wID, HWND, BOOL&)
{
	DoDataExchange(DDX_SAVE);

	m_raceListWindow.ShowWindow(false);	// ウィンドウ位置保存

	json jsonSetting;
	std::ifstream fs((GetExeDirectory() / "setting.json").wstring());
	if (fs) {
		fs >> jsonSetting;
	}
	fs.close();

	if (IsIconic() == FALSE) {
		{
			CRect rcWindow;
			GetWindowRect(&rcWindow);
			jsonSetting["MainDlg"]["WindowRect"] =
				nlohmann::json::array({ rcWindow.left, rcWindow.top, rcWindow.right, rcWindow.bottom });

			jsonSetting["MainDlg"]["ShowRaceList"] = m_bShowRaceList;
			jsonSetting["MainDlg"]["ShowExOpts"] = m_bShowExOpts;
		}
		{
			CRect rcWindow;
			m_previewWindow.GetWindowRect(&rcWindow);
			jsonSetting["PreviewWindow"]["WindowRect"] =
				nlohmann::json::array({ rcWindow.left, rcWindow.top, rcWindow.right, rcWindow.bottom });
			bool showWindow = m_previewWindow.IsWindowVisible() != 0;
			jsonSetting["PreviewWindow"]["ShowWindow"] = showWindow;
		}
	}

	std::ofstream ofs((GetExeDirectory() / "setting.json").wstring());
	ofs << jsonSetting.dump(4);
	ofs.close();

	DestroyWindow();
	::PostQuitMessage(0);
	return 0;
}

void CMainDlg::OnShowConfigDlg(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	const int prevTheme = m_config.theme;
	const int prevSCMethod = m_config.screenCaptureMethod;
	ConfigDlg dlg(m_config);
	auto ret = dlg.DoModal(m_hWnd);
	if (ret == IDOK) {
		if (prevTheme != m_config.theme) {
			ChangeGlobalTheme(m_config.theme);
			OnThemeChanged();
			m_raceListWindow.OnThemeChanged();
			m_previewWindow.OnThemeChanged();
			m_popupRichEdit.OnThemeChanged();
		}
		if (prevSCMethod != m_config.screenCaptureMethod) {
			std::unique_lock lock(m_mtxScreennShotWindow);
			m_screenshotWindow.reset();
		}

		SetWindowPos(m_config.windowTopMost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);	
	}
}

void CMainDlg::OnShowPreviewWindow(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	m_previewWindow.ShowWindow(SW_NORMAL);
}

void CMainDlg::OnTimer(UINT_PTR nIDEvent)
{
}

// コンボボックスから育成ウマ娘が変更された場合
void CMainDlg::OnSelChangeUmaMusume(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	const int index = m_cmbUmaMusume.GetCurSel();
	if (index == -1) {
		return;
	}
	CString umaName;
	m_cmbUmaMusume.GetLBText(index, umaName);
	if (umaName.Left(1) == L"☆") {
		m_umaEventLibrary.ChangeIkuseiUmaMusume(L"");
		return;
	}
	m_umaEventLibrary.ChangeIkuseiUmaMusume((LPCWSTR)umaName);
}

// ドッキング状態ならレース一覧ウィンドウを同時に動かす
LRESULT CMainDlg::OnDockingProcess(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	auto funcDockingMove = [this]() -> bool {
		CRect rcParentWindow;
		GetWindowRect(&rcParentWindow);

		CRect rcWindow;
		m_raceListWindow.GetWindowRect(&rcWindow);
		CRect rcClient;
		m_raceListWindow.GetClientRect(&rcClient);

		const int cxPadding = (rcWindow.Width() - rcClient.Width()) - (GetSystemMetrics(SM_CXBORDER) * 2);//GetSystemMetrics(SM_CXSIZEFRAME) * 2;
		const int cyPadding = GetSystemMetrics(SM_CYSIZEFRAME) * 2;

		bool bDocking = false;
		// メインの右にある
		if (std::abs(rcParentWindow.right - rcWindow.left) <= RaceListWindow::kDockingMargin) {
			rcWindow.MoveToX(rcParentWindow.right - cxPadding);
			bDocking = true;

			// メインの左にある
		} else if (std::abs(rcParentWindow.left - rcWindow.right) <= RaceListWindow::kDockingMargin) {
			rcWindow.MoveToX(rcParentWindow.left - rcWindow.Width() + cxPadding);
			bDocking = true;

			// メインの上にある
		} else if (std::abs(rcParentWindow.top - rcWindow.bottom) <= RaceListWindow::kDockingMargin) {
			rcWindow.MoveToY(rcParentWindow.top - rcWindow.Height() + cyPadding);
			bDocking = true;

			// メインの下にある
		} else if (std::abs(rcParentWindow.bottom - rcWindow.top) <= RaceListWindow::kDockingMargin) {
			rcWindow.MoveToY(rcParentWindow.bottom - cyPadding);
			bDocking = true;
		}
		if (bDocking) {
			m_raceListWindow.MoveWindow(&rcWindow);

			m_ptRelativeDockingPos.x = rcWindow.left - rcParentWindow.left;
			m_ptRelativeDockingPos.y = rcWindow.top - rcParentWindow.top;
		}
		return bDocking;
	};
	if (uMsg == WM_ENTERSIZEMOVE) {
		m_bDockingMove = false;
	} else if (m_bDockingMove) {
		CRect rcWindow;
		GetWindowRect(&rcWindow);

		CPoint ptActualPos = m_ptRelativeDockingPos;
		ptActualPos.x += rcWindow.left;
		ptActualPos.y += rcWindow.top;
		m_raceListWindow.SetWindowPos(NULL, ptActualPos.x, ptActualPos.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	}
	return TRUE;
}

// ダイアログの背景色を白に変更
HBRUSH CMainDlg::OnCtlColorDlg(CDCHandle dc, CWindow wnd)
{
	// 選択肢エディットの背景色を設定
	const int ctrlID = wnd.GetDlgCtrlID();
	if (IDC_EDIT_OPTION1 <= ctrlID && ctrlID <= IDC_EDIT_OPTION5) {
		int i = ctrlID - IDC_EDIT_OPTION1;
		dc.SetBkMode(OPAQUE);
		dc.SetBkColor(m_optionBkColor[i]);
		return m_brsOptions[i];
	}
	SetMsgHandled(FALSE);
	return (HBRUSH)::GetStockObject(WHITE_BRUSH);
}

void CMainDlg::OnScreenShot(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	if (::GetKeyState(VK_CONTROL) < 0) {	// Ctrl を押しながらだとプレビューからIRする
		Utility::timer timer;
		//auto ssImage = m_umaTextRecoginzer.ScreenShot();
		auto image = m_previewWindow.GetImage();
		if (!image) {
			return;
		}
		Gdiplus::Bitmap bmp(image->GetWidth(), image->GetHeight(), PixelFormat24bppRGB);
		Gdiplus::Graphics graphics(&bmp);
		graphics.DrawImage(image, 0, 0);

		bool success = m_umaTextRecoginzer.TextRecognizer(&bmp);
		INFO_LOG << L"TextRecognizer processing time: " << timer.format();

		// 育成ウマ娘名
		std::wstring prevUmaName = m_umaEventLibrary.GetCurrentIkuseiUmaMusume();
		m_umaEventLibrary.AnbigiousChangeIkuseImaMusume(m_umaTextRecoginzer.GetUmaMusumeName());
		std::wstring nowUmaName = m_umaEventLibrary.GetCurrentIkuseiUmaMusume();
		if (prevUmaName != nowUmaName) {
			// コンボボックスを変更
			const int count = m_cmbUmaMusume.GetCount();
			for (int i = 0; i < count; ++i) {
				CString name;
				m_cmbUmaMusume.GetLBText(i, name);
				if (name == nowUmaName.c_str()) {
					m_cmbUmaMusume.SetCurSel(i);
					break;
				}
			}
		}

		// イベント検索
		auto optUmaEvent = m_umaEventLibrary.AmbiguousSearchEvent(
			m_umaTextRecoginzer.GetEventName(), 
			m_umaTextRecoginzer.GetEventBottomOption() );
		if (optUmaEvent && m_eventName != optUmaEvent->eventName.c_str()) {
			m_eventName = optUmaEvent->eventName.c_str();
			DoDataExchange(DDX_LOAD, IDC_EDIT_EVENTNAME);

			_UpdateEventOptions(*optUmaEvent);

			m_eventSource = optUmaEvent->parentCharaEvent->name.c_str();
			DoDataExchange(DDX_LOAD, IDC_EDIT_EVENT_SOURCE);
		}


		// 現在ターン
		bool ikuseiTop = m_umaTextRecoginzer.IsTrainingMenu() || m_umaTextRecoginzer.IsIkuseiTop();
		m_raceListWindow.AnbigiousChangeCurrentTurn(m_umaTextRecoginzer.GetCurrentTurn(), ikuseiTop);

		// レース距離
		m_raceListWindow.EntryRaceDistance(m_umaTextRecoginzer.GetEntryRaceDistance());

		//++count;
		CString title;
		title.Format(m_config.i18n.GetCSText(STR_PROC_TIME), UTF16fromUTF8(timer.format()).c_str());
		ChangeWindowTitle((LPCWSTR)title)
			;
	} else {
		LPCWSTR className = m_targetClassName.GetLength() ? (LPCWSTR)m_targetClassName : nullptr;
		LPCWSTR windowName = m_targetWindowName.GetLength() ? (LPCWSTR)m_targetWindowName : nullptr;
		HWND hwndTarget = ::FindWindow(className, windowName);
		if (!hwndTarget) {
			ChangeWindowTitle(m_config.i18n.GetWSText(STR_WIN_NOT_FOND));
			return;
		}
		auto ssFolderPath = GetExeDirectory() / L"screenshot";
		// オプションで設定されたフォルダ
		if (!m_config.screenShotFolder.empty() && boost::filesystem::is_directory(m_config.screenShotFolder)) {
			ssFolderPath = m_config.screenShotFolder;
		} else {
			// 既定のフォルダ
			if (!fs::is_directory(ssFolderPath)) {
				fs::create_directory(ssFolderPath);
			}
		}

		auto ssPath = ssFolderPath / (L"screenshot_" + std::to_wstring(std::time(nullptr)) + L".png");
		if (GetKeyState(VK_SHIFT) < 0) {
			ssPath = ssFolderPath / L"screenshot.png";
		}
		// 
		auto image = _ScreenShotUmaWindow();
		if (!image) {
			ChangeWindowTitle(m_config.i18n.GetWSText(STR_SS_FAILED));
			return;
		}

		auto pngEncoder = GetEncoderByMimeType(L"image/png");
		auto ret = image->Save(ssPath.c_str(), &pngEncoder->Clsid);
		bool success = ret == Gdiplus::Ok;
		//bool success = SaveWindowScreenShot(hwndTarget, ssPath.wstring());
		//bool success = SaveScreenShot(L"", ssPath.wstring());
		ATLASSERT(success);

		m_previewWindow.UpdateImage(ssPath.wstring());
	}
}


void CMainDlg::OnStart(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	INFO_LOG << L"OnStart";

	CButton btnStart = GetDlgItem(IDC_CHECK_START);
	bool bChecked = btnStart.GetCheck() == BST_CHECKED;
	if (bChecked) {
		ATLASSERT(!m_threadAutoDetect.joinable());
		btnStart.SetWindowText(m_config.i18n.GetCSText(STR_STOP));
		m_cancelAutoDetect = false;
		m_threadAutoDetect = std::thread([this]()
		{
			INFO_LOG << L"thread begin";

			auto funcChangeIkuseiUmaMusumeName = [this](const std::vector<std::wstring>& umaMusumeNameList) {
				std::wstring prevUmaName = m_umaEventLibrary.GetCurrentIkuseiUmaMusume();
				m_umaEventLibrary.AnbigiousChangeIkuseImaMusume(umaMusumeNameList);
				std::wstring nowUmaName = m_umaEventLibrary.GetCurrentIkuseiUmaMusume();
				if (prevUmaName != nowUmaName) {
					// コンボボックスを変更
					const int count = m_cmbUmaMusume.GetCount();
					for (int i = 0; i < count; ++i) {
						CString name;
						m_cmbUmaMusume.GetLBText(i, name);
						if (name == nowUmaName.c_str()) {
							m_cmbUmaMusume.SetCurSel(i);
							break;
						}
					}
				}
			};

			// 初回のみ能力詳細からウマ娘名を取得する
			auto ssImage2 = _ScreenShotUmaWindow();
			std::wstring ikuseiUmaMusumeName =  m_umaTextRecoginzer.GetIkuseiUmaMusumeName(ssImage2.get());
			if (ikuseiUmaMusumeName.length()) {
				funcChangeIkuseiUmaMusumeName(std::vector<std::wstring>({ ikuseiUmaMusumeName }));
			}
			ssImage2.reset();

			const auto milisecInterval = m_config.refreshInterval * 1000;
			int count = 0;
			while (!m_cancelAutoDetect.load()) {
				Utility::timer timer;

				const auto begin = std::chrono::steady_clock::now();

				auto ssImage = _ScreenShotUmaWindow();	// m_umaTextRecoginzer.ScreenShot();
				if (ssImage) {
					const int imageWidth = ssImage->GetWidth();
					const int imageHeight = ssImage->GetHeight();
					if (imageHeight < imageWidth) {	// 横画面なら何もしない
						m_previewWindow.UpdateImage(ssImage.release());
						::Sleep(milisecInterval);
						continue;
					}
				}
				bool success = m_umaTextRecoginzer.TextRecognizer(ssImage.get());
				if (success) {
					if (m_cancelAutoDetect.load()) {
						break;	// cancel
					}

					bool updateImage = true;
					if (m_config.stopUpdatePreviewOnTraining && !m_umaTextRecoginzer.IsTrainingMenu()) {
						updateImage = false;
					}
					if (updateImage) {
						m_previewWindow.UpdateImage(ssImage.release());
					}

					// 育成ウマ娘名
					funcChangeIkuseiUmaMusumeName(m_umaTextRecoginzer.GetUmaMusumeName());

					// イベント検索
					auto optUmaEvent = m_umaEventLibrary.AmbiguousSearchEvent(
						m_umaTextRecoginzer.GetEventName(), 
						m_umaTextRecoginzer.GetEventBottomOption() );
					if (optUmaEvent && m_eventName != optUmaEvent->eventName.c_str()) {
						m_eventName = optUmaEvent->eventName.c_str();
						DoDataExchange(DDX_LOAD, IDC_EDIT_EVENTNAME);

						_UpdateEventOptions(*optUmaEvent);

						m_eventSource = optUmaEvent->parentCharaEvent->name.c_str();
						DoDataExchange(DDX_LOAD, IDC_EDIT_EVENT_SOURCE);
					}


					// 現在ターン
					bool ikuseiTop = m_umaTextRecoginzer.IsTrainingMenu() || m_umaTextRecoginzer.IsIkuseiTop();
					m_raceListWindow.AnbigiousChangeCurrentTurn(m_umaTextRecoginzer.GetCurrentTurn(), ikuseiTop);

					// レース距離
					m_raceListWindow.EntryRaceDistance(m_umaTextRecoginzer.GetEntryRaceDistance());

					++count;
					CString title;
					title.Format(L"scan: %d %s", count, (LPCWSTR)CA2W(timer.format(3, "[%ws]").c_str()));
					ChangeWindowTitle((LPCWSTR)title);

					// wait
					auto end = std::chrono::steady_clock::now();
					auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
					do {
						::Sleep(50);
						end = std::chrono::steady_clock::now();
						elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
					} while (elapsed < milisecInterval);

				} else {
					if (!ssImage) {
						ChangeWindowTitle(m_config.i18n.GetWSText(STR_WIN_NOT_FOND));
					} else {
						ChangeWindowTitle(m_config.i18n.GetWSText(STR_FAILED));
					}
					int sleepCount = 0;
					enum { kMaxSleepCount = 30 };
					while (!m_cancelAutoDetect.load() && sleepCount < kMaxSleepCount) {
						::Sleep(1000);
						++sleepCount;
					}
				}
			}
			// finish
			{
				std::unique_lock lock(m_mtxScreennShotWindow);
				m_screenshotWindow.reset();
			}

			if (m_threadAutoDetect.joinable()) {
				CButton btnStart = GetDlgItem(IDC_CHECK_START);
				btnStart.SetWindowText(m_config.i18n.GetCSText(IDC_CHECK_START));
				btnStart.EnableWindow(TRUE);
				m_threadAutoDetect.detach();
			}
		});
		//OnTimer(kAutoOCRTimerID);
		//SetTimer(kAutoOCRTimerID, kAutoOCRTimerInterval);
	} else {
		if (m_threadAutoDetect.joinable()) {
			btnStart.SetWindowText(m_config.i18n.GetCSText(STR_STOPPING));
			btnStart.EnableWindow(FALSE);
			m_cancelAutoDetect = true;
		}
		//KillTimer(kAutoOCRTimerID);
	}
}

void CMainDlg::OnEventNameChanged(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	DoDataExchange(DDX_SAVE, IDC_EDIT_EVENTNAME);
	if (m_eventName.IsEmpty() || GetFocus() != GetDlgItem(IDC_EDIT_EVENTNAME)) {
		return;
	}
	std::vector<std::wstring> eventNames;
	eventNames.emplace_back((LPCWSTR)m_eventName);
	auto optUmaEvent = m_umaEventLibrary.AmbiguousSearchEvent(eventNames, { L"" });
	if (optUmaEvent) {
		ChangeWindowTitle(optUmaEvent->eventName);
		_UpdateEventOptions(*optUmaEvent);

		m_eventSource = optUmaEvent->parentCharaEvent->name.c_str();
		DoDataExchange(DDX_LOAD, IDC_EDIT_EVENT_SOURCE);
	}
	
}

// イベント選択肢の効果を修正する
void CMainDlg::OnEventRevision(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	UmaEventLibrary::UmaEvent umaEvent;
	DoDataExchange(DDX_SAVE, IDC_EDIT_EVENTNAME);
	DoDataExchange(DDX_SAVE, IDC_EDIT_EVENT_SOURCE);
	if (m_eventName.IsEmpty()) {
		MessageBox(m_config.i18n.GetCSText(STR_NO_EVENT));
		return;
	}
	if (m_eventSource.IsEmpty()) {
		MessageBox(m_config.i18n.GetCSText(STR_NO_SOURCE));
		return;
	}
	
	json jsonOptionsArray = json::array();
	const size_t count = umaEvent.eventOptions.size();
	for (size_t i = 0; i < count; ++i) {
		const int IDC_OPTION = IDC_EDIT_OPTION1 + i;
		const int IDC_EFFECT = IDC_EDIT_EFFECT1 + i;
		CString text;
		GetDlgItem(IDC_OPTION).GetWindowText(text);
		umaEvent.eventOptions[i].option = (LPCWSTR)text;
		GetDlgItem(IDC_EFFECT).GetWindowText(text);
		umaEvent.eventOptions[i].effect = (LPCWSTR)text;
		boost::algorithm::replace_all(umaEvent.eventOptions[i].effect, L"\r\n", L"\n");

		if (umaEvent.eventOptions[i].option.empty()) {
			break;
		}
		json jsonOption = {
			{"Option", UTF8fromUTF16(umaEvent.eventOptions[i].option) },
			{"Effect", UTF8fromUTF16(umaEvent.eventOptions[i].effect) }
		};
		jsonOptionsArray.push_back(jsonOption);
	}
	if (jsonOptionsArray.empty()) {
		MessageBox(m_config.i18n.GetCSText(STR_NO_OPTION));
		return;
	}

	CString msg;
	msg.Format(m_config.i18n.GetCSText(STR_CORRECT_CHECK), (LPCWSTR)m_eventName);
	if (MessageBox(msg, L"Check", MB_YESNO) == IDNO) {
		return;
	}
	{
		json jsonRevisionLibrary;
		std::ifstream ifs((GetExeDirectory() / L"UmaLibrary" / "UmaMusumeLibraryRevision.json").wstring());
		if (ifs) {
			ifs >> jsonRevisionLibrary;
			ifs.close();
		}

		std::string source = UTF8fromUTF16((LPCWSTR)m_eventSource);
		std::string eventName = UTF8fromUTF16((LPCWSTR)m_eventName);

		bool update = false;
		json& jsonEventList = jsonRevisionLibrary[source]["Event"];
		if (jsonEventList.is_array()) {
			// 更新
			for (json& jsonEvent : jsonEventList) {
				auto eventElm = *jsonEvent.items().begin();
				std::string orgEventName = eventElm.key();
				if (orgEventName == eventName) {
					json& jsonOptions = eventElm.value();
					jsonOptions.clear();		// 選択肢を一旦全部消す
					jsonOptions = jsonOptionsArray;
					update = true;
					break;
				}
			}
		}
		// 追加
		if (!update) {
			json jsonEvent;
			jsonEvent[eventName] = jsonOptionsArray;
			jsonEventList.push_back(jsonEvent);
		}

		// 保存
		std::ofstream ofs((GetExeDirectory() / L"UmaLibrary" / "UmaMusumeLibraryRevision.json").wstring());
		ATLASSERT(ofs);
		if (!ofs) {
			MessageBox(m_config.i18n.GetCSText(STR_ERROR_OPEN_REV));
			return;
		}
		ofs << jsonRevisionLibrary.dump(2);
		ofs.close();

		m_umaEventLibrary.LoadUmaMusumeLibrary();
		MessageBox(m_config.i18n.GetCSText(STR_SUCCESS));
	}
}

// カーソル下の効果エディットをポップアップ表示させる
BOOL CMainDlg::OnSetCursor(CWindow wnd, UINT nHitTest, UINT message)
{
	if (::GetAsyncKeyState(VK_MENU) < 0) {
		return 0;
	}
	if (::GetFocus() == m_popupRichEdit.GetRichEdit()) {
		return 0;
	}

	if (message == WM_MOUSEMOVE) {
		CPoint ptCursor;
		::GetCursorPos(&ptCursor);

		for (size_t i = 0; i < kMaxOptionEffect; ++i) {
			const int IDC_EFFECT = IDC_EDIT_EFFECT1 + i;
			CRichEditCtrl richEdit = GetDlgItem(IDC_EFFECT);
			CRect rcClient;
			richEdit.GetClientRect(&rcClient);
			richEdit.MapWindowPoints(NULL, &rcClient);
			if (rcClient.PtInRect(ptCursor)) {

				// 効果テキスト取得
				CString text;
				richEdit.GetWindowText(text.GetBuffer(kMaxEffectTextLength), kMaxEffectTextLength);
				text.ReleaseBuffer();
				text.Trim();
				if (text.IsEmpty()) {
					break;
				}


				// テキストの描写範囲を取得
				CRect rcEditDesktop;
				richEdit.GetClientRect(&rcEditDesktop);

				CDCHandle dc(richEdit.GetDC());
				HFONT prevFont = dc.SelectFont(m_effectFont);

				CRect rcText;
				dc.DrawText(text, text.GetLength(), &rcText, DT_CALCRECT);
				rcEditDesktop = rcText;
				rcEditDesktop.top = kPopupRichEditTopLeftMargin;
				rcEditDesktop.left = kPopupRichEditTopLeftMargin;
				rcEditDesktop.right += kPopupRichEditRightBottomMargin;
				rcEditDesktop.bottom += kPopupRichEditRightBottomMargin;

				dc.SelectFont(prevFont);

				// テキストがエディットボックスからはみ出さなければポップアップは表示しない
				if (rcEditDesktop.Width() <= rcClient.Width() && rcEditDesktop.Height() <= rcClient.Height()) {
					return 0;
				}
				rcEditDesktop.right = max(static_cast<int>(rcEditDesktop.right), rcClient.Width());

				m_popupRichEdit.SetOriginalEffectRichEditRect(rcClient);
				m_popupRichEdit.SetEventName(m_eventName);

				// テキストコピー
				m_popupRichEdit.SetOriginalEffectRichEdit(richEdit);
				_UpdateEventEffect(m_popupRichEdit.GetRichEdit(), (LPCWSTR)text);

				// ポップアップ表示させる				
				richEdit.MapWindowPoints(NULL, &rcEditDesktop);

				m_popupRichEdit.SetWindowPos(NULL, &rcEditDesktop, SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
				//m_popupRichEdit.ShowWindow(SW_SHOWNOACTIVATE);
				return 0;
			}
		}
	}
	if (::GetFocus() != m_popupRichEdit.GetRichEdit() && m_popupRichEdit.GetOriginalEffectRichEdit()) {

		// 効果テキスト取得
		CString text = m_popupRichEdit.GetEffectText(m_eventName);
		if (text.GetLength()) {
			// フォーカスを失う前に、ポップアップリッチエディットに書き込まれた内容を元のリッチエディットに書き戻す
			_UpdateEventEffect(m_popupRichEdit.GetOriginalEffectRichEdit(), (LPCWSTR)text);
		}

		m_popupRichEdit.ShowWindow(SW_HIDE);
		m_popupRichEdit.SetOriginalEffectRichEdit(NULL);
	}
	return 0;
}

// スクリーンショット 右クリック
void CMainDlg::OnScreenShotButtonUp(UINT nFlags, CPoint point)
{
	auto ssFolderPath = GetExeDirectory() / L"screenshot";
	if (fs::is_directory(m_config.screenShotFolder)) {
		ssFolderPath = m_config.screenShotFolder;
	}
	::ShellExecute(NULL, NULL, ssFolderPath.c_str(), NULL, NULL, SW_NORMAL);
}

bool CMainDlg::_ReloadUmaMusumeLibrary()
{
	if (!m_umaEventLibrary.LoadUmaMusumeLibrary()) {
		ERROR_LOG << L"LoadUmaMusumeLibrary failed";
		ATLASSERT(FALSE);
		return false;
	} else {
		// 育成ウマ娘のリストをコンボボックスに追加
		m_cmbUmaMusume.ResetContent();

		const std::wstring& currentIkuseiUmaMusume = m_umaEventLibrary.GetCurrentIkuseiUmaMusume();

		CString currentProperty;
		for (const auto& uma : m_umaEventLibrary.GetIkuseiUmaMusumeEventList()) {
			if (currentProperty != uma->property.c_str()) {
				currentProperty = uma->property.c_str();
				m_cmbUmaMusume.AddString(currentProperty);
			}

			int n = m_cmbUmaMusume.AddString(uma->name.c_str());
			if (uma->name == currentIkuseiUmaMusume) {
				m_cmbUmaMusume.SetCurSel(n);
			}
		}
	}
	return true;
}
void CMainDlg::_InitRaceListWindow(){
	INFO_LOG << L"initializing racelist windows...";

	// レース一覧ウィンドウの位置を保存しておく＆非表示化
	m_raceListWindow.ShowWindow(false);

	// 子ウィンドウ化
	m_raceListWindow.ModifyStyle(WS_POPUPWINDOW | WS_CAPTION, WS_CHILD);
	m_raceListWindow.SetParent(m_hWnd);

	// RaceListWindowの位置を調節
	CRect rcOptGroup;
	GetDlgItem(IDC_STATIC_OPT_GROUP).GetClientRect(&rcOptGroup);
	GetDlgItem(IDC_STATIC_OPT_GROUP).MapWindowPoints(m_hWnd, &rcOptGroup);
	m_raceListWindow.SetWindowPos(NULL, rcOptGroup.right, 0, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	m_bShowRaceList = !m_bShowRaceList;
	OnShowHideRaceList(0, 0, NULL);
}

// レース一覧のためにウィンドウの幅を伸ばしたり縮めたりする
void CMainDlg::_ExtentOrShrinkWindow(bool bExtent)
{
	CRect rcWindow;
	GetWindowRect(&rcWindow);

	int windowWidth = 0;
	if (bExtent) {
		ATLASSERT(IsChild(m_raceListWindow));
		CRect rcClientGroup;
		CWindow wndRaceListGroup = m_raceListWindow.GetDlgItem(IDC_STATIC_RACELIST_GROUP);
		wndRaceListGroup.GetClientRect(&rcClientGroup);
		wndRaceListGroup.MapWindowPoints(m_hWnd, &rcClientGroup);
		windowWidth = rcClientGroup.right;

		m_raceListWindow.SetWindowPos(NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	} else {
		CRect rcCtrl;
		GetDlgItem(IDC_BUTTON_SHOWHIDE_RACELIST).GetClientRect(&rcCtrl);
		GetDlgItem(IDC_BUTTON_SHOWHIDE_RACELIST).MapWindowPoints(m_hWnd, &rcCtrl);
		windowWidth = rcCtrl.right;

		m_raceListWindow.SetWindowPos(NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW);
	}
	//AdjustWindowRectEx(&rcCtrl, GetStyle(), FALSE, GetExStyle());

	enum { kMargin = 24 };
	windowWidth += kMargin;
	SetWindowPos(NULL, 0, 0, windowWidth, rcWindow.Height(), SWP_NOMOVE | SWP_NOZORDER);
}

void CMainDlg::_UpdateEventOptions(const UmaEventLibrary::UmaEvent& umaEvent)
{
	const size_t count = umaEvent.eventOptions.size();
	for (size_t i = 0; i < count; ++i) {
		const int IDC_OPTION = IDC_EDIT_OPTION1 + i;
		const int IDC_EFFECT = IDC_EDIT_EFFECT1 + i;
		GetDlgItem(IDC_OPTION).SetWindowText(umaEvent.eventOptions[i].option.c_str());
		//GetDlgItem(IDC_EFFECT).SetWindowText(umaEvent.eventOptions[i].effect.c_str());
		_UpdateEventEffect(GetDlgItem(IDC_EFFECT).m_hWnd, umaEvent.eventOptions[i].effect);

	}
}

void CMainDlg::_UpdateEventEffect(CRichEditCtrl richEdit, const std::wstring& effectText)
{
	richEdit.SetWindowText(effectText.c_str());

	auto funcChangeTextColor = [this](CRichEditCtrl richEdit, CString searchText, COLORREF textColor) 
	{
		// 選択範囲のテキストフォーマット
		CHARFORMAT2W cf = {};
		cf.dwMask = CFM_COLOR | CFM_WEIGHT;
		cf.crTextColor = textColor;
		cf.wWeight = FW_BOLD;

		const int textLength = richEdit.GetTextLength();

		FINDTEXTEXW ft = {};
		ft.chrg.cpMax = -1;
		ft.lpstrText = searchText;
		while (LONG pos = richEdit.FindTextW(FR_DOWN, ft) != -1) {

			// 後ろに数字があれば選択範囲を拡大させる
			++ft.chrgText.cpMax;
			richEdit.SetSel(ft.chrgText.cpMin, ft.chrgText.cpMax);

			CString selText;
			richEdit.GetSelText(selText);
			if (selText.GetLength()) {
				while (std::iswdigit(selText[selText.GetLength() - 1])) {
					++ft.chrgText.cpMax;
					if (textLength < ft.chrgText.cpMax) {
						break;
					}
					richEdit.SetSel(ft.chrgText.cpMin, ft.chrgText.cpMax);
					richEdit.GetSelText(selText);
				}
			}
			--ft.chrgText.cpMax;
			richEdit.SetSel(ft.chrgText.cpMin, ft.chrgText.cpMax);
			richEdit.SetSelectionCharFormat(cf);

			ft.chrg.cpMin = ft.chrgText.cpMax;	// 次へ
		}
	};

	// 獲得スキルの詳細を追記する
	CString kSkillDetailHeader = m_config.i18n.GetCSText(STR_SKILL_EFFECT);
	constexpr LPCWSTR kSkillSeparetor = L"····················";
	if (effectText.find(kSkillDetailHeader) == std::wstring::npos) {
		std::unordered_set<std::wstring> setSkillName;
		bool bFirst = true;
		std::wregex rx(LR"(『(.+?)』)");
		for (std::wsregex_iterator it(effectText.begin(), effectText.end(), rx), end; it != end; ++it) {
			std::wstring skillName = it->str(1);
			if (setSkillName.find(skillName) != setSkillName.end()) {
				continue;	// 同じスキル名は追加しない
			}

			if (auto optSkillEffect = m_skillLibrary.SearchSkillEffect(skillName)) {
				CString appendText;
				appendText.Format(L"\r\n%s\r\n[%s]\r\n%s",
					bFirst ? kSkillDetailHeader : kSkillSeparetor, skillName.c_str(), optSkillEffect->c_str());
				bFirst = false;
				richEdit.AppendText(appendText);
				setSkillName.insert(skillName);
			}
		}
	}

	// ステータス上昇降下へ色を付ける
	funcChangeTextColor(richEdit, L"+", m_effectStatusInc);
	funcChangeTextColor(richEdit, L"-", m_effectStatusDec);
	funcChangeTextColor(richEdit, L"<WARN>", m_effectStatusDec);
	
	
	richEdit.SetSel(0, 0);
}

std::unique_ptr<Gdiplus::Bitmap> CMainDlg::_ScreenShotUmaWindow()
{
	std::unique_lock lock(m_mtxScreennShotWindow);
	if (!m_screenshotWindow) {
		switch (m_config.screenCaptureMethod) {
		case Config::kGDI:
			m_screenshotWindow.reset(new GDIWindowCapture());
			break;

		case Config::kDesktopDuplication:
			m_screenshotWindow.reset(new DesktopDuplication());
			break;

		case Config::kWindowsGraphicsCapture:
			if (WindowsGraphicsCaptureWrapper::IsDllLoaded()) {
				m_screenshotWindow.reset(WindowsGraphicsCaptureWrapper::CreateWindowsGraphicsCapture());
			} else {
				m_screenshotWindow.reset(new GDIWindowCapture());
			}
			break;

		default:
			ERROR_LOG << L"m_config.screenCaptureMethod is unknown";
			ATLASSERT(FALSE);
			m_screenshotWindow.reset(new GDIWindowCapture());
			//return nullptr;
		}
	}

	LPCWSTR className = m_targetClassName.GetLength() ? (LPCWSTR)m_targetClassName : nullptr;
	LPCWSTR windowName = m_targetWindowName.GetLength() ? (LPCWSTR)m_targetWindowName : nullptr;
	CWindow hwndTarget = ::FindWindow(className, windowName);
	if (!hwndTarget) {
		return nullptr;
	}

	auto ss = m_screenshotWindow->ScreenShot(hwndTarget);
	return ss;
}
void CMainDlg::_CheckUmaCruiseU(){
	
	CString versionURL = L"https://cdn.jsdelivr.net/gh/RyoLee/UmaCruise-U@master/appversion.txt";
	CString upgradeURL = L"https://cdn.jsdelivr.net/gh/RyoLee/UmaCruise-U@res/UmaCruise-U.7z";
	if (auto optVersion = HttpDownloadData(versionURL)) {
		std::wstring latestVersion = UTF16fromUTF8(optVersion.get());
		boost::algorithm::trim_all(latestVersion);
		if (latestVersion !=  m_config.kAppVersion) {
			CString msg;
			msg.Format(m_config.i18n.GetCSText(STR_NEW_UCU), latestVersion.c_str(),  m_config.kAppVersion);
			if (MessageBox(msg, L"Update", MB_ICONINFORMATION|MB_YESNO) == IDYES) {
				::ShellExecute(NULL, nullptr, upgradeURL, nullptr, nullptr, SW_NORMAL);
				exit(0);
			}
		}
	} else {
		MessageBox(m_config.i18n.GetCSText(STR_DOWNLOAD_FAILED), L"Error", MB_ICONERROR);
		return;
	}
}
void CMainDlg::_CheckUmaLibrary(I18N::CODE_639_3166 language)
{
	try {
		std::ifstream ifs((GetExeDirectory() / L"UmaLibrary" / "Common.json").wstring());
		ATLASSERT(ifs);
		if (!ifs) {
			MessageBox(m_config.i18n.GetCSText(STR_ERROR_COMMON_LOAD_FAILED));
			return;
		}
		json jsonCommon;
		ifs >> jsonCommon;
		std::string libraryURL;
		if(I18N::CODE_639_3166::ja_JP != language){
			std::string ts = jsonCommon["Common"]["LibraryURL"]["default"];
			libraryURL = ts;
		}
		else{
			std::string ts = jsonCommon["Common"]["LibraryURL"]["ja_JP"];
			libraryURL = ts;
		}
		libraryURL += "?" + std::to_string(std::time(nullptr));	// キャッシュ取得回避
		// ファイルサイズ取得
		auto umaLibraryPath = GetExeDirectory() / L"UmaLibrary" / L"UmaMusumeLibrary.json";
		const DWORD umaLibraryFileSize = static_cast<DWORD>(fs::file_size(umaLibraryPath));

		CUrl downloadUrl(libraryURL.c_str());
		auto hConnect = HttpConnect(downloadUrl);
		auto hRequest = HttpOpenRequest(downloadUrl, hConnect, L"HEAD", L"", true);
		if (HttpSendRequestAndReceiveResponse(hRequest)) {
			int statusCode = HttpQueryStatusCode(hRequest);
			if (statusCode == 200) {
				DWORD contentLength = 0;
				HttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH, contentLength);
				if (umaLibraryFileSize != contentLength) {	// ファイルサイズ比較
					if(MessageBox(m_config.i18n.GetCSText(STR_NEW_LIB), L"Update", MB_ICONINFORMATION|MB_YESNO)==IDYES)
					{
						auto optDLData = HttpDownloadData(downloadUrl.GetURL());
						if (optDLData) {
							fs::remove(umaLibraryPath);
							SaveFile(umaLibraryPath, optDLData.get());
							return;
						} else {
							MessageBox(m_config.i18n.GetCSText(STR_DOWNLOAD_FAILED), L"Error", MB_ICONERROR);
							return;
						}
					}
					else {
						return;
					}
				} else {
					return;
				}
			} else {
				CString errorText;
				errorText.Format(m_config.i18n.GetCSText(STR_ERROR_HTTP_CODE), statusCode);
				MessageBox(errorText, L"Error", MB_ICONERROR);
				return;
			}
		} else {
			MessageBox(m_config.i18n.GetCSText(STR_FAILED), L"Error", MB_ICONERROR);
			return;
		}
	} catch (boost::exception& e) {
		std::string expText = boost::diagnostic_information(e);
		ERROR_LOG << L"CheckUmaLibrary exception: " << (LPCWSTR)CA2W(expText.c_str());
		int a = 0;
	}
	ATLASSERT(FALSE);
	MessageBox(m_config.i18n.GetCSText(STR_ERROR), L"Error", MB_ICONERROR);
}

