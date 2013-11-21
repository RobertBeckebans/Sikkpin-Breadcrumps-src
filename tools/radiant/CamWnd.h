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
#if !defined(AFX_CAMWND_H__44B4BA03_781B_11D1_B53C_00AA00A410FC__INCLUDED_)
#define AFX_CAMWND_H__44B4BA03_781B_11D1_B53C_00AA00A410FC__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

typedef enum {
	cd_wire,
	cd_solid,
	cd_texture,
	cd_light,
	cd_blend
} camera_draw_mode;

typedef struct {
	int			width, height;

	idVec3		origin;
	idAngles	angles;

	camera_draw_mode	draw_mode;

	idVec3		color;				// background 

	idVec3		forward, right, up;	// move matrix
	idVec3		vup, vpn, vright;	// view matrix
	float		viewDist; // sikk - Added - Rotate Around Selection
} camera_t;


/////////////////////////////////////////////////////////////////////////////
// CCamWnd window
class CXYWnd;

class CCamWnd : public CWnd {
	DECLARE_DYNCREATE(CCamWnd);
// Construction
public:
				CCamWnd( void );
	virtual		~CCamWnd( void );

// Attributes
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CCamWnd)
protected:
	virtual BOOL	PreCreateWindow( CREATESTRUCT& cs );
	//}}AFX_VIRTUAL

// Implementation
public:
	void			ShiftTexture_BrushPrimit( face_t *f, int x, int y );
	void			SetXYFriend( CXYWnd* pWnd );
	camera_t&		Camera( void ){ return m_Camera; };
	void			Cam_MouseControl( void );
	void			Cam_KeyControl( float dtime );	// sikk - Added for wasd movement
	void 			Cam_ChangeFloor( bool up );
	void 			BuildRendererState( void );
	void 			ToggleRenderMode( void );
	void 			ToggleRebuildMode( void );
	void 			ToggleEntityMode( void );
	void 			ToggleSelectMode( void );
	void 			ToggleAnimationMode( void );
	void 			ToggleSoundMode( void );
	void 			SetProjectionMatrix( void );
	void 			UpdateCameraView( void );

	void 			BuildEntityRenderState( entity_t *ent, bool update );
	bool 			GetRenderMode( void ) { return renderMode; }
	bool 			GetRebuildMode( void ) { return rebuildMode; }
	bool 			GetEntityMode( void ) { return entityMode; }
	bool 			GetAnimationMode( void ) { return animationMode; }
	bool 			GetSelectMode( void ) { return selectMode; }
	bool 			GetSoundMode( void ) { return soundMode; }

	bool 			UpdateRenderEntities( void );
	void 			MarkWorldDirty( void );

	void 			SetView( const idVec3 &origin, const idAngles &angles ) { m_Camera.origin = origin; m_Camera.angles = angles; }
	void 			Cam_BuildMatrix();	// sikk - temp move until I write public wasd move functions

	void			PositionView( void );	// sikk - Added - Center on Selection

	brush_t*		CreateDropBrush( void );	// sikk - Added - New Brush creation in Cam window
	
	CXYWnd*			m_pXYFriend;

protected:
	void 			Cam_Init( void );
	void 			Cam_PositionDrag( void );
	void 			Cam_PositionRotate( void );	// sikk - Added - Rotate Around Selection
	void 			Cam_Rotate( int x, int y, idVec3 org ); // sikk - Added - Rotate Around Selection
	void 			Cam_MouseLook( void );
	void 			Cam_MouseDown( int x, int y, int buttons );
	void 			Cam_MouseUp( int x, int y, int buttons );
	void 			Cam_MouseMoved( int x, int y, int buttons );
	void			NewBrushDrag( int x, int y );	// sikk - Added - New Brush creation in Cam window
	void 			InitCull( void );
	bool 			CullBrush( brush_t *b, bool cubicOnly );
	void 			Cam_Draw( void );
	void 			Cam_Render( void );
// ---> sikk - Various Draw Functions
	void 			Cam_DrawMapBounds( void );
	void 			Cam_DrawWorldAxis( void );
	void 			Cam_DrawCameraAxis( void );
	void 			Cam_DrawClipPoints( void );
// <--- sikk - Various Draw Functions

	// game renderer interaction
	void			FreeRendererState( void );
	void			UpdateCaption( void );
	bool			BuildBrushRenderData( brush_t *brush );
	void			DrawEntityData( void );

	qhandle_t		worldModelDef;
	idRenderModel	*worldModel;		// createRawModel of the brush and patch geometry
	bool			worldDirty;
	bool			renderMode;
	bool			rebuildMode;
	bool			entityMode;
	bool			selectMode;
	bool			animationMode;
	bool			soundMode;

	camera_t		m_Camera;
	int				m_nCambuttonstate;
	CPoint 			m_ptButton;
	CPoint 			m_ptCursor;
	CPoint 			m_ptLastCursor;
	CPoint  		m_ptDown;	// sikk - Added for context menu
	CPoint  		m_ptLBDown;	// sikk - Clip Point Manipulation
	face_t* 		m_pSide_select;
	idVec3 			m_vCull1;
	idVec3 			m_vCull2;
	int 			m_nCullv1[3];
	int 			m_nCullv2[3];
	bool 			m_bClipMode;
	idVec3 			saveOrg;
	idAngles 		saveAng;
	bool 			saveValid;

	bool 			m_bCanCreateBrush;	// sikk - Added - New Brush creation in Cam window

	idVec3			m_vPressdelta;	// sikk - Added - Clip Point Manipulation
	// Generated message map functions
protected:
	void 			OriginalMouseDown( UINT nFlags, CPoint point );
	void 			OriginalMouseUp( UINT nFlags, CPoint point );
	//{{AFX_MSG(CCamWnd)
	afx_msg void	OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags );
	afx_msg void	OnPaint( void );
	afx_msg void	OnDestroy( void );
	afx_msg void	OnClose( void );
	afx_msg void	OnMouseMove( UINT nFlags, CPoint point );
	afx_msg void	OnLButtonDown( UINT nFlags, CPoint point );
	afx_msg void	OnLButtonUp( UINT nFlags, CPoint point );
	afx_msg void	OnMButtonDown( UINT nFlags, CPoint point );
	afx_msg void	OnMButtonUp( UINT nFlags, CPoint point );
	afx_msg void	OnRButtonDown( UINT nFlags, CPoint point );
	afx_msg void	OnRButtonUp( UINT nFlags, CPoint point );
	afx_msg int		OnCreate( LPCREATESTRUCT lpCreateStruct );
	afx_msg void	OnSize( UINT nType, int cx, int cy );
// ---> sikk - Window Snapping
	afx_msg void	OnSizing( UINT nSide, LPRECT lpRect );
	afx_msg void	OnMoving( UINT nSide, LPRECT lpRect );
// <--- sikk - Window Snapping
	afx_msg void	OnKeyUp( UINT nChar, UINT nRepCnt, UINT nFlags );
	afx_msg void	OnTimer( UINT nIDEvent );
	afx_msg BOOL	OnMouseWheel( UINT nFlags, short zDelta, CPoint pt );	// sikk - Mousewheel Support for cam window
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

/////////////////////////////////////////////////////////////////////////////

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_CAMWND_H__44B4BA03_781B_11D1_B53C_00AA00A410FC__INCLUDED_)
