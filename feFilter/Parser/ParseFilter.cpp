#include "StdAfx.h"
#include "ParseFilter.h"
#include "../common/filterUtil.h"
#include <wmsdkidl.h>
#include <process.h>

WCHAR filterNam[][20]={
	L"dsNetMedia",
	L"ViderRender",
	L"AudioRender",
	L"AsyncIO",
	L"Parser"
};

#define  DEFAULT_WIDTH 208
#define DEFAULT_HEIGHT 160

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
	static int times = 0;
	++times;
	DbgLog((LOG_TRACE, 0, TEXT("recv data -----: %d\r"), times ));
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
CDataInputPin::CDataInputPin( CBaseFilter *pFilter , CCritSec *pLock , HRESULT *phr , UNIT_BUF_POOL *pbufpool , CFeFFmpeg* pffmpeg )
	:CBasePin( NAME("parser input pin") , pFilter , pLock , phr , L"Input" , PINDIR_INPUT ) , m_pullPin( pbufpool ) , m_pFeFFmpeg(pffmpeg)
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
	m_pFeFFmpeg->Start();
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
CVideoOutPin::CVideoOutPin( CBaseFilter *pFilter , CCritSec *pLock , HRESULT *phr , CObjPool<AVPicture> *pool , VIDEOINFO *videoinfo , GUID *dstFmt)
	:CFePushPin( NAME("video out pull pin") , pFilter , pLock , phr , L"video_out" ) , m_pAVPicturePool(pool) , m_pVideoinfo(videoinfo) ,
	m_pvideoDstFmt(dstFmt)
{

}

CVideoOutPin::~CVideoOutPin()
{

}

//申明sample大小
HRESULT CVideoOutPin::DecideBufferSize(IMemAllocator * pAlloc,__inout ALLOCATOR_PROPERTIES * ppropInputRequest)
{	
	CheckPointer(pAlloc, E_POINTER);
	CheckPointer(ppropInputRequest, E_POINTER);
	HRESULT hr;
	CAutoLock cAutoLock(m_pLock);

	VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER*) m_mt.Format();

	// Ensure a minimum number of buffers
	ppropInputRequest->cBuffers = 1;
	ppropInputRequest->cbBuffer = pvi->bmiHeader.biSizeImage;
	ppropInputRequest->cbAlign = 1; //框架好像会自动设置为1 ??
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
	return S_OK;
}

HRESULT CVideoOutPin::GetMediaType(int iPosition, __inout CMediaType *pMediaType)
{
	CAutoLock cAutoLock(m_pLock);
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
			pvi->bmiHeader.biCompression = BI_RGB;
			pvi->bmiHeader.biBitCount    = 32;			
		}
		break;
	case 1:
		{
			pvi->bmiHeader.biCompression = BI_RGB;
			pvi->bmiHeader.biBitCount    = 24;
		}		
		break;
	case 2:
		{
			// 16 bit per pixel RGB565
			pvi->bmiHeader.biCompression = BI_BITFIELDS;
			pvi->bmiHeader.biBitCount    = 16;
			for(int i = 0; i < 3; i++)
				pvi->TrueColorInfo.dwBitMasks[i] = bits565[i];
			/*pvi->TrueColorInfo.dwBitMasks[0] = 0xf800;
			pvi->TrueColorInfo.dwBitMasks[1] = 0x7e0;
			pvi->TrueColorInfo.dwBitMasks[2] = 0x1f;*/
		}
		break;
	case 3:
		{
			// 16 bit per pixel RGB555
			pvi->bmiHeader.biCompression = BI_BITFIELDS;
			pvi->bmiHeader.biBitCount    = 16;
			for(int i = 0; i < 3; i++)
				pvi->TrueColorInfo.dwBitMasks[i] = bits555[i];
		}
		break;
	case 4:
		{
			//I420
			pvi->bmiHeader.biCompression = MAKEFOURCC( 'I', '4', '2', '0');
			pvi->bmiHeader.biBitCount    = 16;
		}
		break;
	default:
		break;
	
	}
	pvi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER); //bitmapinfoheader结构体的长度
	pvi->bmiHeader.biWidth = m_pVideoinfo->bmiHeader.biWidth;
	pvi->bmiHeader.biHeight = m_pVideoinfo->bmiHeader.biHeight ;
	pvi->bmiHeader.biPlanes = 1; //必填1
	pvi->bmiHeader.biSizeImage = GetBitmapSize( &(pvi->bmiHeader) ); //实际的图像数据占用的字节数
	pvi->bmiHeader.biClrImportant = 0; //指定本图象中重要的颜色数，如果该值为零，则认为所有的颜色都是重要的。
	pvi->bmiHeader.biClrUsed = 0; //调色板中实际使用的颜色数,这个值通常为0，表示使用biBitCount确定的全部颜色

	pvi->AvgTimePerFrame =  m_pVideoinfo->AvgTimePerFrame; //(REFERENCE_TIME)500000;
	m_iDefaultRepeatTime = pvi->AvgTimePerFrame;

	// Work out the GUID for the subtype from the header info.
	const GUID SubTypeGUID = GetBitmapSubtype(&pvi->bmiHeader);
	*m_pvideoDstFmt = SubTypeGUID;

	SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
	SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

	pMediaType->SetType(&MEDIATYPE_Video);
	pMediaType->SetFormatType(&FORMAT_VideoInfo);
	pMediaType->SetTemporalCompression(FALSE);
	pMediaType->SetSubtype( &SubTypeGUID );
	pMediaType->SetSampleSize( pvi->bmiHeader.biSizeImage );
	return S_OK;
}

HRESULT CVideoOutPin::SetMediaType(const CMediaType *pmt)
{
	__super::SetMediaType( pmt );
	return S_OK;
}

HRESULT CVideoOutPin::OnThreadCreate(void)
{
	m_rtSampleTime = 0;	
	m_iRepeatTime = m_iDefaultRepeatTime;
	return S_OK;
}

HRESULT CVideoOutPin::CheckMediaType(const CMediaType *pMediaType)
{
	CheckPointer(pMediaType,E_POINTER);

	if((*(pMediaType->Type()) != MEDIATYPE_Video) ||   // we only output video
		!(pMediaType->IsFixedSize()))                  // in fixed size samples
	{                                                  
		return E_INVALIDARG;
	}

	// Check for the subtypes we support
	const GUID *SubType = pMediaType->Subtype();
	*m_pvideoDstFmt = *SubType;
	if (SubType == NULL)
		return E_INVALIDARG;

	if( !IsEqualGUID(*SubType , WMMEDIASUBTYPE_I420 )
		&& !IsEqualGUID(*SubType , WMMEDIASUBTYPE_RGB32 )
		&& !IsEqualGUID(*SubType , WMMEDIASUBTYPE_RGB24 )
		&& !IsEqualGUID(*SubType , WMMEDIASUBTYPE_RGB565 )
		&& !IsEqualGUID(*SubType , WMMEDIASUBTYPE_RGB555 )
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
	static int times = 0;
	CParseFilter *parserFilter = dynamic_cast<CParseFilter*>(m_pFilter);
	if ( 0 == times )
	{
		HANDLE *hevent = parserFilter->GetHandleArray();
		WaitForMultipleObjects( 2 , hevent , TRUE , INFINITE );
		/*IReferenceClock *pRefClock;
		m_pFilter->GetSyncSource( &pRefClock );
		REFERENCE_TIME rfRun;
		pRefClock->GetTime( &rfRun );
		m_rtSampleTime = rfRun - parserFilter->getRefTime();*/
		m_rtSampleTime = parserFilter->getSampleTime();
		DbgLog((LOG_TRACE, 0, TEXT("video m_rtSampleTime -----: %d\r"), m_rtSampleTime ));
	}
	++times;

	CheckPointer(pSamp, E_POINTER);
	CAutoLock cAutoLock( m_pLock );
	BYTE *pData;
	long cbData;
	// Access the sample's data buffer
	pSamp->GetPointer(&pData);
	cbData = pSamp->GetSize();
	ZeroMemory( pData , cbData );
	// Check that we're still using video
	ASSERT(m_mt.formattype == FORMAT_VideoInfo);
	AVPicture* pict = m_pAVPicturePool->GetOneUnit( CObjPool<AVPicture>::OPCMD::READ_DATA );

	memcpy( pData , pict->data[0] , pict->linesize[0]*m_pVideoinfo->bmiHeader.biHeight  );
	pSamp->SetActualDataLength( pict->linesize[0]*m_pVideoinfo->bmiHeader.biHeight );
	avpicture_free( pict );
	m_pAVPicturePool->CommitOneUnit( pict , CObjPool<AVPicture>::OPCMD::READ_DATA );

	// The current time is the sample's start
	CRefTime rtStart = m_rtSampleTime;

	// Increment to find the finish time
	m_rtSampleTime += (REFERENCE_TIME)m_iRepeatTime;
	pSamp->SetTime((REFERENCE_TIME *) &rtStart,(REFERENCE_TIME *) &m_rtSampleTime);
	//pSamp->SetTime( NULL , NULL );

	pSamp->SetSyncPoint(TRUE);

	DbgLog((LOG_TRACE, 0, TEXT("video out -----: %d\r"), times ));
	return S_OK;
}

STDMETHODIMP CVideoOutPin::Notify(IBaseFilter * pSender, Quality q)
{
	if ( q.Late > 0 )
	{
		m_rtSampleTime += q.Late;
	}
	return S_OK;
}

//////////////////////////////////////////////////////////////////////////
//audio out pin
CAudioOutPin::CAudioOutPin(CBaseFilter *pFilter , CCritSec *pLock , HRESULT *phr , CObjPool<AUDIO_PACK> *audiopool , WAVEFORMATEX *pwafefmt)
	:CFePushPin(NAME("audio pin out") , pFilter , pLock ,phr , L"audio_out" ) , m_pAudioPool(audiopool) , m_pwavFmt(pwafefmt)
{

}

CAudioOutPin::~CAudioOutPin()
{

}

HRESULT CAudioOutPin::FillBuffer(IMediaSample *pSamp)
{
	static int times = 0;
	CParseFilter *parserFilter = dynamic_cast<CParseFilter*>(m_pFilter);
	if ( 0 == times )
	{
		HANDLE *hevent = parserFilter->GetHandleArray();
		WaitForMultipleObjects( 2 , hevent , TRUE , INFINITE );
		/*IReferenceClock *pRefClock;
		m_pFilter->GetSyncSource( &pRefClock );
		REFERENCE_TIME rfRun;
		pRefClock->GetTime( &rfRun );
		m_rtSampleTime = rfRun - parserFilter->getRefTime();*/
		m_rtSampleTime = parserFilter->getSampleTime();
		DbgLog((LOG_TRACE, 0, TEXT("audio m_rtSampleTime -----: %d\r"), m_rtSampleTime ));
	}
	++times;
	BYTE *pData;
	long cbData;
	HRESULT hr = S_FALSE;
	CAutoLock cAutoLock( m_pLock );
	CheckPointer(pSamp, E_POINTER);
	pSamp->GetPointer(&pData);
	cbData = pSamp->GetSize();
	AUDIO_PACK *pkt = m_pAudioPool->GetOneUnit( CObjPool<AUDIO_PACK>::OPCMD::READ_DATA );
	memcpy( pData , pkt->audio_buf2 , pkt->samplesize );	
	m_pAudioPool->CommitOneUnit( pkt , CObjPool<AUDIO_PACK>::OPCMD::READ_DATA );

	// The current time is the sample's start
	CRefTime rtStart = m_rtSampleTime;
	double dbsec = (double)pkt->samplesize / (double)m_pwavFmt->nAvgBytesPerSec ;
	m_rtSampleTime = rtStart + dbsec * UNITS;
	pSamp->SetTime((REFERENCE_TIME *) &rtStart,(REFERENCE_TIME *) &m_rtSampleTime);
	//pSamp->SetTime( NULL , NULL );
	pSamp->SetActualDataLength( pkt->samplesize );
	pSamp->SetSyncPoint(TRUE);
	
	DbgLog((LOG_TRACE, 0, TEXT("audio out -----: %d\r"), times ));
	return S_OK;
}

//impl CBaseOutputPin
HRESULT CAudioOutPin::DecideBufferSize(IMemAllocator * pAlloc , __inout ALLOCATOR_PROPERTIES * ppropInputRequest)
{
	HRESULT hr;
	CAutoLock cAutoLock(m_pLock);
	CheckPointer(pAlloc, E_POINTER);
	CheckPointer(ppropInputRequest, E_POINTER);
	//m_nSpaceTime   =   m_waveFormat.nChannels*m_waveFormat.wBitsPerSample*m_waveFormat.nAvgBytesPerSec*24   /   8   /   1000;

	// Ensure a minimum number of buffers
	ppropInputRequest->cBuffers = 4;
	ppropInputRequest->cbBuffer = AVCODEC_MAX_AUDIO_FRAME_SIZE * 4;
	ppropInputRequest->cbAlign = 1; //框架好像会自动设置为1 ??
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
	//ASSERT(Actual.cBuffers == 1);

	return S_OK;
}

HRESULT CAudioOutPin::GetMediaType(int iPosition, __inout CMediaType *pMediaType)
{
	CAutoLock cAutoLock(m_pLock);
	CheckPointer(pMediaType, E_POINTER);

	if(iPosition < 0)
		return E_INVALIDARG;

	// Have we run off the end of types?
	if(iPosition > 0)
		return VFW_S_NO_MORE_ITEMS;

	WAVEFORMATEX *pWaveFmt = (WAVEFORMATEX*)pMediaType->AllocFormatBuffer( sizeof(WAVEFORMATEX) );

	if(NULL == pWaveFmt)
		return(E_OUTOFMEMORY);

	ZeroMemory( pWaveFmt , sizeof(WAVEFORMATEX) );
	pWaveFmt->cbSize = 0 /*sizeof(WAVEFORMATEX)*/;
	pWaveFmt->wFormatTag = WAVE_FORMAT_PCM;
	//todo 添加详细的参数
	pWaveFmt->nChannels = m_pwavFmt->nChannels; //pNetSourceFilter->getWaveProp()->nChannels;   //共多少声道
	pWaveFmt->wBitsPerSample = m_pwavFmt->wBitsPerSample; //pNetSourceFilter->getWaveProp()->wBitsPerSample; //每个样本多少位
	pWaveFmt->nSamplesPerSec = m_pwavFmt->nSamplesPerSec; //22050; //pNetSourceFilter->getWaveProp()->nSamplesPerSec; //每秒采样多少样本

	pWaveFmt->nBlockAlign = pWaveFmt->nChannels * pWaveFmt->wBitsPerSample / 8;
	pWaveFmt->nAvgBytesPerSec = pWaveFmt->nBlockAlign * pWaveFmt->nSamplesPerSec;

	pMediaType->SetType(&MEDIATYPE_Audio);
	pMediaType->SetFormatType(&FORMAT_WaveFormatEx);
	pMediaType->SetSubtype( &MEDIASUBTYPE_PCM /*MEDIASUBTYPE_WAVE*/ );
	pMediaType->SetTemporalCompression(FALSE);
	pMediaType->SetSampleSize( AVCODEC_MAX_AUDIO_FRAME_SIZE * 4 );
	return S_OK;
}

HRESULT CAudioOutPin::SetMediaType(const CMediaType *pmt)
{
	__super::SetMediaType( pmt );
	return S_OK;
}

HRESULT CAudioOutPin::OnThreadCreate(void)
{
	m_rtSampleTime = 0;
	return S_OK;
}

STDMETHODIMP CAudioOutPin::EndOfStream(void)
{
	return S_OK;
}

STDMETHODIMP CAudioOutPin::BeginFlush(void)
{
	return S_OK;
}

STDMETHODIMP CAudioOutPin::EndFlush(void)
{
	return S_OK;
}

STDMETHODIMP CAudioOutPin::Notify(IBaseFilter * pSender, Quality q)
{
	return S_OK;
}

//impl CBasePin
HRESULT CAudioOutPin::CheckMediaType(const CMediaType *pmt)
{
	CheckPointer(pmt,E_POINTER);

	if((*(pmt->Type()) != MEDIATYPE_Audio) ||   // we only output video
		!(pmt->IsFixedSize()))                  // in fixed size samples
	{                                                  
		return E_INVALIDARG;
	}

	// Check for the subtypes we support
	const GUID *SubType = pmt->Subtype();
	if (SubType == NULL)
		return E_INVALIDARG;

	if( !IsEqualGUID(*SubType , MEDIASUBTYPE_PCM ) )
	{
		return E_INVALIDARG;
	}

	// Get the format area of the media type
	WAVEFORMATEX *wavefmt = (WAVEFORMATEX *) pmt->Format();

	if(wavefmt == NULL)
		return E_INVALIDARG;

	return S_OK;
}

//////////////////////////////////////////////////////////////////////////
//parserFilter
CParseFilter::CParseFilter(LPUNKNOWN pUnk, HRESULT *phr)
	:CBaseFilter( NAME("parse filter") , pUnk , &m_csFilter , CLSID_Parser , phr ),
	m_pffmpeg(CFeFFmpeg::GetInstance( &m_bufpool , &m_picpool , &m_audiopool , &m_videoinfo , &m_waveFmt , &m_videoDstFmt , this)),
	m_DataInputPin( this, &m_csFilter , phr ,&m_bufpool , m_pffmpeg ) ,
	m_picpool(UNITQUEUE,UNITSIZE),m_audiopool(UNITQUEUE,UNITSIZE),
	m_VideoOutPin( this, &m_csOutPin1 , phr , &m_picpool , &m_videoinfo , &m_videoDstFmt ), //video out pin
	m_AudOutPin( this , &m_csOutPin2 , phr , &m_audiopool , &m_waveFmt )
{
	InitPool( &m_bufpool , 10 , 131072 );
	//m_pffmpeg = CFeFFmpeg::GetInstance( &m_bufpool , &m_picpool , &m_audiopool , &m_videoinfo , &m_waveFmt , &m_videoDstFmt);
	m_hSyncAlmost[0] = CreateEvent( NULL , TRUE , FALSE , NULL );
	m_hSyncAlmost[1] = CreateEvent( NULL , TRUE , FALSE , NULL );

	//开始设置视频,音频的属性
	m_videoinfo.bmiHeader.biWidth = DEFAULT_WIDTH;
	m_videoinfo.bmiHeader.biHeight = DEFAULT_HEIGHT;
	m_videoinfo.AvgTimePerFrame = 500000;

	m_waveFmt.nChannels = 2;
	m_waveFmt.wBitsPerSample = 16;
	m_waveFmt.nSamplesPerSec = 22050;
	m_waveFmt.wFormatTag = WAVE_FORMAT_PCM;
}


CParseFilter::~CParseFilter(void)
{
	DestoryPool( &m_bufpool );
	CloseHandle( m_hSyncAlmost[0] );
	CloseHandle( m_hSyncAlmost[1] );
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

STDMETHODIMP CParseFilter::Run(REFERENCE_TIME tStart)
{
	HRESULT hr = CBaseFilter::Run( tStart );
	_beginthreadex( NULL , 0 , CheckOutThread , this , 0 , 0 );
	_beginthreadex( NULL , 0 , CheckAlmostThread , this , 0 , 0 );
	IReferenceClock *pRefClock;
	this->GetSyncSource( &pRefClock );
	pRefClock->GetTime( &m_rtRun );

	return hr;
}

unsigned int CParseFilter::CheckOutThread( void *arg )
{
	CParseFilter *pFilter = (CParseFilter*)arg;
	pFilter->ProcOutConnect();
	return 0;
}

int CParseFilter::ProcOutConnect()
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
		if ( WAIT_OBJECT_0 == WaitForSingleObject( hevent , INFINITE ) )
		{
			while ( hr = pMediaEvent->GetEvent( &ecode , &param1 , &param2 , 0 ) , SUCCEEDED(hr) )
			{
				hr = pMediaEvent->FreeEventParams( ecode , param1 , param2 );
				if ( SUCCEEDED(hr) && WM_APP+101 == ecode )
				{
					IMediaFilter *pmedia;
					pfilterGraph->QueryInterface( IID_IMediaFilter , (void**)&pmedia );
					pmedia->Stop();
				}
			}
		}
	}
	pMediaEvent->Release();
	return 0;
}

unsigned int CParseFilter::CheckAlmostThread( void *arg )
{
	CParseFilter *pFilter = (CParseFilter*)arg;
	pFilter->NotifyStartSync();
	return 0;
}

int CParseFilter::NotifyStartSync()
{
	m_picpool.WaitAlmost();
	//m_audiopool.WaitAlmost();
	IReferenceClock *pRefClock;
	GetSyncSource( &pRefClock );
	REFERENCE_TIME rfRun;
	pRefClock->GetTime( &rfRun );
	m_rtSampleTime = rfRun - m_rtRun;
	SetEvent( m_hSyncAlmost[0] );
	SetEvent( m_hSyncAlmost[1] );
	return 0;
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
			return &m_AudOutPin;
		}
	}
	return NULL;
}