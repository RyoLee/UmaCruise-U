#pragma once
#include "I18N.h"

struct Config
{
	int		refreshInterval = 1;
	bool	autoStart = true;
	bool	stopUpdatePreviewOnTraining = true;
	bool 	autoCheckDB = true;
	bool 	autoCheckUpgrade =true;
	bool	notifyFavoriteRaceHold = true;
	enum Theme {
		kAuto, kDark, kLight,
	};
	Theme	theme = kAuto;
	bool	windowTopMost = false;
	I18N::CODE_639_3166 language = I18N::CODE_639_3166::en_US;
	bool	LoadConfig();
	void	SaveConfig();
};

