#pragma once
#include "../common/DefInterface.h"
#include <pullpin.h>

class CDataPull : public CPullPin
{
public:
	CDataPull();
	~CDataPull();

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
};

class CDataInputPin : public CBasePin
{
public:
	CDataInputPin( CBaseFilter *pFilter , CCritSec *pLock , HRESULT *phr );
	~CDataInputPin();

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

