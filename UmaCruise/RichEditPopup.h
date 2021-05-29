#pragma once

#include "DarkModeUI.h"

class RichEditPopup : 
	public CWindowImpl<RichEditPopup>,
	public DarkModeUI<RichEditPopup>
{
public:

	void	SetFont(HFONT font) {
		m_font = font;
	}

	HWND	Create(HWND hwndParent);

	CRichEditCtrl	GetRichEdit() {
		return m_effectEdit;
	}

	void	SetOriginalEffectRichEdit(CRichEditCtrl richEdit) {
		m_originalEffectEdit = richEdit;
	}

	CRichEditCtrl	GetOriginalEffectRichEdit() {
		return m_originalEffectEdit;
	}

	void	SetOriginalEffectRichEditRect(const CRect& rc) {
		m_rcOriginalEffectRichEdit = rc;
	}

	BEGIN_MSG_MAP_EX(RichEditPopup)
		MSG_WM_CREATE(OnCreate)
		MSG_WM_DESTROY(OnDestroy)
		MSG_WM_SIZE(OnSize)
		MSG_WM_SETCURSOR(OnSetCursor)

		CHAIN_MSG_MAP(DarkModeUI<RichEditPopup>)
	END_MSG_MAP()

	int OnCreate(LPCREATESTRUCT lpCreateStruct);
	void OnDestroy();
	void OnSize(UINT nType, CSize size);
	BOOL OnSetCursor(CWindow wnd, UINT nHitTest, UINT message);

private:
	CFont	m_font;
	CRichEditCtrl	m_effectEdit;
	CRichEditCtrl	m_originalEffectEdit;
	CRect	m_rcOriginalEffectRichEdit;
};

