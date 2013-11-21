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

/*
===============================================================================

	Player control of the Doom Marine.
	This object handles all player movement and world interaction.

===============================================================================
*/

// distance between ladder rungs (actually is half that distance, but this sounds better)
const int LADDER_RUNG_DISTANCE = 32;

// amount of health per dose from the health station
const int HEALTH_PER_DOSE = 10;

const int HEALTHPULSE_TIME = 333;

// how many units to raise spectator above default view height so it's in the head of someone
const int SPECTATE_RAISE = 25;

// minimum speed to bob and play run/walk animations at
const float MIN_BOB_SPEED = 5.0f;

const idEventDef EV_Player_GetButtons( "getButtons", NULL, 'd' );
const idEventDef EV_Player_GetMove( "getMove", NULL, 'v' );
const idEventDef EV_Player_GetViewAngles( "getViewAngles", NULL, 'v' );
const idEventDef EV_Player_StopFxFov( "stopFxFov" );
const idEventDef EV_Player_ExitTeleporter( "exitTeleporter" );
const idEventDef EV_Player_LevelTrigger( "levelTrigger" );
const idEventDef EV_SpectatorTouch( "spectatorTouch", "et" );

const idEventDef EV_Player_GetLightBrightness( "getLightBrightness", NULL, 'f' );	// sikk - Player Light
const idEventDef EV_Player_GetCrumbPct( "getCrumbPct", NULL, 'f' );					// sikk - Crumb Count
const idEventDef EV_Player_GetIllumination( "getIllumination", NULL, 'f' );			// sikk - Player Illumination Test

CLASS_DECLARATION( idActor, idPlayer )
	EVENT( EV_Player_GetButtons,			idPlayer::Event_GetButtons )
	EVENT( EV_Player_GetMove,				idPlayer::Event_GetMove )
	EVENT( EV_Player_GetViewAngles,			idPlayer::Event_GetViewAngles )
	EVENT( EV_Player_StopFxFov,				idPlayer::Event_StopFxFov )
	EVENT( EV_Player_ExitTeleporter,		idPlayer::Event_ExitTeleporter )
	EVENT( EV_Player_LevelTrigger,			idPlayer::Event_LevelTrigger )
	EVENT( EV_Gibbed,						idPlayer::Event_Gibbed )

	EVENT( EV_Player_GetLightBrightness,	idPlayer::Event_GetLightBrightness )	// sikk - Player Light
	EVENT( EV_Player_GetCrumbPct,			idPlayer::Event_GetCrumbPct )			// sikk - Crumb Count
	EVENT( EV_Player_GetIllumination,		idPlayer::Event_GetIllumination )		// sikk - Player Illumination Test
END_CLASS

const int MAX_RESPAWN_TIME = 5000;
const int RAGDOLL_DEATH_TIME = 3000;
const int STEPUP_TIME = 200;
const int MAX_INVENTORY_ITEMS = 20;

/*
==============
idInventory::Clear
==============
*/
void idInventory::Clear( void ) {
	maxHealth		= 0;

	items.DeleteContents( true );

	levelTriggers.Clear();

	nextItemPickup = 0;
	nextItemNum = 1;
	onePickupTime = 0;
	pickupItemNames.Clear();

	lastGiveTime = 0;
}

/*
==============
idInventory::GetPersistantData
==============
*/
void idInventory::GetPersistantData( idDict &dict ) {
	int		i;
	int		num;
	idDict	*item;
	idStr	key;
	const idKeyValue *kv;

	// items
	num = 0;
	for( i = 0; i < items.Num(); i++ ) {
		item = items[ i ];

		// copy all keys with "inv_"
		kv = item->MatchPrefix( "inv_" );
		if ( kv ) {
			while( kv ) {
				sprintf( key, "item_%i %s", num, kv->GetKey().c_str() );
				dict.Set( key, kv->GetValue() );
				kv = item->MatchPrefix( "inv_", kv );
			}
			num++;
		}
	}
	dict.SetInt( "items", num );

	dict.SetInt( "levelTriggers", levelTriggers.Num() );
	for ( i = 0; i < levelTriggers.Num(); i++ ) {
		sprintf( key, "levelTrigger_Level_%i", i );
		dict.Set( key, levelTriggers[i].levelName );
		sprintf( key, "levelTrigger_Trigger_%i", i );
		dict.Set( key, levelTriggers[i].triggerName );
	}
}

/*
==============
idInventory::RestoreInventory
==============
*/
void idInventory::RestoreInventory( idPlayer *owner, const idDict &dict ) {
	int			i;
	int			num;
	idDict		*item;
	idStr		key;
	idStr		itemname;
	const idKeyValue *kv;

	Clear();

	// health
	maxHealth = dict.GetInt( "maxhealth", "100" );

	// items
	num = dict.GetInt( "items" );
	items.SetNum( num );
	for( i = 0; i < num; i++ ) {
		item = new idDict();
		items[ i ] = item;
		sprintf( itemname, "item_%i ", i );
		kv = dict.MatchPrefix( itemname );
		while( kv ) {
			key = kv->GetKey();
			key.Strip( itemname );
			item->Set( key, kv->GetValue() );
			kv = dict.MatchPrefix( itemname, kv );
		}
	}

	num = dict.GetInt( "levelTriggers" );
	for ( i = 0; i < num; i++ ) {
		sprintf( itemname, "levelTrigger_Level_%i", i );
		idLevelTriggerInfo lti;
		lti.levelName = dict.GetString( itemname );
		sprintf( itemname, "levelTrigger_Trigger_%i", i );
		lti.triggerName = dict.GetString( itemname );
		levelTriggers.Append( lti );
	}
}

/*
==============
idInventory::Save
==============
*/
void idInventory::Save( idSaveGame *savefile ) const {
	int i;

	savefile->WriteInt( maxHealth );

	savefile->WriteInt( items.Num() );
	for( i = 0; i < items.Num(); i++ ) {
		savefile->WriteDict( items[ i ] );
	}

	savefile->WriteInt( nextItemPickup );
	savefile->WriteInt( nextItemNum );
	savefile->WriteInt( onePickupTime );

	savefile->WriteInt( pickupItemNames.Num() );
	for( i = 0; i < pickupItemNames.Num(); i++ ) {
		savefile->WriteString( pickupItemNames[i].icon );
		savefile->WriteString( pickupItemNames[i].name );
	}

	savefile->WriteInt( levelTriggers.Num() );
	for ( i = 0; i < levelTriggers.Num(); i++ ) {
		savefile->WriteString( levelTriggers[i].levelName );
		savefile->WriteString( levelTriggers[i].triggerName );
	}

	savefile->WriteInt( lastGiveTime );
}

/*
==============
idInventory::Restore
==============
*/
void idInventory::Restore( idRestoreGame *savefile ) {
	int i, num;

	savefile->ReadInt( maxHealth );

	savefile->ReadInt( num );
	for ( i = 0; i < num; i++ ) {
		idDict *itemdict = new idDict;
		savefile->ReadDict( itemdict );
		items.Append( itemdict );
	}

	savefile->ReadInt( nextItemPickup );
	savefile->ReadInt( nextItemNum );
	savefile->ReadInt( onePickupTime );

	savefile->ReadInt( num );
	for ( i = 0; i < num; i++ ) {
		idItemInfo info;
		savefile->ReadString( info.icon );
		savefile->ReadString( info.name );
		pickupItemNames.Append( info );
	}

	savefile->ReadInt( num );
	for ( i = 0; i < num; i++ ) {
		idLevelTriggerInfo lti;
		savefile->ReadString( lti.levelName );
		savefile->ReadString( lti.triggerName );
		levelTriggers.Append( lti );
	}

	savefile->ReadInt( lastGiveTime );
}

/*
==============
idInventory::AddPickupName
==============
*/
void idInventory::AddPickupName( const char *name, const char *icon ) {
	int num;

	num = pickupItemNames.Num();
	if ( ( num == 0 ) || ( pickupItemNames[ num - 1 ].name.Icmp( name ) != 0 ) ) {
		idItemInfo &info = pickupItemNames.Alloc();

		if ( idStr::Cmpn( name, STRTABLE_ID, STRTABLE_ID_LENGTH ) == 0 ) {
			info.name = common->GetLanguageDict()->GetString( name );
		} else {
			info.name = name;
		}
		info.icon = icon;
	} 
}

/*
==============
idInventory::Give
==============
*/
bool idInventory::Give( idPlayer *owner, const idDict &spawnArgs, const char *statname, const char *value, int *idealWeapon, bool updateHud ) {
	idItemInfo				info;

	if ( !idStr::Icmp( statname, "item" ) || !idStr::Icmp( statname, "icon" ) || !idStr::Icmp( statname, "name" ) || !idStr::Icmp( statname, "msg" ) ) {	// sikk - added msg to the list
		// ignore these as they're handled elsewhere
		return false;
	} else {
		// unknown item
		gameLocal.Warning( "Unknown stat '%s' added to player's inventory", statname );
		return false;
	}
}

/*
==============
idPlayer::idPlayer
==============
*/
idPlayer::idPlayer() {
	memset( &usercmd, 0, sizeof( usercmd ) );

	noclip					= false;
	godmode					= false;

	spawnAnglesSet			= false;
	spawnAngles				= ang_zero;
	viewAngles				= ang_zero;
	cmdAngles				= ang_zero;

	oldButtons				= 0;
	buttonMask				= 0;
	oldFlags				= 0;

	lastHitTime				= 0;
	lastSndHitTime			= 0;
	lastSavingThrowTime		= 0;

	hud						= NULL;
	stats					= NULL;	// sikk - Level Stats System: GUI

	heartRate				= BASE_HEARTRATE;
	heartInfo.Init( 0, 0, 0, 0 );
	lastHeartAdjust			= 0;
	lastHeartBeat			= 0;
	lastDmgTime				= 0;
	deathClearContentsTime	= 0;
	stamina					= 0.0f;

	firstPersonViewOrigin	= vec3_zero;
	firstPersonViewAxis		= mat3_identity;

	bobFoot					= 0;
	bobFrac					= 0.0f;
	bobfracsin				= 0.0f;
	bobCycle				= 0;
	xyspeed					= 0.0f;
	stepUpTime				= 0;
	stepUpDelta				= 0.0f;
	idealLegsYaw			= 0.0f;
	legsYaw					= 0.0f;
	legsForward				= true;
	oldViewYaw				= 0.0f;
	viewBobAngles			= ang_zero;
	viewBob					= vec3_zero;
	landChange				= 0;
	landTime				= 0;

	gibDeath				= false;
	gibsLaunched			= false;
	gibsDir					= vec3_zero;

	centerView.Init( 0, 0, 0, 0 );
	fxFov					= false;

	influenceFov			= 0;
	influenceActive			= 0;
	influenceRadius			= 0.0f;
	influenceEntity			= NULL;
	influenceMaterial		= NULL;
	influenceSkin			= NULL;

	privateCameraView		= NULL;

	memset( loggedViewAngles, 0, sizeof( loggedViewAngles ) );
	memset( loggedAccel, 0, sizeof( loggedAccel ) );
	currentLoggedAccel	= 0;

	focusTime				= 0;
	focusGUIent				= NULL;
	focusUI					= NULL;
	focusVehicle			= NULL;
	cursor					= NULL;
	
	oldMouseX				= 0;
	oldMouseY				= 0;

	lastDamageDef			= 0;
	lastDamageDir			= vec3_zero;
	lastDamageLocation		= 0;
	smoothedFrame			= 0;
	smoothedOriginUpdated	= false;
	smoothedOrigin			= vec3_zero;
	smoothedAngles			= ang_zero;

	fl.networkSync			= true;

// ---> sikk - New Initializations
	gameLocal.monsters.Clear();
	numCrumbs				= 0;			// sikk - Crumb Count
	endLevelTime			= 0;			// sikk - End Level Time
	doEndLevel				= false;		// sikk - End Level Flag
	sticky					= 1.0f;			// sikk - trigger_sticky

	focusMoveableTimer		= 0;			// sikk - Object Manipulation

	focusDistance			= 0.0f;			// sikk - Depth of Field PostProcess

// ---> sikk - Threat Level
	threatLevel.heartbeatLerp.Init( 0, 0, 0, 0 );
	threatLevel.fovLerp.Init( 0, 0, 0, 0 );
	threatLevel.feedbackLerp.Init( 0, 0, 0, 0 );
	threatLevel.proximityLerp.Init( 0, 0, 0, 0 );
// <--- sikk - Threat Level

// ---> sikk - Level Music
	musicTime				= 0;
	musicFadeTime			= 0;
	music1ShotTime			= 0;
	musicFlag				= 0;
// <--- sikk - Level Music

// ---> sikk - Player Light
	lightDefHandle = -1;
	lightOffset = vec3_zero;
	lightBrightness = 0.0f;
	lightBrightnessTime = 0;
	lightBrightnessLerp.Init( 0, 0, 0, 0 );
	//memset( &renderLight, 0, sizeof( renderLight ) );
// <--- sikk - Player Light

// <--- sikk - New Initializations
}

/*
==============
idPlayer::~idPlayer()

Release any resources used by the player.
==============
*/
idPlayer::~idPlayer() {
	FreeLightDef();	// sikk - Player Light
}

/*
==============
idPlayer::Init
==============
*/
void idPlayer::Init( void ) {
	const char			*value;
	const idKeyValue	*kv;

	noclip					= false;
	godmode					= false;

	oldButtons				= 0;
	oldFlags				= 0;

	lastDmgTime				= 0;
	lastHeartAdjust			= 0;
	lastHeartBeat			= 0;
	heartInfo.Init( 0, 0, 0, 0 );
	heartRate = BASE_HEARTRATE;
	AdjustHeartRate( BASE_HEARTRATE, 0.0f, 0.0f, true );

	bobCycle				= 0;
	bobFrac					= 0.0f;
	landChange				= 0;
	landTime				= 0;
	centerView.Init( 0, 0, 0, 0 );
	fxFov					= false;

	influenceFov			= 0;
	influenceActive			= 0;
	influenceRadius			= 0.0f;
	influenceEntity			= NULL;
	influenceMaterial		= NULL;
	influenceSkin			= NULL;

	currentLoggedAccel		= 0;

	focusTime				= 0;
	focusGUIent				= NULL;
	focusUI					= NULL;
	focusVehicle			= NULL;

// ---> sikk - New Initializations
	gameLocal.monsters.Clear();
	numCrumbs				= 0;			// sikk - Crumb Count
	endLevelTime			= 0;			// sikk - End Level Time
	doEndLevel				= false;		// sikk - End Level Flag
	sticky					= 1.0f;			// sikk - trigger_sticky

	focusMoveableTimer		= 0;			// sikk - Object Manipulation

	focusDistance			= 0.0f;			// sikk - Depth of Field PostProcess

// ---> sikk - Threat Level
	threatLevel.heartbeatLerp.Init( 0, 0, 0, 0 );
	threatLevel.fovLerp.Init( 0, 0, 0, 0 );
	threatLevel.feedbackLerp.Init( 0, 0, 0, 0 );
	threatLevel.proximityLerp.Init( 0, 0, 0, 0 );
// <--- sikk - Threat Level

// ---> sikk - Level Music
	musicTime				= 0;
	musicFadeTime			= 0;
	music1ShotTime			= 0;
	musicFlag				= 0;
// <--- sikk - Level Music

// ---> sikk - Player Light
	lightDefHandle = -1;
	lightOffset = vec3_zero;
	lightBrightness = 0.0f;
	lightBrightnessTime = 0;
	lightBrightnessLerp.Init( 0, 0, 0, 0 );
	//memset( &renderLight, 0, sizeof( renderLight ) );
// <--- sikk - Player Light

// <--- sikk - New Initializations

	// remove any damage effects
	playerView.ClearEffects();

	// damage values
	fl.takedamage			= true;
	ClearPain();

	// restore persistent data
	RestorePersistantInfo();

	bobCycle		= 0;
	stamina			= 0.0f;

	idealLegsYaw = 0.0f;
	legsYaw = 0.0f;
	legsForward	= true;
	oldViewYaw = 0.0f;

	// set the pm_ cvars
	kv = spawnArgs.MatchPrefix( "pm_", NULL );
	while( kv ) {
		cvarSystem->SetCVarString( kv->GetKey(), kv->GetValue() );
		kv = spawnArgs.MatchPrefix( "pm_", kv );
	}

	// stamina always initialized to maximum
	stamina = pm_stamina.GetFloat();

	gibDeath = false;
	gibsLaunched = false;
	gibsDir.Zero();

	// set the gravity
	physicsObj.SetGravity( gameLocal.GetGravity() );

	// start out standing
	SetEyeHeight( pm_normalviewheight.GetFloat() );

	stepUpTime = 0;
	stepUpDelta = 0.0f;
	viewBobAngles.Zero();
	viewBob.Zero();

	value = spawnArgs.GetString( "model" );
	if ( value && ( *value != 0 ) ) {
		SetModel( value );
	}

	if ( cursor ) {
		cursor->SetStateString( "combatcursor", "1" );
		cursor->SetStateString( "itemcursor", "0" );
		cursor->SetStateString( "guicursor", "0" );
	}

	// initialize the script variables
	AI_FORWARD		= false;
	AI_BACKWARD		= false;
	AI_STRAFE_LEFT	= false;
	AI_STRAFE_RIGHT	= false;
	AI_ATTACK_HELD	= false;
	AI_WEAPON_FIRED	= false;
	AI_JUMP			= false;
	AI_DEAD			= false;
	AI_CROUCH		= false;
	AI_ONGROUND		= true;
	AI_ONLADDER		= false;
	AI_HARDLANDING	= false;
	AI_SOFTLANDING	= false;
	AI_RUN			= false;
	AI_PAIN			= false;
	AI_RELOAD		= false;
	AI_TELEPORT		= false;
	AI_TURN_LEFT	= false;
	AI_TURN_RIGHT	= false;

	// reset the script object
	ConstructScriptObject();

	// execute the script so the script object's constructor takes effect immediately
	scriptThread->Execute();
	
	SetPrivateCameraView( NULL );

	cvarSystem->SetCVarBool( "ui_chat", false );

	// sikk - monster precache hack
	gameLocal.FindEntityDef( "monster_it" );
}

/*
==============
idPlayer::Spawn

Prepare any resources used by the player.
==============
*/
void idPlayer::Spawn( void ) {
	idStr		temp;
	idBounds	bounds;

	if ( entityNumber >= MAX_CLIENTS ) {
		gameLocal.Error( "entityNum > MAX_CLIENTS for player.  Player may only be spawned with a client." );
	}

	// allow thinking during cinematics
	cinematic = true;

	// set our collision model
	physicsObj.SetSelf( this );
	SetClipModel();
	physicsObj.SetMass( spawnArgs.GetFloat( "mass", "100" ) );
	physicsObj.SetContents( CONTENTS_BODY );
	physicsObj.SetClipMask( MASK_PLAYERSOLID );
	SetPhysics( &physicsObj );
	InitAASLocation();

	// only the local player needs guis
	if ( entityNumber == gameLocal.localClientNum ) {
		// load HUD
		if ( spawnArgs.GetString( "hud", "", temp )  ) {
			hud = uiManager->FindGui( temp, true, false, true );
//			stats = uiManager->FindGui( "guis/levelstats.gui", true, false, true );	// sikk - Level Stats System: GUI
		}

		if ( hud ) {
			hud->Activate( true, gameLocal.time );
		}

		// load cursor
		if ( spawnArgs.GetString( "cursor", "", temp ) ) {
			cursor = uiManager->FindGui( temp, true );
		}
		if ( cursor ) {
			cursor->Activate( true, gameLocal.time );
		}
	}

	SetLastHitTime( 0 );

	// set up conditions for animation
	LinkScriptVariables();

	animator.RemoveOriginOffset( true );

	// initialize user info related settings
	// on server, we wait for the userinfo broadcast, as this controls when the player is initially spawned in game
	if ( entityNumber == gameLocal.localClientNum ) {
		UserInfoChanged( false );
	}

	// create combat collision hull for exact collision detection
	SetCombatModel();

	// init the damage effects
	playerView.SetPlayerEntity( this );

	// supress model in non-player views, but allow it in mirrors and remote views
	renderEntity.suppressSurfaceInViewID = entityNumber + 1;

	// don't project shadow on self or weapon
	renderEntity.noSelfShadow = true;

	SpawnFromSpawnSpot();

	// trigger playtesting item gives, if we didn't get here from a previous level
	// the devmap key will be set on the first devmap, but cleared on any level transitions
	if ( gameLocal.serverInfo.FindKey( "devmap" ) ) {
		// fire a trigger with the name "devmap"
		idEntity *ent = gameLocal.FindEntity( "devmap" );
		if ( ent ) {
			ent->ActivateTargets( this );
		}
	}
	
	if ( hud ) {
		hud->StateChanged( gameLocal.time );
	}

	if ( inventory.levelTriggers.Num() ) {
		PostEventMS( &EV_Player_LevelTrigger, 0 );
	}

	// sikk - Always start a map at base health
	health = spawnArgs.GetInt( "basehealth", "100" ) ;	// sikk - Start Fresh with each level
	g_damageScale.SetFloat( 1.0f );

	SpawnMusic();

// ---> sikk - Player Light
	idStr	shaderName;
	idVec3	light_color;
	memset( &renderLight, 0, sizeof( renderLight ) );
	shaderName = spawnArgs.GetString( "mtr_light_shader" );
	if ( *(const char *)shaderName ) {
		renderLight.shader = declManager->FindMaterial( shaderName, false );
		renderLight.pointLight = true;
		renderLight.lightRadius[0] =
		renderLight.lightRadius[1] =
		renderLight.lightRadius[2] = spawnArgs.GetFloat( "light_radius" );
		spawnArgs.GetVector( "light_color", "1 1 1", light_color );
		renderLight.shaderParms[0] = light_color[0];
		renderLight.shaderParms[1] = light_color[1];
		renderLight.shaderParms[2] = light_color[2];
		renderLight.shaderParms[3] = 0.0f; //spawnArgs.GetFloat( "light_brightness", "1.0" ); // sikk - alpha holds light brightness
	}
	spawnArgs.GetVector( "light_offset", "0 0 0", lightOffset );
// <--- sikk - Player Light
}

/*
==============
idPlayer::Think

Called every tic for each player
==============
*/
void idPlayer::Think( void ) {
	// latch button actions
	oldButtons = usercmd.buttons;

	// grab out usercmd
	usercmd_t oldCmd = usercmd;
	usercmd = gameLocal.usercmds[ entityNumber ];
	buttonMask &= usercmd.buttons;
	usercmd.buttons &= ~buttonMask;

	if ( gameLocal.inCinematic && gameLocal.skipCinematic ) {
		return;
	}

	// if this is the very first frame of the map, set the delta view angles
	// based on the usercmd angles
	if ( !spawnAnglesSet && ( gameLocal.GameState() != GAMESTATE_STARTUP ) ) {
		spawnAnglesSet = true;
		SetViewAngles( spawnAngles );
		oldFlags = usercmd.flags;
	}

	if ( gameLocal.inCinematic || influenceActive ) {
		usercmd.forwardmove = 0;
		usercmd.rightmove = 0;
		usercmd.upmove = 0;
	}

	// log movement changes for weapon bobbing effects
	if ( usercmd.forwardmove != oldCmd.forwardmove ) {
		loggedAccel_t	*acc = &loggedAccel[currentLoggedAccel&(NUM_LOGGED_ACCELS-1)];
		currentLoggedAccel++;
		acc->time = gameLocal.time;
		acc->dir[0] = usercmd.forwardmove - oldCmd.forwardmove;
		acc->dir[1] = acc->dir[2] = 0;
	}

	if ( usercmd.rightmove != oldCmd.rightmove ) {
		loggedAccel_t	*acc = &loggedAccel[currentLoggedAccel&(NUM_LOGGED_ACCELS-1)];
		currentLoggedAccel++;
		acc->time = gameLocal.time;
		acc->dir[1] = usercmd.rightmove - oldCmd.rightmove;
		acc->dir[0] = acc->dir[2] = 0;
	}

	// freelook centering
	if ( ( usercmd.buttons ^ oldCmd.buttons ) & BUTTON_MLOOK ) {
		centerView.Init( gameLocal.time, 200, viewAngles.pitch, 0 );
	}

	// if we have an active gui, we will unrotate the view angles as
	// we turn the mouse movements into gui events
	idUserInterface *gui = ActiveGui();
	if ( gui && gui != focusUI ) {
		RouteGuiMouse( gui );
	}

	EvaluateControls();

	Move();

	if ( !g_stopTime.GetBool() ) {
		if ( !noclip && ( health > 0 ) && !IsHidden() ) {
			TouchTriggers();
		}

		SetCurrentHeartRate();

		// not done on clients for various reasons. don't do it on server and save the sound channel for other things
		float scale = g_damageScale.GetFloat();
		if ( g_useDynamicProtection.GetBool() && scale < 1.0f && gameLocal.time - lastDmgTime > 500 ) {
			if ( scale < 1.0f ) {
				scale += 0.05f;
			}
			if ( scale > 1.0f ) {
				scale = 1.0f;
			}
			g_damageScale.SetFloat( scale );
		}

		// update GUIs, Items, and character interactions
		UpdateFocus();
		
		UpdateLocation();

		// update player script
		UpdateScript();

		// service animations
		if ( !gameLocal.inCinematic ) {
    		UpdateConditions();
		}

		// clear out our pain flag so we can tell if we recieve any damage between now and the next time we think
		AI_PAIN = false;
	}

	// calculate the exact bobbed view position, which is used to
	// position the view weapon, among other things
	CalculateFirstPersonView();

	// this may use firstPersonView, or a thirdPeroson / camera view
	CalculateRenderView();

//	UpdateHud();

	UpdateMusic();	// sikk - Level Music

	UpdatePlayerLight( ( usercmd.buttons & BUTTON_ATTACK ) );	// sikk - Player Light

	UpdateMonsterSpawns();	// sikk - Monster Count

	UpdateCrumbCount();	// sikk - Crumb Count

	UpdateThreatLevel();	// sikk - Threat Level

// sikk - no player model
	if ( !g_stopTime.GetBool() ) {
	//	LinkCombat();

		playerView.CalculateShake();
	}

	if ( !( thinkFlags & TH_THINK ) ) {
		gameLocal.Printf( "player %d not thinking?\n", entityNumber );
	}

	if ( g_showEnemies.GetBool() ) {
		idActor *ent;
		int num = 0;
		for( ent = enemyList.Next(); ent != NULL; ent = ent->enemyNode.Next() ) {
			gameLocal.Printf( "enemy (%d)'%s'\n", ent->entityNumber, ent->name.c_str() );
			gameRenderWorld->DebugBounds( colorRed, ent->GetPhysics()->GetBounds().Expand( 2 ), ent->GetPhysics()->GetOrigin() );
			num++;
		}
		gameLocal.Printf( "%d: enemies\n", num );
	}

// ---> sikk - Portal Sky Box
	// determine if portal sky is in pvs
	gameLocal.portalSkyActive = gameLocal.pvs.CheckAreasForPortalSky( gameLocal.GetPlayerPVS(), GetPhysics()->GetOrigin() );
// <--- sikk - Portal Sky Box
}

/*
===========
idPlayer::Save
===========
*/
void idPlayer::Save( idSaveGame *savefile ) const {
	int i;

	savefile->WriteUsercmd( usercmd );
	playerView.Save( savefile );

	savefile->WriteBool( noclip );
	savefile->WriteBool( godmode );

	// don't save spawnAnglesSet, since we'll have to reset them after loading the savegame
	savefile->WriteAngles( spawnAngles );
	savefile->WriteAngles( viewAngles );
	savefile->WriteAngles( cmdAngles );

	savefile->WriteInt( buttonMask );
	savefile->WriteInt( oldButtons );
	savefile->WriteInt( oldFlags );

	savefile->WriteInt( lastHitTime );
	savefile->WriteInt( lastSndHitTime );

	// idBoolFields don't need to be saved, just re-linked in Restore

	inventory.Save( savefile );

	savefile->WriteUserInterface( hud, false );
	savefile->WriteUserInterface( stats, false );	// sikk - Level Stats System

	savefile->WriteInt( heartRate );

	savefile->WriteFloat( heartInfo.GetStartTime() );
	savefile->WriteFloat( heartInfo.GetDuration() );
	savefile->WriteFloat( heartInfo.GetStartValue() );
	savefile->WriteFloat( heartInfo.GetEndValue() );

	savefile->WriteInt( lastHeartAdjust );
	savefile->WriteInt( lastHeartBeat );
	savefile->WriteInt( lastDmgTime );
	savefile->WriteInt( deathClearContentsTime );
	savefile->WriteFloat( stamina );

	savefile->WriteVec3( firstPersonViewOrigin );
	savefile->WriteMat3( firstPersonViewAxis );

	savefile->WriteStaticObject( physicsObj );

	savefile->WriteInt( aasLocation.Num() );
	for ( i = 0; i < aasLocation.Num(); i++ ) {
		savefile->WriteInt( aasLocation[ i ].areaNum );
		savefile->WriteVec3( aasLocation[ i ].pos );
	}

	savefile->WriteInt( bobFoot );
	savefile->WriteFloat( bobFrac );
	savefile->WriteFloat( bobfracsin );
	savefile->WriteInt( bobCycle );
	savefile->WriteFloat( xyspeed );
	savefile->WriteInt( stepUpTime );
	savefile->WriteFloat( stepUpDelta );
	savefile->WriteFloat( idealLegsYaw );
	savefile->WriteFloat( legsYaw );
	savefile->WriteBool( legsForward );
	savefile->WriteFloat( oldViewYaw );
	savefile->WriteAngles( viewBobAngles );
	savefile->WriteVec3( viewBob );
	savefile->WriteInt( landChange );
	savefile->WriteInt( landTime );

	savefile->WriteBool( gibDeath );
	savefile->WriteBool( gibsLaunched );
	savefile->WriteVec3( gibsDir );

	savefile->WriteFloat( centerView.GetStartTime() );
	savefile->WriteFloat( centerView.GetDuration() );
	savefile->WriteFloat( centerView.GetStartValue() );
	savefile->WriteFloat( centerView.GetEndValue() );

	savefile->WriteBool( fxFov );

	savefile->WriteFloat( influenceFov );
	savefile->WriteInt( influenceActive );
	savefile->WriteFloat( influenceRadius );
	savefile->WriteObject( influenceEntity );
	savefile->WriteMaterial( influenceMaterial );
	savefile->WriteSkin( influenceSkin );

	savefile->WriteObject( privateCameraView );

	for ( i = 0; i < NUM_LOGGED_VIEW_ANGLES; i++ ) {
		savefile->WriteAngles( loggedViewAngles[ i ] );
	}
	for ( i = 0; i < NUM_LOGGED_ACCELS; i++ ) {
		savefile->WriteInt( loggedAccel[ i ].time );
		savefile->WriteVec3( loggedAccel[ i ].dir );
	}
	savefile->WriteInt( currentLoggedAccel );

	savefile->WriteObject( focusGUIent );
	// can't save focusUI
	savefile->WriteInt( focusTime );
	savefile->WriteObject( focusVehicle );
	savefile->WriteUserInterface( cursor, false );

	savefile->WriteInt( oldMouseX );
	savefile->WriteInt( oldMouseY );

	savefile->WriteInt( lastDamageDef );
	savefile->WriteVec3( lastDamageDir );
	savefile->WriteInt( lastDamageLocation );
	savefile->WriteInt( smoothedFrame );
	savefile->WriteBool( smoothedOriginUpdated );
	savefile->WriteVec3( smoothedOrigin );
	savefile->WriteAngles( smoothedAngles );

	savefile->WriteInt( numCrumbs );			// sikk - Crumb Count
	savefile->WriteInt( gameLocal.numCrumbs );		// sikk - Crumb Count
//	savefile->WriteInt( gameLocal.monsters );			// sikk - Monster Count

// ---> sikk - Level Music
	savefile->WriteSoundShader( sndAmbientLooping1 );
	savefile->WriteSoundShader( sndAmbientLooping2 );
	savefile->WriteSoundShader( sndAmbient1shot );
// <--- sikk - Level Music

// ---> sikk - Player Light
	savefile->WriteRenderLight( renderLight );
	savefile->WriteInt( (int)lightDefHandle );
	savefile->WriteVec3( lightOffset );
// <--- sikk - Player Light

	if ( hud ) {
		hud->SetStateString( "message", common->GetLanguageDict()->GetString( "#str_02916" ) );
		hud->HandleNamedEvent( "Message" );
	}
}

/*
===========
idPlayer::Restore
===========
*/
void idPlayer::Restore( idRestoreGame *savefile ) {
	int	  i;
	int	  num;
	float set;

	savefile->ReadUsercmd( usercmd );
	playerView.Restore( savefile );

	savefile->ReadBool( noclip );
	savefile->ReadBool( godmode );

	savefile->ReadAngles( spawnAngles );
	savefile->ReadAngles( viewAngles );
	savefile->ReadAngles( cmdAngles );

	memset( usercmd.angles, 0, sizeof( usercmd.angles ) );
	SetViewAngles( viewAngles );
	spawnAnglesSet = true;

	savefile->ReadInt( buttonMask );
	savefile->ReadInt( oldButtons );
	savefile->ReadInt( oldFlags );

	usercmd.flags = 0;
	oldFlags = 0;

	savefile->ReadInt( lastHitTime );
	savefile->ReadInt( lastSndHitTime );

	// Re-link idBoolFields to the scriptObject, values will be restored in scriptObject's restore
	LinkScriptVariables();

	inventory.Restore( savefile );

	savefile->ReadUserInterface( hud );
	savefile->ReadUserInterface( stats );	// sikk - Level Stats System

	savefile->ReadInt( heartRate );

	savefile->ReadFloat( set );
	heartInfo.SetStartTime( set );
	savefile->ReadFloat( set );
	heartInfo.SetDuration( set );
	savefile->ReadFloat( set );
	heartInfo.SetStartValue( set );
	savefile->ReadFloat( set );
	heartInfo.SetEndValue( set );

	savefile->ReadInt( lastHeartAdjust );
	savefile->ReadInt( lastHeartBeat );
	savefile->ReadInt( lastDmgTime );
	savefile->ReadInt( deathClearContentsTime );
	savefile->ReadFloat( stamina );

	savefile->ReadVec3( firstPersonViewOrigin );
	savefile->ReadMat3( firstPersonViewAxis );

	// don't bother saving dragEntity since it's a dev tool
	dragEntity.Clear();

	savefile->ReadStaticObject( physicsObj );
	RestorePhysics( &physicsObj );

	savefile->ReadInt( num );
	aasLocation.SetGranularity( 1 );
	aasLocation.SetNum( num );
	for ( i = 0; i < num; i++ ) {
		savefile->ReadInt( aasLocation[ i ].areaNum );
		savefile->ReadVec3( aasLocation[ i ].pos );
	}

	savefile->ReadInt( bobFoot );
	savefile->ReadFloat( bobFrac );
	savefile->ReadFloat( bobfracsin );
	savefile->ReadInt( bobCycle );
	savefile->ReadFloat( xyspeed );
	savefile->ReadInt( stepUpTime );
	savefile->ReadFloat( stepUpDelta );
	savefile->ReadFloat( idealLegsYaw );
	savefile->ReadFloat( legsYaw );
	savefile->ReadBool( legsForward );
	savefile->ReadFloat( oldViewYaw );
	savefile->ReadAngles( viewBobAngles );
	savefile->ReadVec3( viewBob );
	savefile->ReadInt( landChange );
	savefile->ReadInt( landTime );

	savefile->ReadBool( gibDeath );
	savefile->ReadBool( gibsLaunched );
	savefile->ReadVec3( gibsDir );

	savefile->ReadFloat( set );
	centerView.SetStartTime( set );
	savefile->ReadFloat( set );
	centerView.SetDuration( set );
	savefile->ReadFloat( set );
	centerView.SetStartValue( set );
	savefile->ReadFloat( set );
	centerView.SetEndValue( set );

	savefile->ReadBool( fxFov );

	savefile->ReadFloat( influenceFov );
	savefile->ReadInt( influenceActive );
	savefile->ReadFloat( influenceRadius );
	savefile->ReadObject( reinterpret_cast<idClass *&>( influenceEntity ) );
	savefile->ReadMaterial( influenceMaterial );
	savefile->ReadSkin( influenceSkin );

	savefile->ReadObject( reinterpret_cast<idClass *&>( privateCameraView ) );

	for ( i = 0; i < NUM_LOGGED_VIEW_ANGLES; i++ ) {
		savefile->ReadAngles( loggedViewAngles[ i ] );
	}
	for ( i = 0; i < NUM_LOGGED_ACCELS; i++ ) {
		savefile->ReadInt( loggedAccel[ i ].time );
		savefile->ReadVec3( loggedAccel[ i ].dir );
	}
	savefile->ReadInt( currentLoggedAccel );

	savefile->ReadObject( reinterpret_cast<idClass *&>( focusGUIent ) );
	// can't save focusUI
	focusUI = NULL;
	savefile->ReadInt( focusTime );
	savefile->ReadObject( reinterpret_cast<idClass *&>( focusVehicle ) );
	savefile->ReadUserInterface( cursor );

	savefile->ReadInt( oldMouseX );
	savefile->ReadInt( oldMouseY );

	savefile->ReadInt( lastDamageDef );
	savefile->ReadVec3( lastDamageDir );
	savefile->ReadInt( lastDamageLocation );
	savefile->ReadInt( smoothedFrame );
	savefile->ReadBool( smoothedOriginUpdated );
	savefile->ReadVec3( smoothedOrigin );
	savefile->ReadAngles( smoothedAngles );

	savefile->ReadInt( numCrumbs );				// sikk - Crumb Count
	savefile->ReadInt( gameLocal.numCrumbs );		// sikk - Crumb Count
//	savefile->ReadInt( gameLocal.monsters );			// sikk - Monster Count

// ---> sikk - Level Music
	savefile->ReadSoundShader( sndAmbientLooping1 );
	savefile->ReadSoundShader( sndAmbientLooping2 );
	savefile->ReadSoundShader( sndAmbient1shot );
// <--- sikk - Level Music

// ---> sikk - Player Light
	savefile->ReadRenderLight( renderLight );
	savefile->ReadInt( (int &)lightDefHandle );
	savefile->ReadVec3( lightOffset );
// <--- sikk - Player Light

	// set the pm_ cvars
	const idKeyValue	*kv;
	kv = spawnArgs.MatchPrefix( "pm_", NULL );
	while( kv ) {
		cvarSystem->SetCVarString( kv->GetKey(), kv->GetValue() );
		kv = spawnArgs.MatchPrefix( "pm_", kv );
	}

	grabEntity.Clear();	// sikk - Object Manipulation

	// create combat collision hull for exact collision detection
	SetCombatModel();
}

/*
===============
idPlayer::PrepareForRestart
================
*/
void idPlayer::PrepareForRestart( void ) {
	// we will be restarting program, clear the client entities from program-related things first
	ShutdownThreads();

	// the sound world is going to be cleared, don't keep references to emitters
	FreeSoundEmitter( false );
}

/*
===============
idPlayer::Restart
================
*/
void idPlayer::Restart( void ) {
	idActor::Restart();
	
	// choose a random spot and prepare the point of view in case player is left spectating
	assert( spectating );
	SpawnFromSpawnSpot();
}

/*
==============
idPlayer::LinkScriptVariables

set up conditions for animation
==============
*/
void idPlayer::LinkScriptVariables( void ) {
	AI_FORWARD.LinkTo(			scriptObject, "AI_FORWARD" );
	AI_BACKWARD.LinkTo(			scriptObject, "AI_BACKWARD" );
	AI_STRAFE_LEFT.LinkTo(		scriptObject, "AI_STRAFE_LEFT" );
	AI_STRAFE_RIGHT.LinkTo(		scriptObject, "AI_STRAFE_RIGHT" );
	AI_ATTACK_HELD.LinkTo(		scriptObject, "AI_ATTACK_HELD" );
	AI_WEAPON_FIRED.LinkTo(		scriptObject, "AI_WEAPON_FIRED" );
	AI_JUMP.LinkTo(				scriptObject, "AI_JUMP" );
	AI_DEAD.LinkTo(				scriptObject, "AI_DEAD" );
	AI_CROUCH.LinkTo(			scriptObject, "AI_CROUCH" );
	AI_ONGROUND.LinkTo(			scriptObject, "AI_ONGROUND" );
	AI_ONLADDER.LinkTo(			scriptObject, "AI_ONLADDER" );
	AI_HARDLANDING.LinkTo(		scriptObject, "AI_HARDLANDING" );
	AI_SOFTLANDING.LinkTo(		scriptObject, "AI_SOFTLANDING" );
	AI_RUN.LinkTo(				scriptObject, "AI_RUN" );
	AI_PAIN.LinkTo(				scriptObject, "AI_PAIN" );
	AI_RELOAD.LinkTo(			scriptObject, "AI_RELOAD" );
	AI_TELEPORT.LinkTo(			scriptObject, "AI_TELEPORT" );
	AI_TURN_LEFT.LinkTo(		scriptObject, "AI_TURN_LEFT" );
	AI_TURN_RIGHT.LinkTo(		scriptObject, "AI_TURN_RIGHT" );
}

/*
===========
idPlayer::SelectInitialSpawnPoint

Try to find a spawn point marked 'initial', otherwise
use normal spawn selection.
============
*/
void idPlayer::SelectInitialSpawnPoint( idVec3 &origin, idAngles &angles ) {
	idEntity *spot;
	idStr skin;

	spot = gameLocal.SelectInitialSpawnPoint( this );

	// set the player skin from the spawn location
	if ( spot->spawnArgs.GetString( "skin", NULL, skin ) ) {
		spawnArgs.Set( "spawn_skin", skin );
	}

	// activate the spawn locations targets
	spot->PostEventMS( &EV_ActivateTargets, 0, this );

	origin = spot->GetPhysics()->GetOrigin();
	origin[2] += 4.0f + CM_BOX_EPSILON;		// move up to make sure the player is at least an epsilon above the floor
	angles = spot->GetPhysics()->GetAxis().ToAngles();
}

/*
===========
idPlayer::SpawnFromSpawnSpot

Chooses a spawn location and spawns the player
============
*/
void idPlayer::SpawnFromSpawnSpot( void ) {
	idVec3		spawn_origin;
	idAngles	spawn_angles;
	
	SelectInitialSpawnPoint( spawn_origin, spawn_angles );
	SpawnToPoint( spawn_origin, spawn_angles );
}

/*
===========
idPlayer::SpawnToPoint

Called every time a client is placed fresh in the world:
after the first ClientBegin, and after each respawn
Initializes all non-persistant parts of playerState

when called here with spectating set to true, just place yourself and init
============
*/
void idPlayer::SpawnToPoint( const idVec3 &spawn_origin, const idAngles &spawn_angles ) {
	idVec3 spec_origin;

	assert( !gameLocal.isClient );

	Init();

	fl.noknockback = false;

	// stop any ragdolls being used
	StopRagdoll();

	// set back the player physics
	SetPhysics( &physicsObj );

	physicsObj.SetClipModelAxis();
	physicsObj.EnableClip();

	SetCombatContents( true );

	physicsObj.SetLinearVelocity( vec3_origin );

	// setup our initial view
	SetOrigin( spawn_origin );

	// if this is the first spawn of the map, we don't have a usercmd yet,
	// so the delta angles won't be correct.  This will be fixed on the first think.
	viewAngles = ang_zero;
	SetDeltaViewAngles( ang_zero );
	SetViewAngles( spawn_angles );
	spawnAngles = spawn_angles;
	spawnAnglesSet = false;

	legsForward = true;
	legsYaw = 0.0f;
	idealLegsYaw = 0.0f;
	oldViewYaw = viewAngles.yaw;

	Show();

	idEntityFx::StartFx( spawnArgs.GetString( "fx_spawn" ), &spawn_origin, NULL, this, true );
	AI_TELEPORT = true;

	// kill anything at the new position
	physicsObj.SetClipMask( MASK_PLAYERSOLID ); // the clip mask is usually maintained in Move(), but KillBox requires it
	gameLocal.KillBox( this );

	// don't allow full run speed for a bit
	physicsObj.SetKnockBack( 100 );

	// set our respawn time and buttons so that if we're killed we don't respawn immediately
	minRespawnTime = gameLocal.time;
	maxRespawnTime = gameLocal.time;

	privateCameraView = NULL;

	BecomeActive( TH_THINK );

	// run a client frame to drop exactly to the floor,
	// initialize animations and other things
	Think();
}

/*
===============
idPlayer::SavePersistantInfo

Saves any inventory and player stats when changing levels.
===============
*/
void idPlayer::SavePersistantInfo( void ) {
	idDict &playerInfo = gameLocal.persistentPlayerInfo[ entityNumber ];

	playerInfo.Clear();
	inventory.GetPersistantData( playerInfo );
	playerInfo.SetInt( "health", health );
}

/*
===============
idPlayer::RestorePersistantInfo

Restores any inventory and player stats when changing levels.
===============
*/
void idPlayer::RestorePersistantInfo( void ) {
	spawnArgs.Copy( gameLocal.persistentPlayerInfo[ entityNumber ] );
	inventory.RestoreInventory( this, spawnArgs );
	health = spawnArgs.GetInt( "basehealth", "100" );
}

/*
================
idPlayer::GetUserInfo
================
*/
idDict *idPlayer::GetUserInfo( void ) {
	return &gameLocal.userInfo[ entityNumber ];
}

/*
==============
idPlayer::UserInfoChanged
==============
*/
bool idPlayer::UserInfoChanged( bool canModify ) {
	return false;
}

/*
===============
idPlayer::UpdateHudStats
===============
*/
void idPlayer::UpdateHudStats( idUserInterface *_hud ) {
	assert( _hud );

	_hud->SetStateFloat( "playerSpeed", GetPhysics()->GetLinearVelocity().Length() );
}

/*
===============
idPlayer::DrawHUD
===============
*/
void idPlayer::DrawHUD( idUserInterface *_hud ) {
// ---> sikk - Level Stats System: GUI - we want to stats to draw no matter what
	//if ( scoreBoardOpen ) {
	//	stats->Redraw( gameLocal.realClientTime );
	//}
// <--- sikk - Level Stats System: GUI

	if ( influenceActive != INFLUENCE_NONE || privateCameraView || gameLocal.GetCamera() || !_hud || !g_showHud.GetBool() ) {
		return;
	}

	UpdateHudStats( _hud );

	// FIXME: this is temp to allow the sound meter to show up in the hud
	// it should be commented out before shipping but the code can remain
	// for mod developers to enable for the same functionality
	_hud->SetStateInt( "s_debug", cvarSystem->GetCVarInteger( "s_showLevelMeter" ) );

//	weapon.GetEntity()->UpdateGUI();

	_hud->Redraw( gameLocal.realClientTime );

	// weapon targeting crosshair
	if ( !GuiActive() ) {
		if ( cursor ) {
			cursor->Redraw( gameLocal.realClientTime );
		}
	}
}

/*
===============
idPlayer::EnterCinematic
===============
*/
void idPlayer::EnterCinematic( void ) {
	Hide();
	physicsObj.SetLinearVelocity( vec3_origin );
	SetState( "EnterCinematic" );
	UpdateScript();

	AI_FORWARD		= false;
	AI_BACKWARD		= false;
	AI_STRAFE_LEFT	= false;
	AI_STRAFE_RIGHT	= false;
	AI_RUN			= false;
	AI_ATTACK_HELD	= false;
	AI_WEAPON_FIRED	= false;
	AI_JUMP			= false;
	AI_CROUCH		= false;
	AI_ONGROUND		= true;
	AI_ONLADDER		= false;
	AI_DEAD			= ( health <= 0 );
	AI_RUN			= false;
	AI_PAIN			= false;
	AI_HARDLANDING	= false;
	AI_SOFTLANDING	= false;
	AI_RELOAD		= false;
	AI_TELEPORT		= false;
	AI_TURN_LEFT	= false;
	AI_TURN_RIGHT	= false;
}

/*
===============
idPlayer::ExitCinematic
===============
*/
void idPlayer::ExitCinematic( void ) {
	Show();
	SetState( "ExitCinematic" );
	UpdateScript();
}

/*
=====================
idPlayer::UpdateConditions
=====================
*/
void idPlayer::UpdateConditions( void ) {
	idVec3	velocity;
	float	fallspeed;
	float	forwardspeed;
	float	sidespeed;

	// minus the push velocity to avoid playing the walking animation and sounds when riding a mover
	velocity = physicsObj.GetLinearVelocity() - physicsObj.GetPushedLinearVelocity();
	fallspeed = velocity * physicsObj.GetGravityNormal();

	if ( influenceActive ) {
		AI_FORWARD		= false;
		AI_BACKWARD		= false;
		AI_STRAFE_LEFT	= false;
		AI_STRAFE_RIGHT	= false;
	} else if ( gameLocal.time - lastDmgTime < 500 ) {
		forwardspeed = velocity * viewAxis[ 0 ];
		sidespeed = velocity * viewAxis[ 1 ];
		AI_FORWARD		= AI_ONGROUND && ( forwardspeed > 20.01f );
		AI_BACKWARD		= AI_ONGROUND && ( forwardspeed < -20.01f );
		AI_STRAFE_LEFT	= AI_ONGROUND && ( sidespeed > 20.01f );
		AI_STRAFE_RIGHT	= AI_ONGROUND && ( sidespeed < -20.01f );
	} else if ( xyspeed > MIN_BOB_SPEED ) {
		AI_FORWARD		= AI_ONGROUND && ( usercmd.forwardmove > 0 );
		AI_BACKWARD		= AI_ONGROUND && ( usercmd.forwardmove < 0 );
		AI_STRAFE_LEFT	= AI_ONGROUND && ( usercmd.rightmove < 0 );
		AI_STRAFE_RIGHT	= AI_ONGROUND && ( usercmd.rightmove > 0 );
	} else {
		AI_FORWARD		= false;
		AI_BACKWARD		= false;
		AI_STRAFE_LEFT	= false;
		AI_STRAFE_RIGHT	= false;
	}

	AI_RUN			= ( usercmd.buttons & BUTTON_RUN ) && ( ( !pm_stamina.GetFloat() ) || ( stamina > pm_staminathreshold.GetFloat() ) );
	AI_DEAD			= ( health <= 0 );
}

/*
===============
idPlayer::Give
===============
*/
bool idPlayer::Give( const char *statname, const char *value ) {
	if ( AI_DEAD ) {
		return false;
	}

	if ( !idStr::Icmp( statname, "health" ) ) {
		int amount = atoi( value );

		if ( amount ) {
			health += amount;
			health = health > inventory.maxHealth ? inventory.maxHealth : health;
		}
	} else if ( !idStr::Icmp( statname, "crumb" ) ) {	// sikk - Cumb Count
		int amount = atoi( value );
		numCrumbs += amount;
	}
	return true;
}

/*
===============
idPlayer::GiveItem

Returns false if the item shouldn't be picked up
===============
*/
bool idPlayer::GiveItem( idItem *item ) {
	int					i;
	const idKeyValue	*arg;
	idDict				attr;
	bool				gave;
	int					numPickup;

	item->GetAttributes( attr );

	gave = false;
	numPickup = inventory.pickupItemNames.Num();
	for ( i = 0; i < attr.GetNumKeyVals(); i++ ) {
		arg = attr.GetKeyVal( i );
		if ( Give( arg->GetKey(), arg->GetValue() ) ) {
			gave = true;
		}
	}

	// display the pickup feedback on the hud
	if ( gave && ( numPickup == inventory.pickupItemNames.Num() ) ) {
		inventory.AddPickupName( item->spawnArgs.GetString( "inv_name" ), item->spawnArgs.GetString( "inv_icon" ) );
	}

// ---> sikk - Centerprint Messages
	const char *msg;
	item->spawnArgs.GetString( "inv_msg", "", &msg );
	if ( msg[0] != '\0' ) {
		Centerprint( msg );
	}
// <--- sikk - Centerprint Messages

	return gave;
}

/*
===============
idPlayer::GiveInventoryItem
===============
*/
bool idPlayer::GiveInventoryItem( idDict *item ) {
	inventory.items.Append( new idDict( *item ) );
	idItemInfo info;
	const char* itemName = item->GetString( "inv_name" );
	if ( idStr::Cmpn( itemName, STRTABLE_ID, STRTABLE_ID_LENGTH ) == 0 ) {
		info.name = common->GetLanguageDict()->GetString( itemName );
	} else {
		info.name = itemName;
	}
//	info.icon = item->GetString( "inv_icon" );
	info.icon = item->GetString( "inv_msg" );
	inventory.pickupItemNames.Append( info );
	if ( !info.icon.IsEmpty() ) {
		Centerprint( info.icon );	// sikk - Centerprint Messages
	}

	return true;
}

/*
===============
idPlayer::FindInventoryItem
===============
*/
idDict *idPlayer::FindInventoryItem( const char *name ) {
	for ( int i = 0; i < inventory.items.Num(); i++ ) {
		const char *iname = inventory.items[i]->GetString( "inv_name" );
		if ( iname && *iname ) {
			if ( idStr::Icmp( name, iname ) == 0 ) {
				return inventory.items[i];
			}
		}
	}
	return NULL;
}

/*
===============
idPlayer::RemoveInventoryItem
===============
*/
void idPlayer::RemoveInventoryItem( const char *name ) {
	idDict *item = FindInventoryItem( name );
	if ( item ) {
		RemoveInventoryItem( item );
	}
}

/*
===============
idPlayer::RemoveInventoryItem
===============
*/
void idPlayer::RemoveInventoryItem( idDict *item ) {
	inventory.items.Remove( item );
	delete item;
}

/*
===============
idPlayer::GiveItem
===============
*/
void idPlayer::GiveItem( const char *itemname ) {
	idDict args;

	args.Set( "classname", itemname );
	args.Set( "owner", name.c_str() );
	gameLocal.SpawnEntityDef( args );
	if ( hud ) {
		hud->HandleNamedEvent( "itemPickup" );
	}
}

/*
===============
idPlayer::ActiveGui
===============
*/
idUserInterface *idPlayer::ActiveGui( void ) {
	return focusUI;
}

/*
===============
idPlayer::HandleSingleGuiCommand
===============
*/
bool idPlayer::HandleSingleGuiCommand( idEntity *entityGui, idLexer *src ) {
	idToken token;

	if ( !src->ReadToken( &token ) ) {
		return false;
	}

	if ( token == ";" ) {
		return false;
	}

	if ( token.Icmp( "addhealth" ) == 0 ) {
		if ( entityGui && health < 100 ) {
			int _health = entityGui->spawnArgs.GetInt( "gui_parm1" );
			int amt = ( _health >= HEALTH_PER_DOSE ) ? HEALTH_PER_DOSE : _health;
			_health -= amt;
			entityGui->spawnArgs.SetInt( "gui_parm1", _health );
			if ( entityGui->GetRenderEntity() && entityGui->GetRenderEntity()->gui[ 0 ] ) {
				entityGui->GetRenderEntity()->gui[ 0 ]->SetStateInt( "gui_parm1", _health );
			}
			health += amt;
			if ( health > 100 ) {
				health = 100;
			}
		}
		return true;
	}

	if ( token.Icmp( "ready" ) == 0 ) {
		PerformImpulse( IMPULSE_17 );
		return true;
	}

	src->UnreadToken( &token );
	return false;
}

/*
==============
idPlayer::Collide
==============
*/
bool idPlayer::Collide( const trace_t &collision, const idVec3 &velocity ) {
	idEntity *other;

	other = gameLocal.entities[ collision.c.entityNum ];
	if ( other ) {
		other->Signal( SIG_TOUCH );
		if ( other->RespondsTo( EV_Touch ) ) {
			other->ProcessEvent( &EV_Touch, this, &collision );
		}
	}
	return false;
}

/*
================
idPlayer::UpdateLocation

Searches nearby locations 
================
*/
void idPlayer::UpdateLocation( void ) {
	if ( hud ) {
		idLocationEntity *locationEntity = gameLocal.LocationForPoint( GetEyePosition() );
		if ( locationEntity ) {
			hud->SetStateString( "location", locationEntity->GetLocation() );
		} else {
			hud->SetStateString( "location", common->GetLanguageDict()->GetString( "#str_02911" ) );
		}
	}
}

/*
================
idPlayer::ClearFocus

Clears the focus cursor
================
*/
void idPlayer::ClearFocus( void ) {
	focusGUIent		= NULL;
	focusUI			= NULL;
	focusVehicle	= NULL;
	focusMoveable	= NULL;	// sikk - Object Manipulation
	focusMoveableId	= 0;	// sikk - Object Manipulation
}

/*
================
idPlayer::UpdateFocus

Searches nearby entities for interactive guis, possibly making one of them
the focus and sending it a mouse move event
================
*/
void idPlayer::UpdateFocus( void ) {
	idClipModel *clipModelList[ MAX_GENTITIES ];
	idClipModel *clip;
	int			listedClipModels;
	idEntity	*oldFocus;
	idEntity	*ent;
	idUserInterface *oldUI;
	idAFEntity_Vehicle *oldVehicle;
	int			i, j;
	idVec3		start, end;
	bool		allowFocus;
	const char *command;
	trace_t		trace;
	guiPoint_t	pt;
	const idKeyValue *kv;
	sysEvent_t	ev;
	idUserInterface *ui;

	if ( gameLocal.inCinematic ) {
		return;
	}

// ---> sikk - Depth of Field PostProcess
	//if ( r_useDepthOfField.GetInteger() == 3 && renderView ) {
	//	start = renderView->vieworg;
	//	end = start + renderView->viewaxis.ToAngles().ToForward() * 16384.0f;
	//	gameLocal.clip.TracePoint( trace, start, end, MASK_SHOT_RENDERMODEL, this );
	//	focusDistance = focusDistance * 0.95 + trace.fraction * 0.05;
	//}
// <--- sikk - Depth of Field PostProcess

	// only update the focus character when attack button isn't pressed so players
	// can still chainsaw NPC's
	if ( usercmd.buttons & BUTTON_ATTACK ) {
		allowFocus = false;
	} else {
		allowFocus = true;
	}

	oldFocus		= focusGUIent;
	oldUI			= focusUI;
	oldVehicle		= focusVehicle;

	if ( focusTime <= gameLocal.time ) {
		ClearFocus();
	}

	start = GetEyePosition();
	end = start + viewAngles.ToForward() * 80.0f;

	idBounds bounds( start );
	bounds.AddPoint( end );

	listedClipModels = gameLocal.clip.ClipModelsTouchingBounds( bounds, -1, clipModelList, MAX_GENTITIES );

	// no pretense at sorting here, just assume that there will only be one active
	// gui within range along the trace
	for ( i = 0; i < listedClipModels; i++ ) {
		clip = clipModelList[ i ];
		ent = clip->GetEntity();

		if ( ent->IsHidden() ) {
			continue;
		}

		if ( allowFocus ) {
			if ( ent->IsType( idAFEntity_Vehicle::Type ) ) {
				gameLocal.clip.TracePoint( trace, start, end, MASK_SHOT_RENDERMODEL, this );
				if ( ( trace.fraction < 1.0f ) && ( trace.c.entityNum == ent->entityNumber ) ) {
					ClearFocus();
					focusVehicle = static_cast<idAFEntity_Vehicle *>( ent );
					focusTime = gameLocal.time + FOCUS_TIME;
					break;
				}
				continue;
			}
		}

		if ( !ent->GetRenderEntity() || !ent->GetRenderEntity()->gui[ 0 ] || !ent->GetRenderEntity()->gui[ 0 ]->IsInteractive() ) {
			continue;
		}

		if ( ent->spawnArgs.GetBool( "inv_item" ) ) {
			// don't allow guis on pickup items focus
			continue;
		}

		pt = gameRenderWorld->GuiTrace( ent->GetModelDefHandle(), start, end );
		if ( pt.x != -1 ) {
			// we have a hit
			renderEntity_t *focusGUIrenderEntity = ent->GetRenderEntity();
			if ( !focusGUIrenderEntity ) {
				continue;
			}

			if ( pt.guiId == 1 ) {
				ui = focusGUIrenderEntity->gui[ 0 ];
			} else if ( pt.guiId == 2 ) {
				ui = focusGUIrenderEntity->gui[ 1 ];
			} else {
				ui = focusGUIrenderEntity->gui[ 2 ];
			}
			
			if ( ui == NULL ) {
				continue;
			}

			ClearFocus();
			focusGUIent = ent;
			focusUI = ui;

			if ( oldFocus != ent ) {
				// new activation
				// going to see if we have anything in inventory a gui might be interested in
				// need to enumerate inventory items
				focusUI->SetStateInt( "inv_count", inventory.items.Num() );
				for ( j = 0; j < inventory.items.Num(); j++ ) {
					idDict *item = inventory.items[ j ];
					const char *iname = item->GetString( "inv_name" );
					const char *iicon = item->GetString( "inv_icon" );
					const char *itext = item->GetString( "inv_text" );

					focusUI->SetStateString( va( "inv_name_%i", j), iname );
					focusUI->SetStateString( va( "inv_icon_%i", j), iicon );
					focusUI->SetStateString( va( "inv_text_%i", j), itext );
					kv = item->MatchPrefix("inv_id", NULL);
					if ( kv ) {
						focusUI->SetStateString( va( "inv_id_%i", j ), kv->GetValue() );
					}
					focusUI->SetStateInt( iname, 1 );
				}

				focusUI->SetStateString( "player_health", va("%i", health ) );
				kv = focusGUIent->spawnArgs.MatchPrefix( "gui_parm", NULL );
				while ( kv ) {
					focusUI->SetStateString( kv->GetKey(), kv->GetValue() );
					kv = focusGUIent->spawnArgs.MatchPrefix( "gui_parm", kv );
				}
			}

			// clamp the mouse to the corner
			ev = sys->GenerateMouseMoveEvent( -2000, -2000 );
			command = focusUI->HandleEvent( &ev, gameLocal.time );
 			HandleGuiCommands( focusGUIent, command );

			// move to an absolute position
			ev = sys->GenerateMouseMoveEvent( pt.x * SCREEN_WIDTH, pt.y * SCREEN_HEIGHT );
			command = focusUI->HandleEvent( &ev, gameLocal.time );
			HandleGuiCommands( focusGUIent, command );
			focusTime = gameLocal.time + FOCUS_GUI_TIME;
			break;
		}
	}

// ---> sikk - Use Function
	gameLocal.clip.TracePoint( trace, start, end, MASK_SHOT_RENDERMODEL | CONTENTS_BODY, this );
	if ( trace.fraction < 1.0f ) {
		ent = gameLocal.entities[ trace.c.entityNum ];
// ---> sikk - Object Manipulation
		if ( ent && ent->IsType( idMoveable::Type ) && g_grabMode.GetBool() ) {
			ClearFocus();
			focusMoveable = ent;
			focusMoveableId = trace.c.id;
			focusTime = gameLocal.time + FOCUS_TIME;
			focusMoveableTimer = gameLocal.time + 1000;
		}
// <--- sikk - Object Manipulation
	}
// <--- sikk - Use Function

	if ( focusGUIent && focusUI ) {
		if ( !oldFocus || oldFocus != focusGUIent ) {
			command = focusUI->Activate( true, gameLocal.time );
			HandleGuiCommands( focusGUIent, command );
			StartSound( "snd_guienter", SND_CHANNEL_ANY, SSF_PRIVATE_SOUND, false, NULL );
		}
	} else if ( oldFocus && oldUI ) {
		command = oldUI->Activate( false, gameLocal.time );
		HandleGuiCommands( oldFocus, command );
		StartSound( "snd_guiexit", SND_CHANNEL_ANY, SSF_PRIVATE_SOUND, false, NULL );
	}
}

/*
=================
idPlayer::CrashLand

Check for hard landings that generate sound events
=================
*/
void idPlayer::CrashLand( const idVec3 &oldOrigin, const idVec3 &oldVelocity ) {
	idVec3		origin, velocity;
	idVec3		gravityVector, gravityNormal;
	float		delta;
	float		hardDelta, fatalDelta;
	float		dist;
	float		vel, acc;
	float		t;
	float		a, b, c, den;
	waterLevel_t waterLevel;
	bool		noDamage;

	AI_SOFTLANDING = false;
	AI_HARDLANDING = false;

	// if the player is not on the ground
	if ( !physicsObj.HasGroundContacts() ) {
		return;
	}

	gravityNormal = physicsObj.GetGravityNormal();

	// if the player wasn't going down
	if ( ( oldVelocity * -gravityNormal ) >= 0.0f ) {
		return;
	}

	waterLevel = physicsObj.GetWaterLevel();

	// never take falling damage if completely underwater
	if ( waterLevel == WATERLEVEL_HEAD ) {
		return;
	}

	// no falling damage if touching a nodamage surface
	noDamage = true;	// sikk - Disable Fall Damage - was "false"

	for ( int i = 0; i < physicsObj.GetNumContacts(); i++ ) {
		const contactInfo_t &contact = physicsObj.GetContact( i );
		if ( contact.material->GetSurfaceFlags() & SURF_NODAMAGE ) {
			noDamage = true;
			StartSound( "snd_land_hard", SND_CHANNEL_VOICE, SSF_PRIVATE_SOUND, false, NULL );
			break;
		}
	}

	origin = GetPhysics()->GetOrigin();
	gravityVector = physicsObj.GetGravity();

	// calculate the exact velocity on landing
	dist = ( origin - oldOrigin ) * -gravityNormal;
	vel = oldVelocity * -gravityNormal;
	acc = -gravityVector.Length();

	a = acc / 2.0f;
	b = vel;
	c = -dist;

	den = b * b - 4.0f * a * c;
	if ( den < 0 ) {
		return;
	}
	t = ( -b - idMath::Sqrt( den ) ) / ( 2.0f * a );

	delta = vel + t * acc;
	delta = delta * delta * 0.0001;

	// reduce falling damage if there is standing water
	if ( waterLevel == WATERLEVEL_WAIST ) {
		delta *= 0.25f;
	}
	if ( waterLevel == WATERLEVEL_FEET ) {
		delta *= 0.5f;
	}

	if ( delta < 1.0f ) {
		return;
	}

	// allow falling a bit further for multiplayer
	fatalDelta	= 75.0f;
	hardDelta	= 50.0f;

	if ( delta > fatalDelta ) {
		AI_HARDLANDING = true;
		landChange = -32;
		landTime = gameLocal.time;
		if ( !noDamage ) {
			pain_debounce_time = gameLocal.time + pain_delay + 1;  // ignore pain since we'll play our landing anim
			Damage( NULL, NULL, idVec3( 0, 0, -1 ), "damage_fatalfall", 1.0f, 0 );
		}
	} else if ( delta > hardDelta ) {
		AI_HARDLANDING = true;
		landChange	= -24;
		landTime	= gameLocal.time;
		if ( !noDamage ) {
			pain_debounce_time = gameLocal.time + pain_delay + 1;  // ignore pain since we'll play our landing anim
			Damage( NULL, NULL, idVec3( 0, 0, -1 ), "damage_hardfall", 1.0f, 0 );
		}
	} else if ( delta > 30 ) {
		AI_HARDLANDING = true;
		landChange	= -16;
		landTime	= gameLocal.time;
		if ( !noDamage ) {
			pain_debounce_time = gameLocal.time + pain_delay + 1;  // ignore pain since we'll play our landing anim
			Damage( NULL, NULL, idVec3( 0, 0, -1 ), "damage_softfall", 1.0f, 0 );
		}
	} else if ( delta > 7 ) {
		AI_SOFTLANDING = true;
		landChange	= -8;
		landTime	= gameLocal.time;
	} else if ( delta > 3 ) {
		// just walk on
	}
}

/*
===============
idPlayer::BobCycle
===============
*/
void idPlayer::BobCycle( const idVec3 &pushVelocity ) {
	float		bobmove;
	int			old, deltaTime;
	idVec3		vel, gravityDir, velocity;
	idMat3		viewaxis;
	float		bob;
	float		delta;
	float		speed;
	float		f;

	//
	// calculate speed and cycle to be used for
	// all cyclic walking effects
	//
	velocity = physicsObj.GetLinearVelocity() - pushVelocity;

	gravityDir = physicsObj.GetGravityNormal();
	vel = velocity - ( velocity * gravityDir ) * gravityDir;
	xyspeed = vel.LengthFast();

	if ( !physicsObj.HasGroundContacts() || influenceActive == INFLUENCE_LEVEL2 ) {
		// airborne
		bobCycle = 0;
		bobFoot = 0;
		bobfracsin = 0;
	} else if ( ( !usercmd.forwardmove && !usercmd.rightmove ) || ( xyspeed <= MIN_BOB_SPEED ) ) {
		// start at beginning of cycle again
		bobCycle = 0;
		bobFoot = 0;
		bobfracsin = 0;
	} else {
		if ( physicsObj.IsCrouching() ) {
			bobmove = pm_crouchbob.GetFloat();
			// ducked characters never play footsteps
		} else {
			// vary the bobbing based on the speed of the player
			bobmove = pm_walkbob.GetFloat() * ( 1.0f - bobFrac ) + pm_runbob.GetFloat() * bobFrac;
		}

		// check for footstep / splash sounds
		old = bobCycle;
		bobCycle = (int)( old + bobmove * gameLocal.msec ) & 255;
		bobFoot = ( bobCycle & 128 ) >> 7;
		bobfracsin = idMath::Fabs( sin( ( bobCycle & 127 ) / 127.0 * idMath::PI ) );
	}

	// calculate angles for view bobbing
	viewBobAngles.Zero();

	viewaxis = viewAngles.ToMat3() * physicsObj.GetGravityAxis();

	// add angles based on velocity
	delta = velocity * viewaxis[0];
	viewBobAngles.pitch += delta * pm_runpitch.GetFloat();
	
	delta = velocity * viewaxis[1];
	viewBobAngles.roll -= delta * pm_runroll.GetFloat();

	// add angles based on bob
	// make sure the bob is visible even at low speeds
	speed = xyspeed > 200 ? xyspeed : 200;

	delta = bobfracsin * pm_bobpitch.GetFloat() * speed;
	if ( physicsObj.IsCrouching() ) {
		delta *= 3;		// crouching
	}
	viewBobAngles.pitch += delta;
	delta = bobfracsin * pm_bobroll.GetFloat() * speed;
	if ( physicsObj.IsCrouching() ) {
		delta *= 3;		// crouching accentuates roll
	}
	if ( bobFoot & 1 ) {
		delta = -delta;
	}
	viewBobAngles.roll += delta;

	// calculate position for view bobbing
	viewBob.Zero();

	if ( physicsObj.HasSteppedUp() ) {

		// check for stepping up before a previous step is completed
		deltaTime = gameLocal.time - stepUpTime;
		if ( deltaTime < STEPUP_TIME ) {
			stepUpDelta = stepUpDelta * ( STEPUP_TIME - deltaTime ) / STEPUP_TIME + physicsObj.GetStepUp();
		} else {
			stepUpDelta = physicsObj.GetStepUp();
		}
		if ( stepUpDelta > 2.0f * pm_stepsize.GetFloat() ) {
			stepUpDelta = 2.0f * pm_stepsize.GetFloat();
		}
		stepUpTime = gameLocal.time;
	}

	idVec3 gravity = physicsObj.GetGravityNormal();

	// if the player stepped up recently
	deltaTime = gameLocal.time - stepUpTime;
	if ( deltaTime < STEPUP_TIME ) {
		viewBob += gravity * ( stepUpDelta * ( STEPUP_TIME - deltaTime ) / STEPUP_TIME );
	}

	// add bob height after any movement smoothing
	bob = bobfracsin * xyspeed * pm_bobup.GetFloat();
	if ( bob > 6 ) {
		bob = 6;
	}
	viewBob[2] += bob;

	// add fall height
	delta = gameLocal.time - landTime;
	if ( delta < LAND_DEFLECT_TIME ) {
		f = delta / LAND_DEFLECT_TIME;
		viewBob -= gravity * ( landChange * f );
	} else if ( delta < LAND_DEFLECT_TIME + LAND_RETURN_TIME ) {
		delta -= LAND_DEFLECT_TIME;
		f = 1.0 - ( delta / LAND_RETURN_TIME );
		viewBob -= gravity * ( landChange * f );
	}
}

/*
================
idPlayer::UpdateDeltaViewAngles
================
*/
void idPlayer::UpdateDeltaViewAngles( const idAngles &angles ) {
	// set the delta angle
	idAngles delta;
	for ( int i = 0; i < 3; i++ ) {
		delta[ i ] = angles[ i ] - SHORT2ANGLE( usercmd.angles[ i ] );
	}
	SetDeltaViewAngles( delta );
}

/*
================
idPlayer::SetViewAngles
================
*/
void idPlayer::SetViewAngles( const idAngles &angles ) {
	UpdateDeltaViewAngles( angles );
	viewAngles = angles;
}

/*
================
idPlayer::UpdateViewAngles
================
*/
void idPlayer::UpdateViewAngles( void ) {
	int i;
	idAngles delta;

	if ( !noclip && ( gameLocal.inCinematic || privateCameraView || gameLocal.GetCamera() || influenceActive == INFLUENCE_LEVEL2 ) ) {
		// no view changes at all, but we still want to update the deltas or else when
		// we get out of this mode, our view will snap to a kind of random angle
		UpdateDeltaViewAngles( viewAngles );
		return;
	}

	// if dead
	if ( health <= 0 ) {
		if ( pm_thirdPersonDeath.GetBool() ) {
			viewAngles.roll = 0.0f;
			viewAngles.pitch = 30.0f;
		} else {
			viewAngles.roll = 40.0f;
			viewAngles.pitch = -15.0f;
		}
		return;
	}

	// circularly clamp the angles with deltas
	for ( i = 0; i < 3; i++ ) {
		cmdAngles[i] = SHORT2ANGLE( usercmd.angles[i] );
		if ( influenceActive == INFLUENCE_LEVEL3 ) {
			viewAngles[i] += idMath::ClampFloat( -1.0f, 1.0f, idMath::AngleDelta( idMath::AngleNormalize180( SHORT2ANGLE( usercmd.angles[i]) + deltaViewAngles[i] ) , viewAngles[i] ) );
		} else {
			viewAngles[i] = idMath::AngleNormalize180( SHORT2ANGLE( usercmd.angles[i]) + deltaViewAngles[i] );
		}
	}
	if ( !centerView.IsDone( gameLocal.time ) ) {
		viewAngles.pitch = centerView.GetCurrentValue(gameLocal.time);
	}

	// clamp the pitch
	if ( noclip ) {
		if ( viewAngles.pitch > 89.0f ) {
			// don't let the player look down more than 89 degrees while noclipping
			viewAngles.pitch = 89.0f;
		} else if ( viewAngles.pitch < -89.0f ) {
			// don't let the player look up more than 89 degrees while noclipping
			viewAngles.pitch = -89.0f;
		}
	} else {
		if ( viewAngles.pitch > pm_maxviewpitch.GetFloat() ) {
			// don't let the player look down enough to see the shadow of his (non-existant) feet
			viewAngles.pitch = pm_maxviewpitch.GetFloat();
		} else if ( viewAngles.pitch < pm_minviewpitch.GetFloat() ) {
			// don't let the player look up more than 89 degrees
			viewAngles.pitch = pm_minviewpitch.GetFloat();
		}
	}

	UpdateDeltaViewAngles( viewAngles );

	// orient the model towards the direction we're looking
	SetAngles( idAngles( 0, viewAngles.yaw, 0 ) );

	// save in the log for analyzing weapon angle offsets
	loggedViewAngles[ gameLocal.framenum & (NUM_LOGGED_VIEW_ANGLES-1) ] = viewAngles;
}

/*
==============
idPlayer::AdjustHeartRate

Player heartrate works as follows

DEF_HEARTRATE is resting heartrate

Taking damage when health is above 75 adjusts heart rate by 1 beat per second
Taking damage when health is below 75 adjusts heart rate by 5 beats per second
Maximum heartrate from damage is MAX_HEARTRATE

Firing a weapon adds 1 beat per second up to a maximum of COMBAT_HEARTRATE

Being at less than 25% stamina adds 5 beats per second up to ZEROSTAMINA_HEARTRATE

All heartrates are target rates.. the heart rate will start falling as soon as there have been no adjustments for 5 seconds
Once it starts falling it always tries to get to DEF_HEARTRATE

The exception to the above rule is upon death at which point the rate is set to DYING_HEARTRATE and starts falling 
immediately to zero

Heart rate volumes go from zero ( -40 db for DEF_HEARTRATE to 5 db for MAX_HEARTRATE ) the volume is 
scaled linearly based on the actual rate

Exception to the above rule is once the player is dead, the dying heart rate starts at either the current volume if
it is audible or -10db and scales to 8db on the last few beats
==============
*/
void idPlayer::AdjustHeartRate( int target, float timeInSecs, float delay, bool force ) {
	if ( heartInfo.GetEndValue() == target ) {
		return;
	}
	if ( AI_DEAD && !force ) {
		return;
	}

    lastHeartAdjust = gameLocal.time;
	heartInfo.Init( gameLocal.time + delay * 1000, timeInSecs * 1000, heartRate, target );
}

/*
==============
idPlayer::GetBaseHeartRate
==============
*/
int idPlayer::GetBaseHeartRate( void ) {
	int rate = idMath::FtoiFast( BASE_HEARTRATE + ( ZEROSTAMINA_HEARTRATE - BASE_HEARTRATE ) * ( 1.0f - stamina / pm_stamina.GetFloat() ) );
	rate += idMath::FtoiFast( ( MAX_HEARTRATE - rate ) * ( threatLevel.proximityLerp.GetCurrentValue( gameLocal.time ) + threatLevel.fovLerp.GetCurrentValue( gameLocal.time ) ) * 0.5f );
	if ( rate > MAX_HEARTRATE ) {
		rate = MAX_HEARTRATE;
	}

	if ( g_testHeartRate.GetInteger() > 0 ) {
		rate = g_testHeartRate.GetInteger();
	}
	return rate;
}

/*
==============
idPlayer::SetCurrentHeartRate
==============
*/
void idPlayer::SetCurrentHeartRate( void ) {
	int currentRate = GetBaseHeartRate();
	heartRate = heartInfo.GetCurrentValue( gameLocal.time );

	if ( health >= 0 ) {
		if ( currentRate > heartRate && gameLocal.time > lastHeartAdjust + 1000 ) {
			AdjustHeartRate( currentRate, 1.0f, 0.0f, false );
		} else if ( currentRate < heartRate && gameLocal.time > lastHeartAdjust + 10000 ) { 
			AdjustHeartRate( BASE_HEARTRATE, 10.0f, 0.0f, false );
		}
	}

	int bps = SEC2MS( 60.0f / heartRate );
	float pct = 0.0;
	
	int maxVol = 0;
	if ( heartRate > BASE_HEARTRATE && health > 0 ) {
		pct = idMath::ClampFloat( 0.0f, 1.0f, (float)( heartRate - BASE_HEARTRATE ) / (float)( MAX_HEARTRATE - BASE_HEARTRATE ) );
		maxVol = DMG_VOLUME;
	} else if ( health <= 0 ) {
		pct = idMath::ClampFloat( 0.0f, 1.0f, (float)( heartRate - DYING_HEARTRATE ) / (float)( BASE_HEARTRATE - DYING_HEARTRATE ) );
		maxVol = DEATH_VOLUME;
	} 

	if ( gameLocal.time >= lastHeartBeat + bps ) {
		if ( pct > 0 ) {
			StartSound( "snd_heartbeat", SND_CHANNEL_HEART, SSF_PRIVATE_SOUND, false, NULL );
			// modify just this channel to a custom volume
			soundShaderParms_t	parms;
			memset( &parms, 0, sizeof( parms ) );
			float f = 1.0f - ( ( 1.0f - pct ) * ( 1.0f - pct ) );
			parms.volume = f * ( maxVol - ZERO_VOLUME ) + ZERO_VOLUME;
			refSound.referenceSound->ModifySound( SND_CHANNEL_HEART, &parms );
		//	gameLocal.Printf( "Heartbeat: bps = %d, time = %d\n", bps, gameLocal.time - lastHeartBeat );
			lastHeartBeat = gameLocal.time;
		}
	}

	if ( GetSoundEmitter() ) {
		float pulse = GetSoundEmitter()->CurrentAmplitude( SND_CHANNEL_HEART ) * pct;
		if ( pulse > threatLevel.heartbeatLerp.GetCurrentValue( gameLocal.time ) ) {
			//gameLocal.Printf( "Pulse: pulse = %f, pct = %f\n", GetSoundEmitter()->CurrentAmplitude( SND_CHANNEL_HEART ), pct );
			threatLevel.heartbeatLerp.Init( gameLocal.time, bps, pulse, 0 );	// sikk - heartbeat fx
		}
	}
}

/*
==============
idPlayer::SetClipModel
==============
*/
void idPlayer::SetClipModel( void ) {
	idBounds bounds;

	float bbox = pm_bboxwidth.GetFloat() * 0.5f;
	bounds[0].Set( -bbox, -bbox, 0 );
	bounds[1].Set( bbox, bbox, pm_normalheight.GetFloat() );

	// the origin of the clip model needs to be set before calling SetClipModel
	// otherwise our physics object's current origin value gets reset to 0
	idClipModel *newClip;
	if ( pm_usecylinder.GetBool() ) {
		newClip = new idClipModel( idTraceModel( bounds, 8 ) );
		newClip->Translate( physicsObj.PlayerGetOrigin() );
		physicsObj.SetClipModel( newClip, 1.0f );
	} else {
		newClip = new idClipModel( idTraceModel( bounds ) );
		newClip->Translate( physicsObj.PlayerGetOrigin() );
		physicsObj.SetClipModel( newClip, 1.0f );
	}
}

/*
==============
idPlayer::UseVehicle
==============
*/
void idPlayer::UseVehicle( bool drive ) {
	if ( drive ) {
		Hide();
		focusVehicle->Use( this );
	} else {
		Show();
		static_cast<idAFEntity_Vehicle*>( GetBindMaster() )->Use( this );
	}
}

/*
==============
idPlayer::PerformImpulse
==============
*/
void idPlayer::PerformImpulse( int impulse ) {
	switch( impulse ) {
		case IMPULSE_0: { break; }
		// keep these (5 - 15) open for future weapons
		case IMPULSE_1: { break; }
		case IMPULSE_2: { break; }
		case IMPULSE_3: { break; }
		case IMPULSE_4: { break; }
		case IMPULSE_5: { break; }
		case IMPULSE_6: { break; }
		case IMPULSE_7: { break; }
		case IMPULSE_8: { break; }
		case IMPULSE_9: { break; }
		case IMPULSE_10: { break; }
		case IMPULSE_11: { break; }
		case IMPULSE_12: { break; }
		case IMPULSE_13: { break; }
		case IMPULSE_14: { break; }
		case IMPULSE_15: { break; }
		case IMPULSE_16: { break; }
		case IMPULSE_17: { break; }
		case IMPULSE_18: { break; }
		case IMPULSE_19: { break; }
		case IMPULSE_20: { break; }
		case IMPULSE_21: { break; }
		case IMPULSE_22: { break; }
		case IMPULSE_23: { break; }
		case IMPULSE_24: { break; }
		case IMPULSE_25: { break; }
		case IMPULSE_26: { break; }
		case IMPULSE_27: { break; }
		case IMPULSE_28: { break; }
		case IMPULSE_29: {
			// sikk - do we really need this?
			centerView.Init( gameLocal.time, 200, viewAngles.pitch, 0 );
			break;
		}
// ---> sikk - Use Function: Overriding the default UseVehicle() call but still allowing a vehicle to be used
		case IMPULSE_40: {
			bool valid = false;
			if ( grabEntity.GetGrabEntity() ) {
				grabEntity.StopDrag( this, true );
				valid = true;
			} else {
				if ( GetBindMaster() && GetBindMaster()->IsType( idAFEntity_Vehicle::Type ) ) {
					UseVehicle( false );
					valid = true;
				} else if ( focusVehicle ) {
					UseVehicle( true );
					valid = true;
				} else if ( focusMoveable && grabEntity.GetThrownTime() < gameLocal.time ) {
					grabEntity.StartDrag( this, focusMoveable, focusMoveableId );
					valid = true;
				}
			}
			
			//if ( !valid )
			//	StartSoundShader( declManager->FindSound( "use_fail" ), SND_CHANNEL_VOICE, 0, false, NULL );

			break;
		}
// <--- sikk - Use Function
	} 
}

bool idPlayer::HandleESC( void ) {
	if ( gameLocal.inCinematic ) {
		return SkipCinematic();
	}

	return false;
}

bool idPlayer::SkipCinematic( void ) {
	StartSound( "snd_skipcinematic", SND_CHANNEL_ANY, SSF_PRIVATE_SOUND, false, NULL );
	return gameLocal.SkipCinematic();
}

/*
==============
idPlayer::EvaluateControls
==============
*/
void idPlayer::EvaluateControls( void ) {
	bool restart = false;

	// check for respawning
	if ( health <= 0 ) {
// ---> sikk - Lives System
		if ( ( gameLocal.time > minRespawnTime ) && ( usercmd.buttons & BUTTON_ATTACK ) ) {
			//if ( GetLevelStat( LIVES ) ) {
			//	Respawn();
			//} else {
				restart = true;
			//}
		} else if ( gameLocal.time > maxRespawnTime /*&& GetLevelStat( LIVES ) <= 0*/ ) {
			restart = true;
		}
// <--- sikk - Lives System

		if ( !g_testDeath.GetBool() && restart ) {	// sikk - Lives System
			// in single player, we let the session handle restarting the level or loading a game
			gameLocal.sessionCommand = "died";
		}
	}

	if ( ( usercmd.flags & UCF_IMPULSE_SEQUENCE ) != ( oldFlags & UCF_IMPULSE_SEQUENCE ) ) {
		PerformImpulse( usercmd.impulse );
	}

	oldFlags = usercmd.flags;

	AdjustSpeed();

	// update the viewangles
	UpdateViewAngles();
}

/*
==============
idPlayer::AdjustSpeed
==============
*/
void idPlayer::AdjustSpeed( void ) {
	float speed, crouchspeed, ladderspeed, rate;

	 if ( noclip ) {
		speed = pm_noclipspeed.GetFloat();
		bobFrac = 0.0f;
	} else if ( usercmd.buttons & BUTTON_RUN && ( usercmd.forwardmove || usercmd.rightmove ) ){
		stamina -= MS2SEC( gameLocal.msec );
		if ( stamina < 0 ) {
			stamina = 0;
		}
		if ( ( !pm_stamina.GetFloat() ) || ( stamina > pm_staminathreshold.GetFloat() ) ) {
			bobFrac = 1.0f;
		} else if ( pm_staminathreshold.GetFloat() <= 0.0001f ) {
			bobFrac = 0.0f;
		} else {
			bobFrac = stamina / pm_staminathreshold.GetFloat();
		}
		speed = pm_walkspeed.GetFloat() * ( 1.0f - bobFrac ) + pm_runspeed.GetFloat() * bobFrac;
	} else {
		rate = pm_staminarate.GetFloat();
		// increase 25% faster when not moving
		if ( ( usercmd.forwardmove == 0 ) && ( usercmd.rightmove == 0 ) ) {
			 rate *= 2;
		}
		stamina += rate * MS2SEC( gameLocal.msec );
		if ( stamina > pm_stamina.GetFloat() ) {
			stamina = pm_stamina.GetFloat();
		}
		speed = pm_walkspeed.GetFloat();
		bobFrac = 0.0f;
	}
	crouchspeed = pm_crouchspeed.GetFloat();
	ladderspeed = pm_ladderspeed.GetFloat();	// sikk - 

	if ( influenceActive == INFLUENCE_LEVEL3 ) {
		speed *= 0.33f;
	}

	if ( AI_BACKWARD ) {
		speed *= 0.5f;
	}

	physicsObj.SetSpeed( speed, crouchspeed, ladderspeed );
}

/*
==============
idPlayer::AdjustBodyAngles
==============
*/
void idPlayer::AdjustBodyAngles( void ) {
	idMat3	lookAxis;
	idMat3	legsAxis;
	bool	blend;
	float	diff;
	float	frac;
	float	upBlend;
	float	forwardBlend;
	float	downBlend;

	if ( health < 0 ) {
		return;
	}

	blend = true;

	if ( !physicsObj.HasGroundContacts() ) {
		idealLegsYaw = 0.0f;
		legsForward = true;
	} else if ( usercmd.forwardmove < 0 ) {
		idealLegsYaw = idMath::AngleNormalize180( idVec3( -usercmd.forwardmove, usercmd.rightmove, 0.0f ).ToYaw() );
		legsForward = false;
	} else if ( usercmd.forwardmove > 0 ) {
		idealLegsYaw = idMath::AngleNormalize180( idVec3( usercmd.forwardmove, -usercmd.rightmove, 0.0f ).ToYaw() );
		legsForward = true;
	} else if ( ( usercmd.rightmove != 0 ) && physicsObj.IsCrouching() ) {
		if ( !legsForward ) {
			idealLegsYaw = idMath::AngleNormalize180( idVec3( idMath::Abs( usercmd.rightmove ), usercmd.rightmove, 0.0f ).ToYaw() );
		} else {
			idealLegsYaw = idMath::AngleNormalize180( idVec3( idMath::Abs( usercmd.rightmove ), -usercmd.rightmove, 0.0f ).ToYaw() );
		}
	} else if ( usercmd.rightmove != 0 ) {
		idealLegsYaw = 0.0f;
		legsForward = true;
	} else {
		legsForward = true;
		diff = idMath::Fabs( idealLegsYaw - legsYaw );
		idealLegsYaw = idealLegsYaw - idMath::AngleNormalize180( viewAngles.yaw - oldViewYaw );
		if ( diff < 0.1f ) {
			legsYaw = idealLegsYaw;
			blend = false;
		}
	}

	if ( !physicsObj.IsCrouching() ) {
		legsForward = true;
	}

	oldViewYaw = viewAngles.yaw;

	AI_TURN_LEFT = false;
	AI_TURN_RIGHT = false;
	if ( idealLegsYaw < -45.0f ) {
		idealLegsYaw = 0;
		AI_TURN_RIGHT = true;
		blend = true;
	} else if ( idealLegsYaw > 45.0f ) {
		idealLegsYaw = 0;
		AI_TURN_LEFT = true;
		blend = true;
	}

	if ( blend ) {
		legsYaw = legsYaw * 0.9f + idealLegsYaw * 0.1f;
	}
	legsAxis = idAngles( 0.0f, legsYaw, 0.0f ).ToMat3();
	//animator.SetJointAxis( hipJoint, JOINTMOD_WORLD, legsAxis );

	// calculate the blending between down, straight, and up
	frac = viewAngles.pitch / 90.0f;
	if ( frac > 0.0f ) {
		downBlend		= frac;
		forwardBlend	= 1.0f - frac;
		upBlend			= 0.0f;
	} else {
		downBlend		= 0.0f;
		forwardBlend	= 1.0f + frac;
		upBlend			= -frac;
	}

 //   animator.CurrentAnim( ANIMCHANNEL_TORSO )->SetSyncedAnimWeight( 0, downBlend );
	//animator.CurrentAnim( ANIMCHANNEL_TORSO )->SetSyncedAnimWeight( 1, forwardBlend );
	//animator.CurrentAnim( ANIMCHANNEL_TORSO )->SetSyncedAnimWeight( 2, upBlend );

	//animator.CurrentAnim( ANIMCHANNEL_LEGS )->SetSyncedAnimWeight( 0, downBlend );
	//animator.CurrentAnim( ANIMCHANNEL_LEGS )->SetSyncedAnimWeight( 1, forwardBlend );
	//animator.CurrentAnim( ANIMCHANNEL_LEGS )->SetSyncedAnimWeight( 2, upBlend );
}

/*
==============
idPlayer::InitAASLocation
==============
*/
void idPlayer::InitAASLocation( void ) {
	int		i;
	int		num;
	idVec3	size;
	idBounds bounds;
	idAAS	*aas;
	idVec3	origin;

	GetFloorPos( 64.0f, origin );

	num = gameLocal.NumAAS();
	aasLocation.SetGranularity( 1 );
	aasLocation.SetNum( num );	
	for( i = 0; i < aasLocation.Num(); i++ ) {
		aasLocation[ i ].areaNum = 0;
		aasLocation[ i ].pos = origin;
		aas = gameLocal.GetAAS( i );
		if ( aas && aas->GetSettings() ) {
			size = aas->GetSettings()->boundingBoxes[0][1];
			bounds[0] = -size;
			size.z = 32.0f;
			bounds[1] = size;

			aasLocation[ i ].areaNum = aas->PointReachableAreaNum( origin, bounds, AREA_REACHABLE_WALK );
		}
	}
}

/*
==============
idPlayer::SetAASLocation
==============
*/
void idPlayer::SetAASLocation( void ) {
	int		i;
	int		areaNum;
	idVec3	size;
	idBounds bounds;
	idAAS	*aas;
	idVec3	origin;

	if ( !GetFloorPos( 64.0f, origin ) ) {
		return;
	}
	
	for ( i = 0; i < aasLocation.Num(); i++ ) {
		aas = gameLocal.GetAAS( i );
		if ( !aas ) {
			continue;
		}

		size = aas->GetSettings()->boundingBoxes[0][1];
		bounds[0] = -size;
		size.z = 32.0f;
		bounds[1] = size;

		areaNum = aas->PointReachableAreaNum( origin, bounds, AREA_REACHABLE_WALK );
		if ( areaNum ) {
			aasLocation[ i ].pos = origin;
			aasLocation[ i ].areaNum = areaNum;
		}
	}
}

/*
==============
idPlayer::GetAASLocation
==============
*/
void idPlayer::GetAASLocation( idAAS *aas, idVec3 &pos, int &areaNum ) const {
	int i;

	if ( aas != NULL ) {
		for ( i = 0; i < aasLocation.Num(); i++ ) {
			if ( aas == gameLocal.GetAAS( i ) ) {
				areaNum = aasLocation[ i ].areaNum;
				pos = aasLocation[ i ].pos;
				return;
			}
		}
	}

	areaNum = 0;
	pos = physicsObj.GetOrigin();
}

/*
==============
idPlayer::Move
==============
*/
void idPlayer::Move( void ) {
	float newEyeOffset;
	idVec3 oldOrigin;
	idVec3 oldVelocity;
	idVec3 pushVelocity;

	// save old origin and velocity for crashlanding
	oldOrigin = physicsObj.GetOrigin();
	oldVelocity = physicsObj.GetLinearVelocity();
	pushVelocity = physicsObj.GetPushedLinearVelocity();

	// set physics variables
	physicsObj.SetMaxStepHeight( pm_stepsize.GetFloat() );
	physicsObj.SetMaxJumpHeight( pm_jumpheight.GetFloat());

	if ( noclip ) {
		physicsObj.SetContents( 0 );
		physicsObj.SetMovementType( PM_NOCLIP );
	} else if ( health <= 0 ) {
		physicsObj.SetContents( CONTENTS_CORPSE | CONTENTS_MONSTERCLIP );
		physicsObj.SetMovementType( PM_DEAD );
	} else if ( gameLocal.inCinematic || gameLocal.GetCamera() || privateCameraView || ( influenceActive == INFLUENCE_LEVEL2 ) ) {
		physicsObj.SetContents( CONTENTS_BODY );
		physicsObj.SetMovementType( PM_FREEZE );
	} else {
		physicsObj.SetContents( CONTENTS_BODY );
		physicsObj.SetMovementType( PM_NORMAL );
	}

	if ( health <= 0 ) {
		physicsObj.SetClipMask( MASK_DEADSOLID );
	} else {
		physicsObj.SetClipMask( MASK_PLAYERSOLID );
	}

	physicsObj.SetDebugLevel( g_debugMove.GetBool() );
	physicsObj.SetPlayerInput( usercmd, viewAngles );

	// FIXME: physics gets disabled somehow
	BecomeActive( TH_PHYSICS );
	RunPhysics();

	// update our last valid AAS location for the AI
	SetAASLocation();

	if ( health <= 0 ) {
		newEyeOffset = pm_deadviewheight.GetFloat();
	} else if ( physicsObj.IsCrouching() ) {
		newEyeOffset = pm_crouchviewheight.GetFloat();
	} else if ( GetBindMaster() && GetBindMaster()->IsType( idAFEntity_Vehicle::Type ) ) {
		newEyeOffset = 0.0f;
	} else {
		newEyeOffset = pm_normalviewheight.GetFloat();
	}

	if ( EyeHeight() != newEyeOffset ) {
		// smooth out duck height changes
		SetEyeHeight( EyeHeight() * pm_crouchrate.GetFloat() + newEyeOffset * ( 1.0f - pm_crouchrate.GetFloat() ) );
	}

	if ( noclip || gameLocal.inCinematic || ( influenceActive == INFLUENCE_LEVEL2 ) ) {
		AI_CROUCH	= false;
		AI_ONGROUND	= ( influenceActive == INFLUENCE_LEVEL2 );
		AI_ONLADDER	= false;
		AI_JUMP		= false;
	} else {
		AI_CROUCH	= physicsObj.IsCrouching();
		AI_ONGROUND	= physicsObj.HasGroundContacts();
		AI_ONLADDER	= physicsObj.OnLadder();
		AI_JUMP		= physicsObj.HasJumped();

		// check if we're standing on top of a monster and give a push if we are
		idEntity *groundEnt = physicsObj.GetGroundEntity();
		if ( groundEnt && groundEnt->IsType( idAI::Type ) ) {
			idVec3 vel = physicsObj.GetLinearVelocity();
			if ( vel.ToVec2().LengthSqr() < 0.1f ) {
				vel.ToVec2() = physicsObj.GetOrigin().ToVec2() - groundEnt->GetPhysics()->GetAbsBounds().GetCenter().ToVec2();
				vel.ToVec2().NormalizeFast();
				vel.ToVec2() *= pm_walkspeed.GetFloat();
			} else {
				// give em a push in the direction they're going
				vel *= 1.1f;
			}
			physicsObj.SetLinearVelocity( vel );
		}
	}

	if ( AI_JUMP ) {
		// bounce the view weapon
 		loggedAccel_t *acc = &loggedAccel[ currentLoggedAccel & ( NUM_LOGGED_ACCELS - 1 ) ];
		currentLoggedAccel++;
		acc->time = gameLocal.time;
		acc->dir[2] = 200;
		acc->dir[0] = acc->dir[1] = 0;
	}

	if ( AI_ONLADDER ) {
		int old_rung = oldOrigin.z / LADDER_RUNG_DISTANCE;
		int new_rung = physicsObj.GetOrigin().z / LADDER_RUNG_DISTANCE;

		if ( old_rung != new_rung ) {
			StartSound( "snd_stepladder", SND_CHANNEL_ANY, SSF_PRIVATE_SOUND, false, NULL );
		}
	}

	BobCycle( pushVelocity );
	CrashLand( oldOrigin, oldVelocity );
}

/*
==============
idPlayer::UpdateHud
==============
*/
void idPlayer::UpdateHud( void ) {
// sikk - TODO: Simplify...
	if ( !hud ) {
		return;
	}

	if ( entityNumber != gameLocal.localClientNum ) {
		return;
	}

	int c = inventory.pickupItemNames.Num();
	if ( c > 0 ) {
		if ( gameLocal.time > inventory.nextItemPickup ) {
			if ( inventory.nextItemPickup && gameLocal.time - inventory.nextItemPickup > 2000 ) {
				inventory.nextItemNum = 1;
			}
			int i;
			for ( i = 0; i < 5, i < c; i++ ) {
				hud->SetStateString( va( "itemtext%i", inventory.nextItemNum ), inventory.pickupItemNames[0].name );
				hud->SetStateString( va( "itemicon%i", inventory.nextItemNum ), inventory.pickupItemNames[0].icon );
				hud->HandleNamedEvent( va( "itemPickup%i", inventory.nextItemNum++ ) );
				inventory.pickupItemNames.RemoveIndex( 0 );
				if (inventory.nextItemNum == 1 ) {
					inventory.onePickupTime = gameLocal.time;
				} else 	if ( inventory.nextItemNum > 5 ) {
					inventory.nextItemNum = 1;
					inventory.nextItemPickup = inventory.onePickupTime + 2000;
				} else {
					inventory.nextItemPickup = gameLocal.time + 400;
				}
			}
		}
	}
}

/*
==============
idPlayer::StartFxOnBone
==============
*/
void idPlayer::StartFxOnBone( const char *fx, const char *bone ) {
	idVec3 offset;
	idMat3 axis;
	jointHandle_t jointHandle = GetAnimator()->GetJointHandle( bone );

	if ( jointHandle == INVALID_JOINT ) {
		gameLocal.Printf( "Cannot find bone %s\n", bone );
		return;
	}

	if ( GetAnimator()->GetJointTransform( jointHandle, gameLocal.time, offset, axis ) ) {
		offset = GetPhysics()->GetOrigin() + offset * GetPhysics()->GetAxis();
		axis = axis * GetPhysics()->GetAxis();
	}

	idEntityFx::StartFx( fx, &offset, &axis, this, true );
}

/*
=================
idPlayer::RouteGuiMouse
=================
*/
void idPlayer::RouteGuiMouse( idUserInterface *gui ) {
	sysEvent_t ev;
	const char *command;

	if ( usercmd.mx != oldMouseX || usercmd.my != oldMouseY ) {
		ev = sys->GenerateMouseMoveEvent( usercmd.mx - oldMouseX, usercmd.my - oldMouseY );
		command = gui->HandleEvent( &ev, gameLocal.time );
		oldMouseX = usercmd.mx;
		oldMouseY = usercmd.my;
	}
}

/*
==================
idPlayer::LookAtKiller
==================
*/
void idPlayer::LookAtKiller( idEntity *inflictor, idEntity *attacker ) {
	idVec3 dir;
	
	if ( attacker && attacker != this ) {
		dir = attacker->GetPhysics()->GetOrigin() - GetPhysics()->GetOrigin();
	} else if ( inflictor && inflictor != this ) {
		dir = inflictor->GetPhysics()->GetOrigin() - GetPhysics()->GetOrigin();
	} else {
		dir = viewAxis[ 0 ];
	}

	idAngles ang( 0, dir.ToYaw(), 0 );
	SetViewAngles( ang );
}

/*
==============
idPlayer::Kill
==============
*/
void idPlayer::Kill( bool delayRespawn, bool nodamage ) {
	 if ( health > 0 ) {
		godmode = false;
		Damage( this, this, vec3_origin, "damage_suicide", 1.0f, INVALID_JOINT );
		if ( delayRespawn ) {
			int delay = spawnArgs.GetFloat( "respawn_delay" );
			minRespawnTime = gameLocal.time + SEC2MS( delay );
			maxRespawnTime = minRespawnTime + MAX_RESPAWN_TIME;
		}
	}
}

/*
==================
idPlayer::Killed
==================
*/
void idPlayer::Killed( idEntity *inflictor, idEntity *attacker, int damage, const idVec3 &dir, int location ) {
	float delay;

	assert( !gameLocal.isClient );

	// stop taking knockback once dead
	fl.noknockback = true;
	if ( health < -999 ) {
		health = -999;
	}

	if ( AI_DEAD ) {
		AI_PAIN = true;
		return;
	}

	heartInfo.Init( 0, 0, 0, BASE_HEARTRATE );
	AdjustHeartRate( DEAD_HEARTRATE, 10.0f, 0.0f, true );

// sikk - Lives System
//	if ( GetLevelStat( LIVES ) <= 0 && !g_testDeath.GetBool() ) {
		playerView.Fade( colorBlack, 5000 );
//	}

	AI_DEAD = true;

// ---> sikk - Lives System
	//if ( GetLevelStat( LIVES ) <= 0 ) {
	//	SetAnimState( ANIMCHANNEL_LEGS, "Legs_Death", 4 );
	//	SetAnimState( ANIMCHANNEL_TORSO, "Torso_Death", 4 );
	//	SetWaitState( "" );
	//}
// <--- sikk - Lives System

//	animator.ClearAllJoints();

	if ( StartRagdoll() ) {
		pm_modelView.SetInteger( 0 );
		minRespawnTime = gameLocal.time + RAGDOLL_DEATH_TIME;
		maxRespawnTime = minRespawnTime + MAX_RESPAWN_TIME;
	} else {
		// don't allow respawn until the death anim is done
		// g_forcerespawn may force spawning at some later time
		delay = spawnArgs.GetFloat( "respawn_delay" );
		minRespawnTime = gameLocal.time + SEC2MS( delay );
		maxRespawnTime = minRespawnTime + MAX_RESPAWN_TIME;
	}

	physicsObj.SetMovementType( PM_DEAD );

//	StartSound( "snd_death", SND_CHANNEL_VOICE, SSF_PRIVATE_SOUND, false, NULL );
//	StopSound( SND_CHANNEL_BODY2, false );

	fl.takedamage = true;		// can still be gibbed

	if ( !g_testDeath.GetBool() ) {
		LookAtKiller( inflictor, attacker );
	}

// ---> sikk - Lives System
//	if ( GetLevelStat( LIVES ) <= 0 ) {
		physicsObj.SetContents( CONTENTS_CORPSE | CONTENTS_MONSTERCLIP );
//	}
// <--- sikk - Lives System

	FreeLightDef();	// sikk - Player Light

	UpdateVisuals();
}

/*
=====================
idPlayer::GetAIAimTargets

Returns positions for the AI to aim at.
=====================
*/
void idPlayer::GetAIAimTargets( const idVec3 &lastSightPos, idVec3 &headPos, idVec3 &chestPos ) {
	idVec3 offset;
	idMat3 axis;
	idVec3 origin;
	
	origin = lastSightPos - physicsObj.GetOrigin();
	headPos = idVec3( 0, 0, 68 ) + origin;
	chestPos = idVec3( 0, 0, 48 ) + origin;
}

/*
================
idPlayer::DamageFeedback

callback function for when another entity received damage from this entity.  damage can be adjusted and returned to the caller.
================
*/
void idPlayer::DamageFeedback( idEntity *victim, idEntity *inflictor, int &damage ) {
	assert( !gameLocal.isClient );
	if ( damage && ( victim != this ) && victim->IsType( idActor::Type ) ) {
		SetLastHitTime( gameLocal.time );
	}
}

/*
=================
idPlayer::CalcDamagePoints

Calculates how many health and armor points will be inflicted, but
doesn't actually do anything with them.  This is used to tell when an attack
would have killed the player, possibly allowing a "saving throw"
=================
*/
void idPlayer::CalcDamagePoints( idEntity *inflictor, idEntity *attacker, const idDict *damageDef,
							   const float damageScale, const int location, int *health, int *armor ) {
	int damage, armorDamage = 0;

	damage = damageDef->GetInt( "damage", "20" );
	damage = GetDamageForLocation( damage, location );
	damage *= damageScale;

	// always give half damage if hurting self
	if ( attacker == this ) {
		damage *= damageDef->GetFloat( "selfDamageScale", "1" );
	}

	// check for completely getting out of the damage
	if ( !damageDef->GetBool( "noGod" ) ) {
		// check for godmode
		if ( godmode ) {
			damage = 0;
		}
	}

	// inform the attacker that they hit someone
	attacker->DamageFeedback( this, inflictor, damage );

	*health = damage;
	*armor = armorDamage;
}

/*
============
Damage

this		entity that is being damaged
inflictor	entity that is causing the damage
attacker	entity that caused the inflictor to damage targ
	example: this=monster, inflictor=rocket, attacker=player

dir			direction of the attack for knockback in global space

damageDef	an idDict with all the options for damage effects

inflictor, attacker, dir, and point can be NULL for environmental effects
============
*/
void idPlayer::Damage( idEntity *inflictor, idEntity *attacker, const idVec3 &dir,
					   const char *damageDefName, const float damageScale, const int location ) {
	idVec3		kick;
	int			damage;
	int			armorSave;
	int			knockback;
	idVec3		damage_from;
	idVec3		localDamageVector;	
	float		attackerPushScale;

	if ( !fl.takedamage || noclip || gameLocal.inCinematic ) {
		return;
	}

	if ( !inflictor ) {
		inflictor = gameLocal.world;
	}
	if ( !attacker ) {
		attacker = gameLocal.world;
	}
	
	if ( attacker->IsType( idAI::Type ) ) {
		// don't take damage from monsters during influences
		if ( influenceActive != 0 ) {
			return;
		}
	}

	const idDeclEntityDef *	damageDef = gameLocal.FindEntityDef( damageDefName, false );
	if ( !damageDef ) {
		gameLocal.Warning( "Unknown damageDef '%s'", damageDefName );
		return;
	}

	if ( damageDef->dict.GetBool( "ignore_player" ) ) {
		return;
	}

	CalcDamagePoints( inflictor, attacker, &damageDef->dict, damageScale, location, &damage, &armorSave );

	// determine knockback
	damageDef->dict.GetInt( "knockback", "20", knockback );

	if ( knockback != 0 && !fl.noknockback ) {
		if ( attacker == this ) {
			damageDef->dict.GetFloat( "attackerPushScale", "0", attackerPushScale );
		} else {
			attackerPushScale = 1.0f;
		}

		kick = dir;
		kick.Normalize();
		kick *= g_knockback.GetFloat() * knockback * attackerPushScale / 200.0f;
		physicsObj.SetLinearVelocity( physicsObj.GetLinearVelocity() + kick );

		// set the timer so that the player can't cancel out the movement immediately
		physicsObj.SetKnockBack( idMath::ClampInt( 50, 200, knockback * 2 ) );
	}

	if ( damageDef->dict.GetBool( "burn" ) ) {
		StartSound( "snd_burn", SND_CHANNEL_BODY3, SSF_PRIVATE_SOUND, false, NULL );
	}

	if ( g_debugDamage.GetInteger() ) {
		gameLocal.Printf( "client:%i health:%i damage:%i armor:%i\n", entityNumber, health, damage, armorSave );
	}

	// move the world direction vector to local coordinates
	damage_from = dir;
	damage_from.Normalize();
	
	viewAxis.ProjectVector( damage_from, localDamageVector );

	// add to the damage inflicted on a player this frame
	// the total will be turned into screen blends and view angle kicks
	// at the end of the frame
	if ( health > 0 ) {
		playerView.DamageImpulse( localDamageVector, &damageDef->dict );
	}

	// do the damage
	if ( damage > 0 ) {
		float scale = g_damageScale.GetFloat();
		if ( scale > 0.0f ) {
			damage *= scale;
		}
		if ( damage < 1 ) {
			damage = 1;
		}

		health -= damage;

		if ( health <= 0 ) {
			if ( health < -999 ) {
				health = -999;
			}

			lastDmgTime = gameLocal.time;
			Killed( inflictor, attacker, damage, dir, location );

		} else {
			// force a blink
			blink_time = 0;

			// let the anim script know we took damage
			AI_PAIN = Pain( inflictor, attacker, damage, dir, location );
			if ( !g_testDeath.GetBool() ) {
				lastDmgTime = gameLocal.time;
			}
		}
	} else {
		// don't accumulate impulses
		if ( af.IsLoaded() ) {
			// clear impacts
			af.Rest();

			// physics is turned off by calling af.Rest()
			BecomeActive( TH_PHYSICS );
		}
	}

	lastDamageDef = damageDef->Index();
	lastDamageDir = damage_from;
	lastDamageLocation = location;
}

/*
===========
idPlayer::Teleport
============
*/
void idPlayer::Teleport( const idVec3 &origin, const idAngles &angles, idEntity *destination ) {
	idVec3 org;

	SetOrigin( origin + idVec3( 0, 0, CM_CLIP_EPSILON ) );
	if ( GetFloorPos( 16.0f, org ) ) {
		SetOrigin( org );
	}

	// clear the ik heights so model doesn't appear in the wrong place
	walkIK.EnableAll();

	GetPhysics()->SetLinearVelocity( vec3_origin );

	SetViewAngles( angles );

	legsYaw = 0.0f;
	idealLegsYaw = 0.0f;
	oldViewYaw = viewAngles.yaw;

	playerView.Flash( colorWhite, 140 );

	UpdateVisuals();

	teleportEntity = destination;

	if ( !noclip ) {
		// kill anything at the new position
		gameLocal.KillBox( this, true );
	}
}

/*
====================
idPlayer::SetPrivateCameraView
====================
*/
void idPlayer::SetPrivateCameraView( idCamera *camView ) {
	privateCameraView = camView;
	if ( camView ) {
		Hide();
	} else {
		Show();
	}
}

/*
====================
idPlayer::DefaultFov

Returns the base FOV
====================
*/
float idPlayer::DefaultFov( void ) const {
	float fov = g_fov.GetFloat();
	return fov;
}

/*
====================
idPlayer::CalcFov

Fixed fov at intermissions, otherwise account for fov variable and zooms.
====================
*/
float idPlayer::CalcFov( bool honorZoom ) {
	float fov;

	if ( fxFov ) {
		return DefaultFov() + 10.0f + cos( ( gameLocal.time + 2000 ) * 0.01 ) * 10.0f;
	}

	if ( influenceFov ) {
		return influenceFov;
	}

	fov = DefaultFov();

	// bound normal viewsize
	if ( fov < 1 ) {
		fov = 1;
	} else if ( fov > 179 ) {
		fov = 179;
	}

	return fov;
}

/*
===============
idPlayer::OffsetThirdPersonView
===============
*/
void idPlayer::OffsetThirdPersonView( float angle, float range, float height, bool clip ) {
	idVec3			view;
	idVec3			focusAngles;
	trace_t			trace;
	idVec3			focusPoint;
	float			focusDist;
	float			forwardScale, sideScale;
	idVec3			origin;
	idAngles		angles;
	idMat3			axis;
	idBounds		bounds;

	angles = viewAngles;
	GetViewPos( origin, axis );

//	sikk - commented out to allow camera to follow pitch when "pm_thirdPersonAngle" != 0
//	if ( angle ) {
//		angles.pitch = 0.0f;
//	}

	if ( angles.pitch > 45.0f ) {
		angles.pitch = 45.0f;		// don't go too far overhead
	}

	focusPoint = origin + angles.ToForward() * THIRD_PERSON_FOCUS_DISTANCE;
	focusPoint.z += height;
	view = origin;
	view.z += 8 + height;

	angles.pitch *= 0.5f;
	renderView->viewaxis = angles.ToMat3() * physicsObj.GetGravityAxis();

	idMath::SinCos( DEG2RAD( angle ), sideScale, forwardScale );
	view -= range * forwardScale * renderView->viewaxis[ 0 ];

// ---> sikk - Thirdpesron Camera - adjustable horizontal position
//	view += range * sideScale * renderView->viewaxis[ 1 ];
	view += range * ( sideScale - ( pm_thirdPersonOffset.GetFloat() * 0.01 ) ) * renderView->viewaxis[ 1 ];
// <--- sikk - Thirdpesron Camera

	if ( clip ) {
		// trace a ray from the origin to the viewpoint to make sure the view isn't
		// in a solid block.  Use an 8 by 8 block to prevent the view from near clipping anything
		bounds = idBounds( idVec3( -4, -4, -4 ), idVec3( 4, 4, 4 ) );
		gameLocal.clip.TraceBounds( trace, origin, view, bounds, MASK_SOLID, this );
		if ( trace.fraction != 1.0f ) {
			view = trace.endpos;
			view.z += ( 1.0f - trace.fraction ) * 32.0f;

			// try another trace to this position, because a tunnel may have the ceiling
			// close enough that this is poking out
			gameLocal.clip.TraceBounds( trace, origin, view, bounds, MASK_SOLID, this );
			view = trace.endpos;
		}
	}

	// select pitch to look at focus point from vieword
	focusPoint -= view;
	focusDist = idMath::Sqrt( focusPoint[0] * focusPoint[0] + focusPoint[1] * focusPoint[1] );
	if ( focusDist < 1.0f ) {
		focusDist = 1.0f;	// should never happen
	}

	angles.pitch = - RAD2DEG( atan2( focusPoint.z, focusDist ) );
	angles.yaw -= angle;

	renderView->vieworg = view;
	renderView->viewaxis = angles.ToMat3() * physicsObj.GetGravityAxis();
	renderView->viewID = 0;
}

/*
===============
idPlayer::GetEyePosition
===============
*/
idVec3 idPlayer::GetEyePosition( void ) const {
	idVec3 org;
 
	// use the smoothed origin if spectating another player in multiplayer
	org = GetPhysics()->GetOrigin();
	return org + ( GetPhysics()->GetGravityNormal() * -eyeOffset.z );
}

/*
===============
idPlayer::GetViewPos
===============
*/
void idPlayer::GetViewPos( idVec3 &origin, idMat3 &axis ) const {
	idAngles angles;

	// if dead, fix the angle and don't add any kick
	if ( health <= 0 ) {
		angles.yaw = viewAngles.yaw;
		angles.roll = 40;
		angles.pitch = -15;
		axis = angles.ToMat3();
		origin = GetEyePosition();
	} else {
		origin = GetEyePosition() + viewBob;
		angles = viewAngles + viewBobAngles + playerView.AngleOffset();
		axis = angles.ToMat3() * physicsObj.GetGravityAxis();

		// adjust the origin based on the camera nodal distance (eye distance from neck)
		origin += physicsObj.GetGravityNormal() * g_viewNodalZ.GetFloat();
		origin += axis[0] * g_viewNodalX.GetFloat() + axis[2] * g_viewNodalZ.GetFloat();
	}
}

/*
===============
idPlayer::CalculateFirstPersonView
===============
*/
void idPlayer::CalculateFirstPersonView( void ) {
	if ( ( pm_modelView.GetInteger() == 1 ) || ( ( pm_modelView.GetInteger() == 2 ) && ( health <= 0 ) ) ) {
		//	Displays the view from the point of view of the "camera" joint in the player model
		idMat3 axis;
		idVec3 origin;
		idAngles ang;

		ang = viewBobAngles + playerView.AngleOffset();
		ang.yaw += viewAxis[ 0 ].ToYaw();
		
		jointHandle_t joint = animator.GetJointHandle( "camera" );	// sikk - Modified - "pm_modelView" Fix
//		jointHandle_t joint = head.GetEntity()->GetAnimator()->GetJointHandle( "mouth12" );	// sikk - Added -"pm_modelView" Fix
		GetViewPos( firstPersonViewOrigin, firstPersonViewAxis );	// sikk - Added -"pm_modelView" Fix
		animator.GetJointTransform( joint, gameLocal.time, origin, axis );	// sikk - Commented - "pm_modelView" Fix
		firstPersonViewOrigin = ( origin ) * ( viewAxis * physicsObj.GetGravityAxis() ) + physicsObj.GetOrigin() + viewBob;
//		firstPersonViewAxis = axis * ang.ToMat3() * physicsObj.GetGravityAxis();	// sikk - Commented - "pm_modelView" Fix
	} else {
		// offset for local bobbing and kicks
		GetViewPos( firstPersonViewOrigin, firstPersonViewAxis );
#if 0
		// shakefrom sound stuff only happens in first person
		firstPersonViewAxis = firstPersonViewAxis * playerView.ShakeAxis();
#endif
	}
}

/*
==================
idPlayer::GetRenderView

Returns the renderView that was calculated for this tic
==================
*/
renderView_t *idPlayer::GetRenderView( void ) {
	return renderView;
}

/*
==================
idPlayer::CalculateRenderView

create the renderView for the current tic
==================
*/
void idPlayer::CalculateRenderView( void ) {
	int i;
	float range;

	if ( !renderView ) {
		renderView = new renderView_t;
	}
	memset( renderView, 0, sizeof( *renderView ) );

	// copy global shader parms
	for( i = 0; i < MAX_GLOBAL_SHADER_PARMS; i++ ) {
		renderView->shaderParms[ i ] = gameLocal.globalShaderParms[ i ];
	}
	renderView->globalMaterial = gameLocal.GetGlobalMaterial();
	renderView->time = gameLocal.time;

	// calculate size of 3D view
	renderView->x = 0;
	renderView->y = 0;
	renderView->width = SCREEN_WIDTH;
	renderView->height = SCREEN_HEIGHT;
	renderView->viewID = 0;

	// check if we should be drawing from a camera's POV
	if ( !noclip && (gameLocal.GetCamera() || privateCameraView) ) {
		// get origin, axis, and fov
		if ( privateCameraView ) {
			privateCameraView->GetViewParms( renderView );
		} else {
			gameLocal.GetCamera()->GetViewParms( renderView );
		}
	} else {
		if ( g_stopTime.GetBool() ) {
			renderView->vieworg = firstPersonViewOrigin;
			renderView->viewaxis = firstPersonViewAxis;

			if ( !pm_thirdPerson.GetBool() ) {
				// set the viewID to the clientNum + 1, so we can suppress the right player bodies and
				// allow the right player view weapons
				renderView->viewID = entityNumber + 1;
			}
		} else if ( pm_thirdPerson.GetBool() ) {
			OffsetThirdPersonView( pm_thirdPersonAngle.GetFloat(), pm_thirdPersonRange.GetFloat(), pm_thirdPersonHeight.GetFloat(), pm_thirdPersonClip.GetBool() );
		} else if ( pm_thirdPersonDeath.GetBool() ) {
			range = gameLocal.time < minRespawnTime ? ( gameLocal.time + RAGDOLL_DEATH_TIME - minRespawnTime ) * ( 120.0f / RAGDOLL_DEATH_TIME ) : 120.0f;
			OffsetThirdPersonView( 0.0f, 20.0f + range, 0.0f, false );
		} else {
			renderView->vieworg = firstPersonViewOrigin;
			renderView->viewaxis = firstPersonViewAxis;

			// set the viewID to the clientNum + 1, so we can suppress the right player bodies and
			// allow the right player view weapons
			if ( pm_modelView.GetInteger() == 0 ) {
				renderView->viewID = entityNumber + 1;
				renderEntity.suppressSurfaceInViewID = entityNumber + 1;
			}
		}
		
		// field of view
		gameLocal.CalcFov( CalcFov( true ), renderView->fov_x, renderView->fov_y );
	}

	if ( renderView->fov_y == 0 ) {
		common->Error( "renderView->fov_y == 0" );
	}

	if ( g_showviewpos.GetBool() ) {
		gameLocal.Printf( "%s : %s\n", renderView->vieworg.ToString(), renderView->viewaxis.ToAngles().ToString() );
	}
}

/*
=============
idPlayer::SetLastHitTime
=============
*/
void idPlayer::SetLastHitTime( int time ) {
	if ( time && lastHitTime != time ) {
		lastHitToggle ^= 1;
	}
	lastHitTime = time;
	if ( !time ) {
		// level start and inits
		return;
	}
	if ( ( time - lastSndHitTime ) > 10 ) {
		lastSndHitTime = time;
	}
	if ( cursor ) {
		cursor->HandleNamedEvent( "hitTime" );
	}
}

/*
=============
idPlayer::SetInfluenceLevel
=============
*/
void idPlayer::SetInfluenceLevel( int level ) {
	if ( level != influenceActive ) {
		if ( level ) {
			for ( idEntity *ent = gameLocal.spawnedEntities.Next(); ent != NULL; ent = ent->spawnNode.Next() ) {
				if ( ent->IsType( idProjectile::Type ) ) {
					// remove all projectiles
					ent->PostEventMS( &EV_Remove, 0 );
				}
			}
		} else {
			physicsObj.SetLinearVelocity( vec3_origin );
		}
		influenceActive = level;
	}
}

/*
=============
idPlayer::SetInfluenceView
=============
*/
void idPlayer::SetInfluenceView( const char *mtr, const char *skinname, float radius, idEntity *ent ) {
	influenceMaterial = NULL;
	influenceEntity = NULL;
	influenceSkin = NULL;
	if ( mtr && *mtr ) {
		influenceMaterial = declManager->FindMaterial( mtr );
	}
	if ( skinname && *skinname ) {
		influenceSkin = declManager->FindSkin( skinname );
		if ( head.GetEntity() ) {
			head.GetEntity()->GetRenderEntity()->shaderParms[ SHADERPARM_TIMEOFFSET ] = -MS2SEC( gameLocal.time );
		}
		UpdateVisuals();
	}
	influenceRadius = radius;
	if ( radius > 0.0f ) {
		influenceEntity = ent;
	}
}

/*
=============
idPlayer::SetInfluenceFov
=============
*/
void idPlayer::SetInfluenceFov( float fov ) {
	influenceFov = fov;
}

/*
================
idPlayer::OnLadder
================
*/
bool idPlayer::OnLadder( void ) const {
	return physicsObj.OnLadder();
}

/*
==================
idPlayer::Event_GetButtons
==================
*/
void idPlayer::Event_GetButtons( void ) {
	idThread::ReturnInt( usercmd.buttons );
}

/*
==================
idPlayer::Event_GetMove
==================
*/
void idPlayer::Event_GetMove( void ) {
	idVec3 move( usercmd.forwardmove, usercmd.rightmove, usercmd.upmove );
	idThread::ReturnVector( move );
}

/*
================
idPlayer::Event_GetViewAngles
================
*/
void idPlayer::Event_GetViewAngles( void ) {
	idThread::ReturnVector( idVec3( viewAngles[0], viewAngles[1], viewAngles[2] ) );
}

/*
==================
idPlayer::Event_StopFxFov
==================
*/
void idPlayer::Event_StopFxFov( void ) {
	fxFov = false;
}

/*
==================
idPlayer::StartFxFov 
==================
*/
void idPlayer::StartFxFov( float duration ) { 
	fxFov = true;
	PostEventSec( &EV_Player_StopFxFov, duration );
}

/*
==================
idPlayer::TeleportDeath
==================
*/
void idPlayer::TeleportDeath( int killer ) {
	teleportKiller = killer;
}

/*
==================
idPlayer::Event_ExitTeleporter
==================
*/
void idPlayer::Event_ExitTeleporter( void ) {
	idEntity	*exitEnt;
	float		pushVel;

	// verify and setup
	exitEnt = teleportEntity.GetEntity();
	if ( !exitEnt ) {
		common->DPrintf( "Event_ExitTeleporter player %d while not being teleported\n", entityNumber );
		return;
	}

	pushVel = exitEnt->spawnArgs.GetFloat( "push", "300" );

	SetPrivateCameraView( NULL );
	// setup origin and push according to the exit target
	SetOrigin( exitEnt->GetPhysics()->GetOrigin() + idVec3( 0, 0, CM_CLIP_EPSILON ) );
	SetViewAngles( exitEnt->GetPhysics()->GetAxis().ToAngles() );
	physicsObj.SetLinearVelocity( exitEnt->GetPhysics()->GetAxis()[ 0 ] * pushVel );
	physicsObj.ClearPushedVelocity();
	// teleport fx
	playerView.Flash( colorWhite, 120 );

	// clear the ik heights so model doesn't appear in the wrong place
	walkIK.EnableAll();

	UpdateVisuals();

	StartSound( "snd_teleport_exit", SND_CHANNEL_ANY, SSF_PRIVATE_SOUND, false, NULL );

	if ( teleportKiller != -1 ) {
		// we got killed while being teleported
		Damage( gameLocal.entities[ teleportKiller ], gameLocal.entities[ teleportKiller ], vec3_origin, "damage_telefrag", 1.0f, INVALID_JOINT );
		teleportKiller = -1;
	} else {
		// kill anything that would have waited at teleport exit
		gameLocal.KillBox( this );
	}
	teleportEntity = NULL;
}

/*
================
idPlayer::GetPhysicsToVisualTransform
================
*/
bool idPlayer::GetPhysicsToVisualTransform( idVec3 &origin, idMat3 &axis ) {
	if ( af.IsActive() ) {
		af.GetPhysicsToVisualTransform( origin, axis );
		return true;
	}

	axis = viewAxis;
	origin = modelOffset;

	return true;
}

/*
================
idPlayer::GetPhysicsToSoundTransform
================
*/
bool idPlayer::GetPhysicsToSoundTransform( idVec3 &origin, idMat3 &axis ) {
	idCamera *camera;

	if ( privateCameraView ) {
		camera = privateCameraView;
	} else {
		camera = gameLocal.GetCamera();
	}

	if ( camera ) {
		renderView_t view;

		memset( &view, 0, sizeof( view ) );
		camera->GetViewParms( &view );
		origin = view.vieworg;
		axis = view.viewaxis;
		return true;
	} else {
		return idActor::GetPhysicsToSoundTransform( origin, axis );
	}
}

/*
================
idPlayer::Hide
================
*/
void idPlayer::Hide( void ) {
	idActor::Hide();
}

/*
================
idPlayer::Show
================
*/
void idPlayer::Show( void ) {
	idActor::Show();
}

/*
===============
idPlayer::SetLevelTrigger
===============
*/
void idPlayer::SetLevelTrigger( const char *levelName, const char *triggerName ) {
	if ( levelName && *levelName && triggerName && *triggerName ) {
		idLevelTriggerInfo lti;
		lti.levelName = levelName;
		lti.triggerName = triggerName;
		inventory.levelTriggers.Append( lti );
	}
}

/*
===============
idPlayer::Event_LevelTrigger
===============
*/
void idPlayer::Event_LevelTrigger( void ) {
	idStr mapName = gameLocal.GetMapName();
	mapName.StripPath();
	mapName.StripFileExtension();
	for ( int i = inventory.levelTriggers.Num() - 1; i >= 0; i-- ) {
		if ( idStr::Icmp( mapName, inventory.levelTriggers[i].levelName) == 0 ){
			idEntity *ent = gameLocal.FindEntity( inventory.levelTriggers[i].triggerName );
			if ( ent ) {
				ent->PostEventMS( &EV_Activate, 1, this );
			}
		}
	}
}

/*
===============
idPlayer::Event_Gibbed
===============
*/
void idPlayer::Event_Gibbed( void ) {
}

// ---> sikk - Level Music
/*
===============
idPlayer::SpawnMusic
==============
*/
void idPlayer::SpawnMusic( void ) {
	idEntity* ent;
	idDict args;

	ent = gameLocal.FindEntity( "sound_ambient_1" );
	if ( ent == NULL ) {
		sndAmbientLooping1 = declManager->FindSound( "sound_ambient_1" );
		args.Set( "classname", "speaker" );
		args.Set( "name", "sound_ambient_1" );
		//args.Set( "s_shader", "sound_ambient_1" );
		args.SetVector( "origin", GetPhysics()->GetOrigin() );
		gameLocal.SpawnEntityDef( args, &ent );
		gameLocal.program.ReturnEntity( ent );
		args.Clear();
	}
	ent = gameLocal.FindEntity( "sound_ambient_2" );
	if ( ent == NULL ) {
		sndAmbientLooping2 = declManager->FindSound( "sound_ambient_2" );
		args.Set( "classname", "speaker" );
		args.Set( "name", "sound_ambient_2" );
		//args.Set( "s_shader", "sound_ambient_2" );
		args.SetVector( "origin", GetPhysics()->GetOrigin() );
		gameLocal.SpawnEntityDef( args, &ent );
		gameLocal.program.ReturnEntity( ent );
		args.Clear();
	}
	ent = gameLocal.FindEntity( "sound_ambient_1shot" );
	if ( ent == NULL ) {
		sndAmbient1shot = declManager->FindSound( "sound_ambient_1shot" );
		args.Set( "classname", "speaker" );
		args.Set( "name", "sound_ambient_1shot" );
		//args.Set( "s_shader", "sound_ambient_1shot" );
		args.SetVector( "origin", GetPhysics()->GetOrigin() );
		gameLocal.SpawnEntityDef( args, &ent );
		gameLocal.program.ReturnEntity( ent );
		args.Clear();
	}
	
	StartSound( "snd_threat_fov", SND_CHANNEL_VOICE2, 0, false, NULL );
	soundShaderParms_t	parms;
	memset( &parms, 0, sizeof( parms ) );
	parms.volume = -60;
	refSound.referenceSound->ModifySound( SND_CHANNEL_VOICE2, &parms );
}

/*
===============
idPlayer::UpdateMusic
==============
*/
void idPlayer::UpdateMusic( void ) {
	idEntity *ent1, *ent2, *ent3;
	//float volumeMain = s_volumeMusic.GetFloat();
//	const char* sound_ambient;

	ent1 = gameLocal.FindEntity( "sound_ambient_1" );
	ent2 = gameLocal.FindEntity( "sound_ambient_2" );
	ent3 = gameLocal.FindEntity( "sound_ambient_1shot" );

	// manage looping ambient sounds
	if ( ent1 && ent2 ){
		if ( musicFadeTime < gameLocal.time ) {
			int len = 0;
			musicFadeTime = gameLocal.random.RandomFloat() * 4.0f + 4.0f;
			if ( musicFlag == 0 ) { // fade out 2, fade in 1
				ent2->FadeSound( SND_CHANNEL_ANY, -40, musicFadeTime );
				ent2->PostEventSec( &EV_StopSound, musicFadeTime, SND_CHANNEL_ANY, 0 );

//				SetSoundVolume( -40 );
				len = ent1->StartSoundShaderLength( sndAmbientLooping1, SND_CHANNEL_ANY, 0, false, NULL );
				ent1->PostEventMS( &EV_FadeSound, 20, SND_CHANNEL_ANY, -40.0f, 0.0f );
				ent1->PostEventMS( &EV_FadeSound, 100, SND_CHANNEL_ANY, 0.0f, musicFadeTime * 0.5f );
				musicFlag = 1;
//				gameLocal.Printf( "playing ambient 1...\n" );
			} else { // fade out 1, fade in 2
				ent1->FadeSound( SND_CHANNEL_ANY, -40, musicFadeTime );
				ent1->PostEventSec( &EV_StopSound, musicFadeTime, SND_CHANNEL_ANY, 0 );

//				ent2->SetSoundVolume( -40 );
				len = ent2->StartSoundShaderLength( sndAmbientLooping2, SND_CHANNEL_ANY, 0, false, NULL );
				ent2->PostEventMS( &EV_FadeSound, 20, SND_CHANNEL_ANY, -40.0f, 0.0f );
				ent2->PostEventMS( &EV_FadeSound, 100, SND_CHANNEL_ANY, 0.0f, musicFadeTime * 0.5f );
				musicFlag = 0;
//				gameLocal.Printf( "playing ambient 2...\n" );
			}
			musicFadeTime = SEC2MS( gameLocal.random.RandomFloat() * ( MS2SEC( len ) - g_musicTimeAmbientMin.GetFloat() - musicFadeTime ) + g_musicTimeAmbientMin.GetFloat() );
			musicFadeTime += gameLocal.time;
		} 
	}
	
	// manage 1-shot ambient sounds
	if ( music1ShotTime <= 0 ) {
		music1ShotTime = gameLocal.random.RandomFloat() * g_musicTime1ShotMax.GetFloat() + g_musicTime1ShotMin.GetFloat();
		music1ShotTime = SEC2MS( music1ShotTime ) + gameLocal.time;
	} else if ( ent3 && music1ShotTime < gameLocal.time ) {
		idAAS *pAAS48 = gameLocal.GetAAS( "aas48" );
		if ( pAAS48 ) {
			float randFloat = gameLocal.random.RandomFloat();
			int areaNum = idMath::FtoiFast( randFloat * pAAS48->GetNumAreas( 0 ) );
			idVec3 org = pAAS48->AreaCenter( areaNum ) + idVec3( 0.0f, 0.0f, 64.0f );
			ent3->SetOrigin( org );
		}

		ent3->StartSoundShader( sndAmbient1shot, SND_CHANNEL_ANY, 0, false, NULL );
		music1ShotTime = gameLocal.random.RandomFloat() * ( g_musicTime1ShotMax.GetFloat() - g_musicTime1ShotMin.GetFloat() ) + g_musicTime1ShotMin.GetFloat();
		music1ShotTime = SEC2MS( music1ShotTime ) + gameLocal.time;
//		gameLocal.Printf( "playing ambient 1 shot...\n" );
	}
	//if ( ent1 && ent1->GetSoundEmitter() ) {
	//	hud->SetStateFloat( "ambSnd1", ent1->GetSoundEmitter()->CurrentAmplitude() );
	//}
	//if ( ent2 && ent2->GetSoundEmitter() ) {
	//	hud->SetStateFloat( "ambSnd2", ent2->GetSoundEmitter()->CurrentAmplitude() );
	//}
	if ( ent3 && ent3->GetSoundEmitter() ) {
		//if ( ent3->GetSoundEmitter()->CurrentAmplitude() > threatLevel.fovLerp.GetCurrentValue( gameLocal.time ) ) {
		//	threatLevel.fov = ent3->GetSoundEmitter()->CurrentAmplitude();
		//	threatLevel.fovLerp.Init( gameLocal.time, 3000, threatLevel.fov, 0 );
		//}
		if ( ent3->GetSoundEmitter()->CurrentAmplitude() > threatLevel.proximityLerp.GetCurrentValue( gameLocal.time ) ) {
			float scale = 1.0f - idMath::ClampFloat( 0.0f, 1.0f, idVec3( GetPhysics()->GetOrigin() - ent3->GetPhysics()->GetOrigin() ).LengthFast() * 0.00025f );
			scale = ent3->GetSoundEmitter()->CurrentAmplitude() * scale;
			threatLevel.proximityLerp.Init( gameLocal.time, 5000, scale, 0 );
		}
	//	hud->SetStateFloat( "ambSnd3", ent3->GetSoundEmitter()->CurrentAmplitude() );
	}
}
// <--- sikk - Level Music

// ---> sikk - Centerprint Messages
/*
==================
idPlayer::Centerprint
==================
*/
void idPlayer::Centerprint( const char *msg, const char *soundShader, bool toConsole ) {
	// print to the hud
	if ( hud ) {
		hud->SetStateString( "player_centerprintmsg", msg );
		hud->HandleNamedEvent( "centerprintMsg" );
	}

	if ( soundShader[0] != '\0' ) {
		const idSoundShader *shader = declManager->FindSound( soundShader );
		StartSoundShader( shader, SND_CHANNEL_RADIO, SSF_PRIVATE_SOUND, 0, NULL );
	}

	// print to the console
	if ( toConsole ) {
		gameLocal.Printf( "**********************************\n" );
		gameLocal.Printf( msg );
		gameLocal.Printf( "\n**********************************\n" );
	}
}
// <--- sikk - Centerprint Messages

// ---> sikk - Target Post FX
/*
==================
idPlayer::SetTargetPostFX
==================
*/
void idPlayer::SetTargetPostFX( const char *mtr, idVec4 color ) {
	playerView.SetTargetPostFX( mtr, color );
}
// <--- sikk - Target Post FX


// ---> sikk - Player Light
/*
=================
idPlayer::FreeLightDef
=================
*/
void idPlayer::FreeLightDef( void ) {
	if ( lightDefHandle != -1 ) {
		renderLight.lightRadius = vec3_zero;
		gameRenderWorld->FreeLightDef( lightDefHandle );
		lightDefHandle = -1;
	}
}

/*
=================
idPlayer::UpdatePlayerLight
=================
*/
void idPlayer::UpdatePlayerLight( bool doCharge ) {
	float crumbPct = GetCrumbPct();
	// update the brightness
	if ( doCharge ) {
		//if ( gameLocal.time > lightBrightnessTime + 500 ) {
		//	lightBrightness += 1.0f;
			lightBrightness = crumbPct * 7.5f + 0.5f;
			//if ( lightBrightness > max ) {
			//	lightBrightness = max;
			//}
			lightBrightnessTime = gameLocal.time;
		//}
		lightBrightnessLerp.Init( gameLocal.time, 500.0f, lightBrightnessLerp.GetCurrentValue( gameLocal.time ), lightBrightness );
	} else {
		lightBrightness = lightBrightnessLerp.GetCurrentValue( gameLocal.time );
		lightBrightnessLerp.Init( gameLocal.time, 1000.0f * lightBrightness * 0.25f, lightBrightness, 0.0f );
	}

	// update the light
	if ( renderLight.lightRadius.x > 0.0f ) {
		renderLight.origin = firstPersonViewOrigin + firstPersonViewAxis * lightOffset;
		renderLight.axis = firstPersonViewAxis;
		if ( ( lightDefHandle != -1 ) ) {
			renderLight.shaderParms[SHADERPARM_ALPHA] = lightBrightnessLerp.GetCurrentValue( gameLocal.time );
			gameRenderWorld->UpdateLightDef( lightDefHandle, &renderLight );
		} else {
			lightDefHandle = gameRenderWorld->AddLightDef( &renderLight );
		}
	}

	//idEntity *ent;
	//ent = gameLocal.FindEntity( name + "_ambientlight" );
	//if ( !ent ) {
	//	idDict args;
	//	// spawn ambient light
	//	args.Set( "classname", "light" );
	//	args.Set( "name", name + "_ambientlight" );				// light name
	//	args.Set( "texture", "lights/amb_default" );			// light texture
	//	args.Set( "_color", "1 1 1" );							// light color
	//	args.Set( "intensity", "0.25" );						// light intensity
	//	args.Set( "ambient", "1" );								// light type
	//	args.Set( "noShadows", "1" );							// light type
	//	args.Set( "light_radius", "32 32 32" );					// light radius
	//	gameLocal.SpawnEntityDef( args, &ent );
	//	ent->Bind( this, false );								// light bind parent
	//	ent->SetOrigin( idVec3( 0, 0, 64 ) );					// light origin
	//}
}

/*
==================
idPlayer::Event_GetLightBrightness 
==================
*/
void idPlayer::Event_GetLightBrightness( void ) {
	idThread::ReturnFloat( lightBrightnessLerp.GetCurrentValue( gameLocal.time ) * 0.125f );
}
// <--- sikk - Player Light

// ---> sikk - Crumb Count
/*
==================
idPlayer::UpdateCrumbCount 
==================
*/
void idPlayer::UpdateCrumbCount( void ) {
	if ( numCrumbs && numCrumbs >= gameLocal.numCrumbs && !doEndLevel ) {
		idEntity *ent = gameLocal.FindEntity( "target_endlevel" );
		if ( ent ) {
			ent->PostEventMS( &EV_Activate, 1000, this );
		}
		SetInfluenceLevel( 2 );
		endLevelTime = gameLocal.time;
		doEndLevel = true;
		playerView.Fade( colorWhite, 1000 );
		lightBrightness = 8.0f;
		UpdatePlayerLight( true );
	}
}

/*
==================
idPlayer::GetCrumbPct 
==================
*/
float idPlayer::GetCrumbPct( void ) {
	if ( numCrumbs == 0 ) {
		return 0.0f;
	}
	return (float)numCrumbs / gameLocal.numCrumbs;
}
/*
==================
idPlayer::Event_GetCrumbPct 
==================
*/
void idPlayer::Event_GetCrumbPct( void ) {
	idThread::ReturnFloat( GetCrumbPct() );
}
// <--- sikk - Crumb Count

// ---> sikk - Monster Count
/*
=================
idPlayer::UpdateMonsterSpawns
=================
*/
void idPlayer::UpdateMonsterSpawns( void ) {
	float crumbPct;
	idVec3 spawn_origin;
	idAngles spawn_angles;
	idEntity *spot, *ent;
	idDict args;
	bool doSpawn = false;
	crumbPct = GetCrumbPct();

	if ( gameLocal.monsters.Num() >= 4 || crumbPct < 0.2f ) {
		return;
	}

	if ( gameLocal.monsters.Num() <= 0 && crumbPct >= 0.2f ) {
		doSpawn = true;
	} else if ( gameLocal.monsters.Num() == 1 && crumbPct >= 0.4f ) {
		doSpawn = true;
	} else if ( gameLocal.monsters.Num() == 2 && crumbPct >= 0.6f ) {
		doSpawn = true;
	} else if ( gameLocal.monsters.Num() == 3 && crumbPct >= 0.8f ) {
		doSpawn = true;
	}

	if ( doSpawn ) {
		spot = gameLocal.SelectInitialSpawnPoint( NULL );

		spawn_origin = spot->GetPhysics()->GetOrigin();
		spawn_origin[2] += 4.0f + CM_BOX_EPSILON;		// move up to make sure the player is at least an epsilon above the floor
		spawn_angles = spot->GetPhysics()->GetAxis().ToAngles();

		args.Set( "classname", "monster_it" );
		args.SetVector( "origin", spawn_origin );
		args.SetFloat( "angle", spawn_angles.yaw );
		gameLocal.SpawnEntityDef( args, &ent );
		gameLocal.monsters.Append( static_cast<idAI *>( ent ) );
		gameLocal.program.ReturnEntity( ent );
		args.Clear();
	}
}
// <--- sikk - Monster Count

// ---> sikk - Player Illumination Test
/*
==================
idPlayer::GetIllumination  
==================
*/
float idPlayer::GetIllumination( void ) {
	idEntity *	ent;
	idEntity *	entityList[ MAX_GENTITIES ];
	int			numListedEntities;
	int			i;
	float		lum;
	idVec3		org = GetPhysics()->GetOrigin() + idVec3( 0, 0, 48 );
	idBounds bounds = idBounds( idVec3( -128, -128, 0 ), idVec3( 128, 128, 128 ) );
	bounds += GetPhysics()->GetOrigin();

	numListedEntities = gameLocal.clip.EntitiesTouchingBounds( bounds, -1, entityList, MAX_GENTITIES );
	for ( i = 0, lum = 0.0f; i < numListedEntities && lum < 1.0f; i++ ) {
		ent = entityList[ i ];
		if ( ent->IsType( idItem::Type ) ) {
			float dist = idVec3( org - ent->GetRenderEntity()->origin ).Length();
			dist = idMath::ClampFloat( 0.0f, 1.0f, 1.0f - dist * 0.0078125f );
			lum += dist;
		}
	}
	return lum;
}

/*
==================
idPlayer::Event_GetIllumination 
==================
*/
void idPlayer::Event_GetIllumination( void ) {
	idThread::ReturnFloat( GetIllumination () );
}
// <--- sikk - Player Illumination Test

// ---> sikk - Threat Level
/*
=================
idPlayer::UpdateThreatLevel
=================
*/
void idPlayer::UpdateThreatLevel( void ) {
	if ( gameLocal.monsters.Num() <= 0 ) {
		return;
	}

	idEntity *	ent;
	int			i;
	float		threatProx, threatFOV, threatFeedback;
	idVec3		org = GetPhysics()->GetOrigin();
	
	threatProx = threatFOV = threatFeedback = 0.0f;

	for ( i = 0; i < gameLocal.monsters.Num(); i++ ) {
		ent = gameLocal.monsters[ i ];
		idVec3 entOrg = ent->GetRenderEntity()->origin;
		float dist = idVec3( org - entOrg ).Length();
		float prox = idMath::ClampFloat( 0.0f, 1.0f, 1.0f - ( dist - 64 ) * 0.002232142857f ) ;
		threatProx = max( threatProx, prox );
		threatFOV = max( threatFOV, prox );
			
		if ( CanSee( ent, true ) ) {
			threatFeedback = idMath::ClampFloat( 0.0f, 1.0f, 1.0f - dist * 0.00048828125f );
			threatFOV = max( threatFOV, threatFeedback );
		}
	}

	if ( threatFOV > threatLevel.fovLerp.GetCurrentValue( gameLocal.time ) ) {
		threatLevel.fovLerp.Init( gameLocal.time, 5000, threatFOV, 0 );
	}
	if ( threatProx > threatLevel.proximityLerp.GetCurrentValue( gameLocal.time ) ) {
		threatLevel.proximityLerp.Init( gameLocal.time, 10000, threatProx, 0 );
	}
	if ( threatFeedback > threatLevel.feedbackLerp.GetCurrentValue( gameLocal.time ) ) {
		threatLevel.feedbackLerp.Init( gameLocal.time, 3000, threatFeedback, 0 );
	}
	soundShaderParms_t	parms;
	memset( &parms, 0, sizeof( parms ) );
	parms.volume = threatLevel.feedbackLerp.GetCurrentValue( gameLocal.time ) * 60 - 60;
	refSound.referenceSound->ModifySound( SND_CHANNEL_VOICE2, &parms );
}
// <--- sikk - Threat Level