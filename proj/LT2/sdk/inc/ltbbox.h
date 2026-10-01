
/*

This file defines the LTBBox (Lithtech Bounding Box) class.

*/

#ifndef __LTBBOX_H__
#define __LTBBOX_H__


	#include "ltvector.h"


	// ---------------------------------------------------------------------- //
	// LTBBox class.
	// ---------------------------------------------------------------------- //

	class LTBBox
	{
	public:

		LTBBox() {}

		LTBBox(const LTVector & vCenter, const LTVector & vDims)
			: m_vMin(vCenter - vDims),
			  m_vMax(vCenter + vDims) {} 

		// Returns true if the point is inside the box.  Returns false if the point is
		// on an edge or outside the box.
		LTBOOL		PtInsideBox(const LTVector & pt) const;

		// Returns true if the point is inside or touching the box.
		LTBOOL		PtTouchingBox(const LTVector & pt) const;

		// Returns true if the intersection of the boxes have > 0 area.
		LTBOOL		IntersectsBox(const LTBBox & theBox) const;
		
		// Returns true if the intersection of the boxes have >= 0 area.
		LTBOOL		TouchesBox(const LTBBox & theBox) const;

		// Get centerpoint and dimensions.
		LTVector	GetCenter() const;
		LTVector	GetDims() const;

		
		friend void LTStream_Read(ILTStream *pStream, LTBBox &vec);
		friend void LTStream_Write(ILTStream *pStream, const LTBBox &vec);
	

	private :

		LTVector	m_vMin;
		LTVector	m_vMax;
	};



	// ---------------------------------------------------------------------- //
	// Inlines.
	// ---------------------------------------------------------------------- //

	inline LTBOOL LTBBox::PtInsideBox(const LTVector & pt) const
	{
		return pt.x > m_vMin.x && 
			pt.y > m_vMin.y &&
			pt.z > m_vMin.z &&
			pt.x < m_vMax.x &&
			pt.y < m_vMax.y &&
			pt.z < m_vMax.z;
	}

	inline LTBOOL LTBBox::PtTouchingBox(const LTVector & pt) const
	{
		return pt.x >= m_vMin.x && 
			pt.y >= m_vMin.y &&
			pt.z >= m_vMin.z &&
			pt.x <= m_vMax.x &&
			pt.y <= m_vMax.y &&
			pt.z <= m_vMax.z;
	}

	inline LTBOOL LTBBox::IntersectsBox(const LTBBox & theBox) const
	{
		return !(
			theBox.m_vMin.x >= m_vMax.x || 
			theBox.m_vMin.y >= m_vMax.y || 
			theBox.m_vMin.z >= m_vMax.z || 
			theBox.m_vMax.x <= m_vMin.x || 
			theBox.m_vMax.y <= m_vMin.y || 
			theBox.m_vMax.z <= m_vMin.z
		);
	}

	inline LTBOOL LTBBox::TouchesBox(const LTBBox & theBox) const
	{
		return !(
			theBox.m_vMin.x > m_vMax.x || 
			theBox.m_vMin.y > m_vMax.y || 
			theBox.m_vMin.z > m_vMax.z || 
			theBox.m_vMax.x < m_vMin.x || 
			theBox.m_vMax.y < m_vMin.y || 
			theBox.m_vMax.z < m_vMin.z
		);
	}

	inline LTVector LTBBox::GetCenter() const
	{
		return (m_vMin + m_vMax) * 0.5f;
	}

	inline LTVector LTBBox::GetDims() const
	{
		return (m_vMax - m_vMin) * 0.5f;
	}

	//------------------------------------------------------------------
	// Read/write functions.
	//------------------------------------------------------------------

	
	inline void LTStream_Read(ILTStream *pStream, LTBBox & box)
	{
		LTStream_Read(pStream,box.m_vMin);
		LTStream_Read(pStream,box.m_vMax);
	}

	inline void LTStream_Write(ILTStream *pStream, const LTBBox & box)
	{
		LTStream_Write(pStream,box.m_vMin);
		LTStream_Write(pStream,box.m_vMax);
	}


#endif  // __LTBBOX_H__
