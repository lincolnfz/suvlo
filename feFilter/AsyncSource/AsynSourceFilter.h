#pragma once
#include "asyncio.h"
#include "FeBufPool.h"
#include "Wrapmms.h"

class CAsynReader;

class CAsynSourceOutPin : public CBasePin , public IAsyncReader
{
protected:
	CAsynReader* m_pReader; //所属filter指针
	BOOL m_bQueriedForAsyncReader;
	CAsyncIo * m_pIo;

public:
	CAsynSourceOutPin(HRESULT * phr, CAsynReader *pReader , CAsyncIo *pIo, CCritSec * pLock);
	virtual ~CAsynSourceOutPin();
	virtual HRESULT InitAllocator(__deref_out IMemAllocator **ppAlloc);

	// --- CUnknown ---
	// need to expose IAsyncReader
	DECLARE_IUNKNOWN
	STDMETHODIMP NonDelegatingQueryInterface(REFIID, void**);

	// --- IPin methods ---
	STDMETHODIMP Connect(
		IPin * pReceivePin,
		const AM_MEDIA_TYPE *pmt   // optional media type
		);

	// --- CBasePin methods ---
	// check if the pin can support this specific proposed type and format
	virtual HRESULT CheckMediaType(const CMediaType *);

	// check that the connection is ok before verifying it
	// can be overridden eg to check what interfaces will be supported.
	virtual HRESULT CheckConnect(IPin *pPin){
		m_bQueriedForAsyncReader = FALSE;
		return CBasePin::CheckConnect(pPin);
	}

	// Set and release resources required for a connection
	virtual HRESULT BreakConnect(){
		m_bQueriedForAsyncReader = FALSE;
		return CBasePin::BreakConnect();
	}

	virtual HRESULT CompleteConnect(IPin *pReceivePin){
		if (m_bQueriedForAsyncReader) {
			return CBasePin::CompleteConnect(pReceivePin);
		} else {

#ifdef VFW_E_NO_TRANSPORT
			return VFW_E_NO_TRANSPORT;
#else
			return E_FAIL;
#endif
		}
	}

	// returns the preferred formats for a pin
	virtual HRESULT GetMediaType(int iPosition, __inout CMediaType *pMediaType);

	// --- IAsyncReader methods ---
	// pass in your preferred allocator and your preferred properties.
	// method returns the actual allocator to be used. Call GetProperties
	// on returned allocator to learn alignment and prefix etc chosen.
	// this allocator will be not be committed and decommitted by
	// the async reader, only by the consumer.
	STDMETHODIMP RequestAllocator(
		IMemAllocator* pPreferred,
		ALLOCATOR_PROPERTIES* pProps,
		IMemAllocator ** ppActual);

	// queue a request for data.
	// media sample start and stop times contain the requested absolute
	// byte position (start inclusive, stop exclusive).
	// may fail if sample not obtained from agreed allocator.
	// may fail if start/stop position does not match agreed alignment.
	// samples allocated from source pin's allocator may fail
	// GetPointer until after returning from WaitForNext.
	STDMETHODIMP Request(
		IMediaSample* pSample,
		DWORD_PTR dwUser);         // user context

	// block until the next sample is completed or the timeout occurs.
	// timeout (millisecs) may be 0 or INFINITE. Samples may not
	// be delivered in order. If there is a read error of any sort, a
	// notification will already have been sent by the source filter,
	// and STDMETHODIMP will be an error.
	STDMETHODIMP WaitForNext(
		DWORD dwTimeout,
		IMediaSample** ppSample,  // completed sample
		DWORD_PTR * pdwUser);     // user context

	// sync read of data. Sample passed in must have been acquired from
	// the agreed allocator. Start and stop position must be aligned.
	// equivalent to a Request/WaitForNext pair, but may avoid the
	// need for a thread on the source filter.
	STDMETHODIMP SyncReadAligned(
		IMediaSample* pSample);


	// sync read. works in stopped state as well as run state.
	// need not be aligned. Will fail if read is beyond actual total
	// length.
	STDMETHODIMP SyncRead(
		LONGLONG llPosition,  // absolute file position
		LONG lLength,         // nr bytes required
		BYTE* pBuffer);       // write data here

	// return total length of stream, and currently available length.
	// reads for beyond the available length but within the total length will
	// normally succeed but may block for a long period.
	STDMETHODIMP Length(
		LONGLONG* pTotal,
		LONGLONG* pAvailable);

	// cause all outstanding reads to return, possibly with a failure code
	// (VFW_E_TIMEOUT) indicating they were cancelled.
	// these are defined on IAsyncReader and IPin
	STDMETHODIMP BeginFlush(void);
	STDMETHODIMP EndFlush(void);
};

class CAsynReader : public CBaseFilter
{
protected:
	// filter-wide lock
	CCritSec m_csFilter;

	// all i/o done here
	CAsyncIo m_Io;

	// Type we think our data is
	CMediaType m_mt;

	// our output pin
	CAsynSourceOutPin m_OutputPin;

public:

	// construction / destruction

	CAsynReader(
		TCHAR *pName,
		LPUNKNOWN pUnk,
		CAsyncStream *pStream,
		HRESULT *phr);

	virtual ~CAsynReader();



	// --- CBaseFilter methods ---
	int GetPinCount();
	CBasePin *GetPin(int n);

	// --- Access our media type
	const CMediaType *LoadType() const
	{
		return &m_mt;
	}

	virtual HRESULT Connect(
		IPin * pReceivePin,
		const AM_MEDIA_TYPE *pmt   // optional media type
		)
	{
		return m_OutputPin.CBasePin::Connect(pReceivePin, pmt);
	}
};

class CAsynSourceFilter : public CAsynReader , public IFeFileSource
{
protected:
	CFeBufPool m_feBufPool;
	CWrapmms m_wrapmms;
	//上面两个变量顺序不能颠倒?,应该是可以

public:
	CAsynSourceFilter(LPUNKNOWN pUnk, HRESULT *phr);
	virtual ~CAsynSourceFilter();

	static CUnknown * WINAPI CreateInstance(LPUNKNOWN, HRESULT *);

	DECLARE_IUNKNOWN
	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void **ppv)
	{
		if (riid == IID_IFeFileSource) {
			return GetInterface((IFeFileSource *)this, ppv);
		} else {
			return CBaseFilter::NonDelegatingQueryInterface(riid, ppv);
		}
	}

	//--- IFeFileSource methods ---
	STDMETHODIMP Play( LPCWSTR url );
	STDMETHODIMP Seek( ULONG64 utime );
	STDMETHODIMP Stop( void );
};
