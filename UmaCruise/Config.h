#pragma once
#include "I18N.h"
#include "boost/filesystem.hpp"

#ifdef _DEBUG
#define	DEBUG_STRING	L"_Debug"
#else
#define	DEBUG_STRING
#endif
#define VER_BASE L"v1.18"
#define	VER_POSTFIX L"-sp3"

struct Config
{
	int		refreshInterval = 1;
	bool	autoStart = true;
	bool	stopUpdatePreviewOnTraining = true;
	bool 	autoCheckDB = true;
	bool 	autoCheckUpgrade = true;
	bool	notifyFavoriteRaceHold = true;
	enum Theme {
		kAuto, kDark, kLight,
	};
	Theme	theme = kAuto;
	bool	windowTopMost = false;
	boost::filesystem::path screenShotFolder;
	I18N::CODE_639_3166 language = I18N::CODE_639_3166::en_US;

	enum ScreenCaptureMethod {
		kGDI, kDesktopDuplication, kWindowsGraphicsCapture,
	};
	ScreenCaptureMethod	screenCaptureMethod = kGDI;

	bool	LoadConfig();
	void	SaveConfig();
	I18N	i18n;
	CFont	gFont;
	
	LPCWSTR	bAppVersion = VER_BASE;
	LPCWSTR kAppVersion = VER_BASE VER_POSTFIX DEBUG_STRING;
};

