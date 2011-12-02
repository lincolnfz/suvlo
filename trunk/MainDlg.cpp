#include "StdAfx.h"
#include "MainDlg.h"
#include <bkwin/bkcolor.h>

WCHAR filterNam[][20]={
	L"dsNetMedia",
	L"ViderRender"
};

CMainDlg* pMainDlg = NULL;
extern CString strAppPath;

typedef HRESULT ( __stdcall *DLLGETCLASSOBJECT )( const IID& rClsID , const IID& riid , void** pv );

DLLGETCLASSOBJECT p_dllgetclassObject;


unsigned int __stdcall run( void* parm )
{
	char* url = (char*)parm;
	//pMainDlg->m_wrapmms.play(url);
	return 0;
}


CMainDlg::CMainDlg(void):CBkDialogImpl<CMainDlg>(IDR_BK_MAIN_DIALOG)
{
	m_hdll_source = NULL;
	m_pGraphBuilder = NULL;
	pMainDlg = this;
	BkString::Load(IDR_BK_STRING_DEF); // 加载字符串
	BkFontPool::SetDefaultFont(0, -12); // 设置字体
	BkSkin::LoadSkins(IDR_BK_SKIN_DEF); // 加载皮肤
	BkStyle::LoadStyles(IDR_BK_STYLE_DEF); // 加载风格
	Init();
}


CMainDlg::~CMainDlg(void)
{
	Clean();	
}

LRESULT CMainDlg::OnClose()
{
	EndDialog(0);
	return 0;
}

LRESULT CMainDlg::OnMaxWindow()
{
	if (WS_MAXIMIZE == (GetStyle() & WS_MAXIMIZE))
	{
		SendMessage(WM_SYSCOMMAND, SC_RESTORE | HTCAPTION, 0);
		SetItemAttribute(IDC_BTN_SYS_MAX, "skin", "maxbtn");
	}
	else
	{
		SendMessage(WM_SYSCOMMAND, SC_MAXIMIZE | HTCAPTION, 0);
		SetItemAttribute(IDC_BTN_SYS_MAX, "skin", "restorebtn");
	}

	return 0;
}

LRESULT CMainDlg::OnMinWindow()
{
	SendMessage(WM_SYSCOMMAND, SC_MINIMIZE, 0);
	return 0;
}

void CMainDlg::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
		
}

BOOL CMainDlg::OnInitDialog(CWindow wndFocus, LPARAM lInitParam)
{
	HWND hwnd = GetViewHWND();
	HWND hPlay = m_playwnd.Create( hwnd , CRect (0, 0, 0, 0) , NULL , 0 , 0 );
	int ret = m_playwnd.SetDlgCtrlID(IDC_DLG_PLAY);
	DWORD dw = GetLastError();
	//CBkImageSlider* pBKVol = static_cast<CBkImageSlider*>( m_richView.FindChildByCmdID(IDC_VOL) );	 
	
	
	return FALSE;
}



void CMainDlg::play( const char* url )
{
	//设置videorender播放窗口到程序的界面上
	HWND hPlay = m_playwnd.m_hWnd;
	IBaseFilter *pIBaseFilter = NULL;
	if ( SUCCEEDED( m_pGraphBuilder->FindFilterByName( filterNam[1] , &pIBaseFilter ) ) )
	{
		IVideoWindow *pIVideoWindow = NULL;
		if ( SUCCEEDED(pIBaseFilter->QueryInterface( IID_IVideoWindow , (void**)&pIVideoWindow )) )
		{
			pIVideoWindow->put_MessageDrain((OAHWND)hPlay);
			pIVideoWindow->put_Owner( (OAHWND)hPlay );
			pIVideoWindow->put_WindowStyle(WS_CHILD | WS_CLIPSIBLINGS);
			RECT rc;
			::GetClientRect( hPlay , &rc );
			pIVideoWindow->SetWindowPosition(0 , 0, rc.right , rc.bottom);
			pIVideoWindow->put_Visible(OATRUE);
			pIVideoWindow->Release();
		}
	}
	/*HANDLE hThread;
	unsigned threadID;
	hThread = (HANDLE)_beginthreadex( NULL , NULL , &run , (void*)url , 0 , &threadID );
	CloseHandle( hThread );*/
	IBaseFilter *pBaseFilter = NULL;
	m_pGraphBuilder->FindFilterByName( filterNam[0] , &pBaseFilter );

	INetSource *pNetSource = NULL;
	pBaseFilter->QueryInterface( IID_INetSource , (void**)&pNetSource );
	int size = MultiByteToWideChar( CP_ACP , NULL , url , -1 , NULL , 0 );
	WCHAR *pWchar = new WCHAR[size];
	MultiByteToWideChar( CP_ACP , NULL , url , -1 , pWchar , size );
	pNetSource->play( pWchar );
	delete []pWchar;

	pNetSource->Release();
	pBaseFilter->Release();
}

LRESULT CMainDlg::OnPlay_Pause()
{
	play( "mms://112.65.246.171/easysk/kaodian/2011/km_minfa/minfa_63" );
	//play( "mms://mmc.daumcast.net/mmc/1/500/0902418000208h.wmv" );
	return 0L;
}

LRESULT CMainDlg::OnStop()
{
	return 0L;
}

LRESULT CMainDlg::OnMute()
{
	return 0L;
}

//设置音量大小
LRESULT CMainDlg::OnVol()
{
	CBkImageSlider* pBKVol = static_cast<CBkImageSlider*>( m_richView.FindChildByCmdID(IDC_VOL) );
	return 0L;
}

LRESULT CMainDlg::OnUpdatePosition(UINT uMsg, WPARAM wParam, LPARAM lParam)
{

	return 0L;
}

LRESULT CMainDlg::OnValChange(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch( wParam )
	{
	case IDC_VOL:
		{
			
		}
		break;
	case IDC_SLIDER:
		{
			//int64_t iNewSliderPos = static_cast<double>(m_llLength) / static_cast<double>(100) * lParam;
			
		}
		break;
	default:
		break;
	}
	return 0L;
}

BOOL CMainDlg::Init()
{

	CString dsFilterLibPath;
	dsFilterLibPath.Format( _T("%s\\dsNetMedia.dll") , strAppPath );
	m_hdll_source = CoLoadLibrary( T2OLE(dsFilterLibPath.GetBuffer() ) , FALSE );
	if ( m_hdll_source == NULL )
	{
		return FALSE;
	}
	p_dllgetclassObject = (DLLGETCLASSOBJECT)GetProcAddress( m_hdll_source , "DllGetClassObject" );
	IClassFactory* pFactouy = NULL;
	
	//得到类工厂
	p_dllgetclassObject( CLSID_NetMediaSource , IID_IClassFactory , (void**)&pFactouy );
	IBaseFilter* pBaseFilter = NULL;
	HRESULT hr = pFactouy->CreateInstance( NULL , IID_IBaseFilter , (void**)&pBaseFilter );
	pFactouy->Release();
	
	if( FAILED( CoCreateInstance( CLSID_FilterGraph , NULL , CLSCTX_INPROC_SERVER, IID_IGraphBuilder , (void**)&m_pGraphBuilder ) ) )
	{
		return FALSE;
	}
	if( FAILED( m_pGraphBuilder->AddFilter( pBaseFilter , filterNam[0] ) ) )
	{
		return FALSE;
	}
	pBaseFilter->Release();

	IBaseFilter *pVideoRenderFilter = NULL;
	if( SUCCEEDED( CoCreateInstance( CLSID_VideoRenderer , NULL , CLSCTX_INPROC_SERVER , IID_IBaseFilter , (void**)&pVideoRenderFilter ) ) )
	{
		m_pGraphBuilder->AddFilter( pVideoRenderFilter , filterNam[1] );
		pVideoRenderFilter->Release();
	}
	
	
	return TRUE;
}

BOOL CMainDlg::Clean()
{
	if( m_pGraphBuilder )
	{
		m_pGraphBuilder->Release();
	}
	if ( m_hdll_source )
	{
		CoFreeLibrary( m_hdll_source );
	}
	return TRUE;
}

