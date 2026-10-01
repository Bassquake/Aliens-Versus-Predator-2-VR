//*********************************************************************************
//*********************************************************************************
// Project:		Lith Tech Track Manager
// Purpose:		A general method for scripted events and cutscenes
//*********************************************************************************
// File:		LithTrackBarItems.h
// Created:		November 23, 1999
// Updated:		November 23, 1999
// Author:		Andy Mattingly
//*********************************************************************************
//*********************************************************************************

#ifndef _LITHTECH_TRACK_BAR_ITEMS_H_
#define _LITHTECH_TRACK_BAR_ITEMS_H_

#ifdef _CLIENTBUILD

//*********************************************************************************

#include "lithtrackdefs.h"
#include "lithfontmgr.h"

//*********************************************************************************

struct LITHTRACKGUITEXT
{
	// Constructors and destructors
	LITHTRACKGUITEXT::LITHTRACKGUITEXT();

	// Member variables
	HSTRING			m_szText;
	HDECOLOR		m_cColor;
	DRect			m_rRect;
	DDWORD			m_nWidth;
};

//---------------------------------------------------------------------------------

inline LITHTRACKGUITEXT::LITHTRACKGUITEXT()
{
	memset(this, 0, sizeof(LITHTRACKGUITEXT));
}

//*********************************************************************************

struct LITHTRACKGUIBUTTON
{
	// Constructors and destructors
	LITHTRACKGUIBUTTON::LITHTRACKGUIBUTTON();

	// Member variables
	HSTRING			m_szText;
	HDECOLOR		m_cColor;
	DRect			m_rRect;
	DDWORD			m_nWidth;

	DBOOL			m_bEnabled;
	DBOOL			m_bSelected;
};

//---------------------------------------------------------------------------------

inline LITHTRACKGUIBUTTON::LITHTRACKGUIBUTTON()
{
	memset(this, 0, sizeof(LITHTRACKGUIBUTTON));
	m_bEnabled = DTRUE;
}

//*********************************************************************************

#endif // _CLIENTBUILD

#endif // _LITHTECH_TRACK_BAR_ITEMS_H_