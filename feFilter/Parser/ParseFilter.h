#pragma once
#include "../common/DefInterface.h"
#include <pullpin.h>

class CDataPull : public CPullPin
{
public:
	CDataPull();
	~CDataPull();

	//实现cpullpin虚函数
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
};

class CDataInputPin : public CBasePin
{
public:
	CDataInputPin( CBaseFilter *pFilter , CCritSec *pLock , HRESULT *phr );
	~CDataInputPin();

	//拉模式中调用pullpin的content
	virtual HRESULT CheckConnect(IPin *);

	//拉模式中调用pullpin的disconnect
	virtual HRESULT BreakConnect();

	//拉模式中使用pullpin的active拉动数据,
	virtual HRESULT Active(void);

	//拉模式中使用pullpin的inactive停止数据
	virtual HRESULT Inactive(void);

	//--------CBasePin----------------
	// check if the pin can support this specific proposed type and format
	virtual HRESULT CheckMediaType(const CMediaType *);

	virtual HRESULT STDMETHODCALLTYPE BeginFlush( void);

	virtual HRESULT STDMETHODCALLTYPE EndFlush( void);

protected:
	CDataPull m_pullPin;

};

class CParseFilter : public CBaseFilter
{
protected:
	// filter-wide lock
	CCritSec m_csFilter;
	CDataInputPin *m_pDataInputPin;

public:
	CParseFilter(LPUNKNOWN pUnk, HRESULT *phr);
	~CParseFilter(void);

	static CUnknown * WINAPI CreateInstance(LPUNKNOWN, HRESULT *);

	// you need to supply these to access the pins from the enumerator
	// and for default Stop and Pause/Run activation.
	virtual int GetPinCount();
	virtual CBasePin *GetPin(int n);
};

