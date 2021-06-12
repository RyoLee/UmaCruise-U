#pragma once
#include "Utility\json.hpp"
#include <atlstr.h>

struct I18N
{
	nlohmann::json	data;
	void 	Cover(HWND hDlg, int cID);
	enum CODE_639_3166 {
		zh_CN, zh_TW, en_US, ja_JP,
	};
	const CString C_CODE_639_3166[4] = {
		"zh_CN",
		"zh_TW",
		"en_US",
		"ja_JP"
	};
	bool	Load(CODE_639_3166 language);
	CString GetText(int id);
};