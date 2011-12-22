#include "StdAfx.h"
#include "NetSoruceFilter.h"
#include "filterUtil.h"
#include <wmsdkidl.h>

//滤波器名称
WCHAR filterNam[][20]={
	L"dsNetMedia",
	L"ViderRender",
	L"AudioRender"
};

CNetSourceFilter *g_pNetSourceFilter = NULL;
extern VideoState *g_pVideoState;

#define  DEFAULT_WIDTH 208
#define DEFAULT_HEIGHT 160

CVideoStreamPin::CVideoStreamPin(HRESULT *phr, CSource *pFilter)
	:CSourceStream(NAME("Push net Source"), phr, pFilter, L"Video_Out"),m_iDefaultRepeatTime(500000)
{
	m_piexlformat = PIX_FMT_RGB32;
	m_iPixelSize = 4;
	m_bYUV = FALSE;
}

CVideoStreamPin::~CVideoStreamPin()
{
	
}

void CVideoStreamPin::updateFmtInfo( const CMediaType * pmt)
{
	const GUID *SubType = pmt->Subtype();
	VIDEOINFO *pvi = (VIDEOINFO*)pmt->Format();
	m_iPixelSize = pvi->bmiHeader.biBitCount / 8;
	if( IsEqualGUID(*SubType , WMMEDIASUBTYPE_I420 ) )
	{
		//m_piexlformat = 
		m_bYUV = TRUE;
	}
	else if ( IsEqualGUID(*SubType , WMMEDIASUBTYPE_RGB32 ) )
	{
		m_piexlformat = PIX_FMT_RGB32;
		m_bYUV = FALSE;
	}
	else if ( IsEqualGUID(*SubType , WMMEDIASUBTYPE_RGB24 ) )
	{
		m_piexlformat = PIX_FMT_RGB24;
		m_bYUV = FALSE;
	}
	else if ( IsEqualGUID(*SubType , WMMEDIASUBTYPE_RGB565 ) )
	{
		m_piexlformat = PIX_FMT_RGB565;
		m_bYUV = FALSE;
	}
	else if ( IsEqualGUID(*SubType , WMMEDIASUBTYPE_RGB555 ) )
	{
		m_piexlformat = PIX_FMT_RGB555;
		m_bYUV = FALSE;
	}
}

HRESULT CVideoStreamPin::SetMediaType(const CMediaType * pmt)
{
	__super::SetMediaType(pmt);
	
	updateFmtInfo(pmt);
	return S_OK;
}

void SaveAsBMP (AVPicture *pAVPic, int width, int height, int index, int bpp)
{
	char buf[5] = {0};
	BITMAPFILEHEADER bmpheader;
	BITMAPINFOHEADER bmpinfo;
	FILE *fp;

	char filename[20] = "C:\\test";
	_itoa (index, buf, 10);
	strcat (filename, buf);
	strcat (filename, ".bmp");

	if ( (fp=fopen(filename,"wb+")) == NULL )
	{
		printf ("open file failed!\n");
		return;
	}

	bmpheader.bfType = 0x4d42;
	bmpheader.bfReserved1 = 0;
	bmpheader.bfReserved2 = 0;
	bmpheader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	bmpheader.bfSize = bmpheader.bfOffBits + width*height*bpp/8;

	bmpinfo.biSize = sizeof(BITMAPINFOHEADER);
	bmpinfo.biWidth = width;
	bmpinfo.biHeight = height;
	bmpinfo.biPlanes = 1;
	bmpinfo.biBitCount = bpp;
	bmpinfo.biCompression = BI_RGB;
	bmpinfo.biSizeImage = (width*bpp+31)/32*4*height;
	bmpinfo.biXPelsPerMeter = 100;
	bmpinfo.biYPelsPerMeter = 100;
	bmpinfo.biClrUsed = 0;
	bmpinfo.biClrImportant = 0;

	fwrite (&bmpheader, sizeof(bmpheader), 1, fp);
	fwrite (&bmpinfo, sizeof(bmpinfo), 1, fp);
	fwrite (pAVPic->data[0], width*height*bpp/8, 1, fp);

	fclose(fp);
}

long CVideoStreamPin::generateRGBDate( BYTE *pDst , SwsContext **ctx , AVFrame *pFrame , 
	int widthSrc , int heightSrc , PixelFormat pixFmtSrc , 
	int widthDst , int heightDst , PixelFormat pixFmtDst )
{
	long lsize = 0;
	*ctx = sws_getCachedContext( *ctx ,
		widthSrc ,
		heightSrc ,
		pixFmtSrc , 
		widthDst ,
		heightDst ,
		pixFmtDst ,  SWS_BICUBIC , NULL , NULL , NULL );
	if ( *ctx )
	{
		 AVPicture pict;
		int ret = avpicture_alloc( &pict , pixFmtDst , 
				widthDst , 
				heightDst);
		//倒置颠倒的图像
		/*
		pAVFrame->data[0] = pAVFrame->data[0]+pAVFrame->linesize[0]*( g_pVideoState->video_st->codec->height-1 );
		pAVFrame->data[1] = pAVFrame->data[1]+pAVFrame->linesize[0]*g_pVideoState->video_st->codec->height/4-1;  
		pAVFrame->data[2] = pAVFrame->data[2]+pAVFrame->linesize[0]*g_pVideoState->video_st->codec->height/4-1; 
		pAVFrame->linesize[0] *= -1;
		pAVFrame->linesize[1] *= -1;  
		pAVFrame->linesize[2] *= -1;
		*/
		//倒置颠倒的图像 ok
				
		//转换成rgb
		if ( !ret && sws_scale( *ctx , pFrame->data , pFrame->linesize , 0 , heightSrc , pict.data , pict.linesize ) )
		{
			lsize = pict.linesize[0]*heightSrc;
			CopyMemory(pDst , pict.data[0] , lsize );
			pDst += lsize;
			if ( pict.linesize[1] )
			{
				int tmplen = pict.linesize[1]*heightSrc;
				CopyMemory(pDst , pict.data[1] , tmplen );
				pDst += tmplen;
				lsize += tmplen;
			}
			if ( pict.linesize[2] )
			{
				int tmplen = pict.linesize[2]*heightSrc;
				CopyMemory(pDst , pict.data[2] , tmplen );
				pDst += tmplen;
				lsize += tmplen;
			}

			avpicture_free( &pict );
		}
				
	}
	return lsize;
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
	ZeroMemory( pData , cbData );
	// Check that we're still using video
	ASSERT(m_mt.formattype == FORMAT_VideoInfo);

	VIDEOINFOHEADER *pvih = (VIDEOINFOHEADER*)m_mt.pbFormat;

	CNetSourceFilter* pNetSourceFilter = dynamic_cast<CNetSourceFilter*>(m_pFilter);
	if ( pNetSourceFilter )
	{
		DataLink<VideoData> *pAVFrameLink = pNetSourceFilter->getVideoFrameLink();
		if ( pAVFrameLink )
		{
			DataNode<VideoData> *pAVFrameNode = getDataNode( pAVFrameLink , 1 );
			/*
			AVFrame *pAVFrame = pAVFrameNode->pData.avframe;
			 double pts = pAVFrameNode->pData.pts;
			 free( pAVFrameNode );
			 if ( pAVFrame )
			 {
				 if ( m_bYUV  )
				 {
					 //下位fliter要求使用yuv格式
					 long lsize = generateRGBDate( pData , &g_pVideoState->img_convert_ctx , pAVFrame ,
						 g_pVideoState->video_st->codec->width , g_pVideoState->video_st->codec->height , g_pVideoState->video_st->codec->pix_fmt ,
						 g_pVideoState->video_st->codec->width , g_pVideoState->video_st->codec->height , m_piexlformat );
					 if ( lsize > 0 )
					 {
						 pSamp->SetActualDataLength( lsize );
					 }
				 }
				 else
				 {
					 //下位fliter要求使用rgb格式
					 long lsize = generateRGBDate( pData , &g_pVideoState->img_convert_ctx , pAVFrame ,
						 g_pVideoState->video_st->codec->width , g_pVideoState->video_st->codec->height , g_pVideoState->video_st->codec->pix_fmt ,
						 g_pVideoState->video_st->codec->width , g_pVideoState->video_st->codec->height , m_piexlformat );
					 if ( lsize > 0 )
					 {
						 pSamp->SetActualDataLength( lsize );
					 }
				 }

				 				 
				 //清理frame
				 av_free( pAVFrame);


			 }*/

		CopyMemory( pData , pAVFrameNode->pData.vdata , pAVFrameNode->pData.ldatalen  );
		pSamp->SetActualDataLength( pAVFrameNode->pData.ldatalen );

		// The current time is the sample's start
		CRefTime rtStart = m_rtSampleTime;

		// Increment to find the finish time
		m_rtSampleTime += (REFERENCE_TIME)m_iRepeatTime;
		pSamp->SetTime((REFERENCE_TIME *) &rtStart,(REFERENCE_TIME *) &m_rtSampleTime);

		//REFERENCE_TIME rt = pts * UNITS;
		//REFERENCE_TIME rtend = rt;// + 50 * 10000;
		//pSamp->SetTime( &rt , &rtend );

		pSamp->SetSyncPoint(TRUE);
		
		LONGLONG lls,lle;
		pSamp->GetTime( &lls , &lle );
		DbgLog((LOG_TRACE, 0, TEXT("video s:%I64d ,e:%I64d\r"), lls,lle));

			 
		}
	}

	return NOERROR;
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

	updateFmtInfo(pMediaType);
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
	pvi->bmiHeader.biWidth = DEFAULT_WIDTH;
	pvi->bmiHeader.biHeight = DEFAULT_HEIGHT;
	pvi->bmiHeader.biPlanes = 1; //必填1
	pvi->bmiHeader.biSizeImage = GetBitmapSize( &(pvi->bmiHeader) ); //实际的图像数据占用的字节数
	pvi->bmiHeader.biClrImportant = 0; //指定本图象中重要的颜色数，如果该值为零，则认为所有的颜色都是重要的。
	pvi->bmiHeader.biClrUsed = 0; //调色板中实际使用的颜色数,这个值通常为0，表示使用biBitCount确定的全部颜色

	pvi->AvgTimePerFrame = (REFERENCE_TIME)500000;

	// Work out the GUID for the subtype from the header info.
	const GUID SubTypeGUID = GetBitmapSubtype(&pvi->bmiHeader);

	SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
	SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

	pMediaType->SetType(&MEDIATYPE_Video);
	pMediaType->SetFormatType(&FORMAT_VideoInfo);
	pMediaType->SetTemporalCompression(FALSE);
	pMediaType->SetSubtype( &SubTypeGUID );
	pMediaType->SetSampleSize( pvi->bmiHeader.biSizeImage );

	updateFmtInfo(pMediaType);
	return NOERROR;
}

STDMETHODIMP CVideoStreamPin::Notify(IBaseFilter * pSender, Quality q)
{
	//// Adjust the repeat rate.
	//if(q.Proportion<=0)
	//{
	//	m_iRepeatTime = 1000;        // We don't go slower than 1 per second
	//}
	//else
	//{
	//	m_iRepeatTime = m_iRepeatTime*1000 / q.Proportion;
	//	if(m_iRepeatTime>1000)
	//	{
	//		m_iRepeatTime = 1000;    // We don't go slower than 1 per second
	//	}
	//	else if(m_iRepeatTime<10)
	//	{
	//		m_iRepeatTime = 10;      // We don't go faster than 100/sec
	//	}
	//}

	// skip forwards
	//if(q.Late > 0)
	//	m_rtSampleTime += q.Late;
	
	return NOERROR;
}

HRESULT CVideoStreamPin::OnThreadCreate(void)
{
	CAutoLock cAutoLockShared(&m_cSharedState);
	m_rtSampleTime = 0;

	// we need to also reset the repeat time in case the system
	// clock is turned off after m_iRepeatTime gets very big
	m_iRepeatTime = m_iDefaultRepeatTime;

	return S_OK;
}



//////////////////////////////////////////////////////////////////////////
//音频接口

CAudioStreamPin::CAudioStreamPin(HRESULT *phr, CSource *pFilter)
	:CSourceStream(NAME("Push net Source"), phr, pFilter, L"Audio_Out")
{
	m_times = 0;
	m_nSamplesPerSec = 44100;
}

CAudioStreamPin::~CAudioStreamPin()
{

}

HRESULT CAudioStreamPin::SetNewType( CMediaType *pMediaType )
{
	m_nSamplesPerSec = 22050;
	CheckPointer(pMediaType, E_POINTER);
	WAVEFORMATEX *pWaveFmt = (WAVEFORMATEX*)pMediaType->AllocFormatBuffer( sizeof(WAVEFORMATEX) );
	CNetSourceFilter* pNetSourceFilter = dynamic_cast<CNetSourceFilter*>(m_pFilter);

	if(NULL == pWaveFmt)
		return(E_OUTOFMEMORY);

	ZeroMemory( pWaveFmt , sizeof(WAVEFORMATEX) );
	pWaveFmt->cbSize = 0 /*sizeof(WAVEFORMATEX)*/;
	pWaveFmt->wFormatTag = WAVE_FORMAT_PCM;
	//todo 添加详细的参数
	pWaveFmt->nChannels = 2; //pNetSourceFilter->getWaveProp()->nChannels;   //共多少声道
	pWaveFmt->wBitsPerSample = 16; //pNetSourceFilter->getWaveProp()->wBitsPerSample; //每个样本多少位
	pWaveFmt->nSamplesPerSec = m_nSamplesPerSec; //pNetSourceFilter->getWaveProp()->nSamplesPerSec; //每秒采样多少样本

	pWaveFmt->nBlockAlign = pWaveFmt->nChannels * pWaveFmt->wBitsPerSample / 8;
	pWaveFmt->nAvgBytesPerSec = pWaveFmt->nBlockAlign * pWaveFmt->nSamplesPerSec;
	m_nAvgSecTime = pWaveFmt->nAvgBytesPerSec;

	pMediaType->SetType(&MEDIATYPE_Audio);
	pMediaType->SetFormatType(&FORMAT_WaveFormatEx);
	pMediaType->SetSubtype( &MEDIASUBTYPE_PCM /*MEDIASUBTYPE_WAVE*/ );
	pMediaType->SetTemporalCompression(FALSE);
	pMediaType->SetSampleSize( AVCODEC_MAX_AUDIO_FRAME_SIZE * 4 );
	return S_OK;
}

HRESULT CAudioStreamPin::ChargeMediaType()
{
	CNetSourceFilter* pNetSourceFilter = dynamic_cast<CNetSourceFilter*>(m_pFilter);
	HRESULT hr = S_FALSE;
	/*hr = m_pAllocator->Decommit();
	hr = m_pAllocator->Release();
	hr = CreateMemoryAllocator( &m_pAllocator );
	hr =m_pAllocator->Commit();*/
	IPinConnection *pinconn = NULL;
	//hr = m_pInputPin->QueryInterface( IID_IPinConnection , (void**)pinconn);
	CMediaType type;
	IPin *pin = NULL;
	m_pInputPin->QueryInterface( IID_IPin , (void **)&pin );
	SetNewType(&type);
	hr = pin->QueryAccept( &type );
	
	hr = pin->Disconnect();
	hr = pin->ReceiveConnection( (IPin*)this , &type );

	IFilterGraph2 *pFilterG2 = NULL;
	m_pFilter->GetFilterGraph()->QueryInterface( IID_IFilterGraph2 , (void **)pFilterG2 );
	IMediaSample *psamp = NULL;
	//hr = m_pAllocator->Decommit();

	//this->GetDeliveryBuffer( &psamp , NULL , NULL , AM_GBF_NOWAIT );
	//hr = m_pAllocator->GetBuffer(&psamp , NULL , NULL , 0);

	//hr = psamp->SetMediaType( &type );
	//AM_MEDIA_TYPE *ptype = NULL;
	//hr = psamp->GetMediaType( &ptype );
	//hr = m_pInputPin->NotifyAllocator( m_pAllocator , FALSE );
	this->m_mt = type;
	

	return S_OK;
}

HRESULT CAudioStreamPin::FillBuffer(IMediaSample *pSamp)
{
	/*if ( m_times == 0 )
	{
	pSamp->SetDiscontinuity( TRUE );
	m_times++;
	}else{
	pSamp->SetDiscontinuity( FALSE );
	}*/
	BYTE *pData;
	long cbData;
	HRESULT hr = S_FALSE;
	CAutoLock cAutoLock( &m_cSharedState );
	CheckPointer(pSamp, E_POINTER);
	pSamp->GetPointer(&pData);
	cbData = pSamp->GetSize();
	CNetSourceFilter* pNetSourceFilter = dynamic_cast<CNetSourceFilter*>(m_pFilter);
	if ( pNetSourceFilter )
	{
		DataLink<AudioData> *pDataLink = pNetSourceFilter->getAudioDataLink();
		DataNode<AudioData> *pNode = getDataNode( pDataLink , 1 );
		if ( pNode )
		{
			memcpy( pData , pNode->pData.audio_buf , pNode->pData.data_size );						
			pSamp->SetActualDataLength( pNode->pData.data_size );
			
			// The current time is the sample's start
			CRefTime rtStart = m_rtSampleTime;
			double dbsec = (double)pNode->pData.data_size / (double)m_nAvgSecTime;
			m_rtSampleTime = rtStart + dbsec * 10000000;
			free( pNode );

			// Increment to find the finish time
			hr = pSamp->SetTime((REFERENCE_TIME *) &rtStart,(REFERENCE_TIME *) &m_rtSampleTime);
			hr = pSamp->SetSyncPoint(TRUE);
			LONGLONG llstart , llend;
			pSamp->GetMediaTime( &llstart , &llend );
			LONGLONG lls,lle;
			pSamp->GetTime( &lls , &lle );
			DbgLog((LOG_TRACE, 0, TEXT("audiorend s:%I64d ,e:%I64d\r"), lls,lle));
		}
	}
	return S_OK;
}

HRESULT CAudioStreamPin::DecideBufferSize( IMemAllocator * pAlloc, __inout ALLOCATOR_PROPERTIES * ppropInputRequest)
{
	HRESULT hr;
	CAutoLock cAutoLock(m_pFilter->pStateLock());
	CheckPointer(pAlloc, E_POINTER);
	CheckPointer(ppropInputRequest, E_POINTER);
	m_mt.Format();
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

HRESULT CAudioStreamPin::GetMediaType(__inout CMediaType *pMediaType)
{
	CAutoLock cAutoLock(m_pFilter->pStateLock());
	CheckPointer(pMediaType, E_POINTER);
	WAVEFORMATEX *pWaveFmt = (WAVEFORMATEX*)pMediaType->AllocFormatBuffer( sizeof(WAVEFORMATEX) );
	CNetSourceFilter* pNetSourceFilter = dynamic_cast<CNetSourceFilter*>(m_pFilter);

	if(NULL == pWaveFmt)
		return(E_OUTOFMEMORY);

	ZeroMemory( pWaveFmt , sizeof(WAVEFORMATEX) );
	pWaveFmt->cbSize = 0 /*sizeof(WAVEFORMATEX)*/;
	pWaveFmt->wFormatTag = WAVE_FORMAT_PCM;
	//todo 添加详细的参数
	pWaveFmt->nChannels = 2; //pNetSourceFilter->getWaveProp()->nChannels;   //共多少声道
	pWaveFmt->wBitsPerSample = 16; //pNetSourceFilter->getWaveProp()->wBitsPerSample; //每个样本多少位
	pWaveFmt->nSamplesPerSec = m_nSamplesPerSec; //22050; //pNetSourceFilter->getWaveProp()->nSamplesPerSec; //每秒采样多少样本

	pWaveFmt->nBlockAlign = pWaveFmt->nChannels * pWaveFmt->wBitsPerSample / 8;
	pWaveFmt->nAvgBytesPerSec = pWaveFmt->nBlockAlign * pWaveFmt->nSamplesPerSec;
	m_nAvgSecTime = pWaveFmt->nAvgBytesPerSec;

	pMediaType->SetType(&MEDIATYPE_Audio);
	pMediaType->SetFormatType(&FORMAT_WaveFormatEx);
	pMediaType->SetSubtype( &MEDIASUBTYPE_PCM /*MEDIASUBTYPE_WAVE*/ );
	pMediaType->SetTemporalCompression(FALSE);
	pMediaType->SetSampleSize( AVCODEC_MAX_AUDIO_FRAME_SIZE * 4 );

	return S_OK;
}

HRESULT CAudioStreamPin::Inactive(void)
{
	__super::Inactive();
	//Sleep(500);
	//CNetSourceFilter* pNetSourceFilter = dynamic_cast<CNetSourceFilter*>(m_pFilter);
	//pNetSourceFilter->NoitfyStart(  );
	Sleep(400);
	CNetSourceFilter* pNetSourceFilter = dynamic_cast<CNetSourceFilter*>(m_pFilter);
	pNetSourceFilter->m_eventStart.Set();
	return S_OK;
}

CMediaType *g_audioType = NULL;

HRESULT CAudioStreamPin::OnThreadCreate(void)
{
	CAutoLock cAutoLockShared(&m_cSharedState);
	m_rtSampleTime = 0;

	// we need to also reset the repeat time in case the system
	// clock is turned off after m_iRepeatTime gets very big
	m_iRepeatTime = 10000000;
	//ChargeMediaType();
	m_nSamplesPerSec = 22050;
	g_audioType = new CMediaType();
	this->SetNewType( g_audioType );
	CNetSourceFilter* pNetSourceFilter = dynamic_cast<CNetSourceFilter*>(m_pFilter);
	if ( m_times == 0 )
	{
		//pNetSourceFilter->NoitfyStart( TRUE );
		pNetSourceFilter->FlowStop();
		pNetSourceFilter->ReConnect();
		++m_times;
	}
	

	return S_OK;
}

STDMETHODIMP CAudioStreamPin::Notify(IBaseFilter * pSender, Quality q)
{
	return S_OK;
}

CNetSourceFilter::CNetSourceFilter(IUnknown *pUnk, HRESULT *phr)
 :CSource( NAME("net protocol source") , pUnk , CLSID_NetMediaSource )
{
	m_pVideoPin = new CVideoStreamPin( phr , this );
	m_pAudioPin = new CAudioStreamPin( phr , this );
	initDataLink( &m_pVideoFrameLink );
	//initAVFrameLink( &m_pAudioFrameLink );
	initDataLink( &m_pAudioDataLink );
	g_pNetSourceFilter = this;
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
	destoryDataLink( &m_pVideoFrameLink );
	//destoryAVFrameLink( &m_pAudioFrameLink );
	destoryDataLink( &m_pAudioDataLink );
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

void CNetSourceFilter::NoitfyStart( BOOL bRestart , CMediaType *pType )
{
	/************************************************************************/
	/* 开始联接filter                                                        */
	/************************************************************************/
	IGraphBuilder *pGraphBuilder = NULL;
	IFilterGraph *pFilterGraph = GetFilterGraph();
	IMediaFilter *pIMediaFilter = NULL;
	pFilterGraph->QueryInterface( IID_IMediaFilter , (void **)&pIMediaFilter );
	if ( bRestart )
	{
		HRESULT hr = pIMediaFilter->Stop();
		return;
	}

	BOOL bConnect = FALSE;
	HRESULT hr = S_OK;
	if ( pFilterGraph )
	{
		if ( SUCCEEDED( pFilterGraph->QueryInterface( IID_IGraphBuilder , (void **)&pGraphBuilder ) ) )
		{
			
			
			//联接videopin
			IBaseFilter* pVideoReaderFilter = NULL;
			if ( SUCCEEDED( pGraphBuilder->FindFilterByName( filterNam[1] , &pVideoReaderFilter ) ) )
			{
				IPin* pIPin = NULL;
				if ( SUCCEEDED(GetUnconnectedPin( pVideoReaderFilter , PINDIR_INPUT , &pIPin )) )
				{
					hr = m_pVideoPin->Connect(pIPin , NULL); //pGraphBuilder->Connect( m_pVideoPin , pIPin );				
					if ( SUCCEEDED(hr) )
					{
						bConnect = TRUE;						
					}						
					else if ( VFW_E_CANNOT_CONNECT == hr )
					{
						//无法找到中间的过渡的pin
					}
				}
				pVideoReaderFilter->Release();
			}
			//联接videopin完成

			//联接audiopin
			IBaseFilter* pAudioRenderFilter = NULL;
			if ( SUCCEEDED( pGraphBuilder->FindFilterByName( filterNam[2] , &pAudioRenderFilter ) ) )
			{
				IPin* pIPin = NULL;
				if ( SUCCEEDED(GetUnconnectedPin( pAudioRenderFilter , PINDIR_INPUT , &pIPin )) )
				{
					hr = m_pAudioPin->Connect(pIPin , pType); //pGraphBuilder->Connect( m_pVideoPin , pIPin );					
					if ( SUCCEEDED(hr) )
					{
						bConnect = TRUE;						
						
					}						
					else if ( VFW_E_CANNOT_CONNECT == hr )
					{
						//无法找到中间的过渡的pin
					}
				}
				pAudioRenderFilter->Release();
			}
			//联接audiopin完成
			

			bConnect = TRUE;
			if ( bConnect )
			{
				//联接成功
				pIMediaFilter->Run(0); //开始播放视频			
				
			}
			pGraphBuilder->Release();
		}
	}
	pIMediaFilter->Release();
	//构造graph结束
}

DWORD __stdcall CNetSourceFilter::reRun( LPVOID pv )
{
	CNetSourceFilter* p = (CNetSourceFilter*)pv;
	p->m_eventStart.Wait();
	//p->NoitfyStart( FALSE , g_audioType );
	CAudioStreamPin* pAudioPin = dynamic_cast<CAudioStreamPin*>(p->m_pAudioPin);
	pAudioPin->ChargeMediaType();
	p->NoitfyStart( FALSE , g_audioType );
	return 0;
}

void CNetSourceFilter::ReConnect()
{
	DWORD threadid = 0;
	HANDLE hThread = CreateThread(
		NULL,
		0,
		CNetSourceFilter::reRun,
		this,
		0,
		&threadid);
	CloseHandle( hThread);
}

DWORD __stdcall CNetSourceFilter::StopThread( LPVOID pv )
{
	CNetSourceFilter* p = (CNetSourceFilter*)pv;
	p->NoitfyStart( TRUE );
	return 0;
}

void CNetSourceFilter::FlowStop()
{
	DWORD threadid = 0;
	HANDLE hThread = CreateThread(
		NULL,
		0,
		CNetSourceFilter::StopThread,
		this,
		0,
		&threadid);
	CloseHandle( hThread);
}