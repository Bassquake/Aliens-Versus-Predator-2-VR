//*********************************************************************************
//*********************************************************************************
// Project:		Lith Tech Track Manager
// Purpose:		A general method for scripted events and cutscenes
//*********************************************************************************
// File:		LithTrackBarBase.h
// Created:		November 23, 1999
// Updated:		November 23, 1999
// Author:		Andy Mattingly
//*********************************************************************************
//*********************************************************************************

#ifndef _LITHTECH_TRACK_BAR_BASE_H_
#define _LITHTECH_TRACK_BAR_BASE_H_

#ifdef _CLIENTBUILD

//*********************************************************************************

#include "lithtrackdatadefs.h"
#include "lithtrackbaritems.h"
#include "lithfontmgr.h"

//*********************************************************************************

class LithTrackBarBase
{
	public:
		// Constructors and destructors
		LithTrackBarBase();
		~LithTrackBarBase();

		// Initialization and termination functions
		virtual DBOOL	Init(INTERFACE *pInterface = DNULL, LithFont *pFont = DNULL, void *pData = DNULL);
		virtual void	Term();

		// GUI update functions
		virtual DBOOL	Update();
		virtual DBOOL	UpdateResolution(DDWORD nWidth, DDWORD nHeight);
		virtual DBOOL	ResetSurface();

		// GUI drawing functions
		virtual DBOOL	Draw(HSURFACE hScreen);
		virtual DBOOL	UpdateItems();
		virtual DBOOL	DrawItems();

		// Mouse input functions
		virtual void	OnMouseMove(int x, int y);
		virtual void	OnMouseClick();

		// Special control functions
		virtual void	OnEditModeOn();
		virtual void	OnEditModeOff();
		virtual void	OnControlChange();

		// Creation functions
		DRect			CreateTextItem(int nItem, char *szStr, HDECOLOR cColor, int x, int y, DBYTE byJustify = GUI_JUSTIFY_LEFT, DDWORD nWidth = 0);
		DRect			CreateButtonItem(int nItem, char *szStr, HDECOLOR cColor, int x, int y, DBYTE byJustify = GUI_JUSTIFY_LEFT, DDWORD nWidth = 0, DBOOL bEnabled = DTRUE);

		void			UpdateTextItemString(int nItem, char *szStr, DBYTE byJustify = GUI_JUSTIFY_LEFT);
		void			UpdateButtonItemString(int nItem, char *szStr, DBYTE byJustify = GUI_JUSTIFY_LEFT);

		// Help functions
		DBOOL			PointInRect(int x, int y, DRect &rect);

		// Status control functions
		DBOOL			IsOn()			{ return m_bOn; }
		void			On()			{ m_bOn = DTRUE; }
		void			Off()			{ m_bOn = DFALSE; }
		void			Toggle()		{ m_bOn = !m_bOn; }

		DBOOL			HasFocus()					{ return m_bFocus; }
		void			SetFocus(DBOOL bFocus);

		// Format variable access
		DDWORD			GetWidth()						{ return m_nSurfWidth; }
		DDWORD			GetHeight()						{ return m_nSurfHeight; }
		void			GetSize(DDWORD &w, DDWORD &h)	{ w = m_nSurfWidth; h = m_nSurfHeight; }

		// Position variables access
		int				GetPosX()					{ return m_nPosX; }
		int				GetPosY()					{ return m_nPosY; }
		void			SetPosX(int x)				{ m_nPosX = x; }
		void			SetPosY(int y)				{ m_nPosY = y; }
		void			GetPos(int &x, int &y)		{ x = m_nPosX; y = m_nPosY; }
		void			SetPos(int x, int y)		{ m_nPosX = x; m_nPosY = y; }

		// Button status variables access
		DBOOL			ButtonEnabled(int nItem)	{ return m_pButtonItems[nItem].m_bEnabled; }
		DBOOL			ButtonSelected(int nItem)	{ return m_pButtonItems[nItem].m_bSelected; }

	protected:
		// Main interface variable (client only for this class)
		INTERFACE		*m_pInterface;
		void			*m_pData;

		// Resource variables
		HSURFACE		m_hSurface;
		LithFont		*m_pFont;
		LithCursor		m_Cursor;

		// General use variables
		DRect			m_rRect;

		// Status variables
		DBOOL			m_bOn;
		DBOOL			m_bFocus;

		// Formating variables
		int				m_nNumLines;
		DDWORD			m_nSurfWidth;
		DDWORD			m_nSurfHeight;

		// Position variables
		int				m_nPosX;
		int				m_nPosY;

		// Color variables
		HDECOLOR		m_cTitle;
		HDECOLOR		m_cNormal;
		HDECOLOR		m_cHighlight;
		HDECOLOR		m_cSelected;
		HDECOLOR		m_cHSelected;
		HDECOLOR		m_cDisabled;
		HDECOLOR		m_cText;
		HDECOLOR		m_cShadow;

		// Data and control variables
		LITHTRACKGUITEXT		m_pTextItems[TRACKMGR_GUI_BAR_MAX_ITEMS];
		LITHTRACKGUIBUTTON		m_pButtonItems[TRACKMGR_GUI_BAR_MAX_ITEMS];
		int						m_nButtonFocus;
};

//*********************************************************************************

#endif // _CLIENTBUILD

#endif // _LITHTECH_TRACK_BAR_BASE_H_