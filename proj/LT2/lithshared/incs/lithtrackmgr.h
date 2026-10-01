//*********************************************************************************
//*********************************************************************************
// Project:		Lith Tech Track Manager
// Purpose:		A general method for scripted events and cutscenes
//*********************************************************************************
// File:		LithTrackMgr.h
// Created:		November 23, 1999
// Updated:		November 23, 1999
// Author:		Andy Mattingly
//*********************************************************************************
//*********************************************************************************

#ifndef _LITHTECH_TRACK_MGR_H_
#define _LITHTECH_TRACK_MGR_H_

//#ifdef _LITHTECH2

//*********************************************************************************

#include "../lithtrackmgr/lithtrackdefs.h"
#include "../lithtrackmgr/lithtrackbaredit.h"
#include "../lithtrackmgr/lithtrackbarcamera.h"
#include "../lithtrackmgr/lithtrackbartrack.h"
#include "../lithtrackmgr/lithtrackbarkey.h"
#include "../lithtrackmgr/lithtrackbarhelp.h"
#include "../lithtrackmgr/lithtracklist.h"
#include "../lithtrackmgr/lithtrackdrawobj.h"

//*********************************************************************************

#ifndef NO_PRAGMA_LIBS
	#ifdef _CLIENTBUILD
		#ifdef _DEBUG				 
			#pragma comment (lib, "\\proj\\libs\\debug\\lithtrackmgrclient.lib")
		#else
			#pragma comment (lib, "\\proj\\libs\\release\\lithtrackmgrclient.lib")
		#endif
	#else
		#ifdef _DEBUG				 
			#pragma comment (lib, "\\proj\\libs\\debug\\lithtrackmgr.lib")
		#else
			#pragma comment (lib, "\\proj\\libs\\release\\lithtrackmgr.lib")
		#endif
	#endif
#endif

//*********************************************************************************

class LithTrackMgr
{
	public:
		// Constructors and destructors
		LithTrackMgr();
		~LithTrackMgr();

//---------------------------------------------------------------------------------
// Public member functions
//---------------------------------------------------------------------------------

	public:
		// Shared public functions

		DBOOL	Init(INTERFACE *pInterface = DNULL);		// Init variables and allocate memory
		void	Term();										// Terminate any allocated memory

		DBOOL	Update();									// General update

		// Message handling
//		DBOOL	SendMessage(DBYTE nID);						// Handle sending engine messages
//		DBOOL	OnMessage(DBYTE nID, HMESSAGEREAD hMsg);	// Handle receiving engine messages

		void	Send_To_Console(char *szStr, DBOOL bInvalid = DFALSE);	// Sends a message to the console

		INTERFACE*		Interface()		{ return m_pInterface; }		// Retrieve the interface
		DBOOL	Editing()				{ return m_bEditMode; }			// Are we editing?
		LithTrackList&	TrackList()		{ return m_tList;	}			// Get the track list

		// Calculation functions
		void	Rot2Vec(DRotation &rRot, DVector &vAngles);	// Converts a DRotation into euler angles

		// Interface drawing functions
#ifdef _CLIENTBUILD
	public:
		// Client specific public functions

		DBOOL			Draw();											// Draws the interface to the main screen
		void			TrackCommands(int argc, char **argv);			// Check the commands from console
		HLOCALOBJ		GetCamera()		{ return m_dCamera.m_hObj; }	// Retrieve the camera

		DATA_CONTROL&	DataControl()	{ return m_dCtrl; }				// Retrieve the control data structure
		DATA_CAMERA&	DataCamera()	{ return m_dCamera; }			// Retrieve the camera data structure
		DATA_CURSOR&	DataCursor()	{ return m_dCursor; }			// Retrieve the cursor data structure
		DATA_TRACK&		DataTrack()		{ return m_dTrack; }			// Retrieve the track data structure
		DATA_KEY&		DataKey()		{ return m_dKey; }				// Retrieve the key data structure
		DATA_ALIGN&		DataAlign()		{ return m_dAlign; }			// Retrieve the align data structure

		LithTrackDrawObj&	DrawObj()	{ return m_ltDO; }				// Retrieve the draw object for the editing track

		void			GUIBarState(int nBar, DBOOL bOn);				// Turn on / off the GUI Bars
		void			ClearControlType();								// Clear out the control types
		DBYTE&			ControlType()	{ return m_byControlType; }		// Get / set the control type
		void			ControlValues(DBYTE &nDevice, int &x, int &y);	// Retrieve control values

#endif

#ifndef _CLIENTBUILD
	public:
		// Server specific public functions

#endif

//---------------------------------------------------------------------------------
// Member variables
//---------------------------------------------------------------------------------

	protected:
		// Shared member variables
		
		INTERFACE			*m_pInterface;					// Main interface variable (client or server handle)

		DBOOL				m_bEditMode;					// Tells if Edit mode is on or off

		LithTrackList		m_tList;						// List of the tracks


#ifdef _CLIENTBUILD

	protected:
		// Client specific member variables

		DDWORD				m_nScreenWidth;					// The width of the main screen
		DDWORD				m_nScreenHeight;				// The height of the main screen

		DATA_CONTROL		m_dCtrl;						// User control data structure
		DATA_CAMERA			m_dCamera;						// Camera data structure
		DATA_CURSOR			m_dCursor;						// Mouse cursor data structure
		DATA_TRACK			m_dTrack;						// Track data structure
		DATA_KEY			m_dKey;							// Key data structure
		DATA_ALIGN			m_dAlign;						// Align data structure

		DBYTE				m_byControlType;				// Type of control to use
		DBYTE				m_byControlDevice;				// Devices being used for control
		int					m_nControlXAxis;				// Control value along the X axis
		int					m_nControlYAxis;				// Control value along the Y axis

		LithFont			*m_pFont;						// Global font for the interface

		HDECOLOR			m_cTransColor;					// Global transparent color for the interface

		LithTrackBarBase	*m_pGUIBars[TRACKMGR_MAX_GUIBARS];	// Our list of GUI bars
		LithTrackBarEdit	m_EditBar;							// GUI Bar for the main TrackEdit display
		LithTrackBarCamera	m_CameraBar;						// GUI Bar to display camera data
		LithTrackBarTrack	m_TrackBar;							// GUI Bar to handle the track list data
		LithTrackBarKey		m_KeyBar;							// GUI Bar to handle the keyframe list data
		LithTrackBarHelp	m_HelpBar;							// GUI Bar to display help information

		DeviceInput			m_diArray[MAX_INPUT_BUFFER_SIZE];	// Array for tracking the input devices

		LITHFONTCREATESTRUCT m_lfCS;						// Font create structure

		LithTrackDrawObj	m_ltDO;							// Draw object for the track being edited

#endif

#ifndef _CLIENTBUILD

	protected:
		// Server specific member variables

#endif

//---------------------------------------------------------------------------------
// Private member functions
//---------------------------------------------------------------------------------

	private:
		// Shared internal functions


#ifdef _CLIENTBUILD

	private:
		// Client specific internal functions
		DBOOL	TrackCommandEqual(char *szTok, const char *szStr);	// Check for equal commands

		void	Command_EditModeOn();
		void	Command_EditModeOff();

		void	Command_Cursor(int nToks, char **szToks);			// Handle mouse cursor commands
		void	Command_Camera(int nToks, char **szToks);			// Handle camera commands
		void	Command_Font(int nToks, char **szToks);				// Handle font commands
		void	Command_Track(int nToks, char **szToks);			// Handle track commands
		void	Command_Key(int nToks, char **szToks);				// Handle key commands

		DBOOL	Ext_Color(int nToks, char **szToks, HDECOLOR &cColor);		// Fills in color values
		DBOOL	Ext_Size(int nToks, char **szToks, int &nSize);				// Fills in size values
		DBOOL	Ext_Scale(int nToks, char **szToks, float &fScale);			// Fills in scale values
		DBOOL	Ext_Thickness(int nToks, char **szToks, int &nThickness);	// Fills in thickness values
		DBOOL	Ext_Speed(int nToks, char **szToks, float &fSpeed);			// Fills in speed values
		DBOOL	Ext_FOV(int nToks, char **szToks, float &fX, float &fY);	// Fills in FOV values
		DBOOL	Ext_Type(int nToks, char **szToks, char *szType);			// Fills in type values
		DBOOL	Ext_Width(int nToks, char **szToks, int &nWidth);			// Fills in width values
		DBOOL	Ext_Height(int nToks, char **szToks, int &nHeight);			// Fills in height values

		DBOOL	SetupMouseCursor();									// Setup the mouse cursor
		void	UpdateInput();										// Update keyboard and mouse data
		void	UpdateControl();									// Updates specific controls
		void	UpdateMouseClicks();								// Handles response to mouse clicks
		void	HandleScreenClick();								// Handles clicks outside the GUI bars

		DBOOL	SetupCamera();										// Setup the camera
		void	UpdateCamera();										// Update control of the camera

		DBOOL	SetupFont();										// Setup a new font

		void	AddGUIBarToList(LithTrackBarBase *ltGUIBar);		// Add a GUI bar to the list
		void	RemoveGUIBarFromList(LithTrackBarBase *ltGUIBar);	// Remove a GUI bar from the list
		void	UpdateGUIBars();									// Updates the information on the GUI Bars

#endif

#ifndef _CLIENTBUILD

	private:
		// Server specific internal function

#endif
};

//*********************************************************************************

#ifdef _CLIENTBUILD
	void TrackEditFn(int argc, char **argv);
#endif

//*********************************************************************************

extern LithTrackMgr *g_pTrackMgr;

//*********************************************************************************

//#endif // _LITHTECH2

#endif // _LITHTECH_TRACK_MGR_H_
