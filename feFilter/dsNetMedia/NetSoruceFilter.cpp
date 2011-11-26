#include "StdAfx.h"
#include "NetSoruceFilter.h"
#include "common.h"
#include "filterUtil.h"
#include <wmsdkidl.h>

//滤波器名称
WCHAR filterNam[][20]={
	L"dsNetMedia",
	L"ViderRender"
};

#define  DEFAULT_WIDTH 720
#define DEFAULT_HEIGHT 480

CVideoStreamPin::CVideoStreamPin(HRESULT *phr, CSource *pFilter)
	:CSourceStream(NAME("Push net Source"), phr, pFilter, L"Video_Out")
{

	
}

CVideoStreamPin::~CVideoStreamPin()
{

}

HRESULT CVideoStreamPin::SetMediaType(const CMediaType * pmt)
{
	__super::SetMediaType(pmt);

	return S_OK;
}

//填充样本
HRESULT CVideoStreamPin::FillBuffer(IMediaSample *pSamp)
{
	//CWrapFFMpeg* pWrapFFmpeg = m_wrapmms
	CheckPointer(pSamp, E_POINTER);
	CAutoLock cAutoLock( &m_cSharedState );
	BYTE *pData;
	long cbData;
	// Access the sample's data buffer
	pSamp->GetPointer(&pData);
	cbData = pSamp->GetSize();
	// Check that we're still using video
	ASSERT(m_mt.formattype == FORMAT_VideoInfo);

	VIDEOINFOHEADER *pvih = (VIDEOINFOHEADER*)m_mt.pbFormat;

	CNetSourceFilter* pNetSourceFilter = dynamic_cast<CNetSourceFilter*>(m_pFilter);
	if ( pNetSourceFilter )
	{
	}

	return S_OK;
}

//申明大小
HRESULT CVideoStreamPin::DecideBufferSize( IMemAllocator * pAlloc, __inout ALLOCATOR_PROPERTIES * ppropInputRequest)
{	
	CheckPointer(pAlloc, E_POINTER);
	CheckPointer(ppropInputRequest, E_POINTER);
	HRESULT hr;
	CAutoLock cAutoLock(m_pFilter->pStateLock());

	VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER*) m_mt.Format();

	// Ensure a minimum number of buffers
	ppropInputRequest->cBuffers = 1;
	ppropInputRequest->cbBuffer = pvi->bmiHeader.biSizeImage;
	ASSERT( ppropInputRequest->cbBuffer );

	// Ask the allocator to reserve us some sample memory. NOTE: the function
	// can succeed (return NOERROR) but still not have allocated the
	// memory that we requested, so we must check we got whatever we wanted.
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

	// Make sure that we have only 1 buffer (we erase the ball in the
	// old buffer to save having to zero a 200k+ buffer every time
	// we draw a frame)
	ASSERT(Actual.cBuffers == 1);

	return NOERROR;
}

HRESULT CVideoStreamPin::CheckMediaType(const CMediaType *pMediaType)
{
	CheckPointer(pMediaType,E_POINTER);

	if((*(pMediaType->Type()) != MEDIATYPE_Video) ||   // we only output video
		!(pMediaType->IsFixedSize()))                  // in fixed size samples
	{                                                  
		return E_INVALIDARG;
	}

	// Check for the subtypes we support
	const GUID *SubType = pMediaType->Subtype();
	if (SubType == NULL)
		return E_INVALIDARG;

	if( IsEqualGUID(*SubType , WMMEDIASUBTYPE_I420 )
		&& IsEqualGUID(*SubType , WMMEDIASUBTYPE_RGB32 )
		&& IsEqualGUID(*SubType , WMMEDIASUBTYPE_RGB24 ) 
		&& IsEqualGUID(*SubType , WMMEDIASUBTYPE_RGB565 )
		)
	{
		return E_INVALIDARG;
	}

	// Get the format area of the media type
	VIDEOINFO *pvi = (VIDEOINFO *) pMediaType->Format();

	if(pvi == NULL)
		return E_INVALIDARG;


	return S_OK;
}

//比较是否是允许的mediatype
HRESULT CVideoStreamPin::GetMediaType(int iPosition, __inout CMediaType *pMediaType)
{
	CAutoLock cAutoLock(m_pFilter->pStateLock());
	CheckPointer(pMediaType, E_POINTER);

	if(iPosition < 0)
		return E_INVALIDARG;

	// Have we run off the end of types?
	if(iPosition > 3)
		return VFW_S_NO_MORE_ITEMS;

	VIDEOINFO *pvi = (VIDEOINFO *) pMediaType->AllocFormatBuffer(sizeof(VIDEOINFO));
	if(NULL == pvi)
		return(E_OUTOFMEMORY);

	// Initialize the VideoInfo structure before configuring its members
	ZeroMemory(pvi, sizeof(VIDEOINFO));

	switch ( iPosition )
	{
	case 0:
		{
			pvi->bmiHeader.biCompression = MAKEFOURCC( 'I', '4', '2', '0');
			pvi->bmiHeader.biBitCount    = 16;
		}
		break;
	case 1:
		{
			pvi->bmiHeader.biCompression = BI_RGB;
			pvi->bmiHeader.biBitCount    = 32;
		}		
		break;
	case 2:
		{
			pvi->bmiHeader.biCompression = BI_RGB;
			pvi->bmiHeader.biBitCount    = 24;
		}		
		break;
	case 3:
		{
			pvi->bmiHeader.biCompression = BI_BITFIELDS;
			pvi->bmiHeader.biBitCount    = 16;
			pvi->TrueColorInfo.dwBitMasks[0] = 0xf800;
			pvi->TrueColorInfo.dwBitMasks[1] = 0x7e0;
			pvi->TrueColorInfo.dwBitMasks[2] = 0x1f;
		}
		break;
	
	}
	pvi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER); //bitmapinfoheader结构体的长度
	pvi->bmiHeader.biWidth = DEFAULT_WIDTH;
	pvi->bmiHeader.biHeight = DEFAULT_HEIGHT;
	pvi->bmiHeader.biPlanes = 1; //必填1
	pvi->bmiHeader.biSizeImage = GetBitmapSize( &(pvi->bmiHeader) ); //实际的图像数据占用的字节数
	pvi->bmiHeader.biClrImportant = 0; //指定本图象中重要的颜色数，如果该值为零，则认为所有的颜色都是重要的。
	pvi->bmiHeader.biClrUsed = 0; //调色板中实际使用的颜色数,这个值通常为0，表示使用biBitCount确定的全部颜色

	// Work out the GUID for the subtype from the header info.
	const GUID SubTypeGUID = GetBitmapSubtype(&pvi->bmiHeader);

	SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
	SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

	pMediaType->SetType(&MEDIATYPE_Video);
	pMediaType->SetFormatType(&FORMAT_VideoInfo);
	pMediaType->SetTemporalCompression(FALSE);
	pMediaType->SetSubtype( &SubTypeGUID );
	pMediaType->SetSampleSize( pvi->bmiHeader.biSizeImage );

	return NOERROR;

	/*
	VIDEOINFOHEADER   vih; 
	memset(   &vih,   0,   sizeof(   vih   )   ); 
	vih.bmiHeader.biCompression   =   BI_RGB; 
	vih.bmiHeader.biBitCount         =   24; //每个像素的位数
	vih.bmiHeader.biSize                   =   sizeof(BITMAPINFOHEADER);  //bitmapinfoheader结构体的长度
	vih.bmiHeader.biWidth                 =   DEFAULT_WIDTH;//Your   size.x 
	vih.bmiHeader.biHeight               =   DEFAULT_HEIGHT;//Your   size.y 
	vih.bmiHeader.biPlanes               =   1;		//填为1
	vih.bmiHeader.biSizeImage         =   GetBitmapSize(&vih.bmiHeader); //实际的图像数据占用的字节数
	vih.bmiHeader.biClrUsed = 0; //调色板中实际使用的颜色数,这个值通常为0，表示使用biBitCount确定的全部颜色
	vih.bmiHeader.biClrImportant   =   0; //指定本图象中重要的颜色数，如果该值为零，则认为所有的颜色都是重要的。

	pMediaType-> SetType(&MEDIATYPE_Video); //Major Types
	pMediaType-> SetSubtype(&MEDIASUBTYPE_RGB24); //sub type
	pMediaType-> SetFormatType(&FORMAT_VideoInfo);  //formattype
	pMediaType-> SetFormat( (BYTE*)&vih, sizeof( vih ) ); 
	pMediaType-> SetSampleSize(vih.bmiHeader.biSizeImage);	
	return S_OK;
	*/
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
	
	IGraphBuilder *pGraphBuilder = NULL;
	IFilterGraph *pFilterGraph = GetFilterGraph();
	HRESULT hr = S_OK;
	if ( pFilterGraph )
	{
		if ( SUCCEEDED( pFilterGraph->QueryInterface( IID_IGraphBuilder , (void **)&pGraphBuilder ) ) )
		{
			IBaseFilter* pVideoReaderFilter = NULL;
			if ( SUCCEEDED( pGraphBuilder->FindFilterByName( filterNam[1] , &pVideoReaderFilter ) ) )
			{
				IPin* pIPin = NULL;
				if ( SUCCEEDED(GetUnconnectedPin( pVideoReaderFilter , PINDIR_INPUT , &pIPin )) )
				{
					hr = m_pVideoPin->Connect(pIPin , NULL); //pGraphBuilder->Connect( m_pVideoPin , pIPin );					
					if ( SUCCEEDED(hr) )
					{
						//联接成功
						IMediaFilter *pIMediaFilter = NULL;
						if ( SUCCEEDED( pFilterGraph->QueryInterface( IID_IMediaFilter , (void **)&pIMediaFilter ) ) )
						{
							pIMediaFilter->Run(0); //开始播放视频
							pIMediaFilter->Release();
						}
						
						/*this->Run(0);
						this->Stop();
						m_pVideoPin->Disconnect();*/
					}						
					else if ( VFW_E_CANNOT_CONNECT == hr )
					{
						//无法找到中间的过渡的pin
					}
				}
				pVideoReaderFilter->Release();
			}
			
			pGraphBuilder->Release();
		}
	}

	DWORD size = WideCharToMultiByte( CP_OEMCP,NULL,url,-1,NULL,0,NULL,FALSE );
	char *lpurl = new char[size];
	WideCharToMultiByte (CP_OEMCP,NULL,url,-1,lpurl,size,NULL,FALSE);
	//播放远程视频
	m_wrapmms.play( lpurl );
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