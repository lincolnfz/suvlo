#include "StdAfx.h"
#include "MultiControl.h"

CMultiControl::CMultiControl(void)
{
	m_pGraph = NULL;
	m_pMediaControl = NULL;
	m_pMediaEvent = NULL;
}


CMultiControl::~CMultiControl(void)
{
}

BOOL CMultiControl::Init()
{
	//HRESULT hr = ::CoInitialize(NULL);
	HRESULT hr = CoCreateInstance( CLSID_FilterGraph , NULL , CLSCTX_INPROC_SERVER , IID_IGraphBuilder , (LPVOID*)&m_pGraph );

	m_pGraph->QueryInterface( IID_IMediaControl , (LPVOID*)&m_pMediaControl );

	m_pGraph->QueryInterface( IID_IMediaEvent , (LPVOID*)&m_pMediaEvent );

	return TRUE;
}

BOOL CMultiControl::Destory()
{
	m_pMediaControl->Release();
	m_pMediaEvent->Release();
	m_pGraph->Release();
	return TRUE;
}