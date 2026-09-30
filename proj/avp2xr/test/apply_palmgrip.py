# VR: pin the first-person model's right palm (not its eye-point origin) to the aiming controller.
def patch(path, pairs):
    s = open(path, encoding='utf-8').read()
    for a, b in pairs:
        assert s.count(a) == 1, (path, a[:80], s.count(a))
        s = s.replace(a, b)
    open(path, 'w', encoding='utf-8').write(s)

cpp = 'K:/Coding/Aliens-Versus-Predator-2-VR/proj/AVP2/ClientShellDLL/VRMgr.cpp'
patch(cpp, [
    ('static VarTrack\t\tg_vtVRGunOffsetZ;\n',
     'static VarTrack\t\tg_vtVRGunOffsetZ;\n'
     'static VarTrack\t\tg_vtVRGripX;\n'
     'static VarTrack\t\tg_vtVRGripY;\n'
     'static VarTrack\t\tg_vtVRGripZ;\n'),
    ('\tg_vtVRGunOffsetZ.Init(g_pLTClient, "VRGunOffsetZ", LTNULL, 0.0f);\n',
     '\tg_vtVRGunOffsetZ.Init(g_pLTClient, "VRGunOffsetZ", LTNULL, 0.0f);\n'
     '\tg_vtVRGripX.Init(g_pLTClient, "VRGripX", LTNULL, VR_DEFAULT_GRIP_X);\n'
     '\tg_vtVRGripY.Init(g_pLTClient, "VRGripY", LTNULL, VR_DEFAULT_GRIP_Y);\n'
     '\tg_vtVRGripZ.Init(g_pLTClient, "VRGripZ", LTNULL, VR_DEFAULT_GRIP_Z);\n'),
    ('#define VR_SNAP_RELEASE\t\t\t0.3f\n',
     '#define VR_SNAP_RELEASE\t\t\t0.3f\n'
     '\n'
     '// Where the right palm is in a first-person model (model space: right, up, forward) when the\n'
     '// model has no hand nodes to find it by (shotgun, minigun, smartgun, exosuit...).\n'
     '#define VR_DEFAULT_GRIP_X\t\t6.0f\n'
     '#define VR_DEFAULT_GRIP_Y\t\t-10.0f\n'
     '#define VR_DEFAULT_GRIP_Z\t\t16.0f\n'
     '\n'
     '// Right wrist and middle-finger base node names used by the first-person models\n'
     'static const char *s_szWristNodes[] = { "Wrist_r1", "_zN_Wrist_r1", "r_wrist", "_zN_r_wrist" };\n'
     'static const char *s_szKnuckleNodes[] = { "Middle_r1", "_zN_Middle_r1", "r_middle1", "_zN_r_middle1" };\n'
     '\n'
     '// Model-space position of the first of the named nodes the model has\n'
     'static LTBOOL FindModelNode(HOBJECT hObj, const char **szNames, int nNames, LTVector &vPos)\n'
     '{\n'
     '\tModelLT *pModelLT = g_pLTClient->GetModelLT();\n'
     '\tfor(int i = 0; i < nNames; i++)\n'
     '\t{\n'
     '\t\tHMODELNODE hNode;\n'
     '\t\tLTransform tTransform;\n'
     '\t\tif(pModelLT->GetNode(hObj, (char*)szNames[i], hNode) == LT_OK &&\n'
     '\t\t   pModelLT->GetNodeTransform(hObj, hNode, tTransform, LTFALSE) == LT_OK)\n'
     '\t\t{\n'
     '\t\t\tg_pLTClient->GetTransformLT()->GetPos(tTransform, vPos);\n'
     '\t\t\treturn LTTRUE;\n'
     '\t\t}\n'
     '\t}\n'
     '\treturn LTFALSE;\n'
     '}\n'),
    ('''	// The model is made to be seen from its origin with the gun at the weapon offset, so put
	// that viewpoint behind the controller and the gun lands in the hand. VRGunOffset (world
	// units, in the controller's frame: right, up, forward) and VRGunScale fine-tune the fit.
	LTFLOAT fScale = g_vtVRGunScale.GetFloat();
	LTVector vOffset = pWeapon->GetWeaponOffset() * fScale;
	vOffset -= LTVector(g_vtVRGunOffsetX.GetFloat(), g_vtVRGunOffsetY.GetFloat(), g_vtVRGunOffsetZ.GetFloat());
	LTVector vOrigin = m_vAimPos - Rotate(m_rAimRot, vOffset);
''',
     '''	// The model's origin is the flat-screen eye point, with the arms reaching forward from it,
	// so placing the origin at the controller puts the hand an arm's length ahead of the real
	// one. Instead the model's right palm (between its wrist and middle knuckle, tracked every
	// frame so it stays put through the animations) goes on the controller. Models without hand
	// nodes use VRGrip. VRGunOffset (world units, in the controller's frame: right, up, forward)
	// and VRGunScale fine-tune the fit.
	LTFLOAT fScale = g_vtVRGunScale.GetFloat();
	LTVector vWrist, vKnuckle, vPalm;
	const char *szGrip = "default";
	LTBOOL bWrist = FindModelNode(hWeapon, s_szWristNodes, 4, vWrist);
	LTBOOL bKnuckle = FindModelNode(hWeapon, s_szKnuckleNodes, 4, vKnuckle);
	if(bWrist && bKnuckle)
	{
		vPalm = (vWrist + vKnuckle) * 0.5f;
		szGrip = "palm";
	}
	else if(bWrist)
	{
		vPalm = vWrist;
		szGrip = "wrist";
	}
	else
		vPalm = LTVector(g_vtVRGripX.GetFloat(), g_vtVRGripY.GetFloat(), g_vtVRGripZ.GetFloat());

	LTVector vOffset = vPalm * fScale;
	vOffset -= LTVector(g_vtVRGunOffsetX.GetFloat(), g_vtVRGunOffsetY.GetFloat(), g_vtVRGunOffsetZ.GetFloat());
	LTVector vOrigin = m_vAimPos - Rotate(m_rAimRot, vOffset);

	// Log the grip whenever the weapon changes, for tuning
	static WEAPON *s_pLastWeapon = LTNULL;
	if(pWeapon->GetWeapon() != s_pLastWeapon)
	{
		s_pLastWeapon = pWeapon->GetWeapon();
		VRLog("Weapon %s: grip %s (%.1f %.1f %.1f), wrist %d (%.1f %.1f %.1f), knuckle %d (%.1f %.1f %.1f)",
			s_pLastWeapon ? s_pLastWeapon->szName : "?", szGrip, vPalm.x, vPalm.y, vPalm.z,
			bWrist, bWrist ? vWrist.x : 0.0f, bWrist ? vWrist.y : 0.0f, bWrist ? vWrist.z : 0.0f,
			bKnuckle, bKnuckle ? vKnuckle.x : 0.0f, bKnuckle ? vKnuckle.y : 0.0f, bKnuckle ? vKnuckle.z : 0.0f);
	}
'''),
])
print('patched')
