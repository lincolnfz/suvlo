#include <streams.h>
#include "asyncio.h"
#include "FeBufPool.h"


CFeBufPool::CFeBufPool(void)
{
}


CFeBufPool::~CFeBufPool(void)
{
}

HRESULT CFeBufPool::SetPointer(LONGLONG llPos)
{
	return S_OK;
}

HRESULT CFeBufPool::Read(PBYTE pbBuffer, DWORD dwBytesToRead, BOOL bAlign, LPDWORD pdwBytesRead)
{
	return S_OK;
}

LONGLONG CFeBufPool::Size(LONGLONG *pSizeAvailable /*= NULL*/)
{
	return S_OK;
}

DWORD CFeBufPool::Alignment()
{
	return 1;
}

void CFeBufPool::Lock()
{
	
}

void CFeBufPool::Unlock()
{
	
}