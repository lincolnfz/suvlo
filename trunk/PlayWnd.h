#pragma once

//template<class T , class TBase = CWindow , class TWinTraits = CControlWinTraits >
class CPlayWnd : public CWindowImpl<CPlayWnd/* , TBase , TWinTraits*/>
{
public:
	DECLARE_WND_CLASS(_T("playwnd"))
	/*BEGIN_MSG_MAP_EX(CPlayWnd)
	MSG_WM_RBUTTONUP(OnRButtonUp)
	END_MSG_MAP()*/
	BEGIN_MSG_MAP(CPlayWnd)
	//MESSAGE_HANDLER(WM_RBUTTONUP , OnRButtonUp)
	MESSAGE_HANDLER(WM_ERASEBKGND, OnEraseBkgnd)
	END_MSG_MAP()
	CPlayWnd(void);//:CWindowImpl<T , TBase , TWinTraits>
	//{}
	
	virtual ~CPlayWnd(void);
	//{}

	LRESULT OnRButtonUp( UINT , WPARAM , LPARAM , BOOL );
	LRESULT OnEraseBkgnd( UINT , WPARAM , LPARAM , BOOL );
	//void OnRButtonUp( UINT, CPoint ); 
};

