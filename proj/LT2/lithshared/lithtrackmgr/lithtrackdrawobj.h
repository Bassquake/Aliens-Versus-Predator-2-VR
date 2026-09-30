//*********************************************************************************
//*********************************************************************************
// Project:		Lith Tech Track Manager
// Purpose:		A general method for scripted events and cutscenes
//*********************************************************************************
// File:		LithTrackDrawObj.h
// Created:		November 23, 1999
// Updated:		November 23, 1999
// Author:		Andy Mattingly
//*********************************************************************************
//*********************************************************************************

#ifndef _LITHTECH_TRACK_DRAW_OBJ_H_
#define _LITHTECH_TRACK_DRAW_OBJ_H_

#ifdef _CLIENTBUILD

//*********************************************************************************

#include "lithtrack.h"

//*********************************************************************************

class LithTrackDrawObj
{
	public:
		// Constructors and destructors
		LithTrackDrawObj();
		~LithTrackDrawObj();

		DBOOL		Init(INTERFACE *pInterface);			// Init the draw object
		void		Term();									// Delete the draw object

		void		Track(LithTrack *pTrack);				// Sets the track that the lines will represent
		LithTrack*	Track()		{ return m_pTrack; }		// Retrieve the track that is being drawn

		void		Reset();								// Reinits the lines of the track
		void		Update();								// Setup the lines to the current track

		void		Select(DBOOL bSelect);					// Set this path to selected or non-selected colors
		void		SelectKey(int nKey);					// Set which key is highlighted
		void		SetPathDetail(int nDetail);				// Set the path curve detail

		void		Visible(DBOOL bVisible);				// Set this path to visible or not
		DBOOL		Visible()	{ return m_bVisible; }		// Is the path visible?

		DVector		Pos();									// Get the object position
		void		Pos(DVector &vPos);						// Set the object position
		void		Pos(DFLOAT x, DFLOAT y, DFLOAT z);		// Set the object position

		DVector		Rot();									// Get the object rotation
		void		Rot(DVector &vRot);						// Set the object rotation
		void		Rot(DFLOAT x, DFLOAT y, DFLOAT z);		// Set the object rotation

	protected:
		// Member variables
		INTERFACE	*m_pInterface;							// Main interface pointer

		HLOCALOBJ	m_hObj;									// The line object to draw with
		DBOOL		m_bVisible;								// Is this object visible?
		DBOOL		m_bSelected;							// Is this object selected?

		DFLOAT		m_fPitch;								// Object pitch
		DFLOAT		m_fYaw;									// Object yaw
		DFLOAT		m_fRoll;								// Object roll

		LithTrack	*m_pTrack;								// Track that the lines will represent
		int			m_nNumKeys;								// The number of keys in the current track

		HDELINE		*m_pLines;								// Line list
		DDWORD		m_nNumLines;							// The number of lines in the list
		DDWORD		m_nNumKeyLines;							// The number of lines used to draw keys
		DDWORD		m_nNumPathLines;						// The number of lines used to draw the path

		DVector		m_vKeyColor;							// Color of the lines making up the keys
		DVector		m_vKeyHColor;							// Color of the selected key
		DVector		m_vPathColor;							// Color of the lines making up the paths
		DVector		m_vMarkerColor;							// Color of the lines making up the marker
		DVector		m_vFlexColor;							// Color of the flex lines of the selected key
		DVector		m_vGreyColor;							// Color of the lines when not selected

		int			m_nLastNumKeys;							// The last number of keys accounted for
		int			m_nPathDetail;							// The detail of path curve lines
};

//*********************************************************************************

#endif // _CLIENTBUILD

#endif // _LITHTECH_TRACK_KEY_H_
