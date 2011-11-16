#include "StdAfx.h"
#include "PlayWnd.h"


CPlayWnd::CPlayWnd(void)
{
}


CPlayWnd::~CPlayWnd(void)
{
}

LRESULT CPlayWnd::OnRButtonUp( UINT uMsg , WPARAM wParam, LPARAM lParam, BOOL bHandled )
{
	CPoint pt( LOWORD(lParam) , HIWORD(lParam) );
	return 0L;
}

LRESULT CPlayWnd::OnEraseBkgnd( UINT uMsg , WPARAM wParam, LPARAM lParam, BOOL bHandled )
{
	HBRUSH hbrush = ::CreateSolidBrush( RGB(0,0,0) );
	HDC hdc = (HDC)wParam;
	CRect rc;
	this->GetClientRect( &rc );
	::FillRect( hdc , &rc , hbrush );
	DeleteObject( hbrush );
	
	return 0L;
}

//void CPlayWnd::OnRButtonUp( UINT wparm , CPoint point )
//{
//
//}