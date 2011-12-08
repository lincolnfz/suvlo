//#include "StdAfx.h"
#include "WrapFFMpeg.h"
#include "NetSoruceFilter.h"


#include <assert.h>
#include <process.h>
#include <crtdbg.h>

#pragma comment( lib , "../../lib/avcodec.lib" )
#pragma comment( lib , "../../lib/avdevice.lib" )
#pragma comment( lib , "../../lib/avfilter.lib" )
#pragma comment( lib , "../../lib/avformat.lib" )
#pragma comment( lib , "../../lib/avutil.lib" )
#pragma comment( lib , "../../lib/swscale.lib" )

VideoState *g_pVideoState = NULL;


#define MAX_QUEUE_SIZE (15 * 1024 * 1024)
#define MIN_AUDIOQ_SIZE (20 * 16 * 1024)
#define MIN_FRAMES 5

/* SDL audio buffer size, in samples. Should be small to have precise
   A/V sync as SDL does not have hardware buffer fullness info. */
#define SDL_AUDIO_BUFFER_SIZE 1024

/* no AV sync correction is done if below the AV sync threshold */
#define AV_SYNC_THRESHOLD 0.01
/* no AV correction is done if too big error */
#define AV_NOSYNC_THRESHOLD 10.0

/* maximum audio speed change to get correct sync */
#define SAMPLE_CORRECTION_PERCENT_MAX 10

/* we use about AUDIO_DIFF_AVG_NB A-V differences to make the average */
#define AUDIO_DIFF_AVG_NB   20

static int sws_flags = SWS_BICUBIC;

enum {
	AV_SYNC_AUDIO_MASTER, /* default choice */
	AV_SYNC_VIDEO_MASTER,
	AV_SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */
};



/* options specified by the user */
static AVInputFormat *file_iformat;
static const char *input_filename;
static const char *window_title;
static int fs_screen_width;
static int fs_screen_height;
static int screen_width = 0;
static int screen_height = 0;
static int audio_disable;
static int video_disable;
static int wanted_stream[AVMEDIA_TYPE_NB]={ -1 , -1 , -1 , -1 , -1 };
static int seek_by_bytes=-1;
static int display_disable;
static int show_status = 1;
static int av_sync_type = AV_SYNC_AUDIO_MASTER;
static int64_t start_time = AV_NOPTS_VALUE;
static int64_t duration = AV_NOPTS_VALUE;
static int workaround_bugs = 1;
static int fast = 0;
static int genpts = 0;
static int lowres = 0;
static int idct = FF_IDCT_AUTO;
static enum AVDiscard skip_frame= AVDISCARD_DEFAULT;
static enum AVDiscard skip_idct= AVDISCARD_DEFAULT;
static enum AVDiscard skip_loop_filter= AVDISCARD_DEFAULT;
static int error_recognition = FF_ER_CAREFUL;
static int error_concealment = 3;
static int decoder_reorder_pts= -1;
static int autoexit;
static int exit_on_keydown;
static int exit_on_mousedown;
static int loop=1;
static int framedrop=-1;
//static enum ShowMode show_mode = VideoState.ShowMode.SHOW_MODE_NONE;
static const char *audio_codec_name;
static const char *subtitle_codec_name;
static const char *video_codec_name;

static int rdftspeed=20;
#if CONFIG_AVFILTER
static char *vfilters = NULL;
#endif

/* current context */
static int is_full_screen;
static int64_t audio_callback_time;

static AVPacket flush_pkt;

#define FF_ALLOC_EVENT   (WM_USER)
#define FF_REFRESH_EVENT (WM_USER + 1)
#define FF_QUIT_EVENT    (WM_USER + 2)

//lincoln add
AVDictionary  *format_opts , *codec_opts;
char* pMemBuf;
CWrapFFMpeg* pWrapFFMpeg;
extern CNetSourceFilter *g_pNetSourceFilter;

int mem_open(URLContext *h, const char *url, int flags)
{
	h->priv_data = pWrapFFMpeg;
	return 0;
}

int mem_read(URLContext *h, unsigned char *buf, int size)
{
	CWrapFFMpeg* pFFMpeg = (CWrapFFMpeg*)h->priv_data;
	return pFFMpeg->ReadBuf( (char*)buf , size );
}

int mem_get_handle(URLContext *h)
{
	return (intptr_t) h->priv_data;
}

URLProtocol memProtocol;

void init_new_protocol()
{
	memset( &memProtocol , 0 , sizeof(memProtocol) );
	memProtocol.name = "femem";
	memProtocol.url_open = mem_open;
	memProtocol.url_read = mem_read;
	memProtocol.url_get_file_handle = mem_get_handle;

}

CWrapFFMpeg::CWrapFFMpeg(void)
{
	InitializeCriticalSection(&m_calcSection);
	Init();
	m_pVideoState = NULL;
	m_eof = 0;
	m_hScrapMemEvent = CreateEvent( NULL , FALSE , FALSE  , NULL );
	m_hFillMemEvent = CreateEvent( NULL , FALSE , FALSE  , NULL );
	m_hOpMemLock = CreateMutex( NULL , FALSE , NULL );
	pMemBuf = (char*)malloc(MEM_BUF_SIZE);
	m_cycle_head = (char*)malloc( CYCLE_BUF_SIZE * sizeof(char) );
	m_cycle_end = m_cycle_head + CYCLE_BUF_SIZE; //环型内存的终界位置,不能存数据	
	pWrapFFMpeg = this;
	flush_buf();
}


CWrapFFMpeg::~CWrapFFMpeg(void)
{
	free( pMemBuf );
	CloseHandle( m_hScrapMemEvent );
	CloseHandle( m_hFillMemEvent );
	CloseHandle( m_hOpMemLock );
	DeleteCriticalSection( &m_calcSection );
	clean();
}

void CWrapFFMpeg::flush_buf()
{
	m_valid_head = m_valid_end = m_cycle_head;
}

void CWrapFFMpeg::Init()
{
	av_register_all();
	avcodec_init();
	avcodec_register_all();
	avdevice_register_all();
	avfilter_register_all();
	init_new_protocol();
	av_register_protocol2( &memProtocol , sizeof(memProtocol) );
}

void CWrapFFMpeg::clean()
{
	if ( m_pVideoState )
	{
		av_freep( m_pVideoState );
		m_pVideoState = NULL;
	}
}

int CWrapFFMpeg::calc_remain_size()
{
	//EnterCriticalSection( &m_calcSection );
	WaitForSingleObject( m_hOpMemLock , INFINITE );
	int size = CYCLE_BUF_SIZE;
	if ( m_valid_head == m_valid_end )
	{
		size = CYCLE_BUF_SIZE;
	}
	else if ( m_valid_head < m_valid_end )
	{
		size = (m_valid_head - m_cycle_head) + ( m_cycle_end - m_valid_end );
	}
	else if ( m_valid_head > m_valid_end )
	{
		size = m_valid_head - m_valid_end;
	}
	//LeaveCriticalSection( &m_calcSection );
	ReleaseMutex( m_hOpMemLock );
	return size;
}

int CWrapFFMpeg::calc_valid_size()
{
	return CYCLE_BUF_SIZE - calc_remain_size();
}

void CWrapFFMpeg::Fillbuf( char* srcbuf , int len , int eof )
{
	if ( m_pVideoState == NULL )
	{
		notify_new_launch();
	}
	while( calc_remain_size() < len ) //判断是否有足够空间存放数据
	{
		WaitForSingleObject( m_hScrapMemEvent , INFINITE );
	}

	//已有足够的空间保存数据
	WaitForSingleObject( m_hOpMemLock , INFINITE );
	m_eof = eof;
	
	if ( m_valid_head < m_valid_end )
	{
		int hsize , remainsize;		
		int left_size = m_cycle_end - m_valid_end;
		_ASSERT( left_size >= 0 );
		hsize = len;
		remainsize = 0;
		if( len > left_size )
		{
			hsize = left_size;
			remainsize = len - hsize;
		}
		if( hsize > 0 )
		{
			memcpy( m_valid_end , srcbuf , hsize );
			m_valid_end += hsize;
		}
		if ( remainsize > 0 )
		{
			memcpy( m_cycle_head , srcbuf + hsize , remainsize );
			m_valid_end = m_cycle_head + remainsize;
		}
	}
	else if ( m_valid_head > m_valid_end )
	{
		memcpy( m_valid_end , srcbuf , len );
		m_valid_end += len;
	}
	else if ( m_valid_head == m_valid_end )
	{
		m_valid_head = m_valid_end = m_cycle_head;
		memcpy( m_valid_head , srcbuf , len );
		m_valid_end += len;
	}
	ReleaseMutex( m_hOpMemLock );
	SetEvent( m_hFillMemEvent );
}

int CWrapFFMpeg::ReadBuf( char* destbuf , int len )
{
	int readlen = len;
	while( m_eof == 0 && calc_valid_size() < readlen )
	{
		WaitForSingleObject( m_hFillMemEvent , INFINITE );
	}
	if ( m_eof )
	{
		readlen = calc_valid_size();
	}
	WaitForSingleObject( m_hOpMemLock , INFINITE );
	if ( m_valid_head < m_valid_end )
	{
		memcpy( destbuf , m_valid_head ,readlen );
		m_valid_head += readlen;
	}
	else if ( m_valid_head > m_valid_end )
	{
		int hsize = m_cycle_end - m_valid_head;
		_ASSERT( hsize >= 0 );
		int remainsize = readlen - hsize;
		if( hsize > 0 )
		{
			memcpy( destbuf , m_valid_head , hsize );
			m_valid_head += hsize;
		}
		if ( remainsize > 0 )
		{
			memcpy( destbuf + hsize , m_cycle_head , remainsize );
			m_valid_head = m_cycle_head + remainsize;
		}
	}
	else if ( m_valid_head == m_valid_end )
	{
		readlen = 0;
	}
	ReleaseMutex( m_hOpMemLock );
	SetEvent( m_hScrapMemEvent );
	return readlen;
}

//数据包队列处理
static int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
	AVPacketList *pkt1;

	/* duplicate the packet */
	if (pkt!=&flush_pkt && av_dup_packet(pkt) < 0)
		return -1;

	pkt1 = (AVPacketList*)av_malloc(sizeof(AVPacketList));
	if (!pkt1)
		return -1;
	pkt1->pkt = *pkt;
	pkt1->next = NULL;


	WaitForSingleObject( q->mutex , INFINITE );

	if (!q->last_pkt)

		q->first_pkt = pkt1;
	else
		q->last_pkt->next = pkt1;
	q->last_pkt = pkt1;
	q->nb_packets++;
	q->size += pkt1->pkt.size + sizeof(*pkt1);
	/* XXX: should duplicate packet data in DV case */
	SetEvent( q->cond );
	ReleaseMutex( q->mutex );		
	
	return 0;
}

/* packet queue handling */
static void packet_queue_init(PacketQueue *q)
{
	memset(q, 0, sizeof(PacketQueue));
	q->mutex = CreateMutex(NULL , FALSE , NULL);
	q->cond = CreateEvent( NULL , FALSE , FALSE , NULL );	
	packet_queue_put(q, &flush_pkt);
}

static void packet_queue_flush(PacketQueue *q)
{
	AVPacketList *pkt, *pkt1;

	WaitForSingleObject( q->mutex , INFINITE );
	for(pkt = q->first_pkt; pkt != NULL; pkt = pkt1) {
		pkt1 = pkt->next;
		av_free_packet(&pkt->pkt);
		av_freep(&pkt);
	}
	q->last_pkt = NULL;
	q->first_pkt = NULL;
	q->nb_packets = 0;
	q->size = 0;
	ReleaseMutex( q->mutex );
}

static void packet_queue_end(PacketQueue *q)
{
	packet_queue_flush(q);

	CloseHandle( q->mutex );
	CloseHandle( q->cond );
	q->mutex = NULL;
	q->cond = NULL;
}

static void packet_queue_abort(PacketQueue *q)
{
	WaitForSingleObject( q->mutex , INFINITE );

	q->abort_request = 1;
	
	ReleaseMutex( q->mutex );	
	SetEvent( q->cond );
}

/* return < 0 if aborted, 0 if no packet and > 0 if packet. 取出队列中的包,赋值给pkt.然后删除包 */
static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block)
{
	AVPacketList *pkt1;
	int ret;

	WaitForSingleObject( q->mutex , INFINITE );

	for(;;) {
		if (q->abort_request) {
			ret = -1;
			break;
		}

		pkt1 = q->first_pkt;
		if (pkt1) {
			q->first_pkt = pkt1->next;
			if (!q->first_pkt)
				q->last_pkt = NULL;
			--(q->nb_packets);
			q->size -= pkt1->pkt.size + sizeof(*pkt1);
			*pkt = pkt1->pkt;
			av_free(pkt1);
			ret = 1;
			break;
		} else if (!block) {
			ret = 0;
			break;
		} else {
			//SDL_CondWait(q->cond, q->mutex);			
			ReleaseMutex( q->mutex );			
			WaitForSingleObject( q->cond , INFINITE );
			WaitForSingleObject( q->mutex , INFINITE );			
		}
	}
	ReleaseMutex( q->mutex );
	return ret;
}

/* get the current audio clock value */
static double get_audio_clock(VideoState *is)
{
	if (is->paused) {
		return is->audio_current_pts;
	} else {
		return is->audio_current_pts_drift + av_gettime() / 1000000.0;
	}
}

/* get the current video clock value */
static double get_video_clock(VideoState *is)
{
	if (is->paused) {
		return is->video_current_pts;
	} else {
		return is->video_current_pts_drift + av_gettime() / 1000000.0;
	}
}

/* get the current external clock value */
static double get_external_clock(VideoState *is)
{
	int64_t ti;
	ti = av_gettime();
	return is->external_clock + ((ti - is->external_clock_time) * 1e-6);
}

/* get the current master clock value */
static double get_master_clock(VideoState *is)
{
	double val;

	if (is->av_sync_type == AV_SYNC_VIDEO_MASTER) {
		if (is->video_st)
			val = get_video_clock(is);
		else
			val = get_audio_clock(is);
	} else if (is->av_sync_type == AV_SYNC_AUDIO_MASTER) {
		if (is->audio_st)
			val = get_audio_clock(is);
		else
			val = get_video_clock(is);
	} else {
		val = get_external_clock(is);
	}
	return val;
}

int check_stream_specifier(AVFormatContext *s, AVStream *st, const char *spec)
{
	if (*spec <= '9' && *spec >= '0')                                        /* opt:index */
		return strtol(spec, NULL, 0) == st->index;
	else if (*spec == 'v' || *spec == 'a' || *spec == 's' || *spec == 'd' || *spec == 't') { /* opt:[vasdt] */
		enum AVMediaType type;

		switch (*spec++) {
		case 'v': type = AVMEDIA_TYPE_VIDEO;    break;
		case 'a': type = AVMEDIA_TYPE_AUDIO;    break;
		case 's': type = AVMEDIA_TYPE_SUBTITLE; break;
		case 'd': type = AVMEDIA_TYPE_DATA;     break;
		case 't': type = AVMEDIA_TYPE_ATTACHMENT; break;
		default: abort(); // never reached, silence warning
		}
		if (type != st->codec->codec_type)
			return 0;
		if (*spec++ == ':') {                                   /* possibly followed by :index */
			int i, index = strtol(spec, NULL, 0);
			for (i = 0; i < s->nb_streams; i++)
				if (s->streams[i]->codec->codec_type == type && index-- == 0)
					return i == st->index;
			return 0;
		}
		return 1;
	} else if (*spec == 'p' && *(spec + 1) == ':') {
		int prog_id, i, j;
		char *endptr;
		spec += 2;
		prog_id = strtol(spec, &endptr, 0);
		for (i = 0; i < s->nb_programs; i++) {
			if (s->programs[i]->id != prog_id)
				continue;

			if (*endptr++ == ':') {
				int stream_idx = strtol(endptr, NULL, 0);
				return (stream_idx >= 0 && stream_idx < s->programs[i]->nb_stream_indexes &&
					st->index == s->programs[i]->stream_index[stream_idx]);
			}

			for (j = 0; j < s->programs[i]->nb_stream_indexes; j++)
				if (st->index == s->programs[i]->stream_index[j])
					return 1;
		}
		return 0;
	} else if (!*spec) /* empty specifier, matches everything */
		return 1;

	av_log(s, AV_LOG_ERROR, "Invalid stream specifier: %s.\n", spec);
	return AVERROR(EINVAL);
}

AVDictionary *filter_codec_opts(AVDictionary *opts, enum CodecID codec_id, AVFormatContext *s, AVStream *st)
{
	AVDictionary    *ret = NULL;
	AVDictionaryEntry *t = NULL;
	AVCodec       *codec = s->oformat ? avcodec_find_encoder(codec_id) : avcodec_find_decoder(codec_id);
	int            flags = s->oformat ? AV_OPT_FLAG_ENCODING_PARAM : AV_OPT_FLAG_DECODING_PARAM;
	char          prefix = 0;
	const AVClass    *cc = avcodec_get_class();

	if (!codec)
		return NULL;

	switch (codec->type) {
	case AVMEDIA_TYPE_VIDEO:    prefix = 'v'; flags |= AV_OPT_FLAG_VIDEO_PARAM;    break;
	case AVMEDIA_TYPE_AUDIO:    prefix = 'a'; flags |= AV_OPT_FLAG_AUDIO_PARAM;    break;
	case AVMEDIA_TYPE_SUBTITLE: prefix = 's'; flags |= AV_OPT_FLAG_SUBTITLE_PARAM; break;
	}

	while (t = av_dict_get(opts, "", t, AV_DICT_IGNORE_SUFFIX)) {
		char *p = strchr(t->key, ':');

		/* check stream specification in opt name */
		if (p)
			switch (check_stream_specifier(s, st, p + 1)) {
			case  1: *p = 0; break;
			case  0:         continue;
			default:         return NULL;
		}

		if (av_opt_find(&cc, t->key, NULL, flags, AV_OPT_SEARCH_FAKE_OBJ) ||
			(codec && codec->priv_class && av_opt_find(&codec->priv_class, t->key, NULL, flags, AV_OPT_SEARCH_FAKE_OBJ)))
			av_dict_set(&ret, t->key, t->value, 0);
		else if (t->key[0] == prefix && av_opt_find(&cc, t->key+1, NULL, flags, AV_OPT_SEARCH_FAKE_OBJ))
			av_dict_set(&ret, t->key+1, t->value, 0);

		if (p)
			*p = ':';
	}
	return ret;
}

AVDictionary **setup_find_stream_info_opts(AVFormatContext *s, AVDictionary *codec_opts)
{
	int i;
	AVDictionary **opts;

	if (!s->nb_streams)
		return NULL;
	opts = (AVDictionary**)av_mallocz(s->nb_streams * sizeof(*opts));
	if (!opts) {
		av_log(NULL, AV_LOG_ERROR, "Could not alloc memory for stream options.\n");
		return NULL;
	}
	for (i = 0; i < s->nb_streams; i++)
		opts[i] = filter_codec_opts(codec_opts, s->streams[i]->codec->codec_id, s, s->streams[i]);
	return opts;
}

//得到解码后的帧数据
static int get_video_frame(VideoState *is, AVFrame *frame, int64_t *pts, AVPacket *pkt)
{
	int got_picture, i;

	if (packet_queue_get(&is->videoq, pkt, 1) < 0)
		return -1;

	if (pkt->data == flush_pkt.data) {
		//如果是遇到结束视频的包
		avcodec_flush_buffers(is->video_st->codec);

		//SDL_LockMutex(is->pictq_mutex);
		WaitForSingleObject( is->pictq_mutex , INFINITE );
		//Make sure there are no long delay timers (ideally we should just flush the que but thats harder)
		for (i = 0; i < VIDEO_PICTURE_QUEUE_SIZE; i++) {
			is->pictq[i].skip = 1;
		}
		while (is->pictq_size && !is->videoq.abort_request) {
			//SDL_CondWait(is->pictq_cond, is->pictq_mutex);
			ReleaseMutex( is->pictq_mutex );
			WaitForSingleObject( is->pictq_cond , INFINITE );
			WaitForSingleObject( is->pictq_mutex , INFINITE);
		}
		is->video_current_pos = -1;
		is->frame_last_pts = AV_NOPTS_VALUE;
		is->frame_last_duration = 0;
		is->frame_timer = (double)av_gettime() / 1000000.0;
		is->frame_last_dropped_pts = AV_NOPTS_VALUE;
		ReleaseMutex(is->pictq_mutex);

		return 0;
	}

	avcodec_decode_video2(is->video_st->codec, frame, &got_picture, pkt);

	if (got_picture) {
		int ret = 1;

		if (decoder_reorder_pts == -1) {
			*pts = frame->best_effort_timestamp;
		} else if (decoder_reorder_pts) {
			*pts = frame->pkt_pts;
		} else {
			*pts = frame->pkt_dts;
		}

		if (*pts == AV_NOPTS_VALUE) {
			*pts = 0;
		}
		/*
		if (((is->av_sync_type == AV_SYNC_AUDIO_MASTER && is->audio_st) || is->av_sync_type == AV_SYNC_EXTERNAL_CLOCK) &&
			(framedrop>0 || (framedrop && is->audio_st))) {
				WaitForSingleObject(is->pictq_mutex , INFINITE);
				if (is->frame_last_pts != AV_NOPTS_VALUE && *pts) {
					double clockdiff = get_video_clock(is) - get_master_clock(is);
					double dpts = av_q2d(is->video_st->time_base) * *pts;
					double ptsdiff = dpts - is->frame_last_pts;
					if (fabs(clockdiff) < AV_NOSYNC_THRESHOLD &&
						ptsdiff > 0 && ptsdiff < AV_NOSYNC_THRESHOLD &&
						clockdiff + ptsdiff - is->frame_last_filter_delay < 0) {
							is->frame_last_dropped_pos = pkt->pos;
							is->frame_last_dropped_pts = dpts;
							is->frame_drops_early++;
							ret = 0;
					}
				}
				ReleaseMutex(is->pictq_mutex);
		}*/		

		if (ret)
		{
			is->frame_last_returned_time = av_gettime() / 1000000.0;
		}
		/*
		AVPicture targetFrame;
		DWORD dwActualDstLength;
		memset(&targetFrame,0,sizeof(AVPicture));*/
	

		return ret;
	}
	return 0;
}

//解码音频数据
static int decode_audio_frame( VideoState *is , AudioData *outdata , AVPacket *pkt ){
	int ret = 0;
	if ( pkt->data == flush_pkt.data )
	{
		//遇到结束的数据包
		return 0;
	}
	ret = avcodec_decode_audio3( is->audio_st->codec , (int16_t *)(outdata->audio_buf) , &(outdata->data_size) , pkt );
	return ret;
}

/**
解码音频的线程
*/
unsigned int __stdcall decode_audio_thread( void* arg )
{
	VideoState *is = (VideoState*)arg;
	//AVFrameLink *pAudioLink = g_pNetSourceFilter->getAudioFrameLink();
	DataLink<AudioData> *pAudioLink = g_pNetSourceFilter->getAudioDataLink();
	for (;;)
	{
		AVPacket pkt;
		packet_queue_get( &is->audioq , &pkt , 1 );
		DataNode<AudioData> *pDataNode = (DataNode<AudioData>*)malloc(sizeof(DataNode<AudioData>));
		memset( pDataNode , 0 , sizeof(DataNode<AudioData>) );
		pDataNode->pData.data_size = sizeof( pDataNode->pData.audio_buf );
		pDataNode->pNext = NULL;
		int ret = decode_audio_frame( is , &(pDataNode->pData) , &pkt );
		av_free_packet(&pkt);
		if ( ret < 0 )
		{
			free( pDataNode );
			goto end;
		}
		putDataLink( pAudioLink , pDataNode );
		
	}
	
end:
	return 0;
}

/**
解码视频的线程
**/
unsigned int __stdcall decode_video_thread( void* pArg )
{
	VideoState *is = (VideoState*)pArg;	
	int64_t pts_int = AV_NOPTS_VALUE, pos = -1;
	double pts;
	int ret;
	DataLink<VideoData> *pAVFrameLink = g_pNetSourceFilter->getVideoFrameLink();

	for (;;)
	{
		AVPacket pkt;
		AVFrame *frame= avcodec_alloc_frame();
		ret = get_video_frame(is, frame, &pts_int, &pkt);
		pos = pkt.pos;
		av_free_packet(&pkt);

		if (ret < 0){
			av_free(frame);
			goto the_end;
		}

		is->frame_last_filter_delay = av_gettime() / 1000000.0 - is->frame_last_returned_time;
		if (fabs(is->frame_last_filter_delay) > AV_NOSYNC_THRESHOLD / 10.0)
			is->frame_last_filter_delay = 0;

		pts = pts_int*av_q2d(is->video_st->time_base);

		//ret = queue_picture(is, frame, pts, pos);
		//写到directshow的内存中
		DataNode<VideoData> *pNode =  (DataNode<VideoData>*)malloc( sizeof(DataNode<VideoData>) );
		pNode->pData.avframe = frame;
		pNode->pData.pts = pts_int;
		pNode->pNext = NULL;

		putDataLink( pAVFrameLink , pNode );
		//putAVFrameLink( pAVFrameLink , newNode );

		//if (ret < 0)
		//	goto the_end;
	}

the_end:
	//av_free(frame);
	return 0;
}

/* open a given stream. Return 0 if OK */ //这个函数的注释还没翻译过来
static int stream_component_open(VideoState *is, int stream_index)
{
    AVFormatContext *ic = is->ic;
    AVCodecContext *avctx;
    AVCodec *codec;
    //SDL_AudioSpec wanted_spec, spec;
    AVDictionary *opts;
    AVDictionaryEntry *t = NULL;
    int64_t wanted_channel_layout = 0;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return -1;
    avctx = ic->streams[stream_index]->codec;

    opts = filter_codec_opts(codec_opts, avctx->codec_id, ic, ic->streams[stream_index]);

    codec = avcodec_find_decoder(avctx->codec_id);
    switch(avctx->codec_type){
        case AVMEDIA_TYPE_AUDIO   : if(audio_codec_name   ) codec= avcodec_find_decoder_by_name(   audio_codec_name); break;
        case AVMEDIA_TYPE_SUBTITLE: if(subtitle_codec_name) codec= avcodec_find_decoder_by_name(subtitle_codec_name); break;
        case AVMEDIA_TYPE_VIDEO   : if(video_codec_name   ) codec= avcodec_find_decoder_by_name(   video_codec_name); break;
    }
    if (!codec)
        return -1;

    avctx->workaround_bugs = workaround_bugs;
    avctx->lowres = lowres;
    if(lowres) avctx->flags |= CODEC_FLAG_EMU_EDGE;
    avctx->idct_algo= idct;
    if(fast) avctx->flags2 |= CODEC_FLAG2_FAST;
    avctx->skip_frame= skip_frame;
    avctx->skip_idct= skip_idct;
    avctx->skip_loop_filter= skip_loop_filter;
    avctx->error_recognition= error_recognition;
    avctx->error_concealment= error_concealment;

    if(codec->capabilities & CODEC_CAP_DR1)
        avctx->flags |= CODEC_FLAG_EMU_EDGE;
	
	/*
    if (avctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        wanted_channel_layout = (avctx->channel_layout && avctx->channels == av_get_channel_layout_nb_channels(avctx->channels)) ? avctx->channel_layout : av_get_default_channel_layout(avctx->channels);
        wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
        wanted_spec.channels = av_get_channel_layout_nb_channels(wanted_channel_layout);
        wanted_spec.freq = avctx->sample_rate;
        if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
            fprintf(stderr, "Invalid sample rate or channel count!\n");
            return -1;
        }
    }
	*/

    if (!codec ||
        avcodec_open2(avctx, codec, &opts) < 0)
        return -1;
    if ((t = av_dict_get(opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
        av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
        return AVERROR_OPTION_NOT_FOUND;
    }

    /* prepare audio output */
    /*
	if (avctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        wanted_spec.format = AUDIO_S16SYS;
        wanted_spec.silence = 0;
        wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
        wanted_spec.callback = sdl_audio_callback;
        wanted_spec.userdata = is;
        if (SDL_OpenAudio(&wanted_spec, &spec) < 0) {
            fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError());
            return -1;
        }
        is->audio_hw_buf_size = spec.size;
        if (spec.format != AUDIO_S16SYS) {
            fprintf(stderr, "SDL advised audio format %d is not supported!\n", spec.format);
            return -1;
        }
        if (spec.channels != wanted_spec.channels) {
            wanted_channel_layout = av_get_default_channel_layout(spec.channels);
            if (!wanted_channel_layout) {
                fprintf(stderr, "SDL advised channel count %d is not supported!\n", spec.channels);
                return -1;
            }
        }
        is->audio_src_fmt = is->audio_tgt_fmt = AV_SAMPLE_FMT_S16;
        is->audio_src_freq = is->audio_tgt_freq = spec.freq;
        is->audio_src_channel_layout = is->audio_tgt_channel_layout = wanted_channel_layout;
        is->audio_src_channels = is->audio_tgt_channels = spec.channels;
    }
	*/

    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    switch(avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        {
			is->audio_stream = stream_index;
			is->audio_st = ic->streams[stream_index];
			is->audio_buf_size = 0;
			is->audio_buf_index = 0;

			/* init averaging filter */
			is->audio_diff_avg_coef = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
			is->audio_diff_avg_count = 0;
			/* since we do not have a precise anough audio fifo fullness,
			   we correct audio sync only if larger than this threshold */
			//is->audio_diff_threshold = 2.0 * SDL_AUDIO_BUFFER_SIZE / wanted_spec.freq;

			memset(&is->audio_pkt, 0, sizeof(is->audio_pkt));
			packet_queue_init(&is->audioq);
			unsigned int tid;
			is->audio_tid = (HANDLE)_beginthreadex( NULL , 0 , decode_audio_thread , is , 0 , &tid );
			CloseHandle ( is->audio_tid );
		}
       // SDL_PauseAudio(0);
        break;
    case AVMEDIA_TYPE_VIDEO:
		{
			is->video_stream = stream_index;
			is->video_st = ic->streams[stream_index];

			packet_queue_init(&is->videoq);
		   // is->video_tid = SDL_CreateThread(video_thread, is);
			unsigned int tid;
			is->video_tid = (HANDLE)_beginthreadex( NULL , 0 , decode_video_thread , is , 0 , &tid );
			CloseHandle(is->video_tid);
		}
        break;
    case AVMEDIA_TYPE_SUBTITLE:
		{
			is->subtitle_stream = stream_index;
			is->subtitle_st = ic->streams[stream_index];
			packet_queue_init(&is->subtitleq);

		   // is->subtitle_tid = SDL_CreateThread(subtitle_thread, is);
		}
        break;
    default:
        break;
    }
    return 0;
}

static void stream_component_close(VideoState *is, int stream_index)
{
    AVFormatContext *ic = is->ic;
    AVCodecContext *avctx;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return;
    avctx = ic->streams[stream_index]->codec;

    switch(avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        packet_queue_abort(&is->audioq);

        //SDL_CloseAudio();

        packet_queue_end(&is->audioq);
        //if (is->swr_ctx)
        //   swr_free(&is->swr_ctx);
        av_free_packet(&is->audio_pkt);

        if (is->rdft) {
            av_rdft_end(is->rdft);
            av_freep(&is->rdft_data);
        }
        break;
    case AVMEDIA_TYPE_VIDEO:
        packet_queue_abort(&is->videoq);

        /* note: we also signal this mutex to make sure we deblock the
           video thread in all cases */
        /*
		SDL_LockMutex(is->pictq_mutex);
        SDL_CondSignal(is->pictq_cond);
        SDL_UnlockMutex(is->pictq_mutex);

        SDL_WaitThread(is->video_tid, NULL);*/

        packet_queue_end(&is->videoq);
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        packet_queue_abort(&is->subtitleq);

        /* note: we also signal this mutex to make sure we deblock the
           video thread in all cases */
       // SDL_LockMutex(is->subpq_mutex);
        is->subtitle_stream_changed = 1;

        /*
		SDL_CondSignal(is->subpq_cond);
        SDL_UnlockMutex(is->subpq_mutex);

        SDL_WaitThread(is->subtitle_tid, NULL);
		*/

        packet_queue_end(&is->subtitleq);
        break;
    default:
        break;
    }

    ic->streams[stream_index]->discard = AVDISCARD_ALL;
    avcodec_close(avctx);
    switch(avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        is->audio_st = NULL;
        is->audio_stream = -1;
        break;
    case AVMEDIA_TYPE_VIDEO:
        is->video_st = NULL;
        is->video_stream = -1;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        is->subtitle_st = NULL;
        is->subtitle_stream = -1;
        break;
    default:
        break;
    }
}

int read_packet(void *opaque, uint8_t *buf, int buf_size)
{
	URLProtocol* protocol = (URLProtocol*)opaque;
	//protocol->url_read()

	return buf_size;
}

int initcodec( char* buf, int len , AVFormatContext** pfCtx )
{
	int err,ret;
	AVProbeData probedata;
	probedata.buf = (unsigned char*)buf;
	probedata.buf_size = len;
	probedata.filename = NULL;
	AVInputFormat* pInFormat = av_probe_input_format( &probedata , AVFMT_NOFILE );

	unsigned char* pbbuf = (unsigned char*)av_malloc(MEM_BUF_SIZE);
	AVFormatContext* pFormatCtx = avformat_alloc_context();
	*pfCtx = pFormatCtx;
	pFormatCtx->iformat = pInFormat; //
	AVIOContext* avio = avio_alloc_context( (unsigned char*)pbbuf , 20480 , 0 ,(void*) &memProtocol , read_packet , NULL , NULL );
	pFormatCtx->pb = avio; //读取
	//url_fopen( &(pFormatCtx->pb) , "femem://play.mem" , 0 );


	AVDictionary *tmp = NULL;
	/* allocate private data */
	if (pFormatCtx->iformat->priv_data_size > 0) {
		if (!(pFormatCtx->priv_data = av_mallocz(pFormatCtx->iformat->priv_data_size))) {
			ret = AVERROR(ENOMEM);
		}
		if (pFormatCtx->iformat->priv_class) {
			*(const AVClass**)pFormatCtx->priv_data = pFormatCtx->iformat->priv_class;
			av_opt_set_defaults(pFormatCtx->priv_data);
			if ((ret = av_opt_set_dict(pFormatCtx->priv_data, &tmp)) < 0){}
		}
	}

	//URLContext cc;
	//URLProtocol ax;
	//ByteIOContext

	AVFormatParameters ap = { { 0 } };
	if( pFormatCtx->iformat->read_header )
	{
		err = pFormatCtx->iformat->read_header( pFormatCtx , &ap );
	}

	if (!(pFormatCtx->flags&AVFMT_FLAG_PRIV_OPT) && pFormatCtx->pb && !pFormatCtx->data_offset)
		pFormatCtx->data_offset = avio_tell(pFormatCtx->pb);

	pFormatCtx->raw_packet_buffer_remaining_size = RAW_PACKET_BUFFER_SIZE;

	pFormatCtx->flags |= AVFMT_FLAG_GENPTS;

	AVDictionary** opts;
	av_dict_set(&codec_opts, "request_channels", "2", 0);


	opts = setup_find_stream_info_opts(pFormatCtx, codec_opts);
	unsigned int orig_nb_streams = pFormatCtx->nb_streams;

	//解析流的信息
	err = avformat_find_stream_info(pFormatCtx, opts);

	int i;
	for (i = 0; i < orig_nb_streams; i++)
		av_dict_free(&opts[i]);
	av_freep(&opts);

	for (i = 0; i < pFormatCtx->nb_streams; i++)
		pFormatCtx->streams[i]->discard = AVDISCARD_ALL;

	int st_index[AVMEDIA_TYPE_NB]={-1,-1,-1,-1,-1};
	st_index[AVMEDIA_TYPE_VIDEO] =
		av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO,
		wanted_stream[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);

	st_index[AVMEDIA_TYPE_AUDIO] =
		av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_AUDIO,
		wanted_stream[AVMEDIA_TYPE_AUDIO],
		st_index[AVMEDIA_TYPE_VIDEO],
		NULL, 0);

	st_index[AVMEDIA_TYPE_SUBTITLE] =
		av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_SUBTITLE,
		wanted_stream[AVMEDIA_TYPE_SUBTITLE],
		(st_index[AVMEDIA_TYPE_AUDIO] >= 0 ?
		st_index[AVMEDIA_TYPE_AUDIO] :
	st_index[AVMEDIA_TYPE_VIDEO]),
		NULL, 0);

	av_init_packet(&flush_pkt);
	flush_pkt.data = (uint8_t*)"FLUSH";

	//以上代码把音,视频的数据流分离出来
	if ( st_index[AVMEDIA_TYPE_VIDEO] >= 0  )
	{
		//stream_component_open( pFormatCtx , st_index[AVMEDIA_TYPE_VIDEO] );
	}

	if ( st_index[AVMEDIA_TYPE_AUDIO] >= 0 )
	{
		//stream_component_open( pFormatCtx , st_index[AVMEDIA_TYPE_AUDIO] );
	}


	return 0;
}

//解析内存
void CWrapFFMpeg::DeCodec(char* buf, int len)
{
	static BOOL bFirst = TRUE;
	if ( bFirst == TRUE )
	{
		bFirst = FALSE;
		//initcodec( buf , len , &m_pFormatCtx );		
		memcpy( pMemBuf , buf , len );
		if( av_open_input_file( &m_pFormatCtx , "femem://play.mem" , NULL , 0 , NULL ) < 0 )
			return;
	}
	else
	{

	}
	int genpts= m_pFormatCtx->flags & AVFMT_FLAG_GENPTS;
	AVPacket pkt1 , *pkt = &pkt1;
	for( ;; )
	{
		int ret = av_read_frame( m_pFormatCtx , pkt ); 
		if ( ret < 0 )
		{
			break;
		}
		av_free_packet(pkt);
	}
	
}

//解析内存
unsigned __stdcall readThread( void* arg )
{
	VideoState *is = (VideoState*)arg;
	AVFormatContext *ic = NULL;
	int err, i, ret;
	int st_index[AVMEDIA_TYPE_NB];
	AVPacket pkt1, *pkt = &pkt1;
	int eof=0;
	int pkt_in_play_range = 0;
	AVDictionaryEntry *t;
	AVDictionary **opts;
	int orig_nb_streams;

	memset(st_index, -1, sizeof(st_index));
	is->video_stream = -1;
	is->audio_stream = -1;
	is->subtitle_stream = -1;

	//avio_set_interrupt_cb(decode_interrupt_cb);

	err = avformat_open_input(&ic, is->filename , is->iformat, &format_opts);
	if (err < 0) {
		ret = -1;
		goto fail;
	}
	if ((t = av_dict_get(format_opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
		av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
		ret = AVERROR_OPTION_NOT_FOUND;
		goto fail;
	}
	is->ic = ic;

	if(genpts)
		ic->flags |= AVFMT_FLAG_GENPTS;

	av_dict_set(&codec_opts, "request_channels", "2", 0);

	opts = setup_find_stream_info_opts(ic, codec_opts);
	orig_nb_streams = ic->nb_streams;

	err = avformat_find_stream_info(ic, opts);
	if (err < 0) {
		fprintf(stderr, "%s: could not find codec parameters\n", is->filename);
		ret = -1;
		goto fail;
	}
	for (i = 0; i < orig_nb_streams; i++)
		av_dict_free(&opts[i]);
	av_freep(&opts);

	if(ic->pb)
		ic->pb->eof_reached= 0; //FIXME hack, ffplay maybe should not use url_feof() to test for the end

	if(seek_by_bytes<0)
		seek_by_bytes= !!(ic->iformat->flags & AVFMT_TS_DISCONT);

	//在这工程中永远不会使用ffmpeg 查找文件位置
	/* if seeking requested, we execute it */
	if (start_time != AV_NOPTS_VALUE) {
		int64_t timestamp;

		timestamp = start_time;
		/* add the stream start time */
		if (ic->start_time != AV_NOPTS_VALUE)
			timestamp += ic->start_time;
		ret = avformat_seek_file(ic, -1, INT64_MIN, timestamp, INT64_MAX, 0);
		if (ret < 0) {
			fprintf(stderr, "%s: could not seek to position %0.3f\n",
				is->filename, (double)timestamp / AV_TIME_BASE);
		}
	} //查换文件时间位置

	for (i = 0; i < ic->nb_streams; i++)
		ic->streams[i]->discard = AVDISCARD_ALL;
	if (!video_disable)
		st_index[AVMEDIA_TYPE_VIDEO] =
		av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO,
		wanted_stream[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);
	if (!audio_disable)
		st_index[AVMEDIA_TYPE_AUDIO] =
		av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO,
		wanted_stream[AVMEDIA_TYPE_AUDIO],
		st_index[AVMEDIA_TYPE_VIDEO],
		NULL, 0);
	if (!video_disable)
		st_index[AVMEDIA_TYPE_SUBTITLE] =
		av_find_best_stream(ic, AVMEDIA_TYPE_SUBTITLE,
		wanted_stream[AVMEDIA_TYPE_SUBTITLE],
		(st_index[AVMEDIA_TYPE_AUDIO] >= 0 ?
		st_index[AVMEDIA_TYPE_AUDIO] :
	st_index[AVMEDIA_TYPE_VIDEO]),
		NULL, 0);
	if (show_status) {
		av_dump_format(ic, 0, is->filename, 0);
	}

	//is->show_mode = show_mode;

	/* open the streams */
	if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
		stream_component_open(is, st_index[AVMEDIA_TYPE_AUDIO]);
	}

	ret=-1;
	if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
		ret= stream_component_open(is, st_index[AVMEDIA_TYPE_VIDEO]);
	}

	/*
	is->refresh_tid = SDL_CreateThread(refresh_thread, is);
	if (is->show_mode == SHOW_MODE_NONE)
		is->show_mode = ret >= 0 ? SHOW_MODE_VIDEO : SHOW_MODE_RDFT;
	*/

	if (st_index[AVMEDIA_TYPE_SUBTITLE] >= 0) {
		stream_component_open(is, st_index[AVMEDIA_TYPE_SUBTITLE]);
	}

	if (is->video_stream < 0 && is->audio_stream < 0) {
		ret = -1;
		goto fail;
	}

	//下面是分析解码数据包
	for( ;; )
	{
		if(eof) {
			//播放结束
			if(is->video_stream >= 0){
				av_init_packet(pkt);
				pkt->data=NULL;
				pkt->size=0;
				pkt->stream_index= is->video_stream;
				packet_queue_put(&is->videoq, pkt);
			}
			if (is->audio_stream >= 0 &&
				is->audio_st->codec->codec->capabilities & CODEC_CAP_DELAY) {
					av_init_packet(pkt);
					pkt->data = NULL;
					pkt->size = 0;
					pkt->stream_index = is->audio_stream;
					packet_queue_put(&is->audioq, pkt);
			}
			//播放完毕,退出线程
			ret=AVERROR_EOF;
			goto fail;
			//exit thread

			//SDL_Delay(10);
			if(is->audioq.size + is->videoq.size + is->subtitleq.size ==0){
				if(loop!=1 && (!loop || --loop)){
					//stream_seek(is, start_time != AV_NOPTS_VALUE ? start_time : 0, 0, 0);
				}else if(autoexit){
					ret=AVERROR_EOF;
					goto fail;
				}
			}
			eof=0;
			continue;
		}
		ret = av_read_frame( is->ic , pkt ); 
		if (ret < 0) {
			if (ret == AVERROR_EOF || url_feof(ic->pb))
				{
					eof=1; //必须为1
					continue;
			}
			if (ic->pb && ic->pb->error)
				{
					break;
			}
			//SDL_Delay(100); /* wait for user event */
			continue;
		}
		
		/* check if packet is in play range specified by user, then queue, otherwise discard */
		pkt_in_play_range = duration == AV_NOPTS_VALUE ||
			(pkt->pts - ic->streams[pkt->stream_index]->start_time) *
			av_q2d(ic->streams[pkt->stream_index]->time_base) -
			(double)(start_time != AV_NOPTS_VALUE ? start_time : 0)/1000000
			<= ((double)duration/1000000);
		if (pkt->stream_index == is->audio_stream && pkt_in_play_range) {
			packet_queue_put(&is->audioq, pkt);
		} else if (pkt->stream_index == is->video_stream && pkt_in_play_range) {
			packet_queue_put(&is->videoq, pkt);
		} else if (pkt->stream_index == is->subtitle_stream && pkt_in_play_range) {
			packet_queue_put(&is->subtitleq, pkt);
		} else {
			av_free_packet(pkt);
		}
	}

fail:	

	/* close each stream */
	if (is->audio_stream >= 0)
		stream_component_close(is, is->audio_stream);
	if (is->video_stream >= 0)
		stream_component_close(is, is->video_stream);
	if (is->subtitle_stream >= 0)
		stream_component_close(is, is->subtitle_stream);
	if (is->ic) {
		av_close_input_file(is->ic);
		is->ic = NULL; /* safety */
	}

	avio_set_interrupt_cb(NULL);

	return 0;
}

void CWrapFFMpeg::notify_new_launch( )
{
	char file[] = "femem://play.mem";
	flush_buf();	
	av_init_packet(&flush_pkt);
	flush_pkt.data= (uint8_t*)"FLUSH";
	m_pVideoState = (VideoState*)av_mallocz(sizeof(VideoState));
	VideoState* is = m_pVideoState;
	g_pVideoState = m_pVideoState;
	is->pictq_cond = CreateEvent( NULL , FALSE , FALSE , NULL );
	is->pictq_mutex = CreateMutex( NULL , FALSE , NULL );
	av_strlcpy( is->filename, file, strlen(file) );
	is->iformat = file_iformat;
	_beginthreadex( NULL , 0 , readThread , m_pVideoState , 0 , 0 );
	g_pNetSourceFilter->NoitfyStart();
}