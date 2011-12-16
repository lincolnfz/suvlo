#ifndef _DEFHEAD_H_
#define _DEFHEAD_H_

#include <strsafe.h>
#include "objbase.h "
#include "initguid.h"


// {74E33997-FC0B-4A8E-839B-AFF3ED7CF4E5}
DEFINE_GUID( CLSID_AsynSource ,
	0x74e33997, 0xfc0b, 0x4a8e, 0x83, 0x9b, 0xaf, 0xf3, 0xed, 0x7c, 0xf4, 0xe5);

// {1084F53E-1DD4-4413-928A-4CD37C8BF7FF}
DEFINE_GUID( IID_IFeFileSource , 
	0x1084f53e, 0x1dd4, 0x4413, 0x92, 0x8a, 0x4c, 0xd3, 0x7c, 0x8b, 0xf7, 0xff);

#ifdef _cplusplus
extern "c"{
#endif	

	DECLARE_INTERFACE_( IFeFileSource , IUnknown )
	{
		STDMETHOD( Play )(THIS_ LPCWSTR url)PURE;
		STDMETHOD( Seek )(THIS_ ULONG64 utime)PURE;
		STDMETHOD( Stop )(void )PURE;
	};

#ifdef _cplusplus
}
#endif

#endif