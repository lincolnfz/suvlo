#pragma once
#include <Windows.h>
#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include "common.h"

extern "C"
{
#include "libavutil/avstring.h"
	//#include "libavutil/colorspace.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavutil/avutil.h"	
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
	//#include "libavcodec/audioconvert.h"
#include "libavutil/opt.h"
#include "libavcodec/avfft.h"

# include "libavfilter/avcodec.h"
# include "libavfilter/avfilter.h"
# include "libavfilter/avfiltergraph.h"
# include "libavfilter/buffersink.h"
}

#define BUF_SIZE 32768
#define MEM_BUF_SIZE 32768
#define CYCLE_BUF_SIZE 131072

typedef struct VideoState VideoState;

class CWrapFFMpeg
{
protected:
	void Init();

	/**
	清除播放状态
	*/
	void clean();

	/**
	计算还有多少空间可以使用
	**/
	int calc_remain_size();

	/*
	计算有多少的有效数据空间
	*/
	int calc_valid_size();

	/*
	清空缓存
	*/
	void flush_buf();

	/**
	收到一个新的播放文件流
	**/
	void notify_new_launch( );

	AVFormatContext* m_pFormatCtx;
	char* m_cycle_head; //环状内存的指针
	char* m_cycle_end; //结束位置,不能存放数据

	//有效空间的数据不能被覆盖
	char* m_valid_head; //有效空间的开始位置
	char* m_valid_end; //有效空间的结束位置,不能存放数据

	HANDLE m_hScrapMemEvent;//因为读取数据缓存,而造成数据空间标识为无效
	HANDLE m_hFillMemEvent;//填充数据,引发的信号
	HANDLE m_hOpMemLock; //操作内存时加的锁

	CRITICAL_SECTION m_calcSection; //计算空间保护代码段

	int m_eof; //0还有数据 1没有数据了

	VideoState* m_pVideoState;

public:
	CWrapFFMpeg(void);
	~CWrapFFMpeg(void);
	
	char* getCycleBuf(){ return m_cycle_head; }
	VideoState* getVideoState(){ return m_pVideoState; }

	/**
	*填充内存
	**/
	void Fillbuf( char* srcbuf , int len , int eof );

	/**
	*读取内存
	**/
	int ReadBuf( char* destbuf , int len );


	void DeCodec( char* , int );
	
	
};

