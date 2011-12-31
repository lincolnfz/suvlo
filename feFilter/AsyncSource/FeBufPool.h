#pragma once
#include "../common/feUtil.h"
#include "../common/bufpool.h"

class CBufPoolOper{
	virtual long filldata( char* , long len ) = 0;
};

//////////////////////////////////////////////////////////////////////////
class CFeBufPool : public CAsyncStream
{
public:
	CFeBufPool( int units = 10 , long size = 32768 );
	~CFeBufPool(void);
	virtual HRESULT SetPointer(LONGLONG llPos);
	virtual HRESULT Read(PBYTE pbBuffer, DWORD dwBytesToRead, BOOL bAlign, LPDWORD pdwBytesRead);
	virtual LONGLONG Size(LONGLONG *pSizeAvailable = NULL);
	virtual DWORD Alignment();
	virtual void Lock();
	virtual void Unlock();

	//À©Õ¹·½·¨
	UNIT_BUF_POOL *getPool(){ return &m_poolbuf; }

protected:
	UNIT_BUF_POOL m_poolbuf;
	CCritSec       m_csLock;
};

