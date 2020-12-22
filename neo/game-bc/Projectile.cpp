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

	idProjectile

===============================================================================
*/


static const int BFG_DAMAGE_FREQUENCY			= 100;	// sikk - this should be in the def
static const float BOUNCE_SOUND_MIN_VELOCITY	= 200.0f;
static const float BOUNCE_SOUND_MAX_VELOCITY	= 400.0f;

const idEventDef EV_Explode( "<explode>", NULL );
const idEventDef EV_Fizzle( "<fizzle>", NULL );
const idEventDef EV_RadiusDamage( "<radiusdmg>", "e" );
const idEventDef EV_GetProjectileState( "getProjectileState", NULL, 'd' );

CLASS_DECLARATION( idEntity, idProjectile )
EVENT( EV_Explode,				idProjectile::Event_Explode )
EVENT( EV_Fizzle,				idProjectile::Event_Fizzle )
EVENT( EV_Touch,				idProjectile::Event_Touch )
EVENT( EV_RadiusDamage,			idProjectile::Event_RadiusDamage )
EVENT( EV_GetProjectileState,	idProjectile::Event_GetProjectileState )
END_CLASS

/*
================
idProjectile::idProjectile
================
*/
idProjectile::idProjectile()
{
	owner				= NULL;
	lightDefHandle		= -1;
	thrust				= 0.0f;
	thrust_end			= 0;
	smokeFly			= NULL;
	smokeFlyTime		= 0;
	state				= SPAWNED;
	lightOffset			= vec3_zero;
	lightStartTime		= 0;
	lightEndTime		= 0;
	lightColor			= vec3_zero;
	state				= SPAWNED;
	damagePower			= 1.0f;
	memset( &projectileFlags, 0, sizeof( projectileFlags ) );
	memset( &renderLight, 0, sizeof( renderLight ) );

	// note: for net_instanthit projectiles, we will force this back to false at spawn time
	fl.networkSync		= true;

	netSyncPhysics		= false;
}

/*
================
idProjectile::Spawn
================
*/
void idProjectile::Spawn()
{
	physicsObj.SetSelf( this );
	physicsObj.SetClipModel( new idClipModel( GetPhysics()->GetClipModel() ), 1.0f );
	physicsObj.SetContents( 0 );
	physicsObj.SetClipMask( 0 );
	physicsObj.PutToRest();
	SetPhysics( &physicsObj );
}

/*
================
idProjectile::Save
================
*/
void idProjectile::Save( idSaveGame* savefile ) const
{

	owner.Save( savefile );

	projectileFlags_s flags = projectileFlags;
	LittleBitField( &flags, sizeof( flags ) );
	savefile->Write( &flags, sizeof( flags ) );

	savefile->WriteFloat( thrust );
	savefile->WriteInt( thrust_end );

	savefile->WriteRenderLight( renderLight );
	savefile->WriteInt( ( int )lightDefHandle );
	savefile->WriteVec3( lightOffset );
	savefile->WriteInt( lightStartTime );
	savefile->WriteInt( lightEndTime );
	savefile->WriteVec3( lightColor );

	savefile->WriteParticle( smokeFly );
	savefile->WriteInt( smokeFlyTime );

	savefile->WriteInt( ( int )state );

	savefile->WriteFloat( damagePower );

	savefile->WriteStaticObject( physicsObj );
	savefile->WriteStaticObject( thruster );
}

/*
================
idProjectile::Restore
================
*/
void idProjectile::Restore( idRestoreGame* savefile )
{

	owner.Restore( savefile );

	savefile->Read( &projectileFlags, sizeof( projectileFlags ) );
	LittleBitField( &projectileFlags, sizeof( projectileFlags ) );

	savefile->ReadFloat( thrust );
	savefile->ReadInt( thrust_end );

	savefile->ReadRenderLight( renderLight );
	savefile->ReadInt( ( int& )lightDefHandle );
	savefile->ReadVec3( lightOffset );
	savefile->ReadInt( lightStartTime );
	savefile->ReadInt( lightEndTime );
	savefile->ReadVec3( lightColor );

	savefile->ReadParticle( smokeFly );
	savefile->ReadInt( smokeFlyTime );

	savefile->ReadInt( ( int& )state );

	savefile->ReadFloat( damagePower );

	savefile->ReadStaticObject( physicsObj );
	RestorePhysics( &physicsObj );

	savefile->ReadStaticObject( thruster );
	thruster.SetPhysics( &physicsObj );

	if( smokeFly != NULL )
	{
		idVec3 dir;
		dir = physicsObj.GetLinearVelocity();
		dir.NormalizeFast();
		gameLocal.smokeParticles->EmitSmoke( smokeFly, gameLocal.time, gameLocal.random.RandomFloat(), GetPhysics()->GetOrigin(), GetPhysics()->GetAxis() );
	}
}

/*
================
idProjectile::GetOwner
================
*/
idEntity* idProjectile::GetOwner() const
{
	return owner.GetEntity();
}

/*
================
idProjectile::Create
================
*/
void idProjectile::Create( idEntity* owner, const idVec3& start, const idVec3& dir )
{
	idDict		args;
	idStr		shaderName;
	idVec3		light_color;
	idVec3		light_offset;
	idVec3		tmp;
	idMat3		axis;

	Unbind();

	// align z-axis of model with the direction
	axis = dir.ToMat3();
	tmp = axis[2];
	axis[2] = axis[0];
	axis[0] = -tmp;

	physicsObj.SetOrigin( start );
	physicsObj.SetAxis( axis );

	physicsObj.GetClipModel()->SetOwner( owner );

	this->owner = owner;

	memset( &renderLight, 0, sizeof( renderLight ) );
	shaderName = spawnArgs.GetString( "mtr_light_shader" );
	if( *( const char* )shaderName )
	{
		renderLight.shader = declManager->FindMaterial( shaderName, false );
		renderLight.pointLight = true;
		renderLight.lightRadius[0] =
			renderLight.lightRadius[1] =
				renderLight.lightRadius[2] = spawnArgs.GetFloat( "light_radius" );
		spawnArgs.GetVector( "light_color", "1 1 1", light_color );
		renderLight.shaderParms[0] = light_color[0];
		renderLight.shaderParms[1] = light_color[1];
		renderLight.shaderParms[2] = light_color[2];
		renderLight.shaderParms[3] = spawnArgs.GetFloat( "light_brightness", "1.0" ); // sikk - alpha holds light brightness
	}

	spawnArgs.GetVector( "light_offset", "0 0 0", lightOffset );

	lightStartTime = 0;
	lightEndTime = 0;
	smokeFlyTime = 0;

	damagePower = 1.0f;

	UpdateVisuals();

	state = CREATED;

	if( spawnArgs.GetBool( "net_fullphysics" ) )
	{
		netSyncPhysics = true;
	}
}

/*
=================
idProjectile::~idProjectile
=================
*/
idProjectile::~idProjectile()
{
	StopSound( SND_CHANNEL_ANY, false );
	FreeLightDef();
}

/*
=================
idProjectile::FreeLightDef
=================
*/
void idProjectile::FreeLightDef()
{
	if( lightDefHandle != -1 )
	{
		gameRenderWorld->FreeLightDef( lightDefHandle );
		lightDefHandle = -1;
	}
}

/*
=================
idProjectile::Launch
=================
*/
void idProjectile::Launch( const idVec3& start, const idVec3& dir, const idVec3& pushVelocity, const float timeSinceFire, const float launchPower, const float dmgPower, const bool primary )  	// sikk - Secondary Fire
{
	float			fuse;
	float			startthrust;
	float			endthrust;
	idVec3			velocity;
	idAngles		angular_velocity;
	float			linear_friction;
	float			angular_friction;
	float			contact_friction;
	float			bounce;
	float			mass;
	float			speed;
	float			gravity;
	idVec3			gravVec;
	idVec3			tmp;
	idMat3			axis;
	int				thrust_start;
	int				contents;
	int				clipMask;

	// allow characters to throw projectiles during cinematics, but not the player
	if( owner.GetEntity() && !owner.GetEntity()->IsType( idPlayer::Type ) )
	{
		cinematic = owner.GetEntity()->cinematic;
	}
	else
	{
		cinematic = false;
	}

	thrust				= spawnArgs.GetFloat( "thrust" );
	startthrust			= spawnArgs.GetFloat( "thrust_start" );
	endthrust			= spawnArgs.GetFloat( "thrust_end" );

	spawnArgs.GetVector( "velocity", "0 0 0", velocity );

	speed = velocity.Length() * launchPower;

	damagePower = dmgPower;

	spawnArgs.GetAngles( "angular_velocity", "0 0 0", angular_velocity );

	linear_friction		= spawnArgs.GetFloat( "linear_friction" );
	angular_friction	= spawnArgs.GetFloat( "angular_friction" );
	contact_friction	= spawnArgs.GetFloat( "contact_friction" );
	bounce				= spawnArgs.GetFloat( "bounce" );
	mass				= spawnArgs.GetFloat( "mass" );
	gravity				= spawnArgs.GetFloat( "gravity" );
	fuse				= spawnArgs.GetFloat( "fuse" );

	projectileFlags.detonate_on_world	= spawnArgs.GetBool( "detonate_on_world" );
	projectileFlags.detonate_on_actor	= spawnArgs.GetBool( "detonate_on_actor" );
	projectileFlags.randomShaderSpin	= spawnArgs.GetBool( "random_shader_spin" );

	if( mass <= 0 )
	{
		gameLocal.Error( "Invalid mass on '%s'\n", GetEntityDefName() );
	}

	thrust *= mass;
	thrust_start = SEC2MS( startthrust ) + gameLocal.time;
	thrust_end = SEC2MS( endthrust ) + gameLocal.time;

	lightStartTime = 0;
	lightEndTime = 0;

	if( health )
	{
		fl.takedamage = true;
	}

	gravVec = gameLocal.GetGravity();
	gravVec.NormalizeFast();

	Unbind();

	// align z-axis of model with the direction
	axis = dir.ToMat3();
	tmp = axis[2];
	axis[2] = axis[0];
	axis[0] = -tmp;

	contents = 0;
	clipMask = MASK_SHOT_RENDERMODEL;
	if( spawnArgs.GetBool( "detonate_on_trigger" ) )
	{
		contents |= CONTENTS_TRIGGER;
	}
	if( !spawnArgs.GetBool( "no_contents" ) )
	{
		contents |= CONTENTS_PROJECTILE;
		clipMask |= CONTENTS_PROJECTILE;
	}

	// don't do tracers on client, we don't know origin and direction
	if( spawnArgs.GetBool( "tracers" ) )  	// sikk - 100% Tracer Frequency
	{
		SetModel( spawnArgs.GetString( "model_tracer" ) );
		projectileFlags.isTracer = true;
	}

	physicsObj.SetMass( mass );
	physicsObj.SetFriction( linear_friction, angular_friction, contact_friction );
	if( contact_friction == 0.0f )
	{
		physicsObj.NoContact();
	}
	physicsObj.SetBouncyness( bounce );
	physicsObj.SetGravity( gravVec * gravity );
	physicsObj.SetContents( contents );
	physicsObj.SetClipMask( clipMask );
	physicsObj.SetLinearVelocity( axis[ 2 ] * speed + pushVelocity );
	physicsObj.SetAngularVelocity( angular_velocity.ToAngularVelocity() * axis );
	physicsObj.SetOrigin( start );
	physicsObj.SetAxis( axis );

	thruster.SetPosition( &physicsObj, 0, idVec3( GetPhysics()->GetBounds()[ 0 ].x, 0, 0 ) );

	if( fuse <= 0 )
	{
		// run physics for 1 second
		RunPhysics();
		PostEventMS( &EV_Remove, spawnArgs.GetInt( "remove_time", "1500" ) );
	}
	else if( spawnArgs.GetBool( "detonate_on_fuse" ) )
	{
		fuse -= timeSinceFire;
		if( fuse < 0.0f )
		{
			fuse = 0.0f;
		}
		PostEventSec( &EV_Explode, fuse );
	}
	else
	{
		fuse -= timeSinceFire;
		if( fuse < 0.0f )
		{
			fuse = 0.0f;
		}
		PostEventSec( &EV_Fizzle, fuse );
	}

	if( projectileFlags.isTracer )
	{
		StartSound( "snd_tracer", SND_CHANNEL_BODY, 0, false, NULL );
	}
	else
	{
		StartSound( "snd_fly", SND_CHANNEL_BODY, 0, false, NULL );
	}

	smokeFlyTime = 0;
	const char* smokeName = spawnArgs.GetString( "smoke_fly" );
	if( *smokeName != '\0' )
	{
		smokeFly = static_cast<const idDeclParticle*>( declManager->FindType( DECL_PARTICLE, smokeName ) );
		smokeFlyTime = gameLocal.time;
	}

	// used for the plasma bolts but may have other uses as well
	if( projectileFlags.randomShaderSpin )
	{
		float f = gameLocal.random.RandomFloat();
		f *= 0.5f;
		renderEntity.shaderParms[SHADERPARM_DIVERSITY] = f;
	}

	UpdateVisuals();

	state = LAUNCHED;
}

/*
================
idProjectile::Think
================
*/
void idProjectile::Think()
{
	if( thinkFlags & TH_THINK )
	{
		if( thrust && ( gameLocal.time < thrust_end ) )
		{
			// evaluate force
			thruster.SetForce( GetPhysics()->GetAxis()[ 0 ] * thrust );
			thruster.Evaluate( gameLocal.time );
		}
	}

	// run physics
	RunPhysics();

	Present();

	// add the particles
	if( smokeFly != NULL && smokeFlyTime && !IsHidden() )
	{
		idVec3 dir = -GetPhysics()->GetLinearVelocity();
		dir.Normalize();
		if( !gameLocal.smokeParticles->EmitSmoke( smokeFly, smokeFlyTime, gameLocal.random.RandomFloat(), GetPhysics()->GetOrigin(), dir.ToMat3() ) )
		{
			smokeFlyTime = gameLocal.time;
		}
	}

	// add the light
	if( renderLight.lightRadius.x > 0.0f && g_projectileLights.GetBool() )
	{
		renderLight.origin = GetPhysics()->GetOrigin() + GetPhysics()->GetAxis() * lightOffset;
		renderLight.axis = GetPhysics()->GetAxis();
		if( ( lightDefHandle != -1 ) )
		{
			if( lightEndTime > 0 && gameLocal.time <= lightEndTime + gameLocal.GetMSec() )
			{
				idVec3 color( 0, 0, 0 );
				if( gameLocal.time < lightEndTime )
				{
					float frac = ( float )( gameLocal.time - lightStartTime ) / ( float )( lightEndTime - lightStartTime );
					color.Lerp( lightColor, color, frac );
				}
				renderLight.shaderParms[SHADERPARM_RED] = color.x;
				renderLight.shaderParms[SHADERPARM_GREEN] = color.y;
				renderLight.shaderParms[SHADERPARM_BLUE] = color.z;
			}
			gameRenderWorld->UpdateLightDef( lightDefHandle, &renderLight );
		}
		else
		{
			lightDefHandle = gameRenderWorld->AddLightDef( &renderLight );
		}
	}
}

/*
=================
idProjectile::Collide
=================
*/
bool idProjectile::Collide( const trace_t& collision, const idVec3& velocity )
{
	idEntity*	ent;
	idEntity*	ignore;
	const char*	damageDefName;
	idVec3		dir;
	float		push;
	float		damageScale;

	if( state == EXPLODED || state == FIZZLED )
	{
		return true;
	}

	// remove projectile when a 'noimpact' surface is hit
	if( ( collision.c.material != NULL ) && ( collision.c.material->GetSurfaceFlags() & SURF_NOIMPACT ) )
	{
		PostEventMS( &EV_Remove, 0 );
		common->DPrintf( "Projectile collision no impact\n" );
		return true;
	}

	// get the entity the projectile collided with
	ent = gameLocal.entities[ collision.c.entityNum ];
	if( ent == owner.GetEntity() )
	{
		assert( 0 );
		return true;
	}

	// just get rid of the projectile when it hits a player in noclip
	if( ent->IsType( idPlayer::Type ) && static_cast<idPlayer*>( ent )->noclip )
	{
		PostEventMS( &EV_Remove, 0 );
		return true;
	}

	// direction of projectile
	dir = velocity;
	dir.Normalize();

	// projectiles can apply an additional impulse next to the rigid body physics impulse
	if( spawnArgs.GetFloat( "push", "0", push ) && push > 0.0f )
	{
		ent->ApplyImpulse( this, collision.c.id, collision.c.point, push * dir );
	}

// ---> sikk - Shootable Doors (mainly used for secrets)
	if( ent->IsType( idDoor::Type ) && !static_cast< idDoor* >( ent )->IsOpen() && ent->spawnArgs.GetBool( "shootable" ) )
	{
		ent->ProcessEvent( &EV_Activate , this );
	}
// <--- sikk - Shootable Doors

	if( ent->IsType( idActor::Type ) || ( ent->IsType( idAFAttachment::Type ) && static_cast<const idAFAttachment*>( ent )->GetBody()->IsType( idActor::Type ) ) )
	{
		if( !projectileFlags.detonate_on_actor )
		{
			return false;
		}
	}
	else
	{
		if( !projectileFlags.detonate_on_world )
		{
			if( !StartSound( "snd_ricochet", SND_CHANNEL_ITEM, 0, true, NULL ) )
			{
				float len = velocity.Length();
				if( len > BOUNCE_SOUND_MIN_VELOCITY )
				{
					SetSoundVolume( len > BOUNCE_SOUND_MAX_VELOCITY ? 1.0f : idMath::Sqrt( len - BOUNCE_SOUND_MIN_VELOCITY ) * ( 1.0f / idMath::Sqrt( BOUNCE_SOUND_MAX_VELOCITY - BOUNCE_SOUND_MIN_VELOCITY ) ) );
					StartSound( "snd_bounce", SND_CHANNEL_ANY, 0, true, NULL );
				}
			}
			return false;
		}
	}

	SetOrigin( collision.endpos );
	SetAxis( collision.endAxis );

	// unlink the clip model because we no longer need it
	GetPhysics()->UnlinkClip();

	damageDefName = spawnArgs.GetString( "def_damage" );

	ignore = NULL;

	// if the hit entity takes damage
	if( ent->fl.takedamage )
	{
		if( damagePower )
		{
			damageScale = damagePower;
		}
		else
		{
			damageScale = 1.0f;
		}

		if( damageDefName[0] != '\0' )
		{
			ent->Damage( this, owner.GetEntity(), dir, damageDefName, damageScale, CLIPMODEL_ID_TO_JOINT_HANDLE( collision.c.id ) );
			ignore = ent;
		}
	}

	// if the projectile causes a damage effect
	if( spawnArgs.GetBool( "impact_damage_effect" ) )
	{
		// if the hit entity has a special damage effect
		if( ent->spawnArgs.GetBool( "bleed" ) )
		{
			// sikk - we have to handle combat bbox entities specially
			if( ent->IsType( idAI::Type ) && ent->spawnArgs.GetBool( "use_combat_bbox" ) )
			{
				ent->AddDamageEffect( collision, velocity, damageDefName );
			}
			else
			{
				ent->AddDamageEffect( collision, velocity, damageDefName );
			}
		}
		else
		{
			AddDefaultDamageEffect( collision, velocity );
		}
	}

	Explode( collision, ignore );

	return true;
}

/*
=================
idProjectile::DefaultDamageEffect
=================
*/
void idProjectile::DefaultDamageEffect( idEntity* soundEnt, const idDict& projectileDef, const trace_t& collision, const idVec3& velocity )
{
	const char* decal, *sound, *typeName;
	surfTypes_t materialType;

	if( collision.c.material != NULL )
	{
		materialType = collision.c.material->GetSurfaceType();
	}
	else
	{
		materialType = SURFTYPE_METAL;
	}

	// get material type name
	typeName = gameLocal.sufaceTypeNames[ materialType ];

	// play impact sound
	sound = projectileDef.GetString( va( "snd_%s", typeName ) );
	if( *sound == '\0' )
	{
		sound = projectileDef.GetString( "snd_metal" );
	}
	if( *sound == '\0' )
	{
		sound = projectileDef.GetString( "snd_impact" );
	}
	if( *sound != '\0' )
	{
		soundEnt->StartSoundShader( declManager->FindSound( sound ), SND_CHANNEL_BODY, 0, false, NULL );
	}

	// project decal
	decal = projectileDef.GetString( va( "mtr_detonate_%s", typeName ) );
	if( *decal == '\0' )
	{
		decal = projectileDef.GetString( "mtr_detonate" );
	}
	if( *decal != '\0' )
	{
		gameLocal.ProjectDecal( collision.c.point, -collision.c.normal, 8.0f, true, projectileDef.GetFloat( "decal_size", "6.0" ), decal );
	}
}

/*
=================
idProjectile::AddDefaultDamageEffect
=================
*/
void idProjectile::AddDefaultDamageEffect( const trace_t& collision, const idVec3& velocity )
{
	DefaultDamageEffect( this, spawnArgs, collision, velocity );
}

/*
================
idProjectile::Killed
================
*/
void idProjectile::Killed( idEntity* inflictor, idEntity* attacker, int damage, const idVec3& dir, int location )
{
	if( spawnArgs.GetBool( "detonate_on_death" ) )
	{
		trace_t collision;

		memset( &collision, 0, sizeof( collision ) );
		collision.endAxis = GetPhysics()->GetAxis();
		collision.endpos = GetPhysics()->GetOrigin();
		collision.c.point = GetPhysics()->GetOrigin();
		collision.c.normal.Set( 0, 0, 1 );
		Explode( collision, NULL );
		physicsObj.ClearContacts();
		physicsObj.PutToRest();
	}
	else
	{
		Fizzle();
	}
}

/*
================
idProjectile::Fizzle
================
*/
void idProjectile::Fizzle()
{

	if( state == EXPLODED || state == FIZZLED )
	{
		return;
	}

	StopSound( SND_CHANNEL_BODY, false );
	StartSound( "snd_fizzle", SND_CHANNEL_BODY, 0, false, NULL );

	// fizzle FX
	const char* psystem = spawnArgs.GetString( "smoke_fuse" );
	if( psystem && *psystem )
	{
//FIXME:SMOKE		gameLocal.particles->SpawnParticles( GetPhysics()->GetOrigin(), vec3_origin, psystem );
	}

	// we need to work out how long the effects last and then remove them at that time
	// for example, bullets have no real effects
	if( smokeFly && smokeFlyTime )
	{
		smokeFlyTime = 0;
	}

	fl.takedamage = false;
	physicsObj.SetContents( 0 );
	physicsObj.GetClipModel()->Unlink();
	physicsObj.PutToRest();

	Hide();
	FreeLightDef();

	state = FIZZLED;

	CancelEvents( &EV_Fizzle );
	PostEventMS( &EV_Remove, spawnArgs.GetInt( "remove_time", "1500" ) );
}

/*
================
idProjectile::Event_RadiusDamage
================
*/
void idProjectile::Event_RadiusDamage( idEntity* ignore )
{
	const char* splash_damage = spawnArgs.GetString( "def_splash_damage" );
	if( splash_damage[0] != '\0' )
	{
		gameLocal.RadiusDamage( physicsObj.GetOrigin(), this, owner.GetEntity(), ignore, this, splash_damage, damagePower );
	}
}

/*
================
idProjectile::Event_GetProjectileState
================
*/
void idProjectile::Event_GetProjectileState()
{
	idThread::ReturnInt( state );
}

/*
================
idProjectile::Explode
================
*/
void idProjectile::Explode( const trace_t& collision, idEntity* ignore )
{
	const char* fxname, *light_shader, *sndExplode;
	float		light_fadetime;
	idVec3		normal;
	int			removeTime;

	if( state == EXPLODED || state == FIZZLED )
	{
		return;
	}

	// stop sound
	StopSound( SND_CHANNEL_BODY2, false );

	// play explode sound
	switch( ( int ) damagePower )
	{
		case 2:
			sndExplode = "snd_explode2";
			break;
		case 3:
			sndExplode = "snd_explode3";
			break;
		case 4:
			sndExplode = "snd_explode4";
			break;
		default:
			sndExplode = "snd_explode";
			break;
	}
	StartSound( sndExplode, SND_CHANNEL_BODY, 0, true, NULL );

	// we need to work out how long the effects last and then remove them at that time
	// for example, bullets have no real effects
	if( smokeFly && smokeFlyTime )
	{
		smokeFlyTime = 0;
	}

	Hide();
	FreeLightDef();

	if( spawnArgs.GetVector( "detonation_axis", "", normal ) )
	{
		GetPhysics()->SetAxis( normal.ToMat3() );
	}
	GetPhysics()->SetOrigin( collision.endpos + 2.0f * collision.c.normal );

	// default remove time
	removeTime = spawnArgs.GetInt( "remove_time", "1500" );

	// change the model, usually to a PRT
	fxname = NULL;
	if( g_testParticle.GetInteger() == TEST_PARTICLE_IMPACT )
	{
		fxname = g_testParticleName.GetString();
	}
	else
	{
		fxname = spawnArgs.GetString( "model_detonate" );
	}

	int surfaceType = collision.c.material != NULL ? collision.c.material->GetSurfaceType() : SURFTYPE_METAL;

	// sikk - combat bbox blood workaround
	idEntity* ent = gameLocal.entities[ collision.c.entityNum ];
	if( ent->IsType( idAI::Type ) && ent->spawnArgs.GetBool( "use_combat_bbox" ) )
	{
		surfaceType = SURFTYPE_FLESH;
	}

	if( !( fxname && *fxname ) )
	{
		//if ( ( surfaceType == SURFTYPE_NONE ) || ( surfaceType == SURFTYPE_METAL ) || ( surfaceType == SURFTYPE_STONE ) ) {
		//	fxname = spawnArgs.GetString( "model_smokespark" );
		//} else if ( surfaceType == SURFTYPE_RICOCHET ) {
		//	fxname = spawnArgs.GetString( "model_ricochet" );
		//} else {
		//	fxname = spawnArgs.GetString( "model_smoke" );
		//}

		switch( surfaceType )
		{
			case SURFTYPE_NONE:
				fxname = spawnArgs.GetString( "model_impact_default" );
				break;
			case SURFTYPE_METAL:
				fxname = spawnArgs.GetString( "model_impact_metal" );
				break;
			case SURFTYPE_STONE:
				fxname = spawnArgs.GetString( "model_impact_stone" );
				break;
			case SURFTYPE_FLESH:
				fxname = spawnArgs.GetString( "model_impact_flesh" );
				break;
			case SURFTYPE_WOOD:
				fxname = spawnArgs.GetString( "model_impact_wood" );
				break;
			case SURFTYPE_CARDBOARD:
				fxname = spawnArgs.GetString( "model_impact_cardboard" );
				break;
			case SURFTYPE_LIQUID:
				fxname = spawnArgs.GetString( "model_impact_liquid" );
				break;
			case SURFTYPE_GLASS:
				fxname = spawnArgs.GetString( "model_impact_glass" );
				break;
			case SURFTYPE_PLASTIC:
				fxname = spawnArgs.GetString( "model_impact_plastic" );
				break;
			case SURFTYPE_RICOCHET:
				fxname = spawnArgs.GetString( "model_impact_ricochet" );
				break;
			default:
				fxname = spawnArgs.GetString( "model_impact_default" );
				break;
		}
	}

	if( fxname && *fxname )
	{
		SetModel( fxname );
		renderEntity.shaderParms[SHADERPARM_RED] =
			renderEntity.shaderParms[SHADERPARM_GREEN] =
				renderEntity.shaderParms[SHADERPARM_BLUE] =
					renderEntity.shaderParms[SHADERPARM_ALPHA] = 1.0f;
		renderEntity.shaderParms[SHADERPARM_TIMEOFFSET] = -MS2SEC( gameLocal.time );
		renderEntity.shaderParms[SHADERPARM_DIVERSITY] = gameLocal.random.CRandomFloat();

		Show();
		removeTime = ( removeTime > 3000 ) ? removeTime : 3000;
	}

	// explosion light
	light_shader = spawnArgs.GetString( "mtr_explode_light_shader" );
	if( *light_shader )
	{
		renderLight.shader = declManager->FindMaterial( light_shader, false );
		renderLight.pointLight = true;
		renderLight.lightRadius[0] =
			renderLight.lightRadius[1] =
				renderLight.lightRadius[2] = spawnArgs.GetFloat( "explode_light_radius" );
		spawnArgs.GetVector( "explode_light_color", "1 1 1", lightColor );
		renderLight.shaderParms[SHADERPARM_RED] = lightColor.x;
		renderLight.shaderParms[SHADERPARM_GREEN] = lightColor.y;
		renderLight.shaderParms[SHADERPARM_BLUE] = lightColor.z;
		renderLight.shaderParms[SHADERPARM_ALPHA] = spawnArgs.GetFloat( "explode_light_brightness", "1.0" ); // sikk - alpha holds light brightness
		renderLight.shaderParms[SHADERPARM_TIMEOFFSET] = -MS2SEC( gameLocal.time );
		light_fadetime = spawnArgs.GetFloat( "explode_light_fadetime", "0.5" );
		lightStartTime = gameLocal.time;
		lightEndTime = gameLocal.time + SEC2MS( light_fadetime );
		BecomeActive( TH_THINK );
	}

	fl.takedamage = false;
	physicsObj.SetContents( 0 );
	physicsObj.PutToRest();

	state = EXPLODED;

	// alert the ai
	gameLocal.AlertAI( owner.GetEntity() );

	// bind the projectile to the impact entity if necesary
	if( gameLocal.entities[collision.c.entityNum] && spawnArgs.GetBool( "bindOnImpact" ) )
	{
		Bind( gameLocal.entities[collision.c.entityNum], true );
	}

	// splash damage
	if( !projectileFlags.noSplashDamage )
	{
		float delay = spawnArgs.GetFloat( "delay_splash" );
		if( delay )
		{
			if( removeTime < SEC2MS( delay ) )
			{
				removeTime = SEC2MS( delay + 0.10 );
			}
			PostEventSec( &EV_RadiusDamage, delay, ignore );
		}
		else
		{
			Event_RadiusDamage( ignore );
		}
	}

	// spawn debris entities
	int fxdebris = spawnArgs.GetInt( "debris_count" );
	if( fxdebris )
	{
		const idDict* debris = gameLocal.FindEntityDefDict( "projectile_debris", false );
		if( debris )
		{
			int amount = gameLocal.random.RandomInt( fxdebris );
			for( int i = 0; i < amount; i++ )
			{
				idEntity* ent;
				idVec3 dir;
				dir.x = gameLocal.random.CRandomFloat() * 4.0f;
				dir.y = gameLocal.random.CRandomFloat() * 4.0f;
				dir.z = gameLocal.random.RandomFloat() * 8.0f;
				dir.Normalize();

				gameLocal.SpawnEntityDef( *debris, &ent, false );
				if( !ent || !ent->IsType( idDebris::Type ) )
				{
					gameLocal.Error( "'projectile_debris' is not an idDebris" );
				}

				idDebris* debris = static_cast<idDebris*>( ent );
				debris->Create( owner.GetEntity(), physicsObj.GetOrigin(), dir.ToMat3() );
				debris->Launch();
			}
		}
		debris = gameLocal.FindEntityDefDict( "projectile_shrapnel", false );
		if( debris )
		{
			int amount = gameLocal.random.RandomInt( fxdebris );
			for( int i = 0; i < amount; i++ )
			{
				idEntity* ent;
				idVec3 dir;
				dir.x = gameLocal.random.CRandomFloat() * 8.0f;
				dir.y = gameLocal.random.CRandomFloat() * 8.0f;
				dir.z = gameLocal.random.RandomFloat() * 8.0f + 8.0f;
				dir.Normalize();

				gameLocal.SpawnEntityDef( *debris, &ent, false );
				if( !ent || !ent->IsType( idDebris::Type ) )
				{
					gameLocal.Error( "'projectile_shrapnel' is not an idDebris" );
				}

				idDebris* debris = static_cast<idDebris*>( ent );
				debris->Create( owner.GetEntity(), physicsObj.GetOrigin(), dir.ToMat3() );
				debris->Launch();
			}
		}
	}

	CancelEvents( &EV_Explode );
	PostEventMS( &EV_Remove, removeTime );
}

/*
================
idProjectile::GetVelocity
================
*/
idVec3 idProjectile::GetVelocity( const idDict* projectile )
{
	idVec3 velocity;

	projectile->GetVector( "velocity", "0 0 0", velocity );
	return velocity;
}

/*
================
idProjectile::GetGravity
================
*/
idVec3 idProjectile::GetGravity( const idDict* projectile )
{
	float gravity;

	gravity = projectile->GetFloat( "gravity" );
	return idVec3( 0, 0, -gravity );
}

/*
================
idProjectile::Event_Explode
================
*/
void idProjectile::Event_Explode()
{
	trace_t collision;

	memset( &collision, 0, sizeof( collision ) );
	collision.endAxis = GetPhysics()->GetAxis();
	collision.endpos = GetPhysics()->GetOrigin();
	collision.c.point = GetPhysics()->GetOrigin();
	collision.c.normal.Set( 0, 0, 1 );
	AddDefaultDamageEffect( collision, collision.c.normal );
	Explode( collision, NULL );
}

/*
================
idProjectile::Event_Fizzle
================
*/
void idProjectile::Event_Fizzle()
{
	Fizzle();
}

/*
================
idProjectile::Event_Touch
================
*/
void idProjectile::Event_Touch( idEntity* other, trace_t* trace )
{

	if( IsHidden() )
	{
		return;
	}

	if( other != owner.GetEntity() )
	{
		trace_t collision;

		memset( &collision, 0, sizeof( collision ) );
		collision.endAxis = GetPhysics()->GetAxis();
		collision.endpos = GetPhysics()->GetOrigin();
		collision.c.point = GetPhysics()->GetOrigin();
		collision.c.normal.Set( 0, 0, 1 );
		AddDefaultDamageEffect( collision, collision.c.normal );
		Explode( collision, NULL );
	}
}


/*
===============================================================================

	idGuidedProjectile

===============================================================================
*/

CLASS_DECLARATION( idProjectile, idGuidedProjectile )
END_CLASS

/*
================
idGuidedProjectile::idGuidedProjectile
================
*/
idGuidedProjectile::idGuidedProjectile()
{
	enemy			= NULL;
	speed			= 0.0f;
	turn_max		= 0.0f;
	clamp_dist		= 0.0f;
	rndScale		= ang_zero;
	rndAng			= ang_zero;
	rndUpdateTime	= 0;
	angles			= ang_zero;
	burstMode		= false;
	burstDist		= 0;
	burstVelocity	= 0.0f;
	unGuided		= false;
}

/*
=================
idGuidedProjectile::~idGuidedProjectile
=================
*/
idGuidedProjectile::~idGuidedProjectile()
{
}

/*
================
idGuidedProjectile::Spawn
================
*/
void idGuidedProjectile::Spawn()
{
}

/*
================
idGuidedProjectile::Save
================
*/
void idGuidedProjectile::Save( idSaveGame* savefile ) const
{
	enemy.Save( savefile );
	savefile->WriteFloat( speed );
	savefile->WriteAngles( rndScale );
	savefile->WriteAngles( rndAng );
	savefile->WriteInt( rndUpdateTime );
	savefile->WriteFloat( turn_max );
	savefile->WriteFloat( clamp_dist );
	savefile->WriteAngles( angles );
	savefile->WriteBool( burstMode );
	savefile->WriteBool( unGuided );
	savefile->WriteFloat( burstDist );
	savefile->WriteFloat( burstVelocity );
}

/*
================
idGuidedProjectile::Restore
================
*/
void idGuidedProjectile::Restore( idRestoreGame* savefile )
{
	enemy.Restore( savefile );
	savefile->ReadFloat( speed );
	savefile->ReadAngles( rndScale );
	savefile->ReadAngles( rndAng );
	savefile->ReadInt( rndUpdateTime );
	savefile->ReadFloat( turn_max );
	savefile->ReadFloat( clamp_dist );
	savefile->ReadAngles( angles );
	savefile->ReadBool( burstMode );
	savefile->ReadBool( unGuided );
	savefile->ReadFloat( burstDist );
	savefile->ReadFloat( burstVelocity );
}


/*
================
idGuidedProjectile::GetSeekPos
================
*/
void idGuidedProjectile::GetSeekPos( idVec3& out )
{
	idEntity* enemyEnt = enemy.GetEntity();
	if( enemyEnt )
	{
		if( enemyEnt->IsType( idActor::Type ) )
		{
			out = static_cast<idActor*>( enemyEnt )->GetEyePosition();
			out.z -= 12.0f;
		}
		else
		{
			out = enemyEnt->GetPhysics()->GetOrigin();
		}
	}
	else
	{
		out = GetPhysics()->GetOrigin() + physicsObj.GetLinearVelocity() * 2.0f;
	}
}

/*
================
idGuidedProjectile::Think
================
*/
void idGuidedProjectile::Think()
{
	idVec3		dir;
	idVec3		seekPos;
	idVec3		velocity;
	idVec3		nose;
	idVec3		tmp;
	idMat3		axis;
	idAngles	dirAng;
	idAngles	diff;
	float		dist;
	float		frac;
	int			i;

	if( state == LAUNCHED && !unGuided )
	{

		GetSeekPos( seekPos );

		if( rndUpdateTime < gameLocal.time )
		{
			rndAng[ 0 ] = rndScale[ 0 ] * gameLocal.random.CRandomFloat();
			rndAng[ 1 ] = rndScale[ 1 ] * gameLocal.random.CRandomFloat();
			rndAng[ 2 ] = rndScale[ 2 ] * gameLocal.random.CRandomFloat();
			rndUpdateTime = gameLocal.time + 200;
		}

		nose = physicsObj.GetOrigin() + 10.0f * physicsObj.GetAxis()[0];

		dir = seekPos - nose;
		dist = dir.Normalize();
		dirAng = dir.ToAngles();

		// make it more accurate as it gets closer
		frac = dist / clamp_dist;
		if( frac > 1.0f )
		{
			frac = 1.0f;
		}

		diff = dirAng - angles + rndAng * frac;

		// clamp the to the max turn rate
		diff.Normalize180();
		for( i = 0; i < 3; i++ )
		{
			if( diff[ i ] > turn_max )
			{
				diff[ i ] = turn_max;
			}
			else if( diff[ i ] < -turn_max )
			{
				diff[ i ] = -turn_max;
			}
		}
		angles += diff;

		// make the visual model always points the dir we're traveling
		dir = angles.ToForward();
		velocity = dir * speed;

		if( burstMode && dist < burstDist )
		{
			unGuided = true;
			velocity *= burstVelocity;
		}

		physicsObj.SetLinearVelocity( velocity );

		// align z-axis of model with the direction
		axis = dir.ToMat3();
		tmp = axis[2];
		axis[2] = axis[0];
		axis[0] = -tmp;

		GetPhysics()->SetAxis( axis );
	}

	idProjectile::Think();
}

/*
=================
idGuidedProjectile::Launch
=================
*/
void idGuidedProjectile::Launch( const idVec3& start, const idVec3& dir, const idVec3& pushVelocity, const float timeSinceFire, const float launchPower, float dmgPower, const bool primary )
{
	idProjectile::Launch( start, dir, pushVelocity, timeSinceFire, launchPower, dmgPower );
	if( owner.GetEntity() )
	{
		if( owner.GetEntity()->IsType( idAI::Type ) )
		{
			enemy = static_cast<idAI*>( owner.GetEntity() )->GetEnemy();
		}
		else if( owner.GetEntity()->IsType( idPlayer::Type ) )
		{
			trace_t tr;
			idPlayer* player = static_cast<idPlayer*>( owner.GetEntity() );
			idVec3 start = player->GetEyePosition();
			idVec3 end = start + player->viewAxis[0] * 1000.0f;
			gameLocal.clip.TracePoint( tr, start, end, MASK_SHOT_RENDERMODEL | CONTENTS_BODY, owner.GetEntity() );
			if( tr.fraction < 1.0f )
			{
				enemy = gameLocal.GetTraceEntity( tr );
			}
			// ignore actors on the player's team
			if( enemy.GetEntity() == NULL || !enemy.GetEntity()->IsType( idActor::Type ) || ( static_cast<idActor*>( enemy.GetEntity() )->team == player->team ) )
			{
				enemy = player->EnemyWithMostHealth();
			}
		}
	}
	const idVec3& vel = physicsObj.GetLinearVelocity();
	angles = vel.ToAngles();
	speed = vel.Length();
	rndScale = spawnArgs.GetAngles( "random", "15 15 0" );
	turn_max = spawnArgs.GetFloat( "turn_max", "180" ) / ( float )USERCMD_HZ;
	clamp_dist = spawnArgs.GetFloat( "clamp_dist", "256" );
	burstMode = spawnArgs.GetBool( "burstMode" );
	unGuided = false;
	burstDist = spawnArgs.GetFloat( "burstDist", "64" );
	burstVelocity = spawnArgs.GetFloat( "burstVelocity", "1.25" );
	UpdateVisuals();
}

/*
===============================================================================

	idDebris

===============================================================================
*/

CLASS_DECLARATION( idEntity, idDebris )
EVENT( EV_Explode,			idDebris::Event_Explode )
EVENT( EV_Fizzle,			idDebris::Event_Fizzle )
END_CLASS

/*
================
idDebris::Spawn
================
*/
void idDebris::Spawn()
{
	owner = NULL;
	smokeFly = NULL;
	smokeFlyTime = 0;
}

/*
================
idDebris::Create
================
*/
void idDebris::Create( idEntity* owner, const idVec3& start, const idMat3& axis )
{
	Unbind();
	GetPhysics()->SetOrigin( start );
	GetPhysics()->SetAxis( axis );
	GetPhysics()->SetContents( 0 );
	this->owner = owner;
	smokeFly = NULL;
	smokeFlyTime = 0;
	sndBounce = NULL;
	UpdateVisuals();
}

/*
=================
idDebris::idDebris
=================
*/
idDebris::idDebris()
{
	owner = NULL;
	smokeFly = NULL;
	smokeFlyTime = 0;
	sndBounce = NULL;
}

/*
=================
idDebris::~idDebris
=================
*/
idDebris::~idDebris()
{
}

/*
=================
idDebris::Save
=================
*/
void idDebris::Save( idSaveGame* savefile ) const
{
	owner.Save( savefile );

	savefile->WriteStaticObject( physicsObj );

	savefile->WriteParticle( smokeFly );
	savefile->WriteInt( smokeFlyTime );
	savefile->WriteSoundShader( sndBounce );
}

/*
=================
idDebris::Restore
=================
*/
void idDebris::Restore( idRestoreGame* savefile )
{
	owner.Restore( savefile );

	savefile->ReadStaticObject( physicsObj );
	RestorePhysics( &physicsObj );

	savefile->ReadParticle( smokeFly );
	savefile->ReadInt( smokeFlyTime );
	savefile->ReadSoundShader( sndBounce );
}

/*
=================
idDebris::Launch
=================
*/
void idDebris::Launch()
{
	float		fuse;
	idVec3		velocity;
	idAngles	angular_velocity;
	float		linear_friction;
	float		angular_friction;
	float		contact_friction;
	float		bounce;
	float		mass;
	float		gravity;
	idVec3		gravVec;
	bool		randomVelocity;
	idMat3		axis;

	renderEntity.shaderParms[ SHADERPARM_TIMEOFFSET ] = -MS2SEC( gameLocal.time );

	spawnArgs.GetVector( "velocity", "0 0 0", velocity );
	spawnArgs.GetAngles( "angular_velocity", "0 0 0", angular_velocity );

	linear_friction		= spawnArgs.GetFloat( "linear_friction" );
	angular_friction	= spawnArgs.GetFloat( "angular_friction" );
	contact_friction	= spawnArgs.GetFloat( "contact_friction" );
	bounce				= spawnArgs.GetFloat( "bounce" );
	mass				= spawnArgs.GetFloat( "mass" );
	gravity				= spawnArgs.GetFloat( "gravity" );
	fuse				= spawnArgs.GetFloat( "fuse" );
	randomVelocity		= spawnArgs.GetBool( "random_velocity" );

	if( mass <= 0 )
	{
		gameLocal.Error( "Invalid mass on '%s'\n", GetEntityDefName() );
	}

	if( randomVelocity )
	{
		velocity.x *= gameLocal.random.RandomFloat() + 0.5f;
		velocity.y *= gameLocal.random.RandomFloat() + 0.5f;
		velocity.z *= gameLocal.random.RandomFloat() + 0.5f;
	}

	if( health )
	{
		fl.takedamage = true;
	}

	gravVec = gameLocal.GetGravity();
	gravVec.NormalizeFast();
	axis = GetPhysics()->GetAxis();

	Unbind();

	physicsObj.SetSelf( this );

	// check if a clip model is set
	const char* clipModelName;
	idTraceModel trm;
	spawnArgs.GetString( "clipmodel", "", &clipModelName );
	if( !clipModelName[0] )
	{
		clipModelName = spawnArgs.GetString( "model" );		// use the visual model
	}

	// load the trace model
	if( !collisionModelManager->TrmFromModel( clipModelName, trm ) )
	{
		// default to a box
		physicsObj.SetClipBox( renderEntity.bounds, 1.0f );
	}
	else
	{
		physicsObj.SetClipModel( new idClipModel( trm ), 1.0f );
	}

	physicsObj.GetClipModel()->SetOwner( owner.GetEntity() );
	physicsObj.SetMass( mass );
	physicsObj.SetFriction( linear_friction, angular_friction, contact_friction );
	if( contact_friction == 0.0f )
	{
		physicsObj.NoContact();
	}
	physicsObj.SetBouncyness( bounce );
	physicsObj.SetGravity( gravVec * gravity );
	physicsObj.SetContents( 0 );
	physicsObj.SetClipMask( MASK_SOLID | CONTENTS_MOVEABLECLIP );
	physicsObj.SetLinearVelocity( axis[ 0 ] * velocity[ 0 ] + axis[ 1 ] * velocity[ 1 ] + axis[ 2 ] * velocity[ 2 ] );
	physicsObj.SetAngularVelocity( angular_velocity.ToAngularVelocity() * axis );
	physicsObj.SetOrigin( GetPhysics()->GetOrigin() );
	physicsObj.SetAxis( axis );
	SetPhysics( &physicsObj );

	if( fuse <= 0 )
	{
		// run physics for 1 second
		RunPhysics();
		PostEventMS( &EV_Remove, 0 );
	}
	else if( spawnArgs.GetBool( "detonate_on_fuse" ) )
	{
		if( fuse < 0.0f )
		{
			fuse = 0.0f;
		}
		RunPhysics();
		PostEventSec( &EV_Explode, fuse );
	}
	else
	{
		if( fuse < 0.0f )
		{
			fuse = 0.0f;
		}
		PostEventSec( &EV_Fizzle, fuse );
	}

	StartSound( "snd_fly", SND_CHANNEL_BODY, 0, false, NULL );

	smokeFly = NULL;
	smokeFlyTime = 0;
	const char* smokeName = spawnArgs.GetString( "smoke_fly" );
	if( *smokeName != '\0' )
	{
		smokeFly = static_cast<const idDeclParticle*>( declManager->FindType( DECL_PARTICLE, smokeName ) );
		smokeFlyTime = gameLocal.time;
		gameLocal.smokeParticles->EmitSmoke( smokeFly, smokeFlyTime, gameLocal.random.CRandomFloat(), GetPhysics()->GetOrigin(), GetPhysics()->GetAxis() );
	}

	const char* sndName = spawnArgs.GetString( "snd_bounce" );
	if( *sndName != '\0' )
	{
		sndBounce = declManager->FindSound( sndName );
	}

	UpdateVisuals();
}

/*
================
idDebris::Think
================
*/
void idDebris::Think()
{

	// run physics
	RunPhysics();
	Present();

	if( smokeFly && smokeFlyTime )
	{
		if( !gameLocal.smokeParticles->EmitSmoke( smokeFly, smokeFlyTime, gameLocal.random.CRandomFloat(), GetPhysics()->GetOrigin(), GetPhysics()->GetAxis() ) )
		{
			smokeFlyTime = 0;
		}
	}
}

/*
================
idDebris::Killed
================
*/
void idDebris::Killed( idEntity* inflictor, idEntity* attacker, int damage, const idVec3& dir, int location )
{
	if( spawnArgs.GetBool( "detonate_on_death" ) )
	{
		Explode();
	}
	else
	{
		Fizzle();
	}
}

/*
=================
idDebris::Collide
=================
*/
bool idDebris::Collide( const trace_t& collision, const idVec3& velocity )
{
	if( sndBounce != NULL )
	{
		StartSoundShader( sndBounce, SND_CHANNEL_BODY, 0, false, NULL );
	}
	sndBounce = NULL;
	return false;
}


/*
================
idDebris::Fizzle
================
*/
void idDebris::Fizzle()
{
	if( IsHidden() )
	{
		// already exploded
		return;
	}

	StopSound( SND_CHANNEL_ANY, false );
	StartSound( "snd_fizzle", SND_CHANNEL_BODY, 0, false, NULL );

	// fizzle FX
	const char* smokeName = spawnArgs.GetString( "smoke_fuse" );
	if( *smokeName != '\0' )
	{
		smokeFly = static_cast<const idDeclParticle*>( declManager->FindType( DECL_PARTICLE, smokeName ) );
		smokeFlyTime = gameLocal.time;
		gameLocal.smokeParticles->EmitSmoke( smokeFly, smokeFlyTime, gameLocal.random.CRandomFloat(), GetPhysics()->GetOrigin(), GetPhysics()->GetAxis() );
	}

	fl.takedamage = false;
	physicsObj.SetContents( 0 );
	physicsObj.PutToRest();

	Hide();

	CancelEvents( &EV_Fizzle );
	PostEventMS( &EV_Remove, 0 );
}

/*
================
idDebris::Explode
================
*/
void idDebris::Explode()
{
	if( IsHidden() )
	{
		// already exploded
		return;
	}

	StopSound( SND_CHANNEL_ANY, false );
	StartSound( "snd_explode", SND_CHANNEL_BODY, 0, false, NULL );

	Hide();

	// these must not be "live forever" particle systems
	smokeFly = NULL;
	smokeFlyTime = 0;
	const char* smokeName = spawnArgs.GetString( "smoke_detonate" );
	if( *smokeName != '\0' )
	{
		smokeFly = static_cast<const idDeclParticle*>( declManager->FindType( DECL_PARTICLE, smokeName ) );
		smokeFlyTime = gameLocal.time;
		gameLocal.smokeParticles->EmitSmoke( smokeFly, smokeFlyTime, gameLocal.random.CRandomFloat(), GetPhysics()->GetOrigin(), GetPhysics()->GetAxis() );
	}

	fl.takedamage = false;
	physicsObj.SetContents( 0 );
	physicsObj.PutToRest();

	CancelEvents( &EV_Explode );
	PostEventMS( &EV_Remove, 0 );
}

/*
================
idDebris::Event_Explode
================
*/
void idDebris::Event_Explode()
{
	Explode();
}

/*
================
idDebris::Event_Fizzle
================
*/
void idDebris::Event_Fizzle()
{
	Fizzle();
}
