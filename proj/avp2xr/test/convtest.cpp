// Checks the OpenXR -> LithTech pose conversion used by VRMgr, using LithTech's quaternion code.
#include <stdio.h>
#include <math.h>
#include "ltquatbase.h"

static void Fwd(const float q[4], float out[3]) { float r[3], u[3]; quat_GetVectors(q, r, u, out); }

// Same as VRMgr.cpp QuatFromBasis: quaternion (x, y, z, w) whose rotation matrix has columns r, u, f.
static void QuatFromBasis(const float r[3], const float u[3], const float f[3], float q[4])
{
	float m00 = r[0], m10 = r[1], m20 = r[2], m01 = u[0], m11 = u[1], m21 = u[2], m02 = f[0], m12 = f[1], m22 = f[2];
	float t = m00 + m11 + m22;
	if (t > 0.0f) { float s = sqrtf(t + 1.0f) * 2.0f; q[3] = 0.25f * s; q[0] = (m21 - m12) / s; q[1] = (m02 - m20) / s; q[2] = (m10 - m01) / s; }
	else if (m00 > m11 && m00 > m22) { float s = sqrtf(1.0f + m00 - m11 - m22) * 2.0f; q[3] = (m21 - m12) / s; q[0] = 0.25f * s; q[1] = (m01 + m10) / s; q[2] = (m02 + m20) / s; }
	else if (m11 > m22) { float s = sqrtf(1.0f + m11 - m00 - m22) * 2.0f; q[3] = (m02 - m20) / s; q[0] = (m01 + m10) / s; q[1] = 0.25f * s; q[2] = (m12 + m21) / s; }
	else { float s = sqrtf(1.0f + m22 - m00 - m11) * 2.0f; q[3] = (m10 - m01) / s; q[0] = (m02 + m20) / s; q[1] = (m12 + m21) / s; q[2] = 0.25f * s; }
}

// OpenXR quaternion -> LithTech, as in VRMgr::GetEyeCamera
static void Conv(const float xr[4], float lt[4]) { lt[0] = -xr[0]; lt[1] = -xr[1]; lt[2] = xr[2]; lt[3] = xr[3]; }

int main()
{
	const float a = 0.5f;  // ~28.6 degrees
	struct { const char* name; float xr[4]; const char* expect; } cases[] = {
		{ "head turned left (+yaw about +Y in OpenXR)", { 0, sinf(a / 2), 0, cosf(a / 2) }, "forward.x < 0 (left)" },
		{ "head pitched up (+rotation about +X in OpenXR)", { sinf(a / 2), 0, 0, cosf(a / 2) }, "forward.y > 0 (up)" },
	};
	int fails = 0;
	for (auto& c : cases)
	{
		float lt[4], f[3];
		Conv(c.xr, lt);
		Fwd(lt, f);
		bool ok = (c.expect[8] == 'x') ? f[0] < -0.4f : f[1] > 0.4f;
		printf("%-48s forward = (%.3f, %.3f, %.3f)  expect %s: %s\n", c.name, f[0], f[1], f[2], c.expect, ok ? "OK" : "FAIL");
		fails += !ok;
	}

	// Composition: mouse yaw 90 deg right (LithTech +Y), head turned left 90 deg -> facing forward (+Z).
	float mouse[4] = { 0, sinf(0.785398f), 0, cosf(0.785398f) };
	float headXr[4] = { 0, sinf(0.785398f), 0, cosf(0.785398f) }, head[4], eye[4], f[3];
	Conv(headXr, head);
	quat_Mul(eye, mouse, head);
	Fwd(eye, f);
	bool ok = f[2] > 0.99f;
	printf("%-48s forward = (%.3f, %.3f, %.3f)  expect +Z: %s\n", "mouse right 90 * head left 90", f[0], f[1], f[2], ok ? "OK" : "FAIL");
	fails += !ok;

	// Mouse yaw right 90, then head pitch up: should look up while facing +X.
	float headUpXr[4] = { sinf(a / 2), 0, 0, cosf(a / 2) }, headUp[4];
	Conv(headUpXr, headUp);
	quat_Mul(eye, mouse, headUp);
	Fwd(eye, f);
	ok = f[0] > 0.8f && f[1] > 0.4f;
	printf("%-48s forward = (%.3f, %.3f, %.3f)  expect +X and up: %s\n", "mouse right 90 * head up", f[0], f[1], f[2], ok ? "OK" : "FAIL");
	fails += !ok;
	// Yaw from the right vector survives heavy pitch: camera = yaw 30 (about +Y) * pitch 80 down (about +X).
	{
		float yaw[4] = { 0, sinf(0.261799f), 0, cosf(0.261799f) }, pitch[4] = { sinf(0.698132f), 0, 0, cosf(0.698132f) }, cam[4];
		quat_Mul(cam, yaw, pitch);
		float r[3], u[3], fw[3];
		quat_GetVectors(cam, r, u, fw);
		float y = atan2f(-r[2], r[0]) * 57.29578f;
		ok = fabsf(y - 30.0f) < 0.01f && fw[1] < -0.9f;
		printf("%-48s yaw = %.2f (forward y %.2f)  expect 30, looking down: %s\n", "camera yaw 30 pitched 80 down", y, fw[1], ok ? "OK" : "FAIL");
		fails += !ok;

		// Player view = anchor yaw * head(pitch up 20): forward should be yaw 30 and up 20.
		float hx[4] = { sinf(0.174533f), 0, 0, cosf(0.174533f) }, h[4], eye[4], ey[4] = { 0, sinf(0.261799f), 0, cosf(0.261799f) };
		Conv(hx, h);
		quat_Mul(eye, ey, h);
		Fwd(eye, fw);
		float ey2 = atan2f(fw[0], fw[2]) * 57.29578f, ep = asinf(fw[1]) * 57.29578f;
		ok = fabsf(ey2 - 30.0f) < 0.01f && fabsf(ep - 20.0f) < 0.01f;
		printf("%-48s yaw %.2f pitch %.2f  expect 30 / 20 up: %s\n", "yaw 30 * head up 20", ey2, ep, ok ? "OK" : "FAIL");
		fails += !ok;
	}
	// Stick directions (VRMgr::GetMoveDirection) match LithTech's right/forward for a head yaw of 70 deg.
	{
		float yaw = 1.22173f, q[4] = { 0, sinf(yaw / 2), 0, cosf(yaw / 2) }, r[3], u[3], fw[3];
		quat_GetVectors(q, r, u, fw);
		float mf[3] = { sinf(yaw), 0, cosf(yaw) }, mr[3] = { cosf(yaw), 0, -sinf(yaw) };
		float err = 0;
		for (int i = 0; i < 3; ++i) err += fabsf(mf[i] - fw[i]) + fabsf(mr[i] - r[i]);
		ok = err < 1e-4f;
		printf("%-48s stick up/right vs LithTech forward/right, error %.6f: %s\n", "move frame at head yaw 70", err, ok ? "OK" : "FAIL");
		fails += !ok;
	}

	// Body turn (PlayerMovement::UpdateRotation): facing +Z, aim towards (1, 0, 1) -> turn +45 and face it.
	{
		float body[4] = { 0, 0, 0, 1 }, r[3], u[3], fw[3];
		quat_GetVectors(body, r, u, fw);
		float d[3] = { 0.7071f, 0, 0.7071f };
		float c[3] = { fw[1] * d[2] - fw[2] * d[1], fw[2] * d[0] - fw[0] * d[2], fw[0] * d[1] - fw[1] * d[0] };
		float turn = atan2f(c[0] * u[0] + c[1] * u[1] + c[2] * u[2], fw[0] * d[0] + fw[1] * d[1] + fw[2] * d[2]);
		float s = sinf(turn / 2), tq[4] = { u[0] * s, u[1] * s, u[2] * s, cosf(turn / 2) }, out[4];
		quat_Mul(out, tq, body);
		Fwd(out, fw);
		ok = fabsf(turn * 57.29578f - 45.0f) < 0.01f && fabsf(fw[0] - 0.7071f) < 1e-3f && fabsf(fw[2] - 0.7071f) < 1e-3f;
		printf("%-48s turn %.2f, forward (%.3f %.3f %.3f)  expect +45 onto aim: %s\n", "body turn to aim", turn * 57.29578f, fw[0], fw[1], fw[2], ok ? "OK" : "FAIL");
		fails += !ok;
	}
	// Rotation from a right/up/forward basis (VRMgr RotationFromBasis): matrix columns, then back.
	{
		float q[4] = { 0.2f, -0.5f, 0.3f, 0.787401f }, r[3], u[3], fw[3], m[4][4] = {}, back[4], r2[3], u2[3], f2[3];
		quat_GetVectors(q, r, u, fw);
		for (int i = 0; i < 3; ++i) { m[i][0] = r[i]; m[i][1] = u[i]; m[i][2] = fw[i]; }
		m[3][3] = 1.0f;
		quat_ConvertFromMatrix(back, m);
		quat_GetVectors(back, r2, u2, f2);
		float err = 0;
		for (int i = 0; i < 3; ++i) err += fabsf(r[i] - r2[i]) + fabsf(u[i] - u2[i]) + fabsf(fw[i] - f2[i]);
		ok = err < 1e-4f;
		printf("%-48s vector error %.6f: %s\n", "rotation from basis round trip", err, ok ? "OK" : "FAIL");
		fails += !ok;
	}
	// QuatFromBasis round trips through LithTech's quat_GetVectors, including the non-trace branches.
	{
		float tests[4][4] = { { 0.2f, -0.5f, 0.3f, 0.787401f }, { 0.9f, 0.1f, 0.1f, 0.4f }, { 0.1f, 0.95f, 0.1f, 0.26f }, { 0.05f, 0.1f, 0.98f, 0.15f } };
		float worst = 0;
		for (auto& t : tests)
		{
			float n = sqrtf(t[0] * t[0] + t[1] * t[1] + t[2] * t[2] + t[3] * t[3]), q[4] = { t[0] / n, t[1] / n, t[2] / n, t[3] / n };
			float r[3], u[3], fw[3], back[4], r2[3], u2[3], f2[3];
			quat_GetVectors(q, r, u, fw);
			QuatFromBasis(r, u, fw, back);
			quat_GetVectors(back, r2, u2, f2);
			float err = 0;
			for (int i = 0; i < 3; ++i) err += fabsf(r[i] - r2[i]) + fabsf(u[i] - u2[i]) + fabsf(fw[i] - f2[i]);
			if (err > worst) worst = err;
		}
		ok = worst < 1e-4f;
		printf("%-48s worst vector error %.6f: %s\n", "QuatFromBasis (all 4 branches)", worst, ok ? "OK" : "FAIL");
		fails += !ok;
	}
	return fails;
}
