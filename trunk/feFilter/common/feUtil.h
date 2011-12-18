#ifndef _FEUTIL_H_
#define _FEUTIL_H_
#include <Windows.h>
class CFeLockMutex{
public:
	CFeLockMutex( void*& mutex ):m_Mutex(mutex){
		WaitForSingleObject( m_Mutex , INFINITE );
	}
	~CFeLockMutex(){
		ReleaseMutex( m_Mutex );
	}
private:
	void*& m_Mutex;
};

class CFeLockCriSec{
public:
	CFeLockCriSec( CRITICAL_SECTION& cs ):m_cs(cs){
		EnterCriticalSection(&m_cs);
	}
	~CFeLockCriSec(){
		LeaveCriticalSection(&m_cs);
	}

private:
	CRITICAL_SECTION &m_cs;
};

template<class T>
struct DataNode
{
	T pData;
	DataNode *pNext;
};

template<class T>
struct DataLink 
{
	DataNode<T> *pHead , *pTail;
	void *hMutex; //操作整个队列的鑜
	void *hEvent;
	int nb_size;
};

template<class T>
int initDataLink( DataLink<T> **ppDataLink )
{
	*ppDataLink = new DataLink<T>();//(DataLink<T>*)malloc( sizeof(DataLink<T>) );
	(*ppDataLink)->nb_size = 0;
	(*ppDataLink)->pHead = NULL;
	(*ppDataLink)->pTail = NULL;
	(*ppDataLink)->hEvent = CreateEvent( NULL , FALSE , FALSE , NULL );
	(*ppDataLink)->hMutex = CreateMutex( NULL , FALSE , NULL );
	return 0;
}

template<class T>
int putDataLink( DataLink<T> *pDataLink , DataNode<T> *pDatanode )
{
	if ( !pDatanode )
	{
		return 0;
	}
	pDatanode->pNext = NULL;
	CFeLockMutex( pDataLink->hMutex );
	if ( pDataLink->pHead == NULL )
	{
		pDataLink->pHead = pDatanode;
	}
	if ( pDataLink->pTail == NULL )
	{
		pDataLink->pTail = pDatanode;
	}
	else
	{
		pDataLink->pTail->pNext = pDatanode;
		pDataLink->pTail = pDatanode;
	}
	++(pDataLink->nb_size);
	SetEvent( pDataLink->hEvent );
	return 0;
}

template<class T>
DataNode<T> *getDataLink( DataLink<T> *pDataLink , int block = 1)
{
	DataNode<T> *pNode = NULL;
	WaitForSingleObject( pDataLink->hMutex , INFINITE );
	for ( ;; )
	{
		if( pDataLink->pHead )
		{
			pNode = pDataLink->pHead;
			pDataLink->pHead = pDataLink->pHead->pNext;
			--(pDataLink->nb_size);
			if ( pNode == pDataLink->pTail )
			{
				pDataLink->pTail = NULL;
			}
			break;
		}
		else if ( !block )
		{
			break;
		}
		else
		{
			ReleaseMutex( pDataLink->hMutex );
			WaitForSingleObject( pDataLink->hEvent , INFINITE );
			WaitForSingleObject( pDataLink->hMutex , INFINITE );
		}
	}
	ReleaseMutex( pDataLink->hMutex );
	return pNode;
}

template<class T>
int flushDataLink( DataLink<T> *pDataLink )
{
	CFeLockMutex( pDataLink->hMutex );
	DataNode<T> *pVisit = NULL;
	while( pDataLink->pHead )
	{
		pVisit = pDataLink->pHead;
		pDataLink->pHead = pDataLink->pHead->pNext;
		delete( pVisit );
		--(pDataLink->nb_size);
	}
	pDataLink->pTail = NULL;
	return 0;
}

template<class T>
int destoryDataLink( DataLink<T> **ppDataLink )
{
	flushDataLink(*ppDataLink);
	CloseHandle( (*ppDataLink)->hEvent );
	CloseHandle( (*ppDataLink)->hMutex );
	delete( *ppDataLink );
	*ppDataLink = NULL;
	return 0;
}

#endif //_FEUTIL_H_