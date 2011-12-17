#ifndef _OBJPOOL_H_
#define _OBJPOOL_H_
#include "feUtil.h"
/**
对像池
*/

//////////////////////////////////////////////////////////////////////////
//对像
/*
template<class T>
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
	//CObj<T> *m_pNext;
};

template<class T>
T CObj<T>::GetData(){
	return m_Data;
}
*/


//////////////////////////////////////////////////////////////////////////
//对像队列
template<class T>
class CObjQueue{
public:
	CObjQueue( int size ){
		m_cbsize = size;
		Init( m_cbsize );
	}

	~CObjQueue(){
		Clean();
	}

	void Init( int size ){
		m_pObjQueue = new T[size];
	}

	void Clean(){
		delete []m_pObjQueue;
		m_pObjQueue = NULL;
	}

	T *GetQueue(){
		return m_pObjQueue;
	}

protected:
	T *m_pObjQueue;
	int m_cbsize; //有多少对像
};


//////////////////////////////////////////////////////////////////////////
//对像池
template<class T>
class CObjPool
{
public:
	CObjPool( int units = 10 ){
		m_units = units;
		Init(m_units);
	}
	~CObjPool(){
		Clean();
	}
	int Init( int units ){
		m_pObjCollect = new CObjQueue<T>[];
		return 0;
	}
	int Clean(){
		delete []m_pObjCollect;
		m_pObjCollect = NULL;
		return 0;
	}
	virtual int WriteData() = 0;
	virtual int ReadData() = 0;

protected:
	CObjQueue<T> *m_pObjCollect;
	int m_units;
};

//////////////////////////////////////////////////////////////////////////
//示例
class CExample : public CObjPool<char>
{
public:
	CExample(){

	}
	~CExample(){

	}

	virtual int WriteData(){
		return 0;
	}

	virtual int ReadData()
	{
		return 0;
	}
};

#endif //_OBJPOOL_H_