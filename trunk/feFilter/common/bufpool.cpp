#include "bufpool.h"

//////////////////////////////////////////////////////////////////////////
//缓冲池
void InitPool( UNIT_BUF_POOL* pool, int units , long unit_size )
{
	pool->sec = 0.0;
	pool->llRaw = 0;
	pool->pRead = NULL;
	pool->pWrite = NULL;
	initDataLink(&pool->pEmptyLink);
	initDataLink(&pool->pFullLink);
	pool->units = units;
	pool->unit_size = unit_size;
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
	pool->hGetQueueMutex = CreateMutex( NULL , FALSE , NULL );

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
	CloseHandle( pool->hGetQueueMutex );
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
功能:向池提交已经读取完毕的单元,并得到一个新的数据单元
*/
DataNode<UNIT_BUF*> *GetReadUnit(UNIT_BUF_POOL *pool , DataNode<UNIT_BUF*> *pStale )
{	
	DataNode<UNIT_BUF*> *pUnit = pStale;
	if ( !pUnit )
	{
		goto newinstance;
	}
	if ( pUnit->pData->eof ) //这是最后一个有效数据单元
	{
		goto get;
	}
	if ( pUnit->pData->opsize < pUnit->pData->size )
	{
		goto get;
	}
	//pUnit->pData->opsize = 0;
	putDataLink( pool->pEmptyLink , pUnit );

newinstance:
	pUnit = getDataLink( pool->pFullLink );
	pUnit->pData->opsize = 0;

get:
	return pUnit;
}

/**
功能:向池提交已经写完毕的单元,并得到一个空的数据单元
*/
DataNode<UNIT_BUF*> *GetWriteUnit(UNIT_BUF_POOL *pool , DataNode<UNIT_BUF*> *pStale )
{
	DataNode<UNIT_BUF*> *pUnit = pStale;
	if ( !pUnit )
	{
		goto newinstance;
	}
	if ( pUnit->pData->opsize < pUnit->pData->size )
	{
		goto get;
	}
	//pUnit->pData->opsize = 0;
	LONGLONG llpos = pUnit->pData->position;
	putDataLink( pool->pFullLink , pUnit );

newinstance:
	pUnit = getDataLink( pool->pEmptyLink );
	pUnit->pData->opsize = 0;
	pUnit->pData->position += pool->unit_size;

get:
	return pUnit;
}

//如果 len <=0 说明已经没有数据
long WriteData( UNIT_BUF_POOL *pool , char *srcbuf , long len )
{	
	CFeLockMutex( pool->hGetQueueMutex );
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
		pool->pWrite->pData->opsize = 0;
		putDataLink( pool->pFullLink , pool->pWrite );
		pool->pWrite = NULL;
	}

	return total;
}

long ReadData( UNIT_BUF_POOL *pool , char *dstbuf , long len )
{
	CFeLockMutex( pool->hGetQueueMutex );
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
			pool->llPosition = pool->pRead->pData->position + pool->pRead->pData->opsize; //设置当前内存池总的偏移量
		}else{
			pool->pRead->pData->eof = 0;			
			pool->pRead->pData->size = pool->unit_size;
			pool->pRead->pData->opsize = 0;
			putDataLink( pool->pEmptyLink , pool->pRead );
			pool->pRead = NULL;
			break;
		}

	}
	return total;
}