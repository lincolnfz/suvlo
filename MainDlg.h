#pragma once

#include "bkuilib/bkwin/bkres.h"
#include "res.rc2"
#include "res.h"
#include "PlayWnd.h"
//#include "Wrapmms.h"
#include "feFilter/dsNetMedia/common.h"
#include <strmif.h>
#include <DShow.h>

class CMainDlg
	: public CBkDialogImpl<CMainDlg>
	, public CWHRoundRectFrameHelper<CMainDlg>
{
public:
	CMainDlg(void);
	virtual ~CMainDlg(void);

public:
	friend unsigned int __stdcall run( void* parm );
	BOOL Init();
	BOOL Clean();

	BK_NOTIFY_MAP(IDC_RICHVIEW_WIN)
		BK_NOTIFY_ID_COMMAND(IDC_BTN_SYS_CLOSE, OnClose)
		BK_NOTIFY_ID_COMMAND(IDC_BTN_SYS_MAX, OnMaxWindow)
		BK_NOTIFY_ID_COMMAND(IDC_BTN_SYS_MIN, OnMinWindow)
		BK_NOTIFY_ID_COMMAND(IDC_BTN_PLAY_PAUSE, OnPlay_Pause)
		BK_NOTIFY_ID_COMMAND(IDC_BTN_STOP, OnStop)
		BK_NOTIFY_ID_COMMAND(IDC_BTN_MUTE, OnMute)
		BK_NOTIFY_ID_COMMAND(IDC_VOL , OnVol )
	BK_NOTIFY_MAP_END()

	BEGIN_MSG_MAP_EX(CMainDlg)
		MSG_BK_NOTIFY(IDC_RICHVIEW_WIN)
		CHAIN_MSG_MAP(CBkDialogImpl<CMainDlg>)
		CHAIN_MSG_MAP(CWHRoundRectFrameHelper<CMainDlg>)
		REFLECT_NOTIFICATIONS_EX()
		MSG_WM_KEYUP(OnKeyUp)
		MSG_WM_INITDIALOG(OnInitDialog)
		MESSAGE_HANDLER_EX( WM_TIMEPOS , OnUpdatePosition )
		MESSAGE_HANDLER_EX( BK_VALUE_CHANGE , OnValChange )
	END_MSG_MAP()

		LRESULT OnClose();
		LRESULT OnMaxWindow();
		LRESULT OnMinWindow();
		LRESULT OnPlay_Pause();
		LRESULT OnStop();
		LRESULT OnMute();
		LRESULT OnVol();
		void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
		BOOL OnInitDialog(CWindow wndFocus, LPARAM lInitParam);
		void play( const char* );
		LRESULT OnUpdatePosition(UINT uMsg, WPARAM wParam, LPARAM lParam);
		LRESULT OnValChange(UINT uMsg, WPARAM wParam, LPARAM lParam);

protected:
	HINSTANCE m_hdll_source;
	//IGraphBuilder *m_pGraphBuilder;
	IFilterGraph *m_pFilterGraph;

public:
	CPlayWnd m_playwnd;
	//CWrapmms m_wrapmms;
	//int64_t  m_llLength;
};

extern CAppModule _Module;
