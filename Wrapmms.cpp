#include "StdAfx.h"
#include "Wrapmms.h"
#pragma comment( lib , "lib/libmms.lib" )

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


CWrapmms::CWrapmms(void)
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

void CWrapmms::play( const char* url )
{
	//HANDLE hOpenFile = CreateFile( _T("c:\\aa.wmv") , GENERIC_WRITE , 0 , 0 , CREATE_ALWAYS , OPEN_ALWAYS , 0 );
	m_mms_t = mms_connect( 0 , 0 , url , 1 );

	int nlen = 0;
	DWORD dwrite;	
	int eof = 0;
	if ( m_mms_t )
	{
		char* buf = (char*)malloc(RECV_BUF_SIZE);
		while(  true )
		{			
			if( (nlen = mms_read( 0 , m_mms_t , buf , RECV_BUF_SIZE ) ) <= 0 )
			{			
				break;
			}
			//处理收到的数据
			eof = nlen < RECV_BUF_SIZE ? 1 : 0;
			m_ffmpeg.Fillbuf( buf , nlen , eof );
			
			//WriteFile( hOpenFile , buf , nlen , &dwrite , NULL );
			//SetFilePointer(hOpenFile,0,NULL,FILE_END);
		}
		free(buf);
	}
	//CloseHandle( hOpenFile );
}

void CWrapmms::nothing()
{

}