#include "common.h"

int initAVFrameLink( AVFrameLink **avframelink )
{
	*avframelink = (AVFrameLink*)malloc( sizeof(AVFrameLink) );
	(*avframelink)->hMutex = CreateMutex( NULL , FALSE , NULL );
	(*avframelink)->hEvent = CreateEvent( NULL , FALSE , FALSE , NULL );
	InitializeCriticalSection( &((*avframelink)->cs) );
	(*avframelink)->nodehead = NULL;
	(*avframelink)->nodelast = NULL;
	(*avframelink)->nb_size = 0;	
	return 0;
}

 int putAVFrameLink( AVFrameLink *avframelink , AVFrameNode *newNode)
{
	CFeLockMutex( avframelink->hMutex );
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

AVFrameNode* getAVFrameLink( AVFrameLink *avframelink , int block )
{
	//AVFrameNode* pNode = NULL;
	AVFrameNode* pAVFrameNode = NULL;
	WaitForSingleObject( avframelink->hMutex , INFINITE );
	for (;;)
	{
		if ( avframelink->nodehead )
		{
			pAVFrameNode = avframelink->nodehead;
			//pNode = pAVFrameNode->avframe;
			avframelink->nodehead = avframelink->nodehead->next;
			--(avframelink->nb_size);
			if ( 0 == avframelink->nb_size )
			{
				avframelink->nodelast = NULL;
			}
			//free( pAVFrameNode );
			break;
		}else if ( !block )
		{
			break;
		}else{
			ReleaseMutex(avframelink->hMutex);
			WaitForSingleObject(avframelink->hEvent , INFINITE);
			WaitForSingleObject(avframelink->hMutex , INFINITE);
		}
	}
	ReleaseMutex( avframelink->hMutex );
	return pAVFrameNode;
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
	DeleteCriticalSection( &((*avframelink)->cs) );
	free(*avframelink);
	*avframelink = NULL;	
	return 0;
}

long generateRGBDate( BYTE *pDst , SwsContext **ctx , AVFrame *pFrame , 
	int widthSrc , int heightSrc , PixelFormat pixFmtSrc , 
	int widthDst , int heightDst , PixelFormat pixFmtDst )
{
	long lsize = 0;
	*ctx = sws_getCachedContext( *ctx ,
		widthSrc ,
		heightSrc ,
		pixFmtSrc , 
		widthDst ,
		heightDst ,
		pixFmtDst ,  SWS_BICUBIC , NULL , NULL , NULL );
	if ( *ctx )
	{
		 AVPicture pict;
		int ret = avpicture_alloc( &pict , pixFmtDst , 
				widthDst , 
				heightDst);
		//µ¹ÖÃµßµ¹µÄÍ¼Ïñ
		/*
		pAVFrame->data[0] = pAVFrame->data[0]+pAVFrame->linesize[0]*( g_pVideoState->video_st->codec->height-1 );
		pAVFrame->data[1] = pAVFrame->data[1]+pAVFrame->linesize[0]*g_pVideoState->video_st->codec->height/4-1;  
		pAVFrame->data[2] = pAVFrame->data[2]+pAVFrame->linesize[0]*g_pVideoState->video_st->codec->height/4-1; 
		pAVFrame->linesize[0] *= -1;
		pAVFrame->linesize[1] *= -1;  
		pAVFrame->linesize[2] *= -1;
		*/
		//µ¹ÖÃµßµ¹µÄÍ¼Ïñ ok
				
		//×ª»»³Érgb
		if ( !ret && sws_scale( *ctx , pFrame->data , pFrame->linesize , 0 , heightSrc , pict.data , pict.linesize ) )
		{
			lsize = pict.linesize[0]*heightSrc;
			CopyMemory(pDst , pict.data[0] , lsize );
			pDst += lsize;
			if ( pict.linesize[1] )
			{
				int tmplen = pict.linesize[1]*heightSrc;
				CopyMemory(pDst , pict.data[1] , tmplen );
				pDst += tmplen;
				lsize += tmplen;
			}
			if ( pict.linesize[2] )
			{
				int tmplen = pict.linesize[2]*heightSrc;
				CopyMemory(pDst , pict.data[2] , tmplen );
				pDst += tmplen;
				lsize += tmplen;
			}

			avpicture_free( &pict );
		}
				
	}
	return lsize;
}