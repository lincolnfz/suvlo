//////////////////////////////////////////////////////////////////////////////
///////////////////// Graph building methods /////////////////////////////////

#include "filterUtil.h"

#ifdef _DEBUG
#include <crtdbg.h>
#endif

//////////////////////////////////////////////////////////////////////////////
///////////////////// Graph building methods /////////////////////////////////

HRESULT AddFilterByCLSID(
	IGraphBuilder *pGraph,  // Pointer to the Filter Graph Manager.
	const GUID& clsid,      // CLSID of the filter to create.
	LPCWSTR wszName,        // A name for the filter.
	IBaseFilter **ppF)      // Receives a pointer to the filter.
{
	if (!pGraph || ! ppF) return E_POINTER;
	*ppF = 0;
	IBaseFilter *pF = 0;
	HRESULT hr = CoCreateInstance(clsid, 0, CLSCTX_INPROC_SERVER,
		IID_IBaseFilter, reinterpret_cast<void**>(&pF));
	if (SUCCEEDED(hr))
	{
		hr = pGraph->AddFilter(pF, wszName);
		if (SUCCEEDED(hr))
			*ppF = pF;
		else
			pF->Release();
	}
	return hr;
}

HRESULT GetUnconnectedPin(
	IBaseFilter *pFilter,   // Pointer to the filter.
	PIN_DIRECTION PinDir,   // Direction of the pin to find.
	IPin **ppPin)           // Receives a pointer to the pin.
{
	*ppPin = 0;
	IEnumPins *pEnum = 0;
	IPin *pPin = 0;
	HRESULT hr = pFilter->EnumPins(&pEnum);
	if (FAILED(hr))
	{
		return hr;
	}
	while (pEnum->Next(1, &pPin, NULL) == S_OK)
	{
		PIN_DIRECTION ThisPinDir;
		pPin->QueryDirection(&ThisPinDir);
		if (ThisPinDir == PinDir)
		{
			IPin *pTmp = 0;
			hr = pPin->ConnectedTo(&pTmp);
			if (SUCCEEDED(hr))  // Already connected, not the pin we want.
			{
				pTmp->Release();
			}
			else  // Unconnected, this is the pin we want.
			{
				pEnum->Release();
				*ppPin = pPin;
				return S_OK;
			}
		}
		pPin->Release();
	}
	pEnum->Release();
	// Did not find a matching pin.
	return E_FAIL;
}

HRESULT ConnectFilters(
	IGraphBuilder *pGraph, // Filter Graph Manager.
	IPin *pOut,            // Output pin on the upstream filter.
	IBaseFilter *pDest)    // Downstream filter.
{
	if ((pGraph == NULL) || (pOut == NULL) || (pDest == NULL))
	{
		return E_POINTER;
	}
#ifdef _DEBUG
	PIN_DIRECTION PinDir;
	pOut->QueryDirection(&PinDir);
	_ASSERTE(PinDir == PINDIR_OUTPUT);
#endif

	// Find an input pin on the downstream filter.
	IPin *pIn = 0;
	HRESULT hr = GetUnconnectedPin(pDest, PINDIR_INPUT, &pIn);
	if (FAILED(hr))
	{
		return hr;
	}
	// Try to connect them.
	hr = pGraph->Connect(pOut, pIn);
	pIn->Release();
	return hr;
}

HRESULT ConnectFilters(
	IGraphBuilder *pGraph, 
	IBaseFilter *pSrc, 
	IBaseFilter *pDest)
{
	if ((pGraph == NULL) || (pSrc == NULL) || (pDest == NULL))
	{
		return E_POINTER;
	}

	// Find an output pin on the first filter.
	IPin *pOut = 0;
	HRESULT hr = GetUnconnectedPin(pSrc, PINDIR_OUTPUT, &pOut);
	if (FAILED(hr)) 
	{
		return hr;
	}
	hr = ConnectFilters(pGraph, pOut, pDest);
	pOut->Release();
	return hr;
}

HRESULT FindFilterInterface(
	IGraphBuilder *pGraph, // Pointer to the Filter Graph Manager.
	REFGUID iid,           // IID of the interface to retrieve.
	void **ppUnk)          // Receives the interface pointer.
{
	if (!pGraph || !ppUnk) return E_POINTER;

	HRESULT hr = E_FAIL;
	IEnumFilters *pEnum = NULL;
	IBaseFilter *pF = NULL;
	if (FAILED(pGraph->EnumFilters(&pEnum)))
	{
		return E_FAIL;
	}
	// Query every filter for the interface.
	while (S_OK == pEnum->Next(1, &pF, 0))
	{
		hr = pF->QueryInterface(iid, ppUnk);
		pF->Release();
		if (SUCCEEDED(hr))
		{
			break;
		}
	}
	pEnum->Release();
	return hr;
}

HRESULT FindPinInterface(
	IBaseFilter *pFilter,  // Pointer to the filter to search.
	REFGUID iid,           // IID of the interface.
	void **ppUnk)          // Receives the interface pointer.
{
	if (!pFilter || !ppUnk) return E_POINTER;

	HRESULT hr = E_FAIL;
	IEnumPins *pEnum = 0;
	if (FAILED(pFilter->EnumPins(&pEnum)))
	{
		return E_FAIL;
	}
	// Query every pin for the interface.
	IPin *pPin = 0;
	while (S_OK == pEnum->Next(1, &pPin, 0))
	{
		hr = pPin->QueryInterface(iid, ppUnk);
		pPin->Release();
		if (SUCCEEDED(hr))
		{
			break;
		}
	}
	pEnum->Release();
	return hr;
}

HRESULT FindInterfaceAnywhere(
	IGraphBuilder *pGraph, 
	REFGUID iid, 
	void **ppUnk)
{
	if (!pGraph || !ppUnk) return E_POINTER;
	HRESULT hr = E_FAIL;
	IEnumFilters *pEnum = 0;
	if (FAILED(pGraph->EnumFilters(&pEnum)))
	{
		return E_FAIL;
	}
	// Loop through every filter in the graph.
	IBaseFilter *pF = 0;
	while (S_OK == pEnum->Next(1, &pF, 0))
	{
		hr = pF->QueryInterface(iid, ppUnk);
		if (FAILED(hr))
		{
			// The filter does not expose the interface, but maybe
			// one of its pins does.
			hr = FindPinInterface(pF, iid, ppUnk);
		}
		pF->Release();
		if (SUCCEEDED(hr))
		{
			break;
		}
	}
	pEnum->Release();
	return hr;
}

// Get the first upstream or downstream filter
HRESULT GetNextFilter(
	IBaseFilter *pFilter, // Pointer to the starting filter
	PIN_DIRECTION Dir,    // Direction to search (upstream or downstream)
	IBaseFilter **ppNext) // Receives a pointer to the next filter.
{
	if (!pFilter || !ppNext) return E_POINTER;

	IEnumPins *pEnum = 0;
	IPin *pPin = 0;
	HRESULT hr = pFilter->EnumPins(&pEnum);
	if (FAILED(hr)) return hr;
	while (S_OK == pEnum->Next(1, &pPin, 0))
	{
		// See if this pin matches the specified direction.
		PIN_DIRECTION ThisPinDir;
		hr = pPin->QueryDirection(&ThisPinDir);
		if (FAILED(hr))
		{
			// Something strange happened.
			hr = E_UNEXPECTED;
			pPin->Release();
			break;
		}
		if (ThisPinDir == Dir)
		{
			// Check if the pin is connected to another pin.
			IPin *pPinNext = 0;
			hr = pPin->ConnectedTo(&pPinNext);
			if (SUCCEEDED(hr))
			{
				// Get the filter that owns that pin.
				PIN_INFO PinInfo;
				hr = pPinNext->QueryPinInfo(&PinInfo);
				pPinNext->Release();
				pPin->Release();
				pEnum->Release();
				if (FAILED(hr) || (PinInfo.pFilter == NULL))
				{
					// Something strange happened.
					return E_UNEXPECTED;
				}
				// This is the filter we're looking for.
				*ppNext = PinInfo.pFilter; // Client must release.
				return S_OK;
			}
		}
		pPin->Release();
	}
	pEnum->Release();
	// Did not find a matching filter.
	return E_FAIL;
}


// Find all the immediate upstream or downstream peers of a filter.
HRESULT GetPeerFilters(
	IBaseFilter *pFilter, // Pointer to the starting filter
	PIN_DIRECTION Dir,    // Direction to search (upstream or downstream)
	CFilterList &FilterList)  // Collect the results in this list.
{
	if (!pFilter) return E_POINTER;

	IEnumPins *pEnum = 0;
	IPin *pPin = 0;
	HRESULT hr = pFilter->EnumPins(&pEnum);
	if (FAILED(hr)) return hr;
	while (S_OK == pEnum->Next(1, &pPin, 0))
	{
		// See if this pin matches the specified direction.
		PIN_DIRECTION ThisPinDir;
		hr = pPin->QueryDirection(&ThisPinDir);
		if (FAILED(hr))
		{
			// Something strange happened.
			hr = E_UNEXPECTED;
			pPin->Release();
			break;
		}
		if (ThisPinDir == Dir)
		{
			// Check if the pin is connected to another pin.
			IPin *pPinNext = 0;
			hr = pPin->ConnectedTo(&pPinNext);
			if (SUCCEEDED(hr))
			{
				// Get the filter that owns that pin.
				PIN_INFO PinInfo;
				hr = pPinNext->QueryPinInfo(&PinInfo);
				pPinNext->Release();
				if (FAILED(hr) || (PinInfo.pFilter == NULL))
				{
					// Something strange happened.
					pPin->Release();
					pEnum->Release();
					return E_UNEXPECTED;
				}
				// Insert the filter into the list.
				AddFilterUnique(FilterList, PinInfo.pFilter);
				PinInfo.pFilter->Release();
			}
		}
		pPin->Release();
	}
	pEnum->Release();
	return S_OK;
}

void AddFilterUnique(CFilterList &FilterList, IBaseFilter *pNew)
{
	if (pNew == NULL) return;

	POSITION pos = FilterList.GetHeadPosition();
	while (pos)
	{
		IBaseFilter *pF = FilterList.GetNext(pos);
		if (IsEqualObject(pF, pNew))
		{
			return;
		}
	}
	pNew->AddRef();  // The caller must release everything in the list.
	FilterList.AddTail(pNew);
}


// Tear down everything downstream of a given filter
void NukeDownstream(IGraphBuilder * inGraph, IBaseFilter * inFilter) 
{
	if (inGraph && inFilter)
	{
		IEnumPins * pinEnum = 0;
		if (SUCCEEDED(inFilter->EnumPins(&pinEnum)))
		{
			pinEnum->Reset();
			IPin * pin = 0;
			ULONG cFetched = 0;
			bool pass = true;
			while (pass && SUCCEEDED(pinEnum->Next(1, &pin, &cFetched)))
			{
				if (pin && cFetched)
				{
					IPin * connectedPin = 0;
					pin->ConnectedTo(&connectedPin);
					if(connectedPin) 
					{
						PIN_INFO pininfo;
						if (SUCCEEDED(connectedPin->QueryPinInfo(&pininfo)))
						{
							if(pininfo.dir == PINDIR_INPUT) 
							{
								NukeDownstream(inGraph, pininfo.pFilter);
								inGraph->Disconnect(connectedPin);
								inGraph->Disconnect(pin);
								inGraph->RemoveFilter(pininfo.pFilter);
							}
							pininfo.pFilter->Release();
						}
						connectedPin->Release();
					}
					pin->Release();
				}
				else
				{
					pass = false;
				}
			}
			pinEnum->Release();
		}
	}
}

// Tear down everything upstream of a given filter
void NukeUpstream(IGraphBuilder * inGraph, IBaseFilter * inFilter) 
{
	if (inGraph && inFilter)
	{
		IEnumPins * pinEnum = 0;
		if (SUCCEEDED(inFilter->EnumPins(&pinEnum)))
		{
			pinEnum->Reset();
			IPin * pin = 0;
			ULONG cFetched = 0;
			bool pass = true;
			while (pass && SUCCEEDED(pinEnum->Next(1, &pin, &cFetched)))
			{
				if (pin && cFetched)
				{
					IPin * connectedPin = 0;
					pin->ConnectedTo(&connectedPin);
					if(connectedPin) 
					{
						PIN_INFO pininfo;
						if (SUCCEEDED(connectedPin->QueryPinInfo(&pininfo)))
						{
							if(pininfo.dir == PINDIR_OUTPUT) 
							{
								NukeUpstream(inGraph, pininfo.pFilter);
								inGraph->Disconnect(connectedPin);
								inGraph->Disconnect(pin);
								inGraph->RemoveFilter(pininfo.pFilter);
							}
							pininfo.pFilter->Release();
						}
						connectedPin->Release();
					}
					pin->Release();
				}
				else
				{
					pass = false;
				}
			}
			pinEnum->Release();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////