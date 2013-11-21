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

#ifndef __GAME_PLAYER_H__
#define __GAME_PLAYER_H__

/*
===============================================================================

	Player entity.
	
===============================================================================
*/

extern const idEventDef EV_Player_GetButtons;
extern const idEventDef EV_Player_GetMove;
extern const idEventDef EV_Player_GetViewAngles;
extern const idEventDef EV_Player_ExitTeleporter;
extern const idEventDef EV_SpectatorTouch;

extern const idEventDef EV_Player_GetLightBrightness;	// sikk - Player Light
extern const idEventDef EV_Player_GetCrumbPct;			// sikk - Crumb count
extern const idEventDef EV_Player_GetIllumination;		// sikk - Player Illumination Test

const float THIRD_PERSON_FOCUS_DISTANCE	= 512.0f;
const int	LAND_DEFLECT_TIME = 150;
const int	LAND_RETURN_TIME = 300;
const int	FOCUS_TIME = 300;
const int	FOCUS_GUI_TIME = 500;

const int DEAD_HEARTRATE = 0;			// fall to as you die
const int LOWHEALTH_HEARTRATE_ADJ = 20; // 
const int DYING_HEARTRATE = 30;			// used for volumen calc when dying/dead
const int BASE_HEARTRATE = 70;			// default
const int ZEROSTAMINA_HEARTRATE = 100;  // no stamina
const int MAX_HEARTRATE = 120;			// maximum
const int ZERO_VOLUME = -20;			// volume at zero
const int DMG_VOLUME = 0;				// volume when taking damage
const int DEATH_VOLUME = 5;				// volume at death

const int ASYNC_PLAYER_INV_AMMO_BITS = idMath::BitsForInteger( 999 );	// 9 bits to cover the range [0, 999]
const int ASYNC_PLAYER_INV_CLIP_BITS = -7;								// -7 bits to cover the range [-1, 60]

struct idItemInfo {
	idStr name;
	idStr icon;
};

struct idLevelTriggerInfo {
	idStr levelName;
	idStr triggerName;
};

// influence levels
enum influenceLevels_e {
	INFLUENCE_NONE = 0,			// none
	INFLUENCE_LEVEL1,			// no gun or hud
	INFLUENCE_LEVEL2,			// no gun, hud, movement
	INFLUENCE_LEVEL3,			// slow player movement
};

class idInventory {
public:
	int						maxHealth;

	idList<idDict *>		items;

	int						lastGiveTime;

	idList<idLevelTriggerInfo> levelTriggers;

							idInventory() { Clear(); }
							~idInventory() { Clear(); }

	// save games
	void					Save( idSaveGame *savefile ) const;					// archives object for save game file
	void					Restore( idRestoreGame *savefile );					// unarchives object from save game file

	void					Clear( void );
	void					GetPersistantData( idDict &dict );
	void					RestoreInventory( idPlayer *owner, const idDict &dict );
	bool					Give( idPlayer *owner, const idDict &spawnArgs, const char *statname, const char *value, int *idealWeapon, bool updateHud );
	void					AddPickupName( const char *name, const char *icon );

	int						nextItemPickup;
	int						nextItemNum;
	int						onePickupTime;
	idList<idItemInfo>		pickupItemNames;
};

typedef struct loggedAccel_s {
	int		time;
	idVec3	dir;		// scaled larger for running
} loggedAccel_t;

typedef struct aasLocation_s {
	int		areaNum;
	idVec3	pos;
} aasLocation_t;

typedef struct threatLevel_s{
	idInterpolate<float>	proximityLerp;
	idInterpolate<float>	fovLerp;
	idInterpolate<float>	feedbackLerp;
	idInterpolate<float>	heartbeatLerp;
} threatLevel_t;

// ---> sikk - Level Stats System
enum levelStats_e {
	TIME = 0, 
	SCORE,
	SCORETOTAL,
	LEVELERSFOUND,
	LEVELERSTOTAL,
	LIVES,
	NUM_STATS
};

typedef struct levelStats_s {
	int	time;
	int	score;	// this is per level score, total score is kept in idInventory so it maintains across levels
	int	scoreTotal;
	int	levelersFound;
	int	levelersTotal;
	int	lives;
} levelStats_t;
// <--- sikk - Level Stats System

class idPlayer : public idActor {
public:
	enum {
		EVENT_IMPULSE = idEntity::EVENT_MAXEVENTS,
		EVENT_EXIT_TELEPORTER,
		EVENT_ABORT_TELEPORTER,
		EVENT_POWERUP,
		EVENT_SPECTATE,
		EVENT_MAXEVENTS
	};

	usercmd_t				usercmd;

	class idPlayerView		playerView;			// handles damage kicks and effects

	bool					noclip;
	bool					godmode;

	bool					spawnAnglesSet;		// on first usercmd, we must set deltaAngles
	idAngles				spawnAngles;
	idAngles				viewAngles;			// player view angles
	idAngles				cmdAngles;			// player cmd angles

	int						buttonMask;
	int						oldButtons;
	int						oldFlags;

	int						lastHitTime;			// last time projectile fired by player hit target
	int						lastSndHitTime;			// MP hit sound - != lastHitTime because we throttle
	int						lastSavingThrowTime;	// for the "free miss" effect

	idScriptBool			AI_FORWARD;
	idScriptBool			AI_BACKWARD;
	idScriptBool			AI_STRAFE_LEFT;
	idScriptBool			AI_STRAFE_RIGHT;
	idScriptBool			AI_ATTACK_HELD;
	idScriptBool			AI_WEAPON_FIRED;
	idScriptBool			AI_JUMP;
	idScriptBool			AI_CROUCH;
	idScriptBool			AI_ONGROUND;
	idScriptBool			AI_ONLADDER;
	idScriptBool			AI_DEAD;
	idScriptBool			AI_RUN;
	idScriptBool			AI_PAIN;
	idScriptBool			AI_HARDLANDING;
	idScriptBool			AI_SOFTLANDING;
	idScriptBool			AI_RELOAD;
	idScriptBool			AI_TELEPORT;
	idScriptBool			AI_TURN_LEFT;
	idScriptBool			AI_TURN_RIGHT;

	// inventory
	idInventory				inventory;

	idUserInterface *		hud;				// MP: is NULL if not local player
	idUserInterface *		stats;				// sikk - Level Stats System: GUI

	int						heartRate;
	idInterpolate<float>	heartInfo;
	int						lastHeartAdjust;
	int						lastHeartBeat;
	int						lastDmgTime;
	int						deathClearContentsTime;
	bool					doingDeathSkin;
	float					stamina;

	bool					lastHitToggle;

	idEntityPtr<idEntity>	teleportEntity;		// while being teleported, this is set to the entity we'll use for exit
	int						teleportKiller;		// entity number of an entity killing us at teleporter exit

	// timers
	int						minRespawnTime;		// can respawn when time > this, force after g_forcerespawn
	int						maxRespawnTime;		// force respawn after this time

	// the first person view values are always calculated, even
	// if a third person view is used
	idVec3					firstPersonViewOrigin;
	idMat3					firstPersonViewAxis;

	idDragEntity			dragEntity;

public:
	CLASS_PROTOTYPE( idPlayer );

							idPlayer();
	virtual					~idPlayer();

	void					Spawn( void );
	void					Think( void );

	// save games
	void					Save( idSaveGame *savefile ) const;					// archives object for save game file
	void					Restore( idRestoreGame *savefile );					// unarchives object from save game file

	virtual void			Hide( void );
	virtual void			Show( void );

	void					Init( void );
	void					PrepareForRestart( void );
	virtual void			Restart( void );
	void					LinkScriptVariables( void );
	void					SelectInitialSpawnPoint( idVec3 &origin, idAngles &angles );
	void					SpawnFromSpawnSpot( void );
	void					SpawnToPoint( const idVec3	&spawn_origin, const idAngles &spawn_angles );
	void					SetClipModel( void );	// spectator mode uses a different bbox size

	void					SavePersistantInfo( void );
	void					RestorePersistantInfo( void );
	void					SetLevelTrigger( const char *levelName, const char *triggerName );

	bool					UserInfoChanged( bool canModify );
	idDict *				GetUserInfo( void );

	void					EnterCinematic( void );
	void					ExitCinematic( void );
	bool					HandleESC( void );
	bool					SkipCinematic( void );

	void					UpdateConditions( void );
	void					SetViewAngles( const idAngles &angles );

							// delta view angles to allow movers to rotate the view of the player
	void					UpdateDeltaViewAngles( const idAngles &angles );

	virtual bool			Collide( const trace_t &collision, const idVec3 &velocity );

	virtual void			GetAASLocation( idAAS *aas, idVec3 &pos, int &areaNum ) const;
	virtual void			GetAIAimTargets( const idVec3 &lastSightPos, idVec3 &headPos, idVec3 &chestPos );
	virtual void			DamageFeedback( idEntity *victim, idEntity *inflictor, int &damage );
	void					CalcDamagePoints(  idEntity *inflictor, idEntity *attacker, const idDict *damageDef,
							   const float damageScale, const int location, int *health, int *armor );
	virtual	void			Damage( idEntity *inflictor, idEntity *attacker, const idVec3 &dir, const char *damageDefName, const float damageScale, const int location );

							// use exitEntityNum to specify a teleport with private camera view and delayed exit
	virtual void			Teleport( const idVec3 &origin, const idAngles &angles, idEntity *destination );

	void					Kill( bool delayRespawn, bool nodamage );
	virtual void			Killed( idEntity *inflictor, idEntity *attacker, int damage, const idVec3 &dir, int location );
	void					StartFxOnBone(const char *fx, const char *bone);

	renderView_t *			GetRenderView( void );
	void					CalculateRenderView( void );	// called every tic by player code
	void					CalculateFirstPersonView( void );

	void					DrawHUD( idUserInterface *hud );

	float					DefaultFov( void ) const;
	float					CalcFov( bool honorZoom );
	idVec3					GetEyePosition( void ) const;
	void					GetViewPos( idVec3 &origin, idMat3 &axis ) const;
	void					OffsetThirdPersonView( float angle, float range, float height, bool clip );

	bool					Give( const char *statname, const char *value );
	bool					GiveItem( idItem *item );
	void					GiveItem( const char *name );
	
	bool					GiveInventoryItem( idDict *item );
	void					RemoveInventoryItem( idDict *item );
	bool					GiveInventoryItem( const char *name );
	void					RemoveInventoryItem( const char *name );
	idDict *				FindInventoryItem( const char *name );

	void					SetLastHitTime( int time );

	void					AdjustHeartRate( int target, float timeInSecs, float delay, bool force );
	void					SetCurrentHeartRate( void );
	int						GetBaseHeartRate( void );
	
	virtual bool			HandleSingleGuiCommand( idEntity *entityGui, idLexer *src );
	bool					GuiActive( void ) { return focusGUIent != NULL; }

	void					PerformImpulse( int impulse );
	void					RouteGuiMouse( idUserInterface *gui );
	void					UpdateHud( void );
	void					UpdateHudStats( idUserInterface *hud );
	void					SetInfluenceFov( float fov );
	void					SetInfluenceView( const char *mtr, const char *skinname, float radius, idEntity *ent );
	void					SetInfluenceLevel( int level );
	int						GetInfluenceLevel( void ) { return influenceActive; };
	void					SetPrivateCameraView( idCamera *camView );
	idCamera *				GetPrivateCameraView( void ) const { return privateCameraView; }
	void					StartFxFov( float duration  );

	virtual bool			GetPhysicsToVisualTransform( idVec3 &origin, idMat3 &axis );
	virtual bool			GetPhysicsToSoundTransform( idVec3 &origin, idMat3 &axis );

	bool					IsInTeleport( void );

	idEntity*				GetInfluenceEntity( void ) { return influenceEntity; };
	const idMaterial*		GetInfluenceMaterial( void ) { return influenceMaterial; };
	float					GetInfluenceRadius( void ) { return influenceRadius; };

	void					TeleportDeath( int killer );

	bool					OnLadder( void ) const;

	float					sticky;	// sikk - Sticky movement modifier

	int						endLevelTime;	// sikk - End Level Time
	bool					doEndLevel;		// sikk - End Level Flag

	float					GetIllumination( void );		// sikk - Player illumination test

	void					UpdateMonsterSpawns( void );	// sikk - Monster Count

// ---> sikk - Threat Level
	threatLevel_t			threatLevel;
	void					UpdateThreatLevel( void );
// <--- sikk - Threat Level

// ---> sikk - Crumb Count
	int						numCrumbs;
	float					GetCrumbPct( void );
	void					UpdateCrumbCount( void );
// <--- sikk - Crumb Count

// ---> sikk - Player Light
	renderLight_t			renderLight;
	qhandle_t				lightDefHandle;				// handle to renderer light def
	idVec3					lightOffset;
	float					lightBrightness;
	int						lightBrightnessTime;
	idInterpolate<float>	lightBrightnessLerp;
	virtual void			FreeLightDef( void );
	void					UpdatePlayerLight( bool doCharge );
// <--- sikk - Player Light

// ---> sikk - Target Post FX
	void					SetTargetPostFX( const char *mtr, idVec4 color );
// <--- sikk - Target Post FX

// ---> sikk - Centerprint Messages
	void					Centerprint( const char *msg, const char *soundShader = "", bool toConsole = false );
// <--- sikk - Centerprint Messages

// ---> sikk - Level Music
	int						musicTime;
	float					musicFadeTime;
	int						music1ShotTime;
	int						musicFlag;
	const idSoundShader *	sndAmbientLooping1;
	const idSoundShader *	sndAmbientLooping2;
	const idSoundShader *	sndAmbient1shot;
	void					SpawnMusic( void );
	void					UpdateMusic( void );
// <--- sikk - Level Music

// ---> sikk - Object Manipulation
	idGrabEntity			grabEntity;
	idEntity*				focusMoveable;
	int						focusMoveableId;
	int						focusMoveableTimer;
// <--- sikk - Object Manipulation

	float					focusDistance;	// sikk - Depth of Field PostProcess

private:
	idPhysics_Player		physicsObj;			// player physics

	idList<aasLocation_t>	aasLocation;		// for AI tracking the player

	int						bobFoot;
	float					bobFrac;
	float					bobfracsin;
	int						bobCycle;			// for view bobbing and footstep generation
	float					xyspeed;
	int						stepUpTime;
	float					stepUpDelta;
	float					idealLegsYaw;
	float					legsYaw;
	bool					legsForward;
	float					oldViewYaw;
	idAngles				viewBobAngles;
	idVec3					viewBob;
	int						landChange;
	int						landTime;

	bool					gibDeath;
	bool					gibsLaunched;
	idVec3					gibsDir;

	idInterpolate<float>	centerView;
	bool					fxFov;

	float					influenceFov;
	int						influenceActive;		// level of influence.. 1 == no gun or hud .. 2 == 1 + no movement
	idEntity *				influenceEntity;
	const idMaterial *		influenceMaterial;
	float					influenceRadius;
	const idDeclSkin *		influenceSkin;

	idCamera *				privateCameraView;

	static const int		NUM_LOGGED_VIEW_ANGLES = 64;		// for weapon turning angle offsets
	idAngles				loggedViewAngles[NUM_LOGGED_VIEW_ANGLES];	// [gameLocal.framenum&(LOGGED_VIEW_ANGLES-1)]
	static const int		NUM_LOGGED_ACCELS = 16;			// for weapon turning angle offsets
	loggedAccel_t			loggedAccel[NUM_LOGGED_ACCELS];	// [currentLoggedAccel & (NUM_LOGGED_ACCELS-1)]
	int						currentLoggedAccel;

	// if there is a focusGUIent, the attack button will be changed into mouse clicks
	idEntity *				focusGUIent;
	idUserInterface *		focusUI;				// focusGUIent->renderEntity.gui, gui2, or gui3
	int						focusTime;
	idAFEntity_Vehicle *	focusVehicle;
	idUserInterface *		cursor;
	
	// full screen guis track mouse movements directly
	int						oldMouseX;
	int						oldMouseY;

	int						lastDamageDef;
	idVec3					lastDamageDir;
	int						lastDamageLocation;
	int						smoothedFrame;
	bool					smoothedOriginUpdated;
	idVec3					smoothedOrigin;
	idAngles				smoothedAngles;

	void					LookAtKiller( idEntity *inflictor, idEntity *attacker );

	void					UseObjects( void );
	void					CrashLand( const idVec3 &oldOrigin, const idVec3 &oldVelocity );
	void					BobCycle( const idVec3 &pushVelocity );
	void					UpdateViewAngles( void );
	void					EvaluateControls( void );
	void					AdjustSpeed( void );
	void					AdjustBodyAngles( void );
	void					InitAASLocation( void );
	void					SetAASLocation( void );
	void					Move( void );

	void					ClearFocus( void );
	void					UpdateFocus( void );
	void					UpdateLocation( void );
	idUserInterface *		ActiveGui( void );

	void					UseVehicle( bool drive );	// sikk - function modified to support use function

	void					Event_GetButtons( void );
	void					Event_GetMove( void );
	void					Event_GetViewAngles( void );
	void					Event_StopFxFov( void );
	void					Event_ExitTeleporter( void );
	void					Event_LevelTrigger( void );
	void					Event_Gibbed( void );

	void					Event_GetLightBrightness( void );	// sikk - Player Light
	void					Event_GetCrumbPct( void );			// sikk - Crumb Count
	void					Event_GetIllumination( void );				// sikk - Player Illumination Test
};

ID_INLINE bool idPlayer::IsInTeleport( void ) {
	return ( teleportEntity.GetEntity() != NULL );
}

#endif /* !__GAME_PLAYER_H__ */

