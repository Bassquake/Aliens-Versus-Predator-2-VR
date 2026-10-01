//*********************************************************************************
//*********************************************************************************
// Project:		Lith Tech Track Manager
// Purpose:		A general method for scripted events and cutscenes
//*********************************************************************************
// File:		LithTrack.h
// Created:		November 23, 1999
// Updated:		November 23, 1999
// Author:		Andy Mattingly
//*********************************************************************************
//*********************************************************************************

#ifndef _LITHTECH_TRACK_H_
#define _LITHTECH_TRACK_H_

//*********************************************************************************

#include "lithtrackkey.h"

//*********************************************************************************

#define TRACK_NAME_SIZE						60
#define TRACK_NAME_BUFFER_SIZE				64

//*********************************************************************************

class LithTrack
{
	public:
		// Constructors and destructors
		LithTrack();
		~LithTrack();

		LTBOOL			Init(char *szName = LTNULL);		// Init the track with the starting settings
		LTBOOL			Load(FILE *file);					// Load the track from a file
		void			Term();								// Delete the whole track

		void			Name(char *szName);					// Set the name of the track
		LTBOOL			Copy(LithTrack *pTrack);			// Copy the track

		LTBOOL			InsertKey(int nKey);				// Insert a new key after the ID passed in
		LTBOOL			DeleteKey(int nKey);				// Delete the key of the ID passed in

		LithTrackKey*	Key(int nKey);						// Retreive track data by it's ID

		int				NumKeys()	{ return m_nNumKeys; }	// Retreive the number of key frames
		char*			Name()		{ return m_szName; }	// Retreive the name of the track

	protected:
		char			m_szName[TRACK_NAME_BUFFER_SIZE];	// The name of the track

		LithTrackKey	*m_pList[TRACKMGR_MAX_TRACK_KEYS];	// Pointers to all available keyframes
		int				m_nNumKeys;							// The total number of keyframes in this track
};

//*********************************************************************************

#endif // _LITHTECH_TRACK_H_
