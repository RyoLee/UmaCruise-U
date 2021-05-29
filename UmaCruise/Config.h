#pragma once

struct Config
{
	int		refreshInterval = 1;
	bool	autoStart = false;
	bool 	autoCheckDB = true;
	bool	stopUpdatePreviewOnTraining = false;
	bool	notifyFavoriteRaceHold = true;
	enum Theme {
		kAuto, kDark, kLight,
	};
	Theme	theme = kAuto;

	bool	LoadConfig();
	void	SaveConfig();
};

