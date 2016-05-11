//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//
// Health.cpp
//
// implementation of CHudHealth class
//
#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "view.h"

#include "iclientmode.h"

#include <KeyValues.h>
#include <vgui/ISurface.h>
#include <vgui/ISystem.h>
#include <vgui_controls/AnimationController.h>

#include <vgui/ILocalize.h>

using namespace vgui;

#include "hudelement.h"
#include "hud_basetimer.h"

#include "convar.h"

#include "hl2mp_gamerules.h"

#include "engine/ienginesound.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Purpose: Health panel
//-----------------------------------------------------------------------------
class CHudMapTimer : public CHudElement, public CHudBaseTimer
{
	DECLARE_CLASS_SIMPLE(CHudMapTimer, CHudBaseTimer);

public:
	CHudMapTimer( const char *pElementName );
	virtual void Init( void );
	virtual void VidInit( void );
	virtual void Reset( void );
	virtual void OnThink();

private:
	bool m_bStarted;
	int m_iLastSecond;
};	

DECLARE_HUDELEMENT( CHudMapTimer );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudMapTimer::CHudMapTimer( const char *pElementName ) : CHudElement( pElementName ), CHudBaseTimer(NULL, "HudMapTimer")
{
	SetHiddenBits( HIDEHUD_NEEDSUIT );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMapTimer::Init()
{
	enginesound->PrecacheSound("ui/timer_tick.wav");
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMapTimer::Reset()
{
	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("MapTimerInit");
	SetMinutes(0);
	SetSeconds(0);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMapTimer::VidInit()
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMapTimer::OnThink()
{
	CHL2MPRules *rules = HL2MPRules();

	if (!rules)
		return;

	float timeleft = (rules->GetMapRemainingTime());

	if (timeleft <= 0.0)
	{
		SetPaintEnabled(false);
		SetPaintBackgroundEnabled(false);
		m_bStarted = false;
		return;
	}

	if (!m_bStarted)
	{
		SetPaintEnabled(true);
		SetPaintBackgroundEnabled(true);
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("MapTimerInit");
		m_bStarted = true;
	}

	int minutes, seconds;

	if ((timeleft / 60.0) >= 1)
	{
		minutes = Floor2Int(timeleft / 60.0);
		seconds = timeleft - (Floor2Int(timeleft / 60.0) * 60);
	}
	else
	{
		minutes = 0;
		seconds = Floor2Int(timeleft);
	}

	if (seconds != m_iLastSecond)
	{
		if (minutes == 0)
		{
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("MapTimerPulse");
			if (seconds <= 10)
			{
				CLocalPlayerFilter filter;
				enginesound->EmitSound(filter, -1, CHAN_AUTO, "ui/timer_tick.wav", 1.0, SNDLVL_NORM, 0);
			}
		}
		
		SetMinutes(minutes);
		SetSeconds(seconds);
		m_iLastSecond = seconds;
	}
}