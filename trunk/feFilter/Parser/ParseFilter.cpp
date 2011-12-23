#include "StdAfx.h"
#include "ParseFilter.h"
#include "../common/filterUtil.h"

//////////////////////////////////////////////////////////////////////////
//datapull继承类
CDataPull::CDataPull(UNIT_BUF_POOL *pbufpool) : m_pbufpool(pbufpool)
{
}

CDataPull::~CDataPull()
{
}

HRESULT CDataPull::Receive(IMediaSample* pSample)
{
	//这里操作接收到的数据
	long lActualDateLen = pSample->GetActualDataLength();
	BYTE *pbyte = NULL;
	pSample->GetPointer( &pbyte );
	WriteData( m_pbufpool , (char*)pbyte , lActualDateLen );
	return S_OK;
}

HRESULT CDataPull::EndOfStream(void)
{
	return S_OK;
}

void CDataPull::OnError(HRESULT hr)
{
	
}

HRESULT CDataPull::BeginFlush()
{
	return S_OK;
}

HRESULT CDataPull::EndFlush()
{
	return S_OK;
}

//////////////////////////////////////////////////////////////////////////
//dataInputPin
CDataInputPin::CDataInputPin( CBaseFilter *pFilter , CCritSec *pLock , HRESULT *phr , UNIT_BUF_POOL *pbufpool )
	:CBasePin( NAME("parser input pin") , pFilter , pLock , phr , L"Input" , PINDIR_INPUT ) , m_pullPin( pbufpool )
{
	
}

CDataInputPin::~CDataInputPin()
{

}

HRESULT CDataInputPin::CheckConnect(IPin *pRecvPin)
{
	IMemAllocator *pMemAlloc = NULL;
	if ( FAILED( InitAllocator( &pMemAlloc ) ) )
	{
		pMemAlloc = NULL;
	}
	
	//这里自身提供IMemAlloc,并工作在同步模式
	return m_pullPin.Connect( pRecvPin , pMemAlloc , TRUE ) ;
}

HRESULT CDataInputPin::BreakConnect()
{
	return m_pullPin.Disconnect();
}

HRESULT CDataInputPin::Active(void)
{
	return m_pullPin.Active();
}

HRESULT CDataInputPin::Inactive(void)
{
	return m_pullPin.Inactive();
}

HRESULT CDataInputPin::CheckMediaType(const CMediaType *pmt)
{
	CheckPointer(pmt,E_POINTER);
	if ( *(pmt->Type()) != MEDIATYPE_Stream  )
	{
		return E_INVALIDARG;
	}
	const GUID *subType = pmt->Subtype();
	if ( !IsEqualGUID(*subType , MEDIASUBTYPE_NULL ) )
	{
		return E_INVALIDARG;
	}
	return S_OK;
}

HRESULT STDMETHODCALLTYPE CDataInputPin::BeginFlush( void)
{
	return m_pullPin.BeginFlush();
}

HRESULT STDMETHODCALLTYPE CDataInputPin::EndFlush( void)
{
	return m_pullPin.EndFlush();
}

//////////////////////////////////////////////////////////////////////////
//CFePushPin
CFePushPin::CFePushPin( LPCTSTR lpObjectName , CBaseFilter *pFilter , CCritSec *pLock , HRESULT *phr , LPCWSTR lpName )
	:CBaseOutputPin( lpObjectName , pFilter , pLock , phr , lpName )
{

}

CFePushPin::~CFePushPin()
{

}

HRESULT CFePushPin::Active()
{
	CAutoLock lock( m_pLock );

	HRESULT hr;

	if (m_pFilter->IsActive()) {
		return S_FALSE;	// succeeded, but did not allocate resources (they already exist...)
	}

	// do nothing if not connected - its ok not to connect to
	// all pins of a source filter
	if (!IsConnected()) {
		return NOERROR;
	}

	hr = CBaseOutputPin::Active();
	if (FAILED(hr)) {
		return hr;
	}

	ASSERT(!ThreadExists());

	// start the thread
	if (!Create()) {
		return E_FAIL;
	}

	// Tell thread to initialize. If OnThreadCreate Fails, so does this.
	hr = Init();
	if (FAILED(hr))
		return hr;

	return Pause();
}

HRESULT CFePushPin::Inactive(void)
{
	CAutoLock lock(m_pLock);

	HRESULT hr;

	// do nothing if not connected - its ok not to connect to
	// all pins of a source filter
	if (!IsConnected()) {
		return NOERROR;
	}

	// !!! need to do this before trying to stop the thread, because
	// we may be stuck waiting for our own allocator!!!

	hr = CBaseOutputPin::Inactive();  // call this first to Decommit the allocator
	if (FAILED(hr)) {
		return hr;
	}

	if (ThreadExists()) {
		hr = Stop();

		if (FAILED(hr)) {
			return hr;
		}

		hr = Exit();
		if (FAILED(hr)) {
			return hr;
		}

		Close();	// Wait for the thread to exit, then tidy up.
	}

	// hr = CBaseOutputPin::Inactive();  // call this first to Decommit the allocator
	//if (FAILED(hr)) {
	//	return hr;
	//}

	return NOERROR;
}

DWORD CFePushPin::ThreadProc(void)
{
	HRESULT hr;  // the return code from calls
	Command com;

	do {
		com = GetRequest();
		if (com != CMD_INIT) {
			DbgLog((LOG_ERROR, 1, TEXT("Thread expected init command")));
			Reply((DWORD) E_UNEXPECTED);
		}
	} while (com != CMD_INIT);

	DbgLog((LOG_TRACE, 1, TEXT("CSourceStream worker thread initializing")));

	hr = OnThreadCreate(); // perform set up tasks
	if (FAILED(hr)) {
		DbgLog((LOG_ERROR, 1, TEXT("CSourceStream::OnThreadCreate failed. Aborting thread.")));
		OnThreadDestroy();
		Reply(hr);	// send failed return code from OnThreadCreate
		return 1;
	}

	// Initialisation suceeded
	Reply(NOERROR);

	Command cmd;
	do {
		cmd = GetRequest();

		switch (cmd) {

		case CMD_EXIT:
			Reply(NOERROR);
			break;

		case CMD_RUN:
			DbgLog((LOG_ERROR, 1, TEXT("CMD_RUN received before a CMD_PAUSE???")));
			// !!! fall through???

		case CMD_PAUSE:
			Reply(NOERROR);
			DoBufferProcessingLoop();
			break;

		case CMD_STOP:
			Reply(NOERROR);
			break;

		default:
			DbgLog((LOG_ERROR, 1, TEXT("Unknown command %d received!"), cmd));
			Reply((DWORD) E_NOTIMPL);
			break;
		}
	} while (cmd != CMD_EXIT);

	hr = OnThreadDestroy();	// tidy up.
	if (FAILED(hr)) {
		DbgLog((LOG_ERROR, 1, TEXT("CSourceStream::OnThreadDestroy failed. Exiting thread.")));
		return 1;
	}

	DbgLog((LOG_TRACE, 1, TEXT("CSourceStream worker thread exiting")));
	return 0;
}

HRESULT CFePushPin::DoBufferProcessingLoop(void)
{
	Command com;

	OnThreadStartPlay();

	do {
		while (!CheckRequest(&com)) {

			IMediaSample *pSample;

			//REFERENCE_TIME rtStart , rtEnd;
			HRESULT hr = GetDeliveryBuffer(&pSample,NULL,NULL,0);
			//这里可以得到sample时间
			//pSample->SetTime( &rtStart , &rtEnd );

			if (FAILED(hr)) {
				Sleep(1);
				continue;	// go round again. Perhaps the error will go away
				// or the allocator is decommited & we will be asked to
				// exit soon.
			}

			// Virtual function user will override.
			hr = FillBuffer(pSample);

			if (hr == S_OK) {
				hr = Deliver(pSample); //这时调用下级filter的receive函数
				pSample->Release();

				// downstream filter returns S_FALSE if it wants us to
				// stop or an error if it's reporting an error.
				if(hr != S_OK)
				{
					DbgLog((LOG_TRACE, 2, TEXT("Deliver() returned %08x; stopping"), hr));
					return S_OK;
				}

			} else if (hr == S_FALSE) {
				// derived class wants us to stop pushing data
				pSample->Release();
				DeliverEndOfStream();
				return S_OK;
			} else {
				// derived class encountered an error
				pSample->Release();
				DbgLog((LOG_ERROR, 1, TEXT("Error %08lX from FillBuffer!!!"), hr));
				DeliverEndOfStream();
				m_pFilter->NotifyEvent(EC_ERRORABORT, hr, 0);
				return hr;
			}

			// all paths release the sample
		}

		// For all commands sent to us there must be a Reply call!

		if (com == CMD_RUN || com == CMD_PAUSE) {
			Reply(NOERROR);
		} else if (com != CMD_STOP) {
			Reply((DWORD) E_UNEXPECTED);
			DbgLog((LOG_ERROR, 1, TEXT("Unexpected command!!!")));
		}
	} while (com != CMD_STOP);

	return S_FALSE;
}

//////////////////////////////////////////////////////////////////////////
//video out pin
CVideoOutPin::CVideoOutPin( CBaseFilter *pFilter , CCritSec *pLock , HRESULT *phr )
	:CFePushPin( NAME("video out pull pin") , pFilter , pLock , phr , L"video out" )
{

}

CVideoOutPin::~CVideoOutPin()
{

}

HRESULT CVideoOutPin::DecideBufferSize(IMemAllocator * pAlloc,__inout ALLOCATOR_PROPERTIES * ppropInputRequest)
{
	return S_OK;
}

HRESULT CVideoOutPin::GetMediaType(int iPosition, __inout CMediaType *pMediaType)
{
	return S_OK;
}

HRESULT CVideoOutPin::SetMediaType(const CMediaType *pmt)
{
	__super::SetMediaType( pmt );
	return S_OK;
}

HRESULT CVideoOutPin::CheckMediaType(const CMediaType *)
{
	return S_OK;
}

STDMETHODIMP CVideoOutPin::BeginFlush( void)
{
	return S_OK;
}

STDMETHODIMP CVideoOutPin::EndFlush( void)
{
	return S_OK;
}

STDMETHODIMP CVideoOutPin::EndOfStream(void)
{
	return S_OK;
}

HRESULT CVideoOutPin::FillBuffer(IMediaSample *pSamp)
{
	return S_OK;
}


//////////////////////////////////////////////////////////////////////////
//parserFilter
CParseFilter::CParseFilter(LPUNKNOWN pUnk, HRESULT *phr)
	:CBaseFilter( NAME("parse filter") , pUnk , &m_csFilter , CLSID_Parser , phr ),
	m_DataInputPin( this, &m_csFilter , phr ,&m_bufpool ) , m_VideoOutPin( this, &m_csFilter , phr ),
	m_picpool(5,30)
{
	InitPool( &m_bufpool , 10 , 131072 );
	m_pffmpeg = CFeFFmpeg::GetInstance( &m_bufpool , &m_picpool );
}


CParseFilter::~CParseFilter(void)
{
	DestoryPool( &m_bufpool );
	CFeFFmpeg::Destory();
}

CUnknown * WINAPI CParseFilter::CreateInstance(LPUNKNOWN pUnk, HRESULT *phr)
{
	CParseFilter *pNew = new CParseFilter(pUnk , phr);
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

int CParseFilter::GetPinCount()
{
	return 3;
}

CBasePin * CParseFilter::GetPin(int n)
{
	CBasePin *pin;
	int nPin = GetPinCount();
	if ( nPin > 0 && n >= 0 && n < nPin )
	{
		if ( 0 == n )
		{
			return &m_DataInputPin;
		}else if( 1 == n ){
			return &m_VideoOutPin;
		}else if ( 2 == n )
		{
			return pin;
		}
	}
	return NULL;
}