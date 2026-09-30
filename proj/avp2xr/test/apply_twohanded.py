# Two-handed aiming in the VR cshell.
def patch(path, pairs):
    s = open(path, encoding='utf-8').read()
    for a, b in pairs:
        assert s.count(a) == 1, (path, a[:80], s.count(a))
        s = s.replace(a, b)
    open(path, 'w', encoding='utf-8').write(s)

patch('K:/Coding/Aliens-Versus-Predator-2-VR/proj/AVP2/ClientShellDLL/VRMgr.cpp', [
    ('''static VarTrack		g_vtVRGunScale;
''', '''static VarTrack		g_vtVRGunScale;
static VarTrack		g_vtVRTwoHanded;
'''),
    ('''	g_vtVRGunScale.Init(g_pLTClient, "VRGunScale", LTNULL, 1.0f);
''', '''	g_vtVRGunScale.Init(g_pLTClient, "VRGunScale", LTNULL, 1.0f);
	g_vtVRTwoHanded.Init(g_pLTClient, "VRTwoHanded", LTNULL, 1.0f);
'''),
    ('''static LTVector Rotate(const LTRotation &rRot, const LTVector &v)''', '''// Two-handed aiming engages when the off hand is in front of the gun hand: between these
// distances (metres) and within this angle of where the gun hand points. It lets go outside
// the wider limits, so it doesn't flicker at the edges.
#define VR_TWOHAND_MIN			0.15f
#define VR_TWOHAND_MAX			0.75f
#define VR_TWOHAND_ANGLE		40.0f
#define VR_TWOHAND_RELEASE_MIN	0.10f
#define VR_TWOHAND_RELEASE_MAX	0.90f
#define VR_TWOHAND_RELEASE_ANGLE 60.0f

// Standard (right-hand rule) cross product. LTVector::Cross returns the reverse.
static LTVector CrossStd(const LTVector &a, const LTVector &b)
{
	return LTVector(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

// Rotation whose right/up/forward vectors are vR/vU/vF (orthonormal): the rotation matrix has
// them as columns (see quat_GetVectors). Tested against LithTech's math in avp2xr\\test\\convtest.
static LTRotation QuatFromBasis(const LTVector &vR, const LTVector &vU, const LTVector &vF)
{
	float m00 = vR.x, m10 = vR.y, m20 = vR.z, m01 = vU.x, m11 = vU.y, m21 = vU.z, m02 = vF.x, m12 = vF.y, m22 = vF.z;
	float q[4], t = m00 + m11 + m22;
	if(t > 0.0f)				{ float s = (float)sqrt(t + 1.0f) * 2.0f; q[3] = 0.25f * s; q[0] = (m21 - m12) / s; q[1] = (m02 - m20) / s; q[2] = (m10 - m01) / s; }
	else if(m00 > m11 && m00 > m22)	{ float s = (float)sqrt(1.0f + m00 - m11 - m22) * 2.0f; q[3] = (m21 - m12) / s; q[0] = 0.25f * s; q[1] = (m01 + m10) / s; q[2] = (m02 + m20) / s; }
	else if(m11 > m22)			{ float s = (float)sqrt(1.0f + m11 - m00 - m22) * 2.0f; q[3] = (m02 - m20) / s; q[0] = (m01 + m10) / s; q[1] = 0.25f * s; q[2] = (m12 + m21) / s; }
	else						{ float s = (float)sqrt(1.0f + m22 - m00 - m11) * 2.0f; q[3] = (m10 - m01) / s; q[0] = (m02 + m20) / s; q[1] = (m12 + m21) / s; q[2] = 0.25f * s; }
	return LTRotation(q[0], q[1], q[2], q[3]);
}

static LTVector Rotate(const LTRotation &rRot, const LTVector &v)'''),
    ('''		m_vAimPos = vPos + Rotate(rYaw, FromXrPos(m_Input.aimPosition, ref) * fScale);
		m_rAimRot = rYaw * FromXr(m_Input.aimOrientation);
		m_bAimWorld = LTTRUE;
''', '''		m_vAimPos = vPos + Rotate(rYaw, FromXrPos(m_Input.aimPosition, ref) * fScale);
		m_rAimRot = rYaw * FromXr(m_Input.aimOrientation);
		m_bAimWorld = LTTRUE;

		// Two-handed aiming: with the off hand in front of the gun hand, the gun points from
		// the gun hand to the off hand, keeping the gun hand's roll.
		m_bTwoHanded = m_bTwoHanded && m_Input.offValid;
		if(m_Input.offValid && g_vtVRTwoHanded.GetFloat() != 0.0f)
		{
			LTVector vOff = vPos + Rotate(rYaw, FromXrPos(m_Input.offPosition, ref) * fScale);
			LTVector vToOff = vOff - m_vAimPos;
			LTFLOAT fLen = vToOff.Mag();
			LTFLOAT fMetres = fLen / fScale;
			LTVector vR, vU, vF;
			LTRotation rAim = m_rAimRot;
			g_pLTClient->GetMathLT()->GetRotationVectors(rAim, vR, vU, vF);
			LTFLOAT fCos = (fLen > 0.0001f) ? vToOff.Dot(vF) / fLen : -1.0f;

			if(m_bTwoHanded)
				m_bTwoHanded = fMetres > VR_TWOHAND_RELEASE_MIN && fMetres < VR_TWOHAND_RELEASE_MAX &&
							   fCos > (LTFLOAT)cos(MATH_DEGREES_TO_RADIANS(VR_TWOHAND_RELEASE_ANGLE));
			else
				m_bTwoHanded = fMetres > VR_TWOHAND_MIN && fMetres < VR_TWOHAND_MAX &&
							   fCos > (LTFLOAT)cos(MATH_DEGREES_TO_RADIANS(VR_TWOHAND_ANGLE));

			if(m_bTwoHanded)
			{
				LTVector vForward = vToOff / fLen;
				LTVector vUp = vU - vForward * vU.Dot(vForward);
				if(vUp.MagSqr() > 0.0001f)
				{
					vUp.Norm();
					m_rAimRot = QuatFromBasis(CrossStd(vUp, vForward), vUp, vForward);
				}
			}
		}
		else
			m_bTwoHanded = LTFALSE;
'''),
    ('''	m_bSnapArmed = LTTRUE;
	m_bCycleArmed = LTTRUE;''', '''	m_bSnapArmed = LTTRUE;
	m_bCycleArmed = LTTRUE;
	m_bTwoHanded = LTFALSE;'''),
])

patch('K:/Coding/Aliens-Versus-Predator-2-VR/proj/AVP2/ClientShellDLL/VRMgr.h', [
    ('''		LTBOOL			m_bCycleArmed;			// ...and since the last weapon cycle''', '''		LTBOOL			m_bCycleArmed;			// ...and since the last weapon cycle
		LTBOOL			m_bTwoHanded;			// the off hand is steadying the gun (aim runs hand to hand)'''),
    ('''//   VRGunOffsetX/Y/Z  moves the weapon in the controller's frame (world units: right, up, forward)''', '''//   VRGunOffsetX/Y/Z  moves the weapon in the controller's frame (world units: right, up, forward)
//   VRTwoHanded   1 = with the left hand in front of the right, the gun aims from right hand to left'''),
])
print('ok')
