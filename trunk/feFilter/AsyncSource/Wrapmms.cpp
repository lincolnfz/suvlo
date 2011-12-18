//#include "StdAfx.h"
#include <Windows.h>
#include <process.h>
#include <streams.h>
#include "Wrapmms.h"
#include "asyncio.h"
#include "FeBufPool.h"


#pragma comment( lib , "../../lib/libmms.lib" )

#define CLOSE_SOCKET(SOCK)\
	if( INVALID_SOCKET != SOCK )\
	{\
		closesocket( SOCK );\
	}\

SOCKET client = INVALID_SOCKET ;

mms_off_t mms_io_write(void *data, int socket, char *buf, mms_off_t num)
{
	mms_off_t len = send( socket , buf , num , 0 );
	return len;
}

mms_off_t mms_io_read(void *data, int socket, char *buf, mms_off_t num)
{
	mms_off_t len = 0, ret;
	while (len < num)
	{
		ret = (off_t)recv(socket, buf + len, num - len, 0);
		if(ret == 0)
			break; /* EOF */
		if(ret < 0) {
			/*lprintf("read error @ len = %lld: %s\n", (long long int) len,
				strerror(get_errno()));*/
			DWORD dwErrno = WSAGetLastError();
			switch( dwErrno )
			{
			case WSAEWOULDBLOCK:
				continue;
			default:
				/* if already read something, return it, we will fail next time */
				return len ? len : ret; 
			}
		}
		len += ret;
	}
	return len;
}

int mms_io_select(void *data, int fd, int state, int timeout_msec)
{
	int i = 0;
	return 0;
}

int mms_io_tcp_connect(void *data, const char *host, int port)
{
	CLOSE_SOCKET(client);
	client = socket( AF_INET , SOCK_STREAM , 0 );
	struct sockaddr_in addr;
	memset( &addr , 0 , sizeof(sockaddr_in) );
	addr.sin_family = AF_INET;
	addr.sin_port = ntohs(port);
	addr.sin_addr.S_un.S_addr = inet_addr(host);	
	int ret = connect( client , (struct sockaddr*)&addr , sizeof(sockaddr) );
	DWORD err = WSAGetLastError();
 	return client;
}


CWrapmms::CWrapmms(UNIT_BUF_POOL *pool )
{
	m_io_t = (mms_io_t*)malloc( sizeof( mms_io_t ) );
	m_io_t->connect = mms_io_tcp_connect;
	m_io_t->read = mms_io_read;
	m_io_t->select = mms_io_select;
	m_io_t->write = mms_io_write;
	m_io_t->connect_data = (void*)malloc( 4096 );
	m_io_t->read_data = (void*)malloc(4096);
	m_io_t->select_data = (void*)malloc(4096);
	m_io_t->write_data = (void*)malloc(4096);
	m_playbuf = (char*)malloc( RECV_BUF_SIZE );
	m_pBufpool = pool;
	m_totalsec = 0;
	m_totalraw = 0;
}


CWrapmms::~CWrapmms(void)
{
	CLOSE_SOCKET(client);
	free( m_io_t->connect_data );
	free( m_io_t->read_data );
	free( m_io_t->select_data );
	free( m_io_t->write_data );
	free( m_io_t );
	free( m_playbuf );
}

unsigned CALLBACK CWrapmms::recvdata( void *arge )
{
	CWrapmms* pMMS = (CWrapmms*)arge;
	pMMS->m_mms_t = mms_connect( 0 , 0 , pMMS->m_url , 1 );

	int nlen = 0;
	DWORD dwrite;	
	int eof = 0;
	if ( pMMS->m_mms_t )
	{
		pMMS->m_totalsec = mms_get_time_length( pMMS->m_mms_t );
		pMMS->m_totalraw = mms_get_raw_time_length( pMMS->m_mms_t );
		pMMS->m_pBufpool->sec = pMMS->m_totalsec;
		pMMS->m_pBufpool->llRaw = pMMS->m_totalraw;
		char* buf = (char*)malloc(RECV_BUF_SIZE);
		while(  true )
		{		
			/*
			if( (nlen = mms_read( 0 , pMMS->m_mms_t , buf , RECV_BUF_SIZE ) ) <= 0 )
			{			
				break;
			}
			//处理收到的数据
			eof = nlen < RECV_BUF_SIZE ? 1 : 0;
			pMMS->m_ffmpeg.Fillbuf( buf , nlen , eof );
			*/

			nlen = mms_read( 0 , pMMS->m_mms_t , buf , RECV_BUF_SIZE );
			WriteData( pMMS->m_pBufpool , buf , nlen );
			if ( nlen <= 0 )
			{
				break;
			}

		}
		free(buf);
	}
	//CloseHandle( hOpenFile );
	return 0;
}

void CWrapmms::openfile( const char* url )
{
	//HANDLE hOpenFile = CreateFile( _T("c:\\aa.wmv") , GENERIC_WRITE , 0 , 0 , CREATE_ALWAYS , OPEN_ALWAYS , 0 );
	strcpy( m_url , url );
	_beginthreadex( NULL , 0 , recvdata , this , 0 , 0 );
}