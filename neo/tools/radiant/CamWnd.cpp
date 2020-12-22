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
#include "XYWnd.h"
#include "CamWnd.h"
#include "splines.h"
#include <GL/glu.h>

#include "../../renderer/tr_local.h"
#include "../../renderer/model_local.h"	// for idRenderModelMD5

#ifdef _DEBUG
	#define new DEBUG_NEW
	#undef THIS_FILE
	static char THIS_FILE[] = __FILE__;
#endif
extern void DrawPathLines();

int g_axialAnchor = -1;
int g_axialDest = -1;
bool g_bAxialMode = false;

void ValidateAxialPoints()
{
	int faceCount = g_ptrSelectedFaces.GetSize();
	if( faceCount > 0 )
	{
		face_t*	selFace = reinterpret_cast<face_t*>( g_ptrSelectedFaces.GetAt( 0 ) );
		if( g_axialAnchor >= selFace->face_winding->GetNumPoints() )
		{
			g_axialAnchor = 0;
		}
		if( g_axialDest >= selFace->face_winding->GetNumPoints() )
		{
			g_axialDest = 0;
		}
	}
	else
	{
		g_axialDest = 0;
		g_axialAnchor = 0;
	}
}

// CCamWnd
IMPLEMENT_DYNCREATE( CCamWnd, CWnd );

/*
 =======================================================================================================================
 =======================================================================================================================
 */
CCamWnd::CCamWnd()
{
	memset( &m_Camera, 0, sizeof( camera_t ) );
	m_pXYFriend = NULL;
	m_pSide_select = NULL;
	m_bClipMode = false;
	worldDirty = true;
	worldModel = NULL;
	renderMode = false;
	rebuildMode = false;
	entityMode = false;
	animationMode = false;
	selectMode = false;
	soundMode = false;
	saveValid = false;
	m_bCanCreateBrush = false;	// sikk -

	Cam_Init();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
CCamWnd::~CCamWnd()
{
}

BEGIN_MESSAGE_MAP( CCamWnd, CWnd )
//{{AFX_MSG_MAP(CCamWnd)
	ON_WM_KEYDOWN()
	ON_WM_PAINT()
	ON_WM_DESTROY()
	ON_WM_CLOSE()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MBUTTONDOWN()
	ON_WM_MBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_CREATE()
	ON_WM_SIZE()
// ---> sikk - Window Snapping
	ON_WM_SIZING()
	ON_WM_MOVING()
// <--- sikk - Window Snapping
	ON_WM_KEYUP()
	ON_WM_NCCALCSIZE()
	ON_WM_KILLFOCUS()
	ON_WM_SETFOCUS()
	ON_WM_TIMER()
	ON_WM_MOUSEWHEEL()	// sikk - Mousewheel Support for cam window
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/*
 =======================================================================================================================
 =======================================================================================================================
 */
LONG WINAPI CamWndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	RECT rect;

	GetClientRect( hWnd, &rect );

	switch( uMsg )
	{
		case WM_KILLFOCUS:
		case WM_SETFOCUS:
			SendMessage( hWnd, WM_NCACTIVATE, uMsg == WM_SETFOCUS, 0 );
			return 0;
		case WM_NCCALCSIZE: // don't let windows copy pixels
			DefWindowProc( hWnd, uMsg, wParam, lParam );
			return WVR_REDRAW;
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}

//
// =======================================================================================================================
//    CCamWnd message handlers
// =======================================================================================================================
//
BOOL CCamWnd::PreCreateWindow( CREATESTRUCT& cs )
{
	WNDCLASS	wc;
	HINSTANCE	hInstance = AfxGetInstanceHandle();
	if( ::GetClassInfo( hInstance, CAMERA_WINDOW_CLASS, &wc ) == FALSE )
	{
		// Register a new class
		memset( &wc, 0, sizeof( wc ) );

		// wc.style = CS_NOCLOSE | CS_OWNDC;
		wc.style = CS_NOCLOSE;
		wc.lpszClassName = CAMERA_WINDOW_CLASS;
		wc.hCursor = LoadCursor( NULL, IDC_ARROW );
		wc.lpfnWndProc = CamWndProc;
		if( AfxRegisterClass( &wc ) == FALSE )
		{
			Error( "CCamWnd RegisterClass: failed" );
		}
	}

	cs.lpszClass = CAMERA_WINDOW_CLASS;
	cs.lpszName = "Camera View: Edit Mode";
	if( cs.style != QE3_CHILDSTYLE )
	{
		cs.style = QE3_SPLITTER_STYLE;
	}
	cs.dwExStyle = WS_EX_TOOLWINDOW;	// sikk - Added - Tool window uses smaller title bar (more screen space for editing)

	BOOL bResult = CWnd::PreCreateWindow( cs );

	// See if the class already exists and if not then we need to register our new
	// window class.
	return bResult;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void CCamWnd::OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags )
{
	g_pParentWnd->HandleKey( nChar, nRepCnt, nFlags );
}

brush_t* g_pSplitList = NULL;

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void CCamWnd::OnPaint()
{
	CPaintDC dc( this );	// device context for painting
	bool bPaint = true;

	if( !qwglMakeCurrent( dc.m_hDC, win32.hGLRC ) )
	{
		common->Printf( "ERROR: wglMakeCurrent failed..\n " );
		common->Printf( "Please restart " EDITOR_WINDOWTEXT " if the camera view is not working\n" );
	}
	else
	{
		QE_CheckOpenGLForErrors();
		g_pSplitList = NULL;
		if( g_bClipMode )
		{
			if( g_Clip1.Set() && g_Clip2.Set() )
			{
				g_pSplitList = ( ( g_pParentWnd->ActiveXY()->GetViewType() == XZ ) ? !g_bSwitch : g_bSwitch ) ? &g_brBackSplits : &g_brFrontSplits;
			}
		}

		Cam_Draw();



		QE_CheckOpenGLForErrors();
		qwglSwapBuffers( dc.m_hDC );
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void CCamWnd::SetXYFriend( CXYWnd* pWnd )
{
	m_pXYFriend = pWnd;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void CCamWnd::OnDestroy()
{
	CWnd::OnDestroy();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void CCamWnd::OnClose()
{
	CWnd::OnClose();
}

extern void Select_RotateTexture( float amt, bool absolute );

// ---> sikk - Cam Win Clip Point Manipulation
extern float fDiff( float f1, float f2 );// {
//	if ( f1 > f2 ) {
//		return f1 - f2;
//	} else {
//		return f2 - f1;
//	}
//}
/*
================
AxializeVector
================
*/
static void AxializeVector( idVec3& v )
{
	idVec3	a;
	float	o;
	int		i;

	if( !v[0] && !v[1] )
	{
		return;
	}

	if( !v[1] && !v[2] )
	{
		return;
	}

	if( !v[0] && !v[2] )
	{
		return;
	}

	for( i = 0; i < 3; i++ )
	{
		a[i] = idMath::Fabs( v[i] );
	}

	if( a[0] > a[1] && a[0] > a[2] )
	{
		i = 0;
	}
	else if( a[1] > a[0] && a[1] > a[2] )
	{
		i = 1;
	}
	else
	{
		i = 2;
	}

	o = v[i];
	VectorCopy( vec3_origin, v );
	if( o < 0 )
	{
		v[i] = -1;
	}
	else
	{
		v[i] = 1;
	}
}
// <--- sikk - Cam Win Clip Point Manipulation

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void CCamWnd::OnMouseMove( UINT nFlags, CPoint point )
{
	CRect	r;
	GetClientRect( r );
// ---> sikk - Added for Context menu
	m_ptDown.x = 0;
	m_ptDown.y = 0;
// <--- sikk - Added for Context menu

// ---> sikk - TODO: Fix this so mouse texture maniipulation is useable
	if( GetCapture() == this && ( GetAsyncKeyState( VK_MENU ) & 0x8000 ) )
	{
		//if ( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) {
		//	Select_RotateTexture( (float)point.y - m_ptLastCursor.y );
		//} else if ( GetAsyncKeyState( VK_SHIFT ) & 0x8000) {
		//	Select_ScaleTexture( ( (float)point.x / m_ptLastCursor.x ), (float)m_ptLastCursor.y / point.y );
		//} else {
		//	Select_ShiftTexture( ( (float)point.x - m_ptLastCursor.x ) * 0.1f, ( (float)m_ptLastCursor.y - point.y ) * 0.1f );
		//}
// <--- sikk -
	}
	else
	{
// ---> sikk - Cam Win Clip Point Manipulation
		bool bCrossHair = false;
		if( m_pXYFriend->ClipMode() && !( m_nCambuttonstate & MK_RBUTTON ) )
		{
			const char* status;
			float pressx = m_ptLBDown.x;
			float pressy = r.bottom - 1 - m_ptLBDown.y;
			float x = point.x;
			float y = r.bottom - 1 - point.y;

			if( g_pMovingClip && GetCapture() == this )
			{
				idVec3 xAxis, yAxis, move, delta;
				float scale = idVec3( g_pMovingClip->m_ptClip - m_Camera.origin ).LengthFast() / 768;

				// clear along one axis
				if( m_nCambuttonstate == ( MK_LBUTTON | MK_SHIFT ) )
				{
					if( abs( x - pressx ) > abs( y - pressy ) )
					{
						y = pressy;
					}
					else
					{
						x = pressx;
					}
				}

				VectorCopy( m_Camera.vright, xAxis );
				VectorCopy( m_Camera.vup, yAxis );
				AxializeVector( xAxis );
				AxializeVector( yAxis );
				for( int i = 0; i < 3; i++ )
				{
					move[i] = ( xAxis[i] * ( x - pressx ) + yAxis[i] * ( y - pressy ) ) * scale;
					move[i] = floor( move[i] / g_qeglobals.d_gridsize + 0.5 ) * g_qeglobals.d_gridsize;
				}
				VectorSubtract( move, m_vPressdelta, delta );
				VectorCopy( move, m_vPressdelta );
				VectorAdd( g_pMovingClip->m_ptClip, delta, g_pMovingClip->m_ptClip );

				m_bCanCreateBrush = false;	// sikk -
				bCrossHair = true;
				//printf( status, "x:: %.1f  y:: %.1f  z:: %.1f", g_pMovingClip->m_ptClip.x, g_pMovingClip->m_ptClip.x, g_pMovingClip->m_ptClip.x );
				//g_pParentWnd->SetStatusText( 1, status );
				Sys_UpdateWindows( W_ALL );
			}
			else
			{
				VectorCopy( vec3_origin, m_vPressdelta );
				g_pMovingClip = NULL;

				// calc ray direction
				idVec3	dir, temp;
				float up = ( float )( y - m_Camera.height / 2 ) / ( m_Camera.width / 2 );
				float right = ( float )( x - m_Camera.width / 2 ) / ( m_Camera.width / 2 );
				for( int i = 0; i < 3; i++ )
				{
					dir[i] = m_Camera.vpn[i] + m_Camera.vright[i] * right + m_Camera.vup[i] * up;
				}
				dir.Normalize();

				// find the clip point closest to the ray
				float d, bestd;
				bestd = 8;

				if( g_Clip1.Set() )
				{
					temp = g_Clip1.m_ptClip - ( m_Camera.origin + ( g_Clip1.m_ptClip - m_Camera.origin ) * dir * dir );
					d = temp.Length();
					if( d <= bestd )
					{
						bestd = d;
						bCrossHair = true;
						g_pMovingClip = &g_Clip1;
					}
				}
				if( g_Clip2.Set() )
				{
					temp = g_Clip2.m_ptClip - ( m_Camera.origin + ( g_Clip2.m_ptClip - m_Camera.origin ) * dir * dir );
					d = temp.Length();
					if( d <= bestd )
					{
						bestd = d;
						bCrossHair = true;
						g_pMovingClip = &g_Clip2;
					}
				}
				if( g_Clip3.Set() )
				{
					temp = g_Clip3.m_ptClip - ( m_Camera.origin + ( g_Clip3.m_ptClip - m_Camera.origin ) * dir * dir );
					d = temp.Length();
					if( d <= bestd )
					{
						bCrossHair = true;
						g_pMovingClip = &g_Clip3;
					}
				}
			}

			if( bCrossHair == false )
			{
				Cam_MouseMoved( point.x, r.bottom - 1 - point.y, nFlags );
			}
		}
		else
		{
			Cam_MouseMoved( point.x, r.bottom - 1 - point.y, nFlags );
		}

		if( bCrossHair )
		{
			SetCursor( ::LoadCursor( NULL, IDC_CROSS ) );
		}
		else
		{
			SetCursor( ::LoadCursor( NULL, IDC_ARROW ) );
		}
// <--- sikk - Cam Win Clip Point Manipulation
//		Cam_MouseMoved( point.x, r.bottom - 1 - point.y, nFlags );
	}

	m_ptLastCursor = point;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void CCamWnd::OnLButtonDown( UINT nFlags, CPoint point )
{
	m_ptLastCursor = point;
// ---> sikk - Bring window to front
	if( g_pParentWnd->GetTopWindow() != this )
	{
		BringWindowToTop();
	}
// <--- sikk - Bring window to front
	m_ptLBDown = point;	// sikk - Clip Point Manipulation
	OriginalMouseDown( nFlags, point );
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void CCamWnd::OnLButtonUp( UINT nFlags, CPoint point )
{
	OriginalMouseUp( nFlags, point );
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void CCamWnd::OnMButtonDown( UINT nFlags, CPoint point )
{
// ---> sikk - Bring window to front
	if( g_pParentWnd->GetTopWindow() != this )
	{
		BringWindowToTop();
	}
// <--- sikk - Bring window to front
	OriginalMouseDown( nFlags, point );
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void CCamWnd::OnMButtonUp( UINT nFlags, CPoint point )
{
	OriginalMouseUp( nFlags, point );
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void CCamWnd::OnRButtonDown( UINT nFlags, CPoint point )
{
// ---> sikk - Bring window to front
	if( g_pParentWnd->GetTopWindow() != this )
	{
		BringWindowToTop();
	}
// <--- sikk - Bring window to front
	m_ptDown = point;	// sikk - Added for context menu
	OriginalMouseDown( nFlags, point );
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void CCamWnd::OnRButtonUp( UINT nFlags, CPoint point )
{
// ---> sikk - Added - Context Menu
	if( point == m_ptDown )  	// mouse didn't move
	{
		bool bGo = true;
		if( ( GetAsyncKeyState( VK_MENU ) & 0x8000 ) ||
				( GetAsyncKeyState( VK_CONTROL ) & 0x8000 ) ||
				( GetAsyncKeyState( VK_SHIFT ) & 0x8000 ) )
		{
			bGo = false;
		}

		if( bGo )
		{
			// TODO: This should be Cam specific
			m_pXYFriend->HandleDrop();
		}
	}
// <--- sikk - Added - Context Menu

	OriginalMouseUp( nFlags, point );
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int CCamWnd::OnCreate( LPCREATESTRUCT lpCreateStruct )
{
	if( CWnd::OnCreate( lpCreateStruct ) == -1 )
	{
		return -1;
	}

	CDC* pDC = GetDC();
	HDC hDC = pDC->GetSafeHdc();

	QEW_SetupPixelFormat( hDC, true );

	HFONT hfont = CreateFont(
					  12,	// logical height of font
					  0,	// logical average character width
					  0,	// angle of escapement
					  0,	// base-line orientation angle
					  0,	// font weight
					  0,	// italic attribute flag
					  0,	// underline attribute flag
					  0,	// strikeout attribute flag
					  0,	// character set identifier
					  0,	// output precision
					  0,	// clipping precision
					  0,	// output quality
					  FIXED_PITCH | FF_MODERN,	// pitch and family
					  "Lucida Console" 	// pointer to typeface name string
				  );

	if( !hfont )
	{
		Error( "couldn't create font" );
	}

	HFONT hOldFont = ( HFONT )SelectObject( hDC, hfont );

	wglMakeCurrent( hDC, win32.hGLRC );

	if( ( g_qeglobals.d_font_list = qglGenLists( 256 ) ) == 0 )
	{
		common->Warning( "couldn't create font dlists" );
	}

	// create the bitmap display lists we're making images of glyphs 0 thru 255
	if( !qwglUseFontBitmaps( hDC, 0, 255, g_qeglobals.d_font_list ) )
	{
		common->Warning( "wglUseFontBitmaps failed (%d).  Trying again.", GetLastError() );

		// FIXME: This is really wacky, sometimes the first call fails, but calling it again makes it work
		//		This probably indicates there's something wrong somewhere else in the code, but I'm not sure what
		if( !qwglUseFontBitmaps( hDC, 0, 255, g_qeglobals.d_font_list ) )
		{
			common->Warning( "wglUseFontBitmaps failed again (%d).  Trying outlines.", GetLastError() );
			if( !qwglUseFontOutlines( hDC, 0, 255, g_qeglobals.d_font_list, 0.0f, 0.1f, WGL_FONT_LINES, NULL ) )
			{
				common->Warning( "wglUseFontOutlines also failed (%d), no coordinate text will be visible.", GetLastError() );
			}
		}
	}

	SelectObject( hDC, hOldFont );
	ReleaseDC( pDC );

	// indicate start of glyph display lists
	qglListBase( g_qeglobals.d_font_list );

	// report OpenGL information
	common->Printf( "GL_VENDOR: %s\n", qglGetString( GL_VENDOR ) );
	common->Printf( "GL_RENDERER: %s\n", qglGetString( GL_RENDERER ) );
	common->Printf( "GL_VERSION: %s\n", qglGetString( GL_VERSION ) );
	common->Printf( "GL_EXTENSIONS: %s\n", qglGetString( GL_EXTENSIONS ) );

	return 0;
}

/*
================
OriginalMouseUp
================
*/
void CCamWnd::OriginalMouseUp( UINT nFlags, CPoint point )
{
	CRect	r;
	GetClientRect( r );
	Cam_MouseUp( point.x, r.bottom - 1 - point.y, nFlags );
	if( !( nFlags & ( MK_LBUTTON | MK_RBUTTON | MK_MBUTTON ) ) )
	{
		ReleaseCapture();
	}
}

/*
================
OriginalMouseDown
================
*/
void CCamWnd::OriginalMouseDown( UINT nFlags, CPoint point )
{
	// if (GetTopWindow()->GetSafeHwnd() != GetSafeHwnd()) BringWindowToTop();
	CRect r;
	GetClientRect( r );
	SetFocus();
	SetCapture();

	// if (!(GetAsyncKeyState(VK_MENU) & 0x8000))
	Cam_MouseDown( point.x, r.bottom - 1 - point.y, nFlags );
}

/*
================
Cam_Init
================
*/
void CCamWnd::Cam_Init()
{
	// m_Camera.draw_mode = cd_texture;
	m_Camera.origin[0] = -64.0f;
	m_Camera.origin[1] = -64.0f;
	m_Camera.origin[2] = 64.0f;
	m_Camera.angles[YAW] = -45.0f;
	m_Camera.angles[PITCH] = -45.0f;
	m_Camera.color[0] = 0.3f;
	m_Camera.color[1] = 0.3f;
	m_Camera.color[2] = 0.3f;
	m_Camera.viewDist = 512.0f;	// sikk - Added - Rotate Around Selection
}

/*
================
Cam_BuildMatrix
================
*/
void CCamWnd::Cam_BuildMatrix()
{
	float	xa, ya;
	float	matrix[4][4];
	int		i;

	xa = ( renderMode ? -m_Camera.angles[PITCH] : m_Camera.angles[PITCH] ) * idMath::M_DEG2RAD;
	ya = m_Camera.angles[YAW] * idMath::M_DEG2RAD;

	// the movement matrix is kept 2d
	m_Camera.forward[0] = cos( ya );
	m_Camera.forward[1] = sin( ya );
	m_Camera.right[0] = m_Camera.forward[1];
	m_Camera.right[1] = -m_Camera.forward[0];

	qglGetFloatv( GL_PROJECTION_MATRIX, &matrix[0][0] );

	for( i = 0; i < 3; i++ )
	{
		m_Camera.vright[i] = matrix[i][0];
		m_Camera.vup[i] = matrix[i][1];
		m_Camera.vpn[i] = matrix[i][2];
	}

	m_Camera.vright.Normalize();
	m_Camera.vup.Normalize();
	m_Camera.vpn.Normalize();
	InitCull();
}

/*
================
Cam_ChangeFloor
================
*/
void CCamWnd::Cam_ChangeFloor( bool up )
{
	brush_t* b;
	float	d, bestd, current;
	idVec3	start, dir;

	start[0] = m_Camera.origin[0];
	start[1] = m_Camera.origin[1];
	start[2] = HUGE_DISTANCE;
	dir[0] = dir[1] = 0;
	dir[2] = -1;

	current = HUGE_DISTANCE - ( m_Camera.origin[2] - 72 );
	if( up )
	{
		bestd = 0;
	}
	else
	{
		bestd = HUGE_DISTANCE * 2;
	}

	for( b = active_brushes.next; b != &active_brushes; b = b->next )
	{
		if( !Brush_Ray( start, dir, b, &d ) )
		{
			continue;
		}
		if( up && d < current && d > bestd )
		{
			bestd = d;
		}
		if( !up && d > current && d < bestd )
		{
			bestd = d;
		}
	}

	if( bestd == 0 || bestd == HUGE_DISTANCE * 2 )
	{
		return;
	}

	m_Camera.origin[2] += current - bestd;
	Sys_UpdateWindows( W_CAMERA | W_Z_OVERLAY );
}

/*
================
Cam_PositionDrag
================
*/
void CCamWnd::Cam_PositionDrag()
{
	int x, y;
	Sys_GetCursorPos( &x, &y );
	if( x != m_ptCursor.x || y != m_ptCursor.y )
	{
		x -= m_ptCursor.x;
		VectorMA( m_Camera.origin, x, m_Camera.vright, m_Camera.origin );
		y -= m_ptCursor.y;
		m_Camera.origin[2] -= y;
		SetCursorPos( m_ptCursor.x, m_ptCursor.y );
		Sys_UpdateWindows( W_CAMERA | W_XY_OVERLAY );
	}
}

/*
================
Cam_Rotate
================
*/
void CCamWnd::Cam_Rotate( int x, int y, idVec3 org )
{
	float	dist, pitch;
	idVec3	work, forward, dir, vecdist;

	for( int i = 0; i < 3; i++ )
	{
		vecdist[i] = fabs( ( m_Camera.origin[i] - org[i] ) );
		vecdist[i] *= vecdist[i];
	}

	m_Camera.viewDist = dist = sqrt( vecdist[0] + vecdist[1] + vecdist[2] );
	VectorSubtract( m_Camera.origin, org, work );
	//VectorToAngles( work, m_Camera.angles );
	m_Camera.angles = work.ToAngles();
	m_Camera.angles[PITCH] = -m_Camera.angles[PITCH];

	if( m_Camera.angles[PITCH] > 100 )
	{
		m_Camera.angles[PITCH] -= 360;
	}
	m_Camera.angles[PITCH] += ( float )y * 0.25f;
	m_Camera.angles[PITCH] = idMath::ClampFloat( -89, 89, m_Camera.angles[PITCH] );
	m_Camera.angles[PITCH] = -m_Camera.angles[PITCH];

	m_Camera.angles[YAW] -= ( float )x * 0.25f;

	forward = m_Camera.angles.ToForward();
//	forward[2] = -forward[2];
	VectorMA( org, dist, forward, m_Camera.origin );

	dir = org - m_Camera.origin;
	dir.Normalize();
	m_Camera.angles[1] = RAD2DEG( atan2( dir[1], dir[0] ) );
	m_Camera.angles[0] = RAD2DEG( asin( dir[2] ) );

	Cam_BuildMatrix();

	Sys_SetCursorPos( m_ptCursor.x, m_ptCursor.y );
	Sys_UpdateWindows( W_ALL );
}

/*
==================
Cam_PositionRotate
==================
*/
void CCamWnd::Cam_PositionRotate()
{
	int		x, y, i;
	idVec3	mins, maxs, forward;
	idVec3	org;
	brush_t*	b;
	face_t*	f;

	SetCursor( NULL ); // sikk - Remove Cursor
	//Sys_GetCursorPos( &x, &y );
	CPoint current;
	GetCursorPos( &current );

	if( current.x != m_ptCursor.x || current.y != m_ptCursor.y )
	{
		current.x -= m_ptCursor.x;
		current.y -= m_ptCursor.y;

		if( selected_brushes.next != &selected_brushes )
		{
			mins[0] = mins[1] = mins[2] = 99999;
			maxs[0] = maxs[1] = maxs[2] = -99999;
			for( b = selected_brushes.next; b != &selected_brushes; b = b->next )
			{
				for( i = 0; i < 3; i++ )
				{
					mins[i] = min( mins[i], b->mins[i] );
					maxs[i] = max( maxs[i], b->maxs[i] );
				}
			}
			org = ( mins + maxs ) * 0.5f;
		}
		else if( g_ptrSelectedFaces.GetCount() > 0 )
		{
			mins[0] = mins[1] = mins[2] = 99999;
			maxs[0] = maxs[1] = maxs[2] = -99999;

			//		f = g_pfaceSelectedFace;
			// rotate around last selected face
			f = reinterpret_cast<face_t*>( g_ptrSelectedFaces.GetAt( g_ptrSelectedFaces.GetSize() - 1 ) );
			//for ( j = 0; j < f->face_winding->numpoints; j++ ) {
			//	for ( i = 0; i < 3; i++ ) {
			//		if ( f->face_winding->points[j][i] > maxs[i] ) {
			//			maxs[i] = f->face_winding->points[j][i];
			//		}
			//		if ( f->face_winding->points[j][i] < mins[i] ) {
			//			mins[i] = f->face_winding->points[j][i];
			//		}
			//	}
			//}

			org = f->face_winding->GetCenter();
		}
		else
		{
			//AngleVectors(g_qeglobals.d_camera.angles, forward, NULL, NULL);
			forward = m_Camera.angles.ToForward();
			forward[2] = -forward[2];
			VectorMA( m_Camera.origin, m_Camera.viewDist, forward, org );

			//	org = m_Camera.origin += -m_Camera.viewDist * m_Camera.vpn;
		}

		Cam_Rotate( current.x, current.y, org );
	}
}

/*
==================
Cam_MouseLook
==================
*/
void CCamWnd::Cam_MouseLook()
{
	CPoint current;
	GetCursorPos( &current );
	if( current.x != m_ptCursor.x || current.y != m_ptCursor.y )
	{
		SetCursor( NULL ); // sikk - Remove Cursor
		current.x -= m_ptCursor.x;
		current.y -= m_ptCursor.y;

		m_Camera.angles[PITCH] -= ( float )current.y * 0.25f;
		m_Camera.angles[PITCH] = idMath::ClampFloat( -89, 89, m_Camera.angles[PITCH] );	// sikk - Added
		m_Camera.angles[YAW] -= ( float )current.x * 0.25f;

		SetCursorPos( m_ptCursor.x, m_ptCursor.y );

//		Cam_BuildMatrix();	// sikk - This is screwing up movement with wasd?
	}
}

/*
==================
Cam_MouseControl
==================
*/
void CCamWnd::Cam_MouseControl()
{
// ---> sikk - Modified Camera Control
	CPoint current;

	GetCursorPos( &current );
	if( current.x != m_ptCursor.x || current.y != m_ptCursor.y )
	{
		SetCursor( NULL ); // sikk - Remove Cursor
		current.x -= m_ptCursor.x;
		current.y -= m_ptCursor.y;

		m_Camera.origin += ( float )current.y * 2.0f * -m_Camera.vpn;
		m_Camera.angles[YAW] += ( float )current.x * -0.0625f;

		SetCursorPos( m_ptCursor.x, m_ptCursor.y );
	}

// <--- sikk - Modified Camera Control

//	Cam_BuildMatrix();
	int nUpdate = ( g_PrefsDlg.m_bCamXYUpdate ) ? ( W_CAMERA | W_XY ) : ( W_CAMERA );
	Sys_UpdateWindows( nUpdate );
}

/*
==================
Cam_KeyControl
==================
*/
void CCamWnd::Cam_KeyControl( float dtime )
{
// ---> sikk - Modified Camera Control
	/*	int		xl, xh;
		int		yl, yh;
		float	xf, yf;
		if (g_PrefsDlg.m_nMouseButtons == 2) {
			if (m_nCambuttonstate != (MK_RBUTTON | MK_SHIFT)) {
				return;
			}
		}
		else {
			if (m_nCambuttonstate != MK_RBUTTON) {
				return;
			}
		}

		xf = (float)(m_ptButton.x - m_Camera.width / 2) / (m_Camera.width / 2);
		yf = (float)(m_ptButton.y - m_Camera.height / 2) / (m_Camera.height / 2);

		xl = m_Camera.width / 3;
		xh = xl * 2;
		yl = m_Camera.height / 3;
		yh = yl * 2;

		// common->Printf("xf-%f yf-%f xl-%i xh-i% yl-i% yh-i%\n",xf,yf,xl,xh,yl,yh);
	#if 0

		// strafe
		if (buttony < yl && (buttonx < xl || buttonx > xh)) {
			VectorMA(camera.origin, xf * dtime * g_nMoveSpeed, camera.right, camera.origin);
		}
		else
	#endif
		{
			xf *= 1.0f - idMath::Fabs(yf);
			if ( xf < 0.0f ) {
				xf += 0.1f;
				if ( xf > 0.0f ) {
					xf = 0.0f;
				}
			}
			else {
				xf -= 0.1f;
				if ( xf < 0.0f ) {
					xf = 0.0f;
				}
			}

			VectorMA(m_Camera.origin, yf * dtime * g_PrefsDlg.m_nMoveSpeed, m_Camera.forward, m_Camera.origin);
			m_Camera.angles[YAW] += xf * -dtime * g_PrefsDlg.m_nAngleSpeed;
		}
	*/
	if( ( ::GetAsyncKeyState( VK_CONTROL ) || ::GetAsyncKeyState( VK_MENU ) || ::GetAsyncKeyState( VK_SHIFT ) ) ||
			g_pParentWnd->GetFocus() != g_pParentWnd->GetCamera() &&
			g_pParentWnd->GetFocus() != g_pParentWnd->GetXYWnd() &&
			g_pParentWnd->GetFocus() != g_pParentWnd->GetXZWnd() &&
			g_pParentWnd->GetFocus() != g_pParentWnd->GetYZWnd() &&
			g_pParentWnd->GetFocus() != g_pParentWnd->GetZWnd() )
	{
		return;
	}
	int forward	= ::GetAsyncKeyState( 'W' );
	int back	= ::GetAsyncKeyState( 'S' );
	int left	= ::GetAsyncKeyState( 'A' );
	int right	= ::GetAsyncKeyState( 'D' );
	//float speed = ( ::GetAsyncKeyState( VK_SHIFT ) ) ? 0.25f : 1.0f;
	if( forward || back || left || right )
	{
		SetCursor( NULL ); // sikk - Remove Cursor

		m_Camera.origin += -m_Camera.vpn * dtime * forward * 0.0625f;
		m_Camera.origin += m_Camera.vpn * dtime * back * 0.0625f;
		m_Camera.origin += m_Camera.vright * dtime * left * 0.0625f;
		m_Camera.origin += -m_Camera.vright * dtime * right * 0.0625f;

//		Cam_BuildMatrix();
		int nUpdate = ( g_PrefsDlg.m_bCamXYUpdate ) ? ( W_CAMERA | W_XY ) : ( W_CAMERA );
		Sys_UpdateWindows( nUpdate );
		g_pParentWnd->PostMessage( WM_TIMER, 0, 0 );
	}
}
// <--- sikk - Key wsad control

/*
==================
Cam_MouseDown
==================
*/
void CCamWnd::Cam_MouseDown( int x, int y, int buttons )
{
	idVec3	dir;
	float	f, r, u;
	int		i;

	// calc ray direction
	u = ( float )( y - m_Camera.height / 2 ) / ( m_Camera.width / 2 );
	r = ( float )( x - m_Camera.width / 2 ) / ( m_Camera.width / 2 );
	f = 1;

	for( i = 0; i < 3; i++ )
	{
		dir[i] = m_Camera.vpn[i] * f + m_Camera.vright[i] * r + m_Camera.vup[i] * u;
	}

	dir.Normalize();

	GetCursorPos( &m_ptCursor );

	m_nCambuttonstate = buttons;
	m_ptButton.x = x;
	m_ptButton.y = y;

	//
	// LBUTTON = manipulate selection shift-LBUTTON = select middle button = grab
	// texture ctrl-middle button = set entire brush to texture ctrl-shift-middle
	// button = set single face to texture
	//
	int nMouseButton = g_PrefsDlg.m_nMouseButtons == 2 ? MK_RBUTTON : MK_MBUTTON;
	if( ( buttons == MK_LBUTTON ) ||
			( buttons == ( MK_LBUTTON | MK_SHIFT ) ) ||
			( buttons == ( MK_LBUTTON | MK_CONTROL ) ) ||
			( buttons == ( MK_LBUTTON | MK_CONTROL | MK_SHIFT ) ) ||
			( buttons == nMouseButton ) ||
			( buttons == ( nMouseButton | MK_SHIFT ) ) ||
			( buttons == ( nMouseButton | MK_CONTROL ) ) ||
			( buttons == ( nMouseButton | MK_SHIFT | MK_CONTROL ) ) )
	{
		if( g_PrefsDlg.m_nMouseButtons == 2 && ( buttons == ( MK_RBUTTON | MK_SHIFT ) ) )
		{
			Cam_MouseControl();
		}
// ---> sikk -
		if( buttons == MK_LBUTTON && selected_brushes.next == &selected_brushes )
		{
			m_bCanCreateBrush = true;
		}
		else
		{
// ---> sikk - Camera Drag Fix
			if( selected_brushes.next != &selected_brushes )
			{
				brush_t* b;
				idVec3 selectedMins, selectedMaxs, dist;
				selectedMins = selected_brushes.next->mins;
				selectedMaxs = selected_brushes.next->maxs;
				for( b = selected_brushes.next; b != &selected_brushes; b = b->next )
				{
					for( i = 0; i < 3; i++ )
					{
						selectedMins[i] = min( selectedMins[i], b->mins[i] );
						selectedMaxs[i] = max( selectedMaxs[i], b->maxs[i] );
					}
				}
				dist = g_pParentWnd->GetCamera()->Camera().origin - idBounds( selectedMins, selectedMaxs ).GetCenter();
				m_Camera.viewDist = dist.LengthFast();
			}
// <--- sikk - Camera Drag Fix

			// something global needs to track which window is responsible for stuff
			Patch_SetView( W_CAMERA );
			Drag_Begin( x, y, buttons, m_Camera.vright, m_Camera.vup, m_Camera.origin, dir );
		}

		return;
	}

	//if ( buttons == MK_RBUTTON ) {
	//	Cam_MouseControl( 0.1f );
	//}
}

/*
==================
Cam_MouseUp
==================
*/
void CCamWnd::Cam_MouseUp( int x, int y, int buttons )
{
// ---> sikk -
	bool bPress_selection = ( selected_brushes.next != &selected_brushes );
	if( m_bCanCreateBrush && m_nCambuttonstate == MK_LBUTTON && !bPress_selection && g_qeglobals.d_select_mode == sel_brush )
	{
		NewBrushDrag( x, y );
		m_bCanCreateBrush = false;
//		return;
	}

	m_nCambuttonstate = 0;
	Drag_MouseUp( buttons );
}

/*
==================
Cam_MouseMoved
==================
*/
void CCamWnd::Cam_MouseMoved( int x, int y, int buttons )
{
	m_nCambuttonstate = buttons;
	m_bCanCreateBrush = false;	// sikk -

	if( !buttons )
	{
		return;
	}

	m_ptButton.x = x;
	m_ptButton.y = y;

// ---> sikk - Modified Camera Control
//	if (buttons == (MK_RBUTTON | MK_CONTROL)) {
	if( buttons == ( MK_RBUTTON | MK_LBUTTON ) )
	{
		Cam_PositionDrag();
		Sys_UpdateWindows( W_XY | W_CAMERA | W_Z );
		return;
	}
	else if( ( buttons == ( MK_RBUTTON | MK_CONTROL ) ) ||
			 ( buttons == MK_LBUTTON && selected_brushes.next == &selected_brushes ) )  // | MK_SHIFT) ) {
	{
		// lbutton without selection = drag new brush if (m_nButtonstate == MK_LBUTTON &&
		// !m_bPress_selection && g_qeglobals.d_select_mode != sel_curvepoint &&
		// g_qeglobals.d_select_mode != sel_splineedit)
		//
		Cam_MouseControl();
	}
	else if( buttons == ( MK_RBUTTON | MK_SHIFT ) )
	{
		Cam_PositionRotate();
		Sys_UpdateWindows( W_XY | W_CAMERA | W_Z );
		return;
	}
	else if( buttons == MK_RBUTTON )
	{
		Cam_MouseLook();
		Sys_UpdateWindows( W_XY | W_CAMERA | W_Z );
		return;
	}
// <--- sikk - Modified Camera Control

	GetCursorPos( &m_ptCursor );

	if( buttons & ( MK_LBUTTON | MK_MBUTTON ) )
	{
		Drag_MouseMoved( x, y, buttons );
		Sys_UpdateWindows( W_XY | W_CAMERA | W_Z );
	}
}

/*
==================
InitCull
==================
*/
void CCamWnd::InitCull()
{
	VectorSubtract( m_Camera.vpn, m_Camera.vright, m_vCull1 );
	VectorAdd( m_Camera.vpn, m_Camera.vright, m_vCull2 );

	for( int i = 0; i < 3; i++ )
	{
		if( m_vCull1[i] > 0 )
		{
			m_nCullv1[i] = 3 + i;
		}
		else
		{
			m_nCullv1[i] = i;
		}

		if( m_vCull2[i] > 0 )
		{
			m_nCullv2[i] = 3 + i;
		}
		else
		{
			m_nCullv2[i] = i;
		}
	}
}

/*
==================
CullBrush
==================
*/
bool CCamWnd::CullBrush( brush_t* b, bool cubicOnly )
{
	int		i;
	idVec3	point;
	float	d;

	if( b->forceVisibile )
	{
		return false;
	}

	if( g_PrefsDlg.m_bCubicClipping )
	{
		float distance = g_PrefsDlg.m_nCubicScale * 64;

		idVec3 mid;
		for( i = 0; i < 3; i++ )
		{
			mid[i] = ( b->mins[i] + ( ( b->maxs[i] - b->mins[i] ) / 2 ) );
		}

		point = mid - m_Camera.origin;
		if( point.Length() > distance )
		{
			return true;
		}

	}

	if( cubicOnly )
	{
		return false;
	}

	for( i = 0; i < 3; i++ )
	{
		point[i] = b->mins[m_nCullv1[i]] - m_Camera.origin[i];
	}

	d = DotProduct( point, m_vCull1 );
	if( d < -1 )
	{
		return true;
	}

	for( i = 0; i < 3; i++ )
	{
		point[i] = b->mins[m_nCullv2[i]] - m_Camera.origin[i];
	}

	d = DotProduct( point, m_vCull2 );
	if( d < -1 )
	{
		return true;
	}

	return false;
}

#if 0
/*
==================
DrawLightRadius
==================
*/
void CCamWnd::DrawLightRadius( brush_t* pBrush )
{
	// if lighting
	int nRadius = Brush_LightRadius( pBrush );
	if( nRadius > 0 )
	{
		Brush_SetLightColor( pBrush );
		qglEnable( GL_BLEND );
		qglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		qglDisable( GL_BLEND );
		qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	}
}
#endif

/*
==================
setGLMode
==================
*/
void setGLMode( int mode )
{
	switch( mode )
	{
		case cd_wire:
			qglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
			globalImages->BindNull();
			qglDisable( GL_BLEND );
			qglDisable( GL_DEPTH_TEST );
			qglColor3f( 1.0f, 1.0f, 1.0f );
			break;
		case cd_solid:
			qglCullFace( GL_FRONT );
			qglEnable( GL_CULL_FACE );
			qglShadeModel( GL_FLAT );
			qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
			globalImages->BindNull();
			qglDisable( GL_BLEND );
			qglEnable( GL_DEPTH_TEST );
			qglDepthFunc( GL_LEQUAL );
			break;
		case cd_texture:
			qglCullFace( GL_FRONT );
			qglEnable( GL_CULL_FACE );
			qglShadeModel( GL_FLAT );
			qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
			qglDisable( GL_BLEND );
			qglEnable( GL_DEPTH_TEST );
			qglDepthFunc( GL_LEQUAL );
			break;

		case cd_blend:
			qglCullFace( GL_FRONT );
			qglEnable( GL_CULL_FACE );
			qglShadeModel( GL_FLAT );
			qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
			qglDisable( GL_DEPTH_TEST );
			qglEnable( GL_BLEND );
			qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
			break;
	}
}

extern void glLabeledPoint( idVec4& color, idVec3& point, float size, const char* label );

/*
==================
DrawAxial
==================
*/
void DrawAxial( face_t* selFace )
{
	if( g_bAxialMode )
	{
		idVec3 points[4];

		for( int j = 0; j < selFace->face_winding->GetNumPoints(); j++ )
		{
			glLabeledPoint( idVec4( 1, 1, 1, 1 ), ( *selFace->face_winding )[j].ToVec3(), 3, va( "%i", j ) );
		}

		ValidateAxialPoints();
		points[0] = ( *selFace->face_winding )[g_axialAnchor].ToVec3();
		VectorMA( points[0], 1, selFace->plane, points[0] );
		VectorMA( points[0], 4, selFace->plane, points[1] );
		points[3] = ( *selFace->face_winding )[g_axialDest].ToVec3();
		VectorMA( points[3], 1, selFace->plane, points[3] );
		VectorMA( points[3], 4, selFace->plane, points[2] );
		glLabeledPoint( idVec4( 1, 0, 0, 1 ), points[1], 3, "Anchor" );
		glLabeledPoint( idVec4( 1, 1, 0, 1 ), points[2], 3, "Dest" );
		qglBegin( GL_LINE_STRIP );
		qglVertex3fv( points[0].ToFloatPtr() );
		qglVertex3fv( points[1].ToFloatPtr() );
		qglVertex3fv( points[2].ToFloatPtr() );
		qglVertex3fv( points[3].ToFloatPtr() );
		qglEnd();
	}
}

/*
 =======================================================================================================================
    Cam_Draw
 =======================================================================================================================
 */
/*
==================
SetProjectionMatrix
==================
*/
void CCamWnd::SetProjectionMatrix()
{
	float xfov = 90;
	float yfov = 2 * atan( ( float )m_Camera.height / m_Camera.width ) * idMath::M_RAD2DEG;
#if 0
	float screenaspect = ( float )m_Camera.width / m_Camera.height;
	qglLoadIdentity();
	gluPerspective( yfov, screenaspect, 2, 8192 );
#else
	float	xmin, xmax, ymin, ymax;
	float	width, height;
	float	zNear;
	float	projectionMatrix[16];

	//
	// set up projection matrix
	//
	zNear = r_znear.GetFloat();

	ymax = zNear * tan( yfov * idMath::PI / 360.0f );
	ymin = -ymax;

	xmax = zNear * tan( xfov * idMath::PI / 360.0f );
	xmin = -xmax;

	width = xmax - xmin;
	height = ymax - ymin;

	projectionMatrix[0] = 2 * zNear / width;
	projectionMatrix[4] = 0;
	projectionMatrix[8] = ( xmax + xmin ) / width;	// normally 0
	projectionMatrix[12] = 0;

	projectionMatrix[1] = 0;
	projectionMatrix[5] = 2 * zNear / height;
	projectionMatrix[9] = ( ymax + ymin ) / height;	// normally 0
	projectionMatrix[13] = 0;

	// this is the far-plane-at-infinity formulation
	projectionMatrix[2] = 0;
	projectionMatrix[6] = 0;
	projectionMatrix[10] = -1;
	projectionMatrix[14] = -2 * zNear;

	projectionMatrix[3] = 0;
	projectionMatrix[7] = 0;
	projectionMatrix[11] = -1;
	projectionMatrix[15] = 0;

	qglLoadMatrixf( projectionMatrix );
#endif
}

/*
==================
Cam_Draw
==================
*/
void CCamWnd::Cam_Draw()
{
	brush_t* brush;
	face_t*	face;

// ---> sikk - added - Translucent Brushes
	brush_t* transBrushes[ 65536 ];
	brush_t* transBrushesSel[ 65536 ];
	int		numTransBrushes = 0;
	int		numTransBrushesSel = 0;
// <--- sikk - added - Translucent Brushes

	// float yfov;
	int i;

	if( !active_brushes.next )
	{
		return;					// not valid yet
	}

	// set the sound origin for both simple draw and rendered mode
	// the editor uses opposite pitch convention
	idMat3	axis = idAngles( -m_Camera.angles.pitch, m_Camera.angles.yaw, m_Camera.angles.roll ).ToMat3();
	g_qeglobals.sw->PlaceListener( m_Camera.origin, axis, 0, Sys_Milliseconds(), "Undefined" );

	if( renderMode )
	{
		Cam_Render();
	}

	qglViewport( 0, 0, m_Camera.width, m_Camera.height );
	qglScissor( 0, 0, m_Camera.width, m_Camera.height );
	qglClearColor( g_qeglobals.d_savedinfo.colors[COLOR_CAMERABACK][0], g_qeglobals.d_savedinfo.colors[COLOR_CAMERABACK][1], g_qeglobals.d_savedinfo.colors[COLOR_CAMERABACK][2], 0 );

	if( !renderMode )
	{
		qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}

	qglDisable( GL_LIGHTING );
	qglMatrixMode( GL_PROJECTION );

	SetProjectionMatrix();

	qglRotatef( -90, 1, 0, 0 );	// put Z going up
	qglRotatef( 90, 0, 0, 1 );	// put Z going up
	qglRotatef( m_Camera.angles[0], 0, 1, 0 );
	qglRotatef( -m_Camera.angles[1], 0, 0, 1 );
	qglTranslatef( -m_Camera.origin[0], -m_Camera.origin[1], -m_Camera.origin[2] );

	Cam_BuildMatrix();

// ---> sikk - Show Map Boundry Box
	if( !renderMode )
	{
		Cam_DrawMapBounds();
	}
// <--- sikk - Show Map Boundry Box

	// Draw Normal Opaque Brushes
	for( brush = active_brushes.next; brush != &active_brushes; brush = brush->next )
	{
		if( CullBrush( brush, false ) )
		{
			continue;
		}
		if( FilterBrush( brush ) )
		{
			continue;
		}
		if( renderMode )
		{
			if( !( entityMode && brush->owner->eclass->fixedsize ) )
			{
				continue;
			}
		}
		setGLMode( m_Camera.draw_mode );

// ---> sikk - added - skip special brushes (clip, triggers, etc.) so we can draw them after with an alpha blend
		bool draw = true;
		if( !brush->owner->eclass->fixedsize )
		{
			for( face = brush->brush_faces; face; face = face->next )
			{
				if( face->d_texture && face->d_texture->GetEditorAlpha() < 1.0f )
				{
					draw = false;
					transBrushes[ numTransBrushes ] = brush;
					numTransBrushes++;
					break;
				}
			}
		}

		if( draw )
		{
			Brush_Draw( brush );
		}
// <--- sikk - added
	}

	// Draw Selected Opaque Brushes
	//qglDepthMask ( 1 ); // Ok, write now
	qglMatrixMode( GL_PROJECTION );
	qglTranslatef( g_qeglobals.d_select_translate[0], g_qeglobals.d_select_translate[1], g_qeglobals.d_select_translate[2] );

	brush_t* pList = ( g_bClipMode && g_pSplitList ) ? g_pSplitList : &selected_brushes;
	if( !renderMode )
	{
		// draw normally
		for( brush = pList->next; brush != pList; brush = brush->next )
		{
			if( brush->pPatch )
			{
				continue;
			}
			setGLMode( m_Camera.draw_mode );

// ---> sikk - added - draw special brushes (clip, triggers, etc.) with an alpha blend (fixedsized ents also when selected)
			bool draw = true;
			for( face = brush->brush_faces; face; face = face->next )
			{
				if( face->d_texture && face->d_texture->GetEditorAlpha() < 1.0f )
				{
					draw = false;
					transBrushesSel[ numTransBrushesSel ] = brush;
					numTransBrushesSel++;
					break;
				}
			}
			if( draw )
			{
				Brush_Draw( brush, true );
			}
// <--- sikk - added
		}

// ---> sikk - Camera Grid
		idVec3 selectedMins, selectedMaxs, snappedMins, snappedMaxs, snappedMins8, snappedMaxs8;
		int grid8 = g_qeglobals.d_gridsize * 8;
		selectedMins = pList->next->mins;
		selectedMaxs = pList->next->maxs;

		for( brush = pList->next; brush != pList; brush = brush->next )
		{
			for( i = 0; i < 3; i++ )
			{
				selectedMins[i] = min( selectedMins[i], brush->mins[i] );
				selectedMaxs[i] = max( selectedMaxs[i], brush->maxs[i] );
			}
		}
		for( i = 0; i < 3; i++ )
		{
			snappedMins[i] = ceil( selectedMins[i] / g_qeglobals.d_gridsize ) * g_qeglobals.d_gridsize + -grid8;
			snappedMaxs[i] = floor( selectedMaxs[i] / g_qeglobals.d_gridsize ) * g_qeglobals.d_gridsize + grid8;
			snappedMins8[i] = ceil( selectedMins[i] / grid8 ) * grid8 + -grid8;
			snappedMaxs8[i] = floor( selectedMaxs[i] / grid8 ) * grid8 + grid8;
		}

		if( selected_brushes.next != &selected_brushes )
		{
			qglColor3fv( g_qeglobals.d_savedinfo.colors[ COLOR_GRIDMINOR ].ToFloatPtr() );
			qglLineWidth( 1 );
			qglBegin( GL_LINES );
			qglVertex3f( selectedMins.x + -grid8, selectedMins.y, selectedMins.z );
			qglVertex3f( selectedMaxs.x + grid8, selectedMins.y, selectedMins.z );
			qglVertex3f( selectedMins.x + -grid8, selectedMins.y, selectedMaxs.z );
			qglVertex3f( selectedMaxs.x + grid8, selectedMins.y, selectedMaxs.z );
			qglVertex3f( selectedMins.x + -grid8, selectedMaxs.y, selectedMins.z );
			qglVertex3f( selectedMaxs.x + grid8, selectedMaxs.y, selectedMins.z );
			qglVertex3f( selectedMins.x + -grid8, selectedMaxs.y, selectedMaxs.z );
			qglVertex3f( selectedMaxs.x + grid8, selectedMaxs.y, selectedMaxs.z );

			qglVertex3f( selectedMins.x, selectedMins.y + -grid8, selectedMins.z );
			qglVertex3f( selectedMins.x, selectedMaxs.y + grid8, selectedMins.z );
			qglVertex3f( selectedMins.x, selectedMins.y + -grid8, selectedMaxs.z );
			qglVertex3f( selectedMins.x, selectedMaxs.y + grid8, selectedMaxs.z );
			qglVertex3f( selectedMaxs.x, selectedMins.y + -grid8, selectedMins.z );
			qglVertex3f( selectedMaxs.x, selectedMaxs.y + grid8, selectedMins.z );
			qglVertex3f( selectedMaxs.x, selectedMins.y + -grid8, selectedMaxs.z );
			qglVertex3f( selectedMaxs.x, selectedMaxs.y + grid8, selectedMaxs.z );

			qglVertex3f( selectedMins.x, selectedMins.y, selectedMins.z + -grid8 );
			qglVertex3f( selectedMins.x, selectedMins.y, selectedMaxs.z + grid8 );
			qglVertex3f( selectedMins.x, selectedMaxs.y, selectedMins.z + -grid8 );
			qglVertex3f( selectedMins.x, selectedMaxs.y, selectedMaxs.z + grid8 );
			qglVertex3f( selectedMaxs.x, selectedMins.y, selectedMins.z + -grid8 );
			qglVertex3f( selectedMaxs.x, selectedMins.y, selectedMaxs.z + grid8 );
			qglVertex3f( selectedMaxs.x, selectedMaxs.y, selectedMins.z + -grid8 );
			qglVertex3f( selectedMaxs.x, selectedMaxs.y, selectedMaxs.z + grid8 );
			qglEnd();

			qglColor3fv( g_qeglobals.d_savedinfo.colors[ COLOR_GRIDMAJOR ].ToFloatPtr() );
			float f;
			for( f = snappedMins.x; f <= snappedMaxs.x; f += g_qeglobals.d_gridsize )
			{
				if( f == snappedMins8.x || f == snappedMins8.x + grid8 ||
						f == snappedMaxs8.x || f == snappedMaxs8.x + -grid8 )
				{
					qglPointSize( 4 );
				}
				else
				{
					qglPointSize( 2 );
				}
				if( f <= selectedMins.x || f >= selectedMaxs.x )
				{
					qglBegin( GL_POINTS );
					qglVertex3f( f, selectedMins.y, selectedMins.z );
					qglVertex3f( f, selectedMins.y, selectedMaxs.z );
					qglVertex3f( f, selectedMaxs.y, selectedMins.z );
					qglVertex3f( f, selectedMaxs.y, selectedMaxs.z );
					qglEnd();
				}
			}
			for( f = snappedMins.y; f <= snappedMaxs.y; f += g_qeglobals.d_gridsize )
			{
				if( f == snappedMins8.y || f == snappedMins8.y + grid8 ||
						f == snappedMaxs8.y || f == snappedMaxs8.y + -grid8 )
				{
					qglPointSize( 4 );
				}
				else
				{
					qglPointSize( 2 );
				}
				if( f <= selectedMins.y || f >= selectedMaxs.y )
				{
					qglBegin( GL_POINTS );
					qglVertex3f( selectedMins.x, f, selectedMins.z );
					qglVertex3f( selectedMins.x, f, selectedMaxs.z );
					qglVertex3f( selectedMaxs.x, f, selectedMins.z );
					qglVertex3f( selectedMaxs.x, f, selectedMaxs.z );
					qglEnd();
				}
			}
			for( f = snappedMins.z; f <= snappedMaxs.z; f += g_qeglobals.d_gridsize )
			{
				if( f == snappedMins8.z || f == snappedMins8.z + grid8 ||
						f == snappedMaxs8.z || f == snappedMaxs8.z + -grid8 )
				{
					qglPointSize( 4 );
				}
				else
				{
					qglPointSize( 2 );
				}
				if( f <= selectedMins.z || f >= selectedMaxs.z )
				{
					qglBegin( GL_POINTS );
					qglVertex3f( selectedMins.x, selectedMins.y, f );
					qglVertex3f( selectedMins.x, selectedMaxs.y, f );
					qglVertex3f( selectedMaxs.x, selectedMins.y, f );
					qglVertex3f( selectedMaxs.x, selectedMaxs.y, f );
					qglEnd();
				}
			}
		}
// <--- sikk - Camera Grid/Axis
	}

	// Draw Selection Blend for Opaque Faces
	// blend on top
	setGLMode( m_Camera.draw_mode );
	qglDisable( GL_LIGHTING );
	GL_Cull( CT_TWO_SIDED );	// sikk - added
	qglEnable( GL_BLEND );
	qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	qglColor4f( g_qeglobals.d_savedinfo.colors[COLOR_SELBRUSHES][0], g_qeglobals.d_savedinfo.colors[COLOR_SELBRUSHES][1], g_qeglobals.d_savedinfo.colors[COLOR_SELBRUSHES][2], 0.25f );
	globalImages->BindNull();
	for( brush = pList->next; brush != pList; brush = brush->next )
	{
		if( brush->pPatch || brush->modelHandle > 0 )
		{
// ---> sikk - added - draw special brushes (clip, triggers, etc.) with an alpha blend (fixedsized ents also when selected)
			bool draw = true;
			for( face = brush->brush_faces; face; face = face->next )
			{
				if( face->d_texture && face->d_texture->GetEditorAlpha() < 1.0f )
				{
					draw = false;
					break;
				}
			}
			if( draw )
			{
				Brush_Draw( brush, true );
			}
// <--- sikk - added

			// DHM - Nerve:: patch display lists/models mess with the state
			qglEnable( GL_BLEND );
			qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
			qglColor4f( g_qeglobals.d_savedinfo.colors[COLOR_SELBRUSHES][0], g_qeglobals.d_savedinfo.colors[COLOR_SELBRUSHES][1], g_qeglobals.d_savedinfo.colors[COLOR_SELBRUSHES][2], 0.25f );
			globalImages->BindNull();
			continue;
		}

		if( brush->owner->eclass->entityModel )
		{
			continue;
		}

		for( face = brush->brush_faces; face; face = face->next )
		{
			if( face->d_texture && face->d_texture->GetEditorAlpha() >= 1.0f )  	// sikk - Added
			{
				Face_Draw( face );
			}
		}
	}

	int nCount = g_ptrSelectedFaces.GetSize();
	if( !renderMode )
	{
		for( int i = 0; i < nCount; i++ )
		{
			face_t*	selFace = reinterpret_cast<face_t*>( g_ptrSelectedFaces.GetAt( i ) );
			if( selFace->d_texture && selFace->d_texture->GetEditorAlpha() >= 1.0f )  	// sikk - Added
			{
				Face_Draw( selFace );
				DrawAxial( selFace );
			}
		}
	}
	GL_Cull( CT_FRONT_SIDED );	// sikk - added

// ---> sikk - added - loop again to draw the alpha blended brushes
	//setGLMode( m_Camera.draw_mode );
	//qglEnable( GL_LIGHTING );
	// Draw Normal Alpha Blended Brushes
//			GL_State( GLS_DEPTHMASK | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_LESS  );
	for( int i = 0; i < numTransBrushes; i++ )
	{
		qglEnable( GL_BLEND );
		qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		qglDepthMask( GL_FALSE );
		Brush_Draw( transBrushes[ i ], false, true );
	}

	// Draw Selected Alpha Blended Brushes
	for( int i = 0; i < numTransBrushesSel; i++ )
	{
		qglEnable( GL_BLEND );
		qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
		qglDepthMask( GL_FALSE );
		Brush_Draw( transBrushesSel[ i ], true, true );
	}

	// Draw Selection Blend for Alpha Blended Faces
	// blend on top
	setGLMode( m_Camera.draw_mode );
	GL_Cull( CT_TWO_SIDED );	// sikk - added
	qglDisable( GL_LIGHTING );
	qglEnable( GL_BLEND );
	qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	qglColor4f( g_qeglobals.d_savedinfo.colors[COLOR_SELBRUSHES][0], g_qeglobals.d_savedinfo.colors[COLOR_SELBRUSHES][1], g_qeglobals.d_savedinfo.colors[COLOR_SELBRUSHES][2], 0.25f );
	globalImages->BindNull();
	for( int i = 0; i < numTransBrushesSel; i++ )
	{
		if( transBrushesSel[ i ]->pPatch || transBrushesSel[ i ]->modelHandle > 0 )
		{
			bool draw = false;
			for( face = brush->brush_faces; face; face = face->next )
			{
				if( face->d_texture && face->d_texture->GetEditorAlpha() < 1.0f )
				{
					draw = true;
					break;
				}
			}
			if( draw )
			{
				Brush_Draw( brush, true, true );
			}

			// DHM - Nerve:: patch display lists/models mess with the state
			qglEnable( GL_BLEND );
			qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
			qglColor4f( g_qeglobals.d_savedinfo.colors[COLOR_SELBRUSHES][0], g_qeglobals.d_savedinfo.colors[COLOR_SELBRUSHES][1], g_qeglobals.d_savedinfo.colors[COLOR_SELBRUSHES][2], 0.25f );
			globalImages->BindNull();
			continue;
		}
		if( transBrushesSel[ i ]->owner->eclass->entityModel )
		{
			continue;
		}
		for( face = transBrushesSel[ i ]->brush_faces; face; face = face->next )
		{
			Face_Draw( face );
		}
	}

	if( !renderMode )
	{
		for( int i = 0; i < nCount; i++ )
		{
			face_t*	selFace = reinterpret_cast<face_t*>( g_ptrSelectedFaces.GetAt( i ) );
			if( selFace->d_texture && selFace->d_texture->GetEditorAlpha() < 1.0f )  	// sikk - Added
			{
				Face_Draw( selFace );
				DrawAxial( selFace );
			}
		}
	}
	GL_Cull( CT_FRONT_SIDED );	// sikk - added

// ---> sikk - Camera Axis
	if( !renderMode )
	{
		Cam_DrawWorldAxis();
	}
// <--- sikk - Camera Axis

	// non-zbuffered outline
	qglDisable( GL_BLEND );
	qglDisable( GL_DEPTH_TEST );
	qglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

	if( renderMode )
	{
		qglColor3f( 1, 0, 0 );
		for( int i = 0; i < nCount; i++ )
		{
			face_t* selFace = reinterpret_cast<face_t*>( g_ptrSelectedFaces.GetAt( i ) );
			Face_Draw( selFace );
		}
	}

	GL_Cull( CT_TWO_SIDED );	// sikk - added
	qglColor3f( 1, 1, 1 );
	for( brush = pList->next; brush != pList; brush = brush->next )
	{
		if( brush->pPatch || brush->modelHandle > 0 )
		{
			continue;
		}

		for( face = brush->brush_faces; face; face = face->next )
		{
			Face_Draw( face );
		}
	}
	GL_Cull( CT_FRONT_SIDED );	// sikk - added

	// edge / vertex flags
	if( g_qeglobals.d_select_mode == sel_vertex )
	{
		qglPointSize( 4 );
		qglColor3f( 0, 1, 0 );
		qglBegin( GL_POINTS );
		for( i = 0; i < g_qeglobals.d_numpoints; i++ )
		{
			qglVertex3fv( g_qeglobals.d_points[i].ToFloatPtr() );
		}

		qglEnd();
		qglPointSize( 1 );
	}
	else if( g_qeglobals.d_select_mode == sel_edge )
	{
		float*	v1, *v2;

		qglPointSize( 4 );
		qglColor3f( 0, 0, 1 );
		qglBegin( GL_POINTS );
		for( i = 0; i < g_qeglobals.d_numedges; i++ )
		{
			v1 = g_qeglobals.d_points[g_qeglobals.d_edges[i].p1].ToFloatPtr();
			v2 = g_qeglobals.d_points[g_qeglobals.d_edges[i].p2].ToFloatPtr();
			qglVertex3f( ( v1[0] + v2[0] ) * 0.5f, ( v1[1] + v2[1] ) * 0.5f, ( v1[2] + v2[2] ) * 0.5f );
		}

		qglEnd();
		qglPointSize( 1 );
	}

	g_splineList->draw( static_cast<bool>( g_qeglobals.d_select_mode == sel_addpoint || g_qeglobals.d_select_mode == sel_editpoint ) );

	if( g_qeglobals.selectObject && ( g_qeglobals.d_select_mode == sel_addpoint || g_qeglobals.d_select_mode == sel_editpoint ) )
	{
		g_qeglobals.selectObject->drawSelection();
	}

	// draw pointfile
	qglEnable( GL_DEPTH_TEST );

	DrawPathLines();

	if( g_qeglobals.d_pointfile_display_list )
	{
		Pointfile_Draw();
	}

// ---> sikk - Draw Clip Points in Camera
	if( g_bClipMode )
	{
		Cam_DrawClipPoints();
	}
// <--- sikk - Draw Clip Points in Camera

// ---> sikk - Camera View Axis
	if( !renderMode )
	{
		Cam_DrawCameraAxis();
	}
// <--- sikk - Camera View Axis

	//
	// bind back to the default texture so that we don't have problems elsewhere
	// using/modifying texture maps between contexts
	//
	globalImages->BindNull();

	qglFinish();
	QE_CheckOpenGLForErrors();

	if( !renderMode )
	{
		// clean up any deffered tri's
		R_ToggleSmpFrame();
	}
}

// ---> sikk - Various Draw Functions

/*
==================
Cam_DrawMapBounds
==================
*/
void CCamWnd::Cam_DrawMapBounds()
{
	qglBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	qglEnable( GL_BLEND );
	qglColor4f( g_qeglobals.d_savedinfo.colors[COLOR_GRIDMAJOR][0], g_qeglobals.d_savedinfo.colors[COLOR_GRIDMAJOR][1], g_qeglobals.d_savedinfo.colors[COLOR_GRIDMAJOR][2], 0.25f );
	glBegin( GL_LINE_LOOP );
	glVertex3f( -131072, -131072, -131072 );
	glVertex3f( 131072, -131072, -131072 );
	glVertex3f( 131072, 131072, -131072 );
	glVertex3f( -131072, 131072, -131072 );
	glEnd();

	glBegin( GL_LINE_LOOP );
	glVertex3f( -131072, -131072, 131072 );
	glVertex3f( 131072, -131072, 131072 );
	glVertex3f( 131072, 131072, 131072 );
	glVertex3f( -131072, 131072, 131072 );
	glEnd();

	glBegin( GL_LINES );
	glVertex3f( -131072, -131072, -131072 );
	glVertex3f( -131072, -131072, 131072 );
	glVertex3f( 131072, -131072, -131072 );
	glVertex3f( 131072, -131072, 131072 );
	glVertex3f( -131072, 131072, -131072 );
	glVertex3f( -131072, 131072, 131072 );
	glVertex3f( 131072, 131072, -131072 );
	glVertex3f( 131072, 131072, 131072 );
	glEnd();
	qglDisable( GL_BLEND );
}

/*
==================
Cam_DrawWorldAxis
==================
*/
void CCamWnd::Cam_DrawWorldAxis()
{
	qglColor4f( 1, 0, 0, 0.25f );
	qglBegin( GL_LINES );
	qglVertex2i( -131072, 0 );
	qglVertex2i( 0, 0 );
	qglVertex3f( m_Camera.origin.x, m_Camera.origin.y, m_Camera.origin.z );
	qglVertex3f( m_Camera.origin.x, m_Camera.origin.x, m_Camera.origin.x );
	qglEnd();
	qglColor4f( 1, 0, 0, 0.75f );
	qglBegin( GL_LINES );
	qglVertex2i( 0, 0 );
	qglVertex2i( 131072, 0 );
	qglEnd();

	qglColor4f( 0, 1, 0, 0.25f );
	qglBegin( GL_LINES );
	qglVertex2i( 0, -131072 );
	qglVertex2i( 0, 0 );
	qglEnd();
	qglColor4f( 0, 1, 0, 0.75f );
	qglBegin( GL_LINES );
	qglVertex2i( 0, 0 );
	qglVertex2i( 0, 131072 );
	qglEnd();

	qglColor4f( 0, 0, 1, 0.25f );
	qglBegin( GL_LINES );
	qglVertex3i( 0, 0, -131072 );
	qglVertex3i( 0, 0, 0 );
	qglEnd();
	qglColor4f( 0, 0, 1, 0.75f );
	qglBegin( GL_LINES );
	qglVertex3i( 0, 0, 0 );
	qglVertex3i( 0, 0, 131072 );
	qglEnd();
	qglDisable( GL_BLEND );
}

/*
==================
Cam_DrawCameraAxis
==================
*/
void CCamWnd::Cam_DrawCameraAxis()
{
	idVec3 org = m_Camera.vpn * 0.0625f;

	qglLoadIdentity();
	qglMatrixMode( GL_PROJECTION );
	qglPushMatrix();
	qglLoadIdentity();
	qglOrtho( -48 * 0.0625f, ( m_Camera.width - 48 ) * 0.0625f, ( m_Camera.height - 48 ) * 0.0625f, -48 * 0.0625f, -1, 2 );
	qglRotatef( 90, 1, 0, 0 );	// put Z going up
	qglRotatef( -90, 0, 0, 1 );	// put Z going up
	qglRotatef( m_Camera.angles[0], 0, 1, 0 );
	qglRotatef( m_Camera.angles[1], 0, 0, 1 );

	qglLineWidth( 2 );
	qglColor3f( 1, 0, 0 );
	qglBegin( GL_LINES );
	qglVertex3f( org.x, org.y, org.z );
	qglVertex3f( org.x + 1, org.y, org.z );
	qglEnd();

	qglColor3f( 0, 1, 0 );
	qglBegin( GL_LINES );
	qglVertex3f( org.x, org.y, org.z );
	qglVertex3f( org.x, org.y + -1, org.z );
	qglEnd();

	qglColor3f( 0, 0, 1 );
	qglBegin( GL_LINES );
	qglVertex3f( org.x, org.y, org.z );
	qglVertex3f( org.x, org.y, org.z + 1 );
	qglEnd();
	qglLineWidth( 1 );

	qglPopMatrix();
	qglMatrixMode( GL_MODELVIEW );
}

/*
==================
Cam_DrawClipPoints
==================
*/
void CCamWnd::Cam_DrawClipPoints()
{
	qglEnable( GL_DEPTH_TEST );
	qglPointSize( 8 );
	qglColor3fv( g_qeglobals.d_savedinfo.colors[COLOR_CLIPPER].ToFloatPtr() );
	qglBegin( GL_POINTS );
	if( g_Clip1.Set() )
	{
		qglVertex3fv( g_Clip1 );
	}
	if( g_Clip2.Set() )
	{
		qglVertex3fv( g_Clip2 );
	}
	if( g_Clip3.Set() )
	{
		qglVertex3fv( g_Clip3 );
	}
	qglEnd();

	qglBegin( GL_LINE_LOOP );
	if( g_Clip1.Set() )
	{
		qglVertex3fv( g_Clip1 );
	}
	if( g_Clip2.Set() )
	{
		qglVertex3fv( g_Clip2 );
	}
	if( g_Clip3.Set() )
	{
		qglVertex3fv( g_Clip3 );
	}
	qglEnd();

	if( g_Clip3.Set() )
	{
		qglEnable( GL_BLEND );
		GL_Cull( CT_TWO_SIDED );
		qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		qglColor4f( g_qeglobals.d_savedinfo.colors[COLOR_CLIPPER][0], g_qeglobals.d_savedinfo.colors[COLOR_CLIPPER][1], g_qeglobals.d_savedinfo.colors[COLOR_CLIPPER][2], 0.25f );
		qglBegin( GL_TRIANGLES );
		if( g_Clip1.Set() )
		{
			qglVertex3fv( g_Clip1 );
		}
		if( g_Clip2.Set() )
		{
			qglVertex3fv( g_Clip2 );
		}
		if( g_Clip3.Set() )
		{
			qglVertex3fv( g_Clip3 );
		}
		GL_Cull( CT_FRONT_SIDED );
		qglEnd();
	}

	qglEnd();

	qglColor3fv( g_qeglobals.d_savedinfo.colors[COLOR_CLIPPER].ToFloatPtr() );
	qglPointSize( 8 );
	CString strMsg;
	if( g_Clip1.Set() )
	{
		qglRasterPos3f( g_Clip1.m_ptClip[0] + 4, g_Clip1.m_ptClip[1] + 4, g_Clip1.m_ptClip[2] + 4 );
		strMsg = "1";
		// strMsg.Format("1 (%f, %f, %f)", g_Clip1[0], g_Clip1[1], g_Clip1[2]);
		qglCallLists( strMsg.GetLength(), GL_UNSIGNED_BYTE, strMsg );
	}
	if( g_Clip2.Set() )
	{
		qglRasterPos3f( g_Clip2.m_ptClip[0] + 4, g_Clip2.m_ptClip[1] + 4, g_Clip2.m_ptClip[2] + 4 );
		strMsg = "2";
		// strMsg.Format("2 (%f, %f, %f)", g_Clip2[0], g_Clip2[1], g_Clip2[2]);
		qglCallLists( strMsg.GetLength(), GL_UNSIGNED_BYTE, strMsg );
	}
	if( g_Clip3.Set() )
	{
		qglRasterPos3f( g_Clip3.m_ptClip[0] + 4, g_Clip3.m_ptClip[1] + 4, g_Clip3.m_ptClip[2] + 4 );
		strMsg = "3";
		// strMsg.Format("3 (%f, %f, %f)", g_Clip3[0], g_Clip3[1], g_Clip3[2]);
		qglCallLists( strMsg.GetLength(), GL_UNSIGNED_BYTE, strMsg );
	}
}
// <--- sikk - Various Draw Functions

void CCamWnd::OnSize( UINT nType, int cx, int cy )
{
	CWnd::OnSize( nType, cx, cy );

	CRect	rect;
	GetClientRect( rect );
	m_Camera.width = rect.right;
	m_Camera.height = rect.bottom;
	InvalidateRect( NULL, false );
}

// ---> sikk - Window Snapping
void CCamWnd::OnSizing( UINT nSide, LPRECT lpRect )
{
	if( TryDocking( GetSafeHwnd(), nSide, lpRect, 0 ) )
	{
		return;
	}
}
void CCamWnd::OnMoving( UINT nSide, LPRECT lpRect )
{
	if( TryDocking( GetSafeHwnd(), nSide, lpRect, 0 ) )
	{
		return;
	}
}
// <--- sikk - Window Snapping


/*
 =======================================================================================================================
 =======================================================================================================================
 */
void CCamWnd::OnKeyUp( UINT nChar, UINT nRepCnt, UINT nFlags )
{
	g_pParentWnd->HandleKey( nChar, nRepCnt, nFlags, false );
}

//
// =======================================================================================================================
//    Timo brush primitive texture shifting, using camera view to select translations::
// =======================================================================================================================
//
/*
==================
ShiftTexture_BrushPrimit
==================
*/
void CCamWnd::ShiftTexture_BrushPrimit( face_t* f, int x, int y )
{
	/*
		idVec3	texS, texT;
		idVec3	viewX, viewY;
		int		XS, XT, YS, YT;
		int		outS, outT;
	#ifdef _DEBUG
		if (!g_qeglobals.m_bBrushPrimitMode) {
			common->Printf("Warning : unexpected call to CCamWnd::ShiftTexture_BrushPrimit with brush primitive mode disbaled\n");
			return;
		}
	#endif
		// compute face axis base
		//ComputeAxisBase(f->plane.Normal(), texS, texT);

		// compute camera view vectors
		VectorCopy(m_Camera.vup, viewY);
		VectorCopy(m_Camera.vright, viewX);

		// compute best vectors
		//ComputeBest2DVector(viewX, texS, texT, XS, XT);
		//ComputeBest2DVector(viewY, texS, texT, YS, YT);

		// check this is not a degenerate case
		if ((XS == YS) && (XT == YT))
		{
	#ifdef _DEBUG
			common->Printf("Warning : degenerate best vectors axis base in CCamWnd::ShiftTexture_BrushPrimit\n");
	#endif
			// forget it
			Select_ShiftTexture_BrushPrimit(f, x, y, false);
			return;
		}

		// compute best fitted translation in face axis base
		outS = XS * x + YS * y;
		outT = XT * x + YT * y;

		// call actual texture shifting code
		Select_ShiftTexture_BrushPrimit(f, outS, outT, false);
	*/
}

/*
==================
IsBModel
==================
*/
bool IsBModel( brush_t* b )
{
	const char* v = ValueForKey( b->owner, "model" );
	if( v && *v )
	{
		const char* n = ValueForKey( b->owner, "name" );
		return ( stricmp( n, v ) == 0 );
	}
	return false;
}

/*
================
BuildEntityRenderState

Creates or updates modelDef and lightDef for an entity
================
*/
int Brush_ToTris( brush_t* brush, idTriList* tris, idMatList* mats, bool models, bool bmodel );

void CCamWnd::BuildEntityRenderState( entity_t* ent, bool update )
{
	const char*	v;
	idDict		spawnArgs;
	const char*	name = NULL;

	Entity_UpdateSoundEmitter( ent );

	// delete the existing def if we aren't creating a brand new world
	if( !update )
	{
		if( ent->lightDef >= 0 )
		{
			g_qeglobals.rw->FreeLightDef( ent->lightDef );
			ent->lightDef = -1;
		}
		if( ent->modelDef >= 0 )
		{
			g_qeglobals.rw->FreeEntityDef( ent->modelDef );
			ent->modelDef = -1;
		}
	}

	// if an entity doesn't have any brushes at all, don't do anything
	if( ent->brushes.onext == &ent->brushes )
	{
		return;
	}

	// if the brush isn't displayed (filtered or culled), don't do anything
	if( FilterBrush( ent->brushes.onext ) )
	{
		return;
	}

	spawnArgs = ent->epairs;
	if( ent->eclass->defArgs.FindKey( "model" ) )
	{
		spawnArgs.Set( "model", ent->eclass->defArgs.GetString( "model" ) );
	}

	// any entity can have a model
	name = ValueForKey( ent, "name" );
	v = spawnArgs.GetString( "model" );
	if( v && *v )
	{
		renderEntity_t	refent;

		refent.referenceSound = ent->soundEmitter;

		if( !stricmp( name, v ) )
		{
			// build the model from brushes
			idTriList tris( 1024 );
			idMatList mats( 1024 );

			for( brush_t* b = ent->brushes.onext; b != &ent->brushes; b = b->onext )
			{
				Brush_ToTris( b, &tris, &mats, false, true );
			}

			if( ent->modelDef >= 0 )
			{
				g_qeglobals.rw->FreeEntityDef( ent->modelDef );
				ent->modelDef = -1;
			}

			idRenderModel* bmodel = renderModelManager->FindModel( name );

			if( bmodel )
			{
				renderModelManager->RemoveModel( bmodel );
				renderModelManager->FreeModel( bmodel );
			}

			bmodel = renderModelManager->AllocModel();

			bmodel->InitEmpty( name );

			// add the surfaces to the renderModel
			modelSurface_t	surf;
			for( int i = 0 ; i < tris.Num() ; i++ )
			{
				surf.geometry = tris[i];
				surf.shader = mats[i];
				bmodel->AddSurface( surf );
			}

			bmodel->FinishSurfaces();

			renderModelManager->AddModel( bmodel );

			// FIXME: brush entities
			gameEdit->ParseSpawnArgsToRenderEntity( &spawnArgs, &refent );

			ent->modelDef = g_qeglobals.rw->AddEntityDef( &refent );

		}
		else
		{
			// use the game's epair parsing code so
			// we can use the same renderEntity generation
			gameEdit->ParseSpawnArgsToRenderEntity( &spawnArgs, &refent );
			idRenderModelMD5* md5 = dynamic_cast<idRenderModelMD5*>( refent.hModel );
			if( md5 )
			{
				idStr str;
				spawnArgs.GetString( "anim", "idle", str );
				refent.numJoints = md5->NumJoints();
				if( update && refent.joints )
				{
					Mem_Free16( refent.joints );
				}
				refent.joints = ( idJointMat* )Mem_Alloc16( refent.numJoints * sizeof( *refent.joints ) );
				const idMD5Anim* anim = gameEdit->ANIM_GetAnimFromEntityDef( spawnArgs.GetString( "classname" ), str );
				int frame = spawnArgs.GetInt( "frame" ) + 1;
				if( frame < 1 )
				{
					frame = 1;
				}
				const idVec3& offset = gameEdit->ANIM_GetModelOffsetFromEntityDef( spawnArgs.GetString( "classname" ) );
				gameEdit->ANIM_CreateAnimFrame( md5, anim, refent.numJoints, refent.joints, ( frame * 1000 ) / 24, offset, false );
			}
			if( ent->modelDef >= 0 )
			{
				g_qeglobals.rw->FreeEntityDef( ent->modelDef );
			}
			ent->modelDef = g_qeglobals.rw->AddEntityDef( &refent );
		}
	}

	// check for lightdefs
	if( !( ent->eclass->nShowFlags & ECLASS_LIGHT ) )
	{
		return;
	}

	if( spawnArgs.GetBool( "start_off" ) )
	{
		return;
	}
	// use the game's epair parsing code so
	// we can use the same renderLight generation

	renderLight_t	lightParms;

	gameEdit->ParseSpawnArgsToRenderLight( &spawnArgs, &lightParms );
	lightParms.referenceSound = ent->soundEmitter;

	if( update && ent->lightDef >= 0 )
	{
		g_qeglobals.rw->UpdateLightDef( ent->lightDef, &lightParms );
	}
	else
	{
		if( ent->lightDef >= 0 )
		{
			g_qeglobals.rw->FreeLightDef( ent->lightDef );
		}
		ent->lightDef = g_qeglobals.rw->AddLightDef( &lightParms );
	}

}

/*
==================
Tris_ToOBJ
==================
*/
void Tris_ToOBJ( const char* outFile, idTriList* tris, idMatList* mats )
{
	idFile* f = fileSystem->OpenExplicitFileWrite( outFile );
	if( f )
	{
		char out[1024];
		strcpy( out, outFile );
		StripExtension( out );

		idList<idStr*> matNames;
		int i, j, k;
		int indexBase = 1;
		idStr lastMaterial( "" );
		int matCount = 0;
		//idStr basePath = cvarSystem->GetCVarString( "fs_savepath" );
		f->Printf( "mtllib %s.mtl\n", out );
		for( i = 0; i < tris->Num(); i++ )
		{
			srfTriangles_t* tri = ( *tris )[i];
			for( j = 0; j < tri->numVerts; j++ )
			{
				f->Printf( "v %f %f %f\n", tri->verts[j].xyz.x, tri->verts[j].xyz.z, -tri->verts[j].xyz.y );
			}
			for( j = 0; j < tri->numVerts; j++ )
			{
				f->Printf( "vt %f %f\n", tri->verts[j].st.x, 1.0f - tri->verts[j].st.y );
			}
			for( j = 0; j < tri->numVerts; j++ )
			{
				f->Printf( "vn %f %f %f\n", tri->verts[j].normal.x, tri->verts[j].normal.y, tri->verts[j].normal.z );
			}

			if( stricmp( ( *mats )[i]->GetName(), lastMaterial ) )
			{
				lastMaterial = ( *mats )[i]->GetName();

				bool found = false;
				for( k = 0; k < matNames.Num(); k++ )
				{
					if( idStr::Icmp( matNames[k]->c_str(), lastMaterial.c_str() ) == 0 )
					{
						found = true;
						// f->Printf( "usemtl m%i\n", k );
						f->Printf( "usemtl %s\n", lastMaterial.c_str() );
						break;
					}
				}

				if( !found )
				{
					// f->Printf( "usemtl m%i\n", matCount++ );
					f->Printf( "usemtl %s\n", lastMaterial.c_str() );
					matNames.Append( new idStr( lastMaterial ) );
				}
			}

			for( int j = 0; j < tri->numIndexes; j += 3 )
			{
				int i1, i2, i3;
				i1 = tri->indexes[j + 2] + indexBase;
				i2 = tri->indexes[j + 1] + indexBase;
				i3 = tri->indexes[j] + indexBase;
				f->Printf( "f %i/%i/%i %i/%i/%i %i/%i/%i\n", i1, i1, i1, i2, i2, i2, i3, i3, i3 );
			}

			indexBase += tri->numVerts;

		}
		fileSystem->CloseFile( f );

		strcat( out, ".mtl" );
		f = fileSystem->OpenExplicitFileWrite( out );
		if( f )
		{
			for( k = 0; k < matNames.Num(); k++ )
			{
				// This presumes the diffuse tga name matches the material name
				f->Printf( "newmtl %s\n\tNs 0\n\td 1\n\tillum 2\n\tKd 0 0 0 \n\tKs 0.22 0.22 0.22 \n\tKa 0 0 0 \n\tmap_Kd %s/base/%s.tga\n\n\n", matNames[k]->c_str(), "z:/d3xp", matNames[k]->c_str() );
			}
			fileSystem->CloseFile( f );
		}

	}
}

/*
==================
Brush_TransformModel
==================
*/
int Brush_TransformModel( brush_t* brush, idTriList* tris, idMatList* mats )
{
	int ret = 0;
	if( brush->modelHandle > 0 )
	{
		idRenderModel* model = brush->modelHandle;
		if( model )
		{
			float	a = FloatForKey( brush->owner, "angle" );
			float	s, c;
			//FIXME: support full rotation matrix
			bool matrix = false;
			if( a )
			{
				s = sin( DEG2RAD( a ) );
				c = cos( DEG2RAD( a ) );
			}
			idMat3 mat;
			if( GetMatrixForKey( brush->owner, "rotation", mat ) )
			{
				matrix = true;
			}


			for( int i = 0; i < model->NumSurfaces() ; i++ )
			{
				const modelSurface_t*	surf = model->Surface( i );
				srfTriangles_t*	tri = surf->geometry;
				srfTriangles_t* tri2 = R_CopyStaticTriSurf( tri );
				for( int j = 0; j < tri2->numVerts; j++ )
				{
					idVec3	v;
					if( matrix )
					{
						v = tri2->verts[j].xyz * brush->owner->rotation + brush->owner->origin;
					}
					else
					{
						v = tri2->verts[j].xyz;
						VectorAdd( v, brush->owner->origin, v );
						float x = v[0];
						float y = v[1];
						if( a )
						{
							float	x2 = ( ( ( x - brush->owner->origin[0] ) * c ) - ( ( y - brush->owner->origin[1] ) * s ) ) + brush->owner->origin[0];
							float	y2 = ( ( ( x - brush->owner->origin[0] ) * s ) + ( ( y - brush->owner->origin[1] ) * c ) ) + brush->owner->origin[1];
							x = x2;
							y = y2;
						}
						v[0] = x;
						v[1] = y;
					}
					tri2->verts[j].xyz = v;
				}
				tris->Append( tri2 );
				mats->Append( surf->shader );
			}
			return model->NumSurfaces();
		}
	}
	return ret;
}

#define	MAX_TRI_SURFACES	16384
/*
==================
Brush_ToTris
==================
*/
int Brush_ToTris( brush_t* brush, idTriList* tris, idMatList* mats, bool models, bool bmodel )
{
	int i, j;
	srfTriangles_t*	tri;
	//
	// patches
	//
	if( brush->modelHandle > 0 )
	{
		if( !models )
		{
			return 0;
		}
		else
		{
			return Brush_TransformModel( brush, tris, mats );
		}
	}

	int numSurfaces = 0;

	if( brush->owner->eclass->fixedsize && !brush->entityModel )
	{
		return NULL;
	}

	if( brush->pPatch )
	{
		patchMesh_t* pm;
		int width, height;

		pm = brush->pPatch;

		// build a patch mesh
		idSurface_Patch* cp = new idSurface_Patch( pm->width * 6, pm->height * 6 );
		cp->SetSize( pm->width, pm->height );
		for( i = 0; i < pm->width; i++ )
		{
			for( j = 0; j < pm->height; j++ )
			{
				( *cp )[ j * cp->GetWidth() + i ].xyz = pm->ctrl( i, j ).xyz;
				( *cp )[ j * cp->GetWidth() + i ].st = pm->ctrl( i, j ).st;
			}
		}

		// subdivide it
		if( pm->explicitSubdivisions )
		{
			cp->SubdivideExplicit( pm->horzSubdivisions, pm->vertSubdivisions, true );
		}
		else
		{
			cp->Subdivide( DEFAULT_CURVE_MAX_ERROR, DEFAULT_CURVE_MAX_ERROR, DEFAULT_CURVE_MAX_LENGTH, true );
		}
		width = cp->GetWidth();
		height = cp->GetHeight();

		// convert to srfTriangles
		tri = R_AllocStaticTriSurf();
		tri->numVerts = width * height;
		tri->numIndexes = 6 * ( width - 1 ) * ( height - 1 );
		R_AllocStaticTriSurfVerts( tri, tri->numVerts );
		R_AllocStaticTriSurfIndexes( tri, tri->numIndexes );
		for( i = 0; i < tri->numVerts; i++ )
		{
			tri->verts[i] = ( *cp )[i];
			if( bmodel )
			{
				tri->verts[i].xyz -= brush->owner->origin;
			}
		}

		tri->numIndexes = 0;
		for( i = 1; i < width; i++ )
		{
			for( j = 1; j < height; j++ )
			{
				tri->indexes[tri->numIndexes++] = ( j - 1 ) * width + i;
				tri->indexes[tri->numIndexes++] = ( j - 1 ) * width + i - 1;
				tri->indexes[tri->numIndexes++] = j * width + i - 1;

				tri->indexes[tri->numIndexes++] = j * width + i;
				tri->indexes[tri->numIndexes++] = ( j - 1 ) * width + i;
				tri->indexes[tri->numIndexes++] = j * width + i - 1;
			}
		}

		delete cp;

		tris->Append( tri );
		mats->Append( pm->d_texture );
		//surfaces[numSurfaces] = tri;
		//materials[numSurfaces] = pm->d_texture;
		return 1;
	}

	//
	// normal brush
	//
	for( face_t* face = brush->brush_faces; face; face = face->next )
	{
		idWinding* w;

		w = face->face_winding;
		if( !w )
		{
			continue;	// freed or degenerate face
		}

		tri = R_AllocStaticTriSurf();
		tri->numVerts = w->GetNumPoints();
		tri->numIndexes = ( w->GetNumPoints() - 2 ) * 3;
		R_AllocStaticTriSurfVerts( tri, tri->numVerts );
		R_AllocStaticTriSurfIndexes( tri, tri->numIndexes );

		for( i = 0; i < tri->numVerts; i++ )
		{

			tri->verts[i].Clear();

			tri->verts[i].xyz[0] = ( *w )[i][0];
			tri->verts[i].xyz[1] = ( *w )[i][1];
			tri->verts[i].xyz[2] = ( *w )[i][2];

			if( bmodel )
			{
				tri->verts[i].xyz -= brush->owner->origin;
			}

			tri->verts[i].st[0] = ( *w )[i][3];
			tri->verts[i].st[1] = ( *w )[i][4];

			tri->verts[i].normal = face->plane.Normal();
		}

		tri->numIndexes = 0;
		for( i = 2; i < w->GetNumPoints(); i++ )
		{
			tri->indexes[tri->numIndexes++] = 0;
			tri->indexes[tri->numIndexes++] = i - 1;
			tri->indexes[tri->numIndexes++] = i;
		}

		tris->Append( tri );
		mats->Append( face->d_texture );
		numSurfaces++;
	}

	return numSurfaces;
}

/*
==================
Select_ToOBJ
==================
*/
void Select_ToOBJ()
{
	int i;
	CFileDialog dlgFile( FALSE, "obj", NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, "Wavefront object files (*.obj)|*.obj||", g_pParentWnd );
	if( dlgFile.DoModal() == IDOK )
	{
		idTriList tris( 1024 );
		idMatList mats( 1024 );

		for( brush_t* b = selected_brushes.next; b != &selected_brushes; b = b->next )
		{

			if( b->hiddenBrush )
			{
				continue;
			}

			if( FilterBrush( b ) )
			{
				continue;
			}

			Brush_ToTris( b, &tris, &mats, true, false );
		}

		Tris_ToOBJ( dlgFile.GetPathName().GetBuffer( 0 ), &tris, &mats );

		for( i = 0; i < tris.Num(); i++ )
		{
			R_FreeStaticTriSurf( tris[i] );
		}
		tris.Clear();
	}
}

/*
==================
Select_ToCM
==================
*/
void Select_ToCM()
{
	CFileDialog dlgFile( FALSE, "lwo, ase", NULL, 0, "(*.lwo)|*.lwo|(*.ase)|*.ase|(*.ma)|*.ma||", g_pParentWnd );

	if( dlgFile.DoModal() == IDOK )
	{
		idMapEntity* mapEnt;
		idMapPrimitive* p;
		idStr name;

		name = fileSystem->OSPathToRelativePath( dlgFile.GetPathName() );
		name.BackSlashesToSlashes();

		mapEnt = new idMapEntity();
		mapEnt->epairs.Set( "name", name.c_str() );

		for( brush_t* b = selected_brushes.next; b != &selected_brushes; b = b->next )
		{

			if( b->hiddenBrush )
			{
				continue;
			}

			if( FilterBrush( b ) )
			{
				continue;
			}

			p = BrushToMapPrimitive( b, b->owner->origin );
			if( p )
			{
				mapEnt->AddPrimitive( p );
			}
		}

		collisionModelManager->WriteCollisionModelForMapEntity( mapEnt, name.c_str() );

		delete mapEnt;
	}
}


/*
=================
BuildRendererState

Builds models, lightdefs, and modeldefs for the current editor data
so it can be rendered by the game renderSystem
=================
*/
void CCamWnd::BuildRendererState()
{
	renderEntity_t	worldEntity;
	entity_t*	ent;
	brush_t*		brush;

	FreeRendererState();

	// the renderWorld holds all the references and defs
	g_qeglobals.rw->InitFromMap( NULL );

	// create the raw model for all the brushes
	int numBrushes = 0;
	int numSurfaces = 0;

	// the renderModel for the world holds all the geometry that isn't in an entity
	worldModel = renderModelManager->AllocModel();
	worldModel->InitEmpty( "EditorWorldModel" );

	for( brush_t* brushList = &active_brushes ; brushList ;
			brushList = ( brushList == &active_brushes ) ? &selected_brushes : NULL )
	{

		for( brush = brushList->next; brush != brushList; brush = brush->next )
		{

			if( brush->hiddenBrush )
			{
				continue;
			}

			if( FilterBrush( brush ) )
			{
				continue;
			}

			if( CullBrush( brush, true ) )
			{
				continue;
			}

			idTriList tris( 1024 );
			idMatList mats( 1024 );

			if( !IsBModel( brush ) )
			{
				numSurfaces += Brush_ToTris( brush, &tris, &mats, false, false );
			}

			// add the surfaces to the renderModel
			modelSurface_t	surf;
			for( int i = 0 ; i < tris.Num() ; i++ )
			{
				surf.geometry = tris[i];
				surf.shader = mats[i];
				worldModel->AddSurface( surf );
			}

			numBrushes++;
		}
	}

	// bound and clean the triangles
	worldModel->FinishSurfaces();

	// the worldEntity just has the handle for the worldModel
	memset( &worldEntity, 0, sizeof( worldEntity ) );
	worldEntity.hModel = worldModel;
	worldEntity.axis = mat3_default;
	worldEntity.shaderParms[0] = 1;
	worldEntity.shaderParms[1] = 1;
	worldEntity.shaderParms[2] = 1;
	worldEntity.shaderParms[3] = 1;

	worldModelDef = g_qeglobals.rw->AddEntityDef( &worldEntity );

	// create the light and model entities exactly the way the game code would
	for( ent = entities.next ; ent != &entities ; ent = ent->next )
	{
		if( ent->brushes.onext == &ent->brushes )
		{
			continue;
		}

		if( CullBrush( ent->brushes.onext, true ) )
		{
			continue;
		}

		if( Map_IsBrushFiltered( ent->brushes.onext ) )
		{
			continue;
		}

		BuildEntityRenderState( ent, false );
	}

	//common->Printf("Render data used %d brushes\n", numBrushes);
	worldDirty = false;
	UpdateCaption();
}

/*
==================
UpdateRenderEntities

  Creates a new entity state list
  returns true if a repaint is needed
==================
*/
bool CCamWnd::UpdateRenderEntities()
{
	if( rebuildMode )
	{
		return false;
	}

	bool ret = false;
	for( entity_t* ent = entities.next; ent != &entities; ent = ent->next )
	{
		BuildEntityRenderState( ent, ( ent->lightDef != -1 || ent->modelDef != -1 || ent->soundEmitter ) ? true : false );
		if( ret == false && ent->modelDef || ent->lightDef )
		{
			ret = true;
		}
	}
	return ret;
}

/*
==================
FreeRendererState

  Frees the render state data
==================
*/
void CCamWnd::FreeRendererState()
{
	for( entity_t* ent = entities.next; ent != &entities; ent = ent->next )
	{
		if( ent->lightDef >= 0 )
		{
			g_qeglobals.rw->FreeLightDef( ent->lightDef );
			ent->lightDef = -1;
		}

		if( ent->modelDef >= 0 )
		{
			renderEntity_t* refent = const_cast<renderEntity_t*>( g_qeglobals.rw->GetRenderEntity( ent->modelDef ) );
			if( refent )
			{
				if( refent->callbackData )
				{
					Mem_Free( refent->callbackData );
					refent->callbackData = NULL;
				}
				if( refent->joints )
				{
					Mem_Free16( refent->joints );
					refent->joints = NULL;
				}
			}
			g_qeglobals.rw->FreeEntityDef( ent->modelDef );
			ent->modelDef = -1;
		}
	}

	if( worldModel )
	{
		renderModelManager->FreeModel( worldModel );
		worldModel = NULL;
	}
}

/*
==================
UpdateCaption

  updates the caption based on rendermode and whether the render mode needs updated
==================
*/
void CCamWnd::UpdateCaption()
{
	idStr strCaption;

	if( worldDirty )
	{
		strCaption = "*";
	}
	// FIXME:
	strCaption += ( renderMode ) ? "Camera View: Render Mode" : "Camera View: Edit Mode";
	if( renderMode )
	{
		strCaption += ( rebuildMode ) ? " (realtime)" : "";
		strCaption += ( entityMode ) ? " +lights" : "";
		strCaption += ( selectMode ) ? " +selected" : "";
		strCaption += ( animationMode ) ? " +anim" : "";
	}
	strCaption += ( soundMode ) ? " +snd" : "";
	SetWindowText( strCaption );
}

/*
==================
ToggleRenderMode
==================
*/
void CCamWnd::ToggleRenderMode()
{
	renderMode ^= 1;
	UpdateCaption();
}

/*
==================
ToggleRebuildMode
==================
*/
void CCamWnd::ToggleRebuildMode()
{
	rebuildMode ^= 1;
	UpdateCaption();
}

/*
==================
ToggleEntityMode
==================
*/
void CCamWnd::ToggleEntityMode()
{
	entityMode ^= 1;
	UpdateCaption();
}

/*
==================
ToggleAnimationMode
==================
*/
void CCamWnd::ToggleAnimationMode()
{
	animationMode ^= 1;
	if( animationMode )
	{
		SetTimer( 0, 10, NULL );
	}
	else
	{
		KillTimer( 0 );
	}
	UpdateCaption();
}

/*
==================
ToggleSoundMode
==================
*/
void CCamWnd::ToggleSoundMode()
{
	soundMode ^= 1;

	UpdateCaption();

	for( entity_t* ent = entities.next; ent != &entities; ent = ent->next )
	{
		Entity_UpdateSoundEmitter( ent );
	}
}

/*
==================
ToggleSelectMode
==================
*/
void CCamWnd::ToggleSelectMode()
{
	selectMode ^= 1;
	UpdateCaption();
}

/*
==================
MarkWorldDirty
==================
*/
void CCamWnd::MarkWorldDirty()
{
	worldDirty = true;
	UpdateCaption();
}

extern void glBox( idVec4& color, idVec3& point, float size );
/*
==================
DrawEntityData

  Draws entity data ( experimental )
==================
*/
void CCamWnd::DrawEntityData()
{
	qglMatrixMode( GL_MODELVIEW );
	qglLoadIdentity();
	qglMatrixMode( GL_PROJECTION );
	qglLoadIdentity();

	SetProjectionMatrix();

	qglRotatef( -90, 1, 0, 0 );	// put Z going up
	qglRotatef( 90, 0, 0, 1 );	// put Z going up
	qglRotatef( m_Camera.angles[0], 0, 1, 0 );
	qglRotatef( -m_Camera.angles[1], 0, 0, 1 );
	qglTranslatef( -m_Camera.origin[0], -m_Camera.origin[1], -m_Camera.origin[2] );

	Cam_BuildMatrix();

	if( !( entityMode || selectMode ) )
	{
		return;
	}

	qglDisable( GL_BLEND );
	qglDisable( GL_DEPTH_TEST );
	qglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
	globalImages->BindNull();
	idVec3 color( 0, 1, 0 );
	qglColor3fv( color.ToFloatPtr() );

	brush_t* brushList = &active_brushes;
	int pass = 0;
	while( brushList )
	{
		for( brush_t* brush = brushList->next; brush != brushList; brush = brush->next )
		{
			if( CullBrush( brush, true ) )
			{
				continue;
			}
			if( FilterBrush( brush ) )
			{
				continue;
			}
			if( ( pass == 1 && selectMode ) || ( entityMode && pass == 0 && brush->owner->lightDef >= 0 ) )
			{
				Brush_DrawXY( brush, 0, true, true );
			}
		}
		brushList = ( brushList == &active_brushes ) ? &selected_brushes : NULL;
		color.x = 1;
		color.y = 0;
		pass++;
		qglColor3fv( color.ToFloatPtr() );
	}
}

/*
==================
Cam_Render

	This used the renderSystem to
	draw a fully lit view of the world
==================
 */
void CCamWnd::Cam_Render()
{
	renderView_t refdef;
	CPaintDC dc( this );	// device context for painting

	if( !active_brushes.next )
	{
		return;					// not valid yet
	}

	if( !qwglMakeCurrent( dc.m_hDC, win32.hGLRC ) )
	{
		common->Printf( "ERROR: wglMakeCurrent failed..\n " );
		common->Printf( "Please restart " EDITOR_WINDOWTEXT " if the camera view is not working\n" );
		return;
	}

	// save the editor state
	//qglPushAttrib( GL_ALL_ATTRIB_BITS );
	qglClearColor( 0.1f, 0.1f, 0.1f, 0.0f );
	qglScissor( 0, 0, m_Camera.width, m_Camera.height );
	qglClear( GL_COLOR_BUFFER_BIT );

	//	qwglSwapBuffers(dc.m_hDC);

	// create the model, using explicit normals
	if( rebuildMode && worldDirty )
	{
		BuildRendererState();
	}

	// render it
	renderSystem->BeginFrame( m_Camera.width, m_Camera.height );

	memset( &refdef, 0, sizeof( refdef ) );
	refdef.vieworg = m_Camera.origin;

	// the editor uses opposite pitch convention
	refdef.viewaxis = idAngles( -m_Camera.angles.pitch, m_Camera.angles.yaw, m_Camera.angles.roll ).ToMat3();

	refdef.width = SCREEN_WIDTH;
	refdef.height = SCREEN_HEIGHT;
	refdef.fov_x = 90;
	refdef.fov_y = 2 * atan( ( float )m_Camera.height / m_Camera.width ) * idMath::M_RAD2DEG;

	// only set in animation mode to give a consistent look
	if( animationMode )
	{
		refdef.time = eventLoop->Milliseconds();
	}

	g_qeglobals.rw->RenderScene( &refdef );

	int	frontEnd, backEnd;

	renderSystem->EndFrame( &frontEnd, &backEnd );
//common->Printf( "front:%i back:%i\n", frontEnd, backEnd );

	//qglPopAttrib();
	//DrawEntityData();

	//qwglSwapBuffers(dc.m_hDC);
	// get back to the editor state
	qglMatrixMode( GL_MODELVIEW );
	qglLoadIdentity();
	Cam_BuildMatrix();
}

void CCamWnd::OnTimer( UINT nIDEvent )
{
	if( animationMode || nIDEvent == 1 )
	{
		Sys_UpdateWindows( W_CAMERA );
	}
	if( nIDEvent == 1 )
	{
		KillTimer( 1 );
	}

	if( !animationMode )
	{
		KillTimer( 0 );
	}
}

/*
==================
UpdateCameraView
==================
*/
void CCamWnd::UpdateCameraView()
{
	if( QE_SingleBrush( true, true ) )
	{
		brush_t* b = selected_brushes.next;
		if( b->owner->eclass->nShowFlags & ECLASS_CAMERAVIEW )
		{
			// find the entity that targets this
			const char* name = ValueForKey( b->owner, "name" );
			entity_t* ent = FindEntity( "target", name );
			if( ent )
			{
				if( !saveValid )
				{
					saveOrg = m_Camera.origin;
					saveAng = m_Camera.angles;
					saveValid = true;
				}
				idVec3 v = b->owner->origin - ent->origin;
				v.Normalize();
				idAngles ang = v.ToMat3().ToAngles();
				ang.pitch = -ang.pitch;
				ang.roll = 0.0f;
				SetView( ent->origin, ang );
				Cam_BuildMatrix();
				Sys_UpdateWindows( W_CAMERA );
				return;
			}
		}
	}
	if( saveValid )
	{
		SetView( saveOrg, saveAng );
		Cam_BuildMatrix();
		Sys_UpdateWindows( W_CAMERA );
		saveValid = false;
	}
}

// ---> sikk - Mousewheel Support for cam window
BOOL CCamWnd::OnMouseWheel( UINT nFlags, short zDelta, CPoint pt )
{
	if( zDelta > 0 )
	{
//		g_pParentWnd->OnCameraForward();
		m_Camera.origin += 32.0f * m_Camera.vpn;
		int nUpdate = ( g_PrefsDlg.m_bCamXYUpdate ) ? ( W_CAMERA | W_XY ) : ( W_CAMERA );
		Sys_UpdateWindows( nUpdate );
	}
	else
	{
//		g_pParentWnd->OnCameraBack();
		m_Camera.origin += -32.0f * m_Camera.vpn;
		int nUpdate = ( g_PrefsDlg.m_bCamXYUpdate ) ? ( W_CAMERA | W_XY ) : ( W_CAMERA );
		Sys_UpdateWindows( nUpdate );
	}
	return TRUE;
}
// <--- sikk - Mousewheel Support for cam window

// ---> sikk - New Brush creation in Cam window
/*
==================
NewBrushDrag
==================
*/
void CCamWnd::NewBrushDrag( int x, int y )
{
	idVec3	mins, maxs, junk;
	int		i;
	float	temp;
	brush_t* n;

	if( radiant_entityMode.GetBool() )
	{
		return;
	}

	//if ( !DragDelta( x, y, junk ) ) {
	//	return;
	//}

	// delete the current selection
	if( selected_brushes.next != &selected_brushes )
	{
		Brush_Free( selected_brushes.next );
	}

	//SnapToPoint( m_nPressx, m_nPressy, mins );

	//int nDim = ( m_nViewType == XY ) ? 2 : ( m_nViewType == YZ ) ? 0 : 1;

	//mins[nDim] = g_qeglobals.d_gridsize * 8 * ( (int)( g_qeglobals.d_new_brush_bottom[nDim] / g_qeglobals.d_gridsize) );
	//SnapToPoint( x, y, maxs );
	//maxs[nDim] = g_qeglobals.d_gridsize * ( (int)( g_qeglobals.d_new_brush_top[nDim] / g_qeglobals.d_gridsize) );
	//if ( maxs[nDim] <= mins[nDim] ) {
	//	maxs[nDim] = mins[nDim] + g_qeglobals.d_gridsize;
	//}
	idVec3	dir, origin;
	float	f, r, u;

	// calc ray direction
	u = ( float )( y - m_Camera.height / 2 ) / ( m_Camera.width / 2 );
	r = ( float )( x - m_Camera.width / 2 ) / ( m_Camera.width / 2 );
	f = 1;

	for( i = 0; i < 3; i++ )
	{
		dir[i] = m_Camera.vpn[i] * f + m_Camera.vright[i] * r + m_Camera.vup[i] * u;
	}
	dir.Normalize();

	qertrace_t	t;
	t = Test_Ray( m_Camera.origin, dir, 0 );
	if( t.brush )
	{
		origin = m_Camera.origin + dir * ( t.dist - g_qeglobals.d_gridsize );
	}
	else
	{
		origin = m_Camera.origin + dir * ( 64 + g_qeglobals.d_gridsize );
	}

	int offset;
	if( g_qeglobals.d_gridsize <= 1 )
	{
		offset = g_qeglobals.d_gridsize * 4;
	}
	else if( g_qeglobals.d_gridsize == 2 )
	{
		offset = g_qeglobals.d_gridsize * 2;
	}
	else if( g_qeglobals.d_gridsize == 4 )
	{
		offset = g_qeglobals.d_gridsize;
	}
	else if( g_qeglobals.d_gridsize == 8 )
	{
		offset = g_qeglobals.d_gridsize;
	}
	else if( g_qeglobals.d_gridsize == 16 )
	{
		offset = g_qeglobals.d_gridsize;
	}
	else if( g_qeglobals.d_gridsize == 32 )
	{
		offset = g_qeglobals.d_gridsize / 2;
	}
	else if( g_qeglobals.d_gridsize == 64 )
	{
		offset = g_qeglobals.d_gridsize / 2;
	}
	else if( g_qeglobals.d_gridsize == 128 )
	{
		offset = g_qeglobals.d_gridsize / 2;
	}
	else if( g_qeglobals.d_gridsize == 256 )
	{
		offset = g_qeglobals.d_gridsize / 2;
	}

	mins = origin - idVec3( offset, offset, offset );
	for( i = 0; i < 3; i++ )
	{
		temp = ( mins[i] + 0.5f ) / g_qeglobals.d_gridsize;
		temp = floor( temp );
		mins[i] = temp * g_qeglobals.d_gridsize;
	}
	maxs = mins + idVec3( offset * 2, offset * 2, offset * 2 );

	//for ( i = 0; i < 3; i++ ) {
	//	if ( mins[i] == maxs[i] ) {
	//		return; // don't create a degenerate brush
	//	}

	//	if ( mins[i] > maxs[i] ) {
	//		temp = mins[i];
	//		mins[i] = maxs[i];
	//		maxs[i] = temp;
	//	}
	//}

	n = Brush_Create( mins, maxs, &g_qeglobals.d_texturewin.texdef );
	if( !n )
	{
		return;
	}

	idVec3	vSize;
	VectorSubtract( maxs, mins, vSize );
	CString strStatus;
	strStatus.Format( "Size X:: %.1f  Y:: %.1f  Z:: %.1f", vSize[0], vSize[1], vSize[2] );
	g_pParentWnd->SetStatusText( 2, strStatus );

	Brush_AddToList( n, &selected_brushes );
	Entity_LinkBrush( world_entity, n );
	Brush_Build( n );

	Sys_UpdateWindows( W_ALL );
}

/*
==================
CreateDropBrush
==================
*/
brush_t* CCamWnd::CreateDropBrush()
{
	idVec3	mins, maxs, junk;
	int		i, x, y;
	float	temp;
	brush_t* n;

	// delete the current selection
	if( selected_brushes.next != &selected_brushes )
	{
		Brush_Free( selected_brushes.next );
	}

	//SnapToPoint( m_nPressx, m_nPressy, mins );

	//int nDim = ( m_nViewType == XY ) ? 2 : ( m_nViewType == YZ ) ? 0 : 1;

	//mins[nDim] = g_qeglobals.d_gridsize * 8 * ( (int)( g_qeglobals.d_new_brush_bottom[nDim] / g_qeglobals.d_gridsize) );
	//SnapToPoint( x, y, maxs );
	//maxs[nDim] = g_qeglobals.d_gridsize * ( (int)( g_qeglobals.d_new_brush_top[nDim] / g_qeglobals.d_gridsize) );
	//if ( maxs[nDim] <= mins[nDim] ) {
	//	maxs[nDim] = mins[nDim] + g_qeglobals.d_gridsize;
	//}
	idVec3	dir, origin;
	float	f, r, u;
	CRect	rect;
	GetClientRect( rect );
	x = m_ptDown.x;
	y = rect.bottom - 1 - m_ptDown.y;

	// calc ray direction
	u = ( float )( y - m_Camera.height / 2 ) / ( m_Camera.width / 2 );
	r = ( float )( x - m_Camera.width / 2 ) / ( m_Camera.width / 2 );
	f = 1;

	for( i = 0; i < 3; i++ )
	{
		dir[i] = m_Camera.vpn[i] * f + m_Camera.vright[i] * r + m_Camera.vup[i] * u;
	}
	dir.Normalize();

	qertrace_t	t;
	t = Test_Ray( m_Camera.origin, dir, 0 );
	if( t.brush )
	{
		origin = m_Camera.origin + dir * ( t.dist - g_qeglobals.d_gridsize );
	}
	else
	{
		origin = m_Camera.origin + dir * ( 64 + g_qeglobals.d_gridsize );
	}

	int offset;
	if( g_qeglobals.d_gridsize == 1 )
	{
		offset = g_qeglobals.d_gridsize * 4;
	}
	else if( g_qeglobals.d_gridsize == 2 )
	{
		offset = g_qeglobals.d_gridsize * 2;
	}
	else if( g_qeglobals.d_gridsize == 4 )
	{
		offset = g_qeglobals.d_gridsize;
	}
	else if( g_qeglobals.d_gridsize == 8 )
	{
		offset = g_qeglobals.d_gridsize;
	}
	else if( g_qeglobals.d_gridsize == 16 )
	{
		offset = g_qeglobals.d_gridsize;
	}
	else if( g_qeglobals.d_gridsize == 32 )
	{
		offset = g_qeglobals.d_gridsize / 2;
	}
	else if( g_qeglobals.d_gridsize == 64 )
	{
		offset = g_qeglobals.d_gridsize / 2;
	}
	else if( g_qeglobals.d_gridsize == 128 )
	{
		offset = g_qeglobals.d_gridsize / 2;
	}
	else if( g_qeglobals.d_gridsize == 256 )
	{
		offset = g_qeglobals.d_gridsize / 2;
	}

	mins = origin - idVec3( offset, offset, offset );
	for( i = 0; i < 3; i++ )
	{
		temp = mins[i] / g_qeglobals.d_gridsize;
		temp = floor( temp );
		mins[i] = temp * g_qeglobals.d_gridsize;
	}
	maxs = mins + idVec3( offset * 2, offset * 2, offset * 2 );

	//for ( i = 0; i < 3; i++ ) {
	//	if ( mins[i] == maxs[i] ) {
	//		return; // don't create a degenerate brush
	//	}

	//	if ( mins[i] > maxs[i] ) {
	//		temp = mins[i];
	//		mins[i] = maxs[i];
	//		maxs[i] = temp;
	//	}
	//}

	n = Brush_Create( mins, maxs, &g_qeglobals.d_texturewin.texdef );
	if( !n )
	{
		return NULL;
	}

	idVec3	vSize;
	VectorSubtract( maxs, mins, vSize );
	CString strStatus;
	strStatus.Format( "Size X:: %.1f  Y:: %.1f  Z:: %.1f", vSize[0], vSize[1], vSize[2] );
	g_pParentWnd->SetStatusText( 2, strStatus );

	Brush_AddToList( n, &selected_brushes );
	Entity_LinkBrush( world_entity, n );
	Brush_Build( n );

	Sys_UpdateWindows( W_ALL );

	return n;
}
// <---	sikk - New Brush creation in Cam window

// sikk---> Center Camera on Selection. Same as PositionView() for XY View
/*
==================
PositionView
==================
*/
void CCamWnd::PositionView()
{
	brush_t* b;

	b = selected_brushes.next;
	if( b && b->next != b )
	{
		idVec3	mins, maxs, org, dist;
		float len;
		mins = b->mins;
		maxs = b->maxs;
		for( b = selected_brushes.next; b != &selected_brushes; b = b->next )
		{
			for( int i = 0; i < 3; i++ )
			{
				mins[i] = min( mins[i], b->mins[i] );
				maxs[i] = max( maxs[i], b->maxs[i] );
			}
		}
		org = ( mins + maxs ) * 0.5f;
		dist = ( maxs - mins );
		len = dist.LengthFast() * 0.5f;

		m_Camera.origin[0] = org[0] - len;
		m_Camera.origin[1] = org[1] - len;
		m_Camera.origin[2] = org[2] + len * 0.5f;
		m_Camera.angles[0] = -22.5;
		m_Camera.angles[1] = 45;
		m_Camera.angles[2] = 0;
	}
	else
	{
		// TODO: Do something else here?
		m_Camera.origin[0] = m_Camera.origin[0];
		m_Camera.origin[1] = m_Camera.origin[1];
		m_Camera.origin[2] = m_Camera.origin[2];
	}
}
// <---sikk
