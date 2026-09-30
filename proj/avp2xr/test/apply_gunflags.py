# Use the direct flag API on the weapon model (as CWeaponModel does) and log what happens.
def patch(path, pairs):
    s = open(path, encoding='utf-8').read()
    for a, b in pairs:
        assert s.count(a) == 1, (path, a[:80], s.count(a))
        s = s.replace(a, b)
    open(path, 'w', encoding='utf-8').write(s)

patch('K:/Coding/Aliens-Versus-Predator-2-VR/proj/AVP2/ClientShellDLL/VRMgr.cpp', [
    ('''	g_pLTClient->Common()->GetObjectFlags(hWeapon, OFT_Flags, m_nWeaponFlags);''',
     '''	m_nWeaponFlags = g_pLTClient->GetObjectFlags(hWeapon);'''),
    ('''	g_pLTClient->Common()->SetObjectFlags(hWeapon, OFT_Flags, m_nWeaponFlags & ~FLAG_REALLYCLOSE);''',
     '''	g_pLTClient->SetObjectFlags(hWeapon, m_nWeaponFlags & ~FLAG_REALLYCLOSE);'''),
    ('''		g_pLTClient->Common()->SetObjectFlags(hWeapon, OFT_Flags, m_nWeaponFlags);''',
     '''		g_pLTClient->SetObjectFlags(hWeapon, m_nWeaponFlags);'''),
    ('''	LTVector vScale = m_vWeaponScale * fScale;
	g_pLTClient->SetObjectScale(hWeapon, &vScale);
	g_pLTClient->SetObjectPos(hWeapon, &vOrigin, LTTRUE);
	g_pLTClient->SetObjectRotation(hWeapon, &m_rAimRot);
}''', '''	LTVector vScale = m_vWeaponScale * fScale;
	g_pLTClient->SetObjectScale(hWeapon, &vScale);
	g_pLTClient->SetObjectPos(hWeapon, &vOrigin, LTTRUE);
	g_pLTClient->SetObjectRotation(hWeapon, &m_rAimRot);

	// Diagnostics, every couple of seconds: did the flag change stick, and where did it go?
	static uint32 s_nNextLog = 0;
	uint32 nNow = GetTickCount();
	if((int32)(nNow - s_nNextLog) >= 0)
	{
		s_nNextLog = nNow + 2000;
		LTVector vCam, vGot, vGotScale;
		g_pLTClient->GetObjectPos(g_pGameClientShell->GetPlayerCameraObject(), &vCam);
		g_pLTClient->GetObjectPos(hWeapon, &vGot);
		g_pLTClient->GetObjectScale(hWeapon, &vGotScale);
		uint32 nNowFlags = g_pLTClient->GetObjectFlags(hWeapon);
		VRLog("Weapon %p: flags %08x -> %08x (reallyclose %s), game pos (%.1f %.1f %.1f) scale %.2f, "
			"placed (%.1f %.1f %.1f) scale %.2f, controller (%.1f %.1f %.1f), camera (%.1f %.1f %.1f), two-handed %d",
			hWeapon, m_nWeaponFlags, nNowFlags, (nNowFlags & FLAG_REALLYCLOSE) ? "STILL SET" : "cleared",
			m_vWeaponPos.x, m_vWeaponPos.y, m_vWeaponPos.z, m_vWeaponScale.x,
			vGot.x, vGot.y, vGot.z, vGotScale.x, m_vAimPos.x, m_vAimPos.y, m_vAimPos.z,
			vCam.x, vCam.y, vCam.z, m_bTwoHanded);
	}
}'''),
    # Also note when the weapon can't be placed at all
    ('''void VRMgr::PlaceWeapon()
{
	if(!m_bAimWorld || m_bWeaponMoved)
		return;''', '''void VRMgr::PlaceWeapon()
{
	if(!m_bAimWorld || m_bWeaponMoved)
	{
		static uint32 s_nNextSkipLog = 0;
		if(!m_bAimWorld && (int32)(GetTickCount() - s_nNextSkipLog) >= 0)
		{
			s_nNextSkipLog = GetTickCount() + 2000;
			VRLog("Weapon not placed: no aim pose (input %d, aim tracked %d)", m_bInput, m_Input.aimValid);
		}
		return;
	}'''),
])
print('ok')
