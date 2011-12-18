#include <streams.h>
#include "asyncio.h"
#include "FeBufPool.h"


CFeBufPool::CFeBufPool(int units/* = 10*/ , long size/* = 65536*/)
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

//�����ļ��Ĳ��ų���100nsΪ��λ (10^-7)
LONGLONG CFeBufPool::Size(LONGLONG *pSizeAvailable /*= NULL*/)
{
	return m_poolbuf.llRaw /= 10000000;
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

//////////////////////////////////////////////////////////////////////////
//�����
void InitPool( UNIT_BUF_POOL* pool, int units , long size )
{
	pool->sec = 0.0;
	pool->llRaw = 0;
	pool->pRead = NULL;
	pool->pWrite = NULL;
	initDataLink(&pool->pEmptyLink);
	initDataLink(&pool->pFullLink);
	pool->units = units;
	pool->unit_size = size;
	pool->pList = new UNIT_BUF[ pool->units ];
	int idx = 0;
	for ( idx = 0 ; idx < pool->units ; ++idx )
	{	
		pool->pList[idx].opsize = 0;
		pool->pList[idx].eof = 0;
		pool->pList[idx].size = pool->unit_size;
		pool->pList[idx].position = 0;
		pool->pList[idx].pHead = new char[pool->pList[idx].size];
		DataNode<UNIT_BUF*> *pnode = new DataNode<UNIT_BUF*>;
		pnode->pData = &pool->pList[idx];
		putDataLink( pool->pEmptyLink , pnode );
	}	
	
}

void DestoryPool( UNIT_BUF_POOL* pool)
{
	int idx = 0;
	for ( idx = 0 ; idx < pool->units ; ++idx )
	{
		delete [](pool->pList[idx].pHead);		
		pool->pList[idx].pHead = NULL;
		pool->pList[idx].size = 0;
	}
	delete [](pool->pList);	
	pool->pList = NULL;
	pool->units = 0;
	destoryDataLink(&pool->pEmptyLink);
	destoryDataLink(&pool->pFullLink);
}

void ResetPool( UNIT_BUF_POOL* pool){
	pool->sec = 0.0;
	pool->llRaw = 0;
	pool->pRead = NULL;
	pool->pWrite = NULL;
	int idx = 0;
	for ( idx = 0 ; idx < pool->units ; ++idx )
	{
		pool->pList[idx].opsize = 0;
		pool->pList[idx].eof = 0;
		pool->pList[idx].position = 0;
		pool->pList[idx].size = pool->unit_size;
	}
}

/**
����:����ύ�Ѿ���ȡ��ϵĵ�Ԫ,���õ�һ���µ����ݵ�Ԫ
*/
DataNode<UNIT_BUF*> *GetReadUnit(UNIT_BUF_POOL *pool , DataNode<UNIT_BUF*> *pStale )
{
	DataNode<UNIT_BUF*> *pUnit = pStale;
	if ( !pUnit )
	{
		goto getnew;
	}
	if ( pUnit->pData->eof ) //�������һ����Ч���ݵ�Ԫ
	{
		return pUnit;
	}
	if ( pUnit && pUnit->pData->opsize < pUnit->pData->size )
	{
		return pUnit;
	}
	pUnit->pData->opsize = 0;
	putDataLink( pool->pEmptyLink , pUnit );

getnew:
	pUnit = getDataLink( pool->pFullLink );
	return pUnit;
}

/**
����:����ύ�Ѿ�д��ϵĵ�Ԫ,���õ�һ���յ����ݵ�Ԫ
*/
DataNode<UNIT_BUF*> *GetWriteUnit(UNIT_BUF_POOL *pool , DataNode<UNIT_BUF*> *pStale )
{
	DataNode<UNIT_BUF*> *pUnit = pStale;
	if ( !pUnit )
	{
		goto getnew;
	}
	if ( pUnit && pUnit->pData->opsize < pUnit->pData->size )
	{
		return pUnit;
	}
	pUnit->pData->opsize = 0;
	LONGLONG llpos = pUnit->pData->position;
	putDataLink( pool->pFullLink , pUnit );

getnew:
	pUnit = getDataLink( pool->pEmptyLink ); 
	pUnit->pData->position += pool->unit_size;
	return pUnit;
}

//��� len <=0 ˵���Ѿ�û������
long WriteData( UNIT_BUF_POOL *pool , char *srcbuf , long len )
{		
	long total = 0;
	UNIT_BUF *pBufUnit = NULL;
	while( len > 0 ){
		long remain = 0;
		long lop = 0;
		pool->pWrite = GetWriteUnit( pool , pool->pWrite );
		pBufUnit = pool->pWrite->pData;
		remain = pBufUnit->size - pBufUnit->opsize;
		lop = (remain >= len) ? len : remain;
		CopyMemory( pBufUnit->pHead + pBufUnit->opsize , srcbuf , lop );
		pBufUnit->opsize += lop;
		len -= lop;
		total += lop;
	}
	if ( total <= 0 )
	{
		pool->pWrite = GetWriteUnit( pool , pool->pWrite );
		pool->pWrite->pData->eof = 1;
		pool->pWrite->pData->size = pool->pWrite->pData->opsize;
		putDataLink( pool->pFullLink , pool->pWrite );
	}

	return total;
}

long ReadData( UNIT_BUF_POOL *pool , char *dstbuf , long len )
{
	long total = 0;
	UNIT_BUF *pBufUnit = NULL;
	while( len > 0 ){
		long remain = 0;
		long lop = 0;
		pool->pRead = GetReadUnit( pool , pool->pRead );
		pBufUnit = pool->pRead->pData;
		remain = pBufUnit->size - pBufUnit->opsize;
		lop = (remain >= len) ? len : remain;
		if ( lop > 0 )
		{
			CopyMemory( dstbuf , pBufUnit->pHead + pBufUnit->opsize , lop );
			pBufUnit->opsize += lop;
			len -= lop;
			total += lop;
		}else{
			pool->pRead->pData->eof = 0;
			pool->pRead->pData->opsize = 0;
			pool->pRead->pData->size = pool->unit_size;
			putDataLink( pool->pEmptyLink , pool->pRead );
			break;
		}
		
	}
	return total;
}