#include "common.h"

int initAVFrameLink( AVFrameLink **avframelink )
{
	*avframelink = (AVFrameLink*)malloc( sizeof(AVFrameLink) );
	(*avframelink)->hMutex = CreateMutex( NULL , FALSE , NULL );
	(*avframelink)->hEvent = CreateEvent( NULL , TRUE , FALSE , NULL );
	(*avframelink)->nodehead = NULL;
	(*avframelink)->nodelast = NULL;
	(*avframelink)->nb_size = 0;
	return 0;
}

 int putAVFrameLink( AVFrameLink *avframelink , AVFrame* avframe)
{
	CFeLockMutex( avframelink->hMutex );
	AVFrameNode *newNode = (AVFrameNode*)malloc( sizeof(AVFrameNode) ) ;
	newNode->avframe = avframe;
	newNode->next = NULL;
	if ( avframelink->nodehead == NULL )
	{
		avframelink->nodehead = newNode;
	}
	if ( avframelink->nodelast == NULL )
	{
		avframelink->nodelast = newNode;
	}
	else
	{
		avframelink->nodelast->next = newNode;
		avframelink->nodelast = newNode;
	}
	++(avframelink->nb_size);
	SetEvent( avframelink->hEvent );
	return 0;
}

AVFrame* getAVFrameLink( AVFrameLink *avframelink )
{
	WaitForSingleObject( avframelink->hEvent , INFINITE );
	CFeLockMutex( avframelink->hMutex );
	AVFrame* pNode = NULL;
	AVFrameNode* pAVFrameNode = NULL;
	if ( avframelink->nodehead )
	{
		pAVFrameNode = avframelink->nodehead;
		pNode = pAVFrameNode->avframe;
		avframelink->nodehead = avframelink->nodehead->next;
		--(avframelink->nb_size);
		if ( 0 == avframelink->nb_size )
		{
			avframelink->nodelast = NULL;
		}
		free( pAVFrameNode );
	}
	else
	{
		ResetEvent( avframelink->hEvent );
	}
	return pNode;
}

int flushAVFrameLink( AVFrameLink *avframelink )
{
	CFeLockMutex( avframelink->hMutex );
	AVFrameNode* pAVFrameNode = NULL;
	while ( avframelink->nodehead )
	{
		pAVFrameNode = avframelink->nodehead;
		avframelink->nodehead = avframelink->nodehead->next;
		free( pAVFrameNode );
		--(avframelink->nb_size);
	}
	avframelink->nodelast = NULL;
	return 0;
}

int destoryAVFrameLink( AVFrameLink **avframelink )
{
	flushAVFrameLink( *avframelink );
	CloseHandle( (*avframelink)->hMutex );
	CloseHandle( (*avframelink)->hEvent );
	free(*avframelink);
	*avframelink = NULL;
	return 0;
}