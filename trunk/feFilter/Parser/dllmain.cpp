// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "stdafx.h"
#include "ParseFilter.h"

#define g_wszParserFilter L"Parser Filter"

//定义媒体类型
AMOVIESETUP_MEDIATYPE sudMediaTypes[]={
	{
		&MEDIATYPE_Stream ,  // Major type
		&MEDIASUBTYPE_NULL  // Minor type
	},
	{
		&MEDIATYPE_Video ,  // Major type
		&MEDIASUBTYPE_NULL  // Minor type
	},
	{
		&MEDIATYPE_Audio ,  // Major type
		&MEDIASUBTYPE_NULL  // Minor type
	}
};

//定义引脚
AMOVIESETUP_PIN subPin[] = {
	{
		L"Stream_Input",      // Obsolete, not used.
		FALSE,			// Is this pin rendered?
		FALSE,           // Is it an output pin?
		FALSE,          // Can the filter create zero instances?
		FALSE,          // Does the filter create multiple instances?
		&CLSID_NULL,    // Obsolete.
		NULL,           // Obsolete.
		1, // Number of media types.
		&sudMediaTypes[0] // Pointer to media types.
	},
	{
		L"Video_Output",      // Obsolete, not used.
		FALSE,			// Is this pin rendered?
		TRUE,           // Is it an output pin?
		FALSE,          // Can the filter create zero instances?
		FALSE,          // Does the filter create multiple instances?
		&CLSID_NULL,    // Obsolete.
		NULL,           // Obsolete.
		1, // Number of media types.
		&sudMediaTypes[1] // Pointer to media types.
	},
	{
		L"Audio_Output",      // Obsolete, not used.
		FALSE,			// Is this pin rendered?
		TRUE,           // Is it an output pin?
		FALSE,          // Can the filter create zero instances?
		FALSE,          // Does the filter create multiple instances?
		&CLSID_NULL,    // Obsolete.
		NULL,           // Obsolete.
		1, // Number of media types.
		&sudMediaTypes[2] // Pointer to media types.
	}	

};

//定义滤波器
AMOVIESETUP_FILTER subMediaFilters[] = {
	{
		&CLSID_Parser,    // Filter CLSID
		g_wszParserFilter,        // String name
		MERIT_DO_NOT_USE,       // Filter merit
		sizeof(subPin)/sizeof(subPin[0]), // Number pins
		subPin     // Pin details
	}
};

// List of class IDs and creator functions for the class factory. This
// provides the link between the OLE entry point in the DLL and an object
// being created. The class factory will call the static CreateInstance.
// We provide a set of filters in this one DLL.

CFactoryTemplate g_Templates[] = {
	{
		g_wszParserFilter,    // Name
		&CLSID_Parser,  // CLSID
		CParseFilter::CreateInstance,                    // Method to create an instance of MyComponent
		NULL,                    // Initialization function
		&subMediaFilters[0]   // Set-up information (for filters)
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
