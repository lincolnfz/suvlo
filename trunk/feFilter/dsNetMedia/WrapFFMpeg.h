#pragma once
#include <Windows.h>
#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include "common.h"

//#define BUF_SIZE 32768
#define MEM_BUF_SIZE 32768
#define CYCLE_BUF_SIZE 131072

typedef struct _tagVideoState VideoState;

class CWrapFFMpeg
{
protected:
	void Init();

	/**
	�������״̬
	*/
	void clean();

	/**
	���㻹�ж��ٿռ����ʹ��
	**/
	int calc_remain_size();

	/*
	�����ж��ٵ���Ч���ݿռ�
	*/
	int calc_valid_size();

	/*
	��ջ���
	*/
	void flush_buf();

	/**
	�յ�һ���µĲ����ļ���
	**/
	void notify_new_launch( );

	AVFormatContext* m_pFormatCtx;
	char* m_cycle_head; //��״�ڴ��ָ��
	char* m_cycle_end; //����λ��,���ܴ������

	//��Ч�ռ�����ݲ��ܱ�����
	char* m_valid_head; //��Ч�ռ�Ŀ�ʼλ��
	char* m_valid_end; //��Ч�ռ�Ľ���λ��,���ܴ������

	HANDLE m_hScrapMemEvent;//��Ϊ��ȡ���ݻ���,��������ݿռ��ʶΪ��Ч
	HANDLE m_hFillMemEvent;//�������,�������ź�
	HANDLE m_hOpMemLock; //�����ڴ�ʱ�ӵ���

	CRITICAL_SECTION m_calcSection; //����ռ䱣�������

	int m_eof; //0�������� 1û��������

	VideoState* m_pVideoState;

public:
	CWrapFFMpeg(void);
	~CWrapFFMpeg(void);
	
	char* getCycleBuf(){ return m_cycle_head; }
	VideoState* getVideoState(){ return m_pVideoState; }

	/**
	*����ڴ�
	**/
	void Fillbuf( char* srcbuf , int len , int eof );

	/**
	*��ȡ�ڴ�
	**/
	int ReadBuf( char* destbuf , int len );


	void DeCodec( char* , int );
	
	
};
