#include "stdafx.h"
#include "Config.h"

#include <fstream>

#include "Utility\CodeConvert.h"
#include "Utility\CommonUtility.h"
#include "Utility\json.hpp"

using json = nlohmann::json;
using namespace CodeConvert;


bool Config::LoadConfig()
{
	if(gFont.IsNull()){
		gFont.CreatePointFont(90,L"Segoe UI");
	}
	std::ifstream fs((GetExeDirectory() / "setting.json").wstring());
	if (!fs) {
		return true;
	}
	json jsonSetting;
	fs >> jsonSetting;

	if (jsonSetting["Config"].is_null()) {
		return true;	// default
	}

	refreshInterval = jsonSetting["Config"].value("RefreshInterval", refreshInterval);
	autoStart = jsonSetting["Config"].value("AutoStart", autoStart);
	stopUpdatePreviewOnTraining = jsonSetting["Config"].value("StopUpdatePreviewOnTraining", stopUpdatePreviewOnTraining);
	autoCheckDB = jsonSetting["Config"].value("AutoCheckDB", autoCheckDB);
	autoCheckUpgrade = jsonSetting["Config"].value("AutoCheckUpgrade", autoCheckUpgrade);
	notifyFavoriteRaceHold = jsonSetting["Config"].value("NotifyFavoriteRaceHold", notifyFavoriteRaceHold);
	theme = jsonSetting["Config"].value("Theme", theme);
	windowTopMost = jsonSetting["Config"].value("WindowTopMost", windowTopMost);
	language = jsonSetting["Config"].value("Language", language);
	screenShotFolder = UTF16fromUTF8(jsonSetting["Config"].value("ScreenShotFolder", ""));
	i18n.Load(language);
    return true;
}

void Config::SaveConfig()
{
	json jsonSetting;
	std::ifstream fs((GetExeDirectory() / "setting.json").wstring());
	if (fs) {
		fs >> jsonSetting;
	}
	fs.close();

	jsonSetting["Config"]["RefreshInterval"] = refreshInterval;
	jsonSetting["Config"]["AutoStart"] = autoStart;
	jsonSetting["Config"]["StopUpdatePreviewOnTraining"] = stopUpdatePreviewOnTraining;
	jsonSetting["Config"]["AutoCheckDB"] = autoCheckDB;
	jsonSetting["Config"]["AutoCheckUpgrade"] = autoCheckUpgrade;	
	jsonSetting["Config"]["NotifyFavoriteRaceHold"] = notifyFavoriteRaceHold;
	jsonSetting["Config"]["Theme"] = theme;
	jsonSetting["Config"]["WindowTopMost"] = windowTopMost;
	jsonSetting["Config"]["ScreenShotFolder"] = UTF8fromUTF16(screenShotFolder.wstring());
	jsonSetting["Config"]["Language"] = language;

	std::ofstream ofs((GetExeDirectory() / "setting.json").wstring());
	ofs << jsonSetting.dump(4);
}
