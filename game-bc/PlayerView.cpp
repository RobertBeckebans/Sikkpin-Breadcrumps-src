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

#include "../idlib/precompiled.h"
#pragma hdrstop

#include "Game_local.h"

const int IMPULSE_DELAY = 150;

// ---> sikk - Portal Sky Box
static int MakePowerOfTwo( int num ) {
	int pot;
	for ( pot = 1; pot < num; pot <<= 1 ) {}
	return pot;
}
// <--- sikk - Portal Sky Box

/*
==============
idPlayerView::idPlayerView
==============
*/
idPlayerView::idPlayerView( void ) {
	memset( screenBlobs, 0, sizeof( screenBlobs ) );
	memset( &view, 0, sizeof( view ) );
	player					= NULL;

	tunnelMaterial			= declManager->FindMaterial( "postFX/tunnel" );
	bloodSprayMaterial		= NULL;

// ---> sikk - PostProcess Effects
	blackMaterial			= declManager->FindMaterial( "_black" );
	whiteMaterial			= declManager->FindMaterial( "_white" );
	currentRenderMaterial	= declManager->FindMaterial( "_currentRender" );
	scratchMaterial			= declManager->FindMaterial( "_scratch" );
	edgeAAMaterial			= declManager->FindMaterial( "postFX/edgeAA" );
	//hdrLumMaterial			= declManager->FindMaterial( "postFX/hdrLum" );
	//hdrBloomMaterial		= declManager->FindMaterial( "postFX/hdrBloom" );
	//hdrFinalMaterial		= declManager->FindMaterial( "postFX/hdrFinal" );
	bloomMaterial			= declManager->FindMaterial( "postFX/bloom" );
	ssaoMaterial			= declManager->FindMaterial( "postFX/ssao" );
	//sunShaftsMaterial		= declManager->FindMaterial( "postFX/sunShafts" );
	//dofMaterial				= declManager->FindMaterial( "postFX/dof" );
	colorGradingMaterial	= declManager->FindMaterial( "postFX/colorGrading" );
	//screenWipeMaterial		= declManager->FindMaterial( "postFX/screenwipe" );
	threatFOVMaterial		= declManager->FindMaterial( "postFX/threatFOV" );
	threatProxMaterial		= declManager->FindMaterial( "postFX/threatProx" );
	threatPulseMaterial		= declManager->FindMaterial( "postFX/threatPulse" );
	//bDepthRendered			= false;
	clearSSAO				= false;
// <--- sikk - PostProcess Effects

// ---> sikk - Target Post FX
	targetPostFxMtr = "";
	targetPostFxColor.Set( 1.0f, 1.0f, 1.0f, 1.0f );
// <--- sikk - Target Post FX

	bfgVision				= false;
	dvFinishTime			= 0;
	kickFinishTime			= 0;
	kickAngles.Zero();
	lastDamageTime			= 0.0f;
	fadeTime				= 0;
	fadeRate				= 0.0;
	fadeFromColor.Zero();
	fadeToColor.Zero();
	fadeColor.Zero();
	shakeAng.Zero();

	ClearEffects();
}

/*
==============
idPlayerView::Save
==============
*/
void idPlayerView::Save( idSaveGame *savefile ) const {
	int i;
	const screenBlob_t *blob;

	blob = &screenBlobs[ 0 ];
	for( i = 0; i < MAX_SCREEN_BLOBS; i++, blob++ ) {
		savefile->WriteMaterial( blob->material );
		savefile->WriteFloat( blob->x );
		savefile->WriteFloat( blob->y );
		savefile->WriteFloat( blob->w );
		savefile->WriteFloat( blob->h );
		savefile->WriteFloat( blob->s1 );
		savefile->WriteFloat( blob->t1 );
		savefile->WriteFloat( blob->s2 );
		savefile->WriteFloat( blob->t2 );
		savefile->WriteInt( blob->finishTime );
		savefile->WriteInt( blob->startFadeTime );
		savefile->WriteFloat( blob->driftAmount );
	}

	savefile->WriteInt( dvFinishTime );
	savefile->WriteMaterial( scratchMaterial );
	savefile->WriteInt( kickFinishTime );
	savefile->WriteAngles( kickAngles );
	savefile->WriteBool( bfgVision );

	savefile->WriteMaterial( tunnelMaterial );
	savefile->WriteMaterial( bloodSprayMaterial );
	savefile->WriteFloat( lastDamageTime );

	savefile->WriteVec4( fadeColor );
	savefile->WriteVec4( fadeToColor );
	savefile->WriteVec4( fadeFromColor );
	savefile->WriteFloat( fadeRate );
	savefile->WriteInt( fadeTime );

	savefile->WriteAngles( shakeAng );

	savefile->WriteObject( player );
	savefile->WriteRenderView( view );
}

/*
==============
idPlayerView::Restore
==============
*/
void idPlayerView::Restore( idRestoreGame *savefile ) {
	int i;
	screenBlob_t *blob;

	blob = &screenBlobs[ 0 ];
	for ( i = 0; i < MAX_SCREEN_BLOBS; i++, blob++ ) {
		savefile->ReadMaterial( blob->material );
		savefile->ReadFloat( blob->x );
		savefile->ReadFloat( blob->y );
		savefile->ReadFloat( blob->w );
		savefile->ReadFloat( blob->h );
		savefile->ReadFloat( blob->s1 );
		savefile->ReadFloat( blob->t1 );
		savefile->ReadFloat( blob->s2 );
		savefile->ReadFloat( blob->t2 );
		savefile->ReadInt( blob->finishTime );
		savefile->ReadInt( blob->startFadeTime );
		savefile->ReadFloat( blob->driftAmount );
	}

	savefile->ReadInt( dvFinishTime );
	savefile->ReadMaterial( scratchMaterial );
	savefile->ReadInt( kickFinishTime );
	savefile->ReadAngles( kickAngles );			
	savefile->ReadBool( bfgVision );

	savefile->ReadMaterial( tunnelMaterial );
	savefile->ReadMaterial( bloodSprayMaterial );
	savefile->ReadFloat( lastDamageTime );

	savefile->ReadVec4( fadeColor );
	savefile->ReadVec4( fadeToColor );
	savefile->ReadVec4( fadeFromColor );
	savefile->ReadFloat( fadeRate );
	savefile->ReadInt( fadeTime );

	savefile->ReadAngles( shakeAng );

	savefile->ReadObject( reinterpret_cast<idClass *&>( player ) );
	savefile->ReadRenderView( view );
}

/*
==============
idPlayerView::SetPlayerEntity
==============
*/
void idPlayerView::SetPlayerEntity( idPlayer *playerEnt ) {
	player = playerEnt;
}

/*
==============
idPlayerView::ClearEffects
==============
*/
void idPlayerView::ClearEffects( void ) {
	lastDamageTime = MS2SEC( gameLocal.time - 99999 );

	dvFinishTime = ( gameLocal.time - 99999 );
	kickFinishTime = ( gameLocal.time - 99999 );

	for ( int i = 0; i < MAX_SCREEN_BLOBS; i++ ) {
		screenBlobs[i].finishTime = gameLocal.time;
	}

	fadeTime = 0;
	bfgVision = false;
}

/*
==============
idPlayerView::GetScreenBlob
==============
*/
screenBlob_t *idPlayerView::GetScreenBlob( void ) {
	screenBlob_t *oldest = &screenBlobs[0];

	for ( int i = 1 ; i < MAX_SCREEN_BLOBS ; i++ ) {
		if ( screenBlobs[i].finishTime < oldest->finishTime ) {
			oldest = &screenBlobs[i];
		}
	}
	return oldest;
}

/*
==============
idPlayerView::DamageImpulse

LocalKickDir is the direction of force in the player's coordinate system,
which will determine the head kick direction
==============
*/
void idPlayerView::DamageImpulse( idVec3 localKickDir, const idDict *damageDef ) {
	// keep shotgun from obliterating the view
	if ( lastDamageTime > 0.0f && SEC2MS( lastDamageTime ) + IMPULSE_DELAY > gameLocal.time ) {
		return;
	}

	float dvTime = damageDef->GetFloat( "dv_time" );
	if ( dvTime ) {
		if ( dvFinishTime < gameLocal.time ) {
			dvFinishTime = gameLocal.time;
		}
		dvFinishTime += g_dvTime.GetFloat() * dvTime;
		// don't let it add up too much in god mode
		if ( dvFinishTime > gameLocal.time + 5000 ) {
			dvFinishTime = gameLocal.time + 5000;
		}
	}

	// head angle kick
	float kickTime = damageDef->GetFloat( "kick_time" );
	if ( kickTime ) {
		kickFinishTime = gameLocal.time + g_kickTime.GetFloat() * kickTime;

		// forward / back kick will pitch view
		kickAngles[0] = localKickDir[0];
		// side kick will yaw view
		kickAngles[1] = localKickDir[1] * 0.5f;
		// up / down kick will pitch view
		kickAngles[0] += localKickDir[2];
		// roll will come from side
		kickAngles[2] = localKickDir[1];

		float kickAmplitude = damageDef->GetFloat( "kick_amplitude" );
		if ( kickAmplitude ) {
			kickAngles *= kickAmplitude;
		}
	}

	// screen blob
	float blobTime = damageDef->GetFloat( "blob_time" );
	if ( blobTime ) {
		screenBlob_t *blob = GetScreenBlob();
		blob->startFadeTime = gameLocal.time;
		blob->finishTime = gameLocal.time + blobTime * g_blobTime.GetFloat();
	
		blob->driftAmount = 0.0f;	// sikk - Blood Spray Screen Effect - keeps damage blood splats from drifting 

		const char *materialName = damageDef->GetString( "mtr_blob" );
		blob->material = declManager->FindMaterial( materialName );
		blob->x = damageDef->GetFloat( "blob_x" );
		blob->x += ( gameLocal.random.RandomInt() & 63 ) - 32;
		blob->y = damageDef->GetFloat( "blob_y" );
		blob->y += ( gameLocal.random.RandomInt() & 63 ) - 32;
		
		float scale = ( 256 + ( ( gameLocal.random.RandomInt() & 63 ) - 32 ) ) / 256.0f;
		blob->w = damageDef->GetFloat( "blob_width" ) * g_blobSize.GetFloat() * scale;
		blob->h = damageDef->GetFloat( "blob_height" ) * g_blobSize.GetFloat() * scale;
		blob->s1 = 0;
		blob->t1 = 0;
		blob->s2 = 1;
		blob->t2 = 1;
	}

	// save lastDamageTime for tunnel vision attenuation
	lastDamageTime = MS2SEC( gameLocal.time );
}

/*
==================
idPlayerView::AddBloodSpray

If we need a more generic way to add blobs then we can do that
but having it localized here lets the material be pre-looked up etc.
==================
*/
void idPlayerView::AddBloodSpray( float duration ) {
	if ( duration <= 0 || bloodSprayMaterial == NULL || g_skipViewEffects.GetBool() ) {
		return;
	}

// ---> sikk - Blood Spray Screen Effect
	// Use random material
	if ( gameLocal.random.RandomFloat() < 0.5f )
		bloodSprayMaterial = declManager->FindMaterial( "postFX/bloodspray1" );
	else
		bloodSprayMaterial = declManager->FindMaterial( "postFX/bloodspray2" );
// <--- sikk - Blood Spray Screen Effect

	// visit this for chainsaw
	screenBlob_t *blob = GetScreenBlob();
	blob->startFadeTime = gameLocal.time;
	blob->finishTime = gameLocal.time + SEC2MS( duration );
	blob->material = bloodSprayMaterial;
	blob->x = ( gameLocal.random.RandomInt() & 63 ) - 32;
	blob->y = ( gameLocal.random.RandomInt() & 63 ) - 32;
	blob->driftAmount = 0.0f;// 0.5f + gameLocal.random.CRandomFloat() * 0.5f;	// sikk - No more drifting
	float scale = ( 256 + ( ( gameLocal.random.RandomInt() & 63 ) - 32 ) ) / 256.0f;
	blob->w = 640 * g_blobSize.GetFloat() * scale;	// sikk - This was "600". Typo?
	blob->h = 480 * g_blobSize.GetFloat() * scale;
	float s1 = 0.0f;
	float t1 = 0.0f;
	float s2 = 1.0f;
	float t2 = 1.0f;
// ---> sikk - No more drifting
/*	if ( blob->driftAmount < 0.6 ) {
		s1 = 1.0f;
		s2 = 0.0f;
	} else if ( blob->driftAmount < 0.75 ) {
		t1 = 1.0f;
		t2 = 0.0f;
	} else if ( blob->driftAmount < 0.85 ) {
		s1 = 1.0f;
		s2 = 0.0f;
		t1 = 1.0f;
		t2 = 0.0f;
	}*/
	float f = gameLocal.random.CRandomFloat();
	if ( f < 0.25 ) {
		s1 = 1.0f;
		s2 = 0.0f;
	} else if ( f < 0.5 ) {
		t1 = 1.0f;
		t2 = 0.0f;
	} else if ( f < 0.75 ) {
		s1 = 1.0f;
		s2 = 0.0f;
		t1 = 1.0f;
		t2 = 0.0f;
	}
// <--- sikk - No more drifting
	blob->s1 = s1;
	blob->t1 = t1;
	blob->s2 = s2;
	blob->t2 = t2;
}

/*
===================
idPlayerView::CalculateShake
===================
*/
void idPlayerView::CalculateShake( void ) {
	idVec3	origin, matrix;
	float shakeVolume = gameSoundWorld->CurrentShakeAmplitudeForPosition( gameLocal.time, player->firstPersonViewOrigin );

	// shakeVolume should somehow be molded into an angle here
	// it should be thought of as being in the range 0.0 -> 1.0, although
	// since CurrentShakeAmplitudeForPosition() returns all the shake sounds
	// the player can hear, it can go over 1.0 too.
	shakeAng[0] = gameLocal.random.CRandomFloat() * shakeVolume;
	shakeAng[1] = gameLocal.random.CRandomFloat() * shakeVolume;
	shakeAng[2] = gameLocal.random.CRandomFloat() * shakeVolume;
}

/*
===================
idPlayerView::ShakeAxis
===================
*/
idMat3 idPlayerView::ShakeAxis( void ) const {
	return shakeAng.ToMat3();
}

/*
===================
idPlayerView::AngleOffset

  kickVector, a world space direction that the attack should 
===================
*/
idAngles idPlayerView::AngleOffset( void ) const {
	idAngles ang;

	ang.Zero();

	if ( gameLocal.time < kickFinishTime ) {
		float offset = kickFinishTime - gameLocal.time;
		ang = kickAngles * offset * offset * g_kickAmplitude.GetFloat();
		for ( int i = 0; i < 3; i++ ) {
			ang[i] = idMath::ClampFloat( -70.0f, 70.0f, ang[i] );
		}
	}
	return ang;
}

/*
==================
idPlayerView::SingleView
==================
*/
void idPlayerView::SingleView( const renderView_t *view ) {
	// normal rendering
	if ( !view ) {
		return;
	}

	// hack the shake in at the very last moment, so it can't cause any consistency problems
	hackedView = *view;
	hackedView.viewaxis = hackedView.viewaxis * ShakeAxis();

// ---> sikk - Portal Sky Box
	if ( gameLocal.portalSkyEnt.GetEntity()  && gameLocal.IsPortalSkyAcive() && g_enablePortalSky.GetBool() ) {
		renderView_t portalView = hackedView;
		portalView.vieworg = gameLocal.portalSkyEnt.GetEntity()->GetPhysics()->GetOrigin();
		portalView.cramZNear = true;
		gameRenderWorld->RenderScene( &portalView );
		renderSystem->CaptureRenderToImage( "_portalRender" );
		hackedView.forceUpdate = true;	// FIX: for smoke particles not drawing when portalSky present
	}
// <--- sikk - Portal Sky Box

	if ( r_useSSAO.GetBool() ) {
		renderView_t ssaoView = hackedView;
		PostFX_SSAO( &ssaoView );
		clearSSAO = true;
		hackedView.forceUpdate = true;	// FIX: for smoke particles not drawing when portalSky present
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, blackMaterial );
	} else if ( clearSSAO ) {
		renderSystem->CropRenderSize( 1, 1, true, true );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, whiteMaterial );
		renderSystem->CaptureRenderToImage( "_ssao" );
		renderSystem->UnCrop();
		clearSSAO = false;
	}

	gameRenderWorld->RenderScene( &hackedView );
}

/*
=================
idPlayerView::Flash

flashes the player view with the given color
=================
*/
void idPlayerView::Flash( idVec4 color, int time ) {
	Fade( idVec4( 0.0f, 0.0f, 0.0f, 0.0f ), time);
	fadeFromColor = colorWhite;
}

/*
=================
idPlayerView::Fade

used for level transition fades
assumes: color.w is 0 or 1
=================
*/
void idPlayerView::Fade( idVec4 color, int time ) {
	if ( !fadeTime ) {
		fadeFromColor.Set( 0.0f, 0.0f, 0.0f, 1.0f - color[ 3 ] );
	} else {
		fadeFromColor = fadeColor;
	}
	fadeToColor = color;

	if ( time <= 0 ) {
		fadeRate = 0;
		time = 0;
		fadeColor = fadeToColor;
	} else {
		fadeRate = 1.0f / (float)time;
	}

	if ( gameLocal.realClientTime == 0 && time == 0 ) {
		fadeTime = 1;
	} else {
		fadeTime = gameLocal.realClientTime + time;
	}
}

/*
=================
idPlayerView::ScreenFade
=================
*/
void idPlayerView::ScreenFade( void ) {
	int msec = fadeTime - gameLocal.realClientTime;
	float t;

	if ( msec <= 0 ) {
		fadeColor = fadeToColor;
		if ( fadeColor[ 3 ] == 0.0f )
			fadeTime = 0;
	} else {
		t = (float)msec * fadeRate;
		fadeColor = fadeFromColor * t + fadeToColor * ( 1.0f - t );
	}

	if ( fadeColor[ 3 ] != 0.0f ) {
		renderSystem->SetColor4( fadeColor[ 0 ], fadeColor[ 1 ], fadeColor[ 2 ], fadeColor[ 3 ] );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, whiteMaterial );
	}
}

/*
===================
idPlayerView::RenderPlayerView
===================
*/
void idPlayerView::RenderPlayerView( idUserInterface *hud ) {
	const renderView_t *view = player->GetRenderView();

	// place the sound origin for the player
	gameSoundWorld->PlaceListener( view->vieworg, view->viewaxis, player->entityNumber + 1, gameLocal.time, hud ? hud->State().GetString( "location" ) : "Undefined" );

	SingleView( view );

	if ( !g_skipViewEffects.GetBool() )
		DoPostFX();

	// if the objective system is up, don't draw hud
	player->DrawHUD( hud );

	if ( fadeTime )
		ScreenFade();

	//prevTime = gameLocal.time;	// sikk - update prevTime
}

// ---> sikk - PostProcess Effects
/*
===================
idPlayerView::DoPostFX
===================
*/
void idPlayerView::DoPostFX( void ) {
	//bDepthRendered = false;

	if ( r_useEdgeAA.GetBool() ) {
		PostFX_EdgeAA();
	//} else {
	//	renderSystem->CaptureRenderToImage( "_currentRender" );
	//	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, declManager->FindMaterial( "currentRender" ) );
	}

	//if ( r_useSunShafts.GetBool() )
	//	PostFX_SunShafts();

	//if ( r_useHDR.GetBool() )
	//	PostFX_HDR();

	if ( r_useBloom.GetBool() /*&& !r_useHDR.GetBool()*/ )
		PostFX_Bloom();

	//if ( r_useDepthOfField.GetBool() )
	//	PostFX_DoF();

	if ( r_useColorGrading.GetBool() )
		PostFX_ColorGrading();

	PostFX_ThreatLevelFX();

	//if ( r_useLensFlares.GetBool() )
	//	PostFX_LensFlare();

	if ( targetPostFxMtr[0] != '\0' )
		PostFX_TargetPostFX();

	//PostFX_ScreenBlobs();

	if ( !gameLocal.inCinematic )
		PostFX_TunnelVision();

	if ( player->GetInfluenceMaterial() || player->GetInfluenceEntity() )
		PostFX_InfluenceVision();

	if ( g_doubleVision.GetBool() && gameLocal.time < dvFinishTime )
		PostFX_DoubleVision();

	//if ( player->doEndLevel )
	//	PostFX_ScreenWipe();

	// test a single material drawn over everything
	if ( g_testPostProcess.GetString()[0] ) {
		const idMaterial *mtr = declManager->FindMaterial( g_testPostProcess.GetString(), false );
		if ( !mtr ) {
			common->Printf( "Material not found.\n" );
			g_testPostProcess.SetString( "" );
		} else {
			renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, 1.0f );
			renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, mtr );
		}
	}
}

/*
===================
idPlayerView::RenderDepth
===================
*/
//void idPlayerView::RenderDepth( void ) {
//	if ( bDepthRendered )
//		return;
//
//	renderSystem->CaptureDepthToImage( "_depth" );
//	bDepthRendered = true;
//}

/*
===================
idPlayerView::PostFX_EdgeAA
===================
*/
void idPlayerView::PostFX_EdgeAA( void ) {
	renderSystem->CaptureRenderToImage( "_currentRender" );
	renderSystem->SetColor4( r_edgeAASampleScale.GetFloat(), r_edgeAAFilterScale.GetFloat(), 1.0f, r_useEdgeAA.GetFloat() );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, edgeAAMaterial );
}

/*
===================
idPlayerView::PostFX_HDR
===================
*/
/*void idPlayerView::PostFX_HDR( void ) {
	float fElapsedTime	= MS2SEC( gameLocal.time - prevTime );
	int	nWidthPoT		= MakePowerOfTwo( renderSystem->GetScreenWidth() );
	int	nHeightPoT		= MakePowerOfTwo( renderSystem->GetScreenHeight() );
	int	nBloomWidth		= nWidthPoT / 4;
	int	nBloomHeight	= nHeightPoT / 4;

	// capture original scene image
	renderSystem->CaptureRenderToImage( "_currentRender" );

	// create lower res luminance map
	renderSystem->CropRenderSize( 256, 256, true, true );
	renderSystem->SetShaderParm( 4, 0 );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, hdrLumMaterial );
	renderSystem->CaptureRenderToImage( "_hdrLum" );
	renderSystem->CaptureRenderToImage( "_hdrLumAvg" );
	renderSystem->UnCrop();

	// create average scene luminance map by using a 4x4 downsampling chain and box-filtering
	// Output will be a 1x1 pixel of the average luminance
	for ( int i = 256; i > 1; i *= 0.5 ) {
		renderSystem->CropRenderSize( i, i, true, true );
		renderSystem->SetShaderParm( 4, 1 );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, hdrLumMaterial );
		renderSystem->CaptureRenderToImage( "_hdrLumAvg" );
		renderSystem->UnCrop();
	}

	// create adapted luminance map based on current average luminance and previous adapted luminance maps
	renderSystem->CropRenderSize( 2, 2, true, true );
	renderSystem->SetColor4( r_hdrAdaptationRate.GetFloat(), fElapsedTime, r_hdrLumThresholdMin.GetFloat(), r_hdrLumThresholdMax.GetFloat() );
	renderSystem->SetShaderParm( 4, 2 );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, hdrLumMaterial );
	renderSystem->CaptureRenderToImage( "_hdrLumAdpt" );
	renderSystem->UnCrop();

	// perform bright pass filter on _currentRender for bloom/glare/flare textures
	if ( r_hdrBloomScale.GetFloat() ) {
		// downsize bloom texture
		if ( r_hdrBloomHighQuality.GetBool() ) {
			//for ( int i = nWidthPoT, j = nHeightPoT; i < nBloomWidth && j < nBloomHeight; i *= 0.5f, j *= 0.5f ) {
			//	renderSystem->CropRenderSize( i, j, true );
			//	renderSystem->SetColor4( r_hdrBloomMiddleGray.GetFloat(), r_hdrBloomWhitePoint.GetFloat(), r_hdrBloomThreshold.GetFloat(), r_hdrBloomOffset.GetFloat() );
			//	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, hdrBloomDownsizeMaterial );
			//	renderSystem->CaptureRenderToImage( "_hdrBloom" );
			//	renderSystem->UnCrop();
			//}
		} else {
			renderSystem->CropRenderSize( nBloomWidth, nBloomHeight, true, true );
			renderSystem->SetShaderParm( 4, 0 );
			renderSystem->SetColor4( r_hdrBloomMiddleGray.GetFloat(), r_hdrBloomWhitePoint.GetFloat(), r_hdrBloomThreshold.GetFloat(), r_hdrBloomOffset.GetFloat() );
			renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, hdrBloomMaterial );
			renderSystem->CaptureRenderToImage( "_hdrBloom" );
		}

		// blur bloom texture
		for ( int i = 0; i < 2; i++ ) {
			renderSystem->SetColor4( r_hdrBloomSize.GetFloat(), 0.0f, 1.0f, 1.0f );
			renderSystem->SetShaderParm( 4, 1 );
			renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, hdrBloomMaterial );
			renderSystem->CaptureRenderToImage( "_hdrBloom" );
			renderSystem->SetColor4( 0.0f, r_hdrBloomSize.GetFloat(), 1.0, 1.0f );
			renderSystem->SetShaderParm( 4, 1 );
			renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, hdrBloomMaterial );
			renderSystem->CaptureRenderToImage( "_hdrBloom" );
		}
		renderSystem->UnCrop();
	} else {
		// bloom off (clear textures)
		renderSystem->CropRenderSize( 1, 1, true, true );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, blackMaterial );
		renderSystem->CaptureRenderToImage( "_hdrBloom" );
		renderSystem->UnCrop();
	}

	if ( r_hdrDither.GetBool() ) {
		float size = 16.0f * r_hdrDitherSize.GetFloat();
		renderSystem->SetColor4( renderSystem->GetScreenWidth() / size, renderSystem->GetScreenHeight() / size, 1.0f, 1.0f );
		renderSystem->SetShaderParm( 4, 0 );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, hdrFinalMaterial );
		renderSystem->CaptureRenderToImage( "_hdrDither" );
	} else {
		renderSystem->CropRenderSize( 1, 1, true, true );
		renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, 1.0f );
		renderSystem->SetShaderParm( 4, 1 );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, hdrFinalMaterial );
		renderSystem->CaptureRenderToImage( "_hdrDither" );
		renderSystem->UnCrop();
	}

	// perform final tone mapping
	renderSystem->SetColor4( r_hdrMiddleGray.GetFloat(), r_hdrWhitePoint.GetFloat(), r_hdrBlueShiftFactor.GetFloat(), r_hdrBloomScale.GetFloat() );
	renderSystem->SetShaderParm( 4, 2 );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, hdrFinalMaterial );
}*/

/*
===================
idPlayerView::PostFX_Bloom
===================
*/
void idPlayerView::PostFX_Bloom( void ) {
//	int	nWidthPoT		= MakePowerOfTwo( renderSystem->GetScreenWidth() );
//	int	nHeightPoT		= MakePowerOfTwo( renderSystem->GetScreenHeight() );
	int	nBloomWidth		= renderSystem->GetScreenWidth() / 2;//nWidthPoT / 4;
	int	nBloomHeight	= renderSystem->GetScreenHeight() / 2;//nHeightPoT / 4;

	renderSystem->CaptureRenderToImage( "_currentRender" );

	// create bloom texture
	renderSystem->CropRenderSize( nBloomWidth, nBloomHeight, false, true );
	renderSystem->SetColor4( r_bloomGamma.GetFloat(), 1.0f, 1.0f, 0.0f );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, bloomMaterial );
	renderSystem->CaptureRenderToImage( "_bloom" );

	// blur bloom texture
	for ( int i = 0; i < r_bloomBlurIterations.GetInteger(); i++ ) {
		renderSystem->SetColor4( r_bloomBlurScaleX.GetFloat(), 0.0f, 1.0f, 1.0f );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, bloomMaterial );
		renderSystem->CaptureRenderToImage( "_bloom" );
		renderSystem->SetColor4( 0.0f, r_bloomBlurScaleY.GetFloat(), 1.0f, 1.0f );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, bloomMaterial );
		renderSystem->CaptureRenderToImage( "_bloom" );
	}
	renderSystem->UnCrop();

	// blend original and bloom textures
	renderSystem->SetColor4( r_bloomScale.GetFloat(), 1.0f, 1.0f, 2.0f );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, bloomMaterial );
}

/*
===================
idPlayerView::PostFX_SSAO
===================
*/
void idPlayerView::PostFX_SSAO( const renderView_t *view ) {
	//int	nWidth	= MakePowerOfTwo( renderSystem->GetScreenWidth() );
	//int	nHeight	= MakePowerOfTwo( renderSystem->GetScreenHeight() );
	int	nWidth	= renderSystem->GetScreenWidth();
	int	nHeight	= renderSystem->GetScreenHeight();
	bool pot = ( r_ssaoBufferSize.GetInteger() == 2 ) ? false : true;
	if ( r_ssaoBufferSize.GetInteger() > 1 ) {
		nWidth *= 0.5f; nHeight *= 0.5f;
	}

	//renderSystem->CaptureRenderToImage( "_currentRender" );
	//RenderDepth();
	if ( r_ssaoBufferSize.GetBool() ) {
		renderSystem->CropRenderSize( nWidth, nHeight, pot, true );
	}
	gameRenderWorld->RenderSceneSSAO( view );
	// sample occlusion using our depth buffer
	renderSystem->SetColor4( r_ssaoRadius.GetFloat(), r_ssaoBias.GetFloat(), r_ssaoAmount.GetFloat(), r_ssaoSelfOcclusion.GetFloat() );
	renderSystem->SetShaderParm( 4, 0 );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, ssaoMaterial );
	renderSystem->CaptureRenderToImage( "_ssao" );
	// blur ssao buffer
	for ( int i = 0; i < r_ssaoBlurQuality.GetInteger(); i++ ) {
		renderSystem->SetColor4( r_ssaoBlurScale.GetFloat(), r_ssaoBlurEpsilon.GetFloat(), 1.0f, 1.0f );
		renderSystem->SetShaderParm( 4, 1 );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, ssaoMaterial );
		renderSystem->CaptureRenderToImage( "_ssao" );
	}
	if ( r_ssaoBufferSize.GetBool() ) {
		renderSystem->UnCrop();
	}}

/*
===================
idPlayerView::PostFX_SunShafts
===================
*/
/*void idPlayerView::PostFX_SunShafts( void ) {
	idMat3 axis;
	idVec3 origin;
	idVec3 viewVector[3];
	player->GetViewPos( origin, axis );
	player->viewAngles.ToVectors( &viewVector[0], &viewVector[1], &viewVector[2] );

	idVec3 sunOrigin	= idVec3( r_sunOriginX.GetFloat(), r_sunOriginY.GetFloat(), r_sunOriginZ.GetFloat() ) ;
	idVec3 dist			= origin - sunOrigin;
	float length		= dist.Length();
	idVec3 sunVector	= dist / length;

	float VdotS[3];
	for ( int i = 0; i < 3; i++ ) {
		VdotS[i] = viewVector[i] * -sunVector;
	}
//	float sign = VdotS[0];
//	VdotS[0] = idMath::ClampFloat( 0.0f, 1.0f, VdotS[0] );
	idVec3 ndc;
	renderSystem->GlobalToNormalizedDeviceCoordinates( sunOrigin, ndc );
	ndc.x = ndc.x * 0.5 + 0.5;
	ndc.y = ndc.y * 0.5 + 0.5;
	
	int	nWidth	= MakePowerOfTwo( renderSystem->GetScreenWidth() );
	int	nHeight	= MakePowerOfTwo( renderSystem->GetScreenHeight() );

	renderSystem->CaptureRenderToImage( "_currentRender" );
	RenderDepth();

	renderSystem->CropRenderSize( nWidth, nHeight, true, true );
	renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, 1.0f );
	renderSystem->SetShaderParm( 4, 0 );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, sunShaftsMaterial );
	renderSystem->CaptureRenderToImage( "_sunShafts" );

	// blur textures
	for ( int i = 0; i < r_sunShaftsQuality.GetInteger(); i++ ) {
		renderSystem->SetColor4( r_sunShaftsSize.GetFloat(), ndc.x, ndc.y, VdotS[0] );
		renderSystem->SetShaderParm( 4, 1 );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, sunShaftsMaterial );
		renderSystem->CaptureRenderToImage( "_sunShafts" );
	}
	renderSystem->UnCrop();

	// add mask to scene
	renderSystem->SetColor4( r_sunShaftsStrength.GetFloat(), r_sunShaftsHazeStrength.GetFloat(), ndc.x, ndc.y );
	renderSystem->SetShaderParm( 4, 2 );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, sunShaftsMaterial );
}*/

/*
===================
idPlayerView::PostFX_LensFlare
===================
*/
/*void idPlayerView::PostFX_LensFlare( void ) {
	idMat3 axis;
	idVec3 origin;
	idVec3 viewVector[3];
	const idMaterial *mtr;
	player->GetViewPos( origin, axis );
	player->viewAngles.ToVectors( &viewVector[0], &viewVector[1], &viewVector[2] );

	idVec3 sunOrigin	= idVec3( r_sunOriginX.GetFloat(), r_sunOriginY.GetFloat(), r_sunOriginZ.GetFloat() ) ;
	idVec3 dist			= origin - sunOrigin;
	idVec3 sunVector	= dist / dist.Length();

	float VdotS[ 3 ];
	for ( int i = 0; i < 3; i++ ) {
		VdotS[ i ] = viewVector[ i ] * -sunVector;
	}
	VdotS[ 0 ] = idMath::ClampFloat( 0.0f, 1.0f, VdotS[ 0 ] );
	VdotS[ 0 ] *= VdotS[ 0 ];

	
	mtr = declManager->FindMaterial( "postFX/lensFlare1" );
	if ( VdotS[ 0 ] > 0 ) {
		trace_t trace;
		gameLocal.clip.TracePoint( trace, origin, sunOrigin, MASK_SOLID, player );

		if ( trace.c.material->NoFragment() || trace.fraction == 1.0f ) { // Trace succeeded, or it hit a skybox
			float strength = VdotS[ 0 ];
			float length;
			idVec3 ndc;
			idVec2 ssDir, ssDist, uv;

			renderSystem->GlobalToNormalizedDeviceCoordinates( sunOrigin, ndc );

			ndc.x = ndc.x * 0.5f + 0.5f;
			ndc.y = 1.0f - ( ndc.y * 0.5f + 0.5f );
			ssDist.x = 0.5f - ndc.x;
			ssDist.y = 0.5f - ndc.y;
			length = ssDist.Length();
			ssDir = ssDist / length;

			// Draw a lens flare on the screen
			uv.x = ( ndc.x * SCREEN_WIDTH ) - 256.0f;
			uv.y = ( ndc.y * SCREEN_HEIGHT ) - 256.0f;
			renderSystem->SetColor4( VdotS[ 0 ], VdotS[ 0 ], VdotS[ 0 ], 0.0f );
			renderSystem->DrawStretchPic( uv.x, uv.y, 512.0f, 512.0f, 0.0f, 1.0f, 1.0f, 0.0f, mtr );

			uv.x = ( ( length * -1.25f * ssDir.x + 0.5f ) * SCREEN_WIDTH ) - 32.0f;
			uv.y = ( ( length * -1.25f * ssDir.y + 0.5f ) * SCREEN_HEIGHT ) - 32.0f;
			renderSystem->SetColor4( strength * 0.1, strength * 0.3, strength * 1.0, 1.0f );
			renderSystem->DrawStretchPic( uv.x, uv.y, 64.0f, 64.0f, 0.0f, 1.0f, 1.0f, 0.0f, mtr );

			uv.x = ( ( length * 0.05f * ssDir.x + 0.5f ) * SCREEN_WIDTH ) - 16.0f;
			uv.y = ( ( length * 0.05f * ssDir.y + 0.5f ) * SCREEN_HEIGHT ) - 16.0f;
			renderSystem->SetColor4( strength * 0.1, strength * 0.3, strength * 1.0, 1.0f );
			renderSystem->DrawStretchPic( uv.x, uv.y, 32.0f, 32.0f, 0.0f, 1.0f, 1.0f, 0.0f, mtr );

			uv.x = ( ( length * -0.3333f * ssDir.x + 0.5f ) * SCREEN_WIDTH ) - 16.0f;
			uv.y = ( ( length * -0.3333f * ssDir.y + 0.5f ) * SCREEN_HEIGHT ) - 16.0f;
			renderSystem->SetColor4( strength * 0.1, strength * 0.3, strength * 1.0, 2.0f );
			renderSystem->DrawStretchPic( uv.x, uv.y, 32.0f, 32.0f, 0.0f, 1.0f, 1.0f, 0.0f, mtr );

			uv.x = ( ( length * 0.75f * ssDir.x + 0.5f ) * SCREEN_WIDTH ) - 64.0f;
			uv.y = ( ( length * 0.75f * ssDir.y + 0.5f ) * SCREEN_HEIGHT ) - 64.0f;
			renderSystem->SetColor4( strength * 0.1, strength * 0.3, strength * 1.0, 2.0f );
			renderSystem->DrawStretchPic( uv.x, uv.y, 128.0f, 128.0f, 0.0f, 1.0f, 1.0f, 0.0f, mtr );

			uv.x = ( ( length * 0.15f * ssDir.x + 0.5f ) * SCREEN_WIDTH ) - 32.0;
			uv.y = ( ( length * 0.15f * ssDir.y + 0.5f ) * SCREEN_HEIGHT ) - 32.0f;
			renderSystem->SetColor4( strength * 0.1, strength * 0.3, strength * 1.0, 3.0f );
			renderSystem->DrawStretchPic( uv.x, uv.y, 64.0f, 64.0f, 0.0f, 1.0f, 1.0f, 0.0f, mtr );

			uv.x = ( ( length * -0.6f * ssDir.x + 0.5f ) * SCREEN_WIDTH ) - 16.0;
			uv.y = ( ( length * -0.6f * ssDir.y + 0.5f ) * SCREEN_HEIGHT ) - 16.0f;
			renderSystem->SetColor4( strength * 0.1, strength * 0.3, strength * 1.0, 3.0f );
			renderSystem->DrawStretchPic( uv.x, uv.y, 32.0f, 32.0f, 0.0f, 1.0f, 1.0f, 0.0f, mtr );

			uv.x = ( ( length * -0.1f * ssDir.x + 0.5f ) * SCREEN_WIDTH ) - 16.0;
			uv.y = ( ( length * -0.1f * ssDir.y + 0.5f ) * SCREEN_HEIGHT ) - 16.0f;
			renderSystem->SetColor4( strength * 0.1, strength * 0.3, strength * 1.0, 3.0f );
			renderSystem->DrawStretchPic( uv.x, uv.y, 32.0f, 32.0f, 0.0f, 1.0f, 1.0f, 0.0f, mtr );

			uv.x = ( ( length * 0.4f * ssDir.x + 0.5f ) * SCREEN_WIDTH ) - 16.0f;
			uv.y = ( ( length * 0.4f * ssDir.y + 0.5f ) * SCREEN_HEIGHT ) - 16.0f;
			renderSystem->SetColor4( strength * 0.1, strength * 0.3, strength * 1.0, 4.0f );
			renderSystem->DrawStretchPic( uv.x, uv.y, 32.0f, 32.0f, 0.0f, 1.0f, 1.0f, 0.0f, mtr );

			uv.x = ( ( length * -0.4f * ssDir.x + 0.5f ) * SCREEN_WIDTH ) - 32.0f;
			uv.y = ( ( length * -0.4f * ssDir.y + 0.5f ) * SCREEN_HEIGHT ) - 32.0f;
			renderSystem->SetColor4( strength * 0.1, strength * 0.3, strength * 1.0, 4.0f );
			renderSystem->DrawStretchPic( uv.x, uv.y, 64.0f, 64.0f, 0.0f, 1.0f, 1.0f, 0.0f, mtr );

			uv.x = ( ( length * 0.5f * ssDir.x + 0.5f ) * SCREEN_WIDTH ) - 128.0f;
			uv.y = ( ( length * 0.5f * ssDir.y + 0.5f ) * SCREEN_HEIGHT ) - 128.0f;
			renderSystem->SetColor4( strength * 0.1, strength * 0.3, strength * 1.0, 5.0f );
			renderSystem->DrawStretchPic( uv.x, uv.y, 256.0f, 256.0f, 0.0f, 1.0f, 1.0f, 0.0f, mtr );
		}
	}
}*/

/*
===================
idPlayerView::PostFX_DoF
===================
*/
/*void idPlayerView::PostFX_DoF( void ) {
	if ( gameLocal.inCinematic || r_useDepthOfField.GetInteger() > 1 ) {
		int	nWidth	= MakePowerOfTwo( renderSystem->GetScreenWidth() );
		int	nHeight	= MakePowerOfTwo( renderSystem->GetScreenHeight() );

		renderSystem->CaptureRenderToImage( "_currentRender" );

		//RenderDepth();

		renderSystem->CropRenderSize( nWidth, nHeight, true, true );
		if ( r_useDepthOfField.GetInteger() == 2 )
			renderSystem->SetColor4( r_dofNear.GetInteger(), r_dofFocus.GetInteger(), r_dofFar.GetInteger(), 1.0f );
		else if ( gameLocal.inCinematic )
			renderSystem->SetColor4( r_dofNear.GetInteger(), r_dofFocus.GetInteger(), r_dofFar.GetInteger(), 1.0f );	// don't blur in front of the focal plane for cinematics
		else
			renderSystem->SetColor4( player->focusDistance, 1.0f, 1.0f, 0.0f );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, dofMaterial );
		renderSystem->CaptureRenderToImage( "_dof" );
		renderSystem->UnCrop();

		// blur scene using our depth of field mask
		renderSystem->SetColor4( r_dofBlurScale.GetFloat(), r_dofBlurQuality.GetInteger(), 1.0f, 2.0f );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, dofMaterial );
		if ( r_dofBlurQuality.GetInteger() == 2 ) {
			renderSystem->CaptureRenderToImage( "_currentRender" );
			renderSystem->SetColor4( r_dofBlurScale.GetFloat(), r_dofBlurQuality.GetInteger() + 2.0f, 1.0f, 2.0f );
			renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, dofMaterial );
		}
	}
}*/

/*
===================
idPlayerView::PostFX_ColorGrading
===================
*/
void idPlayerView::PostFX_ColorGrading( void ) {
	int	nWidth	= MakePowerOfTwo( renderSystem->GetScreenWidth() );
	int	nHeight	= MakePowerOfTwo( renderSystem->GetScreenHeight() );

	renderSystem->CaptureRenderToImage( "_currentRender" );

	// unsharp mask buffer
	renderSystem->CropRenderSize( nWidth, nHeight, true, true );
	renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, 0.0f );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, colorGradingMaterial );
	renderSystem->CaptureRenderToImage( "_blurRender" );
	renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, 1.0f );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, colorGradingMaterial );
	renderSystem->CaptureRenderToImage( "_blurRender" );
	renderSystem->UnCrop();

	renderSystem->SetColor4( r_colorGradingParm.GetInteger(), r_colorGradingSharpness.GetFloat(), 1.0f, 2.0f );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, colorGradingMaterial );
}

/*
===================
idPlayerView::PostFX_DoubleVision
===================
*/
void idPlayerView::PostFX_DoubleVision( void ) {
	int offset	= dvFinishTime - gameLocal.time;
	float scale	= ( offset * g_dvAmplitude.GetFloat() ) > 0.5f ? 0.5f : offset * g_dvAmplitude.GetFloat();
	float shift	= fabs( scale * sin( sqrtf( offset ) * g_dvFrequency.GetFloat() ) ); 

	renderSystem->CaptureRenderToImage( "_scratch" );

	// if double vision, render to a texture
	//renderSystem->CropRenderSize( nWidth, nHeight, true, true );
	//renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, shiftScale.y, shiftScale.x, 0.0f, currentRenderMaterial );
	//renderSystem->CaptureRenderToImage( "_scratch" );
	//renderSystem->UnCrop();

	idVec4 color( 1.0f, 1.0f, 1.0f, 1.0f );

	renderSystem->SetColor4( color.x, color.y, color.z, 1.0f );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, shift, 1.0f, 1.0f, 0.0f, scratchMaterial );
	renderSystem->SetColor4( color.x, color.y, color.z, 0.5f );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 1.0f, 1.0f - shift, 0.0f, scratchMaterial );
}

/*
===================
idPlayerView::PostFX_InfluenceVision
===================
*/
void idPlayerView::PostFX_InfluenceVision( void ) {
	float distance = 0.0f;
	float pct = 1.0f;

	if ( player->GetInfluenceEntity() ) {
		distance = ( player->GetInfluenceEntity()->GetPhysics()->GetOrigin() - player->GetPhysics()->GetOrigin() ).Length();
		if ( player->GetInfluenceRadius() != 0.0f && distance < player->GetInfluenceRadius() ) {
			pct = distance / player->GetInfluenceRadius();
			pct = 1.0f - idMath::ClampFloat( 0.0f, 1.0f, pct );
		}
	}
	if ( player->GetInfluenceMaterial() ) {
		renderSystem->CaptureRenderToImage( "_currentRender" );
		renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, pct );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, player->GetInfluenceMaterial() );
	}// else if ( player->GetInfluenceEntity() ) {
//		int offset = 25 + sinf( gameLocal.time );	// sikk - variable initialized but not used
//		PostFX_DoubleVision( view, pct * offset );
//	}
}

/*
===================
idPlayerView::PostFX_TunnelVision
===================
*/
void idPlayerView::PostFX_TunnelVision( void ) {
	float health = 0.0f;
	float alpha;
	float denominator = 100;

	if ( g_testHealthVision.GetFloat() != 0.0f )
		health = g_testHealthVision.GetFloat();
	else
		health = player->health;

	alpha = idMath::ClampFloat( 0.0f, 1.0f, health / denominator );

	if ( alpha < 1.0f  ) {
		if ( g_testHealthVision.GetFloat() ) {	// sikk - fix for "g_testHealthVision"
			renderSystem->SetColor4( MS2SEC( gameLocal.time ), 1.0f, 1.0f, alpha );
		} else { 
			renderSystem->SetColor4( ( player->health <= 0.0f ) ? MS2SEC( gameLocal.time ) : lastDamageTime, 1.0f, 1.0f, ( player->health <= 0.0f ) ? 0.0f : alpha );
		}

		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, tunnelMaterial );
	}
}

/*
===================
idPlayerView::PostFX_ScreenBlobs
===================
*/
void idPlayerView::PostFX_ScreenBlobs( void ) {
	for ( int i = 0; i < MAX_SCREEN_BLOBS; i++ ) {
		screenBlob_t *blob = &screenBlobs[ i ];

		if ( blob->finishTime <= gameLocal.time )
			continue;

		blob->y += blob->driftAmount;

		float fade = (float)( blob->finishTime - gameLocal.time ) / ( blob->finishTime - blob->startFadeTime );

		if ( fade > 1.0f )
			fade = 1.0f;

		if ( fade ) {
			renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, fade );
			renderSystem->DrawStretchPic( blob->x, blob->y, blob->w, blob->h, blob->s1, blob->t1, blob->s2, blob->t2, blob->material );
		}
	}
}

/*
===================
idPlayerView::PostFX_ScreenWipe
===================
*/
//void idPlayerView::PostFX_ScreenWipe( void ) {
//	renderSystem->CaptureRenderToImage( "_currentRender" );
//	renderSystem->SetColor4( 1.0, 1.0, 1.0, gameLocal.time - player->endLevelTime );
//	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, screenWipeMaterial );
//}

/*
===================
idPlayerView::PostFX_TargetPostFX
===================
*/
void idPlayerView::PostFX_TargetPostFX( void ) {
	renderSystem->CaptureRenderToImage( "_currentRender" );
	renderSystem->SetColor( targetPostFxColor );
	renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, declManager->FindMaterial( targetPostFxMtr ) );
}

/*
===================
idPlayerView::PostFX_ThreatLevelFX
===================
*/
void idPlayerView::PostFX_ThreatLevelFX( void ) {
	float prox = player->threatLevel.proximityLerp.GetCurrentValue( gameLocal.time );
	float fov = player->threatLevel.fovLerp.GetCurrentValue( gameLocal.time );
	float pulse = player->threatLevel.heartbeatLerp.GetCurrentValue( gameLocal.time );
	if ( prox > 0.0f ) {
		renderSystem->CaptureRenderToImage( "_currentRender" );
		renderSystem->SetColor4( prox, 1.0f, 1.0f, 0.0f );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, threatProxMaterial );
		renderSystem->CaptureRenderToImage( "_currentRender" );
		renderSystem->SetColor4( prox, 1.0f, 1.0f, 1.0f );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, threatProxMaterial );
		
	}
	if ( fov > 0.0f ) {
//		int offset	= gameLocal.time * 0.25f;
		//float scale	= fov * 4;
//		float shift	= scale * sin( DEG2RAD( offset ) ); 
		float scale	= fov * 0.95;
		renderSystem->CaptureRenderToImage( "_currentRender" );
		renderSystem->SetColor4( scale, 1.0f, 1.0f, 1.0f );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0, 0.0, 1.0, 1.0f, threatFOVMaterial );
		//renderSystem->DrawStretchPic( shift, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0, 1.0, 1.0, 0.0f, currentRenderMaterial );
		//renderSystem->SetColor4( 1.0f, 1.0f, 1.0f, 0.5f );
		//renderSystem->DrawStretchPic( -shift, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0, 1.0, 1.0, 0.0f, currentRenderMaterial );
	}
	if ( pulse > 0.0f ) {
		renderSystem->CaptureRenderToImage( "_currentRender" );
		renderSystem->SetColor4( pulse, 1.0f, 1.0f, 1.0f );
		renderSystem->DrawStretchPic( 0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f, 0.0f, 1.0f, 1.0f, threatPulseMaterial );
	}
	renderSystem->CaptureRenderToImage( "_prevRender" );
}
// <--- sikk - PostProcess Effects