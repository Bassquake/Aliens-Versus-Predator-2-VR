# Adds controller button handling to the VR cshell (VRMgr + PlayerMovement).
def patch(path, pairs):
    s = open(path, encoding='utf-8').read()
    for a, b in pairs:
        assert s.count(a) == 1, (path, a[:80], s.count(a))
        s = s.replace(a, b)
    open(path, 'w', encoding='utf-8').write(s)

patch('K:/Coding/Aliens-Versus-Predator-2-VR/proj/AVP2/ClientShellDLL/VRMgr.cpp', [
    ('#include "GameClientShell.h"\n', '#include "GameClientShell.h"\n#include "CommandIds.h"\n'),
    ('''	m_pApi->SetGameResolution((int)nWidth, (int)nHeight);
}
''', '''	m_pApi->SetGameResolution((int)nWidth, (int)nHeight);

	UpdateButtons();
}

// ----------------------------------------------------------------------- //
// Controller buttons go to the game the same way keys do: press and release as
// OnCommandOn/OnCommandOff. Held buttons also feed the control flags (see PlayerMovement).

static const struct { unsigned int nButton; int nCommand; } s_ButtonCommands[] =
{
	{ AVP2XR_BTN_FIRE,		COMMAND_ID_FIRING },
	{ AVP2XR_BTN_ALTFIRE,	COMMAND_ID_ALT_FIRING },
	{ AVP2XR_BTN_JUMP,		COMMAND_ID_JUMP },
	{ AVP2XR_BTN_CROUCH,	COMMAND_ID_DUCK },
	{ AVP2XR_BTN_USE,		COMMAND_ID_ACTIVATE },
	{ AVP2XR_BTN_VISION,	COMMAND_ID_NEXT_VISIONMODE },
	{ AVP2XR_BTN_RELOAD,	COMMAND_ID_RELOAD },
};

void VRMgr::UpdateButtons()
{
	// Everything counts as released once controller input stops (menus, flat screen)
	unsigned int nNow = m_bLastFrameInput ? m_Input.buttons : 0;
	unsigned int nChanged = nNow ^ m_nButtons;
	m_nButtons = nNow;

	for(int i = 0; i < sizeof(s_ButtonCommands) / sizeof(s_ButtonCommands[0]); i++)
	{
		if(!(nChanged & s_ButtonCommands[i].nButton))
			continue;
		if(nNow & s_ButtonCommands[i].nButton)
			g_pGameClientShell->OnCommandOn(s_ButtonCommands[i].nCommand);
		else
			g_pGameClientShell->OnCommandOff(s_ButtonCommands[i].nCommand);
	}

	// Right stick up/down cycles weapons, one per push. Only when up/down is the stick's main
	// direction, so a diagonal doesn't snap turn as well.
	if(m_bLastFrameInput && m_Input.turnValid)
	{
		LTFLOAT x = m_Input.turn, y = m_Input.turnY;
		if(m_bCycleArmed && (LTFLOAT)fabs(y) > VR_SNAP_PRESS && fabs(y) > fabs(x))
		{
			int nCommand = (y > 0.0f) ? COMMAND_ID_NEXT_WEAPON : COMMAND_ID_PREV_WEAPON;
			g_pGameClientShell->OnCommandOn(nCommand);
			g_pGameClientShell->OnCommandOff(nCommand);
			m_bCycleArmed = LTFALSE;
		}
		else if((LTFLOAT)fabs(y) < VR_SNAP_RELEASE)
			m_bCycleArmed = LTTRUE;
	}
}
'''),
    ('''			if(m_bSnapArmed && (x > VR_SNAP_PRESS || x < -VR_SNAP_PRESS))''',
     '''			if(m_bSnapArmed && (LTFLOAT)fabs(x) > VR_SNAP_PRESS && fabs(x) > fabs(m_Input.turnY))'''),
    ('''	m_nRecenterCount = 0;
	m_bSnapArmed = LTTRUE;
''', '''	m_nRecenterCount = 0;
	m_bSnapArmed = LTTRUE;
	m_bCycleArmed = LTTRUE;
	m_nButtons = 0;
'''),
])

patch('K:/Coding/Aliens-Versus-Predator-2-VR/proj/AVP2/ClientShellDLL/VRMgr.h', [
    ('''	private:
		LTBOOL		FindApi();''', '''	private:
		LTBOOL		FindApi();
		void		UpdateButtons();'''),
])

patch('K:/Coding/Aliens-Versus-Predator-2-VR/proj/AVP2/Shared/PlayerMovement.cpp', [
    ('''				if(m_pInterface->IsCommandOn(COMMAND_ID_ALT_FIRING))	dwCtrl |= CM_FLAG_ALTFIRING;
''', '''				if(m_pInterface->IsCommandOn(COMMAND_ID_ALT_FIRING))	dwCtrl |= CM_FLAG_ALTFIRING;

				// VR controller buttons held (their presses also go through OnCommandOn)
				VRMgr *pVR = g_pGameClientShell->GetVRMgr();
				if(pVR->IsButtonHeld(AVP2XR_BTN_FIRE))		dwCtrl |= CM_FLAG_PRIMEFIRING;
				if(pVR->IsButtonHeld(AVP2XR_BTN_ALTFIRE))	dwCtrl |= CM_FLAG_ALTFIRING;
				if(pVR->IsButtonHeld(AVP2XR_BTN_JUMP))		dwCtrl |= CM_FLAG_JUMP;
				if(pVR->IsButtonHeld(AVP2XR_BTN_CROUCH))	dwCtrl |= CM_FLAG_DUCK;
'''),
])

# The character set log verified the ButeMgr order fix; take it out so the log stays small.
patch('K:/Coding/Aliens-Versus-Predator-2-VR/proj/AVP2/ClientShellDLL/CharacterFX.cpp', [
    ('''	// Diagnostics: which character set the server's index maps to on this client
	VRLog("Character %p: set %d = %s, mode %s, model %s", m_hServerObject, m_cs.nCharacterSet,
		g_pCharacterButeMgr->GetCharacterButes(m_cs.nCharacterSet).m_szName, mode.c_str(),
		(LPCSTR)g_pCharacterButeMgr->GetDefaultModel(m_cs.nCharacterSet));

''', ''),
])
print('ok')
