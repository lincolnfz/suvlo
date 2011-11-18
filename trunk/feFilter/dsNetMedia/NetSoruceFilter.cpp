#include "StdAfx.h"
#include "NetSoruceFilter.h"

#define  DEFAULT_WIDTH 1982
#define DEFAULT_HEIGHT 1080

CVideoStreamPin::CVideoStreamPin(HRESULT *phr, CSource *pFilter)
	:CSourceStream(NAME("Push net Source"), phr, pFilter, L"Out")
{

	
}

CVideoStreamPin::~CVideoStreamPin()
{

}

//填充样本
HRESULT CVideoStreamPin::FillBuffer(IMediaSample *pSamp)
{
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
	vih.bmiHeader.biCompression   =   MAKEFOURCC( 'Y ', 'U ', 'Y ', 'V '); 
	vih.bmiHeader.biBitCount         =   16; 
	vih.bmiHeader.biSize                   =   sizeof(BITMAPINFOHEADER); 
	vih.bmiHeader.biWidth                 =   DEFAULT_WIDTH;//Your   size.x 
	vih.bmiHeader.biHeight               =   DEFAULT_HEIGHT;//Your   size.y 
	vih.bmiHeader.biPlanes               =   1; 
	vih.bmiHeader.biSizeImage         =   GetBitmapSize(&vih.bmiHeader); 
	vih.bmiHeader.biClrImportant   =   0; 

	pMediaType-> SetType(&MEDIATYPE_Video); //Major Types
	pMediaType-> SetSubtype(&MEDIASUBTYPE_YUY2); //sub type
	pMediaType-> SetFormatType(&FORMAT_VideoInfo);  //formattype
	pMediaType-> SetFormat(   (BYTE*)   &vih,   sizeof(   vih   )   ); 
	pMediaType-> SetSampleSize(vih.bmiHeader.biSizeImage);


	return S_OK;
}

//音频接口
CAudioStreamPin::CAudioStreamPin(HRESULT *phr, CSource *pFilter)
	:CSourceStream(NAME("Push net Source"), phr, pFilter, L"Out")
{
	
}

CAudioStreamPin::~CAudioStreamPin()
{

}

HRESULT CAudioStreamPin::FillBuffer(IMediaSample *pSamp)
{
	BYTE *pData;
	long cbData;

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
	m_wrapmmsg.play( lpurl );
	delete []lpurl;
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