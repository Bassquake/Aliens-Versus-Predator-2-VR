# VR: draw the first-person weapon as a normal world object at the aiming controller.
def patch(path, pairs):
    s = open(path, encoding='utf-8').read()
    for a, b in pairs:
        assert s.count(a) == 1, (path, a[:80], s.count(a))
        s = s.replace(a, b)
    open(path, 'w', encoding='utf-8').write(s)

vr = 'K:/Coding/Aliens-Versus-Predator-2-VR/proj/AVP2/ClientShellDLL/VRMgr.cpp'
s = open(vr, encoding='utf-8').read()
start = s.index('void VRMgr::PlaceWeaponForEye(')
end = s.index('// ----------------------------------------------------------------------- //', s.index('void VRMgr::RestoreWeapon()'))
s = s[:start] + '''void VRMgr::PlaceWeapon()
{
	if(!m_bAimWorld || m_bWeaponMoved)
		return;

	CWeaponModel *pWeapon = g_pGameClientShell->GetWeaponModel();
	HOBJECT hWeapon = pWeapon ? pWeapon->GetHandle() : LTNULL;
	if(!hWeapon)
		return;

	// Remember the game's own (view-relative, really-close) placement to put back afterwards
	g_pLTClient->GetObjectPos(hWeapon, &m_vWeaponPos);
	g_pLTClient->GetObjectRotation(hWeapon, &m_rWeaponRot);
	g_pLTClient->GetObjectScale(hWeapon, &m_vWeaponScale);
	g_pLTClient->Common()->GetObjectFlags(hWeapon, OFT_Flags, m_nWeaponFlags);
	m_bWeaponMoved = LTTRUE;

	// Really-close objects are drawn with their own view-relative projection, which makes them
	// look tiny and far away in VR and swing with the head. Drawn as a normal world object
	// instead, the weapon has real size and stereo depth and stays in the hand.
	g_pLTClient->Common()->SetObjectFlags(hWeapon, OFT_Flags, m_nWeaponFlags & ~FLAG_REALLYCLOSE);

	// The model is made to be seen from its origin with the gun at the weapon offset, so put
	// that viewpoint behind the controller and the gun lands in the hand. VRGunOffset (world
	// units, in the controller's frame: right, up, forward) and VRGunScale fine-tune the fit.
	LTFLOAT fScale = g_vtVRGunScale.GetFloat();
	LTVector vOffset = pWeapon->GetWeaponOffset() * fScale;
	vOffset -= LTVector(g_vtVRGunOffsetX.GetFloat(), g_vtVRGunOffsetY.GetFloat(), g_vtVRGunOffsetZ.GetFloat());
	LTVector vOrigin = m_vAimPos - Rotate(m_rAimRot, vOffset);

	LTVector vScale = m_vWeaponScale * fScale;
	g_pLTClient->SetObjectScale(hWeapon, &vScale);
	g_pLTClient->SetObjectPos(hWeapon, &vOrigin, LTTRUE);
	g_pLTClient->SetObjectRotation(hWeapon, &m_rAimRot);
}

// ----------------------------------------------------------------------- //

void VRMgr::RestoreWeapon()
{
	if(!m_bWeaponMoved)
		return;

	CWeaponModel *pWeapon = g_pGameClientShell->GetWeaponModel();
	HOBJECT hWeapon = pWeapon ? pWeapon->GetHandle() : LTNULL;
	if(hWeapon)
	{
		g_pLTClient->Common()->SetObjectFlags(hWeapon, OFT_Flags, m_nWeaponFlags);
		g_pLTClient->SetObjectScale(hWeapon, &m_vWeaponScale);
		g_pLTClient->SetObjectPos(hWeapon, &m_vWeaponPos, LTTRUE);
		g_pLTClient->SetObjectRotation(hWeapon, &m_rWeaponRot);
	}
	m_bWeaponMoved = LTFALSE;
}

''' + s[end:]
open(vr, 'w', encoding='utf-8').write(s)

patch(vr, [
    ('''static VarTrack		g_vtVRSnapTurn;
''', '''static VarTrack		g_vtVRSnapTurn;
static VarTrack		g_vtVRGunScale;
static VarTrack		g_vtVRGunOffsetX;
static VarTrack		g_vtVRGunOffsetY;
static VarTrack		g_vtVRGunOffsetZ;
'''),
    ('''	g_vtVRSnapTurn.Init(g_pLTClient, "VRSnapTurn", LTNULL, 45.0f);
''', '''	g_vtVRSnapTurn.Init(g_pLTClient, "VRSnapTurn", LTNULL, 45.0f);
	g_vtVRGunScale.Init(g_pLTClient, "VRGunScale", LTNULL, 1.0f);
	g_vtVRGunOffsetX.Init(g_pLTClient, "VRGunOffsetX", LTNULL, 0.0f);
	g_vtVRGunOffsetY.Init(g_pLTClient, "VRGunOffsetY", LTNULL, 0.0f);
	g_vtVRGunOffsetZ.Init(g_pLTClient, "VRGunOffsetZ", LTNULL, 0.0f);
'''),
])

patch('K:/Coding/Aliens-Versus-Predator-2-VR/proj/AVP2/ClientShellDLL/VRMgr.h', [
    ('''		// Called by CameraMgr around each eye's render of the player camera: draws the
		// first-person weapon at the aiming controller instead of fixed to the view.
		void		PlaceWeaponForEye(const LTVector &vEyePos, const LTRotation &rEyeRot);
		void		RestoreWeapon();''', '''		// Called by CameraMgr around the eye renders of the player camera: draws the
		// first-person weapon as a world object at the aiming controller, then puts the game's
		// own view-relative placement back.
		void		PlaceWeapon();
		void		RestoreWeapon();'''),
    ('''		LTVector		m_vWeaponPos;			// the weapon's own (view-relative) placement, to restore
		LTRotation		m_rWeaponRot;''', '''		LTVector		m_vWeaponPos;			// the weapon's own (view-relative) placement, to restore
		LTRotation		m_rWeaponRot;
		LTVector		m_vWeaponScale;
		uint32			m_nWeaponFlags;'''),
    ('''//   VRSnapTurn    degrees per right-stick snap turn (default 45, 0 = off)''', '''//   VRSnapTurn    degrees per right-stick snap turn (default 45, 0 = off)
//   VRGunScale    size of the first-person weapon and hands (default 1)
//   VRGunOffsetX/Y/Z  moves the weapon in the controller's frame (world units: right, up, forward)'''),
])

patch('K:/Coding/Aliens-Versus-Predator-2-VR/proj/AVP2/ClientShellDLL/CameraMgr.cpp', [
    ('''		pVR->PlaceWeaponForEye(vEyePos, rEyeRot);
''', ''),
    ('''	for(int nEye = 0; nEye < 2; nEye++)
	{''', '''	pVR->PlaceWeapon();

	for(int nEye = 0; nEye < 2; nEye++)
	{'''),
])
print('ok')
