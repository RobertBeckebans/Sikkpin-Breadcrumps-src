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
#include "ZWnd.h"

#ifdef _DEBUG
	#define new DEBUG_NEW
	#undef THIS_FILE
	static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CZWnd
IMPLEMENT_DYNCREATE( CZWnd, CWnd );

CZWnd::CZWnd()
{
	m_pZClip = NULL;
	Z_Init();	// sikk - Moved z.h/z.cpp contents into zwnd.h/zwnd.cpp
}

CZWnd::~CZWnd()
{
}

BEGIN_MESSAGE_MAP( CZWnd, CWnd )
	//{{AFX_MSG_MAP(CZWnd)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_KEYDOWN()
	ON_WM_LBUTTONDOWN()
	ON_WM_MBUTTONDOWN()
	ON_WM_RBUTTONDOWN()
	ON_WM_PAINT()
	ON_WM_GETMINMAXINFO()
	ON_WM_MOUSEMOVE()
	ON_WM_SIZE()
// ---> sikk - Window Snapping
	ON_WM_SIZING()
	ON_WM_MOVING()
// <--- sikk - Window Snapping
	ON_WM_NCCALCSIZE()
	ON_WM_KILLFOCUS()
	ON_WM_SETFOCUS()
	ON_WM_CLOSE()
	ON_WM_LBUTTONUP()
	ON_WM_MBUTTONUP()
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEWHEEL()
	ON_WM_KEYUP()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CZWnd message handlers

int CZWnd::OnCreate( LPCREATESTRUCT lpCreateStruct )
{
	if( CWnd::OnCreate( lpCreateStruct ) == -1 )
	{
		return -1;
	}

	m_dcZ = ::GetDC( GetSafeHwnd() );
	QEW_SetupPixelFormat( m_dcZ, false );

	m_pZClip = new CZClip();

	return 0;
}

void CZWnd::OnDestroy()
{
	if( m_pZClip )
	{
		delete m_pZClip;
		m_pZClip = NULL;
	}

	CWnd::OnDestroy();
}

void CZWnd::OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags )
{
	g_pParentWnd->HandleKey( nChar, nRepCnt, nFlags );
}

void CZWnd::OnKeyUp( UINT nChar, UINT nRepCnt, UINT nFlags )
{
	g_pParentWnd->HandleKey( nChar, nRepCnt, nFlags, false );
}

void CZWnd::OnLButtonDown( UINT nFlags, CPoint point )
{
	SetFocus();
	SetCapture();
	CRect rctZ;
	GetClientRect( rctZ );
// ---> sikk - Bring window to front
	if( g_pParentWnd->GetTopWindow() != this )
	{
		BringWindowToTop();
	}
// <--- sikk - Bring window to front
	Z_MouseDown( point.x, rctZ.Height() - 1 - point.y, nFlags );
}

void CZWnd::OnMButtonDown( UINT nFlags, CPoint point )
{
	SetFocus();
	SetCapture();
	CRect rctZ;
	GetClientRect( rctZ );
// ---> sikk - Bring window to front
	if( g_pParentWnd->GetTopWindow() != this )
	{
		BringWindowToTop();
	}
// <--- sikk - Bring window to front
	Z_MouseDown( point.x, rctZ.Height() - 1 - point.y, nFlags );
}

void CZWnd::OnRButtonDown( UINT nFlags, CPoint point )
{
	SetFocus();
	SetCapture();
	CRect rctZ;
	GetClientRect( rctZ );
// ---> sikk - Bring window to front
	if( g_pParentWnd->GetTopWindow() != this )
	{
		BringWindowToTop();
	}
// <--- sikk - Bring window to front
	Z_MouseDown( point.x, rctZ.Height() - 1 - point.y, nFlags );
}

void CZWnd::OnPaint()
{
	CPaintDC dc( this ); // device context for painting
	//if (!wglMakeCurrent(m_dcZ, m_hglrcZ))
	//if (!qwglMakeCurrent(dc.m_hDC, m_hglrcZ))
	if( !qwglMakeCurrent( dc.m_hDC, win32.hGLRC ) )
	{
		common->Printf( "ERROR: wglMakeCurrent failed..\n " );
		common->Printf( "Please restart " EDITOR_WINDOWTEXT " if the Z view is not working\n" );
	}
	else
	{
		QE_CheckOpenGLForErrors();

		Z_Draw();
		//qwglSwapBuffers( m_dcZ );
		qwglSwapBuffers( dc.m_hDC );
		TRACE( "Z Paint\n" );
	}
}

void CZWnd::OnGetMinMaxInfo( MINMAXINFO FAR* lpMMI )
{
	lpMMI->ptMinTrackSize.x = ZWIN_WIDTH;
}

void CZWnd::OnMouseMove( UINT nFlags, CPoint point )
{
	CRect rctZ;
	GetClientRect( rctZ );
	float fz = z.origin[2] + ( ( rctZ.Height() - 1 - point.y ) - ( z.height / 2 ) ) / z.scale;
	fz = floor( fz / g_qeglobals.d_gridsize + 0.5 ) * g_qeglobals.d_gridsize;
	CString strStatus;
	strStatus.Format( "Z:: %.1f", fz );
	g_pParentWnd->SetStatusText( 1, strStatus );
	Z_MouseMoved( point.x, rctZ.Height() - 1 - point.y, nFlags );
}

void CZWnd::OnSize( UINT nType, int cx, int cy )
{
	CWnd::OnSize( nType, cx, cy );
	CRect rctZ;
	GetClientRect( rctZ );
	z.width = rctZ.right;
	z.height = rctZ.bottom;
	if( z.width < 10 )
	{
		z.width = 10;
	}
	if( z.height < 10 )
	{
		z.height = 10;
	}
	Invalidate();
}

// ---> sikk - Window Snapping
void CZWnd::OnSizing( UINT nSide, LPRECT lpRect )
{
	if( TryDocking( GetSafeHwnd(), nSide, lpRect, 0 ) )
	{
		return;
	}
}
void CZWnd::OnMoving( UINT nSide, LPRECT lpRect )
{
	if( TryDocking( GetSafeHwnd(), nSide, lpRect, 0 ) )
	{
		return;
	}
}
// <--- sikk - Window Snapping

void CZWnd::OnNcCalcSize( BOOL bCalcValidRects, NCCALCSIZE_PARAMS FAR* lpncsp )
{
	CWnd::OnNcCalcSize( bCalcValidRects, lpncsp );
}

void CZWnd::OnKillFocus( CWnd* pNewWnd )
{
	CWnd::OnKillFocus( pNewWnd );
	SendMessage( WM_NCACTIVATE, FALSE , 0 );
}

void CZWnd::OnSetFocus( CWnd* pOldWnd )
{
	CWnd::OnSetFocus( pOldWnd );
	SendMessage( WM_NCACTIVATE, TRUE , 0 );
}

void CZWnd::OnClose()
{
	CWnd::OnClose();
}

void CZWnd::OnLButtonUp( UINT nFlags, CPoint point )
{
	CRect rctZ;
	GetClientRect( rctZ );
	Z_MouseUp( point.x, rctZ.bottom - 1 - point.y, nFlags );
	if( !( nFlags & ( MK_LBUTTON | MK_RBUTTON | MK_MBUTTON ) ) )
	{
		ReleaseCapture();
	}
}

void CZWnd::OnMButtonUp( UINT nFlags, CPoint point )
{
	CRect rctZ;
	GetClientRect( rctZ );
	Z_MouseUp( point.x, rctZ.bottom - 1 - point.y, nFlags );
	if( !( nFlags & ( MK_LBUTTON | MK_RBUTTON | MK_MBUTTON ) ) )
	{
		ReleaseCapture();
	}
}

void CZWnd::OnRButtonUp( UINT nFlags, CPoint point )
{
	CRect rctZ;
	GetClientRect( rctZ );
	Z_MouseUp( point.x, rctZ.bottom - 1 - point.y, nFlags );
	if( !( nFlags & ( MK_LBUTTON | MK_RBUTTON | MK_MBUTTON ) ) )
	{
		ReleaseCapture();
	}
}

BOOL CZWnd::OnMouseWheel( UINT nFlags, short zDelta, CPoint pt )
{
	if( zDelta > 0 )
	{
		g_pParentWnd->OnViewZzoomin();
	}
	else
	{
		g_pParentWnd->OnViewZzoomout();
	}
	return TRUE;
}

BOOL CZWnd::PreCreateWindow( CREATESTRUCT& cs )
{
	WNDCLASS wc;
	HINSTANCE hInstance = AfxGetInstanceHandle();
	if( ::GetClassInfo( hInstance, Z_WINDOW_CLASS, &wc ) == FALSE )
	{
		// Register a new class
		memset( &wc, 0, sizeof( wc ) );
		wc.style         = CS_NOCLOSE;// | CS_OWNDC;
		wc.lpszClassName = Z_WINDOW_CLASS;
		wc.hCursor       = LoadCursor( NULL, IDC_ARROW );
		wc.lpfnWndProc	 = ::DefWindowProc;
		if( AfxRegisterClass( &wc ) == FALSE )
		{
			Error( "CZWnd RegisterClass: failed" );
		}
	}

	cs.lpszClass = Z_WINDOW_CLASS;
	cs.lpszName = "Z View";
	if( cs.style != QE3_CHILDSTYLE )
	{
		cs.style = QE3_SPLITTER_STYLE;
	}
	cs.dwExStyle = WS_EX_TOOLWINDOW;	// sikk - Added - Tool window uses smaller tital bar (more screen space for editing)

	return CWnd::PreCreateWindow( cs );
}


// ---> sikk - Moved z.h/z.cpp contents here
#define PAGEFLIPS	2

z_t z;

/*
==============
CZWnd::Z_Init
==============
*/
void CZWnd::Z_Init()
{
	z.origin[0] = 0;
	z.origin[1] = 0;
	z.origin[2] = 0;

	z.scale = 1;
}

/* MOUSE ACTIONS */
static int	cursorx, cursory;

/*
==============
CZWnd::Z_MouseDown
==============
*/
void CZWnd::Z_MouseDown( int x, int y, int buttons )
{
	idVec3	org, dir, vup, vright;
	brush_t* b;

	Sys_GetCursorPos( &cursorx, &cursory );

	vup[0] = 0;
	vup[1] = 0;
	vup[2] = 1 / z.scale;

	VectorCopy( z.origin, org );
	org[2] += ( y - ( z.height / 2 ) ) / z.scale;
	org[1] = MIN_WORLD_COORD;

	b = selected_brushes.next;
	if( b != &selected_brushes )
	{
		org[0] = ( b->mins[0] + b->maxs[0] ) / 2;
	}

	dir[0] = 0;
	dir[1] = 1;
	dir[2] = 0;

	vright[0] = 0;
	vright[1] = 0;
	vright[2] = 0;

	// new mouse code for ZClip, I'll do this stuff before falling through into the standard ZWindow mouse code...
	if( g_pParentWnd->GetZWnd()->m_pZClip )  	// should always be the case I think, but this is safer
	{
		bool bToggle = false;
		bool bSetTop = false;
		bool bSetBot = false;
		bool bReset  = false;

		if( g_PrefsDlg.m_nMouseButtons == 2 )
		{
			// 2 button mice...
			bToggle = ( GetKeyState( VK_F1 ) & 0x8000 ) != 0;
			bSetTop = ( GetKeyState( VK_F2 ) & 0x8000 ) != 0;
			bSetBot = ( GetKeyState( VK_F3 ) & 0x8000 ) != 0;
			bReset  = ( GetKeyState( VK_F4 ) & 0x8000 ) != 0;
		}
		else
		{
			// 3 button mice...
			bToggle = ( buttons == ( MK_RBUTTON | MK_SHIFT | MK_CONTROL ) );
			bSetTop = ( buttons == ( MK_RBUTTON | MK_SHIFT ) );
			bSetBot = ( buttons == ( MK_RBUTTON | MK_CONTROL ) );
			bReset  = ( GetKeyState( VK_F4 ) & 0x8000 ) != 0;
		}

		if( bToggle )
		{
			g_pParentWnd->GetZWnd()->m_pZClip->Enable( !( g_pParentWnd->GetZWnd()->m_pZClip->IsEnabled() ) );
			Sys_UpdateWindows( W_ALL );
			return;
		}

		if( bSetTop )
		{
			g_pParentWnd->GetZWnd()->m_pZClip->SetTop( org[2] );
			Sys_UpdateWindows( W_ALL );
			return;
		}

		if( bSetBot )
		{
			g_pParentWnd->GetZWnd()->m_pZClip->SetBottom( org[2] );
			Sys_UpdateWindows( W_ALL );
			return;
		}

		if( bReset )
		{
			g_pParentWnd->GetZWnd()->m_pZClip->Reset();
			Sys_UpdateWindows( W_ALL );
			return;
		}
	}

	//
	// LBUTTON = manipulate selection shift-LBUTTON = select middle button = grab
	// texture ctrl-middle button = set entire brush to texture ctrl-shift-middle
	// button = set single face to texture
	//

	// see code above for these next 3, I just commented them here as well for clarity...
	//
	// ctrl-shift-RIGHT button = toggle ZClip on/off
	//      shift-RIGHT button = set ZClip top marker
	//       ctrl-RIGHT button = set ZClip bottom marker

	int nMouseButton = g_PrefsDlg.m_nMouseButtons == 2 ? MK_RBUTTON : MK_MBUTTON;
	if( ( buttons == MK_LBUTTON ) ||
			( buttons == ( MK_LBUTTON | MK_SHIFT ) ) ||
			( buttons == MK_MBUTTON ) || //(buttons == (MK_MBUTTON|MK_CONTROL)) ||
			( buttons == ( nMouseButton | MK_SHIFT | MK_CONTROL ) ) )
	{
		Drag_Begin( x, y, buttons, vright, vup, org, dir );
		return;
	}

	// control mbutton = move camera
	if( ( buttons == ( MK_CONTROL | nMouseButton ) ) || ( buttons == ( MK_CONTROL | MK_LBUTTON ) ) )
	{
		g_pParentWnd->GetCamera()->Camera().origin[2] = org[2];
		Sys_UpdateWindows( W_CAMERA | W_XY_OVERLAY | W_Z );
	}
}

/*
==============
CZWnd::Z_MouseUp
==============
*/
void CZWnd::Z_MouseUp( int x, int y, int buttons )
{
	Drag_MouseUp();
	while( ::ShowCursor( TRUE ) < 0 )
		;
}

/*
==============
CZWnd::Z_MouseMoved
==============
*/
void CZWnd::Z_MouseMoved( int x, int y, int buttons )
{
	if( !buttons )
	{
		return;
	}

	if( buttons == MK_LBUTTON )
	{
		Drag_MouseMoved( x, y, buttons );
		Sys_UpdateWindows( W_Z | W_CAMERA_ICON | W_XY );
		return;
	}

	// rbutton = drag z origin
	if( buttons == MK_RBUTTON )
	{
		if( !( GetAsyncKeyState( VK_MENU ) & 0x8000 ) )
		{
			Sys_GetCursorPos( &x, &y );
			if( y != cursory )
			{
				z.origin[2] += ( y - cursory ) / z.scale;	// sikk - Added "/ z.scale"
				::ShowCursor( FALSE );
				Sys_SetCursorPos( cursorx, cursory );
				Sys_UpdateWindows( W_Z );
			}

			return;
		}
	}

// ---> sikk - Mouse Zoom
	// rbutton + lbutton = zoom z view
	if( buttons == ( MK_RBUTTON | MK_LBUTTON ) || ( buttons == MK_RBUTTON && ( GetAsyncKeyState( VK_MENU ) & 0x8000 ) ) )
	{
		Sys_GetCursorPos( &x, &y );
		if( y != cursory )
		{
			z.scale += ( y - cursory ) * z.scale * 0.00390625f;
			if( z.scale > 64.0f )
			{
				z.scale = 64.0f;
			}
			if( z.scale < 0.003125f )
			{
				z.scale =  0.003125f;
			}
			::ShowCursor( FALSE );
			Sys_SetCursorPos( cursorx, cursory );
			Sys_UpdateWindows( W_Z );
		}
		return;
	}
// <--- sikk - Mouse Zoom


	// control mbutton = move camera
	int nMouseButton = g_PrefsDlg.m_nMouseButtons == 2 ? MK_RBUTTON : MK_MBUTTON;
	if( ( buttons == ( MK_CONTROL | nMouseButton ) ) || ( buttons == ( MK_CONTROL | MK_LBUTTON ) ) )
	{
		g_pParentWnd->GetCamera()->Camera().origin[2] = z.origin[2] + ( y - ( z.height / 2 ) ) / z.scale;
		Sys_UpdateWindows( W_CAMERA | W_XY_OVERLAY | W_Z );
	}
}

/*
==============
CZWnd::Z_DrawGrid
==============
*/
void CZWnd::Z_DrawGrid()
{
	float	zz, zb, ze;
	int		w, h;
	char	text[ 32 ];

	w = z.width / 2 / z.scale;
	h = z.height / 2 / z.scale;

// ---> sikk - Fixed Grid
//	int nSize = 1.0f / g_qeglobals.d_gridsize * 256;
	float fScale = 1.0f / g_qeglobals.d_gridsize * 8;
	int stepSize = g_qeglobals.d_gridsize * 8 * fScale / 2 / z.scale;
	if( stepSize < g_qeglobals.d_gridsize * 8 )
	{
		stepSize = g_qeglobals.d_gridsize * 8;
	}
	else
	{
		int i;
		for( i = 1; i < stepSize; i <<= 1 ) {}
		stepSize = i;
	}
//	int stepSize = max( 64, g_qeglobals.d_gridsize );	// sikk - Larger Grid Sizes - Added
// <--- sikk - Fixed Grid

	zb = z.origin[2] - h;
	if( zb < region_mins[ 2 ] )
	{
		zb = region_mins[ 2 ];
	}
	zb = stepSize * floor( zb / stepSize );				// sikk - Larger Grid Sizes	- was 64

	ze = z.origin[2] + h;
	if( ze > region_maxs[ 2 ] )
	{
		ze = region_maxs[ 2 ];
	}
	ze = stepSize * ceil( ze / stepSize );				// sikk - Larger Grid Sizes	- was 64

	// draw minor blocks
	if(  //z.scale > fScale &&	// sikk - Fixed grid
		g_qeglobals.d_showgrid &&
		//g_qeglobals.d_gridsize * z.scale >= 4 &&
		!g_qeglobals.d_savedinfo.colors[ COLOR_GRIDMINOR ].Compare( g_qeglobals.d_savedinfo.colors[ COLOR_GRIDBACK ] ) )
	{

		qglColor3fv( g_qeglobals.d_savedinfo.colors[ COLOR_GRIDMINOR ].ToFloatPtr() );
		qglBegin( GL_LINES );
		for( zz = zb; zz < ze; zz += stepSize / 8 )
		{
// ---> sikk - Fixed grid
			//if ( !( (int)zz & 63 ) ) {
			//	continue;
			//}
//<--- sikk - Fixed grid
			qglVertex2f( -w, zz );
			qglVertex2f( w, zz );
		}
		qglEnd();
	}

	// draw major blocks
	qglBegin( GL_LINES );
	qglColor3fv( g_qeglobals.d_savedinfo.colors[ COLOR_GRIDMAJOR ].ToFloatPtr() );
	qglVertex2f( 0, zb );
	qglVertex2f( 0, ze );
	for( zz = zb; zz < ze; zz += stepSize )  			// sikk - Larger Grid Sizes	- was 64
	{
		if( zz == 0 )
		{
			qglColor3fv( g_qeglobals.d_savedinfo.colors[ COLOR_GRIDTEXT ].ToFloatPtr() );
		}
		else
		{
			qglColor3fv( g_qeglobals.d_savedinfo.colors[ COLOR_GRIDMAJOR ].ToFloatPtr() );
		}
		qglVertex2f( -w, zz );
		qglVertex2f( w, zz );
	}
	qglEnd();

	// draw coordinate text if needed
	qglColor3fv( g_qeglobals.d_savedinfo.colors[ COLOR_GRIDTEXT ].ToFloatPtr() );

	for( zz = zb; zz < ze; zz += stepSize )
	{
		qglRasterPos2f( -w + 1, zz );
		sprintf( text, "%i", ( int )zz );
		qglCallLists( strlen( text ), GL_UNSIGNED_BYTE, text );
	}
}

#define CAM_HEIGHT	64	// height of main part
#define CAM_GIZMO	8	// height of the gizmo

/*
==============
CZWnd::Z_DrawCameraIcon
==============
*/
void CZWnd::Z_DrawCameraIcon()
{
	float	x, y;
	int		xCam = z.width / 4 / z.scale;	// sikk -

	x = 0;
	y = g_pParentWnd->GetCamera()->Camera().origin[2];

	qglColor3f( 0.0, 0.0, 1.0 );
	qglBegin( GL_LINE_STRIP );
	qglVertex3f( x - xCam, y, 0 );
	qglVertex3f( x, y + CAM_GIZMO, 0 );
	qglVertex3f( x + xCam, y, 0 );
	qglVertex3f( x, y - CAM_GIZMO, 0 );
	qglVertex3f( x - xCam, y, 0 );
	qglVertex3f( x + xCam, y, 0 );
	qglVertex3f( x + xCam, y - CAM_HEIGHT, 0 );
	qglVertex3f( x - xCam, y - CAM_HEIGHT, 0 );
	qglVertex3f( x - xCam, y, 0 );
	qglEnd();
}

/*
==============
CZWnd::Z_DrawZClip
==============
*/
void CZWnd::Z_DrawZClip()
{
	float x, y;

	x = 0;
	y = g_pParentWnd->GetCamera()->Camera().origin[2];

	if( g_pParentWnd->GetZWnd()->m_pZClip )  	// should always be the case I think
	{
		g_pParentWnd->GetZWnd()->m_pZClip->Paint();
	}
}

GLbitfield glbitClear = GL_COLOR_BUFFER_BIT;	// HACK

/*
==============
CZWnd::Z_Draw
==============
*/
void CZWnd::Z_Draw()
{
	brush_t*		brush;
	float		w, h;
	float		top, bottom;
	idVec3		org_top, org_bottom, dir_up, dir_down;
	int			xCam = z.width / 3 / z.scale;	// sikk - Keep brush widths proportional to window

	if( !active_brushes.next )
	{
		return; // not valid yet
	}

	// clear
	qglViewport( 0, 0, z.width, z.height );
	qglScissor( 0, 0, z.width, z.height );

	qglClearColor( g_qeglobals.d_savedinfo.colors[COLOR_GRIDBACK][0],
				   g_qeglobals.d_savedinfo.colors[COLOR_GRIDBACK][1],
				   g_qeglobals.d_savedinfo.colors[COLOR_GRIDBACK][2],
				   0 );

	/*
	 * GL Bug £
	 * When not using hw acceleration, gl will fault if we clear the depth buffer bit
	 * on the first pass. The hack fix is to set the GL_DEPTH_BUFFER_BIT only after
	 * Z_Draw() has been called once. Yeah, right. £
	 * qglClear(glbitClear);
	 */
	qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	//
	// glbitClear |= GL_DEPTH_BUFFER_BIT;
	// qglClear(GL_DEPTH_BUFFER_BIT);
	//
	qglMatrixMode( GL_PROJECTION );
	qglLoadIdentity();

	w = z.width / 2 / z.scale;
	h = z.height / 2 / z.scale;
	qglOrtho( -w, w, z.origin[2] - h, z.origin[2] + h, -8, 8 );

	globalImages->BindNull();
	qglDisable( GL_DEPTH_TEST );
	qglDisable( GL_BLEND );

	// now draw the grid
	Z_DrawGrid();

	// draw stuff
	qglDisable( GL_CULL_FACE );

	qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

	globalImages->BindNull();

	// draw filled interiors and edges
	dir_up[0] = 0;
	dir_up[1] = 0;
	dir_up[2] = 1;
	dir_down[0] = 0;
	dir_down[1] = 0;
	dir_down[2] = -1;
	VectorCopy( z.origin, org_top );
	org_top[2] = 4096;
	VectorCopy( z.origin, org_bottom );
	org_bottom[2] = -4096;

	for( brush = active_brushes.next; brush != &active_brushes; brush = brush->next )
	{
		if( brush->mins[0] >= z.origin[0] ||
				brush->maxs[0] <= z.origin[0] ||
				brush->mins[1] >= z.origin[1] ||
				brush->maxs[1] <= z.origin[1] )
		{
			continue;
		}

		if( !Brush_Ray( org_top, dir_down, brush, &top ) )
		{
			continue;
		}

		top = org_top[2] - top;
		if( !Brush_Ray( org_bottom, dir_up, brush, &bottom ) )
		{
			continue;
		}

		bottom = org_bottom[2] + bottom;

		//q = declManager->FindMaterial( brush->brush_faces->texdef.name );
		qglColor3f( brush->owner->eclass->color.x, brush->owner->eclass->color.y, brush->owner->eclass->color.z );
		qglBegin( GL_QUADS );
		qglVertex2f( -xCam, bottom );
		qglVertex2f( xCam, bottom );
		qglVertex2f( xCam, top );
		qglVertex2f( -xCam, top );
		qglEnd();

		qglColor3f( 1, 1, 1 );
		qglBegin( GL_LINE_LOOP );
		qglVertex2f( -xCam, bottom );
		qglVertex2f( xCam, bottom );
		qglVertex2f( xCam, top );
		qglVertex2f( -xCam, top );
		qglEnd();
	}

	// now draw selected brushes
	for( brush = selected_brushes.next; brush != &selected_brushes; brush = brush->next )
	{
		if( !( brush->mins[0] >= z.origin[0] ||
				brush->maxs[0] <= z.origin[0] ||
				brush->mins[1] >= z.origin[1] ||
				brush->maxs[1] <= z.origin[1] ) )
		{
			if( Brush_Ray( org_top, dir_down, brush, &top ) )
			{
				top = org_top[2] - top;
				if( Brush_Ray( org_bottom, dir_up, brush, &bottom ) )
				{
					bottom = org_bottom[2] + bottom;

					//q = declManager->FindMaterial( brush->brush_faces->texdef.name );
					qglColor3f( brush->owner->eclass->color.x, brush->owner->eclass->color.y, brush->owner->eclass->color.z );
					qglBegin( GL_QUADS );
					qglVertex2f( -xCam, bottom );
					qglVertex2f( xCam, bottom );
					qglVertex2f( xCam, top );
					qglVertex2f( -xCam, top );
					qglEnd();
				}
			}
		}

		qglColor3fv( g_qeglobals.d_savedinfo.colors[COLOR_SELBRUSHES].ToFloatPtr() );
		qglBegin( GL_LINE_LOOP );
		qglVertex2f( -xCam, brush->mins[2] );
		qglVertex2f( xCam, brush->mins[2] );
		qglVertex2f( xCam, brush->maxs[2] );
		qglVertex2f( -xCam, brush->maxs[2] );
		qglEnd();
	}

	Z_DrawCameraIcon();
	Z_DrawZClip();

	qglFinish();
	QE_CheckOpenGLForErrors();
}
// <--- sikk - Moved z.h/z.cpp contents here

// ---> sikk - Added for center view command
/*
==============
CZWnd::PositionView
==============
*/
void CZWnd::PositionView()
{
	brush_t* b = selected_brushes.next;
	if( b && b->next != b )
	{
		z.origin[2] = ( b->maxs[2] - b->mins[2] ) * 0.5f + b->mins[2];
	}
	else
	{
		z.origin[2] = g_pParentWnd->GetCamera()->Camera().origin[2];
	}
}
// <--- sikk - Added for center view command
