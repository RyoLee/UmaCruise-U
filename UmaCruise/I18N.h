#pragma once
#include "Utility\json.hpp"
#include <atlstr.h>
#include <wtl\atlapp.h>
#include <wtl\atlgdi.h>

struct I18N
{
	nlohmann::json data;
	void Cover(HWND hDlg,const CFont& gFont);
	static const int kMAXLanguage = 4;
	enum CODE_639_3166
	{
		zh_CN,
		zh_TW,
		en_US,
		ja_JP,
	};
	const LPCWSTR C_CODE_639_3166[kMAXLanguage] = {
		L"zh_CN",
		L"zh_TW",
		L"en_US",
		L"ja_JP"};
	bool Load(CODE_639_3166 language);
	CString GetCSText(int id);
	std::wstring  GetWSText(int id);
};