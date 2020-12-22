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
#if !defined(AFX_PATCHDENSITYDLG_H__509162A1_1023_11D2_AFFB_00AA00A410FC__INCLUDED_)
#define AFX_PATCHDENSITYDLG_H__509162A1_1023_11D2_AFFB_00AA00A410FC__INCLUDED_

#if _MSC_VER >= 1000
	#pragma once
#endif // _MSC_VER >= 1000
// PatchDensityDlg.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CPatchDensityDlg dialog

class CPatchDensityDlg : public CDialog
{
// Construction
public:
	CPatchDensityDlg( CWnd* pParent = NULL );   // standard constructor

// Dialog Data
	//{{AFX_DATA(CPatchDensityDlg)
	enum { IDD = IDD_DIALOG_NEWPATCH };
	CComboBox	m_wndWidth;
	CComboBox	m_wndHeight;
	//}}AFX_DATA


// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CPatchDensityDlg)
protected:
	virtual void DoDataExchange( CDataExchange* pDX );    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:

	// Generated message map functions
	//{{AFX_MSG(CPatchDensityDlg)
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnCbnSelchangeComboHeight();
	afx_msg void OnCbnSelchangeComboWidth();
};

//{{AFX_INSERT_LOCATION}}
// Microsoft Developer Studio will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_PATCHDENSITYDLG_H__509162A1_1023_11D2_AFFB_00AA00A410FC__INCLUDED_)