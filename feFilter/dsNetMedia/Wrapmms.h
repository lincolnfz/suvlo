#pragma once

#include <libmms/mmsx.h>
#include <libmms/mmsio.h>
#include <libmms/mms.h>
#include <libmms/mmsh.h>
#include <libmms/mmsx.h>
#include "WrapFFMpeg.h"

#define  RECV_BUF_SIZE 8192
class CWrapmms
{
public:
	CWrapmms(void);
	~CWrapmms(void);

	void play( const char* lpUrl );
	void nothing();
	static unsigned CALLBACK recvdata( void *arge );

protected:
	mms_io_t* m_io_t;
	mms_t* m_mms_t;
	char* m_playbuf;
	char m_url[256];
	CWrapFFMpeg m_ffmpeg;

protected:
	
};

