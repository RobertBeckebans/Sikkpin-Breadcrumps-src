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
#if !defined(AFX_SURFACEDLG_H__D84E0C22_9EEA_11D1_B570_00AA00A410FC__INCLUDED_)
#define AFX_SURFACEDLG_H__D84E0C22_9EEA_11D1_B570_00AA00A410FC__INCLUDED_

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000
// SurfaceDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CSurfaceDlg dialog

class CSurfaceDlg : public CDialog {
	bool			m_bPatchMode;
	CWnd			*focusControl;
	patchMesh_t		*m_Patch;	// sikk - Merged Patch Inspector into Surface Inspector

	// Construction
public:
					CSurfaceDlg( CWnd* pParent = NULL );   // standard constructor
	void			SetTexMods( void );

// ---> sikk - Merged Patch Inspector into Surface Inspector
// <--- sikk - Merged Patch Inspector into Surface Inspector
	void			UpdateInfo( void );
	void			SetPatchInfo( void );
	void			GetPatchInfo( void );
// <--- sikk - Merged Patch Inspector into Surface Inspector

// Dialog Data
	//{{AFX_DATA(CSurfaceDlg)
	enum { IDD = IDD_SURFACE };
	CEdit			m_wndRotateEdit;
	CEdit			m_wndVert;
	CEdit			m_wndHorz;
	CSliderCtrl		m_wndVerticalSubdivisions;
	CSliderCtrl		m_wndHorzSubdivisions;
	CSpinButtonCtrl	m_wndWidth;
	CSpinButtonCtrl	m_wndHeight;
	CSpinButtonCtrl	m_wndVShift;
	CSpinButtonCtrl	m_wndVScale;
	CSpinButtonCtrl	m_wndRotate;
	CSpinButtonCtrl	m_wndHShift;
	CSpinButtonCtrl	m_wndHScale;
	int				m_nHorz;
	int				m_nVert;
	float			m_horzScale;
	float			m_horzShift;
	float			m_rotate;
	float			m_vertScale;
	float			m_vertShift;
	CString			m_strMaterial;
	BOOL			m_subdivide;
	float			m_fHeight;
	float			m_fWidth;
	BOOL			m_absolute;

// ---> sikk - Merged Patch Inspector into Surface Inspector
	CSpinButtonCtrl	m_wndPatchVShift;
	CSpinButtonCtrl	m_wndPatchVScale;
	CSpinButtonCtrl	m_wndPatchRotate;
	CSpinButtonCtrl	m_wndPatchHShift;
	CSpinButtonCtrl	m_wndPatchHScale;
	CComboBox		m_wndType;
	CComboBox		m_wndRows;
	CComboBox		m_wndCols;
	float			m_fS;
	float			m_fT;
	float			m_fX;
	float			m_fY;
	float			m_fZ;
	float			m_fPatchHScale;
	float			m_fPatchHShift;
	float			m_fPatchRotate;
	float			m_fPatchVScale;
	float			m_fPatchVShift;
// <--- sikk - Merged Patch Inspector into Surface Inspector
	//}}AFX_DATA

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CSurfaceDlg)
public:
	virtual BOOL	PreTranslateMessage( MSG* pMsg );

protected:
	virtual void	DoDataExchange( CDataExchange* pDX );    // DDX/DDV support
	virtual BOOL	PreCreateWindow( CREATESTRUCT& cs );
	//}}AFX_VIRTUAL

// Implementation
protected:
	void			UpdateSpinners( int nScrollCode, int nPos, CScrollBar* pBar );
	void			UpdateSpinners( bool bUp, int nID );

	void			UpdateRowColInfo( void );	// sikk - Merged Patch Inspector into Surface Inspector

	// Generated message map functions
	//{{AFX_MSG(CSurfaceDlg)
	virtual BOOL	OnInitDialog( void );
	afx_msg void	OnHScroll( UINT nSBCode, UINT nPos, CScrollBar* pScrollBar );
	afx_msg void	OnKeyDown( UINT nChar, UINT nRepCnt, UINT nFlags );
	afx_msg void	OnVScroll( UINT nSBCode, UINT nPos, CScrollBar* pScrollBar );
	afx_msg void	OnApply( void );
	virtual void	OnOK( void );
	afx_msg void	OnClose( void );
	virtual void	OnCancel( void );
	afx_msg void	OnDestroy( void );
	afx_msg void	OnBtnCancel( void );
	afx_msg void	OnBtnColor( void );
	afx_msg HBRUSH	OnCtlColor( CDC* pDC, CWnd* pWnd, UINT nCtlColor );
	afx_msg int		OnCreate( LPCREATESTRUCT lpCreateStruct );
	afx_msg void	OnDeltaPosSpin( NMHDR* pNMHDR, LRESULT* pResult );
	afx_msg void	OnBtnPatchdetails( void );
	afx_msg void	OnBtnPatchnatural( void );
	afx_msg void	OnBtnPatchreset( void );
	afx_msg void	OnBtnAxial( void );
	afx_msg void	OnBtnBrushfit( void );
	afx_msg void	OnBtnFacefit( void );
	afx_msg void	OnCheckSubdivide( void );
	afx_msg void	OnChangeEditHorz( void );
	afx_msg void	OnChangeEditVert( void );
	afx_msg void	OnSetfocusHscale( void );
	afx_msg void	OnKillfocusHscale( void );
	afx_msg void	OnKillfocusVscale( void );
	afx_msg void	OnSetfocusVscale( void );
	afx_msg void	OnKillfocusEditWidth( void );
	afx_msg void	OnSetfocusEditWidth( void );
	afx_msg void	OnKillfocusEditHeight( void );
	afx_msg void	OnSetfocusEditHeight( void );
	afx_msg void	OnBtnFlipx( void );
	afx_msg void	OnBtnFlipy( void );
	afx_msg void	OnKillfocusRotate( void );
	afx_msg void	OnSetfocusRotate( void );

// ---> sikk - Merged Patch Inspector into Surface Inspector
	afx_msg void	OnBtnPatchfit( void );
	afx_msg void	OnSelchangeComboCol( void );
	afx_msg void	OnSelchangeComboRow( void );
	afx_msg void	OnSelchangeComboType( void );
// <--- sikk - Merged Patch Inspector into Surface Inspector
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()

public:
	afx_msg void	OnEnChangeVshift( void );
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_SURFACEDLG_H__D84E0C22_9EEA_11D1_B570_00AA00A410FC__INCLUDED_)
