#ifndef _OBJPOOL_H_
#define _OBJPOOL_H_
#include "feUtil.h"
/**
�����
*/

//////////////////////////////////////////////////////////////////////////
//����
template<class T>
class CObj
{
public:
	CObj( ){
	}
	virtual ~CObj(){}

	T& GetData();

protected:
	T m_Data;
};

template<class T>
T& CObj<T>::GetData(){
	return m_Data;
}


//////////////////////////////////////////////////////////////////////////
//object queue
template<class T>
class CObjQueue{
public:
	CObjQueue(){
		m_pObjQueue = NULL;
	}

	virtual ~CObjQueue(){
		Destory();
	}

	void Init( long size ){
		m_cbsize = size;
		m_cursor = 0;
		m_eof = 0;
		m_pObjQueue = new CObj<T>[size]();
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

	/*CObj<T> *GetQueue(){
		return m_pObjQueue;
	}*/

	long Getcbsize(){ return m_cbsize; }
	long Getcursor(){ return m_cursor; }
	long IncCursor( long l ){ m_cursor += l; return m_cursor; }
	void Resetcursor(){ m_cursor = 0; }
	int IsEof(){return m_eof; }
	void SetPosition( LONGLONG ll ){ m_position = ll; }
	LONGLONG GetPosition(){ return m_position; }
	T *GetData(){
	   return &(m_pObjQueue[m_cursor].GetData() );
    }
	long CommitData(){
		return IncCursor(1);
	}

protected:
	CObj<T> *m_pObjQueue;
	long m_cbsize; //�ж��ٶ���ռ�
	long m_cursor; //��ǰ���α�,��ģʽ��ʾ�Ѷ�����λ��,дģʽ��ʾ��д����λ��
	int m_eof;
	LONGLONG m_position;
};


//////////////////////////////////////////////////////////////////////////
//object pool
#define GETCLASSFUN(C,F) C##::*##F()

template<class T>
class CObjPool
{
public:
	enum OPCMD {WRITE_DATA, READ_DATA};

	CObjPool( int units , long size){
		m_pObjCollect = NULL;
		m_units = units;
		m_size = size;
		m_pEmptyList = NULL;
		m_pFullList = NULL;
		Init( m_units , m_size );
	}
	virtual ~CObjPool(){
		Destory();
	}

protected:
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
		if ( pNode->pData->IsEof() )
		{
			return pNode;
		}
		if ( pNode->pData->Getcursor() < pNode->pData->Getcbsize() )
		{
			return pNode;
		}
		pNode->pData->Resetcursor();
		putDataLink( m_pEmptyList , pNode ); //���ݶ���,�鷵���ն���
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
		putDataLink( m_pFullList , pNode ); //����д��,�黹����ɶ���
getnew:
		pNode = getDataLink( m_pEmptyList );
		pNode->pData->SetPosition( ll + m_size );
		return pNode;
	}

	//������ʵ����,������������ȡ���ݵķ���
	/*
	virtual int WriteData() = 0;
	virtual int ReadData() = 0;
	*/
	
public:
	//get one object from pool
	//
	virtual T * GetOneUnit( OPCMD op ){
		T *pData = NULL;
		if ( op == READ_DATA )
		{
			//Ҫһ�����Զ���unit
			m_pRead = this->GetReadQueue( this->m_pRead );
			pData = m_pRead->pData->GetData();
			
		}else if ( op == WRITE_DATA )
		{
			//Ҫһ������д��unit
			m_pWrite = this->GetWriteQueue( this->m_pWrite );
			pData = m_pWrite->pData->GetData();
		}
		return pData;
	}

	//����ύ�Ѳ�����Ķ���ʵ��ֻ���α�+1
	//
	virtual int CommitOneUnit( T *pdata , OPCMD op ){
		if ( op == READ_DATA )
		{			
			this->m_pRead->pData->CommitData();
		}else if ( op == WRITE_DATA )
		{			
			this->m_pWrite->pData->CommitData();
		}
		return 0;
	}

	virtual int Flush()
	{
		return 0;
	}

	

protected:
	CObjQueue<T> *m_pObjCollect;
	DataLink<CObjQueue<T>*> *m_pEmptyList , *m_pFullList;
	DataNode<CObjQueue<T>*> *m_pWrite , *m_pRead;
	int m_units;
	long m_size;
};

template< class T>
void PutDataPool( CObjPool<T> *pool , T* data )
{
	T *org = pool->GetOneUnit( CObjPool<T>::OPCMD::WRITE_DATA );
	*org = *data;
	pool->CommitOneUnit( org , CObjPool<T>::OPCMD::WRITE_DATA );
}

/**************
/*    ���ﲻдGetPoolData����                                                                 
***********************/

//////////////////////////////////////////////////////////////////////////
//ʾ��
class CExample : public CObjPool<char>
{
public:
	CExample():CObjPool<char>(10,100)
	{

	}
	virtual ~CExample(){

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