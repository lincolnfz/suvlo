#ifndef _COMMON_H
#define _COMMON_H
#include   "objbase.h "
#include   "initguid.h " 
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

typedef struct _tagAVFrameNode{
	AVFrame *avframe;
	struct _tagAVFrameNode *next;
}AVFrameNode;

typedef struct _tagAVFrameLink{
	AVFrameNode *nodehead;
	AVFrameNode *nodelast;
	int nb_size;
	void *hMutex;
	void *hEvent;
}AVFrameLink;

int initAVFrameLink( AVFrameLink **avframelink );

int putAVFrameLink( AVFrameLink *avframelink , AVFrame* avframe);

AVFrame* getAVFrameLink( AVFrameLink *avframelink );

int flushAVFrameLink( AVFrameLink *avframelink );

int destoryAVFrameLink( AVFrameLink **avframelink );

class CFeLockMutex{
public:
	CFeLockMutex( void*& mutex ):m_Mutex(mutex){
		WaitForSingleObject( m_Mutex , INFINITE );
	}
	~CFeLockMutex(){
		ReleaseMutex( m_Mutex );
	}
private:
	void*& m_Mutex;
};

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