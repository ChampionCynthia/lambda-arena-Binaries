//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Egon
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "NPCEvent.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "Sprite.h"
#include "Beam_Shared.h"
#include "Takedamageinfo.h"
//#include "basecombatcharacter.h"
//#include "AI_BaseNPC.h"

#include "gamerules.h"
#include "in_buttons.h"

#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "IEffects.h"

#ifdef CLIENT_DLL
#include "c_hl2mp_player.h"
#include "ClientEffectPrecacheSystem.h"
#include "c_te_effect_dispatch.h"
#include "prediction.h"
#else
#include "hl2mp_player.h"
#include "te_effect_dispatch.h"
#include "..\server\ilagcompensationmanager.h"
#endif

#ifndef CLIENT_DLL
#include "soundent.h"
#include "game.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


enum EGON_FIRESTATE { FIRE_OFF, FIRE_STARTUP, FIRE_CHARGE };

#define EGON_PULSE_INTERVAL			0.1
#define EGON_DISCHARGE_INTERVAL		0.1

#define EGON_BEAM_SPRITE		"sprites/xbeam1.vmt"

ConVar la_weapon_egon_dmg("la_weapon_egon_dmg", "10", FCVAR_REPLICATED | FCVAR_NOTIFY, "Gluon Gun damage. Default: 10");

//-----------------------------------------------------------------------------
// CWeaponEgon
//-----------------------------------------------------------------------------

#ifdef CLIENT_DLL
#define CWeaponEgon C_WeaponEgon
#endif

class CWeaponEgon : public CBaseHL2MPCombatWeapon
{
public:

	DECLARE_CLASS(CWeaponEgon, CBaseHL2MPCombatWeapon);
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

    CWeaponEgon(void);

	virtual bool	Deploy( void );
	void	PrimaryAttack( void );
    virtual void    Precache( void );
    
	void	SecondaryAttack( void )
	{
		PrimaryAttack();
	}

	void	WeaponIdle( void );
	bool	Holster( CBaseCombatWeapon *pSwitchingTo = NULL );

    //	DECLARE_SERVERCLASS();
    //	DECLARE_DATADESC();
	DECLARE_ACTTABLE();

private:
	bool	HasAmmo( void );
	void	UseAmmo( int count );
	void	Attack( void );
	void	EndAttack( void );
	void	Fire( const Vector &vecOrigSrc, const Vector &vecDir );
	void	UpdateEffect( const Vector &startPoint, const Vector &endPoint );
	void	CreateEffect( void );
	void	DestroyEffect( void );

	CNetworkVar(EGON_FIRESTATE, m_fireState);
	CNetworkVar(float, m_flAmmoUseTime);	// since we use < 1 point of ammo per update, we subtract ammo on a timer.
	float				m_flShakeTime;
	float				m_flStartFireTime;
	float				m_flDmgTime;

	//CNetworkHandle(CBeam, m_hBeam);
	//CNetworkHandle(CBeam, m_hNoise);
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponEgon, DT_WeaponEgon );

BEGIN_NETWORK_TABLE( CWeaponEgon, DT_WeaponEgon )
#ifdef CLIENT_DLL
RecvPropInt(RECVINFO(m_fireState)),
RecvPropFloat(RECVINFO(m_flAmmoUseTime)),
//RecvPropEHandle(RECVINFO(m_hBeam)),
//RecvPropEHandle(RECVINFO(m_hNoise)),
#else
SendPropInt(SENDINFO(m_fireState)),
SendPropFloat(SENDINFO(m_flAmmoUseTime)),
//SendPropEHandle(SENDINFO(m_hBeam)),
//SendPropEHandle(SENDINFO(m_hNoise)),
#endif
END_NETWORK_TABLE()    

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponEgon )
DEFINE_PRED_FIELD(m_fireState, FIELD_INTEGER, FTYPEDESC_INSENDTABLE),
DEFINE_PRED_FIELD(m_flAmmoUseTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE),
//DEFINE_PRED_FIELD(m_hBeam, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK),
//DEFINE_PRED_FIELD(m_hNoise, FIELD_EHANDLE, FTYPEDESC_INSENDTABLE | FTYPEDESC_NOERRORCHECK),
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS( weapon_egon, CWeaponEgon );
PRECACHE_WEAPON_REGISTER( weapon_egon );

acttable_t	CWeaponEgon::m_acttable[] =
{
	{ ACT_HL2MP_IDLE, ACT_HL2MP_IDLE_AR2, false },
	{ ACT_HL2MP_RUN, ACT_HL2MP_RUN_AR2, false },
	{ ACT_HL2MP_IDLE_CROUCH, ACT_HL2MP_IDLE_CROUCH_AR2, false },
	{ ACT_HL2MP_WALK_CROUCH, ACT_HL2MP_WALK_CROUCH_AR2, false },
	{ ACT_HL2MP_GESTURE_RANGE_ATTACK, ACT_HL2MP_GESTURE_RANGE_ATTACK_AR2, false },
	{ ACT_HL2MP_GESTURE_RELOAD, ACT_HL2MP_GESTURE_RELOAD_AR2, false },
	{ ACT_HL2MP_JUMP, ACT_HL2MP_JUMP_AR2, false },
	{ ACT_RANGE_ATTACK1, ACT_RANGE_ATTACK_AR2, false },
};

IMPLEMENT_ACTTABLE(CWeaponEgon);

/*
IMPLEMENT_SERVERCLASS_ST( CWeaponEgon, DT_WeaponEgon )
END_SEND_TABLE()
*/

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponEgon::CWeaponEgon( void )
{
	m_bReloadsSingly	= false;
	m_bFiresUnderwater	= true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponEgon::Precache( void )
{
	PrecacheModel( EGON_BEAM_SPRITE );

	BaseClass::Precache();
}

bool CWeaponEgon::Deploy( void )
{
	m_fireState = FIRE_OFF;

	return BaseClass::Deploy();
}

bool CWeaponEgon::HasAmmo( void )
{
	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());
	if ( !pPlayer )
	{
		return false;
	}

	if ( pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
		return false;

	return true;
}

void CWeaponEgon::UseAmmo( int count )
{
	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());
	if ( !pPlayer )
	{
		return;
	}

	if ( pPlayer->GetAmmoCount( m_iPrimaryAmmoType ) >= count )
		pPlayer->RemoveAmmo( count, m_iPrimaryAmmoType );
	else
		pPlayer->RemoveAmmo( pPlayer->GetAmmoCount( m_iPrimaryAmmoType ), m_iPrimaryAmmoType );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponEgon::PrimaryAttack( void )
{
	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());
	if ( !pPlayer )
	{
		return;
	}

	// don't fire underwater
	if ( pPlayer->GetWaterLevel() == 3 )
	{
		if ( m_fireState != FIRE_OFF )
		{
			EndAttack();
		}
		else
		{
			WeaponSound( EMPTY );
		}

		m_flNextPrimaryAttack = gpGlobals->curtime + 0.5;
		m_flNextSecondaryAttack = gpGlobals->curtime + 0.5;
		return;
	}

	Vector vecAiming	= pPlayer->GetAutoaimVector( 0 );
	Vector vecSrc		= pPlayer->Weapon_ShootPosition( );

	switch( m_fireState )
	{
		case FIRE_OFF:
		{
			if ( !HasAmmo() )
			{
				m_flNextPrimaryAttack = gpGlobals->curtime + 0.25;
				m_flNextSecondaryAttack = gpGlobals->curtime + 0.25;
				WeaponSound( EMPTY );
				return;
			}

			m_flAmmoUseTime = gpGlobals->curtime;// start using ammo ASAP.

			WeaponSound(SINGLE);

			SendWeaponAnim( ACT_VM_PRIMARYATTACK );
						
			m_flShakeTime = 0;
			m_flStartFireTime = gpGlobals->curtime;

			SetWeaponIdleTime( gpGlobals->curtime + 0.1 );

			m_flDmgTime = gpGlobals->curtime + EGON_PULSE_INTERVAL;
			m_fireState = FIRE_STARTUP;
		}
		break;

		case FIRE_STARTUP:
		{
			if (!HasAmmo())
			{
				EndAttack();
				m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;
				m_flNextSecondaryAttack = gpGlobals->curtime + 1.0;
				return;
			}

			Fire(vecSrc, vecAiming);

			if ( gpGlobals->curtime >= ( m_flStartFireTime + 2.0 ) )
			{
				WeaponSound(SPECIAL2);

				m_fireState = FIRE_CHARGE;
			}
		}
		case FIRE_CHARGE:
		{
			if ( !HasAmmo() )
			{
				EndAttack();
				m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;
				m_flNextSecondaryAttack = gpGlobals->curtime + 1.0;
				return;
			}

			Fire(vecSrc, vecAiming);
		}
		break;
	}
}

void CWeaponEgon::Fire( const Vector &vecOrigSrc, const Vector &vecDir )
{
	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());
	if ( !pPlayer )
	{
		return;
	}

	//CSoundEnt::InsertSound( SOUND_COMBAT, GetAbsOrigin(), 450, 0.1 );
    //WeaponSound( SINGLE );

	Vector vecDest	= vecOrigSrc + (vecDir * MAX_TRACE_LENGTH);

#ifndef CLIENT_DLL
	lagcompensation->StartLagCompensation(pPlayer, pPlayer->GetCurrentCommand());
#endif

	trace_t	tr;
	UTIL_TraceLine( vecOrigSrc, vecDest, MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr );

	if (!tr.allsolid && pPlayer->IsAlive())
	{
		CBaseEntity *pEntity = tr.m_pEnt;

		if ((m_flDmgTime < gpGlobals->curtime) && (pEntity != NULL))
		{
			// wide mode does damage to the ent, and radius damage
			if (pEntity->m_takedamage != DAMAGE_NO)
			{
				ClearMultiDamage();
				CTakeDamageInfo info(this, pPlayer, la_weapon_egon_dmg.GetFloat() * g_pGameRules->GetDamageMultiplier(), DMG_ENERGYBEAM | DMG_ALWAYSGIB);
				CalculateMeleeDamageForce(&info, vecDir, tr.endpos);
				pEntity->DispatchTraceAttack(info, vecDir, &tr);
				ApplyMultiDamage();
			}

			if (g_pGameRules->IsMultiplayer())
			{
				// radius damage a little more potent in multiplayer.
#ifndef CLIENT_DLL
				RadiusDamage(CTakeDamageInfo(this, pPlayer, la_weapon_egon_dmg.GetFloat() * g_pGameRules->GetDamageMultiplier() / 4, DMG_ENERGYBEAM | DMG_BLAST | DMG_ALWAYSGIB), tr.endpos, 128, CLASS_NONE, NULL);
#endif
			}

			if (g_pGameRules->IsMultiplayer())
			{
				//multiplayer uses 5 ammo/second
				if (gpGlobals->curtime >= m_flAmmoUseTime)
				{
					UseAmmo(1);
					m_flAmmoUseTime = gpGlobals->curtime + 0.2;
				}
			}
			else
			{
				// Wide mode uses 10 charges per second in single player
				if (gpGlobals->curtime >= m_flAmmoUseTime)
				{
					UseAmmo(1);
					m_flAmmoUseTime = gpGlobals->curtime + 0.1;
				}
			}

			m_flDmgTime = gpGlobals->curtime + EGON_DISCHARGE_INTERVAL;
			if (m_flShakeTime < gpGlobals->curtime)
			{
#ifndef CLIENT_DLL
				UTIL_ScreenShake(tr.endpos, 5.0, 150.0, 0.75, 250.0, SHAKE_START);
#endif
				m_flShakeTime = gpGlobals->curtime + 1.5;
			}
		}

		Vector vecUp, vecRight;
		QAngle angDir;

		VectorAngles(vecDir, angDir);
		AngleVectors(angDir, NULL, &vecRight, &vecUp);

		Vector tmpSrc = vecOrigSrc + (vecUp * -8) + (vecRight * 3);
		UpdateEffect(tmpSrc, tr.endpos);
	}

#ifndef CLIENT_DLL         
	lagcompensation->FinishLagCompensation(pPlayer);
#endif
}

void CWeaponEgon::UpdateEffect( const Vector &startPoint, const Vector &endPoint )
{
	/*
	if ( !m_hBeam )
	{
		CreateEffect();
	}

	if ( m_hBeam )
	{
		m_hBeam->SetStartPos( endPoint );
	}
	*/

	// [Striker] Need to rewrite this... Not sure how. This eats up too much bandwidth.
	DispatchParticleEffect("egon_beam", endPoint, PATTACH_POINT_FOLLOW, this, "muzzle", false);

	/*
	if ( m_hNoise )
	{
		m_hNoise->SetStartPos( endPoint );
	}
	*/
}

void CWeaponEgon::CreateEffect( void )
{
	/*
#ifndef CLIENT_DLL    
	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());
	if ( !pPlayer )
	{
		return;
	}

	DestroyEffect();

	m_hBeam = CBeam::BeamCreate( EGON_BEAM_SPRITE, 7.0 );
	m_hBeam->PointEntInit( GetAbsOrigin(), this );
	m_hBeam->SetBeamFlags( FBEAM_SINENOISE );
	m_hBeam->SetEndAttachment( LookupAttachment("Muzzle") );
	m_hBeam->AddSpawnFlags( SF_BEAM_TEMPORARY );	// Flag these to be destroyed on save/restore or level transition
	m_hBeam->SetOwnerEntity( pPlayer );
	m_hBeam->SetScrollRate( 10 );
	m_hBeam->SetBrightness( 255 );
	m_hBeam->SetColor( 50, 50, 255 );
	m_hBeam->SetNoise( 0.2 );
	m_hBeam->SetPredictionEligible(true);

	m_hNoise = CBeam::BeamCreate( EGON_BEAM_SPRITE, 10.0 );
	m_hNoise->PointEntInit( GetAbsOrigin(), this );
	m_hNoise->SetEndAttachment( LookupAttachment("Muzzle") );
	m_hNoise->AddSpawnFlags( SF_BEAM_TEMPORARY );
	m_hNoise->SetOwnerEntity( pPlayer );
	m_hNoise->SetScrollRate( 25 );
	m_hNoise->SetBrightness( 255 );
	m_hNoise->SetColor( 50, 50, 255 );
	m_hNoise->SetNoise( 1.0 );
	m_hNoise->SetPredictionEligible(true);
#endif   
	*/
}


void CWeaponEgon::DestroyEffect( void )
{
	/*
#ifndef CLIENT_DLL    
	if ( m_hBeam )
	{
		UTIL_Remove( m_hBeam );
		m_hBeam = NULL;
	}
	if ( m_hNoise )
	{
		UTIL_Remove( m_hNoise );
		m_hNoise = NULL;
	}
#endif
	*/
}

void CWeaponEgon::EndAttack( void )
{
	StopWeaponSound(SPECIAL2);
	
	if ( m_fireState != FIRE_OFF )
	{
		WeaponSound(SPECIAL1);
	}

	SetWeaponIdleTime( gpGlobals->curtime + 2.0 );
	m_flNextPrimaryAttack = gpGlobals->curtime + 0.5;
	m_flNextSecondaryAttack = gpGlobals->curtime + 0.5;

	m_fireState = FIRE_OFF;

	DestroyEffect();
}

bool CWeaponEgon::Holster( CBaseCombatWeapon *pSwitchingTo )
{
    EndAttack();

	return BaseClass::Holster( pSwitchingTo );
}

void CWeaponEgon::WeaponIdle( void )
{
	if ( !HasWeaponIdleTimeElapsed() )
		return;

	if ( m_fireState != FIRE_OFF )
		 EndAttack();
	
	int iAnim;

	float flRand = random->RandomFloat( 0,1 );
	float flIdleTime;
	if ( flRand <= 0.5 )
	{
		iAnim = ACT_VM_IDLE;
		flIdleTime = gpGlobals->curtime + random->RandomFloat( 10, 15 );
	}
	else 
	{
		iAnim = ACT_VM_FIDGET;
		flIdleTime = gpGlobals->curtime + 3.0;
	}

	SendWeaponAnim( iAnim );

	SetWeaponIdleTime( flIdleTime );
}