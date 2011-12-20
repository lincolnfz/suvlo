#include "StdAfx.h"
#include "ParseFilter.h"
#include "../common/filterUtil.h"

//////////////////////////////////////////////////////////////////////////
//datapull继承类
CDataPull::CDataPull()
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
CDataInputPin::CDataInputPin( CBaseFilter *pFilter , CCritSec *pLock , HRESULT *phr )
	:CBasePin( NAME("parser input pin") , pFilter , pLock , phr , L"Input" , PINDIR_INPUT )
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
//parserFilter
CParseFilter::CParseFilter(LPUNKNOWN pUnk, HRESULT *phr)
	:CBaseFilter( NAME("parse filter") , pUnk , &m_csFilter , CLSID_Parser , phr )
{
	m_pDataInputPin = new CDataInputPin( this , &m_csFilter , phr );
}


CParseFilter::~CParseFilter(void)
{
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
			return m_pDataInputPin;
		}else if( 1 == n ){
			return pin;
		}else if ( 2 == n )
		{
			return pin;
		}
	}
}