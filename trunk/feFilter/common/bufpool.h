#ifndef _BUFPOOL_H_
#define _BUFPOOL_H_
#include "./feUtil.h"

struct UNIT_BUF 
{
	long size;		//���浥Ԫ�Ĵ�С
	long opsize; //�Ѳ����˶�������,��ʼ0,�ڶ�ȡ�±�ʾ�Ѷ��˶�������,дģʽ�±�ʾд�˶�������
	int eof; //���ǽ����ĵ�Ԫ���� 1����
	LONGLONG position; //��ǰ���л�ַ
	char *pHead;
	char *pCur;
	int idx;
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
	double sec;
	int eof;
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

#endif //_BUFPOOL_H_