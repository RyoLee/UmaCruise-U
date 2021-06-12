#pragma once
#include "I18N.h"

struct Config
{
	int		refreshInterval = 1;
	bool	autoStart = false;
	bool	stopUpdatePreviewOnTraining = false;
	bool 	autoCheckDB = true;
	bool 	autoCheckUpgrade =true;
	bool	notifyFavoriteRaceHold = true;
	enum Theme {
		kAuto, kDark, kLight,
	};
	Theme	theme = kAuto;
	bool	windowTopMost = false;
	I18N::CODE_639_3166 language = I18N::CODE_639_3166::zh_CN;
	bool	LoadConfig();
	void	SaveConfig();
};

