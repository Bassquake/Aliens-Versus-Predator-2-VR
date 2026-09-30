//*********************************************************************************
//*********************************************************************************
// Project:		Lith Tech Track Manager
// Purpose:		A general method for scripted events and cutscenes
//*********************************************************************************
// File:		LithTrackBarHelp.h
// Created:		November 23, 1999
// Updated:		November 23, 1999
// Author:		Andy Mattingly
//*********************************************************************************
//*********************************************************************************

#ifndef _LITHTECH_TRACK_BAR_HELP_H_
#define _LITHTECH_TRACK_BAR_HELP_H_

#ifdef _CLIENTBUILD

//*********************************************************************************

#include "lithtrackbarbase.h"

//*********************************************************************************

#define HELPBAR_TEXT_TITLE							0
#define HELPBAR_TEXT_GENERAL1						1
#define HELPBAR_TEXT_GENERAL2						2
#define HELPBAR_TEXT_COMMANDS						3
#define HELPBAR_TEXT_EXT_COMMANDS					4

//*********************************************************************************

class LithTrackBarHelp : public LithTrackBarBase
{
	public:
		// Initialization and termination functions
		DBOOL			Init(INTERFACE *pInterface = DNULL, LithFont *pFont = DNULL, void *pData = DNULL);

		// GUI update functions
		DBOOL			UpdateResolution(DDWORD nWidth, DDWORD nHeight);

	protected:
		void			SetupLines(int nWidth, int nHeight);
};

//*********************************************************************************

#endif // _CLIENTBUILD

#endif // _LITHTECH_TRACK_BAR_HELP_H_
