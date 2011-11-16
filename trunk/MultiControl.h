#pragma once
#include <DShow.h>

class CMultiControl
{
public:
	CMultiControl(void);
	~CMultiControl(void);

	BOOL Init();
	BOOL Destory();

protected:
	IGraphBuilder* m_pGraph;
	IMediaControl* m_pMediaControl;
	IMediaEvent* m_pMediaEvent;

};

