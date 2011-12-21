#pragma once

#include <libmms/mmsx.h>
#include <libmms/mmsio.h>
#include <libmms/mms.h>
#include <libmms/mmsh.h>
#include <libmms/mmsx.h>

#define  RECV_BUF_SIZE 8192

struct UNIT_BUF_POOL;
class CWrapmms
{
public:
	CWrapmms(UNIT_BUF_POOL*);
	virtual ~CWrapmms(void);

	void openfile( const char* lpUrl );
	static unsigned CALLBACK recvdata( void *arge );
	double getTotalSec(){ return m_totalsec; }
	uint64_t getTotalRaw(){ return m_filelen; }

protected:
	mms_io_t* m_io_t;
	mms_t* m_mms_t;
	char* m_playbuf;
	char m_url[256];
	UNIT_BUF_POOL *m_pBufpool;
	double m_totalsec;
	uint64_t m_timelen; //时间长度换算成100ns单位计量
	uint64_t m_filelen; //文件长度
};

