
#include <assert.h>
#include <process.h>
#include <crtdbg.h>
#include "FeFFmpeg.h"

#pragma comment( lib , "../../lib/avcodec.lib" )
#pragma comment( lib , "../../lib/avdevice.lib" )
#pragma comment( lib , "../../lib/avfilter.lib" )
#pragma comment( lib , "../../lib/avformat.lib" )
#pragma comment( lib , "../../lib/avutil.lib" )
#pragma comment( lib , "../../lib/swscale.lib" )

const int UNITQUEUE = 5;
const long UNITSIZE = 30;

CFeFFmpeg *g_pFeFFmpeg = NULL;
CFeFFmpeg* CFeFFmpeg::m_pFeFFmpeg = NULL;

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
static int64_t start_time = AV_NOPTS_VALUE;
static int64_t duration = AV_NOPTS_VALUE;

int fe_mem_open(URLContext *h, const char *url, int flags)
{
	h->priv_data = g_pFeFFmpeg;
	return 0;
}

int fe_mem_read(URLContext *h, unsigned char *buf, int size)
{
	CFeFFmpeg* pFFMpeg = (CFeFFmpeg*)h->priv_data;
	if ( !pFFMpeg )
	{
		return 0;
	}
	return pFFMpeg->ReadBuf( (char*)buf , size );
}

int fe_mem_get_handle(URLContext *h)
{
	return (intptr_t) h->priv_data;
}

URLProtocol fe_memProtocol;

void fe_init_new_protocol()
{
	memset( &fe_memProtocol , 0 , sizeof(fe_memProtocol) );
	fe_memProtocol.name = "femem";
	fe_memProtocol.url_open = fe_mem_open;
	fe_memProtocol.url_read = fe_mem_read;
	fe_memProtocol.url_get_file_handle = fe_mem_get_handle;
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

AVDictionary *filter_codec_opts(AVDictionary *opts, AVCodec *codec, AVFormatContext *s, AVStream *st)
{
	AVDictionary    *ret = NULL;
	AVDictionaryEntry *t = NULL;
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
	*opts = (AVDictionary *)av_mallocz(s->nb_streams * sizeof(*opts));
	if (!opts) {
		av_log(NULL, AV_LOG_ERROR, "Could not alloc memory for stream options.\n");
		return NULL;
	}
	for (i = 0; i < s->nb_streams; i++)
		opts[i] = filter_codec_opts(codec_opts, avcodec_find_decoder(s->streams[i]->codec->codec_id), s, s->streams[i]);
	return opts;
}


CFeFFmpeg* CFeFFmpeg::GetInstance( UNIT_BUF_POOL *pool , CObjPool<AVPicture>* picpool )
{
	if ( CFeFFmpeg::m_pFeFFmpeg == NULL )
	{		
		CFeFFmpeg::m_pFeFFmpeg = new CFeFFmpeg(pool,picpool);
		g_pFeFFmpeg = CFeFFmpeg::m_pFeFFmpeg;
	}
	return CFeFFmpeg::m_pFeFFmpeg;
}


int CFeFFmpeg::Destory()
{
	int ret = 0;
	if ( CFeFFmpeg::m_pFeFFmpeg )
	{
		delete CFeFFmpeg::m_pFeFFmpeg;
		CFeFFmpeg::m_pFeFFmpeg = NULL;
		g_pFeFFmpeg = CFeFFmpeg::m_pFeFFmpeg;
		ret = 1;
	}
	return ret;
}

CFeFFmpeg::CFeFFmpeg( UNIT_BUF_POOL *pool ,  CObjPool<AVPicture>* picpool ) : m_videopool(UNITQUEUE,UNITSIZE) ,
	m_audiopool(UNITQUEUE,UNITSIZE) , m_pbufpool(pool) , m_picpool(picpool)
{
	av_register_all();
	avcodec_init();
	avcodec_register_all();
	avdevice_register_all();
	avfilter_register_all();

	//register one new protocol
	fe_init_new_protocol();
	av_register_protocol2( &fe_memProtocol , sizeof(fe_memProtocol) );

	av_init_packet(&m_flush_pkt);
	m_flush_pkt.data = (uint8_t*)"FLUSH";
}

CFeFFmpeg::~CFeFFmpeg()
{
	
}

int CFeFFmpeg::ReadBuf(char* destbuf , int len)
{
	return ReadData( m_pbufpool , destbuf , len );
}

void CFeFFmpeg::InitPacketPool( CObjPool<AVPacket> *pool )
{	
	PutDataPool( pool , &m_flush_pkt );
}

//void CFeFFmpeg::PutPacketPool( CObjPool<AVPacket> *pool , AVPacket *pkt )
//{
//	AVPacket *pack = pool->GetOneUnit( CObjPool<AVPacket>::OPCMD::WRITE_DATA );
//	*pack = *pkt;
//	pool->CommitOneUnit( pack , CObjPool<AVPacket>::OPCMD::WRITE_DATA );
//}

int CFeFFmpeg::Start()
{
	char file[] = "femem://play.mem";
	av_strlcpy( m_filenam , file , strlen(file) );
	int i = 0;
	int t = sizeof(m_wanted_stream);
	for ( i=0 ; i < t ; ++i )
	{
		m_wanted_stream[i] = -1;
	}
	m_video_disable = 0;
	m_audio_disable = 0;
	m_show_status = 0;
	_beginthreadex( NULL , 0 , ReadThread ,  this , 0 , 0);
	
	return 0;
}

int CFeFFmpeg::stream_component_open( int stream_index )
{
	AVFormatContext *ic = m_ic;
	AVCodecContext *avctx;
	AVCodec *codec;
	AVDictionary *opts;
	AVDictionaryEntry *t = NULL;
	int64_t wanted_channel_layout = 0;

	if (stream_index < 0 || stream_index >= ic->nb_streams)
		return -1;
	avctx = ic->streams[stream_index]->codec;
	codec = avcodec_find_decoder(avctx->codec_id);
	opts = filter_codec_opts(m_codec_opts, codec, ic, ic->streams[stream_index]);

	if (!codec)
		return -1;

	avctx->workaround_bugs = workaround_bugs;
	avctx->lowres = lowres;
	if(avctx->lowres > codec->max_lowres){
		av_log(avctx, AV_LOG_WARNING, "The maximum value for lowres supported by the decoder is %d\n",
			codec->max_lowres);
		avctx->lowres= codec->max_lowres;
	}
	if(avctx->lowres) avctx->flags |= CODEC_FLAG_EMU_EDGE;
	avctx->idct_algo= idct;
	if(fast) avctx->flags2 |= CODEC_FLAG2_FAST;
	avctx->skip_frame= skip_frame;
	avctx->skip_idct= skip_idct;
	avctx->skip_loop_filter= skip_loop_filter;
	avctx->error_recognition= error_recognition;
	avctx->error_concealment= error_concealment;

	if(codec->capabilities & CODEC_CAP_DR1)
		avctx->flags |= CODEC_FLAG_EMU_EDGE;

	if (!codec ||
		avcodec_open2(avctx, codec, &opts) < 0)
		return -1;
	if ((t = av_dict_get(opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
		av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
		return AVERROR_OPTION_NOT_FOUND;
	}

	ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
	switch(avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        {
			m_audio_stream = stream_index;
			m_audio_st = ic->streams[stream_index];
			InitPacketPool( &m_audiopool );
			_beginthreadex( NULL , 0 , DecodeAudioThread ,  this , 0 , 0);
		}
        break;
    case AVMEDIA_TYPE_VIDEO:
		{
			m_video_stream = stream_index;
			m_video_st = ic->streams[stream_index];
			InitPacketPool( &m_videopool );
			_beginthreadex( NULL , 0 , DecodeVideoThread ,  this , 0 , 0);
		}
        break;
    case AVMEDIA_TYPE_SUBTITLE:
		{
			m_subtitle_stream = stream_index;
			m_subtitle_st = ic->streams[stream_index];

		}
        break;
    default:
        break;
    }

	return 0;
}

int CFeFFmpeg::stream_component_close(int stream_index)
{
	AVFormatContext *ic = m_ic;
	AVCodecContext *avctx;

	if (stream_index < 0 || stream_index >= ic->nb_streams)
		return 0;
	avctx = ic->streams[stream_index]->codec;
	switch(avctx->codec_type) {
	case AVMEDIA_TYPE_AUDIO:
		{

		}
		break;
	case AVMEDIA_TYPE_VIDEO:
		{

		}
		break;
	case AVMEDIA_TYPE_SUBTITLE:
		{

		}
		break;
	}

	ic->streams[stream_index]->discard = AVDISCARD_ALL;
	avcodec_close(avctx);
	switch(avctx->codec_type) {
	case AVMEDIA_TYPE_AUDIO:
		m_audio_st = NULL;
		m_audio_stream = -1;
		break;
	case AVMEDIA_TYPE_VIDEO:
		m_video_st = NULL;
		m_video_stream = -1;
		break;
	case AVMEDIA_TYPE_SUBTITLE:
		m_subtitle_st = NULL;
		m_subtitle_stream = -1;
		break;
	default:
		break;
	}

	return 0;
}

unsigned int CFeFFmpeg::ReadThread( void *avg )
{
	CFeFFmpeg *pFeFFmpeg = (CFeFFmpeg*)avg;
	pFeFFmpeg->DoProcessingLoop();

	return 0;
}

int CFeFFmpeg::DoProcessingLoop()
{
	AVFormatContext *ic = NULL;
	m_img_convert_ctx = NULL;
	int err, i, ret;
	int st_index[AVMEDIA_TYPE_NB];
	AVPacket pkt1, *pkt = &pkt1;
	int eof=0;
	int pkt_in_play_range = 0;
	AVDictionaryEntry *t;
	AVDictionary **opts;
	int orig_nb_streams;

	memset(st_index, -1, sizeof(st_index));
	m_video_stream = -1;
	m_audio_stream = -1;
	m_subtitle_stream = -1;
	err = avformat_open_input(&ic, m_filenam , NULL , NULL);
	if (err < 0) {
		ret = -1;
		goto fail;
	}
	m_ic = ic;
	if(genpts)
		ic->flags |= AVFMT_FLAG_GENPTS;

	av_dict_set( &m_codec_opts, "request_channels", "2", 0);
	opts = setup_find_stream_info_opts(ic, m_codec_opts);
	orig_nb_streams = ic->nb_streams;

	err = avformat_find_stream_info(ic, opts);
	if (err < 0) {
		fprintf(stderr, "%s: could not find codec parameters\n", m_filenam);
		ret = -1;
		goto fail;
	}
	for (i = 0; i < orig_nb_streams; i++)
		av_dict_free(&opts[i]);
	av_freep(&opts);

	if(ic->pb)
		ic->pb->eof_reached= 0; //FIXME hack, ffplay maybe should not use url_feof() to test for the end

	//查找文件的流数据格式
	for (i = 0; i < ic->nb_streams; i++)
		ic->streams[i]->discard = AVDISCARD_ALL;

	if (!m_video_disable)
		//查找视频流
		st_index[AVMEDIA_TYPE_VIDEO] =
		av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO,
		m_wanted_stream[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);
	if (!m_audio_disable)
		//查找音频流
		st_index[AVMEDIA_TYPE_AUDIO] =
		av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO,
		m_wanted_stream[AVMEDIA_TYPE_AUDIO],
		st_index[AVMEDIA_TYPE_VIDEO],
		NULL, 0);
	if (!m_video_disable)
		//查找字幕
		st_index[AVMEDIA_TYPE_SUBTITLE] =
		av_find_best_stream(ic, AVMEDIA_TYPE_SUBTITLE,
		m_wanted_stream[AVMEDIA_TYPE_SUBTITLE],
		(st_index[AVMEDIA_TYPE_AUDIO] >= 0 ?
		st_index[AVMEDIA_TYPE_AUDIO] :
	st_index[AVMEDIA_TYPE_VIDEO]),
		NULL, 0);
	if ( m_show_status) {
		av_dump_format(ic, 0, m_filenam , 0);
	}

	//////////////////////////////////////////////////////////////////////////
	//打开各种流
	/* open the streams */
	if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
		ret = stream_component_open(st_index[AVMEDIA_TYPE_AUDIO]);
	}

	if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
		ret= stream_component_open(st_index[AVMEDIA_TYPE_VIDEO]);
	}

	if (st_index[AVMEDIA_TYPE_SUBTITLE] >= 0) {
		ret = stream_component_open(st_index[AVMEDIA_TYPE_SUBTITLE]);
	}

	if (m_video_stream < 0 && m_audio_stream < 0) {
		ret = -1;
		goto fail;
	}

	for (;;)
	{
		if (eof)
		{
			//播放结束
			if ( m_video_stream >= 0 )
			{
				//video
				av_init_packet(pkt);
				pkt->data=NULL;
				pkt->size=0;
				pkt->stream_index = m_video_stream;
				PutDataPool( &m_videopool , pkt );
			}
			if ( m_audio_stream >= 0 && m_audio_st->codec->codec->capabilities & CODEC_CAP_DELAY )
			{
				//audio
				av_init_packet(pkt);
				pkt->data=NULL;
				pkt->size=0;
				pkt->stream_index = m_audio_stream;
				PutDataPool( &m_audiopool , pkt );
			}
			eof = 0;
			continue;
		}
		ret = av_read_frame(ic, pkt);
		if (ret < 0) {
			if (ret == AVERROR_EOF || url_feof(ic->pb))
				eof=1;
			if (ic->pb && ic->pb->error)
				break;
			//SDL_Delay(100); /* wait for user event */
			continue;
		}
		/* check if packet is in play range specified by user, then queue, otherwise discard */
		pkt_in_play_range = duration == AV_NOPTS_VALUE ||
			(pkt->pts - ic->streams[pkt->stream_index]->start_time) *
			av_q2d(ic->streams[pkt->stream_index]->time_base) -
			(double)(start_time != AV_NOPTS_VALUE ? start_time : 0)/1000000
			<= ((double)duration/1000000);

		if (pkt->stream_index == m_audio_stream && pkt_in_play_range) {
			PutDataPool( &m_audiopool , pkt );
		} else if (pkt->stream_index == m_video_stream && pkt_in_play_range) {
			PutDataPool( &m_videopool , pkt );
		} else if (pkt->stream_index == m_subtitle_stream && pkt_in_play_range) {
			//暂不支持播放字幕
			av_free_packet(pkt);
		} else {
			av_free_packet(pkt);
		}
	}

	/* wait until the end */
	//while (!is->abort_request) {
	//	SDL_Delay(100);
	//}

	ret = 0;
fail:
	/* close each stream */
	if (m_audio_stream >= 0)
		stream_component_close(m_audio_stream);
	if (m_video_stream >= 0)
		stream_component_close(m_video_stream);
	if (m_subtitle_stream >= 0)
		stream_component_close(m_subtitle_stream);
	if (ic) {
		av_close_input_file(ic);
	}

	if (m_img_convert_ctx)
		sws_freeContext(m_img_convert_ctx);

	return 0;
}

unsigned int __stdcall CFeFFmpeg::DecodeVideoThread( void *avg )
{
	CFeFFmpeg *pFeFFmpeg = (CFeFFmpeg*)avg;
	pFeFFmpeg->DoVideoDecodeLoop();
	return 0;
}

unsigned int __stdcall CFeFFmpeg::DecodeAudioThread( void *avg )
{
	CFeFFmpeg *pFeFFmpeg = (CFeFFmpeg*)avg;
	pFeFFmpeg->DoAudioDecodeLoop();
	return 0;
}

int CFeFFmpeg::DoVideoDecodeLoop()
{
	AVFrame *frame= avcodec_alloc_frame();
	int ret = 0;
	for (;;)
	{
		ret = GetVideoFrame( frame );
		if( ret < 0 ) goto end;
	}

end:
	av_free( frame );
	return 0;
}



int CFeFFmpeg::GetVideoFrame( AVFrame *frame  )
{
	int got_picture;
	AVPacket*pkt = m_videopool.GetOneUnit( CObjPool<AVPacket>::OPCMD::READ_DATA );
	if ( pkt == NULL )
	{
		return -1;
	}
	if ( IsFlushPacket( pkt ) )
	{
		//here flush pool data
		return 0;
	}

	//得到数据放在frame中
	avcodec_decode_video2(m_video_st->codec, frame, &got_picture, pkt);
	av_free_packet( pkt );
	m_videopool.CommitOneUnit( pkt , CObjPool<AVPacket>::OPCMD::READ_DATA );

	if ( got_picture )
	{
		int ret = 1;
		return ret;
	}
	//ImgCover()

	return 0;
}

int CFeFFmpeg::ImgCover( SwsContext **ctx , AVFrame *pFrame , AVPicture* pict, 
	int widthSrc , int heightSrc , PixelFormat pixFmtSrc , 
	int widthDst , int heightDst , PixelFormat pixFmtDst )
{
	int ret = 0;
	*ctx = sws_getCachedContext( *ctx ,
		widthSrc ,
		heightSrc ,
		pixFmtSrc , 
		widthDst ,
		heightDst ,
		pixFmtDst ,  SWS_BICUBIC , NULL , NULL , NULL );

	if ( *ctx )
	{
		ret = avpicture_alloc( pict , pixFmtDst , 
				widthDst , 
				heightDst);
		//倒置颠倒的图像
		/*
		pAVFrame->data[0] = pAVFrame->data[0]+pAVFrame->linesize[0]*( g_pVideoState->video_st->codec->height-1 );
		pAVFrame->data[1] = pAVFrame->data[1]+pAVFrame->linesize[0]*g_pVideoState->video_st->codec->height/4-1;  
		pAVFrame->data[2] = pAVFrame->data[2]+pAVFrame->linesize[0]*g_pVideoState->video_st->codec->height/4-1; 
		pAVFrame->linesize[0] *= -1;
		pAVFrame->linesize[1] *= -1;  
		pAVFrame->linesize[2] *= -1;
		*/
		//倒置颠倒的图像 ok
				
		//转换成rgb
		if ( !ret && sws_scale( *ctx , pFrame->data , pFrame->linesize , 0 , heightSrc , pict->data , pict->linesize ) )
		{
			ret = 1;
		}else{
			ret = -1;
		}
				
	}

	return ret;
}

int CFeFFmpeg::DoAudioDecodeLoop()
{
	AVFrame *frame= avcodec_alloc_frame();
	int ret = 0;
	for (;;)
	{
		GetAudioFrame( frame );
		if( ret < 0 ) goto end;
	}

end:
	av_free( frame );
	return 0;
}

int CFeFFmpeg::GetAudioFrame(AVFrame *frame)
{
	AVPacket*pkt = m_audiopool.GetOneUnit( CObjPool<AVPacket>::OPCMD::READ_DATA );

	m_audiopool.CommitOneUnit( pkt , CObjPool<AVPacket>::OPCMD::READ_DATA );
	return 0;
}

