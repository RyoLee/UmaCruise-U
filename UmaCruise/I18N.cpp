#include "stdafx.h"
#include "I18N.h"

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

void I18N::Cover(HWND hDlg, int cID)
{
	CString text = GetText(cID);
	CWindow cw = GetDlgItem(hDlg, cID);
	cw.SetWindowText(text);
}

CString I18N::GetText(int id){
	CString csId;
	csId.Format(_T("%d"), id);
	CString text = data[csId];
	ATLASSERT(!(text.IsEmpty()||text.GetLength()==0));
	return text;
}
