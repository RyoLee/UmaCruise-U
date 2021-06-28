#include "stdafx.h"
#include "I18N.h"
#include <codecvt>
#include <string>


bool I18N::Load(CODE_639_3166 language)
{
	std::wstring tName = std::wstring(C_CODE_639_3166[language]) + L".json";
	std::ifstream fsd((GetExeDirectory() / L"UmaLibrary" / L"en_US.json").wstring());			//default language
	std::ifstream fst((GetExeDirectory() / L"UmaLibrary" / tName.c_str()).wstring());			//target language
	ATLASSERT(fsd);
	fsd >> data;
	if (!fst) {
		return false;
	}
	nlohmann::json tj;
	fst >> tj;
	for (auto it = tj.begin(); it != tj.end(); it++) {
		data[it.key()] = it.value();
	}
    return true;
}

void I18N::Cover(HWND hDlg,const CFont& gFont)
{
	std::vector<HWND> childWindowList;
	EnumChildWindows(hDlg, [](HWND hwnd, LPARAM lParam) -> BOOL {
		auto pList = (std::vector<HWND>*)lParam;
		pList->emplace_back(hwnd);
		return TRUE;
		}, (LPARAM)&childWindowList);

	for (HWND hwnd : childWindowList) {
		int nID = ::GetDlgCtrlID(hwnd);
		char ptr[1024];
		itoa(nID, ptr, 10);
		CWindow w = ::GetDlgItem(hDlg, nID);
		if (w.IsWindow()) {
			w.SetFont(gFont, true);
			if (data.contains(ptr)) {
				std::string ts = data[ptr];
				CString text = ts.c_str();
				w.SetWindowText(text);
			}
		}
	}
}
CString I18N::GetCSText(int id){
	CString text = GetWSText(id).c_str();
	ATLASSERT(!(text.IsEmpty()||text.GetLength()==0));
	return text;
}
std::wstring convert(const std::string& input)
{
	try
	{
		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		return converter.from_bytes(input);
	}
	catch (std::range_error& e)
	{
		size_t length = input.length();
		std::wstring result;
		result.reserve(length);
		for (size_t i = 0; i < length; i++)
		{
			result.push_back(input[i] & 0xFF);
		}
		return result;
	}
}
std::wstring  I18N::GetWSText(int id){
	char ptr[1024];
	itoa(id, ptr, 10);
	ATLASSERT(data.contains(ptr));
	std::string ts = data[ptr];
	return convert(ts);
}