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

protected:
	CCritSec m_cSharedState;            // Protects our internal state

	int m_iRepeatTime;                  // Time in msec between frames
	const int m_iDefaultRepeatTime;     // Initial m_iRepeatTime
	CRefTime m_rtSampleTime;            // The time stamp for each sample

};

class CAudioStreamPin : public CSourceStream
{
public:
	CAudioStreamPin(HRESULT *phr, CSource *pFilter);
	virtual ~CAudioStreamPin();

protected:
	virtual HRESULT FillBuffer(IMediaSample *pSamp); //pure

	virtual HRESULT DecideBufferSize( IMemAllocator * pAlloc, __inout ALLOCATOR_PROPERTIES * ppropInputRequest); //pure

	virtual HRESULT GetMediaType(__inout CMediaType *pMediaType);

protected:
	CCritSec m_cSharedState;            // Protects our internal state

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
	
	AVFrameLink *getVideoFrameLink(){ return m_pAVFrameLink;}
protected:
	IPin *m_pVideoPin , *m_pAudioPin;
	CWrapmms m_wrapmms;
	HANDLE m_hAlive_pack_event;
	AVFrameLink *m_pAVFrameLink;
};

