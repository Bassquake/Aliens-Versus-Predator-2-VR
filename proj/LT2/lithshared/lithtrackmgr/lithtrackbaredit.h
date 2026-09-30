//*********************************************************************************
//*********************************************************************************
// Project:		Lith Tech Track Manager
// Purpose:		A general method for scripted events and cutscenes
//*********************************************************************************
// File:		LithTrackBarEdit.h
// Created:		November 23, 1999
// Updated:		November 23, 1999
// Author:		Andy Mattingly
//*********************************************************************************
//*********************************************************************************

#ifndef _LITHTECH_TRACK_BAR_EDIT_H_
#define _LITHTECH_TRACK_BAR_EDIT_H_

#ifdef _CLIENTBUILD

//*********************************************************************************

#include "lithtrackbarbase.h"

//*********************************************************************************

#define EDITBAR_TEXT_TITLE							0
#define EDITBAR_TEXT_REFERENCE_TYPE					1
#define EDITBAR_TEXT_REFERENCE_POS					2

#define EDITBAR_BUTTON_MANAGER						0
#define EDITBAR_BUTTON_CAMERA						1
#define EDITBAR_BUTTON_TRACK						2
#define EDITBAR_BUTTON_KEY							3
#define EDITBAR_BUTTON_HELP							4
#define EDITBAR_BUTTON_REFERENCE					5

//*********************************************************************************

class LithTrackBarEdit : public LithTrackBarBase
{
	public:
		// Initialization and termination functions
		DBOOL			Init(INTERFACE *pInterface = DNULL, LithFont *pFont = DNULL, void *pData = DNULL);

		// GUI update functions
		DBOOL			Update();
		DBOOL			UpdateResolution(DDWORD nWidth, DDWORD nHeight);
		DBOOL			UpdateItems();

		// Mouse input functions
		void			OnMouseClick();

	protected:
		void			SetupLines(int nWidth, int nHeight);

		DATA_ALIGN		m_dAlign;
};

//*********************************************************************************

#endif // _CLIENTBUILD

#endif // _LITHTECH_TRACK_GUI_BAR_H_
