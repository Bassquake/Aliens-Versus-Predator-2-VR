# Adds controller buttons (API v7) to the avp2xr proxy and header.
import re

def patch(path, pairs):
    s = open(path, encoding='utf-8').read()
    for a, b in pairs:
        assert s.count(a) == 1, (path, a[:80], s.count(a))
        s = s.replace(a, b)
    open(path, 'w', encoding='utf-8').write(s)

patch('K:/Coding/Aliens-Versus-Predator-2-VR/proj/avp2xr/avp2xr_api.h', [
    ('#define AVP2XR_API_VERSION 6', '#define AVP2XR_API_VERSION 7'),
    ('''// SubmitStereo hudFlags''', '''// Avp2XrInput::buttons
#define AVP2XR_BTN_FIRE			0x01	// right trigger
#define AVP2XR_BTN_ALTFIRE		0x02	// right grip
#define AVP2XR_BTN_JUMP			0x04	// right A
#define AVP2XR_BTN_CROUCH		0x08	// right B
#define AVP2XR_BTN_USE			0x10	// left X (left A on Index)
#define AVP2XR_BTN_VISION		0x20	// left Y (left B on Index)
#define AVP2XR_BTN_RELOAD		0x40	// left grip

// SubmitStereo hudFlags'''),
    ('''	int turnValid;            // the turn stick is bound
	float turn;               // right thumbstick x: -1 (left) to 1 (right), for snap turning
};''', '''	int turnValid;            // the turn stick is bound
	float turn;               // right thumbstick x: -1 (left) to 1 (right), for snap turning
	float turnY;              // right thumbstick y: -1 (down) to 1 (up), for weapon cycling
	unsigned int buttons;     // AVP2XR_BTN_* held this frame
};'''),
])

patch('K:/Coding/Aliens-Versus-Predator-2-VR/proj/avp2xr/avp2xr.cpp', [
    ('\tX(xrGetActionStateVector2f) \\\n', '\tX(xrGetActionStateVector2f) \\\n\tX(xrGetActionStateBoolean) \\\n'),
    ('''static XrAction g_turnAction = XR_NULL_HANDLE;''', '''static XrAction g_turnAction = XR_NULL_HANDLE;

// Buttons: boolean actions, one per AVP2XR_BTN_* bit. Paths are per interaction profile:
// Touch, Index, WMR, Vive, simple. nullptr = not bound on that controller.
static struct ButtonAction
{
	const char* name;
	const char* localized;
	unsigned int bit;
	const char* paths[5];
	XrAction action;
} g_buttons[] =
{
	{ "fire", "Fire", AVP2XR_BTN_FIRE, { "/user/hand/right/input/trigger/value", "/user/hand/right/input/trigger/value",
		"/user/hand/right/input/trigger/value", "/user/hand/right/input/trigger/click", "/user/hand/right/input/select/click" } },
	{ "altfire", "Alt fire", AVP2XR_BTN_ALTFIRE, { "/user/hand/right/input/squeeze/value", "/user/hand/right/input/squeeze/value",
		"/user/hand/right/input/squeeze/click", "/user/hand/right/input/squeeze/click", nullptr } },
	{ "jump", "Jump", AVP2XR_BTN_JUMP, { "/user/hand/right/input/a/click", "/user/hand/right/input/a/click", nullptr, nullptr, nullptr } },
	{ "crouch", "Crouch", AVP2XR_BTN_CROUCH, { "/user/hand/right/input/b/click", "/user/hand/right/input/b/click", nullptr, nullptr, nullptr } },
	{ "use", "Use", AVP2XR_BTN_USE, { "/user/hand/left/input/x/click", "/user/hand/left/input/a/click", nullptr, nullptr, nullptr } },
	{ "vision", "Vision mode", AVP2XR_BTN_VISION, { "/user/hand/left/input/y/click", "/user/hand/left/input/b/click", nullptr, nullptr, nullptr } },
	{ "reload", "Reload", AVP2XR_BTN_RELOAD, { "/user/hand/left/input/squeeze/value", "/user/hand/left/input/squeeze/value",
		"/user/hand/left/input/squeeze/click", "/user/hand/left/input/squeeze/click", nullptr } },
};
static const size_t g_numButtons = sizeof(g_buttons) / sizeof(g_buttons[0]);'''),
    ('''static void SuggestBindings(const char* profile, const char* movePath, const char* aimPath, const char* turnPath)
{
	XrActionSuggestedBinding b[3];
	uint32_t n = 0;
	if (movePath)
		b[n++] = { g_moveAction, Path(movePath) };
	if (turnPath)
		b[n++] = { g_turnAction, Path(turnPath) };''', '''// nProfile indexes ButtonAction::paths (0 Touch, 1 Index, 2 WMR, 3 Vive, 4 simple).
static void SuggestBindings(const char* profile, int nProfile, const char* movePath, const char* aimPath, const char* turnPath)
{
	XrActionSuggestedBinding b[3 + sizeof(g_buttons) / sizeof(g_buttons[0])];
	uint32_t n = 0;
	if (movePath)
		b[n++] = { g_moveAction, Path(movePath) };
	if (turnPath)
		b[n++] = { g_turnAction, Path(turnPath) };
	for (size_t i = 0; i < g_numButtons; ++i)
		if (g_buttons[i].paths[nProfile])
			b[n++] = { g_buttons[i].action, Path(g_buttons[i].paths[nProfile]) };'''),
    ('''	aci.actionType = XR_ACTION_TYPE_POSE_INPUT;''', '''	aci.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
	for (size_t i = 0; i < g_numButtons; ++i)
	{
		strcpy_s(aci.actionName, g_buttons[i].name);
		strcpy_s(aci.localizedActionName, g_buttons[i].localized);
		r = x_xrCreateAction(g_actionSet, &aci, &g_buttons[i].action);
		if (XR_FAILED(r)) { Log("Input: creating the %s action failed: %s", g_buttons[i].name, XrStr(r)); return; }
	}

	aci.actionType = XR_ACTION_TYPE_POSE_INPUT;'''),
    ('SuggestBindings("/interaction_profiles/oculus/touch_controller", "', 'SuggestBindings("/interaction_profiles/oculus/touch_controller", 0, "'),
    ('SuggestBindings("/interaction_profiles/valve/index_controller", "', 'SuggestBindings("/interaction_profiles/valve/index_controller", 1, "'),
    ('SuggestBindings("/interaction_profiles/microsoft/motion_controller", "', 'SuggestBindings("/interaction_profiles/microsoft/motion_controller", 2, "'),
    ('SuggestBindings("/interaction_profiles/htc/vive_controller", "', 'SuggestBindings("/interaction_profiles/htc/vive_controller", 3, "'),
    ('SuggestBindings("/interaction_profiles/khr/simple_controller", nullptr,', 'SuggestBindings("/interaction_profiles/khr/simple_controller", 4, nullptr,'),
    ('''	Log("Input: left stick moves, right controller aims, right stick snap turns");''',
     '''	Log("Input: left stick moves, right controller aims, right stick snap turns / cycles weapons, buttons bound");'''),
    ('''		g_input.turnValid = 1;
		g_input.turn = turn.currentState.x;
	}
	else
		g_input.turnValid = 0;
''', '''		g_input.turnValid = 1;
		g_input.turn = turn.currentState.x;
		g_input.turnY = turn.currentState.y;
	}
	else
		g_input.turnValid = 0;

	g_input.buttons = 0;
	for (size_t i = 0; i < g_numButtons; ++i)
	{
		gi.action = g_buttons[i].action;
		XrActionStateBoolean bs = { XR_TYPE_ACTION_STATE_BOOLEAN };
		if (XR_SUCCEEDED(x_xrGetActionStateBoolean(g_session, &gi, &bs)) && bs.isActive && bs.currentState)
			g_input.buttons |= g_buttons[i].bit;
	}
'''),
    ('''		g_input.aimValid = g_input.moveValid = g_input.turnValid = 0;''', '''		g_input.aimValid = g_input.moveValid = g_input.turnValid = 0;
		g_input.buttons = 0;'''),
    ('''	g_moveAction = g_aimAction = g_turnAction = XR_NULL_HANDLE;''', '''	g_moveAction = g_aimAction = g_turnAction = XR_NULL_HANDLE;
	for (size_t i = 0; i < g_numButtons; ++i)
		g_buttons[i].action = XR_NULL_HANDLE;'''),
])
print('ok')
