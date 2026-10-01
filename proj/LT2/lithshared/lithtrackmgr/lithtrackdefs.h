//*********************************************************************************
//*********************************************************************************
// Project:		Lith Tech Track Manager
// Purpose:		A general method for scripted events and cutscenes
//*********************************************************************************
// File:		LithTrackDefs.h
// Created:		November 23, 1999
// Updated:		November 23, 1999
// Author:		Andy Mattingly
//*********************************************************************************
//*********************************************************************************

#ifndef _LITHTECH_TRACK_DEFS_H_
#define _LITHTECH_TRACK_DEFS_H_

//*********************************************************************************

#include "ltbasedefs.h"

//*********************************************************************************

#ifdef _CLIENTBUILD
	#include "clientheaders.h"
#else
	#include "serverheaders.h"
#endif

//*********************************************************************************

#define TRACKMGR_GUI_BAR_EDGE						2
#define TRACKMGR_GUI_BAR_EDGE_SPACE					4
#define TRACKMGR_GUI_BAR_MAX_ITEMS					32

#define TRACKMGR_DEFAULT_MOUSE_CURSOR_THICKNESS		2
#define TRACKMGR_DEFAULT_MOUSE_CURSOR_SIZE			20
#define TRACKMGR_DEFAULT_FONT_WIDTH					5
#define TRACKMGR_DEFAULT_FONT_HEIGHT				12
#define TRACKMGR_DEFAULT_SCREEN_WIDTH				640
#define TRACKMGR_DEFAULT_SCREEN_HEIGHT				480
#define TRACKMGR_DEFAULT_CURSOR_SPEED				1.0f
#define TRACKMGR_DEFAULT_CAMERA_ROT_SPEED			0.5f
#define TRACKMGR_DEFAULT_CAMERA_MOVE_SPEED			250.0f
#define TRACKMGR_DEFAULT_CAMERA_FOV_X				90.0f
#define TRACKMGR_DEFAULT_CAMERA_FOV_Y				75.0f
#define TRACKMGR_DEFAULT_TRACK_ROT_SPEED			0.5f
#define TRACKMGR_DEFAULT_TRACK_MOVE_SPEED			100.0f
#define TRACKMGR_DEFAULT_KEY_ROT_SPEED				0.25f
#define TRACKMGR_DEFAULT_KEY_MOVE_SPEED				50.0f
#define TRACKMGR_DEFAULT_PATH_DETAIL				12
#define TRACKMGR_DEFAULT_NEW_KEY_POS_OFFSET			100.0f
#define TRACKMGR_DEFAULT_NEW_KEY_TIME_OFFSET		3.0f
#define TRACKMGR_DEFAULT_TRACK_SELECT_FAST			5
#define TRACKMGR_DEFAULT_KEY_SELECT_FAST			5

#define TRACKMGR_MAX_MOUSE_CURSOR_SIZE				128
#define TRACKMGR_MAX_GUIBARS						32
#define TRACKMGR_MAX_TRACKS							512
#define TRACKMGR_MAX_TRACK_KEYS						128
#define TRACKMGR_MAX_PATH_DETAIL					64

#define TRACKMGR_DRAW_KEY_LINES						12
#define TRACKMGR_DRAW_KEY_DIM						5.0f
#define TRACKMGR_DRAW_MARKER_LINES					8

#define TRACKMGR_DATA_FILE							"TrackMgr.dat"
#define TRACKMGR_CONFIG_FILE						"TrackMgr.cfg"
#define TRACKMGR_TRACK_NAME							"Track000"

//*********************************************************************************

#define GUI_JUSTIFY_LEFT					0
#define GUI_JUSTIFY_CENTER					1
#define GUI_JUSTIFY_RIGHT					2

//*********************************************************************************

#define GUI_BAR_NONE						0
#define GUI_BAR_EDIT						1
#define GUI_BAR_MANAGER						2
#define GUI_BAR_CAMERA						3
#define GUI_BAR_TRACK						4
#define GUI_BAR_KEY							5
#define GUI_BAR_HELP						6

//*********************************************************************************

#define GUI_CONTROL_NONE					0x00
#define GUI_CONTROL_CAMERA					0x01
#define GUI_CONTROL_EDIT_TRACK				0x02
#define GUI_CONTROL_TRACK					0x04
#define GUI_CONTROL_KEY						0x08

#define GUI_CONTROL_LOCK_POS_X				0x01
#define GUI_CONTROL_LOCK_POS_Y				0x02
#define GUI_CONTROL_LOCK_POS_Z				0x04
#define GUI_CONTROL_LOCK_ROT_X				0x10
#define GUI_CONTROL_LOCK_ROT_Y				0x20
#define GUI_CONTROL_LOCK_ROT_Z				0x40

//*********************************************************************************

#define GUI_DEVICE_NONE						0x00
#define GUI_DEVICE_KEYBOARD					0x01
#define GUI_DEVICE_MOUSE					0x02

//*********************************************************************************

#define VAR_COMMAND_TRACKEDIT				"TrackEdit"

#define COM_GUI_MANAGER						"Manager"
#define COM_GUI_DETAILS						"Details"
#define COM_GUI_HELP						"Help"
#define COM_GUI_CURSOR						"Cursor"
#define COM_GUI_CAMERA						"Camera"
#define COM_GUI_FONT						"Font"
#define COM_GUI_TRACK						"Track"
#define COM_GUI_KEY							"Key"
#define COM_TRK_NEW							"New"
#define COM_TRK_DELETE						"Delete"
#define COM_TRK_COPY						"Copy"
#define COM_TRK_RENAME						"Rename"
#define COM_TRK_SELECT						"Select"
#define COM_EXT_COLOR						"Color"
#define COM_EXT_SIZE						"Size"
#define COM_EXT_SCALE						"Scale"
#define COM_EXT_THICKNESS					"Thickness"
#define COM_EXT_SPEED						"Speed"
#define COM_EXT_FOV							"FOV"
#define COM_EXT_TYPE						"Type"
#define COM_EXT_WIDTH						"Width"
#define COM_EXT_HEIGHT						"Height"
#define COM_VAL_ON							"On"
#define COM_VAL_OFF							"Off"
#define COM_VAL_ALL							"All"
#define COM_VAL_HELP						"?"

#define MSG_INVALID_PARAMS					"LithTrackMgr :: Invalid parameters!"

#define HELP_COM_TRACKEDIT					"TrackEdit [On/Off]"
#define HELP_COM_CURSOR						"Cursor [Size/Thickness/Color/Speed]"
#define HELP_COM_CAMERA						"Camera [Speed/Scale/FOV]"
#define HELP_COM_FONT						"Font [Type/Width/Height]"
#define HELP_COM_TRACK						"Track []"
#define HELP_COM_KEY						"Key []"

#define HELP_EXT_COLOR						"Color [0.0 - 1.0] [0.0 - 1.0] [0.0 - 1.0]"
#define HELP_EXT_SIZE						"Size [<positive integer>]"
#define HELP_EXT_SCALE						"Scale [<positive float>]"
#define HELP_EXT_THICKNESS					"Thickness [<positive integer>]"
#define HELP_EXT_SPEED						"Speed [<positive float>]"
#define HELP_EXT_FOV						"FOV [<positive float>] [<positive float>]"
#define HELP_EXT_TYPE						"Type [<string>]"
#define HELP_EXT_WIDTH						"Width [<positive integer>]"
#define HELP_EXT_HEIGHT						"Height [<positive integer>]"

#define HELP_TEXT_GENERAL1					"All commands in the console are formatted in this fashion:\n'TrackEdit <command> <ext command> <values>'"
#define HELP_TEXT_GENERAL2					"Type a '?' after commands in the console to get help on them.\nExample: 'TrackEdit Camera ?' will give you a list of Camera commands."
#define HELP_TEXT_COMMANDS					"COMMANDS: Track, Key, Camera, Cursor, Font"
#define HELP_TEXT_EXT_COMMANDS				"EXT COMMANDS: Color, Size, Scale, Thickness, Speed, FOV, Type, Width, Height"

#define TITLE_TRACK_EDIT					"TrackEdit:"
#define TITLE_TRACK_MANAGER					"Track Manager:"
#define TITLE_TRACK_DETAILS					"Track Details:"
#define TITLE_TRACK_KEY_DETAILS				"Key Details:"
#define TITLE_TRACK_CAMERA					"Camera Details:"
#define TITLE_TRACK_HELP					"TrackEdit Help:"

#define BUTTON_MANAGER						"Manager"
#define BUTTON_TRACK						"Track"
#define BUTTON_KEY							"Key"
#define BUTTON_CAMERA						"Camera"
#define BUTTON_HELP							"Help"
#define BUTTON_NEW							"New"
#define BUTTON_INSERT						"Insert"
#define BUTTON_COPY							"Copy"
#define BUTTON_DELETE						"Delete"
#define BUTTON_CONTROL						"Control"
#define BUTTON_DISPLAY						"Display"
#define BUTTON_ALIGN						"Align"
#define BUTTON_RESET						"Reset"
#define BUTTON_REFERENCE					"Reference"
#define BUTTON_PREV_FAST					"<<"
#define BUTTON_PREV							"<"
#define BUTTON_SCROLL						"*"
#define BUTTON_NEXT							">"
#define BUTTON_NEXT_FAST					">>"
#define BUTTON_X							"X"
#define BUTTON_Y							"Y"
#define BUTTON_Z							"Z"

#define TEXT_POSITION						"Position: x = %.2f  y = %.2f  z = %.2f"
#define TEXT_ROTATION						"Rotation: pitch = %.2f  yaw = %.2f  roll = %.2f"
#define TEXT_FOV							"FOV: x = %.2f  y = %.2f"
#define TEXT_TRACK							"Track Select:"
#define TEXT_KEY							"Key Select:"
#define TEXT_RANGE							"%d / %d"
#define TEXT_NAME							"Name: %s"
#define TEXT_LOCK_POS						"Lock Pos: "
#define TEXT_LOCK_ROT						"Lock Rot: "
#define TEXT_FLEX							"Flex: %.2f"
#define TEXT_TIME							"Time: %.2f"
#define TEXT_REFERENCE_TYPE					": %s"
#define TEXT_REFERENCE_POS					" %.2f %.2f %.2f"

#define ERROR_NEW_TRACK						"ERROR!! The new track could not be created!"
#define ERROR_NEW_KEY						"ERROR!! The new keyframe could not be created!"
#define ERROR_DELETE_TRACK					"ERROR!! Tried to delete an invalid track!"
#define ERROR_DELETE_KEY					"ERROR!! Tried to delete an invalid keyframe!"
#define ERROR_COPY_TRACK_1					"ERROR!! Tried to copy an invalid track!"
#define ERROR_COPY_TRACK_2					"ERROR!! The copy destination name is already used!"
#define ERROR_TRACK_NULL					"ERROR!! A NULL track was returned!"
#define ERROR_KEY_NULL						"ERROR!! A NULL keyframe was returned!"
#define ERROR_TRACK_OUT_OF_RANGE			"ERROR!! A track ID was out of range!"
#define ERROR_KEY_OUT_OF_RANGE				"ERROR!! A keyframe ID was out of range!"

//*********************************************************************************

#ifdef _CLIENTBUILD
	#define INTERFACE	CClientDE
#else
	#define INTERFACE	CServerDE
#endif

//*********************************************************************************

#endif
