#pragma once

class CFeBufPool : public CAsyncStream
{
public:
	CFeBufPool(void);
	~CFeBufPool(void);
	virtual HRESULT SetPointer(LONGLONG llPos) ;
	virtual HRESULT Read(PBYTE pbBuffer, DWORD dwBytesToRead, BOOL bAlign, LPDWORD pdwBytesRead);
	virtual LONGLONG Size(LONGLONG *pSizeAvailable = NULL);
	virtual DWORD Alignment();
	virtual void Lock();
	virtual void Unlock();
};

