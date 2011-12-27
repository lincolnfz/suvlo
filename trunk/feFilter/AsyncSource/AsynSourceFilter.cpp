#include <windows.h>
#include <streams.h>
#include <process.h>
#include "../common/DefInterface.h"
#include "AsynSourceFilter.h"
#include "../common/filterUtil.h"

WCHAR filterNam[][20]={
	L"dsNetMedia",
	L"ViderRender",
	L"AudioRender",
	L"AsyncIO",
	L"Parser"
};

CAsynSourceOutPin::CAsynSourceOutPin(HRESULT * phr, CAsynReader *pReader ,CAsyncIo *pIo, CCritSec * pLock)
	:CBasePin(NAME("Async output pin"), pReader , pLock , phr , L"Output" , PINDIR_OUTPUT),
	m_pReader(pReader),m_pIo(pIo)
{

}

CAsynSourceOutPin::~CAsynSourceOutPin(){

}

//初始化内存分配器
HRESULT CAsynSourceOutPin::InitAllocator(__deref_out IMemAllocator **ppAlloc)
{
	CheckPointer(ppAlloc,E_POINTER);

	HRESULT hr = NOERROR;
	CMemAllocator *pMemObject = NULL;

	*ppAlloc = NULL;

	/* Create a default memory allocator */
	pMemObject = new CMemAllocator(NAME("Base memory allocator"), NULL, &hr);
	if(pMemObject == NULL)
	{
		return E_OUTOFMEMORY;
	}
	if(FAILED(hr))
	{
		delete pMemObject;
		return hr;
	}

	/* Get a reference counted IID_IMemAllocator interface */
	hr = pMemObject->QueryInterface(IID_IMemAllocator,(void **)ppAlloc);
	if(FAILED(hr))
	{
		delete pMemObject;
		return E_NOINTERFACE;
	}

	ASSERT(*ppAlloc != NULL);
	return NOERROR;
}

STDMETHODIMP CAsynSourceOutPin::NonDelegatingQueryInterface(REFIID riid, void** ppv)
{
	CheckPointer(ppv,E_POINTER);

	if(riid == IID_IAsyncReader)
	{
		m_bQueriedForAsyncReader = TRUE;
		return GetInterface((IAsyncReader*) this, ppv);
	}
	else
	{
		return CBasePin::NonDelegatingQueryInterface(riid, ppv);
	}
}

STDMETHODIMP
	CAsynSourceOutPin::Connect(
	IPin * pReceivePin,
	const AM_MEDIA_TYPE *pmt   // optional media type
	)
{
	return m_pReader->Connect(pReceivePin, pmt); //这里实际上是调用 CBasePin::Content
}

HRESULT CAsynSourceOutPin::GetMediaType(int iPosition, __inout CMediaType *pMediaType)
{
	if(iPosition < 0)
	{
		return E_INVALIDARG;
	}
	if(iPosition > 0)
	{
		return VFW_S_NO_MORE_ITEMS;
	}

	CheckPointer(pMediaType,E_POINTER); 
	CheckPointer(m_pReader,E_UNEXPECTED); 

	*pMediaType = *m_pReader->LoadType();

	return S_OK;
}

HRESULT CAsynSourceOutPin::CheckMediaType(const CMediaType* pType)
{
	CAutoLock lck(m_pLock);

	/*  We treat MEDIASUBTYPE_NULL subtype as a wild card */
	if((m_pReader->LoadType()->majortype == pType->majortype) &&
		(m_pReader->LoadType()->subtype == MEDIASUBTYPE_NULL   ||
		m_pReader->LoadType()->subtype == pType->subtype))
	{
		return S_OK;
	}

	return S_FALSE;
}

//输入pin向输出pin寻求一个分配器
STDMETHODIMP CAsynSourceOutPin::RequestAllocator(
	IMemAllocator* pPreferred,
	ALLOCATOR_PROPERTIES* pProps,
	IMemAllocator ** ppActual)
{
	CheckPointer(pPreferred,E_POINTER);
	CheckPointer(pProps,E_POINTER);
	CheckPointer(ppActual,E_POINTER);
	ASSERT(m_pIo);

	// we care about alignment but nothing else
	if(!pProps->cbAlign || !m_pIo->IsAligned(pProps->cbAlign))
	{
		m_pIo->Alignment(&pProps->cbAlign);
	}

	ALLOCATOR_PROPERTIES Actual;
	HRESULT hr;

	if(pPreferred)
	{
		//要求分配器期望的属性
		hr = pPreferred->SetProperties(pProps, &Actual);

		if(SUCCEEDED(hr) && m_pIo->IsAligned(Actual.cbAlign))
		{
			pPreferred->AddRef();
			*ppActual = pPreferred;
			return S_OK;
		}
	}
	//下端的filter不提供MemAlloc,这时自已创建MemAlloc
	// create our own allocator
	IMemAllocator* pAlloc;
	hr = InitAllocator(&pAlloc);
	if(FAILED(hr))
	{
		return hr;
	}

	//...and see if we can make it suitable
	hr = pAlloc->SetProperties(pProps, &Actual);
	if(SUCCEEDED(hr) && m_pIo->IsAligned(Actual.cbAlign))
	{
		// we need to release our refcount on pAlloc, and addref
		// it to pass a refcount to the caller - this is a net nothing.
		*ppActual = pAlloc;
		return S_OK;
	}

	// failed to find a suitable allocator
	pAlloc->Release();

	// if we failed because of the IsAligned test, the error code will
	// not be failure
	if(SUCCEEDED(hr))
	{
		hr = VFW_E_BADALIGN;
	}
	return hr;
}

STDMETHODIMP CAsynSourceOutPin::Request(
	IMediaSample* pSample,
	DWORD_PTR dwUser)
{
	CheckPointer(pSample,E_POINTER);

	REFERENCE_TIME tStart, tStop;
	HRESULT hr = pSample->GetTime(&tStart, &tStop);
	if(FAILED(hr))
	{
		return hr;
	}

	LONGLONG llPos = tStart / UNITS;
	LONG lLength = (LONG) ((tStop - tStart) / UNITS);

	LONGLONG llTotal=0, llAvailable=0;

	hr = m_pIo->Length(&llTotal, &llAvailable);
	if(llPos + lLength > llTotal)
	{
		// the end needs to be aligned, but may have been aligned
		// on a coarser alignment.
		LONG lAlign;
		m_pIo->Alignment(&lAlign);

		llTotal = (llTotal + lAlign -1) & ~(lAlign-1);

		if(llPos + lLength > llTotal)
		{
			lLength = (LONG) (llTotal - llPos);

			// must be reducing this!
			ASSERT((llTotal * UNITS) <= tStop);
			tStop = llTotal * UNITS;
			pSample->SetTime(&tStart, &tStop);
		}
	}

	BYTE* pBuffer;
	hr = pSample->GetPointer(&pBuffer);
	if(FAILED(hr))
	{
		return hr;
	}

	return m_pIo->Request(llPos,
		lLength,
		TRUE,
		pBuffer,
		(LPVOID)pSample,
		dwUser);
}

STDMETHODIMP CAsynSourceOutPin::WaitForNext(
	DWORD dwTimeout,
	IMediaSample** ppSample,  // completed sample
	DWORD_PTR * pdwUser)
{
	CheckPointer(ppSample,E_POINTER);

	LONG cbActual;
	IMediaSample* pSample=0;

	HRESULT hr = m_pIo->WaitForNext(dwTimeout,
		(LPVOID*) &pSample,
		pdwUser,
		&cbActual);

	if(SUCCEEDED(hr))
	{
		pSample->SetActualDataLength(cbActual);
	}

	*ppSample = pSample;
	return hr;
}

STDMETHODIMP CAsynSourceOutPin::SyncReadAligned(
	IMediaSample* pSample)
{
	CheckPointer(pSample,E_POINTER);

	REFERENCE_TIME tStart, tStop;
	HRESULT hr = pSample->GetTime(&tStart, &tStop);
	if(FAILED(hr))
	{
		return hr;
	}

	LONGLONG llPos = tStart / UNITS;
	LONG lLength = (LONG) ((tStop - tStart) / UNITS);

	LONGLONG llTotal;
	LONGLONG llAvailable;

	hr = m_pIo->Length(&llTotal, &llAvailable);
	if(llPos + lLength > llTotal)
	{
		// the end needs to be aligned, but may have been aligned
		// on a coarser alignment.
		LONG lAlign;
		m_pIo->Alignment(&lAlign);

		llTotal = (llTotal + lAlign -1) & ~(lAlign-1);

		if(llPos + lLength > llTotal)
		{
			lLength = (LONG) (llTotal - llPos);

			// must be reducing this!
			ASSERT((llTotal * UNITS) <= tStop);
			tStop = llTotal * UNITS;
			pSample->SetTime(&tStart, &tStop);
		}
	}

	BYTE* pBuffer;
	hr = pSample->GetPointer(&pBuffer);
	if(FAILED(hr))
	{
		return hr;
	}

	LONG cbActual;
	hr = m_pIo->SyncReadAligned(llPos,
		lLength,
		pBuffer,
		&cbActual,
		pSample);

	pSample->SetActualDataLength(cbActual);
	return hr;
}

STDMETHODIMP CAsynSourceOutPin::SyncRead(
	LONGLONG llPosition,  // absolute file position
	LONG lLength,         // nr bytes required
	BYTE* pBuffer)
{
	return m_pIo->SyncRead(llPosition, lLength, pBuffer);
}

STDMETHODIMP CAsynSourceOutPin::Length(
	LONGLONG* pTotal,
	LONGLONG* pAvailable)
{
	return m_pIo->Length(pTotal, pAvailable);
}

STDMETHODIMP CAsynSourceOutPin::BeginFlush(void)
{
	return m_pIo->BeginFlush();
}
STDMETHODIMP CAsynSourceOutPin::EndFlush(void)
{
	return m_pIo->EndFlush();
}

//////////////////////////////////////////////////////////////////////////
//读取类
CAsynReader::CAsynReader(
	TCHAR *pName,
	LPUNKNOWN pUnk,
	CAsyncStream *pStream,
	HRESULT *phr):CBaseFilter( pName , pUnk , &m_csFilter , CLSID_AsynSource , NULL ), m_Io(pStream) , 
	m_OutputPin( phr , this , &m_Io , &m_csFilter )
{
	//set use media type
	m_mt.SetType(&MEDIATYPE_Stream);
	m_mt.SetSubtype(&MEDIASUBTYPE_NULL);
	m_mt.SetTemporalCompression(TRUE);
}

CAsynReader::~CAsynReader(){

}

int CAsynReader::GetPinCount()
{
	return 1;
}

CBasePin * CAsynReader::GetPin(int n)
{
	if((GetPinCount() > 0) && (n == 0))
	{
		return &m_OutputPin;
	}
	else
	{
		return NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
//asyncfilter

CAsynSourceFilter::CAsynSourceFilter(LPUNKNOWN pUnk, HRESULT *phr) : CAsynReader(NAME("feIO Reader") , pUnk , &m_feBufPool , phr),
	m_wrapmms( m_feBufPool.getPool() , this )
{

}

CAsynSourceFilter::~CAsynSourceFilter()
{

}

CUnknown * WINAPI CAsynSourceFilter::CreateInstance(LPUNKNOWN pUnk, HRESULT *phr)
{
	ASSERT(phr);

	//  DLLEntry does the right thing with the return code and
	//  the returned value on failure

	CAsynSourceFilter *pNew = new CAsynSourceFilter( pUnk, phr);
	if( pNew == NULL )
	{
		*phr = E_OUTOFMEMORY;
	}
	else
	{
		*phr = S_OK;
	}
	return pNew;
}

unsigned int CAsynSourceFilter::CheckThread( void *arg )
{
	CAsynSourceFilter *pFilter = (CAsynSourceFilter*)arg;
	pFilter->notifyDownRecv();
	return 0;
}

int CAsynSourceFilter::notifyDownRecv()
{
	IMediaEventEx *pMediaEvent;
	IFilterGraph *pfilterGraph = GetFilterGraph();
	pfilterGraph->QueryInterface(IID_IMediaEventEx , (void**)&pMediaEvent);
	HANDLE hevent;
	HRESULT hr;
	BOOL bDone = FALSE;
	long ecode , param1 , param2;
	pMediaEvent->GetEventHandle( (OAEVENT*)&hevent );
	while( !bDone ){
		if ( WAIT_OBJECT_0 == WaitForSingleObject( hevent , 100 ) )
		{
			while ( hr = pMediaEvent->GetEvent( &ecode , &param1 , &param2 , 0 ) , SUCCEEDED(hr) )
			{
				hr = pMediaEvent->FreeEventParams( ecode , param1 , param2 );
				if ( SUCCEEDED(hr) && WM_APP+100 == ecode )
				{
					IBaseFilter *parser;
					pfilterGraph->FindFilterByName( filterNam[4] , &parser );
					IPin *pin;
					parser->FindPin( L"Input" , &pin );
					hr = m_OutputPin.Connect( pin , NULL );
					pin->Release();
					
					IBaseFilter *renderfilter;
					pfilterGraph->FindFilterByName( filterNam[1] , &renderfilter );
					IPin *precv;
					parser->FindPin( L"video_out" , &pin );
					GetUnconnectedPin( renderfilter , PINDIR_INPUT , &precv );
					hr = pin->Connect( precv , NULL );

					IMediaFilter *pmedia;
					pfilterGraph->QueryInterface( IID_IMediaFilter , (void**)&pmedia );
					pmedia->Run(0);
					pmedia->Release();
					bDone = TRUE;
					break;
				}
			}
		}
	}
	pMediaEvent->Release();
	return 0;
}

STDMETHODIMP CAsynSourceFilter::Play( LPCWSTR url )
{
	DWORD size = WideCharToMultiByte( CP_OEMCP,NULL,url,-1,NULL,0,NULL,FALSE );
	char *lpurl = new char[size];
	WideCharToMultiByte (CP_OEMCP,NULL,url,-1,lpurl,size,NULL,FALSE);
	//播放远程视频
	m_wrapmms.openfile( lpurl );
	delete []lpurl;

	//IMediaEventEx *pevent;
	//this->GetFilterGraph()->QueryInterface(IID_IMediaEventEx , (void**)&pevent);
	_beginthreadex( NULL , 0 , CheckThread , this , 0 , 0 );

	return S_OK;
}

STDMETHODIMP CAsynSourceFilter::Seek( ULONG64 utime )
{
	return S_OK;
}

STDMETHODIMP CAsynSourceFilter::Stop( void )
{
	return S_OK;
}

