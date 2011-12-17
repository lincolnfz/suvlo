#pragma once
#include "../common/feUtil.h"
#include "../common/ObjPool.h"

struct UNIT_BUF 
{
	long size;		//缓存单元的大小
	long opsize; //已操作了多少数据,初始0,在读取下表示已读了多少数据,写模式下表示写了多少数据
	char *pHead;
};

struct UNIT_BUF_POOL
{
	int units;
	UNIT_BUF *pList; //pList主要的队列 pEmpty空的对列 pFull填满的队列
	DataLink<UNIT_BUF*> *pEmptyLink , *pFullLink;
	DataNode<UNIT_BUF*> *pWrite , *pRead; //当前处在活动状态的填充单元与读取单元
};

	/************************************************************************/
	/* 初始缓冲池                                                            */
	/************************************************************************/
	void InitPool( UNIT_BUF_POOL* , int , long );

	/**
	清理缓冲池
	*/
	void CleanPool( UNIT_BUF_POOL* );

	DataNode<UNIT_BUF*> *GetReadUnit(UNIT_BUF_POOL *pool , DataNode<UNIT_BUF*> *pStale );

	DataNode<UNIT_BUF*> *GetWriteUnit(UNIT_BUF_POOL *pool , DataNode<UNIT_BUF*> *pStale );

	/*
	srcbuf数据写进池
	*/
	long WriteData( UNIT_BUF_POOL *pool , char *srcbuf , long len );

	/*
	池的数据写出dstbuf
	*/
	long ReadData( UNIT_BUF_POOL *pool , char *dstbuf , long len );

class CFeBufPool : public CAsyncStream
{
public:
	CFeBufPool( int units = 10 , long size = 65536 );
	~CFeBufPool(void);
	virtual HRESULT SetPointer(LONGLONG llPos) ;
	virtual HRESULT Read(PBYTE pbBuffer, DWORD dwBytesToRead, BOOL bAlign, LPDWORD pdwBytesRead);
	virtual LONGLONG Size(LONGLONG *pSizeAvailable = NULL);
	virtual DWORD Alignment();
	virtual void Lock();
	virtual void Unlock();

protected:
	UNIT_BUF_POOL m_poolbuf;
	CExample example;
};

