#include "StdAfx.h"
#include "NetSoruceFilter.h"
#include "common.h"

//滤波器名称
WCHAR filterNam[][20]={
	L"dsNetMedia",
	L"ViderRender"
};

#define  DEFAULT_WIDTH 1920
#define DEFAULT_HEIGHT 1080

CVideoStreamPin::CVideoStreamPin(HRESULT *phr, CSource *pFilter)
	:CSourceStream(NAME("Push net Source"), phr, pFilter, L"Video_Out")
{

	
}

CVideoStreamPin::~CVideoStreamPin()
{

}

//填充样本
HRESULT CVideoStreamPin::FillBuffer(IMediaSample *pSamp)
{
	//CWrapFFMpeg* pWrapFFmpeg = m_wrapmms
	CAutoLock cAutoLock( &m_cSharedState );
	CheckPointer(pSamp, E_POINTER);
	return S_OK;
}

//申明大小
HRESULT CVideoStreamPin::DecideBufferSize( IMemAllocator * pAlloc, __inout ALLOCATOR_PROPERTIES * ppropInputRequest)
{
	HRESULT hr;
	CAutoLock cAutoLock(m_pFilter->pStateLock());

	CheckPointer(pAlloc, E_POINTER);
	CheckPointer(ppropInputRequest, E_POINTER);

	VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER*) m_mt.Format();

	// Ensure a minimum number of buffers
	if (ppropInputRequest->cBuffers == 0)
	{
		ppropInputRequest->cBuffers = 2;
	}
	ppropInputRequest->cbBuffer = pvi->bmiHeader.biSizeImage;

	ALLOCATOR_PROPERTIES Actual;
	hr = pAlloc->SetProperties(ppropInputRequest, &Actual);
	if (FAILED(hr)) 
	{
		return hr;
	}

	// Is this allocator unsuitable?
	if (Actual.cbBuffer < ppropInputRequest->cbBuffer) 
	{
		return E_FAIL;
	}

	return S_OK;
}

//比较是否是允许的mediatype
HRESULT CVideoStreamPin::GetMediaType(__inout CMediaType *pMediaType)
{
	CAutoLock cAutoLock(m_pFilter->pStateLock());
	CheckPointer(pMediaType, E_POINTER);

	VIDEOINFOHEADER   vih; 
	memset(   &vih,   0,   sizeof(   vih   )   ); 
	vih.bmiHeader.biCompression   =   MAKEFOURCC( 'I', '4', '2', '0'); 
	vih.bmiHeader.biBitCount         =   16; //每个像素的位数
	vih.bmiHeader.biSize                   =   sizeof(BITMAPINFOHEADER);  //bitmapinfoheader结构体的长度
	vih.bmiHeader.biWidth                 =   DEFAULT_WIDTH;//Your   size.x 
	vih.bmiHeader.biHeight               =   DEFAULT_HEIGHT;//Your   size.y 
	vih.bmiHeader.biPlanes               =   1;		//填为1
	vih.bmiHeader.biSizeImage         =   GetBitmapSize(&vih.bmiHeader); //实际的图像数据占用的字节数
	vih.bmiHeader.biClrUsed = 0; //调色板中实际使用的颜色数,这个值通常为0，表示使用biBitCount确定的全部颜色
	vih.bmiHeader.biClrImportant   =   0; //指定本图象中重要的颜色数，如果该值为零，则认为所有的颜色都是重要的。

	pMediaType-> SetType(&MEDIATYPE_Video); //Major Types
	pMediaType-> SetSubtype(&MEDIASUBTYPE_YUY2); //sub type
	pMediaType-> SetFormatType(&FORMAT_VideoInfo);  //formattype
	pMediaType-> SetFormat(   (BYTE*)   &vih,   sizeof(   vih   )   ); 
	pMediaType-> SetSampleSize(vih.bmiHeader.biSizeImage);


	return S_OK;
}

//音频接口
CAudioStreamPin::CAudioStreamPin(HRESULT *phr, CSource *pFilter)
	:CSourceStream(NAME("Push net Source"), phr, pFilter, L"Audio_Out")
{
	
}

CAudioStreamPin::~CAudioStreamPin()
{

}

HRESULT CAudioStreamPin::FillBuffer(IMediaSample *pSamp)
{
	BYTE *pData;
	long cbData;
	CAutoLock cAutoLock( &m_cSharedState );
	CheckPointer(pSamp, E_POINTER);
	pSamp->GetPointer(&pData);
	cbData = pSamp->GetSize();
	return S_OK;
}

HRESULT CAudioStreamPin::DecideBufferSize( IMemAllocator * pAlloc, __inout ALLOCATOR_PROPERTIES * ppropInputRequest)
{
	HRESULT hr;
	CAutoLock cAutoLock(m_pFilter->pStateLock());
	CheckPointer(pAlloc, E_POINTER);
	CheckPointer(ppropInputRequest, E_POINTER);
	m_mt.Format();
	

	return S_OK;
}

HRESULT CAudioStreamPin::GetMediaType(__inout CMediaType *pMediaType)
{
	CAutoLock cAutoLock(m_pFilter->pStateLock());
	CheckPointer(pMediaType, E_POINTER);

	return S_OK;
}

CNetSourceFilter::CNetSourceFilter(IUnknown *pUnk, HRESULT *phr)
 :CSource( NAME("net protocol source") , pUnk , CLSID_NetMediaSource )
{
	m_pVideoPin = new CVideoStreamPin( phr , this );
	m_pAudioPin = new CAudioStreamPin( phr , this );
	//m_pVideoPin->AddRef();
	//m_pAudioPin->AddRef();
	if (phr)
	{
		if ( m_pVideoPin == NULL || m_pAudioPin == NULL )
			*phr = E_OUTOFMEMORY;
		else
			*phr = S_OK;
	}

}


CNetSourceFilter::~CNetSourceFilter(void)
{
	//delete m_pVideoPin; 
	//delete m_pAudioPin;
	/*if( m_pVideoPin )
	{
		m_pVideoPin->Release();
	}
	if ( m_pAudioPin )
	{
		m_pAudioPin->Release();
	}*/
	
}

CUnknown* __stdcall CNetSourceFilter::CreateInstance(LPUNKNOWN pUnk, HRESULT *phr)
{
	CNetSourceFilter* pNew = new CNetSourceFilter( pUnk , phr  );
	if( pNew == NULL )
	{
		*phr = E_OUTOFMEMORY;
	}
	else
	{
		//pNew->AddRef();
		*phr = S_OK;
	}
	return pNew;
}

STDMETHODIMP CNetSourceFilter::play(LPCWSTR url)
{
	DWORD size = WideCharToMultiByte( CP_OEMCP,NULL,url,-1,NULL,0,NULL,FALSE );
	char *lpurl = new char[size];
	WideCharToMultiByte (CP_OEMCP,NULL,url,-1,lpurl,size,NULL,FALSE);
	//m_wrapmms.play( lpurl );
	delete []lpurl;
	IGraphBuilder *pGraphBuilder = NULL;
	IFilterGraph *pFilterGraph = GetFilterGraph();
	if ( pFilterGraph )
	{
		if ( SUCCEEDED( pFilterGraph->QueryInterface( IID_IGraphBuilder , (void **)&pGraphBuilder ) ) )
		{
			//pGraphBuilder->Render( m_pVideoPin );
			IBaseFilter* pBaseFilter = NULL;
			if ( SUCCEEDED( pGraphBuilder->FindFilterByName( filterNam[1] , &pBaseFilter ) ) )
			{			
				pBaseFilter->Release();
			}
			/*IOverlay *pIOverlay = NULL;
			if ( SUCCEEDED( pBaseFilter->QueryInterface( IID_IOverlay , (void**)&pIOverlay ) ) )
			{
				pBaseFilter->f
				pGraphBuilder->Connect( m_pVideoPin , pIOverlay );
			}*/
			
			

			pGraphBuilder->Release();
		}
	}
	
	
	return S_OK;
}

STDMETHODIMP CNetSourceFilter::seek(ULONG64 utime)
{
	return S_OK;
}


STDMETHODIMP CNetSourceFilter::NonDelegatingQueryInterface(REFIID riid, __deref_out void ** ppv)
{
	CheckPointer(ppv,E_POINTER);
	if (  riid == IID_INetSource )
	{
		return GetInterface( (INetSource*)this , ppv );
	}
	else
	{
		return CSource::NonDelegatingQueryInterface( riid , ppv );
	}
	
}