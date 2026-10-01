//*********************************************************************************
//*********************************************************************************
// Project:		Lith Tech Track Manager
// Purpose:		A general method for scripted events and cutscenes
//*********************************************************************************
// File:		LithTrackList.h
// Created:		November 23, 1999
// Updated:		November 23, 1999
// Author:		Andy Mattingly
//*********************************************************************************
//*********************************************************************************

#ifndef _LITHTECH_TRACK_LIST_H_
#define _LITHTECH_TRACK_LIST_H_

//*********************************************************************************

#include "lithtrack.h"

//*********************************************************************************

class LithTrackList
{
	public:
		// Constructors and destructors
		LithTrackList();
		~LithTrackList();

		DBOOL		Init(char *szFile = TRACKMGR_DATA_FILE);		// Init the list from a file
		void		Term();											// Terminate the list

		DBOOL		New(char *szName = DNULL);						// Adds a blank new track to the end of the list
		DBOOL		Copy(int nTrack, char *szName = DNULL);			// Copys a track into a new one at the end
		DBOOL		Copy(char *szName, char *szName2 = DNULL);		// Copys a track into a new one at the end
		DBOOL		Delete(int nTrack);								// Removes a specific track by it's ID
		DBOOL		Delete(char *szName);							// Removes a specific track by it's name

		LithTrack*	Track(int nTrack);								// Retreive track data by it's ID
		LithTrack*	Track(char *szName, int *nTrack = DNULL);		// Retreive track data by it's name and get the ID

		int			NumTracks()		{ return m_nNumTracks; }		// Retreive the number of available tracks

	protected:
		LithTrack	*m_pList[TRACKMGR_MAX_TRACKS];					// Pointers to all available tracks
		int			m_nNumTracks;									// The total number of tracks available

	private:
		DBOOL		LoadTracks(FILE *file);							// Load in the track data

		DBOOL		CreateNewName(char *szNewName, const char *szName);	// Create an original track name
};

//*********************************************************************************

#endif // _LITHTECH_TRACK_LIST_H_
