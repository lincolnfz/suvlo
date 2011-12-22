#pragma once
#include <Windows.h>
#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include "../common/bufpool.h"

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

/* NOTE: the size must be big enough to compensate the hardware audio buffersize size */
#define SAMPLE_ARRAY_SIZE (2*65536)

typedef struct _tagPacketQueue {
	AVPacketList *first_pkt, *last_pkt;
	int nb_packets;
	int size;
	int abort_request;
	HANDLE mutex;
	HANDLE cond;
} PacketQueue;

#define VIDEO_PICTURE_QUEUE_SIZE 2
#define SUBPICTURE_QUEUE_SIZE 4

typedef struct _tagVideoPicture {
	double pts;                                  ///<presentation time stamp for this picture
	double duration;                             ///<expected duration of the frame
	int64_t pos;                                 ///<byte position in file
	int skip;
	//SDL_Overlay *bmp;
	int width, height; /* source height & width */
	int allocated;
	int reallocate;
	enum PixelFormat pix_fmt;

#if CONFIG_AVFILTER
	AVFilterBufferRef *picref;
#endif
} VideoPicture;

typedef struct _tagSubPicture {
	double pts; /* presentation time stamp for this picture */
	AVSubtitle sub;
} SubPicture;

typedef struct _tagVideoState {
    //SDL_Thread *read_tid;
    //SDL_Thread *video_tid;
    //SDL_Thread *refresh_tid;
	HANDLE video_tid;
	HANDLE audio_tid;
    AVInputFormat *iformat;
    int no_background;
    int abort_request;
    int paused;
    int last_paused;
    int seek_req;
    int seek_flags;
    int64_t seek_pos;
    int64_t seek_rel;
    int read_pause_return;
    AVFormatContext *ic;

    int audio_stream;

    int av_sync_type;
    double external_clock; /* external clock base */
    int64_t external_clock_time;

    double audio_clock;
    double audio_diff_cum; /* used for AV difference average computation */
    double audio_diff_avg_coef;
    double audio_diff_threshold;
    int audio_diff_avg_count;
    AVStream *audio_st;
    PacketQueue audioq;
    int audio_hw_buf_size;
    /* samples output by the codec. we reserve more space for avsync
       compensation, resampling and format conversion */
    DECLARE_ALIGNED(16,uint8_t,audio_buf1)[AVCODEC_MAX_AUDIO_FRAME_SIZE * 4];
    DECLARE_ALIGNED(16,uint8_t,audio_buf2)[AVCODEC_MAX_AUDIO_FRAME_SIZE * 4];
    uint8_t *audio_buf;
    unsigned int audio_buf_size; /* in bytes */
    int audio_buf_index; /* in bytes */
    int audio_write_buf_size;
    AVPacket audio_pkt_temp;
    AVPacket audio_pkt;
    enum AVSampleFormat audio_src_fmt;
    enum AVSampleFormat audio_tgt_fmt;
    int audio_src_channels;
    int audio_tgt_channels;
    int64_t audio_src_channel_layout;
    int64_t audio_tgt_channel_layout;
    int audio_src_freq;
    int audio_tgt_freq;
    struct SwsContext *img_convert_ctx;
    double audio_current_pts;
    double audio_current_pts_drift;
    int frame_drops_early;
    int frame_drops_late;

    enum ShowMode {
        SHOW_MODE_NONE = -1, SHOW_MODE_VIDEO = 0, SHOW_MODE_WAVES, SHOW_MODE_RDFT, SHOW_MODE_NB
    } show_mode;
    int16_t sample_array[SAMPLE_ARRAY_SIZE];
    int sample_array_index;
    int last_i_start;
    RDFTContext *rdft;
    int rdft_bits;
    FFTSample *rdft_data;
    int xpos;

    //SDL_Thread *subtitle_tid;
    int subtitle_stream;
    int subtitle_stream_changed;
    AVStream *subtitle_st;
    PacketQueue subtitleq;
    SubPicture subpq[SUBPICTURE_QUEUE_SIZE];
    int subpq_size, subpq_rindex, subpq_windex;
    //SDL_mutex *subpq_mutex;
    //SDL_cond *subpq_cond;

    double frame_timer;
    double frame_last_pts;
    double frame_last_duration;
    double frame_last_dropped_pts;
    double frame_last_returned_time;
    double frame_last_filter_delay;
    int64_t frame_last_dropped_pos;
    double video_clock;                          ///<pts of last decoded frame / predicted pts of next decoded frame
    int video_stream;
    AVStream *video_st;
    PacketQueue videoq;
    double video_current_pts;                    ///<current displayed pts (different from video_clock if frame fifos are used)
    double video_current_pts_drift;              ///<video_current_pts - time (av_gettime) at which we updated video_current_pts - used to have running video pts
    int64_t video_current_pos;                   ///<current displayed file pos
    VideoPicture pictq[VIDEO_PICTURE_QUEUE_SIZE];
    int pictq_size, pictq_rindex, pictq_windex;
    //SDL_mutex *pictq_mutex;
    //SDL_cond *pictq_cond;
	HANDLE pictq_mutex;
	HANDLE pictq_cond;
//#if !CONFIG_AVFILTER
//    struct SwsContext *img_convert_ctx;
//#endif

    char filename[1024];
    int width, height, xleft, ytop;
    int step;

#if CONFIG_AVFILTER
    AVFilterContext *out_video_filter;          ///<the last filter in the video chain
#endif

    int refresh;
} VideoState;

typedef struct _tagVideoData{
	AVFrame *avframe;
	double pts;
	unsigned long ldatalen;
	BYTE vdata[360000];
}VideoData;

typedef struct _tagAudioData{
	DECLARE_ALIGNED(16,uint8_t,audio_buf)[AVCODEC_MAX_AUDIO_FRAME_SIZE * 4];
	int data_size;
}AudioData;

//以上是原先从common.h移过来的代码
//////////////////////////////////////////////////////////////////////////

//#define BUF_SIZE 32768
#define MEM_BUF_SIZE 32768
#define CYCLE_BUF_SIZE 786432

//typedef struct _tagVideoState VideoState;

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

	UNIT_BUF_POOL* m_pBufpool;

public:
	CWrapFFMpeg(UNIT_BUF_POOL *);
	virtual ~CWrapFFMpeg(void);
	
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

	int ReadBuf2(char* destbuf , int len);


	void DeCodec( char* , int );
	
	
};

