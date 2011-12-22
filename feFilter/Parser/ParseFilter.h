#pragma once
#include "../common/DefInterface.h"
#include "../common/bufpool.h"
#include "../common/ObjPool.h"
#include <pullpin.h>
#include "WrapFFMpeg.h"

class CDataPull : public CPullPin
{
public:
	CDataPull( UNIT_BUF_POOL *pbufpool );
	virtual ~CDataPull();

	//ʵ��cpullpin�麯��
	// override this to handle data arrival
	// return value other than S_OK will stop data
	virtual HRESULT Receive(IMediaSample*);

	// override this to handle end-of-stream
	virtual HRESULT EndOfStream(void);

	// called on runtime errors that will have caused pulling
	// to stop
	// these errors are all returned from the upstream filter, who
	// will have already reported any errors to the filtergraph.
	virtual void OnError(HRESULT hr);

	// flush this pin and all downstream
	virtual HRESULT BeginFlush();
	virtual HRESULT EndFlush();

protected:
	UNIT_BUF_POOL *m_pbufpool;
};

class CDataInputPin : public CBasePin
{
public:
	CDataInputPin( CBaseFilter *pFilter , CCritSec *pLock , HRESULT *phr , UNIT_BUF_POOL *pbufpool );
	virtual ~CDataInputPin();

	//��ģʽ�е���pullpin��content
	virtual HRESULT CheckConnect(IPin *);

	//��ģʽ�е���pullpin��disconnect
	virtual HRESULT BreakConnect();

	//��ģʽ��ʹ��pullpin��active��������,
	virtual HRESULT Active(void);

	//��ģʽ��ʹ��pullpin��inactiveֹͣ����
	virtual HRESULT Inactive(void);

	//--------CBasePin----------------
	// check if the pin can support this specific proposed type and format
	virtual HRESULT CheckMediaType(const CMediaType *);

	virtual HRESULT STDMETHODCALLTYPE BeginFlush( void);

	virtual HRESULT STDMETHODCALLTYPE EndFlush( void);

protected:
	CDataPull m_pullPin;

};

class CFePushPin : public CAMThread , public CBaseOutputPin
{
public:
	CFePushPin( LPCTSTR lpObjectName , CBaseFilter *pFilter , CCritSec *pLock , HRESULT *phr , LPCWSTR lpName );
	virtual ~CFePushPin();

	// *
	// * Worker Thread
	// *
	//���´������߳�
	HRESULT Active(void);    // Starts up the worker thread
	HRESULT Inactive(void);  // Exits the worker thread.

protected:

	// Override this to provide the worker thread a means
	// of processing a buffer
	virtual HRESULT FillBuffer(IMediaSample *pSamp) PURE;

	// Called as the thread is created/destroyed - use to perform
	// jobs such as start/stop streaming mode
	// If OnThreadCreate returns an error the thread will exit.
	virtual HRESULT OnThreadCreate(void) {return NOERROR;};
	virtual HRESULT OnThreadDestroy(void) {return NOERROR;};
	virtual HRESULT OnThreadStartPlay(void) {return NOERROR;};

public:
	// thread commands
	enum Command {CMD_INIT, CMD_PAUSE, CMD_RUN, CMD_STOP, CMD_EXIT};
	HRESULT Init(void) { return CAMThread::CallWorker(CMD_INIT); }
	HRESULT Exit(void) { return CAMThread::CallWorker(CMD_EXIT); }
	HRESULT Run(void) { return CAMThread::CallWorker(CMD_RUN); }
	HRESULT Pause(void) { return CAMThread::CallWorker(CMD_PAUSE); }
	HRESULT Stop(void) { return CAMThread::CallWorker(CMD_STOP); }

protected:
	Command GetRequest(void) { return (Command) CAMThread::GetRequest(); }
	BOOL    CheckRequest(Command *pCom) { return CAMThread::CheckRequest( (DWORD *) pCom); }

	// override these if you want to add thread commands
	virtual DWORD ThreadProc(void);  		// the thread function

	virtual HRESULT DoBufferProcessingLoop(void);    // the loop executed whilst running

	//���϶��Ǵ������߳�
	//////////////////////////////////////////////////////////////////////////

	//////////////////////////////////////////////////////////////////////////
	//�����Ǵ���ý���ʽ
public:
	//impl CBaseOutputPin
	virtual HRESULT DecideBufferSize(IMemAllocator * pAlloc,__inout ALLOCATOR_PROPERTIES * ppropInputRequest)PURE;
	virtual HRESULT GetMediaType(int iPosition, __inout CMediaType *pMediaType){ return CBaseOutputPin::GetMediaType( iPosition , pMediaType ); }
	virtual HRESULT SetMediaType(const CMediaType *pmt){ return CBaseOutputPin::SetMediaType(pmt); }
	//STDMETHODIMP EndOfStream(void);
	//STDMETHODIMP BeginFlush(void);
	//STDMETHODIMP EndFlush(void);

	//impl CBasePin
	virtual HRESULT CheckMediaType(const CMediaType *)PURE;

protected:
	CCritSec *m_pLock;
};


//////////////////////////////////////////////////////////////////////////
class CVideoOutPin : public CFePushPin
{
public:
	CVideoOutPin( CBaseFilter *pFilter , CCritSec *pLock , HRESULT *phr );
	virtual ~CVideoOutPin();

	virtual HRESULT FillBuffer(IMediaSample *pSamp);

	//impl CBaseOutputPin
	virtual HRESULT DecideBufferSize(IMemAllocator * pAlloc,__inout ALLOCATOR_PROPERTIES * ppropInputRequest);
	virtual HRESULT GetMediaType(int iPosition, __inout CMediaType *pMediaType);
	virtual HRESULT SetMediaType(const CMediaType *);
	STDMETHODIMP EndOfStream(void);
	STDMETHODIMP BeginFlush(void);
	STDMETHODIMP EndFlush(void);

	//impl CBasePin
	virtual HRESULT CheckMediaType(const CMediaType *);
};

//����filter,��Ҫ���н��빤��
class CParseFilter : public CBaseFilter
{
protected:
	// filter-wide lock
	CCritSec m_csFilter;
	CDataInputPin m_DataInputPin;
	CVideoOutPin m_VideoOutPin;
	CWrapFFMpeg m_ffmpeg;

	BITMAPFILEHEADER m_bmpHead;
	WAVEFORMATEX m_waveFmt;
	UNIT_BUF_POOL m_bufpool;
	//CObjPool<>
public:
	CParseFilter(LPUNKNOWN pUnk, HRESULT *phr);
	virtual ~CParseFilter(void);

	static CUnknown * WINAPI CreateInstance(LPUNKNOWN, HRESULT *);

	// you need to supply these to access the pins from the enumerator
	// and for default Stop and Pause/Run activation.
	virtual int GetPinCount();
	virtual CBasePin *GetPin(int n);
};

