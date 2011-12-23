#ifndef _FEFFMPEG_H_
#define _FEFFMPEG_H_

#include <Windows.h>
#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include "../common/bufpool.h"
#include "../common/ObjPool.h"

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

class CFeFFmpeg
{
public:
	static CFeFFmpeg* GetInstance( UNIT_BUF_POOL* , CObjPool<AVPicture>* );
	static int Destory();

	virtual ~CFeFFmpeg();
	int ReadBuf(char* destbuf , int len);
	int Start();
	static unsigned int __stdcall ReadThread( void *avg );
	

protected:
	CFeFFmpeg( UNIT_BUF_POOL* , CObjPool<AVPicture>* );

	int ImgCover( SwsContext **ctx , AVFrame *pFrame , AVPicture* pict, 
		int widthSrc , int heightSrc , PixelFormat pixFmtSrc , 
		int widthDst , int heightDst , PixelFormat pixFmtDst );

	//init audio video pool when start one new file
	void InitPacketPool(CObjPool<AVPacket> *pool);
	//void PutPacketPool( CObjPool<AVPacket> *pool , AVPacket *pkt );
	int stream_component_open( int stream_index );
	int stream_component_close(int stream_index);
	int DoProcessingLoop();
	//
	int IsFlushPacket( AVPacket *pkt ){
		return  (pkt->data == m_flush_pkt.data) ? 1 : 0 ;
	}

	int GetVideoFrame( AVFrame* );
	int GetAudioFrame();
	int DoVideoDecodeLoop();
	int DoAudioDecodeLoop();
	static unsigned int __stdcall DecodeVideoThread( void *avg );
	static unsigned int __stdcall DecodeAudioThread( void *avg );


protected:
	static CFeFFmpeg *m_pFeFFmpeg;
	CObjPool<AVPacket> m_videopool;
	CObjPool<AVPacket> m_audiopool;
	char m_filenam[1024];
	UNIT_BUF_POOL *m_pbufpool;
	CObjPool<AVPicture> *m_picpool;

	//////////////////////////////////////////////////////////////////////////
	//以下与ffmpeg相关
	AVFormatContext *m_ic;
	AVDictionary *m_codec_opts;
	AVStream *m_audio_st;	//音频流
	AVStream *m_video_st; //视频流
	AVStream *m_subtitle_st; //字幕流
	SwsContext *m_img_convert_ctx;
	int m_video_stream; //视频流的索引值
	int m_audio_stream; //音频流的索引值
	int m_subtitle_stream; //字幕索引值
	int m_wanted_stream[AVMEDIA_TYPE_NB];
	int m_video_disable;
	int m_audio_disable;
	int m_show_status;
	AVPacket m_flush_pkt;
	
};

#endif //_FEFFMPEG_H_