/****************************************************************************
;
;	MODULE:		LithSimpAryStat (.H)
;
;	PURPOSE:	Simple array template class that can check bounds in debug builds.
;
;	HISTORY:	APR-16-98 [blb]
;
;	NOTICE:		Copyright (c) 1998, MONOLITH, Inc.
;
***************************************************************************/


#ifndef LithSimpAryStat_H
#define LithSimpAryStat_H

#include "lith.h"

// examples of use
//#define CLithSimpAryStatVoidPtr		CLithSimpAryStat<void*,10>
//#define CLithSimpAryStatChar			CLithSimpAryStat<char,5>


#ifdef _DEBUG
#ifndef _LithSimpAryStat_CheckBounds
#define _LithSimpAryStat_CheckBounds
#endif
#endif

	
template<class Type, int nSize>
class CLithSimpAryStat
{
public:

	// Copy data to the array
	void Copy( const Type* pSource, const unsigned int nLen )
	{
#ifdef _LithSimpAryStat_CheckBounds
		ASSERT( nLen <= m_nSize );
#endif
		memcpy(m_pData, pSource, nLen);
	}

	// Get an element of the array
	Type& Get( const unsigned int nIndex ) 
	{
#ifdef _LithSimpAryStat_CheckBounds
		ASSERT( nIndex < nSize );
#endif
		return m_pData[nIndex];
	}

	// Set an element of the array
	void Set( const unsigned int nIndex, Type &data )
	{
#ifdef _LithSimpAryStat_CheckBounds
		ASSERT( nIndex < nSize );
#endif
		m_pData[nIndex] = data;
	}


	// Access to the array with the standard []
//	Type& operator[]( const unsigned int nIndex ) const { return Get(nIndex); };

	// allow direct access to the array if necessary
//	operator const Type* () { return m_pData; };
	operator Type* () { return m_pData; };

private:
#ifdef _LithSimpAryStat_CheckBounds
	unsigned int m_nSize;
#endif
	Type m_pData[nSize];
};


#endif 

