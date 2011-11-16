/************************************************************************
    This file is part of VLCWrapper.
    
    File:    CVlcDialogDlg.h
    Desc.:   CVlcDialogDlg - A simple dialog based media player application.

    Author:  Alex Skoruppa
    Date:    08/10/2009
	Updated: 09/11/2010
    eM@il:   alex.skoruppa@googlemail.com

    VLCWrapper is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
     
    VLCWrapper is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.
     
    You should have received a copy of the GNU General Public License
    along with VLCWrapper.  If not, see <http://www.gnu.org/licenses/>.
************************************************************************/
#pragma once
#include "afxwin.h"
#include <afxtempl.h>
#include "VLCWrapper.h"

class CVlcDialogDlg : public CDialog
{
    VLCWrapper          m_VLCPlayer;
    bool                m_bMuteFlag;
    int64_t             m_llLength;    
    CList<CWnd*, CWnd*> m_listDlgItems;
    CSize               m_CurrentDlgSize;
    CSize               m_MinDlgSize;
    HICON               m_NoMute;
    HICON               m_Mute;
    bool                m_bCreated;

    //{{AFX_DATA(CVlcDialogDlg)
    CButton     m_MediaControlGroup;
    CButton     m_buttonPlay;
    CButton     m_buttonPause;
    CButton     m_buttonStop;
    CButton     m_buttonMute;
    CButton     m_buttonOpen;
    CStatic     m_VLC;
    CStatic     m_VolumeText;
    CStatic     m_VolumeLevel;
    CStatic     m_MediaPosition;
    CSliderCtrl m_MediaSlider;
    CSliderCtrl m_VolumeSlider;
    //}}AFX_DATA

public:
	CVlcDialogDlg(CWnd* pParent = NULL);
	~CVlcDialogDlg();

    void UpdatePosition();

	enum { IDD = IDD_VLCDIALOG_DIALOG };

protected:
    //{{AFX_VIRTUAL(CButtonDialog)
	virtual void DoDataExchange(CDataExchange* pDX);
    //}}AFX_VIRTUAL
	virtual void OnOK();
	HICON m_hIcon;

    //{{AFX_MSG(CVlcDialogDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo( MINMAXINFO FAR* lpMMI );
	//}}AFX_MSG
	afx_msg void OnBnClickedButtonPlay();
	afx_msg void OnBnClickedButtonStop();
	afx_msg void OnBnClickedButtonPause();
    afx_msg void OnBnClickedButtonMute();
	afx_msg void OnBnClickedButtonLoad();        
	DECLARE_MESSAGE_MAP()
    
    void RecalcLayout(int cx, int cy);
};
