#include "cbase.h"
#include "quake3bob.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// Client class for pickups, determines if an item should rotate in place.
//-----------------------------------------------------------------------------
class C_Item : public C_BaseAnimating
{
	DECLARE_CLASS(C_Item, C_BaseAnimating);
	DECLARE_CLIENTCLASS();
public:
	C_Item() {
		ClientRotAng = { 0.0, FRand(0.0, 359.0), 0.0 };
		m_bQuake3Bob = false;
	}
	void Spawn() { ClientThink(); }

	void ClientThink();
	void PostDataUpdate(DataUpdateType_t updateType);
	bool ShouldDraw();

	bool	m_bQuake3Bob;

	Vector	m_vOriginalSpawnOrigin;
	Vector	m_vOriginalSpawnAngles;

private:
	QAngle		ClientRotAng; // m_angRotation is stomped sometimes (CItem returning the ent to spawn position?)
};

IMPLEMENT_CLIENTCLASS_DT(C_Item, DT_Item, CItem)
RecvPropBool(RECVINFO(m_bQuake3Bob)),
RecvPropVector(RECVINFO(m_vOriginalSpawnOrigin)),
RecvPropVector(RECVINFO(m_vOriginalSpawnAngles)),
END_RECV_TABLE()

void C_Item::ClientThink()
{
	if (IsAbsQueriesValid() && m_bQuake3Bob)
	{
		// Rotate
		Quake3Rotate(this, ClientRotAng);
		Quake3Bob(this, m_vOriginalSpawnOrigin);
	}

	SetNextClientThink(CLIENT_THINK_ALWAYS);
}

void C_Item::PostDataUpdate(DataUpdateType_t updateType)
{
	return BaseClass::PostDataUpdate(updateType);
}

bool C_Item::ShouldDraw()
{
	return BaseClass::ShouldDraw();
}