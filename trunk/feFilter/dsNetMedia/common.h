#ifndef _COMMON_H
#define _COMMON_H
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
	HANDLE decode_audio_thread;
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
    struct SwrContext *swr_ctx;
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
#if !CONFIG_AVFILTER
    struct SwsContext *img_convert_ctx;
#endif

    char filename[1024];
    int width, height, xleft, ytop;
    int step;

#if CONFIG_AVFILTER
    AVFilterContext *out_video_filter;          ///<the last filter in the video chain
#endif

    int refresh;
} VideoState;

// {7CCDB408-C5CA-447B-A34B-E1106F0D0119}
DEFINE_GUID( CLSID_NetMediaSource , 
	0x7ccdb408, 0xc5ca, 0x447b, 0xa3, 0x4b, 0xe1, 0x10, 0x6f, 0xd, 0x1, 0x19);

// {245E62C2-432B-46DA-A0CF-A9B1D8710481}
DEFINE_GUID( IID_INetSource, 
	0x245e62c2, 0x432b, 0x46da, 0xa0, 0xcf, 0xa9, 0xb1, 0xd8, 0x71, 0x4, 0x81);

#ifdef _cplusplus
extern "c"{
#endif	

	DECLARE_INTERFACE_( INetSource , IUnknown )
	{
		STDMETHOD( play )(THIS_ LPCWSTR url)PURE;
		STDMETHOD( seek )(THIS_ ULONG64 utime)PURE;
	};

#ifdef _cplusplus
}
#endif

typedef int (*NotifyParserOK)();

#endif