//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=Copyright 2015 SIOSPHERE/Sub-Zero-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
//	PURPOSE: GAUSS GUN
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=


#include "cbase.h"
#include "beam_shared.h"
#include "AmmoDef.h"
#include "in_buttons.h"
#include "weapon_hl2mpbasehlmpcombatweapon.h"
#include "soundenvelope.h"

#ifdef CLIENT_DLL
#include "c_hl2mp_player.h"
#include "ClientEffectPrecacheSystem.h"
#else
#include "hl2mp_player.h"
#include "..\server\ilagcompensationmanager.h"
#endif

#include "particle_parse.h"

#ifdef CLIENT_DLL
#define CWeaponGauss C_WeaponGauss
#endif

//Gauss Definitions

#define GAUSS_BEAM_SPRITE "sprites/laserbeam.vmt"
#define	GAUSS_CHARGE_TIME			0.3f
#define	MAX_GAUSS_CHARGE			16
#define	MAX_GAUSS_CHARGE_TIME		3.0f
#define	DANGER_GAUSS_CHARGE_TIME	10

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CLIENT_DLL
CLIENTEFFECT_REGISTER_BEGIN(PrecacheEffectGauss)
CLIENTEFFECT_MATERIAL("sprites/laserbeam")
CLIENTEFFECT_REGISTER_END()
#endif

class CWeaponGauss : public CBaseHL2MPCombatWeapon
{
public:

	DECLARE_CLASS(CWeaponGauss, CBaseHL2MPCombatWeapon);
	CWeaponGauss(void);

	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	void	Spawn(void);
	void	Precache(void);
	void	PrimaryAttack(void);
	void	SecondaryAttack(void);
	void	AddViewKick(void);

	bool	Holster(CBaseCombatWeapon *pSwitchingTo = NULL);

	void	ItemPostFrame(void);

	float	GetFireRate(void) { return 0.2f; }

	void	DoWallBreak(Vector startPos, Vector endPos, Vector aimDir, trace_t *ptr, CBasePlayer *pOwner, bool m_bBreakAll);
	bool	DidPunchThrough(trace_t	*tr);

	DECLARE_ACTTABLE();
protected:
	void	Fire(void);
	void	ChargedFire(void);
	void	ChargedFireFirstBeam(void);
	void	StopChargeSound(void);
	void	DrawBeam(const Vector &startPos, const Vector &endPos, bool charged, bool useMuzzle = false);
	void	IncreaseCharge(void);
private:
	CSoundPatch *m_sndCharge;

	CNetworkVar(bool, m_bCharging);
	CNetworkVar(bool, m_bChargeIndicated);

	CNetworkVar(float, m_flNextChargeTime);
	CNetworkVar(float, m_flChargeStartTime);
	CNetworkVar(float, m_flCoilMaxVelocity);
	CNetworkVar(float, m_flCoilVelocity);
	CNetworkVar(float, m_flCoilAngle);
};

IMPLEMENT_NETWORKCLASS_ALIASED(WeaponGauss, DT_WeaponGauss)

BEGIN_NETWORK_TABLE(CWeaponGauss, DT_WeaponGauss)
#ifdef CLIENT_DLL
RecvPropBool(RECVINFO(m_bCharging)),
RecvPropBool(RECVINFO(m_bChargeIndicated)),
RecvPropFloat(RECVINFO(m_flNextChargeTime)),
RecvPropFloat(RECVINFO(m_flChargeStartTime)),
RecvPropFloat(RECVINFO(m_flCoilMaxVelocity)),
RecvPropFloat(RECVINFO(m_flCoilVelocity)),
RecvPropFloat(RECVINFO(m_flCoilAngle)),
#else
SendPropBool(SENDINFO(m_bCharging)),
SendPropBool(SENDINFO(m_bChargeIndicated)),
SendPropFloat(SENDINFO(m_flNextChargeTime)),
SendPropFloat(SENDINFO(m_flChargeStartTime)),
SendPropFloat(SENDINFO(m_flCoilMaxVelocity)),
SendPropFloat(SENDINFO(m_flCoilVelocity)),
SendPropFloat(SENDINFO(m_flCoilAngle)),
#endif
END_NETWORK_TABLE()

#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA(CWeaponGauss)
END_PREDICTION_DATA()
#endif

LINK_ENTITY_TO_CLASS(weapon_gauss, CWeaponGauss);
PRECACHE_WEAPON_REGISTER(weapon_gauss);

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

ConVar sk_dmg_gauss("sk_dmg_gauss", "10");
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CWeaponGauss::CWeaponGauss(void)
{
	m_flNextChargeTime = 0;
	m_flChargeStartTime = 0;
	m_sndCharge = NULL;
	m_bCharging = false;
	m_bChargeIndicated = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGauss::Precache(void)
{
	PrecacheModel(GAUSS_BEAM_SPRITE);

#ifndef CLIENT_DLL
	enginesound->PrecacheSound("weapons/gauss/chargeloop.wav");
#endif
	BaseClass::Precache();
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGauss::Spawn(void)
{
	BaseClass::Spawn();
	SetActivity(ACT_HL2MP_IDLE);
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGauss::PrimaryAttack(void)
{
	CBasePlayer *pOwner = ToBasePlayer(GetOwner());
	if (pOwner == NULL)
		return;

	WeaponSound(SINGLE);
	//WeaponSound(SPECIAL2);

	SendWeaponAnim(ACT_VM_PRIMARYATTACK);

	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	pOwner->RemoveAmmo(1, m_iPrimaryAmmoType);

	Fire();

	m_flCoilMaxVelocity = 0.0f;
	m_flCoilVelocity = 1000.0f;
	return;

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGauss::Fire(void)
{
	CBasePlayer *pOwner = ToBasePlayer(GetOwner());

	if (pOwner == NULL)
		return;

	m_bCharging = false;

	Vector  startPos = pOwner->Weapon_ShootPosition();
	Vector  aimDir = pOwner->GetAutoaimVector(AUTOAIM_5DEGREES);

	Vector vecUp, vecRight;
	VectorVectors(aimDir, vecRight, vecUp);

	float x, y, z;

	//Gassian spread
	do {
		x = random->RandomFloat(-0.5, 0.5) + random->RandomFloat(-0.5, 0.5);
		y = random->RandomFloat(-0.5, 0.5) + random->RandomFloat(-0.5, 0.5);
		z = x*x + y*y;
	} while (z > 1);


	Vector  endPos = startPos + (aimDir * MAX_TRACE_LENGTH);

#ifndef CLIENT_DLL
	lagcompensation->StartLagCompensation(pOwner, pOwner->GetCurrentCommand());
#endif

	//Shoot a shot straight out
	trace_t tr;
	UTIL_TraceLine(startPos, endPos, MASK_SHOT, pOwner, COLLISION_GROUP_NONE, &tr);

#ifndef CLIENT_DLL
	ClearMultiDamage();
#endif

#ifndef CLIENT_DLL    
	CBaseEntity *pHit = tr.m_pEnt;

	CTakeDamageInfo dmgInfo(this, pOwner, sk_dmg_gauss.GetFloat(), DMG_SHOCK | DMG_BULLET);

	if (pHit != NULL)
	{
		CalculateBulletDamageForce(&dmgInfo, m_iPrimaryAmmoType, aimDir, tr.endpos, 7.0f * 5.0f);
		pHit->DispatchTraceAttack(dmgInfo, aimDir, &tr);
	}
#endif

	if (tr.DidHitWorld())
	{
		float hitAngle = -DotProduct(tr.plane.normal, aimDir);

		if (hitAngle < 0.5f)
		{
			Vector vReflection;

			vReflection = 2.0 * tr.plane.normal * hitAngle + aimDir;

			startPos = tr.endpos;
			endPos = startPos + (vReflection * MAX_TRACE_LENGTH);

			//Draw beam to reflection point
			DrawBeam(tr.startpos, tr.endpos, false, true);

			CPVSFilter filter(tr.endpos);
			te->GaussExplosion(filter, 0.0f, tr.endpos, tr.plane.normal, 0);

			UTIL_ImpactTrace(&tr, GetAmmoDef()->DamageType(m_iPrimaryAmmoType), "ImpactGauss");

			//Find new reflection end position
			UTIL_TraceLine(startPos, endPos, MASK_SHOT, pOwner, COLLISION_GROUP_NONE, &tr);

			if (tr.m_pEnt != NULL)
			{
#ifndef CLIENT_DLL
				dmgInfo.SetDamageForce(GetAmmoDef()->DamageForce(m_iPrimaryAmmoType) * vReflection);
				dmgInfo.SetDamagePosition(tr.endpos);
				tr.m_pEnt->DispatchTraceAttack(dmgInfo, vReflection, &tr);
#endif
			}

			//Connect reflection point to end
			DrawBeam(tr.startpos, tr.endpos, false);
		}
		else
		{
			DrawBeam(tr.startpos, tr.endpos, false, true);
		}
	}
	else
	{
		DrawBeam(tr.startpos, tr.endpos, false, true);
	}
#ifndef CLIENT_DLL         
	ApplyMultiDamage();
	lagcompensation->FinishLagCompensation(pOwner);
#endif

	UTIL_ImpactTrace(&tr, GetAmmoDef()->DamageType(m_iPrimaryAmmoType), "ImpactGauss");

	CPVSFilter filter(tr.endpos);
	te->GaussExplosion(filter, 0.0f, tr.endpos, tr.plane.normal, 0);

	m_flNextSecondaryAttack = gpGlobals->curtime + 1.0f;

	AddViewKick();

	return;
}
//-----------------------------------------------------------------------------
// Purpose: Draw the first Carged Beam
//-----------------------------------------------------------------------------
void CWeaponGauss::ChargedFireFirstBeam(void)
{
	CBasePlayer *pOwner = ToBasePlayer(GetOwner());

	if (pOwner == NULL)
		return;

	Vector  startPos = pOwner->Weapon_ShootPosition();
	Vector  aimDir = pOwner->GetAutoaimVector(AUTOAIM_5DEGREES);
	Vector  endPos = startPos + (aimDir * MAX_TRACE_LENGTH);

	//Shoot a shot straight out
	trace_t tr;
	UTIL_TraceLine(startPos, endPos, MASK_SHOT, pOwner, COLLISION_GROUP_NONE, &tr);
	startPos = tr.endpos;

	//Draw beam
	DrawBeam(tr.startpos, tr.endpos, true, true);

	CPVSFilter filter(tr.endpos);
	te->GaussExplosion(filter, 0.0f, tr.endpos, tr.plane.normal, 0);

	UTIL_ImpactTrace(&tr, GetAmmoDef()->DamageType(m_iPrimaryAmmoType), "ImpactGauss");
	return;
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGauss::SecondaryAttack(void)
{
	CBasePlayer *pOwner = ToBasePlayer(GetOwner());

	if (pOwner == NULL)
		return;

	if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
		return;

	if (m_bCharging == false)
	{
		//Start looping animation
		SendWeaponAnim(ACT_VM_PULLBACK);

		//Start looping sound
		if (m_sndCharge == NULL)
		{
			CPASAttenuationFilter filter(this);
			m_sndCharge = (CSoundEnvelopeController::GetController()).SoundCreate(filter, entindex(), CHAN_STATIC, "weapons/gauss/chargeloop.wav", ATTN_NORM);
		}

		assert(m_sndCharge != NULL);
		if (m_sndCharge != NULL)
		{
			(CSoundEnvelopeController::GetController()).Play(m_sndCharge, 1.0f, 50);
			(CSoundEnvelopeController::GetController()).SoundChangePitch(m_sndCharge, 250, MAX_GAUSS_CHARGE_TIME);
		}

		m_flChargeStartTime = gpGlobals->curtime;
		m_bCharging = true;
		m_bChargeIndicated = false;

		//Decrement power
		pOwner->RemoveAmmo(1, m_iPrimaryAmmoType);
	}

	IncreaseCharge();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGauss::IncreaseCharge(void)
{
	if (m_flNextChargeTime > gpGlobals->curtime)
		return;
	CBasePlayer *pOwner = ToBasePlayer(GetOwner());

	if (pOwner == NULL)
		return;

	//Check our charge time
	if ((gpGlobals->curtime - m_flChargeStartTime) > MAX_GAUSS_CHARGE_TIME)
	{
		//Notify the player they're at maximum charge
		if (m_bChargeIndicated == false)
		{
			//WeaponSound(SPECIAL2);
			m_bChargeIndicated = true;
		}
		if ((gpGlobals->curtime - m_flChargeStartTime) > DANGER_GAUSS_CHARGE_TIME)
		{
			//Damage the player
			WeaponSound(SPECIAL1);
			// Add DMG_CRUSH because we don't want any physics force
#ifndef CLIENT_DLL
			pOwner->TakeDamage(CTakeDamageInfo(this, this, 5, DMG_SHOCK | DMG_BULLET));
			color32 gaussDamage = { 255, 128, 0, 128 };
			UTIL_ScreenFade(pOwner, gaussDamage, 0.2f, 0.2f, FFADE_IN);
#endif
			m_flNextChargeTime = gpGlobals->curtime + random->RandomFloat(0.5f, 2.5f);

		}
		return;
	}
	//Decrement power
	pOwner->RemoveAmmo(1, m_iPrimaryAmmoType);

	//Make sure we can draw power
	if (pOwner->GetAmmoCount(m_iPrimaryAmmoType) <= 0)
	{
		ChargedFire();
		ChargedFireFirstBeam();
		return;
	}

	m_flNextChargeTime = gpGlobals->curtime + GAUSS_CHARGE_TIME;
	return;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGauss::ChargedFire(void)
{
	CBasePlayer *pOwner = ToBasePlayer(GetOwner());

	if (pOwner == NULL)
		return;

	bool penetrated = false;

	//Play shock sounds
	WeaponSound(WPN_DOUBLE);
	//WeaponSound(SPECIAL2);

	SendWeaponAnim(ACT_VM_SECONDARYATTACK);

	StopChargeSound();

	m_bCharging = false;
	m_bChargeIndicated = false;

	m_flNextSecondaryAttack = gpGlobals->curtime + 1.0f;

#ifndef CLIENT_DLL
	lagcompensation->StartLagCompensation(pOwner, pOwner->GetCurrentCommand());
#endif

	//Shoot a shot straight
	Vector  startPos = pOwner->Weapon_ShootPosition();
	Vector  aimDir = pOwner->GetAutoaimVector(AUTOAIM_5DEGREES);
	Vector  endPos = startPos + (aimDir * MAX_TRACE_LENGTH);

	//Find Damage
	float flChargeAmount = (gpGlobals->curtime - m_flChargeStartTime) / MAX_GAUSS_CHARGE_TIME;
	//Clamp This
	if (flChargeAmount > 1.0f)
	{
		flChargeAmount = 1.0f;
	}

	trace_t tr;
	UTIL_TraceLine(startPos, endPos, MASK_SHOT, pOwner, COLLISION_GROUP_NONE, &tr); //Trace from gun to wall

	UTIL_ImpactTrace(&tr, GetAmmoDef()->DamageType(m_iPrimaryAmmoType), "ImpactGauss");
	UTIL_DecalTrace(&tr, "RedGlowFade");


#ifndef CLIENT_DLL
	float flDamage = 3 + (22 * flChargeAmount);

	RadiusDamage(CTakeDamageInfo(this, pOwner, flDamage, DMG_SHOCK), tr.endpos, 10.0f, CLASS_PLAYER_ALLY, pOwner);
	ClearMultiDamage();
#endif

	UTIL_ImpactTrace(&tr, GetAmmoDef()->DamageType(m_iPrimaryAmmoType), "ImpactGauss");
	UTIL_DecalTrace(&tr, "RedGlowFade");

#ifndef CLIENT_DLL
	RadiusDamage(CTakeDamageInfo(this, pOwner, flDamage, DMG_SHOCK), tr.endpos, 10.0f, CLASS_PLAYER_ALLY, pOwner);
#endif

	CBaseEntity *pHit = tr.m_pEnt;

	if (tr.DidHitWorld())
	{
		UTIL_ImpactTrace(&tr, GetAmmoDef()->DamageType(m_iPrimaryAmmoType), "ImpactGauss");
		UTIL_DecalTrace(&tr, "RedGlowFade");

		CPVSFilter filter(tr.endpos);
		te->GaussExplosion(filter, 0.0f, tr.endpos, tr.plane.normal, 0);

		Vector  testPos = tr.endpos + (aimDir * 128.0f);

		UTIL_TraceLine(testPos, tr.endpos, MASK_SHOT, pOwner, COLLISION_GROUP_NONE, &tr); //Trace to backside of first wall

		UTIL_ImpactTrace(&tr, GetAmmoDef()->DamageType(m_iPrimaryAmmoType), "ImpactGauss");
		UTIL_DecalTrace(&tr, "RedGlowFade");

#ifndef CLIENT_DLL
		RadiusDamage(CTakeDamageInfo(this, pOwner, flDamage, DMG_SHOCK), tr.endpos, 10.0f, CLASS_PLAYER_ALLY, pOwner);
#endif


		if (tr.allsolid == false)
		{
			UTIL_DecalTrace(&tr, "RedGlowFade");
			UTIL_ImpactTrace(&tr, GetAmmoDef()->DamageType(m_iPrimaryAmmoType), "ImpactGauss");

#ifndef CLIENT_DLL
			RadiusDamage(CTakeDamageInfo(this, pOwner, flDamage, DMG_SHOCK), tr.endpos, 10.0f, CLASS_PLAYER_ALLY, pOwner);
#endif

			penetrated = true;

			UTIL_ImpactTrace(&tr, GetAmmoDef()->DamageType(m_iPrimaryAmmoType), "ImpactGauss");
			UTIL_DecalTrace(&tr, "RedGlowFade");
#ifndef CLIENT_DLL
			RadiusDamage(CTakeDamageInfo(this, pOwner, flDamage, DMG_SHOCK), tr.endpos, 10.0f, CLASS_PLAYER_ALLY, pOwner);
#endif

		}

	}
	else if (pHit != NULL)
	{
#ifndef CLIENT_DLL
		// CTakeDamageInfo dmgInfo( this, pOwner, sk_plr_max_dmg_gauss.GetFloat(), DMG_SHOCK );
		//		  CalculateBulletDamageForce( &dmgInfo, m_iPrimaryAmmoType, aimDir, tr.endpos );
		UTIL_ImpactTrace(&tr, GetAmmoDef()->DamageType(m_iPrimaryAmmoType), "ImpactGauss");
		UTIL_DecalTrace(&tr, "RedGlowFade");
		//Do Direct damage to anything in our path
		//		  pHit->DispatchTraceAttack( dmgInfo, aimDir, &tr );
#endif
	}
#ifndef CLIENT_DLL
	ApplyMultiDamage();
#endif

#ifndef CLIENT_DLL
	RadiusDamage(CTakeDamageInfo(this, pOwner, flDamage, DMG_SHOCK), tr.endpos, 10.0f, CLASS_PLAYER_ALLY, pOwner);
#endif

	Vector  newPos = tr.endpos + (aimDir * MAX_TRACE_LENGTH);
	QAngle  viewPunch;
	viewPunch.x = random->RandomFloat(-4.0f, -8.0f);
	viewPunch.y = random->RandomFloat(-0.25f, 0.25f);
	viewPunch.z = 0;
	pOwner->ViewPunch(viewPunch);

	// DrawBeam( startPos, tr.endpos, true, true ); //Draw beam from gun through first wall.
#ifndef CLIENT_DLL
	Vector	recoilForce = pOwner->BodyDirection3D() * -(flDamage * 15.0f);
	recoilForce[2] += 128.0f;

	pOwner->ApplyAbsVelocityImpulse(recoilForce);
#endif
	CPVSFilter filter(tr.endpos);
	te->GaussExplosion(filter, 0.0f, tr.endpos, tr.plane.normal, 0);

#ifndef CLIENT_DLL
	RadiusDamage(CTakeDamageInfo(this, pOwner, flDamage, DMG_SHOCK), tr.endpos, 10.0f, CLASS_PLAYER_ALLY, pOwner);
#endif

	if (penetrated == true)
	{
		trace_t beam_tr;
		Vector vecDest = tr.endpos + aimDir * MAX_TRACE_LENGTH;
		UTIL_TraceLine(tr.endpos, vecDest, MASK_SHOT, pOwner, COLLISION_GROUP_NONE, &beam_tr); //Traces from back of first wall to second wall

#ifndef CLIENT_DLL
		float flDamage = 37 + (100 * flChargeAmount);
#endif

		UTIL_TraceLine(beam_tr.endpos + aimDir * 128.0f, beam_tr.endpos, MASK_SHOT, pOwner, COLLISION_GROUP_NONE, &beam_tr); //Traces To back of second wall
		UTIL_ImpactTrace(&beam_tr, GetAmmoDef()->DamageType(m_iPrimaryAmmoType), "ImpactGauss");
		UTIL_DecalTrace(&beam_tr, "RedGlowFade");

#ifndef CLIENT_DLL
		RadiusDamage(CTakeDamageInfo(this, pOwner, flDamage, DMG_SHOCK), tr.endpos, 10.0f, CLASS_PLAYER_ALLY, pOwner);
#endif

		DrawBeam(tr.endpos, beam_tr.endpos, true, false);
		DoWallBreak(tr.endpos, newPos, aimDir, &tr, pOwner, true);

		UTIL_ImpactTrace(&beam_tr, GetAmmoDef()->DamageType(m_iPrimaryAmmoType), "ImpactGauss");
		UTIL_DecalTrace(&beam_tr, "RedGlowFade");

#ifndef CLIENT_DLL
		RadiusDamage(CTakeDamageInfo(this, pOwner, flDamage, DMG_SHOCK), tr.endpos, 10.0f, CLASS_PLAYER_ALLY, pOwner);
#endif
	}

#ifndef CLIENT_DLL
	lagcompensation->FinishLagCompensation(pOwner);
#endif

	return;
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponGauss::DidPunchThrough(trace_t *tr)
{
	if (tr->DidHitWorld() && tr->surface.flags != SURF_SKY && ((tr->startsolid == false) || (tr->startsolid == true && tr->allsolid == false)))
		return true;
	return false;
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGauss::DoWallBreak(Vector startPos, Vector endPos, Vector aimDir, trace_t *ptr, CBasePlayer *pOwner, bool m_bBreakAll){
	trace_t *temp = ptr;
	if (m_bBreakAll){
		Vector tempPos = endPos;
		Vector beamStart = startPos;
		int x = 0;
		while (DidPunchThrough(ptr)){
			temp = ptr;
			if (x == 0){
				UTIL_TraceLine(startPos, tempPos, MASK_SHOT, pOwner, COLLISION_GROUP_NONE, ptr);
				x = 1;
			}
			else{
				UTIL_TraceLine(endPos, startPos, MASK_SHOT, pOwner, COLLISION_GROUP_NONE, ptr);
				x = 0;
			}
			if (ptr->DidHitWorld() && ptr->surface.flags != SURF_SKY){
				UTIL_ImpactTrace(ptr, GetAmmoDef()->DamageType(m_iPrimaryAmmoType), "ImpactGauss");
				UTIL_DecalTrace(ptr, "RedGlowFade");
			}
			startPos = ptr->endpos;
			tempPos = ptr->endpos + (aimDir * MAX_TRACE_LENGTH + aimDir * 128.0f);

		}
	}
	else{
		UTIL_TraceLine(startPos, endPos, MASK_SHOT, pOwner, COLLISION_GROUP_NONE, ptr); //Trace from gun to wall
	}

	if (!DidPunchThrough(ptr)){
		ptr = temp;
		return;
	}
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGauss::DrawBeam(const Vector &startPos, const Vector &endPos, bool charged, bool useMuzzle)
{
	if (charged)
	{
		if (useMuzzle)
		{
			DispatchParticleEffect("gaussbeam_charged", endPos, PATTACH_POINT_FOLLOW, this, "0", false);
		}
		else
		{
			DispatchParticleEffect("gaussbeam_charged", startPos, endPos, GetAbsAngles(), this);
		}
	}
	else
	{
		if (useMuzzle)
		{
			DispatchParticleEffect("gaussbeam", endPos, PATTACH_POINT_FOLLOW, this, "0", false);
		}
		else
		{
			DispatchParticleEffect("gaussbeam", startPos, endPos, GetAbsAngles(), this);
		}
	}

	return;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

void CWeaponGauss::AddViewKick(void)
{
	//Get the view kick
	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());

	if (pPlayer == NULL)
		return;

	QAngle	viewPunch;

	viewPunch.x = random->RandomFloat(-0.5f, -0.2f);
	viewPunch.y = random->RandomFloat(-0.5f, 0.5f);
	viewPunch.z = 0;

	pPlayer->ViewPunch(viewPunch);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGauss::ItemPostFrame(void)
{
	//Get the view kick
	CBasePlayer *pPlayer = ToBasePlayer(GetOwner());
	if (pPlayer == NULL)
		return;

	if (pPlayer->m_afButtonReleased & IN_ATTACK2)
	{
		if (m_bCharging){
			ChargedFire();
			ChargedFireFirstBeam();
		}
	}

	BaseClass::ItemPostFrame();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CWeaponGauss::StopChargeSound(void)
{
	if (m_sndCharge != NULL)
	{
		(CSoundEnvelopeController::GetController()).SoundFadeOut(m_sndCharge, 0.1f);
	}
	m_sndCharge = NULL;

}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSwitchingTo - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CWeaponGauss::Holster(CBaseCombatWeapon *pSwitchingTo)
{
	StopChargeSound();
	if (m_bCharging == true){
		ChargedFire();
		ChargedFireFirstBeam();
	}
	m_bCharging = false;
	m_bChargeIndicated = false;



	if (m_sndCharge != NULL)
	{
		(CSoundEnvelopeController::GetController()).SoundFadeOut(m_sndCharge, 0.1f);
	}
	m_sndCharge = NULL;

	StopChargeSound();

	return BaseClass::Holster(pSwitchingTo);
}