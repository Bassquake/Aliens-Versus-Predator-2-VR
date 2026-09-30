//*********************************************************************************
//*********************************************************************************
// Project:		Lith Tech Track Manager
// Purpose:		A general method for scripted events and cutscenes
//*********************************************************************************
// File:		LithTrackKey.h
// Created:		November 23, 1999
// Updated:		November 23, 1999
// Author:		Andy Mattingly
//*********************************************************************************
//*********************************************************************************

#ifndef _LITHTECH_TRACK_KEY_H_
#define _LITHTECH_TRACK_KEY_H_

//*********************************************************************************

#include "lithtrackdefs.h"
#include <stdio.h>

//*********************************************************************************

class LithTrackKey
{
	public:
		// Constructors and destructors
		LithTrackKey();
		~LithTrackKey();

		LTBOOL	Init();										// Init the key with the starting settings
		LTBOOL	Load(FILE *file);							// Load the key from a file
		void	Term();										// Delete the key

		LTBOOL	Copy(LithTrackKey *pKey);					// Copy the key

		void	CalcCtrlPts();								// Calculate the control points based off the flex value
		void	CalcCurve(LithTrackKey *pKey = LTNULL);		// Calculate the curve values from this key to the next
		void	CalcCurvePt(LTVector &vPt, LTFLOAT fTime);	// Calculate a point on the curve

	public:
		// Member variables
		LTVector	m_vPos;									// Offset from previous key position
		LTVector	m_vRot;									// Offset from previous key rotation

		// Control point variables
		LTFLOAT		m_fFlex;								// The amount to flex along the forward axis
		LTVector	m_vCtrlPt1, m_vCtrlPt2;					// The two control points calculated using the flex value and rot

		// Curve path variables
		LTFLOAT		m_fXA, m_fXB, m_fXC;					// The three precalculated curve values for X
		LTFLOAT		m_fYA, m_fYB, m_fYC;					// The three precalculated curve values for Y
		LTFLOAT		m_fZA, m_fZB, m_fZC;					// The three precalculated curve values for Z

		// Time variables
		LTFLOAT		m_fTime;
};

//*********************************************************************************

#endif // _LITHTECH_TRACK_KEY_H_
