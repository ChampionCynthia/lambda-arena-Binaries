//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Gauss
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "NPCEvent.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "takedamageinfo.h"
#include "gamerules.h"
#include "in_buttons.h"

#ifndef CLIENT_DLL
#include "soundent.h"
#include "game.h"
#endif

#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "soundenvelope.h"
#include "shake.h"
#include "effect_dispatch_data.h"
#include "AmmoDef.h"

#ifdef CLIENT_DLL
#include "c_hl2mp_player.h"
#include "c_te_effect_dispatch.h"
#else
#include "hl2mp_player.h"
#include "te_effect_dispatch.h"
#include "..\server\ilagcompensationmanager.h"
#endif
#include "soundemittersystem/isoundemittersystembase.h"

#ifdef CLIENT_DLL
#define CWeaponGauss C_WeaponGauss
#endif

//-----------------------------------------------------------------------------
// CWeaponGauss
//-----------------------------------------------------------------------------


class CWeaponGauss : public CBaseHL2MPCombatWeapon
{
	DECLARE_CLASS(CWeaponGauss, CBaseHL2MPCombatWeapon);
public:

	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponGauss(void);

	void	Precache(void);
	void	PrimaryAttack(void);
	void	SecondaryAttack(void);
	void	WeaponIdle(void);
	void	AddViewKick(void);
	bool	Deploy(void);
	bool	Holster(CBaseCombatWeapon *pSwitchingTo = NULL);

	DECLARE_DATADESC();
	DECLARE_ACTTABLE();

private:
	void	StopSpinSound(void);
	float	GetFullChargeTime(void);
	void	CalculateChargePercent(void);
	void	StartFire(void);
	void	Fire(Vector vecOrigSrc, Vector vecDir, float flDamage);

private:
	//	int			m_nAttackState;
	//	bool		m_bPrimaryFire;
	CNetworkVar(int, m_nAttackState);
	CNetworkVar(bool, m_bPrimaryFire);

	CNetworkVar(float, m_flNextAmmoBurn);

	float m_flStartCharge;

	int m_nAmmoDrained;
	float m_flChargePercent;

	CSoundPatch	*m_sndCharge;
};

IMPLEMENT_NETWORKCLASS_ALIASED(WeaponGauss, DT_WeaponGauss);

BEGIN_NETWORK_TABLE(CWeaponGauss, DT_WeaponGauss)
#ifdef CLIENT_DLL
RecvPropInt(RECVINFO(m_nAttackState)),
RecvPropBool(RECVINFO(m_bPrimaryFire)),
RecvPropFloat(RECVINFO(m_flNextAmmoBurn)),
#else
SendPropInt(SENDINFO(m_nAttackState)),
SendPropBool(SENDINFO(m_bPrimaryFire)),
SendPropFloat(SENDINFO(m_flNextAmmoBurn)),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA(CWeaponGauss)
#ifdef CLIENT_DLL
DEFINE_PRED_FIELD(m_nAttackState, FIELD_INTEGER, FTYPEDESC_INSENDTABLE),
DEFINE_PRED_FIELD(m_bPrimaryFire, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE),
DEFINE_PRED_FIELD(m_flNextAmmoBurn, FIELD_FLOAT, FTYPEDESC_INSENDTABLE),
#endif
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS(weapon_gauss, CWeaponGauss);

PRECACHE_WEAPON_REGISTER(weapon_gauss);

BEGIN_DATADESC(CWeaponGauss)
DEFINE_FIELD(m_nAttackState, FIELD_INTEGER),
DEFINE_FIELD(m_bPrimaryFire, FIELD_BOOLEAN),
DEFINE_SOUNDPATCH(m_sndCharge),
END_DATADESC()

acttable_t	CWeaponGauss::m_acttable[] =
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

IMPLEMENT_ACTTABLE(CWeaponGauss);

ConVar la_weapon_gauss_dmg("la_weapon_gauss_dmg", "20", FCVAR_REPLICATED | FCVAR_NOTIFY, "Tau Cannon primary fire damage. Default: 20");
ConVar la_weapon_gauss_recoil_min("la_weapon_gauss_recoil_min", "125", FCVAR_REPLICATED | FCVAR_NOTIFY, "Tau Cannon recoil minimum speed in HU/s. Default: 125");
ConVar la_weapon_gauss_recoil_max("la_weapon_gauss_recoil_max", "1000", FCVAR_REPLICATED | FCVAR_NOTIFY, "Tau Cannon recoil maximum speed in HU/s. Default: 1000");
ConVar la_weapon_gauss_chargedmg_min("la_weapon_gauss_chargedmg_min", "25", FCVAR_REPLICATED | FCVAR_NOTIFY, "Tau Cannon charge minimum damage. Default: 25");
ConVar la_weapon_gauss_chargedmg_max("la_weapon_gauss_chargedmg_max", "200", FCVAR_REPLICATED | FCVAR_NOTIFY, "Tau Cannon charge maximum damage. Default: 200");
ConVar la_weapon_gauss_chargetime("la_weapon_gauss_chargetime", "1.5", FCVAR_REPLICATED | FCVAR_NOTIFY, "Tau Cannon charge time in seconds. Default: 1.5");

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CWeaponGauss::CWeaponGauss(void)
{
	m_bReloadsSingly = false;
	m_bFiresUnderwater = false;

	m_bPrimaryFire = false;
	m_nAttackState = 0;
	m_sndCharge = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGauss::Precache(void)
{
	PrecacheScriptSound("Weapon_Gauss.Zap1");
	PrecacheScriptSound("Weapon_Gauss.Zap2");
	PrecacheScriptSound("Weapon_Gauss.ChargeLoop");

	BaseClass::Precache();
}

float CWeaponGauss::GetFullChargeTime(void)
{
	return la_weapon_gauss_chargetime.GetFloat();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponGauss::PrimaryAttack(void)
{
	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());
	if (!pPlayer)
	{
		return;
	}

	if (pPlayer->GetAmmoCount(m_iPrimaryAmmoType) < 2)
	{
		WeaponSound(EMPTY);
		pPlayer->SetNextAttack(gpGlobals->curtime + 0.5);
		return;
	}

	//FIXME	pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_FIRE_VOLUME;
	m_bPrimaryFire = true;

	pPlayer->RemoveAmmo(2, m_iPrimaryAmmoType);

	StartFire();
	m_nAttackState = 0;
	SetWeaponIdleTime(gpGlobals->curtime + 1.0);
	pPlayer->SetNextAttack(gpGlobals->curtime + 0.2);
}

// I know, code duplication... but I had to make this function because
// FLerp was overloaded and shit wasn't compiling properly for some reason.
inline float GaussLerp(float minVal, float maxVal, float t)
{
	return minVal + (maxVal - minVal) * t;
}

void CWeaponGauss::CalculateChargePercent(void)
{
	m_flChargePercent = (gpGlobals->curtime - m_flStartCharge) / GetFullChargeTime();
	if (m_flChargePercent > 1.0)
	{
		m_flChargePercent = 1.0;
	}
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CWeaponGauss::SecondaryAttack(void)
{
	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());
	if (!pPlayer)
	{
		return;
	}

	// don't fire underwater
	if (pPlayer->GetWaterLevel() == 3)
	{
		if (m_nAttackState != 0)
		{
			EmitSound("Weapon_Gauss.Zap1");
			SendWeaponAnim(ACT_VM_IDLE);
			m_nAttackState = 0;
		}
		else
		{
			WeaponSound(EMPTY);
		}

		m_flNextSecondaryAttack = m_flNextPrimaryAttack = gpGlobals->curtime + 0.5;
		return;
	}

	if (m_nAttackState == 0)
	{
		if (pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
		{
			WeaponSound(EMPTY);
			pPlayer->SetNextAttack(gpGlobals->curtime + 0.5);
			return;
		}

		m_bPrimaryFire = false;

		pPlayer->RemoveAmmo(1, m_iPrimaryAmmoType);	// take one ammo just to start the spin
		m_flNextAmmoBurn = gpGlobals->curtime;

		// spin up
		//FIXME		pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_CHARGE_VOLUME;

		SendWeaponAnim(ACT_VM_PULLBACK_LOW);
		m_nAttackState = 1;
		SetWeaponIdleTime(gpGlobals->curtime + 0.5);

		m_flStartCharge = gpGlobals->curtime;
		CalculateChargePercent();
		m_nAmmoDrained = 0;
		
		//Start looping sound
		if (m_sndCharge == NULL)
		{
			CBroadcastRecipientFilter filter;
			m_sndCharge = (CSoundEnvelopeController::GetController()).SoundCreate(filter, entindex(), CHAN_WEAPON, "Weapon_Gauss.ChargeLoop", ATTN_NORM);
		}

		if (m_sndCharge != NULL)
		{
			(CSoundEnvelopeController::GetController()).Play(m_sndCharge, 1.0f, 110);
		}
	}
	else if (m_nAttackState == 1)
	{
		if (HasWeaponIdleTimeElapsed())
		{
			SendWeaponAnim(ACT_VM_PULLBACK);
			m_flStartCharge = gpGlobals->curtime;
			m_nAttackState = 2;
		}
	}
	else
	{
		// during the charging process, eat one bit of ammo every once in a while
		if (gpGlobals->curtime >= m_flNextAmmoBurn && m_nAmmoDrained < 10)
		{
			m_nAmmoDrained++;
			pPlayer->RemoveAmmo(1, m_iPrimaryAmmoType);

			m_flNextAmmoBurn = gpGlobals->curtime + (GetFullChargeTime() * 0.1);
		}

		if (pPlayer->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
		{
			// out of ammo! force the gun to fire
			StartFire();
			m_nAttackState = 0;
			SetWeaponIdleTime(gpGlobals->curtime + 1.0);
			pPlayer->SetNextAttack(gpGlobals->curtime + 1);
			return;
		}
		
		CalculateChargePercent();
		int pitch = (int)GaussLerp(150.0f, 250.0f, m_flChargePercent);

		// ALERT( at_console, "%d %d %d\n", m_nAttackState, m_iSoundState, pitch );

		//		if ( m_iSoundState == 0 )
		//			ALERT( at_console, "sound state %d\n", m_iSoundState );

		if (m_sndCharge != NULL)
		{
			(CSoundEnvelopeController::GetController()).SoundChangePitch(m_sndCharge, pitch, 0);
		}

		//FIXME		m_pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_CHARGE_VOLUME;

		// m_flTimeWeaponIdle = gpGlobals->curtime + 0.1;
		if (m_flStartCharge < gpGlobals->curtime - 10)
		{
			// Player charged up too long. Zap him.
			EmitSound("Weapon_Gauss.Zap1");
			EmitSound("Weapon_Gauss.Zap2");

			m_nAttackState = 0;
			SetWeaponIdleTime(gpGlobals->curtime + 1.0);
			pPlayer->SetNextAttack(gpGlobals->curtime + 1.0);

#if !defined(CLIENT_DLL )
			// Add DMG_CRUSH because we don't want any physics force
			pPlayer->TakeDamage(CTakeDamageInfo(this, this, 50, DMG_SHOCK | DMG_CRUSH));

			color32 gaussDamage = { 255, 128, 0, 128 };
			UTIL_ScreenFade(pPlayer, gaussDamage, 2, 0.5, FFADE_IN);
#endif

			SendWeaponAnim(ACT_VM_IDLE);

			StopSpinSound();
			// Player may have been killed and this weapon dropped, don't execute any more code after this!
			return;
		}
	}
}

//=========================================================
// StartFire- since all of this code has to run and then 
// call Fire(), it was easier at this point to rip it out 
// of weaponidle() and make its own function then to try to
// merge this into Fire(), which has some identical variable names 
//=========================================================
void CWeaponGauss::StartFire(void)
{
	float flDamage;

	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());
	if (!pPlayer)
	{
		return;
	}

	Vector vecAiming = pPlayer->GetAutoaimVector(AUTOAIM_5DEGREES);
	Vector vecSrc = pPlayer->Weapon_ShootPosition();

	if (m_bPrimaryFire)
	{
		flDamage = la_weapon_gauss_dmg.GetFloat() * g_pGameRules->GetDamageMultiplier();
	}
	else
	{
		flDamage = GaussLerp(la_weapon_gauss_chargedmg_min.GetFloat(), la_weapon_gauss_chargedmg_max.GetFloat(), m_flChargePercent);
	}

	if (!m_bPrimaryFire)
	{
		Vector vecRecoil = vecAiming * -GaussLerp(la_weapon_gauss_recoil_min.GetFloat(), la_weapon_gauss_recoil_max.GetFloat(), m_flChargePercent);
		pPlayer->ApplyAbsVelocityImpulse(vecRecoil);
	}

	Fire(vecSrc, vecAiming, flDamage);
}

void CWeaponGauss::Fire(Vector vecOrigSrc, Vector vecDir, float flDamage)
{
	CBaseEntity *pIgnore;
	Vector		vecSrc = vecOrigSrc;
	Vector		vecDest = vecSrc + vecDir * MAX_TRACE_LENGTH;
	bool		fFirstBeam = true;
	bool		fHasPunched = false;
	float		flMaxFrac = 1.0;
	int			nMaxHits = 10;

	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());
	if (!pPlayer)
	{
		return;
	}

	//FIXME	pPlayer->m_iWeaponVolume = GAUSS_PRIMARY_FIRE_VOLUME;

	StopSpinSound();

	pIgnore = pPlayer;

	//	ALERT( at_console, "%f %f\n", tr.flFraction, flMaxFrac );
#ifndef CLIENT_DLL
	lagcompensation->StartLagCompensation(pPlayer, pPlayer->GetCurrentCommand());
#endif
	while (flDamage > 10 && nMaxHits > 0)
	{
		trace_t	tr;

		nMaxHits--;

		// ALERT( at_console, "." );
		UTIL_TraceLine(vecSrc, vecDest, MASK_SHOT, pIgnore, COLLISION_GROUP_NONE, &tr);

		if (tr.allsolid)
			break;

		CBaseEntity *pEntity = tr.m_pEnt;
		if (pEntity == NULL)
			break;

		CBroadcastRecipientFilter filter;
		CEffectData	data6;
		if (fFirstBeam)
		{
			fFirstBeam = false;
			if (m_bPrimaryFire)
			{
				DispatchParticleEffect("gaussbeam", tr.endpos, PATTACH_POINT_FOLLOW, this, "muzzle", false);
			}
			else
			{
				DispatchParticleEffect("gaussbeam_charged", tr.endpos, PATTACH_POINT_FOLLOW, this, "muzzle", false);
			}
		}
		else
		{
			if (m_bPrimaryFire)
			{
				DispatchParticleEffect("gaussbeam", vecSrc, tr.endpos, GetAbsAngles(), this);
			}
			else
			{
				DispatchParticleEffect("gaussbeam_charged", vecSrc, tr.endpos, GetAbsAngles(), this);
			}
		}

		bool fShouldDamageEntity = (pEntity->m_takedamage != DAMAGE_NO);

		if (fShouldDamageEntity)
		{
			ClearMultiDamage();
			CTakeDamageInfo info(this, pPlayer, flDamage, DMG_ENERGYBEAM);
			CalculateMeleeDamageForce(&info, vecDir, tr.endpos);
			pEntity->DispatchTraceAttack(info, vecDir, &tr);
			ApplyMultiDamage();
		}

		if (pEntity->IsBSPModel() && !fShouldDamageEntity)
		{
			float n;

			pIgnore = NULL;

			n = -DotProduct(tr.plane.normal, vecDir);

			if (tr.surface.flags & SURF_SKY)
			{
				break;
			}

			if (n < 0.5) // 60 degrees
			{
				// ALERT( at_console, "reflect %f\n", n );
				// reflect
				Vector vecReflect;

				vecReflect = 2.0 * tr.plane.normal * n + vecDir;
				flMaxFrac = flMaxFrac - tr.fraction;
				vecDir = vecReflect;
				vecSrc = tr.endpos;// + vecDir * 8;
				vecDest = vecSrc + vecDir * MAX_TRACE_LENGTH;

#if !defined(CLIENT_DLL)
				// explode a bit
				RadiusDamage(CTakeDamageInfo(this, pPlayer, flDamage * n, DMG_BLAST), tr.endpos, flDamage * n * 2.5, CLASS_NONE, NULL);
#endif

				te->GaussExplosion(filter, 0.0f, tr.endpos, tr.plane.normal, 0);

				// lose energy
				if (n == 0)
					n = 0.1;

				flDamage = flDamage * (1 - n);
			}
			else
			{
				// tunnel
				UTIL_ImpactTrace(&tr, DMG_ENERGYBEAM);
				te->GaussExplosion(filter, 0.0f, tr.endpos, tr.plane.normal, 0);

				// limit it to one hole punch
				if (fHasPunched)
					break;

				fHasPunched = true;

				// try punching through wall if secondary attack (primary is incapable of breaking through)
				if (!m_bPrimaryFire)
				{
					trace_t punch_tr;

					UTIL_TraceLine(tr.endpos + vecDir * 8, vecDest, MASK_SHOT, pIgnore, COLLISION_GROUP_NONE, &punch_tr);

					if (!punch_tr.allsolid)
					{
						trace_t exit_tr;
						// trace backwards to find exit point
						UTIL_TraceLine(punch_tr.endpos, tr.endpos, MASK_SHOT, pIgnore, COLLISION_GROUP_NONE, &exit_tr);

						float n = (exit_tr.endpos - tr.endpos).Length();

						if (n < flDamage)
						{
							if (n == 0)
								n = 1;

							flDamage -= n;

							te->GaussExplosion(filter, 0.0f, tr.endpos, tr.plane.normal, 0);

							UTIL_ImpactTrace(&exit_tr, DMG_ENERGYBEAM);

							te->GaussExplosion(filter, 0.0f, exit_tr.endpos, tr.plane.normal, 0);

#if !defined( CLIENT_DLL)
							// exit blast damage
							float flDamageRadius = flDamage * 1.75;  // Old code == 2.5
							RadiusDamage(CTakeDamageInfo(this, pPlayer, flDamage, DMG_BLAST), exit_tr.endpos + vecDir * 8, flDamageRadius, CLASS_NONE, NULL);

							CSoundEnt::InsertSound(SOUND_COMBAT, GetAbsOrigin(), 1024, 3.0);
#endif

							vecSrc = exit_tr.endpos + vecDir;
						}
						else
						{
							break;	// [Striker] Prevent the shot from coming right back for you.
									// Might have been intentional, but it's a crappy mechanic.
						}
					}
					else
					{
						//ALERT( at_console, "blocked %f\n", n );
						flDamage = 0;
					}
				}
				else
				{
					//ALERT( at_console, "blocked solid\n" );
					if (m_bPrimaryFire)
					{
						// slug doesn't punch through ever with primary 
						// fire, so leave a little glowy bit and make some balls
						//te->GaussExplosion(filter, 0.0f, tr.endpos, tr.plane.normal, 0);
#if !defined( CLIENT_DLL)
						CSoundEnt::InsertSound(SOUND_COMBAT, GetAbsOrigin(), 600, 0.5);
#endif
					}

					flDamage = 0;
				}

			}
		}
		else
		{
			vecSrc = tr.endpos + vecDir;
			pIgnore = pEntity;
		}
	}

#ifndef CLIENT_DLL
	lagcompensation->FinishLagCompensation(pPlayer);
#endif

	pPlayer->ViewPunch(QAngle(-2, 0, 0));

	if (m_bPrimaryFire)
	{
		SendWeaponAnim(ACT_VM_PRIMARYATTACK);
		WeaponSound(SINGLE);
	}
	else
	{
		SendWeaponAnim(ACT_VM_SECONDARYATTACK);
		WeaponSound(WPN_DOUBLE);
	}
}

void CWeaponGauss::WeaponIdle(void)
{
	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());
	if (!pPlayer)
	{
		return;
	}

	if (!HasWeaponIdleTimeElapsed())
		return;

	if (m_nAttackState != 0)
	{
		StartFire();
		m_nAttackState = 0;
		SetWeaponIdleTime(gpGlobals->curtime + 2.0);
	}
	else
	{
		float flRand = random->RandomFloat(0, 1);
		if (flRand <= 0.75)
		{
			SendWeaponAnim(ACT_VM_IDLE);
			SetWeaponIdleTime(gpGlobals->curtime + random->RandomFloat(10, 15));
		}
		else
		{
			SendWeaponAnim(ACT_VM_FIDGET);
			SetWeaponIdleTime(gpGlobals->curtime + 3);
		}
	}
}

/*
==================================================
AddViewKick
==================================================
*/

void CWeaponGauss::AddViewKick(void)
{
}

bool CWeaponGauss::Deploy(void)
{
	if (DefaultDeploy((char*)GetViewModel(), (char*)GetWorldModel(), ACT_VM_DRAW, (char*)GetAnimPrefix()))
	{
		return true;
	}
	else
	{
		return false;
	}
}

bool CWeaponGauss::Holster(CBaseCombatWeapon *pSwitchingTo)
{

	StopSpinSound();
	m_nAttackState = 0;

	return BaseClass::Holster(pSwitchingTo);
}

void CWeaponGauss::StopSpinSound(void)
{
	if (m_sndCharge != NULL)
	{
		(CSoundEnvelopeController::GetController()).SoundDestroy(m_sndCharge);
		m_sndCharge = NULL;
	}
}