//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CS_GAMEVARS_SHARED_H
#define CS_GAMEVARS_SHARED_H
#ifdef _WIN32
#pragma once
#endif

#include "convar.h"

extern ConVar mp_forcecamera;
extern ConVar mp_allowspectators;
extern ConVar friendlyfire;
extern ConVar mp_fadetoblack;

// [Striker] Lambda Arena Shared Vars
extern ConVar la_sv_hitmarkers;
extern ConVar la_sv_damagepopup;

#endif // CS_GAMEVARS_SHARED_H
