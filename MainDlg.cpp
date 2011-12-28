#include "StdAfx.h"
#include "MainDlg.h"
#include <bkwin/bkcolor.h>
#include "feFilter/common/DefInterface.h"
//#include "feFilter/common/filterUtil.h"

WCHAR filterNam[][20]={
	L"dsNetMedia",
	L"ViderRender",
	L"AudioRender",
	L"AsyncIO",
	L"Parser"
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

HRESULT GetUnconnectedPin(
	IBaseFilter *pFilter,   // Pointer to the filter.
	PIN_DIRECTION PinDir,   // Direction of the pin to find.
	IPin **ppPin)           // Receives a pointer to the pin.
{
	*ppPin = 0;
	IEnumPins *pEnum = 0;
	IPin *pPin = 0;
	HRESULT hr = pFilter->EnumPins(&pEnum);
	if (FAILED(hr))
	{
		return hr;
	}
	while (pEnum->Next(1, &pPin, NULL) == S_OK)
	{
		PIN_DIRECTION ThisPinDir;
		pPin->QueryDirection(&ThisPinDir);
		if (ThisPinDir == PinDir)
		{
			IPin *pTmp = 0;
			hr = pPin->ConnectedTo(&pTmp);
			if (SUCCEEDED(hr))  // Already connected, not the pin we want.
			{
				pTmp->Release();
			}
			else  // Unconnected, this is the pin we want.
			{
				pEnum->Release();
				*ppPin = pPin;
				return S_OK;
			}
		}
		pPin->Release();
	}
	pEnum->Release();
	// Did not find a matching pin.
	return E_FAIL;
}


CMainDlg::CMainDlg(void):CBkDialogImpl<CMainDlg>(IDR_BK_MAIN_DIALOG)
{
	m_hdll_source = NULL;
	m_pFilterGraph = NULL;
	pMainDlg = this;
	BkString::Load(IDR_BK_STRING_DEF); // 加载字符串
	BkFontPool::SetDefaultFont(0, -12); // 设置字体
	BkSkin::LoadSkins(IDR_BK_SKIN_DEF); // 加载皮肤
	BkStyle::LoadStyles(IDR_BK_STYLE_DEF); // 加载风格
	Init2();
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
	if ( SUCCEEDED( m_pFilterGraph->FindFilterByName( filterNam[1] , &pIBaseFilter ) ) )
	{
		IVideoWindow *pIVideoWindow = NULL;
		if ( SUCCEEDED(pIBaseFilter->QueryInterface( IID_IVideoWindow , (void**)&pIVideoWindow )) )
		{
			//pIVideoWindow->put_MessageDrain((OAHWND)hPlay);
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
	m_pFilterGraph->FindFilterByName( filterNam[0] , &pBaseFilter );

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
	//play( "mms://112.65.246.171/easysk/kaodian/2011/km_minfa/minfa_63" );
	//play( "mms://mmc.daumcast.net/mmc/1/500/0902418000208h.wmv" );
	IBaseFilter *pBaseFilter;
	m_pFilterGraph->FindFilterByName( filterNam[3] , &pBaseFilter );
	IFeFileSource *pFeFile;
	pBaseFilter->QueryInterface( IID_IFeFileSource , (void**)&pFeFile );
	pBaseFilter->Release();

	//m_pFilterGraph->FindFilterByName(  )
	pFeFile->Play( L"mms://112.65.246.171/easysk/kaodian/2011/km_minfa/minfa_63" );

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
	HRESULT hr = S_FALSE;
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
	hr = pFactouy->CreateInstance( NULL , IID_IBaseFilter , (void**)&pBaseFilter );
	pFactouy->Release();
	
	if( FAILED( CoCreateInstance( CLSID_FilterGraph , NULL , CLSCTX_INPROC_SERVER, IID_IFilterGraph , (void**)&m_pFilterGraph ) ) )
	{
		return FALSE;
	}
	if( FAILED( m_pFilterGraph->AddFilter( pBaseFilter , filterNam[0] ) ) )
	{
		return FALSE;
	}
	pBaseFilter->Release();

	IBaseFilter *pVideoRenderFilter = NULL;
	if( SUCCEEDED( CoCreateInstance( CLSID_VideoRenderer , NULL , CLSCTX_INPROC_SERVER , IID_IBaseFilter , (void**)&pVideoRenderFilter ) ) )
	{
		m_pFilterGraph->AddFilter( pVideoRenderFilter , filterNam[1] );
		pVideoRenderFilter->Release();
	}

	IBaseFilter *pAudioRenderFilter = NULL;
	if ( SUCCEEDED( CoCreateInstance( CLSID_DSoundRender , NULL , CLSCTX_INPROC_SERVER , IID_IBaseFilter , (void**)&pAudioRenderFilter  ) ) )
	{
		m_pFilterGraph->AddFilter( pAudioRenderFilter , filterNam[2] );
		//pAudioRenderFilter->
		IPinConnection *pConnPin = NULL;
		HRESULT hr = pAudioRenderFilter->QueryInterface( IID_IPinConnection , (void **)&pConnPin );
		pAudioRenderFilter->Release();
	}

	IMediaFilter *pIMediaFilter = NULL;
	if ( SUCCEEDED( m_pFilterGraph->QueryInterface( IID_IMediaFilter , (void **)&pIMediaFilter ) ) )
	{
		IReferenceClock *pRefClock = NULL;
		if ( SUCCEEDED(pAudioRenderFilter->QueryInterface( IID_IReferenceClock , (void**)&pRefClock )) )
		{
			hr = pIMediaFilter->SetSyncSource(pRefClock);
			hr = pVideoRenderFilter->SetSyncSource(pRefClock);
			hr = pAudioRenderFilter->SetSyncSource(pRefClock);
		}
	}
	 	 		
	return TRUE;
}

BOOL CMainDlg::Init2()
{
	HRESULT hr = S_FALSE;
	CString dsFilterLibPath;
	dsFilterLibPath.Format( _T("%s\\AsyncSource.dll") , strAppPath );
	m_hdll_source = CoLoadLibrary( T2OLE(dsFilterLibPath.GetBuffer() ) , FALSE );
	if ( m_hdll_source == NULL )
	{
		return FALSE;
	}
	p_dllgetclassObject = (DLLGETCLASSOBJECT)GetProcAddress( m_hdll_source , "DllGetClassObject" );
	IClassFactory* pFactouy = NULL;
	//得到类工厂
	p_dllgetclassObject( CLSID_AsynSource , IID_IClassFactory , (void**)&pFactouy );
	IBaseFilter* pBaseFilter = NULL;
	hr = pFactouy->CreateInstance( NULL , IID_IBaseFilter , (void**)&pBaseFilter );
	pFactouy->Release();

	if( FAILED( CoCreateInstance( CLSID_FilterGraph , NULL , CLSCTX_INPROC_SERVER, IID_IFilterGraph , (void**)&m_pFilterGraph ) ) )
	{
		return FALSE;
	}
	//async io
	if( FAILED( m_pFilterGraph->AddFilter( pBaseFilter , filterNam[3] ) ) )
	{
		return FALSE;
	}	
	pBaseFilter->Release();

	//parser.dll
	dsFilterLibPath.Format( _T("%s\\Parser.dll") , strAppPath );
	m_hdll_parse = CoLoadLibrary( T2OLE(dsFilterLibPath.GetBuffer() ) , FALSE );
	if ( m_hdll_parse == NULL )
	{
		return FALSE;
	}
	p_dllgetclassObject = (DLLGETCLASSOBJECT)GetProcAddress( m_hdll_parse , "DllGetClassObject" );
	pFactouy = NULL;
	//得到类工厂
	p_dllgetclassObject( CLSID_Parser , IID_IClassFactory , (void**)&pFactouy );
	pBaseFilter = NULL;
	hr = pFactouy->CreateInstance( NULL , IID_IBaseFilter , (void**)&pBaseFilter );
	pFactouy->Release();

	//parser
	if( FAILED( m_pFilterGraph->AddFilter( pBaseFilter , filterNam[4] ) ) )
	{
		return FALSE;
	}
	pBaseFilter->Release();


	//显示输出
	IBaseFilter *pVideoRenderFilter = NULL;
	if( SUCCEEDED( CoCreateInstance( CLSID_VideoRenderer , NULL , CLSCTX_INPROC_SERVER , IID_IBaseFilter , (void**)&pVideoRenderFilter ) ) )
	{
		m_pFilterGraph->AddFilter( pVideoRenderFilter , filterNam[1] );
		pVideoRenderFilter->Release();
	}

	//声音输出
	IBaseFilter *pAudioRenderFilter = NULL;
	if ( SUCCEEDED( CoCreateInstance( CLSID_DSoundRender , NULL , CLSCTX_INPROC_SERVER , IID_IBaseFilter , (void**)&pAudioRenderFilter  ) ) )
	{
		m_pFilterGraph->AddFilter( pAudioRenderFilter , filterNam[2] );
		pAudioRenderFilter->Release();
	}

	IMediaFilter *pIMediaFilter = NULL;
	if ( SUCCEEDED( m_pFilterGraph->QueryInterface( IID_IMediaFilter , (void **)&pIMediaFilter ) ) )
	{
		IReferenceClock *pRefClock = NULL;
		if ( SUCCEEDED(pAudioRenderFilter->QueryInterface( IID_IReferenceClock , (void**)&pRefClock )) )
		{
			hr = pIMediaFilter->SetSyncSource(pRefClock);
			hr = pVideoRenderFilter->SetSyncSource(pRefClock);
			hr = pAudioRenderFilter->SetSyncSource(pRefClock);
		}
	}

	/*
	m_pFilterGraph->FindFilterByName( filterNam[3] , &pBaseFilter );
	IFeFileSource *pFeFile;
	pBaseFilter->QueryInterface( IID_IFeFileSource , (void**)&pFeFile );
	pBaseFilter->Release();

	pFeFile->Play( L"mms://112.65.246.171/easysk/kaodian/2011/km_minfa/minfa_63" );
	*/

	return TRUE;
}

BOOL CMainDlg::Clean()
{
	if( m_pFilterGraph )
	{
		m_pFilterGraph->Release();
	}
	if ( m_hdll_source )
	{
		CoFreeLibrary( m_hdll_source );
	}
	if ( m_hdll_parse )
	{
		CoFreeLibrary( m_hdll_parse );
	}
	return TRUE;
}

