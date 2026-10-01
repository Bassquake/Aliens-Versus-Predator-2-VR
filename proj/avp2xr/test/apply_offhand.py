# Adds the left (off-hand) controller grip pose to the proxy (API v8).
def patch(path, pairs):
    s = open(path, encoding='utf-8').read()
    for a, b in pairs:
        assert s.count(a) == 1, (path, a[:80], s.count(a))
        s = s.replace(a, b)
    open(path, 'w', encoding='utf-8').write(s)

patch('K:/Coding/Aliens-Versus-Predator-2-VR/proj/avp2xr/avp2xr_api.h', [
    ('#define AVP2XR_API_VERSION 7', '#define AVP2XR_API_VERSION 8'),
    ('''	unsigned int buttons;     // AVP2XR_BTN_* held this frame
};''', '''	unsigned int buttons;     // AVP2XR_BTN_* held this frame
	int offValid;             // the off-hand (left) controller is tracked
	float offOrientation[4];  // its grip pose: x, y, z, w
	float offPosition[3];
};'''),
])

patch('K:/Coding/Aliens-Versus-Predator-2-VR/proj/avp2xr/avp2xr.cpp', [
    ('''static XrSpace g_aimSpace = XR_NULL_HANDLE;''', '''static XrSpace g_aimSpace = XR_NULL_HANDLE;
static XrAction g_offAction = XR_NULL_HANDLE;   // left controller grip pose (two-handed aiming)
static XrSpace g_offSpace = XR_NULL_HANDLE;'''),
    ('''	XrActionSuggestedBinding b[3 + sizeof(g_buttons) / sizeof(g_buttons[0])];
	uint32_t n = 0;''', '''	XrActionSuggestedBinding b[4 + sizeof(g_buttons) / sizeof(g_buttons[0])];
	uint32_t n = 0;
	b[n++] = { g_offAction, Path("/user/hand/left/input/grip/pose") };'''),
    ('''	r = x_xrCreateAction(g_actionSet, &aci, &g_aimAction);
	if (XR_FAILED(r)) { Log("Input: creating the aim action failed: %s", XrStr(r)); return; }
''', '''	r = x_xrCreateAction(g_actionSet, &aci, &g_aimAction);
	if (XR_FAILED(r)) { Log("Input: creating the aim action failed: %s", XrStr(r)); return; }

	strcpy_s(aci.actionName, "offhand");
	strcpy_s(aci.localizedActionName, "Off hand");
	r = x_xrCreateAction(g_actionSet, &aci, &g_offAction);
	if (XR_FAILED(r)) { Log("Input: creating the off-hand action failed: %s", XrStr(r)); return; }
'''),
    ('''	r = x_xrCreateActionSpace(g_session, &sci, &g_aimSpace);
	if (XR_FAILED(r)) { Log("Input: creating the aim space failed: %s", XrStr(r)); return; }
''', '''	r = x_xrCreateActionSpace(g_session, &sci, &g_aimSpace);
	if (XR_FAILED(r)) { Log("Input: creating the aim space failed: %s", XrStr(r)); return; }

	sci.action = g_offAction;
	r = x_xrCreateActionSpace(g_session, &sci, &g_offSpace);
	if (XR_FAILED(r)) { Log("Input: creating the off-hand space failed: %s", XrStr(r)); return; }
'''),
    ('''	else
		g_input.aimValid = 0;
''', '''	else
		g_input.aimValid = 0;

	XrSpaceLocation off = { XR_TYPE_SPACE_LOCATION };
	if (XR_SUCCEEDED(x_xrLocateSpace(g_offSpace, g_space, g_frameState.predictedDisplayTime, &off)) &&
		(off.locationFlags & needed) == needed)
	{
		g_input.offValid = 1;
		g_input.offOrientation[0] = off.pose.orientation.x;
		g_input.offOrientation[1] = off.pose.orientation.y;
		g_input.offOrientation[2] = off.pose.orientation.z;
		g_input.offOrientation[3] = off.pose.orientation.w;
		g_input.offPosition[0] = off.pose.position.x;
		g_input.offPosition[1] = off.pose.position.y;
		g_input.offPosition[2] = off.pose.position.z;
	}
	else
		g_input.offValid = 0;
'''),
    ('''		g_input.aimValid = g_input.moveValid = g_input.turnValid = 0;
		g_input.buttons = 0;''', '''		g_input.aimValid = g_input.moveValid = g_input.turnValid = g_input.offValid = 0;
		g_input.buttons = 0;'''),
    ('''	g_aimSpace = XR_NULL_HANDLE;
	g_inputReady = false;''', '''	g_aimSpace = XR_NULL_HANDLE;
	g_offAction = XR_NULL_HANDLE;
	g_offSpace = XR_NULL_HANDLE;
	g_inputReady = false;'''),
])
print('ok')
