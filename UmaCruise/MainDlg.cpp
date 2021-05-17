
#include "stdafx.h"
#include "MainDlg.h"

#include <tesseract\baseapi.h>
#include <leptonica\allheaders.h>

#include <opencv2\opencv.hpp>

#include "Utility\CodeConvert.h"
#include "Utility\CommonUtility.h"
#include "Utility\json.hpp"
#include "Utility\timer.h"
#include "Utility\Logger.h"
#include "Utility\WinHTTPWrapper.h"
#include "win32-darkmode\DarkMode.h"

#include "ConfigDlg.h"

using json = nlohmann::json;
using namespace CodeConvert;
using namespace cv;
using namespace WinHTTPWrapper;


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



// android版
bool SaveScreenShot(const std::wstring& device, const std::wstring& filePath)
{
	auto adbPath = GetExeDirectory() / L"platform-tools" / L"adb.exe";
	std::wstring targetDevice;
	if (device.length() > 0) {
		targetDevice = L" -s " + device;
	}

	std::wstring deviceSSPath = L"/sdcard/screen.png";
	//if (g_targetDevice.substr(0, 3) != L"127") {
	//	deviceSSPath = L"/sdcard/Screenshots/screen.png";
	//}

	std::wstring commandLine = targetDevice + L" shell screencap -p " + deviceSSPath;
	DWORD ret = StartProcess(adbPath, commandLine);
	if (ret != 0) {
		return false;
		//throw std::runtime_error("shell screencap failed");
	}

	std::wstring ssPath = filePath;
	commandLine = std::wstring(targetDevice + L" pull " + deviceSSPath + L" \"") + ssPath + L"\"";
	ret = StartProcess(adbPath, commandLine);
	if (ret != 0) {
		return false;
		//throw std::runtime_error("pull /sdcard/screen.png failed");
	}
	return true;
}



/////////////////////////////////////////////////////////////////////////////

CMainDlg::CMainDlg() : m_raceListWindow(m_config)
{
}

BOOL CMainDlg::PreTranslateMessage(MSG* pMsg)
{
	return CWindow::IsDialogMessage(pMsg);
}

void CMainDlg::ChangeWindowTitle(const std::wstring& title)
{
	CString str = L"UmaCruise-U - ";
	str.Format(L"UmaCruise-U %s - %s", kAppVersion, title.c_str());
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
		MessageBox(L"It does not work correctly because the folder name contains unicode.\nPlease move the folder to a shallower level (such as C:\\).", L"Error", MB_ICONERROR);
	}

	m_config.LoadConfig();

	ChangeGlobalTheme(m_config.theme);

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

	// プレビューウィンドウ作成
	m_previewWindow.Create(m_hWnd);
	m_previewWindow.SetNotifyDragdropBounds([this](const CRect& rcBounds) {
		m_rcBounds = rcBounds;
	});

	// UmaMusumeLibraryを読み込み
	if (!m_umaEventLibrary.LoadUmaMusumeLibrary()) {
		ERROR_LOG << L"LoadUmaMusumeLibrary failed";
		ATLASSERT(FALSE);
	} else {
		// 育成ウマ娘のリストをコンボボックスに追加
		CString currentProperty;
		for (const auto& uma : m_umaEventLibrary.GetIkuseiUmaMusumeEventList()) {
			if (currentProperty != uma->property.c_str()) {
				currentProperty = uma->property.c_str();
				m_cmbUmaMusume.AddString(currentProperty);
			}
			m_cmbUmaMusume.AddString(uma->name.c_str());
		}
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
				ChangeWindowTitle(L"Common.json does not exist...");
			} else {
				json jsonCommon;
				ifs >> jsonCommon;

				m_targetWindowName = UTF16fromUTF8(jsonCommon["Common"]["TargetWindow"]["WindowName"].get<std::string>()).c_str();
				m_targetClassName = UTF16fromUTF8(jsonCommon["Common"]["TargetWindow"]["ClassName"].get<std::string>()).c_str();
				bool checkLibrary = jsonCommon["Common"]["UmaMusumeLibrary"]["AutoCheck"];
				if(checkLibrary){
					try {
						std::string libraryURL = jsonCommon["Common"]["UmaMusumeLibrary"]["URL"];
						auto umaLibraryPath = GetExeDirectory() / L"UmaLibrary" / L"UmaMusumeLibrary.json";
						const DWORD umaLibraryFileSize = static_cast<DWORD>(fs::file_size(umaLibraryPath));
						CUrl downloadUrl(libraryURL.c_str());
						auto hConnect = HttpConnect(downloadUrl);
						auto hRequest = HttpOpenRequest(downloadUrl, hConnect, L"HEAD");
						if (HttpSendRequestAndReceiveResponse(hRequest)) {
							int statusCode = HttpQueryStatusCode(hRequest);
							if (statusCode == 200) {
								DWORD contentLength = 0;
								HttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH, contentLength);
								if (umaLibraryFileSize != contentLength) {
									MessageBox(L"UmaMusumeLibrary.json\nA new version avaliable!", L"Update", MB_ICONINFORMATION);
								}
							}
						}
					}
					catch (boost::exception& e) {
						std::string expText = boost::diagnostic_information(e);
						ERROR_LOG << L"OnCheckUmaLibrary exception: " << (LPCWSTR)CA2W(expText.c_str());
					}
				}
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

				m_bShowRaceList = jsonSetting["MainDlg"].value<bool>("ShowRaceList", m_bShowRaceList);
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
		_DockOrPopupRaceListWindow();

		DoDataExchange(DDX_LOAD);

	} catch (std::exception& e)
	{
		ATLTRACE(L"%s\n", (LPCWSTR)(CA2W(e.what())));
		ERROR_LOG << L"LoadConfig failed: " << (LPCWSTR)(CA2W(e.what()));
		ATLASSERT(FALSE);
	}
	ChangeWindowTitle(L"init suscess!");

	if (m_config.autoStart) {
		CButton(GetDlgItem(IDC_CHECK_START)).SetCheck(BST_CHECKED);
		OnStart(0, 0, NULL);
	}

	DarkModeInit();

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
		::Sleep(2 * 1000);
	}

	return 0;
}

LRESULT CMainDlg::OnAppAbout(WORD, WORD, HWND, BOOL&)
{
	CAboutDlg dlg(m_previewWindow);
	dlg.DoModal();
	return 0;
}

// レースリストの表示を切り替え
void CMainDlg::OnShowHideRaceList(UINT uNotifyCode, int nID, CWindow wndCtl)
{
	m_bShowRaceList = !m_bShowRaceList;

	if (m_config.popupRaceListWindow) {
		m_raceListWindow.ShowWindow(m_bShowRaceList);

	} else {
		_ExtentOrShrinkWindow(m_bShowRaceList);	
	}
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
	const bool prevPopupRaceListWindow = m_config.popupRaceListWindow;
	const int prevTheme = m_config.theme;
	ConfigDlg dlg(m_config);
	auto ret = dlg.DoModal(m_hWnd);
	if (ret == IDOK) {
		if (prevPopupRaceListWindow != m_config.popupRaceListWindow) {
			_DockOrPopupRaceListWindow();
		}
		if (prevTheme != m_config.theme) {
			ChangeGlobalTheme(m_config.theme);
			OnThemeChanged();
			m_raceListWindow.OnThemeChanged();
			m_previewWindow.OnThemeChanged();
		}
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
		if (m_config.popupRaceListWindow) {
			m_bDockingMove = funcDockingMove();
		}
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

			m_eventSource = m_umaEventLibrary.GetLastEventSource().c_str();
			DoDataExchange(DDX_LOAD, IDC_EDIT_EVENT_SOURCE);
		}


		// 現在ターン
		m_raceListWindow.AnbigiousChangeCurrentTurn(m_umaTextRecoginzer.GetCurrentTurn());

		// レース距離
		m_raceListWindow.EntryRaceDistance(m_umaTextRecoginzer.GetEntryRaceDistance());

		//++count;
		CString title;
		title.Format(L"Processing time: %s", UTF16fromUTF8(timer.format()).c_str());
		ChangeWindowTitle((LPCWSTR)title)
			;
	} else {
		LPCWSTR className = m_targetClassName.GetLength() ? (LPCWSTR)m_targetClassName : nullptr;
		LPCWSTR windowName = m_targetWindowName.GetLength() ? (LPCWSTR)m_targetWindowName : nullptr;
		HWND hwndTarget = ::FindWindow(className, windowName);
		if (!hwndTarget) {
			ChangeWindowTitle(L"Umamusume's window not found...");
			return;
		}
		auto ssFolderPath = GetExeDirectory() / L"screenshot";
		if (!fs::is_directory(ssFolderPath)) {
			fs::create_directory(ssFolderPath);
		}

		auto ssPath = ssFolderPath / (L"screenshot_" + std::to_wstring(std::time(nullptr)) + L".png");
		if (GetKeyState(VK_SHIFT) < 0) {
			ssPath = ssFolderPath / L"screenshot.png";
		}
		// 
		auto image = m_umaTextRecoginzer.ScreenShot();
		if (!image) {
			ChangeWindowTitle(L"Screenshot failed...");
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
		btnStart.SetWindowText(L"Stop");
		m_cancelAutoDetect = false;
		m_threadAutoDetect = std::thread([this]()
		{
			INFO_LOG << L"thread begin";

			int count = 0;
			while (!m_cancelAutoDetect.load()) {
				Utility::timer timer;

				const auto begin = std::chrono::steady_clock::now();

				auto ssImage = m_umaTextRecoginzer.ScreenShot();
				bool success = m_umaTextRecoginzer.TextRecognizer(ssImage.get());
				if (success) {
					bool updateImage = true;
					if (m_config.stopUpdatePreviewOnTraining && !m_umaTextRecoginzer.IsTrainingMenu()) {
						updateImage = false;
					}
					if (updateImage) {
						m_previewWindow.UpdateImage(ssImage.release());
					}

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

						m_eventSource = m_umaEventLibrary.GetLastEventSource().c_str();
						DoDataExchange(DDX_LOAD, IDC_EDIT_EVENT_SOURCE);
					}


					// 現在ターン
					m_raceListWindow.AnbigiousChangeCurrentTurn(m_umaTextRecoginzer.GetCurrentTurn());

					// レース距離
					m_raceListWindow.EntryRaceDistance(m_umaTextRecoginzer.GetEntryRaceDistance());

					++count;
					CString title;
					title.Format(L"scan: %d %s", count, (LPCWSTR)CA2W(timer.format(3, "[%ws]").c_str()));
					ChangeWindowTitle((LPCWSTR)title);

					// wait
					const auto milisecInterval = m_config.refreshInterval * 1000;
					auto end = std::chrono::steady_clock::now();
					auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
					do {
						::Sleep(50);
						end = std::chrono::steady_clock::now();
						elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
					} while (elapsed < milisecInterval);

				} else {
					if (!ssImage) {
						ChangeWindowTitle(L"Umamusume's window not found...");
					} else {
						ChangeWindowTitle(L"Failed...");
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
			if (m_threadAutoDetect.joinable()) {
				CButton btnStart = GetDlgItem(IDC_CHECK_START);
				btnStart.SetWindowText(L"Start");
				btnStart.EnableWindow(TRUE);
				m_threadAutoDetect.detach();
			}
		});
		//OnTimer(kAutoOCRTimerID);
		//SetTimer(kAutoOCRTimerID, kAutoOCRTimerInterval);
	} else {
		if (m_threadAutoDetect.joinable()) {
			btnStart.SetWindowText(L"Stopping...");
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

		m_eventSource = m_umaEventLibrary.GetLastEventSource().c_str();
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
		MessageBox(L"Event is empty.");
		return;
	}
	if (m_eventSource.IsEmpty()) {
		MessageBox(L"Source is empty.");
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
		MessageBox(L"Options are empty.");
		return;
	}

	CString msg;
	msg.Format(L"Correct options for event [%s].\nAre you sure?", (LPCWSTR)m_eventName);
	if (MessageBox(msg, L"Yes", MB_YESNO) == IDNO) {
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
			MessageBox(L"Open failed:UmaMusumeLibraryRevision.json");
			return;
		}
		ofs << jsonRevisionLibrary.dump(2);
		ofs.close();

		m_umaEventLibrary.LoadUmaMusumeLibrary();
		MessageBox(L"Corrected", L"Success");
	}
}


// レース一覧をメインダイアログにドッキングさせるか、ポップアップウィンドウ化させる
void CMainDlg::_DockOrPopupRaceListWindow()
{
	if (!m_config.popupRaceListWindow) {
		// docking
		INFO_LOG << L"docking";

		// レース一覧ウィンドウの位置を保存しておく＆非表示化
		m_raceListWindow.ShowWindow(false);

		// 子ウィンドウ化
		m_raceListWindow.ModifyStyle(WS_POPUPWINDOW | WS_CAPTION, WS_CHILD);
		m_raceListWindow.SetParent(m_hWnd);

		// RaceListWindowの位置を調節
		CRect rcShowHideButton;
		GetDlgItem(IDC_BUTTON_SHOWHIDE_RACELIST).GetClientRect(&rcShowHideButton);
		GetDlgItem(IDC_BUTTON_SHOWHIDE_RACELIST).MapWindowPoints(m_hWnd, &rcShowHideButton);
		m_raceListWindow.SetWindowPos(NULL, rcShowHideButton.right, 0, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	} else {	
		// popup
		INFO_LOG << L"popup";

		// ポップアップウィンドウ化
		m_raceListWindow.ModifyStyle(WS_CHILD, WS_POPUPWINDOW | WS_CAPTION);
		m_raceListWindow.SetParent(NULL);

		// メインダイアログの幅を縮小させる
		_ExtentOrShrinkWindow(false);
	}
	// レース一覧の表示/非表示を復元
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
	enum { kRightMargin = 23 };
	windowWidth += kRightMargin;
	SetWindowPos(NULL, 0, 0, windowWidth, rcWindow.Height(), SWP_NOMOVE | SWP_NOZORDER);
}

void CMainDlg::_UpdateEventOptions(const UmaEventLibrary::UmaEvent& umaEvent)
{
	const size_t count = umaEvent.eventOptions.size();
	for (size_t i = 0; i < count; ++i) {
		const int IDC_OPTION = IDC_EDIT_OPTION1 + i;
		const int IDC_EFFECT = IDC_EDIT_EFFECT1 + i;
		GetDlgItem(IDC_OPTION).SetWindowText(umaEvent.eventOptions[i].option.c_str());
		GetDlgItem(IDC_EFFECT).SetWindowText(umaEvent.eventOptions[i].effect.c_str());

	}
}


