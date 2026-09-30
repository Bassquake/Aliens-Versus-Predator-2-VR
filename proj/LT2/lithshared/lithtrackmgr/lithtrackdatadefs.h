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

#ifndef _LITHTECH_TRACK_DATA_DEFS_H_
#define _LITHTECH_TRACK_DATA_DEFS_H_

//*********************************************************************************

#include "ltbasedefs.h"
#include "lithtrackdefs.h"

//*********************************************************************************

struct DATA_CONTROL
{
	DATA_CONTROL::DATA_CONTROL();

	LTBOOL				m_bUpArrow;					// Up arrow pressed
	LTBOOL				m_bDownArrow;				// Down arrow pressed
	LTBOOL				m_bLeftArrow;				// Left arrow pressed
	LTBOOL				m_bRightArrow;				// Right arrow pressed
	LTBOOL				m_bShiftKey;				// Shift key pressed
	LTBOOL				m_bCtrlKey;					// Ctrl key pressed
	LTBOOL				m_bMouse0;					// Mouse button 0 pressed
	LTBOOL				m_bMouse1;					// Mouse button 1 pressed

	LTBOOL				m_bMouse0Clicked;			// Has the mouse button 0 been clicked?
	LTBOOL				m_bMouse1Clicked;			// Has the mouse button 1 been clicked?
};

//---------------------------------------------------------------------------------

inline DATA_CONTROL::DATA_CONTROL()
{
	memset(this, 0, sizeof(DATA_CONTROL));
}

//*********************************************************************************

struct DATA_CAMERA
{
	DATA_CAMERA::DATA_CAMERA();

	HLOCALOBJ			m_hObj;						// Camera for TrackMgr
	LTFLOAT				m_fPitch;					// Camera pitch
	LTFLOAT				m_fYaw;						// Camera yaw
	LTFLOAT				m_fRoll;					// Camera roll
	LTFLOAT				m_fRotSpeed;				// Camera rotation speed (scale)
	LTFLOAT				m_fMoveSpeed;				// Speed for the camera to move per second
	LTFLOAT				m_fFOVX;					// X value for field of view
	LTFLOAT				m_fFOVY;					// Y value for field of view
	LTVector			m_vPos;						// Current position of the camera

	DBYTE				m_byLockControls;			// Which controls are locked?
};

//---------------------------------------------------------------------------------

inline DATA_CAMERA::DATA_CAMERA()
{
	memset(this, 0, sizeof(DATA_CAMERA));

	m_fRotSpeed		= TRACKMGR_DEFAULT_CAMERA_ROT_SPEED;
	m_fMoveSpeed	= TRACKMGR_DEFAULT_CAMERA_MOVE_SPEED;
	m_fFOVX			= TRACKMGR_DEFAULT_CAMERA_FOV_X;
	m_fFOVY			= TRACKMGR_DEFAULT_CAMERA_FOV_Y;
}

//*********************************************************************************

struct DATA_CURSOR
{
	DATA_CURSOR::DATA_CURSOR();

	HSURFACE			m_hSurf;					// Surface of the mouse cursor bitmap
	int					m_nX;						// X location of the mouse cursor
	int					m_nY;						// Y location of the mouse cursor
	LTFLOAT				m_fSpeed;					// Speed scale of the mouse cursor
	HLTCOLOR			m_hColor;					// Color of the mouse cursor
	int					m_nSize;					// Size (in pixels) of the mouse cursor <square>
	int					m_nThickness;				// Thickness of the mouse cursor crosshair bitmap
};

//---------------------------------------------------------------------------------

inline DATA_CURSOR::DATA_CURSOR()
{
	memset(this, 0, sizeof(DATA_CURSOR));

	m_fSpeed = TRACKMGR_DEFAULT_CURSOR_SPEED;
	m_nSize = TRACKMGR_DEFAULT_MOUSE_CURSOR_SIZE;
	m_nThickness = TRACKMGR_DEFAULT_MOUSE_CURSOR_THICKNESS;
}

//*********************************************************************************

struct DATA_TRACK
{
	DATA_TRACK::DATA_TRACK();

	LTFLOAT				m_fRotSpeed;				// Track rotation speed (scale)
	LTFLOAT				m_fMoveSpeed;				// Speed for the track to move per second

	DBYTE				m_byLockControls;			// Which controls are locked?
};

//---------------------------------------------------------------------------------

inline DATA_TRACK::DATA_TRACK()
{
	memset(this, 0, sizeof(DATA_TRACK));

	m_fRotSpeed		= TRACKMGR_DEFAULT_TRACK_ROT_SPEED;
	m_fMoveSpeed	= TRACKMGR_DEFAULT_TRACK_MOVE_SPEED;
}

//*********************************************************************************

struct DATA_KEY
{
	DATA_KEY::DATA_KEY();

	LTFLOAT				m_fRotSpeed;				// Key rotation speed (scale)
	LTFLOAT				m_fMoveSpeed;				// Speed for the key to move per second

	DBYTE				m_byLockControls;			// Which controls are locked?
};

//---------------------------------------------------------------------------------

inline DATA_KEY::DATA_KEY()
{
	memset(this, 0, sizeof(DATA_KEY));

	m_fRotSpeed		= TRACKMGR_DEFAULT_KEY_ROT_SPEED;
	m_fMoveSpeed	= TRACKMGR_DEFAULT_KEY_MOVE_SPEED;
}

//*********************************************************************************

struct DATA_ALIGN
{
	DATA_ALIGN::DATA_ALIGN();

	HLOCALOBJ			m_hObj;						// Object to align to
	LTVector			m_vPos;						// Position of the object or point
	LTRotation			m_rRot;						// Rotation of the object or plane

	LTBOOL				m_bCamera;					// Align to the camera?
};

//---------------------------------------------------------------------------------

inline DATA_ALIGN::DATA_ALIGN()
{
	memset(this, 0, sizeof(DATA_ALIGN));

	m_bCamera = LTTRUE;
}

//*********************************************************************************

#endif // _LITHTECH_TRACK_DATA_DEFS_H_
