// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include <streams.h>
#include <initguid.h>
#include "defhead.h"
#include "AsynSourceFilter.h"

#define g_wszAsynSourceFilter L"Asyn Source Filter"

//定义媒体类型
AMOVIESETUP_MEDIATYPE sudMediaTypes[]={
	{
		&MEDIATYPE_Stream ,  // Major type
		&MEDIASUBTYPE_NULL  // Minor type
	}
};

//定义引脚
AMOVIESETUP_PIN subOutputPin[]={
	{
		L"Stream_Output",      // Obsolete, not used.
		FALSE,			// Is this pin rendered?
		TRUE,           // Is it an output pin?
		FALSE,          // Can the filter create zero instances?
		FALSE,          // Does the filter create multiple instances?
		&CLSID_NULL,    // Obsolete.
		NULL,           // Obsolete.
		1, // Number of media types.
		&sudMediaTypes[0] // Pointer to media types.
	}
};

//定义滤波器
AMOVIESETUP_FILTER subNetMediaFilters[] = {
	{
		&CLSID_AsynSource,    // Filter CLSID
		g_wszAsynSourceFilter,        // String name
		MERIT_DO_NOT_USE,       // Filter merit
		sizeof(subOutputPin)/sizeof(subOutputPin[0]), // Number pins
		subOutputPin     // Pin details
	}
};

// List of class IDs and creator functions for the class factory. This
// provides the link between the OLE entry point in the DLL and an object
// being created. The class factory will call the static CreateInstance.
// We provide a set of filters in this one DLL.

CFactoryTemplate g_Templates[] = {
	{
		g_wszAsynSourceFilter,    // Name
		&CLSID_AsynSource,  // CLSID
		CAsynSourceFilter::CreateInstance,                    // Method to create an instance of MyComponent
		NULL,                    // Initialization function
		&subNetMediaFilters[0]   // Set-up information (for filters)
	}
};

int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);

STDAPI DllRegisterServer()
{
	return AMovieDllRegisterServer2( TRUE );
}

STDAPI DllUnregisterServer()
{
	return AMovieDllRegisterServer2( FALSE );
}

//
// DllEntryPoint
//
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, 
	DWORD  dwReason, 
	LPVOID lpReserved)
{
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}