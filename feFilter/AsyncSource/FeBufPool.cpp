#include <streams.h>
#include "asyncio.h"
#include "FeBufPool.h"


CFeBufPool::CFeBufPool(int units/* = 10*/ , long size/* = 131072*/)
{
	//m_poolbuf = NULL;
	InitPool( &m_poolbuf , units , size );
}


CFeBufPool::~CFeBufPool(void)
{
	DestoryPool( &m_poolbuf );
}

//���õ�ǰλ��
HRESULT CFeBufPool::SetPointer(LONGLONG llPos)
{
	m_poolbuf.llPosition = llPos;
	return S_OK;
}

HRESULT CFeBufPool::Read(PBYTE pbBuffer, DWORD dwBytesToRead, BOOL bAlign, LPDWORD pdwBytesRead)
{
	*pdwBytesRead = ReadData( &m_poolbuf , (PCHAR)pbBuffer , dwBytesToRead );
	return S_OK;
}

//�����ļ��ĳ���
LONGLONG CFeBufPool::Size(LONGLONG *pSizeAvailable /*= NULL*/)
{
	return m_poolbuf.llRaw;
}

//���ݰ�1�ֽڶ���
DWORD CFeBufPool::Alignment()
{
	return 1;
}

void CFeBufPool::Lock()
{
	m_csLock.Lock();
}

void CFeBufPool::Unlock()
{
	m_csLock.Unlock();
}
