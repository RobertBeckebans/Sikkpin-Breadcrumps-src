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

	Client running game code:
	- entity events don't work and should not be issued
	- entities should never be spawned outside idGameLocal::ClientReadSnapshot

===============================================================================
*/

// adds tags to the network protocol to detect when things go bad ( internal consistency )
// NOTE: this changes the network protocol
#ifndef ASYNC_WRITE_TAGS
	#define ASYNC_WRITE_TAGS 0
#endif

idCVar net_clientShowSnapshot( "net_clientShowSnapshot", "0", CVAR_GAME | CVAR_INTEGER, "", 0, 3, idCmdSystem::ArgCompletion_Integer<0,3> );
idCVar net_clientShowSnapshotRadius( "net_clientShowSnapshotRadius", "128", CVAR_GAME | CVAR_FLOAT, "" );
idCVar net_clientSmoothing( "net_clientSmoothing", "0.8", CVAR_GAME | CVAR_FLOAT, "smooth other clients angles and position.", 0.0f, 0.95f );
idCVar net_clientSelfSmoothing( "net_clientSelfSmoothing", "0.6", CVAR_GAME | CVAR_FLOAT, "smooth self position if network causes prediction error.", 0.0f, 0.95f );
idCVar net_clientMaxPrediction( "net_clientMaxPrediction", "1000", CVAR_SYSTEM | CVAR_INTEGER | CVAR_NOCHEAT, "maximum number of milliseconds a client can predict ahead of server." );
idCVar net_clientLagOMeter( "net_clientLagOMeter", "1", CVAR_GAME | CVAR_BOOL | CVAR_NOCHEAT | CVAR_ARCHIVE, "draw prediction graph" );

/*
================
idGameLocal::InitAsyncNetwork
================
*/
void idGameLocal::InitAsyncNetwork( void ) {
	int i, type;

	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		for ( type = 0; type < declManager->GetNumDeclTypes(); type++ ) {
			clientDeclRemap[i][type].Clear();
		}
	}

	memset( clientEntityStates, 0, sizeof( clientEntityStates ) );
	memset( clientPVS, 0, sizeof( clientPVS ) );
	memset( clientSnapshots, 0, sizeof( clientSnapshots ) );

	eventQueue.Init();
	savedEventQueue.Init();

	entityDefBits = -( idMath::BitsForInteger( declManager->GetNumDecls( DECL_ENTITYDEF ) ) + 1 );
	localClientNum = 0; // on a listen server SetLocalUser will set this right
	realClientTime = 0;
	isNewFrame = true;
	clientSmoothing = net_clientSmoothing.GetFloat();
}

/*
================
idGameLocal::ShutdownAsyncNetwork
================
*/
void idGameLocal::ShutdownAsyncNetwork( void ) {
	entityStateAllocator.Shutdown();
	snapshotAllocator.Shutdown();
	eventQueue.Shutdown();
	savedEventQueue.Shutdown();
	memset( clientEntityStates, 0, sizeof( clientEntityStates ) );
	memset( clientPVS, 0, sizeof( clientPVS ) );
	memset( clientSnapshots, 0, sizeof( clientSnapshots ) );
}

/*
================
idGameLocal::InitLocalClient
================
*/
void idGameLocal::InitLocalClient( int clientNum ) {
	localClientNum = clientNum;
	clientSmoothing = net_clientSmoothing.GetFloat();
}

/*
================
idGameLocal::InitClientDeclRemap
================
*/
void idGameLocal::InitClientDeclRemap( int clientNum ) {
	int type, i, num;

	for ( type = 0; type < declManager->GetNumDeclTypes(); type++ ) {

		// only implicit materials and sound shaders decls are used
		if ( type != DECL_MATERIAL && type != DECL_SOUND ) {
			continue;
		}

		num = declManager->GetNumDecls( (declType_t) type );
		clientDeclRemap[clientNum][type].Clear();
		clientDeclRemap[clientNum][type].AssureSize( num, -1 );

		// pre-initialize the remap with non-implicit decls, all non-implicit decls are always going
		// to be in order and in sync between server and client because of the decl manager checksum
		for ( i = 0; i < num; i++ ) {
			const idDecl *decl = declManager->DeclByIndex( (declType_t) type, i, false );
			if ( decl->IsImplicit() ) {
				// once the first implicit decl is found all remaining decls are considered implicit as well
				break;
			}
			clientDeclRemap[clientNum][type][i] = i;
		}
	}
}

/*
================
idGameLocal::ServerSendDeclRemapToClient
================
*/
void idGameLocal::ServerSendDeclRemapToClient( int clientNum, declType_t type, int index ) {
	idBitMsg	outMsg;
	byte		msgBuf[MAX_GAME_MESSAGE_SIZE];

	// if no client connected for this spot
	if ( entities[clientNum] == NULL ) {
		return;
	}
	// increase size of list if required
	if ( index >= clientDeclRemap[clientNum][type].Num() ) {
		clientDeclRemap[clientNum][(int)type].AssureSize( index + 1, -1 );
	}
	// if already remapped
	if ( clientDeclRemap[clientNum][(int)type][index] != -1 ) {
		return;
	}

	const idDecl *decl = declManager->DeclByIndex( type, index, false );
	if ( decl == NULL ) {
		gameLocal.Error( "server tried to remap bad %s decl index %d", declManager->GetDeclNameFromType( type ), index );
		return;
	}

	// set the index at the server
	clientDeclRemap[clientNum][(int)type][index] = index;

	// write update to client
	outMsg.Init( msgBuf, sizeof( msgBuf ) );
	outMsg.BeginWriting();
	outMsg.WriteByte( GAME_RELIABLE_MESSAGE_REMAP_DECL );
	outMsg.WriteByte( type );
	outMsg.WriteLong( index );
	outMsg.WriteString( decl->GetName() );
	networkSystem->ServerSendReliableMessage( clientNum, outMsg );
}

/*
================
idGameLocal::ServerRemapDecl
================
*/
int idGameLocal::ServerRemapDecl( int clientNum, declType_t type, int index ) {

	// only implicit materials and sound shaders decls are used
	if ( type != DECL_MATERIAL && type != DECL_SOUND ) {
		return index;
	}

	if ( clientNum == -1 ) {
		for ( int i = 0; i < MAX_CLIENTS; i++ ) {
			ServerSendDeclRemapToClient( i, type, index );
		}
	} else {
		ServerSendDeclRemapToClient( clientNum, type, index );
	}
	return index;
}

/*
================
idGameLocal::ClientRemapDecl
================
*/
int idGameLocal::ClientRemapDecl( declType_t type, int index ) {

	// only implicit materials and sound shaders decls are used
	if ( type != DECL_MATERIAL && type != DECL_SOUND ) {
		return index;
	}

	// negative indexes are sometimes used for NULL decls
	if ( index < 0 ) {
		return index;
	}

	// make sure the index is valid
	if ( clientDeclRemap[localClientNum][(int)type].Num() == 0 ) {
		gameLocal.Error( "client received decl index %d before %s decl remap was initialized", index, declManager->GetDeclNameFromType( type ) );
		return -1;
	}
	if ( index >= clientDeclRemap[localClientNum][(int)type].Num() ) {
		gameLocal.Error( "client received unmapped %s decl index %d from server", declManager->GetDeclNameFromType( type ), index );
		return -1;
	}
	if ( clientDeclRemap[localClientNum][(int)type][index] == -1 ) {
		gameLocal.Error( "client received unmapped %s decl index %d from server", declManager->GetDeclNameFromType( type ), index );
		return -1;
	}
	return clientDeclRemap[localClientNum][type][index];
}

/*
================
idGameLocal::ServerAllowClient
================
*/
allowReply_t idGameLocal::ServerAllowClient( int numClients, const char *IP, const char *guid, const char *password, char reason[ MAX_STRING_CHARS ] ) {
	reason[0] = '\0';

	if ( !serverInfo.GetInt( "si_maxPlayers" ) ) {
		idStr::snPrintf( reason, MAX_STRING_CHARS, "#str_07140" );
		return ALLOW_NOTYET;
	}

	if ( numClients >= serverInfo.GetInt( "si_maxPlayers" ) ) {
		idStr::snPrintf( reason, MAX_STRING_CHARS, "#str_07141" );
		return ALLOW_NOTYET;
	}

	if ( !cvarSystem->GetCVarBool( "si_usepass" ) ) {
		return ALLOW_YES;
	}

	const char *pass = cvarSystem->GetCVarString( "g_password" );
	if ( pass[ 0 ] == '\0' ) {
		common->Warning( "si_usepass is set but g_password is empty" );
		cmdSystem->BufferCommandText( CMD_EXEC_NOW, "say si_usepass is set but g_password is empty" );
		// avoids silent misconfigured state
		idStr::snPrintf( reason, MAX_STRING_CHARS, "#str_07142" );
		return ALLOW_NOTYET;
	}

	if ( !idStr::Cmp( pass, password ) ) {
		return ALLOW_YES;
	}

	idStr::snPrintf( reason, MAX_STRING_CHARS, "#str_07143" );
	Printf( "Rejecting client %s from IP %s: invalid password\n", guid, IP );
	return ALLOW_BADPASS;
}

/*
================
idGameLocal::ServerClientConnect
================
*/
void idGameLocal::ServerClientConnect( int clientNum, const char *guid ) {
	// make sure no parasite entity is left
	if ( entities[ clientNum ] ) {
		common->DPrintf( "ServerClientConnect: remove old player entity\n" );
		delete entities[ clientNum ];
	}
	userInfo[ clientNum ].Clear();
	Printf( "client %d connected.\n", clientNum );
}

/*
================
idGameLocal::ServerClientBegin
================
*/
void idGameLocal::ServerClientBegin( int clientNum ) {
	idBitMsg	outMsg;
	byte		msgBuf[MAX_GAME_MESSAGE_SIZE];

	// initialize the decl remap
	InitClientDeclRemap( clientNum );

	// send message to initialize decl remap at the client (this is always the very first reliable game message)
	outMsg.Init( msgBuf, sizeof( msgBuf ) );
	outMsg.BeginWriting();
	outMsg.WriteByte( GAME_RELIABLE_MESSAGE_INIT_DECL_REMAP );
	networkSystem->ServerSendReliableMessage( clientNum, outMsg );

	// spawn the player
	SpawnPlayer( clientNum );

	// send message to spawn the player at the clients
	outMsg.Init( msgBuf, sizeof( msgBuf ) );
	outMsg.BeginWriting();
	outMsg.WriteByte( GAME_RELIABLE_MESSAGE_SPAWN_PLAYER );
	outMsg.WriteByte( clientNum );
	outMsg.WriteLong( spawnIds[ clientNum ] );
	networkSystem->ServerSendReliableMessage( -1, outMsg );
}

/*
================
idGameLocal::ServerClientDisconnect
================
*/
void idGameLocal::ServerClientDisconnect( int clientNum ) {
	int			i;
	idBitMsg	outMsg;
	byte		msgBuf[MAX_GAME_MESSAGE_SIZE];

	outMsg.Init( msgBuf, sizeof( msgBuf ) );
	outMsg.BeginWriting();
	outMsg.WriteByte( GAME_RELIABLE_MESSAGE_DELETE_ENT );
	outMsg.WriteBits( ( spawnIds[ clientNum ] << GENTITYNUM_BITS ) | clientNum, 32 ); // see GetSpawnId
	networkSystem->ServerSendReliableMessage( -1, outMsg );

	// free snapshots stored for this client
	FreeSnapshotsOlderThanSequence( clientNum, 0x7FFFFFFF );

	// free entity states stored for this client
	for ( i = 0; i < MAX_GENTITIES; i++ ) {
		if ( clientEntityStates[ clientNum ][ i ] ) {
			entityStateAllocator.Free( clientEntityStates[ clientNum ][ i ] );
			clientEntityStates[ clientNum ][ i ] = NULL;
		}
	}

	// clear the client PVS
	memset( clientPVS[ clientNum ], 0, sizeof( clientPVS[ clientNum ] ) );

	// delete the player entity
	delete entities[ clientNum ];
}

/*
================
idGameLocal::ServerWriteInitialReliableMessages

  Send reliable messages to initialize the client game up to a certain initial state.
================
*/
void idGameLocal::ServerWriteInitialReliableMessages( int clientNum ) {
	int			i;
	idBitMsg	outMsg;
	byte		msgBuf[MAX_GAME_MESSAGE_SIZE];
	entityNetEvent_t *event;

	// spawn players
	for ( i = 0; i < MAX_CLIENTS; i++ ) {
		if ( entities[i] == NULL || i == clientNum ) {
			continue;
		}
		outMsg.Init( msgBuf, sizeof( msgBuf ) );
		outMsg.BeginWriting( );
		outMsg.WriteByte( GAME_RELIABLE_MESSAGE_SPAWN_PLAYER );
		outMsg.WriteByte( i );
		outMsg.WriteLong( spawnIds[ i ] );
		networkSystem->ServerSendReliableMessage( clientNum, outMsg );
	}

	// send all saved events
	for ( event = savedEventQueue.Start(); event; event = event->next ) {
		outMsg.Init( msgBuf, sizeof( msgBuf ) );
		outMsg.BeginWriting();
		outMsg.WriteByte( GAME_RELIABLE_MESSAGE_EVENT );
		outMsg.WriteBits( event->spawnId, 32 );
		outMsg.WriteByte( event->event );
		outMsg.WriteLong( event->time );
		outMsg.WriteBits( event->paramsSize, idMath::BitsForInteger( MAX_EVENT_PARAM_SIZE ) );
		if ( event->paramsSize ) {
			outMsg.WriteData( event->paramsBuf, event->paramsSize );
		}

		networkSystem->ServerSendReliableMessage( clientNum, outMsg );
	}

	// update portals for opened doors
	int numPortals = gameRenderWorld->NumPortals();
	outMsg.Init( msgBuf, sizeof( msgBuf ) );
	outMsg.BeginWriting();
	outMsg.WriteByte( GAME_RELIABLE_MESSAGE_PORTALSTATES );
	outMsg.WriteLong( numPortals );
	for ( i = 0; i < numPortals; i++ ) {
		outMsg.WriteBits( gameRenderWorld->GetPortalState( (qhandle_t) (i+1) ) , NUM_RENDER_PORTAL_BITS );
	}
	networkSystem->ServerSendReliableMessage( clientNum, outMsg );
}

/*
================
idGameLocal::SaveEntityNetworkEvent
================
*/
void idGameLocal::SaveEntityNetworkEvent( const idEntity *ent, int eventId, const idBitMsg *msg ) {
	entityNetEvent_t *event;

	event = savedEventQueue.Alloc();
	event->spawnId = GetSpawnId( ent );
	event->event = eventId;
	event->time = time;
	if ( msg ) {
		event->paramsSize = msg->GetSize();
		memcpy( event->paramsBuf, msg->GetData(), msg->GetSize() );
	} else {
		event->paramsSize = 0;
	}

	savedEventQueue.Enqueue( event, idEventQueue::OUTOFORDER_IGNORE );
}

/*
================
idGameLocal::FreeSnapshotsOlderThanSequence
================
*/
void idGameLocal::FreeSnapshotsOlderThanSequence( int clientNum, int sequence ) {
	snapshot_t *snapshot, *lastSnapshot, *nextSnapshot;
	entityState_t *state;

	for ( lastSnapshot = NULL, snapshot = clientSnapshots[clientNum]; snapshot; snapshot = nextSnapshot ) {
		nextSnapshot = snapshot->next;
		if ( snapshot->sequence < sequence ) {
			for ( state = snapshot->firstEntityState; state; state = snapshot->firstEntityState ) {
				snapshot->firstEntityState = snapshot->firstEntityState->next;
				entityStateAllocator.Free( state );
			}
			if ( lastSnapshot ) {
				lastSnapshot->next = snapshot->next;
			} else {
				clientSnapshots[clientNum] = snapshot->next;
			}
			snapshotAllocator.Free( snapshot );
		} else {
			lastSnapshot = snapshot;
		}
	}
}

/*
================
idGameLocal::ApplySnapshot
================
*/
bool idGameLocal::ApplySnapshot( int clientNum, int sequence ) {
	snapshot_t *snapshot, *lastSnapshot, *nextSnapshot;
	entityState_t *state;

	FreeSnapshotsOlderThanSequence( clientNum, sequence );

	for ( lastSnapshot = NULL, snapshot = clientSnapshots[clientNum]; snapshot; snapshot = nextSnapshot ) {
		nextSnapshot = snapshot->next;
		if ( snapshot->sequence == sequence ) {
			for ( state = snapshot->firstEntityState; state; state = state->next ) {
				if ( clientEntityStates[clientNum][state->entityNumber] ) {
					entityStateAllocator.Free( clientEntityStates[clientNum][state->entityNumber] );
				}
				clientEntityStates[clientNum][state->entityNumber] = state;
			}
			memcpy( clientPVS[clientNum], snapshot->pvs, sizeof( snapshot->pvs ) );
			if ( lastSnapshot ) {
				lastSnapshot->next = nextSnapshot;
			} else {
				clientSnapshots[clientNum] = nextSnapshot;
			}
			snapshotAllocator.Free( snapshot );
			return true;
		} else {
			lastSnapshot = snapshot;
		}
	}

	return false;
}

/*
================
idGameLocal::WriteGameStateToSnapshot
================
*/
void idGameLocal::WriteGameStateToSnapshot( idBitMsgDelta &msg ) const {
	int i;

	for( i = 0; i < MAX_GLOBAL_SHADER_PARMS; i++ ) {
		msg.WriteFloat( globalShaderParms[i] );
	}
}

/*
================
idGameLocal::ReadGameStateFromSnapshot
================
*/
void idGameLocal::ReadGameStateFromSnapshot( const idBitMsgDelta &msg ) {
	int i;

	for( i = 0; i < MAX_GLOBAL_SHADER_PARMS; i++ ) {
		globalShaderParms[i] = msg.ReadFloat();
	}
}

/*
================
idGameLocal::ServerWriteSnapshot

  Write a snapshot of the current game state for the given client.
================
*/
void idGameLocal::ServerWriteSnapshot( int clientNum, int sequence, idBitMsg &msg, byte *clientInPVS, int numPVSClients ) {
}

/*
================
idGameLocal::ServerApplySnapshot
================
*/
bool idGameLocal::ServerApplySnapshot( int clientNum, int sequence ) {
	return ApplySnapshot( clientNum, sequence );
}

/*
================
idGameLocal::NetworkEventWarning
================
*/
void idGameLocal::NetworkEventWarning( const entityNetEvent_t *event, const char *fmt, ... ) {
	char buf[1024];
	int length = 0;
	va_list argptr;

	int entityNum	= event->spawnId & ( ( 1 << GENTITYNUM_BITS ) - 1 );
	int id			= event->spawnId >> GENTITYNUM_BITS;

	length += idStr::snPrintf( buf+length, sizeof(buf)-1-length, "event %d for entity %d %d: ", event->event, entityNum, id );
	va_start( argptr, fmt );
	length = idStr::vsnPrintf( buf+length, sizeof(buf)-1-length, fmt, argptr );
	va_end( argptr );
	idStr::Append( buf, sizeof(buf), "\n" );

	common->DWarning( buf );
}

/*
================
idGameLocal::ServerProcessEntityNetworkEventQueue
================
*/
void idGameLocal::ServerProcessEntityNetworkEventQueue( void ) {
	idEntity			*ent;
	entityNetEvent_t	*event;
	idBitMsg			eventMsg;

	while ( eventQueue.Start() ) {
		event = eventQueue.Start();

		if ( event->time > time ) {
			break;
		}

		idEntityPtr< idEntity > entPtr;
			
		if( !entPtr.SetSpawnId( event->spawnId ) ) {
			NetworkEventWarning( event, "Entity does not exist any longer, or has not been spawned yet." );
		} else {
			ent = entPtr.GetEntity();
			assert( ent );

			eventMsg.Init( event->paramsBuf, sizeof( event->paramsBuf ) );
			eventMsg.SetSize( event->paramsSize );
			eventMsg.BeginReading();
		}

		entityNetEvent_t* freedEvent = eventQueue.Dequeue();
		assert( freedEvent == event );
		eventQueue.Free( event );
	}
}

/*
================
idGameLocal::ServerSendChatMessage
================
*/
void idGameLocal::ServerSendChatMessage( int to, const char *name, const char *text ) {
	idBitMsg outMsg;
	byte msgBuf[ MAX_GAME_MESSAGE_SIZE ];

	outMsg.Init( msgBuf, sizeof( msgBuf ) );
	outMsg.BeginWriting();
	outMsg.WriteByte( GAME_RELIABLE_MESSAGE_CHAT );
	outMsg.WriteString( name );
	outMsg.WriteString( text, -1, false );
	networkSystem->ServerSendReliableMessage( to, outMsg );

	if ( to == -1 || to == localClientNum ) {
	}
}

/*
================
idGameLocal::ServerProcessReliableMessage
================
*/
void idGameLocal::ServerProcessReliableMessage( int clientNum, const idBitMsg &msg ) {
}

/*
================
idGameLocal::ClientShowSnapshot
================
*/
void idGameLocal::ClientShowSnapshot( int clientNum ) const {
	int baseBits;
	idEntity *ent;
	idPlayer *player;
	idMat3 viewAxis;
	idBounds viewBounds;
	entityState_t *base;

	if ( !net_clientShowSnapshot.GetInteger() ) {
		return;
	}

	player = static_cast<idPlayer *>( entities[clientNum] );
	if ( !player ) {
		return;
	}

	viewAxis = player->viewAngles.ToMat3();
	viewBounds = player->GetPhysics()->GetAbsBounds().Expand( net_clientShowSnapshotRadius.GetFloat() );

	for( ent = snapshotEntities.Next(); ent != NULL; ent = ent->snapshotNode.Next() ) {

		if ( net_clientShowSnapshot.GetInteger() == 1 && ent->snapshotBits == 0 ) {
			continue;
		}

		const idBounds &entBounds = ent->GetPhysics()->GetAbsBounds();

		if ( !entBounds.IntersectsBounds( viewBounds ) ) {
			continue;
		}

		base = clientEntityStates[clientNum][ent->entityNumber];
		if ( base ) {
			baseBits = base->state.GetNumBitsWritten();
		} else {
			baseBits = 0;
		}

		if ( net_clientShowSnapshot.GetInteger() == 2 && baseBits == 0 ) {
			continue;
		}

		gameRenderWorld->DebugBounds( colorGreen, entBounds );
		gameRenderWorld->DrawText( va( "%d: %s (%d,%d bytes of %d,%d)\n", ent->entityNumber,
						ent->name.c_str(), ent->snapshotBits >> 3, ent->snapshotBits & 7, baseBits >> 3, baseBits & 7 ),
							entBounds.GetCenter(), 0.1f, colorWhite, viewAxis, 1 );
	}
}

/*
================
idGameLocal::ClientReadSnapshot
================
*/
void idGameLocal::ClientReadSnapshot( int clientNum, int sequence, const int gameFrame, const int gameTime, const int dupeUsercmds, const int aheadOfServer, const idBitMsg &msg ) {
}

/*
================
idGameLocal::ClientApplySnapshot
================
*/
bool idGameLocal::ClientApplySnapshot( int clientNum, int sequence ) {
	return ApplySnapshot( clientNum, sequence );
}

/*
================
idGameLocal::ClientProcessEntityNetworkEventQueue
================
*/
void idGameLocal::ClientProcessEntityNetworkEventQueue( void ) {
	idEntity			*ent;
	entityNetEvent_t	*event;
	idBitMsg			eventMsg;

	while( eventQueue.Start() ) {
		event = eventQueue.Start();

		// only process forward, in order
		if ( event->time > time ) {
			break;
		}

		idEntityPtr< idEntity > entPtr;
			
		if( !entPtr.SetSpawnId( event->spawnId ) ) {
			if( !gameLocal.entities[ event->spawnId & ( ( 1 << GENTITYNUM_BITS ) - 1 ) ] ) {
				// if new entity exists in this position, silently ignore
				NetworkEventWarning( event, "Entity does not exist any longer, or has not been spawned yet." );
			}
		} else {
			ent = entPtr.GetEntity();
			assert( ent );

			eventMsg.Init( event->paramsBuf, sizeof( event->paramsBuf ) );
			eventMsg.SetSize( event->paramsSize );
			eventMsg.BeginReading();
		}

		entityNetEvent_t* freedEvent = eventQueue.Dequeue();
		assert( freedEvent == event );
		eventQueue.Free( event );
	}
}

/*
================
idGameLocal::ClientProcessReliableMessage
================
*/
void idGameLocal::ClientProcessReliableMessage( int clientNum, const idBitMsg &msg ) {
}

/*
================
idGameLocal::ClientPrediction
================
*/
gameReturn_t idGameLocal::ClientPrediction( int clientNum, const usercmd_t *clientCmds, bool lastPredictFrame ) {
	gameReturn_t ret;
	ret.sessionCommand[ 0 ] = '\0';
	return ret;
}

/*
===============
idGameLocal::Tokenize
===============
*/
void idGameLocal::Tokenize( idStrList &out, const char *in ) {
	char buf[ MAX_STRING_CHARS ];
	char *token, *next;
	
	idStr::Copynz( buf, in, MAX_STRING_CHARS );
	token = buf;
	next = strchr( token, ';' );
	while ( token ) {
		if ( next ) {
			*next = '\0';
		}
		idStr::ToLower( token );
		out.Append( token );
		if ( next ) {
			token = next + 1;
			next = strchr( token, ';' );
		} else {
			token = NULL;
		}		
	}
}

/*
===============
idGameLocal::DownloadRequest
===============
*/
bool idGameLocal::DownloadRequest( const char *IP, const char *guid, const char *paks, char urls[ MAX_STRING_CHARS ] ) {
	if ( !cvarSystem->GetCVarInteger( "net_serverDownload" ) ) {
		return false;
	}
	if ( cvarSystem->GetCVarInteger( "net_serverDownload" ) == 1 ) {
		// 1: single URL redirect
		if ( !strlen( cvarSystem->GetCVarString( "si_serverURL" ) ) ) {
			common->Warning( "si_serverURL not set" );
			return false;
		}
		idStr::snPrintf( urls, MAX_STRING_CHARS, "1;%s", cvarSystem->GetCVarString( "si_serverURL" ) );
		return true;
	} else {
		// 2: table of pak URLs
		// first token is the game pak if request, empty if not requested by the client
		// there may be empty tokens for paks the server couldn't pinpoint - the order matters
		idStr reply = "2;";
		idStrList dlTable, pakList;
		int i, j;

		Tokenize( dlTable, cvarSystem->GetCVarString( "net_serverDlTable" ) );
		Tokenize( pakList, paks );

		for ( i = 0; i < pakList.Num(); i++ ) {
			if ( i > 0 ) {
				reply += ";";
			}
			if ( pakList[ i ][ 0 ] == '\0' ) {
 				if ( i == 0 ) {
					// pak 0 will always miss when client doesn't ask for game bin
					common->DPrintf( "no game pak request\n" );
				} else {
					common->DPrintf( "no pak %d\n", i );
				}
				continue;
			}
			for ( j = 0; j < dlTable.Num(); j++ ) {
				if ( !fileSystem->FilenameCompare( pakList[ i ], dlTable[ j ] ) ) {
					break;
				}
			}
			if ( j == dlTable.Num() ) {
				common->Printf( "download for %s: pak not matched: %s\n", IP, pakList[ i ].c_str() );
			} else {
				idStr url = cvarSystem->GetCVarString( "net_serverDlBaseURL" );
				url.AppendPath( dlTable[ j ] );
				reply += url;
				common->DPrintf( "download for %s: %s\n", IP, url.c_str() );
			}
		}
		
		idStr::Copynz( urls, reply, MAX_STRING_CHARS );
		return true;
	}
//	return false;	// sikk - warning C4702: unreachable code
}

/*
===============
idEventQueue::Alloc
===============
*/
entityNetEvent_t* idEventQueue::Alloc() {
	entityNetEvent_t* event = eventAllocator.Alloc();
	event->prev = NULL;
	event->next = NULL;
	return event;
}

/*
===============
idEventQueue::Free
===============
*/
void idEventQueue::Free( entityNetEvent_t *event ) {
	// should only be called on an unlinked event!
	assert( !event->next && !event->prev );
	eventAllocator.Free( event );
}

/*
===============
idEventQueue::Shutdown
===============
*/
void idEventQueue::Shutdown() {
	eventAllocator.Shutdown();
	this->Init();
}

/*
===============
idEventQueue::Init
===============
*/
void idEventQueue::Init( void ) {
	start = NULL;
	end = NULL;
}

/*
===============
idEventQueue::Dequeue
===============
*/
entityNetEvent_t* idEventQueue::Dequeue( void ) {
	entityNetEvent_t* event = start;
	if ( !event ) {
		return NULL;
	}

	start = start->next;

	if ( !start ) {
		end = NULL;
	} else {
		start->prev = NULL;
	}

	event->next = NULL;
	event->prev = NULL;

	return event;
}

/*
===============
idEventQueue::RemoveLast
===============
*/
entityNetEvent_t* idEventQueue::RemoveLast( void ) {
	entityNetEvent_t *event = end;
	if ( !event ) {
		return NULL;
	}

	end = event->prev;

	if ( !end ) {
		start = NULL;
	} else {
		end->next = NULL;		
	}

	event->next = NULL;
	event->prev = NULL;

	return event;
}

/*
===============
idEventQueue::Enqueue
===============
*/
void idEventQueue::Enqueue( entityNetEvent_t *event, outOfOrderBehaviour_t behaviour ) {
	if ( behaviour == OUTOFORDER_DROP ) {
		// go backwards through the queue and determine if there are
		// any out-of-order events
		while ( end && end->time > event->time ) {
			entityNetEvent_t *outOfOrder = RemoveLast();
			common->DPrintf( "WARNING: new event with id %d ( time %d ) caused removal of event with id %d ( time %d ), game time = %d.\n", event->event, event->time, outOfOrder->event, outOfOrder->time, gameLocal.time );
			Free( outOfOrder );
		}
	} else if ( behaviour == OUTOFORDER_SORT && end ) {
		// NOT TESTED -- sorting out of order packets hasn't been
		//				 tested yet... wasn't strictly necessary for
		//				 the patch fix.
		entityNetEvent_t *cur = end;
		// iterate until we find a time < the new event's
		while ( cur && cur->time > event->time ) {
			cur = cur->prev;
		}
		if ( !cur ) {
			// add to start
			event->next = start;
			event->prev = NULL;
			start = event;
		} else {
			// insert
			event->prev = cur;
			event->next = cur->next;
			cur->next = event;
		}
		return;
	} 

	// add the new event
	event->next = NULL;
	event->prev = NULL;

	if ( end ) {
		end->next = event;
		event->prev = end;
	} else {
		start = event;
	}
	end = event;
}
