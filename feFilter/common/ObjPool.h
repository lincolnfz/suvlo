#ifndef _OBJPOOL_H_
#define _OBJPOOL_H_
#include "feUtil.h"
/**
对像池
*/

//////////////////////////////////////////////////////////////////////////
//对像
/*
template<class T,class N>
class CObj
{
public:
	CObj( T data ){
		m_Data = data;
	}
	~CObj(){}

	T GetData();

protected:
	T m_Data;
	N m_Da1;
	//CObj<T> *m_pNext;
};

template<class T , class N>
T CObj<T,N>::GetData(){
	return m_Data;
}
*/


//////////////////////////////////////////////////////////////////////////
//对像队列
template<class T>
class CObjQueue{
public:
	CObjQueue(){
		m_pObjQueue = NULL;
	}

	~CObjQueue(){
		Destory();
	}

	void Init( long size ){
		m_cbsize = size;
		m_cursor = 0;
		m_eof = 0;
		m_pObjQueue = new T[size]();
	}

	void Reset(){
		m_cbsize = size;
		m_cursor = 0;
		m_eof = 0;
	}

	void Destory(){
		if ( m_pObjQueue )
		{
			delete []m_pObjQueue;
		}	
		m_pObjQueue = NULL;
		m_cursor = 0;
		m_cbsize = 0;
	}

	T *GetQueue(){
		return m_pObjQueue;
	}

	long Getcbsize(){ return m_cbsize; }
	long Getcursor(){ return m_cursor; }
	void Resetcursor(){ m_cursor = 0; }
	int GetEof(){return m_eof; }
	void SetPosition( LONGLONG ll ){ m_position = ll; }
	LONGLONG GetPosition(){ return m_position; }

protected:
	T *m_pObjQueue;
	long m_cbsize; //有多少对像空间
	long m_cursor; //当前的游标,读模式表示已读到的位置,写模式表示已写到的位置
	int m_eof;
	LONGLONG m_position;
};


//////////////////////////////////////////////////////////////////////////
//对像池
#define GETCLASSFUN(C , F) C::*F

template<class T>
class CObjPool
{
public:
	CObjPool( int units , long size){
		m_pObjCollect = NULL;
		m_units = units;
		m_size = size;
		m_pEmptyList = NULL;
		m_pFullList = NULL;
		Init( m_units , m_size );
	}
	~CObjPool(){
		Destory();
	}
	int Init( int units , long size ){
		m_units = units;
		m_size = size;
		m_pWrite = NULL;
		m_pRead = NULL;
		if ( !m_pEmptyList )
		{
			initDataLink( &m_pEmptyList );
		}
		if ( !m_pFullList )
		{
			initDataLink( &m_pFullList );
		}
		m_pObjCollect = new CObjQueue<T>[units]();
		int idx = 0;
		for ( idx = 0 ; idx < units ; ++idx )
		{
			m_pObjCollect[idx].Init(size);
			DataNode<CObjQueue<T>*> *pNode = new DataNode<CObjQueue<T>*>();
			pNode->pData = &m_pObjCollect[idx];
			putDataLink( m_pEmptyList , pNode );
		}

		return 0;
	}

	int Destory(){
		if ( m_pEmptyList )
		{
			destoryDataLink( &m_pEmptyList );
			m_pEmptyList = NULL;
		}
		if ( m_pFullList )
		{
			destoryDataLink( &m_pFullList );
			m_pFullList = NULL;
		}
		delete []m_pObjCollect;
		m_pObjCollect = NULL;
		m_pWrite = NULL;
		m_pRead = NULL;
		return 0;
	}

	DataNode<CObjQueue<T>*> *GetReadQueue( DataNode<CObjQueue<T>*>* pStale )
	{
		DataNode<CObjQueue<T>*>* pNode = pStale;
		if ( !pNode )
		{
			goto getnew;
		}
		if ( pNode->pData->GetEof() )
		{
			return pNode;
		}
		if ( pNode->pData->Getcursor() < pNode->pData->Getcbsize() )
		{
			return pNode;
		}
		pNode->pData->Resetcursor();
		putDataLink( m_pEmptyList , pNode );
getnew:
		pNode = getDataLink( m_pFullList );
		return pNode;
	}

	DataNode<CObjQueue<T>*> *GetWriteQueue( DataNode<CObjQueue<T>*>* pStale )
	{
		DataNode<CObjQueue<T>*>* pNode = pStale;
		if ( !pNode )
		{
			goto getnew;
		}
		if ( pNode->pData->Getcursor() < pNode->pData->Getcbsize() )
		{
			return pNode;
		}
		pNode->pData->Resetcursor();
		LONGLONG ll = pNode->pData->GetPosition();
		putDataLink( m_pFullList , pNode );
getnew:
		pNode = getDataLink( m_pEmptyList );
		pNode->pData->SetPosition( ll + m_size );
		return pNode;
	}

	//在派生实现类,必须有填充与获取数据的方法
	/*
	virtual int WriteData() = 0;
	virtual int ReadData() = 0;
	*/
	

	virtual int RequestData(){

		return 0;
	}

	virtual int FillData(){

		return 0;
	}

protected:
	CObjQueue<T> *m_pObjCollect;
	DataLink<CObjQueue<T>*> *m_pEmptyList , *m_pFullList;
	DataNode<CObjQueue<T>*> *m_pWrite , *m_pRead;
	int m_units;
	long m_size;
};

//////////////////////////////////////////////////////////////////////////
//示例
class CExample : public CObjPool<char>
{
public:
	CExample():CObjPool<char>(10,100)
	{

	}
	~CExample(){

	}

	/*virtual int WriteData( char parm ){
		return 0;
	}

	virtual int ReadData()
	{
		return 0;
	}*/
};

#endif //_OBJPOOL_H_