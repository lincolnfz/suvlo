#include <windows.h>
#include <streams.h>
#include "defhead.h"
#include "AsynSourceFilter.h"

const long ONCEPACKSIZE = 8 * 1024;

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

//向输出pin寻求一个分配器
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
		hr = pPreferred->SetProperties(pProps, &Actual);

		if(SUCCEEDED(hr) && m_pIo->IsAligned(Actual.cbAlign))
		{
			pPreferred->AddRef();
			*ppActual = pPreferred;
			return S_OK;
		}
	}

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
	HRESULT hr;
	BYTE* pBuffer;
	hr = pSample->GetPointer(&pBuffer);
	if(FAILED(hr))
	{
		return hr;
	}

	return m_pIo->Request(0, ONCEPACKSIZE, TRUE, pBuffer, (LPVOID)pSample, dwUser);
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

	BYTE *pByte = NULL;
	hr = pSample->GetPointer( &pByte );
	if(FAILED(hr))
	{
		return hr;
	}
	long cbActual = 0;
	m_pIo->SyncReadAligned( 0 , ONCEPACKSIZE , pByte , &cbActual , pSample );
	pSample->SetActualDataLength( cbActual );

	return S_OK;
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
	HRESULT *phr):CBaseFilter( pName , pUnk , &m_csFilter , CLSID_AsynSource , NULL ), m_Io(pStream)
{
	m_pOutputPin = new CAsynSourceOutPin( phr , this, &m_Io , &m_csFilter );
	//set use media type
	m_mt.SetType(&MEDIATYPE_Stream);
	m_mt.SetSubtype(&MEDIASUBTYPE_NULL);
	m_mt.SetTemporalCompression(TRUE);
	m_mt.lSampleSize = 1;
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
		return m_pOutputPin;
	}
	else
	{
		return NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
//asyncfilter

CAsynSourceFilter::CAsynSourceFilter(LPUNKNOWN pUnk, HRESULT *phr) : CAsynReader(NAME("feIO Reader") , pUnk , &m_feBufPool , phr),
	m_wrapmms( m_feBufPool.getPool() )
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

STDMETHODIMP CAsynSourceFilter::Play( LPCWSTR url )
{
	DWORD size = WideCharToMultiByte( CP_OEMCP,NULL,url,-1,NULL,0,NULL,FALSE );
	char *lpurl = new char[size];
	WideCharToMultiByte (CP_OEMCP,NULL,url,-1,lpurl,size,NULL,FALSE);
	//播放远程视频
	m_wrapmms.openfile( lpurl );
	delete []lpurl;

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

