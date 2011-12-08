#ifndef _FILTER_UTIL_H_
#define _FILTER_UTIL_H_

#include <streams.h>

HRESULT AddFilterByCLSID(
	IGraphBuilder *pGraph,  // Pointer to the Filter Graph Manager.
	const GUID& clsid,      // CLSID of the filter to create.
	LPCWSTR wszName,        // A name for the filter.
	IBaseFilter **ppF      // Receives a pointer to the filter.
	);

HRESULT GetUnconnectedPin(
	IBaseFilter *pFilter,   // Pointer to the filter.
	PIN_DIRECTION PinDir,   // Direction of the pin to find.
	IPin **ppPin           // Receives a pointer to the pin.
	);

HRESULT ConnectFilters(
	IGraphBuilder *pGraph, // Filter Graph Manager.
	IPin *pOut,            // Output pin on the upstream filter.
	IBaseFilter *pDest    // Downstream filter.
	);

HRESULT ConnectFilters(
	IGraphBuilder *pGraph, 
	IBaseFilter *pSrc, 
	IBaseFilter *pDest
	);

HRESULT FindFilterInterface(
	IGraphBuilder *pGraph, // Pointer to the Filter Graph Manager.
	REFGUID iid,           // IID of the interface to retrieve.
	void **ppUnk          // Receives the interface pointer.
	);

HRESULT FindPinInterface(
	IBaseFilter *pFilter,  // Pointer to the filter to search.
	REFGUID iid,           // IID of the interface.
	void **ppUnk          // Receives the interface pointer.
	);

HRESULT FindInterfaceAnywhere(
	IGraphBuilder *pGraph, 
	REFGUID iid, 
	void **ppUnk
	);

HRESULT GetNextFilter(
	IBaseFilter *pFilter, // Pointer to the starting filter
	PIN_DIRECTION Dir,    // Direction to search (upstream or downstream)
	IBaseFilter **ppNext // Receives a pointer to the next filter.
	);

// Define a typedef for a list of filters.
typedef CGenericList<IBaseFilter> CFilterList;

// Forward declaration. Adds a filter to the list unless it's a duplicate.
void AddFilterUnique(CFilterList &FilterList, IBaseFilter *pNew);

// Find all the immediate upstream or downstream peers of a filter.
HRESULT GetPeerFilters(
	IBaseFilter *pFilter, // Pointer to the starting filter
	PIN_DIRECTION Dir,    // Direction to search (upstream or downstream)
	CFilterList &FilterList  // Collect the results in this list.
	);

// Tear down everything downstream of a given filter
void NukeDownstream(IGraphBuilder * inGraph, IBaseFilter * inFilter);

// Tear down everything upstream of a given filter
void NukeUpstream(IGraphBuilder * inGraph, IBaseFilter * inFilter);

//得到filter时间
HRESULT GetRefTime( IFilterGraph* , REFERENCE_TIME* );

#endif