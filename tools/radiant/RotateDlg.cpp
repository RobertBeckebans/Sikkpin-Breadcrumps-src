/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).  

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "../../idlib/precompiled.h"
#pragma hdrstop

#include "qe3.h"
#include "Radiant.h"
#include "RotateDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CRotateDlg dialog

CRotateDlg g_dlgRotate;	// sikk - New rotate dialog functionality

CRotateDlg::CRotateDlg( CWnd* pParent /*=NULL*/ )
	: CDialog( CRotateDlg::IDD, pParent ) {
	//{{AFX_DATA_INIT(CRotateDlg)
// ---> sikk - New rotate dialog functionality
	m_nRotX = m_nRotY = m_nRotZ = 0;
	m_nOldRotX = m_nOldRotY = m_nOldRotZ = 0;
// <--- sikk - New rotate dialog functionality
	//}}AFX_DATA_INIT
}

void CRotateDlg::DoDataExchange( CDataExchange* pDX ) {
	CDialog::DoDataExchange( pDX );
	//{{AFX_DATA_MAP(CRotateDlg)
// ---> sikk - New rotate dialog functionality
	DDX_Control( pDX, IDC_ROTX, m_wndEditRotX );
	DDX_Control( pDX, IDC_ROTY, m_wndEditRotY );
	DDX_Control( pDX, IDC_ROTZ, m_wndEditRotZ );
	DDX_Control( pDX, IDC_SLIDER_ROTX, m_wndRotX );
	DDX_Control( pDX, IDC_SLIDER_ROTY, m_wndRotY );
	DDX_Control( pDX, IDC_SLIDER_ROTZ, m_wndRotZ );
	DDX_Text( pDX, IDC_ROTX, m_nRotX );
	DDX_Text( pDX, IDC_ROTY, m_nRotY );
	DDX_Text( pDX, IDC_ROTZ, m_nRotZ );
	DDV_MinMaxInt( pDX, m_nRotX, -359, 359 );
	DDV_MinMaxInt( pDX, m_nRotY, -359, 359 );
	DDV_MinMaxInt( pDX, m_nRotZ, -359, 359 );
// <--- sikk - New rotate dialog functionality
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP( CRotateDlg, CDialog )
	//{{AFX_MSG_MAP(CRotateDlg)
// ---> sikk - New rotate dialog functionality
	ON_WM_HSCROLL()
// <--- sikk - New rotate dialog functionality
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CRotateDlg message handlers

HWND	g_rotatewin = NULL;	// sikk - New rotate dialog functionality

BOOL CRotateDlg::OnInitDialog( void ) {
	CDialog::OnInitDialog();

// ---> sikk - New rotate dialog functionality
	g_rotatewin = GetSafeHwnd();
	m_wndRotX.SetRange( -359, 359 );
	m_wndRotX.SetBuddy( &m_wndEditRotX, FALSE );
	m_wndRotX.SetPos( m_nRotX );
	m_wndRotY.SetRange( -359, 359 );
	m_wndRotY.SetBuddy( &m_wndEditRotY, FALSE );
	m_wndRotY.SetPos( m_nRotY );
	m_wndRotZ.SetRange( -359, 359 );
	m_wndRotZ.SetBuddy( &m_wndEditRotZ, FALSE );
	m_wndRotZ.SetPos( m_nRotZ );
// <--- sikk - New rotate dialog functionality

	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

// ---> sikk - New rotate dialog functionality
void CRotateDlg::OnHScroll( UINT nSBCode, UINT nPos, CScrollBar* pScrollBar ) {
	UpdateData( TRUE );
	if ( pScrollBar->IsKindOf( RUNTIME_CLASS( CSliderCtrl ) ) ) {
		CSliderCtrl *ctrl = reinterpret_cast<CSliderCtrl*>( pScrollBar );
		assert( ctrl );
		if ( ctrl == &m_wndRotX ) {
			m_nOldRotX = m_nRotX;
			m_nRotX = ctrl->GetPos();
			Select_RotateAxis( 0, (float)( m_nRotX - m_nOldRotX ) );
		} else if ( ctrl == &m_wndRotY ) {
			m_nOldRotY = m_nRotY;
			m_nRotY = ctrl->GetPos();
			Select_RotateAxis( 1, (float)( m_nRotY - m_nOldRotY ) );
		} else {
			m_nOldRotZ = m_nRotZ;
			m_nRotZ = ctrl->GetPos();
			Select_RotateAxis( 2, (float)( m_nRotZ - m_nOldRotZ ) );
		}		
		UpdateData( FALSE );
	}
	Sys_UpdateWindows( W_CAMERA | W_XY );
}

void DoRotate( void ) {
	if ( g_rotatewin == NULL && g_dlgRotate.GetSafeHwnd() == NULL ) {
		g_dlgRotate.Create( IDD_ROTATE );
		CRect rct;
		LONG lSize = sizeof( rct );
		//if ( LoadRegistryInfo( "radiant_SurfaceWindow", &rct, &lSize ) )  {
		//	g_dlgRotate.SetWindowPos( NULL, rct.left, rct.top, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW );
		//}
		g_dlgRotate.ShowWindow( SW_SHOW );
		Sys_UpdateWindows( W_ALL );
	} else {
		g_rotatewin = g_dlgRotate.GetSafeHwnd();
		//g_dlgSurface.SetTexMods ();
		g_dlgRotate.ShowWindow( SW_SHOW );
	}
}		

// <--- sikk - New rotate dialog functionality
