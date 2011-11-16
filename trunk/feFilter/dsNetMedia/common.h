#ifndef _COMMON_H
#define _COMMON_H

// {7CCDB408-C5CA-447B-A34B-E1106F0D0119}
DEFINE_GUID( CLSID_NetMediaSource , 
	0x7ccdb408, 0xc5ca, 0x447b, 0xa3, 0x4b, 0xe1, 0x10, 0x6f, 0xd, 0x1, 0x19);

// {245E62C2-432B-46DA-A0CF-A9B1D8710481}
DEFINE_GUID( IID_INetSource, 
	0x245e62c2, 0x432b, 0x46da, 0xa0, 0xcf, 0xa9, 0xb1, 0xd8, 0x71, 0x4, 0x81);

#ifdef _cplusplus
extern "c"{
#endif	

	DECLARE_INTERFACE_( INetSource , IUnknown )
	{
		STDMETHOD( play )(THIS_ LPCWSTR url)PURE;
		STDMETHOD( seek )(THIS_ ULONG64 utime)PURE;
	};

#ifdef _cplusplus
}
#endif

typedef int (*NotifyParserOK)();

#endif