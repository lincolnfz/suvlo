#pragma once

#include <streams.h>
#include <dvdmedia.h>
#include "common.h"
#include "Wrapmms.h"
#include "WrapFFMpeg.h"


#define g_wszNetSourceFilter L"Net Media Protocol Source Filter"

class CVideoStreamPin : public CSourceStream
{
public:
	CVideoStreamPin(HRESULT *phr, CSource *pFilter);
	virtual ~CVideoStreamPin();

public:
	virtual HRESULT SetMediaType(const CMediaType *);

	// Resets the stream time to zero
	HRESULT OnThreadCreate(void);

	// Quality control notifications sent to us
	STDMETHODIMP Notify(IBaseFilter * pSender, Quality q);	

protected:
	virtual HRESULT FillBuffer(IMediaSample *pSamp); //pure

	virtual HRESULT DecideBufferSize( IMemAllocator * pAlloc, __inout ALLOCATOR_PROPERTIES * ppropInputRequest); //pure

	virtual HRESULT CheckMediaType(const CMediaType *pMediaType);

	virtual HRESULT GetMediaType(int iPosition, __inout CMediaType *pMediaType);

	//更新filter格式属性
	void updateFmtInfo( const CMediaType * );
	
	//返回实际填充的大小
	long generateRGBDate( BYTE *pDst , SwsContext **ctx , AVFrame *pFrame , 
		int widthSrc , int heightSrc , PixelFormat pixFmtSrc , 
		int widthDst , int heightDst , PixelFormat pixFmtDst );

protected:
	CCritSec m_cSharedState;            // Protects our internal state

	int m_iRepeatTime;                  // Time in msec between frames
	const int m_iDefaultRepeatTime;     // Initial m_iRepeatTime
	CRefTime m_rtSampleTime;            // The time stamp for each sample
	PixelFormat m_piexlformat; //像素类型,当使用rgb使用时,供ffmpeg使用
	int m_iPixelSize;
	BOOL m_bYUV;

};

class CAudioStreamPin : public CSourceStream
{
public:
	CAudioStreamPin(HRESULT *phr, CSource *pFilter);
	virtual ~CAudioStreamPin();
	// Resets the stream time to zero
	HRESULT OnThreadCreate(void);

	// Quality control notifications sent to us
	STDMETHODIMP Notify(IBaseFilter * pSender, Quality q);	

protected:
	virtual HRESULT FillBuffer(IMediaSample *pSamp); //pure

	virtual HRESULT DecideBufferSize( IMemAllocator * pAlloc, __inout ALLOCATOR_PROPERTIES * ppropInputRequest); //pure

	virtual HRESULT GetMediaType(__inout CMediaType *pMediaType);

protected:
	CCritSec m_cSharedState;            // Protects our internal state

	int m_iRepeatTime;                  // Time in msec between frames
	CRefTime m_rtSampleTime;            // The time stamp for each sample

};

class CNetSourceFilter : public CSource , public INetSource
{
public:
	DECLARE_IUNKNOWN
	CNetSourceFilter(IUnknown *pUnk, HRESULT *phr);
	virtual ~CNetSourceFilter(void);

	static CUnknown* __stdcall CreateInstance(LPUNKNOWN pUnk, HRESULT *phr);

	STDMETHODIMP play(LPCWSTR url);
	STDMETHODIMP seek(ULONG64 utime);

	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, __deref_out void ** ppv);
	
	DataLink<VideoData> *getVideoFrameLink(){ return m_pVideoFrameLink;}
	DataLink<AudioData> *getAudioDataLink(){return m_pAudioDataLink;}
	WAVEFORMATEX *getWaveProp(){ return &m_waveProp; }
	void NoitfyStart();
protected:
	IPin *m_pVideoPin , *m_pAudioPin;
	CWrapmms m_wrapmms;
	HANDLE m_hAlive_pack_event;
	DataLink<VideoData> *m_pVideoFrameLink;
	//AVFrameLink *m_pAudioFrameLink;
	DataLink<AudioData> *m_pAudioDataLink;
	WAVEFORMATEX m_waveProp;
};

