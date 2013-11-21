/*
===========================================================================

Doom 3 GPL Source Code
Copyright( C ) 1999-2011 id Software LLC, a ZeniMax Media company. 

This file is part of the Doom 3 GPL Source Code( ?Doom 3 Source Code? ).  

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
( at your option ) any later version.

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

// patch stuff
patchMesh_t*	MakeNewPatch( int width, int height );
brush_t*		AddBrushForPatch( patchMesh_t *pm, bool bLinkToWorld = true );
brush_t*		Patch_GenericMesh( int nWidth, int nHeight, int nOrientation = 2, bool bDeleteSource = true, bool bOverride = false, patchMesh_t *parent = NULL );
patchMesh_t*	Patch_GenerateGeneric( int width, int height, int orientation, const idVec3 &mins, const idVec3 &maxs );	// sikk - Moved here from source file
void			Patch_ReadFile( char *name );
void			Patch_WriteFile( char *name ); 
void			Patch_BuildPoints( brush_t *b );
void 			Patch_Move( patchMesh_t *p, const idVec3 vMove, bool bRebuild = false );
//++timo had to add a default value for bSnap( see Patch_ApplyMatrix call from Select_ApplyMatrix in select.cpp )
void 			Patch_ApplyMatrix( patchMesh_t *p, const idVec3 vOrigin, const idMat3 matrix, bool bSnap = false );
void 			Patch_EditPatch( void );
void 			Patch_Deselect( void );
void 			Patch_Deselect( patchMesh_t *p );
void 			Patch_Delete( patchMesh_t *p );
int				Patch_MemorySize( patchMesh_t *p );
void 			Patch_Select( patchMesh_t *p );
void 			Patch_Scale( patchMesh_t *p, const idVec3 vOrigin, const idVec3 vAmt, bool bRebuilt = true );
void 			Patch_Cleanup( void );
void 			Patch_SetView( int n );
void 			Patch_SetTexture( patchMesh_t *p, texdef_t *tex_def );
void 			Patch_SetTextureName( patchMesh_t *p, const char *name );
void 			Patch_BrushToMesh( bool bCone = false, bool bBevel = false, bool bEndcap = false, bool bSquare = false, int nHeight = 3 );
// ---> sikk - Added - Cube patch
void 			Patch_BrushToCubeMesh( int nHeight = 3 );
patchMesh_t*	Patch_GenerateCube( int width, int height, int orientation, const idVec3 &mins, const idVec3 &maxs );
void			Patch_GenerateQuick( int width, int height, int orientation );
// <--- sikk - Added - Cube patch
bool 			Patch_DragScale( patchMesh_t *p, idVec3 vAmt, idVec3 vMove );
void 			Patch_ReadBuffer( char* pBuff, bool bSelect = false );
void 			Patch_WriteFile( CMemFile* pMemFile );
void 			Patch_UpdateSelected( idVec3 vMove );
void 			Patch_AddRow( patchMesh_t *p );
brush_t*		Patch_Parse( bool bOld );
void 			Patch_Write( patchMesh_t *p, FILE *f );
void 			Patch_Write( patchMesh_t *p, CMemFile *file );
void 			Patch_AdjustColumns( patchMesh_t *p, int nCols );
void 			Patch_AdjustRows( patchMesh_t *p, int nRows );
void 			Patch_AdjustSelected( bool bInsert, bool bColumn, bool bFlag );
patchMesh_t*	Patch_Duplicate( patchMesh_t *pFrom );
void 			Patch_RotateTexture( patchMesh_t *p, float fAngle );
void 			Patch_ScaleTexture( patchMesh_t *p, float fx, float fy, bool absolute );
void 			Patch_ShiftTexture( patchMesh_t *p, float fx, float fy, bool autoAdjust );
void 			Patch_DrawCam( patchMesh_t *p, bool selected );
void 			Patch_DrawXY( patchMesh_t *p, bool selected = false );	// sikk - Added bSelected arg
void 			Patch_InsertColumn( patchMesh_t *p, bool bAdd );
void 			Patch_InsertRow( patchMesh_t *p, bool bAdd );
void 			Patch_RemoveRow( patchMesh_t *p, bool bFirst );
void 			Patch_RemoveColumn( patchMesh_t *p, bool bFirst );
void 			Patch_ToggleInverted( void );
void 			Patch_Restore( patchMesh_t *p );
void 			Patch_Save( patchMesh_t *p );
void 			Patch_SetTextureInfo( texdef_t* pt );
void 			Patch_NaturalTexturing( void );
void 			Patch_ResetTexturing( float fx, float fy );
void 			Patch_FitTexture( patchMesh_t *p, float fx, float fy );
void 			Patch_FitTexturing( void );
void 			Patch_BendToggle( void );
void 			Patch_StartInsDel( void );
void 			Patch_BendHandleTAB( void );
void 			Patch_BendHandleENTER( void );
void 			Patch_SelectBendNormal( void );
void 			Patch_SelectBendAxis( void );
patchMesh_t*	SinglePatchSelected( void );
void 			Patch_CapCurrent( bool bInvertedBevel = false, bool bInvertedEndcap = false );
void 			Patch_DisperseRows( void );
void 			Patch_DisperseColumns( void );
void 			Patch_NaturalizeSelected( bool bCap = false, bool bCycleCap = false, bool alt = false );
void 			Patch_SubdivideSelected( bool subdivide, int horz, int vert );
void 			Patch_Naturalize( patchMesh_t *p, bool horz = true, bool vert = true, bool alt = false );
void 			Patch_SelectAreaPoints( void );
void 			Patch_InvertTexture( bool bY );
void 			Patch_InsDelToggle( void );
void 			Patch_InsDelHandleTAB( void );
void 			Patch_InsDelHandleENTER( void );
void 			Patch_SetOverlays( void );
void 			Patch_ClearOverlays( void );
void 			Patch_Thicken( int nAmount, bool bSeam );
void 			Patch_Transpose( void );
void 			Patch_Freeze( void );
void 			Patch_MakeDirty( patchMesh_t *p );
void 			Patch_UnFreeze( bool bAll );
const char*		Patch_GetTextureName( void );
void 			Patch_FindReplaceTexture( brush_t *pb, const char *pFind, const char *pReplace, bool bForce );
void 			Patch_ReplaceQTexture( brush_t *pb, idMaterial *pOld, idMaterial *pNew );
void 			Patch_FromTriangle( idVec5 vx, idVec5 vy, idVec5 vz );
const char*		Patch_GetKeyValue( patchMesh_t *p, const char *pKey );
void 			Patch_SetEpair( patchMesh_t *p, const char *pKey, const char *pValue );
void 			Patch_FlipTexture( patchMesh_t *p, bool y );
void 			Patch_CalcBounds( patchMesh_t *p, idVec3 &vMin, idVec3 &vMax );	// sikk - Added header decl since I moved "Select_SnapToGrid( void )" into select.cpp/h

void 			Patch_InvertNormals( void );	// sikk - Added - Patch Invert Normals

bool WINAPI		OnlyPatchesSelected( void );
bool WINAPI		AnyPatchesSelected( void );
void WINAPI		Patch_Rebuild( patchMesh_t *p );

extern bool		g_bPatchShowBounds;
extern bool		g_bPatchWireFrame;
extern bool		g_bPatchWeld;
extern bool		g_bPatchDrillDown;
extern bool		g_bPatchInsertMode;
extern bool		g_bPatchBendMode;
extern idVec3	g_vBendOrigin;
