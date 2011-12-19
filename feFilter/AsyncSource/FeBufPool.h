#pragma once
#include "../common/feUtil.h"


struct UNIT_BUF 
{
	long size;		//���浥Ԫ�Ĵ�С
	long opsize; //�Ѳ����˶�������,��ʼ0,�ڶ�ȡ�±�ʾ�Ѷ��˶�������,дģʽ�±�ʾд�˶�������
	int eof; //���ǽ����ĵ�Ԫ���� 1����
	LONGLONG position; //��ǰ���л�ַ
	char *pHead;
};

struct UNIT_BUF_POOL
{
	int units;
	long unit_size;
	UNIT_BUF *pList; //pList��Ҫ�Ķ��� pEmpty�յĶ��� pFull�����Ķ���
	DataLink<UNIT_BUF*> *pEmptyLink , *pFullLink;
	DataNode<UNIT_BUF*> *pWrite , *pRead; //��ǰ���ڻ״̬����䵥Ԫ���ȡ��Ԫ
	LONGLONG llRaw; //ȫ������
	LONGLONG llPosition; //��ǰ��λ��
	LONGLONG llSize; //���ϵ�ȫ����С
	double sec;
};

	/************************************************************************/
	/* ��ʼ�����                                                            */
	/************************************************************************/
	void InitPool( UNIT_BUF_POOL* , int , long );

	/**
	�������
	*/
	void DestoryPool( UNIT_BUF_POOL* );

	void ResetPool( UNIT_BUF_POOL* );

	DataNode<UNIT_BUF*> *GetReadUnit(UNIT_BUF_POOL *pool , DataNode<UNIT_BUF*> *pStale );

	DataNode<UNIT_BUF*> *GetWriteUnit(UNIT_BUF_POOL *pool , DataNode<UNIT_BUF*> *pStale );

	/*
	srcbuf����д����
	*/
	long WriteData( UNIT_BUF_POOL *pool , char *srcbuf , long len );

	/*
	�ص�����д��dstbuf
	*/
	long ReadData( UNIT_BUF_POOL *pool , char *dstbuf , long len );

class CBufPoolOper{
	virtual long filldata( char* , long len ) = 0;
};

//////////////////////////////////////////////////////////////////////////
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

	//��չ����
	UNIT_BUF_POOL *getPool(){ return &m_poolbuf; }

protected:
	UNIT_BUF_POOL m_poolbuf;
	CCritSec       m_csLock;
};

